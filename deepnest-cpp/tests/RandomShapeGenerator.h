#ifndef RANDOM_SHAPE_GENERATOR_H
#define RANDOM_SHAPE_GENERATOR_H

#include <QPainterPath>
#include <QPointF>
#include <vector>
#include <random>

/**
 * @brief Random shape generator for testing nesting algorithms
 *
 * This class provides utilities for generating random geometric shapes
 * for testing purposes. It can create rectangles, polygons, circles,
 * L-shapes, T-shapes, and other complex forms.
 *
 * All shapes are generated with proper CCW winding order for compatibility
 * with the nesting engine.
 */
class RandomShapeGenerator {
public:
    /**
     * @brief Shape type enumeration
     */
    enum ShapeType {
        RECTANGLE,      ///< Simple rectangle
        SQUARE,         ///< Square (equal width and height)
        POLYGON,        ///< Regular or irregular polygon
        CIRCLE,         ///< Circle/ellipse
        L_SHAPE,        ///< L-shaped polygon
        T_SHAPE,        ///< T-shaped polygon
        CROSS,          ///< Cross/plus shape
        STAR,           ///< Star polygon
        RANDOM_CONVEX,  ///< Random convex polygon
        RANDOM_ANY      ///< Random shape of any type
    };

    /**
     * @brief Configuration for shape generation
     */
    struct GeneratorConfig {
        double minWidth;        ///< Minimum shape width
        double maxWidth;        ///< Maximum shape width
        double minHeight;       ///< Minimum shape height
        double maxHeight;       ///< Maximum shape height
        int minSides;           ///< Minimum sides for polygons (default: 5)
        int maxSides;           ///< Maximum sides for polygons (default: 8)
        bool allowHoles;        ///< Allow holes in shapes (default: false)
        unsigned int seed;      ///< Random seed (0 = random)

        /**
         * @brief Default constructor with reasonable defaults
         */
        GeneratorConfig()
            : minWidth(30.0)
            , maxWidth(150.0)
            , minHeight(30.0)
            , maxHeight(150.0)
            , minSides(5)
            , maxSides(8)
            , allowHoles(false)
            , seed(0)
        {}
    };

    /**
     * @brief Generate a random rectangle
     *
     * @param minWidth Minimum width
     * @param maxWidth Maximum width
     * @param minHeight Minimum height
     * @param maxHeight Maximum height
     * @param rng Random number generator
     * @return QPainterPath representing the rectangle
     */
    static QPainterPath generateRandomRectangle(
        double minWidth, double maxWidth,
        double minHeight, double maxHeight,
        std::mt19937& rng
    );

    /**
     * @brief Generate a random square
     *
     * @param minSize Minimum side length
     * @param maxSize Maximum side length
     * @param rng Random number generator
     * @return QPainterPath representing the square
     */
    static QPainterPath generateRandomSquare(
        double minSize, double maxSize,
        std::mt19937& rng
    );

    /**
     * @brief Generate a random regular polygon
     *
     * @param sides Number of sides (must be >= 3)
     * @param minRadius Minimum radius
     * @param maxRadius Maximum radius
     * @param rng Random number generator
     * @return QPainterPath representing the polygon
     */
    static QPainterPath generateRegularPolygon(
        int sides,
        double minRadius, double maxRadius,
        std::mt19937& rng
    );

    /**
     * @brief Generate a random irregular polygon
     *
     * @param sides Number of sides
     * @param minRadius Minimum radius
     * @param maxRadius Maximum radius
     * @param irregularity How irregular (0.0 = regular, 1.0 = very irregular)
     * @param rng Random number generator
     * @return QPainterPath representing the polygon
     */
    static QPainterPath generateIrregularPolygon(
        int sides,
        double minRadius, double maxRadius,
        double irregularity,
        std::mt19937& rng
    );

    /**
     * @brief Generate a random circle
     *
     * @param minRadius Minimum radius
     * @param maxRadius Maximum radius
     * @param rng Random number generator
     * @return QPainterPath representing the circle
     */
    static QPainterPath generateRandomCircle(
        double minRadius, double maxRadius,
        std::mt19937& rng
    );

    /**
     * @brief Generate an L-shaped polygon
     *
     * @param minSize Minimum dimension
     * @param maxSize Maximum dimension
     * @param rng Random number generator
     * @return QPainterPath representing the L-shape
     */
    static QPainterPath generateLShape(
        double minSize, double maxSize,
        std::mt19937& rng
    );

    /**
     * @brief Generate a T-shaped polygon
     *
     * @param minSize Minimum dimension
     * @param maxSize Maximum dimension
     * @param rng Random number generator
     * @return QPainterPath representing the T-shape
     */
    static QPainterPath generateTShape(
        double minSize, double maxSize,
        std::mt19937& rng
    );

    /**
     * @brief Generate a cross/plus shaped polygon
     *
     * @param minSize Minimum dimension
     * @param maxSize Maximum dimension
     * @param rng Random number generator
     * @return QPainterPath representing the cross
     */
    static QPainterPath generateCross(
        double minSize, double maxSize,
        std::mt19937& rng
    );

    /**
     * @brief Generate a star polygon
     *
     * @param points Number of star points (must be >= 3)
     * @param minRadius Minimum outer radius
     * @param maxRadius Maximum outer radius
     * @param innerRadiusRatio Ratio of inner to outer radius (0.0-1.0)
     * @param rng Random number generator
     * @return QPainterPath representing the star
     */
    static QPainterPath generateStar(
        int points,
        double minRadius, double maxRadius,
        double innerRadiusRatio,
        std::mt19937& rng
    );

    /**
     * @brief Generate a random convex polygon
     *
     * Uses gift wrapping to ensure convexity.
     *
     * @param numPoints Number of vertices
     * @param minRadius Minimum radius
     * @param maxRadius Maximum radius
     * @param rng Random number generator
     * @return QPainterPath representing the convex polygon
     */
    static QPainterPath generateRandomConvexPolygon(
        int numPoints,
        double minRadius, double maxRadius,
        std::mt19937& rng
    );

    /**
     * @brief Generate a random shape of specified type
     *
     * @param type Shape type to generate
     * @param config Generation configuration
     * @param rng Random number generator
     * @return QPainterPath representing the shape
     */
    static QPainterPath generateShape(
        ShapeType type,
        const GeneratorConfig& config,
        std::mt19937& rng
    );

    /**
     * @brief Generate a test set of random shapes
     *
     * @param numShapes Number of shapes to generate
     * @param containerWidth Container width (for sizing reference)
     * @param containerHeight Container height (for sizing reference)
     * @param config Generation configuration
     * @return Vector of QPainterPath shapes
     */
    static std::vector<QPainterPath> generateTestSet(
        int numShapes,
        double containerWidth,
        double containerHeight,
        const GeneratorConfig& config = GeneratorConfig()
    );

    /**
     * @brief Generate a test set with specific shape distribution
     *
     * @param rectangles Number of rectangles
     * @param polygons Number of polygons
     * @param lShapes Number of L-shapes
     * @param tShapes Number of T-shapes
     * @param config Generation configuration
     * @return Vector of QPainterPath shapes
     */
    static std::vector<QPainterPath> generateMixedTestSet(
        int rectangles,
        int polygons,
        int lShapes,
        int tShapes,
        const GeneratorConfig& config = GeneratorConfig()
    );

    /**
     * @brief Generate a container/sheet rectangle
     *
     * @param width Container width
     * @param height Container height
     * @return QPainterPath representing the container
     */
    static QPainterPath generateContainer(double width, double height);

    /**
     * @brief Generate a container with margin
     *
     * Creates a container slightly larger than the bounding box of provided shapes.
     *
     * @param shapes Shapes that will be nested
     * @param margin Extra margin around shapes
     * @return QPainterPath representing the container
     */
    static QPainterPath generateContainerForShapes(
        const std::vector<QPainterPath>& shapes,
        double margin = 50.0
    );

private:
    /**
     * @brief Create random number generator with seed
     *
     * @param seed Seed value (0 = random)
     * @return Random number generator
     */
    static std::mt19937 createRNG(unsigned int seed);

    /**
     * @brief Compute convex hull of points (for convex polygon generation)
     *
     * @param points Input points
     * @return Points forming convex hull in CCW order
     */
    static std::vector<QPointF> computeConvexHull(const std::vector<QPointF>& points);
};

#endif // RANDOM_SHAPE_GENERATOR_H
