#ifndef DEEPNEST_DEBUG_CONFIG_H
#define DEEPNEST_DEBUG_CONFIG_H

/**
 * @file Debug Config.h
 * @brief Debug logging configuration for DeepNest C++ library
 *
 * Control debug logging output by setting these defines to 0 or 1.
 * This allows selective debugging of different subsystems without
 * excessive log output.
 *
 * Usage in code:
 * ```cpp
 * #include "DebugConfig.h"
 *
 * #if DEBUG_NFP
 *     std::cerr << "NFP: Calculating NFP for polygons A=" << A.id << ", B=" << B.id << std::endl;
 * #endif
 * ```
 */

// =============================================================================
// DEBUG LOGGING ENABLES
// =============================================================================

/**
 * @brief Enable NFP (No-Fit Polygon) calculation debug logging
 *
 * When enabled, logs detailed information about:
 * - MinkowskiSum::calculateNFP() calls
 * - Polygon IDs, point counts, inner/outer mode
 * - Boost.Polygon conversion steps
 * - Scale factors, transformations
 * - Success/failure of NFP generation
 *
 * Use for debugging NFP calculation issues.
 */
#ifndef DEBUG_NFP
#define DEBUG_NFP 0
#endif

/**
 * @brief Enable Genetic Algorithm debug logging
 *
 * When enabled, logs detailed information about:
 * - Population initialization
 * - Generation evolution
 * - Crossover and mutation operations
 * - Fitness calculations
 * - Best individual selection
 *
 * Use for debugging genetic algorithm behavior.
 */
#ifndef DEBUG_GA
#define DEBUG_GA 0
#endif

/**
 * @brief Enable nesting engine debug logging
 *
 * When enabled, logs detailed information about:
 * - Nesting engine lifecycle (start/stop/step)
 * - Part and sheet loading
 * - Placement operations
 * - Configuration changes
 * - Result generation and updates
 *
 * Use for debugging overall nesting process.
 */
#ifndef DEBUG_NESTING
#define DEBUG_NESTING 0
#endif

/**
 * @brief Enable thread management debug logging
 *
 * When enabled, logs detailed information about:
 * - Worker thread creation and termination
 * - Thread pool initialization
 * - Thread synchronization (locks, waits)
 * - Parallel processing coordination
 * - Thread-safe callback invocations
 *
 * Use for debugging threading issues, race conditions, deadlocks.
 * **CRITICAL for diagnosing protection faults and crashes on stop/exit.**
 */
#ifndef DEBUG_THREADS
#define DEBUG_THREADS 0
#endif

/**
 * @brief Enable memory management debug logging
 *
 * When enabled, logs detailed information about:
 * - Object construction/destruction (especially destructors)
 * - Memory allocations/deallocations
 * - Smart pointer lifecycle (unique_ptr, shared_ptr)
 * - Raw pointer operations (DANGEROUS - should be eliminated)
 * - Callback lifetime management
 *
 * Use for debugging memory leaks, use-after-free, dangling pointers.
 * **CRITICAL for diagnosing protection faults and access violations.**
 */
#ifndef DEBUG_MEMORY
#define DEBUG_MEMORY 0
#endif

/**
 * @brief Enable geometry operations debug logging
 *
 * When enabled, logs detailed information about:
 * - Polygon transformations (rotate, translate, scale)
 * - Bounding box calculations
 * - Area computations
 * - Point-in-polygon tests
 * - Convex hull operations
 *
 * Use for debugging geometric calculations.
 */
#ifndef DEBUG_GEOMETRY
#define DEBUG_GEOMETRY 0
#endif

/**
 * @brief Enable placement strategy debug logging
 *
 * When enabled, logs detailed information about:
 * - Placement position selection
 * - Gravity/bounding box/convex hull strategies
 * - Collision detection
 * - Overlap checking
 * - Merge line detection
 *
 * Use for debugging placement quality issues.
 */
#ifndef DEBUG_PLACEMENT
#define DEBUG_PLACEMENT 0
#endif

// =============================================================================
// DEBUG LOGGING MACROS
// =============================================================================

/**
 * @brief Conditional debug logging macro for NFP
 */
#if DEBUG_NFP
    #define LOG_NFP(msg) std::cerr << "[NFP] " << msg << std::endl
#else
    #define LOG_NFP(msg) ((void)0)
#endif

/**
 * @brief Conditional debug logging macro for GA
 */
#if DEBUG_GA
    #define LOG_GA(msg) std::cerr << "[GA] " << msg << std::endl
#else
    #define LOG_GA(msg) ((void)0)
#endif

/**
 * @brief Conditional debug logging macro for Nesting
 */
#if DEBUG_NESTING
    #define LOG_NESTING(msg) std::cerr << "[NEST] " << msg << std::endl
#else
    #define LOG_NESTING(msg) ((void)0)
#endif

/**
 * @brief Conditional debug logging macro for Threads
 */
#if DEBUG_THREADS
    #define LOG_THREAD(msg) std::cerr << "[THREAD] " << msg << std::endl
#else
    #define LOG_THREAD(msg) ((void)0)
#endif

/**
 * @brief Conditional debug logging macro for Memory
 */
#if DEBUG_MEMORY
    #define LOG_MEMORY(msg) std::cerr << "[MEM] " << msg << std::endl
#else
    #define LOG_MEMORY(msg) ((void)0)
#endif

/**
 * @brief Conditional debug logging macro for Geometry
 */
#if DEBUG_GEOMETRY
    #define LOG_GEOMETRY(msg) std::cerr << "[GEO] " << msg << std::endl
#else
    #define LOG_GEOMETRY(msg) ((void)0)
#endif

/**
 * @brief Conditional debug logging macro for Placement
 */
#if DEBUG_PLACEMENT
    #define LOG_PLACEMENT(msg) std::cerr << "[PLACE] " << msg << std::endl
#else
    #define LOG_PLACEMENT(msg) ((void)0)
#endif

#endif // DEEPNEST_DEBUG_CONFIG_H
