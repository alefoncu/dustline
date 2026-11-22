import SceneKit
import AppKit
import simd

func cg(_ f: Float) -> CGFloat { CGFloat(f) }
func color(_ r: Float, _ g: Float, _ b: Float, _ a: Float = 1) -> NSColor {
    NSColor(srgbRed: cg(r), green: cg(g), blue: cg(b), alpha: cg(a))
}

func solid(_ c: NSColor, specular: CGFloat = 0.25, shininess: CGFloat = 0.3) -> SCNMaterial {
    let m = SCNMaterial()
    m.lightingModel = .blinn
    m.diffuse.contents = c
    m.specular.contents = NSColor(white: specular, alpha: 1)
    m.shininess = shininess
    return m
}

func emissive(_ c: NSColor) -> SCNMaterial {
    let m = SCNMaterial()
    m.lightingModel = .constant
    m.diffuse.contents = c
    m.emission.contents = c
    return m
}

func boxNode(_ w: Float, _ h: Float, _ l: Float, _ m: SCNMaterial, _ x: Float, _ y: Float, _ z: Float, chamfer: Float = 0) -> SCNNode {
    let g = SCNBox(width: cg(w), height: cg(h), length: cg(l), chamferRadius: cg(chamfer))
    g.materials = [m]
    let n = SCNNode(geometry: g)
    n.simdPosition = V3(x, y, z)
    return n
}

func tubeNode(_ a: V3, _ b: V3, _ r: Float, _ m: SCNMaterial) -> SCNNode {
    let d = b - a
    let len = simd_length(d)
    let g = SCNCylinder(radius: cg(r), height: cg(len))
    g.radialSegmentCount = 8
    g.materials = [m]
    let n = SCNNode(geometry: g)
    n.simdPosition = (a + b) / 2
    n.simdOrientation = simd_quatf(from: V3(0, 1, 0), to: d / len)
    return n
}

let softDotImage: NSImage = {
    let size = NSSize(width: 64, height: 64)
    let img = NSImage(size: size)
    img.lockFocus()
    let grad = NSGradient(colors: [NSColor(white: 1, alpha: 1), NSColor(white: 1, alpha: 0)])!
    grad.draw(in: NSBezierPath(ovalIn: NSRect(origin: .zero, size: size)), relativeCenterPosition: .zero)
    img.unlockFocus()
    return img
}()

// MARK: - Car visual

final class CarVisual {
    let root = SCNNode()
    var pivots: [SCNNode] = []
    var spins: [SCNNode] = []
    var dust: [SCNParticleSystem] = []
    let setup: Setup
    let spec: Spec
    let wheelX: Float

    init(setup: Setup) {
        self.setup = setup
        spec = Spec(setup)
        let m = carModels[setup.model]
        let c = m.colors[min(setup.color, m.colors.count - 1)]
        let paint = solid(color(c.0, c.1, c.2), specular: 0.7, shininess: 0.6)
        let glass = solid(color(0.05, 0.07, 0.1), specular: 1, shininess: 0.9)
        let dark = solid(color(0.07, 0.07, 0.08), specular: 0.1)
        let metal = solid(color(0.55, 0.56, 0.58), specular: 0.9, shininess: 0.8)
        let L = m.length, W = m.width, H = m.height
        let sp = spec
        let hl = sp.halfBodyL, hw = sp.halfBodyW
        let roof = H - 0.6
        wheelX = hw + setup.tireWidth * 0.35

        switch m.style {
        case .boxySUV, .classic4x4, .crawler:
            let lowerTop: Float = m.style == .crawler ? 0.65 : 0.5
            root.addChildNode(boxNode(W - 0.08, lowerTop + 0.42, L - 0.2, paint, 0, (lowerTop - 0.42) / 2, 0, chamfer: 0.06))
            let ch = roof - lowerTop
            let cl = L * (m.style == .crawler ? 0.5 : 0.7)
            root.addChildNode(boxNode(W - 0.22, ch, cl, glass, 0, lowerTop + ch / 2, -0.15, chamfer: 0.04))
            root.addChildNode(boxNode(W - 0.18, 0.1, cl + 0.06, paint, 0, roof, -0.15, chamfer: 0.03))
            // bumpers & grille
            root.addChildNode(boxNode(W - 0.1, 0.22, 0.25, dark, 0, -0.2, hl + 0.1))
            root.addChildNode(boxNode(W - 0.1, 0.22, 0.25, dark, 0, -0.2, -hl - 0.1))
            root.addChildNode(boxNode(W * 0.45, 0.3, 0.06, dark, 0, 0.18, hl + 0.01))
            if m.style != .crawler {
                // spare wheel on the back
                let spare = SCNCylinder(radius: 0.38, height: 0.28)
                spare.materials = [dark]
                let sn = SCNNode(geometry: spare)
                sn.simdPosition = V3(0, 0.45, -hl - 0.28)
                sn.eulerAngles.x = .pi / 2
                root.addChildNode(sn)
            } else {
                for x in [-hw + 0.1, hw - 0.1] {
                    root.addChildNode(tubeNode(V3(x, roof, 0.7), V3(x, roof, -1.0), 0.05, metal))
                }
            }
        case .wedgeSUV:
            root.addChildNode(boxNode(W - 0.06, 0.78, L - 0.2, paint, 0, 0.0, 0, chamfer: 0.2))
            let ch = roof - 0.35
            root.addChildNode(boxNode(W - 0.3, ch, L * 0.46, glass, 0, 0.35 + ch / 2, -L * 0.1, chamfer: 0.18))
            root.addChildNode(boxNode(W - 0.26, 0.08, L * 0.36, paint, 0, roof, -L * 0.1, chamfer: 0.04))
            root.addChildNode(boxNode(W * 0.6, 0.12, 0.06, dark, 0, -0.05, hl + 0.02))
        case .microBox:
            root.addChildNode(boxNode(W - 0.08, 0.9, L - 0.1, paint, 0, 0.0, 0, chamfer: 0.12))
            let ch = roof - 0.45
            root.addChildNode(boxNode(W - 0.2, ch, L * 0.68, glass, 0, 0.45 + ch / 2, -0.05, chamfer: 0.08))
            root.addChildNode(boxNode(W - 0.16, 0.08, L * 0.68 + 0.04, paint, 0, roof, -0.05, chamfer: 0.04))
        case .pickup:
            root.addChildNode(boxNode(W - 0.08, 0.92, L - 0.2, paint, 0, 0.04, 0, chamfer: 0.06))
            let ch = roof - 0.5
            root.addChildNode(boxNode(W - 0.22, ch, L * 0.34, glass, 0, 0.5 + ch / 2, L * 0.12, chamfer: 0.05))
            root.addChildNode(boxNode(W - 0.18, 0.1, L * 0.34 + 0.05, paint, 0, roof, L * 0.12, chamfer: 0.03))
            let bedZ = -L * 0.2
            root.addChildNode(boxNode(0.08, 0.38, L * 0.34, paint, -hw + 0.04, 0.69, bedZ))
            root.addChildNode(boxNode(0.08, 0.38, L * 0.34, paint, hw - 0.04, 0.69, bedZ))
            root.addChildNode(boxNode(W - 0.1, 0.38, 0.08, paint, 0, 0.69, -hl + 0.04))
            root.addChildNode(boxNode(W - 0.1, 0.38, 0.08, paint, 0, 0.69, bedZ + L * 0.17))
            root.addChildNode(boxNode(W - 0.1, 0.22, 0.25, dark, 0, -0.2, hl + 0.1))
        case .buggy:
            root.addChildNode(boxNode(W - 0.7, 0.12, L - 0.5, dark, 0, -0.35, 0))
            root.addChildNode(boxNode(W - 0.8, 0.5, 0.9, paint, 0, -0.05, hl - 0.4, chamfer: 0.1))      // nose
            root.addChildNode(boxNode(W - 0.9, 0.45, 0.7, metal, 0, 0.0, -hl + 0.45))                      // engine
            root.addChildNode(boxNode(0.4, 0.15, 0.35, dark, -0.3, 0.05, 0.0))                           // seats
            root.addChildNode(boxNode(0.4, 0.15, 0.35, dark, 0.3, 0.05, 0.0))
            root.addChildNode(boxNode(0.4, 0.5, 0.1, dark, -0.3, 0.3, -0.2))
            root.addChildNode(boxNode(0.4, 0.5, 0.1, dark, 0.3, 0.3, -0.2))
            let cx = W / 2 - 0.45
            let top = roof
            for s: Float in [-1, 1] {
                root.addChildNode(tubeNode(V3(s * cx, -0.3, 0.6), V3(s * cx, top, 0.35), 0.05, paint))
                root.addChildNode(tubeNode(V3(s * cx, -0.3, -0.7), V3(s * cx, top, -0.5), 0.05, paint))
                root.addChildNode(tubeNode(V3(s * cx, top, 0.35), V3(s * cx, top, -0.5), 0.05, paint))
                root.addChildNode(tubeNode(V3(s * cx, -0.3, 0.6), V3(s * cx, -0.3, -0.7), 0.05, paint))
            }
            root.addChildNode(tubeNode(V3(-cx, top, 0.35), V3(cx, top, 0.35), 0.05, paint))
            root.addChildNode(tubeNode(V3(-cx, top, -0.5), V3(cx, top, -0.5), 0.05, paint))
        }

        // Lights
        let hlM = emissive(color(1, 0.97, 0.8))
        let tlM = emissive(color(0.9, 0.05, 0.05))
        let ly: Float = m.style == .buggy ? 0.0 : 0.28
        for s: Float in [-1, 1] {
            root.addChildNode(boxNode(0.28, 0.16, 0.06, hlM, s * (hw - 0.35), ly, hl + 0.03))
            root.addChildNode(boxNode(0.22, 0.16, 0.06, tlM, s * (hw - 0.3), ly, -hl - 0.03))
        }

        // Wheels
        let tireM = solid(color(0.04, 0.04, 0.045), specular: 0.05)
        let rimM = solid(setup.tireType == .studded ? color(0.8, 0.8, 0.82) : color(0.7, 0.7, 0.72), specular: 0.9, shininess: 0.8)
        let r = setup.tireRadius, tw = setup.tireWidth
        for i in 0..<4 {
            let pivot = SCNNode()
            let spin = SCNNode()
            let tire = SCNCylinder(radius: cg(r), height: cg(tw))
            tire.radialSegmentCount = 20
            tire.materials = [tireM]
            let tn = SCNNode(geometry: tire)
            tn.eulerAngles.z = .pi / 2
            let rim = SCNCylinder(radius: cg(r * 0.58), height: cg(tw * 1.02))
            rim.radialSegmentCount = 12
            rim.materials = [rimM]
            let rn = SCNNode(geometry: rim)
            rn.eulerAngles.z = .pi / 2
            spin.addChildNode(tn)
            spin.addChildNode(rn)
            // markers so rotation is visible + tread/studs
            let knobs = setup.tireType == .road ? 0 : (setup.tireType == .balloon ? 6 : 12)
            let knobM = setup.tireType == .studded ? metal : tireM
            for k in 0..<knobs {
                let a = Float(k) / Float(knobs) * 2 * .pi
                let kh: Float = setup.tireType == .studded ? 0.06 : (setup.tireType == .mud ? 0.1 : 0.05)
                let kn = boxNode(tw * (setup.tireType == .studded ? 0.12 : 0.9), kh, r * 0.18, knobM, 0, sin(a) * (r + kh * 0.3), cos(a) * (r + kh * 0.3))
                kn.simdOrientation = simd_quatf(angle: -a, axis: V3(1, 0, 0))
                spin.addChildNode(kn)
            }
            spin.addChildNode(boxNode(tw * 1.04, r * 0.5, 0.05, metal, 0, r * 0.3, 0))
            pivot.addChildNode(spin)
            root.addChildNode(pivot)
            pivots.append(pivot)
            spins.append(spin)

            if i >= 2 {
                let ps = SCNParticleSystem()
                ps.birthRate = 0
                ps.particleLifeSpan = 1.1
                ps.particleLifeSpanVariation = 0.5
                ps.particleSize = 0.55
                ps.particleSizeVariation = 0.3
                ps.particleVelocity = 2.2
                ps.particleVelocityVariation = 1.5
                ps.spreadingAngle = 50
                ps.emittingDirection = SCNVector3(0, 1, -0.3)
                ps.acceleration = SCNVector3(0, 0.4, 0)
                ps.isAffectedByGravity = false
                ps.isLocal = false
                ps.blendMode = .alpha
                ps.particleImage = softDotImage
                ps.particleColor = color(0.7, 0.6, 0.45, 0.6)
                ps.sortingMode = .distance
                let c = SCNParticlePropertyController(animation: {
                    let a = CAKeyframeAnimation()
                    a.values = [0.0, 0.6, 0.0]
                    a.keyTimes = [0, 0.25, 1]
                    return a
                }())
                ps.propertyControllers = [.opacity: c]
                let dn = SCNNode()
                dn.simdPosition = V3(0, -r + 0.1, 0)
                dn.addParticleSystem(ps)
                pivot.addChildNode(dn)
                dust.append(ps)
            }
        }
    }

    func update(_ v: Vehicle, throttle: Float) {
        root.simdPosition = v.pos
        root.simdOrientation = v.rot
        let sp = v.spec
        for i in 0..<4 {
            let w = v.wheels[i]
            let sx: Float = w.local.x > 0 ? 1 : -1
            pivots[i].simdPosition = V3(sx * wheelX, w.local.y - w.length, w.local.z)
            pivots[i].eulerAngles.y = w.front ? cg(v.steerAngle) : 0
            spins[i].eulerAngles.x = cg(w.spin)
        }
        let speed = simd_length(v.vel)
        for (k, ps) in dust.enumerated() {
            let w = v.wheels[2 + k]
            var rate: Float = 0
            var col = color(0.7, 0.6, 0.45, 0.5)
            var size: Float = 0.5
            if w.contact {
                switch w.surface.kind {
                case .sand: col = color(0.85, 0.75, 0.5, 0.6); rate = speed * 9; size = 0.7
                case .dirt: col = color(0.6, 0.5, 0.35, 0.5); rate = speed * 6
                case .mud: col = color(0.22, 0.15, 0.09, 0.85); rate = speed * 7; size = 0.4
                case .snow: col = color(0.95, 0.97, 1, 0.75); rate = speed * 10; size = 0.6
                case .water: col = color(0.85, 0.92, 1, 0.6); rate = speed * 14; size = 0.8
                case .grass: col = color(0.45, 0.5, 0.35, 0.4); rate = speed * 1.5
                case .rock: col = color(0.6, 0.6, 0.6, 0.4); rate = speed * 2
                }
                rate += abs(throttle) * 20 * (speed < 6 ? 1 : 0)
            }
            ps.birthRate = CGFloat(min(rate, 220))
            ps.particleColor = col
            ps.particleSize = cg(size)
        }
        _ = sp
    }
}

// MARK: - Weather shader (snow cover + wet ground), shared by terrain / props

let weatherShader = """
#pragma arguments
float snow;
float wet;
#pragma body
float3 wn = normalize((scn_frame.inverseViewTransform * float4(_surface.normal, 0.0)).xyz);
float3 wp = (scn_frame.inverseViewTransform * float4(_surface.position, 1.0)).xyz;
float up = smoothstep(0.55, 0.85, wn.y);
float s = snow * up;
float low = smoothstep(10.0, 3.0, wp.y);
float m = wet * low;
float3 c = _surface.diffuse.rgb;
c = mix(c, c * float3(0.55, 0.45, 0.36), m * 0.8);
c = mix(c, float3(0.93, 0.95, 1.0), s);
_surface.diffuse.rgb = c;
"""

func weatherMaterial(_ base: SCNMaterial) -> SCNMaterial {
    base.shaderModifiers = [.surface: weatherShader]
    base.setValue(NSNumber(value: 0.0), forKey: "snow")
    base.setValue(NSNumber(value: 0.0), forKey: "wet")
    return base
}

// MARK: - Terrain and props

final class WorldVisual {
    let root = SCNNode()
    var weatherMats: [SCNMaterial] = []
    let terrainMat: SCNMaterial

    init(world: WorldData) {
        let tm = SCNMaterial()
        tm.lightingModel = .lambert
        tm.diffuse.contents = NSColor.white
        tm.isDoubleSided = true
        terrainMat = weatherMaterial(tm)
        weatherMats.append(terrainMat)

        let trunk = weatherMaterial(solid(color(0.30, 0.20, 0.12), specular: 0.05))
        let leafA = weatherMaterial(solid(color(0.12, 0.33, 0.14), specular: 0.05))
        let leafB = weatherMaterial(solid(color(0.20, 0.38, 0.12), specular: 0.05))
        let rockM = weatherMaterial(solid(color(0.45, 0.43, 0.41), specular: 0.1))
        weatherMats += [trunk, leafA, leafB, rockM]

        let n = 10
        let size = WorldData.half * 2 / Float(n)
        for cz in 0..<n {
            for cx in 0..<n {
                let chunk = WorldVisual.makeChunk(world, cx, cz, size)
                chunk.geometry?.materials = [terrainMat]
                root.addChildNode(chunk)
                let x0 = -WorldData.half + Float(cx) * size, z0 = -WorldData.half + Float(cz) * size
                let props = SCNNode()
                for t in world.trees where t.x >= x0 && t.x < x0 + size && t.z >= z0 && t.z < z0 + size {
                    let tn = SCNNode()
                    tn.addChildNode(WorldVisual.cyl(0.22, 2.2, trunk, 0, 1.1, 0))
                    let lm = hash2(Int32(t.x * 10), Int32(t.z * 10), 3) > 0.5 ? leafA : leafB
                    tn.addChildNode(WorldVisual.cone(1.9, 3.6, lm, 0, 3.4, 0))
                    tn.addChildNode(WorldVisual.cone(1.4, 3.0, lm, 0, 5.2, 0))
                    tn.addChildNode(WorldVisual.cone(0.9, 2.4, lm, 0, 6.7, 0))
                    tn.simdPosition = V3(t.x, t.y - 0.1, t.z)
                    tn.simdScale = V3(repeating: t.scale)
                    props.addChildNode(tn)
                }
                for r in world.rocks where r.x >= x0 && r.x < x0 + size && r.z >= z0 && r.z < z0 + size {
                    props.addChildNode(WorldVisual.rock(r.scale, rockM, V3(r.x, r.y + r.scale * 0.12, r.z), r.rot))
                }
                if !props.childNodes.isEmpty && ProcessInfo.processInfo.environment["NOPROPS"] == nil { root.addChildNode(props.flattenedClone()) }
            }
        }

        let water = SCNPlane(width: cg(WorldData.half * 2 + 200), height: cg(WorldData.half * 2 + 200))
        let wm = SCNMaterial()
        wm.lightingModel = .blinn
        wm.diffuse.contents = color(0.10, 0.30, 0.42, 0.78)
        wm.specular.contents = NSColor.white
        wm.shininess = 0.9
        wm.isDoubleSided = true
        wm.writesToDepthBuffer = false
        water.materials = [wm]
        let wn = SCNNode(geometry: water)
        wn.eulerAngles.x = -.pi / 2
        wn.simdPosition = V3(0, WorldData.waterLevel - 0.02, 0)
        wn.renderingOrder = 10
        root.addChildNode(wn)
    }

    func setWeather(snow: Float, wet: Float) {
        for m in weatherMats {
            m.setValue(NSNumber(value: snow), forKey: "snow")
            m.setValue(NSNumber(value: wet), forKey: "wet")
        }
    }

    static func cyl(_ r: Float, _ h: Float, _ m: SCNMaterial, _ x: Float, _ y: Float, _ z: Float) -> SCNNode {
        let g = SCNCylinder(radius: cg(r), height: cg(h)); g.radialSegmentCount = 6; g.materials = [m]
        let n = SCNNode(geometry: g); n.simdPosition = V3(x, y, z); return n
    }
    static func cone(_ r: Float, _ h: Float, _ m: SCNMaterial, _ x: Float, _ y: Float, _ z: Float) -> SCNNode {
        let g = SCNCone(topRadius: 0, bottomRadius: cg(r), height: cg(h)); g.radialSegmentCount = 7; g.materials = [m]
        let n = SCNNode(geometry: g); n.simdPosition = V3(x, y, z); return n
    }
    static func rock(_ s: Float, _ m: SCNMaterial, _ p: V3, _ rot: Float) -> SCNNode {
        let g = SCNSphere(radius: 1); g.segmentCount = 7; g.materials = [m]
        let n = SCNNode(geometry: g)
        n.simdPosition = p
        n.simdScale = V3(s, s * 0.72, s * 0.9)
        n.eulerAngles = SCNVector3(cg(rot * 0.3), cg(rot), cg(rot * 0.2))
        return n
    }

    static func data(_ a: [Float]) -> Data { a.withUnsafeBufferPointer { Data(buffer: $0) } }

    static func makeChunk(_ world: WorldData, _ cx: Int, _ cz: Int, _ size: Float) -> SCNNode {
        let n = 40
        let s = size / Float(n)
        let x0 = -WorldData.half + Float(cx) * size
        let z0 = -WorldData.half + Float(cz) * size
        let stride = n + 3
        var hs = [Float](repeating: 0, count: stride * stride)
        for j in 0..<stride { for i in 0..<stride {
            hs[j * stride + i] = world.height(x0 + Float(i - 1) * s, z0 + Float(j - 1) * s)
        } }
        let vc = (n + 1) * (n + 1)
        var pos = [Float](); pos.reserveCapacity(vc * 3)
        var nor = [Float](); nor.reserveCapacity(vc * 3)
        var col = [Float](); col.reserveCapacity(vc * 4)
        for j in 0...n { for i in 0...n {
            let h = hs[(j + 1) * stride + (i + 1)]
            let dx = (hs[(j + 1) * stride + i + 2] - hs[(j + 1) * stride + i]) / (2 * s)
            let dz = (hs[(j + 2) * stride + i + 1] - hs[j * stride + i + 1]) / (2 * s)
            let nn = simd_normalize(V3(-dx, 1, -dz))
            let x = x0 + Float(i) * s, z = z0 + Float(j) * s
            pos += [x, h, z]
            nor += [nn.x, nn.y, nn.z]
            let c = world.color(x, z, h, nn.y)
            col += [c.x, c.y, c.z, 1]
        } }
        var idx = [UInt32](); idx.reserveCapacity(n * n * 6)
        for j in 0..<n { for i in 0..<n {
            let a = UInt32(j * (n + 1) + i), b = UInt32((j + 1) * (n + 1) + i)
            let c = a + 1, d = b + 1
            idx += [a, b, c, b, d, c]
        } }
        let src = SCNGeometrySource(data: data(pos), semantic: .vertex, vectorCount: vc, usesFloatComponents: true,
                                    componentsPerVector: 3, bytesPerComponent: 4, dataOffset: 0, dataStride: 12)
        let nsrc = SCNGeometrySource(data: data(nor), semantic: .normal, vectorCount: vc, usesFloatComponents: true,
                                     componentsPerVector: 3, bytesPerComponent: 4, dataOffset: 0, dataStride: 12)
        let csrc = SCNGeometrySource(data: data(col), semantic: .color, vectorCount: vc, usesFloatComponents: true,
                                     componentsPerVector: 4, bytesPerComponent: 4, dataOffset: 0, dataStride: 16)
        let el = SCNGeometryElement(indices: idx, primitiveType: .triangles)
        let g = SCNGeometry(sources: [src, nsrc, csrc], elements: [el])
        return SCNNode(geometry: g)
    }
}
