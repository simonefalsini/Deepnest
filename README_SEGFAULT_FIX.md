# SEGMENTATION FAULT - INVESTIGATION & FIX GUIDE

## Quick Start

Your application crashes with a **segmentation fault on the second nesting run**. This is caused by **unexecuted lambda tasks** that capture dangling references to destroyed objects.

**5 critical bugs identified. All must be fixed.**

---

## Documentation Files

Start with these files in this order:

### 1. **INVESTIGATION_SUMMARY.txt** (QUICK START - 3 min read)
   - Executive summary of the problem
   - Quick reference bug table
   - Files needing changes
   - Effort estimate

### 2. **CRASH_DIAGRAM.txt** (VISUALIZATION - 5 min read)
   - Timeline showing when the crash happens
   - Visual representation of the queue issue
   - Task lifecycle diagram
   - Why the fix works

### 3. **FIXES_REQUIRED.md** (IMPLEMENTATION - 20 min read)
   - Exact code changes for each of 5 fixes
   - Before/after code samples
   - Detailed explanations
   - Verification commands

### 4. **SEGFAULT_ANALYSIS.md** (DEEP DIVE - 15 min read)
   - Comprehensive technical analysis
   - All 5 bugs explained in detail
   - Root cause hierarchy
   - Memory management analysis

### 5. **INVESTIGATION_COMPLETE.txt** (REFERENCE - 10 min read)
   - Complete investigation summary
   - Files analyzed
   - Evidence provided
   - Testing strategy

---

## The Problem in 30 Seconds

1. First run: Nesting engine creates a thread pool with a task queue
2. Tasks are enqueued and some execute, but not all
3. You click Stop: Engine tries to shut down but **doesn't drain the queue**
4. Second run: New engine created, old engine destroyed
5. **Old tasks in the queue still execute** but reference destroyed objects
6. **SEGMENTATION FAULT** when accessing dangling pointers

---

## The Fix in 30 Seconds

1. **Drain the queue** before stopping (`ParallelProcessor::stop()`)
2. **Change lambda captures** from references to safe values (`processPopulation()`)
3. **Add safety check** before posting tasks (`enqueue()`)
4. **Reset processor** in initialize (`NestingEngine::initialize()`)
5. **Destroy engine** in clear (`DeepNestSolver::clear()`)

---

## Files That Need Changes

| File | Function | Fix | Priority |
|------|----------|-----|----------|
| ParallelProcessor.cpp | stop() | Drain queue | CRITICAL |
| ParallelProcessor.cpp | processPopulation() | Fix captures | CRITICAL |
| ParallelProcessor.h | enqueue() | Add check | HIGH |
| NestingEngine.cpp | initialize() | Reset processor | MEDIUM |
| DeepNestSolver.cpp | clear() | Reset engine | MEDIUM |

---

## Step-by-Step Implementation

1. Read **FIXES_REQUIRED.md** carefully
2. Make Fix #1 (ParallelProcessor::stop) - CRITICAL
3. Make Fix #2 (Lambda captures) - CRITICAL
4. Make Fix #3 (enqueue check) - HIGH
5. Make Fix #4 (initialize reset) - MEDIUM
6. Make Fix #5 (clear reset) - MEDIUM
7. Compile and test

Estimated time: 1 hour (45 min coding + 15 min testing)

---

## How to Verify the Fix

### Test Scenario That Was Crashing

```cpp
solver.start(100);
solver.step();  // ... multiple times ...
solver.stop();

// CRASHES HERE without fixes:
solver.clear();
solver.start(100);  // <- Segmentation fault!
```

### After Fixes

This scenario should complete without crashing.

### Full Validation

```bash
cd /home/user/Deepnest/deepnest-cpp
cmake --build build
ctest --verbose

# Check for memory leaks
valgrind --leak-check=full ./build/bin/TestApplication
```

---

## Root Cause Explained

### The Core Issue

ParallelProcessor uses boost::asio::io_context which has its own lifecycle:

```
NestingEngine created
  └─ ParallelProcessor created
     └─ io_context created
        └─ worker threads created
           └─ Task queue initialized

[Nesting runs, tasks enqueued and executed]

NestingEngine::stop() called
  └─ ParallelProcessor::stop() called
     └─ workGuard_.reset()
     └─ threads_.join_all()  ← Threads exit, but...
     └─ ioContext_.stop()     ← Queue still has unexecuted tasks!

[Some tasks still in queue, waiting to execute]

NestingEngine destroyed
  └─ Population destroyed    ← But tasks still capture &population!
  └─ PlacementWorker destroyed ← But tasks still capture &worker!
  └─ ParallelProcessor destroyed
     └─ io_context destroyed

[New engine created, new queue starts]

[OLD task from OLD queue suddenly executes]
  └─ Task accesses &population ← DANGLING REFERENCE!
  └─ SEGMENTATION FAULT!
```

### Why This Happens

The io_context's task queue outlives the objects that feed it. It's an **asynchronous lifetime problem**.

### Why boost::asio::stop() Doesn't Help

`ioContext_.stop()` **prevents new tasks from being posted**, but it **does NOT drain existing tasks** from the queue. Unexecuted tasks remain and may execute later when:
- A new io_context calls `run()` and accidentally picks up old handlers
- The io_context is reused
- Race conditions allow old threads to consume from the queue

---

## Why This Matters

- **Critical for production**: Any app that runs nesting multiple times will crash
- **Race condition**: Timing-dependent, may not crash every time
- **Memory corruption**: Could corrupt data or crash unpredictably
- **Deployment blocker**: Cannot release with this bug

---

## Technical Details

### Bug Locations

**Bug #1: Queue Not Flushed**
- **File**: `/home/user/Deepnest/deepnest-cpp/src/parallel/ParallelProcessor.cpp`
- **Lines**: 36-53
- **Issue**: `ioContext_.stop()` called without draining queue
- **Fix**: Add `while (ioContext_.poll() > 0)` before `threads_.join_all()`

**Bug #2: Dangling References**
- **File**: `/home/user/Deepnest/deepnest-cpp/src/parallel/ParallelProcessor.cpp`
- **Lines**: 114-148
- **Issue**: Lambda captures `[&population, &worker, ...]` by reference
- **Fix**: Capture by value or use `this->getPopulationObject()`

**Bug #3: No Stopped Check**
- **File**: `/home/user/Deepnest/deepnest-cpp/include/deepnest/parallel/ParallelProcessor.h`
- **Lines**: 158-175
- **Issue**: `enqueue()` posts without checking `stopped_` flag
- **Fix**: Add early return if `stopped_`

**Bug #4: Processor Not Reset**
- **File**: `/home/user/Deepnest/deepnest-cpp/src/engine/NestingEngine.cpp`
- **Lines**: 30-50
- **Issue**: `geneticAlgorithm_.reset()` but not `parallelProcessor_.reset()`
- **Fix**: Add `parallelProcessor_.reset()`

**Bug #5: Engine Not Destroyed**
- **File**: `/home/user/Deepnest/deepnest-cpp/src/DeepNestSolver.cpp`
- **Lines**: 107-110
- **Issue**: `clear()` doesn't call `engine_.reset()`
- **Fix**: Add `engine_.reset()` after `stop()`

---

## Questions?

See the detailed analysis files:
- Technical details: SEGFAULT_ANALYSIS.md
- Implementation guide: FIXES_REQUIRED.md
- Visualization: CRASH_DIAGRAM.txt

---

## Summary

| Item | Value |
|------|-------|
| Problem | Segfault on 2nd nesting run |
| Root Cause | Unexecuted queue tasks with dangling refs |
| Bugs Found | 5 (2 CRITICAL, 1 HIGH, 2 MEDIUM) |
| Files to Fix | 4 files |
| Estimated Fix Time | 1 hour |
| Risk Level | Low (straightforward fixes) |
| Impact | Critical (blocks production) |

