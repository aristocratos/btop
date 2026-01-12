#!/usr/bin/env python3
"""
ANE (Apple Neural Engine) Load Test for btop

This script generates load on the Apple Neural Engine to verify
that btop correctly displays ANE activity and power metrics.

Requirements:
- macOS with Apple Silicon (M1/M2/M3/M4)
- Python 3.8+
- coremltools: pip install coremltools
- numpy: pip install numpy

Usage:
    python3 ane_load_test.py [--duration SECONDS] [--intensity low|medium|high]
"""

import argparse
import time
import sys
import os

def check_apple_silicon():
    """Check if running on Apple Silicon."""
    import platform
    if platform.machine() != 'arm64':
        print("Error: This test requires Apple Silicon (M1/M2/M3/M4)")
        sys.exit(1)
    print(f"✓ Running on Apple Silicon ({platform.machine()})")

def create_test_model(size='medium'):
    """Create a simple CoreML model that runs on ANE."""
    try:
        import coremltools as ct
        import numpy as np
    except ImportError:
        print("Error: Required packages not installed.")
        print("Run: pip install coremltools numpy")
        sys.exit(1)

    # Model sizes for different intensity levels
    sizes = {
        'low': (64, 64, 32),
        'medium': (256, 256, 128),
        'high': (512, 512, 256)
    }

    input_size, hidden_size, output_size = sizes.get(size, sizes['medium'])

    print(f"Creating test model (intensity: {size})...")
    print(f"  Input: {input_size}, Hidden: {hidden_size}, Output: {output_size}")

    # Create a simple neural network using coremltools
    from coremltools.converters.mil import Builder as mb
    from coremltools.converters.mil.mil import types

    @mb.program(input_specs=[mb.TensorSpec(shape=(1, input_size), dtype=types.fp16)])
    def simple_nn(x):
        # First layer
        w1 = mb.const(val=np.random.randn(input_size, hidden_size).astype(np.float16) * 0.01)
        x = mb.matmul(x=x, y=w1)
        x = mb.relu(x=x)

        # Second layer
        w2 = mb.const(val=np.random.randn(hidden_size, hidden_size).astype(np.float16) * 0.01)
        x = mb.matmul(x=x, y=w2)
        x = mb.relu(x=x)

        # Third layer
        w3 = mb.const(val=np.random.randn(hidden_size, output_size).astype(np.float16) * 0.01)
        x = mb.matmul(x=x, y=w3)

        return x

    # Convert to CoreML model
    model = ct.convert(
        simple_nn,
        compute_units=ct.ComputeUnit.ALL,  # Allow ANE usage
        minimum_deployment_target=ct.target.macOS13
    )

    return model, input_size

def run_ane_load_test(duration=30, intensity='medium'):
    """Run continuous inference to generate ANE load."""
    import numpy as np

    check_apple_silicon()

    model, input_size = create_test_model(intensity)

    print(f"\n{'='*50}")
    print(f"ANE Load Test")
    print(f"{'='*50}")
    print(f"Duration: {duration} seconds")
    print(f"Intensity: {intensity}")
    print(f"\nMonitor ANE activity in btop (press '5' to show GPU panel)")
    print(f"{'='*50}\n")

    # Prepare input data
    input_data = {'x': np.random.randn(1, input_size).astype(np.float16)}

    start_time = time.time()
    iterations = 0
    last_report = start_time

    print("Running inference loop...")
    print("Time     | Iterations | Iter/sec")
    print("-" * 40)

    try:
        while time.time() - start_time < duration:
            # Run inference - this should use ANE
            _ = model.predict(input_data)
            iterations += 1

            # Report progress every second
            current_time = time.time()
            if current_time - last_report >= 1.0:
                elapsed = current_time - start_time
                rate = iterations / elapsed
                print(f"{elapsed:6.1f}s  | {iterations:10d} | {rate:8.1f}")
                last_report = current_time

    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")

    elapsed = time.time() - start_time
    print("-" * 40)
    print(f"\nTest completed!")
    print(f"Total iterations: {iterations}")
    print(f"Total time: {elapsed:.1f} seconds")
    print(f"Average rate: {iterations/elapsed:.1f} iterations/second")
    print(f"\nCheck btop to verify ANE activity was recorded.")

def run_simple_test():
    """Run a simpler test using built-in macOS ML capabilities."""
    print("Running simple ANE test using Vision framework...")

    check_apple_silicon()

    # Use subprocess to run a simple Swift script that uses Vision/CoreML
    swift_code = '''
import Foundation
import CoreML
import Accelerate

// Simple matrix operations that may use ANE
let size = 1024
var matrix1 = [Float](repeating: 0, count: size * size)
var matrix2 = [Float](repeating: 0, count: size * size)
var result = [Float](repeating: 0, count: size * size)

// Initialize with random values
for i in 0..<(size * size) {
    matrix1[i] = Float.random(in: -1...1)
    matrix2[i] = Float.random(in: -1...1)
}

print("Starting matrix multiplication loop...")
let startTime = Date()
var iterations = 0

while Date().timeIntervalSince(startTime) < 30 {
    // Matrix multiply using Accelerate (may use ANE for large operations)
    vDSP_mmul(matrix1, 1, matrix2, 1, &result, 1, vDSP_Length(size), vDSP_Length(size), vDSP_Length(size))
    iterations += 1

    if iterations % 100 == 0 {
        let elapsed = Date().timeIntervalSince(startTime)
        print("Iterations: \\(iterations), Rate: \\(Double(iterations)/elapsed) iter/s")
    }
}

let elapsed = Date().timeIntervalSince(startTime)
print("\\nCompleted \\(iterations) iterations in \\(elapsed) seconds")
print("Rate: \\(Double(iterations)/elapsed) iterations/second")
'''

    import subprocess
    import tempfile

    with tempfile.NamedTemporaryFile(mode='w', suffix='.swift', delete=False) as f:
        f.write(swift_code)
        swift_file = f.name

    try:
        print("\nCompiling Swift test...")
        compile_result = subprocess.run(
            ['swiftc', '-O', '-o', '/tmp/ane_test', swift_file],
            capture_output=True, text=True
        )

        if compile_result.returncode != 0:
            print(f"Compilation failed: {compile_result.stderr}")
            return

        print("Running test (30 seconds)...")
        print("Monitor ANE activity in btop\n")

        subprocess.run(['/tmp/ane_test'])

    finally:
        os.unlink(swift_file)
        if os.path.exists('/tmp/ane_test'):
            os.unlink('/tmp/ane_test')

def main():
    parser = argparse.ArgumentParser(description='ANE Load Test for btop')
    parser.add_argument('--duration', type=int, default=30,
                        help='Test duration in seconds (default: 30)')
    parser.add_argument('--intensity', choices=['low', 'medium', 'high'],
                        default='medium', help='Test intensity (default: medium)')
    parser.add_argument('--simple', action='store_true',
                        help='Run simple test without coremltools dependency')

    args = parser.parse_args()

    print("=" * 50)
    print("ANE (Apple Neural Engine) Load Test for btop")
    print("=" * 50)
    print()

    if args.simple:
        run_simple_test()
    else:
        try:
            run_ane_load_test(args.duration, args.intensity)
        except ImportError:
            print("\ncoremltools not available, falling back to simple test...")
            run_simple_test()

if __name__ == '__main__':
    main()
