import Foundation

private let logFormatter: DateFormatter = {
    let f = DateFormatter()
    f.dateFormat = "HH:mm:ss"
    return f
}()

func log(_ msg: String) {
    print("[\(logFormatter.string(from: Date()))] \(msg)")
    // launchd routes stdout to a file, so it's block-buffered — flush every
    // line or the logs lag the process (Python used print(flush=True)).
    fflush(stdout)
}
