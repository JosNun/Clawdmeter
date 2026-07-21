import Foundation

/// Thread-safe boolean for the stop signal — set from a signal handler, read
/// from the async main loop.
final class Flag: @unchecked Sendable {
    private let lock = NSLock()
    private var value = false
    var isSet: Bool { lock.lock(); defer { lock.unlock() }; return value }
    func set() { lock.lock(); value = true; lock.unlock() }
}

/// Run a subprocess with a wall-clock timeout. Returns nil if it fails to launch
/// or overruns `timeout` (the daemon shells out to `security`/`defaults`/`blueutil`,
/// and blueutil in particular *hangs* rather than erroring when it lacks its own
/// Bluetooth TCC grant — so every call is bounded).
func runProcess(_ path: String, _ args: [String], timeout: Double) -> (stdout: String, status: Int32)? {
    let p = Process()
    p.executableURL = URL(fileURLWithPath: path)
    p.arguments = args
    let outPipe = Pipe()
    p.standardOutput = outPipe
    p.standardError = Pipe()
    do { try p.run() } catch { return nil }

    let done = DispatchSemaphore(value: 0)
    DispatchQueue.global().async { p.waitUntilExit(); done.signal() }
    if done.wait(timeout: .now() + timeout) == .timedOut {
        p.terminate()
        return nil
    }
    let data = outPipe.fileHandleForReading.readDataToEndOfFile()
    return (String(data: data, encoding: .utf8) ?? "", p.terminationStatus)
}

/// Sleep up to `seconds`, waking early if `stop` fires (keeps SIGTERM shutdown snappy).
func sleepInterruptible(_ seconds: Double, _ stop: Flag) async {
    let end = Date().addingTimeInterval(seconds)
    while !stop.isSet && Date() < end {
        try? await Task.sleep(nanoseconds: 200_000_000)
    }
}
