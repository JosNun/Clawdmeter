import Foundation

/// Parse the `~/.config/claude-usage-monitor/config` file: `key=value` lines,
/// `#` comments stripped, keys lowercased. Returns the raw value for `key`, or nil.
private func configValue(_ key: String) -> String? {
    guard let text = try? String(contentsOf: CONFIG_FILE, encoding: .utf8) else { return nil }
    for rawLine in text.split(separator: "\n", omittingEmptySubsequences: false) {
        let line = rawLine.split(separator: "#", maxSplits: 1)[0]
            .trimmingCharacters(in: .whitespaces)
        guard let eq = line.firstIndex(of: "=") else { continue }
        let k = line[..<eq].trimmingCharacters(in: .whitespaces).lowercased()
        if k == key {
            return line[line.index(after: eq)...].trimmingCharacters(in: .whitespaces)
        }
    }
    return nil
}

/// Config dirs to poll (`config_dirs`, comma-separated), default `[~/.claude]`.
func readConfigDirs() -> [URL] {
    guard let raw = configValue("config_dirs"), !raw.isEmpty else { return [DEFAULT_CONFIG_DIR] }
    let dirs = raw.split(separator: ",")
        .map { NSString(string: $0.trimmingCharacters(in: .whitespaces)).expandingTildeInPath }
        .filter { !$0.isEmpty }
        .map { URL(fileURLWithPath: $0) }
    return dirs.isEmpty ? [DEFAULT_CONFIG_DIR] : dirs
}

/// `chime` opt-in (off|on), default off.
func readChimeOn() -> Bool { configValue("chime")?.lowercased() == "on" }

/// `clock` mode (off|auto|12|24), default off.
func readClock() -> String {
    let v = configValue("clock")?.lowercased() ?? "off"
    return ["off", "auto", "12", "24"].contains(v) ? v : "off"
}

/// Best-effort 12h/24h detection: the explicit macOS toggle first, then the
/// locale's preferred hour format. Returns 12 or 24.
func detectHourFormat() -> Int {
    for (key, result) in [("AppleICUForce24HourTime", 24), ("AppleICUForce12HourTime", 12)] {
        if let r = runProcess("/usr/bin/defaults", ["read", "-g", key], timeout: 3),
           r.stdout.trimmingCharacters(in: .whitespacesAndNewlines) == "1" {
            return result
        }
    }
    // "j" resolves to the locale's preferred hour skeleton; an 'a' (am/pm) or
    // 'h' (1–12 cycle) means 12-hour.
    let fmt = DateFormatter.dateFormat(fromTemplate: "j", options: 0, locale: .current) ?? ""
    return (fmt.contains("a") || fmt.contains("h")) ? 12 : 24
}

/// Attach the optional chime + clock fields the config opts into (mutates payload).
func addConfigFields(_ payload: inout Payload) {
    if readChimeOn() { payload.c = 1 }

    let clock = readClock()
    if clock != "off" {
        payload.tf = clock == "24" ? 24 : clock == "12" ? 12 : detectHourFormat()
        // Local wall-clock epoch = UTC epoch shifted by the tz offset, so the
        // device shows local time without an RTC.
        let offset = TimeZone.current.secondsFromGMT(for: Date())
        payload.t = Int(Date().timeIntervalSince1970) + offset
    }
}

/// Fraction of the (assumed calendar-monthly) billing period elapsed. Returns
/// tp (0–100), pd (days), rd (e.g. "Jul 19"). Enterprise/overage only.
func billingPeriodInfo(now: Double, resetTs: String) -> (tp: Int, pd: Int, rd: String?) {
    guard let periodEnd = Double(resetTs), periodEnd > 0 else { return (0, 30, nil) }

    let cal = Calendar.current
    let dtEnd = Date(timeIntervalSince1970: periodEnd)
    guard let dtStart = cal.date(byAdding: .month, value: -1, to: dtEnd) else { return (0, 30, nil) }

    let periodStart = dtStart.timeIntervalSince1970
    let periodLen = periodEnd - periodStart
    guard periodLen > 0 else { return (0, 30, nil) }

    let pct = (now - periodStart) / periodLen * 100
    let days = Int((periodLen / 86400).rounded())

    let fmt = DateFormatter()
    fmt.dateFormat = "MMM d"
    fmt.locale = Locale(identifier: "en_US")   // stable "Jul 19" regardless of host locale
    return (max(0, min(100, Int(pct.rounded()))), days, fmt.string(from: dtEnd))
}
