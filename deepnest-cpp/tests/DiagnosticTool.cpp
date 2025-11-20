#include <QCoreApplication>
#include <QDebug>
#include <QPainterPath>
#include <iostream>
#include <limits>
#include <cmath>
#include "deepnest/config/DeepNestConfig.h"
#include "deepnest/core/Types.h"
#include "deepnest/nfp/MinkowskiSum.h"
#include "SVGLoader.h"

using namespace deepnest;

void analyzePolygon(const Polygon& poly, int index) {
    if (poly.points.empty()) {
        std::cout << "  Part " << index << ": EMPTY" << std::endl;
        return;
    }

    // Calculate bounds
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();

    for (const auto& pt : poly.points) {
        minX = std::min(minX, pt.x);
        maxX = std::max(maxX, pt.x);
        minY = std::min(minY, pt.y);
        maxY = std::max(maxY, pt.y);
    }

    double width = maxX - minX;
    double height = maxY - minY;
    double maxCoord = std::max(std::fabs(minX), std::max(std::fabs(maxX),
                               std::max(std::fabs(minY), std::fabs(maxY))));

    std::cout << "  Part " << index << ": " << poly.points.size() << " points, "
              << "bounds=[" << minX << ", " << minY << " to " << maxX << ", " << maxY << "], "
              << "size=" << width << "x" << height << ", "
              << "maxCoord=" << maxCoord << std::endl;
}

void testMinkowskiSum(const Polygon& sheet, const Polygon& part, int partId) {
    std::cout << "\n=== Testing Minkowski Sum: Sheet vs Part " << partId << " ===" << std::endl;

    // Try to calculate NFP
    try {
        std::cout << "  Attempting NFP calculation..." << std::endl;
        auto nfps = MinkowskiSum::calculateNFP(sheet, part, true);
        if (nfps.empty()) {
            std::cout << "  RESULT: FAILED (returned empty)" << std::endl;
        } else {
            std::cout << "  RESULT: SUCCESS (" << nfps.size() << " NFP polygons)" << std::endl;
            // Show first NFP polygon details
            if (!nfps[0].points.empty()) {
                std::cout << "    First NFP has " << nfps[0].points.size() << " points" << std::endl;
            }
        }
    }
    catch (const std::exception& e) {
        std::cout << "  RESULT: EXCEPTION: " << e.what() << std::endl;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::string svgPath = argc > 1 ? argv[1] : "../tests/testdata/test__SVGnest-output_clean.svg";

    std::cout << "========================================" << std::endl;
    std::cout << "DeepNest C++ SVG Diagnostic Tool" << std::endl;
    std::cout << "========================================\n" << std::endl;

    std::cout << "Loading SVG: " << svgPath << std::endl;

    // Load SVG
    SVGLoader loader;
    auto loadResult = loader.loadFile(QString::fromStdString(svgPath));

    if (loadResult.shapes.empty()) {
        std::cerr << "ERROR: No parts loaded from SVG" << std::endl;
        return 1;
    }

    std::cout << "Loaded " << loadResult.shapes.size() << " shapes\n" << std::endl;

    // Convert shapes to polygons
    std::vector<Polygon> parts;
    for (const auto& shape : loadResult.shapes) {
        // Convert QPainterPath to Polygon
        Polygon poly;
        for (int i = 0; i < shape.path.elementCount(); ++i) {
            QPainterPath::Element el = shape.path.elementAt(i);
            if (el.type == QPainterPath::MoveToElement || el.type == QPainterPath::LineToElement) {
                poly.points.push_back(Point(el.x, el.y));
            }
        }
        if (!poly.points.empty()) {
            parts.push_back(poly);
        }
    }

    std::cout << "Converted to " << parts.size() << " polygons\n" << std::endl;

    // Analyze each part
    std::cout << "=== PART ANALYSIS ===" << std::endl;
    for (size_t i = 0; i < parts.size() && i < 10; ++i) {
        analyzePolygon(parts[i], i);
    }
    if (parts.size() > 10) {
        std::cout << "  ... and " << (parts.size() - 10) << " more parts" << std::endl;
    }

    // Create a simple sheet
    Polygon sheet;
    sheet.points.push_back(Point(0, 0));
    sheet.points.push_back(Point(500, 0));
    sheet.points.push_back(Point(500, 300));
    sheet.points.push_back(Point(0, 300));

    std::cout << "\n=== SHEET ANALYSIS ===" << std::endl;
    std::cout << "  Sheet: 500x300" << std::endl;

    // Use actual bin from SVG
    Polygon actualSheet;
    actualSheet.points.push_back(Point(0, 0));
    actualSheet.points.push_back(Point(511.822, 0));
    actualSheet.points.push_back(Point(511.822, 339.235));
    actualSheet.points.push_back(Point(0, 339.235));

    // Test sheet-to-part NFP (inner NFP)
    std::cout << "\n=== INNER NFP TESTS (Sheet to Part) ===" << std::endl;
    int testCount = std::min(5, static_cast<int>(parts.size()));
    for (int i = 0; i < testCount; ++i) {
        testMinkowskiSum(actualSheet, parts[i], i);
    }

    // Test part-to-part NFP (outer NFP) - THIS IS CRITICAL
    std::cout << "\n=== OUTER NFP TESTS (Part to Part) ===" << std::endl;
    testCount = std::min(10, static_cast<int>(parts.size()));
    int successCount = 0;
    int failCount = 0;

    for (int i = 0; i < testCount && i < static_cast<int>(parts.size()) - 1; ++i) {
        for (int j = i + 1; j < testCount && j < static_cast<int>(parts.size()); ++j) {
            std::cout << "\nTesting Part " << i << " vs Part " << j << "..." << std::endl;
            try {
                auto nfps = MinkowskiSum::calculateNFP(parts[i], parts[j], false);
                if (nfps.empty()) {
                    std::cout << "  FAILED (empty)" << std::endl;
                    failCount++;
                } else {
                    std::cout << "  SUCCESS (" << nfps.size() << " NFPs)" << std::endl;
                    successCount++;
                }
            }
            catch (const std::exception& e) {
                std::cout << "  EXCEPTION: " << e.what() << std::endl;
                failCount++;
            }
        }
    }

    // Statistics
    std::cout << "\n=== STATISTICS ===" << std::endl;
    std::cout << "  Total parts: " << parts.size() << std::endl;
    std::cout << "  Part-to-part tests: " << (successCount + failCount) << std::endl;
    std::cout << "  Success: " << successCount << std::endl;
    std::cout << "  Failed: " << failCount << std::endl;
    std::cout << "  Success rate: " << (100.0 * successCount / (successCount + failCount)) << "%" << std::endl;
    std::cout << "  INT_MAX: " << std::numeric_limits<int>::max() << std::endl;
    std::cout << "  0.1 * INT_MAX: " << (0.1 * std::numeric_limits<int>::max()) << std::endl;

    return 0;
}
