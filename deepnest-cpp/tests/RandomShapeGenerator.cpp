#include "RandomShapeGenerator.h"
#include <QRectF>
#include <cmath>
#include <algorithm>

// Generate random rectangle
QPainterPath RandomShapeGenerator::generateRandomRectangle(
    double minWidth, double maxWidth,
    double minHeight, double maxHeight,
    std::mt19937& rng)
{
    std::uniform_real_distribution<> widthDist(minWidth, maxWidth);
    std::uniform_real_distribution<> heightDist(minHeight, maxHeight);

    double w = widthDist(rng);
    double h = heightDist(rng);

    QPainterPath path;
    path.addRect(0, 0, w, h);
    return path;
}

// Generate random square
QPainterPath RandomShapeGenerator::generateRandomSquare(
    double minSize, double maxSize,
    std::mt19937& rng)
{
    std::uniform_real_distribution<> sizeDist(minSize, maxSize);
    double size = sizeDist(rng);

    QPainterPath path;
    path.addRect(0, 0, size, size);
    return path;
}

// Generate regular polygon
QPainterPath RandomShapeGenerator::generateRegularPolygon(
    int sides,
    double minRadius, double maxRadius,
    std::mt19937& rng)
{
    if (sides < 3) sides = 3;

    std::uniform_real_distribution<> radiusDist(minRadius, maxRadius);
    double radius = radiusDist(rng);

    QPainterPath path;

    for (int i = 0; i < sides; ++i) {
        double angle = 2.0 * M_PI * i / sides;
        double x = radius * std::cos(angle) + radius;
        double y = radius * std::sin(angle) + radius;

        if (i == 0) {
            path.moveTo(x, y);
        } else {
            path.lineTo(x, y);
        }
    }

    path.closeSubpath();
    return path;
}

// Generate irregular polygon
QPainterPath RandomShapeGenerator::generateIrregularPolygon(
    int sides,
    double minRadius, double maxRadius,
    double irregularity,
    std::mt19937& rng)
{
    if (sides < 3) sides = 3;

    std::uniform_real_distribution<> radiusDist(minRadius, maxRadius);
    std::uniform_real_distribution<> angleDist(-irregularity * M_PI / sides,
                                              irregularity * M_PI / sides);
    std::uniform_real_distribution<> radiusVariation(1.0 - irregularity * 0.5,
                                                     1.0 + irregularity * 0.5);

    double baseRadius = radiusDist(rng);
    QPainterPath path;

    for (int i = 0; i < sides; ++i) {
        double angle = 2.0 * M_PI * i / sides + angleDist(rng);
        double radius = baseRadius * radiusVariation(rng);
        double x = radius * std::cos(angle) + baseRadius;
        double y = radius * std::sin(angle) + baseRadius;

        if (i == 0) {
            path.moveTo(x, y);
        } else {
            path.lineTo(x, y);
        }
    }

    path.closeSubpath();
    return path;
}

// Generate random circle
QPainterPath RandomShapeGenerator::generateRandomCircle(
    double minRadius, double maxRadius,
    std::mt19937& rng)
{
    std::uniform_real_distribution<> radiusDist(minRadius, maxRadius);
    double radius = radiusDist(rng);

    QPainterPath path;
    path.addEllipse(QPointF(radius, radius), radius, radius);
    return path;
}

// Generate L-shape
QPainterPath RandomShapeGenerator::generateLShape(
    double minSize, double maxSize,
    std::mt19937& rng)
{
    std::uniform_real_distribution<> sizeDist(minSize, maxSize);

    double width1 = sizeDist(rng);
    double height1 = sizeDist(rng);
    double width2 = sizeDist(rng) * 0.6;  // Smaller branch
    double height2 = sizeDist(rng) * 0.6;

    QPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(width1, 0);
    path.lineTo(width1, height2);
    path.lineTo(width2, height2);
    path.lineTo(width2, height1);
    path.lineTo(0, height1);
    path.closeSubpath();

    return path;
}

// Generate T-shape
QPainterPath RandomShapeGenerator::generateTShape(
    double minSize, double maxSize,
    std::mt19937& rng)
{
    std::uniform_real_distribution<> sizeDist(minSize, maxSize);

    double topWidth = sizeDist(rng);
    double topHeight = sizeDist(rng) * 0.4;
    double stemWidth = topWidth * 0.4;
    double stemHeight = sizeDist(rng) * 0.8;

    double stemX = (topWidth - stemWidth) / 2;

    QPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(topWidth, 0);
    path.lineTo(topWidth, topHeight);
    path.lineTo(stemX + stemWidth, topHeight);
    path.lineTo(stemX + stemWidth, topHeight + stemHeight);
    path.lineTo(stemX, topHeight + stemHeight);
    path.lineTo(stemX, topHeight);
    path.lineTo(0, topHeight);
    path.closeSubpath();

    return path;
}

// Generate cross shape
QPainterPath RandomShapeGenerator::generateCross(
    double minSize, double maxSize,
    std::mt19937& rng)
{
    std::uniform_real_distribution<> sizeDist(minSize, maxSize);

    double totalSize = sizeDist(rng);
    double armWidth = totalSize * 0.35;
    double centerOffset = (totalSize - armWidth) / 2;

    QPainterPath path;

    // Top arm
    path.moveTo(centerOffset, 0);
    path.lineTo(centerOffset + armWidth, 0);
    path.lineTo(centerOffset + armWidth, centerOffset);

    // Right arm
    path.lineTo(totalSize, centerOffset);
    path.lineTo(totalSize, centerOffset + armWidth);
    path.lineTo(centerOffset + armWidth, centerOffset + armWidth);

    // Bottom arm
    path.lineTo(centerOffset + armWidth, totalSize);
    path.lineTo(centerOffset, totalSize);
    path.lineTo(centerOffset, centerOffset + armWidth);

    // Left arm
    path.lineTo(0, centerOffset + armWidth);
    path.lineTo(0, centerOffset);
    path.lineTo(centerOffset, centerOffset);

    path.closeSubpath();

    return path;
}

// Generate star
QPainterPath RandomShapeGenerator::generateStar(
    int points,
    double minRadius, double maxRadius,
    double innerRadiusRatio,
    std::mt19937& rng)
{
    if (points < 3) points = 3;

    std::uniform_real_distribution<> radiusDist(minRadius, maxRadius);
    double outerRadius = radiusDist(rng);
    double innerRadius = outerRadius * innerRadiusRatio;

    QPainterPath path;

    for (int i = 0; i < points * 2; ++i) {
        double angle = M_PI * i / points;
        double radius = (i % 2 == 0) ? outerRadius : innerRadius;

        double x = radius * std::cos(angle - M_PI / 2) + outerRadius;
        double y = radius * std::sin(angle - M_PI / 2) + outerRadius;

        if (i == 0) {
            path.moveTo(x, y);
        } else {
            path.lineTo(x, y);
        }
    }

    path.closeSubpath();
    return path;
}

// Generate random convex polygon
QPainterPath RandomShapeGenerator::generateRandomConvexPolygon(
    int numPoints,
    double minRadius, double maxRadius,
    std::mt19937& rng)
{
    if (numPoints < 3) numPoints = 3;

    std::uniform_real_distribution<> radiusDist(minRadius, maxRadius);
    std::uniform_real_distribution<> angleDist(0.0, 2.0 * M_PI);

    // Generate random points
    std::vector<QPointF> points;
    double centerRadius = (minRadius + maxRadius) / 2;

    for (int i = 0; i < numPoints; ++i) {
        double angle = angleDist(rng);
        double radius = radiusDist(rng);
        double x = radius * std::cos(angle) + centerRadius;
        double y = radius * std::sin(angle) + centerRadius;
        points.push_back(QPointF(x, y));
    }

    // Compute convex hull
    std::vector<QPointF> hull = computeConvexHull(points);

    // Create path from hull
    QPainterPath path;
    if (!hull.empty()) {
        path.moveTo(hull[0]);
        for (size_t i = 1; i < hull.size(); ++i) {
            path.lineTo(hull[i]);
        }
        path.closeSubpath();
    }

    return path;
}

// Generate shape of specific type
QPainterPath RandomShapeGenerator::generateShape(
    ShapeType type,
    const GeneratorConfig& config,
    std::mt19937& rng)
{
    switch (type) {
        case RECTANGLE:
            return generateRandomRectangle(config.minWidth, config.maxWidth,
                                         config.minHeight, config.maxHeight, rng);

        case SQUARE:
            return generateRandomSquare(std::min(config.minWidth, config.minHeight),
                                       std::min(config.maxWidth, config.maxHeight), rng);

        case POLYGON: {
            std::uniform_int_distribution<> sidesDist(config.minSides, config.maxSides);
            int sides = sidesDist(rng);
            double minR = std::min(config.minWidth, config.minHeight) / 2;
            double maxR = std::min(config.maxWidth, config.maxHeight) / 2;
            return generateIrregularPolygon(sides, minR, maxR, 0.3, rng);
        }

        case CIRCLE: {
            double minR = std::min(config.minWidth, config.minHeight) / 2;
            double maxR = std::min(config.maxWidth, config.maxHeight) / 2;
            return generateRandomCircle(minR, maxR, rng);
        }

        case L_SHAPE:
            return generateLShape(std::min(config.minWidth, config.minHeight),
                                std::min(config.maxWidth, config.maxHeight), rng);

        case T_SHAPE:
            return generateTShape(std::min(config.minWidth, config.minHeight),
                                std::min(config.maxWidth, config.maxHeight), rng);

        case CROSS:
            return generateCross(std::min(config.minWidth, config.minHeight),
                               std::min(config.maxWidth, config.maxHeight), rng);

        case STAR: {
            std::uniform_int_distribution<> pointsDist(5, 8);
            int points = pointsDist(rng);
            double minR = std::min(config.minWidth, config.minHeight) / 2;
            double maxR = std::min(config.maxWidth, config.maxHeight) / 2;
            return generateStar(points, minR, maxR, 0.4, rng);
        }

        case RANDOM_CONVEX: {
            std::uniform_int_distribution<> pointsDist(config.minSides, config.maxSides);
            int points = pointsDist(rng);
            double minR = std::min(config.minWidth, config.minHeight) / 2;
            double maxR = std::min(config.maxWidth, config.maxHeight) / 2;
            return generateRandomConvexPolygon(points, minR, maxR, rng);
        }

        case RANDOM_ANY: {
            std::uniform_int_distribution<> typeDist(0, 8);
            ShapeType randomType = static_cast<ShapeType>(typeDist(rng));
            return generateShape(randomType, config, rng);
        }

        default:
            return generateRandomRectangle(config.minWidth, config.maxWidth,
                                         config.minHeight, config.maxHeight, rng);
    }
}

// Generate test set
std::vector<QPainterPath> RandomShapeGenerator::generateTestSet(
    int numShapes,
    double containerWidth,
    double containerHeight,
    const GeneratorConfig& config)
{
    std::mt19937 rng = createRNG(config.seed);
    std::vector<QPainterPath> shapes;
    shapes.reserve(numShapes);

    // Adjust size based on container
    GeneratorConfig adjustedConfig = config;
    if (adjustedConfig.maxWidth > containerWidth * 0.4) {
        adjustedConfig.maxWidth = containerWidth * 0.4;
    }
    if (adjustedConfig.maxHeight > containerHeight * 0.4) {
        adjustedConfig.maxHeight = containerHeight * 0.4;
    }

    for (int i = 0; i < numShapes; ++i) {
        QPainterPath shape = generateShape(RANDOM_ANY, adjustedConfig, rng);
        shapes.push_back(shape);
    }

    return shapes;
}

// Generate mixed test set
std::vector<QPainterPath> RandomShapeGenerator::generateMixedTestSet(
    int rectangles,
    int polygons,
    int lShapes,
    int tShapes,
    const GeneratorConfig& config)
{
    std::mt19937 rng = createRNG(config.seed);
    std::vector<QPainterPath> shapes;
    shapes.reserve(rectangles + polygons + lShapes + tShapes);

    // Generate rectangles
    for (int i = 0; i < rectangles; ++i) {
        shapes.push_back(generateShape(RECTANGLE, config, rng));
    }

    // Generate polygons
    for (int i = 0; i < polygons; ++i) {
        shapes.push_back(generateShape(POLYGON, config, rng));
    }

    // Generate L-shapes
    for (int i = 0; i < lShapes; ++i) {
        shapes.push_back(generateShape(L_SHAPE, config, rng));
    }

    // Generate T-shapes
    for (int i = 0; i < tShapes; ++i) {
        shapes.push_back(generateShape(T_SHAPE, config, rng));
    }

    return shapes;
}

// Generate container
QPainterPath RandomShapeGenerator::generateContainer(double width, double height)
{
    QPainterPath path;
    path.addRect(0, 0, width, height);
    return path;
}

// Generate container for shapes
QPainterPath RandomShapeGenerator::generateContainerForShapes(
    const std::vector<QPainterPath>& shapes,
    double margin)
{
    if (shapes.empty()) {
        return generateContainer(500, 400);  // Default size
    }

    // Calculate total area
    double totalArea = 0;
    for (const auto& shape : shapes) {
        QRectF bounds = shape.boundingRect();
        totalArea += bounds.width() * bounds.height();
    }

    // Estimate container size (assume 60% efficiency)
    double containerArea = totalArea / 0.6;
    double containerWidth = std::sqrt(containerArea * 1.5) + margin * 2;  // 3:2 aspect ratio
    double containerHeight = std::sqrt(containerArea / 1.5) + margin * 2;

    return generateContainer(containerWidth, containerHeight);
}

// Private helper: Create RNG
std::mt19937 RandomShapeGenerator::createRNG(unsigned int seed)
{
    if (seed == 0) {
        std::random_device rd;
        return std::mt19937(rd());
    }
    return std::mt19937(seed);
}

// Private helper: Compute convex hull (Graham scan)
std::vector<QPointF> RandomShapeGenerator::computeConvexHull(const std::vector<QPointF>& points)
{
    if (points.size() < 3) {
        return points;
    }

    // Find point with lowest y-coordinate (and leftmost if tie)
    auto lowestPoint = *std::min_element(points.begin(), points.end(),
        [](const QPointF& a, const QPointF& b) {
            return (a.y() < b.y()) || (a.y() == b.y() && a.x() < b.x());
        });

    // Sort points by polar angle with respect to lowest point
    std::vector<QPointF> sortedPoints = points;
    std::sort(sortedPoints.begin(), sortedPoints.end(),
        [&lowestPoint](const QPointF& a, const QPointF& b) {
            if (a == lowestPoint) return true;
            if (b == lowestPoint) return false;

            double dx1 = a.x() - lowestPoint.x();
            double dy1 = a.y() - lowestPoint.y();
            double dx2 = b.x() - lowestPoint.x();
            double dy2 = b.y() - lowestPoint.y();

            double cross = dx1 * dy2 - dy1 * dx2;
            if (cross != 0) return cross > 0;

            // If collinear, sort by distance
            return (dx1 * dx1 + dy1 * dy1) < (dx2 * dx2 + dy2 * dy2);
        });

    // Graham scan
    std::vector<QPointF> hull;
    for (const auto& point : sortedPoints) {
        while (hull.size() >= 2) {
            const QPointF& p1 = hull[hull.size() - 2];
            const QPointF& p2 = hull[hull.size() - 1];

            double cross = (p2.x() - p1.x()) * (point.y() - p1.y()) -
                          (p2.y() - p1.y()) * (point.x() - p1.x());

            if (cross <= 0) {
                hull.pop_back();
            } else {
                break;
            }
        }
        hull.push_back(point);
    }

    return hull;
}
