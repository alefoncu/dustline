import AppKit
import SceneKit
import simd

func selfTest() {
    let world = WorldData()
    print("trees \(world.trees.count) rocks \(world.rocks.count) beacons \(world.beacons.count)")
    let near = world.trees.map { ($0.x * $0.x + $0.z * $0.z).squareRoot() }.sorted().prefix(5)
    print("nearest trees to spawn:", Array(near))
    func run(_ name: String, _ s: Setup, weather: (Float, Float)? = nil) {
        if let w = weather { world.weather.wet = w.0; world.weather.snowCover = w.1 }
        let v = Vehicle(world: world, at: V3(0, world.height(0, 0) + 2, 0), setup: s)
        var c = Controls(); c.throttle = 1
        var minH: Float = 99; var maxV: Float = 0; var t: Float = 0
        let dt: Float = 1 / 120
        while t < 25 {
            if t > 14 { c.steer = 0.5 }
            v.step(dt, c); t += dt
            minH = min(minH, v.pos.y - world.height(v.pos.x, v.pos.z))
            maxV = max(maxV, simd_length(v.vel))
            if v.pos.y.isNaN { print("NaN!"); return }
        }
        print(String(format: "%-30@ hp %5.0f mass %5.0f top %4.0f km/h minH %.2f flipped %@", name as NSString,
                     v.spec.hp * (1 + 0.5 * s.turbo), v.spec.mass, maxV * 3.6, minH, v.isFlipped ? "YES" : "no"))
    }
    for i in 0..<carModels.count { run(carModels[i].name, Setup.stock(i)) }
}

func argValue(_ name: String) -> String? {
    let a = CommandLine.arguments
    if let i = a.firstIndex(of: name), i + 1 < a.count { return a[i + 1] }
    return nil
}

if CommandLine.arguments.contains("--selftest") { selfTest(); exit(0) }

final class AppDelegate: NSObject, NSApplicationDelegate {
    var window: NSWindow!
    var game: Game!
    var garage: GaragePanel!

    func applicationDidFinishLaunching(_ n: Notification) {
        let frame = NSRect(x: 0, y: 0, width: 1360, height: 840)
        window = NSWindow(contentRect: frame, styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered, defer: false)
        window.title = "Dustline"
        window.center()
        let view = GameView(frame: frame)
        view.autoresizingMask = [.width, .height]
        window.contentView = view
        window.makeKeyAndOrderFront(nil)
        window.makeFirstResponder(view)
        NSApp.activate(ignoringOtherApps: true)

        game = Game(view: view)

        garage = GaragePanel(game: game)
        garage.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(garage)
        NSLayoutConstraint.activate([
            garage.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            garage.topAnchor.constraint(equalTo: view.topAnchor),
            garage.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            garage.widthAnchor.constraint(equalToConstant: 340),
        ])
        game.garageChanged = { [weak self] open in
            DispatchQueue.main.async {
                guard let self else { return }
                self.garage.isHidden = !open
                if open { self.garage.s = self.game.setup; self.garage.sync() }
                self.window.makeFirstResponder(view)
            }
        }

        // debugging / testing options
        if let m = argValue("--model"), let i = Int(m), i < carModels.count { garage.s = Setup.stock(i); game.apply(garage.s) }
        if let w = argValue("--weather") {
            let k: WeatherKind = ["rain": .rain, "snow": .snow, "fog": .fog][w] ?? .clear
            game.weather.force(k)
            game.weather.instant = true
        }
        if CommandLine.arguments.contains("--garage") { game.garageOpen = true }
        if let path = argValue("--shot") {
            let delay = Double(argValue("--delay") ?? "6") ?? 6
            DispatchQueue.main.asyncAfter(deadline: .now() + delay) {
                let img = view.snapshot()
                if let tiff = img.tiffRepresentation, let rep = NSBitmapImageRep(data: tiff),
                   let png = rep.representation(using: .png, properties: [:]) {
                    try? png.write(to: URL(fileURLWithPath: path))
                }
                NSApp.terminate(nil)
            }
        }
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ app: NSApplication) -> Bool { true }
}

let app = NSApplication.shared
app.setActivationPolicy(.regular)
let menu = NSMenu()
let appItem = NSMenuItem()
menu.addItem(appItem)
let appMenu = NSMenu()
appMenu.addItem(withTitle: "Salir de Dustline", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
appItem.submenu = appMenu
app.mainMenu = menu
let delegate = AppDelegate()
app.delegate = delegate
app.run()
