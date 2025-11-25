/**
 * Standalone test for searchStartPoint debugging
 */

#include <iostream>
#include <vector>
#include "deepnest/core/Point.h"
#include "deepnest/geometry/GeometryUtil.h"

using namespace deepnest;

int main() {
    std::cout << "=== searchStartPoint Debug Tests ===" << std::endl;

    // Test 1: OUTER square - This one PASSES
    {
        std::cout << "\n--- Test 1: OUTER square ---" << std::endl;
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
        };
        std::vector<Point> B = {
            Point(0, 0), Point(5, 0), Point(5, 5), Point(0, 5)
        };

        auto result = GeometryUtil::searchStartPoint(A, B, false, {});
        std::cout << "Result: " << (result.has_value() ? "FOUND" : "NOT FOUND") << std::endl;
        if (result.has_value()) {
            std::cout << "  Point: (" << result->x << ", " << result->y << ")" << std::endl;
        }
    }

    // Test 2: INNER square - This one FAILS
    {
        std::cout << "\n--- Test 2: INNER square ---" << std::endl;
        std::vector<Point> A = {
            Point(0, 0), Point(20, 0), Point(20, 20), Point(0, 20)
        };
        std::vector<Point> B = {
            Point(0, 0), Point(5, 0), Point(5, 5), Point(0, 5)
        };

        auto result = GeometryUtil::searchStartPoint(A, B, true, {});
        std::cout << "Result: " << (result.has_value() ? "FOUND" : "NOT FOUND") << std::endl;
        if (result.has_value()) {
            std::cout << "  Point: (" << result->x << ", " << result->y << ")" << std::endl;
        }
    }

    // Test 3: OUTER triangle - This one FAILS
    {
        std::cout << "\n--- Test 3: OUTER triangle ---" << std::endl;
        std::vector<Point> A = {
            Point(0, 0), Point(10, 0), Point(5, 10)
        };
        // Note: Point now uses int64_t, convert decimal coordinates to integers (x10)
        std::vector<Point> B = {
            Point(0, 0), Point(30, 0), Point(15, 30)
        };

        auto result = GeometryUtil::searchStartPoint(A, B, false, {});
        std::cout << "Result: " << (result.has_value() ? "FOUND" : "NOT FOUND") << std::endl;
        if (result.has_value()) {
            std::cout << "  Point: (" << result->x << ", " << result->y << ")" << std::endl;
        }
    }

    return 0;
}
