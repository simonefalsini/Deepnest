# SEGFAULT FIXES - IMPLEMENTATION CHECKLIST

## Quick Reference

| # | Severity | File | Function | Issue | Fix |
|---|----------|------|----------|-------|-----|
| 1 | CRITICAL | ParallelProcessor.cpp | stop() | Queue not drained before stop | Add `ioContext_.poll()` drain loop |
| 2 | CRITICAL | ParallelProcessor.cpp | processPopulation() | Dangling reference captures | Capture `this` and use `this->population_` |
| 3 | HIGH | ParallelProcessor.h | enqueue() | No stopped_ check | Add early return if `stopped_` |
| 4 | MEDIUM | NestingEngine.cpp | initialize() | ParallelProcessor not reset | Add `parallelProcessor_.reset()` |
| 5 | MEDIUM | DeepNestSolver.cpp | clear() | Engine not destroyed | Add `engine_.reset()` after stop |

---

## Detailed Fix Instructions

### FIX #1: ParallelProcessor::stop() - Drain Queue

**File**: `/home/user/Deepnest/deepnest-cpp/src/parallel/ParallelProcessor.cpp`  
**Lines**: 36-53

**BEFORE**:
```cpp
void ParallelProcessor::stop() {
    boost::lock_guard<boost::mutex> lock(mutex_);

    if (stopped_) {
        return;
    }

    stopped_ = true;
    workGuard_.reset();
    threads_.join_all();
    ioContext_.stop();
}
```

**AFTER**:
```cpp
void ParallelProcessor::stop() {
    boost::lock_guard<boost::mutex> lock(mutex_);

    if (stopped_) {
        return;
    }

    stopped_ = true;
    
    // CRITICAL FIX: Drain the task queue before destroying io_context
    // This ensures any queued lambdas execute before owner is destroyed
    // Without this, lambdas with dangling references may execute later
    while (ioContext_.poll() > 0) {
        // Process one handler at a time
        // Continue until no more handlers are available
    }
    
    workGuard_.reset();
    threads_.join_all();
    ioContext_.stop();
}
```

**Why**: `ioContext_.stop()` prevents new tasks from being processed but doesn't clear the queue. Unexecuted lambdas remain and execute after owner destruction, accessing dangling references.

---

### FIX #2: ParallelProcessor::processPopulation() - Fix Lambda Captures

**File**: `/home/user/Deepnest/deepnest-cpp/src/parallel/ParallelProcessor.cpp`  
**Lines**: 114-148

**BEFORE**:
```cpp
enqueue([&population, index, sheetsCopy, &worker, individualCopy, this]() mutable {
    // ... code ...
    {
        boost::lock_guard<boost::mutex> lock(this->mutex_);
        auto& individuals = population.getIndividuals();  // ← DANGLING REF
        if (index < individuals.size()) {
            individuals[index].fitness = result.fitness;
        }
    }
});
```

**AFTER**:
```cpp
// Store index for thread-safe access
size_t safeIndex = i;

// Capture only safe values, not references to ephemeral objects
enqueue([safeIndex, sheetsCopy, workerCopy, individualCopy, this]() mutable {
    std::vector<Polygon> parts;
    for (size_t j = 0; j < individualCopy.placement.size(); ++j) {
        Polygon part = *individualCopy.placement[j];
        part.rotation = individualCopy.rotation[j];
        parts.push_back(part);
    }

    // Execute placement
    PlacementWorker::PlacementResult result = workerCopy.placeParts(sheetsCopy, parts);

    // Update individual with result - THREAD SAFE ACCESS
    {
        boost::lock_guard<boost::mutex> lock(this->mutex_);
        auto& individuals = this->getPopulationObject().getIndividuals();
        // ← Now accessing through 'this', NOT by dangling reference
        if (safeIndex < individuals.size()) {
            individuals[safeIndex].fitness = result.fitness;
            individuals[safeIndex].area = result.area;
            individuals[safeIndex].mergedLength = result.mergedLength;
            individuals[safeIndex].placements = result.placements;
            individuals[safeIndex].processing = false;
        }
    }
});
```

**Key Changes**:
- Changed `&population` to `this` (captured safely)
- Changed `&worker` to `workerCopy` (value copy)
- Access population through `this->getPopulationObject()` instead of reference
- Use `safeIndex` instead of `index` for clarity

**Why**: References to `&population` and `&worker` become dangling if engine is destroyed before lambda executes. By capturing `this` and using getter methods, we ensure safe access.

---

### FIX #3: ParallelProcessor::enqueue() - Add Stopped Check

**File**: `/home/user/Deepnest/deepnest-cpp/include/deepnest/parallel/ParallelProcessor.h`  
**Lines**: 158-175

**BEFORE**:
```cpp
template<typename Func>
auto ParallelProcessor::enqueue(Func&& f) -> std::future<typename std::result_of<Func()>::type> {
    using return_type = typename std::result_of<Func()>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::forward<Func>(f)
    );

    std::future<return_type> result = task->get_future();

    boost::asio::post(ioContext_, [task]() {
        (*task)();
    });

    return result;
}
```

**AFTER**:
```cpp
template<typename Func>
auto ParallelProcessor::enqueue(Func&& f) -> std::future<typename std::result_of<Func()>::type> {
    using return_type = typename std::result_of<Func()>::type;

    // CRITICAL FIX: Check if processor is stopped before posting
    // Prevents posting to a stopped io_context (undefined behavior)
    if (stopped_) {
        auto promise = std::make_shared<std::promise<return_type>>();
        promise->set_exception(std::make_exception_ptr(
            std::runtime_error("ParallelProcessor is stopped or stopping")));
        return promise->get_future();
    }

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::forward<Func>(f)
    );

    std::future<return_type> result = task->get_future();

    boost::asio::post(ioContext_, [task]() {
        (*task)();
    });

    return result;
}
```

**Why**: Without this check, a race condition can occur where `stop()` is called while `enqueue()` is posting. Posting to a stopped io_context is undefined behavior.

---

### FIX #4: NestingEngine::initialize() - Reset ParallelProcessor

**File**: `/home/user/Deepnest/deepnest-cpp/src/engine/NestingEngine.cpp`  
**Lines**: 30-50

**BEFORE**:
```cpp
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
    // ... continues ...
}
```

**AFTER**:
```cpp
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
    
    // CRITICAL FIX: Also reset parallel processor to clear any lingering tasks
    // Ensures old tasks don't execute with stale references
    parallelProcessor_.reset();
    
    // ... continues with rest of initialization ...
}
```

**Why**: If `initialize()` is called twice without destroying the engine, old ParallelProcessor tasks may still be queued with references to the old population.

---

### FIX #5: DeepNestSolver::clear() - Destroy Engine

**File**: `/home/user/Deepnest/deepnest-cpp/src/DeepNestSolver.cpp`  
**Lines**: 107-110

**BEFORE**:
```cpp
void DeepNestSolver::clear() {
    clearParts();
    clearSheets();
}
```

**AFTER**:
```cpp
void DeepNestSolver::clear() {
    // CRITICAL FIX: Explicitly stop and destroy engine
    // Ensures all threads are properly joined and queued tasks are drained
    if (engine_) {
        engine_->stop();  // Gracefully stops engine
        engine_.reset();  // Destroys engine and clears unique_ptr
    }
    
    clearParts();
    clearSheets();
    
    running_ = false;  // Ensure running flag is cleared
}
```

**Why**: Without explicitly resetting the engine, the old NestingEngine remains in memory until the next `start()` call. This creates a window where old threads/tasks may still be executing while new ones are being created.

---

## Verification Commands

After applying fixes, compile and test:

```bash
# Rebuild
cd /home/user/Deepnest/deepnest-cpp
cmake --build build
ctest --verbose

# Run with valgrind to check for memory errors
valgrind --leak-check=full --show-leak-kinds=all ./build/TestApplication
```

---

## Testing Sequence

1. **Single Run Test**: Verify single nesting run works correctly
   - Load shapes
   - Click Start
   - Wait for completion or click Stop
   - No crash

2. **Double Run Test**: Verify second run doesn't crash
   - Load shapes
   - Click Start
   - Wait 5 seconds, click Stop
   - Click Start again ← **THIS IS WHERE IT CRASHES**
   - Should complete without segfault

3. **Rapid Fire Test**: Verify rapid start/stop cycles
   - Load shapes
   - Click Start, immediately click Stop
   - Click Start again immediately
   - Repeat 5 times
   - No crash

4. **Memory Check**:
   ```bash
   valgrind --leak-check=full --error-exitcode=1 ./build/bin/TestApplication
   # Should report "All heap blocks were freed"
   ```

---

## Expected Outcome

After all 5 fixes are applied:
- No segmentation fault on second run
- No dangling pointer accesses
- Thread pool properly cleaned up
- Task queue drained before destruction
- Memory properly freed on cleanup

