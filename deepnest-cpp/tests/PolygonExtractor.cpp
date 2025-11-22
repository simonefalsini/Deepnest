/**
 * @file PolygonExtractor.cpp
 * @brief Extract and test problematic polygon pairs
 *
 * This tool extracts specific polygon pairs from an SVG file and:
 * 1. Saves them as individual SVG files for visualization
 * 2. Tests Minkowski sum calculation
 * 3. Tests orbital tracing NFP calculation
 * 4. Exports debug information
 */
#include "../include/deepnest/engine/NestingEngine.h"

#include "SVGLoader.h"
#include "../include/deepnest/nfp/MinkowskiSum.h"
#include "../include/deepnest/geometry/GeometryUtil.h"
#include "../include/deepnest/geometry/ConvexHull.h"
#include "../include/deepnest/geometry/PolygonOperations.h"
#include "../include/deepnest/converters/QtBoostConverter.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <iostream>
#include <vector>


// Known problematic pairs from test runs
struct ProblematicPair {
    int idA;
    int idB;
    std::string description;
};

std::vector<ProblematicPair> problematicPairs = {
    {45, 55, "9 points vs 8 points"},
    {57, 58, "8 points vs 8 points"},
    {47, 64, "8 points vs 4 points"},
    {30, 68, "14 points vs 5 points"},
    {39, 60, "13 points vs 8 points"},
};

// Structure to track failed NFP comparisons
struct NFPComparisonFailure {
    int idA;
    int idB;
    bool isInner;
    std::string failureType;  // "empty_minkowski", "empty_orbital", "mismatch"
    double hausdorffDistance;
    double areaPercent;
    int minkowskiPoints;
    int orbitalPoints;
};

// Comparison result structure
struct ComparisonResult {
    bool passed;
    std::string failureType;
    double hausdorffDistance;
    double areaPercent;
    int minkowskiPoints;
    int orbitalPoints;
};

/**
 * Calculate Hausdorff distance between two polygons
 * Returns the maximum minimum distance from any point of poly1 to poly2
 */
double hausdorffDistance(const std::vector<deepnest::Point>& poly1, const std::vector<deepnest::Point>& poly2) {
    if (poly1.empty() || poly2.empty()) {
        return std::numeric_limits<double>::infinity();
    }

    double maxDist = 0.0;

    // For each point in poly1, find minimum distance to any point in poly2
    for (const auto& p1 : poly1) {
        double minDist = std::numeric_limits<double>::infinity();
        for (const auto& p2 : poly2) {
            double dx = p1.x - p2.x;
            double dy = p1.y - p2.y;
            double dist = std::sqrt(dx * dx + dy * dy);
            minDist = std::min(minDist, dist);
        }
        maxDist = std::max(maxDist, minDist);
    }

    return maxDist;
}

/**
 * Compare two NFP results and return comparison metrics (silent version for --all-pairs)
 */
ComparisonResult compareNFPsSilent(const std::vector<deepnest::Polygon>& minkowskiNFPs,
                                    const std::vector<deepnest::Polygon>& orbitalNFPs) {
    ComparisonResult result;
    result.passed = false;
    result.minkowskiPoints = minkowskiNFPs.empty() ? 0 : (minkowskiNFPs[0].points.empty() ? 0 : minkowskiNFPs[0].points.size());
    result.orbitalPoints = orbitalNFPs.empty() ? 0 : (orbitalNFPs[0].points.empty() ? 0 : orbitalNFPs[0].points.size());

    if (minkowskiNFPs.empty() && orbitalNFPs.empty()) {
        result.failureType = "both_empty";
        result.hausdorffDistance = 0.0;
        result.areaPercent = 0.0;
        result.passed = true;  // Both failed equally
        return result;
    }

    if (minkowskiNFPs.empty() || minkowskiNFPs[0].points.empty()) {
        result.failureType = "empty_minkowski";
        result.hausdorffDistance = std::numeric_limits<double>::infinity();
        result.areaPercent = 100.0;
        return result;
    }

    if (orbitalNFPs.empty() || orbitalNFPs[0].points.empty()) {
        result.failureType = "empty_orbital";
        result.hausdorffDistance = std::numeric_limits<double>::infinity();
        result.areaPercent = 100.0;
        return result;
    }

    // Compare first NFP
    const auto& minkNFP = minkowskiNFPs[0];
    const auto& orbNFP = orbitalNFPs[0];

    // Calculate Hausdorff distance
    double dist_M_to_O = hausdorffDistance(minkNFP.points, orbNFP.points);
    double dist_O_to_M = hausdorffDistance(orbNFP.points, minkNFP.points);
    result.hausdorffDistance = std::max(dist_M_to_O, dist_O_to_M);

    // Calculate area difference
    double minkArea = std::abs(deepnest::GeometryUtil::polygonArea(minkNFP.points));
    double orbArea = std::abs(deepnest::GeometryUtil::polygonArea(orbNFP.points));
    double areaDiff = std::abs(minkArea - orbArea);
    result.areaPercent = (minkArea > 0) ? (areaDiff / minkArea * 100.0) : 0.0;

    // Determine if NFPs match
    const double TOLERANCE = 0.5;
    const double AREA_TOLERANCE = 5.0;  // 5% area difference

    if (result.hausdorffDistance < TOLERANCE && result.areaPercent < AREA_TOLERANCE) {
        result.passed = true;
        result.failureType = "pass";
    } else if (result.hausdorffDistance < 5.0) {
        result.passed = false;
        result.failureType = "minor_mismatch";
    } else {
        result.passed = false;
        result.failureType = "major_mismatch";
    }

    return result;
}

/**
 * Compare two NFP results and report differences (verbose version for single tests)
 */
void compareNFPs(const std::vector<deepnest::Polygon>& minkowskiNFPs,
                 const std::vector<deepnest::Polygon>& orbitalNFPs,
                 const std::string& testLabel = "") {
    std::cout << "\n=== NFP Comparison" << (testLabel.empty() ? "" : " (" + testLabel + ")") << " ===" << std::endl;

    ComparisonResult result = compareNFPsSilent(minkowskiNFPs, orbitalNFPs);

    std::cout << "  Point count comparison:" << std::endl;
    std::cout << "    Minkowski: " << result.minkowskiPoints << " points" << std::endl;
    std::cout << "    Orbital:   " << result.orbitalPoints << " points" << std::endl;

    if (result.failureType == "both_empty") {
        std::cout << "  Both algorithms returned empty NFPs" << std::endl;
        return;
    }

    if (result.failureType == "empty_minkowski") {
        std::cout << "  ⚠️  WARNING: Minkowski returned empty, Orbital returned NFP" << std::endl;
        return;
    }

    if (result.failureType == "empty_orbital") {
        std::cout << "  ⚠️  WARNING: Orbital returned empty, Minkowski returned NFP" << std::endl;
        return;
    }

    std::cout << "  Hausdorff distance: " << result.hausdorffDistance << std::endl;
    std::cout << "  Area difference: " << result.areaPercent << "%" << std::endl;

    if (result.passed) {
        std::cout << "  ✅ MATCH: NFPs are equivalent" << std::endl;
    } else if (result.failureType == "minor_mismatch") {
        std::cout << "  ⚠️  MINOR DIFFERENCE: NFPs are close but not exact" << std::endl;
    } else {
        std::cout << "  ❌ MISMATCH: NFPs are significantly different" << std::endl;
    }
}

/**
 * Save a polygon to SVG file for visualization
 */
bool savePolygonToSVG(const deepnest::Polygon& poly, const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cerr << "Failed to open " << filename.toStdString() << std::endl;
        return false;
    }

    QTextStream out(&file);

    // Calculate bounding box
    auto bbox = poly.bounds();
    double padding = 10.0;

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" ";
    out << "width=\"" << (bbox.width + 2*padding) << "\" ";
    out << "height=\"" << (bbox.height + 2*padding) << "\" ";
    out << "viewBox=\"" << (bbox.x - padding) << " " << (bbox.y - padding) << " ";
    out << (bbox.width + 2*padding) << " " << (bbox.height + 2*padding) << "\">\n";

    // Add polygon
    out << "  <polygon points=\"";
    for (size_t i = 0; i < poly.points.size(); ++i) {
        if (i > 0) out << " ";
        out << poly.points[i].x << "," << poly.points[i].y;
    }
    out << "\" fill=\"lightblue\" stroke=\"blue\" stroke-width=\"0.5\"/>\n";

    // Add point markers for debugging
    for (size_t i = 0; i < poly.points.size(); ++i) {
        out << "  <circle cx=\"" << poly.points[i].x << "\" cy=\"" << poly.points[i].y
            << "\" r=\"0.5\" fill=\"red\"/>\n";
        out << "  <text x=\"" << poly.points[i].x + 1 << "\" y=\"" << poly.points[i].y - 1
            << "\" font-size=\"3\" fill=\"black\">" << i << "</text>\n";
    }

    out << "</svg>\n";
    file.close();

    std::cout << "  Saved to: " << filename.toStdString() << std::endl;
    return true;
}

/**
 * Save both polygons and their NFP result to combined SVG
 */
bool savePairToSVG(const deepnest::Polygon& polyA, const deepnest::Polygon& polyB,
                   const std::vector<deepnest::Polygon>& nfps, const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);

    // Calculate combined bounding box
    auto bboxA = polyA.bounds();
    auto bboxB = polyB.bounds();

    double minX = std::min(bboxA.x, bboxB.x);
    double minY = std::min(bboxA.y, bboxB.y);
    double maxX = std::max(bboxA.x + bboxA.width, bboxB.x + bboxB.width);
    double maxY = std::max(bboxA.y + bboxA.height, bboxB.y + bboxB.height);

    // Add NFP bounds if present
    for (const auto& nfp : nfps) {
        if (!nfp.points.empty()) {
            auto nfpBox = nfp.bounds();
            minX = std::min(minX, nfpBox.x);
            minY = std::min(minY, nfpBox.y);
            maxX = std::max(maxX, nfpBox.x + nfpBox.width);
            maxY = std::max(maxY, nfpBox.y + nfpBox.height);
        }
    }

    double width = maxX - minX;
    double height = maxY - minY;
    double padding = 20.0;

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" ";
    out << "width=\"" << (width + 2*padding) << "\" ";
    out << "height=\"" << (height + 2*padding) << "\" ";
    out << "viewBox=\"" << (minX - padding) << " " << (minY - padding) << " ";
    out << (width + 2*padding) << " " << (height + 2*padding) << "\">\n";

    // Add title
    out << "  <text x=\"" << (minX - padding + 5) << "\" y=\"" << (minY - padding + 10)
        << "\" font-size=\"8\" fill=\"black\">Polygon A(id=" << polyA.id
        << ") vs B(id=" << polyB.id << ")</text>\n";

    // Draw polygon A
    out << "  <polygon points=\"";
    for (size_t i = 0; i < polyA.points.size(); ++i) {
        if (i > 0) out << " ";
        out << polyA.points[i].x << "," << polyA.points[i].y;
    }
    out << "\" fill=\"lightcoral\" fill-opacity=\"0.5\" stroke=\"red\" stroke-width=\"1\"/>\n";

    // Draw polygon B
    out << "  <polygon points=\"";
    for (size_t i = 0; i < polyB.points.size(); ++i) {
        if (i > 0) out << " ";
        out << polyB.points[i].x << "," << polyB.points[i].y;
    }
    out << "\" fill=\"lightblue\" fill-opacity=\"0.5\" stroke=\"blue\" stroke-width=\"1\"/>\n";

    // Draw NFPs if present
    for (const auto& nfp : nfps) {
        if (!nfp.points.empty()) {
            out << "  <polygon points=\"";
            for (size_t i = 0; i < nfp.points.size(); ++i) {
                if (i > 0) out << " ";
                out << nfp.points[i].x << "," << nfp.points[i].y;
            }
            out << "\" fill=\"lightgreen\" fill-opacity=\"0.3\" stroke=\"green\" stroke-width=\"0.5\"/>\n";
        }
    }

    out << "</svg>\n";
    file.close();

    return true;
}

/**
 * Test Minkowski sum for a polygon pair
 */
std::vector<deepnest::Polygon> testMinkowskiSum(const deepnest::Polygon& polyA, const deepnest::Polygon& polyB, bool inside) {
    std::cout << "\n=== Testing Minkowski Sum ===" << std::endl;
    std::cout << "  Polygon A: " << polyA.points.size() << " points" << std::endl;
    std::cout << "  Polygon B: " << polyB.points.size() << " points" << std::endl;
    std::cout << "  Mode: " << (inside ? "INSIDE" : "OUTSIDE") << std::endl;

    // Print first few points for debugging
    std::cout << "  A points[0-3]: ";
    for (size_t i = 0; i < std::min(size_t(4), polyA.points.size()); ++i) {
        std::cout << "(" << polyA.points[i].x << "," << polyA.points[i].y << ") ";
    }
    std::cout << std::endl;

    std::cout << "  B points[0-3]: ";
    for (size_t i = 0; i < std::min(size_t(4), polyB.points.size()); ++i) {
        std::cout << "(" << polyB.points[i].x << "," << polyB.points[i].y << ") ";
    }
    std::cout << std::endl;

    // Test Minkowski sum
    auto nfps = deepnest::MinkowskiSum::calculateNFP(polyA, polyB, inside);

    // CRITICAL: Apply B[0] translation to match NFPCalculator behavior
    // JavaScript (background.js:202-203): clipperNfp[i].x += B[0].x; clipperNfp[i].y += B[0].y;
    // C++ (NFPCalculator.cpp:113): result.translate(B.points[0].x, B.points[0].y)
    if (!nfps.empty() && !polyB.points.empty()) {
        for (auto& nfp : nfps) {
            nfp = nfp.translate(polyB.points[0].x, polyB.points[0].y);
        }
        std::cout << "  Translated NFP by B[0] = (" << polyB.points[0].x << ", " << polyB.points[0].y << ")" << std::endl;
    }

    if (nfps.empty()) {
        std::cout << "  ❌ FAILED: Minkowski sum returned empty result" << std::endl;
    } else {
        std::cout << "  ✅ SUCCESS: " << nfps.size() << " NFP polygon(s) generated" << std::endl;
        for (size_t i = 0; i < nfps.size(); ++i) {
            std::cout << "    NFP[" << i << "]: " << nfps[i].points.size() << " points" << std::endl;
        }

        // Save combined visualization
        QString filename = QString("polygon_pair_%1_%2_minkowski.svg")
                          .arg(polyA.id).arg(polyB.id);
        savePairToSVG(polyA, polyB, nfps, filename);
        std::cout << "  Saved visualization: " << filename.toStdString() << std::endl;
    }

    return nfps;
}

/**
 * Test orbital tracing for a polygon pair
 */
std::vector<deepnest::Polygon> testOrbitalTracing(const deepnest::Polygon& polyA, const deepnest::Polygon& polyB, bool inside) {
    std::cout << "\n=== Testing Orbital Tracing ===" << std::endl;
    std::cout << "  Polygon A: " << polyA.points.size() << " points" << std::endl;
    std::cout << "  Polygon B: " << polyB.points.size() << " points" << std::endl;
    std::cout << "  Mode: " << (inside ? "INSIDE" : "OUTSIDE") << std::endl;

    // Test orbital tracing
    auto nfpPoints = deepnest::GeometryUtil::noFitPolygon(polyA.points, polyB.points, inside, false);

    std::vector<deepnest::Polygon> nfps;

    if (nfpPoints.empty() || nfpPoints[0].empty()) {
        std::cout << "  ❌ FAILED: Orbital tracing returned empty result" << std::endl;
    } else {
        std::cout << "  ✅ SUCCESS: " << nfpPoints.size() << " NFP region(s) generated" << std::endl;
        for (size_t i = 0; i < nfpPoints.size(); ++i) {
            std::cout << "    NFP[" << i << "]: " << nfpPoints[i].size() << " points" << std::endl;
        }

        // Convert to Polygon for visualization
        for (const auto& points : nfpPoints) {
            deepnest::Polygon nfp;
            nfp.points = points;
            nfps.push_back(nfp);
        }

        QString filename = QString("polygon_pair_%1_%2_orbital.svg")
                          .arg(polyA.id).arg(polyB.id);
        savePairToSVG(polyA, polyB, nfps, filename);
        std::cout << "  Saved visualization: " << filename.toStdString() << std::endl;
    }

    return nfps;
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <svg-file> [--all-pairs]" << std::endl;
        std::cerr << "\nOptions:" << std::endl;
        std::cerr << "  <svg-file>     SVG file to load polygons from" << std::endl;
        std::cerr << "  --all-pairs    Test ALL possible NFP pairs (outer + inner)" << std::endl;
        std::cerr << "                 Without this, tests only predefined problematic pairs" << std::endl;
        std::cerr << "\nThis tool extracts polygon pairs and tests NFP calculations." << std::endl;
        return 1;
    }

    QString svgFile = argv[1];
    bool testAllPairs = false;

    // Check for options
    int targetIdA = -1;
    int targetIdB = -1;
    double targetRotA = 0.0;
    double targetRotB = 0.0;

    for (int i = 2; i < argc; i++) {
        QString arg = argv[i];
        if (arg == "--all-pairs") {
            testAllPairs = true;
        } else if (arg == "--test-pair" && i + 2 < argc) {
            targetIdA = QString(argv[++i]).toInt();
            targetIdB = QString(argv[++i]).toInt();
        } else if (arg == "--rotA" && i + 1 < argc) {
            targetRotA = QString(argv[++i]).toDouble();
        } else if (arg == "--rotB" && i + 1 < argc) {
            targetRotB = QString(argv[++i]).toDouble();
        }
    }

    // Additional Debug Options
    double debugSpacing = 0.0;
    bool useConvexHull = false;
    bool useBoundingBox = false;

    for (int i = 2; i < argc; i++) {
        QString arg = argv[i];
        if (arg == "--spacing" && i + 1 < argc) {
            debugSpacing = QString(argv[++i]).toDouble();
        } else if (arg == "--use-hull") {
            useConvexHull = true;
        } else if (arg == "--use-box") {
            useBoundingBox = true;
        }
    }

    if (debugSpacing > 0 || useConvexHull || useBoundingBox) {
        std::cout << "Debug Transformations Enabled:" << std::endl;
        if (debugSpacing > 0) std::cout << "  - Spacing: " << debugSpacing << std::endl;
        if (useConvexHull) std::cout << "  - Use Convex Hull" << std::endl;
        if (useBoundingBox) std::cout << "  - Use Bounding Box" << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "  Polygon Extractor & NFP Tester" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Loading: " << svgFile.toStdString() << std::endl;

    // Load SVG
    SVGLoader::LoadResult result = SVGLoader::loadFile(svgFile);

    if (!result.success()) {
        std::cerr << "Failed to load SVG: " << result.errorMessage.toStdString() << std::endl;
        return 1;
    }

    // Convert shapes to polygons
    std::vector<deepnest::Polygon> polygons;
    for (int i = 0; i < static_cast<int>(result.shapes.size()); ++i) {
        QPainterPath shapePath = result.shapes[i].path;
        if (!result.shapes[i].transform.isIdentity()) {
            shapePath = result.shapes[i].transform.map(shapePath);
        }

        deepnest::Polygon poly = deepnest::QtBoostConverter::fromQPainterPath(shapePath, i);
        if (poly.isValid()) {
            // Apply Debug Transformations
            deepnest::Polygon processedPoly = poly;

            // 1. Apply Spacing
            if (debugSpacing > 0) {
                processedPoly = deepnest::NestingEngine::applySpacing(processedPoly, debugSpacing, 0.3);
            }

            // 2. Apply Convex Hull
            if (useConvexHull) {
                processedPoly.points = deepnest::ConvexHull::computeHull(processedPoly.points);
                processedPoly.children.clear(); // Hull has no holes
            }

            // 3. Apply Bounding Box
            if (useBoundingBox) {
                deepnest::BoundingBox box = processedPoly.bounds();
                processedPoly.points = {
                    {box.x, box.y},
                    {box.x + box.width, box.y},
                    {box.x + box.width, box.y + box.height},
                    {box.x, box.y + box.height}
                };
                processedPoly.children.clear();
            }
            
            // Update ID to reflect transformations
            processedPoly.id = poly.id;

            polygons.push_back(processedPoly);
        }
    }

    std::cout << "Loaded " << polygons.size() << " polygons" << std::endl;

    if (testAllPairs) {
        // ==================================================
        // TEST ALL POSSIBLE NFP PAIRS
        // ==================================================
        std::cout << "\n========================================" << std::endl;
        std::cout << "  TESTING ALL PAIRS MODE" << std::endl;
        std::cout << "========================================" << std::endl;

        int totalOuterPairs = (polygons.size() * (polygons.size() - 1)) / 2;
        int totalInnerPairs = polygons.size();
        int totalTests = totalOuterPairs + totalInnerPairs;

        std::cout << "Total tests to perform: " << totalTests << std::endl;
        std::cout << "  - Outer NFP (part vs part): " << totalOuterPairs << " pairs" << std::endl;
        std::cout << "  - Inner NFP (sheet vs part): " << totalInnerPairs << " pairs" << std::endl;

        int testsCompleted = 0;
        int testsPassed = 0;
        int testsMinorDiff = 0;
        int testsMajorMismatch = 0;
        int testsEmpty = 0;
        std::vector<NFPComparisonFailure> failures;

        // ========== PART 1: Test all OUTER NFP pairs (part vs part) ==========
        std::cout << "\n========================================" << std::endl;
        std::cout << "  PART 1: OUTER NFP (part vs part)" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Testing " << totalOuterPairs << " pairs..." << std::endl;

        for (size_t i = 0; i < polygons.size(); i++) {
            for (size_t j = i + 1; j < polygons.size(); j++) {
                testsCompleted++;

                // Progress indicator every 100 tests
                if (testsCompleted % 100 == 0 || testsCompleted == 1) {
                    std::cout << "  Progress: " << testsCompleted << "/" << totalTests
                              << " (Pass: " << testsPassed << ", Fail: " << (testsCompleted - testsPassed) << ")" << std::endl;
                }

                try {
                    // Test both algorithms
                    auto minkowskiNFPs = deepnest::MinkowskiSum::calculateNFP(polygons[i], polygons[j], false);

                    // Apply B[0] translation to Minkowski NFP
                    if (!minkowskiNFPs.empty() && !polygons[j].points.empty()) {
                        for (auto& nfp : minkowskiNFPs) {
                            nfp = nfp.translate(polygons[j].points[0].x, polygons[j].points[0].y);
                        }
                    }

                    auto orbitalPoints = deepnest::GeometryUtil::noFitPolygon(polygons[i].points, polygons[j].points, false, false);

                    // Convert orbital to Polygon
                    std::vector<deepnest::Polygon> orbitalNFPs;
                    for (const auto& points : orbitalPoints) {
                        deepnest::Polygon nfp;
                        nfp.points = points;
                        orbitalNFPs.push_back(nfp);
                    }

                    // Compare results
                    auto comparison = compareNFPsSilent(minkowskiNFPs, orbitalNFPs);

                    if (comparison.passed) {
                        testsPassed++;
                    } else {
                        // Record failure
                        NFPComparisonFailure failure;
                        failure.idA = polygons[i].id;
                        failure.idB = polygons[j].id;
                        failure.isInner = false;
                        failure.failureType = comparison.failureType;
                        failure.hausdorffDistance = comparison.hausdorffDistance;
                        failure.areaPercent = comparison.areaPercent;
                        failure.minkowskiPoints = comparison.minkowskiPoints;
                        failure.orbitalPoints = comparison.orbitalPoints;
                        failures.push_back(failure);

                        if (comparison.failureType == "minor_mismatch") {
                            testsMinorDiff++;
                        } else if (comparison.failureType == "major_mismatch") {
                            testsMajorMismatch++;
                        } else {
                            testsEmpty++;
                        }
                    }

                } catch (const std::exception& e) {
                    NFPComparisonFailure failure;
                    failure.idA = polygons[i].id;
                    failure.idB = polygons[j].id;
                    failure.isInner = false;
                    failure.failureType = "exception: " + std::string(e.what());
                    failure.hausdorffDistance = std::numeric_limits<double>::infinity();
                    failure.areaPercent = 100.0;
                    failure.minkowskiPoints = 0;
                    failure.orbitalPoints = 0;
                    failures.push_back(failure);
                    testsEmpty++;
                } catch (...) {
                    NFPComparisonFailure failure;
                    failure.idA = polygons[i].id;
                    failure.idB = polygons[j].id;
                    failure.isInner = false;
                    failure.failureType = "unknown_exception";
                    failure.hausdorffDistance = std::numeric_limits<double>::infinity();
                    failure.areaPercent = 100.0;
                    failure.minkowskiPoints = 0;
                    failure.orbitalPoints = 0;
                    failures.push_back(failure);
                    testsEmpty++;
                }
            }
        }

        // ========== PART 2: Test all INNER NFP pairs (sheet vs part) ==========
        std::cout << "\n========================================" << std::endl;
        std::cout << "  PART 2: INNER NFP (sheet vs part)" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Skipping INNER NFP tests (focus on OUTER for now)" << std::endl;
        // Note: INNER NFP testing can be added later if needed

        // ========== SUMMARY ==========
        std::cout << "\n========================================" << std::endl;
        std::cout << "  ALL PAIRS TEST SUMMARY" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Total tests completed: " << testsCompleted << std::endl;
        std::cout << "  ✅ PASSED: " << testsPassed << " (" << (testsPassed * 100.0 / testsCompleted) << "%)" << std::endl;
        std::cout << "  ⚠️  MINOR DIFF: " << testsMinorDiff << std::endl;
        std::cout << "  ❌ MAJOR MISMATCH: " << testsMajorMismatch << std::endl;
        std::cout << "  💥 EMPTY/EXCEPTION: " << testsEmpty << std::endl;

        // Print failed comparisons
        if (!failures.empty()) {
            std::cout << "\n========================================" << std::endl;
            std::cout << "  FAILED COMPARISONS (" << failures.size() << ")" << std::endl;
            std::cout << "========================================" << std::endl;

            // Print first 50 failures in detail
            int printLimit = std::min(50, (int)failures.size());
            for (int i = 0; i < printLimit; i++) {
                const auto& f = failures[i];
                std::cout << "\n" << (i+1) << ". Pair " << f.idA << " vs " << f.idB
                          << " (" << (f.isInner ? "INNER" : "OUTER") << ")" << std::endl;
                std::cout << "   Type: " << f.failureType << std::endl;
                std::cout << "   Points: Mink=" << f.minkowskiPoints << ", Orb=" << f.orbitalPoints << std::endl;
                if (std::isfinite(f.hausdorffDistance)) {
                    std::cout << "   Hausdorff: " << f.hausdorffDistance << ", Area diff: " << f.areaPercent << "%" << std::endl;
                }
            }

            if (failures.size() > 50) {
                std::cout << "\n... and " << (failures.size() - 50) << " more failures" << std::endl;
            }

            // Print command to re-test first failure
            if (!failures.empty()) {
                const auto& first = failures[0];
                std::cout << "\n========================================" << std::endl;
                std::cout << "  TO DEBUG FIRST FAILURE" << std::endl;
                std::cout << "========================================" << std::endl;
                std::cout << "Run:" << std::endl;
                std::cout << "./bin/PolygonExtractor " << argv[1] << " --test-pair "
                          << first.idA << " " << first.idB << std::endl;
            }
        } else {
            std::cout << "\n✅ ALL TESTS PASSED!" << std::endl;
        }

    } else if (targetIdA != -1 && targetIdB != -1) {
        // ==================================================
        // TEST SPECIFIC PAIR WITH ROTATION
        // ==================================================
        std::cout << "\nTesting specific pair: A(id=" << targetIdA << ") vs B(id=" << targetIdB << ")" << std::endl;
        std::cout << "Rotations: A=" << targetRotA << " deg, B=" << targetRotB << " deg" << std::endl;

        deepnest::Polygon* polyA = nullptr;
        deepnest::Polygon* polyB = nullptr;

        for (auto& poly : polygons) {
            if (poly.id == targetIdA) polyA = &poly;
            if (poly.id == targetIdB) polyB = &poly;
        }

        if (!polyA || !polyB) {
            std::cerr << "Error: Could not find one or both polygons" << std::endl;
            return 1;
        }

        // Apply rotations
        deepnest::Polygon rotatedA = *polyA;
        if (std::abs(targetRotA) > 1e-6) {
            rotatedA = polyA->rotate(targetRotA);
            rotatedA.id = polyA->id;
        }

        deepnest::Polygon rotatedB = *polyB;
        if (std::abs(targetRotB) > 1e-6) {
            rotatedB = polyB->rotate(targetRotB);
            rotatedB.id = polyB->id;
        }

        // Save rotated polygons
        savePolygonToSVG(rotatedA, QString("polygon_%1_A_rot%2.svg").arg(targetIdA).arg(targetRotA));
        savePolygonToSVG(rotatedB, QString("polygon_%1_B_rot%2.svg").arg(targetIdB).arg(targetRotB));

        // Test NFP
        auto minkowskiNFPs = testMinkowskiSum(rotatedA, rotatedB, false);
        auto orbitalNFPs = testOrbitalTracing(rotatedA, rotatedB, false);

        // Compare results
        compareNFPs(minkowskiNFPs, orbitalNFPs, "Pair " + std::to_string(targetIdA) + " vs " + std::to_string(targetIdB));

    } else {
        // ==================================================
        // TEST ONLY PREDEFINED PROBLEMATIC PAIRS
        // ==================================================
        std::cout << "\nTesting predefined problematic pairs..." << std::endl;
        std::cout << "(Use --all-pairs to test all possible combinations)" << std::endl;
        std::cout << "(Use --test-pair <idA> <idB> [--rotA <deg>] [--rotB <deg>] to test specific pair)" << std::endl;

        // Process each problematic pair
        for (const auto& pair : problematicPairs) {
            std::cout << "\n========================================" << std::endl;
            std::cout << "Testing pair: A(id=" << pair.idA << ") vs B(id=" << pair.idB << ")" << std::endl;
            std::cout << "Description: " << pair.description << std::endl;
            std::cout << "========================================" << std::endl;

            // Find polygons by ID
            deepnest::Polygon* polyA = nullptr;
            deepnest::Polygon* polyB = nullptr;

            for (auto& poly : polygons) {
                if (poly.id == pair.idA) polyA = &poly;
                if (poly.id == pair.idB) polyB = &poly;
            }

            if (!polyA || !polyB) {
                std::cout << "⚠️  Could not find one or both polygons" << std::endl;
                continue;
            }

            // Save individual polygons
            savePolygonToSVG(*polyA, QString("polygon_%1_A.svg").arg(pair.idA));
            savePolygonToSVG(*polyB, QString("polygon_%1_B.svg").arg(pair.idB));

            // Test both algorithms with OUTSIDE mode (part-to-part collision)
            auto minkowskiNFPs = testMinkowskiSum(*polyA, *polyB, false);
            auto orbitalNFPs = testOrbitalTracing(*polyA, *polyB, false);

            // Compare results
            compareNFPs(minkowskiNFPs, orbitalNFPs, "Pair " + std::to_string(pair.idA) + " vs " + std::to_string(pair.idB));
        }
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "Testing complete!" << std::endl;
    std::cout << "Check console output and generated SVG files" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
