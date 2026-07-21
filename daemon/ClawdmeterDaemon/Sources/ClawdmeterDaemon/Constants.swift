import Foundation
import CoreBluetooth

let DEVICE_NAME = "Clawdmeter"
let SERVICE_UUID = CBUUID(string: "4c41555a-4465-7669-6365-000000000001")
let RX_CHAR_UUID = CBUUID(string: "4c41555a-4465-7669-6365-000000000002")  // daemon → device (JSON)
let REQ_CHAR_UUID = CBUUID(string: "4c41555a-4465-7669-6365-000000000004") // device → daemon (refresh nudge)
let HID_SERVICE_UUID = CBUUID(string: "1812")                              // generic HID, for the retrieve fallback

let POLL_INTERVAL: Double = 60   // seconds between API polls
let TICK: Double = 5             // inner-loop wake cadence (detect disconnect / refresh fast)
let CONNECT_TIMEOUT: Double = 20
let SCAN_TIMEOUT: Double = 15
let WRITE_TIMEOUT: Double = 10   // a .withResponse write with no callback ⇒ link is gone
let MAX_BACKOFF: Double = 60

let KEYCHAIN_SERVICE = "Claude Code-credentials"
let HOME = FileManager.default.homeDirectoryForCurrentUser
let DEFAULT_CONFIG_DIR = HOME.appendingPathComponent(".claude")
let CONFIG_FILE = HOME.appendingPathComponent(".config/claude-usage-monitor/config")

let API_URL = URL(string: "https://api.anthropic.com/v1/messages")!
let API_HEADERS: [String: String] = [
    "anthropic-version": "2023-06-01",
    "anthropic-beta": "oauth-2025-04-20",
    "Content-Type": "application/json",
    "User-Agent": "claude-code/2.1.5",
]
// A 1-token request whose only purpose is to read the rate-limit response
// headers; the body is discarded.
let API_BODY_DATA: Data = try! JSONSerialization.data(withJSONObject: [
    "model": "claude-haiku-4-5-20251001",
    "max_tokens": 1,
    "messages": [["role": "user", "content": "hi"]],
])

enum BLEError: Error, CustomStringConvertible {
    case bluetoothUnavailable(CBManagerState)
    case serviceMissing
    case characteristicMissing(String)
    case linkLost        // radio powered off / peripheral vanished mid-operation
    case writeTimedOut   // .withResponse write never acked

    var description: String {
        switch self {
        case .bluetoothUnavailable(let s): return "Bluetooth unavailable (state=\(s.rawValue))"
        case .serviceMissing: return "service \(SERVICE_UUID) not found"
        case .characteristicMissing(let c): return "characteristic \(c) not found"
        case .linkLost: return "BLE link lost"
        case .writeTimedOut: return "write timed out after \(Int(WRITE_TIMEOUT))s"
        }
    }
}
