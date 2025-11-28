#ifndef DEEPNEST_CLIPPER2_THREAD_GUARD_H
#define DEEPNEST_CLIPPER2_THREAD_GUARD_H

#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

namespace deepnest {
namespace threading {

/**
 * @brief Global mutex to protect Clipper2 operations
 *
 * CRITICAL: Clipper2 library is NOT thread-safe. Internal structures
 * (particularly in ConvertHorzSegsToJoins and other clipper operations)
 * get corrupted when accessed concurrently from multiple threads.
 *
 * This mutex MUST be locked before ANY Clipper2 operation including:
 * - Clipper2Lib::MinkowskiSum
 * - Clipper2Lib::Union
 * - Clipper2Lib::Intersect
 * - Clipper2Lib::Difference
 * - Clipper2Lib::Inflate/Deflate
 * - Any ClipperD or Clipper64 execute operations
 *
 * USAGE:
 * ```cpp
 * #include <deepnest/threading/Clipper2ThreadGuard.h>
 *
 * // Before any Clipper2 operation:
 * {
 *     boost::lock_guard<boost::mutex> lock(deepnest::threading::getClipper2Mutex());
 *     // Safe to call Clipper2 here
 *     auto result = Clipper2Lib::MinkowskiSum(...);
 * }
 * // Lock automatically released
 * ```
 *
 * WHY THIS IS NEEDED:
 * Without this protection, you get crashes like:
 * "Exception thrown: write access violation. _Parent_proxy was 0x..."
 * in std::_Iterator_base12::_Adopt during ConvertHorzSegsToJoins
 *
 * The crash occurs because Clipper2 uses internal iterators and containers
 * that are not designed for concurrent access. When Thread A is iterating
 * through HorzSegments while Thread B modifies related structures, the
 * iterator becomes invalid leading to access violations.
 */
boost::mutex& getClipper2Mutex();

/**
 * @brief RAII guard for Clipper2 operations
 *
 * Simpler alternative to manual lock_guard usage.
 *
 * USAGE:
 * ```cpp
 * {
 *     Clipper2Guard guard;
 *     auto result = Clipper2Lib::MinkowskiSum(...);
 * }
 * ```
 */
class Clipper2Guard {
public:
    Clipper2Guard() : lock_(getClipper2Mutex()) {}

    // Non-copyable, non-movable
    Clipper2Guard(const Clipper2Guard&) = delete;
    Clipper2Guard& operator=(const Clipper2Guard&) = delete;
    Clipper2Guard(Clipper2Guard&&) = delete;
    Clipper2Guard& operator=(Clipper2Guard&&) = delete;

private:
    boost::lock_guard<boost::mutex> lock_;
};

} // namespace threading
} // namespace deepnest

#endif // DEEPNEST_CLIPPER2_THREAD_GUARD_H
