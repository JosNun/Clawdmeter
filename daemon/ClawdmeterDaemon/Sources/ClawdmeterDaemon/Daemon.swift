import Foundation

// Retained for the life of the process so the signal sources keep firing.
nonisolated(unsafe) private var signalSources: [DispatchSourceSignal] = []

private func installSignalHandlers(_ stop: Flag) {
    for sig in [SIGINT, SIGTERM] {
        signal(sig, SIG_IGN)   // disable default terminate; the source handles it
        // Run on a background queue: an async @main never services the main
        // dispatch queue, so a .main source would never fire.
        let src = DispatchSource.makeSignalSource(signal: sig, queue: .global())
        src.setEventHandler { log("Stopping"); stop.set() }
        src.resume()
        signalSources.append(src)
    }
}

@main
struct Daemon {
    static func main() async {
        let stop = Flag()
        installSignalHandlers(stop)

        let ble = BLE()
        ble.start()
        let selector = PlanSelector()

        log("=== Clawdmeter Daemon (Swift, CoreBluetooth) ===")
        log("Poll interval: \(Int(POLL_INTERVAL))s")

        var backoff: Double = 1
        var skip: String? = nil   // a peripheral handle to skip for one cycle after a failed connect

        while !stop.isSet {
            guard let target = await ble.discover(skip: skip, scanTimeout: SCAN_TIMEOUT) else {
                skip = nil
                log("Device not found, retrying in \(Int(backoff))s…")
                await sleepInterruptible(backoff, stop)
                backoff = min(backoff * 2, MAX_BACKOFF)
                continue
            }
            skip = nil

            let addr = target.identifier.uuidString
            let ok = await ble.connectAndRun(target, stop: stop) {
                await pollActivePayload(selector: selector)
            }

            if ok {
                backoff = 1
            } else {
                // Skip this stale handle next cycle so retrieveConnected can't
                // trap us — the scan fallback stays reachable.
                skip = addr
                await sleepInterruptible(backoff, stop)
                backoff = min(backoff * 2, MAX_BACKOFF)
            }
        }

        log("Daemon stopped")
        exit(0)
    }
}
