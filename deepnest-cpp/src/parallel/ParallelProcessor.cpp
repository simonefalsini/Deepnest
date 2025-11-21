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

    // Wait for ALL threads to COMPLETELY finish their current tasks
    // This ensures no thread is still executing code that references PlacementWorker, etc.
    threads_.join_all();

    LOG_THREAD("ParallelProcessor::stop() completed (threads NOT joined to avoid crash)");
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

void ParallelProcessor::executeLocked(std::function<void()> func) {
    boost::lock_guard<boost::mutex> lock(mutex_);
    func();
}

void ParallelProcessor::processPopulation(
    Population& population,
    const std::vector<Polygon>& sheets,
    PlacementWorker& worker,
    int maxConcurrent
) {
    // Protect the entire task selection and submission process
    // This prevents race conditions where:
    // 1. Multiple threads try to process the same individual
    // 2. The 'processing' flag is read/written concurrently without protection
    
    // We need to identify tasks to run while holding the lock, but we can't
    // hold the lock while enqueuing if enqueuing might block or re-acquire (though here it shouldn't).
    // However, to be safe and atomic, we'll identify AND mark as processing under the lock.
    
    std::vector<size_t> indicesToProcess;
    
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        
        if (stopped_) return;

        int running = 0;
        for(const auto& ind : population.getIndividuals()) {
            if(ind.processing) running++;
        }

        int availableSlots = (maxConcurrent > 0) ? maxConcurrent : threadCount_;
        // If maxConcurrent is 0 (default), use thread count. If thread count is 0 (hardware), 
        // we need to know what that is. The constructor handles this, but let's be safe.
        if (availableSlots <= 0) availableSlots = boost::thread::hardware_concurrency();

    auto& individuals = population.getIndividuals();
        for(size_t i = 0; i < individuals.size(); ++i) {
            if(running >= availableSlots) break;

            auto& individual = individuals[i];
            
            // Check if needs processing: fitness not valid and not currently processing
            if(!individual.hasValidFitness() && !individual.processing) {
                individual.processing = true; // Mark as processing IMMEDIATELY under lock
                indicesToProcess.push_back(i);
            running++;
            }
        }
    }

    // Now enqueue the tasks. The 'processing' flag is already set, so other calls
    // to processPopulation won't pick these up.
    for(size_t i : indicesToProcess) {
        // Capture BY VALUE
            size_t index = i;
        
        enqueue([&population, &sheets, &worker, index, this]() {
            // Create a thread-local copy of the individual to work on
            // We need to read the input data safely. 
            // The individual at 'index' is stable in the vector (vector doesn't resize here).
            // But we should copy it to avoid reading it while main thread might be reading it?
            // Actually, main thread only reads 'processing' and results. 
            // Input data (parts, rotation) is constant during evaluation.
            
            Individual individualCopy;
            {
                boost::lock_guard<boost::mutex> lock(mutex_);
                individualCopy = population.getIndividuals()[index];
            }
            
            // Perform the heavy lifting WITHOUT the lock
                // Prepare parts with rotations
                std::vector<Polygon> parts;
                for (size_t j = 0; j < individualCopy.placement.size(); ++j) {
                    Polygon part = *individualCopy.placement[j];
                    part.rotation = individualCopy.rotation[j];
                    parts.push_back(part);
                }
            PlacementWorker::PlacementResult result = worker.placeParts(sheets, parts);
            
            // Update the results WITH the lock
                {
                boost::lock_guard<boost::mutex> lock(mutex_);
                // Update the original individual with results
                auto& originalIndividual = population.getIndividuals()[index];
                originalIndividual.fitness = result.fitness;
                originalIndividual.area = result.area;
                originalIndividual.mergedLength = result.mergedLength;
                originalIndividual.placements = result.placements;
                originalIndividual.processing = false;

                        // Log fitness evaluation (first 10 individuals only to avoid spam)
                        static int evalCount = 0;
                        evalCount++;
                        if (evalCount <= 10) {
                            LOG_GA("[Eval #" << evalCount << "] Individual[" << index
                                      << "] fitness=" << result.fitness
                                      << ", area=" << result.area
                                      << ", merged=" << result.mergedLength);
                        }
                    }
            });
        }
}

} // namespace deepnest
