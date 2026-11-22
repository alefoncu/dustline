import SceneKit
import AppKit
import simd

enum WeatherKind: Int, CaseIterable {
    case clear, rain, snow, fog
    var label: String { ["Despejado", "Lluvia", "Nevada", "Niebla"][rawValue] }
}

/// Cycles weather, updates the physical state (wet ground, snow cover) and the scene look.
final class WeatherSystem {
    let world: WorldData
    let scene: SCNScene
    let sun: SCNNode
    let ambient: SCNNode
    let precip = SCNNode()
    let rainPS = SCNParticleSystem()
    let snowPS = SCNParticleSystem()

    var target: WeatherKind = .clear
    var auto = true
    private var timer: Float = 45
    private var rng = SplitMix64(42)
    private var sky = V3(0.55, 0.75, 0.95)
    var instant = false

    init(world: WorldData, scene: SCNScene, sun: SCNNode, ambient: SCNNode) {
        self.world = world
        self.scene = scene
        self.sun = sun
        self.ambient = ambient

        rainPS.birthRate = 0
        rainPS.particleLifeSpan = 0.85
        rainPS.particleSize = 0.03
        rainPS.particleVelocity = 36
        rainPS.emittingDirection = SCNVector3(0.08, -1, 0)
        rainPS.spreadingAngle = 2
        rainPS.emitterShape = SCNBox(width: 60, height: 1, length: 60, chamferRadius: 0)
        rainPS.birthLocation = .volume
        rainPS.orientationMode = .billboardScreenAligned
        rainPS.stretchFactor = 0.05
        rainPS.isLocal = false
        rainPS.blendMode = .alpha
        rainPS.particleImage = softDotImage
        rainPS.particleColor = color(0.75, 0.85, 1, 0.55)
        rainPS.isAffectedByGravity = false

        snowPS.birthRate = 0
        snowPS.particleLifeSpan = 7
        snowPS.particleSize = 0.17
        snowPS.particleSizeVariation = 0.08
        snowPS.particleVelocity = 3.4
        snowPS.particleVelocityVariation = 1.2
        snowPS.emittingDirection = SCNVector3(0.3, -1, 0.1)
        snowPS.spreadingAngle = 35
        snowPS.emitterShape = SCNBox(width: 70, height: 1, length: 70, chamferRadius: 0)
        snowPS.birthLocation = .volume
        snowPS.isLocal = false
        snowPS.blendMode = .alpha
        snowPS.particleImage = softDotImage
        snowPS.particleColor = color(1, 1, 1, 0.9)
        snowPS.isAffectedByGravity = false

        let rn = SCNNode(), sn = SCNNode()
        rn.addParticleSystem(rainPS)
        sn.addParticleSystem(snowPS)
        precip.addChildNode(rn)
        precip.addChildNode(sn)
        scene.rootNode.addChildNode(precip)
        scene.fogDensityExponent = 1
        apply(dt: 0)
    }

    func force(_ k: WeatherKind) {
        target = k
        auto = false
        timer = 120
    }

    func setAuto() { auto = true; timer = 5 }

    private func goal(_ k: WeatherKind) -> (rain: Float, snow: Float, fog: Float, sky: V3, sun: Float, amb: Float) {
        switch k {
        case .clear: return (0, 0, 0, V3(0.55, 0.75, 0.95), 1.5, 0.55)
        case .rain: return (1, 0, 0.35, V3(0.42, 0.47, 0.52), 0.55, 0.5)
        case .snow: return (0, 1, 0.45, V3(0.80, 0.83, 0.88), 0.8, 0.65)
        case .fog: return (0, 0, 1, V3(0.72, 0.75, 0.78), 0.7, 0.6)
        }
    }

    func update(dt: Float, center: V3) {
        precip.simdPosition = center + V3(0, 24, 0)
        if auto {
            timer -= dt
            if timer <= 0 {
                timer = rng.range(55, 95)
                let r = rng.float()
                target = r < 0.30 ? .clear : (r < 0.58 ? .rain : (r < 0.82 ? .snow : .fog))
            }
        }
        apply(dt: dt)
    }

    private func apply(dt: Float) {
        let g = goal(target)
        let w = world.weather
        let k = instant ? 1 : min(1, dt / 9)
        w.rain += (g.rain - w.rain) * k
        w.snow += (g.snow - w.snow) * k
        w.fog += (g.fog - w.fog) * k
        sky += (g.sky - sky) * k

        // ground state
        if instant {
            w.wet = g.rain > 0 ? 1 : 0
            w.snowCover = g.snow > 0 ? 1 : 0
            instant = false
        }
        w.wet += (w.rain * 0.07 - (w.rain < 0.15 ? 0.008 : 0)) * dt
        w.wet = clamp(w.wet, 0, 1)
        w.snowCover += (w.snow * 0.05 - (w.snow < 0.1 ? 0.01 : 0)) * dt
        w.snowCover = clamp(w.snowCover, 0, 1)
        if w.snowCover > 0 { w.wet = max(0, w.wet - 0.03 * dt * w.snowCover) }

        let c = NSColor(srgbRed: cg(sky.x), green: cg(sky.y), blue: cg(sky.z), alpha: 1)
        scene.background.contents = c
        scene.fogColor = c
        scene.fogStartDistance = cg(mix(220, 4, w.fog) * (1 - 0.2 * max(w.rain, w.snow)))
        scene.fogEndDistance = cg(mix(760, 95, w.fog) * (1 - 0.35 * max(w.rain, w.snow)))
        sun.light?.intensity = cg(mix(1000, 500, max(w.rain, w.fog * 0.6)))
        ambient.light?.intensity = cg(mix(380, 520, max(w.snow, w.fog)))
        rainPS.birthRate = cg(w.rain * 2600)
        snowPS.birthRate = cg(w.snow * 1500)
    }
}
