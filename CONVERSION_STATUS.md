# DeepNest Integer Conversion - Status Document

Generated: 2025-11-25

## Test Files Documentation

### Core Test Suites

1. **StepVerificationTests.cpp** (31,189 bytes)
   - Validates all 25 implementation steps from conversion_step.md
   - Tests: Types, Point, BoundingBox, GeometryUtil, Polygon operations, NFP, GA, Placement
   - Critical for validating the conversion

2. **GeometryValidationTests.cpp** (22,683 bytes)
   - Tests geometric utility functions
   - Line intersection, point-in-polygon, area calculations
   - Bezier and Arc linearization
   - Will need tolerance updates for integer math

3. **NFPValidationTests.cpp** (31,403 bytes)
   - Validates No-Fit Polygon calculations
   - Tests Minkowski sum approach
   - Compares against reference results
   - Critical for NFP conversion validation

4. **MinkowskiSumComparisonTest.cpp** (15,183 bytes)
   - Compares Minkowski sum with different algorithms
   - Performance and accuracy tests
   - Will validate integer NFP calculations

5. **FitnessTests.cpp** (16,863 bytes)
   - Tests placement fitness calculations
   - Gravity, bounding box, convex hull strategies
   - Needs update for integer coordinates

6. **GeneticAlgorithmTests.cpp** (14,340 bytes)
   - Tests GA components: Individual, Population, crossover, mutation
   - Should mostly work with integer coordinates

7. **OverlapDetectionTest.cpp** (11,777 bytes)
   - Tests overlap detection between parts
   - Uses tolerance parameters - needs integer tolerance

8. **DeepnestLibTests.cpp** (2,328 bytes)
   - High-level library integration tests
   - Will need inputScale parameter

9. **JSComparisonTests.cpp** (15,818 bytes)
   - Compares results with JavaScript version
   - May need tolerance adjustments

### Support Files

10. **TestApplication.cpp** (65,172 bytes)
    - Qt GUI test application
    - Needs inputScale in UI
    - Visualization updates for integer coordinates

11. **SVGLoader.cpp/h** (31,929 + 8,638 bytes)
    - Loads SVG files and converts to Polygon
    - CRITICAL: Must apply inputScale at load time

12. **RandomShapeGenerator.cpp/h** (15,188 + 9,184 bytes)
    - Generates random test shapes
    - Should generate integer coordinates directly

13. **ConfigDialog.cpp/h** (15,451 + 5,436 bytes)
    - Configuration UI
    - Must add inputScale control

### Debug/Utility Tools

14. **NFPDebug.cpp** (1,348 bytes)
    - NFP debugging utilities
    - Update for integer coordinates

15. **SearchStartPointDebug.cpp** (2,273 bytes)
    - Debugs searchStartPoint (WILL BE REMOVED)

16. **PolygonExtractor.cpp** (33,731 bytes)
    - Extracts polygons from various sources
    - Needs inputScale support

17. **DiagnosticTool.cpp** (6,695 bytes)
    - General diagnostics
    - Update for integer types

## Complete File List (76 files)

### Header Files (30 files)

#### Core (4 files)
- deepnest-cpp/include/deepnest/core/BoundingBox.h
- deepnest-cpp/include/deepnest/core/Point.h
- deepnest-cpp/include/deepnest/core/Polygon.h
- deepnest-cpp/include/deepnest/core/Types.h ⭐ CRITICAL - Base types

#### Geometry (6 files)
- deepnest-cpp/include/deepnest/geometry/ConvexHull.h
- deepnest-cpp/include/deepnest/geometry/GeometryUtil.h ⭐ CRITICAL - Base geometry
- deepnest-cpp/include/deepnest/geometry/GeometryUtilAdvanced.h ⭐ Will be cleaned
- deepnest-cpp/include/deepnest/geometry/OrbitalTypes.h ❌ TO REMOVE
- deepnest-cpp/include/deepnest/geometry/PolygonHierarchy.h
- deepnest-cpp/include/deepnest/geometry/PolygonOperations.h
- deepnest-cpp/include/deepnest/geometry/Transformation.h

#### NFP (3 files)
- deepnest-cpp/include/deepnest/nfp/MinkowskiSum.h ⭐ CRITICAL - NFP calculation
- deepnest-cpp/include/deepnest/nfp/NFPCache.h
- deepnest-cpp/include/deepnest/nfp/NFPCalculator.h

#### Algorithm (3 files)
- deepnest-cpp/include/deepnest/algorithm/GeneticAlgorithm.h
- deepnest-cpp/include/deepnest/algorithm/Individual.h
- deepnest-cpp/include/deepnest/algorithm/Population.h

#### Placement (3 files)
- deepnest-cpp/include/deepnest/placement/MergeDetection.h
- deepnest-cpp/include/deepnest/placement/PlacementStrategy.h
- deepnest-cpp/include/deepnest/placement/PlacementWorker.h

#### Engine & Config (2 files)
- deepnest-cpp/include/deepnest/config/DeepNestConfig.h ⭐ CRITICAL - Add inputScale
- deepnest-cpp/include/deepnest/engine/NestingEngine.h

#### Other (4 files)
- deepnest-cpp/include/deepnest/converters/QtBoostConverter.h ⭐ CRITICAL - Scaling
- deepnest-cpp/include/deepnest/parallel/ParallelProcessor.h
- deepnest-cpp/include/deepnest/DebugConfig.h
- deepnest-cpp/include/deepnest/DeepNestSolver.h ⭐ CRITICAL - Public API

#### Test Headers (5 files)
- deepnest-cpp/tests/ConfigDialog.h
- deepnest-cpp/tests/ContainerDialog.h
- deepnest-cpp/tests/RandomShapeGenerator.h
- deepnest-cpp/tests/SVGLoader.h ⭐ Add inputScale
- deepnest-cpp/tests/TestApplication.h
- deepnest-cpp/tests/TestRunners.h

### Implementation Files (46 files)

#### Core (4 files)
- deepnest-cpp/src/core/Point.cpp
- deepnest-cpp/src/core/Polygon.cpp
- deepnest-cpp/src/core/Types.cpp
- deepnest-cpp/src/core/BoundingBox.cpp (if exists)

#### Geometry (7 files)
- deepnest-cpp/src/geometry/ConvexHull.cpp
- deepnest-cpp/src/geometry/GeometryUtil.cpp ⭐ CRITICAL - Clean & convert
- deepnest-cpp/src/geometry/GeometryUtilAdvanced.cpp ⭐ Clean & convert
- deepnest-cpp/src/geometry/OrbitalHelpers.cpp ❌ TO REMOVE
- deepnest-cpp/src/geometry/PolygonHierarchy.cpp
- deepnest-cpp/src/geometry/PolygonOperations.cpp
- deepnest-cpp/src/geometry/Transformation.cpp

#### NFP (3 files)
- deepnest-cpp/src/nfp/MinkowskiSum.cpp ⭐ CRITICAL
- deepnest-cpp/src/nfp/NFPCache.cpp
- deepnest-cpp/src/nfp/NFPCalculator.cpp

#### Algorithm (3 files)
- deepnest-cpp/src/algorithm/GeneticAlgorithm.cpp
- deepnest-cpp/src/algorithm/Individual.cpp
- deepnest-cpp/src/algorithm/Population.cpp

#### Placement (3 files)
- deepnest-cpp/src/placement/MergeDetection.cpp
- deepnest-cpp/src/placement/PlacementStrategy.cpp
- deepnest-cpp/src/placement/PlacementWorker.cpp

#### Engine & Config (3 files)
- deepnest-cpp/src/config/DeepNestConfig.cpp ⭐ Add inputScale
- deepnest-cpp/src/engine/NestingEngine.cpp
- deepnest-cpp/src/DeepNestSolver.cpp ⭐ Public API

#### Other (2 files)
- deepnest-cpp/src/converters/QtBoostConverter.cpp ⭐ CRITICAL - Scaling
- deepnest-cpp/src/parallel/ParallelProcessor.cpp

#### Test Files (19 files)
- deepnest-cpp/tests/ConfigDialog.cpp
- deepnest-cpp/tests/ContainerDialog.cpp
- deepnest-cpp/tests/DeepnestLibTests.cpp
- deepnest-cpp/tests/DiagnosticTool.cpp
- deepnest-cpp/tests/FitnessTests.cpp
- deepnest-cpp/tests/GeneticAlgorithmTests.cpp
- deepnest-cpp/tests/GeometryValidationTests.cpp
- deepnest-cpp/tests/JSComparisonTests.cpp
- deepnest-cpp/tests/MinkowskiSumComparisonTest.cpp
- deepnest-cpp/tests/NFPDebug.cpp
- deepnest-cpp/tests/NFPValidationTests.cpp
- deepnest-cpp/tests/OverlapDetectionTest.cpp
- deepnest-cpp/tests/PolygonExtractor.cpp
- deepnest-cpp/tests/RandomShapeGenerator.cpp
- deepnest-cpp/tests/SearchStartPointDebug.cpp ❌ TO REMOVE/UPDATE
- deepnest-cpp/tests/StepVerificationTests.cpp ⭐ CRITICAL
- deepnest-cpp/tests/SVGLoader.cpp ⭐ Add inputScale
- deepnest-cpp/tests/TestApplication.cpp
- deepnest-cpp/tests/main.cpp

#### Other
- deepnest-cpp/trace_test.cpp (root test file)

## Legend
⭐ CRITICAL - Must be modified for conversion to work
❌ TO REMOVE - Will be deleted (noFitPolygon and related)
🔧 UPDATE - Needs updates for integer math

## Summary

- **Total files**: 76
- **Critical files**: 12
- **Files to remove**: 2-3 (OrbitalTypes.h, OrbitalHelpers.cpp, SearchStartPointDebug.cpp)
- **Test files**: 19
- **Header files**: 30
- **Implementation files**: 46

## Double/Float Usage Analysis

Will be performed in next step (grep for double/float in all files).

## Build System

- **Primary**: CMake (deepnest-cpp/CMakeLists.txt)
- **Legacy**: qmake (*.pro files)
- **Conversion**: Use CMake for building

## Double/Float Usage Analysis

**Total occurrences**: 625 in include/ and src/

### Top 30 Files by Double/Float Usage

```
112 src/geometry/GeometryUtil.cpp                    ⭐⭐⭐ HIGHEST PRIORITY
 69 src/geometry/GeometryUtilAdvanced.cpp            ⭐⭐⭐ HIGH (will be cleaned)
 36 include/deepnest/geometry/GeometryUtil.h         ⭐⭐⭐ CRITICAL
 33 src/placement/PlacementStrategy.cpp              ⭐⭐ HIGH
 31 src/nfp/MinkowskiSum.cpp                         ⭐⭐⭐ CRITICAL NFP
 31 include/deepnest/core/BoundingBox.h              ⭐⭐⭐ CRITICAL
 26 src/geometry/Transformation.cpp                  ⭐⭐ HIGH
 25 include/deepnest/core/Point.h                    ⭐⭐⭐ CRITICAL
 23 src/placement/MergeDetection.cpp                 ⭐⭐ MEDIUM
 21 src/placement/PlacementWorker.cpp                ⭐⭐ HIGH
 17 src/nfp/NFPCalculator.cpp                        ⭐⭐ HIGH
 17 include/deepnest/geometry/Transformation.h       ⭐⭐ HIGH
 15 src/core/Polygon.cpp                             ⭐⭐ HIGH
 14 src/geometry/PolygonOperations.cpp               ⭐⭐ MEDIUM
 13 include/deepnest/config/DeepNestConfig.h         ⭐ LOW (some stay double)
 11 src/algorithm/Individual.cpp                     ⭐ LOW
 11 include/deepnest/geometry/PolygonOperations.h    ⭐⭐ MEDIUM
 10 include/deepnest/core/Polygon.h                  ⭐⭐ HIGH
  9 src/geometry/ConvexHull.cpp                      ⭐⭐ MEDIUM
  9 include/deepnest/nfp/NFPCache.h                  ⭐ LOW
  9 include/deepnest/nfp/MinkowskiSum.h              ⭐⭐ HIGH
  8 src/geometry/OrbitalHelpers.cpp                  ❌ TO REMOVE
  8 src/algorithm/Population.cpp                     ⭐ LOW
  8 include/deepnest/placement/PlacementStrategy.h   ⭐⭐ MEDIUM
  6 src/engine/NestingEngine.cpp                     ⭐ MEDIUM
  6 include/deepnest/placement/PlacementWorker.h     ⭐ MEDIUM
  6 include/deepnest/placement/MergeDetection.h      ⭐ MEDIUM
  6 include/deepnest/engine/NestingEngine.h          ⭐ MEDIUM
  6 include/deepnest/algorithm/Individual.h          ⭐ LOW
  5 include/deepnest/geometry/GeometryUtilAdvanced.h ⭐⭐ HIGH
```

### Priority Categories

**⭐⭐⭐ CRITICAL (Must convert first)**:
- Types.h (base types)
- Point.h / BoundingBox.h (coordinate types)
- GeometryUtil.h/.cpp (fundamental geometry)
- MinkowskiSum (NFP calculation)

**⭐⭐ HIGH (Convert early)**:
- Transformation (rotations)
- PlacementStrategy/Worker (placement logic)
- Polygon operations
- NFPCalculator

**⭐ MEDIUM/LOW (Convert later)**:
- GA components (mostly work with references)
- Config (some params stay double like inputScale)
- Engine/Solver (coordinate pass-through)

**❌ TO REMOVE**:
- OrbitalHelpers.cpp (8 occurrences - will be deleted)

## Notes

- Some double usage is legitimate (e.g., inputScale, fitness values, angles)
- Focus conversion on coordinate types and geometric calculations
- Math functions (sqrt, sin, cos) may use double for intermediate calculations

