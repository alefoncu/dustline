import AppKit

final class SliderRow: NSStackView {
    let titleLabel = NSTextField(labelWithString: "")
    let valueLabel = NSTextField(labelWithString: "")
    let slider = NSSlider()
    var onChange: ((Float) -> Void)?
    var format: (Float) -> String

    init(title: String, min: Float, max: Float, format: @escaping (Float) -> String) {
        self.format = format
        super.init(frame: .zero)
        orientation = .vertical
        spacing = 1
        alignment = .leading
        titleLabel.stringValue = title
        titleLabel.font = .systemFont(ofSize: 11, weight: .semibold)
        valueLabel.font = .monospacedSystemFont(ofSize: 11, weight: .regular)
        valueLabel.alignment = .right
        let top = NSStackView(views: [titleLabel, valueLabel])
        top.distribution = .fill
        slider.minValue = Double(min)
        slider.maxValue = Double(max)
        slider.target = self
        slider.action = #selector(changed)
        slider.isContinuous = true
        slider.refusesFirstResponder = true
        addArrangedSubview(top)
        addArrangedSubview(slider)
        top.widthAnchor.constraint(equalTo: widthAnchor).isActive = true
        slider.widthAnchor.constraint(equalTo: widthAnchor).isActive = true
    }
    required init?(coder: NSCoder) { fatalError() }

    func set(_ v: Float) { slider.floatValue = v; valueLabel.stringValue = format(v) }
    @objc func changed() {
        let v = slider.floatValue
        valueLabel.stringValue = format(v)
        onChange?(v)
    }
}

final class PopupRow: NSStackView {
    let popup = NSPopUpButton()
    var onChange: ((Int) -> Void)?
    init(title: String, items: [String]) {
        super.init(frame: .zero)
        orientation = .vertical
        spacing = 2
        alignment = .leading
        let l = NSTextField(labelWithString: title)
        l.font = .systemFont(ofSize: 11, weight: .semibold)
        popup.addItems(withTitles: items)
        popup.target = self
        popup.action = #selector(changed)
        popup.refusesFirstResponder = true
        addArrangedSubview(l)
        addArrangedSubview(popup)
        popup.widthAnchor.constraint(equalTo: widthAnchor).isActive = true
    }
    required init?(coder: NSCoder) { fatalError() }
    @objc func changed() { onChange?(popup.indexOfSelectedItem) }
}

final class GaragePanel: NSVisualEffectView {
    unowned let game: Game
    var s: Setup
    let model = PopupRow(title: "VEHÍCULO", items: carModels.map { $0.name })
    let paint = PopupRow(title: "COLOR", items: ["Color 1", "Color 2", "Color 3"])
    let blurb = NSTextField(wrappingLabelWithString: "")
    let engine = SliderRow(title: "MOTOR (cilindrada)", min: 600, max: 9000, format: { String(format: "%.0f cc", $0) })
    let turbo = SliderRow(title: "TURBO", min: 0, max: 3, format: { $0 < 0.05 ? "sin turbo" : String(format: "nivel %.1f", $0) })
    let ballast = SliderRow(title: "LASTRE / ALIGERAR", min: -300, max: 800, format: { String(format: "%+.0f kg", $0) })
    let stiff = SliderRow(title: "SUSPENSIÓN · dureza", min: 0.4, max: 3, format: { String(format: "x%.2f", $0) })
    let travel = SliderRow(title: "SUSPENSIÓN · recorrido / altura", min: 0.25, max: 1.0, format: { String(format: "%.0f cm", $0 * 100) })
    let radius = SliderRow(title: "RUEDAS · radio", min: 0.2, max: 0.85, format: { String(format: "%.0f cm", $0 * 100) })
    let width = SliderRow(title: "RUEDAS · ancho", min: 0.12, max: 0.55, format: { String(format: "%.0f cm", $0 * 100) })
    let tire = PopupRow(title: "RUEDAS · tipo", items: TireType.allCases.map { $0.label })
    let drive = PopupRow(title: "TRACCIÓN", items: Drive.allCases.map { $0.label })
    let readout = NSTextField(wrappingLabelWithString: "")

    init(game: Game) {
        self.game = game
        s = game.setup
        super.init(frame: .zero)
        material = .hudWindow
        blendingMode = .withinWindow
        state = .active
        wantsLayer = true

        let title = NSTextField(labelWithString: "GARAJE")
        title.font = .systemFont(ofSize: 18, weight: .heavy)
        blurb.font = .systemFont(ofSize: 11)
        blurb.textColor = .secondaryLabelColor
        readout.font = .monospacedSystemFont(ofSize: 11, weight: .regular)

        let stock = NSButton(title: "Restaurar original", target: self, action: #selector(restoreStock))
        let close = NSButton(title: "Conducir (G)", target: self, action: #selector(closeGarage))
        stock.refusesFirstResponder = true
        close.refusesFirstResponder = true
        close.keyEquivalent = ""
        let buttons = NSStackView(views: [stock, close])
        buttons.distribution = .fillEqually

        let stack = NSStackView(views: [title, model, paint, blurb, engine, turbo, ballast, stiff, travel, radius, width, tire, drive, readout, buttons])
        stack.orientation = .vertical
        stack.alignment = .leading
        stack.spacing = 8
        stack.translatesAutoresizingMaskIntoConstraints = false
        stack.edgeInsets = NSEdgeInsets(top: 16, left: 16, bottom: 16, right: 16)
        addSubview(stack)
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: leadingAnchor),
            stack.trailingAnchor.constraint(equalTo: trailingAnchor),
            stack.topAnchor.constraint(equalTo: topAnchor),
        ])
        for v in [model, paint, blurb, engine, turbo, ballast, stiff, travel, radius, width, tire, drive, readout, buttons] as [NSView] {
            v.widthAnchor.constraint(equalTo: stack.widthAnchor, constant: -32).isActive = true
        }

        model.onChange = { [unowned self] i in s = Game.savedSetup(i); sync(); push() }
        paint.onChange = { [unowned self] i in s.color = i; push() }
        engine.onChange = { [unowned self] v in s.engineCC = v; push() }
        turbo.onChange = { [unowned self] v in s.turbo = v; push() }
        ballast.onChange = { [unowned self] v in s.ballast = v; push() }
        stiff.onChange = { [unowned self] v in s.stiffness = v; push() }
        travel.onChange = { [unowned self] v in s.travel = v; push() }
        radius.onChange = { [unowned self] v in s.tireRadius = v; push() }
        width.onChange = { [unowned self] v in s.tireWidth = v; push() }
        tire.onChange = { [unowned self] i in s.tireType = TireType.allCases[i]; push() }
        drive.onChange = { [unowned self] i in s.drive = Drive.allCases[i]; push() }
        sync()
        isHidden = true
    }
    required init?(coder: NSCoder) { fatalError() }

    func sync() {
        s.model = min(s.model, carModels.count - 1)
        model.popup.selectItem(at: s.model)
        paint.popup.selectItem(at: s.color)
        blurb.stringValue = carModels[s.model].blurb
        engine.set(s.engineCC); turbo.set(s.turbo); ballast.set(s.ballast)
        stiff.set(s.stiffness); travel.set(s.travel); radius.set(s.tireRadius); width.set(s.tireWidth)
        tire.popup.selectItem(at: s.tireType.rawValue)
        drive.popup.selectItem(at: s.drive.rawValue)
        refreshReadout()
    }

    func push() {
        blurb.stringValue = carModels[s.model].blurb
        game.apply(s)
        refreshReadout()
    }

    func refreshReadout() {
        let sp = Spec(s)
        let hp = sp.hp * (1 + 0.5 * s.turbo)
        readout.stringValue = String(format: "Potencia   %.0f CV (%.0f sin turbo)\nPeso       %.0f kg\nPotencia/peso %.0f CV/t\nVel. máx. (marchas) ~%.0f km/h\nAltura libre ~%.0f cm",
                                     hp, sp.hp, sp.mass, hp / sp.mass * 1000, sp.vMaxGear * 3.6, (sp.restLen + sp.wheelRadius - 0.3) * 100)
    }

    @objc func restoreStock() { s = Setup.stock(s.model); sync(); push() }
    @objc func closeGarage() { game.garageOpen = false }
}
