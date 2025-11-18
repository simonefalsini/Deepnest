# DeepNest C++ Testing Guide

This directory contains test applications for the DeepNest C++ library.

## Test Applications

### 1. Step Verification Tests (StepVerificationTests)

A comprehensive command-line test suite that verifies all 25 steps of the DeepNest conversion.

**Features:**
- Tests each component systematically
- Provides detailed pass/fail results for each step
- Reports execution time for each test
- Validates:
  - Core types (Point, BoundingBox, Polygon)
  - Configuration system
  - Geometry utilities and operations
  - Convex hull computation
  - 2D transformations
  - NFP calculation and caching
  - Genetic algorithm components
  - Placement strategies
  - Nesting engine
  - DeepNestSolver API
  - Qt-Boost converters
  - SVG loading
  - Random shape generation

**Building with qmake:**
```bash
cd tests
qmake StepVerificationTests.pro
make
./StepVerificationTests
```

**Building with CMake:**
```bash
cd tests
mkdir build_verification
cd build_verification
cmake -f ../CMakeLists_verification.txt ..
make
./StepVerificationTests
```

**Expected Output:**
```
========================================
DeepNest C++ Step Verification Tests
Testing all 25 conversion steps
========================================

Testing Step 01_ProjectStructure... PASS (0.05 ms)
Testing Step 02_BaseTypes... PASS (0.12 ms)
Testing Step 03_Configuration... PASS (0.08 ms)
...
========================================
Test Summary
========================================

Total tests: 25
Passed: 25
Failed: 0
Total time: 45.23 ms

✓ All steps verified successfully!
```

### 2. Qt Test Application (TestApplication)

An interactive GUI application for testing and visualizing the nesting process.

**Features:**
- Load shapes from SVG files
- Generate random test shapes
- Configure nesting parameters
- Real-time visualization
- Progress monitoring
- Export results to SVG

**Building with qmake:**
```bash
cd tests
qmake TestApplication.pro
make
./TestApplication
```

**Building with CMake:**
```bash
cd tests
mkdir build_gui
cd build_gui
cmake ..
make
./TestApplication
```

**Usage:**
1. **Load SVG**: File → Load SVG or press Ctrl+O
2. **Generate Random Shapes**: File → Generate Random or press Ctrl+G
3. **Configure**: Edit → Settings or press Ctrl+,
4. **Start Nesting**: Nest → Start or press F5
5. **Stop Nesting**: Nest → Stop or press Esc
6. **Export Results**: File → Export SVG or press Ctrl+E

## Quick Start

### Running All Tests

To verify the entire implementation:

```bash
# Build the library first
cd deepnest-cpp
qmake
make
sudo make install

# Build and run verification tests
cd tests
qmake StepVerificationTests.pro
make
./StepVerificationTests
```

### Testing with Sample Data

1. Create a test SVG file with shapes:

```svg
<!-- test_shapes.svg -->
<svg xmlns="http://www.w3.org/2000/svg" width="500" height="500">
  <!-- Container (sheet) -->
  <rect id="container" x="0" y="0" width="500" height="400"
        fill="none" stroke="black"/>

  <!-- Parts to nest -->
  <rect x="0" y="0" width="80" height="60" fill="blue"/>
  <rect x="0" y="0" width="100" height="50" fill="red"/>
  <circle cx="40" cy="40" r="30" fill="green"/>
  <path d="M 0 0 L 50 0 L 50 50 L 25 50 L 25 25 L 0 25 Z"
        fill="yellow"/>
</svg>
```

2. Load in TestApplication and run nesting

## Test Coverage

### Core Components (Steps 1-10)

- ✅ **Step 1**: Project structure validation
- ✅ **Step 2**: Point and BoundingBox operations
- ✅ **Step 3**: Configuration get/set operations
- ✅ **Step 4**: Geometry utility functions
- ✅ **Step 5**: Polygon operations (offset, simplify, boolean)
- ✅ **Step 6**: Convex hull computation (Graham scan)
- ✅ **Step 7**: 2D affine transformations
- ✅ **Step 8**: Polygon class with holes support
- ✅ **Step 9**: Thread-safe NFP caching
- ✅ **Step 10**: Minkowski sum computation

### Algorithm Components (Steps 11-15)

- ✅ **Step 11**: NFP calculator with caching
- ✅ **Step 12**: Individual representation and operations
- ✅ **Step 13**: Population management and evolution
- ✅ **Step 14**: Genetic algorithm orchestration
- ✅ **Step 15**: Placement strategies (gravity, bbox, convex)

### Engine Components (Steps 16-20)

- ✅ **Step 16**: Merge lines detection
- ✅ **Step 17**: Placement worker
- ✅ **Step 18**: Parallel processing
- ✅ **Step 19**: Nesting engine coordination
- ✅ **Step 20**: DeepNestSolver API

### Qt Integration (Steps 21-24)

- ✅ **Step 21**: Qt-Boost type converters
- ✅ **Step 22**: Qt GUI test application
- ✅ **Step 23**: SVG file loader and parser
- ✅ **Step 24**: Random shape generator

### Build System (Step 25)

- ✅ **Step 25**: qmake and CMake build configurations

## Debugging Tests

### Enable Verbose Output

Modify `StepVerificationTests.cpp` to add detailed output:

```cpp
#define VERBOSE_TESTS 1

#ifdef VERBOSE_TESTS
    std::cout << "  Detail: " << someDetail << std::endl;
#endif
```

### Run Specific Test

Comment out tests in `main()`:

```cpp
int main() {
    // ... other tests commented out
    testStep11_NFPCalculator();  // Run only this test
    // ...
}
```

### Memory Leak Checking

Using Valgrind on Linux:

```bash
valgrind --leak-check=full ./StepVerificationTests
```

Using Dr. Memory on Windows:

```bash
drmemory -- StepVerificationTests.exe
```

## Performance Benchmarking

The verification tests report execution time for each step. To benchmark specific operations:

```cpp
auto start = std::chrono::high_resolution_clock::now();

// ... operation to benchmark ...

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration<double, std::milli>(end - start).count();
std::cout << "Operation took: " << duration << " ms" << std::endl;
```

## Continuous Integration

For automated testing in CI/CD pipelines:

```bash
#!/bin/bash
# ci_test.sh

set -e  # Exit on error

echo "Building DeepNest C++..."
cd deepnest-cpp
qmake
make

echo "Running verification tests..."
cd tests
qmake StepVerificationTests.pro
make
./StepVerificationTests

if [ $? -eq 0 ]; then
    echo "✓ All tests passed!"
    exit 0
else
    echo "✗ Tests failed!"
    exit 1
fi
```

## Known Issues

1. **Boost.Polygon precision**: Some tests use epsilon comparison (0.01) due to floating-point arithmetic
2. **Qt event loop**: GUI tests (Step 22) verify compilation only, not full runtime behavior
3. **File I/O**: SVG loader tests require valid file paths

## Contributing Tests

When adding new features, please:

1. Add corresponding test in `StepVerificationTests.cpp`
2. Update this documentation
3. Ensure all tests pass before committing
4. Add integration tests in TestApplication if GUI-related

## Support

For issues with tests:
- Check that all dependencies are installed
- Verify library paths in `.pro` or `CMakeLists.txt`
- Review test output for specific failure messages
- Ensure DeepNest library is built and installed

## License

Same as DeepNest C++ (GPLv3)
