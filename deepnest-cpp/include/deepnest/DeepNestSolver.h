#ifndef DEEPNEST_SOLVER_H
#define DEEPNEST_SOLVER_H

#include "engine/NestingEngine.h"
#include "config/DeepNestConfig.h"
#include "core/Polygon.h"
#include <memory>
#include <vector>
#include <functional>

namespace deepnest {

/**
 * @brief Part specification for nesting
 *
 * Represents a part to be nested with its quantity
 */
struct PartSpec {
    /**
     * @brief Polygon representing the part shape
     */
    Polygon polygon;

    /**
     * @brief Quantity of this part to nest
     */
    int quantity;

    /**
     * @brief Optional name for the part
     */
    std::string name;

    /**
     * @brief Constructor
     */
    PartSpec(const Polygon& poly, int qty, const std::string& partName = "")
        : polygon(poly)
        , quantity(qty)
        , name(partName)
    {}
};

/**
 * @brief Sheet specification for nesting
 *
 * Represents a sheet on which parts will be placed
 */
struct SheetSpec {
    /**
     * @brief Polygon representing the sheet shape
     */
    Polygon polygon;

    /**
     * @brief Quantity of sheets available
     */
    int quantity;

    /**
     * @brief Optional name for the sheet
     */
    std::string name;

    /**
     * @brief Constructor
     */
    SheetSpec(const Polygon& poly, int qty, const std::string& sheetName = "")
        : polygon(poly)
        , quantity(qty)
        , name(sheetName)
    {}
};

/**
 * @brief Main DeepNest solver interface
 *
 * This is the primary user-facing class for the DeepNest library.
 * It provides a simple, high-level interface for nesting optimization.
 *
 * Basic usage:
 * 1. Create solver with configuration
 * 2. Add parts and sheets
 * 3. Start nesting
 * 4. Monitor progress via callbacks
 * 5. Retrieve results when complete or stopped
 *
 * Example:
 * ```cpp
 * DeepNestSolver solver;
 *
 * // Configure
 * solver.setSpacing(5.0);
 * solver.setRotations(4);
 * solver.setPopulationSize(10);
 *
 * // Add parts
 * solver.addPart(partPolygon, 10);
 *
 * // Add sheet
 * solver.addSheet(sheetPolygon, 1);
 *
 * // Set callbacks
 * solver.setProgressCallback([](const NestProgress& progress) {
 *     std::cout << "Generation: " << progress.generation << std::endl;
 * });
 *
 * // Start nesting
 * solver.start(100); // 100 generations
 *
 * // Run nesting loop (in main thread or timer)
 * while (solver.isRunning()) {
 *     solver.step();
 *     std::this_thread::sleep_for(std::chrono::milliseconds(100));
 * }
 *
 * // Get best result
 * auto bestResult = solver.getBestResult();
 * ```
 */
class DeepNestSolver {
public:
    /**
     * @brief Default constructor
     *
     * Creates solver with default configuration.
     */
    DeepNestSolver();

    /**
     * @brief Constructor with custom configuration
     *
     * @param config Configuration to use
     */
    explicit DeepNestSolver(const DeepNestConfig& config);

    /**
     * @brief Destructor
     */
    ~DeepNestSolver();

    // Configuration methods

    /**
     * @brief Set spacing between parts (in units)
     *
     * @param spacing Spacing amount (default: 0)
     */
    void setSpacing(double spacing);

    /**
     * @brief Set number of rotation angles to try
     *
     * @param rotations Number of rotations (0 = no rotation, 4 = 90° increments)
     */
    void setRotations(int rotations);

    /**
     * @brief Set population size for genetic algorithm
     *
     * @param size Population size (default: 10)
     */
    void setPopulationSize(int size);

    /**
     * @brief Set mutation rate for genetic algorithm
     *
     * @param rate Mutation rate percentage (0-100, default: 10)
     */
    void setMutationRate(int rate);

    /**
     * @brief Set number of parallel threads
     *
     * @param threads Number of threads (0 = auto-detect)
     */
    void setThreads(int threads);

    /**
     * @brief Set placement type
     *
     * @param type Placement type: "gravity", "box", or "convexhull"
     */
    void setPlacementType(const std::string& type);

    /**
     * @brief Enable/disable merge lines optimization
     *
     * @param enable True to enable merge lines detection
     */
    void setMergeLines(bool enable);

    /**
     * @brief Set curve tolerance for polygon simplification
     *
     * @param tolerance Tolerance value (default: 0.3)
     */
    void setCurveTolerance(double tolerance);

    /**
     * @brief Enable/disable polygon simplification
     *
     * @param enable True to enable simplification
     */
    void setSimplify(bool enable);

    /**
     * @brief Get current configuration
     *
     * @return Reference to configuration
     */
    const DeepNestConfig& getConfig() const;

    // Part and sheet management

    /**
     * @brief Add a part to nest
     *
     * @param polygon Part shape
     * @param quantity Number of copies to nest
     * @param name Optional part name
     */
    void addPart(const Polygon& polygon, int quantity = 1, const std::string& name = "");

    /**
     * @brief Add a sheet for nesting
     *
     * @param polygon Sheet shape
     * @param quantity Number of sheets available
     * @param name Optional sheet name
     */
    void addSheet(const Polygon& polygon, int quantity = 1, const std::string& name = "");

    /**
     * @brief Clear all parts
     */
    void clearParts();

    /**
     * @brief Clear all sheets
     */
    void clearSheets();

    /**
     * @brief Clear both parts and sheets
     */
    void clear();

    /**
     * @brief Get number of parts added
     *
     * @return Number of part specifications (not total quantity)
     */
    size_t getPartCount() const;

    /**
     * @brief Get number of sheets added
     *
     * @return Number of sheet specifications (not total quantity)
     */
    size_t getSheetCount() const;

    // Nesting control

    /**
     * @brief Start nesting process
     *
     * Initializes the nesting engine and begins optimization.
     *
     * @param maxGenerations Maximum generations to run (0 = unlimited)
     * @throws std::runtime_error if no parts or sheets added
     */
    void start(int maxGenerations = 0);

    /**
     * @brief Stop nesting process
     *
     * Gracefully stops nesting after current evaluations complete.
     */
    void stop();

    /**
     * @brief Perform one step of nesting
     *
     * This should be called periodically (e.g., from a timer or main loop).
     * Returns false when nesting is complete or stopped.
     *
     * @return True if nesting is still running, false if complete/stopped
     */
    bool step();

    /**
     * @brief Check if nesting is currently running
     *
     * @return True if running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Run nesting synchronously until complete
     *
     * Blocks until nesting completes or is stopped.
     * Calls step() in a loop with small delays.
     *
     * @param maxGenerations Maximum generations to run
     * @param stepDelayMs Delay between steps in milliseconds (default: 100)
     */
    void runUntilComplete(int maxGenerations = 0, int stepDelayMs = 100);

    // Results

    /**
     * @brief Get current progress information
     *
     * @return Progress information
     */
    NestProgress getProgress() const;

    /**
     * @brief Get best result found so far
     *
     * @return Pointer to best result, or nullptr if no results yet
     */
    const NestResult* getBestResult() const;

    /**
     * @brief Get all saved results
     *
     * @return Vector of results, sorted by fitness (best first)
     */
    const std::vector<NestResult>& getResults() const;

    // Callbacks

    /**
     * @brief Set progress callback
     *
     * Called periodically during nesting to report progress.
     *
     * @param callback Progress callback function
     */
    void setProgressCallback(NestingEngine::ProgressCallback callback);

    /**
     * @brief Set result callback
     *
     * Called when a better result is found.
     *
     * @param callback Result callback function
     */
    void setResultCallback(NestingEngine::ResultCallback callback);

private:
    /**
     * @brief Configuration (singleton reference)
     */
    DeepNestConfig& config_;

    /**
     * @brief Nesting engine (created when start() is called)
     */
    std::unique_ptr<NestingEngine> engine_;

    /**
     * @brief Parts to nest
     */
    std::vector<PartSpec> parts_;

    /**
     * @brief Sheets for nesting
     */
    std::vector<SheetSpec> sheets_;

    /**
     * @brief Progress callback
     */
    NestingEngine::ProgressCallback progressCallback_;

    /**
     * @brief Result callback
     */
    NestingEngine::ResultCallback resultCallback_;

    /**
     * @brief Running flag
     */
    bool running_;
};

} // namespace deepnest

#endif // DEEPNEST_SOLVER_H
