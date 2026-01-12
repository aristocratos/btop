#!/usr/bin/env swift
/*
 * ANE (Apple Neural Engine) Long-Running Comprehensive Test for btop
 *
 * This test creates sustained ANE load using multiple approaches:
 * - Vision framework (image classification, face detection, text recognition)
 * - CoreML operations (MLMultiArray processing)
 * - Accelerate/BNNS (neural network operations)
 * - NaturalLanguage framework (text embedding, sentiment analysis)
 *
 * Designed for extended stress testing to validate btop ANE monitoring.
 *
 * Compile:
 *   swiftc -O -o ane_long_test ane_long_test.swift \
 *     -framework Vision -framework CoreML -framework CoreImage \
 *     -framework AppKit -framework Accelerate -framework NaturalLanguage
 *
 * Run:
 *   ./ane_long_test                    # Default 5 minute test
 *   ./ane_long_test --duration 3600    # 1 hour test
 *   ./ane_long_test --mode burst       # Burst load pattern
 *   ./ane_long_test --intensity high   # Maximum ANE utilization
 */

import Foundation
import Vision
import CoreML
import CoreImage
import AppKit
import Accelerate
import NaturalLanguage

// MARK: - Configuration

struct TestConfiguration {
    var durationSeconds: Double = 300  // 5 minutes default
    var intensity: Intensity = .medium
    var mode: TestMode = .sustained
    var verbose: Bool = false
    var reportInterval: Double = 5.0
    var enableVision: Bool = true
    var enableCoreML: Bool = true
    var enableBNNS: Bool = true
    var enableNaturalLanguage: Bool = true
    var warmupSeconds: Double = 10.0
    var cooldownSeconds: Double = 5.0
    var phaseCount: Int = 5

    enum Intensity: String, CaseIterable {
        case low = "low"
        case medium = "medium"
        case high = "high"
        case extreme = "extreme"

        var multiplier: Int {
            switch self {
            case .low: return 1
            case .medium: return 3
            case .high: return 8
            case .extreme: return 15
            }
        }

        var description: String {
            switch self {
            case .low: return "Low (light ANE usage)"
            case .medium: return "Medium (moderate ANE usage)"
            case .high: return "High (heavy ANE usage)"
            case .extreme: return "Extreme (maximum ANE stress)"
            }
        }
    }

    enum TestMode: String, CaseIterable {
        case sustained = "sustained"
        case burst = "burst"
        case cyclic = "cyclic"
        case progressive = "progressive"
        case random = "random"

        var description: String {
            switch self {
            case .sustained: return "Sustained constant load"
            case .burst: return "High-intensity bursts with rest"
            case .cyclic: return "Cycling through intensity levels"
            case .progressive: return "Gradually increasing load"
            case .random: return "Random load variations"
            }
        }
    }
}

// MARK: - Statistics Tracking

class TestStatistics {
    var visionInferences: Int = 0
    var coreMLOperations: Int = 0
    var bnnsOperations: Int = 0
    var nlpOperations: Int = 0
    var totalOperations: Int { visionInferences + coreMLOperations + bnnsOperations + nlpOperations }

    var classificationsFound: Int = 0
    var facesDetected: Int = 0
    var textRegionsFound: Int = 0
    var sentimentsAnalyzed: Int = 0

    var startTime: Date = Date()
    var phaseStartTime: Date = Date()
    var currentPhase: Int = 0
    var currentIntensity: TestConfiguration.Intensity = .medium

    var errors: [String] = []
    var peakOpsPerSecond: Double = 0
    var lastOpsPerSecond: Double = 0

    func reset() {
        visionInferences = 0
        coreMLOperations = 0
        bnnsOperations = 0
        nlpOperations = 0
        classificationsFound = 0
        facesDetected = 0
        textRegionsFound = 0
        sentimentsAnalyzed = 0
        errors = []
        peakOpsPerSecond = 0
        lastOpsPerSecond = 0
        startTime = Date()
        phaseStartTime = Date()
        currentPhase = 0
    }

    func elapsedTime() -> Double {
        return Date().timeIntervalSince(startTime)
    }

    func phaseElapsedTime() -> Double {
        return Date().timeIntervalSince(phaseStartTime)
    }

    func operationsPerSecond() -> Double {
        let elapsed = elapsedTime()
        guard elapsed > 0 else { return 0 }
        return Double(totalOperations) / elapsed
    }
}

// MARK: - Test Engines

class VisionEngine {
    private var classificationRequest: VNClassifyImageRequest
    private var faceRequest: VNDetectFaceRectanglesRequest
    private var textRequest: VNRecognizeTextRequest
    private var objectRequest: VNDetectRectanglesRequest
    private var saliencyRequest: VNGenerateAttentionBasedSaliencyImageRequest

    init() {
        classificationRequest = VNClassifyImageRequest()
        faceRequest = VNDetectFaceRectanglesRequest()
        textRequest = VNRecognizeTextRequest()
        objectRequest = VNDetectRectanglesRequest()
        saliencyRequest = VNGenerateAttentionBasedSaliencyImageRequest()

        // Configure for best ANE usage
        if #available(macOS 12.0, *) {
            classificationRequest.revision = VNClassifyImageRequestRevision2
        }
        textRequest.recognitionLevel = .accurate
        textRequest.usesLanguageCorrection = true
    }

    func createTestImage(width: Int, height: Int, complexity: Int) -> CGImage? {
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

        // Create complex image patterns for more interesting analysis
        let elements = 20 + complexity * 10
        for _ in 0..<elements {
            let r = CGFloat.random(in: 0...1)
            let g = CGFloat.random(in: 0...1)
            let b = CGFloat.random(in: 0...1)
            context.setFillColor(red: r, green: g, blue: b, alpha: 1.0)

            let shapeType = Int.random(in: 0...3)
            let x = CGFloat.random(in: 0...CGFloat(width))
            let y = CGFloat.random(in: 0...CGFloat(height))
            let w = CGFloat.random(in: 10...CGFloat(width/4))
            let h = CGFloat.random(in: 10...CGFloat(height/4))

            switch shapeType {
            case 0: // Rectangle
                context.fill(CGRect(x: x, y: y, width: w, height: h))
            case 1: // Ellipse
                context.fillEllipse(in: CGRect(x: x, y: y, width: w, height: h))
            case 2: // Line
                context.setStrokeColor(red: r, green: g, blue: b, alpha: 1.0)
                context.setLineWidth(CGFloat.random(in: 1...5))
                context.move(to: CGPoint(x: x, y: y))
                context.addLine(to: CGPoint(x: x + w, y: y + h))
                context.strokePath()
            default: // Triangle
                context.move(to: CGPoint(x: x, y: y))
                context.addLine(to: CGPoint(x: x + w, y: y))
                context.addLine(to: CGPoint(x: x + w/2, y: y + h))
                context.closePath()
                context.fillPath()
            }
        }

        // Add some text-like patterns
        context.setFillColor(gray: 0, alpha: 1)
        for _ in 0..<(complexity * 2) {
            let fontSize = CGFloat.random(in: 8...24)
            let x = CGFloat.random(in: 0...CGFloat(width-100))
            let y = CGFloat.random(in: 0...CGFloat(height-30))

            // Draw rectangles that look like text lines
            for j in 0..<Int.random(in: 3...8) {
                let charWidth = CGFloat.random(in: fontSize * 0.3...fontSize * 0.8)
                context.fill(CGRect(x: x + CGFloat(j) * (charWidth + 2), y: y, width: charWidth, height: fontSize * 0.8))
            }
        }

        return context.makeImage()
    }

    func runInference(image: CGImage, stats: TestStatistics) {
        autoreleasepool {
            let handler = VNImageRequestHandler(cgImage: image, options: [:])

            do {
                // Image classification - primary ANE workload
                try handler.perform([classificationRequest])
                if let results = classificationRequest.results {
                    stats.classificationsFound += results.count
                }
                stats.visionInferences += 1

                // Face detection
                try handler.perform([faceRequest])
                if let results = faceRequest.results {
                    stats.facesDetected += results.count
                }
                stats.visionInferences += 1

                // Text recognition
                try handler.perform([textRequest])
                if let results = textRequest.results {
                    stats.textRegionsFound += results.count
                }
                stats.visionInferences += 1

                // Rectangle detection
                try handler.perform([objectRequest])
                stats.visionInferences += 1

                // Saliency analysis
                try handler.perform([saliencyRequest])
                stats.visionInferences += 1

            } catch {
                stats.errors.append("Vision error: \(error.localizedDescription)")
            }
        }
    }
}

class CoreMLEngine {
    private var inputArrays: [MLMultiArray] = []
    private var outputArrays: [MLMultiArray] = []
    private let sizes: [(Int, Int, Int)] = [
        (1, 512, 512),
        (1, 1024, 1024),
        (3, 224, 224),
        (1, 256, 256),
        (4, 128, 128)
    ]

    init() {
        // Pre-allocate arrays
        for (batch, height, width) in sizes {
            if let arr = try? MLMultiArray(shape: [batch, height, width] as [NSNumber], dataType: .float32) {
                inputArrays.append(arr)
            }
            if let arr = try? MLMultiArray(shape: [batch, height, width] as [NSNumber], dataType: .float32) {
                outputArrays.append(arr)
            }
        }

        // Initialize with random data
        for arr in inputArrays {
            initializeArray(arr)
        }
    }

    private func initializeArray(_ array: MLMultiArray) {
        let count = array.count
        let ptr = array.dataPointer.assumingMemoryBound(to: Float.self)
        for i in 0..<count {
            ptr[i] = Float.random(in: -1...1)
        }
    }

    func runOperations(iterations: Int, stats: TestStatistics) {
        for _ in 0..<iterations {
            autoreleasepool {
                let idx = Int.random(in: 0..<min(inputArrays.count, outputArrays.count))
                guard idx < inputArrays.count && idx < outputArrays.count else { return }

                let input = inputArrays[idx]
                let output = outputArrays[idx]
                let count = input.count

                let inPtr = input.dataPointer.assumingMemoryBound(to: Float.self)
                let outPtr = output.dataPointer.assumingMemoryBound(to: Float.self)

                // Simulate neural network operations
                // ReLU
                for i in 0..<count {
                    outPtr[i] = max(0, inPtr[i])
                }

                // Softmax-like normalization
                var sum: Float = 0
                for i in 0..<min(count, 1000) {
                    sum += exp(outPtr[i])
                }
                for i in 0..<min(count, 1000) {
                    outPtr[i] = exp(outPtr[i]) / sum
                }

                // Batch normalization simulation
                var mean: Float = 0
                vDSP_meanv(outPtr, 1, &mean, vDSP_Length(count))
                var variance: Float = 0
                vDSP_measqv(outPtr, 1, &variance, vDSP_Length(count))
                variance = variance - mean * mean
                let stddev = sqrt(variance + 1e-5)
                var negMean = -mean
                vDSP_vsadd(outPtr, 1, &negMean, outPtr, 1, vDSP_Length(count))
                var invStddev = 1.0 / stddev
                vDSP_vsmul(outPtr, 1, &invStddev, outPtr, 1, vDSP_Length(count))

                stats.coreMLOperations += 1
            }
        }
    }
}

class BNNSEngine {
    private var inputData: [Float]
    private var outputData: [Float]
    private var weightData: [Float]
    private let size: Int = 256 * 256
    private let filterSize: Int = 64 * 64

    init() {
        inputData = [Float](repeating: 0, count: size)
        outputData = [Float](repeating: 0, count: size)
        weightData = [Float](repeating: 0, count: filterSize)

        // Initialize
        for i in 0..<size {
            inputData[i] = Float.random(in: -1...1)
        }
        for i in 0..<filterSize {
            weightData[i] = Float.random(in: -0.1...0.1)
        }
    }

    func runOperations(iterations: Int, stats: TestStatistics) {
        inputData.withUnsafeMutableBufferPointer { inputPtr in
            outputData.withUnsafeMutableBufferPointer { outputPtr in
                weightData.withUnsafeBufferPointer { weightPtr in
                    for _ in 0..<iterations {
                        // Convolution-like operations
                        var sum: Float = 0
                        vDSP_sve(inputPtr.baseAddress!, 1, &sum, vDSP_Length(size))

                        // Matrix multiply simulation
                        vDSP_vsmul(inputPtr.baseAddress!, 1, &sum, outputPtr.baseAddress!, 1, vDSP_Length(size))

                        // Element-wise operations
                        vDSP_vadd(inputPtr.baseAddress!, 1, outputPtr.baseAddress!, 1, outputPtr.baseAddress!, 1, vDSP_Length(size))

                        // Dot product
                        var dotResult: Float = 0
                        vDSP_dotpr(inputPtr.baseAddress!, 1, outputPtr.baseAddress!, 1, &dotResult, vDSP_Length(size))

                        // Large matrix multiplication
                        let matSize = 256
                        vDSP_mmul(inputPtr.baseAddress!, 1,
                                  outputPtr.baseAddress!, 1,
                                  outputPtr.baseAddress!, 1,
                                  vDSP_Length(matSize),
                                  vDSP_Length(matSize),
                                  vDSP_Length(matSize))

                        // FFT-like operations
                        vDSP_vabs(outputPtr.baseAddress!, 1, outputPtr.baseAddress!, 1, vDSP_Length(size))
                        vDSP_vneg(outputPtr.baseAddress!, 1, inputPtr.baseAddress!, 1, vDSP_Length(size))

                        stats.bnnsOperations += 1
                    }
                }
            }
        }
    }
}

class NaturalLanguageEngine {
    private let embedding: NLEmbedding?
    private let sentimentTagger: NLTagger
    private let testTexts: [String] = [
        "The quick brown fox jumps over the lazy dog near the riverbank.",
        "Artificial intelligence and machine learning are transforming industries.",
        "Swift programming language provides powerful features for Apple platforms.",
        "Natural language processing enables computers to understand human text.",
        "The Apple Neural Engine accelerates machine learning workloads efficiently.",
        "Deep learning models require significant computational resources to train.",
        "Computer vision algorithms can identify objects in images automatically.",
        "Text classification helps organize and categorize large document collections.",
        "Sentiment analysis determines the emotional tone of written content.",
        "Named entity recognition extracts important information from text data.",
        "Neural networks learn complex patterns from large datasets effectively.",
        "The transformer architecture revolutionized natural language understanding.",
        "Convolutional neural networks excel at image recognition tasks.",
        "Recurrent neural networks process sequential data like time series.",
        "Attention mechanisms help models focus on relevant input features.",
        "Transfer learning enables models to apply knowledge across domains.",
        "Data augmentation increases the effective size of training datasets.",
        "Regularization techniques prevent neural networks from overfitting.",
        "Gradient descent optimizes model parameters during training iterations.",
        "Hyperparameter tuning significantly affects model performance outcomes."
    ]

    init() {
        embedding = NLEmbedding.wordEmbedding(for: .english)
        sentimentTagger = NLTagger(tagSchemes: [.sentimentScore, .lexicalClass, .nameType])
    }

    func runOperations(iterations: Int, stats: TestStatistics) {
        for _ in 0..<iterations {
            autoreleasepool {
                let text = testTexts[Int.random(in: 0..<testTexts.count)]

                // Word embeddings
                if let emb = embedding {
                    let words = text.split(separator: " ").map(String.init)
                    for word in words {
                        _ = emb.vector(for: word.lowercased())
                    }

                    // Similarity computations
                    if words.count >= 2 {
                        _ = emb.distance(between: words[0].lowercased(), and: words[1].lowercased())
                    }

                    // Nearest neighbors (expensive)
                    if let firstWord = words.first {
                        _ = emb.neighbors(for: firstWord.lowercased(), maximumCount: 5)
                    }
                }

                // Sentiment analysis
                sentimentTagger.string = text
                let range = text.startIndex..<text.endIndex

                sentimentTagger.enumerateTags(in: range, unit: .paragraph, scheme: .sentimentScore) { tag, _ in
                    if tag != nil {
                        stats.sentimentsAnalyzed += 1
                    }
                    return true
                }

                // Lexical analysis
                sentimentTagger.enumerateTags(in: range, unit: .word, scheme: .lexicalClass) { _, _ in
                    return true
                }

                stats.nlpOperations += 1
            }
        }
    }
}

// MARK: - Test Runner

class ANELongTest {
    let config: TestConfiguration
    let stats: TestStatistics

    private var visionEngine: VisionEngine?
    private var coreMLEngine: CoreMLEngine?
    private var bnnsEngine: BNNSEngine?
    private var nlpEngine: NaturalLanguageEngine?

    private var shouldStop = false
    private var lastReportTime: Date = Date()

    init(config: TestConfiguration) {
        self.config = config
        self.stats = TestStatistics()

        if config.enableVision {
            visionEngine = VisionEngine()
        }
        if config.enableCoreML {
            coreMLEngine = CoreMLEngine()
        }
        if config.enableBNNS {
            bnnsEngine = BNNSEngine()
        }
        if config.enableNaturalLanguage {
            nlpEngine = NaturalLanguageEngine()
        }
    }

    func run() {
        printHeader()
        stats.reset()

        // Warmup phase
        if config.warmupSeconds > 0 {
            print("\n🔥 Warmup Phase (\(Int(config.warmupSeconds)) seconds)")
            print(String(repeating: "-", count: 60))
            runPhase(duration: config.warmupSeconds, intensity: .low, name: "Warmup")
        }

        // Main test phases
        print("\n🚀 Main Test (\(Int(config.durationSeconds)) seconds, \(config.mode.description))")
        print(String(repeating: "=", count: 60))

        switch config.mode {
        case .sustained:
            runSustainedMode()
        case .burst:
            runBurstMode()
        case .cyclic:
            runCyclicMode()
        case .progressive:
            runProgressiveMode()
        case .random:
            runRandomMode()
        }

        // Cooldown phase
        if config.cooldownSeconds > 0 {
            print("\n❄️ Cooldown Phase (\(Int(config.cooldownSeconds)) seconds)")
            print(String(repeating: "-", count: 60))
            runPhase(duration: config.cooldownSeconds, intensity: .low, name: "Cooldown")
        }

        printSummary()
    }

    private func runSustainedMode() {
        runPhase(duration: config.durationSeconds, intensity: config.intensity, name: "Sustained")
    }

    private func runBurstMode() {
        let burstDuration = 15.0
        let restDuration = 5.0
        var elapsed: Double = 0
        var burstCount = 0

        while elapsed < config.durationSeconds {
            burstCount += 1
            print("\n⚡ Burst \(burstCount)")
            runPhase(duration: min(burstDuration, config.durationSeconds - elapsed), intensity: .extreme, name: "Burst \(burstCount)")
            elapsed += burstDuration

            if elapsed < config.durationSeconds {
                print("\n😴 Rest period")
                runPhase(duration: min(restDuration, config.durationSeconds - elapsed), intensity: .low, name: "Rest")
                elapsed += restDuration
            }
        }
    }

    private func runCyclicMode() {
        let intensities: [TestConfiguration.Intensity] = [.low, .medium, .high, .extreme, .high, .medium]
        let cycleDuration = config.durationSeconds / Double(intensities.count)

        for (index, intensity) in intensities.enumerated() {
            stats.currentPhase = index + 1
            print("\n🔄 Cycle \(index + 1)/\(intensities.count) - \(intensity.rawValue.capitalized)")
            runPhase(duration: cycleDuration, intensity: intensity, name: "Cycle \(index + 1)")
        }
    }

    private func runProgressiveMode() {
        let phases = config.phaseCount
        let phaseDuration = config.durationSeconds / Double(phases)
        let intensities: [TestConfiguration.Intensity] = [.low, .medium, .high, .extreme]

        for phase in 1...phases {
            let intensityIndex = min(phase - 1, intensities.count - 1)
            let intensity = intensities[intensityIndex]

            stats.currentPhase = phase
            print("\n📈 Phase \(phase)/\(phases) - \(intensity.rawValue.capitalized)")
            runPhase(duration: phaseDuration, intensity: intensity, name: "Phase \(phase)")
        }
    }

    private func runRandomMode() {
        let segmentCount = Int(config.durationSeconds / 20.0)
        let segmentDuration = config.durationSeconds / Double(max(segmentCount, 1))

        for segment in 1...max(segmentCount, 1) {
            let intensity = TestConfiguration.Intensity.allCases.randomElement() ?? .medium
            stats.currentPhase = segment
            print("\n🎲 Segment \(segment)/\(segmentCount) - \(intensity.rawValue.capitalized)")
            runPhase(duration: segmentDuration, intensity: intensity, name: "Segment \(segment)")
        }
    }

    private func runPhase(duration: Double, intensity: TestConfiguration.Intensity, name: String) {
        stats.phaseStartTime = Date()
        stats.currentIntensity = intensity
        let phaseStart = Date()
        lastReportTime = phaseStart

        while Date().timeIntervalSince(phaseStart) < duration && !shouldStop {
            runIteration(intensity: intensity)

            // Progress reporting
            let now = Date()
            if now.timeIntervalSince(lastReportTime) >= config.reportInterval {
                printProgress()
                lastReportTime = now
            }
        }
    }

    private func runIteration(intensity: TestConfiguration.Intensity) {
        let multiplier = intensity.multiplier

        // Vision inference
        if let vision = visionEngine {
            let imageSize = 224 + (intensity.multiplier * 100)
            for _ in 0..<multiplier {
                if let image = vision.createTestImage(width: imageSize, height: imageSize, complexity: multiplier) {
                    vision.runInference(image: image, stats: stats)
                }
            }
        }

        // CoreML operations
        if let coreml = coreMLEngine {
            coreml.runOperations(iterations: multiplier * 5, stats: stats)
        }

        // BNNS operations
        if let bnns = bnnsEngine {
            bnns.runOperations(iterations: multiplier * 10, stats: stats)
        }

        // NLP operations
        if let nlp = nlpEngine {
            nlp.runOperations(iterations: multiplier * 2, stats: stats)
        }

        // Update peak rate
        let currentRate = stats.operationsPerSecond()
        if currentRate > stats.peakOpsPerSecond {
            stats.peakOpsPerSecond = currentRate
        }
        stats.lastOpsPerSecond = currentRate
    }

    private func printHeader() {
        let width = 70
        print()
        print(String(repeating: "═", count: width))
        print("║" + " ANE (Apple Neural Engine) Long-Running Stress Test ".padding(toLength: width - 2, withPad: " ", startingAt: 0) + "║")
        print(String(repeating: "═", count: width))
        print()

        #if !arch(arm64)
        print("⚠️  WARNING: Not running on Apple Silicon - ANE tests will be limited")
        #else
        print("✅ Running on Apple Silicon")
        #endif

        print()
        print("Configuration:")
        print("  • Duration:      \(Int(config.durationSeconds)) seconds (\(formatDuration(config.durationSeconds)))")
        print("  • Intensity:     \(config.intensity.description)")
        print("  • Mode:          \(config.mode.description)")
        print("  • Warmup:        \(Int(config.warmupSeconds)) seconds")
        print("  • Cooldown:      \(Int(config.cooldownSeconds)) seconds")
        print("  • Report every:  \(Int(config.reportInterval)) seconds")
        print()
        print("Engines enabled:")
        print("  • Vision:        \(config.enableVision ? "✅" : "❌")")
        print("  • CoreML:        \(config.enableCoreML ? "✅" : "❌")")
        print("  • BNNS:          \(config.enableBNNS ? "✅" : "❌")")
        print("  • NLP:           \(config.enableNaturalLanguage ? "✅" : "❌")")
        print()
        print("💡 Monitor ANE activity in btop (press '5' for GPU panel)")
        print(String(repeating: "-", count: width))
    }

    private func printProgress() {
        let elapsed = stats.elapsedTime()
        let remaining = config.durationSeconds - elapsed + config.warmupSeconds + config.cooldownSeconds
        let rate = stats.operationsPerSecond()

        let bar = createProgressBar(elapsed: elapsed, total: config.durationSeconds + config.warmupSeconds + config.cooldownSeconds)

        print(String(format: """
        %@ │ %5.0fs │ Ops: %7d │ Rate: %6.0f/s │ V:%4d C:%4d B:%4d N:%3d
        """,
            bar,
            elapsed,
            stats.totalOperations,
            rate,
            stats.visionInferences,
            stats.coreMLOperations,
            stats.bnnsOperations,
            stats.nlpOperations
        ))

        if config.verbose {
            print(String(format: "              │ Classifications: %d │ Faces: %d │ Text regions: %d │ Sentiments: %d",
                stats.classificationsFound, stats.facesDetected, stats.textRegionsFound, stats.sentimentsAnalyzed))
        }
    }

    private func createProgressBar(elapsed: Double, total: Double) -> String {
        let width = 20
        let progress = min(elapsed / total, 1.0)
        let filled = Int(progress * Double(width))
        let empty = width - filled

        let filledStr = String(repeating: "█", count: filled)
        let emptyStr = String(repeating: "░", count: empty)
        let percentage = Int(progress * 100)

        return "[\(filledStr)\(emptyStr)] \(String(format: "%3d", percentage))%"
    }

    private func printSummary() {
        let elapsed = stats.elapsedTime()
        let width = 70

        print()
        print(String(repeating: "═", count: width))
        print("║" + " TEST SUMMARY ".padding(toLength: width - 2, withPad: " ", startingAt: 0) + "║")
        print(String(repeating: "═", count: width))
        print()

        print("Duration:")
        print("  • Total time:    \(formatDuration(elapsed))")
        print("  • Mode:          \(config.mode.description)")
        print("  • Intensity:     \(config.intensity.description)")
        print()

        print("Operations:")
        print("  • Total:         \(formatNumber(stats.totalOperations))")
        print("  • Vision:        \(formatNumber(stats.visionInferences))")
        print("  • CoreML:        \(formatNumber(stats.coreMLOperations))")
        print("  • BNNS:          \(formatNumber(stats.bnnsOperations))")
        print("  • NLP:           \(formatNumber(stats.nlpOperations))")
        print()

        print("Performance:")
        print("  • Average rate:  \(String(format: "%.1f", stats.operationsPerSecond())) ops/sec")
        print("  • Peak rate:     \(String(format: "%.1f", stats.peakOpsPerSecond)) ops/sec")
        print()

        print("Detections:")
        print("  • Classifications: \(formatNumber(stats.classificationsFound))")
        print("  • Faces:           \(formatNumber(stats.facesDetected))")
        print("  • Text regions:    \(formatNumber(stats.textRegionsFound))")
        print("  • Sentiments:      \(formatNumber(stats.sentimentsAnalyzed))")
        print()

        if !stats.errors.isEmpty {
            print("⚠️  Errors encountered: \(stats.errors.count)")
            if config.verbose {
                for error in stats.errors.prefix(10) {
                    print("    • \(error)")
                }
                if stats.errors.count > 10 {
                    print("    ... and \(stats.errors.count - 10) more")
                }
            }
            print()
        }

        print(String(repeating: "═", count: width))
        print()
        print("✅ Test completed! Check btop to verify ANE activity was recorded.")
        print()
    }

    private func formatDuration(_ seconds: Double) -> String {
        let hours = Int(seconds) / 3600
        let minutes = (Int(seconds) % 3600) / 60
        let secs = Int(seconds) % 60

        if hours > 0 {
            return String(format: "%dh %dm %ds", hours, minutes, secs)
        } else if minutes > 0 {
            return String(format: "%dm %ds", minutes, secs)
        } else {
            return String(format: "%ds", secs)
        }
    }

    private func formatNumber(_ n: Int) -> String {
        let formatter = NumberFormatter()
        formatter.numberStyle = .decimal
        return formatter.string(from: NSNumber(value: n)) ?? "\(n)"
    }

    func stop() {
        shouldStop = true
    }
}

// MARK: - Signal Handling

var testRunner: ANELongTest?

func handleSignal(_ signal: Int32) {
    print("\n\n⚠️  Interrupt received, stopping test gracefully...")
    testRunner?.stop()
}

signal(SIGINT, handleSignal)
signal(SIGTERM, handleSignal)

// MARK: - Main

func printUsage() {
    print("""
    ANE Long-Running Test for btop

    Usage: ane_long_test [options]

    Options:
        --duration, -d <seconds>   Test duration (default: 300 = 5 minutes)
        --intensity, -i <level>    Load intensity: low, medium, high, extreme (default: medium)
        --mode, -m <mode>          Test mode: sustained, burst, cyclic, progressive, random (default: sustained)
        --warmup <seconds>         Warmup duration (default: 10)
        --cooldown <seconds>       Cooldown duration (default: 5)
        --report <seconds>         Progress report interval (default: 5)
        --phases <count>           Number of phases for progressive mode (default: 5)
        --verbose, -v              Enable verbose output
        --no-vision                Disable Vision engine
        --no-coreml                Disable CoreML engine
        --no-bnns                  Disable BNNS engine
        --no-nlp                   Disable NaturalLanguage engine
        --help, -h                 Show this help

    Intensity Levels:
        low      - Light ANE usage, minimal system impact
        medium   - Moderate ANE usage, balanced workload
        high     - Heavy ANE usage, significant load
        extreme  - Maximum ANE stress, full utilization

    Test Modes:
        sustained   - Constant load at specified intensity
        burst       - 15s high-intensity bursts with 5s rest
        cyclic      - Cycles through all intensity levels
        progressive - Gradually increases from low to extreme
        random      - Random intensity variations

    Examples:
        ./ane_long_test                              # 5 minute medium test
        ./ane_long_test -d 3600 -i high              # 1 hour high intensity
        ./ane_long_test -d 600 -m burst              # 10 minute burst mode
        ./ane_long_test -d 1800 -m progressive -v    # 30 min progressive, verbose
        ./ane_long_test -d 900 --no-nlp --no-bnns    # 15 min Vision+CoreML only

    Press Ctrl+C to stop the test gracefully at any time.
    """)
}

var config = TestConfiguration()
let args = CommandLine.arguments

var i = 1
while i < args.count {
    let arg = args[i]

    switch arg {
    case "--duration", "-d":
        if i + 1 < args.count, let val = Double(args[i + 1]) {
            config.durationSeconds = val
            i += 1
        }
    case "--intensity", "-i":
        if i + 1 < args.count, let val = TestConfiguration.Intensity(rawValue: args[i + 1].lowercased()) {
            config.intensity = val
            i += 1
        }
    case "--mode", "-m":
        if i + 1 < args.count, let val = TestConfiguration.TestMode(rawValue: args[i + 1].lowercased()) {
            config.mode = val
            i += 1
        }
    case "--warmup":
        if i + 1 < args.count, let val = Double(args[i + 1]) {
            config.warmupSeconds = val
            i += 1
        }
    case "--cooldown":
        if i + 1 < args.count, let val = Double(args[i + 1]) {
            config.cooldownSeconds = val
            i += 1
        }
    case "--report":
        if i + 1 < args.count, let val = Double(args[i + 1]) {
            config.reportInterval = val
            i += 1
        }
    case "--phases":
        if i + 1 < args.count, let val = Int(args[i + 1]) {
            config.phaseCount = val
            i += 1
        }
    case "--verbose", "-v":
        config.verbose = true
    case "--no-vision":
        config.enableVision = false
    case "--no-coreml":
        config.enableCoreML = false
    case "--no-bnns":
        config.enableBNNS = false
    case "--no-nlp":
        config.enableNaturalLanguage = false
    case "--help", "-h":
        printUsage()
        exit(0)
    default:
        // Try parsing as duration
        if let val = Double(arg) {
            config.durationSeconds = val
        }
    }

    i += 1
}

// Run test
testRunner = ANELongTest(config: config)
testRunner?.run()
