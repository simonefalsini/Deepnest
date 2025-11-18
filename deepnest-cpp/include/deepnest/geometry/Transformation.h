#ifndef DEEPNEST_TRANSFORMATION_H
#define DEEPNEST_TRANSFORMATION_H

#include "../core/Point.h"
#include <vector>
#include <array>

namespace deepnest {

/**
 * @brief 2D Affine Transformation matrix
 *
 * Represents a 2D affine transformation using a 2x3 matrix:
 * | a  c  e |
 * | b  d  f |
 * | 0  0  1 |
 *
 * Based on matrix.js from the original DeepNest
 */
class Transformation {
private:
    // Matrix elements: [a, b, c, d, e, f]
    std::array<double, 6> matrix_;

public:
    /**
     * @brief Construct identity transformation
     */
    Transformation();

    /**
     * @brief Construct transformation from matrix elements
     *
     * @param a Scale x
     * @param b Skew y
     * @param c Skew x
     * @param d Scale y
     * @param e Translate x
     * @param f Translate y
     */
    Transformation(double a, double b, double c, double d, double e, double f);

    /**
     * @brief Check if this is an identity transformation
     */
    bool isIdentity() const;

    /**
     * @brief Reset to identity transformation
     */
    void reset();

    /**
     * @brief Combine this transformation with another
     *
     * @param other Other transformation to combine
     * @return Reference to this transformation
     */
    Transformation& combine(const Transformation& other);

    /**
     * @brief Apply translation
     *
     * @param tx Translation in x direction
     * @param ty Translation in y direction
     * @return Reference to this transformation
     */
    Transformation& translate(double tx, double ty);

    /**
     * @brief Apply uniform scaling
     *
     * @param s Scale factor (applied to both x and y)
     * @return Reference to this transformation
     */
    Transformation& scale(double s);

    /**
     * @brief Apply non-uniform scaling
     *
     * @param sx Scale factor for x
     * @param sy Scale factor for y
     * @return Reference to this transformation
     */
    Transformation& scale(double sx, double sy);

    /**
     * @brief Apply rotation
     *
     * @param angleDegrees Rotation angle in degrees
     * @param cx Center of rotation x (default 0)
     * @param cy Center of rotation y (default 0)
     * @return Reference to this transformation
     */
    Transformation& rotate(double angleDegrees, double cx = 0.0, double cy = 0.0);

    /**
     * @brief Apply skew along x-axis
     *
     * @param angleDegrees Skew angle in degrees
     * @return Reference to this transformation
     */
    Transformation& skewX(double angleDegrees);

    /**
     * @brief Apply skew along y-axis
     *
     * @param angleDegrees Skew angle in degrees
     * @return Reference to this transformation
     */
    Transformation& skewY(double angleDegrees);

    /**
     * @brief Apply transformation to a point
     *
     * @param p Input point
     * @param isRelative If true, ignore translation component
     * @return Transformed point
     */
    Point apply(const Point& p, bool isRelative = false) const;

    /**
     * @brief Apply transformation to a point (x, y)
     *
     * @param x X coordinate
     * @param y Y coordinate
     * @param isRelative If true, ignore translation component
     * @return Transformed point
     */
    Point apply(double x, double y, bool isRelative = false) const;

    /**
     * @brief Apply transformation to multiple points
     *
     * @param points Input points
     * @param isRelative If true, ignore translation component
     * @return Transformed points
     */
    std::vector<Point> apply(const std::vector<Point>& points, bool isRelative = false) const;

    /**
     * @brief Get matrix elements as array
     *
     * @return Array of 6 elements [a, b, c, d, e, f]
     */
    const std::array<double, 6>& getMatrix() const {
        return matrix_;
    }

    /**
     * @brief Get matrix element by index
     *
     * @param index Index (0-5)
     * @return Matrix element value
     */
    double operator[](size_t index) const {
        return matrix_[index];
    }

    /**
     * @brief Create a rotation transformation
     *
     * Static factory method for rotation
     *
     * @param angleDegrees Rotation angle in degrees
     * @param cx Center of rotation x
     * @param cy Center of rotation y
     * @return Rotation transformation
     */
    static Transformation createRotation(double angleDegrees, double cx = 0.0, double cy = 0.0);

    /**
     * @brief Create a translation transformation
     *
     * @param tx Translation in x
     * @param ty Translation in y
     * @return Translation transformation
     */
    static Transformation createTranslation(double tx, double ty);

    /**
     * @brief Create a scaling transformation
     *
     * @param sx Scale in x
     * @param sy Scale in y
     * @return Scaling transformation
     */
    static Transformation createScaling(double sx, double sy);

private:
    /**
     * @brief Combine two matrices
     *
     * @param m1 First matrix
     * @param m2 Second matrix
     * @return Combined matrix
     */
    static std::array<double, 6> combineMatrices(
        const std::array<double, 6>& m1,
        const std::array<double, 6>& m2
    );
};

} // namespace deepnest

#endif // DEEPNEST_TRANSFORMATION_H
