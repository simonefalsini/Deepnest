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

#include "SVGLoader.h"
#include "../include/deepnest/core/Polygon.h"
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

using namespace deepnest;

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

/**
 * Save a polygon to SVG file for visualization
 */
bool savePolygonToSVG(const Polygon& poly, const QString& filename) {
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
bool savePairToSVG(const Polygon& polyA, const Polygon& polyB,
                   const std::vector<Polygon>& nfps, const QString& filename) {
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
void testMinkowskiSum(const Polygon& polyA, const Polygon& polyB, bool inside) {
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
    auto nfps = MinkowskiSum::calculateNFP(polyA, polyB, inside);

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
}

/**
 * Test orbital tracing for a polygon pair
 */
void testOrbitalTracing(const Polygon& polyA, const Polygon& polyB, bool inside) {
    std::cout << "\n=== Testing Orbital Tracing ===" << std::endl;
    std::cout << "  Polygon A: " << polyA.points.size() << " points" << std::endl;
    std::cout << "  Polygon B: " << polyB.points.size() << " points" << std::endl;
    std::cout << "  Mode: " << (inside ? "INSIDE" : "OUTSIDE") << std::endl;

    // Test orbital tracing
    auto nfpPoints = GeometryUtil::noFitPolygon(polyA.points, polyB.points, inside, false);

    if (nfpPoints.empty() || nfpPoints[0].empty()) {
        std::cout << "  ❌ FAILED: Orbital tracing returned empty result" << std::endl;
    } else {
        std::cout << "  ✅ SUCCESS: " << nfpPoints.size() << " NFP region(s) generated" << std::endl;
        for (size_t i = 0; i < nfpPoints.size(); ++i) {
            std::cout << "    NFP[" << i << "]: " << nfpPoints[i].size() << " points" << std::endl;
        }

        // Convert to Polygon for visualization
        std::vector<Polygon> nfps;
        for (const auto& points : nfpPoints) {
            Polygon nfp;
            nfp.points = points;
            nfps.push_back(nfp);
        }

        QString filename = QString("polygon_pair_%1_%2_orbital.svg")
                          .arg(polyA.id).arg(polyB.id);
        savePairToSVG(polyA, polyB, nfps, filename);
        std::cout << "  Saved visualization: " << filename.toStdString() << std::endl;
    }
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
    std::vector<Polygon> polygons;
    for (int i = 0; i < static_cast<int>(result.shapes.size()); ++i) {
        QPainterPath shapePath = result.shapes[i].path;
        if (!result.shapes[i].transform.isIdentity()) {
            shapePath = result.shapes[i].transform.map(shapePath);
        }

        Polygon poly = QtBoostConverter::fromQPainterPath(shapePath, i);
        if (poly.isValid()) {
            // Apply Debug Transformations
            Polygon processedPoly = poly;

            // 1. Apply Spacing
            if (debugSpacing > 0) {
                // Use PolygonOperations::offset (which is what applySpacing uses)
                // We treat this as a single contour for simplicity here, or wrap in vector
                std::vector<std::vector<Point>> contours = { processedPoly.points };
                for (const auto& child : processedPoly.children) {
                    contours.push_back(child.points);
                }
                
                auto offsetResults = PolygonOperations::offset(contours, debugSpacing);
                
                // Reconstruct (simplistic, taking largest)
                if (!offsetResults.empty()) {
                    // Find largest
                    size_t bestIdx = 0;
                    double maxArea = 0.0;
                    for(size_t k=0; k<offsetResults.size(); ++k) {
                        double a = std::abs(GeometryUtil::polygonArea(offsetResults[k]));
                        if(a > maxArea) { maxArea = a; bestIdx = k; }
                    }
                    processedPoly.points = offsetResults[bestIdx];
                    processedPoly.children.clear(); // Lost holes for now in this debug tool
                } else {
                    std::cerr << "Warning: Spacing reduced polygon " << i << " to nothing." << std::endl;
                }
            }

            // 2. Apply Convex Hull
            if (useConvexHull) {
                processedPoly.points = ConvexHull::computeHull(processedPoly.points);
                processedPoly.children.clear(); // Hull has no holes
            }

            // 3. Apply Bounding Box
            if (useBoundingBox) {
                BoundingBox box = processedPoly.bounds();
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
        int testsFailed = 0;

        // ========== PART 1: Test all OUTER NFP pairs (part vs part) ==========
        std::cout << "\n========================================" << std::endl;
        std::cout << "  PART 1: OUTER NFP (part vs part)" << std::endl;
        std::cout << "========================================" << std::endl;

        for (size_t i = 0; i < polygons.size(); i++) {
            for (size_t j = i + 1; j < polygons.size(); j++) {
                testsCompleted++;

                std::cout << "\n----------------------------------------" << std::endl;
                std::cout << "Test " << testsCompleted << "/" << totalTests << std::endl;
                std::cout << "Outer NFP: Polygon " << polygons[i].id << " vs " << polygons[j].id << std::endl;
                std::cout << "  A: " << polygons[i].points.size() << " points" << std::endl;
                std::cout << "  B: " << polygons[j].points.size() << " points" << std::endl;
                std::cout << "----------------------------------------" << std::endl;

                try {
                    // Test Minkowski sum (outer NFP)
                    auto nfps = MinkowskiSum::calculateNFP(polygons[i], polygons[j], false);

                    if (nfps.empty()) {
                        std::cout << "  ⚠️  WARNING: Returned empty NFP" << std::endl;
                        testsFailed++;
                    } else {
                        std::cout << "  ✅ SUCCESS: " << nfps.size() << " NFP(s) generated" << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cout << "  ❌ EXCEPTION: " << e.what() << std::endl;
                    testsFailed++;
                } catch (...) {
                    std::cout << "  ❌ UNKNOWN EXCEPTION" << std::endl;
                    testsFailed++;
                }
            }
        }

        // ========== PART 2: Test all INNER NFP pairs (sheet vs part) ==========
        std::cout << "\n========================================" << std::endl;
        std::cout << "  PART 2: INNER NFP (sheet vs part)" << std::endl;
        std::cout << "========================================" << std::endl;

        // Create sheet polygon (bounding rectangle of all polygons + margin)
        if (!polygons.empty()) {
            // Calculate combined bounding box
            BoundingBox combinedBox = polygons[0].bounds();
            for (size_t i = 1; i < polygons.size(); i++) {
                BoundingBox box = polygons[i].bounds();
                combinedBox.x = std::min(combinedBox.x, box.x);
                combinedBox.y = std::min(combinedBox.y, box.y);
                double maxX = std::max(combinedBox.x + combinedBox.width, box.x + box.width);
                double maxY = std::max(combinedBox.y + combinedBox.height, box.y + box.height);
                combinedBox.width = maxX - combinedBox.x;
                combinedBox.height = maxY - combinedBox.y;
            }

            // Add margin (20% on each side)
            double margin = std::max(combinedBox.width, combinedBox.height) * 0.2;
            combinedBox.x -= margin;
            combinedBox.y -= margin;
            combinedBox.width += 2 * margin;
            combinedBox.height += 2 * margin;

            // Create sheet polygon (rectangle)
            Polygon sheet;
            sheet.id = 0;// "sheet";
            sheet.points.push_back({combinedBox.x, combinedBox.y});
            sheet.points.push_back({combinedBox.x + combinedBox.width, combinedBox.y});
            sheet.points.push_back({combinedBox.x + combinedBox.width, combinedBox.y + combinedBox.height});
            sheet.points.push_back({combinedBox.x, combinedBox.y + combinedBox.height});

            std::cout << "Sheet rectangle: " << combinedBox.width << " x " << combinedBox.height << std::endl;
            std::cout << "Sheet origin: (" << combinedBox.x << ", " << combinedBox.y << ")" << std::endl;

            // Test each part against sheet
            for (size_t i = 0; i < polygons.size(); i++) {
                testsCompleted++;

                std::cout << "\n----------------------------------------" << std::endl;
                std::cout << "Test " << testsCompleted << "/" << totalTests << std::endl;
                std::cout << "Inner NFP: Sheet vs Polygon " << polygons[i].id << std::endl;
                std::cout << "  Sheet: " << sheet.points.size() << " points (rectangle)" << std::endl;
                std::cout << "  Part: " << polygons[i].points.size() << " points" << std::endl;
                std::cout << "----------------------------------------" << std::endl;

                try {
                    // Test Minkowski sum (inner NFP)
                    auto nfps = MinkowskiSum::calculateNFP(sheet, polygons[i], true);

                    if (nfps.empty()) {
                        std::cout << "  ⚠️  WARNING: Returned empty NFP" << std::endl;
                        testsFailed++;
                    } else {
                        std::cout << "  ✅ SUCCESS: " << nfps.size() << " NFP(s) generated" << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cout << "  ❌ EXCEPTION: " << e.what() << std::endl;
                    testsFailed++;
                } catch (...) {
                    std::cout << "  ❌ UNKNOWN EXCEPTION" << std::endl;
                    testsFailed++;
                }
            }
        }

        // ========== SUMMARY ==========
        std::cout << "\n========================================" << std::endl;
        std::cout << "  ALL PAIRS TEST SUMMARY" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Total tests: " << testsCompleted << std::endl;
        std::cout << "Succeeded: " << (testsCompleted - testsFailed) << std::endl;
        std::cout << "Failed/Warning: " << testsFailed << std::endl;

        if (testsFailed == 0) {
            std::cout << "\n✅ ALL TESTS PASSED!" << std::endl;
        } else {
            std::cout << "\n⚠️  Some tests failed or returned empty results" << std::endl;
        }

    } else if (targetIdA != -1 && targetIdB != -1) {
        // ==================================================
        // TEST SPECIFIC PAIR WITH ROTATION
        // ==================================================
        std::cout << "\nTesting specific pair: A(id=" << targetIdA << ") vs B(id=" << targetIdB << ")" << std::endl;
        std::cout << "Rotations: A=" << targetRotA << " deg, B=" << targetRotB << " deg" << std::endl;

        Polygon* polyA = nullptr;
        Polygon* polyB = nullptr;

        for (auto& poly : polygons) {
            if (poly.id == targetIdA) polyA = &poly;
            if (poly.id == targetIdB) polyB = &poly;
        }

        if (!polyA || !polyB) {
            std::cerr << "Error: Could not find one or both polygons" << std::endl;
            return 1;
        }

        // Apply rotations
        Polygon rotatedA = *polyA;
        if (std::abs(targetRotA) > 1e-6) {
            rotatedA = polyA->rotate(targetRotA);
            rotatedA.id = polyA->id;
        }

        Polygon rotatedB = *polyB;
        if (std::abs(targetRotB) > 1e-6) {
            rotatedB = polyB->rotate(targetRotB);
            rotatedB.id = polyB->id;
        }

        // Save rotated polygons
        savePolygonToSVG(rotatedA, QString("polygon_%1_A_rot%2.svg").arg(targetIdA).arg(targetRotA));
        savePolygonToSVG(rotatedB, QString("polygon_%1_B_rot%2.svg").arg(targetIdB).arg(targetRotB));

        // Test NFP
        testMinkowskiSum(rotatedA, rotatedB, false);
        testOrbitalTracing(rotatedA, rotatedB, false);

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
            Polygon* polyA = nullptr;
            Polygon* polyB = nullptr;

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
            testMinkowskiSum(*polyA, *polyB, false);
            testOrbitalTracing(*polyA, *polyB, false);
        }
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "Testing complete!" << std::endl;
    std::cout << "Check console output and generated SVG files" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
