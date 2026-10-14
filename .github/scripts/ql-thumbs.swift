// Asks Quick Look for each file's thumbnail, as Finder does, and saves it as
// <out dir>/<file name>.png. Prints whether an extension drew it or macOS
// fell back to an icon.
// swift ql-thumbs.swift <out dir> <file>...
import AppKit
import QuickLookThumbnailing

let outDir = URL(fileURLWithPath: CommandLine.arguments[1])
for path in CommandLine.arguments.dropFirst(2) {
    let url = URL(fileURLWithPath: path)
    let name = url.lastPathComponent
    let req = QLThumbnailGenerator.Request(
        fileAt: url, size: CGSize(width: 256, height: 256), scale: 2, representationTypes: .all)
    let done = DispatchSemaphore(value: 0)
    QLThumbnailGenerator.shared.generateBestRepresentation(for: req) { rep, err in
        defer { done.signal() }
        guard let rep = rep else {
            print("\(name): \(err.map { "\($0)" } ?? "no thumbnail")")
            return
        }
        let kind = rep.type == .thumbnail ? "thumbnail" : rep.type == .icon ? "icon" : "low quality"
        print("\(name): \(kind) \(rep.cgImage.width)x\(rep.cgImage.height)")
        let png = NSBitmapImageRep(cgImage: rep.cgImage).representation(using: .png, properties: [:])
        try? png?.write(to: outDir.appendingPathComponent("\(name).png"))
    }
    if done.wait(timeout: .now() + 30) == .timedOut {
        print("\(name): timed out")
    }
}
