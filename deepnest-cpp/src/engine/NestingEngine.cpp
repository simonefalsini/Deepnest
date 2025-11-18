#include "../../include/deepnest/engine/NestingEngine.h"
#include "../../include/deepnest/geometry/GeometryUtil.h"
#include "../../include/deepnest/geometry/PolygonOperations.h"
#include <algorithm>
#include <stdexcept>

namespace deepnest {

NestingEngine::NestingEngine(const DeepNestConfig& config)
    : config_(config)
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
    stop();
}

void NestingEngine::initialize(
    const std::vector<Polygon>& parts,
    const std::vector<int>& quantities,
    const std::vector<Polygon>& sheets,
    const std::vector<int>& sheetQuantities
) {
    if (parts.size() != quantities.size()) {
        throw std::invalid_argument("Parts and quantities arrays must have same size");
    }
    if (sheets.size() != sheetQuantities.size()) {
        throw std::invalid_argument("Sheets and sheetQuantities arrays must have same size");
    }

    // Clear previous state
    parts_.clear();
    partPointers_.clear();
    sheets_.clear();
    results_.clear();
    evaluationsCompleted_ = 0;
    geneticAlgorithm_.reset();

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
                sheet = applySpacing(sheet, -0.5 * config_.spacing);
            }

            sheets_.push_back(sheet);
        }
    }

    // Prepare parts with spacing (expand parts by spacing)
    // JavaScript uses +0.5*spacing for parts
    // JavaScript: var adam = [];
    //             for(i=0; i<parts.length; i++) {
    //               if(!parts[i].sheet) {
    //                 for(j=0; j<parts[i].quantity; j++) {
    //                   var poly = cloneTree(parts[i].polygontree);
    //                   poly.id = id;
    //                   poly.source = i;
    //                   adam.push(poly);
    //                   id++;
    //                 }
    //               }
    //             }
    int id = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
        for (int q = 0; q < quantities[i]; ++q) {
            Polygon part = parts[i];
            part.id = id++;
            part.source = static_cast<int>(i);

            // Apply positive offset to parts (expand)
            if (config_.spacing > 0) {
                part = applySpacing(part, 0.5 * config_.spacing);
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

    // Create pointers for GA
    partPointers_.clear();
    for (auto& part : parts_) {
        partPointers_.push_back(&part);
    }

    // JavaScript: GA = new GeneticAlgorithm(adam, config);
    // Initialize genetic algorithm
    geneticAlgorithm_ = std::make_unique<GeneticAlgorithm>(partPointers_, config_);
}

void NestingEngine::start(
    ProgressCallback progressCallback,
    ResultCallback resultCallback,
    int maxGenerations
) {
    if (!geneticAlgorithm_) {
        throw std::runtime_error("Must call initialize() before start()");
    }

    progressCallback_ = progressCallback;
    resultCallback_ = resultCallback;
    maxGenerations_ = maxGenerations;
    running_ = true;

    // Note: In the JavaScript version, this uses a timer (setInterval)
    // In C++, the user should call step() in their main loop or from a timer
}

void NestingEngine::stop() {
    running_ = false;
    if (parallelProcessor_) {
        parallelProcessor_->stop();
    }
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
        // All individuals evaluated, create next generation
        geneticAlgorithm_->generation();

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
    auto& population = geneticAlgorithm_->getPopulation();
    for (size_t i = 0; i < population.size(); ++i) {
        Individual& individual = population[i];

        // Check if this individual was just evaluated
        if (individual.hasValidFitness() && !individual.processing) {
            // We need to get the placement result somehow
            // For now, we'll create a simplified NestResult from the individual's fitness
            // In a full implementation, we'd store the PlacementResult in the Individual

            // JavaScript: if(this.nests.length == 0 || this.nests[0].fitness > payload.fitness) {
            //               this.nests.unshift(payload);
            //               if(this.nests.length > 10) { this.nests.pop(); }
            //               if(displayCallback) { displayCallback(); }
            //             }

            // For now, create a minimal result
            // TODO: Store full PlacementResult in Individual for complete result tracking
            if (results_.empty() || results_[0].fitness > individual.fitness) {
                NestResult result;
                result.fitness = individual.fitness;
                result.generation = geneticAlgorithm_->getCurrentGeneration();
                result.individualIndex = static_cast<int>(i);
                result.area = 0.0; // TODO: Get from PlacementResult
                result.mergedLength = 0.0; // TODO: Get from PlacementResult

                updateResults(result);

                if (resultCallback_) {
                    resultCallback_(result);
                }
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

Polygon NestingEngine::applySpacing(const Polygon& polygon, double offset) {
    if (std::abs(offset) < 1e-6) {
        return polygon;
    }

    // JavaScript: offsetTree(t, offset, offsetFunction, simpleFunction, inside)
    // Uses ClipperLib offset with config.curveTolerance

    // Use PolygonOperations::offset
    std::vector<std::vector<Point>> offsetResults =
        PolygonOperations::offset(polygon.points, offset, 4.0, config_.curveTolerance);

    if (offsetResults.empty()) {
        // Offset failed, return original
        return polygon;
    }

    // Create new polygon with offset points
    Polygon result = polygon;
    result.points = offsetResults[0];

    // Handle children (holes) - they get opposite offset
    if (!polygon.children.empty()) {
        result.children.clear();
        for (const auto& child : polygon.children) {
            Polygon offsetChild = applySpacing(child, -offset);
            result.children.push_back(offsetChild);
        }
    }

    return result;
}

PlacementWorker::PlacementResult NestingEngine::evaluateIndividual(
    Individual& individual,
    const std::vector<Polygon>& parts,
    const std::vector<Polygon>& sheets
) {
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
