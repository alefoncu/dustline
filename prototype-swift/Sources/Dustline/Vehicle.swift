import Foundation
import simd

struct Controls {
    var throttle: Float = 0   // -1...1
    var steer: Float = 0      // -1 (right) ... 1 (left)
    var brake = false
    var handbrake = false
    var boost = false
}

struct WheelState {
    var local: V3
    var front: Bool
    var length: Float = 0.6   // current suspension length
    var contact = false
    var spin: Float = 0
    var surface = Surface(kind: .grass, mu: 1, drag: 50)
}

/// Raycast-suspension rigid body on the heightfield. Own physics, parameterised by Setup/Spec.
final class Vehicle {
    let world: WorldData
    var pos: V3
    var vel = V3.zero
    var rot = simd_quatf(ix: 0, iy: 0, iz: 0, r: 1)
    var angVel = V3.zero

    private(set) var setup: Setup
    private(set) var spec: Spec

    var wheels: [WheelState] = []
    var steerAngle: Float = 0
    var forwardSpeed: Float = 0
    var groundedWheels = 0
    var currentSurface = Surface(kind: .grass, mu: 1, drag: 50)
    var turboSpool: Float = 0
    var minClearance: Float = 99
    var bodyHits: Float = 0          // impact intensity for effects

    private var corners: [V3] = []

    init(world: WorldData, at p: V3, setup: Setup) {
        self.world = world
        pos = p
        self.setup = setup
        spec = Spec(setup)
        rebuildGeometry()
    }

    func configure(_ s: Setup) {
        setup = s
        spec = Spec(s)
        rebuildGeometry()
    }

    private func rebuildGeometry() {
        let sp = spec
        let old = wheels
        wheels = [
            WheelState(local: V3(sp.halfTrack, -0.15, sp.halfWheelbase), front: true),
            WheelState(local: V3(-sp.halfTrack, -0.15, sp.halfWheelbase), front: true),
            WheelState(local: V3(sp.halfTrack, -0.15, -sp.halfWheelbase), front: false),
            WheelState(local: V3(-sp.halfTrack, -0.15, -sp.halfWheelbase), front: false),
        ]
        for i in 0..<min(old.count, 4) { wheels[i].spin = old[i].spin; wheels[i].length = min(old[i].length, sp.restLen) }
        corners = []
        for sx: Float in [-1, 1] { for sy in [sp.bellyY, sp.roofY] { for sz: Float in [-1, 1] {
            corners.append(V3(sx * sp.halfBodyW, sy, sz * sp.halfBodyL))
        } } }
    }

    func reset(yaw: Float? = nil) {
        let f = rot.act(V3(0, 0, 1))
        let y = yaw ?? atan2(f.x, f.z)
        rot = simd_quatf(angle: y, axis: V3(0, 1, 0))
        pos.y = world.height(pos.x, pos.z) + spec.restLen + spec.wheelRadius + 1.2
        vel = .zero
        angVel = .zero
    }

    var isFlipped: Bool { rot.act(V3(0, 1, 0)).y < 0.2 }
    var currentHP: Float { spec.hp * (1 + 0.5 * setup.turbo * turboSpool) }

    func step(_ dt: Float, _ c: Controls) {
        let sp = spec
        var F = V3.zero
        var T = V3.zero
        let mass = sp.mass

        let fwd = rot.act(V3(0, 0, 1))
        let vLong = simd_dot(vel, fwd)
        forwardSpeed = vLong

        let maxSteer = 0.58 / (1 + abs(vLong) * 0.05)
        steerAngle += (c.steer * maxSteer - steerAngle) * min(1, dt * 7)

        var throttle = c.throttle
        var braking = c.brake
        if throttle * vLong < -1.5 { braking = true; throttle = 0 }

        // Turbo spool: builds under throttle (and boost key), decays when lifting
        let want: Float = (throttle > 0 || c.boost) ? 1 : 0
        turboSpool += (want > turboSpool ? 0.9 : -1.6) * dt * (want > turboSpool ? 1 : 1)
        turboSpool = clamp(turboSpool, 0, 1)
        let hpNow = sp.hp * (1 + 0.5 * setup.turbo * turboSpool) * (c.boost ? 1.25 : 1)
        let powerW = hpNow * 745.7

        groundedWheels = 0
        let widthFactor = clamp(0.85 + (sp.tireWidth - 0.2) * 2, 0.8, 1.3)
        let dragWidth = clamp(0.25 / sp.tireWidth, 0.7, 1.3)

        for i in 0..<4 {
            let w = wheels[i]
            let r = rot.act(w.local)
            let a = pos + r
            let h = world.height(a.x, a.z)
            let n = world.normal(a.x, a.z)
            let dist = (a.y - h) * n.y
            let L = dist - sp.wheelRadius
            wheels[i].length = clamp(L, 0.1, sp.restLen)

            if L >= sp.restLen { wheels[i].contact = false; continue }
            wheels[i].contact = true
            groundedWheels += 1

            var surf = world.surface(a.x, a.z, h, ny: n.y)
            let resp = setup.tireType.response(on: surf.kind)
            let soft = surf.kind == .mud || surf.kind == .sand || surf.kind == .snow || surf.kind == .water
            surf.mu *= resp.mu * (soft ? widthFactor : 1)
            surf.drag *= resp.drag * (soft ? dragWidth : 1) * (0.46 / sp.wheelRadius).squareRoot()
            wheels[i].surface = surf

            let comp = sp.restLen - L
            let vp = vel + simd_cross(angVel, r)
            let vn = simd_dot(vp, n)
            var fs = sp.springK * comp - sp.springC * vn
            if L < 0.1 { fs += 6 * sp.springK * (0.1 - L) }
            fs = max(fs, 0)
            let Fs = n * fs
            F += Fs
            T += simd_cross(r, Fs)

            var wf = fwd
            if w.front { wf = rot.act(V3(sin(steerAngle), 0, cos(steerAngle))) }
            wf = simd_normalize(wf - n * simd_dot(wf, n))
            let lat = simd_cross(n, wf)
            let vl = simd_dot(vp, wf)
            let vs = simd_dot(vp, lat)

            let N = fs
            let mu = max(surf.mu, 0.05)
            let rearHandbrake = c.handbrake && !w.front
            let fyMu = rearHandbrake ? mu * 0.4 : mu
            let eff = mass / 4

            // Drive share
            var share: Float = 0
            switch setup.drive {
            case .awd: share = 0.25
            case .rwd: share = w.front ? 0 : 0.5
            case .fwd: share = w.front ? 0.5 : 0
            }

            var fx: Float = 0
            if throttle != 0 && share > 0 {
                let maxV = sp.vMaxGear * (throttle > 0 ? 1 : 0.32) * (c.boost ? 1.15 : 1)
                let along = vl * (throttle > 0 ? 1 : -1)
                let ratio = along > 0 ? max(0, 1 - pow(along / maxV, 4)) : 1
                let fPower = powerW / max(abs(vl), 2.5)
                let fBase = min(sp.fMax * (1 + 0.5 * setup.turbo * turboSpool), fPower)
                fx = throttle * fBase * share * ratio
            } else if throttle == 0 && !braking {
                fx = -vl * (abs(vl) < 2 ? eff * 14 : eff * 0.25)
            } else if throttle != 0 {
                fx = 0
            }
            if braking || rearHandbrake {
                let maxB = (rearHandbrake && !braking ? 1.0 : 0.9) * mu * N
                fx += -clamp(vl * eff * 12, -maxB, maxB)
            }
            // rolling resistance (dry) + viscous (water/mud)
            let crr = surf.drag * 0.0006
            fx += -tanh(vl * 2) * crr * N
            if surf.kind == .water { fx += -vl * 450 }
            if surf.kind == .mud { fx += -vl * 80 }

            var fy = -vs * eff * 14
            fy = clamp(fy, -fyMu * N, fyMu * N)

            let maxF = mu * N * 1.2
            fx = clamp(fx, -maxF, maxF)
            let mag = (fx * fx + fy * fy).squareRoot()
            if mag > maxF { let s = maxF / mag; fx *= s; fy *= s }

            let Ft = wf * fx + lat * fy
            F += Ft
            T += simd_cross(V3(r.x, 0, r.z), Ft)   // arcade: apply tyre forces at COM height

            wheels[i].spin += (vl / sp.wheelRadius) * dt
        }
        if groundedWheels > 0 {
            var best = wheels.first(where: { $0.contact })!.surface
            for w in wheels where w.contact && w.surface.drag > best.drag { best = w.surface }
            currentSurface = best
        }

        // Body corners
        var clearance: Float = 99
        var hit: Float = 0
        for cl in corners {
            let r = rot.act(cl)
            let a = pos + r
            let h = world.height(a.x, a.z)
            clearance = min(clearance, a.y - h)
            let pen = h - a.y
            if pen > 0 {
                let n = world.normal(a.x, a.z)
                let vp = vel + simd_cross(angVel, r)
                let vn = simd_dot(vp, n)
                let k = 70000 * (mass / 1500)
                let fs = max(0, k * pen - 3500 * (mass / 1500) * vn)
                var Fc = n * fs
                let vt = vp - n * vn
                Fc += -vt * 600 * (mass / 1500) * min(1, pen * 4 + 0.2)
                F += Fc
                T += simd_cross(r, Fc)
                hit = max(hit, -vn)
            }
        }
        minClearance = clearance
        bodyHits = hit

        // Aero + water
        F += -vel * simd_length(vel) * 0.6 * sp.cdA
        let ground = world.height(pos.x, pos.z)
        let depth = clamp(WorldData.waterLevel - (pos.y - 0.6), 0, 1.6)
        if depth > 0 && ground < WorldData.waterLevel {
            F += -vel * 700 * (mass / 1500) * depth
            F += V3(0, 3500 * (mass / 1500) * depth, 0)
        }

        F += V3(0, -9.81 * mass, 0)
        vel += F / mass * dt

        T += -angVel * 1800 * (mass / 1500)
        let Tl = rot.inverse.act(T)
        var wl = rot.inverse.act(angVel)
        wl += Tl / sp.inertia * dt
        angVel = rot.act(wl)

        pos += vel * dt
        let wlen = simd_length(angVel)
        if wlen > 1e-5 {
            let dq = simd_quatf(angle: wlen * dt, axis: angVel / wlen)
            rot = simd_normalize(dq * rot)
        }

        // Obstacles
        let reach = max(sp.halfBodyL - 0.5, 0.3)
        let rad = sp.halfBodyW + 0.1
        for off in [Float(0), reach, -reach] {
            let p = pos + rot.act(V3(0, 0, off))
            world.forEachObstacle(near: p.x, p.z) { o in
                let dx = p.x - o.x, dz = p.z - o.z
                let minD = o.r + rad
                let d2 = dx * dx + dz * dz
                if d2 < minD * minD {
                    let d = max(d2.squareRoot(), 0.001)
                    let nx = dx / d, nz = dz / d
                    let push = minD - d
                    pos.x += nx * push
                    pos.z += nz * push
                    let vn = vel.x * nx + vel.z * nz
                    if vn < 0 {
                        vel.x -= vn * nx * 1.25
                        vel.z -= vn * nz * 1.25
                        bodyHits = max(bodyHits, -vn)
                    }
                }
            }
        }

        let lim = WorldData.half - 8
        if pos.x > lim { pos.x = lim; vel.x = min(vel.x, 0) }
        if pos.x < -lim { pos.x = -lim; vel.x = max(vel.x, 0) }
        if pos.z > lim { pos.z = lim; vel.z = min(vel.z, 0) }
        if pos.z < -lim { pos.z = -lim; vel.z = max(vel.z, 0) }
    }
}
