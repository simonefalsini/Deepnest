# CRITICAL SEGMENTATION FAULT ANALYSIS
## Second-Run Nesting Crash - Root Cause Analysis

---

## SUMMARY OF FINDINGS

The application crashes with a segmentation fault when nesting is run a second time due to **MULTIPLE CRITICAL MEMORY MANAGEMENT BUGS** related to:

1. **Unexecuted lambda tasks capturing dangling references**
2. **ParallelProcessor queue not flushed before destruction**
3. **Race condition between engine destruction and task execution**
4. **Missing safeguard checks in asio task posting**

---

## MOST LIKELY CRASH SEQUENCE

### First Run (Successful):
```
startNesting() 
  → DeepNestSolver::start() 
    → Creates engine_ = new NestingEngine#1
    → engine_->initialize() + engine_->start()
  → step() called repeatedly
    → NestingEngine::step() 
      → parallelProcessor_->processPopulation() 
        → Enqueues lambdas: [&population, &worker, ...](){ ... }
        → Tasks execute, update population
  → stopNesting() 
    → solver_->stop() 
      → engine_->stop() 
        → ParallelProcessor#1->stop() 
          → threads_.join_all() waits for active threads
          → ioContext_.stop() called
```

### Second Run (CRASH):
```
startNesting() 
  → DeepNestSolver::start() 
    → engine_ = std::make_unique<NestingEngine>()
      → OLD ENGINE DESTROYED: ~NestingEngine#1()
         → NestingEngine#1::stop()
         → parallelProcessor_#1->stop()
            → threads_.join_all() ← completes, threads exit
            → ioContext_.stop()   ← but queue may still have tasks!
    → NEW engine_ created: NestingEngine#2
      → New ParallelProcessor#2 created
  → step() called
    → MEANWHILE: Old queued task from engine#1 executes!
       → Lambda executes: [&population, ...](){ 
           individuals = population.getIndividuals() ← DANGLING REFERENCE!
         }
    → SEGMENTATION FAULT!
```

---

## CRITICAL BUGS IDENTIFIED

### BUG #1: ParallelProcessor Queue Not Flushed on Destruction
**Severity**: CRITICAL  
**File**: `/home/user/Deepnest/deepnest-cpp/src/parallel/ParallelProcessor.cpp`  
**Lines**: 36-53

```cpp
void ParallelProcessor::stop() {
    boost::lock_guard<boost::mutex> lock(mutex_);

    if (stopped_) {
        return;
    }

    stopped_ = true;
    workGuard_.reset();           // ← Allows io_context to finish
    threads_.join_all();          // ← Waits for threads to exit
    ioContext_.stop();            // ← Stops context BUT...
}
```

**PROBLEM**: `ioContext_.stop()` stops processing new events but **DOES NOT** flush or discard queued tasks. Unexecuted tasks remain in the queue and may execute AFTER the owning engine is destroyed.

**Example Scenario**:
- `processPopulation()` enqueues 10 tasks
- Only 3 complete before `stop()` is called
- 7 tasks remain in the queue
- `stop()` joins threads (which exit from `ioContext_.run()`)
- `ioContext_.stop()` is called
- Old engine destroyed (while 7 tasks still in queue)
- New engine created and new work posted
- Old queued tasks execute with references to destroyed engine → **CRASH**

### BUG #2: Lambda References to Population May Dangle
**Severity**: CRITICAL  
**File**: `/home/user/Deepnest/deepnest-cpp/src/parallel/ParallelProcessor.cpp`  
**Lines**: 114-148

```cpp
void ParallelProcessor::processPopulation(
    Population& population,
    const std::vector<Polygon>& sheets,
    PlacementWorker& worker,
    int maxConcurrent
) {
    ...
    enqueue([&population, index, sheetsCopy, &worker, individualCopy, this]() mutable {
        // ← Captures &population and &worker BY REFERENCE
        
        std::vector<Polygon> parts;
        for (size_t j = 0; j < individualCopy.placement.size(); ++j) {
            Polygon part = *individualCopy.placement[j];
            part.rotation = individualCopy.rotation[j];
            parts.push_back(part);
        }

        PlacementWorker::PlacementResult result = worker.placeParts(sheetsCopy, parts);
        // ^ If worker is destroyed before lambda executes → USE-AFTER-FREE

        {
            boost::lock_guard<boost::mutex> lock(this->mutex_);
            auto& individuals = population.getIndividuals();
            // ^ If population is destroyed before lambda executes → DANGLING REF
            
            if (index < individuals.size()) {
                individuals[index].fitness = result.fitness;
            }
        }
    });
}
```

**PROBLEM**: References `&population` and `&worker` are captured by reference. If the old engine is destroyed before the lambda executes, these become dangling references.

**Lifecycle**:
1. Lambda enqueued (captures `&population` from engine#1)
2. Engine#1 destruction begins
3. ParallelProcessor#1::stop() joins threads but task still in queue
4. Population member destroyed (but lambda still has reference)
5. New engine#2 created
6. Old queued lambda executes
7. `population.getIndividuals()` accesses freed memory → **SEGFAULT**

### BUG #3: No Safeguard Before Posting to io_context
**Severity**: HIGH  
**File**: `/home/user/Deepnest/deepnest-cpp/include/deepnest/parallel/ParallelProcessor.h`  
**Lines**: 158-175

```cpp
template<typename Func>
auto ParallelProcessor::enqueue(Func&& f) -> std::future<typename std::result_of<Func()>::type> {
    using return_type = typename std::result_of<Func()>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::forward<Func>(f)
    );

    std::future<return_type> result = task->get_future();

    // NO CHECK FOR stopped_ FLAG!
    boost::asio::post(ioContext_, [task]() {
        (*task)();
    });

    return result;
}
```

**PROBLEM**: No check if processor is stopped. Posting to a stopped `io_context_` may cause undefined behavior.

**RACE CONDITION**:
```
Thread A (step):           Thread B (stop):
processPopulation()
  for each individual
    if (not processing)
      individual.processing = true
      enqueue(...)         ← About to post task
                           stop()
                             stopped_ = true
                             workGuard_.reset()
                             threads_.join_all()
                             ioContext_.stop() ← Now stopped!
      boost::asio::post(ioContext_, ...) ← Post to STOPPED context!
```

### BUG #4: ParallelProcessor Not Reset in initialize()
**Severity**: MEDIUM  
**File**: `/home/user/Deepnest/deepnest-cpp/src/engine/NestingEngine.cpp`  
**Lines**: 30-50

```cpp
void NestingEngine::initialize(
    const std::vector<Polygon>& parts,
    const std::vector<int>& quantities,
    const std::vector<Polygon>& sheets,
    const std::vector<int>& sheetQuantities
) {
    // Clear previous state
    parts_.clear();
    partPointers_.clear();
    sheets_.clear();
    results_.clear();
    evaluationsCompleted_ = 0;
    geneticAlgorithm_.reset();  // ← GA is reset here
    
    // ... initialize population ...
    
    geneticAlgorithm_ = std::make_unique<GeneticAlgorithm>(partPointers_, config_);
}
```

**PROBLEM**: 
- `geneticAlgorithm_` is reset to nullptr (line 49)
- `parallelProcessor_` is **NOT** reset
- If `initialize()` called twice without destroying the engine, the old ParallelProcessor still exists but GA is replaced

**Why this matters**: While each engine creates its own ParallelProcessor, if initialize() is called multiple times, there's potential for queued tasks from old GA to reference new GA.

### BUG #5: No Guarantee All Tasks Complete Before stop() Returns
**Severity**: CRITICAL  
**File**: `/home/user/Deepnest/deepnest-cpp/src/parallel/ParallelProcessor.cpp`  
**Lines**: 36-53, and 55-66

```cpp
void ParallelProcessor::stop() {
    boost::lock_guard<boost::mutex> lock(mutex_);

    if (stopped_) {
        return;
    }

    stopped_ = true;
    workGuard_.reset();
    threads_.join_all();      // ← Waits for threads to EXIT
    ioContext_.stop();        // ← Stops processing
}

void ParallelProcessor::waitAll() {
    // Polls until io_context has no pending work
    while (!ioContext_.stopped()) {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
        if (ioContext_.poll() == 0) {
            break;
        }
    }
}
```

**PROBLEM**: `threads_.join_all()` waits for threads to EXIT the `ioContext_.run()` loop, but it does NOT wait for:
1. Currently executing tasks to complete
2. Queued but not-yet-started tasks to execute
3. Tasks in flight to finish

Thread sequence:
```
Worker Thread:
  ioContext_.run()
    while(true) {
      if (handler = get_next_handler()) {  ← May be executing here
        execute(handler)                   ← While main thread calls stop()
      } else if (stopped) {
        break;  ← Exit loop
      }
    }

Main Thread: stop()
  threads_.join_all()  ← Returns when thread exits run() loop
                       ← But task may still be executing!
  ioContext_.stop()    ← io_context destroyed while task running
```

---

## SPECIFIC CODE LOCATIONS NEEDING FIXES

| Issue | File | Lines | Type | Fix |
|-------|------|-------|------|-----|
| Queue not flushed | ParallelProcessor.cpp | 36-53 | Stop | Drain queue before ioContext_.stop() |
| Dangling references | ParallelProcessor.cpp | 114-148 | Lambda | Use value captures or ensure lifetime |
| No stopped check | ParallelProcessor.h | 158-175 | Enqueue | Check stopped_ before post() |
| ParallelProcessor not reset | NestingEngine.cpp | 30-50 | Init | Add parallelProcessor_.reset() |
| Tasks may be executing | ParallelProcessor.cpp | 36-53 | Stop | Implement proper task drainage |

---

## ROOT CAUSE HIERARCHY

```
Level 1 (Immediate Cause):
  → Unexecuted lambda from old engine executes with dangling references
  
Level 2 (Enabler):
  → ParallelProcessor queue not flushed before destruction
  → ioContext_.stop() leaves tasks in queue
  
Level 3 (Design Issues):
  → Lambda captures references instead of values/copies
  → No synchronization between engine destruction and task completion
  → No safeguard in enqueue() to check if processor stopped
  
Level 4 (Root):
  → Asynchronous task queue (boost::asio) has different lifetime than owning object
  → No proper shutdown protocol to drain queued tasks
```

---

## RECOMMENDED FIXES (IN PRIORITY ORDER)

### FIX #1: Drain Task Queue Before Stop (CRITICAL)
**Location**: ParallelProcessor::stop()

```cpp
void ParallelProcessor::stop() {
    boost::lock_guard<boost::mutex> lock(mutex_);

    if (stopped_) {
        return;
    }

    stopped_ = true;
    
    // CRITICAL FIX: Process remaining tasks before stopping context
    // This ensures lambdas execute before owner is destroyed
    while (ioContext_.poll() > 0) {
        // Drain remaining handlers
    }
    
    workGuard_.reset();           // Now allow graceful shutdown
    threads_.join_all();          // Wait for threads to exit
    ioContext_.stop();            // Stop accepting new work
}
```

### FIX #2: Add Safety Check Before Posting (HIGH)
**Location**: ParallelProcessor::enqueue()

```cpp
template<typename Func>
auto ParallelProcessor::enqueue(Func&& f) -> std::future<typename std::result_of<Func()>::type> {
    using return_type = typename std::result_of<Func()>::type;

    if (stopped_) {
        // Return failed future
        auto promise = std::make_shared<std::promise<return_type>>();
        promise->set_exception(std::make_exception_ptr(
            std::runtime_error("ParallelProcessor is stopped")));
        return promise->get_future();
    }

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::forward<Func>(f)
    );

    std::future<return_type> result = task->get_future();
    boost::asio::post(ioContext_, [task]() { (*task)(); });
    return result;
}
```

### FIX #3: Capture Values Instead of References (HIGH)
**Location**: ParallelProcessor::processPopulation()

```cpp
// Instead of capturing by reference:
// enqueue([&population, &worker, ...](){ ... });

// Capture by value or make safe copies:
// Option A: Deep copy population state needed
std::vector<size_t> processedIndices;
for (size_t i = 0; i < population.size(); ++i) {
    if (needs_processing) {
        processedIndices.push_back(i);
    }
}

enqueue([index, sheetsCopy, workerCopy, individualCopy, this]() mutable {
    // Use captured values, no dangling references
    auto& individuals = this->population_.getIndividuals();
    individuals[index].fitness = ...;
});
```

### FIX #4: Reset ParallelProcessor in initialize() (MEDIUM)
**Location**: NestingEngine::initialize()

```cpp
void NestingEngine::initialize(...) {
    // Clear previous state
    parts_.clear();
    partPointers_.clear();
    sheets_.clear();
    results_.clear();
    evaluationsCompleted_ = 0;
    
    // CRITICAL: Reset both GA and parallel processor
    geneticAlgorithm_.reset();
    parallelProcessor_.reset();  // ← ADD THIS LINE
    
    // ... rest of initialization ...
    
    // Recreate parallel processor
    parallelProcessor_ = std::make_unique<ParallelProcessor>(config_.threads);
}
```

### FIX #5: Explicitly Reset Engine on Clear (MEDIUM)
**Location**: DeepNestSolver::clear()

```cpp
void DeepNestSolver::clear() {
    clearParts();
    clearSheets();
    
    // Ensure engine is fully destroyed and recreated on next start()
    if (engine_) {
        engine_->stop();  // Gracefully stop any running operations
        engine_.reset();  // Destroy engine
    }
    running_ = false;
}
```

---

## TESTING STRATEGY TO VERIFY FIXES

```cpp
// Test: Double-run crash scenario
void testDoubleRunCrash() {
    DeepNestSolver solver;
    solver.setPopulationSize(5);
    solver.setSpacing(0.0);
    
    // Create parts and sheets
    std::vector<Polygon> parts = {createTestPolygon(), createTestPolygon()};
    std::vector<Polygon> sheets = {createTestSheet()};
    
    for (const auto& part : parts) solver.addPart(part, 2);
    for (const auto& sheet : sheets) solver.addSheet(sheet, 1);
    
    // First run
    solver.start(100);
    for (int i = 0; i < 10; i++) {
        if (!solver.step()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    solver.stop();
    
    // Second run - THIS IS WHERE IT CRASHES
    solver.clear();
    for (const auto& part : parts) solver.addPart(part, 2);
    for (const auto& sheet : sheets) solver.addSheet(sheet, 1);
    
    solver.start(100);
    for (int i = 0; i < 10; i++) {
        if (!solver.step()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    solver.stop();
    
    std::cout << "SUCCESS: Double-run completed without crash\n";
}
```

---

## PRIORITY SUMMARY

1. **CRITICAL**: Flush queue in ParallelProcessor::stop()
2. **CRITICAL**: Safe lifetime management for captured references  
3. **HIGH**: Add stopped_ check in enqueue()
4. **MEDIUM**: Reset ParallelProcessor in initialize()
5. **MEDIUM**: Explicitly reset engine in clear()

