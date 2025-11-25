#include "../../include/deepnest/geometry/Transformation.h"
#include <cmath>

namespace deepnest {

Transformation::Transformation() {
    // Identity matrix: [1, 0, 0, 1, 0, 0]
    reset();
}

Transformation::Transformation(double a, double b, double c, double d, double e, double f) {
    matrix_[0] = a;
    matrix_[1] = b;
    matrix_[2] = c;
    matrix_[3] = d;
    matrix_[4] = e;
    matrix_[5] = f;
}

void Transformation::reset() {
    matrix_[0] = 1.0;  // a (scale x)
    matrix_[1] = 0.0;  // b (skew y)
    matrix_[2] = 0.0;  // c (skew x)
    matrix_[3] = 1.0;  // d (scale y)
    matrix_[4] = 0.0;  // e (translate x)
    matrix_[5] = 0.0;  // f (translate y)
}

bool Transformation::isIdentity() const {
    return matrix_[0] == 1.0 &&
           matrix_[1] == 0.0 &&
           matrix_[2] == 0.0 &&
           matrix_[3] == 1.0 &&
           matrix_[4] == 0.0 &&
           matrix_[5] == 0.0;
}

std::array<double, 6> Transformation::combineMatrices(
    const std::array<double, 6>& m1,
    const std::array<double, 6>& m2) {

    return {
        m1[0] * m2[0] + m1[2] * m2[1],      // a
        m1[1] * m2[0] + m1[3] * m2[1],      // b
        m1[0] * m2[2] + m1[2] * m2[3],      // c
        m1[1] * m2[2] + m1[3] * m2[3],      // d
        m1[0] * m2[4] + m1[2] * m2[5] + m1[4],  // e
        m1[1] * m2[4] + m1[3] * m2[5] + m1[5]   // f
    };
}

Transformation& Transformation::combine(const Transformation& other) {
    matrix_ = combineMatrices(matrix_, other.matrix_);
    return *this;
}

Transformation& Transformation::translate(double tx, double ty) {
    if (tx != 0.0 || ty != 0.0) {
        std::array<double, 6> translateMatrix = {1.0, 0.0, 0.0, 1.0, tx, ty};
        matrix_ = combineMatrices(matrix_, translateMatrix);
    }
    return *this;
}

Transformation& Transformation::scale(double s) {
    return scale(s, s);
}

Transformation& Transformation::scale(double sx, double sy) {
    if (sx != 1.0 || sy != 1.0) {
        std::array<double, 6> scaleMatrix = {sx, 0.0, 0.0, sy, 0.0, 0.0};
        matrix_ = combineMatrices(matrix_, scaleMatrix);
    }
    return *this;
}

Transformation& Transformation::rotate(double angleDegrees, double cx, double cy) {
    if (angleDegrees != 0.0) {
        // Translate to origin
        if (cx != 0.0 || cy != 0.0) {
            translate(cx, cy);
        }

        // Apply rotation
        double rad = angleDegrees * M_PI / 180.0;
        double cosA = std::cos(rad);
        double sinA = std::sin(rad);

        std::array<double, 6> rotateMatrix = {cosA, sinA, -sinA, cosA, 0.0, 0.0};
        matrix_ = combineMatrices(matrix_, rotateMatrix);

        // Translate back
        if (cx != 0.0 || cy != 0.0) {
            translate(-cx, -cy);
        }
    }
    return *this;
}

Transformation& Transformation::skewX(double angleDegrees) {
    if (angleDegrees != 0.0) {
        double tan = std::tan(angleDegrees * M_PI / 180.0);
        std::array<double, 6> skewMatrix = {1.0, 0.0, tan, 1.0, 0.0, 0.0};
        matrix_ = combineMatrices(matrix_, skewMatrix);
    }
    return *this;
}

Transformation& Transformation::skewY(double angleDegrees) {
    if (angleDegrees != 0.0) {
        double tan = std::tan(angleDegrees * M_PI / 180.0);
        std::array<double, 6> skewMatrix = {1.0, tan, 0.0, 1.0, 0.0, 0.0};
        matrix_ = combineMatrices(matrix_, skewMatrix);
    }
    return *this;
}

Point Transformation::apply(const Point& p, bool isRelative) const {
    // Convert integer coordinates to double for transformation, then round back
    return apply(static_cast<double>(p.x), static_cast<double>(p.y), isRelative);
}

Point Transformation::apply(double x, double y, bool isRelative) const {
    // Apply matrix transformation:
    // x' = a*x + c*y + e
    // y' = b*x + d*y + f
    // Transformations (especially rotations) require double precision arithmetic.
    // We compute in double and round to nearest integer at the end.

    double newX = matrix_[0] * x + matrix_[2] * y;
    double newY = matrix_[1] * x + matrix_[3] * y;

    if (!isRelative) {
        // Add translation component
        newX += matrix_[4];
        newY += matrix_[5];
    }

    // Round to nearest integer coordinate
    CoordType intX = static_cast<CoordType>(std::round(newX));
    CoordType intY = static_cast<CoordType>(std::round(newY));

    // Mark as not exact because transformations (especially rotations) lose precision
    return Point(intX, intY, false);
}

std::vector<Point> Transformation::apply(const std::vector<Point>& points, bool isRelative) const {
    std::vector<Point> transformed;
    transformed.reserve(points.size());

    for (const auto& p : points) {
        transformed.push_back(apply(p, isRelative));
    }

    return transformed;
}

// Static factory methods

Transformation Transformation::createRotation(double angleDegrees, double cx, double cy) {
    Transformation t;
    t.rotate(angleDegrees, cx, cy);
    return t;
}

Transformation Transformation::createTranslation(double tx, double ty) {
    Transformation t;
    t.translate(tx, ty);
    return t;
}

Transformation Transformation::createScaling(double sx, double sy) {
    Transformation t;
    t.scale(sx, sy);
    return t;
}

} // namespace deepnest
