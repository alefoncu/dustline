import Foundation
import simd

enum BodyStyle { case boxySUV, wedgeSUV, microBox, classic4x4, buggy, pickup, crawler }

enum TireType: Int, CaseIterable, Codable {
    case road, allTerrain, mud, studded, balloon
    var label: String {
        switch self {
        case .road: return "Carretera"
        case .allTerrain: return "Todo terreno"
        case .mud: return "Barro (tacos)"
        case .studded: return "Clavos (nieve/hielo)"
        case .balloon: return "Balón (arena)"
        }
    }
    /// (grip multiplier, rolling-drag multiplier) for a surface
    func response(on k: SurfaceKind) -> (mu: Float, drag: Float) {
        switch (self, k) {
        case (.road, .grass): return (0.85, 1)
        case (.road, .dirt): return (0.8, 1)
        case (.road, .sand): return (0.6, 1.3)
        case (.road, .mud): return (0.4, 1.5)
        case (.road, .snow): return (0.5, 1.1)
        case (.road, .water): return (0.6, 1)
        case (.road, .rock): return (1.0, 1)
        case (.allTerrain, .sand): return (0.9, 1)
        case (.allTerrain, .mud): return (0.8, 1)
        case (.allTerrain, .snow): return (0.8, 1)
        case (.allTerrain, _): return (1.0, 1)
        case (.mud, .grass): return (0.95, 1.05)
        case (.mud, .dirt): return (1.1, 1.05)
        case (.mud, .sand): return (1.0, 1)
        case (.mud, .mud): return (1.5, 0.6)
        case (.mud, .snow): return (0.9, 1)
        case (.mud, .water): return (1.1, 1)
        case (.mud, .rock): return (0.9, 1)
        case (.studded, .snow): return (1.9, 0.9)
        case (.studded, .mud): return (1.0, 1)
        case (.studded, .sand): return (0.85, 1)
        case (.studded, .rock): return (1.05, 1)
        case (.studded, .dirt): return (1.05, 1)
        case (.studded, _): return (1.0, 1)
        case (.balloon, .sand): return (1.5, 0.6)
        case (.balloon, .mud): return (1.1, 0.8)
        case (.balloon, .snow): return (1.2, 0.8)
        case (.balloon, .rock): return (0.8, 1)
        case (.balloon, _): return (0.9, 0.95)
        }
    }
}

enum Drive: Int, CaseIterable, Codable {
    case awd, rwd, fwd
    var label: String { ["4x4 (total)", "Trasera", "Delantera"][rawValue] }
}

struct CarModel {
    let name: String
    let style: BodyStyle
    let mass: Float           // stock, kg (incl. stock engine)
    let engineCC: Float       // stock
    let turbo: Float          // stock boost level 0...3
    let length: Float, width: Float, height: Float
    let wheelbase: Float
    let cdA: Float            // drag area
    let tireRadius: Float, tireWidth: Float
    let travel: Float         // stock suspension travel
    let stiffness: Float
    let tire: TireType
    let drive: Drive
    let colors: [(Float, Float, Float)]
    let blurb: String
}

let carModels: [CarModel] = [
    CarModel(name: "Clase G", style: .boxySUV, mass: 2450, engineCC: 4000, turbo: 1, length: 4.8, width: 1.95, height: 1.95,
             wheelbase: 2.95, cdA: 1.15, tireRadius: 0.40, tireWidth: 0.285, travel: 0.55, stiffness: 1.1,
             tire: .allTerrain, drive: .awd, colors: [(0.07, 0.07, 0.08), (0.75, 0.76, 0.78), (0.10, 0.28, 0.18)],
             blurb: "Icono cuadrado, pesado y brutal. Tres diferenciales."),
    CarModel(name: "Urus", style: .wedgeSUV, mass: 2200, engineCC: 4000, turbo: 1.6, length: 5.1, width: 2.02, height: 1.64,
             wheelbase: 3.0, cdA: 0.95, tireRadius: 0.41, tireWidth: 0.325, travel: 0.42, stiffness: 1.5,
             tire: .allTerrain, drive: .awd, colors: [(0.95, 0.75, 0.05), (0.85, 0.12, 0.10), (0.1, 0.1, 0.12)],
             blurb: "SUV deportivo: muchísima potencia, poco recorrido."),
    CarModel(name: "Panda 4x4", style: .microBox, mass: 980, engineCC: 900, turbo: 0, length: 3.7, width: 1.65, height: 1.56,
             wheelbase: 2.3, cdA: 0.8, tireRadius: 0.32, tireWidth: 0.185, travel: 0.42, stiffness: 0.8,
             tire: .allTerrain, drive: .awd, colors: [(0.95, 0.95, 0.93), (0.78, 0.12, 0.12), (0.18, 0.4, 0.7)],
             blurb: "Diminuto y ligero: sube donde nadie lo espera."),
    CarModel(name: "Defender clásico", style: .classic4x4, mass: 1850, engineCC: 2500, turbo: 0.6, length: 4.4, width: 1.8, height: 2.0,
             wheelbase: 2.79, cdA: 1.2, tireRadius: 0.42, tireWidth: 0.235, travel: 0.62, stiffness: 0.9,
             tire: .allTerrain, drive: .awd, colors: [(0.28, 0.36, 0.20), (0.82, 0.78, 0.62), (0.55, 0.12, 0.10)],
             blurb: "Eje rígido, mucho recorrido y carácter."),
    CarModel(name: "Buggy", style: .buggy, mass: 720, engineCC: 1600, turbo: 1, length: 3.3, width: 1.9, height: 1.35,
             wheelbase: 2.2, cdA: 0.7, tireRadius: 0.48, tireWidth: 0.32, travel: 0.82, stiffness: 0.85,
             tire: .balloon, drive: .rwd, colors: [(0.95, 0.4, 0.05), (0.1, 0.6, 0.9), (0.85, 0.85, 0.1)],
             blurb: "Jaula tubular y suspensión larguísima."),
    CarModel(name: "Pick-up 4x4", style: .pickup, mass: 2050, engineCC: 2800, turbo: 1, length: 5.3, width: 1.86, height: 1.85,
             wheelbase: 3.1, cdA: 1.1, tireRadius: 0.44, tireWidth: 0.265, travel: 0.6, stiffness: 1.0,
             tire: .allTerrain, drive: .awd, colors: [(0.15, 0.30, 0.62), (0.85, 0.85, 0.85), (0.55, 0.1, 0.1)],
             blurb: "Carga y tracción. Lo equilibrado."),
    CarModel(name: "Rock Crawler", style: .crawler, mass: 1500, engineCC: 3500, turbo: 0, length: 3.9, width: 1.9, height: 1.9,
             wheelbase: 2.5, cdA: 1.1, tireRadius: 0.62, tireWidth: 0.4, travel: 0.9, stiffness: 0.7,
             tire: .mud, drive: .awd, colors: [(0.85, 0.2, 0.1), (0.9, 0.85, 0.2), (0.2, 0.2, 0.22)],
             blurb: "Neumáticos gigantes y suspensión extrema."),
]

/// Everything the player can change. All continuous, nothing is locked to a tier.
struct Setup: Codable, Equatable {
    var model: Int = 0
    var color: Int = 0
    var engineCC: Float = 4000       // 600 ... 9000
    var turbo: Float = 1             // 0 ... 3
    var stiffness: Float = 1.0       // 0.4 ... 3
    var travel: Float = 0.55         // 0.25 ... 1.0 (lift / suspension travel)
    var tireRadius: Float = 0.40     // 0.2 ... 0.85
    var tireWidth: Float = 0.28      // 0.12 ... 0.55
    var tireType: TireType = .allTerrain
    var drive: Drive = .awd
    var ballast: Float = 0           // -300 ... +800 kg

    static func stock(_ i: Int) -> Setup {
        let m = carModels[i]
        return Setup(model: i, color: 0, engineCC: m.engineCC, turbo: m.turbo, stiffness: m.stiffness, travel: m.travel,
                     tireRadius: m.tireRadius, tireWidth: m.tireWidth, tireType: m.tire, drive: m.drive, ballast: 0)
    }
}

/// Derived physical parameters.
struct Spec {
    var mass: Float
    var hp: Float
    var peakHP: Float         // including full boost
    var fMax: Float
    var vMaxGear: Float
    var springK: Float
    var springC: Float
    var restLen: Float
    var wheelRadius: Float
    var tireWidth: Float
    var inertia: V3
    var halfTrack: Float
    var halfWheelbase: Float
    var halfBodyW: Float
    var halfBodyL: Float
    var bellyY: Float
    var roofY: Float
    var cdA: Float

    init(_ s: Setup) {
        let m = carModels[s.model]
        let kgPerCC: Float = 0.065
        let wheelMass = (s.tireRadius * s.tireRadius * s.tireWidth) * 520   // 4 wheels combined, rough
        let stockWheel = (m.tireRadius * m.tireRadius * m.tireWidth) * 520
        mass = max(450, m.mass + (s.engineCC - m.engineCC) * kgPerCC + (s.turbo - m.turbo) * 30 + (wheelMass - stockWheel) + s.ballast + (s.travel - m.travel) * 60)
        hp = s.engineCC * 0.065
        peakHP = hp * (1 + 0.5 * s.turbo)
        wheelRadius = s.tireRadius
        tireWidth = s.tireWidth
        fMax = peakHP * 38 * (0.46 / s.tireRadius)
        vMaxGear = 34 * (s.tireRadius / 0.46) * (1 + 0.12 * s.turbo) * (m.style == .microBox ? 0.8 : 1)
        springK = 30000 * (mass / 1500) * s.stiffness
        let crit = 2 * (springK * mass / 4).squareRoot()
        springC = crit * 0.42
        restLen = s.travel
        let L = m.length, W = m.width, H = m.height
        inertia = V3(mass / 12 * (H * H + L * L), mass / 12 * (W * W + L * L), mass / 12 * (W * W + H * H)) * V3(1.1, 1.1, 1.3)
        halfTrack = W / 2 - 0.12
        halfWheelbase = m.wheelbase / 2
        halfBodyW = W / 2 - 0.05
        halfBodyL = L / 2 - 0.1
        bellyY = -0.42
        roofY = H - 0.6
        cdA = m.cdA
    }
}
