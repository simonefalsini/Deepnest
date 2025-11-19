#include "../include/deepnest/core/Types.h"
#include "SVGLoader.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QDebug>
#include <QRectF>
#include <cmath>

// Load complete SVG file
SVGLoader::LoadResult SVGLoader::loadFile(const QString& svgPath) {
    LoadResult result;

    QFile file(svgPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = QString("Cannot open file: %1").arg(svgPath);
        return result;
    }

    QXmlStreamReader xml(&file);
    QTransform currentTransform;
    QVector<QTransform> transformStack;

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement()) {
            QString elementName = xml.name().toString().toLower();
            QXmlStreamAttributes attrs = xml.attributes();

            // Get transform if present
            QTransform elementTransform = currentTransform;
            if (attrs.hasAttribute("transform")) {
                QString transformStr = attrs.value("transform").toString();
                QTransform localTransform = parseTransform(transformStr);
                elementTransform = currentTransform * localTransform;
            }

            // Push transform onto stack for nested elements
            if (elementName == "g") {
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
                double x = attrs.hasAttribute("x") ? attrs.value("x").toDouble() : 0;
                double y = attrs.hasAttribute("y") ? attrs.value("y").toDouble() : 0;
                double width = attrs.value("width").toDouble();
                double height = attrs.value("height").toDouble();
                double rx = attrs.hasAttribute("rx") ? attrs.value("rx").toDouble() : 0;
                double ry = attrs.hasAttribute("ry") ? attrs.value("ry").toDouble() : 0;

                shape.path = rectToPath(x, y, width, height, rx, ry);
                shapeFound = true;
            }
            else if (elementName == "circle") {
                double cx = attrs.value("cx").toDouble();
                double cy = attrs.value("cy").toDouble();
                double r = attrs.value("r").toDouble();

                shape.path = circleToPath(cx, cy, r);
                shapeFound = true;
            }
            else if (elementName == "ellipse") {
                double cx = attrs.value("cx").toDouble();
                double cy = attrs.value("cy").toDouble();
                double rx = attrs.value("rx").toDouble();
                double ry = attrs.value("ry").toDouble();

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

            if (shapeFound) {
                if (shape.isContainer && !result.container) {
                    result.container = std::make_unique<Shape>(shape);
                } else {
                    result.shapes.push_back(shape);
                }
            }
        }
        else if (xml.isEndElement()) {
            QString elementName = xml.name().toString().toLower();
            if (elementName == "g" && !transformStack.isEmpty()) {
                currentTransform = transformStack.back();
                transformStack.pop_back();
            }
        }
    }

    if (xml.hasError()) {
        result.errorMessage = QString("XML parse error: %1").arg(xml.errorString());
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
