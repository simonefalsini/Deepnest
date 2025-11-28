#include "../../include/deepnest/threading/Clipper2ThreadGuard.h"

namespace deepnest {
namespace threading {

// Global mutex instance for Clipper2 operations
// CRITICAL: This must be a function-local static to avoid static initialization order fiasco
boost::mutex& getClipper2Mutex() {
    static boost::mutex clipper2Mutex;
    return clipper2Mutex;
}

} // namespace threading
} // namespace deepnest
