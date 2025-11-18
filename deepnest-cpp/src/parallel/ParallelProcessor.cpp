#include "../../include/deepnest/parallel/ParallelProcessor.h"
#include <algorithm>

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
    boost::lock_guard<boost::mutex> lock(mutex_);

    if (stopped_) {
        return;
    }

    stopped_ = true;

    // Destroy work guard to allow io_context to finish
    workGuard_.reset();

    // Wait for all threads to complete
    threads_.join_all();

    // Stop io_context
    ioContext_.stop();
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

            // Create copies for thread safety
            std::vector<Polygon> sheetsCopy = sheets;
            Individual individualCopy = individual.clone();

            // Enqueue placement task
            enqueue([&individual, sheetsCopy, &worker, individualCopy]() mutable {
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

                // Update individual with result
                // JavaScript: GA.population[payload.index].processing = false;
                //             GA.population[payload.index].fitness = payload.fitness;
                individual.fitness = result.fitness;
                individual.processing = false;

                // Note: In a real implementation, you might want to store the full
                // placement result somewhere for later use/visualization
            });
        }
    }
}

} // namespace deepnest
