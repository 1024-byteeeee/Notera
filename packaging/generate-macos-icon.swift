import AppKit
import Foundation

guard CommandLine.arguments.count == 3 else {
    fputs("usage: generate-macos-icon.swift INPUT.png OUTPUT.png\n", stderr)
    exit(2)
}

let inputURL = URL(fileURLWithPath: CommandLine.arguments[1])
let outputURL = URL(fileURLWithPath: CommandLine.arguments[2])
guard let source = NSImage(contentsOf: inputURL) else {
    fputs("unable to load source icon\n", stderr)
    exit(1)
}

let size = 1024
guard let bitmap = NSBitmapImageRep(
    bitmapDataPlanes: nil,
    pixelsWide: size,
    pixelsHigh: size,
    bitsPerSample: 8,
    samplesPerPixel: 4,
    hasAlpha: true,
    isPlanar: false,
    colorSpaceName: .deviceRGB,
    bytesPerRow: 0,
    bitsPerPixel: 0
) else {
    fputs("unable to allocate icon canvas\n", stderr)
    exit(1)
}

NSGraphicsContext.saveGraphicsState()
guard let context = NSGraphicsContext(bitmapImageRep: bitmap) else {
    fputs("unable to create graphics context\n", stderr)
    exit(1)
}
NSGraphicsContext.current = context

let canvas = NSRect(x: 0, y: 0, width: size, height: size)
let background = NSGradient(colors: [
    NSColor(calibratedRed: 1.00, green: 0.79, blue: 0.10, alpha: 1.0),
    NSColor(calibratedRed: 0.98, green: 0.62, blue: 0.00, alpha: 1.0),
    NSColor(calibratedRed: 0.84, green: 0.42, blue: 0.00, alpha: 1.0)
])!
background.draw(in: canvas, angle: 90)
source.draw(in: canvas.insetBy(dx: -64, dy: -64), from: .zero,
            operation: .sourceOver, fraction: 1.0)
context.flushGraphics()
NSGraphicsContext.restoreGraphicsState()

guard let png = bitmap.representation(using: .png, properties: [:]) else {
    fputs("unable to encode icon PNG\n", stderr)
    exit(1)
}
try png.write(to: outputURL, options: .atomic)
