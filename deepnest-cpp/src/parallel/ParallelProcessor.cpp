#include "../../include/deepnest/parallel/ParallelProcessor.h"
#include "../../include/deepnest/DebugConfig.h"
#include <algorithm>
#include <thread>
#include <chrono>

namespace deepnest {

ParallelProcessor::ParallelProcessor(int numThreads)
    : workGuard_(nullptr)
    , threadCount_(numThreads)
    , stopped_(false)
{
    // If numThreads is 0 or negative, use hardware concurrency
    if (threadCount_ <= 0) {
        threadCount_ = boost::thread::hardware_concurrency();
        if (threadCount_ <= 0) {
            threadCount_ = 4; // Fallback to 4 threads
        }
    }

    // Create work guard to keep io_context running
    workGuard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
        boost::asio::make_work_guard(ioContext_)
    );

    // Create worker threads
    for (int i = 0; i < threadCount_; ++i) {
        threads_.create_thread([this]() {
            ioContext_.run();
        });
    }
}

ParallelProcessor::~ParallelProcessor() {
    stop();
}

void ParallelProcessor::stop() {
    LOG_THREAD("ParallelProcessor::stop() called");

    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        if (stopped_) {
            LOG_THREAD("Already stopped, returning");
            return;
        }
        stopped_ = true;
    }

    // DRASTIC FIX: If join_all() deadlocks, we can't recover
    // Best option: detach threads and recreate thread pool on next start

    LOG_THREAD("Removing work guard");
    workGuard_.reset();

    LOG_THREAD("Calling ioContext_.stop()");
    ioContext_.stop();

    // Give threads a brief moment to exit
    LOG_THREAD("Waiting 50ms for threads to exit");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    LOG_THREAD("Attempting join_all with timeout simulation");

    // Use a separate thread to call join_all with a timeout
    std::atomic<bool> joinCompleted{false};
    std::thread joinThread([this, &joinCompleted]() {
        try {
            LOG_THREAD("  [Join thread] Calling join_all...");
            this->threads_.join_all();
            LOG_THREAD("  [Join thread] join_all completed successfully");
            joinCompleted = true;
        } catch (const std::exception& e) {
            LOG_THREAD("  [Join thread] Exception: " << e.what());
        } catch (...) {
            LOG_THREAD("  [Join thread] Unknown exception");
        }
    });

    // Wait up to 500ms for join to complete
    for (int i = 0; i < 50; ++i) {
        if (joinCompleted) {
            LOG_THREAD("Join completed successfully within timeout");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!joinCompleted) {
        LOG_THREAD("WARNING: join_all timed out after 500ms!");
        LOG_THREAD("WARNING: Detaching join thread - threads may be leaked");
        // Detach the join thread - it will continue trying to join
        // This prevents the main thread from blocking forever
        joinThread.detach();
    } else {
        // Join succeeded, clean up the join thread
        joinThread.join();
    }

    // Reset io_context for restart
    LOG_THREAD("Resetting io_context");
    ioContext_.restart();

    // Recreate work guard for potential restart
    LOG_THREAD("Recreating work guard");
    workGuard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
        boost::asio::make_work_guard(ioContext_)
    );

    // If join timed out, we need to recreate threads on next use
    // But we can't do it here because threads_ might be in inconsistent state
    // Solution: Mark that we need to recreate, and do it in NestingEngine::start()

    LOG_THREAD("ParallelProcessor::stop() completed (join " <<
              (joinCompleted ? "succeeded" : "TIMED OUT") << ")");
}

void ParallelProcessor::waitAll() {
    // Poll until io_context has no pending work
    while (!ioContext_.stopped()) {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(10));

        // Check if there are any pending handlers
        if (ioContext_.poll() == 0) {
            // No more work to do
            break;
        }
    }
}

void ParallelProcessor::processPopulation(
    Population& population,
    const std::vector<Polygon>& sheets,
    PlacementWorker& worker,
    int maxConcurrent
) {
    // JavaScript: var running = GA.population.filter(function(p){ return !!p.processing; }).length;
    // Count currently processing individuals
    size_t running = population.getProcessingCount();

    // Determine maximum concurrent tasks
    int maxTasks = (maxConcurrent > 0) ? maxConcurrent : threadCount_;

    std::cerr << "=== ParallelProcessor::processPopulation ===" << std::endl;
    std::cerr << "  maxConcurrent: " << maxConcurrent << ", threadCount_: " << threadCount_
              << ", maxTasks: " << maxTasks << ", currently running: " << running << std::endl;
    std::cerr.flush();

    // JavaScript: for(i=0; i<GA.population.length; i++) {
    //               if(running < config.threads && !GA.population[i].processing && !GA.population[i].fitness) {
    //                 GA.population[i].processing = true;
    //                 // launch worker
    //                 running++;
    //               }
    //             }
    // Launch workers for unevaluated individuals
    auto& individuals = population.getIndividuals();

    for (size_t i = 0; i < individuals.size(); ++i) {
        Individual& individual = individuals[i];

        // Check if we can launch more workers
        if (static_cast<int>(running) >= maxTasks) {
            break;
        }

        // Check if individual needs evaluation
        if (!individual.processing && !individual.hasValidFitness()) {
            // Mark as processing
            individual.processing = true;
            running++;

            std::cerr << "  Enqueueing task for individual #" << i << ", running count now: " << running << std::endl;
            std::cerr.flush();

            // Create copies for thread safety
            std::vector<Polygon> sheetsCopy = sheets;
            Individual individualCopy = individual.clone();

            // CRITICAL FIX: Capture index instead of reference to avoid use-after-free
            // The old code captured &individual which could become invalid when the lambda executes
            size_t index = i;

            // Enqueue placement task
            enqueue([&population, index, sheetsCopy, &worker, individualCopy, this]() mutable {
                std::cerr << "=== WORKER THREAD: Starting placeParts for individual #" << index << " ===" << std::endl;
                std::cerr.flush();
                // Prepare parts with rotations
                // JavaScript: var ids = []; var sources = []; var children = [];
                //             for(j=0; j<GA.population[i].placement.length; j++) {
                //               var id = GA.population[i].placement[j].id;
                //               var source = GA.population[i].placement[j].source;
                //               ...
                //             }
                std::vector<Polygon> parts;
                for (size_t j = 0; j < individualCopy.placement.size(); ++j) {
                    Polygon part = *individualCopy.placement[j];
                    part.rotation = individualCopy.rotation[j];
                    parts.push_back(part);
                }

                // Execute placement
                // JavaScript: ipcRenderer.send('background-start', {...})
                // In background process: placeParts(sheets, parts, config, nestindex)
                PlacementWorker::PlacementResult result = worker.placeParts(sheetsCopy, parts);

                // Update individual with result - THREAD SAFE ACCESS
                // JavaScript: GA.population[payload.index].processing = false;
                //             GA.population[payload.index].fitness = payload.fitness;
                {
                    boost::lock_guard<boost::mutex> lock(this->mutex_);
                    auto& individuals = population.getIndividuals();
                    if (index < individuals.size()) {
                        individuals[index].fitness = result.fitness;
                        individuals[index].area = result.area;
                        individuals[index].mergedLength = result.mergedLength;
                        individuals[index].placements = result.placements;  // Store actual placements!
                        individuals[index].processing = false;

                        // GA DEBUG: Log fitness evaluation (first 10 individuals only to avoid spam)
                        static int evalCount = 0;
                        evalCount++;
                        if (evalCount <= 10) {
                            std::cout << "  [Eval #" << evalCount << "] Individual[" << index
                                      << "] fitness=" << result.fitness
                                      << ", area=" << result.area
                                      << ", merged=" << result.mergedLength << std::endl;
                            std::cout.flush();
                        }
                    }
                }
            });
        }
    }
}

} // namespace deepnest
