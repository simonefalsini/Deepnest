#ifndef DEEPNEST_NFPCACHE_H
#define DEEPNEST_NFPCACHE_H

#include "../core/Polygon.h"
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <unordered_map>
#include <string>
#include <vector>

namespace deepnest {

/**
 * @brief Thread-safe cache for No-Fit Polygon (NFP) calculations
 *
 * The NFP cache stores previously calculated NFPs to avoid redundant
 * computations. Since NFP calculation is expensive, caching can
 * significantly improve performance.
 *
 * Based on window.db object from background.js
 */
class NFPCache {
public:
    /**
     * @brief Cache key for NFP lookup
     */
    struct NFPKey {
        int idA;           // Source ID of polygon A
        int idB;           // Source ID of polygon B
        double rotationA;  // Rotation of polygon A
        double rotationB;  // Rotation of polygon B
        bool inside;       // Whether B is inside A or outside

        NFPKey() : idA(-1), idB(-1), rotationA(0.0), rotationB(0.0), inside(false) {}

        NFPKey(int a, int b, double rotA, double rotB, bool ins = false)
            : idA(a), idB(b), rotationA(rotA), rotationB(rotB), inside(ins) {}

        bool operator==(const NFPKey& other) const {
            return idA == other.idA &&
                   idB == other.idB &&
                   Point::almostEqual(rotationA, other.rotationA) &&
                   Point::almostEqual(rotationB, other.rotationB) &&
                   inside == other.inside;
        }

        std::string toString() const;
    };

    /**
     * @brief Hash function for NFPKey
     */
    struct NFPKeyHash {
        std::size_t operator()(const NFPKey& key) const;
    };

private:
    // Thread-safe cache storage
    mutable boost::shared_mutex mutex_;
    std::unordered_map<std::string, std::vector<Polygon>> cache_;

    // Statistics
    mutable size_t hits_;
    mutable size_t misses_;

public:
    /**
     * @brief Constructor
     */
    NFPCache();

    /**
     * @brief Check if cache contains an entry
     *
     * @param key Cache key
     * @return True if entry exists
     */
    bool has(const NFPKey& key) const;

    /**
     * @brief Find NFP in cache
     *
     * @param key Cache key
     * @param result Output parameter for the NFP (if found)
     * @return True if found, false otherwise
     */
    bool find(const NFPKey& key, std::vector<Polygon>& result) const;

    /**
     * @brief Insert NFP into cache
     *
     * @param key Cache key
     * @param nfp NFP polygons to cache
     */
    void insert(const NFPKey& key, const std::vector<Polygon>& nfp);

    /**
     * @brief Clear the entire cache
     */
    void clear();

    /**
     * @brief Get number of cached entries
     */
    size_t size() const;

    /**
     * @brief Get cache hit count
     */
    size_t hitCount() const { return hits_; }

    /**
     * @brief Get cache miss count
     */
    size_t missCount() const { return misses_; }

    /**
     * @brief Get cache hit rate
     */
    double hitRate() const {
        size_t total = hits_ + misses_;
        return total > 0 ? static_cast<double>(hits_) / total : 0.0;
    }

    /**
     * @brief Reset statistics
     */
    void resetStatistics();

    // ========== Convenience methods ==========

    /**
     * @brief Check if cache contains entry (using parameters)
     */
    bool has(int idA, int idB, double rotA, double rotB, bool inside = false) const {
        return has(NFPKey(idA, idB, rotA, rotB, inside));
    }

    /**
     * @brief Find NFP in cache (using parameters)
     */
    bool find(int idA, int idB, double rotA, double rotB,
              std::vector<Polygon>& result, bool inside = false) const {
        return find(NFPKey(idA, idB, rotA, rotB, inside), result);
    }

    /**
     * @brief Insert NFP into cache (using parameters)
     */
    void insert(int idA, int idB, double rotA, double rotB,
                const std::vector<Polygon>& nfp, bool inside = false) {
        insert(NFPKey(idA, idB, rotA, rotB, inside), nfp);
    }

private:
    /**
     * @brief Generate string key from NFPKey
     */
    static std::string generateKey(const NFPKey& key);
};

} // namespace deepnest

#endif // DEEPNEST_NFPCACHE_H
