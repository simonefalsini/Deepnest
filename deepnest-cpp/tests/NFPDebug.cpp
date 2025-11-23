/**
 * Standalone test for noFitPolygon debugging
 */

#include <iostream>
#include <vector>
#include "deepnest/core/Point.h"
#include "deepnest/geometry/GeometryUtil.h"

using namespace deepnest;

int main() {
    std::cout << "=== noFitPolygon Debug Test ===" << std::endl;

    // Simple square vs square test
    std::vector<Point> A = {
        Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)
    };
    std::vector<Point> B = {
        Point(0, 0), Point(5, 0), Point(5, 5), Point(0, 5)
    };

    std::cout << "\nCalling noFitPolygon(A, B, false, false)..." << std::endl;
    std::cout << "A: 10x10 square, B: 5x5 square, OUTER mode" << std::endl;

    auto result = GeometryUtil::noFitPolygon(A, B, false, false);

    std::cout << "\n=== RESULT ===" << std::endl;
    std::cout << "NFPs generated: " << result.size() << std::endl;

    for (size_t i = 0; i < result.size(); i++) {
        std::cout << "NFP[" << i << "]: " << result[i].size() << " points" << std::endl;
        for (size_t j = 0; j < std::min(result[i].size(), size_t(5)); j++) {
            std::cout << "  [" << j << "]: (" << result[i][j].x << ", " << result[i][j].y << ")" << std::endl;
        }
        if (result[i].size() > 5) {
            std::cout << "  ... (" << (result[i].size() - 5) << " more points)" << std::endl;
        }
    }

    return 0;
}
