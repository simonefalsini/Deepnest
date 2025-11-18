#include "../../include/deepnest/placement/MergeDetection.h"
#include <cmath>
#include <algorithm>

namespace deepnest {

MergeDetection::MergeResult MergeDetection::calculateMergedLength(
    const std::vector<Polygon>& placed,
    const Polygon& newPart,
    double minLength,
    double tolerance
) {
    // Call internal recursive implementation
    return calculateMergedLengthInternal(placed, newPart.points, minLength, tolerance);
}

MergeDetection::MergeResult MergeDetection::calculateMergedLengthInternal(
    const std::vector<Polygon>& parts,
    const std::vector<Point>& p,
    double minLength,
    double tolerance
) {
    // JavaScript: var min2 = minlength*minlength;
    //             var totalLength = 0;
    //             var segments = [];
    double min2 = minLength * minLength;
    MergeResult result;
    result.totalLength = 0.0;

    // JavaScript: for(var i=0; i<p.length; i++)
    // Iterate through edges of the new part
    for (size_t i = 0; i < p.size(); i++) {
        // JavaScript: var A1 = p[i];
        //             if(i+1 == p.length) { A2 = p[0]; }
        //             else { var A2 = p[i+1]; }
        const Point& A1 = p[i];
        const Point& A2 = (i + 1 == p.size()) ? p[0] : p[i + 1];

        // JavaScript: if(!A1.exact || !A2.exact) { continue; }
        if (!A1.exact || !A2.exact) {
            continue;
        }

        // JavaScript: var Ax2 = (A2.x-A1.x)*(A2.x-A1.x);
        //             var Ay2 = (A2.y-A1.y)*(A2.y-A1.y);
        //             if(Ax2+Ay2 < min2) { continue; }
        double Ax2 = (A2.x - A1.x) * (A2.x - A1.x);
        double Ay2 = (A2.y - A1.y) * (A2.y - A1.y);

        if (Ax2 + Ay2 < min2) {
            continue; // Edge too short
        }

        // JavaScript: var angle = Math.atan2((A2.y-A1.y),(A2.x-A1.x));
        //             var c = Math.cos(-angle);
        //             var s = Math.sin(-angle);
        //             var c2 = Math.cos(angle);
        //             var s2 = Math.sin(angle);
        double angle = std::atan2(A2.y - A1.y, A2.x - A1.x);
        double c = std::cos(-angle);
        double s = std::sin(-angle);
        double c2 = std::cos(angle);
        double s2 = std::sin(angle);

        // JavaScript: var relA2 = {x: A2.x-A1.x, y: A2.y-A1.y};
        //             var rotA2x = relA2.x * c - relA2.y * s;
        Point relA2(A2.x - A1.x, A2.y - A1.y);
        double rotA2x = relA2.x * c - relA2.y * s;

        // JavaScript: for(var j=0; j<parts.length; j++)
        // Check all placed parts
        for (size_t j = 0; j < parts.size(); j++) {
            const Polygon& B = parts[j];

            // JavaScript: if(B.length > 1)
            if (B.points.size() > 1) {
                // JavaScript: for(var k=0; k<B.length; k++)
                // Check all edges of this placed part
                for (size_t k = 0; k < B.points.size(); k++) {
                    // JavaScript: var B1 = B[k];
                    //             if(k+1 == B.length) { var B2 = B[0]; }
                    //             else { var B2 = B[k+1]; }
                    const Point& B1 = B.points[k];
                    const Point& B2 = (k + 1 == B.points.size()) ? B.points[0] : B.points[k + 1];

                    // JavaScript: if(!B1.exact || !B2.exact) { continue; }
                    if (!B1.exact || !B2.exact) {
                        continue;
                    }

                    // JavaScript: var Bx2 = (B2.x-B1.x)*(B2.x-B1.x);
                    //             var By2 = (B2.y-B1.y)*(B2.y-B1.y);
                    //             if(Bx2+By2 < min2) { continue; }
                    double Bx2 = (B2.x - B1.x) * (B2.x - B1.x);
                    double By2 = (B2.y - B1.y) * (B2.y - B1.y);

                    if (Bx2 + By2 < min2) {
                        continue; // Edge too short
                    }

                    // JavaScript: var relB1 = {x: B1.x - A1.x, y: B1.y - A1.y};
                    //             var relB2 = {x: B2.x - A1.x, y: B2.y - A1.y};
                    Point relB1(B1.x - A1.x, B1.y - A1.y);
                    Point relB2(B2.x - A1.x, B2.y - A1.y);

                    // JavaScript: var rotB1 = {x: relB1.x * c - relB1.y * s, y: relB1.x * s + relB1.y * c};
                    //             var rotB2 = {x: relB2.x * c - relB2.y * s, y: relB2.x * s + relB2.y * c};
                    Point rotB1(
                        relB1.x * c - relB1.y * s,
                        relB1.x * s + relB1.y * c
                    );
                    Point rotB2(
                        relB2.x * c - relB2.y * s,
                        relB2.x * s + relB2.y * c
                    );

                    // JavaScript: if(!GeometryUtil.almostEqual(rotB1.y, 0, tolerance) ||
                    //                !GeometryUtil.almostEqual(rotB2.y, 0, tolerance)) { continue; }
                    // Check if edge B is aligned with edge A (both y-coordinates should be ~0 after rotation)
                    if (!almostEqual(rotB1.y, 0.0, tolerance) || !almostEqual(rotB2.y, 0.0, tolerance)) {
                        continue; // Not aligned
                    }

                    // JavaScript: var min1 = Math.min(0, rotA2x);
                    //             var max1 = Math.max(0, rotA2x);
                    //             var min2 = Math.min(rotB1.x, rotB2.x);
                    //             var max2 = Math.max(rotB1.x, rotB2.x);
                    double min1 = std::min(0.0, rotA2x);
                    double max1 = std::max(0.0, rotA2x);
                    double min2_seg = std::min(rotB1.x, rotB2.x);
                    double max2_seg = std::max(rotB1.x, rotB2.x);

                    // JavaScript: if(min2 >= max1 || max2 <= min1) { continue; }
                    // Check if segments overlap
                    if (min2_seg >= max1 || max2_seg <= min1) {
                        continue; // No overlap
                    }

                    // JavaScript: var len = 0;
                    //             var relC1x = 0;
                    //             var relC2x = 0;
                    double len = 0.0;
                    double relC1x = 0.0;
                    double relC2x = 0.0;

                    // Calculate overlap length and coordinates
                    // JavaScript: if(GeometryUtil.almostEqual(min1, min2) && GeometryUtil.almostEqual(max1, max2))
                    if (almostEqual(min1, min2_seg, tolerance) && almostEqual(max1, max2_seg, tolerance)) {
                        // A is B (exact match)
                        // JavaScript: len = max1-min1; relC1x = min1; relC2x = max1;
                        len = max1 - min1;
                        relC1x = min1;
                        relC2x = max1;
                    }
                    // JavaScript: else if(min1 > min2 && max1 < max2)
                    else if (min1 > min2_seg && max1 < max2_seg) {
                        // A inside B
                        // JavaScript: len = max1-min1; relC1x = min1; relC2x = max1;
                        len = max1 - min1;
                        relC1x = min1;
                        relC2x = max1;
                    }
                    // JavaScript: else if(min2 > min1 && max2 < max1)
                    else if (min2_seg > min1 && max2_seg < max1) {
                        // B inside A
                        // JavaScript: len = max2-min2; relC1x = min2; relC2x = max2;
                        len = max2_seg - min2_seg;
                        relC1x = min2_seg;
                        relC2x = max2_seg;
                    }
                    else {
                        // Partial overlap
                        // JavaScript: len = Math.max(0, Math.min(max1, max2) - Math.max(min1, min2));
                        //             relC1x = Math.min(max1, max2);
                        //             relC2x = Math.max(min1, min2);
                        len = std::max(0.0, std::min(max1, max2_seg) - std::max(min1, min2_seg));
                        relC1x = std::min(max1, max2_seg);
                        relC2x = std::max(min1, min2_seg);
                    }

                    // JavaScript: if(len*len > min2)
                    if (len * len > min2) {
                        // Add to total length
                        // JavaScript: totalLength += len;
                        result.totalLength += len;

                        // Transform overlap coordinates back to world space
                        // JavaScript: var relC1 = {x: relC1x * c2, y: relC1x * s2};
                        //             var relC2 = {x: relC2x * c2, y: relC2x * s2};
                        Point relC1(relC1x * c2, relC1x * s2);
                        Point relC2(relC2x * c2, relC2x * s2);

                        // JavaScript: var C1 = {x: relC1.x + A1.x, y: relC1.y + A1.y};
                        //             var C2 = {x: relC2.x + A1.x, y: relC2.y + A1.y};
                        Point C1(relC1.x + A1.x, relC1.y + A1.y);
                        Point C2(relC2.x + A1.x, relC2.y + A1.y);

                        // JavaScript: segments.push([C1, C2]);
                        result.segments.push_back(std::make_pair(C1, C2));
                    }
                }
            }

            // JavaScript: if(B.children && B.children.length > 0) {
            //               var child = mergedLength(B.children, p, minlength, tolerance);
            //               totalLength += child.totalLength;
            //               segments = segments.concat(child.segments);
            //             }
            // Recursively process children (holes)
            if (!B.children.empty()) {
                MergeResult childResult = calculateMergedLengthInternal(B.children, p, minLength, tolerance);
                result.totalLength += childResult.totalLength;
                result.segments.insert(result.segments.end(),
                                      childResult.segments.begin(),
                                      childResult.segments.end());
            }
        }
    }

    // JavaScript: return {totalLength: totalLength, segments: segments};
    return result;
}

bool MergeDetection::almostEqual(double a, double b, double tolerance) {
    return std::abs(a - b) < tolerance;
}

} // namespace deepnest
