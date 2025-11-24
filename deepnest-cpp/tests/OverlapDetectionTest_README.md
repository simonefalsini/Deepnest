# Overlap Detection Test Suite

## Overview

This test suite validates the `overlapTolerance` parameter implementation and the `hasSignificantOverlap` method in `PlacementWorker`.

## Test Cases

### 1. No Overlap
- **Description:** Two rectangles 20 units apart
- **Expected:** No overlap detected
- **Validates:** Basic non-overlapping case

### 2. Significant Overlap
- **Description:** Two rectangles with 50% overlap
- **Expected:** Overlap detected
- **Validates:** Clear overlap detection

### 3. Tiny Overlap Below Tolerance
- **Description:** Tiny overlap (0.01 area) with tolerance 0.1
- **Expected:** No overlap flagged
- **Validates:** Tolerance threshold working

### 4. Complete Overlap
- **Description:** Identical parts at same position
- **Expected:** Overlap detected
- **Validates:** 100% overlap case

### 5. Edge Touching
- **Description:** Two rectangles touching at edge
- **Expected:** No overlap (zero area)
- **Validates:** Edge case handling

### 6. Different Shapes
- **Description:** Triangle and rectangle overlapping
- **Expected:** Overlap detected
- **Validates:** Works with different polygon types

### 7. Zero Tolerance
- **Description:** Tiny overlap with zero tolerance
- **Expected:** Overlap detected
- **Validates:** Strictest mode

### 8. Bounding Box Optimization
- **Description:** Parts very far apart
- **Expected:** Fast execution via bbox check
- **Validates:** Performance optimization

### 9. Tolerance Threshold Boundary
- **Description:** Overlap area exactly at tolerance
- **Expected:** No overlap (not > tolerance)
- **Validates:** Boundary condition

### 10. Multiple Overlap Checks
- **Description:** 1000 overlap checks
- **Expected:** Completes in reasonable time
- **Validates:** Performance at scale

## Building

### Using qmake
```bash
cd tests
qmake OverlapDetectionTest.pro
make
./OverlapDetectionTest
```

### Using CMake
Add to `tests/CMakeLists.txt`:
```cmake
add_executable(OverlapDetectionTest OverlapDetectionTest.cpp)
target_link_libraries(OverlapDetectionTest deepnest Clipper2)
```

Then:
```bash
cd build
cmake ..
make OverlapDetectionTest
./tests/OverlapDetectionTest
```

## Expected Output

```
╔════════════════════════════════════════════════════════╗
║   Overlap Detection Test Suite                        ║
║   Testing overlapTolerance parameter                  ║
╚════════════════════════════════════════════════════════╝

=== Test 1: No Overlap ===
✓ PASSED: No overlap detected for distant parts

=== Test 2: Significant Overlap ===
✓ PASSED: Overlap detected for 50% overlapping parts

... (8 more tests) ...

╔════════════════════════════════════════════════════════╗
║   ✓ ALL TESTS PASSED (10/10)                          ║
╚════════════════════════════════════════════════════════╝
```

## Performance Benchmarks

Expected performance on modern hardware:
- Single overlap check: < 1ms
- 1000 overlap checks: < 100ms
- Bbox optimization: < 10μs for distant parts

## Integration

This test can be integrated into the main test suite by adding to `main.cpp`:

```cpp
#include "TestRunners.h"

int runOverlapDetectionTests() {
    // Run OverlapDetectionTest
    return system("./OverlapDetectionTest");
}
```

## Troubleshooting

### Compilation Errors

**Missing Clipper2:**
```bash
cd ../external/clipper2/CPP/Build
cmake ..
make
```

**Linking errors:**
Ensure `libdeepnest.a` is built:
```bash
cd ../build
make deepnest
```

### Test Failures

If tests fail, check:
1. `overlapTolerance` default value (should be 0.0001)
2. Clipper2 intersection working correctly
3. Bounding box calculation in `Polygon::bounds()`

## Author

Created as part of overlapTolerance implementation (2025-11-24)

## See Also

- `overlap_tolerance_implementation_plan.md` - Implementation plan
- `PlacementWorker.cpp` - Implementation
- `DeepNestConfig.h` - Configuration
