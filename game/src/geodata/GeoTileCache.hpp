#pragma once

#include "GeoTypes.hpp"
#include <mutex>
#include <unordered_map>
#include <list>
#include <chrono>
#include <filesystem>

namespace Vehement {
namespace Geo {

/**
 * @brief Cache configuration
 */
struct CacheConfig {
    std::string cachePath = "geodata_cache";  // Cache directory
    size_t maxMemoryCacheMB = 256;            // Max memory cache size
    size_t maxDiskCacheMB = 1024;             // Max disk cache size
    int defaultExpiryHours = 24 * 7;          // 1 week
    bool enableDiskCache = true;
    bool enableCompression = true;

    /**
     * @brief Load from file
     */
    static CacheConfig LoadFromFile(const std::string& path);

    /**
     * @brief Save to file
     */
    void SaveToFile(const std::string& path) const;
};

/**
 * @brief Cache entry metadata
 */
struct CacheEntry {
    TileID tileId;
    int64_t fetchTimestamp = 0;              // When data was fetched
    int64_t expiryTimestamp = 0;             // When data expires
    int64_t lastAccessTime = 0;              // Last access time (for LRU)
    size_t dataSize = 0;                     // Size in bytes
    std::string diskPath;                    // Path to disk cache file
    bool inMemory = false;                   // Is data in memory cache
    bool onDisk = false;                     // Is data on disk

    /**
     * @brief Check if entry is expired
     */
    bool IsExpired() const;

    /**
     * @brief Update last access time
     */
    void Touch();
};

/**
 * @brief Geographic tile cache
 *
 * Provides multi-level caching for geographic data:
 * - Memory cache (LRU eviction)
 * - Disk cache (persistent)
 * - Expiration and refresh policies
 * - Offline mode support
 */
class GeoTileCache {
public:
    GeoTileCache();
    ~GeoTileCache();

    /**
     * @brief Initialize cache
     */
    bool Initialize(const std::string& configPath = "");

    /**
     * @brief Initialize with config
     */
    bool Initialize(const CacheConfig& config);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Get configuration
     */
    const CacheConfig& GetConfig() const { return m_config; }

    // ==========================================================================
    // Cache Operations
    // ==========================================================================

    /**
     * @brief Get tile data from cache
     * @param tile Tile identifier
     * @param outData Output data
     * @return true if found and not expired
     */
    bool Get(const TileID& tile, GeoTileData& outData);

    /**
     * @brief Put tile data into cache
     * @param tile Tile identifier
     * @param data Tile data to cache
     */
    void Put(const TileID& tile, const GeoTileData& data);

    /**
     * @brief Check if tile is in cache
     */
    bool Contains(const TileID& tile) const;

    /**
     * @brief Check if tile is cached and not expired
     */
    bool IsValid(const TileID& tile) const;

    /**
     * @brief Remove tile from cache
     */
    void Remove(const TileID& tile);

    /**
     * @brief Clear all cache
     */
    void Clear();

    /**
     * @brief Clear expired entries
     */
    int ClearExpired();

    // ==========================================================================
    // Batch Operations
    // ==========================================================================

    /**
     * @brief Get multiple tiles
     * @return Map of found tiles
     */
    std::unordered_map<std::string, GeoTileData> GetMultiple(
        const std::vector<TileID>& tiles);

    /**
     * @brief Put multiple tiles
     */
    void PutMultiple(const std::vector<std::pair<TileID, GeoTileData>>& tiles);

    /**
     * @brief Get list of cached tiles
     */
    std::vector<TileID> GetCachedTiles() const;

    /**
     * @brief Get tiles in bounds
     */
    std::vector<TileID> GetTilesInBounds(const GeoBoundingBox& bounds, int zoom) const;

    // ==========================================================================
    // Memory Management
    // ==========================================================================

    /**
     * @brief Get current memory usage (bytes)
     */
    size_t GetMemoryUsage() const { return m_currentMemoryBytes; }

    /**
     * @brief Get current disk usage (bytes)
     */
    size_t GetDiskUsage() const { return m_currentDiskBytes; }

    /**
     * @brief Get number of entries in memory cache
     */
    size_t GetMemoryCacheSize() const { return m_memoryCache.size(); }

    /**
     * @brief Get number of entries in disk cache
     */
    size_t GetDiskCacheSize() const;

    /**
     * @brief Evict entries to free memory
     * @param targetBytes Target memory size
     */
    void EvictMemory(size_t targetBytes);

    /**
     * @brief Evict entries to free disk space
     * @param targetBytes Target disk size
     */
    void EvictDisk(size_t targetBytes);

    // ==========================================================================
    // Offline Support
    // ==========================================================================

    /**
     * @brief Pre-cache tiles for offline use
     * @param tiles Tiles to cache
     * @param provider Data provider to fetch from
     */
    template<typename Provider>
    int Prefetch(const std::vector<TileID>& tiles, Provider& provider);

    /**
     * @brief Export cache for offline bundle
     */
    bool ExportToBundle(const std::string& bundlePath,
                        const std::vector<TileID>& tiles);

    /**
     * @brief Import offline bundle
     */
    bool ImportFromBundle(const std::string& bundlePath);

    /**
     * @brief Get coverage info
     */
    struct CoverageInfo {
        GeoBoundingBox bounds;
        int minZoom = 0;
        int maxZoom = 0;
        size_t tileCount = 0;
        size_t totalSize = 0;
    };
    CoverageInfo GetCoverageInfo() const;

    // ==========================================================================
    // Statistics
    // ==========================================================================

    /**
     * @brief Get cache hit count
     */
    size_t GetHitCount() const { return m_hitCount; }

    /**
     * @brief Get cache miss count
     */
    size_t GetMissCount() const { return m_missCount; }

    /**
     * @brief Get hit rate (0-1)
     */
    float GetHitRate() const;

    /**
     * @brief Reset statistics
     */
    void ResetStatistics();

private:
    /**
     * @brief Load tile from disk cache
     */
    bool LoadFromDisk(const TileID& tile, GeoTileData& outData);

    /**
     * @brief Save tile to disk cache
     */
    bool SaveToDisk(const TileID& tile, const GeoTileData& data);

    /**
     * @brief Get disk cache path for tile
     */
    std::string GetDiskPath(const TileID& tile) const;

    /**
     * @brief Serialize tile data to JSON
     */
    std::string SerializeTileData(const GeoTileData& data) const;

    /**
     * @brief Deserialize tile data from JSON
     */
    bool DeserializeTileData(const std::string& json, GeoTileData& outData) const;

    /**
     * @brief Compress data
     */
    std::vector<uint8_t> Compress(const std::string& data) const;

    /**
     * @brief Decompress data
     */
    std::string Decompress(const std::vector<uint8_t>& data) const;

    /**
     * @brief Estimate data size in bytes
     */
    size_t EstimateSize(const GeoTileData& data) const;

    /**
     * @brief Move entry to front of LRU list
     */
    void TouchLRU(const TileID& tile);

    CacheConfig m_config;
    mutable std::mutex m_mutex;

    // Memory cache (LRU)
    std::unordered_map<std::string, GeoTileData> m_memoryCache;
    std::list<std::string> m_lruList;  // Most recent at front
    std::unordered_map<std::string, std::list<std::string>::iterator> m_lruMap;

    // Cache metadata
    std::unordered_map<std::string, CacheEntry> m_entries;

    // Size tracking
    std::atomic<size_t> m_currentMemoryBytes{0};
    std::atomic<size_t> m_currentDiskBytes{0};

    // Statistics
    std::atomic<size_t> m_hitCount{0};
    std::atomic<size_t> m_missCount{0};
};

// Template implementation
template<typename Provider>
int GeoTileCache::Prefetch(const std::vector<TileID>& tiles, Provider& provider) {
    int count = 0;

    for (const auto& tile : tiles) {
        if (!Contains(tile) || !IsValid(tile)) {
            GeoTileData data = provider.QueryTile(tile);
            if (data.status == DataStatus::Loaded) {
                Put(tile, data);
                ++count;
            }
        }
    }

    return count;
}

} // namespace Geo
} // namespace Vehement
