#include "../../include/deepnest/placement/PlacementWorker.h"
#include "../../include/deepnest/geometry/GeometryUtil.h"
#include "../../include/deepnest/geometry/Transformation.h"
#include "../../include/deepnest/geometry/PolygonOperations.h"
#include <algorithm>
#include <limits>
#include <cmath>

namespace deepnest {

PlacementWorker::PlacementWorker(const DeepNestConfig& config, NFPCalculator& calculator)
    : config_(config)
    , nfpCalculator_(calculator)
{
    // Create placement strategy based on config
    // JavaScript: placementType options are 'gravity', 'box', 'convexhull'
    strategy_ = PlacementStrategy::create(config_.placementType);
}

PlacementWorker::PlacementResult PlacementWorker::placeParts(
    std::vector<Polygon> sheets,
    std::vector<Polygon> parts
) {
    // JavaScript: function placeParts(sheets, parts, config, nestindex)
    PlacementResult result;

    if (sheets.empty()) {
        result.unplacedParts = parts;
        result.fitness = parts.size() * 2.0; // Penalty for unplaced parts
        return result;
    }

    // JavaScript: var totalMerged = 0;
    double totalMerged = 0.0;

    // JavaScript: var rotated = [];
    //             for(i=0; i<parts.length; i++){
    //               var r = rotatePolygon(parts[i], parts[i].rotation);
    //               r.rotation = parts[i].rotation;
    //               r.source = parts[i].source;
    //               r.id = parts[i].id;
    //               rotated.push(r);
    //             }
    //             parts = rotated;
    std::cout << "\n=== PLACEMENT START ===" << std::endl;
    std::cout << "Number of parts to place: " << parts.size() << std::endl;
    std::cout << "Number of sheets: " << sheets.size() << std::endl;
    std::cout.flush();

    std::vector<Polygon> rotatedParts;
    for (size_t idx = 0; idx < parts.size(); ++idx) {
        auto& part = parts[idx];
        Polygon rotated = part.rotate(part.rotation);

        // CRITICAL FIX: DO NOT NORMALIZE after rotation!
        // The JavaScript code does NOT normalize - it keeps negative coordinates
        // NFP calculation DEPENDS on parts[0] being at the rotated position
        // Normalization breaks the coordinate system contract
        // Negative coordinates after rotation are PERFECTLY FINE!

        rotated.rotation = part.rotation;
        rotated.source = part.source;
        rotated.id = part.id;

        // Debug log for first part
        if (idx == 0) {
            std::cout << "\n=== ROTATION DEBUG: First part ===" << std::endl;
            std::cout << "  Original rotation: " << part.rotation << std::endl;
            std::cout << "  Original points[0]: (" << part.points[0].x << ", " << part.points[0].y << ")" << std::endl;
            std::cout << "  Rotated points[0]: (" << rotated.points[0].x << ", " << rotated.points[0].y << ")" << std::endl;
            std::cout.flush();
        }

        rotatedParts.push_back(rotated);
    }
    parts = rotatedParts;

    // JavaScript: var allplacements = [];
    //             var fitness = 0;
    std::vector<std::vector<Placement>> allPlacements;
    double fitness = 0.0;
    double totalSheetArea = 0.0;

    // JavaScript: while(parts.length > 0)
    while (!parts.empty() && !sheets.empty()) {
        // JavaScript: var placed = [];
        //             var placements = [];
        std::vector<Polygon> placed;
        std::vector<Placement> placements;

        // JavaScript: var sheet = sheets.shift();
        //             var sheetarea = Math.abs(GeometryUtil.polygonArea(sheet));
        //             totalsheetarea += sheetarea;
        //             fitness += sheetarea;
        Polygon sheet = sheets.front();
        sheets.erase(sheets.begin());

        double sheetArea = std::abs(GeometryUtil::polygonArea(sheet.points));
        totalSheetArea += sheetArea;
        fitness += sheetArea;

        // JavaScript: for(i=0; i<parts.length; i++)
        for (size_t i = 0; i < parts.size(); ) {
            Polygon& part = parts[i];

            // JavaScript: var sheetNfp = null;
            //             for(j=0; j<(360/config.rotations); j++) {
            //               sheetNfp = getInnerNfp(sheet, part, config);
            //               if(sheetNfp) { break; }
            //               var r = rotatePolygon(part, 360/config.rotations);
            //               // ... update rotation
            //               part = r;
            //               parts[i] = r;
            //             }
            Polygon innerNfp;
            bool foundValidRotation = false;

            // Try to find a valid rotation for the first part
            // (to ensure all parts can be placed if possible)
            int maxRotationAttempts = (placed.empty() && config_.rotations > 0)
                ? (360 / config_.rotations)
                : 1;

            for (int rotAttempt = 0; rotAttempt < maxRotationAttempts; rotAttempt++) {
                // Debug logging for first part's first rotation attempt
                if (placements.empty() && rotAttempt == 0) {
                    std::cout << "\n=== NFP CALCULATION DEBUG ===" << std::endl;
                    std::cout << "  Sheet first 4 points: ";
                    for (size_t p = 0; p < std::min(size_t(4), sheet.points.size()); ++p) {
                        std::cout << "(" << sheet.points[p].x << "," << sheet.points[p].y << ") ";
                    }
                    std::cout << std::endl;
                    std::cout << "  Part first 4 points: ";
                    for (size_t p = 0; p < std::min(size_t(4), part.points.size()); ++p) {
                        std::cout << "(" << part.points[p].x << "," << part.points[p].y << ") ";
                    }
                    std::cout << std::endl;
                    std::cout.flush();
                }

                auto innerNfps = nfpCalculator_.getInnerNFP(sheet, part);
                if (!innerNfps.empty()) {
                    innerNfp = innerNfps[0]; // Use first NFP polygon
                }

                if (!innerNfp.points.empty()) {
                    foundValidRotation = true;
                    break;
                }

                // Try next rotation
                if (rotAttempt < maxRotationAttempts - 1 && config_.rotations > 0) {
                    double rotationStep = 360.0 / config_.rotations;
                    Polygon rotated = part.rotate(rotationStep);

                    // CRITICAL FIX: DO NOT NORMALIZE - see above explanation
                    // Negative coordinates are expected and correct!

                    rotated.rotation = part.rotation + rotationStep;
                    rotated.source = part.source;
                    rotated.id = part.id;

                    if (rotated.rotation >= 360.0) {
                        rotated.rotation = std::fmod(rotated.rotation, 360.0);
                    }

                    part = rotated;
                    parts[i] = rotated;
                }
            }

            // JavaScript: if(!sheetNfp || sheetNfp.length == 0) { continue; }
            if (!foundValidRotation || innerNfp.points.empty()) {
                i++;
                continue;
            }

            Placement position;

            // JavaScript: if(placed.length == 0)
            if (placed.empty()) {
                // JavaScript: for(j=0; j<sheetNfp.length; j++) {
                //               for(k=0; k<sheetNfp[j].length; k++) {
                //                 if(position === null || sheetNfp[j][k].x-part[0].x < position.x || ...)
                //                   position = { x: sheetNfp[j][k].x-part[0].x, y: sheetNfp[j][k].y-part[0].y, ...}
                //               }
                //             }
                // First placement: top-left corner
                bool foundPosition = false;
                Point bestPos;
                double minX = std::numeric_limits<double>::max();
                double minY = std::numeric_limits<double>::max();

                // CRITICAL FIX: Use part.points[0] NOT bbox.min!
                // NFPCalculator translates innerNFP by B.points[0] (see NFPCalculator.cpp:30)
                // We MUST subtract the SAME value to cancel the translation
                // Using bbox.min (0,0) doesn't cancel the points[0] offset!
                Point partRef(part.points[0].x, part.points[0].y);

                // DEBUG LOGGING for first placement
                if (placements.empty()) {
                    std::cout << "\n=== PLACEMENT DEBUG: First placement ===" << std::endl;
                    std::cout << "  Part ID: " << part.id << ", Source: " << part.source
                              << ", Rotation: " << part.rotation << std::endl;
                    std::cout << "  Part.points[0]: (" << partRef.x << ", " << partRef.y << ")" << std::endl;
                    std::cout << "  Part first 4 points: ";
                    for (size_t p = 0; p < std::min(size_t(4), part.points.size()); ++p) {
                        std::cout << "(" << part.points[p].x << "," << part.points[p].y << ") ";
                    }
                    std::cout << std::endl;
                    std::cout << "  InnerNFP has " << innerNfp.points.size() << " points" << std::endl;
                    if (!innerNfp.points.empty()) {
                        std::cout << "  InnerNFP first 4 points: ";
                        for (size_t p = 0; p < std::min(size_t(4), innerNfp.points.size()); ++p) {
                            std::cout << "(" << innerNfp.points[p].x << "," << innerNfp.points[p].y << ") ";
                        }
                        std::cout << std::endl;
                    }
                    std::cout.flush();
                }

                // innerNfp may have children, check main polygon
                for (const auto& nfpPoint : innerNfp.points) {
                    Point candidatePos(
                        nfpPoint.x - partRef.x,
                        nfpPoint.y - partRef.y
                    );

                    if (!foundPosition ||
                        candidatePos.x < minX ||
                        (Point::almostEqual(candidatePos.x, minX) && candidatePos.y < minY)) {
                        minX = candidatePos.x;
                        minY = candidatePos.y;
                        bestPos = candidatePos;
                        foundPosition = true;
                    }
                }

                if (foundPosition) {
                    // DEBUG LOGGING for calculated position
                    if (placements.empty()) {
                        std::cout << "  Selected innerNFP point (minX,minY): ("
                                  << (minX + partRef.x) << ", " << (minY + partRef.y) << ")" << std::endl;
                        std::cout << "  Calculated position: (" << bestPos.x << ", " << bestPos.y << ")" << std::endl;
                        std::cout << "  Formula: nfpPoint - partRef = ("
                                  << (minX + partRef.x) << "," << (minY + partRef.y) << ") - ("
                                  << partRef.x << "," << partRef.y << ") = ("
                                  << minX << "," << minY << ")" << std::endl;
                        std::cout.flush();
                    }

                    position = Placement(bestPos, part.id, part.source, part.rotation);
                    placements.push_back(position);
                    placed.push_back(part);

                    // Remove from parts list
                    parts.erase(parts.begin() + i);
                    // Don't increment i, since we removed this element
                }
                else {
                    i++;
                }
                continue;
            }

            // Not the first part - need to calculate NFPs with placed parts
            // JavaScript: var clipperSheetNfp = innerNfpToClipperCoordinates(sheetNfp, config);
            //             var clipper = new ClipperLib.Clipper();
            //             var combinedNfp = new ClipperLib.Paths();

            std::vector<Polygon> outerNfps;
            bool error = false;

            // JavaScript: for(j=startindex; j<placed.length; j++)
            for (size_t j = 0; j < placed.size(); j++) {
                // JavaScript: nfp = getOuterNfp(placed[j], part);
                Polygon outerNfp = nfpCalculator_.getOuterNFP(placed[j], part, false);

                if (outerNfp.points.empty()) {
                    error = true;
                    break;
                }

                // JavaScript: for(m=0; m<nfp.length; m++) {
                //               nfp[m].x += placements[j].x;
                //               nfp[m].y += placements[j].y;
                //             }
                // Shift to placed location
                for (auto& point : outerNfp.points) {
                    point.x += placements[j].position.x;
                    point.y += placements[j].position.y;
                }

                // Handle children (holes)
                for (auto& child : outerNfp.children) {
                    for (auto& point : child.points) {
                        point.x += placements[j].position.x;
                        point.y += placements[j].position.y;
                    }
                }

                outerNfps.push_back(outerNfp);
            }

            if (error) {
                i++;
                continue;
            }

            // JavaScript: clipper.Execute(ClipperLib.ClipType.ctUnion, combinedNfp, ...)
            // Union all outer NFPs
            std::vector<std::vector<Point>> combinedNfpPoints;
            if (!outerNfps.empty()) {
                // Convert Polygons to point vectors for union
                std::vector<std::vector<Point>> outerNfpPoints;
                for (const auto& nfp : outerNfps) {
                    outerNfpPoints.push_back(nfp.points);
                }
                combinedNfpPoints = PolygonOperations::unionPolygons(outerNfpPoints);
            }

            // JavaScript: var finalNfp = new ClipperLib.Paths();
            //             clipper = new ClipperLib.Clipper();
            //             clipper.AddPaths(combinedNfp, ClipperLib.PolyType.ptClip, true);
            //             clipper.AddPaths(clipperSheetNfp, ClipperLib.PolyType.ptSubject, true);
            //             if(!clipper.Execute(ClipperLib.ClipType.ctDifference, finalNfp, ...))
            // Difference: innerNfp - combinedNfp
            std::vector<Polygon> finalNfp;

            if (combinedNfpPoints.empty()) {
                // No outer NFPs, just use inner NFP
                finalNfp.push_back(innerNfp);
            }
            else {
                // Perform difference operation
                // Convert combined NFP to Polygon for difference
                Polygon combinedNfpPoly;
                if (!combinedNfpPoints.empty()) {
                    combinedNfpPoly.points = combinedNfpPoints[0];
                    // Add remaining as children (holes) if any
                    for (size_t idx = 1; idx < combinedNfpPoints.size(); idx++) {
                        Polygon child;
                        child.points = combinedNfpPoints[idx];
                        combinedNfpPoly.children.push_back(child);
                    }
                }

                std::vector<std::vector<Point>> differenceResult =
                    PolygonOperations::differencePolygons(innerNfp.points, combinedNfpPoly.points);

                // Convert result back to Polygons
                for (const auto& pointVec : differenceResult) {
                    Polygon poly;
                    poly.points = pointVec;
                    finalNfp.push_back(poly);
                }
            }

            // JavaScript: if(!finalNfp || finalNfp.length == 0) { continue; }
            if (finalNfp.empty()) {
                i++;
                continue;
            }

            // Filter small polygons
            // JavaScript: for(j=0; j<finalNfp.length; j++) {
            //               var area = Math.abs(ClipperLib.Clipper.Area(finalNfp[j]));
            //               if(finalNfp[j].length < 3 || area < 0.1*...) {
            //                 finalNfp.splice(j,1); j--;
            //               }
            //             }
            finalNfp.erase(
                std::remove_if(finalNfp.begin(), finalNfp.end(),
                    [](const Polygon& poly) {
                        return poly.points.size() < 3 ||
                               std::abs(GeometryUtil::polygonArea(poly.points)) < 0.1;
                    }),
                finalNfp.end()
            );

            if (finalNfp.empty()) {
                i++;
                continue;
            }

            // JavaScript: var minwidth = null; var minarea = null; ...
            //             for(j=0; j<finalNfp.length; j++) {
            //               nf = finalNfp[j];
            //               for(k=0; k<nf.length; k++) {
            //                 ...
            //               }
            //             }
            // Extract candidate positions and find best one
            // CRITICAL FIX: Pass part to subtract reference point from NFP points
            std::vector<Point> candidatePositions = extractCandidatePositions(finalNfp, part);

            if (candidatePositions.empty()) {
                i++;
                continue;
            }

            // Convert placed parts to PlacedPart format for strategy
            std::vector<PlacedPart> placedForStrategy;
            for (size_t j = 0; j < placed.size(); j++) {
                placedForStrategy.push_back(toPlacedPart(placed[j], placements[j]));
            }

            // Use strategy to find best position
            Point bestPosition = strategy_->findBestPosition(
                part,
                placedForStrategy,
                candidatePositions
            );

            // JavaScript: if(position) { placed.push(path); placements.push(position); }
            // CRITICAL FIX: Accept ANY valid position, including (0,0)!
            // The old condition rejected (0,0) which is a perfectly valid placement
            if (!candidatePositions.empty()) {
                position = Placement(bestPosition, part.id, part.source, part.rotation);
                placements.push_back(position);
                placed.push_back(part);

                // Remove from parts list
                parts.erase(parts.begin() + i);
                // Don't increment i
            }
            else {
                // No valid positions found, skip this part
                i++;
            }
        }

        // JavaScript: if(placements && placements.length > 0) {
        //               allplacements.push(placements);
        //             }
        if (!placements.empty()) {
            allPlacements.push_back(placements);
        }
        else {
            break; // No progress made
        }
    }

    // JavaScript: fitness += 2*parts.length;
    fitness += 2.0 * parts.size(); // Penalty for unplaced parts

    // Calculate total merged length
    // JavaScript: totalMerged = ... (calculated in separate loop)
    totalMerged = calculateTotalMergedLength(allPlacements, rotatedParts);

    // GA DEBUG: Log fitness calculation
    static int placementCount = 0;
    if (placementCount < 3) {  // Log first 3 placements only
        std::cout << "\n=== PLACEMENT RESULT #" << placementCount << " ===" << std::endl;
        std::cout << "  Sheets used: " << allPlacements.size() << std::endl;
        std::cout << "  Total sheet area: " << totalSheetArea << std::endl;
        std::cout << "  Unplaced parts: " << parts.size() << std::endl;
        std::cout << "  Unplaced penalty: " << (2.0 * parts.size()) << std::endl;
        std::cout << "  FINAL FITNESS: " << fitness
                  << " (area=" << totalSheetArea << " + penalty=" << (2.0 * parts.size()) << ")" << std::endl;
        std::cout.flush();
        placementCount++;
    }

    result.placements = allPlacements;
    result.fitness = fitness;
    result.area = totalSheetArea;
    result.mergedLength = totalMerged;
    result.unplacedParts = parts;

    return result;
}

std::vector<Point> PlacementWorker::extractCandidatePositions(
    const std::vector<Polygon>& finalNfp,
    const Polygon& part
) const {
    // JavaScript: for(j=0; j<finalNfp.length; j++) {
    //               nf = finalNfp[j];
    //               for(k=0; k<nf.length; k++) {
    //                 shiftvector = { x: nf[k].x-part[0].x, y: nf[k].y-part[0].y, ... };
    //                 ...
    //               }
    //             }
    std::vector<Point> positions;

    // CRITICAL FIX: Get the part's reference point using points[0] NOT bbox.min!
    // JavaScript code: shiftvector = { x: nf[k].x-part[0].x, y: nf[k].y-part[0].y }
    // NFPCalculator translates NFP by part.points[0], so we MUST subtract the same
    if (part.points.empty()) {
        return positions;
    }

    // Use part.points[0] as reference point to match NFP translation
    Point referencePoint(part.points[0].x, part.points[0].y);

    for (const auto& nfp : finalNfp) {
        // Skip very small NFPs
        if (std::abs(GeometryUtil::polygonArea(nfp.points)) < 2.0) {
            continue;
        }

        for (const auto& point : nfp.points) {
            // Subtract part's reference point (bbox min) to get actual placement position
            // After normalization, bbox min is (0,0), so this just returns point
            Point candidatePos(
                point.x - referencePoint.x,
                point.y - referencePoint.y
            );
            positions.push_back(candidatePos);
        }
    }

    return positions;
}

PlacedPart PlacementWorker::toPlacedPart(
    const Polygon& polygon,
    const Placement& placement
) const {
    PlacedPart placed;
    placed.polygon = polygon;
    placed.position = placement.position;
    placed.rotation = placement.rotation;
    placed.id = placement.id;
    placed.source = placement.source;
    return placed;
}

double PlacementWorker::calculateTotalMergedLength(
    const std::vector<std::vector<Placement>>& allPlacements,
    const std::vector<Polygon>& originalParts
) const {
    // This is a simplified version - full implementation would:
    // 1. For each sheet's placements
    // 2. Transform parts to their placed positions
    // 3. Call MergeDetection::calculateMergedLength
    // 4. Sum up total merged length

    double totalMerged = 0.0;

    // JavaScript: for each sheet in allplacements
    //   for each part in sheet
    //     transform part to placement position
    //     calculate merged length with all other placed parts
    //   totalMerged += sheet merged length

    // For now, return 0 - this would be expanded in full implementation
    // The complexity is that we need to track which original part each
    // placement refers to, transform it, and check for merges

    return totalMerged;
}

} // namespace deepnest
