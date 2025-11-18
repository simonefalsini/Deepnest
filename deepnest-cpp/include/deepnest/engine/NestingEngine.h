#ifndef DEEPNEST_NESTING_ENGINE_H
#define DEEPNEST_NESTING_ENGINE_H

#include "../algorithm/GeneticAlgorithm.h"
#include "../algorithm/Population.h"
#include "../placement/PlacementWorker.h"
#include "../parallel/ParallelProcessor.h"
#include "../nfp/NFPCalculator.h"
#include "../nfp/NFPCache.h"
#include "../config/DeepNestConfig.h"
#include "../core/Polygon.h"
#include <vector>
#include <memory>
#include <functional>

namespace deepnest {

/**
 * @brief Result of a nesting operation
 *
 * Contains all placements for a complete nest (across all sheets)
 */
struct NestResult {
    /**
     * @brief All placements organized by sheet
     */
    std::vector<std::vector<PlacementWorker::Placement>> placements;

    /**
     * @brief Total fitness value (lower is better)
     */
    double fitness;

    /**
     * @brief Total area used
     */
    double area;

    /**
     * @brief Total merged length (for efficiency bonus)
     */
    double mergedLength;

    /**
     * @brief Generation when this result was found
     */
    int generation;

    /**
     * @brief Individual index in population
     */
    int individualIndex;
};

/**
 * @brief Progress information during nesting
 */
struct NestProgress {
    /**
     * @brief Current generation number
     */
    int generation;

    /**
     * @brief Total number of individuals evaluated
     */
    int evaluationsCompleted;

    /**
     * @brief Best fitness found so far
     */
    double bestFitness;

    /**
     * @brief Percentage complete (0-100)
     * Based on generation / max generations
     */
    double percentComplete;
};

/**
 * @brief Main nesting engine that coordinates the entire nesting process
 *
 * The NestingEngine is the central coordinator that manages:
 * 1. Genetic Algorithm for optimization
 * 2. Parallel processing of individuals
 * 3. Placement workers for fitness evaluation
 * 4. NFP calculation and caching
 * 5. Progress tracking and callbacks
 *
 * Process flow:
 * 1. Initialize with parts and sheets
 * 2. Create initial GA population (adam sorted by area)
 * 3. For each generation:
 *    - Evaluate unevaluated individuals in parallel
 *    - Each evaluation uses PlacementWorker to place parts on sheets
 *    - Track best results
 *    - When generation complete, create next generation
 * 4. Continue until max generations or user stops
 *
 * References:
 * - deepnest.js: start() function (line 943-1016)
 * - deepnest.js: launchWorkers() function (line 1040-1153)
 */
class NestingEngine {
public:
    /**
     * @brief Callback type for progress updates
     *
     * Called periodically during nesting to report progress
     */
    using ProgressCallback = std::function<void(const NestProgress&)>;

    /**
     * @brief Callback type for new best results
     *
     * Called when a better nest is found
     */
    using ResultCallback = std::function<void(const NestResult&)>;

    /**
     * @brief Constructor
     *
     * @param config Configuration for nesting
     */
    explicit NestingEngine(const DeepNestConfig& config);

    /**
     * @brief Destructor
     */
    ~NestingEngine();

    /**
     * @brief Initialize nesting with parts and sheets
     *
     * Prepares the nesting process:
     * 1. Applies spacing offset to parts and sheets
     * 2. Creates list of parts to nest (with quantities)
     * 3. Sorts parts by area (largest first - "adam")
     * 4. Initializes genetic algorithm with adam
     *
     * @param parts List of parts to nest (non-sheet polygons)
     * @param quantities Quantity for each part
     * @param sheets List of available sheets
     * @param sheetQuantities Quantity for each sheet
     *
     * Corresponds to JavaScript start() function (line 943-1016)
     */
    void initialize(
        const std::vector<Polygon>& parts,
        const std::vector<int>& quantities,
        const std::vector<Polygon>& sheets,
        const std::vector<int>& sheetQuantities
    );

    /**
     * @brief Start the nesting process
     *
     * Begins asynchronous nesting. The engine will run in background threads
     * and call callbacks for progress and results.
     *
     * @param progressCallback Called periodically with progress updates (optional)
     * @param resultCallback Called when better results are found (optional)
     * @param maxGenerations Maximum number of generations to run (0 = unlimited)
     */
    void start(
        ProgressCallback progressCallback = nullptr,
        ResultCallback resultCallback = nullptr,
        int maxGenerations = 0
    );

    /**
     * @brief Stop the nesting process
     *
     * Gracefully stops nesting after current evaluations complete.
     */
    void stop();

    /**
     * @brief Perform one step of the nesting process
     *
     * This is the main work function, equivalent to JavaScript launchWorkers().
     * It should be called periodically (e.g., from a timer or in a loop).
     *
     * Process:
     * 1. Check if current generation is complete
     * 2. If complete, create next generation
     * 3. Launch parallel workers for unevaluated individuals
     * 4. Update progress and call callbacks
     *
     * @return True if nesting is still running, false if complete or stopped
     *
     * Corresponds to JavaScript launchWorkers() (line 1040-1153)
     */
    bool step();

    /**
     * @brief Check if nesting is currently running
     */
    bool isRunning() const;

    /**
     * @brief Get current progress information
     */
    NestProgress getProgress() const;

    /**
     * @brief Get best result found so far
     *
     * @return Pointer to best result, or nullptr if no results yet
     */
    const NestResult* getBestResult() const;

    /**
     * @brief Get all saved results (top N)
     *
     * Returns the best N results found during nesting.
     *
     * @return Vector of results, sorted by fitness (best first)
     */
    const std::vector<NestResult>& getResults() const;

    /**
     * @brief Get configuration
     */
    const DeepNestConfig& getConfig() const;

private:
    /**
     * @brief Apply spacing offset to a polygon
     *
     * @param polygon Polygon to offset
     * @param offset Offset amount (positive = expand, negative = shrink)
     * @return Offset polygon
     */
    Polygon applySpacing(const Polygon& polygon, double offset);

    /**
     * @brief Evaluate a single individual
     *
     * Uses PlacementWorker to place all parts according to the individual's
     * placement sequence and rotations.
     *
     * @param individual Individual to evaluate
     * @param parts Parts to nest (in adam order)
     * @param sheets Available sheets
     * @return Placement result with fitness
     */
    PlacementWorker::PlacementResult evaluateIndividual(
        Individual& individual,
        const std::vector<Polygon>& parts,
        const std::vector<Polygon>& sheets
    );

    /**
     * @brief Check if current generation is complete
     *
     * @return True if all individuals have valid fitness values
     */
    bool isGenerationComplete() const;

    /**
     * @brief Convert PlacementResult to NestResult
     *
     * @param result Placement result from worker
     * @param generation Current generation
     * @param individualIndex Index of individual
     * @return Nest result
     */
    NestResult toNestResult(
        const PlacementWorker::PlacementResult& result,
        int generation,
        int individualIndex
    ) const;

    /**
     * @brief Update saved results with new result
     *
     * Keeps top N results sorted by fitness.
     *
     * @param result New result to consider
     */
    void updateResults(const NestResult& result);

    /**
     * @brief Configuration
     */
    const DeepNestConfig& config_;

    /**
     * @brief NFP cache for all NFP operations
     */
    NFPCache nfpCache_;

    /**
     * @brief NFP calculator for all NFP operations
     */
    std::unique_ptr<NFPCalculator> nfpCalculator_;

    /**
     * @brief Placement worker for fitness evaluation
     */
    std::unique_ptr<PlacementWorker> placementWorker_;

    /**
     * @brief Parallel processor for concurrent evaluations
     */
    std::unique_ptr<ParallelProcessor> parallelProcessor_;

    /**
     * @brief Genetic algorithm
     */
    std::unique_ptr<GeneticAlgorithm> geneticAlgorithm_;

    /**
     * @brief Parts to nest (adam - sorted by area, with quantities expanded)
     */
    std::vector<Polygon> parts_;

    /**
     * @brief Pointers to parts (for GA)
     */
    std::vector<Polygon*> partPointers_;

    /**
     * @brief Available sheets (with quantities expanded)
     */
    std::vector<Polygon> sheets_;

    /**
     * @brief Running flag
     */
    bool running_;

    /**
     * @brief Maximum generations (0 = unlimited)
     */
    int maxGenerations_;

    /**
     * @brief Total evaluations completed
     */
    int evaluationsCompleted_;

    /**
     * @brief Progress callback
     */
    ProgressCallback progressCallback_;

    /**
     * @brief Result callback
     */
    ResultCallback resultCallback_;

    /**
     * @brief Saved results (top 10 by default)
     */
    std::vector<NestResult> results_;

    /**
     * @brief Maximum results to save
     */
    static const int MAX_SAVED_RESULTS = 10;
};

} // namespace deepnest

#endif // DEEPNEST_NESTING_ENGINE_H
