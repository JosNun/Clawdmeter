import Foundation
// CoreBluetooth predates Sendable; all our mutable state lives on `queue`, so
// the crossings are safe — the annotation tells Swift 6 to trust the framework.
@preconcurrency import CoreBluetooth

/// Native CoreBluetooth driver. Every delegate callback resumes a parked
/// continuation, bridging Apple's callback model into linear async/await.
///
/// Concurrency contract: all mutable state (continuations, peripheral, char
/// handles, flags) is touched ONLY on `queue`. Delegate methods already run on
/// `queue` and mutate directly; the async methods reach in via `queue.sync`/
/// `queue.async`. The async methods themselves run on the cooperative pool, so
/// `queue.sync` never deadlocks (it's never called from `queue`).
final class BLE: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate, @unchecked Sendable {
    private let queue = DispatchQueue(label: "software.playful.clawdmeter-daemon.ble")
    private var central: CBCentralManager!

    private var peripheral: CBPeripheral?
    private var rxChar: CBCharacteristic?
    private var reqChar: CBCharacteristic?

    private var isConnected = false
    private var refreshPending = false

    // Parked continuations (one of each in flight at most).
    private var powerCont: CheckedContinuation<Void, Never>?
    private var scanCont: CheckedContinuation<CBPeripheral?, Never>?
    private var connectCont: CheckedContinuation<Void, Error>?
    private var servicesCont: CheckedContinuation<Void, Error>?
    private var charsCont: CheckedContinuation<Void, Error>?
    private var writeCont: CheckedContinuation<Void, Error>?
    private var refreshCont: CheckedContinuation<Void, Never>?

    func start() {
        central = CBCentralManager(delegate: self, queue: queue)
    }

    // MARK: - Discovery

    /// Find a connectable Clawdmeter, or nil. Order = strongest signal first:
    /// (1) system-connected under our custom service, (2) system-connected HID
    /// with a name match, (3) a name-scan fallback. Steps 1–2 mirror the Python
    /// daemon; step 3 is added for the not-yet-HID-grabbed window and is gated on
    /// an exact name match (the tradeoff: a stranger's device *named* Clawdmeter
    /// could match — acceptable for a personal Mac-only setup).
    func discover(skip: String?, scanTimeout: Double) async -> CBPeripheral? {
        do { try await waitPoweredOn() }
        catch { log("Bluetooth unavailable: \(error)"); return nil }

        if let p = retrieveConnected(SERVICE_UUID, requireName: false, skip: skip) {
            log("Found system-connected peripheral: \(p.name ?? "nil") [\(p.identifier.uuidString)]")
            return p
        }
        if let p = retrieveConnected(HID_SERVICE_UUID, requireName: true, skip: skip) {
            log("Found system-connected HID peripheral: \(p.name ?? "nil") [\(p.identifier.uuidString)]")
            return p
        }
        log("Device not held by OS; scanning by name (\(Int(scanTimeout))s)…")
        return await scanByName(timeout: scanTimeout)
    }

    private func retrieveConnected(_ service: CBUUID, requireName: Bool, skip: String?) -> CBPeripheral? {
        queue.sync {
            for p in central.retrieveConnectedPeripherals(withServices: [service]) {
                if let skip, p.identifier.uuidString == skip { continue }
                if requireName && p.name != DEVICE_NAME { continue }
                return p
            }
            return nil
        }
    }

    private func scanByName(timeout: Double) async -> CBPeripheral? {
        await withCheckedContinuation { (c: CheckedContinuation<CBPeripheral?, Never>) in
            queue.async {
                self.scanCont = c
                self.central.scanForPeripherals(withServices: nil)
                self.queue.asyncAfter(deadline: .now() + timeout) { [weak self] in
                    guard let self, let pending = self.scanCont else { return }
                    self.scanCont = nil
                    self.central.stopScan()
                    pending.resume(returning: nil)
                }
            }
        }
    }

    // MARK: - Connect + run

    /// Connect to `target` and pump payloads until disconnected or stopped.
    /// Returns true if the link was used successfully (≥1 payload written).
    func connectAndRun(_ target: CBPeripheral, stop: Flag,
                       poll: () async -> Payload?) async -> Bool {
        log("Connecting to \(target.identifier.uuidString)…")
        do { try await connect(target, timeout: CONNECT_TIMEOUT) }
        catch {
            log("Connection failed: \(error)")
            if isEncryptionError(error) {
                log("Encryption failed — likely a stale macOS bond; self-healing")
                unpairMacos()
            }
            return false
        }
        log("Connected")
        setConnected(true)

        do { try await discoverGATT() }
        catch { log("Discovery failed: \(error)"); await disconnect(); return false }

        var lastPoll = 0.0
        var used = false
        while connectedFlag() && !stop.isSet {
            let now = Date().timeIntervalSince1970
            if takeRefreshPending() || now - lastPoll >= POLL_INTERVAL {
                // Stamp the attempt up front, not just successes: an unhealthy API
                // (e.g. sustained 529) must still wait POLL_INTERVAL, or the loop
                // re-polls every TICK and hammers a service already crying uncle.
                // A device refresh nudge still bypasses this via takeRefreshPending.
                lastPoll = now
                if let payload = await poll() {
                    if await write(payload) { used = true }
                } else {
                    // Why-nil is already logged at the source (missing token, API
                    // error, or no config dirs); keep this neutral so it doesn't
                    // misattribute an API failure to configuration.
                    log("No payload this cycle")
                }
            }
            await waitRefresh(timeout: TICK)
        }
        await disconnect()
        log(stop.isSet ? "Stopping" : "Device disconnected")
        return used
    }

    private func connect(_ p: CBPeripheral, timeout: Double) async throws {
        try await withCheckedThrowingContinuation { (c: CheckedContinuation<Void, Error>) in
            queue.async {
                self.peripheral = p
                p.delegate = self
                self.connectCont = c
                self.central.connect(p, options: nil)
                self.queue.asyncAfter(deadline: .now() + timeout) { [weak self] in
                    guard let self, let pending = self.connectCont else { return }
                    self.connectCont = nil
                    self.central.cancelPeripheralConnection(p)
                    pending.resume(throwing: CocoaError(.userCancelled))
                }
            }
        }
    }

    private func discoverGATT() async throws {
        guard let p = peripheral else { throw BLEError.serviceMissing }
        try await withCheckedThrowingContinuation { (c: CheckedContinuation<Void, Error>) in
            queue.async { self.servicesCont = c; p.discoverServices([SERVICE_UUID]) }
        }
        guard let service = p.services?.first(where: { $0.uuid == SERVICE_UUID }) else {
            throw BLEError.serviceMissing
        }
        try await withCheckedThrowingContinuation { (c: CheckedContinuation<Void, Error>) in
            queue.async { self.charsCont = c; p.discoverCharacteristics([RX_CHAR_UUID, REQ_CHAR_UUID], for: service) }
        }
        rxChar = service.characteristics?.first { $0.uuid == RX_CHAR_UUID }
        reqChar = service.characteristics?.first { $0.uuid == REQ_CHAR_UUID }
        guard rxChar != nil else { throw BLEError.characteristicMissing("RX") }
        // Refresh nudges are optional (we poll every POLL_INTERVAL regardless);
        // subscribe fire-and-forget — CoreBluetooth's setNotifyValue can't wedge
        // the loop the way bleak's awaited CCCD write could.
        if let req = reqChar { p.setNotifyValue(true, for: req) }
    }

    private func write(_ payload: Payload) async -> Bool {
        guard let p = peripheral, let rx = rxChar else { return false }
        let data = payload.jsonData()
        do {
            try await withCheckedThrowingContinuation { (c: CheckedContinuation<Void, Error>) in
                queue.async {
                    self.writeCont = c
                    p.writeValue(data, for: rx, type: .withResponse)
                    // No callback within WRITE_TIMEOUT ⇒ the link is silently gone
                    // (e.g. radio slept without a didDisconnect). Bound it so the
                    // run loop can't wedge here forever.
                    self.queue.asyncAfter(deadline: .now() + WRITE_TIMEOUT) { [weak self] in
                        guard let self, let pending = self.writeCont else { return }
                        self.writeCont = nil
                        pending.resume(throwing: BLEError.writeTimedOut)
                    }
                }
            }
            log("Sent: \(payload.jsonString())")
            return true
        } catch {
            log("Write failed: \(error)")
            return false
        }
    }

    private func disconnect() async {
        guard let p = peripheral else { return }
        queue.sync { self.isConnected = false }
        central.cancelPeripheralConnection(p)
    }

    // MARK: - Power / refresh helpers

    func waitPoweredOn() async throws {
        if queue.sync(execute: { central.state }) == .poweredOn { return }
        await withCheckedContinuation { (c: CheckedContinuation<Void, Never>) in
            queue.async {
                if self.central.state == .poweredOn { c.resume() } else { self.powerCont = c }
            }
        }
        let state = queue.sync { central.state }
        guard state == .poweredOn else { throw BLEError.bluetoothUnavailable(state) }
    }

    private func setConnected(_ v: Bool) { queue.sync { self.isConnected = v } }
    private func connectedFlag() -> Bool { queue.sync { self.isConnected } }
    private func takeRefreshPending() -> Bool {
        queue.sync { let r = self.refreshPending; self.refreshPending = false; return r }
    }

    /// Park until a refresh nudge arrives, the link drops, or `timeout` elapses.
    private func waitRefresh(timeout: Double) async {
        await withCheckedContinuation { (c: CheckedContinuation<Void, Never>) in
            queue.async {
                if self.refreshPending || !self.isConnected { c.resume(); return }
                self.refreshCont = c
                self.queue.asyncAfter(deadline: .now() + timeout) { [weak self] in
                    guard let self, let pending = self.refreshCont else { return }
                    self.refreshCont = nil
                    pending.resume()
                }
            }
        }
    }

    private func wakeRefresh() {
        if let c = refreshCont { refreshCont = nil; c.resume() }
    }

    // MARK: - CBCentralManagerDelegate

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        let state = central.state
        log("Bluetooth state → \(stateName(state))")
        if state == .poweredOn {
            powerCont?.resume(); powerCont = nil
        } else {
            // The radio can vanish (sleep) WITHOUT firing didDisconnectPeripheral.
            // Without this, isConnected stays true and any awaited callback
            // (notably a .withResponse write) parks forever, wedging the loop.
            teardown(BLEError.linkLost)
        }
    }

    /// Fail every in-flight operation and mark the link down, so the run loop
    /// unwinds and re-enters discover(). Runs on `queue` (delegate context) — the
    /// only place these continuation fields may be touched. powerCont is left
    /// alone: it's owned by waitPoweredOn and resumes only on poweredOn.
    private func teardown(_ error: Error) {
        isConnected = false
        connectCont?.resume(throwing: error); connectCont = nil
        servicesCont?.resume(throwing: error); servicesCont = nil
        charsCont?.resume(throwing: error); charsCont = nil
        writeCont?.resume(throwing: error); writeCont = nil
        if let s = scanCont { scanCont = nil; central.stopScan(); s.resume(returning: nil) }
        wakeRefresh()
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let name = peripheral.name ?? (advertisementData[CBAdvertisementDataLocalNameKey] as? String)
        guard name == DEVICE_NAME, let pending = scanCont else { return }
        log("Found \(DEVICE_NAME) by scan [\(peripheral.identifier.uuidString)] rssi=\(RSSI)")
        scanCont = nil
        central.stopScan()
        self.peripheral = peripheral
        pending.resume(returning: peripheral)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        connectCont?.resume(); connectCont = nil
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        connectCont?.resume(throwing: error ?? CocoaError(.userCancelled)); connectCont = nil
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        // Same teardown as a radio-off: fail in-flight ops (a write could be
        // parked) and let the run loop notice the drop immediately.
        teardown(error ?? BLEError.linkLost)
    }

    // MARK: - CBPeripheralDelegate

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error { servicesCont?.resume(throwing: error) } else { servicesCont?.resume() }
        servicesCont = nil
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if let error { charsCont?.resume(throwing: error) } else { charsCont?.resume() }
        charsCont = nil
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        if let error { writeCont?.resume(throwing: error) } else { writeCont?.resume() }
        writeCont = nil
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard characteristic.uuid == REQ_CHAR_UUID else { return }
        log("Refresh requested by device")
        refreshPending = true
        wakeRefresh()
    }

    private func stateName(_ s: CBManagerState) -> String {
        switch s {
        case .poweredOn: return "poweredOn"
        case .poweredOff: return "poweredOff"
        case .unauthorized: return "unauthorized (grant Bluetooth in System Settings)"
        case .unsupported: return "unsupported"
        case .resetting: return "resetting"
        case .unknown: return "unknown"
        @unknown default: return "?"
        }
    }
}
