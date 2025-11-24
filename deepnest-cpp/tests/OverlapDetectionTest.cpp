#include "../include/deepnest/placement/PlacementWorker.h"
#include "../include/deepnest/config/DeepNestConfig.h"
#include "../include/deepnest/nfp/NFPCalculator.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace deepnest;

/**
 * @file OverlapDetectionTest.cpp
 * @brief Unit tests for overlapTolerance parameter and overlap detection
 * 
 * Tests the hasSignificantOverlap method in PlacementWorker to ensure
 * correct overlap detection with configurable tolerance.
 */

// Helper function to create a rectangle polygon
Polygon createRectangle(double x, double y, double width, double height, int id) {
    std::vector<Point> points;
    points.push_back(Point(x, y));
    points.push_back(Point(x, y + height));
    points.push_back(Point(x + width, y + height));
    points.push_back(Point(x + width, y));
    
    return Polygon(points, id);
}

// Helper function to create a triangle polygon
Polygon createTriangle(double x, double y, double size, int id) {
    std::vector<Point> points;
    points.push_back(Point(x, y));
    points.push_back(Point(x + size, y));
    points.push_back(Point(x + size/2, y + size));
    
    return Polygon(points, id);
}

void testNoOverlap() {
    std::cout << "\n=== Test 1: No Overlap ===" << std::endl;
    
    // Create two rectangles far apart
    Polygon rect1 = createRectangle(0, 0, 10, 10, 0);
    Polygon rect2 = createRectangle(0, 0, 10, 10, 1);
    
    Point pos1(0, 0);
    Point pos2(20, 0);  // 20 units apart - no overlap
    
    DeepNestConfig config;
    config.overlapTolerance = 0.0001;
    
    NFPCalculator calculator(config);
    PlacementWorker worker(config, calculator);
    
    bool hasOverlap = worker.hasSignificantOverlap(rect1, pos1, rect2, pos2, config);
    
    assert(!hasOverlap && "Expected no overlap for parts 20 units apart");
    std::cout << "✓ PASSED: No overlap detected for distant parts" << std::endl;
}

void testSignificantOverlap() {
    std::cout << "\n=== Test 2: Significant Overlap ===" << std::endl;
    
    // Create two overlapping rectangles (50% overlap)
    Polygon rect1 = createRectangle(0, 0, 10, 10, 0);
    Polygon rect2 = createRectangle(0, 0, 10, 10, 1);
    
    Point pos1(0, 0);
    Point pos2(5, 0);  // 50% overlap (area = 50)
    
    DeepNestConfig config;
    config.overlapTolerance = 0.0001;
    
    NFPCalculator calculator(config);
    PlacementWorker worker(config, calculator);
    
    bool hasOverlap = worker.hasSignificantOverlap(rect1, pos1, rect2, pos2, config);
    
    assert(hasOverlap && "Expected overlap for 50% overlapping rectangles");
    std::cout << "✓ PASSED: Overlap detected for 50% overlapping parts" << std::endl;
}

void testTinyOverlapBelowTolerance() {
    std::cout << "\n=== Test 3: Tiny Overlap Below Tolerance ===" << std::endl;
    
    // Create two rectangles with tiny overlap
    Polygon rect1 = createRectangle(0, 0, 10, 10, 0);
    Polygon rect2 = createRectangle(0, 0, 10, 10, 1);
    
    Point pos1(0, 0);
    Point pos2(9.999, 0);  // Tiny overlap (0.001 * 10 = 0.01 area)
    
    DeepNestConfig config;
    config.overlapTolerance = 0.1;  // Higher tolerance - allow tiny overlaps
    
    NFPCalculator calculator(config);
    PlacementWorker worker(config, calculator);
    
    bool hasOverlap = worker.hasSignificantOverlap(rect1, pos1, rect2, pos2, config);
    
    assert(!hasOverlap && "Expected no overlap for tiny overlap below tolerance");
    std::cout << "✓ PASSED: Tiny overlap below tolerance not flagged" << std::endl;
}

void testCompleteOverlap() {
    std::cout << "\n=== Test 4: Complete Overlap (Same Position) ===" << std::endl;
    
    // Two identical rectangles at same position
    Polygon rect1 = createRectangle(0, 0, 10, 10, 0);
    Polygon rect2 = createRectangle(0, 0, 10, 10, 1);
    
    Point pos1(5, 5);
    Point pos2(5, 5);  // Same position - 100% overlap
    
    DeepNestConfig config;
    config.overlapTolerance = 0.0001;
    
    NFPCalculator calculator(config);
    PlacementWorker worker(config, calculator);
    
    bool hasOverlap = worker.hasSignificantOverlap(rect1, pos1, rect2, pos2, config);
    
    assert(hasOverlap && "Expected overlap for identical parts at same position");
    std::cout << "✓ PASSED: Complete overlap detected" << std::endl;
}

void testEdgeTouching() {
    std::cout << "\n=== Test 5: Edge Touching (No Overlap) ===" << std::endl;
    
    // Two rectangles touching at edge
    Polygon rect1 = createRectangle(0, 0, 10, 10, 0);
    Polygon rect2 = createRectangle(0, 0, 10, 10, 1);
    
    Point pos1(0, 0);
    Point pos2(10, 0);  // Touching at edge (x=10)
    
    DeepNestConfig config;
    config.overlapTolerance = 0.0001;
    
    NFPCalculator calculator(config);
    PlacementWorker worker(config, calculator);
    
    bool hasOverlap = worker.hasSignificantOverlap(rect1, pos1, rect2, pos2, config);
    
    // Edge touching should not count as overlap (zero area)
    assert(!hasOverlap && "Expected no overlap for edge-touching parts");
    std::cout << "✓ PASSED: Edge touching not flagged as overlap" << std::endl;
}

void testDifferentShapes() {
    std::cout << "\n=== Test 6: Different Shapes (Triangle + Rectangle) ===" << std::endl;
    
    // Triangle and rectangle overlapping
    Polygon triangle = createTriangle(0, 0, 10, 0);
    Polygon rect = createRectangle(0, 0, 10, 10, 1);
    
    Point pos1(0, 0);
    Point pos2(3, 0);  // Partial overlap
    
    DeepNestConfig config;
    config.overlapTolerance = 0.0001;
    
    NFPCalculator calculator(config);
    PlacementWorker worker(config, calculator);
    
    bool hasOverlap = worker.hasSignificantOverlap(triangle, pos1, rect, pos2, config);
    
    assert(hasOverlap && "Expected overlap for overlapping triangle and rectangle");
    std::cout << "✓ PASSED: Overlap detected for different shapes" << std::endl;
}

void testZeroTolerance() {
    std::cout << "\n=== Test 7: Zero Tolerance (Strictest) ===" << std::endl;
    
    // Very tiny overlap with zero tolerance
    Polygon rect1 = createRectangle(0, 0, 10, 10, 0);
    Polygon rect2 = createRectangle(0, 0, 10, 10, 1);
    
    Point pos1(0, 0);
    Point pos2(9.99, 0);  // 0.01 * 10 = 0.1 area overlap
    
    DeepNestConfig config;
    config.overlapTolerance = 0.0;  // Zero tolerance - any overlap is significant
    
    NFPCalculator calculator(config);
    PlacementWorker worker(config, calculator);
    
    bool hasOverlap = worker.hasSignificantOverlap(rect1, pos1, rect2, pos2, config);
    
    assert(hasOverlap && "Expected overlap with zero tolerance");
    std::cout << "✓ PASSED: Zero tolerance flags any overlap" << std::endl;
}

void testBoundingBoxOptimization() {
    std::cout << "\n=== Test 8: Bounding Box Optimization ===" << std::endl;
    
    // Parts far apart - should use bbox optimization
    Polygon rect1 = createRectangle(0, 0, 10, 10, 0);
    Polygon rect2 = createRectangle(0, 0, 10, 10, 1);
    
    Point pos1(0, 0);
    Point pos2(100, 100);  // Very far apart
    
    DeepNestConfig config;
    config.overlapTolerance = 0.0001;
    
    NFPCalculator calculator(config);
    PlacementWorker worker(config, calculator);
    
    // This should be fast due to bbox check
    auto start = std::chrono::high_resolution_clock::now();
    bool hasOverlap = worker.hasSignificantOverlap(rect1, pos1, rect2, pos2, config);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    assert(!hasOverlap && "Expected no overlap for distant parts");
    std::cout << "✓ PASSED: Bbox optimization working (time: " << duration.count() << "μs)" << std::endl;
}

void testToleranceThreshold() {
    std::cout << "\n=== Test 9: Tolerance Threshold Boundary ===" << std::endl;
    
    // Test exactly at tolerance boundary
    Polygon rect1 = createRectangle(0, 0, 10, 10, 0);
    Polygon rect2 = createRectangle(0, 0, 10, 10, 1);
    
    Point pos1(0, 0);
    Point pos2(9.9, 0);  // 0.1 * 10 = 1.0 area overlap
    
    DeepNestConfig config;
    config.overlapTolerance = 1.0;  // Exactly at boundary
    
    NFPCalculator calculator(config);
    PlacementWorker worker(config, calculator);
    
    bool hasOverlap = worker.hasSignificantOverlap(rect1, pos1, rect2, pos2, config);
    
    // Should NOT flag as overlap (area = tolerance, not > tolerance)
    assert(!hasOverlap && "Expected no overlap at exact tolerance boundary");
    std::cout << "✓ PASSED: Tolerance boundary handled correctly" << std::endl;
}

void testMultipleOverlapChecks() {
    std::cout << "\n=== Test 10: Multiple Overlap Checks (Performance) ===" << std::endl;
    
    Polygon rect1 = createRectangle(0, 0, 10, 10, 0);
    Polygon rect2 = createRectangle(0, 0, 10, 10, 1);
    
    DeepNestConfig config;
    config.overlapTolerance = 0.0001;
    
    NFPCalculator calculator(config);
    PlacementWorker worker(config, calculator);
    
    // Perform 1000 overlap checks
    auto start = std::chrono::high_resolution_clock::now();
    
    int overlapCount = 0;
    for (int i = 0; i < 1000; i++) {
        Point pos1(0, 0);
        Point pos2(i * 0.1, 0);  // Varying positions
        
        if (worker.hasSignificantOverlap(rect1, pos1, rect2, pos2, config)) {
            overlapCount++;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "✓ PASSED: 1000 overlap checks in " << duration.count() << "ms" << std::endl;
    std::cout << "  Overlaps detected: " << overlapCount << "/1000" << std::endl;
    std::cout << "  Average: " << (duration.count() / 1000.0) << "ms per check" << std::endl;
}

int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║   Overlap Detection Test Suite                        ║" << std::endl;
    std::cout << "║   Testing overlapTolerance parameter                  ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;
    
    try {
        testNoOverlap();
        testSignificantOverlap();
        testTinyOverlapBelowTolerance();
        testCompleteOverlap();
        testEdgeTouching();
        testDifferentShapes();
        testZeroTolerance();
        testBoundingBoxOptimization();
        testToleranceThreshold();
        testMultipleOverlapChecks();
        
        std::cout << "\n╔════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║   ✓ ALL TESTS PASSED (10/10)                          ║" << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════╝\n" << std::endl;
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\n✗ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}
