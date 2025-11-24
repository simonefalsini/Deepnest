#include "../../include/deepnest/nfp/NFPCache.h"
#include <sstream>
#include <iomanip>
#include <functional>

namespace deepnest {

// ========== NFPKey Methods ==========

// ========== NFPKey Methods ==========

std::string NFPCache::NFPKey::toString() const {
    // Optimized string construction
    std::string s;
    s.reserve(64); // Pre-allocate reasonable size
    
    s += "A";
    s += std::to_string(idA);
    s += "_B";
    s += std::to_string(idB);
    
    // Use std::to_string for rotation (default 6 decimal places)
    // This is faster than stringstream
    s += "_Ra";
    s += std::to_string(rotationA);
    s += "_Rb";
    s += std::to_string(rotationB);
    
    s += "_I";
    s += (inside ? "1" : "0");
    
    return s;
}

// NFPKeyHash::operator() is now inline in header

// ========== NFPCache Methods ==========

NFPCache::NFPCache()
    : hits_(0)
    , misses_(0)
{}

std::string NFPCache::generateKey(const NFPKey& key) {
    return key.toString();
}

bool NFPCache::has(const NFPKey& key) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return cache_.find(key) != cache_.end();
}

bool NFPCache::find(const NFPKey& key, std::vector<Polygon>& result) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);

    auto it = cache_.find(key);

    if (it != cache_.end()) {
        result = it->second;  // Copy the cached NFP
        ++hits_;
        return true;
    }

    ++misses_;
    return false;
}

void NFPCache::insert(const NFPKey& key, const std::vector<Polygon>& nfp) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);

    cache_[key] = nfp;  // Store a copy of the NFP
}

void NFPCache::clear() {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    cache_.clear();
}

size_t NFPCache::size() const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return cache_.size();
}

void NFPCache::resetStatistics() {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    hits_ = 0;
    misses_ = 0;
}

} // namespace deepnest
