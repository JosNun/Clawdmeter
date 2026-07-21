import Foundation

/// Pull `accessToken` out of a credentials blob: JSON (direct or one level
/// nested under any key), then a regex fallback, then treat the whole blob as a
/// raw token if it looks plausible.
func extractAccessToken(_ blob: String) -> String? {
    let b = blob.trimmingCharacters(in: .whitespacesAndNewlines)
    if b.isEmpty { return nil }

    if let data = b.data(using: .utf8),
       let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any] {
        if let t = obj["accessToken"] as? String { return t }
        for v in obj.values {
            if let nested = v as? [String: Any], let t = nested["accessToken"] as? String {
                return t
            }
        }
    }

    if let re = try? NSRegularExpression(pattern: #""accessToken"\s*:\s*"([^"]+)""#),
       let m = re.firstMatch(in: b, range: NSRange(b.startIndex..., in: b)),
       let r = Range(m.range(at: 1), in: b) {
        return String(b[r])
    }

    if b.range(of: #"^[A-Za-z0-9_\-.~+/=]{20,}$"#, options: .regularExpression) != nil {
        return b
    }
    return nil
}

/// macOS Keychain read via `security` (the default install stores the token
/// there with no file).
private func readTokenKeychain() -> String? {
    guard let r = runProcess("/usr/bin/security",
                             ["find-generic-password", "-s", KEYCHAIN_SERVICE,
                              "-a", NSUserName(), "-w"],
                             timeout: 10) else {
        log("Keychain access error or timeout")
        return nil
    }
    if r.status != 0 {
        log("Keychain read failed (rc=\(r.status))")
        return nil
    }
    return extractAccessToken(r.stdout)
}

/// Token for one config dir: its `.credentials.json` if present, else the
/// Keychain for the default `~/.claude` dir on macOS.
func readTokenFor(_ dir: URL) -> String? {
    let cred = dir.appendingPathComponent(".credentials.json")
    if let text = try? String(contentsOf: cred, encoding: .utf8) {
        return extractAccessToken(text)
    }
    if dir.standardizedFileURL == DEFAULT_CONFIG_DIR.standardizedFileURL {
        return readTokenKeychain()
    }
    return nil
}
