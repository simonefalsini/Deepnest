#include "../../include/deepnest/engine/NestingEngine.h"
#include "../../include/deepnest/geometry/GeometryUtil.h"
#include "../../include/deepnest/geometry/PolygonOperations.h"
#include "../../include/deepnest/DebugConfig.h"
#include <algorithm>
#include <stdexcept>
#include <thread>
#include <chrono>

namespace deepnest {

NestingEngine::NestingEngine(const DeepNestConfig& config)
    : config_(config)
    , nfpCache_()
    , running_(false)
    , maxGenerations_(0)
    , evaluationsCompleted_(0)
{
    // Create NFP calculator with cache
    nfpCalculator_ = std::make_unique<NFPCalculator>(nfpCache_);

    // Create placement worker
    placementWorker_ = std::make_unique<PlacementWorker>(config_, *nfpCalculator_);

    // Create parallel processor with configured thread count
    parallelProcessor_ = std::make_unique<ParallelProcessor>(config_.threads);
}

NestingEngine::~NestingEngine() {
    LOG_MEMORY("NestingEngine destructor entered");

    // Ensure clean shutdown to prevent crash and memory corruption
    LOG_NESTING("Calling stop() from destructor");
    stop();

    // Explicitly clear NFP cache to release all cached polygons
    // This prevents potential crashes from destructing cached Polygon objects
    // that may contain Boost.Polygon data
    LOG_MEMORY("Clearing NFP cache");
    nfpCache_.clear();
    LOG_MEMORY("NFP cache cleared (" << nfpCache_.size() << " entries now)");

    // Explicitly destroy components in correct order
    // ParallelProcessor must be destroyed first to stop all threads
    if (parallelProcessor_) {
        LOG_MEMORY("Destroying parallel processor (should already be null from stop())");
        parallelProcessor_.reset();
    } else {
        LOG_MEMORY("Parallel processor already destroyed");
    }

    // Then destroy placement worker
    if (placementWorker_) {
        LOG_MEMORY("Destroying placement worker");
        placementWorker_.reset();
        LOG_MEMORY("Placement worker destroyed");
    }

    // Then destroy NFP calculator
    if (nfpCalculator_) {
        LOG_MEMORY("Destroying NFP calculator");
        nfpCalculator_.reset();
        LOG_MEMORY("NFP calculator destroyed");
    }

    // Genetic algorithm and population destroyed automatically
    LOG_MEMORY("Genetic algorithm will be destroyed automatically");
    LOG_MEMORY("NestingEngine destructor completed");
}

void NestingEngine::initialize(
    const std::vector<Polygon>& parts,
    const std::vector<int>& quantities,
    const std::vector<Polygon>& sheets,
    const std::vector<int>& sheetQuantities
) {
    LOG_NESTING("NestingEngine::initialize() called with " << parts.size() << " parts, " << sheets.size() << " sheets");

    if (parts.size() != quantities.size()) {
        throw std::invalid_argument("Parts and quantities arrays must have same size");
    }
    if (sheets.size() != sheetQuantities.size()) {
        throw std::invalid_argument("Sheets and sheetQuantities arrays must have same size");
    }

    // Safety check - should not be called while running
    if (running_) {
        throw std::runtime_error("Cannot initialize while nesting is running. Call stop() first.");
    }

    // Clear previous state
    LOG_MEMORY("Clearing previous state: parts_(" << parts_.size() << "), partPointers_(" << partPointers_.size() << "), sheets_(" << sheets_.size() << ")");
    parts_.clear();
    partPointers_.clear();
    sheets_.clear();
    results_.clear();
    evaluationsCompleted_ = 0;
    geneticAlgorithm_.reset();
    LOG_MEMORY("Previous state cleared");

    // JavaScript: for(i=0; i<parts.length; i++) {
    //               if(parts[i].sheet) {
    //                 offsetTree(parts[i].polygontree, -0.5*config.spacing, ...);
    //               } else {
    //                 offsetTree(parts[i].polygontree, 0.5*config.spacing, ...);
    //               }
    //             }

    // Prepare sheets with spacing (shrink sheets by spacing)
    // JavaScript uses -0.5*spacing for sheets
    for (size_t i = 0; i < sheets.size(); ++i) {
        for (int q = 0; q < sheetQuantities[i]; ++q) {
            Polygon sheet = sheets[i];
            sheet.id = static_cast<int>(sheets_.size());
            sheet.source = static_cast<int>(i);

            // Apply negative offset to sheets (shrink)
            if (config_.spacing > 0) {
                sheet = applySpacing(sheet, -0.5 * config_.spacing,  config_.curveTolerance);
                
                // If sheet became invalid/empty after spacing, skip it
                if (sheet.points.size() < 3 || std::abs(sheet.area()) < 1e-6) {
                    continue;
                }
            }

            sheets_.push_back(sheet);
        }
    }

    // Prepare parts with spacing (expand parts by spacing)
    // JavaScript uses +0.5*spacing for parts
    int id = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
        for (int q = 0; q < quantities[i]; ++q) {
            Polygon part = parts[i];
            part.id = id++;
            part.source = static_cast<int>(i);

            // Apply positive offset to parts (expand)
            if (config_.spacing > 0) {
                part = applySpacing(part, 0.5 * config_.spacing,  config_.curveTolerance);
                
                // If part became invalid/empty after spacing, skip it
                if (part.points.size() < 3 || std::abs(part.area()) < 1e-6) {
                    continue;
                }
            }

            parts_.push_back(part);
        }
    }

    // JavaScript: adam.sort(function(a, b) {
    //               return Math.abs(GeometryUtil.polygonArea(b)) - Math.abs(GeometryUtil.polygonArea(a));
    //             });
    // Sort parts by area (largest first)
    std::sort(parts_.begin(), parts_.end(),
        [](const Polygon& a, const Polygon& b) {
            return std::abs(GeometryUtil::polygonArea(a.points)) >
                   std::abs(GeometryUtil::polygonArea(b.points));
        });

    // Create shared_ptr for GA instead of raw pointers
    // This ensures parts stay alive even if parts_ vector is cleared during restart
    LOG_MEMORY("Creating shared_ptr partPointers_ for " << parts_.size() << " parts");
    partPointers_.clear();
    for (auto& part : parts_) {
        // Create shared_ptr copy of each polygon
        // This prevents use-after-free when parts_ is cleared while threads are running
        partPointers_.push_back(std::make_shared<Polygon>(part));
    }
    LOG_MEMORY("Created " << partPointers_.size() << " shared_ptr part pointers");

    // JavaScript: GA = new GeneticAlgorithm(adam, config);
    // Initialize genetic algorithm
    LOG_NESTING("Creating GeneticAlgorithm with " << partPointers_.size() << " parts");
    geneticAlgorithm_ = std::make_unique<GeneticAlgorithm>(partPointers_, config_);
    LOG_NESTING("GeneticAlgorithm created successfully");
    LOG_NESTING("NestingEngine::initialize() completed successfully");
}

void NestingEngine::start(
    ProgressCallback progressCallback,
    ResultCallback resultCallback,
    int maxGenerations
) {
    if (!geneticAlgorithm_) {
        throw std::runtime_error("Must call initialize() before start()");
    }

    // Recreate ParallelProcessor if it was previously stopped
    // A stopped processor cannot be reused (stopped_=true prevents new tasks)
    // This fixes the segfault when running nesting a second time
    if (!parallelProcessor_) {
        parallelProcessor_ = std::make_unique<ParallelProcessor>(config_.threads);
    }

    progressCallback_ = progressCallback;
    resultCallback_ = resultCallback;
    maxGenerations_ = maxGenerations;
    running_ = true;

    // Note: In the JavaScript version, this uses a timer (setInterval)
    // In C++, the user should call step() in their main loop or from a timer
}

void NestingEngine::stop() {
    LOG_NESTING("NestingEngine::stop() called");

    if (!running_) {
        LOG_NESTING("Already stopped, returning");
        return;
    }

    LOG_NESTING("Stopping nesting engine");
    running_ = false;

    if (parallelProcessor_) {
        LOG_THREAD("Stopping parallel processor");

        // Explicit stop with task queue flush
        // This now waits for all pending tasks to complete (see ParallelProcessor::stop())
        parallelProcessor_->stop();

        // Explicit wait to ensure all tasks completed
        LOG_THREAD("Waiting for all tasks to complete");
        parallelProcessor_->waitAll();

        // Destroy the processor to force recreation on next start()
        // A stopped processor cannot be reused, so we must create a fresh one
        LOG_THREAD("Destroying parallel processor");
        parallelProcessor_.reset();

        // Small delay to ensure complete cleanup before destructor
        // This gives time for any remaining cleanup operations in threads
        LOG_THREAD("Waiting 50ms for complete cleanup");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        LOG_THREAD("Parallel processor destroyed and cleaned up");
    }

    LOG_NESTING("Nesting engine stopped successfully");
}

bool NestingEngine::step() {
    if (!running_ || !geneticAlgorithm_) {
        return false;
    }

    // Check if we've reached max generations
    if (maxGenerations_ > 0 && geneticAlgorithm_->getCurrentGeneration() >= maxGenerations_) {
        running_ = false;
        return false;
    }

    // JavaScript: var finished = true;
    //             for(i=0; i<GA.population.length; i++) {
    //               if(!GA.population[i].fitness) {
    //                 finished = false;
    //                 break;
    //               }
    //             }
    //             if(finished) {
    //               console.log('new generation!');
    //               GA.generation();
    //             }

    // Check if current generation is complete
    if (isGenerationComplete()) {
        // Get current best before creating next generation
        const Individual& bestBefore = geneticAlgorithm_->getBestIndividual();
        double fitnessBefore = bestBefore.fitness;
        int genBefore = geneticAlgorithm_->getCurrentGeneration();

        // All individuals evaluated, create next generation
        geneticAlgorithm_->generation();

        // Log generation transition with fitness comparison
        const Individual& bestAfter = geneticAlgorithm_->getBestIndividual();
        LOG_GA("*** Generation " << genBefore << " -> " << geneticAlgorithm_->getCurrentGeneration()
                  << ": Best fitness " << fitnessBefore);
        if (bestAfter.fitness < fitnessBefore) {
            LOG_GA(" -> " << bestAfter.fitness << " (IMPROVED by "
                      << (fitnessBefore - bestAfter.fitness) << ")");
        } else if (bestAfter.fitness == fitnessBefore) {
            LOG_GA(" (NO CHANGE)");
        } else {
            LOG_GA(" -> " << bestAfter.fitness << " (WORSE?!)");
        }

        // Report progress
        if (progressCallback_) {
            progressCallback_(getProgress());
        }
    }

    // JavaScript: var running = GA.population.filter(function(p){ return !!p.processing; }).length;
    //             for(i=0; i<GA.population.length; i++) {
    //               if(running < config.threads && !GA.population[i].processing && !GA.population[i].fitness) {
    //                 GA.population[i].processing = true;
    //                 // launch worker
    //                 running++;
    //               }
    //             }

    // Launch parallel evaluations for unevaluated individuals
    // Note: ParallelProcessor::processPopulation handles the parallel execution
    // Check if parallelProcessor_ exists (may be nullptr after stop())
    if (!parallelProcessor_) {
        running_ = false;
        return false;
    }

    parallelProcessor_->processPopulation(
        geneticAlgorithm_->getPopulationObject(), // Access population object through GA
        sheets_,
        *placementWorker_,
        config_.threads
    );

    // Note: The JavaScript version processes results via IPC callback
    // In our C++ version, the PlacementWorker execution happens in parallel threads
    // and updates Individual.fitness directly. We need to check for new best results.

    // Check for best results and update
    // Use executeLocked to safely read results from individuals
    // This prevents race conditions where worker threads are updating the individual
    // while we are trying to read its fitness or placements.
    std::vector<NestResult> newBetterResults;

    parallelProcessor_->executeLocked([&]() {
    auto& population = geneticAlgorithm_->getPopulation();
    for (size_t i = 0; i < population.size(); ++i) {
        Individual& individual = population[i];

        // Check if this individual was just evaluated
        if (individual.hasValidFitness() && !individual.processing) {
                // Check if it's a new best result
                // We check against results_ here (safe as we are on main thread)
                // to avoid unnecessary copying of placements for non-best results
            if (results_.empty() || results_[0].fitness > individual.fitness) {
                NestResult result;
                result.fitness = individual.fitness;
                result.generation = geneticAlgorithm_->getCurrentGeneration();
                result.individualIndex = static_cast<int>(i);
                result.area = individual.area;
                result.mergedLength = individual.mergedLength;
                    result.placements = individual.placements;  // Copy actual placements! SAFE under lock

                    newBetterResults.push_back(result);
                }
            }
        }
    });

    // Process the collected results outside the lock
    for (const auto& result : newBetterResults) {
        // Re-check condition as we might have multiple better results
        if (results_.empty() || results_[0].fitness > result.fitness) {
                updateResults(result);

                if (resultCallback_) {
                    resultCallback_(result);
                }
            }
        }

    return running_;
}

bool NestingEngine::isRunning() const {
    return running_;
}

NestProgress NestingEngine::getProgress() const {
    NestProgress progress;

    if (geneticAlgorithm_) {
        progress.generation = geneticAlgorithm_->getCurrentGeneration();
        progress.evaluationsCompleted = evaluationsCompleted_;

        if (!results_.empty()) {
            progress.bestFitness = results_[0].fitness;
        } else {
            progress.bestFitness = std::numeric_limits<double>::max();
        }

        if (maxGenerations_ > 0) {
            progress.percentComplete = 100.0 * progress.generation / maxGenerations_;
        } else {
            progress.percentComplete = 0.0;
        }
    } else {
        progress.generation = 0;
        progress.evaluationsCompleted = 0;
        progress.bestFitness = std::numeric_limits<double>::max();
        progress.percentComplete = 0.0;
    }

    return progress;
}

const NestResult* NestingEngine::getBestResult() const {
    if (results_.empty()) {
        return nullptr;
    }
    return &results_[0];
}

const std::vector<NestResult>& NestingEngine::getResults() const {
    return results_;
}

const DeepNestConfig& NestingEngine::getConfig() const {
    return config_;
}

void NestingEngine::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = callback;
}

void NestingEngine::setResultCallback(ResultCallback callback) {
    resultCallback_ = callback;
}

Polygon NestingEngine::applySpacing(const Polygon& polygon, double offset, double curveTolerance) {
    if (std::abs(offset) < 1e-6) {
        return polygon;
    }

    // JavaScript: offsetTree(t, offset, offsetFunction, simpleFunction, inside)
    // Uses ClipperLib offset with config.curveTolerance

    // Use PolygonOperations::offset
    std::vector<std::vector<Point>> offsetResults =
        PolygonOperations::offset(polygon.points, offset, 4.0, curveTolerance);

    if (offsetResults.empty()) {
        // Offset failed or polygon vanished (e.g. negative offset on small part)
        // Return empty polygon to signal invalidity
        Polygon emptyPoly = polygon;
        emptyPoly.points.clear();
        emptyPoly.children.clear();
        return emptyPoly;
    }

    // Create new polygon with offset points
    Polygon result = polygon;
    
    // Clean the result to ensure valid geometry (remove self-intersections)
    // This matches the "clean shapes" strategy
    std::vector<Point> cleanedPoints = PolygonOperations::cleanPolygon(offsetResults[0]);
    
    if (cleanedPoints.size() < 3) {
        // Invalid after cleaning
        result.points.clear();
        result.children.clear();
        return result;
    }
    
    result.points = cleanedPoints;

    // Handle children (holes) - they get opposite offset
    if (!polygon.children.empty()) {
        result.children.clear();
        for (const auto& child : polygon.children) {
            Polygon offsetChild = applySpacing(child, -offset, curveTolerance);
            // Only add valid children
            if (offsetChild.points.size() >= 3 && std::abs(offsetChild.area()) > 1e-6) {
                result.children.push_back(offsetChild);
            }
        }
    }

    return result;
}

PlacementWorker::PlacementResult NestingEngine::evaluateIndividual(
    Individual& individual,
    const std::vector<Polygon>& parts,
    const std::vector<Polygon>& sheets
) {
    // Suppress unused parameter warning - parts are retrieved from individual.placement
    (void)parts;

    // Prepare parts with rotations from individual
    // JavaScript: for(j=0; j<GA.population[i].placement.length; j++) {
    //               var id = GA.population[i].placement[j].id;
    //               var source = GA.population[i].placement[j].source;
    //               var rotation = GA.population[i].rotation[j];
    //               ...
    //             }

    std::vector<Polygon> partsToPlace;
    for (size_t j = 0; j < individual.placement.size(); ++j) {
        Polygon part = *individual.placement[j];
        part.rotation = individual.rotation[j];
        partsToPlace.push_back(part);
    }

    // JavaScript: ipcRenderer.send('background-start', {...});
    // In background: placeParts(sheets, parts, config, nestindex)
    return placementWorker_->placeParts(sheets, partsToPlace);
}

bool NestingEngine::isGenerationComplete() const {
    if (!geneticAlgorithm_) {
        return false;
    }

    return geneticAlgorithm_->isGenerationComplete();
}

NestResult NestingEngine::toNestResult(
    const PlacementWorker::PlacementResult& result,
    int generation,
    int individualIndex
) const {
    NestResult nestResult;
    nestResult.placements = result.placements;
    nestResult.fitness = result.fitness;
    nestResult.area = result.area;
    nestResult.mergedLength = result.mergedLength;
    nestResult.generation = generation;
    nestResult.individualIndex = individualIndex;

    return nestResult;
}

void NestingEngine::updateResults(const NestResult& result) {
    // JavaScript: if(this.nests.length == 0 || this.nests[0].fitness > payload.fitness) {
    //               this.nests.unshift(payload);
    //               if(this.nests.length > 10) { this.nests.pop(); }
    //             }

    // Insert result in sorted order (best first)
    auto insertPos = std::lower_bound(
        results_.begin(),
        results_.end(),
        result,
        [](const NestResult& a, const NestResult& b) {
            return a.fitness < b.fitness;
        }
    );

    results_.insert(insertPos, result);

    // Keep only top N results
    if (results_.size() > MAX_SAVED_RESULTS) {
        results_.resize(MAX_SAVED_RESULTS);
    }
}

} // namespace deepnest
