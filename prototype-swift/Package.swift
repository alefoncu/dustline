// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "Dustline",
    platforms: [.macOS(.v13)],
    targets: [
        .executableTarget(
            name: "Dustline",
            path: "Sources/Dustline",
            swiftSettings: [.unsafeFlags(["-Ounchecked"], .when(configuration: .release))]
        )
    ]
)
