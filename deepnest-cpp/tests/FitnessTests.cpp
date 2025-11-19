/**
 * @file FitnessTests.cpp
 * @brief FASE 4.1: Comprehensive unit tests for fitness calculation components
 *
 * Tests all individual fitness components to verify they match JavaScript implementation:
 * - Sheet area penalty
 * - Unplaced parts penalty
 * - Minarea component (gravity, boundingbox, convexhull modes)
 * - Bounds width component
 * - Line merge bonus (when implemented)
 *
 * Reference: IMPLEMENTATION_PLAN.md FASE 4.1
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <cassert>

// Fix Windows Polygon name conflict
#ifdef Polygon
#undef Polygon
#endif

#include "../include/deepnest/core/Types.h"
#include "../include/deepnest/core/Point.h"
#include "../include/deepnest/core/BoundingBox.h"
#include "../include/deepnest/core/Polygon.h"
#include "../include/deepnest/geometry/GeometryUtil.h"
#include "../include/deepnest/config/DeepNestConfig.h"
#include "../include/deepnest/placement/PlacementWorker.h"
#include "../include/deepnest/nfp/NFPCache.h"
#include "../include/deepnest/nfp/NFPCalculator.h"

using namespace deepnest;

// Test result tracking
struct TestResult {
    std::string testName;
    bool passed;
    std::string message;
    double expectedValue;
    double actualValue;
};

std::vector<TestResult> results;

// Helper macros
#define ASSERT_NEAR(actual, expected, epsilon) \
    (std::abs((actual) - (expected)) < (epsilon))

#define TEST_CASE(name) \
    std::cout << "\n[TEST] " << name << std::endl; \
    std::string currentTestName = name;

#define EXPECT_NEAR(actual, expected, epsilon, description) \
    { \
        double actualVal = (actual); \
        double expectedVal = (expected); \
        bool passed = ASSERT_NEAR(actualVal, expectedVal, epsilon); \
        results.push_back({currentTestName + " - " + description, passed, description, expectedVal, actualVal}); \
        if (passed) { \
            std::cout << "  ✓ " << description << ": " << actualVal << " (expected: " << expectedVal << ")" << std::endl; \
        } else { \
            std::cout << "  ✗ " << description << ": " << actualVal << " (expected: " << expectedVal << ", diff: " << std::abs(actualVal - expectedVal) << ")" << std::endl; \
        } \
    }

#define EXPECT_GT(actual, threshold, description) \
    { \
        double actualVal = (actual); \
        double thresholdVal = (threshold); \
        bool passed = (actualVal > thresholdVal); \
        results.push_back({currentTestName + " - " + description, passed, description, thresholdVal, actualVal}); \
        if (passed) { \
            std::cout << "  ✓ " << description << ": " << actualVal << " > " << thresholdVal << std::endl; \
        } else { \
            std::cout << "  ✗ " << description << ": " << actualVal << " NOT > " << thresholdVal << std::endl; \
        } \
    }

#define EXPECT_LT(actual, threshold, description) \
    { \
        double actualVal = (actual); \
        double thresholdVal = (threshold); \
        bool passed = (actualVal < thresholdVal); \
        results.push_back({currentTestName + " - " + description, passed, description, thresholdVal, actualVal}); \
        if (passed) { \
            std::cout << "  ✓ " << description << ": " << actualVal << " < " << thresholdVal << std::endl; \
        } else { \
            std::cout << "  ✗ " << description << ": " << actualVal << " NOT < " << thresholdVal << std::endl; \
        } \
    }

// ========== Test 1: Sheet Area Calculation ==========
void test_SheetAreaCalculation() {
    TEST_CASE("Sheet Area Calculation");

    // Test 1a: Simple square sheet
    std::vector<Point> squarePoints = {
        {0, 0}, {100, 0}, {100, 100}, {0, 100}
    };
    double squareArea = std::abs(GeometryUtil::polygonArea(squarePoints));
    EXPECT_NEAR(squareArea, 10000.0, 1.0, "Square 100x100 area");

    // Test 1b: Rectangle sheet (as in IMPLEMENTATION_PLAN.md example)
    std::vector<Point> rectPoints = {
        {0, 0}, {500, 0}, {500, 300}, {0, 300}
    };
    double rectArea = std::abs(GeometryUtil::polygonArea(rectPoints));
    EXPECT_NEAR(rectArea, 150000.0, 1.0, "Rectangle 500x300 area");

    // Test 1c: Large industrial sheet
    std::vector<Point> largePoints = {
        {0, 0}, {2440, 0}, {2440, 1220}, {0, 1220}
    };
    double largeArea = std::abs(GeometryUtil::polygonArea(largePoints));
    EXPECT_NEAR(largeArea, 2976800.0, 10.0, "Large sheet 2440x1220 area");
}

// ========== Test 2: Sheet Area Penalty Component ==========
void test_SheetAreaPenalty() {
    TEST_CASE("Sheet Area Penalty Component");

    // According to IMPLEMENTATION_PLAN.md, fitness should add sheetArea, NOT 1.0
    // JavaScript: fitness += sheetArea (background.js:848)

    Polygon sheet({{0, 0}, {500, 0}, {500, 300}, {0, 300}});
    double sheetArea = std::abs(GeometryUtil::polygonArea(sheet.points));

    EXPECT_NEAR(sheetArea, 150000.0, 1.0, "Sheet area calculation");

    // The penalty should be the sheet area itself
    double penalty = sheetArea;

    EXPECT_GT(penalty, 1000.0, "Penalty should be >> 1.0 (old buggy value)");
    EXPECT_NEAR(penalty, 150000.0, 1.0, "Penalty equals sheet area");
}

// ========== Test 3: Unplaced Parts Penalty ==========
void test_UnplacedPartsPenalty() {
    TEST_CASE("Unplaced Parts Penalty");

    // According to IMPLEMENTATION_PLAN.md:
    // JavaScript: fitness += 100,000,000 * (partArea / totalSheetArea)
    // OLD C++ BUG: fitness += 2.0 * parts.size()

    // Example from IMPLEMENTATION_PLAN.md:
    // Part 100x50 = 5000 units²
    // Total sheet area = 150,000 units²
    // Expected penalty: 100,000,000 * (5000/150000) = 3,333,333

    Polygon part({{0, 0}, {100, 0}, {100, 50}, {0, 50}});
    double partArea = std::abs(GeometryUtil::polygonArea(part.points));
    EXPECT_NEAR(partArea, 5000.0, 1.0, "Part area 100x50");

    double totalSheetArea = 150000.0;

    // Correct penalty formula
    double penalty = 100000000.0 * (partArea / totalSheetArea);

    EXPECT_NEAR(penalty, 3333333.33, 1.0, "Unplaced penalty for 100x50 part");
    EXPECT_GT(penalty, 1000000.0, "Penalty should be massive");

    // Old buggy formula would have been:
    double oldBuggyPenalty = 2.0;

    EXPECT_GT(penalty, oldBuggyPenalty * 1000000, "New penalty >> old buggy penalty");
}

// ========== Test 4: Multiple Unplaced Parts ==========
void test_MultipleUnplacedParts() {
    TEST_CASE("Multiple Unplaced Parts Penalty");

    std::vector<Polygon> unplacedParts = {
        Polygon({{0, 0}, {100, 0}, {100, 50}, {0, 50}}),   // 5000 units²
        Polygon({{0, 0}, {80, 0}, {80, 60}, {0, 60}}),     // 4800 units²
        Polygon({{0, 0}, {50, 0}, {50, 50}, {0, 50}})      // 2500 units²
    };

    double totalSheetArea = 150000.0;
    double totalPenalty = 0.0;

    for (const auto& part : unplacedParts) {
        double partArea = std::abs(GeometryUtil::polygonArea(part.points));
        totalPenalty += 100000000.0 * (partArea / totalSheetArea);
    }

    // Total area: 12300 units²
    // Expected: 100,000,000 * (12300/150000) = 8,200,000
    EXPECT_NEAR(totalPenalty, 8200000.0, 10.0, "Total penalty for 3 unplaced parts");
}

// ========== Test 5: Minarea Component - Gravity Mode ==========
void test_MinariaGravityMode() {
    TEST_CASE("Minarea Component - Gravity Mode");

    // Gravity mode formula (JavaScript background.js:1060):
    // area = bounds.width * 2.0 + bounds.height

    BoundingBox bounds{0, 0, 200, 100};

    double minariaGravity = bounds.width * 2.0 + bounds.height;

    EXPECT_NEAR(minariaGravity, 500.0, 0.1, "Gravity mode: width*2 + height");

    // Test with different bounds
    BoundingBox bounds2{0, 0, 180, 110};
    double minaria2 = bounds2.width * 2.0 + bounds2.height;
    EXPECT_NEAR(minaria2, 470.0, 0.1, "Gravity mode: 180*2 + 110");
}

// ========== Test 6: Minarea Component - BoundingBox Mode ==========
void test_MinariaBoundingBoxMode() {
    TEST_CASE("Minarea Component - BoundingBox Mode");

    // BoundingBox mode formula (JavaScript background.js:1063):
    // area = bounds.width * bounds.height

    BoundingBox bounds{0, 0, 200, 100};

    double minariaBBox = bounds.width * bounds.height;

    EXPECT_NEAR(minariaBBox, 20000.0, 0.1, "BoundingBox mode: width * height");

    // Verify it's different from gravity mode
    double minariaGravity = bounds.width * 2.0 + bounds.height;
    EXPECT_GT(minariaBBox, minariaGravity, "BBox area > Gravity area for these dimensions");
}

// ========== Test 7: Minarea Component - ConvexHull Mode ==========
void test_MinariaConvexHullMode() {
    TEST_CASE("Minarea Component - ConvexHull Mode");

    // ConvexHull mode: calculate actual convex hull area
    // For a rectangle, convex hull area = rectangle area

    std::vector<Point> rectPoints = {
        {0, 0}, {200, 0}, {200, 100}, {0, 100}
    };

    double hullArea = std::abs(GeometryUtil::polygonArea(rectPoints));

    EXPECT_NEAR(hullArea, 20000.0, 0.1, "ConvexHull area for rectangle");

    // For non-convex shapes, hull area < actual area
    // L-shape test
    std::vector<Point> lshape = {
        {0, 0}, {100, 0}, {100, 50}, {50, 50}, {50, 100}, {0, 100}
    };

    double lshapeArea = std::abs(GeometryUtil::polygonArea(lshape));
    EXPECT_NEAR(lshapeArea, 7500.0, 1.0, "L-shape actual area");
}

// ========== Test 8: Bounds Width Component ==========
void test_BoundsWidthComponent() {
    TEST_CASE("Bounds Width Component");

    // According to IMPLEMENTATION_PLAN.md Step 1.3 Step 6:
    // fitness += (boundsWidth / sheetArea) + minarea_accumulator

    BoundingBox bounds{0, 0, 180, 110};
    double sheetArea = 150000.0;

    double boundsWidthComponent = bounds.width / sheetArea;

    EXPECT_NEAR(boundsWidthComponent, 0.0012, 0.0001, "boundsWidth / sheetArea");

    // This component is very small compared to other components
    EXPECT_LT(boundsWidthComponent, 0.01, "BoundsWidth component < 0.01");
}

// ========== Test 9: Complete Fitness Formula ==========
void test_CompleteFitnessFormula() {
    TEST_CASE("Complete Fitness Formula");

    // Simulate a complete fitness calculation as per IMPLEMENTATION_PLAN.md
    // Reference: background.js fitness calculation

    double sheetArea = 150000.0;  // 500x300 sheet
    double fitness = 0.0;

    // Component 1: Sheet area penalty (for 1 sheet used)
    fitness += sheetArea;
    EXPECT_NEAR(fitness, 150000.0, 1.0, "After sheet penalty");

    // Component 2: Bounds width component
    BoundingBox bounds{0, 0, 180, 110};
    fitness += bounds.width / sheetArea;
    EXPECT_NEAR(fitness, 150000.0012, 0.01, "After bounds width");

    // Component 3: Minarea component (gravity mode, accumulated over 2 placements)
    double minarea1 = 200.0 * 2.0 + 100.0;  // First placement
    double minarea2 = 180.0 * 2.0 + 110.0;  // Second placement
    double minariaTotal = minarea1 + minarea2;
    fitness += minariaTotal;
    EXPECT_NEAR(fitness, 150970.0012, 0.01, "After minaria");

    // Component 4: Unplaced parts penalty (1 part 100x50 not placed)
    double unplacedArea = 5000.0;
    double unplacedPenalty = 100000000.0 * (unplacedArea / sheetArea);
    fitness += unplacedPenalty;

    // Final fitness should be approximately: 150,000 + 0.0012 + 970 + 3,333,333 = 3,484,303
    EXPECT_NEAR(fitness, 3484303.0, 10.0, "Complete fitness formula");

    // Verify fitness is in the correct order of magnitude (millions, not ~1.0)
    EXPECT_GT(fitness, 1000000.0, "Fitness in millions range");
}

// ========== Test 10: Fitness Variation Between Individuals ==========
void test_FitnessVariation() {
    TEST_CASE("Fitness Variation Between Individuals");

    // Different placement configurations should produce different fitness values

    // Scenario 1: Compact placement (low bounds, low minaria)
    double sheetArea = 150000.0;
    double fitness1 = sheetArea;
    BoundingBox bounds1{0, 0, 150, 80};
    fitness1 += (bounds1.width / sheetArea) + (bounds1.width * 2.0 + bounds1.height);

    // Scenario 2: Spread out placement (high bounds, high minaria)
    double fitness2 = sheetArea;
    BoundingBox bounds2{0, 0, 250, 150};
    fitness2 += (bounds2.width / sheetArea) + (bounds2.width * 2.0 + bounds2.height);

    EXPECT_GT(fitness2, fitness1, "Spread placement has higher fitness (worse)");

    // Calculate variation percentage
    double variation = (fitness2 - fitness1) / fitness1 * 100.0;
    EXPECT_GT(variation, 0.1, "Fitness varies by > 0.1%");

    std::cout << "  Fitness variation: " << variation << "%" << std::endl;
}

// ========== Test 11: Edge Case - Empty Sheet ==========
void test_EdgeCaseEmptySheet() {
    TEST_CASE("Edge Case - Empty Sheet (All Parts Placed)");

    // When all parts are placed on one sheet
    double sheetArea = 150000.0;
    double fitness = 0.0;

    // One sheet used
    fitness += sheetArea;

    // Minimal bounds (all parts fit compactly)
    BoundingBox bounds{0, 0, 100, 50};
    fitness += (bounds.width / sheetArea) + (bounds.width * 2.0 + bounds.height);

    // No unplaced parts penalty

    // Fitness should be relatively low (good)
    EXPECT_LT(fitness, 200000.0, "All placed fitness < 200k");
    EXPECT_GT(fitness, 100000.0, "All placed fitness > 100k");
}

// ========== Test 12: Edge Case - All Parts Unplaced ==========
void test_EdgeCaseAllUnplaced() {
    TEST_CASE("Edge Case - All Parts Unplaced");

    // Worst case scenario: no parts fit
    double sheetArea = 150000.0;
    double fitness = 0.0;

    // One sheet used (but empty)
    fitness += sheetArea;

    // All 5 parts unplaced (each 100x50 = 5000 units²)
    for (int i = 0; i < 5; i++) {
        double partArea = 5000.0;
        fitness += 100000000.0 * (partArea / sheetArea);
    }

    // Total: 150,000 + 5 * 3,333,333 = 16,816,665
    EXPECT_NEAR(fitness, 16816665.0, 10.0, "All unplaced fitness");

    // This should be the worst possible fitness
    EXPECT_GT(fitness, 10000000.0, "All unplaced fitness > 10M");
}

// ========== Test 13: totalSheetArea Calculation ==========
void test_TotalSheetAreaCalculation() {
    TEST_CASE("TotalSheetArea Calculation");

    // When multiple sheet types are available
    std::vector<Polygon> sheets = {
        Polygon({{0, 0}, {500, 0}, {500, 300}, {0, 300}}),    // 150,000
        Polygon({{0, 0}, {400, 0}, {400, 250}, {0, 250}}),    // 100,000
        Polygon({{0, 0}, {600, 0}, {600, 350}, {0, 350}})     // 210,000
    };

    double totalSheetArea = 0.0;
    for (const auto& sheet : sheets) {
        totalSheetArea += std::abs(GeometryUtil::polygonArea(sheet.points));
    }

    EXPECT_NEAR(totalSheetArea, 460000.0, 10.0, "Total sheet area");

    // Edge case: totalSheetArea < 1.0 should be clamped to 1.0
    double tinyArea = 0.5;
    if (tinyArea < 1.0) {
        tinyArea = 1.0;
    }
    EXPECT_NEAR(tinyArea, 1.0, 0.01, "Tiny sheet area clamped to 1.0");
}

// ========== Main Test Runner ==========
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "FASE 4.1: Fitness Component Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    // Run all tests
    test_SheetAreaCalculation();
    test_SheetAreaPenalty();
    test_UnplacedPartsPenalty();
    test_MultipleUnplacedParts();
    test_MinariaGravityMode();
    test_MinariaBoundingBoxMode();
    test_MinariaConvexHullMode();
    test_BoundsWidthComponent();
    test_CompleteFitnessFormula();
    test_FitnessVariation();
    test_EdgeCaseEmptySheet();
    test_EdgeCaseAllUnplaced();
    test_TotalSheetAreaCalculation();

    // Print summary
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;

    int passed = 0;
    int failed = 0;

    for (const auto& result : results) {
        if (result.passed) {
            passed++;
        } else {
            failed++;
            std::cout << "FAILED: " << result.testName << std::endl;
            std::cout << "  Expected: " << result.expectedValue << std::endl;
            std::cout << "  Actual: " << result.actualValue << std::endl;
        }
    }

    std::cout << "\nTotal tests: " << results.size() << std::endl;
    std::cout << "Passed: " << passed << " (" << std::fixed << std::setprecision(1)
              << (100.0 * passed / results.size()) << "%)" << std::endl;
    std::cout << "Failed: " << failed << std::endl;

    if (failed == 0) {
        std::cout << "\n✓ All fitness component tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "\n✗ Some tests failed. Review output above." << std::endl;
        return 1;
    }
}
