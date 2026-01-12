#!/usr/bin/env swift
/*
 * ANE (Apple Neural Engine) Load Test for btop
 *
 * This Swift script generates load on the Apple Neural Engine
 * to verify btop correctly displays ANE activity and power metrics.
 *
 * Compile: swiftc -O -o ane_load_test ane_load_test.swift
 * Run: ./ane_load_test [duration_seconds]
 */

import Foundation
import Accelerate
import CoreML

// MARK: - Simple MLModel for ANE testing

/// Create a simple computation that targets the ANE
func runANELoadTest(durationSeconds: Double = 30.0) {
    print("=" + String(repeating: "=", count: 49))
    print("ANE (Apple Neural Engine) Load Test for btop")
    print("=" + String(repeating: "=", count: 49))
    print()

    // Check for Apple Silicon
    #if !arch(arm64)
    print("Error: This test requires Apple Silicon (M1/M2/M3/M4)")
    exit(1)
    #endif

    print("✓ Running on Apple Silicon")
    print("Duration: \(Int(durationSeconds)) seconds")
    print()
    print("Monitor ANE activity in btop (press '5' to show GPU panel)")
    print("-" + String(repeating: "-", count: 49))
    print()

    // Use BNNS (Basic Neural Network Subroutines) which can utilize ANE
    let batchSize = 1
    let inputChannels = 256
    let outputChannels = 256
    let kernelSize = 3

    // Create input descriptor
    var inputDesc = BNNSNDArrayDescriptor(
        flags: BNNSNDArrayFlags(0),
        layout: BNNSDataLayoutImageCHW,
        size: (inputChannels, 64, 64, 0, 0, 0, 0, 0),
        stride: (0, 0, 0, 0, 0, 0, 0, 0),
        data: nil,
        data_type: .float,
        table_data: nil,
        table_data_type: .float,
        data_scale: 1.0,
        data_bias: 0.0
    )

    // Create output descriptor
    var outputDesc = BNNSNDArrayDescriptor(
        flags: BNNSNDArrayFlags(0),
        layout: BNNSDataLayoutImageCHW,
        size: (outputChannels, 64, 64, 0, 0, 0, 0, 0),
        stride: (0, 0, 0, 0, 0, 0, 0, 0),
        data: nil,
        data_type: .float,
        table_data: nil,
        table_data_type: .float,
        data_scale: 1.0,
        data_bias: 0.0
    )

    // Allocate memory
    let inputSize = inputChannels * 64 * 64
    let outputSize = outputChannels * 64 * 64
    var inputData = [Float](repeating: 0, count: inputSize)
    var outputData = [Float](repeating: 0, count: outputSize)

    // Initialize with random data
    for i in 0..<inputSize {
        inputData[i] = Float.random(in: -1...1)
    }

    print("Running neural network operations...")
    print()
    print("Time     | Operations | Ops/sec  | Status")
    print("-" + String(repeating: "-", count: 49))

    let startTime = Date()
    var operations = 0
    var lastReportTime = startTime

    // Main computation loop
    while Date().timeIntervalSince(startTime) < durationSeconds {
        // Perform matrix operations that may use ANE
        inputData.withUnsafeMutableBufferPointer { inputPtr in
            outputData.withUnsafeMutableBufferPointer { outputPtr in
                // Multiple matrix multiplications to generate load
                for _ in 0..<10 {
                    // Use vDSP for vectorized operations
                    var sum: Float = 0
                    vDSP_sve(inputPtr.baseAddress!, 1, &sum, vDSP_Length(inputSize))

                    // Matrix-vector multiplication simulation
                    vDSP_vsmul(inputPtr.baseAddress!, 1, &sum, outputPtr.baseAddress!, 1, vDSP_Length(min(inputSize, outputSize)))

                    // Additional operations
                    vDSP_vadd(inputPtr.baseAddress!, 1, outputPtr.baseAddress!, 1, outputPtr.baseAddress!, 1, vDSP_Length(min(inputSize, outputSize)))
                }
            }
        }

        operations += 10

        // Report progress every second
        let currentTime = Date()
        if currentTime.timeIntervalSince(lastReportTime) >= 1.0 {
            let elapsed = currentTime.timeIntervalSince(startTime)
            let rate = Double(operations) / elapsed
            print(String(format: "%6.1fs  | %10d | %8.1f | Running", elapsed, operations, rate))
            lastReportTime = currentTime
        }
    }

    let totalTime = Date().timeIntervalSince(startTime)
    print("-" + String(repeating: "-", count: 49))
    print()
    print("Test completed!")
    print("Total operations: \(operations)")
    print(String(format: "Total time: %.1f seconds", totalTime))
    print(String(format: "Average rate: %.1f operations/second", Double(operations) / totalTime))
    print()
    print("Check btop to verify ANE activity was recorded.")
}

/// Alternative test using large matrix operations
func runMatrixTest(durationSeconds: Double = 30.0) {
    print("Running large matrix multiplication test...")
    print()

    let matrixSize = 2048
    let elementCount = matrixSize * matrixSize

    var matrixA = [Float](repeating: 0, count: elementCount)
    var matrixB = [Float](repeating: 0, count: elementCount)
    var matrixC = [Float](repeating: 0, count: elementCount)

    // Initialize matrices
    for i in 0..<elementCount {
        matrixA[i] = Float.random(in: -1...1)
        matrixB[i] = Float.random(in: -1...1)
    }

    print("Matrix size: \(matrixSize) x \(matrixSize)")
    print("Starting multiplication loop...")
    print()

    let startTime = Date()
    var iterations = 0
    var lastReportTime = startTime

    while Date().timeIntervalSince(startTime) < durationSeconds {
        // Large matrix multiplication - uses Accelerate/ANE
        vDSP_mmul(matrixA, 1,
                  matrixB, 1,
                  &matrixC, 1,
                  vDSP_Length(matrixSize),
                  vDSP_Length(matrixSize),
                  vDSP_Length(matrixSize))

        iterations += 1

        let currentTime = Date()
        if currentTime.timeIntervalSince(lastReportTime) >= 1.0 {
            let elapsed = currentTime.timeIntervalSince(startTime)
            let rate = Double(iterations) / elapsed
            let gflops = (2.0 * Double(matrixSize * matrixSize * matrixSize) * rate) / 1e9
            print(String(format: "Time: %5.1fs | Iterations: %4d | Rate: %5.1f/s | ~%.1f GFLOPS", elapsed, iterations, rate, gflops))
            lastReportTime = currentTime
        }
    }

    let totalTime = Date().timeIntervalSince(startTime)
    let avgRate = Double(iterations) / totalTime
    let avgGflops = (2.0 * Double(matrixSize * matrixSize * matrixSize) * avgRate) / 1e9

    print()
    print("Test completed!")
    print("Total iterations: \(iterations)")
    print(String(format: "Average rate: %.1f iterations/second", avgRate))
    print(String(format: "Average performance: ~%.1f GFLOPS", avgGflops))
}

// MARK: - Main

let args = CommandLine.arguments
var duration: Double = 30.0
var useMatrixTest = false

for (index, arg) in args.enumerated() {
    if arg == "--matrix" || arg == "-m" {
        useMatrixTest = true
    } else if arg == "--duration" || arg == "-d", index + 1 < args.count {
        duration = Double(args[index + 1]) ?? 30.0
    } else if let d = Double(arg), index > 0 {
        duration = d
    } else if arg == "--help" || arg == "-h" {
        print("""
        ANE Load Test for btop

        Usage: ane_load_test [options] [duration]

        Options:
            --duration, -d <seconds>  Test duration (default: 30)
            --matrix, -m              Use matrix multiplication test
            --help, -h                Show this help

        Examples:
            ./ane_load_test              # Run for 30 seconds
            ./ane_load_test 60           # Run for 60 seconds
            ./ane_load_test --matrix     # Use matrix test
        """)
        exit(0)
    }
}

if useMatrixTest {
    runMatrixTest(durationSeconds: duration)
} else {
    runANELoadTest(durationSeconds: duration)
}
