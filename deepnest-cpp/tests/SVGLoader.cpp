#include "../include/deepnest/core/Types.h"
#include "SVGLoader.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QDebug>
#include <QRectF>
#include <cmath>
#include "../include/deepnest/geometry/GeometryUtil.h"
#include "../include/deepnest/geometry/PolygonHierarchy.h"

// Load complete SVG file
SVGLoader::LoadResult SVGLoader::loadFile(const QString& svgPath, const Config& config) {
    LoadResult result;

    QFile file(svgPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = QString("Cannot open file: %1").arg(svgPath);
        return result;
    }

    QXmlStreamReader xml(&file);
    QTransform currentTransform;
    QVector<QTransform> transformStack;
    
    // Global scaling factor
    double globalScale = 1.0;
    bool scaleCalculated = false;

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement()) {
            QString elementName = xml.name().toString().toLower();
            QXmlStreamAttributes attrs = xml.attributes();
            
            // Handle root SVG element for scaling
            if (elementName == "svg" && !scaleCalculated) {
                QString widthStr = attrs.value("width").toString();
                QString viewBoxStr = attrs.value("viewBox").toString();
                
                // Calculate scale based on units
                globalScale = getScalingFactor(widthStr, viewBoxStr, config.scale);
                scaleCalculated = true;
                
                // Apply global scale to root transform
                currentTransform.scale(globalScale, globalScale);
            }

            // Get transform if present
            QTransform elementTransform = currentTransform;
            if (attrs.hasAttribute("transform")) {
                QString transformStr = attrs.value("transform").toString();
                QTransform localTransform = parseTransform(transformStr);
                elementTransform = currentTransform * localTransform;
            }

            // Push transform onto stack for nested elements
            if (elementName == "g" || elementName == "svg") {
                transformStack.push_back(currentTransform);
                currentTransform = elementTransform;
                continue;
            }

            // Parse shape elements
            Shape shape;
            shape.transform = elementTransform;
            shape.elementType = elementName;

            if (attrs.hasAttribute("id")) {
                shape.id = attrs.value("id").toString();
            }

            QString className;
            if (attrs.hasAttribute("class")) {
                className = attrs.value("class").toString();
            }

            shape.isContainer = isContainerElement(shape.id, className);

            bool shapeFound = false;

            if (elementName == "path") {
                if (attrs.hasAttribute("d")) {
                    QString pathData = attrs.value("d").toString();
                    shape.path = parsePathData(pathData);
                    shapeFound = true;
                }
            }
            else if (elementName == "rect") {
                double x = attrs.hasAttribute("x") ? parseUnit(attrs.value("x").toString()) : 0;
                double y = attrs.hasAttribute("y") ? parseUnit(attrs.value("y").toString()) : 0;
                double width = parseUnit(attrs.value("width").toString());
                double height = parseUnit(attrs.value("height").toString());
                double rx = attrs.hasAttribute("rx") ? parseUnit(attrs.value("rx").toString()) : 0;
                double ry = attrs.hasAttribute("ry") ? parseUnit(attrs.value("ry").toString()) : 0;

                shape.path = rectToPath(x, y, width, height, rx, ry);
                shapeFound = true;
            }
            else if (elementName == "circle") {
                double cx = parseUnit(attrs.value("cx").toString());
                double cy = parseUnit(attrs.value("cy").toString());
                double r = parseUnit(attrs.value("r").toString());

                shape.path = circleToPath(cx, cy, r);
                shapeFound = true;
            }
            else if (elementName == "ellipse") {
                double cx = parseUnit(attrs.value("cx").toString());
                double cy = parseUnit(attrs.value("cy").toString());
                double rx = parseUnit(attrs.value("rx").toString());
                double ry = parseUnit(attrs.value("ry").toString());

                shape.path = ellipseToPath(cx, cy, rx, ry);
                shapeFound = true;
            }
            else if (elementName == "polygon") {
                if (attrs.hasAttribute("points")) {
                    QString points = attrs.value("points").toString();
                    shape.path = polygonToPath(points);
                    shapeFound = true;
                }
            }
            else if (elementName == "polyline") {
                if (attrs.hasAttribute("points")) {
                    QString points = attrs.value("points").toString();
                    shape.path = polylineToPath(points);
                    shapeFound = true;
                }
            }
            else if (elementName == "line") {
                 double x1 = parseUnit(attrs.value("x1").toString());
                 double y1 = parseUnit(attrs.value("y1").toString());
                 double x2 = parseUnit(attrs.value("x2").toString());
                 double y2 = parseUnit(attrs.value("y2").toString());
                 
                 QPainterPath linePath;
                 linePath.moveTo(x1, y1);
                 linePath.lineTo(x2, y2);
                 shape.path = linePath;
                 shapeFound = true;
            }

            if (shapeFound) {
                // Split paths with multiple subpaths into separate shapes
                // This matches the JavaScript svgparser.js splitPath behavior
                QList<QPolygonF> subpaths = shape.path.toSubpathPolygons();
                
                if (subpaths.size() > 1) {
                    // Multiple subpaths - use PolygonHierarchy to identify parent-child relationships
                    // Convert subpaths to Polygons
                    std::vector<deepnest::Polygon> polygons;
                    polygons.reserve(subpaths.size());
                    
                    for (int i = 0; i < subpaths.size(); ++i) {
                        if (subpaths[i].size() < 3) continue;
                        
                        deepnest::Polygon poly;
                        poly.id = i;
                        poly.points.reserve(subpaths[i].size());
                        
                        for (const QPointF& qpt : subpaths[i]) {
                            poly.points.push_back(deepnest::Point::fromQt(qpt, config.inputScale));
                        }
                        
                        if (poly.isValid()) {
                            polygons.push_back(poly);
                        }
                    }
                    
                    // Build hierarchy - this identifies holes and associates them with parents
                    polygons = deepnest::PolygonHierarchy::buildTree(polygons, 0);
                    
                    // Convert back to Shapes
                    for (const auto& poly : polygons) {
                        Shape subShape = shape; // Copy transform, id, etc.
                        
                        // Convert polygon to QPainterPath
                        subShape.path = poly.toQPainterPath();
                        
                        // Update ID to reflect polygon index
                        if (!subShape.id.isEmpty()) {
                            subShape.id = QString("%1_%2").arg(subShape.id).arg(poly.id);
                        }
                        
                        if (subShape.isContainer && !result.container) {
                            result.container = std::make_unique<Shape>(subShape);
                        } else {
                            result.shapes.push_back(subShape);
                        }
                    }
                } else {
                    // Single subpath or empty - add as-is
                    if (shape.isContainer && !result.container) {
                        result.container = std::make_unique<Shape>(shape);
                    } else {
                        result.shapes.push_back(shape);
                    }
                }
            }
        }
        else if (xml.isEndElement()) {
            QString elementName = xml.name().toString().toLower();
            if ((elementName == "g" || elementName == "svg") && !transformStack.isEmpty()) {
                currentTransform = transformStack.back();
                transformStack.pop_back();
            }
        }
    }

    if (xml.hasError()) {
        result.errorMessage = QString("XML parse error: %1").arg(xml.errorString());
    }
    
    // Perform path merging
    if (result.success()) {
        mergeLines(result.shapes, config.endpointTolerance);
        // Also try to close paths with wider tolerance if needed, as in JS
        // this.mergeLines(this.svgRoot, 3*this.conf.endpointTolerance);
        mergeLines(result.shapes, 3.0 * config.endpointTolerance);
    }

    return result;
}

std::vector<SVGLoader::Shape> SVGLoader::loadShapes(const QString& svgPath) {
    LoadResult result = loadFile(svgPath);
    return result.shapes;
}

QPainterPath SVGLoader::loadContainer(const QString& svgPath) {
    LoadResult result = loadFile(svgPath);

    if (result.container) {
        QPainterPath path = result.container->path;
        if (!result.container->transform.isIdentity()) {
            path = result.container->transform.map(path);
        }
        return path;
    }

    // If no explicit container, find the largest shape
    if (!result.shapes.empty()) {
        double maxArea = 0;
        const Shape* largest = nullptr;

        for (const auto& shape : result.shapes) {
            QRectF bounds = shape.path.boundingRect();
            double area = bounds.width() * bounds.height();
            if (area > maxArea) {
                maxArea = area;
                largest = &shape;
            }
        }

        if (largest) {
            QPainterPath path = largest->path;
            if (!largest->transform.isIdentity()) {
                path = largest->transform.map(path);
            }
            return path;
        }
    }

    return QPainterPath();
}

QPainterPath SVGLoader::parsePathData(const QString& pathData) {
    QPainterPath path;

    int pos = 0;
    int len = pathData.length();

    QPointF currentPoint(0, 0);
    QPointF startPoint(0, 0);
    QPointF controlPoint(0, 0);  // For smooth curve commands
    char lastCommand = ' ';

    while (pos < len) {
        skipWhitespace(pathData, pos);
        if (pos >= len) break;

        char command = pathData[pos].toLatin1();

        // If not a command letter, reuse last command
        if (!((command >= 'A' && command <= 'Z') || (command >= 'a' && command <= 'z'))) {
            command = lastCommand;
        } else {
            pos++;
            lastCommand = command;
        }

        bool relative = (command >= 'a' && command <= 'z');
        command = toupper(command);

        skipWhitespace(pathData, pos);

        double x, y, x1, y1, x2, y2;

        switch (command) {
            case 'M':  // MoveTo
                if (parseCoordinate(pathData, pos, x, y)) {
                    if (relative) {
                        x += currentPoint.x();
                        y += currentPoint.y();
                    }
                    path.moveTo(x, y);
                    currentPoint = QPointF(x, y);
                    startPoint = currentPoint;
                    lastCommand = relative ? 'l' : 'L';  // Subsequent coordinates are lineTo
                }
                break;

            case 'L':  // LineTo
                if (parseCoordinate(pathData, pos, x, y)) {
                    if (relative) {
                        x += currentPoint.x();
                        y += currentPoint.y();
                    }
                    path.lineTo(x, y);
                    currentPoint = QPointF(x, y);
                }
                break;

            case 'H':  // Horizontal line
                x = parseNumber(pathData, pos);
                if (relative) {
                    x += currentPoint.x();
                }
                path.lineTo(x, currentPoint.y());
                currentPoint.setX(x);
                break;

            case 'V':  // Vertical line
                y = parseNumber(pathData, pos);
                if (relative) {
                    y += currentPoint.y();
                }
                path.lineTo(currentPoint.x(), y);
                currentPoint.setY(y);
                break;

            case 'C':  // Cubic Bezier
                if (parseCoordinate(pathData, pos, x1, y1) &&
                    parseCoordinate(pathData, pos, x2, y2) &&
                    parseCoordinate(pathData, pos, x, y)) {

                    if (relative) {
                        x1 += currentPoint.x(); y1 += currentPoint.y();
                        x2 += currentPoint.x(); y2 += currentPoint.y();
                        x += currentPoint.x(); y += currentPoint.y();
                    }
                    path.cubicTo(x1, y1, x2, y2, x, y);
                    controlPoint = QPointF(x2, y2);
                    currentPoint = QPointF(x, y);
                }
                break;

            case 'S':  // Smooth cubic Bezier
                if (parseCoordinate(pathData, pos, x2, y2) &&
                    parseCoordinate(pathData, pos, x, y)) {

                    // Reflect previous control point
                    x1 = 2 * currentPoint.x() - controlPoint.x();
                    y1 = 2 * currentPoint.y() - controlPoint.y();

                    if (relative) {
                        x2 += currentPoint.x(); y2 += currentPoint.y();
                        x += currentPoint.x(); y += currentPoint.y();
                    }
                    path.cubicTo(x1, y1, x2, y2, x, y);
                    controlPoint = QPointF(x2, y2);
                    currentPoint = QPointF(x, y);
                }
                break;

            case 'Q':  // Quadratic Bezier
                if (parseCoordinate(pathData, pos, x1, y1) &&
                    parseCoordinate(pathData, pos, x, y)) {

                    if (relative) {
                        x1 += currentPoint.x(); y1 += currentPoint.y();
                        x += currentPoint.x(); y += currentPoint.y();
                    }
                    path.quadTo(x1, y1, x, y);
                    controlPoint = QPointF(x1, y1);
                    currentPoint = QPointF(x, y);
                }
                break;

            case 'T':  // Smooth quadratic Bezier
                if (parseCoordinate(pathData, pos, x, y)) {
                    // Reflect previous control point
                    x1 = 2 * currentPoint.x() - controlPoint.x();
                    y1 = 2 * currentPoint.y() - controlPoint.y();

                    if (relative) {
                        x += currentPoint.x(); y += currentPoint.y();
                    }
                    path.quadTo(x1, y1, x, y);
                    controlPoint = QPointF(x1, y1);
                    currentPoint = QPointF(x, y);
                }
                break;

            case 'A':  // Arc (simplified - converts to cubic beziers)
                // For simplicity, we'll approximate arcs with the current path position
                // Full arc implementation would be more complex
                {
                    double rx = parseNumber(pathData, pos);
                    double ry = parseNumber(pathData, pos);
                    double xAxisRotation = parseNumber(pathData, pos);
                    int largeArc = (int)parseNumber(pathData, pos);
                    int sweep = (int)parseNumber(pathData, pos);
                    
                    Q_UNUSED(rx);
                    Q_UNUSED(ry);
                    Q_UNUSED(xAxisRotation);
                    Q_UNUSED(largeArc);
                    Q_UNUSED(sweep);

                    if (parseCoordinate(pathData, pos, x, y)) {
                        if (relative) {
                            x += currentPoint.x();
                            y += currentPoint.y();
                        }

                        // Simple approximation: just draw a line
                        // A full implementation would convert to cubic beziers
                        path.lineTo(x, y);
                        currentPoint = QPointF(x, y);
                    }
                }
                break;

            case 'Z':  // ClosePath
                path.closeSubpath();
                currentPoint = startPoint;
                break;

            default:
                // Unknown command, skip
                break;
        }

        skipWhitespace(pathData, pos);
    }

    return path;
}

QTransform SVGLoader::parseTransform(const QString& transformStr) {
    QTransform transform;

    QString str = transformStr.trimmed();
    int pos = 0;
    int len = str.length();

    while (pos < len) {
        skipWhitespace(str, pos);
        if (pos >= len) break;

        // Find transform function name
        int nameStart = pos;
        while (pos < len && str[pos].isLetter()) {
            pos++;
        }

        QString funcName = str.mid(nameStart, pos - nameStart);
        skipWhitespace(str, pos);

        if (pos >= len || str[pos] != '(') {
            break;  // Invalid syntax
        }
        pos++;  // Skip '('

        // Parse parameters
        QVector<double> params;
        while (pos < len && str[pos] != ')') {
            skipWhitespace(str, pos);
            if (str[pos] == ')') break;

            double value = parseNumber(str, pos);
            params.append(value);

            skipWhitespace(str, pos);
            if (pos < len && str[pos] == ',') {
                pos++;
            }
        }

        if (pos < len && str[pos] == ')') {
            pos++;
        }

        // Apply transform
        if (funcName == "translate") {
            double tx = params.value(0, 0);
            double ty = params.value(1, 0);
            transform.translate(tx, ty);
        }
        else if (funcName == "rotate") {
            double angle = params.value(0, 0);
            double cx = params.value(1, 0);
            double cy = params.value(2, 0);

            if (params.size() > 1) {
                transform.translate(cx, cy);
                transform.rotate(angle);
                transform.translate(-cx, -cy);
            } else {
                transform.rotate(angle);
            }
        }
        else if (funcName == "scale") {
            double sx = params.value(0, 1);
            double sy = params.value(1, sx);
            transform.scale(sx, sy);
        }
        else if (funcName == "skewX") {
            double angle = params.value(0, 0);
            transform.shear(std::tan(angle * M_PI / 180.0), 0);
        }
        else if (funcName == "skewY") {
            double angle = params.value(0, 0);
            transform.shear(0, std::tan(angle * M_PI / 180.0));
        }
        else if (funcName == "matrix") {
            if (params.size() >= 6) {
                transform.setMatrix(params[0], params[1], 0,
                                  params[2], params[3], 0,
                                  params[4], params[5], 1);
            }
        }
    }

    return transform;
}

QPainterPath SVGLoader::rectToPath(double x, double y, double width, double height,
                                   double rx, double ry) {
    QPainterPath path;

    if (rx <= 0 && ry <= 0) {
        // Simple rectangle
        path.addRect(x, y, width, height);
    } else {
        // Rounded rectangle
        if (ry <= 0) ry = rx;
        if (rx <= 0) rx = ry;

        // Clamp radii
        rx = std::min(rx, width / 2);
        ry = std::min(ry, height / 2);

        path.addRoundedRect(QRectF(x, y, width, height), rx, ry);
    }

    return path;
}

QPainterPath SVGLoader::circleToPath(double cx, double cy, double r) {
    QPainterPath path;
    path.addEllipse(QPointF(cx, cy), r, r);
    return path;
}

QPainterPath SVGLoader::ellipseToPath(double cx, double cy, double rx, double ry) {
    QPainterPath path;
    path.addEllipse(QPointF(cx, cy), rx, ry);
    return path;
}

QPainterPath SVGLoader::polygonToPath(const QString& points) {
    QPainterPath path;
    QString str = points.trimmed();

    int pos = 0;
    bool first = true;
    double x, y;

    while (parseCoordinate(str, pos, x, y)) {
        if (first) {
            path.moveTo(x, y);
            first = false;
        } else {
            path.lineTo(x, y);
        }
    }

    path.closeSubpath();
    return path;
}

QPainterPath SVGLoader::polylineToPath(const QString& points) {
    QPainterPath path;
    QString str = points.trimmed();

    int pos = 0;
    bool first = true;
    double x, y;

    while (parseCoordinate(str, pos, x, y)) {
        if (first) {
            path.moveTo(x, y);
            first = false;
        } else {
            path.lineTo(x, y);
        }
    }

    // Note: polyline is NOT closed
    return path;
}

bool SVGLoader::isContainerElement(const QString& id, const QString& className) {
    QString idLower = id.toLower();
    QString classLower = className.toLower();

    QStringList containerKeywords = {"container", "sheet", "bin", "stock", "board", "panel"};

    for (const QString& keyword : containerKeywords) {
        if (idLower.contains(keyword) || classLower.contains(keyword)) {
            return true;
        }
    }

    return false;
}

// Helper functions

// Helper to parse units (mm, cm, in, pt, pc, px)
double SVGLoader::parseUnit(const QString& valueStr, double defaultVal) {
    if (valueStr.isEmpty()) return defaultVal;
    
    QString str = valueStr.trimmed();
    double val = 0.0;
    
    // Extract number part
    int endPos = 0;
    // Find where number ends
    bool foundDot = false;
    for (int i = 0; i < str.length(); ++i) {
        QChar c = str[i];
        if (c.isDigit() || c == '-' || c == '+') {
            endPos++;
        } else if (c == '.' && !foundDot) {
            foundDot = true;
            endPos++;
        } else {
            break;
        }
    }
    
    val = str.left(endPos).toDouble();
    QString unit = str.mid(endPos).toLower();
    
    // Standard SVG unit conversion to pixels (96 DPI usually)
    if (unit == "in") {
        return val * 96.0;
    } else if (unit == "mm") {
        return val * 3.7795; // 96 / 25.4
    } else if (unit == "cm") {
        return val * 37.795;
    } else if (unit == "pt") {
        return val * 1.3333; // 96 / 72
    } else if (unit == "pc") {
        return val * 16.0;   // 96 / 6
    }
    
    return val;
}

double SVGLoader::getScalingFactor(const QString& widthStr, const QString& viewBoxStr, double configScale) {
    if (widthStr.isEmpty() || viewBoxStr.isEmpty()) {
        return 1.0;
    }
    
    QStringList vbParts = viewBoxStr.split(QRegExp("[\\s,]+"), QString::SkipEmptyParts);
    if (vbParts.size() < 4) {
        return 1.0;
    }
    
    double vbWidth = vbParts[2].toDouble();
    if (vbWidth <= 0) return 1.0;
    
    // Parse width with units
    QString wStr = widthStr.trimmed();
    double widthVal = 0.0;
    QString unit;
    
    int endPos = 0;
    bool foundDot = false;
    for (int i = 0; i < wStr.length(); ++i) {
        QChar c = wStr[i];
        if (c.isDigit() || c == '-' || c == '+') {
            endPos++;
        } else if (c == '.' && !foundDot) {
            foundDot = true;
            endPos++;
        } else {
            break;
        }
    }
    
    widthVal = wStr.left(endPos).toDouble();
    unit = wStr.mid(endPos).toLower();
    
    if (widthVal <= 0) return 1.0;
    
    // Calculate local scale (pixels per unit of width)
    double localScale = 1.0;
    
    if (unit == "in") {
        localScale = vbWidth / widthVal; 
    } else if (unit == "mm") {
        localScale = (25.4 * vbWidth) / widthVal;
    } else if (unit == "cm") {
        localScale = (2.54 * vbWidth) / widthVal;
    } else if (unit == "pt") {
        localScale = (72.0 * vbWidth) / widthVal;
    } else if (unit == "pc") {
        localScale = (6.0 * vbWidth) / widthVal;
    } else if (unit == "px") {
        localScale = (96.0 * vbWidth) / widthVal;
    } else {
        return 1.0;
    }
    
    return configScale / localScale;
}

// Path merging implementation

SVGLoader::CoincidentResult SVGLoader::getCoincident(const Shape& shape, const std::vector<Shape*>& openPaths, double tolerance) {
    CoincidentResult result = {-1, false, false, false};
    
    QPointF start1 = shape.path.elementAt(0);
    QPointF end1 = shape.path.currentPosition();
    
    // Find index of shape in openPaths (pointer comparison)
    int selfIndex = -1;
    for (int i = 0; i < openPaths.size(); ++i) {
        if (openPaths[i] == &shape) {
            selfIndex = i;
            break;
        }
    }
    
    if (selfIndex == -1 || selfIndex == openPaths.size() - 1) {
        return result;
    }
    
    for (int i = selfIndex + 1; i < openPaths.size(); ++i) {
        Shape* other = openPaths[i];
        QPointF start2 = other->path.elementAt(0);
        QPointF end2 = other->path.currentPosition();
        
        // Check all 4 combinations
        // 1. Start1 == Start2 -> Reverse1, NoReverse2 (so End1 matches Start2)
        if (deepnest::GeometryUtil::almostEqualPoints(deepnest::Point(start1.x(), start1.y()), deepnest::Point(start2.x(), start2.y()), tolerance)) {
            return {i, true, false, true};
        }
        // 2. Start1 == End2 -> Reverse1, Reverse2 (so End1 matches End2 -> End1 matches Start2_rev)
        else if (deepnest::GeometryUtil::almostEqualPoints(deepnest::Point(start1.x(), start1.y()), deepnest::Point(end2.x(), end2.y()), tolerance)) {
            return {i, true, true, true};
        }
        // 3. End1 == End2 -> NoReverse1, Reverse2 (so End1 matches Start2_rev)
        else if (deepnest::GeometryUtil::almostEqualPoints(deepnest::Point(end1.x(), end1.y()), deepnest::Point(end2.x(), end2.y()), tolerance)) {
            return {i, false, true, true};
        }
        // 4. End1 == Start2 -> NoReverse1, NoReverse2 (Already matching)
        else if (deepnest::GeometryUtil::almostEqualPoints(deepnest::Point(end1.x(), end1.y()), deepnest::Point(start2.x(), start2.y()), tolerance)) {
            return {i, false, false, true};
        }
    }
    
    return result;
}

QPainterPath SVGLoader::reversePath(const QPainterPath& path) {
    return path.toReversed();
}

QPainterPath SVGLoader::mergeOpenPaths(const QPainterPath& a, const QPainterPath& b) {
    QPainterPath result = a;
    result.connectPath(b);
    return result;
}

void SVGLoader::mergeLines(std::vector<Shape>& shapes, double tolerance) {
    // Identify open paths
    std::vector<Shape*> openPaths;
    for (auto& shape : shapes) {
        if (!isClosed(shape, tolerance)) {
            openPaths.push_back(&shape);
        } else {
            // Ensure closed paths are explicitly closed
            if (shape.path.elementCount() > 0) {
                 // Check if last element is NOT a close command (implied by isClosed check logic)
                 // But QPainterPath doesn't easily show "Close" element.
                 // We can just closeSubpath() if it's geometrically closed but not logically.
                 // But QPainterPath handles this automatically mostly.
                 // Let's force closeSubpath if close
                 // shape.path.closeSubpath(); // This adds a line to start.
            }
        }
    }
    
    if (openPaths.empty()) return;
    
    // Merge loop
    // We iterate through openPaths. When we merge, we remove the merged-in path from the list.
    // Since we are using pointers to the vector 'shapes', we must be careful not to invalidate them.
    // 'shapes' vector is stable here (passed by ref).
    
    // We use a list/vector of active open paths.
    // When a path is merged into another, we remove it from consideration.
    // Actually, we can just mark it as "merged" or remove from openPaths vector.
    
    for (size_t i = 0; i < openPaths.size(); ++i) {
        Shape* p = openPaths[i];
        if (!p) continue; // Already merged
        
        while (true) {
            CoincidentResult c = getCoincident(*p, openPaths, tolerance);
            
            if (!c.found) break;
            
            Shape* other = openPaths[c.index];
            
            // Perform merges
            if (c.reverse1) {
                p->path = reversePath(p->path);
            }
            if (c.reverse2) {
                other->path = reversePath(other->path);
            }
            
            // Merge other into p
            p->path = mergeOpenPaths(p->path, other->path);
            
            // Remove 'other' from shapes and openPaths
            // We can't easily remove from 'shapes' without invalidating pointers if we resize.
            // Instead, we'll mark 'other' as invalid/empty and remove later, 
            // OR we just clear its path and ignore it.
            other->path = QPainterPath(); // Clear it
            openPaths[c.index] = nullptr; // Remove from openPaths
            
            // Check if p is now closed
            if (isClosed(*p, tolerance)) {
                p->path.closeSubpath();
                openPaths[i] = nullptr; // Remove p from openPaths
                break; // Done with p
            }
            
            // Continue trying to merge more into p
        }
    }
    
    // Remove empty shapes from original vector
    shapes.erase(std::remove_if(shapes.begin(), shapes.end(),
        [](const Shape& s) { return s.path.isEmpty(); }),
        shapes.end());
}

bool SVGLoader::isClosed(const Shape& shape, double tolerance) {
    if (shape.path.elementCount() < 2) return false;
    
    QPointF start = shape.path.elementAt(0);
    QPointF end = shape.path.currentPosition();
    
    double dist = std::sqrt(std::pow(start.x() - end.x(), 2) + std::pow(start.y() - end.y(), 2));
    return dist < tolerance;
}

double SVGLoader::parseNumber(const QString& str, int& pos) {
    skipWhitespace(str, pos);

    int start = pos;
    int len = str.length();

    // Handle sign
    if (pos < len && (str[pos] == '+' || str[pos] == '-')) {
        pos++;
    }

    // Parse integer part
    while (pos < len && str[pos].isDigit()) {
        pos++;
    }

    // Parse decimal part
    if (pos < len && str[pos] == '.') {
        pos++;
        while (pos < len && str[pos].isDigit()) {
            pos++;
        }
    }

    // Parse exponent
    if (pos < len && (str[pos] == 'e' || str[pos] == 'E')) {
        pos++;
        if (pos < len && (str[pos] == '+' || str[pos] == '-')) {
            pos++;
        }
        while (pos < len && str[pos].isDigit()) {
            pos++;
        }
    }

    if (pos > start) {
        return str.mid(start, pos - start).toDouble();
    }

    return 0.0;
}

void SVGLoader::skipWhitespace(const QString& str, int& pos) {
    int len = str.length();
    while (pos < len && (str[pos].isSpace() || str[pos] == ',')) {
        pos++;
    }
}

bool SVGLoader::parseCoordinate(const QString& str, int& pos, double& x, double& y) {
    skipWhitespace(str, pos);

    if (pos >= str.length()) {
        return false;
    }

    x = parseNumber(str, pos);
    skipWhitespace(str, pos);
    y = parseNumber(str, pos);

    return true;
}
