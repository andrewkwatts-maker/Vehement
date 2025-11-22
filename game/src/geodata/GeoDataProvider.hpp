#pragma once

#include "GeoTypes.hpp"
#include <functional>
#include <future>
#include <memory>
#include <chrono>
#include <mutex>
#include <atomic>
#include <queue>

namespace Vehement {
namespace Geo {

// Forward declarations
class GeoTileCache;

/**
 * @brief Rate limiter for API calls
 */
class RateLimiter {
public:
    /**
     * @brief Constructor
     * @param requestsPerSecond Maximum requests per second
     * @param burstSize Maximum burst size
     */
    RateLimiter(double requestsPerSecond = 1.0, int burstSize = 3);

    /**
     * @brief Wait until a request can be made (blocking)
     * @return true if request can proceed, false if shutdown
     */
    bool Acquire();

    /**
     * @brief Try to acquire without blocking
     * @return true if request can proceed immediately
     */
    bool TryAcquire();

    /**
     * @brief Get time until next request can be made
     * @return Milliseconds until next available slot
     */
    int GetWaitTime() const;

    /**
     * @brief Set rate limit
     */
    void SetRate(double requestsPerSecond, int burstSize = 3);

    /**
     * @brief Shutdown the rate limiter
     */
    void Shutdown();

private:
    mutable std::mutex m_mutex;
    std::chrono::steady_clock::time_point m_lastRequest;
    double m_minInterval;  // Minimum interval between requests in seconds
    int m_burstSize;
    int m_availableTokens;
    std::atomic<bool> m_shutdown{false};
};

/**
 * @brief Query result callback
 */
using GeoQueryCallback = std::function<void(const GeoTileData& data, bool success, const std::string& error)>;

/**
 * @brief Progress callback for multi-tile queries
 */
using GeoProgressCallback = std::function<void(int completed, int total, const TileID& currentTile)>;

/**
 * @brief Abstract interface for geographic data providers
 *
 * Implementations fetch data from various sources (OSM, elevation APIs, etc.)
 * and provide caching and rate limiting.
 */
class GeoDataProvider {
public:
    virtual ~GeoDataProvider() = default;

    // ==========================================================================
    // Provider Information
    // ==========================================================================

    /**
     * @brief Get provider name
     */
    virtual std::string GetName() const = 0;

    /**
     * @brief Get provider version
     */
    virtual std::string GetVersion() const = 0;

    /**
     * @brief Check if provider is available (API accessible)
     */
    virtual bool IsAvailable() const = 0;

    /**
     * @brief Get provider attribution string
     */
    virtual std::string GetAttribution() const = 0;

    // ==========================================================================
    // Initialization
    // ==========================================================================

    /**
     * @brief Initialize the provider
     * @param configPath Path to configuration file
     * @return true on success
     */
    virtual bool Initialize(const std::string& configPath = "") = 0;

    /**
     * @brief Shutdown and cleanup
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Set the cache instance
     */
    virtual void SetCache(std::shared_ptr<GeoTileCache> cache) = 0;

    // ==========================================================================
    // Synchronous Queries
    // ==========================================================================

    /**
     * @brief Query data for a bounding box (synchronous)
     * @param bounds Geographic bounding box
     * @param options Query options
     * @return Tile data for the region
     */
    virtual GeoTileData Query(const GeoBoundingBox& bounds,
                              const GeoQueryOptions& options = {}) = 0;

    /**
     * @brief Query data for a specific tile (synchronous)
     * @param tile Tile identifier
     * @param options Query options
     * @return Tile data
     */
    virtual GeoTileData QueryTile(const TileID& tile,
                                   const GeoQueryOptions& options = {}) = 0;

    /**
     * @brief Query data around a point (synchronous)
     * @param center Center coordinate
     * @param radiusMeters Radius in meters
     * @param options Query options
     * @return Tile data for the region
     */
    virtual GeoTileData QueryRadius(const GeoCoordinate& center,
                                     double radiusMeters,
                                     const GeoQueryOptions& options = {}) = 0;

    // ==========================================================================
    // Asynchronous Queries
    // ==========================================================================

    /**
     * @brief Query data asynchronously
     * @param bounds Geographic bounding box
     * @param callback Completion callback
     * @param options Query options
     */
    virtual void QueryAsync(const GeoBoundingBox& bounds,
                            GeoQueryCallback callback,
                            const GeoQueryOptions& options = {}) = 0;

    /**
     * @brief Query tile asynchronously
     * @param tile Tile identifier
     * @param callback Completion callback
     * @param options Query options
     */
    virtual void QueryTileAsync(const TileID& tile,
                                 GeoQueryCallback callback,
                                 const GeoQueryOptions& options = {}) = 0;

    /**
     * @brief Query multiple tiles asynchronously
     * @param tiles List of tile identifiers
     * @param callback Per-tile completion callback
     * @param progress Progress callback
     * @param options Query options
     */
    virtual void QueryTilesAsync(const std::vector<TileID>& tiles,
                                  GeoQueryCallback callback,
                                  GeoProgressCallback progress = nullptr,
                                  const GeoQueryOptions& options = {}) = 0;

    /**
     * @brief Get future for async query
     */
    virtual std::future<GeoTileData> QueryFuture(const GeoBoundingBox& bounds,
                                                  const GeoQueryOptions& options = {}) = 0;

    // ==========================================================================
    // Rate Limiting
    // ==========================================================================

    /**
     * @brief Get the rate limiter
     */
    virtual RateLimiter& GetRateLimiter() = 0;

    /**
     * @brief Set rate limit
     * @param requestsPerSecond Maximum requests per second
     */
    virtual void SetRateLimit(double requestsPerSecond) = 0;

    /**
     * @brief Get current rate limit
     */
    virtual double GetRateLimit() const = 0;

    // ==========================================================================
    // Statistics
    // ==========================================================================

    /**
     * @brief Get total number of API requests made
     */
    virtual size_t GetRequestCount() const = 0;

    /**
     * @brief Get number of cache hits
     */
    virtual size_t GetCacheHits() const = 0;

    /**
     * @brief Get number of cache misses
     */
    virtual size_t GetCacheMisses() const = 0;

    /**
     * @brief Get total bytes downloaded
     */
    virtual size_t GetBytesDownloaded() const = 0;

    /**
     * @brief Reset statistics
     */
    virtual void ResetStatistics() = 0;

    // ==========================================================================
    // Offline Mode
    // ==========================================================================

    /**
     * @brief Enable/disable offline mode (cache only)
     */
    virtual void SetOfflineMode(bool offline) = 0;

    /**
     * @brief Check if in offline mode
     */
    virtual bool IsOfflineMode() const = 0;

    /**
     * @brief Pre-fetch tiles for offline use
     * @param tiles Tiles to fetch
     * @param progress Progress callback
     * @return Number of tiles successfully fetched
     */
    virtual int PrefetchTiles(const std::vector<TileID>& tiles,
                               GeoProgressCallback progress = nullptr) = 0;
};

/**
 * @brief Base implementation with common functionality
 */
class GeoDataProviderBase : public GeoDataProvider {
public:
    GeoDataProviderBase();
    virtual ~GeoDataProviderBase();

    // Rate limiting
    RateLimiter& GetRateLimiter() override { return m_rateLimiter; }
    void SetRateLimit(double requestsPerSecond) override;
    double GetRateLimit() const override { return m_rateLimit; }

    // Cache
    void SetCache(std::shared_ptr<GeoTileCache> cache) override { m_cache = cache; }

    // Statistics
    size_t GetRequestCount() const override { return m_requestCount; }
    size_t GetCacheHits() const override { return m_cacheHits; }
    size_t GetCacheMisses() const override { return m_cacheMisses; }
    size_t GetBytesDownloaded() const override { return m_bytesDownloaded; }
    void ResetStatistics() override;

    // Offline mode
    void SetOfflineMode(bool offline) override { m_offlineMode = offline; }
    bool IsOfflineMode() const override { return m_offlineMode; }

protected:
    /**
     * @brief Check cache for tile data
     */
    bool CheckCache(const TileID& tile, GeoTileData& outData);

    /**
     * @brief Store tile data in cache
     */
    void StoreInCache(const TileID& tile, const GeoTileData& data);

    /**
     * @brief Increment request count
     */
    void IncrementRequestCount() { ++m_requestCount; }

    /**
     * @brief Add to bytes downloaded
     */
    void AddBytesDownloaded(size_t bytes) { m_bytesDownloaded += bytes; }

    /**
     * @brief Increment cache hit count
     */
    void IncrementCacheHits() { ++m_cacheHits; }

    /**
     * @brief Increment cache miss count
     */
    void IncrementCacheMisses() { ++m_cacheMisses; }

    std::shared_ptr<GeoTileCache> m_cache;
    RateLimiter m_rateLimiter;
    double m_rateLimit = 1.0;
    std::atomic<bool> m_offlineMode{false};

    // Statistics (atomic for thread safety)
    std::atomic<size_t> m_requestCount{0};
    std::atomic<size_t> m_cacheHits{0};
    std::atomic<size_t> m_cacheMisses{0};
    std::atomic<size_t> m_bytesDownloaded{0};
};

/**
 * @brief HTTP response for providers
 */
struct HttpResponse {
    int statusCode = 0;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    std::string error;
    size_t downloadSize = 0;
    double downloadTime = 0.0;

    bool IsSuccess() const { return statusCode >= 200 && statusCode < 300; }
};

/**
 * @brief HTTP client interface for providers
 */
class HttpClient {
public:
    virtual ~HttpClient() = default;

    /**
     * @brief Perform GET request
     */
    virtual HttpResponse Get(const std::string& url,
                              const std::unordered_map<std::string, std::string>& headers = {}) = 0;

    /**
     * @brief Perform POST request
     */
    virtual HttpResponse Post(const std::string& url,
                               const std::string& body,
                               const std::string& contentType = "application/json",
                               const std::unordered_map<std::string, std::string>& headers = {}) = 0;

    /**
     * @brief Set timeout
     */
    virtual void SetTimeout(int timeoutSeconds) = 0;

    /**
     * @brief Set user agent
     */
    virtual void SetUserAgent(const std::string& userAgent) = 0;
};

/**
 * @brief CURL-based HTTP client implementation
 */
class CurlHttpClient : public HttpClient {
public:
    CurlHttpClient();
    ~CurlHttpClient();

    HttpResponse Get(const std::string& url,
                      const std::unordered_map<std::string, std::string>& headers = {}) override;

    HttpResponse Post(const std::string& url,
                       const std::string& body,
                       const std::string& contentType = "application/json",
                       const std::unordered_map<std::string, std::string>& headers = {}) override;

    void SetTimeout(int timeoutSeconds) override { m_timeout = timeoutSeconds; }
    void SetUserAgent(const std::string& userAgent) override { m_userAgent = userAgent; }

private:
    int m_timeout = 30;
    std::string m_userAgent = "Vehement2-GeoData/1.0";
};

} // namespace Geo
} // namespace Vehement
