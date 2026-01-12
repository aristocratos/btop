#!/usr/bin/env swift
/*
 * ANE Vision Framework Test - Uses Neural Engine for image analysis
 *
 * Compile: swiftc -O -o ane_vision_test ane_vision_test.swift -framework Vision -framework CoreImage -framework AppKit
 * Run: ./ane_vision_test [duration_seconds]
 */

import Foundation
import Vision
import CoreImage
import AppKit

func createTestImage(width: Int, height: Int) -> CGImage? {
    let colorSpace = CGColorSpaceCreateDeviceRGB()
    let bitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue)

    guard let context = CGContext(
        data: nil,
        width: width,
        height: height,
        bitsPerComponent: 8,
        bytesPerRow: width * 4,
        space: colorSpace,
        bitmapInfo: bitmapInfo.rawValue
    ) else { return nil }

    // Fill with random colored rectangles to make interesting image
    for _ in 0..<50 {
        let r = CGFloat.random(in: 0...1)
        let g = CGFloat.random(in: 0...1)
        let b = CGFloat.random(in: 0...1)
        context.setFillColor(red: r, green: g, blue: b, alpha: 1.0)

        let x = CGFloat.random(in: 0...CGFloat(width))
        let y = CGFloat.random(in: 0...CGFloat(height))
        let w = CGFloat.random(in: 10...100)
        let h = CGFloat.random(in: 10...100)
        context.fill(CGRect(x: x, y: y, width: w, height: h))
    }

    return context.makeImage()
}

func runVisionANETest(duration: Double) {
    print("=" + String(repeating: "=", count: 50))
    print("ANE Vision Framework Test (Neural Engine)")
    print("=" + String(repeating: "=", count: 50))
    print()
    print("Duration: \(Int(duration)) seconds")
    print("This test runs image classification which uses the ANE.")
    print()

    // Create test image
    guard let testImage = createTestImage(width: 640, height: 480) else {
        print("Failed to create test image")
        return
    }
    print("Created test image: 640x480")

    // Create Vision requests that use ANE
    let classificationRequest = VNClassifyImageRequest()

    // Use revision that supports ANE
    if #available(macOS 12.0, *) {
        classificationRequest.revision = VNClassifyImageRequestRevision2
    }

    let faceRequest = VNDetectFaceRectanglesRequest()
    let textRequest = VNRecognizeTextRequest()

    print("Created Vision requests:")
    print("  - Image Classification (VNClassifyImageRequest)")
    print("  - Face Detection (VNDetectFaceRectanglesRequest)")
    print("  - Text Recognition (VNRecognizeTextRequest)")
    print()
    print("Running inference loop...")
    print("-" + String(repeating: "-", count: 50))
    print()
    print("Time     | Inferences | Rate    | Classifications")
    print("-" + String(repeating: "-", count: 50))

    let startTime = Date()
    var iterations = 0
    var totalClassifications = 0
    var lastReport = startTime

    while Date().timeIntervalSince(startTime) < duration {
        autoreleasepool {
            // Create new random image each iteration for varied workload
            guard let img = createTestImage(width: 640, height: 480) else { return }

            let handler = VNImageRequestHandler(cgImage: img, options: [:])

            do {
                // Run classification - this uses ANE!
                try handler.perform([classificationRequest])

                if let results = classificationRequest.results {
                    totalClassifications += results.count
                }

                // Also run face detection for more ANE work
                try handler.perform([faceRequest])

                // And text recognition
                try handler.perform([textRequest])

            } catch {
                // Ignore errors, continue testing
            }
        }

        iterations += 1

        let now = Date()
        if now.timeIntervalSince(lastReport) >= 1.0 {
            let elapsed = now.timeIntervalSince(startTime)
            let rate = Double(iterations) / elapsed
            print(String(format: "%6.1fs  | %10d | %6.1f/s | %d", elapsed, iterations, rate, totalClassifications))
            lastReport = now
        }
    }

    let elapsed = Date().timeIntervalSince(startTime)
    print("-" + String(repeating: "-", count: 50))
    print()
    print("Test completed!")
    print("Total inferences: \(iterations)")
    print(String(format: "Total time: %.1f seconds", elapsed))
    print(String(format: "Average rate: %.2f inferences/second", Double(iterations) / elapsed))
    print("Total classifications found: \(totalClassifications)")
    print()
    print("Check btop/macmon - ANE should show activity!")
}

// Simpler test with just classification
func runSimpleClassificationTest(duration: Double) {
    print("=" + String(repeating: "=", count: 50))
    print("Simple ANE Classification Test")
    print("=" + String(repeating: "=", count: 50))
    print()

    guard let testImage = createTestImage(width: 224, height: 224) else {
        print("Failed to create test image")
        return
    }

    let request = VNClassifyImageRequest()

    print("Running image classification loop for \(Int(duration)) seconds...")
    print("(Image classification uses the Neural Engine)")
    print()

    let startTime = Date()
    var count = 0
    var lastReport = startTime

    while Date().timeIntervalSince(startTime) < duration {
        autoreleasepool {
            let handler = VNImageRequestHandler(cgImage: testImage, options: [:])
            try? handler.perform([request])
            count += 1
        }

        let now = Date()
        if now.timeIntervalSince(lastReport) >= 1.0 {
            let elapsed = now.timeIntervalSince(startTime)
            print(String(format: "Time: %5.1fs | Inferences: %5d | Rate: %.1f/s", elapsed, count, Double(count)/elapsed))
            lastReport = now
        }
    }

    let elapsed = Date().timeIntervalSince(startTime)
    print()
    print("Completed \(count) inferences in \(String(format: "%.1f", elapsed)) seconds")
    print(String(format: "Rate: %.2f inferences/second", Double(count) / elapsed))
}

// Main
let args = CommandLine.arguments
var duration: Double = 30
var simple = false

for (i, arg) in args.enumerated() {
    if arg == "--simple" || arg == "-s" {
        simple = true
    } else if let d = Double(arg), i > 0 {
        duration = d
    } else if arg == "--help" || arg == "-h" {
        print("""
        ANE Vision Test - Uses Neural Engine for image analysis

        Usage: ane_vision_test [options] [duration]

        Options:
            --simple, -s    Run simpler classification-only test
            --help, -h      Show this help

        This test runs Vision framework image classification which
        executes on the Apple Neural Engine (ANE).
        """)
        exit(0)
    }
}

if simple {
    runSimpleClassificationTest(duration: duration)
} else {
    runVisionANETest(duration: duration)
}
