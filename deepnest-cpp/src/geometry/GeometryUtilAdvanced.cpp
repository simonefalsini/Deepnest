#include "../../include/deepnest/geometry/GeometryUtilAdvanced.h"
#include "../../include/deepnest/geometry/GeometryUtil.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace deepnest {
namespace GeometryUtil {

// ========== Helper Functions (Internal) ==========

// Check if point c is on segment ab
static bool onSegmentHelper(const Point& a, const Point& b, const Point& c) {
    return almostEqual(c.x, std::max(a.x, b.x)) &&
           almostEqual(c.x, std::min(a.x, b.x)) &&
           almostEqual(c.y, std::max(a.y, b.y)) &&
           almostEqual(c.y, std::min(a.y, b.y));
}

// ========== polygonEdge ==========

std::vector<Point> polygonEdge(const std::vector<Point>& polygon, const Point& normal) {
    if (polygon.size() < 3) {
        return {};
    }

    // Normalize the normal vector
    Point norm = normalizeVector(normal);

    // Direction perpendicular to normal
    Point direction(-norm.y, norm.x);

    // Find min and max points along the direction
    double minDot = std::numeric_limits<double>::max();
    double maxDot = std::numeric_limits<double>::lowest();
    std::vector<double> dotproducts;
    dotproducts.reserve(polygon.size());

    for (const auto& p : polygon) {
        double dot = p.x * direction.x + p.y * direction.y;
        dotproducts.push_back(dot);
        minDot = std::min(minDot, dot);
        maxDot = std::max(maxDot, dot);
    }

    // Find indices with min/max dot products
    // If multiple vertices have min/max values, choose the one most normal
    size_t indexMin = 0;
    size_t indexMax = 0;
    double normalMin = std::numeric_limits<double>::lowest();
    double normalMax = std::numeric_limits<double>::lowest();

    for (size_t i = 0; i < polygon.size(); i++) {
        if (almostEqual(dotproducts[i], minDot)) {
            double dot = polygon[i].x * norm.x + polygon[i].y * norm.y;
            if (dot > normalMin) {
                normalMin = dot;
                indexMin = i;
            }
        } else if (almostEqual(dotproducts[i], maxDot)) {
            double dot = polygon[i].x * norm.x + polygon[i].y * norm.y;
            if (dot > normalMax) {
                normalMax = dot;
                indexMax = i;
            }
        }
    }

    // Determine scan direction (left=-1, right=1)
    size_t indexLeft = (indexMin == 0) ? polygon.size() - 1 : indexMin - 1;
    size_t indexRight = (indexMin + 1) % polygon.size();

    Point minVertex = polygon[indexMin];
    Point left = polygon[indexLeft];
    Point right = polygon[indexRight];

    Point leftVector(left.x - minVertex.x, left.y - minVertex.y);
    Point rightVector(right.x - minVertex.x, right.y - minVertex.y);

    double dotLeft = leftVector.x * direction.x + leftVector.y * direction.y;
    double dotRight = rightVector.x * direction.x + rightVector.y * direction.y;

    int scanDirection = -1;  // Default to left

    if (almostEqual(dotLeft, 0.0)) {
        scanDirection = 1;
    } else if (almostEqual(dotRight, 0.0)) {
        scanDirection = -1;
    } else {
        double normalDotLeft, normalDotRight;

        if (almostEqual(dotLeft, dotRight)) {
            normalDotLeft = leftVector.x * norm.x + leftVector.y * norm.y;
            normalDotRight = rightVector.x * norm.x + rightVector.y * norm.y;
        } else if (dotLeft < dotRight) {
            normalDotLeft = leftVector.x * norm.x + leftVector.y * norm.y;
            normalDotRight = (rightVector.x * norm.x + rightVector.y * norm.y) * (dotLeft / dotRight);
        } else {
            normalDotLeft = (leftVector.x * norm.x + leftVector.y * norm.y) * (dotRight / dotLeft);
            normalDotRight = rightVector.x * norm.x + rightVector.y * norm.y;
        }

        scanDirection = (normalDotLeft > normalDotRight) ? -1 : 1;
    }

    // Collect edge points from indexMin to indexMax
    std::vector<Point> edge;
    size_t i = indexMin;
    size_t count = 0;

    while (count < polygon.size()) {
        edge.push_back(polygon[i]);

        if (i == indexMax) {
            break;
        }

        if (scanDirection == 1) {
            i = (i + 1) % polygon.size();
        } else {
            i = (i == 0) ? polygon.size() - 1 : i - 1;
        }
        count++;
    }

    return edge;
}

// ========== pointLineDistance ==========

std::optional<double> pointLineDistance(
    const Point& p,
    const Point& s1,
    const Point& s2,
    const Point& normal,
    bool s1inclusive,
    bool s2inclusive)
{
    Point norm = normalizeVector(normal);
    Point dir(norm.y, -norm.x);

    double pdot = p.x * dir.x + p.y * dir.y;
    double s1dot = s1.x * dir.x + s1.y * dir.y;
    double s2dot = s2.x * dir.x + s2.y * dir.y;

    double pdotnorm = p.x * norm.x + p.y * norm.y;
    double s1dotnorm = s1.x * norm.x + s1.y * norm.y;
    double s2dotnorm = s2.x * norm.x + s2.y * norm.y;

    // Point is exactly along the edge in the normal direction
    if (almostEqual(pdot, s1dot) && almostEqual(pdot, s2dot)) {
        // Point lies on an endpoint
        if (almostEqual(pdotnorm, s1dotnorm) || almostEqual(pdotnorm, s2dotnorm)) {
            return std::nullopt;
        }

        // Point is outside both endpoints
        if (pdotnorm > s1dotnorm && pdotnorm > s2dotnorm) {
            return std::min(pdotnorm - s1dotnorm, pdotnorm - s2dotnorm);
        }
        if (pdotnorm < s1dotnorm && pdotnorm < s2dotnorm) {
            return -std::min(s1dotnorm - pdotnorm, s2dotnorm - pdotnorm);
        }

        // Point lies between endpoints
        double diff1 = pdotnorm - s1dotnorm;
        double diff2 = pdotnorm - s2dotnorm;
        return (diff1 > 0) ? diff1 : diff2;
    }
    // Point projects onto s1
    else if (almostEqual(pdot, s1dot)) {
        return s1inclusive ? std::optional<double>(pdotnorm - s1dotnorm) : std::nullopt;
    }
    // Point projects onto s2
    else if (almostEqual(pdot, s2dot)) {
        return s2inclusive ? std::optional<double>(pdotnorm - s2dotnorm) : std::nullopt;
    }
    // Point doesn't collide with segment
    else if ((pdot < s1dot && pdot < s2dot) || (pdot > s1dot && pdot > s2dot)) {
        return std::nullopt;
    }

    return pdotnorm - s1dotnorm + (s1dotnorm - s2dotnorm) * (s1dot - pdot) / (s1dot - s2dot);
}

// ========== pointDistance ==========

std::optional<double> pointDistance(
    const Point& p,
    const Point& s1,
    const Point& s2,
    const Point& normal,
    bool infinite)
{
    Point norm = normalizeVector(normal);
    Point dir(norm.y, -norm.x);

    double pdot = p.x * dir.x + p.y * dir.y;
    double s1dot = s1.x * dir.x + s1.y * dir.y;
    double s2dot = s2.x * dir.x + s2.y * dir.y;

    double pdotnorm = p.x * norm.x + p.y * norm.y;
    double s1dotnorm = s1.x * norm.x + s1.y * norm.y;
    double s2dotnorm = s2.x * norm.x + s2.y * norm.y;

    if (!infinite) {
        // Point doesn't collide with segment or lies directly on vertex
        if (((pdot < s1dot || almostEqual(pdot, s1dot)) && (pdot < s2dot || almostEqual(pdot, s2dot))) ||
            ((pdot > s1dot || almostEqual(pdot, s1dot)) && (pdot > s2dot || almostEqual(pdot, s2dot)))) {
            return std::nullopt;
        }

        if ((almostEqual(pdot, s1dot) && almostEqual(pdot, s2dot)) &&
            (pdotnorm > s1dotnorm && pdotnorm > s2dotnorm)) {
            return std::min(pdotnorm - s1dotnorm, pdotnorm - s2dotnorm);
        }

        if ((almostEqual(pdot, s1dot) && almostEqual(pdot, s2dot)) &&
            (pdotnorm < s1dotnorm && pdotnorm < s2dotnorm)) {
            return -std::min(s1dotnorm - pdotnorm, s2dotnorm - pdotnorm);
        }
    }

    return pdotnorm - s1dotnorm + (s1dotnorm - s2dotnorm) * (s1dot - pdot) / (s1dot - s2dot);
}

// ========== segmentDistance ==========

std::optional<double> segmentDistance(
    const Point& A,
    const Point& B,
    const Point& E,
    const Point& F,
    const Point& direction)
{
    Point normal(direction.y, -direction.x);
    Point reverse(-direction.x, -direction.y);

    double dotA = A.x * normal.x + A.y * normal.y;
    double dotB = B.x * normal.x + B.y * normal.y;
    double dotE = E.x * normal.x + E.y * normal.y;
    double dotF = F.x * normal.x + F.y * normal.y;

    double crossA = A.x * direction.x + A.y * direction.y;
    double crossB = B.x * direction.x + B.y * direction.y;
    double crossE = E.x * direction.x + E.y * direction.y;
    double crossF = F.x * direction.x + F.y * direction.y;

    double crossABmin = std::min(crossA, crossB);
    double crossABmax = std::max(crossA, crossB);
    double crossEFmin = std::min(crossE, crossF);
    double crossEFmax = std::max(crossE, crossF);

    double ABmin = std::min(dotA, dotB);
    double ABmax = std::max(dotA, dotB);
    double EFmin = std::min(dotE, dotF);
    double EFmax = std::max(dotE, dotF);

    // Segments that will merely touch at one point
    if (almostEqual(ABmax, EFmin) || almostEqual(ABmin, EFmax)) {
        return std::nullopt;
    }

    // Segments miss each other completely
    if (ABmax < EFmin || ABmin > EFmax) {
        return std::nullopt;
    }

    // Calculate overlap
    double overlap;
    if ((ABmax > EFmax && ABmin < EFmin) || (EFmax > ABmax && EFmin < ABmin)) {
        overlap = 1.0;
    } else {
        double minMax = std::min(ABmax, EFmax);
        double maxMin = std::max(ABmin, EFmin);
        double maxMax = std::max(ABmax, EFmax);
        double minMin = std::min(ABmin, EFmin);
        overlap = (minMax - maxMin) / (maxMax - minMin);
    }

    // Check if lines are colinear
    double crossABE = (E.y - A.y) * (B.x - A.x) - (E.x - A.x) * (B.y - A.y);
    double crossABF = (F.y - A.y) * (B.x - A.x) - (F.x - A.x) * (B.y - A.y);

    if (almostEqual(crossABE, 0.0) && almostEqual(crossABF, 0.0)) {
        Point ABnorm(B.y - A.y, A.x - B.x);
        Point EFnorm(F.y - E.y, E.x - F.x);

        double ABnormLength = std::sqrt(ABnorm.x * ABnorm.x + ABnorm.y * ABnorm.y);
        ABnorm.x /= ABnormLength;
        ABnorm.y /= ABnormLength;

        double EFnormLength = std::sqrt(EFnorm.x * EFnorm.x + EFnorm.y * EFnorm.y);
        EFnorm.x /= EFnormLength;
        EFnorm.y /= EFnormLength;

        // Segment normals must point in opposite directions
        if (std::abs(ABnorm.y * EFnorm.x - ABnorm.x * EFnorm.y) < TOL &&
            ABnorm.y * EFnorm.y + ABnorm.x * EFnorm.x < 0) {
            double normdot = ABnorm.y * direction.y + ABnorm.x * direction.x;
            if (almostEqual(normdot, 0.0)) {
                return std::nullopt;
            }
            if (normdot < 0) {
                return 0.0;
            }
        }
        return std::nullopt;
    }

    std::vector<double> distances;

    // Check coincident points and distances
    if (almostEqual(dotA, dotE)) {
        distances.push_back(crossE - crossA);
    } else if (almostEqual(dotA, dotF)) {
        distances.push_back(crossF - crossA);
    } else if (dotA > EFmin && dotA < EFmax) {
        auto d = pointDistance(A, E, F, reverse);
        if (d.has_value() && almostEqual(d.value(), 0.0)) {
            auto dB = pointDistance(B, E, F, reverse, true);
            if (dB.has_value() && (dB.value() < 0 || almostEqual(dB.value() * overlap, 0.0))) {
                d = std::nullopt;
            }
        }
        if (d.has_value()) {
            distances.push_back(d.value());
        }
    }

    if (almostEqual(dotB, dotE)) {
        distances.push_back(crossE - crossB);
    } else if (almostEqual(dotB, dotF)) {
        distances.push_back(crossF - crossB);
    } else if (dotB > EFmin && dotB < EFmax) {
        auto d = pointDistance(B, E, F, reverse);
        if (d.has_value() && almostEqual(d.value(), 0.0)) {
            auto dA = pointDistance(A, E, F, reverse, true);
            if (dA.has_value() && (dA.value() < 0 || almostEqual(dA.value() * overlap, 0.0))) {
                d = std::nullopt;
            }
        }
        if (d.has_value()) {
            distances.push_back(d.value());
        }
    }

    if (almostEqual(dotE, dotA)) {
        distances.push_back(crossA - crossE);
    } else if (almostEqual(dotE, dotB)) {
        distances.push_back(crossB - crossE);
    } else if (dotE > ABmin && dotE < ABmax) {
        auto d = pointDistance(E, A, B, direction);
        if (d.has_value() && almostEqual(d.value(), 0.0)) {
            auto dF = pointDistance(F, A, B, direction, true);
            if (dF.has_value() && (dF.value() < 0 || almostEqual(dF.value() * overlap, 0.0))) {
                d = std::nullopt;
            }
        }
        if (d.has_value()) {
            distances.push_back(d.value());
        }
    }

    if (almostEqual(dotF, dotA)) {
        distances.push_back(crossA - crossF);
    } else if (almostEqual(dotF, dotB)) {
        distances.push_back(crossB - crossF);
    } else if (dotF > ABmin && dotF < ABmax) {
        auto d = pointDistance(F, A, B, direction);
        if (d.has_value() && almostEqual(d.value(), 0.0)) {
            auto dE = pointDistance(E, A, B, direction, true);
            if (dE.has_value() && (dE.value() < 0 || almostEqual(dE.value() * overlap, 0.0))) {
                d = std::nullopt;
            }
        }
        if (d.has_value()) {
            distances.push_back(d.value());
        }
    }

    if (distances.empty()) {
        return std::nullopt;
    }

    // Find the distance with minimum absolute value (closest collision)
    // But preserve the sign to indicate direction
    double minDist = distances[0];
    double minAbs = std::abs(distances[0]);

    for (size_t i = 1; i < distances.size(); i++) {
        double absVal = std::abs(distances[i]);
        if (absVal < minAbs) {
            minAbs = absVal;
            minDist = distances[i];
        }
    }

    return minDist;
}

// ========== polygonSlideDistance ==========

std::optional<double> polygonSlideDistance(
    const std::vector<Point>& A,
    const std::vector<Point>& B,
    const Point& direction,
    bool ignoreNegative)
{
    std::vector<Point> edgeA = A;
    std::vector<Point> edgeB = B;

    // Close the loops
    if (!almostEqualPoints(edgeA.front(), edgeA.back())) {
        edgeA.push_back(edgeA.front());
    }
    if (!almostEqualPoints(edgeB.front(), edgeB.back())) {
        edgeB.push_back(edgeB.front());
    }

    Point dir = normalizeVector(direction);
    std::optional<double> distance;

    for (size_t i = 0; i < edgeB.size() - 1; i++) {
        for (size_t j = 0; j < edgeA.size() - 1; j++) {
            const Point& A1 = edgeA[j];
            const Point& A2 = edgeA[j + 1];
            const Point& B1 = edgeB[i];
            const Point& B2 = edgeB[i + 1];

            // Ignore extremely small lines
            if ((almostEqual(A1.x, A2.x) && almostEqual(A1.y, A2.y)) ||
                (almostEqual(B1.x, B2.x) && almostEqual(B1.y, B2.y))) {
                continue;
            }

            // Calculate distance B must move in direction to touch A
            auto d = segmentDistance(B1, B2, A1, A2, dir);

            if (d.has_value() && (!distance.has_value() || d.value() < distance.value())) {
                if (!ignoreNegative || d.value() > 0 || almostEqual(d.value(), 0.0)) {
                    distance = d.value();
                }
            }
        }
    }

    return distance;
}

// ========== polygonProjectionDistance ==========

std::optional<double> polygonProjectionDistance(
    const std::vector<Point>& A,
    const std::vector<Point>& B,
    const Point& direction)
{
    std::vector<Point> edgeA = A;
    std::vector<Point> edgeB = B;

    // Close the loops
    if (!almostEqualPoints(edgeA.front(), edgeA.back())) {
        edgeA.push_back(edgeA.front());
    }
    if (!almostEqualPoints(edgeB.front(), edgeB.back())) {
        edgeB.push_back(edgeB.front());
    }

    std::optional<double> distance;

    for (size_t i = 0; i < edgeB.size(); i++) {
        const Point& p = edgeB[i];
        std::optional<double> minProjection;

        for (size_t j = 0; j < edgeA.size() - 1; j++) {
            const Point& s1 = edgeA[j];
            const Point& s2 = edgeA[j + 1];

            // Skip if edge is parallel to direction
            if (std::abs((s2.y - s1.y) * direction.x - (s2.x - s1.x) * direction.y) < TOL) {
                continue;
            }

            // Project point, ignore edge boundaries
            auto d = pointDistance(p, s1, s2, direction);

            if (d.has_value() && (!minProjection.has_value() || d.value() < minProjection.value())) {
                minProjection = d.value();
            }
        }

        if (minProjection.has_value() && (!distance.has_value() || minProjection.value() > distance.value())) {
            distance = minProjection.value();
        }
    }

    return distance;
}

// ========== searchStartPoint ==========

std::optional<Point> searchStartPoint(
    const std::vector<Point>& A,
    const std::vector<Point>& B,
    bool inside,
    const std::vector<std::vector<Point>>& NFP)
{
    std::cerr << "\n=== searchStartPoint DEBUG ===" << std::endl;
    std::cerr << "  A.size()=" << A.size() << ", B.size()=" << B.size()
              << ", inside=" << inside << ", NFP.size()=" << NFP.size() << std::endl;

    std::vector<Point> edgeA = A;
    std::vector<Point> edgeB = B;

    // Close the loops
    if (!almostEqualPoints(edgeA.front(), edgeA.back())) {
        edgeA.push_back(edgeA.front());
    }
    if (!almostEqualPoints(edgeB.front(), edgeB.back())) {
        edgeB.push_back(edgeB.front());
    }

    std::cerr << "  After closing: edgeA.size()=" << edgeA.size()
              << ", edgeB.size()=" << edgeB.size() << std::endl;

    // Helper lambda to check if point already exists in NFP
    auto inNfp = [](const Point& p, const std::vector<std::vector<Point>>& nfp) -> bool {
        if (nfp.empty()) {
            return false;
        }
        for (const auto& polygon : nfp) {
            for (const auto& point : polygon) {
                if (almostEqual(p.x, point.x) && almostEqual(p.y, point.y)) {
                    return true;
                }
            }
        }
        return false;
    };

    int candidatesChecked = 0;
    int candidatesWithValidBinside = 0;
    int candidatesPassedFirstCheck = 0;
    int candidatesTriedSliding = 0;

    for (size_t i = 0; i < edgeA.size() - 1; i++) {
        for (size_t j = 0; j < edgeB.size(); j++) {
            candidatesChecked++;
            // Calculate offset to place B[j] at A[i]
            Point offset(edgeA[i].x - edgeB[j].x, edgeA[i].y - edgeB[j].y);

            // Check if any point of B is inside A with this offset
            std::optional<bool> Binside;
            for (const auto& bp : edgeB) {
                Point testPoint(bp.x + offset.x, bp.y + offset.y);
                auto inpoly = pointInPolygon(testPoint, edgeA);
                if (inpoly.has_value()) {
                    Binside = inpoly.value();
                    break;
                }
            }

            if (!Binside.has_value()) {
                // All points of B are on the boundary of A - treat as outside for OUTER NFP
                // This can happen when B's vertices lie exactly on A's edges
                Binside = false;
                if (candidatesChecked <= 10) {
                    std::cerr << "    Note: All B points on boundary, treating as outside" << std::endl;
                }
            }

            candidatesWithValidBinside++;

            // Check if this is a valid start point
            Point startPoint = offset;

            // Create offset version of B for intersection test
            std::vector<Point> offsetB;
            for (const auto& bp : edgeB) {
                offsetB.push_back(Point(bp.x + offset.x, bp.y + offset.y));
            }

            bool insideMatch = (Binside.value() && inside) || (!Binside.value() && !inside);
            bool noIntersection = !intersect(edgeA, offsetB);
            bool notInNfp = !inNfp(startPoint, NFP);

            if (candidatesChecked <= 10) {  // Debug first few candidates
                std::cerr << "    Candidate [" << i << "," << j << "]: offset=(" << offset.x << "," << offset.y << ")"
                          << " Binside=" << Binside.value()
                          << ", inside=" << inside
                          << ", insideMatch=" << insideMatch
                          << ", noIntersect=" << noIntersection
                          << ", notInNfp=" << notInNfp << std::endl;
            }

            if (insideMatch && noIntersection && notInNfp) {
                std::cerr << "  FOUND valid start point at [" << i << "," << j << "]: ("
                          << startPoint.x << ", " << startPoint.y << ")" << std::endl;
                return startPoint;
            }

            candidatesPassedFirstCheck++;

            // Try sliding B along the edge vector
            Point edgeVec(edgeA[i + 1].x - edgeA[i].x, edgeA[i + 1].y - edgeA[i].y);

            // Create offset B for projection distance calculation
            std::vector<Point> tempB = edgeB;
            for (auto& bp : tempB) {
                bp.x += offset.x;
                bp.y += offset.y;
            }

            auto d1 = polygonProjectionDistance(edgeA, tempB, edgeVec);
            auto d2 = polygonProjectionDistance(tempB, edgeA, Point(-edgeVec.x, -edgeVec.y));

            std::optional<double> d;
            if (!d1.has_value() && !d2.has_value()) {
                continue;
            } else if (!d1.has_value()) {
                d = d2;
            } else if (!d2.has_value()) {
                d = d1;
            } else {
                d = std::min(d1.value(), d2.value());
            }

            // Only slide if d is positive and not almost zero
            // JavaScript: if(d !== null && !_almostEqual(d,0) && d > 0)
            if (!d.has_value() || almostEqual(d.value(), 0.0) || d.value() <= 0) {
                continue;
            }

            candidatesTriedSliding++;

            double vd2 = edgeVec.x * edgeVec.x + edgeVec.y * edgeVec.y;
            if (d.value() * d.value() < vd2 && !almostEqual(d.value() * d.value(), vd2)) {
                double vd = std::sqrt(vd2);
                edgeVec.x *= d.value() / vd;
                edgeVec.y *= d.value() / vd;
            }

            Point newOffset(offset.x + edgeVec.x, offset.y + edgeVec.y);

            // Check again with new offset
            Binside = std::nullopt;
            for (const auto& bp : edgeB) {
                Point testPoint(bp.x + newOffset.x, bp.y + newOffset.y);
                auto inpoly = pointInPolygon(testPoint, edgeA);
                if (inpoly.has_value()) {
                    Binside = inpoly.value();
                    break;
                }
            }

            startPoint = newOffset;

            offsetB.clear();
            for (const auto& bp : edgeB) {
                offsetB.push_back(Point(bp.x + newOffset.x, bp.y + newOffset.y));
            }

            if (Binside.has_value() &&
                ((Binside.value() && inside) || (!Binside.value() && !inside)) &&
                !intersect(edgeA, offsetB) && !inNfp(startPoint, NFP)) {
                return startPoint;
            }
        }
    }

    // No valid start point found after checking all candidates
    std::cerr << "  FAILED: No valid start point found" << std::endl;
    std::cerr << "  Statistics:" << std::endl;
    std::cerr << "    - Total candidates checked: " << candidatesChecked << std::endl;
    std::cerr << "    - Candidates with valid Binside: " << candidatesWithValidBinside << std::endl;
    std::cerr << "    - Candidates passed first check: " << candidatesPassedFirstCheck << std::endl;
    std::cerr << "    - Candidates tried sliding: " << candidatesTriedSliding << std::endl;

    return std::nullopt;
}

// ========== polygonHull ==========

std::optional<std::vector<Point>> polygonHull(
    const std::vector<Point>& A,
    const std::vector<Point>& B)
{
    if (A.size() < 3 || B.size() < 3) {
        return std::nullopt;
    }

    // Find the extreme point with minimum y coordinate
    double miny = A[0].y;
    size_t startIndex = 0;
    bool startIsA = true;

    for (size_t i = 0; i < A.size(); i++) {
        if (A[i].y < miny) {
            miny = A[i].y;
            startIndex = i;
            startIsA = true;
        }
    }

    for (size_t i = 0; i < B.size(); i++) {
        if (B[i].y < miny) {
            miny = B[i].y;
            startIndex = i;
            startIsA = false;
        }
    }

    // Make A the starting polygon
    std::vector<Point> polyA = startIsA ? A : B;
    std::vector<Point> polyB = startIsA ? B : A;

    std::vector<Point> hull;
    size_t current = startIndex;
    std::optional<size_t> intercept1, intercept2;

    // Scan forward from the starting point
    for (size_t i = 0; i < polyA.size() + 1; i++) {
        size_t next = (current + 1) % polyA.size();
        bool touching = false;

        for (size_t j = 0; j < polyB.size(); j++) {
            size_t nextj = (j + 1) % polyB.size();

            // Check if vertices coincide
            if (almostEqual(polyA[current].x, polyB[j].x) &&
                almostEqual(polyA[current].y, polyB[j].y)) {
                hull.push_back(polyA[current]);
                intercept1 = j;
                touching = true;
                break;
            }
            // Check if B[j] is on edge A[current]-A[next]
            else if (onSegmentHelper(polyA[current], polyA[next], polyB[j])) {
                hull.push_back(polyA[current]);
                hull.push_back(polyB[j]);
                intercept1 = j;
                touching = true;
                break;
            }
            // Check if A[current] is on edge B[j]-B[nextj]
            else if (onSegmentHelper(polyB[j], polyB[nextj], polyA[current])) {
                hull.push_back(polyA[current]);
                hull.push_back(polyB[nextj]);
                intercept1 = nextj;
                touching = true;
                break;
            }
        }

        if (touching) {
            break;
        }

        hull.push_back(polyA[current]);
        current = next;
    }

    // Scan backward from the starting point
    current = (startIndex == 0) ? polyA.size() - 1 : startIndex - 1;
    std::vector<Point> hullReverse;

    for (size_t i = 0; i < polyA.size() + 1; i++) {
        size_t next = (current == 0) ? polyA.size() - 1 : current - 1;
        bool touching = false;

        for (size_t j = 0; j < polyB.size(); j++) {
            size_t nextj = (j + 1) % polyB.size();

            if (almostEqual(polyA[current].x, polyB[j].x) &&
                almostEqual(polyA[current].y, polyB[j].y)) {
                hullReverse.insert(hullReverse.begin(), polyA[current]);
                intercept2 = j;
                touching = true;
                break;
            } else if (onSegmentHelper(polyA[current], polyA[next], polyB[j])) {
                hullReverse.insert(hullReverse.begin(), polyA[current]);
                hullReverse.insert(hullReverse.begin(), polyB[j]);
                intercept2 = j;
                touching = true;
                break;
            } else if (onSegmentHelper(polyB[j], polyB[nextj], polyA[current])) {
                hullReverse.insert(hullReverse.begin(), polyA[current]);
                intercept2 = j;
                touching = true;
                break;
            }
        }

        if (touching) {
            break;
        }

        hullReverse.insert(hullReverse.begin(), polyA[current]);
        current = next;
    }

    // Combine forward and reverse hulls
    hull.insert(hull.begin(), hullReverse.begin(), hullReverse.end());

    // Add B polygon vertices between intercepts
    if (intercept1.has_value() && intercept2.has_value()) {
        size_t i = intercept1.value();
        size_t count = 0;

        while (count < polyB.size()) {
            i = (i + 1) % polyB.size();
            if (i == intercept2.value()) {
                break;
            }
            hull.push_back(polyB[i]);
            count++;
        }
    }

    return hull.empty() ? std::nullopt : std::optional<std::vector<Point>>(hull);
}

} // namespace GeometryUtil
} // namespace deepnest
