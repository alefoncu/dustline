import Foundation
import simd

typealias V3 = SIMD3<Float>

@inline(__always) func clamp(_ x: Float, _ a: Float, _ b: Float) -> Float { min(max(x, a), b) }
@inline(__always) func smoothstep(_ a: Float, _ b: Float, _ x: Float) -> Float {
    let t = clamp((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)
}
@inline(__always) func mix(_ a: Float, _ b: Float, _ t: Float) -> Float { a + (b - a) * t }

// MARK: - Noise

@inline(__always) func hash2(_ x: Int32, _ z: Int32, _ seed: UInt32) -> Float {
    var h = UInt32(bitPattern: x) &* 374761393 &+ UInt32(bitPattern: z) &* 668265263 &+ seed &* 2246822519
    h = (h ^ (h >> 13)) &* 1274126177
    h = h ^ (h >> 16)
    return Float(h & 0xFFFFFF) / Float(0x1000000)
}

func vnoise(_ x: Float, _ z: Float, _ seed: UInt32) -> Float {
    let xi = floor(x), zi = floor(z)
    let fx = x - xi, fz = z - zi
    let ix = Int32(xi), iz = Int32(zi)
    let u = fx * fx * fx * (fx * (fx * 6 - 15) + 10)
    let v = fz * fz * fz * (fz * (fz * 6 - 15) + 10)
    let a = hash2(ix, iz, seed), b = hash2(ix + 1, iz, seed)
    let c = hash2(ix, iz + 1, seed), d = hash2(ix + 1, iz + 1, seed)
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v
}

func fbm(_ x: Float, _ z: Float, _ octaves: Int, _ seed: UInt32) -> Float {
    var amp: Float = 0.5, freq: Float = 1, sum: Float = 0, norm: Float = 0
    for i in 0..<octaves {
        sum += amp * vnoise(x * freq, z * freq, seed &+ UInt32(i) &* 7919)
        norm += amp
        amp *= 0.5
        freq *= 2.03
    }
    return sum / norm
}

struct SplitMix64 {
    var s: UInt64
    init(_ seed: UInt64) { s = seed }
    mutating func next() -> UInt64 {
        s &+= 0x9E3779B97F4A7C15
        var z = s
        z = (z ^ (z >> 30)) &* 0xBF58476D1CE4E5B9
        z = (z ^ (z >> 27)) &* 0x94D049BB133111EB
        return z ^ (z >> 31)
    }
    mutating func float() -> Float { Float(next() >> 40) / Float(1 << 24) }
    mutating func range(_ a: Float, _ b: Float) -> Float { a + (b - a) * float() }
}

// MARK: - Weather state (physics + visuals read this)

final class Weather {
    var rain: Float = 0       // 0...1 current rain intensity
    var snow: Float = 0       // 0...1 current snowfall intensity
    var fog: Float = 0        // 0...1
    var wet: Float = 0        // ground wetness -> mud
    var snowCover: Float = 0  // ground covered in snow
    var name: String {
        if snow > 0.35 { return "Nevada" }
        if rain > 0.35 { return "Lluvia" }
        if fog > 0.5 { return "Niebla" }
        if snowCover > 0.5 { return "Nieve acumulada" }
        if wet > 0.5 { return "Suelo embarrado" }
        return "Despejado"
    }
}

enum SurfaceKind: String {
    case grass = "Hierba", dirt = "Tierra", sand = "Arena", mud = "Barro"
    case snow = "Nieve", water = "Agua", rock = "Roca"
}

struct Surface {
    var kind: SurfaceKind
    var mu: Float      // grip coefficient
    var drag: Float    // extra rolling resistance (N per m/s per wheel)
}

struct Obstacle {
    var x: Float, z: Float, r: Float
    var id: Int = -1   // >= 0 for dynamic obstacles
    var active: Bool = true
}

// MARK: - World data (pure, no rendering)

final class WorldData {
    static let half: Float = 500
    static let waterLevel: Float = 0
    let weather = Weather()

    struct Tree { var x, y, z, scale: Float }
    struct Rock { var x, y, z, scale: Float; var rot: Float }

    var trees: [Tree] = []
    var rocks: [Rock] = []
    var beacons: [V3] = []
    private(set) var staticObstacles: [Obstacle] = []
    private var grid: [Int: [Int]] = [:]
    var dynamicObstacles: [Obstacle] = []

    static let cell: Float = 20

    init() { generate() }

    // Terrain height
    @inline(__always) func rawHeight(_ x: Float, _ z: Float) -> Float {
        let base = fbm(x * 0.0035, z * 0.0035, 5, 11)
        let rid = 1 - abs(2 * fbm(x * 0.006 + 40, z * 0.006 - 17, 4, 23) - 1)
        let mask = smoothstep(0.45, 0.7, fbm(x * 0.003 - 9, z * 0.003 + 5, 3, 37))
        var h = (base - 0.42) * 62 + rid * rid * 34 * mask
        h += fbm(x * 0.05, z * 0.05, 3, 51) * 2.2 + fbm(x * 0.25, z * 0.25, 2, 61) * 0.35
        // flat clearing at spawn
        let d = (x * x + z * z).squareRoot()
        h = mix(h, 5, smoothstep(70, 22, d))
        // boundary mountains
        let e = max(abs(x), abs(z)) - 430
        if e > 0 { h += e * e * 0.02 + e * 0.6 }
        // limit lake depth
        if h < -1 { h = -1 + (h + 1) * 0.35 }
        return h
    }

    @inline(__always) func height(_ x: Float, _ z: Float) -> Float { rawHeight(x, z) }

    func normal(_ x: Float, _ z: Float) -> V3 {
        let e: Float = 0.6
        let dx = (height(x + e, z) - height(x - e, z)) / (2 * e)
        let dz = (height(x, z + e) - height(x, z - e)) / (2 * e)
        return simd_normalize(V3(-dx, 1, -dz))
    }

    /// Permanent mud patches in lowlands (independent of weather).
    func mudPatch(_ x: Float, _ z: Float, _ h: Float) -> Float {
        smoothstep(0.58, 0.68, fbm(x * 0.012 + 100, z * 0.012 - 60, 3, 71)) * smoothstep(9, 3, h)
    }

    func surface(_ x: Float, _ z: Float, _ h: Float, ny: Float) -> Surface {
        let w = weather
        if h < WorldData.waterLevel - 0.05 { return Surface(kind: .water, mu: 0.5, drag: 380) }
        let snowAlt = smoothstep(36, 48, h)
        let snow = max(snowAlt, w.snowCover * smoothstep(0.6, 0.85, ny))
        let mud = max(mudPatch(x, z, h), w.wet * smoothstep(10, 3, h) * 0.85)
        if snow > 0.5 { return Surface(kind: .snow, mu: mix(0.8, 0.4, snow), drag: 70 + 60 * snow) }
        if mud > 0.45 { return Surface(kind: .mud, mu: mix(0.9, 0.5, mud), drag: 90 + 260 * mud) }
        if h < 1.2 { return Surface(kind: .sand, mu: 0.75, drag: 120) }
        if ny < 0.72 { return Surface(kind: .rock, mu: 1.1, drag: 35) }
        let wetGrip = 1 - 0.3 * w.wet
        if fbm(x * 0.02, z * 0.02, 2, 91) > 0.58 { return Surface(kind: .dirt, mu: 0.95 * wetGrip, drag: 45) }
        return Surface(kind: .grass, mu: 1.0 * wetGrip, drag: 55)
    }

    // Base vertex colour (weather is applied by a shader on top)
    func color(_ x: Float, _ z: Float, _ h: Float, _ ny: Float) -> V3 {
        let n = vnoise(x * 0.09, z * 0.09, 5) * 0.2 - 0.1
        var c = V3(0.33, 0.45, 0.19)
        let dry = smoothstep(0.52, 0.64, fbm(x * 0.02, z * 0.02, 2, 91))
        c = simd_mix(c, V3(0.55, 0.47, 0.28), V3(repeating: dry))
        c = simd_mix(c, V3(0.78, 0.70, 0.48), V3(repeating: smoothstep(1.6, 0.6, h)))
        let mud = mudPatch(x, z, h)
        c = simd_mix(c, V3(0.28, 0.20, 0.12), V3(repeating: smoothstep(0.1, 0.5, mud)))
        c = simd_mix(c, V3(0.46, 0.43, 0.40), V3(repeating: smoothstep(0.82, 0.68, ny)))
        c = simd_mix(c, V3(0.90, 0.92, 0.95), V3(repeating: smoothstep(38, 50, h) * smoothstep(0.55, 0.8, ny)))
        if h < 0.2 { c = simd_mix(c, V3(0.22, 0.26, 0.2), V3(repeating: smoothstep(0.2, -0.8, h))) }
        return simd_clamp(c + V3(repeating: n), V3(repeating: 0), V3(repeating: 1))
    }

    // MARK: Generation

    private func generate() {
        var rng = SplitMix64(0xC0FFEE)
        // Trees (clustered into forests)
        var attempts = 0
        while trees.count < 2600 && attempts < 60000 {
            attempts += 1
            let x = rng.range(-WorldData.half + 40, WorldData.half - 40)
            let z = rng.range(-WorldData.half + 40, WorldData.half - 40)
            if x * x + z * z < 30 * 30 { continue }
            let h = height(x, z)
            if h < 1.5 || h > 34 { continue }
            if normal(x, z).y < 0.86 { continue }
            let forest = fbm(x * 0.008 + 200, z * 0.008, 3, 83)
            if forest < 0.5 || rng.float() > (forest - 0.45) * 2.4 { continue }
            if mudPatch(x, z, h) > 0.3 { continue }
            trees.append(Tree(x: x, y: h, z: z, scale: rng.range(0.8, 1.6)))
        }
        attempts = 0
        while rocks.count < 900 && attempts < 40000 {
            attempts += 1
            let x = rng.range(-WorldData.half + 40, WorldData.half - 40)
            let z = rng.range(-WorldData.half + 40, WorldData.half - 40)
            if x * x + z * z < 28 * 28 { continue }
            let h = height(x, z)
            if h < 0.6 { continue }
            let s = rng.float() < 0.18 ? rng.range(2.0, 4.2) : rng.range(0.5, 1.6)
            rocks.append(Rock(x: x, y: h, z: z, scale: s, rot: rng.range(0, 6.28)))
        }
        // Beacons
        attempts = 0
        while beacons.count < 25 && attempts < 20000 {
            attempts += 1
            let x = rng.range(-400, 400), z = rng.range(-400, 400)
            if x * x + z * z < 70 * 70 { continue }
            let h = height(x, z)
            if h < 1.5 || h > 40 || normal(x, z).y < 0.9 { continue }
            if beacons.contains(where: { simd_distance(V3($0.x, 0, $0.z), V3(x, 0, z)) < 90 }) { continue }
            beacons.append(V3(x, h, z))
        }
        // Static obstacle grid: trees and big rocks
        for t in trees { staticObstacles.append(Obstacle(x: t.x, z: t.z, r: 0.45 * t.scale)) }
        for r in rocks where r.scale >= 1.2 { staticObstacles.append(Obstacle(x: r.x, z: r.z, r: r.scale * 0.92)) }
        for (i, o) in staticObstacles.enumerated() {
            grid[cellKey(o.x, o.z), default: []].append(i)
        }
    }

    private func cellKey(_ x: Float, _ z: Float) -> Int {
        let cx = Int(floor(x / WorldData.cell)), cz = Int(floor(z / WorldData.cell))
        return cx &* 100003 &+ cz
    }

    /// Calls body for each obstacle near (x,z).
    func forEachObstacle(near x: Float, _ z: Float, _ body: (Obstacle) -> Void) {
        let cx = Int(floor(x / WorldData.cell)), cz = Int(floor(z / WorldData.cell))
        for i in -1...1 {
            for j in -1...1 {
                if let list = grid[(cx + i) &* 100003 &+ (cz + j)] {
                    for idx in list { body(staticObstacles[idx]) }
                }
            }
        }
        for o in dynamicObstacles where o.active {
            if abs(o.x - x) < 40 && abs(o.z - z) < 40 { body(o) }
        }
    }
}
