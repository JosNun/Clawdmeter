import Foundation

/// The JSON the firmware consumes. Optional fields are omitted by JSONEncoder
/// when nil — which is exactly how the Python daemon conditionally added chime
/// (`c`), clock (`t`/`tf`), and enterprise billing (`tp`/`pd`/`rd`) fields.
struct Payload: Encodable {
    var s: Int          // 5h utilization %
    var sr: Int         // 5h reset, minutes
    var w: Int          // 7d utilization % (0 for enterprise)
    var wr: Int         // 7d reset, minutes
    var st: String      // status string
    var acct: String    // "pro" | "ent"
    var ok: Bool

    var c: Int?         // chime opt-in
    var t: Int?         // local wall-clock epoch
    var tf: Int?        // 12 | 24
    var tp: Int?        // billing period elapsed %
    var pd: Int?        // billing period length, days
    var rd: String?     // reset date label, e.g. "Jul 19"

    func jsonData() -> Data { (try? JSONEncoder().encode(self)) ?? Data() }
    func jsonString() -> String { String(data: jsonData(), encoding: .utf8) ?? "?" }
}
