#ifndef DEEPNEST_PLACEMENT_WORKER_H
#define DEEPNEST_PLACEMENT_WORKER_H

#include "../core/Polygon.h"
#include "../core/Point.h"
#include "../config/DeepNestConfig.h"
#include "../nfp/NFPCalculator.h"
#include "PlacementStrategy.h"
#include "MergeDetection.h"
#include <vector>
#include <memory>

namespace deepnest {

/**
 * @brief Worker class for placing parts on sheets
 *
 * This class handles the core placement logic, calculating where parts
 * should be positioned on sheets to minimize waste.
 *
 * Algorithm:
 * 1. Rotate parts according to their specified rotations
 * 2. For each sheet:
 *    - For each unplaced part:
 *      - Calculate inner NFP (part vs sheet boundary)
 *      - If first part: place at top-left corner
 *      - Otherwise:
 *        - Calculate outer NFPs with all placed parts
 *        - Union all outer NFPs
 *        - Difference with inner NFP to get valid placement region
 *        - Choose best position using placement strategy
 *    - Calculate fitness based on area utilization
 * 3. Calculate merged lines for cutting optimization
 * 4. Return placements with fitness metrics
 *
 * References:
 * - placementworker.js (complete implementation)
 * - background.js: placeParts function (lines 804-1090)
 */
class PlacementWorker {
public:
    /**
     * @brief Structure representing a single placement
     */
    struct Placement {
        Point position;      // Translation offset
        int id;              // Part ID
        int source;          // Source ID
        double rotation;     // Rotation in degrees

        Placement() : id(-1), source(-1), rotation(0.0) {}

        Placement(const Point& pos, int partId, int src, double rot)
            : position(pos), id(partId), source(src), rotation(rot) {}
    };

    /**
     * @brief Result structure for placement operation
     */
    struct PlacementResult {
        /**
         * @brief Placements organized by sheet
         * Each vector<Placement> represents all parts placed on one sheet
         */
        std::vector<std::vector<Placement>> placements;

        /**
         * @brief Fitness score (lower is better)
         * Calculated as: total_sheet_area + parts_unplaced*penalty
         */
        double fitness;

        /**
         * @brief Total sheet area used
         */
        double area;

        /**
         * @brief Total length of merged/aligned lines
         * Used for cutting optimization
         */
        double mergedLength;

        /**
         * @brief Parts that couldn't be placed
         */
        std::vector<Polygon> unplacedParts;

        PlacementResult()
            : fitness(0.0), area(0.0), mergedLength(0.0) {}
    };

    /**
     * @brief Constructor
     *
     * @param config Configuration settings
     * @param calculator NFP calculator instance
     */
    PlacementWorker(const DeepNestConfig& config, NFPCalculator& calculator);

    /**
     * @brief Place parts on sheets
     *
     * Main placement function that distributes parts across sheets,
     * minimizing waste and optimizing for the configured objective.
     *
     * @param sheets Available sheets/bins (will be consumed)
     * @param parts Parts to place (must have rotation field set)
     * @return PlacementResult with placements and metrics
     *
     * References:
     * - background.js line 804: function placeParts(sheets, parts, config, nestindex)
     * - placementworker.js line 59: this.placePaths = function(paths)
     */
    PlacementResult placeParts(
        std::vector<Polygon> sheets,
        std::vector<Polygon> parts
    );

private:
    /**
     * @brief Configuration settings
     */
    const DeepNestConfig& config_;

    /**
     * @brief NFP calculator
     */
    NFPCalculator& nfpCalculator_;

    /**
     * @brief Placement strategy
     */
    std::unique_ptr<PlacementStrategy> strategy_;

    /**
     * @brief Find best position for a part on current sheet
     *
     * Uses the placement strategy to evaluate candidate positions
     * and select the one that minimizes the objective function.
     *
     * @param part Part to place
     * @param placed Already placed parts
     * @param placements Positions of placed parts
     * @param candidatePositions Valid positions from NFP
     * @return Best placement, or null Placement if none found
     */
    Placement findBestPlacement(
        const Polygon& part,
        const std::vector<Polygon>& placed,
        const std::vector<Placement>& placements,
        const std::vector<Point>& candidatePositions
    ) const;

    /**
     * @brief Extract all points from final NFP polygons
     *
     * Flattens the NFP result polygons into a list of candidate positions.
     * Subtracts part[0] from each NFP point to get the actual placement position.
     *
     * @param finalNfp NFP polygons after difference operation
     * @param part Part being placed (needed to subtract part[0])
     * @return List of candidate placement positions
     *
     * References:
     * - placementworker.js line 235: shiftvector = {x: nf[k].x-part[0].x, y: nf[k].y-part[0].y}
     */
    std::vector<Point> extractCandidatePositions(
        const std::vector<Polygon>& finalNfp,
        const Polygon& part
    ) const;

    /**
     * @brief Calculate merged length for all placed parts
     *
     * Detects aligned edges between parts to optimize cutting operations.
     *
     * @param allPlacements All placements across all sheets
     * @param originalParts Original parts (before rotation)
     * @return Total merged line length
     */
    double calculateTotalMergedLength(
        const std::vector<std::vector<Placement>>& allPlacements,
        const std::vector<Polygon>& originalParts
    ) const;

    /**
     * @brief Convert a polygon to placed part representation
     *
     * @param polygon The polygon
     * @param placement Its placement
     * @return PlacedPart structure for strategy evaluation
     */
    PlacedPart toPlacedPart(
        const Polygon& polygon,
        const Placement& placement
    ) const;
};

} // namespace deepnest

#endif // DEEPNEST_PLACEMENT_WORKER_H
