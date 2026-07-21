// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "ClawdmeterDaemon",
    platforms: [.macOS(.v13)],
    targets: [
        .executableTarget(
            name: "ClawdmeterDaemon",
            path: "Sources/ClawdmeterDaemon",
            linkerSettings: [
                // Embed Info.plist into __TEXT,__info_plist so TCC can find the
                // NSBluetoothAlwaysUsageDescription string a bundle-less CLI
                // binary otherwise lacks. Bundle id matches the launchd/.app
                // identity so the Bluetooth grant lands on the same name.
                .unsafeFlags([
                    "-Xlinker", "-sectcreate",
                    "-Xlinker", "__TEXT",
                    "-Xlinker", "__info_plist",
                    "-Xlinker", "Info.plist",
                ])
            ]
        )
    ]
)
