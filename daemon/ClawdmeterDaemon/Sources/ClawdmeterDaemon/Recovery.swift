import Foundation
@preconcurrency import CoreBluetooth

/// True if a connect error is a macOS bonding/encryption mismatch — CBErrorDomain
/// Code=15 ("Failed to encrypt the connection"), typically a stale bond after a
/// firmware reflash. Matched on domain+code with a message fallback so we don't
/// depend on the exact wording.
func isEncryptionError(_ error: Error) -> Bool {
    let ns = error as NSError
    if ns.domain == CBErrorDomain && ns.code == 15 { return true }
    return ns.localizedDescription.lowercased().contains("encrypt")
}

private func blueutilPath() -> String? {
    for p in ["/opt/homebrew/bin/blueutil", "/usr/local/bin/blueutil"] {
        if FileManager.default.isExecutableFile(atPath: p) { return p }
    }
    return nil
}

/// Forget a stale macOS bond for DEVICE_NAME so the next connect re-bonds
/// silently. CoreBluetooth exposes no unpair API, so we shell to `blueutil` and
/// map name → BD_ADDR via `--paired`. Returns true if a bond was removed.
@discardableResult
func unpairMacos() -> Bool {
    guard let bu = blueutilPath() else {
        log("Stale bond detected but `blueutil` is not installed; cannot auto-recover. "
            + "Run `brew install blueutil`, or forget '\(DEVICE_NAME)' in "
            + "System Settings > Bluetooth and reconnect.")
        return false
    }

    guard let paired = runProcess(bu, ["--paired"], timeout: 8) else {
        log("blueutil --paired timed out — it likely lacks its own Bluetooth "
            + "permission. Grant it under System Settings > Privacy & Security > Bluetooth.")
        return false
    }

    var addr: String?
    for line in paired.stdout.split(separator: "\n") {
        if line.contains("name: \"\(DEVICE_NAME)\""),
           let r = line.range(of: #"address:\s*([0-9a-fA-F:-]+)"#, options: .regularExpression) {
            addr = String(line[r]).replacingOccurrences(of: "address:", with: "")
                .trimmingCharacters(in: .whitespaces)
            break
        }
    }
    guard let addr else {
        log("No paired '\(DEVICE_NAME)' found to unpair (already forgotten?)")
        return false
    }

    guard runProcess(bu, ["--unpair", addr], timeout: 8) != nil else { return false }
    log("Unpaired stale bond for '\(DEVICE_NAME)' [\(addr)]; re-pairing on next connect")
    return true
}
