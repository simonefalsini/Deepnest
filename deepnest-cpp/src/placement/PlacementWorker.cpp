#include "../../include/deepnest/placement/PlacementWorker.h"
#include "../../include/deepnest/geometry/GeometryUtil.h"
#include "../../include/deepnest/geometry/Transformation.h"
#include "../../include/deepnest/geometry/PolygonOperations.h"
#include "../../include/deepnest/placement/MergeDetection.h"
#include <clipper2/clipper.h>
#include <algorithm>
#include <limits>
#include <cmath>
#include <map>

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
#ifdef PLACEMENTDEBUG
    std::cout << "\n=== PLACEMENT START ===" << std::endl;
    std::cout << "Number of parts to place: " << parts.size() << std::endl;
    std::cout << "Number of sheets: " << sheets.size() << std::endl;
    std::cout.flush();
#endif
    
    // Optimize: Rotate in-place instead of creating new vector
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

#ifdef PLACEMENTDEBUG
        // Debug log for first part
        if (idx == 0) {
            std::cout << "\n=== ROTATION DEBUG: First part ===" << std::endl;
            std::cout << "  Original rotation: " << part.rotation << std::endl;
            std::cout << "  Original points[0]: (" << part.points[0].x << ", " << part.points[0].y << ")" << std::endl;
            std::cout << "  Rotated points[0]: (" << rotated.points[0].x << ", " << rotated.points[0].y << ")" << std::endl;
            std::cout.flush();
        }
#endif

        parts[idx] = rotated;
    }

    // JavaScript: var allplacements = [];
    //             var fitness = 0;
    std::vector<std::vector<Placement>> allPlacements;
    allPlacements.reserve(sheets.size()); // Reserve for expected number of sheets
    
    double fitness = 0.0;
    double totalSheetArea = 0.0;

    // JavaScript: while(parts.length > 0)
    int sheetIndex = 0;
    while (!parts.empty() && !sheets.empty()) {
        sheetIndex++;
#ifdef PLACEMENTDEBUG        
        std::cout << "Processing sheet " << sheetIndex << ", remaining parts: " << parts.size() << ", remaining sheets: " << sheets.size() << std::endl;

#endif
        // JavaScript: var placed = [];
        //             var placements = [];
        std::vector<Polygon> placed;
        placed.reserve(parts.size()); // Reserve max possible
        
        std::vector<Placement> placements;
        placements.reserve(parts.size()); // Reserve max possible

        // JavaScript background.js:1142: fitness += (minwidth/binarea) + minarea
        double minarea_accumulator = 0.0;

        // JavaScript: var sheet = sheets.shift();
        //             var sheetarea = Math.abs(GeometryUtil.polygonArea(sheet));
        //             totalsheetarea += sheetarea;
        //             fitness += sheetarea;
        // NOTE: Don't remove sheet yet - only remove if we actually place parts on it
        Polygon sheet = sheets.front();

        double sheetArea = std::abs(GeometryUtil::polygonArea(sheet.points));
        totalSheetArea += sheetArea;
        // JavaScript: fitness += sheetarea;
        fitness += sheetArea;

        // JavaScript: for(i=0; i<parts.length; i++)
        for (size_t i = 0; i < parts.size(); ) {
            Polygon& part = parts[i];
#ifdef PLACEMENTDEBUG
            std::cerr << "\n=== PLACEMENT LOOP ITERATION ===" << std::endl;
            std::cerr << "  Iteration i=" << i << ", parts.size()=" << parts.size()
                      << ", placed.size()=" << placed.size() << std::endl;
            std::cerr << "  Current part: id=" << part.id << ", source=" << part.source
                      << ", rotation=" << part.rotation << std::endl;
            std::cerr.flush();
#endif
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

            Polygon basePart = parts[i];
            double rotationStep = (config_.rotations > 0) ? (360.0 / config_.rotations) : 0.0;

            for (int rotAttempt = 0; rotAttempt < maxRotationAttempts; rotAttempt++) {
                // Debug logging for first part's first rotation attempt
                if (placements.empty() && rotAttempt == 0) {
#ifdef PLACEMENTDEBUG
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
 #endif 
                }

                try {
                    auto innerNfps = nfpCalculator_.getInnerNFP(sheet, part);
                    if (!innerNfps.empty()) {
                        innerNfp = innerNfps[0]; // Use first NFP polygon
                    }
                } catch (const std::exception& e) {
                    std::cerr << "CRITICAL ERROR: getInnerNFP failed: " << e.what() << std::endl;
                    continue;
                } catch (...) {
                    std::cerr << "CRITICAL ERROR: getInnerNFP failed with unknown exception" << std::endl;
                    continue;
                }

                if (!innerNfp.points.empty()) {
                    foundValidRotation = true;
                    break;
                }

                // Try next rotation
                if (rotAttempt < maxRotationAttempts - 1 && config_.rotations > 0) {
                    double totalRotation = rotationStep * (rotAttempt + 1);
                    Polygon rotated = basePart.rotate(totalRotation);

                    std::vector<Point> cleanedPoints = PolygonOperations::cleanPolygon(rotated.points);
                    if (!cleanedPoints.empty()) {
                        rotated.points = cleanedPoints;
                    } else {
                        // If cleaning fails (degenerate), skip this rotation
                        continue;
                    }

                    rotated.rotation = basePart.rotation + totalRotation;
                    rotated.source = basePart.source;
                    rotated.id = basePart.id;

                    if (rotated.rotation >= 360.0) {
                        rotated.rotation = std::fmod(rotated.rotation, 360.0);
                    }

                    // Update the part in the vector (and the reference 'part')
                    parts[i] = rotated;
                    // Note: 'part' is a reference to parts[i], so it sees the update
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

                Point partRef(part.points[0].x, part.points[0].y);

                // DEBUG LOGGING for first placement
                if (placements.empty()) {
#ifdef PLACEMENTDEBUG
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
#endif 
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
#ifdef PLACEMENTDEBUG
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

                    std::cerr << "  Creating Placement object..." << std::endl;
                    std::cerr << "    bestPos: (" << bestPos.x << ", " << bestPos.y << ")" << std::endl;
                    std::cerr << "    part.id: " << part.id << ", part.source: " << part.source
                              << ", part.rotation: " << part.rotation << std::endl;
                    std::cerr.flush();
#endif
                    position = Placement(bestPos, part.id, part.source, part.rotation);
#ifdef PLACEMENTDEBUG
                    std::cerr << "  Placement object created, pushing to vector..." << std::endl;
                    std::cerr.flush();
#endif
                    placements.push_back(position);
                    placed.push_back(part);
#ifdef PLACEMENTDEBUG
                    std::cerr << "  FIRST PLACEMENT COMPLETE ===" << std::endl;
                    std::cerr << "    Total placed: " << placed.size() << std::endl;
                    std::cerr << "    Parts remaining before erase: " << parts.size() << std::endl;
#endif
                    // Remove from parts list
                    parts.erase(parts.begin() + i);
#ifdef PLACEMENTDEBUG
                    std::cerr << "    Parts remaining after erase: " << parts.size() << std::endl;
                    std::cerr << "    About to continue loop..." << std::endl;
                    std::cerr.flush();

                    // Don't increment i, since we removed this element
#endif
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
#ifdef PLACEMENTDEBUG
            std::cerr << "=== PLACEMENT DEBUG: Computing outer NFPs ===" << std::endl;
            std::cerr << "  Number of already placed parts: " << placed.size() << std::endl;
#endif
            // JavaScript: for(j=startindex; j<placed.length; j++)
            for (size_t j = 0; j < placed.size(); j++) {
#ifdef PLACEMENTDEBUG
                std::cerr << "  Computing outer NFP for placed[" << j << "] (id=" << placed[j].id
                          << ") vs current part (id=" << part.id << ")" << std::endl;
#endif
                // JavaScript: nfp = getOuterNfp(placed[j], part);
                Polygon outerNfp;
                try {
                    outerNfp = nfpCalculator_.getOuterNFP(placed[j], part, false);
                } catch (const std::exception& e) {
                    std::cerr << "CRITICAL ERROR: getOuterNFP failed for placed part " << placed[j].id 
                              << " vs " << part.id << ": " << e.what() << std::endl;
                    error = true;
                    break;
                } catch (...) {
                    std::cerr << "CRITICAL ERROR: getOuterNFP failed with unknown exception" << std::endl;
                    error = true;
                    break;
                }

#ifdef PLACEMENTDEBUG
                std::cerr << "    Result: " << (outerNfp.points.empty() ? "EMPTY" : std::to_string(outerNfp.points.size()) + " points") << std::endl;
#endif
                if (outerNfp.points.empty()) {
                    std::cerr << "ERROR: getOuterNFP returned empty polygon" << std::endl;
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
#ifdef PLACEMENTDEBUG
            std::cerr << "  Successfully collected " << outerNfps.size() << " outer NFPs" << std::endl;
#endif
            if (error) {
#ifdef PLACEMENTDEBUG
                std::cerr << "  ERROR: One or more outer NFPs were empty, skipping this part" << std::endl;
#endif
                i++;
                continue;
            }

            // JavaScript: clipper.Execute(ClipperLib.ClipType.ctUnion, combinedNfp, ...)
            // Union all outer NFPs
            std::vector<std::vector<Point>> combinedNfpPoints;
            if (!outerNfps.empty()) {
#ifdef PLACEMENTDEBUG
                std::cerr << "=== PLACEMENT DEBUG: Calling unionPolygons ===" << std::endl;
                std::cerr << "  Number of polygons to union: " << outerNfps.size() << std::endl;

#endif
                // Convert Polygons to point vectors for union
                std::vector<std::vector<Point>> outerNfpPoints;
                for (const auto& nfp : outerNfps) {
#ifdef PLACEMENTDEBUG
                    std::cerr << "    Polygon: " << nfp.points.size() << " points" << std::endl;
#endif
                    outerNfpPoints.push_back(nfp.points);
                }

#ifdef PLACEMENTDEBUG
                std::cerr << "  Calling PolygonOperations::unionPolygons..." << std::endl;
#endif
                combinedNfpPoints = PolygonOperations::unionPolygons(outerNfpPoints);
#ifdef PLACEMENTDEBUG
                std::cerr << "  unionPolygons completed successfully! Result: " << combinedNfpPoints.size() << " polygon(s)" << std::endl;
#endif
            }

            // JavaScript: var finalNfp = new ClipperLib.Paths();
            //             clipper = new ClipperLib.Clipper();
            //             clipper.AddPaths(combinedNfp, ClipperLib.PolyType.ptClip, true);
            //             clipper.AddPaths(clipperSheetNfp, ClipperLib.PolyType.ptSubject, true);
            //             if(!clipper.Execute(ClipperLib.ClipType.ctDifference, finalNfp, ...))
            // Difference: innerNfp - combinedNfp
            std::vector<Polygon> finalNfp;

            if (combinedNfpPoints.empty()) {
#ifdef PLACEMENTDEBUG
                std::cerr << "=== PLACEMENT DEBUG: No combined NFPs ===" << std::endl;
                std::cerr << "  Using innerNfp directly (no collisions)" << std::endl;
#endif
                // No outer NFPs, just use inner NFP
                finalNfp.push_back(innerNfp);
            }
            else {
#ifdef PLACEMENTDEBUG
                std::cerr << "=== PLACEMENT DEBUG: Performing difference operation ===" << std::endl;
                std::cerr << "  innerNfp points: " << innerNfp.points.size() << std::endl;
                std::cerr << "  combinedNfpPoints polygons: " << combinedNfpPoints.size() << std::endl;
#endif
                // Perform difference operation
                // Convert combined NFP to Polygon for difference
                Polygon combinedNfpPoly;
                if (!combinedNfpPoints.empty()) {
                    combinedNfpPoly.points = combinedNfpPoints[0];
#ifdef PLACEMENTDEBUG
                    std::cerr << "  combinedNfpPoly[0] points: " << combinedNfpPoly.points.size() << std::endl;
#endif
                    // Add remaining as children (holes) if any
                    for (size_t idx = 1; idx < combinedNfpPoints.size(); idx++) {
                        Polygon child;
                        child.points = combinedNfpPoints[idx];
                        combinedNfpPoly.children.push_back(child);
#ifdef PLACEMENTDEBUG
                        std::cerr << "  combinedNfpPoly child[" << (idx-1) << "] points: " << child.points.size() << std::endl;
#endif
                    }
                }
#ifdef PLACEMENTDEBUG
                std::cerr << "  Calling PolygonOperations::differencePolygons..." << std::endl;
#endif
                std::vector<std::vector<Point>> differenceResult =
                    PolygonOperations::differencePolygons(innerNfp.points, combinedNfpPoly.points);
#ifdef PLACEMENTDEBUG
                std::cerr << "  differencePolygons completed successfully! Result: " << differenceResult.size() << " polygon(s)" << std::endl;
#endif
                // Convert result back to Polygons
                for (const auto& pointVec : differenceResult) {
                    Polygon poly;
                    poly.points = pointVec;
                    finalNfp.push_back(poly);
                }
            }

            // JavaScript: if(!finalNfp || finalNfp.length == 0) { continue; }
            if (finalNfp.empty()) {
                //std::cerr << "  WARNING: finalNfp is empty after difference, skipping part" << std::endl;
                i++;
                continue;
            }
#ifdef PLACEMENTDEBUG
            std::cerr << "=== PLACEMENT DEBUG: finalNfp computed successfully ===" << std::endl;
            std::cerr << "  Number of NFP polygons: " << finalNfp.size() << std::endl;
#endif
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

            // Use strategy to find best position and get area metric
            // LINE MERGE INTEGRATION: Pass config to enable line merge bonus calculation
            BestPositionResult positionResult = strategy_->findBestPosition(
                part,
                placedForStrategy,
                candidatePositions,
                config_
            );

            // JavaScript: background.js:1210-1241
            // Perform overlap check before accepting position
            bool hasOverlap = false;
            
            if (!candidatePositions.empty()) {
                // Check if new part at this position overlaps with any placed part
                Point testPosition = positionResult.position;
                
                for (size_t m = 0; m < placed.size(); m++) {
                    const Polygon& placedPart = placed[m];
                    const Placement& placedPosition = placements[m];
                    
                    Point placedPos(placedPosition.position.x, placedPosition.position.y);
                    
                    if (hasSignificantOverlap(part, testPosition, placedPart, placedPos, config_)) {
                        hasOverlap = true;
#ifdef PLACEMENTDEBUG
                        std::cout << "  Part " << part.id << " overlaps with placed part " 
                                  << placedPart.id << " at position (" 
                                  << testPosition.x << ", " << testPosition.y << ")" << std::endl;
#endif
                        break;
                    }
                }
            }

            // JavaScript: if(position) { placed.push(path); placements.push(position); }
            // Only accept position if no significant overlap
            if (!candidatePositions.empty() && !hasOverlap) {
                position = Placement(positionResult.position, part.id, part.source, part.rotation);
                placements.push_back(position);
                placed.push_back(part);

                minarea_accumulator += positionResult.area;

                // Remove from parts list
                parts.erase(parts.begin() + i);
                // Don't increment i
            }
            else {
                // No valid positions found or overlap detected, skip this part
#ifdef PLACEMENTDEBUG
                if (hasOverlap) {
                    std::cout << "  Part " << part.id << " skipped due to overlap" << std::endl;
                } else {
                    std::cout << "  Part " << part.id << " skipped - no valid positions" << std::endl;
                }
#endif
                i++;
            }
        }

        // JavaScript: if(placements && placements.length > 0) {
        //               allplacements.push(placements);
        //             }
        if (!placements.empty()) {
            // Sheet was used - remove it from the list
            sheets.erase(sheets.begin());
            
            // Calculate bounds fitness (minwidth/binarea)
            // JavaScript: if(minwidth) { fitness += minwidth/binarea; }
            std::vector<Point> allPlacedPoints;

            // Collect all points from placed parts with their placements applied
            for (size_t i = 0; i < placed.size(); i++) {
                const Polygon& placedPart = placed[i];
                const Placement& placement = placements[i];

                // Transform each point by the placement position
                for (const auto& point : placedPart.points) {
                    allPlacedPoints.push_back(Point(
                        point.x + placement.position.x,
                        point.y + placement.position.y
                    ));
                }
            }

            // Calculate bounding box and add width/area to fitness
            if (!allPlacedPoints.empty()) {
                BoundingBox bounds = BoundingBox::fromPoints(allPlacedPoints);
                double boundsWidth = bounds.width;

                // JavaScript background.js:1142: fitness += (minwidth/binarea) + minarea
                fitness += (boundsWidth / sheetArea) + minarea_accumulator;
            }

            allPlacements.push_back(placements);
        }
        else {
            // No parts placed on this sheet
            // This can happen if:
            // 1. Parts don't fit geometrically
            // 2. All remaining parts have overlap issues
            // 3. Sheet is too small for any remaining part
            
            // Remove this sheet and try the next one
            sheets.erase(sheets.begin());
            
            // If no more sheets available, break
            if (sheets.empty()) {
                break;
            }
            // Otherwise continue to next sheet
        }
    }

    // JavaScript: fitness += 100000000*(Math.abs(GeometryUtil.polygonArea(parts[i]))/totalsheetarea);
    double totalSheetAreaSafe = std::max(totalSheetArea, 1.0); // Avoid division by zero
    for (const auto& part : parts) {
        double partArea = std::abs(GeometryUtil::polygonArea(part.points));
        fitness += 100000000.0 * (partArea / totalSheetAreaSafe);
    }

    // Calculate total merged length
    // JavaScript: totalMerged = ... (calculated in separate loop)
    if (config_.mergeLines) {
        totalMerged = calculateTotalMergedLength(allPlacements, parts);
        // JavaScript: fitness -= totalMerged * config.mergeLines;
        // Note: config.mergeLines is a boolean in C++, but in JS it seemed to be used as a weight?
        // Checking JS: if(config.mergeLines) { ... fitness -= merged * config.mergeLines; }
        // Wait, if mergeLines is boolean true (1), it subtracts length.
        // If it's a weight, it should be a double.
        // Let's assume it's a boolean flag enabling the feature, and we subtract the length directly
        // or use a weight if defined.
        // Looking at DeepNestConfig, mergeLines is likely a boolean.
        // If so, we subtract totalMerged.
        fitness -= totalMerged;
    }

    // GA DEBUG: Log fitness calculation with CORRECTED formulas (FIX 1.1, 1.2, 1.3)
    static int placementCount = 0;
    if (placementCount < 3) {  // Log first 3 placements only
#ifdef PLACEMENTDEBUG
        std::cout << "\n=== PLACEMENT RESULT #" << placementCount << " (CORRECTED FITNESS) ===" << std::endl;
        std::cout << "  Sheets used: " << allPlacements.size() << std::endl;
        std::cout << "  Total sheet area: " << totalSheetArea << std::endl;
        std::cout << "  Unplaced parts: " << parts.size() << std::endl;
#endif
        // Calculate fitness breakdown with CORRECTED formulas
        double sheetAreaPenalty = 0.0;
        for (const auto& placements : allPlacements) {
            // Each sheet contributes its full area (FIX 1.1)
            sheetAreaPenalty += totalSheetArea / allPlacements.size(); // Approximate
        }

        double unplacedPenalty = 0.0;
        for (const auto& part : parts) {
            double partArea = std::abs(GeometryUtil::polygonArea(part.points));
            unplacedPenalty += 100000000.0 * (partArea / totalSheetAreaSafe);
        }

        // Remaining is bounds+minarea component
        double boundsAndMinarea = fitness - sheetAreaPenalty - unplacedPenalty;
#ifdef PLACEMENTDEBUG
        std::cout << "  FINAL FITNESS: " << fitness << std::endl;
        std::cout << "    = sheet area penalty (" << sheetAreaPenalty << ")"
                  << " + bounds+minarea (" << boundsAndMinarea << ")"
                  << " + unplaced penalty (" << unplacedPenalty << ")" << std::endl;
        std::cout << "  [NOTE: Should be >> 1.0 if fixes working. Typical range: 100k-10M]" << std::endl;
        std::cout.flush();
#endif
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
    double totalMerged = 0.0;

    // Map part ID to Polygon for quick lookup
    std::map<int, Polygon> partMap;
    for (const auto& part : originalParts) {
        partMap[part.id] = part;
    }

    for (const auto& sheetPlacements : allPlacements) {
        std::vector<Polygon> placedPolygons;
        
        for (const auto& placement : sheetPlacements) {
            auto it = partMap.find(placement.id);
            if (it != partMap.end()) {
                // Create a copy of the part to transform
                Polygon placedPart = it->second;
                
                // Rotate (if not already rotated in originalParts, but originalParts here ARE rotated)
                // In placeParts, we passed 'rotatedParts' to this function.
                // So the parts in partMap are already rotated to their placement rotation?
                // Let's check placeParts:
                // "parts = rotatedParts;" -> yes, 'parts' passed to this function are the rotated ones.
                // However, 'allPlacements' contains the rotation used.
                // If 'originalParts' are already rotated, we just need to translate.
                
                // Wait, 'rotatedParts' in placeParts has the INITIAL rotation.
                // But during placement, parts might be rotated further?
                // No, in the current logic:
                // "parts[i] = rotated;" updates the part in the list.
                // And "placed.push_back(part);" pushes the rotated part.
                // So 'originalParts' passed here (which is 'rotatedParts' from placeParts) 
                // contains the parts AS THEY WERE PLACED (rotation-wise).
                
                // Translate to placed position
                for (auto& p : placedPart.points) {
                    p.x += placement.position.x;
                    p.y += placement.position.y;
                }
                
                // Also translate children
                for (auto& child : placedPart.children) {
                    for (auto& p : child.points) {
                        p.x += placement.position.x;
                        p.y += placement.position.y;
                    }
                }
                
                // Calculate merged length with already placed parts on this sheet
                // We check the new part against all previously placed parts
                MergeDetection::MergeResult result = MergeDetection::calculateMergedLength(
                    placedPolygons,
                    placedPart,
                    0.1, // minLength (arbitrary small value)
                    config_.curveTolerance // tolerance
                );
                
                totalMerged += result.totalLength;
                
                // Add to placed polygons for next iterations
                placedPolygons.push_back(placedPart);
            }
        }
    }

    return totalMerged;
}

bool PlacementWorker::hasSignificantOverlap(
    const Polygon& partA,
    const Point& positionA,
    const Polygon& partB,
    const Point& positionB,
    const DeepNestConfig& config
) const {
    // JavaScript: background.js:1210-1241
    // Perform overlap detection using Clipper2 intersection
    
    using namespace Clipper2Lib;
    
    // Quick bounding box check first (optimization)
    // If bounding boxes don't overlap, parts definitely don't overlap
    Polygon translatedA = partA.translate(positionA.x, positionA.y);
    Polygon translatedB = partB.translate(positionB.x, positionB.y);
    
    BoundingBox bboxA = translatedA.bounds();
    BoundingBox bboxB = translatedB.bounds();
    
    // Check if bounding boxes intersect
    bool bboxesIntersect = !(
        bboxA.x + bboxA.width < bboxB.x ||
        bboxB.x + bboxB.width < bboxA.x ||
        bboxA.y + bboxA.height < bboxB.y ||
        bboxB.y + bboxB.height < bboxA.y
    );
    
    if (!bboxesIntersect) {
        return false;  // No overlap possible
    }

    // Convert to Clipper2 paths
    PathsD pathsA, pathsB;
    
    // Convert partA (already translated)
    PathD pathA;
    for (const auto& p : translatedA.points) {
        pathA.push_back(PointD(p.x, p.y));
    }
    pathsA.push_back(pathA);
    
    // Convert partB (already translated)
    PathD pathB;
    for (const auto& p : translatedB.points) {
        pathB.push_back(PointD(p.x, p.y));
    }
    pathsB.push_back(pathB);
    
    // Perform intersection to find overlap
    PathsD intersection = Intersect(pathsA, pathsB, FillRule::NonZero);
    
    if (intersection.empty()) {
        return false;  // No overlap
    }
    
    // Calculate intersection area
    double intersectionArea = 0.0;
    for (const auto& path : intersection) {
        intersectionArea += std::abs(Area(path));
    }
    
    // Compare with tolerance
    // JavaScript uses: intersectionArea > config.overlapTolerance * clipperScale * clipperScale
    // Since we're using PathsD (double precision), we compare directly without scaling
    return intersectionArea > config.overlapTolerance;
}

} // namespace deepnest
