#ifndef SVG_LOADER_H
#define SVG_LOADER_H

#include <QString>
#include <QTransform>
#include <QPainterPath>
#include <vector>
#include <memory>

/**
 * @brief SVG loader for importing shapes from SVG files
 *
 * This class provides functionality to load and parse SVG files for use
 * in the nesting application. It extracts individual shapes (paths, rectangles,
 * circles, polygons, etc.) and converts them to QPainterPath objects.
 *
 * The loader is designed to be simple and focused on the needs of nesting,
 * extracting only the geometric shapes while ignoring styling information
 * except for transformations.
 *
 * Based on the JavaScript svgparser.js but simplified for testing purposes.
 */
class SVGLoader {
public:
    /**
     * @brief Shape extracted from SVG
     *
     * Represents a single shape (path, rect, circle, etc.) found in the SVG file.
     */
    struct Shape {
        /**
         * @brief Shape ID from SVG (if present)
         */
        QString id;

        /**
         * @brief Geometric path representation
         */
        QPainterPath path;

        /**
         * @brief Cumulative transformation matrix
         *
         * This includes all transformations from the shape's ancestors
         * and the shape itself.
         */
        QTransform transform;

        /**
         * @brief Original SVG element type (e.g., "path", "rect", "circle")
         */
        QString elementType;

        /**
         * @brief Whether this shape is marked as a container/sheet
         *
         * Set to true if the shape has a specific class or ID indicating
         * it should be used as a container rather than a part.
         */
        bool isContainer;

        /**
         * @brief Constructor
         */
        Shape()
            : isContainer(false)
        {}
    };

    /**
     * @brief Result of SVG loading operation
     */
    struct LoadResult {
        /**
         * @brief All shapes found in the SVG
         */
        std::vector<Shape> shapes;

        /**
         * @brief Container/sheet shape (if found)
         *
         * This is the first shape marked as container, or nullptr if none found.
         */
        std::unique_ptr<Shape> container;

        /**
         * @brief Error message (empty if successful)
         */
        QString errorMessage;

        /**
         * @brief Whether loading was successful
         */
        bool success() const {
            return errorMessage.isEmpty();
        }
    };

    /**
     * @brief Load all shapes from an SVG file
     *
     * Parses the SVG file and extracts all recognizable shapes (paths, rects,
     * circles, ellipses, polygons, polylines).
     *
     * @param svgPath Path to the SVG file
     * @return Vector of shapes found in the file
     *
     * @note If the file cannot be read or parsed, returns an empty vector.
     *       Check the returned LoadResult's success() method for errors.
     */
    /**
     * @brief Configuration for SVG loading
     */
    struct Config {
        double tolerance;          // Max bound for bezier->line segment conversion
        double toleranceSvg;       // Fudge factor for browser inaccuracy
        double scale;              // Base scale (default 72)
        double endpointTolerance;  // Tolerance for merging endpoints
        double inputScale;         // Scaling factor for converting physical coords to integers (default 10000)

        Config() : tolerance(2.0), toleranceSvg(0.01), scale(72.0), endpointTolerance(2.0), inputScale(10000.0) {}
    };

    /**
     * @brief Load all shapes from an SVG file
     *
     * @param svgPath Path to the SVG file
     * @param config Optional configuration
     * @return LoadResult containing extracted shapes and status
     */
    static LoadResult loadFile(const QString& svgPath, const Config& config = Config());

    /**
     * @brief Load shapes only (convenience wrapper)
     */
    static std::vector<Shape> loadShapes(const QString& svgPath);

    /**
     * @brief Load container only (convenience wrapper)
     */
    static QPainterPath loadContainer(const QString& svgPath);

public:
    /**
     * @brief Parse SVG path data string
     *
     * Converts an SVG path data string (d attribute) to a QPainterPath.
     * Supports most SVG path commands (M, L, H, V, C, S, Q, T, A, Z).
     *
     * @param pathData SVG path data string
     * @return QPainterPath representation
     */
    static QPainterPath parsePathData(const QString& pathData);

    /**
     * @brief Parse SVG transform attribute
     *
     * Parses an SVG transform attribute string and returns the corresponding
     * QTransform matrix.
     *
     * Supports: translate, rotate, scale, matrix, skewX, skewY
     *
     * @param transformStr SVG transform attribute string
     * @return QTransform matrix
     */
    static QTransform parseTransform(const QString& transformStr);

    /**
     * @brief Convert rectangle to QPainterPath
     *
     * @param x Rectangle x position
     * @param y Rectangle y position
     * @param width Rectangle width
     * @param height Rectangle height
     * @param rx Optional x-radius for rounded corners
     * @param ry Optional y-radius for rounded corners
     * @return QPainterPath representation
     */
    static QPainterPath rectToPath(double x, double y, double width, double height,
                                   double rx = 0, double ry = 0);

    /**
     * @brief Convert circle to QPainterPath
     *
     * @param cx Circle center x
     * @param cy Circle center y
     * @param r Circle radius
     * @return QPainterPath representation
     */
    static QPainterPath circleToPath(double cx, double cy, double r);

    /**
     * @brief Convert ellipse to QPainterPath
     *
     * @param cx Ellipse center x
     * @param cy Ellipse center y
     * @param rx X-radius
     * @param ry Y-radius
     * @return QPainterPath representation
     */
    static QPainterPath ellipseToPath(double cx, double cy, double rx, double ry);

    /**
     * @brief Convert polygon points to QPainterPath
     *
     * @param points Space or comma-separated list of point coordinates
     * @return QPainterPath representation
     */
    static QPainterPath polygonToPath(const QString& points);

    /**
     * @brief Convert polyline points to QPainterPath
     *
     * @param points Space or comma-separated list of point coordinates
     * @return QPainterPath representation (not closed)
     */
    static QPainterPath polylineToPath(const QString& points);

    /**
     * @brief Check if a shape is marked as container
     *
     * Checks the ID and class attributes for indicators like "container",
     * "sheet", "bin", "stock", etc.
     *
     * @param id Element ID
     * @param className Element class attribute
     * @return True if this should be treated as a container
     */
    static bool isContainerElement(const QString& id, const QString& className);

private:
    /**
     * @brief Parse number from string at given position
     *
     * Helper function for parsing SVG data strings.
     *
     * @param str String to parse
     * @param pos Current position (updated after parsing)
     * @return Parsed number
     */
    static double parseNumber(const QString& str, int& pos);

    // Unit handling
    static double parseUnit(const QString& valueStr, double defaultVal = 0.0);
    static double getScalingFactor(const QString& widthStr, const QString& viewBoxStr, double configScale);

    // Path merging helpers
    static void mergeLines(std::vector<Shape>& shapes, double tolerance);
    static bool isClosed(const Shape& shape, double tolerance);
    
    struct CoincidentResult {
        int index;
        bool reverse1;
        bool reverse2;
        bool found;
    };
    
    static CoincidentResult getCoincident(const Shape& shape, const std::vector<Shape*>& openPaths, double tolerance);
    static QPainterPath mergeOpenPaths(const QPainterPath& a, const QPainterPath& b);
    static QPainterPath reversePath(const QPainterPath& path);
    
    /**
     * @brief Skip whitespace and commas
     *
     * Helper function for parsing SVG data strings.
     *
     * @param str String to parse
     * @param pos Current position (updated after skipping)
     */
    static void skipWhitespace(const QString& str, int& pos);

    /**
     * @brief Parse coordinate pair
     *
     * @param str String to parse
     * @param pos Current position (updated after parsing)
     * @param x Output x coordinate
     * @param y Output y coordinate
     * @return True if successfully parsed
     */
    static bool parseCoordinate(const QString& str, int& pos, double& x, double& y);
};

#endif // SVG_LOADER_H
