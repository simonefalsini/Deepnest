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
        std::cerr << "Usage: " << argv[0] << " <svg-file>" << std::endl;
        std::cerr << "\nThis tool extracts problematic polygon pairs and tests NFP calculations." << std::endl;
        return 1;
    }

    QString svgFile = argv[1];

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
            polygons.push_back(poly);
        }
    }

    std::cout << "Loaded " << polygons.size() << " polygons" << std::endl;

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

    std::cout << "\n========================================" << std::endl;
    std::cout << "Testing complete!" << std::endl;
    std::cout << "Check generated SVG files for visualization" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
