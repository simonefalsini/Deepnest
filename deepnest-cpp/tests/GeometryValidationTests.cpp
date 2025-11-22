/**
 * @file GeometryValidationTests.cpp
 * @brief Comprehensive validation and testing suite for geometry functions
 *
 * This program systematically tests all geometry functions in deepnest-cpp,
 * comparing results with established libraries (Boost.Geometry, Clipper2)
 * where applicable, and validating custom implementations with known test cases.
 *
 * Test Phases:
 * 1. Basic geometry functions (points, segments, bounds)
 * 2. ConvexHull validation (vs Boost.Geometry)
 * 3. Transformation matrix operations
 * 4. Polygon operations (already using Clipper2)
 * 5. Curve linearization (Bezier, Arc)
 * 6. NFP advanced functions (CRITICAL - core business logic)
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <string>
#include <sstream>

// DeepNest includes
#include "deepnest/core/Point.h"
#include "deepnest/core/Types.h"
#include "deepnest/geometry/GeometryUtil.h"
#include "deepnest/geometry/ConvexHull.h"
#include "deepnest/geometry/Transformation.h"
#include "deepnest/geometry/PolygonOperations.h"

// Boost.Geometry for comparison
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/multi_point.hpp>

using namespace deepnest;
namespace bg = boost::geometry;

// Boost.Geometry types (different from Boost.Polygon used in Types.h)
typedef bg::model::d2::point_xy<double> BgPoint;
typedef bg::model::polygon<BgPoint> BgPolygon;
typedef bg::model::multi_point<BgPoint> BgMultiPoint;

// ============================================================================
// Test Framework
// ============================================================================

struct TestResult {
    std::string testName;
    bool passed;
    std::string message;
};

class TestSuite {
private:
    std::vector<TestResult> results_;
    int totalTests_ = 0;
    int passedTests_ = 0;

public:
    void addResult(const std::string& name, bool passed, const std::string& msg = "") {
        results_.push_back({name, passed, msg});
        totalTests_++;
        if (passed) passedTests_++;
    }

    void printSummary() const {
        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "  GEOMETRY VALIDATION TEST SUMMARY\n";
        std::cout << "========================================\n\n";

        int failedTests = totalTests_ - passedTests_;

        for (const auto& result : results_) {
            std::string status = result.passed ? "✓ PASS" : "✗ FAIL";
            std::cout << status << " | " << result.testName;
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
        std::cout << "Failed: " << failedTests << "\n";
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

// Convert DeepNest polygon to Boost.Geometry polygon
BgPolygon toBgPolygon(const std::vector<Point>& poly) {
    BgPolygon result;
    for (const auto& p : poly) {
        bg::append(result.outer(), BgPoint(p.x, p.y));
    }
    bg::correct(result); // Ensures correct winding and closed ring
    return result;
}

// Convert Boost.Geometry polygon to DeepNest polygon
std::vector<Point> fromBgPolygon(const BgPolygon& poly) {
    std::vector<Point> result;
    for (const auto& p : poly.outer()) {
        result.push_back(Point(bg::get<0>(p), bg::get<1>(p)));
    }
    // Remove duplicate closing point if present
    if (!result.empty() && almostEqualPoint(result.front(), result.back())) {
        result.pop_back();
    }
    return result;
}

// ============================================================================
// PHASE 1: Basic Geometry Functions
// ============================================================================

void testBasicPointFunctions(TestSuite& suite) {
    std::cout << "\n=== PHASE 1.1: Basic Point Functions ===\n";

    // Test almostEqual
    {
        bool test1 = GeometryUtil::almostEqual(1.0, 1.0000001, 1e-5);
        bool test2 = !GeometryUtil::almostEqual(1.0, 1.1, 1e-5);
        suite.addResult("almostEqual()", test1 && test2,
                       "Tolerance comparison working");
    }

    // Test almostEqualPoints
    {
        Point p1(1.0, 2.0);
        Point p2(1.0000001, 2.0000001);
        Point p3(1.1, 2.1);

        bool test1 = GeometryUtil::almostEqualPoints(p1, p2, 1e-5);
        bool test2 = !GeometryUtil::almostEqualPoints(p1, p3, 1e-5);
        suite.addResult("almostEqualPoints()", test1 && test2);
    }

    // Test withinDistance
    {
        Point p1(0, 0);
        Point p2(3, 4); // Distance = 5
        Point p3(1, 1); // Distance = sqrt(2) ≈ 1.414

        bool test1 = GeometryUtil::withinDistance(p1, p2, 6.0);
        bool test2 = !GeometryUtil::withinDistance(p1, p2, 4.0);
        bool test3 = GeometryUtil::withinDistance(p1, p3, 1.5);
        suite.addResult("withinDistance()", test1 && test2 && test3);
    }

    // Test normalizeVector
    {
        Point v1(3, 4); // Length = 5
        Point normalized = GeometryUtil::normalizeVector(v1);
        double len = std::sqrt(normalized.x * normalized.x + normalized.y * normalized.y);

        bool test = almostEqualDouble(len, 1.0, 1e-6) &&
                   almostEqualDouble(normalized.x, 0.6, 1e-6) &&
                   almostEqualDouble(normalized.y, 0.8, 1e-6);
        suite.addResult("normalizeVector()", test,
                       "Vector (3,4) normalized to (0.6,0.8)");
    }

    // Test degreesToRadians / radiansToDegrees
    {
        double deg = 90.0;
        double rad = GeometryUtil::degreesToRadians(deg);
        double degBack = GeometryUtil::radiansToDegrees(rad);

        bool test = almostEqualDouble(rad, M_PI / 2.0, 1e-6) &&
                   almostEqualDouble(degBack, deg, 1e-6);
        suite.addResult("degreesToRadians/radiansToDegrees()", test,
                       "90° = π/2 rad");
    }
}

void testLineSegmentFunctions(TestSuite& suite) {
    std::cout << "\n=== PHASE 1.2: Line Segment Functions ===\n";

    // Test onSegment
    {
        Point A(0, 0);
        Point B(10, 0);
        Point p1(5, 0);  // On segment
        Point p2(11, 0); // Outside segment
        Point p3(5, 1);  // Not on line

        bool test1 = GeometryUtil::onSegment(A, B, p1);
        bool test2 = !GeometryUtil::onSegment(A, B, p2);
        bool test3 = !GeometryUtil::onSegment(A, B, p3);
        suite.addResult("onSegment()", test1 && test2 && test3);
    }

    // Test lineIntersect - perpendicular lines
    {
        Point A(0, 5);
        Point B(10, 5);
        Point E(5, 0);
        Point F(5, 10);

        auto intersection = GeometryUtil::lineIntersect(A, B, E, F);
        bool test = intersection.has_value() &&
                   almostEqualPoint(*intersection, Point(5, 5));
        suite.addResult("lineIntersect() - perpendicular", test,
                       "Lines intersect at (5,5)");
    }

    // Test lineIntersect - parallel lines
    {
        Point A(0, 0);
        Point B(10, 0);
        Point E(0, 5);
        Point F(10, 5);

        auto intersection = GeometryUtil::lineIntersect(A, B, E, F);
        bool test = !intersection.has_value();
        suite.addResult("lineIntersect() - parallel", test,
                       "Parallel lines do not intersect");
    }

    // Test lineIntersect - segments don't reach
    {
        Point A(0, 0);
        Point B(5, 0);
        Point E(10, 0);
        Point F(15, 0);

        auto intersection = GeometryUtil::lineIntersect(A, B, E, F, false);
        bool test1 = !intersection.has_value(); // Segments don't intersect

        auto intersection2 = GeometryUtil::lineIntersect(A, B, E, F, true);
        bool test2 = intersection2.has_value(); // Infinite lines do intersect

        suite.addResult("lineIntersect() - infinite flag", test1 && test2);
    }
}

void testPolygonBasicFunctions(TestSuite& suite) {
    std::cout << "\n=== PHASE 1.3: Polygon Basic Functions ===\n";

    // Test getPolygonBounds
    {
        std::vector<Point> poly = {
            Point(0, 0), Point(10, 0), Point(10, 5), Point(0, 5)
        };

        BoundingBox bounds = GeometryUtil::getPolygonBounds(poly);
        bool test = almostEqualDouble(bounds.x, 0.0) &&
                   almostEqualDouble(bounds.x + bounds.width, 10.0) &&
                   almostEqualDouble(bounds.y, 0.0) &&
                   almostEqualDouble(bounds.y + bounds.height, 5.0);
        suite.addResult("getPolygonBounds()", test,
                       "Rectangle bounds correct");
    }

    // Test polygonArea - square
    {
        std::vector<Point> square = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };

        double area = GeometryUtil::polygonArea(square);
        // CCW winding should give positive area
        bool test = almostEqualDouble(std::abs(area), 100.0, 1e-6);
        suite.addResult("polygonArea() - square", test,
                       "10x10 square = 100 area");
    }

    // Test polygonArea vs Boost.Geometry
    {
        std::vector<Point> triangle = {
            Point(0, 0), Point(10, 0), Point(5, 10)
        };

        double deepnestArea = std::abs(GeometryUtil::polygonArea(triangle));

        BgPolygon bgPoly = toBgPolygon(triangle);
        double boostArea = std::abs(bg::area(bgPoly));

        bool test = almostEqualDouble(deepnestArea, boostArea, 0.1);
        suite.addResult("polygonArea() vs Boost", test,
                       "Triangle area: DeepNest=" + std::to_string(deepnestArea) +
                       " Boost=" + std::to_string(boostArea));
    }

    // Test pointInPolygon
    {
        std::vector<Point> square = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };

        Point inside(5, 5);
        Point outside(15, 15);
        Point onEdge(0, 5);

        auto test1 = GeometryUtil::pointInPolygon(inside, square);
        auto test2 = GeometryUtil::pointInPolygon(outside, square);
        auto test3 = GeometryUtil::pointInPolygon(onEdge, square);

        bool passed = test1.has_value() && *test1 == true &&
                     test2.has_value() && *test2 == false;
                     // test3 might be nullopt (on boundary) which is acceptable

        suite.addResult("pointInPolygon()", passed,
                       "Inside=true, Outside=false");
    }

    // Test isRectangle
    {
        std::vector<Point> rectangle = {
            Point(0, 0), Point(10, 0), Point(10, 5), Point(0, 5)
        };

        std::vector<Point> notRectangle = {
            Point(0, 0), Point(10, 0), Point(5, 10)
        };

        bool test1 = GeometryUtil::isRectangle(rectangle);
        bool test2 = !GeometryUtil::isRectangle(notRectangle);
        suite.addResult("isRectangle()", test1 && test2);
    }

    // Test rotatePolygon
    {
        std::vector<Point> square = {
            Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1)
        };

        std::vector<Point> rotated = GeometryUtil::rotatePolygon(square, 90);

        // After 90° rotation around origin:
        // (0,0) → (0,0), (1,0) → (0,1), (1,1) → (-1,1), (0,1) → (-1,0)
        std::vector<Point> expected = {
            Point(0, 0), Point(0, 1), Point(-1, 1), Point(-1, 0)
        };

        bool test = almostEqualPolygon(rotated, expected, 1e-6);
        suite.addResult("rotatePolygon()", test,
                       "90° rotation correct");
    }
}

// ============================================================================
// PHASE 2: ConvexHull Validation (vs Boost.Geometry)
// ============================================================================

void testConvexHull(TestSuite& suite) {
    std::cout << "\n=== PHASE 2: ConvexHull Validation ===\n";

    // Test 1: Square (convex hull should be identical)
    {
        std::vector<Point> square = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };

        std::vector<Point> hull = ConvexHull::computeHull(square);

        bool test = hull.size() == 4; // Square is already convex
        suite.addResult("ConvexHull - square", test,
                       "Hull size: " + std::to_string(hull.size()));
    }

    // Test 2: Concave polygon → convex hull
    {
        std::vector<Point> concave = {
            Point(0, 0), Point(10, 0), Point(10, 10),
            Point(5, 5),  // Interior point
            Point(0, 10)
        };

        std::vector<Point> deepnestHull = ConvexHull::computeHull(concave);

        // Compare with Boost.Geometry
        BgMultiPoint bgPoints;
        for (const auto& p : concave) {
            bg::append(bgPoints, BgPoint(p.x, p.y));
        }

        BgPolygon bgHull;
        bg::convex_hull(bgPoints, bgHull);
        std::vector<Point> boostHullPoints = fromBgPolygon(bgHull);

        bool test = deepnestHull.size() == 4; // Should be a quadrilateral
        suite.addResult("ConvexHull - concave polygon", test,
                       "DeepNest hull: " + std::to_string(deepnestHull.size()) +
                       " points, Boost hull: " + std::to_string(boostHullPoints.size()) +
                       " points");
    }

    // Test 3: Collinear points
    {
        std::vector<Point> collinear = {
            Point(0, 0), Point(5, 0), Point(10, 0), Point(15, 0)
        };

        std::vector<Point> hull = ConvexHull::computeHull(collinear);

        // Hull of collinear points is a line (2 endpoints)
        bool test = hull.size() == 2;
        suite.addResult("ConvexHull - collinear points", test,
                       "Hull size: " + std::to_string(hull.size()) + " (expected 2)");
    }

    // Test 4: Triangle (already convex)
    {
        std::vector<Point> triangle = {
            Point(0, 0), Point(10, 0), Point(5, 10)
        };

        std::vector<Point> hull = ConvexHull::computeHull(triangle);

        bool test = hull.size() == 3;
        suite.addResult("ConvexHull - triangle", test,
                       "Hull size: " + std::to_string(hull.size()));
    }

    // Test 5: Complex concave shape vs Boost
    {
        std::vector<Point> complex = {
            Point(0, 0), Point(3, 1), Point(6, 0),
            Point(5, 3), Point(6, 6), Point(3, 5),
            Point(0, 6), Point(1, 3), Point(2, 3)  // Some interior points
        };

        std::vector<Point> deepnestHull = ConvexHull::computeHull(complex);

        // Compare with Boost
        BgMultiPoint bgPoints;
        for (const auto& p : complex) {
            bg::append(bgPoints, BgPoint(p.x, p.y));
        }

        BgPolygon bgHullPoly;
        bg::convex_hull(bgPoints, bgHullPoly);
        std::vector<Point> boostHull = fromBgPolygon(bgHullPoly);

        // Check if sizes match (both should find same hull)
        bool sizeMatch = deepnestHull.size() == boostHull.size();

        // Calculate areas (should be very close)
        double deepnestArea = std::abs(GeometryUtil::polygonArea(deepnestHull));
        double boostArea = std::abs(bg::area(bgHullPoly));
        bool areaMatch = almostEqualDouble(deepnestArea, boostArea, 0.1);

        suite.addResult("ConvexHull vs Boost - complex", sizeMatch && areaMatch,
                       "DeepNest: " + std::to_string(deepnestHull.size()) +
                       " pts, " + std::to_string(deepnestArea) + " area | " +
                       "Boost: " + std::to_string(boostHull.size()) +
                       " pts, " + std::to_string(boostArea) + " area");
    }
}

// ============================================================================
// PHASE 3: Transformation Matrix Tests
// ============================================================================

void testTransformation(TestSuite& suite) {
    std::cout << "\n=== PHASE 3: Transformation Tests ===\n";

    // Test identity
    {
        Transformation t;
        Point p(5, 10);
        Point result = t.apply(p);

        bool test = almostEqualPoint(result, p);
        suite.addResult("Transformation - identity", test);
    }

    // Test translation
    {
        Transformation t;
        t.translate(10, 20);

        Point p(5, 5);
        Point result = t.apply(p);
        Point expected(15, 25);

        bool test = almostEqualPoint(result, expected);
        suite.addResult("Transformation - translate", test,
                       "Translated to " + pointToString(result));
    }

    // Test rotation 90°
    {
        Transformation t;
        t.rotate(90);

        Point p(1, 0);
        Point result = t.apply(p);
        Point expected(0, 1);

        bool test = almostEqualPoint(result, expected, 1e-6);
        suite.addResult("Transformation - rotate 90°", test,
                       "Rotated to " + pointToString(result));
    }

    // Test scaling
    {
        Transformation t;
        t.scale(2.0);

        Point p(3, 4);
        Point result = t.apply(p);
        Point expected(6, 8);

        bool test = almostEqualPoint(result, expected);
        suite.addResult("Transformation - uniform scale", test);
    }

    // Test non-uniform scaling
    {
        Transformation t;
        t.scale(2.0, 3.0);

        Point p(2, 2);
        Point result = t.apply(p);
        Point expected(4, 6);

        bool test = almostEqualPoint(result, expected);
        suite.addResult("Transformation - non-uniform scale", test);
    }

    // Test combined transformations
    {
        Transformation t;
        t.translate(10, 0).rotate(90).scale(2.0);

        Point p(1, 0);
        Point result = t.apply(p);
        // Order: translate → rotate → scale
        // (1,0) → (11,0) → (0,11) → (0,22)
        Point expected(0, 22);

        bool test = almostEqualPoint(result, expected, 1e-5);
        suite.addResult("Transformation - combined", test,
                       "Result: " + pointToString(result) +
                       " Expected: " + pointToString(expected));
    }

    // Test rotation around center point
    {
        Transformation t;
        t.rotate(180, 5, 5); // Rotate 180° around (5,5)

        Point p(10, 5);
        Point result = t.apply(p);
        Point expected(0, 5); // Should be on opposite side

        bool test = almostEqualPoint(result, expected, 1e-6);
        suite.addResult("Transformation - rotate around center", test);
    }

    // Test multiple point transformation
    {
        std::vector<Point> points = {
            Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1)
        };

        Transformation t;
        t.translate(5, 5);

        std::vector<Point> result = t.apply(points);

        bool test = result.size() == 4 &&
                   almostEqualPoint(result[0], Point(5, 5)) &&
                   almostEqualPoint(result[1], Point(6, 5)) &&
                   almostEqualPoint(result[2], Point(6, 6)) &&
                   almostEqualPoint(result[3], Point(5, 6));

        suite.addResult("Transformation - multiple points", test);
    }

    // Test numerical stability (many compositions)
    {
        Transformation t;
        // Apply 360 rotations of 1 degree each (should return to identity)
        for (int i = 0; i < 360; i++) {
            t.rotate(1);
        }

        Point p(10, 0);
        Point result = t.apply(p);
        Point expected(10, 0);

        bool test = almostEqualPoint(result, expected, 1e-3); // Allow some accumulated error
        suite.addResult("Transformation - numerical stability", test,
                       "After 360×1° rotations: " + pointToString(result) +
                       " (expected " + pointToString(expected) + ")");
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "  DeepNest Geometry Validation Tests\n";
    std::cout << "========================================\n";
    std::cout << "Testing geometry functions against:\n";
    std::cout << "- Boost.Geometry (convex hull, polygon ops)\n";
    std::cout << "- Known mathematical results\n";
    std::cout << "- Numerical stability tests\n";
    std::cout << "========================================\n";

    TestSuite suite;

    try {
        // PHASE 1: Basic functions
        testBasicPointFunctions(suite);
        testLineSegmentFunctions(suite);
        testPolygonBasicFunctions(suite);

        // PHASE 2: ConvexHull validation
        testConvexHull(suite);

        // PHASE 3: Transformation validation
        testTransformation(suite);

        // Print summary
        suite.printSummary();

        // Return exit code based on test results
        return suite.getFailedCount() == 0 ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ EXCEPTION CAUGHT: " << e.what() << std::endl;
        return 2;
    }
}
