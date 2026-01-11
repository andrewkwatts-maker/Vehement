#pragma once

/**
 * @file SDFCache.hpp
 * @brief Disk-based caching system for Signed Distance Field data
 *
 * This system provides efficient persistent storage for precomputed SDF data:
 * - Binary format with versioned header
 * - Optional zlib compression
 * - Multiple LOD levels per cache entry
 * - Cache key based on source mesh hash
 * - Automatic cache invalidation on mesh changes
 * - Memory-mapped loading for large SDFs
 * - Background generation with progress callbacks
 *
 * Example usage:
 * @code
 * SDFCacheConfig config;
 * config.cacheDirectory = "cache/sdf";
 * config.compressionLevel = 6;
 *
 * SDFCache cache;
 * cache.Initialize(config);
 *
 * // Cache SDF from mesh
 * SDFCacheParams params;
 * params.resolution = 64;
 * params.generateLODs = true;
 *
 * auto result = cache.CacheSDF(mesh, params, [](float progress) {
 *     std::cout << "Progress: " << (progress * 100) << "%" << std::endl;
 * });
 *
 * // Later, load from cache
 * if (cache.IsCached(result.cacheKey)) {
 *     auto entry = cache.LoadSDF(result.cacheKey);
 *     // Use entry->GetData(), entry->GetBounds(), etc.
 * }
 * @endcode
 */

#include "SDFModel.hpp"
#include "../graphics/Mesh.hpp"
#include "../graphics/MeshToSDFConverter.hpp"
#include "../core/JobSystem.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace Nova {

// Forward declarations
class SDFCache;

// =============================================================================
// Cache Format Constants
// =============================================================================

namespace SDFCacheFormat {
    constexpr uint32_t MAGIC = 0x53444643;      // "SDFC"
    constexpr uint32_t VERSION = 1;
    constexpr size_t MAX_LOD_LEVELS = 8;
    constexpr size_t HEADER_SIZE = 256;         // Reserved header space
}

// =============================================================================
// Cache Configuration
// =============================================================================

/**
 * @brief Configuration for the SDF cache system
 */
struct SDFCacheConfig {
    /// Directory where cache files are stored
    std::string cacheDirectory = "cache/sdf";

    /// Compression level (0 = none, 1-9 = zlib levels, 6 = default)
    int compressionLevel = 6;

    /// Maximum total cache size in bytes (0 = unlimited)
    size_t maxCacheSize = 0;

    /// Maximum age for cache entries in seconds (0 = never expire)
    uint64_t maxCacheAge = 0;

    /// Enable memory-mapped file loading for large SDFs
    bool enableMemoryMapping = true;

    /// Minimum SDF size in bytes to use memory mapping
    size_t memoryMappingThreshold = 16 * 1024 * 1024;  // 16 MB

    /// Enable background generation
    bool enableBackgroundGeneration = true;

    /// Number of worker threads for background generation (0 = auto)
    int backgroundWorkerThreads = 0;

    /// Enable automatic cache cleanup
    bool enableAutoCleanup = true;

    /// Cache cleanup interval in seconds
    float cleanupInterval = 300.0f;  // 5 minutes
};

// =============================================================================
// Cache Entry Data
// =============================================================================

/**
 * @brief LOD level data within a cache entry
 */
struct SDFCacheLOD {
    /// Resolution for this LOD level
    int resolution = 0;

    /// Distance values (size = resolution^3)
    std::vector<float> distances;

    /// Material IDs (optional, size = resolution^3)
    std::vector<uint16_t> materials;

    /// Compressed size in bytes (0 if not compressed)
    size_t compressedSize = 0;

    /// Uncompressed size in bytes
    size_t uncompressedSize = 0;

    /// Get voxel count
    [[nodiscard]] size_t GetVoxelCount() const {
        return static_cast<size_t>(resolution) * resolution * resolution;
    }

    /// Sample distance at normalized coordinates (0-1)
    [[nodiscard]] float SampleDistance(const glm::vec3& uvw) const;

    /// Sample distance at integer coordinates
    [[nodiscard]] float SampleDistanceAt(int x, int y, int z) const;
};

/**
 * @brief Complete cache entry with header and LOD data
 */
class SDFCacheEntry {
public:
    SDFCacheEntry() = default;
    ~SDFCacheEntry() = default;

    // Move-only
    SDFCacheEntry(const SDFCacheEntry&) = delete;
    SDFCacheEntry& operator=(const SDFCacheEntry&) = delete;
    SDFCacheEntry(SDFCacheEntry&&) noexcept = default;
    SDFCacheEntry& operator=(SDFCacheEntry&&) noexcept = default;

    // =========================================================================
    // Properties
    // =========================================================================

    /// Get cache key (hash)
    [[nodiscard]] uint64_t GetCacheKey() const { return m_cacheKey; }

    /// Get source mesh hash (for invalidation)
    [[nodiscard]] uint64_t GetSourceHash() const { return m_sourceHash; }

    /// Get world-space bounds
    [[nodiscard]] const glm::vec3& GetBoundsMin() const { return m_boundsMin; }
    [[nodiscard]] const glm::vec3& GetBoundsMax() const { return m_boundsMax; }

    /// Get bounds as pair
    [[nodiscard]] std::pair<glm::vec3, glm::vec3> GetBounds() const {
        return {m_boundsMin, m_boundsMax};
    }

    /// Get bounds center
    [[nodiscard]] glm::vec3 GetCenter() const {
        return (m_boundsMin + m_boundsMax) * 0.5f;
    }

    /// Get bounds size
    [[nodiscard]] glm::vec3 GetSize() const {
        return m_boundsMax - m_boundsMin;
    }

    /// Get creation timestamp
    [[nodiscard]] std::chrono::system_clock::time_point GetCreationTime() const {
        return m_creationTime;
    }

    /// Get last access timestamp
    [[nodiscard]] std::chrono::system_clock::time_point GetLastAccessTime() const {
        return m_lastAccessTime;
    }

    /// Get file path
    [[nodiscard]] const std::string& GetFilePath() const { return m_filePath; }

    // =========================================================================
    // LOD Access
    // =========================================================================

    /// Get number of LOD levels
    [[nodiscard]] size_t GetLODCount() const { return m_lodLevels.size(); }

    /// Get LOD level by index
    [[nodiscard]] const SDFCacheLOD* GetLOD(size_t level) const {
        return (level < m_lodLevels.size()) ? &m_lodLevels[level] : nullptr;
    }

    /// Get best LOD for a given screen size (in pixels)
    [[nodiscard]] const SDFCacheLOD* GetLODForScreenSize(float screenSize) const;

    /// Get highest resolution LOD
    [[nodiscard]] const SDFCacheLOD* GetHighestLOD() const {
        return m_lodLevels.empty() ? nullptr : &m_lodLevels[0];
    }

    /// Get lowest resolution LOD
    [[nodiscard]] const SDFCacheLOD* GetLowestLOD() const {
        return m_lodLevels.empty() ? nullptr : &m_lodLevels.back();
    }

    /// Get all LOD resolutions
    [[nodiscard]] std::vector<int> GetLODResolutions() const;

    // =========================================================================
    // SDF Evaluation
    // =========================================================================

    /// Evaluate SDF at world position (uses highest LOD)
    [[nodiscard]] float EvaluateSDF(const glm::vec3& worldPos) const;

    /// Evaluate SDF at world position with specific LOD
    [[nodiscard]] float EvaluateSDF(const glm::vec3& worldPos, size_t lodLevel) const;

    /// Calculate normal at world position
    [[nodiscard]] glm::vec3 CalculateNormal(const glm::vec3& worldPos, float epsilon = 0.001f) const;

    // =========================================================================
    // Memory & Stats
    // =========================================================================

    /// Get total memory usage in bytes
    [[nodiscard]] size_t GetMemoryUsage() const;

    /// Get file size on disk in bytes
    [[nodiscard]] size_t GetFileSize() const { return m_fileSize; }

    /// Check if entry is memory-mapped
    [[nodiscard]] bool IsMemoryMapped() const { return m_isMemoryMapped; }

    /// Check if entry is loaded
    [[nodiscard]] bool IsLoaded() const { return !m_lodLevels.empty(); }

    /// Check if entry is compressed
    [[nodiscard]] bool IsCompressed() const { return m_compressionLevel > 0; }

private:
    friend class SDFCache;

    // Header data
    uint64_t m_cacheKey = 0;
    uint64_t m_sourceHash = 0;
    glm::vec3 m_boundsMin{0.0f};
    glm::vec3 m_boundsMax{0.0f};
    int m_compressionLevel = 0;
    std::chrono::system_clock::time_point m_creationTime;
    std::chrono::system_clock::time_point m_lastAccessTime;
    std::string m_filePath;
    size_t m_fileSize = 0;
    bool m_isMemoryMapped = false;

    // LOD data (sorted by resolution, highest first)
    std::vector<SDFCacheLOD> m_lodLevels;

    // Memory-mapped data (if enabled)
    void* m_mappedData = nullptr;
    size_t m_mappedSize = 0;

    // Internal methods
    void UpdateAccessTime();
    glm::vec3 WorldToLocal(const glm::vec3& worldPos) const;
    glm::vec3 LocalToUVW(const glm::vec3& localPos) const;
};

// =============================================================================
// Cache Generation Parameters
// =============================================================================

/**
 * @brief Parameters for SDF cache generation
 */
struct SDFCacheParams {
    /// Base resolution for highest LOD
    int resolution = 64;

    /// Generate LOD levels
    bool generateLODs = true;

    /// Number of LOD levels to generate
    int lodLevelCount = 4;

    /// LOD resolution divisor (each level is resolution / (lodDivisor ^ level))
    int lodDivisor = 2;

    /// Minimum LOD resolution
    int minLODResolution = 8;

    /// Padding around bounds (as fraction of bounds size)
    float boundsPadding = 0.1f;

    /// Enable material storage
    bool storeMaterials = false;

    /// Mesh to SDF conversion settings
    ConversionSettings conversionSettings;

    /// Validate parameters
    [[nodiscard]] bool Validate() const;

    /// Get list of LOD resolutions
    [[nodiscard]] std::vector<int> GetLODResolutions() const;
};

/**
 * @brief Result of cache operation
 */
struct SDFCacheResult {
    /// Operation succeeded
    bool success = false;

    /// Error message (if failed)
    std::string errorMessage;

    /// Cache key (hash)
    uint64_t cacheKey = 0;

    /// File path
    std::string filePath;

    /// File size in bytes
    size_t fileSize = 0;

    /// Generation time in milliseconds
    float generationTimeMs = 0.0f;

    /// Compression ratio (compressed/uncompressed)
    float compressionRatio = 1.0f;
};

/**
 * @brief Progress callback type
 */
using SDFCacheProgressCallback = std::function<void(float progress)>;

// =============================================================================
// Cache Statistics
// =============================================================================

/**
 * @brief Cache statistics
 */
struct SDFCacheStats {
    /// Number of cache entries
    size_t entryCount = 0;

    /// Total cache size in bytes
    size_t totalSize = 0;

    /// Cache hits
    uint64_t hits = 0;

    /// Cache misses
    uint64_t misses = 0;

    /// Cache evictions
    uint64_t evictions = 0;

    /// Average generation time in ms
    float avgGenerationTimeMs = 0.0f;

    /// Hit rate (0-1)
    [[nodiscard]] float GetHitRate() const {
        uint64_t total = hits + misses;
        return total > 0 ? static_cast<float>(hits) / total : 0.0f;
    }
};

// =============================================================================
// SDF Cache System
// =============================================================================

/**
 * @brief Disk-based SDF caching system
 *
 * Provides persistent storage for precomputed SDF data with support for:
 * - Multiple LOD levels
 * - Compression
 * - Memory-mapped loading
 * - Background generation
 * - Automatic cache invalidation
 */
class SDFCache {
public:
    SDFCache();
    ~SDFCache();

    // Non-copyable, non-movable
    SDFCache(const SDFCache&) = delete;
    SDFCache& operator=(const SDFCache&) = delete;
    SDFCache(SDFCache&&) = delete;
    SDFCache& operator=(SDFCache&&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the cache system
     * @param config Cache configuration
     * @return true if initialization succeeded
     */
    bool Initialize(const SDFCacheConfig& config = {});

    /**
     * @brief Shutdown the cache system
     */
    void Shutdown();

    /**
     * @brief Check if cache is initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Get configuration
     */
    [[nodiscard]] const SDFCacheConfig& GetConfig() const { return m_config; }

    // =========================================================================
    // Cache Operations
    // =========================================================================

    /**
     * @brief Cache SDF data from mesh
     * @param mesh Source mesh
     * @param params Cache generation parameters
     * @param progressCallback Optional progress callback
     * @return Cache result
     */
    SDFCacheResult CacheSDF(
        const Mesh& mesh,
        const SDFCacheParams& params = {},
        SDFCacheProgressCallback progressCallback = nullptr);

    /**
     * @brief Cache SDF data from SDF model
     * @param model Source SDF model
     * @param params Cache generation parameters
     * @param progressCallback Optional progress callback
     * @return Cache result
     */
    SDFCacheResult CacheSDF(
        const SDFModel& model,
        const SDFCacheParams& params = {},
        SDFCacheProgressCallback progressCallback = nullptr);

    /**
     * @brief Cache SDF data from evaluation function
     * @param sdfFunc SDF evaluation function
     * @param boundsMin Bounds minimum
     * @param boundsMax Bounds maximum
     * @param sourceHash Source data hash (for invalidation)
     * @param params Cache generation parameters
     * @param progressCallback Optional progress callback
     * @return Cache result
     */
    SDFCacheResult CacheSDF(
        const std::function<float(const glm::vec3&)>& sdfFunc,
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax,
        uint64_t sourceHash,
        const SDFCacheParams& params = {},
        SDFCacheProgressCallback progressCallback = nullptr);

    /**
     * @brief Cache SDF data asynchronously
     * @param mesh Source mesh
     * @param params Cache generation parameters
     * @param completionCallback Called when generation completes
     * @param progressCallback Optional progress callback
     * @return Job handle for tracking
     */
    JobHandle CacheSDFAsync(
        const Mesh& mesh,
        const SDFCacheParams& params,
        std::function<void(SDFCacheResult)> completionCallback,
        SDFCacheProgressCallback progressCallback = nullptr);

    /**
     * @brief Load cached SDF data
     * @param cacheKey Cache key
     * @return Cache entry (nullptr if not found)
     */
    std::unique_ptr<SDFCacheEntry> LoadSDF(uint64_t cacheKey);

    /**
     * @brief Load cached SDF data by file path
     * @param filePath Cache file path
     * @return Cache entry (nullptr if not found)
     */
    std::unique_ptr<SDFCacheEntry> LoadSDFFromFile(const std::string& filePath);

    /**
     * @brief Check if SDF is cached
     * @param cacheKey Cache key
     * @return true if cached
     */
    [[nodiscard]] bool IsCached(uint64_t cacheKey) const;

    /**
     * @brief Check if SDF is cached and valid
     * @param cacheKey Cache key
     * @param sourceHash Current source hash (for invalidation check)
     * @return true if cached and valid
     */
    [[nodiscard]] bool IsCachedAndValid(uint64_t cacheKey, uint64_t sourceHash) const;

    /**
     * @brief Get cache file path for a key
     * @param cacheKey Cache key
     * @return File path
     */
    [[nodiscard]] std::string GetCacheFilePath(uint64_t cacheKey) const;

    // =========================================================================
    // Cache Management
    // =========================================================================

    /**
     * @brief Remove cache entry
     * @param cacheKey Cache key
     * @return true if removed
     */
    bool RemoveEntry(uint64_t cacheKey);

    /**
     * @brief Remove all expired entries
     * @return Number of entries removed
     */
    size_t CleanupExpired();

    /**
     * @brief Remove oldest entries to fit within max size
     * @param targetSize Target cache size (0 = use config max)
     * @return Number of entries removed
     */
    size_t EnforceMaxSize(size_t targetSize = 0);

    /**
     * @brief Clear entire cache
     * @return Number of entries removed
     */
    size_t ClearAll();

    /**
     * @brief Refresh cache index from disk
     */
    void RefreshIndex();

    /**
     * @brief Update function (call periodically for auto-cleanup)
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Hash Functions
    // =========================================================================

    /**
     * @brief Compute hash for mesh data
     * @param mesh Source mesh
     * @return Hash value
     */
    [[nodiscard]] static uint64_t ComputeMeshHash(const Mesh& mesh);

    /**
     * @brief Compute hash for SDF model
     * @param model Source SDF model
     * @return Hash value
     */
    [[nodiscard]] static uint64_t ComputeModelHash(const SDFModel& model);

    /**
     * @brief Compute cache key from source hash and parameters
     * @param sourceHash Source data hash
     * @param params Cache parameters
     * @return Cache key
     */
    [[nodiscard]] static uint64_t ComputeCacheKey(
        uint64_t sourceHash,
        const SDFCacheParams& params);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get cache statistics
     */
    [[nodiscard]] const SDFCacheStats& GetStats() const { return m_stats; }

    /**
     * @brief Get list of all cached entries (metadata only)
     */
    [[nodiscard]] std::vector<std::pair<uint64_t, size_t>> GetCachedEntries() const;

    /**
     * @brief Get total cache size in bytes
     */
    [[nodiscard]] size_t GetTotalCacheSize() const;

    /**
     * @brief Get cache entry count
     */
    [[nodiscard]] size_t GetEntryCount() const;

private:
    // Internal cache index entry
    struct CacheIndexEntry {
        uint64_t cacheKey = 0;
        uint64_t sourceHash = 0;
        std::string filePath;
        size_t fileSize = 0;
        std::chrono::system_clock::time_point creationTime;
        std::chrono::system_clock::time_point lastAccessTime;
    };

    // Initialize cache directory
    bool InitializeDirectory();

    // Load cache index from disk
    bool LoadIndex();

    // Save cache index to disk
    bool SaveIndex();

    // Write cache entry to disk
    bool WriteCacheFile(
        const std::string& filePath,
        uint64_t cacheKey,
        uint64_t sourceHash,
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax,
        const std::vector<SDFCacheLOD>& lodLevels);

    // Read cache entry from disk
    bool ReadCacheFile(
        const std::string& filePath,
        SDFCacheEntry& entry);

    // Memory-mapped file loading
    bool LoadMemoryMapped(const std::string& filePath, SDFCacheEntry& entry);
    void UnloadMemoryMapped(SDFCacheEntry& entry);

    // Compression helpers
    std::vector<uint8_t> CompressData(const void* data, size_t size);
    std::vector<uint8_t> DecompressData(const void* data, size_t compressedSize, size_t uncompressedSize);

    // Generate SDF voxel data
    SDFCacheLOD GenerateLOD(
        const std::function<float(const glm::vec3&)>& sdfFunc,
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax,
        int resolution,
        SDFCacheProgressCallback progressCallback,
        float progressBase,
        float progressRange);

    // State
    bool m_initialized = false;
    SDFCacheConfig m_config;
    SDFCacheStats m_stats;

    // Cache index
    std::unordered_map<uint64_t, CacheIndexEntry> m_index;
    mutable std::mutex m_indexMutex;

    // Auto-cleanup timer
    float m_cleanupTimer = 0.0f;

    // Background generation tracking
    std::atomic<int> m_pendingGenerations{0};
};

} // namespace Nova
