import SceneKit
import SpriteKit
import AppKit
import simd

// MARK: - Input

final class Keys {
    private var down = Set<UInt16>()
    private let lock = NSLock()
    func set(_ k: UInt16, _ v: Bool) { lock.lock(); if v { down.insert(k) } else { down.remove(k) }; lock.unlock() }
    func isDown(_ k: UInt16...) -> Bool { lock.lock(); defer { lock.unlock() }; return k.contains(where: { down.contains($0) }) }
    func clear() { lock.lock(); down.removeAll(); lock.unlock() }
}

// MARK: - HUD

final class HUD {
    let scene: SKScene
    let speed = SKLabelNode(fontNamed: "Menlo-Bold")
    let unit = SKLabelNode(fontNamed: "Menlo")
    let info = SKLabelNode(fontNamed: "Menlo")
    let power = SKLabelNode(fontNamed: "Menlo")
    let goal = SKLabelNode(fontNamed: "Menlo-Bold")
    let arrow = SKLabelNode(fontNamed: "Menlo-Bold")
    let toast = SKLabelNode(fontNamed: "Menlo-Bold")
    let hint = SKLabelNode(fontNamed: "Menlo")
    private var toastTime: Float = 0

    init(size: CGSize) {
        scene = SKScene(size: size)
        scene.scaleMode = .resizeFill
        scene.backgroundColor = .clear
        func style(_ l: SKLabelNode, _ s: CGFloat, _ h: SKLabelHorizontalAlignmentMode) {
            l.fontSize = s; l.fontColor = .white; l.horizontalAlignmentMode = h; l.verticalAlignmentMode = .baseline
            scene.addChild(l)
        }
        style(speed, 54, .left); style(unit, 16, .left); style(info, 14, .left); style(power, 13, .left)
        style(goal, 18, .right); style(arrow, 30, .center); style(toast, 24, .center); style(hint, 12, .center)
        hint.fontColor = NSColor(white: 1, alpha: 0.75)
        unit.text = "km/h"
        hint.text = "W/S acelerar · A/D girar · Espacio freno · Shift turbo · R enderezar · G garaje · 1-4 clima · C cámara"
        layout(size)
    }

    func layout(_ size: CGSize) {
        speed.position = CGPoint(x: 28, y: 56)
        unit.position = CGPoint(x: 30, y: 34)
        power.position = CGPoint(x: 30, y: 14)
        info.position = CGPoint(x: 28, y: size.height - 30)
        goal.position = CGPoint(x: size.width - 28, y: size.height - 30)
        arrow.position = CGPoint(x: size.width / 2, y: size.height - 56)
        toast.position = CGPoint(x: size.width / 2, y: size.height - 110)
        hint.position = CGPoint(x: size.width - 24, y: 10); hint.horizontalAlignmentMode = .right
    }

    func show(_ text: String, seconds: Float = 3) { toast.text = text; toast.alpha = 1; toastTime = seconds }
    func tick(_ dt: Float) {
        if toastTime > 0 { toastTime -= dt; if toastTime < 1 { toast.alpha = CGFloat(max(0, toastTime)) } }
    }
}

// MARK: - View

final class GameView: SCNView {
    var keys = Keys()
    var onKeyDown: ((UInt16) -> Void)?
    override var acceptsFirstResponder: Bool { true }
    override func keyDown(with e: NSEvent) {
        if !e.isARepeat { onKeyDown?(e.keyCode) }
        keys.set(e.keyCode, true)
    }
    override func keyUp(with e: NSEvent) { keys.set(e.keyCode, false) }
    override func flagsChanged(with e: NSEvent) { keys.set(56, e.modifierFlags.contains(.shift)) }
    override func resignFirstResponder() -> Bool { keys.clear(); return true }
    override func layout() {
        super.layout()
        if let s = overlaySKScene { s.size = bounds.size }
    }
}

// MARK: - Game

final class Game: NSObject, SCNSceneRendererDelegate {
    let view: GameView
    let scene = SCNScene()
    let world = WorldData()
    let visual: WorldVisual
    let hud: HUD
    let weather: WeatherSystem
    var vehicle: Vehicle
    var car: CarVisual
    let camNode = SCNNode()
    let sunRig = SCNNode()
    var beaconNodes: [SCNNode] = []
    var collected = Set<Int>()

    var garageOpen = false { didSet { garageChanged?(garageOpen) } }
    var garageChanged: ((Bool) -> Void)?
    var setup: Setup

    private var pending: [() -> Void] = []
    private let lock = NSLock()
    private var lastTime: Double = 0
    private var camYaw: Float = 0
    private var camDistMode = 0
    private var hudTimer: Float = 0
    private var clock: Float = 0
    private var runTimer: Float = 0
    private var started = false
    private var finished = false

    // dynamic obstacles
    private var obstacleNodes: [Int: SCNNode] = [:]
    private var obstacleBirth: [Int: Float] = [:]
    private var pendingActive: [(id: Int, at: Float)] = []
    private var nextObstacleId = 0
    private var spawnTimer: Float = 6
    private var rng = SplitMix64(7)
    private let rockMat = weatherMaterial(solid(color(0.46, 0.44, 0.42), specular: 0.1))
    private let woodMat = weatherMaterial(solid(color(0.36, 0.24, 0.14), specular: 0.05))
    private let crateMat = weatherMaterial(solid(color(0.62, 0.45, 0.22), specular: 0.1))
    private let obstacleRoot = SCNNode()

    init(view: GameView) {
        self.view = view
        setup = Game.loadSetup()
        visual = WorldVisual(world: world)
        hud = HUD(size: view.bounds.size)
        let sh = world.height(0, 0)
        vehicle = Vehicle(world: world, at: V3(0, sh + 3, 0), setup: setup)
        car = CarVisual(setup: setup)

        // lights
        let sun = SCNNode()
        let l = SCNLight()
        l.type = .directional
        l.intensity = 1000
        l.castsShadow = true
        l.shadowMode = .deferred
        l.shadowSampleCount = 4
        l.shadowRadius = 3
        l.shadowColor = NSColor(white: 0, alpha: 0.45)
        l.automaticallyAdjustsShadowProjection = true
        l.maximumShadowDistance = 90
        sun.light = l
        sun.eulerAngles = SCNVector3(-0.95, 0.6, 0)
        sunRig.addChildNode(sun)
        let amb = SCNNode()
        amb.light = SCNLight()
        amb.light!.type = .ambient
        amb.light!.intensity = 380
        scene.rootNode.addChildNode(sunRig)
        scene.rootNode.addChildNode(amb)
        scene.rootNode.addChildNode(visual.root)
        scene.rootNode.addChildNode(obstacleRoot)
        scene.rootNode.addChildNode(car.root)

        let cam = SCNCamera()
        cam.zNear = 0.3
        cam.zFar = 900
        cam.fieldOfView = 62
        camNode.camera = cam
        scene.rootNode.addChildNode(camNode)

        weather = WeatherSystem(world: world, scene: scene, sun: sun, ambient: amb)
        super.init()

        // beacons
        for (i, b) in world.beacons.enumerated() {
            let n = SCNNode()
            let beam = SCNCylinder(radius: 0.35, height: 40)
            beam.materials = [emissive(color(1, 0.55, 0.1, 0.55))]
            let bn = SCNNode(geometry: beam)
            bn.simdPosition = V3(0, 20, 0)
            n.addChildNode(bn)
            let ring = SCNTorus(ringRadius: 2, pipeRadius: 0.2)
            ring.materials = [emissive(color(1, 0.7, 0.2))]
            let rn = SCNNode(geometry: ring)
            rn.simdPosition = V3(0, 2, 0)
            rn.runAction(.repeatForever(.rotateBy(x: 0, y: .pi * 2, z: 0, duration: 4)))
            n.addChildNode(rn)
            n.simdPosition = b
            n.name = "beacon\(i)"
            scene.rootNode.addChildNode(n)
            beaconNodes.append(n)
        }

        view.scene = scene
        view.pointOfView = camNode
        view.delegate = self
        view.overlaySKScene = hud.scene
        view.antialiasingMode = .multisampling4X
        view.preferredFramesPerSecond = 60
        view.isPlaying = true
        view.rendersContinuously = true
        view.backgroundColor = NSColor(srgbRed: 0.55, green: 0.75, blue: 0.95, alpha: 1)
        view.onKeyDown = { [weak self] k in self?.keyPressed(k) }
        hud.show("Encuentra las 25 balizas naranjas · G abre el garaje", seconds: 6)
    }

    // MARK: setup persistence

    static func loadSetup() -> Setup {
        let d = UserDefaults.standard
        let last = d.integer(forKey: "dustline.lastModel")
        if let data = d.data(forKey: "dustline.setup.\(last)"), let s = try? JSONDecoder().decode(Setup.self, from: data) { return s }
        return Setup.stock(min(max(last, 0), carModels.count - 1))
    }
    static func savedSetup(_ model: Int) -> Setup {
        if let data = UserDefaults.standard.data(forKey: "dustline.setup.\(model)"), let s = try? JSONDecoder().decode(Setup.self, from: data) { return s }
        return Setup.stock(model)
    }
    func persist() {
        let d = UserDefaults.standard
        d.set(setup.model, forKey: "dustline.lastModel")
        if let data = try? JSONEncoder().encode(setup) { d.set(data, forKey: "dustline.setup.\(setup.model)") }
    }

    func enqueue(_ f: @escaping () -> Void) { lock.lock(); pending.append(f); lock.unlock() }

    func apply(_ s: Setup) {
        enqueue { [self] in
            setup = s
            persist()
            vehicle.configure(s)
            car.root.removeFromParentNode()
            car = CarVisual(setup: s)
            scene.rootNode.addChildNode(car.root)
            let g = world.height(vehicle.pos.x, vehicle.pos.z)
            vehicle.pos.y = max(vehicle.pos.y, g + s.travel + s.tireRadius + 0.9)
            vehicle.vel = .zero
            vehicle.angVel = .zero
            vehicle.rot = simd_quatf(angle: atan2(vehicle.rot.act(V3(0, 0, 1)).x, vehicle.rot.act(V3(0, 0, 1)).z), axis: V3(0, 1, 0))
        }
    }

    func keyPressed(_ k: UInt16) {
        switch k {
        case 5, 48: garageOpen.toggle()             // G, Tab
        case 15: enqueue { [self] in vehicle.reset() }
        case 8: camDistMode = (camDistMode + 1) % 3
        case 18: weather.force(.clear); hud.show("Clima: despejado")
        case 19: weather.force(.rain); hud.show("Clima: lluvia")
        case 20: weather.force(.snow); hud.show("Clima: nevada")
        case 21: weather.force(.fog); hud.show("Clima: niebla")
        case 29: weather.setAuto(); hud.show("Clima automático")
        default: break
        }
    }

    // MARK: loop

    func renderer(_ renderer: SCNSceneRenderer, updateAtTime time: TimeInterval) {
        if lastTime == 0 { lastTime = time }
        let dt = Float(min(time - lastTime, 0.05))
        lastTime = time

        lock.lock(); let todo = pending; pending.removeAll(); lock.unlock()
        for f in todo { f() }

        clock += dt
        let keys = view.keys

        if !garageOpen {
            var c = Controls()
            c.throttle = (keys.isDown(13, 126) ? 1 : 0) - (keys.isDown(1, 125) ? 1 : 0)
            c.steer = (keys.isDown(0, 123) ? 1 : 0) - (keys.isDown(2, 124) ? 1 : 0)
            c.handbrake = keys.isDown(49)
            c.boost = keys.isDown(56)
            var left = dt
            let h: Float = 1 / 120
            while left > 1e-6 {
                let s = min(h, left)
                vehicle.step(s, c)
                left -= s
            }
            if vehicle.pos.y < -30 || vehicle.pos.y.isNaN { vehicle.pos = V3(0, world.height(0, 0) + 3, 0); vehicle.reset() }
            car.update(vehicle, throttle: c.throttle)
            if !started && simd_length(vehicle.vel) > 1 { started = true }
            if started && !finished { runTimer += dt }
        } else {
            car.update(vehicle, throttle: 0)
        }

        weather.update(dt: dt, center: vehicle.pos)
        visual.setWeather(snow: world.weather.snowCover, wet: world.weather.wet)
        for m in [rockMat, woodMat, crateMat] {
            m.setValue(NSNumber(value: world.weather.snowCover), forKey: "snow")
            m.setValue(NSNumber(value: world.weather.wet), forKey: "wet")
        }
        sunRig.simdPosition = vehicle.pos
        updateObstacles(dt)
        updateBeacons()
        updateCamera(dt)
        updateHUD(dt)
    }

    // MARK: camera

    private func updateCamera(_ dt: Float) {
        let m = carModels[setup.model]
        let f = vehicle.rot.act(V3(0, 0, 1))
        var targetYaw = atan2(f.x, f.z)
        var dist = m.length * 0.9 + 3.6 + Float(camDistMode) * 3
        var height = m.height + 1.3 + Float(camDistMode) * 0.8
        if garageOpen {
            camYaw += dt * 0.45
            targetYaw = camYaw
            dist = m.length * 0.95 + 3.2
            height = m.height * 0.8 + 0.8
        } else {
            var d = targetYaw - camYaw
            while d > .pi { d -= 2 * .pi }
            while d < -.pi { d += 2 * .pi }
            camYaw += d * (1 - exp(-3.5 * dt))
        }
        let off = V3(-sin(camYaw), 0, -cos(camYaw)) * dist
        var p = vehicle.pos + off + V3(0, height, 0)
        p.y = max(p.y, world.height(p.x, p.z) + 1.2)
        camNode.simdPosition = p
        let look = vehicle.pos + V3(0, m.height * 0.45, 0)
        camNode.simdLook(at: look, up: V3(0, 1, 0), localFront: V3(0, 0, -1))
    }

    // MARK: beacons

    private func updateBeacons() {
        for (i, b) in world.beacons.enumerated() where !collected.contains(i) {
            let d = simd_distance(V3(b.x, 0, b.z), V3(vehicle.pos.x, 0, vehicle.pos.z))
            if d < 7 {
                collected.insert(i)
                beaconNodes[i].isHidden = true
                if collected.count == world.beacons.count {
                    finished = true
                    hud.show(String(format: "¡Completado! %d:%02d", Int(runTimer) / 60, Int(runTimer) % 60), seconds: 12)
                } else {
                    hud.show("Baliza \(collected.count)/\(world.beacons.count)", seconds: 2)
                }
            }
        }
    }

    // MARK: dynamic obstacles

    private func updateObstacles(_ dt: Float) {
        // landing activation
        pendingActive.removeAll { p in
            if clock >= p.at {
                for i in world.dynamicObstacles.indices where world.dynamicObstacles[i].id == p.id { world.dynamicObstacles[i].active = true }
                return true
            }
            return false
        }
        // despawn far / old
        let p2 = V2(vehicle.pos.x, vehicle.pos.z)
        for (id, node) in obstacleNodes {
            let q = node.simdPosition
            let far = simd_distance(V2(q.x, q.z), p2) > 240
            let old = clock - (obstacleBirth[id] ?? clock) > 240
            if far || old {
                node.removeFromParentNode()
                obstacleNodes[id] = nil
                world.dynamicObstacles.removeAll { $0.id == id }
            }
        }
        spawnTimer -= dt
        if spawnTimer <= 0 && !garageOpen && obstacleNodes.count < 48 {
            spawnTimer = rng.range(1.8, 4.2)
            spawnObstacle()
        }
    }

    private func spawnObstacle() {
        let f = vehicle.rot.act(V3(0, 0, 1))
        var heading = atan2(f.x, f.z)
        if simd_length(vehicle.vel) > 4 { heading = atan2(vehicle.vel.x, vehicle.vel.z) }
        let a = heading + rng.range(-0.9, 0.9)
        let d = rng.range(45, 130)
        let x = vehicle.pos.x + sin(a) * d, z = vehicle.pos.z + cos(a) * d
        if abs(x) > 460 || abs(z) > 460 { return }
        let h = world.height(x, z)
        if h < 0.6 || world.normal(x, z).y < 0.8 { return }
        let id = nextObstacleId; nextObstacleId += 1
        let node = SCNNode()
        let kind = rng.float()
        var landing = false
        var circles: [(Float, Float, Float)] = []
        var label = ""
        if kind < 0.4 {                        // boulder falling from the sky
            let s = rng.range(1.3, 2.6)
            node.addChildNode(WorldVisual.rock(s, rockMat, V3(0, s * 0.55, 0), rng.range(0, 6)))
            circles = [(0, 0, s * 0.95)]
            landing = true
            label = "¡Roca cayendo al frente!"
        } else if kind < 0.65 {                 // fallen log
            let g = SCNCylinder(radius: 0.55, height: 6.5); g.radialSegmentCount = 8; g.materials = [woodMat]
            let ln = SCNNode(geometry: g)
            ln.eulerAngles = SCNVector3(0, 0, CGFloat.pi / 2)
            ln.simdPosition = V3(0, 0.55, 0)
            let holder = SCNNode(); holder.addChildNode(ln)
            let yaw = rng.range(0, .pi)
            holder.eulerAngles.y = cg(yaw)
            node.addChildNode(holder)
            let dx = cos(yaw) * 1.9, dz = -sin(yaw) * 1.9
            circles = [(dx, dz, 1.0), (-dx, -dz, 1.0), (0, 0, 1.0)]
            landing = rng.float() < 0.5
            if landing { label = "¡Se cae un tronco!" }
        } else if kind < 0.85 {                 // crates
            for (ox, oz) in [(0.0, 0.0), (1.9, 0.3), (0.9, 1.7)] {
                let c = boxNode(1.7, 1.7, 1.7, crateMat, Float(ox), 0.85, Float(oz), chamfer: 0.05)
                node.addChildNode(c)
            }
            circles = [(0, 0, 1.2), (1.9, 0.3, 1.2), (0.9, 1.7, 1.2)]
        } else {                                // rock cluster
            for k in 0..<3 {
                let s = rng.range(1.0, 1.9)
                let ox = Float(k) * 2.0 - 2.0, oz = rng.range(-0.8, 0.8)
                node.addChildNode(WorldVisual.rock(s, rockMat, V3(ox, s * 0.5, oz), rng.range(0, 6)))
                circles.append((ox, oz, s * 0.9))
            }
        }
        node.simdPosition = V3(x, h - 0.05, z)
        obstacleRoot.addChildNode(node)
        obstacleNodes[id] = node
        obstacleBirth[id] = clock
        var activeAt: Float = clock
        if landing {
            node.simdPosition.y = h + 38
            let fall = SCNAction.move(to: SCNVector3(x, h - 0.05, z), duration: 1.5)
            fall.timingMode = .easeIn
            node.runAction(fall)
            activeAt = clock + 1.5
            if d < 110 { hud.show(label, seconds: 2.5) }
        }
        for c in circles {
            world.dynamicObstacles.append(Obstacle(x: x + c.0, z: z + c.1, r: c.2, id: id, active: !landing))
        }
        if landing { pendingActive.append((id, activeAt)) }
    }

    // MARK: HUD

    private func updateHUD(_ dt: Float) {
        hud.tick(dt)
        hudTimer -= dt
        if hudTimer > 0 { return }
        hudTimer = 0.08
        let kmh = Int(simd_length(vehicle.vel) * 3.6)
        hud.speed.text = String(kmh)
        let sp = vehicle.spec
        let boost = Int(vehicle.turboSpool * 100)
        hud.power.text = String(format: "%@ · %.0f CV · %.0f kg · turbo %d%%", carModels[setup.model].name, vehicle.currentHP, sp.mass, boost)
        let w = world.weather
        hud.info.text = "Clima: \(w.name)   ·   Terreno: \(vehicle.currentSurface.kind.rawValue)   ·   \(Int(w.snowCover * 100))% nieve · \(Int(w.wet * 100))% mojado"
        // nearest beacon
        var best: (Float, V3)? = nil
        for (i, b) in world.beacons.enumerated() where !collected.contains(i) {
            let d = simd_distance(V3(b.x, 0, b.z), V3(vehicle.pos.x, 0, vehicle.pos.z))
            if best == nil || d < best!.0 { best = (d, b) }
        }
        let t = Int(runTimer)
        hud.goal.text = "Balizas \(collected.count)/\(world.beacons.count)   \(t / 60):" + String(format: "%02d", t % 60)
        if let b = best {
            let f = vehicle.rot.act(V3(0, 0, 1))
            let carYaw = atan2(f.x, f.z)
            let to = atan2(b.1.x - vehicle.pos.x, b.1.z - vehicle.pos.z)
            var rel = to - carYaw
            while rel > .pi { rel -= 2 * .pi }
            while rel < -.pi { rel += 2 * .pi }
            // rel > 0 means target is to the left
            let arrows = ["↑", "↖", "←", "↙", "↓", "↘", "→", "↗"]
            let idx = Int((rel / (.pi / 4)).rounded())
            hud.arrow.text = arrows[((idx % 8) + 8) % 8] + " \(Int(b.0)) m"
        } else {
            hud.arrow.text = ""
        }
    }
}

typealias V2 = SIMD2<Float>
