// GPU Stress Test for Apple Silicon using Metal
// Compiles with: clang++ -std=c++20 -framework Metal -framework Foundation gpu_stress.mm -o gpu_stress

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include <iostream>
#include <chrono>
#include <thread>

const char* metalShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

kernel void stress_kernel(device float *data [[buffer(0)]],
                          uint id [[thread_position_in_grid]]) {
    float val = data[id];
    // Heavy computation loop
    for (int i = 0; i < 10000; i++) {
        val = sin(val) * cos(val) + tan(val * 0.001);
        val = sqrt(abs(val) + 1.0) * log(abs(val) + 1.0);
        val = pow(abs(val), 0.99) + fmod(val, 3.14159);
    }
    data[id] = val;
}
)";

int main(int argc, char* argv[]) {
    int duration_seconds = 30;
    if (argc > 1) {
        duration_seconds = std::stoi(argv[1]);
    }

    std::cout << "=== Apple Silicon GPU Stress Test ===" << std::endl;
    std::cout << "Duration: " << duration_seconds << " seconds" << std::endl;
    std::cout << "Run btop in another terminal to see GPU usage!" << std::endl;
    std::cout << std::endl;

    @autoreleasepool {
        // Get default Metal device
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            std::cerr << "Metal not supported on this device!" << std::endl;
            return 1;
        }

        std::cout << "GPU: " << [device.name UTF8String] << std::endl;

        // Create command queue
        id<MTLCommandQueue> commandQueue = [device newCommandQueue];

        // Compile shader
        NSError* error = nil;
        NSString* shaderSource = [NSString stringWithUTF8String:metalShaderSource];
        id<MTLLibrary> library = [device newLibraryWithSource:shaderSource options:nil error:&error];
        if (error) {
            std::cerr << "Shader compile error: " << [[error localizedDescription] UTF8String] << std::endl;
            return 1;
        }

        id<MTLFunction> kernelFunction = [library newFunctionWithName:@"stress_kernel"];
        id<MTLComputePipelineState> pipelineState = [device newComputePipelineStateWithFunction:kernelFunction error:&error];
        if (error) {
            std::cerr << "Pipeline error: " << [[error localizedDescription] UTF8String] << std::endl;
            return 1;
        }

        // Create large buffer (64MB of floats)
        const size_t dataSize = 16 * 1024 * 1024; // 16M floats = 64MB
        const size_t bufferSize = dataSize * sizeof(float);

        id<MTLBuffer> buffer = [device newBufferWithLength:bufferSize options:MTLResourceStorageModeShared];
        float* data = (float*)[buffer contents];

        // Initialize with random values
        for (size_t i = 0; i < dataSize; i++) {
            data[i] = (float)(rand() % 1000) / 100.0f;
        }

        std::cout << "Buffer size: " << (bufferSize / 1024 / 1024) << " MB" << std::endl;
        std::cout << std::endl;
        std::cout << "Starting GPU stress..." << std::endl;

        auto startTime = std::chrono::steady_clock::now();
        int iteration = 0;

        while (true) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

            if (elapsed >= duration_seconds) break;

            // Create command buffer and encoder
            id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
            id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];

            [encoder setComputePipelineState:pipelineState];
            [encoder setBuffer:buffer offset:0 atIndex:0];

            // Dispatch threads
            MTLSize gridSize = MTLSizeMake(dataSize, 1, 1);
            NSUInteger threadGroupSize = pipelineState.maxTotalThreadsPerThreadgroup;
            if (threadGroupSize > dataSize) threadGroupSize = dataSize;
            MTLSize threadgroupSize = MTLSizeMake(threadGroupSize, 1, 1);

            [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];
            [encoder endEncoding];

            [commandBuffer commit];
            [commandBuffer waitUntilCompleted];

            iteration++;
            if (iteration % 5 == 0) {
                std::cout << "\rIteration " << iteration << " | Elapsed: " << elapsed << "s / " << duration_seconds << "s" << std::flush;
            }
        }

        std::cout << std::endl << std::endl;
        std::cout << "=== Stress Test Complete ===" << std::endl;
        std::cout << "Total iterations: " << iteration << std::endl;
    }

    return 0;
}
