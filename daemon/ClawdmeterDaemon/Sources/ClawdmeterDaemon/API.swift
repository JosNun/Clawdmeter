import Foundation

/// POST a 1-token request and read the rate-limit headers off the response.
/// Returns a Payload, or nil on any HTTP/network failure.
func pollAPI(token: String) async -> Payload? {
    var req = URLRequest(url: API_URL)
    req.httpMethod = "POST"
    for (k, v) in API_HEADERS { req.setValue(v, forHTTPHeaderField: k) }
    req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
    req.httpBody = API_BODY_DATA
    req.timeoutInterval = 20

    do {
        let (data, resp) = try await URLSession.shared.data(for: req)
        guard let http = resp as? HTTPURLResponse else { return nil }
        if http.statusCode >= 400 {
            let body = String(data: data, encoding: .utf8)?.prefix(200) ?? ""
            log("API HTTP \(http.statusCode): \(body)")
            return nil
        }
        return buildPayload(from: http)
    } catch {
        log("API call failed: \(error)")
        return nil
    }
}

private func buildPayload(from http: HTTPURLResponse) -> Payload {
    func hdr(_ name: String, _ def: String = "0") -> String {
        (http.value(forHTTPHeaderField: name)).flatMap { $0.isEmpty ? nil : $0 } ?? def
    }
    let now = Date().timeIntervalSince1970

    func resetMinutes(_ ts: String) -> Int {
        guard let r = Double(ts) else { return 0 }
        let mins = (r - now) / 60.0
        return mins > 0 ? Int(mins.rounded()) : 0
    }
    func pct(_ util: String) -> Int {
        guard let u = Double(util) else { return 0 }
        return Int((u * 100).rounded())
    }

    var payload: Payload
    // Pro/Max expose 5h + 7d windows; enterprise/overage report a single
    // spending-limit window instead.
    if http.value(forHTTPHeaderField: "anthropic-ratelimit-unified-5h-utilization") != nil {
        payload = Payload(
            s: pct(hdr("anthropic-ratelimit-unified-5h-utilization")),
            sr: resetMinutes(hdr("anthropic-ratelimit-unified-5h-reset")),
            w: pct(hdr("anthropic-ratelimit-unified-7d-utilization")),
            wr: resetMinutes(hdr("anthropic-ratelimit-unified-7d-reset")),
            st: hdr("anthropic-ratelimit-unified-5h-status", "unknown"),
            acct: "pro", ok: true)
    } else {
        let resetTs = hdr("anthropic-ratelimit-unified-overage-reset")
        let billing = billingPeriodInfo(now: now, resetTs: resetTs)
        payload = Payload(
            s: pct(hdr("anthropic-ratelimit-unified-overage-utilization")),
            sr: resetMinutes(resetTs),
            w: 0, wr: 0,
            st: hdr("anthropic-ratelimit-unified-status", "unknown"),
            acct: "ent", ok: true,
            tp: billing.tp, pd: billing.pd, rd: billing.rd)
    }
    addConfigFields(&payload)
    return payload
}

/// Decide which config dir's plan is "active" across polls: the one whose
/// session % rose most recently (a rise stamps a monotonic counter, so the
/// pick is sticky and a window reset — a drop to 0 — isn't mistaken for use).
/// Before any rise is seen, the highest current % wins.
final class PlanSelector {
    private var prevS: [String: Int] = [:]
    private var lastActive: [String: Int] = [:]
    private var seq = 0

    func choose(_ sessions: [String: Int]) -> String {
        seq += 1
        for (d, s) in sessions {
            if let prev = prevS[d], s > prev { lastActive[d] = seq }
            prevS[d] = s
        }
        return sessions.keys.max { a, b in
            (lastActive[a] ?? 0, sessions[a] ?? 0) < (lastActive[b] ?? 0, sessions[b] ?? 0)
        } ?? sessions.keys.first ?? ""
    }
}

/// Poll every configured dir, return the active plan's payload (nil if none
/// yielded one). A single dir collapses to exactly one poll.
func pollActivePayload(selector: PlanSelector) async -> Payload? {
    let dirs = readConfigDirs()
    guard !dirs.isEmpty else { log("No config dirs found"); return nil }
    var payloads: [String: Payload] = [:]
    var sessions: [String: Int] = [:]

    for dir in dirs {
        guard let token = readTokenFor(dir) else {
            log("No token in \(dir.path); skipping")
            continue
        }
        if let p = await pollAPI(token: token) {
            payloads[dir.path] = p
            sessions[dir.path] = p.s
        }
    }
    guard !payloads.isEmpty else { return nil }

    let active = selector.choose(sessions)
    if dirs.count > 1 { log("Active plan: \(active) (s=\(sessions[active] ?? 0))") }
    return payloads[active]
}
