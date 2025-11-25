#include "../include/deepnest/DeepNestSolver.h"
#include "../include/deepnest/geometry/GeometryUtil.h"
#include "../include/deepnest/geometry/PolygonOperations.h"
#include <stdexcept>
#include <thread>
#include <chrono>

namespace deepnest {

DeepNestSolver::DeepNestSolver()
    : config_(DeepNestConfig::getInstance())
    , running_(false)
{
}

DeepNestSolver::DeepNestSolver(const DeepNestConfig& config)
    : config_(DeepNestConfig::getInstance())
    , running_(false)
{
    // Copy configuration values
    config_.curveTolerance = config.curveTolerance;
    config_.spacing = config.spacing;
    config_.rotations = config.rotations;
    config_.populationSize = config.populationSize;
    config_.mutationRate = config.mutationRate;
    config_.threads = config.threads;
    config_.placementType = config.placementType;
    config_.mergeLines = config.mergeLines;
    config_.timeRatio = config.timeRatio;
    config_.simplify = config.simplify;
}

DeepNestSolver::~DeepNestSolver() {
    // CRITICAL FIX: Ensure clean shutdown to prevent crash on exit
    if (running_) {
        stop();
    }
    // Explicitly destroy engine and release all resources before destructor completes
    if (engine_) {
        engine_->stop();  // Ensure threads are stopped
        engine_.reset();  // Trigger destructor and cleanup
    }
}

void DeepNestSolver::setSpacing(double spacing) {
    config_.spacing = spacing;
}

void DeepNestSolver::setRotations(int rotations) {
    config_.rotations = rotations;
}

void DeepNestSolver::setPopulationSize(int size) {
    if (size < 1) {
        throw std::invalid_argument("Population size must be at least 1");
    }
    config_.populationSize = size;
}

void DeepNestSolver::setMutationRate(int rate) {
    if (rate < 0 || rate > 100) {
        throw std::invalid_argument("Mutation rate must be between 0 and 100");
    }
    config_.mutationRate = rate;
}

void DeepNestSolver::setThreads(int threads) {
    config_.threads = threads;
}

void DeepNestSolver::setPlacementType(const std::string& type) {
    if (type != "gravity" && type != "boundingbox" && type != "convexhull") {
        throw std::invalid_argument("Placement type must be 'gravity', 'box', or 'convexhull'");
    }
    config_.placementType = type;
}

void DeepNestSolver::setMergeLines(bool enable) {
    config_.mergeLines = enable;
}

void DeepNestSolver::setTimeRatio(double ratio) {
    config_.timeRatio = ratio;
}

void DeepNestSolver::setCurveTolerance(double tolerance) {
    config_.curveTolerance = tolerance;
}

void DeepNestSolver::setSimplify(bool enable) {
    config_.simplify = enable;
}

const DeepNestConfig& DeepNestSolver::getConfig() const {
    return config_;
}

void DeepNestSolver::addPart(const Polygon& polygon, int quantity, const std::string& name) {
    if (quantity < 1) {
        throw std::invalid_argument("Part quantity must be at least 1");
    }

    // Validate and simplify polygon
    // Strategy:
    // 1. Check basic validity (points >= 3)
    // 2. Remove self-intersections (clean)
    // 3. Simplify using configured tolerance
    // 4. Check validity again

    if (polygon.points.size() < 3) {
        // Discard invalid polygon
        return;
    }

    // Clean polygon (remove self-intersections)
    // Use PolygonOperations::cleanPolygon which uses Clipper's SimplifyPolygon
    std::vector<Point> cleanedPoints = PolygonOperations::cleanPolygon(polygon.points);
    
    if (cleanedPoints.size() < 3) {
        return;
    }

    // Simplify polygon using GeometryUtil::simplifyPolygon as requested
    // Use configured curve tolerance
    std::vector<Point> simplifiedPoints = GeometryUtil::simplifyPolygon(
        cleanedPoints,
        config_.curveTolerance
    );

    if (simplifiedPoints.size() < 3) {
        return;
    }

    // Create new polygon with simplified points
    Polygon simplifiedPoly = polygon;
    simplifiedPoly.points = simplifiedPoints;

    // Also process holes if any
    if (!simplifiedPoly.children.empty()) {
        std::vector<Polygon> validHoles;
        for (const auto& hole : simplifiedPoly.children) {
            if (hole.points.size() < 3) continue;

            std::vector<Point> cleanedHole = PolygonOperations::cleanPolygon(hole.points);
            if (cleanedHole.size() < 3) continue;

            std::vector<Point> simplifiedHole = GeometryUtil::simplifyPolygon(
                cleanedHole,
                config_.curveTolerance
            );

            if (simplifiedHole.size() >= 3) {
                Polygon newHole = hole;
                newHole.points = simplifiedHole;
                validHoles.push_back(newHole);
            }
        }
        simplifiedPoly.children = validHoles;
    }

    // Final check for area
    if (std::abs(simplifiedPoly.area()) < 1e-6) {
        return;
    }

    parts_.push_back(PartSpec(simplifiedPoly, quantity, name));
}

void DeepNestSolver::addSheet(const Polygon& polygon, int quantity, const std::string& name) {
    if (quantity < 1) {
        throw std::invalid_argument("Sheet quantity must be at least 1");
    }

    // Validate and simplify polygon (same strategy as parts)
    if (polygon.points.size() < 3) {
        return;
    }

    std::vector<Point> cleanedPoints = PolygonOperations::cleanPolygon(polygon.points);
    if (cleanedPoints.size() < 3) {
        return;
    }

    std::vector<Point> simplifiedPoints = GeometryUtil::simplifyPolygon(
        cleanedPoints,
        config_.curveTolerance
    );

    if (simplifiedPoints.size() < 3) {
        return;
    }

    Polygon simplifiedPoly = polygon;
    simplifiedPoly.points = simplifiedPoints;

    // Process holes
    if (!simplifiedPoly.children.empty()) {
        std::vector<Polygon> validHoles;
        for (const auto& hole : simplifiedPoly.children) {
            if (hole.points.size() < 3) continue;

            std::vector<Point> cleanedHole = PolygonOperations::cleanPolygon(hole.points);
            if (cleanedHole.size() < 3) continue;

            std::vector<Point> simplifiedHole = GeometryUtil::simplifyPolygon(
                cleanedHole,
                config_.curveTolerance
            );

            if (simplifiedHole.size() >= 3) {
                Polygon newHole = hole;
                newHole.points = simplifiedHole;
                validHoles.push_back(newHole);
            }
        }
        simplifiedPoly.children = validHoles;
    }

    if (std::abs(simplifiedPoly.area()) < 1e-6) {
        return;
    }

    sheets_.push_back(SheetSpec(simplifiedPoly, quantity, name));
}

void DeepNestSolver::clearParts() {
    parts_.clear();
}

void DeepNestSolver::clearSheets() {
    sheets_.clear();
}

void DeepNestSolver::clear() {
    clearParts();
    clearSheets();
}

size_t DeepNestSolver::getPartCount() const {
    return parts_.size();
}

size_t DeepNestSolver::getSheetCount() const {
    return sheets_.size();
}

void DeepNestSolver::start(int maxGenerations) {
    if (running_) {
        throw std::runtime_error("Nesting is already running");
    }

    if (parts_.empty()) {
        throw std::runtime_error("No parts added. Use addPart() to add parts to nest.");
    }

    if (sheets_.empty()) {
        throw std::runtime_error("No sheets added. Use addSheet() to add sheets.");
    }

    // CRITICAL FIX: Explicitly destroy old engine and release all resources
    // before creating a new one to prevent memory corruption and dangling references
    if (engine_) {
        // Stop if still running (defensive)
        engine_->stop();
        // Explicitly reset to trigger destructor and cleanup
        engine_.reset();
    }

    // Create new nesting engine with clean state
    engine_ = std::make_unique<NestingEngine>(config_);

    // Convert PartSpec to Polygon vectors
    std::vector<Polygon> partPolygons;
    std::vector<int> partQuantities;

    for (const auto& part : parts_) {
        partPolygons.push_back(part.polygon);
        partQuantities.push_back(part.quantity);
    }

    // Convert SheetSpec to Polygon vectors
    std::vector<Polygon> sheetPolygons;
    std::vector<int> sheetQuantities;

    for (const auto& sheet : sheets_) {
        sheetPolygons.push_back(sheet.polygon);
        sheetQuantities.push_back(sheet.quantity);
    }

    // Initialize engine
    engine_->initialize(partPolygons, partQuantities, sheetPolygons, sheetQuantities);

    // Start engine
    engine_->start(progressCallback_, resultCallback_, maxGenerations);

    running_ = true;
}

void DeepNestSolver::stop() {
    if (engine_) {
        engine_->stop();
    }
    running_ = false;
}

bool DeepNestSolver::step() {
    if (!running_ || !engine_) {
        return false;
    }

    bool stillRunning = engine_->step();

    if (!stillRunning) {
        running_ = false;
    }

    return stillRunning;
}

bool DeepNestSolver::isRunning() const {
    return running_;
}

void DeepNestSolver::runUntilComplete(int maxGenerations, int stepDelayMs) {
    // Start if not already running
    if (!running_) {
        start(maxGenerations);
    }

    // Run until complete
    while (step()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(stepDelayMs));
    }
}

NestProgress DeepNestSolver::getProgress() const {
    if (!engine_) {
        NestProgress progress;
        progress.generation = 0;
        progress.evaluationsCompleted = 0;
        progress.bestFitness = std::numeric_limits<double>::max();
        progress.percentComplete = 0.0;
        return progress;
    }

    return engine_->getProgress();
}

const NestResult* DeepNestSolver::getBestResult() const {
    if (!engine_) {
        return nullptr;
    }

    return engine_->getBestResult();
}

const std::vector<NestResult>& DeepNestSolver::getResults() const {
    static const std::vector<NestResult> emptyResults;

    if (!engine_) {
        return emptyResults;
    }

    return engine_->getResults();
}

void DeepNestSolver::setProgressCallback(NestingEngine::ProgressCallback callback) {
    progressCallback_ = callback;
    if (engine_) {
        // Update callback on running engine dynamically
        engine_->setProgressCallback(callback);
    }
}

void DeepNestSolver::setResultCallback(NestingEngine::ResultCallback callback) {
    resultCallback_ = callback;
    if (engine_) {
        // Update callback on running engine dynamically
        engine_->setResultCallback(callback);
    }
}

} // namespace deepnest
