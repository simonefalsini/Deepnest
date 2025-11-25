/**
 * @file NFPValidationTests.cpp
 * @brief Comprehensive validation suite for No-Fit Polygon (NFP) functions
 *
 * This program systematically tests all NFP-related functions which are the
 * CORE BUSINESS LOGIC of DeepNest. These functions cannot be replaced with
 * standard libraries and require extensive validation.
 *
 * Test Phases:
 * 1. Distance functions (pointDistance, segmentDistance, polygonSlideDistance)
 * 2. Start point search (searchStartPoint for INNER/OUTER NFPs)
 * 3. Orbital tracing helpers (touching contacts, translation vectors)
 * 4. NFP calculation - simple cases with known results
 * 5. NFP calculation - complex cases (concave polygons)
 * 6. Comparison with existing working test cases
 *
 * NFP Functions Under Test:
 * - pointDistance() - Distance from point to line with direction
 * - segmentDistance() - Distance between segments in direction
 * - polygonSlideDistance() - Slide distance between polygons (CRITICAL)
 * - searchStartPoint() - Find valid start point for NFP tracing
 * - findTouchingContacts() - Detect contact points between polygons
 * - generateTranslationVectors() - Generate candidate slide vectors
 * - isBacktracking() - Detect invalid backtracking moves
 * - noFitPolygon() - Main NFP calculation function (CRITICAL)
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <optional>

// DeepNest includes
#include "deepnest/core/Point.h"
#include "deepnest/core/Types.h"
#include "deepnest/geometry/GeometryUtil.h"

using namespace deepnest;

// ============================================================================
// Test Framework
// ============================================================================

struct TestResult {
    std::string testName;
    bool passed;
    std::string message;
    std::string details;
};

class NFPTestSuite {
private:
    std::vector<TestResult> results_;
    int totalTests_ = 0;
    int passedTests_ = 0;
    std::string currentPhase_;

public:
    void setPhase(const std::string& phase) {
        currentPhase_ = phase;
        std::cout << "\n=== " << phase << " ===\n";
    }

    void addResult(const std::string& name, bool passed,
                   const std::string& msg = "", const std::string& details = "") {
        results_.push_back({name, passed, msg, details});
        totalTests_++;
        if (passed) passedTests_++;

        // Print immediately for visibility
        std::string status = passed ? "✓ PASS" : "✗ FAIL";
        std::cout << status << " | " << name;
        if (!msg.empty()) {
            std::cout << " - " << msg;
        }
        std::cout << "\n";
        if (!passed && !details.empty()) {
            std::cout << "      Details: " << details << "\n";
        }
    }

    void printSummary() const {
        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "  NFP VALIDATION TEST SUMMARY\n";
        std::cout << "========================================\n\n";

        std::string currentPhase = "";
        for (const auto& result : results_) {
            std::string status = result.passed ? "✓" : "✗";
            std::cout << status << " " << result.testName;
            if (!result.message.empty()) {
                std::cout << " - " << result.message;
            }
            std::cout << "\n";
        }

        std::cout << "\n========================================\n";
        std::cout << "Total: " << totalTests_ << " tests\n";
        std::cout << "Passed: " << passedTests_ << " ("
                  << std::fixed << std::setprecision(1)
                  << (100.0 * passedTests_ / totalTests_) << "%)\n";
        std::cout << "Failed: " << (totalTests_ - passedTests_) << "\n";
        std::cout << "========================================\n";
    }

    int getFailedCount() const { return totalTests_ - passedTests_; }
};

// ============================================================================
// Helper Functions
// ============================================================================

bool almostEqualDouble(double a, double b, double tolerance = 1e-6) {
    return std::abs(a - b) < tolerance;
}

bool almostEqualPoint(const Point& a, const Point& b, double tolerance = 1e-6) {
    return almostEqualDouble(a.x, b.x, tolerance) &&
           almostEqualDouble(a.y, b.y, tolerance);
}

bool almostEqualPolygon(const std::vector<Point>& a, const std::vector<Point>& b,
                        double tolerance = 1e-6) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (!almostEqualPoint(a[i], b[i], tolerance)) return false;
    }
    return true;
}

std::string pointToString(const Point& p) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << "(" << p.x << ", " << p.y << ")";
    return oss.str();
}

std::string polygonToString(const std::vector<Point>& poly) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < poly.size(); i++) {
        oss << pointToString(poly[i]);
        if (i < poly.size() - 1) oss << ", ";
    }
    oss << "]";
    return oss.str();
}

std::string optionalDoubleToString(const std::optional<double>& opt) {
    if (opt.has_value()) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4) << *opt;
        return oss.str();
    }
    return "NULL";
}

// ============================================================================
// PHASE 1: Distance Functions
// ============================================================================

void testPointDistance(NFPTestSuite& suite) {
    suite.setPhase("PHASE 1.1: pointDistance() Tests");

    // Test 1: Point perpendicular to horizontal line
    {
        Point p(5, 5);
        Point s1(0, 0);
        Point s2(10, 0);
        Point direction(0, 1); // Vertical direction

        auto distance = GeometryUtil::pointDistance(p, s1, s2, direction, false);

        bool test = distance.has_value() && almostEqualDouble(*distance, 5.0);
        suite.addResult("pointDistance - perpendicular to line", test,
                       "Distance: " + optionalDoubleToString(distance) + " (expected 5.0)");
    }

    // Test 2: Point not projecting onto segment
    {
        Point p(15, 5);
        Point s1(0, 0);
        Point s2(10, 0);
        Point direction(0, 1);

        auto distance = GeometryUtil::pointDistance(p, s1, s2, direction, false);

        bool test = !distance.has_value(); // Should be NULL (point doesn't project onto segment)
        suite.addResult("pointDistance - outside projection", test,
                       "Distance: " + optionalDoubleToString(distance) + " (expected NULL)");
    }

    // Test 3: Point on line (infinite mode)
    {
        Point p(15, 5);
        Point s1(0, 0);
        Point s2(10, 0);
        Point direction(0, 1);

        auto distance = GeometryUtil::pointDistance(p, s1, s2, direction, true);

        bool test = distance.has_value() && almostEqualDouble(*distance, 5.0);
        suite.addResult("pointDistance - infinite mode", test,
                       "Distance: " + optionalDoubleToString(distance) + " (expected 5.0)");
    }

    // Test 4: Negative distance (point in opposite direction)
    {
        Point p(5, -5);
        Point s1(0, 0);
        Point s2(10, 0);
        Point direction(0, 1); // Looking up, but point is below

        auto distance = GeometryUtil::pointDistance(p, s1, s2, direction, false);

        bool test = distance.has_value() && *distance < 0;
        suite.addResult("pointDistance - negative direction", test,
                       "Distance: " + optionalDoubleToString(distance) + " (expected negative)");
    }
}

void testSegmentDistance(NFPTestSuite& suite) {
    suite.setPhase("PHASE 1.2: segmentDistance() Tests");

    // Test 1: Parallel horizontal segments, vertical movement
    {
        Point A(0, 0);
        Point B(10, 0);
        Point E(0, 5);
        Point F(10, 5);
        Point direction(0, 1); // Move up

        auto distance = GeometryUtil::segmentDistance(A, B, E, F, direction);

        bool test = distance.has_value() && almostEqualDouble(*distance, 5.0);
        suite.addResult("segmentDistance - parallel horizontal", test,
                       "Distance: " + optionalDoubleToString(distance) + " (expected 5.0)");
    }

    // Test 2: Perpendicular segments
    {
        Point A(0, 0);
        Point B(10, 0);
        Point E(5, 5);
        Point F(5, 10);
        Point direction(0, 1); // Move up

        auto distance = GeometryUtil::segmentDistance(A, B, E, F, direction);

        bool test = distance.has_value() && almostEqualDouble(*distance, 5.0);
        suite.addResult("segmentDistance - perpendicular", test,
                       "Distance: " + optionalDoubleToString(distance) + " (expected 5.0)");
    }

    // Test 3: Segments don't collide in direction
    {
        Point A(0, 0);
        Point B(10, 0);
        Point E(15, 5);
        Point F(20, 5);
        Point direction(0, 1); // Move up

        auto distance = GeometryUtil::segmentDistance(A, B, E, F, direction);

        bool test = !distance.has_value(); // Should be NULL
        suite.addResult("segmentDistance - no collision", test,
                       "Distance: " + optionalDoubleToString(distance) + " (expected NULL)");
    }

    // Test 4: Diagonal movement
    {
        Point A(0, 0);
        Point B(10, 0);
        Point E(15, 10);
        Point F(20, 10);
        Point direction(1, 1); // Move diagonally
        // Normalize direction
        double len = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        direction.x /= len;
        direction.y /= len;

        auto distance = GeometryUtil::segmentDistance(A, B, E, F, direction);

        bool test = distance.has_value();
        suite.addResult("segmentDistance - diagonal movement", test,
                       "Distance: " + optionalDoubleToString(distance));
    }
}

void testPolygonSlideDistance(NFPTestSuite& suite) {
    suite.setPhase("PHASE 1.3: polygonSlideDistance() Tests - CRITICAL");

    // Test 1: Two squares, vertical slide
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(0, 15), Point(10, 15), Point(10, 25), Point(0, 25)
        };
        Point direction(0, -1); // B slides down toward A

        auto distance = GeometryUtil::polygonSlideDistance(A, B, direction, false);

        bool test = distance.has_value() && almostEqualDouble(*distance, 5.0, 0.1);
        suite.addResult("polygonSlideDistance - squares vertical", test,
                       "Distance: " + optionalDoubleToString(distance) + " (expected ~5.0)",
                       "Two 10x10 squares separated by 5 units vertically");
    }

    // Test 2: Squares already touching
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(0, 10), Point(10, 10), Point(10, 20), Point(0, 20)
        };
        Point direction(0, -1); // B slides down

        auto distance = GeometryUtil::polygonSlideDistance(A, B, direction, false);

        bool test = distance.has_value() && almostEqualDouble(*distance, 0.0, 0.1);
        suite.addResult("polygonSlideDistance - already touching", test,
                       "Distance: " + optionalDoubleToString(distance) + " (expected 0.0)");
    }

    // Test 3: Horizontal slide
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(15, 0), Point(25, 0), Point(25, 10), Point(15, 10)
        };
        Point direction(-1, 0); // B slides left toward A

        auto distance = GeometryUtil::polygonSlideDistance(A, B, direction, false);

        bool test = distance.has_value() && almostEqualDouble(*distance, 5.0, 0.1);
        suite.addResult("polygonSlideDistance - squares horizontal", test,
                       "Distance: " + optionalDoubleToString(distance) + " (expected ~5.0)");
    }

    // Test 4: No collision case
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(15, 15), Point(25, 15), Point(25, 25), Point(15, 25)
        };
        Point direction(0, -1); // B slides down (won't hit A)

        auto distance = GeometryUtil::polygonSlideDistance(A, B, direction, false);

        bool test = !distance.has_value();
        suite.addResult("polygonSlideDistance - no collision", test,
                       "Distance: " + optionalDoubleToString(distance) + " (expected NULL)");
    }

    // Test 5: Triangle sliding toward square
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(5, 15), Point(10, 20), Point(0, 20)
        };
        Point direction(0, -1); // Triangle slides down

        auto distance = GeometryUtil::polygonSlideDistance(A, B, direction, false);

        bool test = distance.has_value() && *distance > 0;
        suite.addResult("polygonSlideDistance - triangle to square", test,
                       "Distance: " + optionalDoubleToString(distance) + " (expected positive)",
                       "Triangle at (5,15)-(10,20)-(0,20) sliding down to square");
    }
}

// ============================================================================
// PHASE 2: Start Point Search
// ============================================================================

void testSearchStartPoint(NFPTestSuite& suite) {
    suite.setPhase("PHASE 2: searchStartPoint() Tests");

    // Test 1: OUTER NFP - B outside A (square)
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(0, 0), Point(5, 0), Point(5, 5), Point(0, 5)
        };

        auto startPoint = GeometryUtil::searchStartPoint(A, B, false, {});

        bool test = startPoint.has_value();
        suite.addResult("searchStartPoint - OUTER square", test,
                       startPoint.has_value() ?
                           "Start: " + pointToString(*startPoint) :
                           "No start point found",
                       "Should find valid start point for B to orbit outside A");
    }

    // Test 2: INNER NFP - B inside A
    {
        std::vector<Point> A = {
            Point(0, 0), Point(20, 0), Point(20, 20), Point(0, 20)
        };
        std::vector<Point> B = {
            Point(0, 0), Point(5, 0), Point(5, 5), Point(0, 5)
        };

        auto startPoint = GeometryUtil::searchStartPoint(A, B, true, {});

        bool test = startPoint.has_value();
        suite.addResult("searchStartPoint - INNER square", test,
                       startPoint.has_value() ?
                           "Start: " + pointToString(*startPoint) :
                           "No start point found",
                       "Should find valid start point for B to orbit inside A");
    }

    // Test 3: Triangle OUTER
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(5, 10)
        };
        // Note: Point now uses int64_t, convert decimal coordinates to integers (x10)
        std::vector<Point> B = {
            Point(0, 0), Point(30, 0), Point(15, 30)
        };

        auto startPoint = GeometryUtil::searchStartPoint(A, B, false, {});

        bool test = startPoint.has_value();
        suite.addResult("searchStartPoint - OUTER triangle", test,
                       startPoint.has_value() ?
                           "Start: " + pointToString(*startPoint) :
                           "No start point found");
    }
}

// ============================================================================
// PHASE 3: Orbital Tracing Helpers
// ============================================================================

void testOrbitalHelpers(NFPTestSuite& suite) {
    suite.setPhase("PHASE 3: Orbital Tracing Helpers");

    // Test 1: findTouchingContacts - vertex to vertex
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(10, 10), Point(20, 10), Point(20, 20), Point(10, 20)
        };
        Point offsetB(0, 0);

        auto contacts = GeometryUtil::findTouchingContacts(A, B, offsetB);

        bool test = !contacts.empty();
        suite.addResult("findTouchingContacts - vertex-vertex", test,
                       "Found " + std::to_string(contacts.size()) + " contact(s)",
                       "Squares touching at corner (10,10)");
    }

    // Test 2: findTouchingContacts - edge touching
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(5, 10), Point(15, 10), Point(15, 20), Point(5, 20)
        };
        Point offsetB(0, 0);

        auto contacts = GeometryUtil::findTouchingContacts(A, B, offsetB);

        bool test = !contacts.empty();
        suite.addResult("findTouchingContacts - edge touching", test,
                       "Found " + std::to_string(contacts.size()) + " contact(s)",
                       "Squares sharing edge");
    }

    // Test 3: findTouchingContacts - no contact
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(15, 15), Point(25, 15), Point(25, 25), Point(15, 25)
        };
        Point offsetB(0, 0);

        auto contacts = GeometryUtil::findTouchingContacts(A, B, offsetB);

        bool test = contacts.empty();
        suite.addResult("findTouchingContacts - no contact", test,
                       "Found " + std::to_string(contacts.size()) + " contact(s) (expected 0)");
    }

    // Test 4: generateTranslationVectors
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(10, 10), Point(20, 10), Point(20, 20), Point(10, 20)
        };
        Point offsetB(0, 0);

        auto contacts = GeometryUtil::findTouchingContacts(A, B, offsetB);

        if (!contacts.empty()) {
            auto vectors = GeometryUtil::generateTranslationVectors(
                contacts[0], A, B, offsetB
            );

            bool test = !vectors.empty();
            suite.addResult("generateTranslationVectors", test,
                           "Generated " + std::to_string(vectors.size()) + " vector(s)",
                           "From touching contact, should generate slide vectors");
        } else {
            suite.addResult("generateTranslationVectors", false,
                           "No contacts to generate vectors from");
        }
    }

    // Test 5: isBacktracking
    {
        TranslationVector vec1;
        vec1.x = 1.0;
        vec1.y = 0.0;
        vec1.polygon = 'A';
        vec1.startIndex = 0;
        vec1.endIndex = 1;

        TranslationVector vec2;
        vec2.x = -1.0;
        vec2.y = 0.0;
        vec2.polygon = 'A';
        vec2.startIndex = 1;
        vec2.endIndex = 0;

        bool isBack = GeometryUtil::isBacktracking(vec2, vec1);

        suite.addResult("isBacktracking - opposite vectors", isBack,
                       "Vectors (1,0) and (-1,0) should be backtracking");
    }

    // Test 6: isBacktracking - perpendicular vectors
    {
        TranslationVector vec1;
        vec1.x = 1.0;
        vec1.y = 0.0;
        vec1.polygon = 'A';
        vec1.startIndex = 0;
        vec1.endIndex = 1;

        TranslationVector vec2;
        vec2.x = 0.0;
        vec2.y = 1.0;
        vec2.polygon = 'B';
        vec2.startIndex = 0;
        vec2.endIndex = 1;

        bool isBack = GeometryUtil::isBacktracking(vec2, vec1);

        suite.addResult("isBacktracking - perpendicular", !isBack,
                       "Perpendicular vectors should NOT be backtracking");
    }
}

// ============================================================================
// PHASE 4: NFP Calculation - Simple Cases
// ============================================================================

void testNFPSimpleCases(NFPTestSuite& suite) {
    suite.setPhase("PHASE 4: noFitPolygon() - Simple Cases with Known Results");

    // Test 1: Square vs Square (OUTER) - NFP should be a larger square
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(0, 0), Point(5, 0), Point(5, 5), Point(0, 5)
        };

        auto nfps = GeometryUtil::noFitPolygon(A, B, false, false);

        bool hasNFP = !nfps.empty();
        bool correctSize = hasNFP && nfps[0].size() >= 4;

        suite.addResult("noFitPolygon - square vs square OUTER", hasNFP && correctSize,
                       "Generated " + std::to_string(nfps.size()) + " NFP(s), " +
                       (hasNFP ? std::to_string(nfps[0].size()) + " points" : "0 points"),
                       "10x10 square vs 5x5 square - NFP should be quadrilateral");
    }

    // Test 2: Triangle vs Triangle (OUTER)
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(5, 10)
        };
        // Note: Point now uses int64_t, convert decimal coordinates to integers (x10)
        std::vector<Point> B = {
            Point(0, 0), Point(50, 0), Point(25, 50)
        };

        auto nfps = GeometryUtil::noFitPolygon(A, B, false, false);

        bool hasNFP = !nfps.empty();
        bool correctSize = hasNFP && nfps[0].size() >= 3;

        suite.addResult("noFitPolygon - triangle vs triangle OUTER", hasNFP && correctSize,
                       "Generated " + std::to_string(nfps.size()) + " NFP(s), " +
                       (hasNFP ? std::to_string(nfps[0].size()) + " points" : "0 points"),
                       "Triangle vs smaller triangle - NFP should be polygon");
    }

    // Test 3: Rectangle vs Point (conceptual - very small square)
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 5), Point(0, 5)
        };
        // Note: Point now uses int64_t, convert decimal coordinates to integers (x100)
        std::vector<Point> B = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };

        auto nfps = GeometryUtil::noFitPolygon(A, B, false, false);

        bool hasNFP = !nfps.empty();

        suite.addResult("noFitPolygon - rectangle vs point-like", hasNFP,
                       "Generated " + std::to_string(nfps.size()) + " NFP(s)",
                       "Rectangle vs tiny square - NFP should approximate rectangle");
    }

    // Test 4: Simple concave shape
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10),
            Point(5, 5), // Makes it concave
            Point(0, 10)
        };
        std::vector<Point> B = {
            Point(0, 0), Point(3, 0), Point(3, 3), Point(0, 3)
        };

        auto nfps = GeometryUtil::noFitPolygon(A, B, false, false);

        bool hasNFP = !nfps.empty();

        suite.addResult("noFitPolygon - concave vs square", hasNFP,
                       "Generated " + std::to_string(nfps.size()) + " NFP(s), " +
                       (hasNFP ? std::to_string(nfps[0].size()) + " points" : "0 points"),
                       "Concave L-shape vs small square");
    }
}

// ============================================================================
// PHASE 5: NFP Calculation - Complex Cases
// ============================================================================

void testNFPComplexCases(NFPTestSuite& suite) {
    suite.setPhase("PHASE 5: noFitPolygon() - Complex Cases");

    // Test 1: Known working case from PolygonExtractor
    // Using simple shapes that we know work
    {
        // Rectangle A
        std::vector<Point> A = {
            Point(0, 0), Point(20, 0), Point(20, 10), Point(0, 10)
        };
        // Rectangle B
        std::vector<Point> B = {
            Point(0, 0), Point(8, 0), Point(8, 6), Point(0, 6)
        };

        auto nfps = GeometryUtil::noFitPolygon(A, B, false, false);

        bool hasNFP = !nfps.empty();
        bool validSize = hasNFP && nfps[0].size() >= 4;

        suite.addResult("noFitPolygon - rectangle vs rectangle", hasNFP && validSize,
                       "Generated " + std::to_string(nfps.size()) + " NFP(s), " +
                       (hasNFP ? std::to_string(nfps[0].size()) + " points" : "0 points"),
                       "20x10 rectangle vs 8x6 rectangle");
    }

    // Test 2: Rotated shapes
    {
        // Square A
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        // Rotated square B (45 degrees approximation)
        double d = 5.0 / std::sqrt(2.0);
        std::vector<Point> B = {
            Point(d, 0), Point(2*d, d), Point(d, 2*d), Point(0, d)
        };

        auto nfps = GeometryUtil::noFitPolygon(A, B, false, false);

        bool hasNFP = !nfps.empty();

        suite.addResult("noFitPolygon - square vs rotated square", hasNFP,
                       "Generated " + std::to_string(nfps.size()) + " NFP(s), " +
                       (hasNFP ? std::to_string(nfps[0].size()) + " points" : "0 points"),
                       "Square vs 45° rotated square");
    }

    // Test 3: searchEdges mode (find all NFPs)
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(0, 0), Point(5, 0), Point(5, 5), Point(0, 5)
        };

        auto nfps = GeometryUtil::noFitPolygon(A, B, false, true); // searchEdges = true

        bool hasNFP = !nfps.empty();

        suite.addResult("noFitPolygon - searchEdges mode", hasNFP,
                       "Generated " + std::to_string(nfps.size()) + " NFP(s)",
                       "Should find all possible NFP loops");
    }

    // Test 4: INNER NFP (B inside A)
    {
        std::vector<Point> A = {
            Point(0, 0), Point(20, 0), Point(20, 20), Point(0, 20)
        };
        std::vector<Point> B = {
            Point(0, 0), Point(5, 0), Point(5, 5), Point(0, 5)
        };

        auto nfps = GeometryUtil::noFitPolygon(A, B, true, false); // inside = true

        bool hasNFP = !nfps.empty();

        suite.addResult("noFitPolygon - INNER square in square", hasNFP,
                       "Generated " + std::to_string(nfps.size()) + " NFP(s), " +
                       (hasNFP ? std::to_string(nfps[0].size()) + " points" : "0 points"),
                       "5x5 square orbiting inside 20x20 square");
    }
}

// ============================================================================
// PHASE 6: Regression Tests (Known Working Cases)
// ============================================================================

void testNFPRegressionCases(NFPTestSuite& suite) {
    suite.setPhase("PHASE 6: Regression Tests - Cases Known to Work");

    // These are based on shapes that work in PolygonExtractor
    // We're testing that they continue to work correctly

    // Test 1: Simple non-overlapping rectangles
    {
        std::vector<Point> A = {
            Point(0, 0), Point(15, 0), Point(15, 8), Point(0, 8)
        };
        std::vector<Point> B = {
            Point(0, 0), Point(6, 0), Point(6, 4), Point(0, 4)
        };

        auto nfps = GeometryUtil::noFitPolygon(A, B, false, false);

        bool test = !nfps.empty() && nfps[0].size() == 4;
        suite.addResult("Regression - simple rectangles", test,
                       nfps.empty() ? "No NFP generated" :
                           std::to_string(nfps[0].size()) + " points (expected 4)");
    }

    // Test 2: noFitPolygonRectangle optimization
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(0, 0), Point(5, 0), Point(5, 5), Point(0, 5)
        };

        // Check if A is recognized as rectangle
        bool isRect = GeometryUtil::isRectangle(A);

        if (isRect) {
            auto nfps = GeometryUtil::noFitPolygonRectangle(A, B);
            bool test = !nfps.empty();
            suite.addResult("Regression - rectangle optimization", test,
                           "Rectangle NFP: " + std::to_string(nfps.size()) + " NFP(s)");
        } else {
            suite.addResult("Regression - rectangle optimization", false,
                           "Square not recognized as rectangle");
        }
    }

    // Test 3: Consistency test - same result with multiple runs
    {
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(0, 0), Point(5, 0), Point(5, 5), Point(0, 5)
        };

        auto nfps1 = GeometryUtil::noFitPolygon(A, B, false, false);
        auto nfps2 = GeometryUtil::noFitPolygon(A, B, false, false);

        bool sameCount = nfps1.size() == nfps2.size();
        bool sameShape = false;

        if (sameCount && !nfps1.empty() && !nfps2.empty()) {
            sameShape = almostEqualPolygon(nfps1[0], nfps2[0], 1e-3);
        }

        suite.addResult("Regression - consistency", sameCount && sameShape,
                       "Run 1: " + std::to_string(nfps1.size()) + " NFPs, " +
                       "Run 2: " + std::to_string(nfps2.size()) + " NFPs",
                       "Multiple runs should produce identical results");
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "  DeepNest NFP Validation Tests\n";
    std::cout << "========================================\n";
    std::cout << "Testing CORE BUSINESS LOGIC:\n";
    std::cout << "- Distance calculation functions\n";
    std::cout << "- Start point search\n";
    std::cout << "- Orbital tracing helpers\n";
    std::cout << "- No-Fit Polygon calculation\n";
    std::cout << "========================================\n";

    NFPTestSuite suite;

    try {
        // PHASE 1: Distance functions
        testPointDistance(suite);
        testSegmentDistance(suite);
        testPolygonSlideDistance(suite);

        // PHASE 2: Start point search
        testSearchStartPoint(suite);

        // PHASE 3: Orbital helpers
        testOrbitalHelpers(suite);

        // PHASE 4: NFP simple cases
        testNFPSimpleCases(suite);

        // PHASE 5: NFP complex cases
        testNFPComplexCases(suite);

        // PHASE 6: Regression tests
        testNFPRegressionCases(suite);

        // Print summary
        suite.printSummary();

        // Return exit code based on test results
        return suite.getFailedCount() == 0 ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ EXCEPTION CAUGHT: " << e.what() << std::endl;
        return 2;
    }
}
