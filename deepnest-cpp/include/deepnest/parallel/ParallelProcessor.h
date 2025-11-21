#ifndef DEEPNEST_PARALLEL_PROCESSOR_H
#define DEEPNEST_PARALLEL_PROCESSOR_H

#include "../algorithm/GeneticAlgorithm.h"
#include "../algorithm/Population.h"
#include "../placement/PlacementWorker.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <vector>
#include <memory>
#include <future>
#include <functional>

namespace deepnest {

/**
 * @brief Parallel processor using Boost.Thread for concurrent placement evaluation
 *
 * This class manages a thread pool for parallel processing of genetic algorithm
 * individuals. It uses boost::asio::io_service as a task queue and boost::thread_group
 * for managing worker threads.
 *
 * The parallel processing pattern:
 * 1. Create thread pool with N worker threads
 * 2. Enqueue placement tasks for unevaluated individuals
 * 3. Workers execute placement concurrently
 * 4. Results update individual fitness asynchronously
 * 5. Continue until generation is complete
 *
 * References:
 * - background.js: launchWorkers function (lines 1040-1153)
 * - deepnest.js: workerTimer and parallel processing logic
 */
class ParallelProcessor {
public:
    /**
     * @brief Constructor
     *
     * Creates a thread pool with the specified number of threads.
     * Threads are started immediately and wait for tasks.
     *
     * @param numThreads Number of worker threads (default: hardware concurrency)
     */
    explicit ParallelProcessor(int numThreads = 0);

    /**
     * @brief Destructor
     *
     * Stops all worker threads gracefully.
     */
    ~ParallelProcessor();

    /**
     * @brief Enqueue a generic task
     *
     * Template method for enqueuing arbitrary tasks to the thread pool.
     * Returns a future that will contain the result when the task completes.
     *
     * @tparam Func Callable type (function, lambda, functor)
     * @param f Task to execute
     * @return Future containing the task result
     *
     * Example:
     * ```cpp
     * auto future = processor.enqueue([]() { return computeResult(); });
     * auto result = future.get(); // Wait for completion
     * ```
     */
    template<typename Func>
    auto enqueue(Func&& f) -> std::future<typename std::result_of<Func()>::type>;

    /**
     * @brief Process population individuals in parallel
     *
     * Evaluates unevaluated individuals in the population using parallel placement.
     * This is the main method for concurrent fitness evaluation.
     *
     * Algorithm:
     * 1. Count currently processing individuals
     * 2. For each unevaluated individual (fitness == 0):
     *    - If slots available (running < numThreads):
     *      - Mark as processing
     *      - Enqueue placement task
     *      - Increment running count
     * 3. Tasks execute concurrently in thread pool
     * 4. On completion, update individual fitness
     *
     * @param population Population to process
     * @param sheets Available sheets for placement
     * @param worker PlacementWorker instance for evaluations
     * @param maxConcurrent Maximum concurrent evaluations (0 = use thread count)
     *
     * References:
     * - background.js line 1105: var running = GA.population.filter(...)
     * - background.js line 1129-1152: for loop launching workers
     */
    void processPopulation(
        Population& population,
        const std::vector<Polygon>& sheets,
        PlacementWorker& worker,
        int maxConcurrent = 0
    );

    /**
     * @brief Get number of worker threads
     *
     * @return Number of threads in the pool
     */
    int getThreadCount() const { return threadCount_; }

    /**
     * @brief Stop all worker threads
     *
     * Stops accepting new tasks and waits for current tasks to complete.
     */
    void stop();

    /**
     * @brief Wait for all tasks to complete
     *
     * Blocks until the task queue is empty.
     */
    void waitAll();

    /**
     * @brief Execute a function under the processor's lock
     * 
     * Allows external components (like NestingEngine) to synchronize access
     * to shared data protected by the processor's mutex.
     * 
     * @param func Function to execute while holding the lock
     */
    void executeLocked(std::function<void()> func);

private:
    /**
     * @brief IO context for task queue
     */
    boost::asio::io_context ioContext_;

    /**
     * @brief Work guard to keep io_context running
     */
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> workGuard_;

    /**
     * @brief Thread group managing worker threads
     */
    boost::thread_group threads_;

    /**
     * @brief Number of worker threads
     */
    int threadCount_;

    /**
     * @brief Flag indicating if processor is stopped
     */
    bool stopped_;

    /**
     * @brief Mutex for thread-safe operations
     */
    mutable boost::mutex mutex_;
};

// Template implementation must be in header
template<typename Func>
auto ParallelProcessor::enqueue(Func&& f) -> std::future<typename std::result_of<Func()>::type> {
    using return_type = typename std::result_of<Func()>::type;

    // CRITICAL FIX: Don't enqueue tasks if processor is stopped
    // This prevents adding tasks that will never execute and may hold dangling references
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        if (stopped_) {
            // Return an immediately ready future with default value
            // This prevents crashes when trying to enqueue after stop()
            std::promise<return_type> promise;

            // MSVC FIX: std::promise<void>::set_value() takes NO arguments!
            // Use if constexpr to handle void vs non-void return types
            if constexpr (std::is_void<return_type>::value) {
                promise.set_value();  // void - no argument
            } else {
                promise.set_value(return_type());  // non-void - default constructed value
            }

            return promise.get_future();
        }
    }

    // Create a packaged_task wrapping the function
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::forward<Func>(f)
    );

    std::future<return_type> result = task->get_future();

    // Post task to io_context
    boost::asio::post(ioContext_, [task]() {
        (*task)();
    });

    return result;
}

} // namespace deepnest

#endif // DEEPNEST_PARALLEL_PROCESSOR_H
