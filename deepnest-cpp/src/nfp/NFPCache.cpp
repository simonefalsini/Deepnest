#include "../../include/deepnest/nfp/NFPCache.h"
#include <sstream>
#include <iomanip>
#include <functional>

namespace deepnest {

// ========== NFPKey Methods ==========

std::string NFPCache::NFPKey::toString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    oss << "A" << idA << "_B" << idB
        << "_Ra" << rotationA << "_Rb" << rotationB
        << "_I" << (inside ? "1" : "0");
    return oss.str();
}

std::size_t NFPCache::NFPKeyHash::operator()(const NFPKey& key) const {
    // Combine hash values using boost::hash_combine pattern
    std::size_t seed = 0;

    auto hash_combine = [](std::size_t& seed, std::size_t hash) {
        seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    hash_combine(seed, std::hash<int>{}(key.idA));
    hash_combine(seed, std::hash<int>{}(key.idB));

    // Normalize rotations to avoid floating point precision issues
    int rotA = static_cast<int>(key.rotationA * 1000000.0);
    int rotB = static_cast<int>(key.rotationB * 1000000.0);
    hash_combine(seed, std::hash<int>{}(rotA));
    hash_combine(seed, std::hash<int>{}(rotB));

    hash_combine(seed, std::hash<bool>{}(key.inside));

    return seed;
}

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
    std::string keyStr = generateKey(key);
    return cache_.find(keyStr) != cache_.end();
}

bool NFPCache::find(const NFPKey& key, std::vector<Polygon>& result) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);

    std::string keyStr = generateKey(key);
    auto it = cache_.find(keyStr);

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

    std::string keyStr = generateKey(key);
    cache_[keyStr] = nfp;  // Store a copy of the NFP
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
