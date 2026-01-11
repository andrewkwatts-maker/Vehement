#pragma once

#include "ImportSettings.hpp"
#include "ImportProgress.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <future>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================

class TextureImporter;
class ModelImporter;
class AnimationImporter;

// ============================================================================
// Asset Processing Structures
// ============================================================================

/**
 * @brief Asset dependency information
 */
struct AssetDependency {
    std::string assetPath;
    std::string dependencyType;  ///< texture, material, skeleton, etc.
    bool required = true;
    uint64_t fileHash = 0;
};

/**
 * @brief Asset metadata for cache
 */
struct AssetCacheEntry {
    std::string sourcePath;
    std::string outputPath;
    std::string assetType;
    uint64_t sourceHash = 0;
    uint64_t settingsHash = 0;
    uint64_t outputHash = 0;
    uint64_t importTime = 0;
    std::vector<AssetDependency> dependencies;
    bool valid = false;
};

/**
 * @brief Processing job
 */
struct ProcessingJob {
    std::string assetPath;
    std::string assetType;
    int priority = 0;
    std::unique_ptr<ImportSettingsBase> settings;
    std::function<void(bool success)> callback;

    bool operator<(const ProcessingJob& other) const {
        return priority < other.priority;
    }
};

/**
 * @brief Platform-specific cooking settings
 */
struct CookingSettings {
    TargetPlatform platform = TargetPlatform::Desktop;
    std::string outputDirectory;
    bool compressOutput = true;
    bool generateManifest = true;
    bool incrementalBuild = true;
    int maxParallelJobs = 4;
};

/**
 * @brief Cooking result
 */
struct CookingResult {
    size_t totalAssets = 0;
    size_t processedAssets = 0;
    size_t skippedAssets = 0;
    size_t failedAssets = 0;
    size_t totalInputSize = 0;
    size_t totalOutputSize = 0;
    int64_t totalTimeMs = 0;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// ============================================================================
// Asset Processor
// ============================================================================

/**
 * @brief Central asset processing pipeline
 *
 * Features:
 * - Asset cooking for target platform
 * - Dependency tracking
 * - Incremental processing
 * - Cache management
 * - Parallel processing
 */
class AssetProcessor {
public:
    AssetProcessor();
    ~AssetProcessor();

    // Non-copyable
    AssetProcessor(const AssetProcessor&) = delete;
    AssetProcessor& operator=(const AssetProcessor&) = delete;

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize the processor
     */
    bool Initialize(const std::string& projectRoot, const std::string& cacheDirectory);

    /**
     * @brief Shutdown the processor
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // -------------------------------------------------------------------------
    // Asset Processing
    // -------------------------------------------------------------------------

    /**
     * @brief Process a single asset
     */
    bool ProcessAsset(const std::string& assetPath, ImportProgress* progress = nullptr);

    /**
     * @brief Process asset with specific settings
     */
    bool ProcessAsset(const std::string& assetPath, const ImportSettingsBase& settings,
                      ImportProgress* progress = nullptr);

    /**
     * @brief Process multiple assets
     */
    CookingResult ProcessAssets(const std::vector<std::string>& assetPaths,
                                 ImportProgressTracker* tracker = nullptr);

    /**
     * @brief Process all assets in directory
     */
    CookingResult ProcessDirectory(const std::string& directory, bool recursive = true,
                                    ImportProgressTracker* tracker = nullptr);

    /**
     * @brief Queue asset for processing
     */
    void QueueAsset(const std::string& assetPath, int priority = 0,
                    std::function<void(bool)> callback = nullptr);

    /**
     * @brief Process queued assets
     */
    CookingResult ProcessQueue(ImportProgressTracker* tracker = nullptr);

    // -------------------------------------------------------------------------
    // Platform Cooking
    // -------------------------------------------------------------------------

    /**
     * @brief Cook all assets for target platform
     */
    CookingResult CookForPlatform(const CookingSettings& settings,
                                   ImportProgressTracker* tracker = nullptr);

    /**
     * @brief Cook specific assets for platform
     */
    CookingResult CookAssetsForPlatform(const std::vector<std::string>& assets,
                                         const CookingSettings& settings,
                                         ImportProgressTracker* tracker = nullptr);

    /**
     * @brief Get platform-specific output path
     */
    [[nodiscard]] std::string GetPlatformOutputPath(const std::string& assetPath,
                                                     TargetPlatform platform) const;

    // -------------------------------------------------------------------------
    // Dependency Tracking
    // -------------------------------------------------------------------------

    /**
     * @brief Get asset dependencies
     */
    [[nodiscard]] std::vector<AssetDependency> GetDependencies(const std::string& assetPath) const;

    /**
     * @brief Get assets that depend on this asset
     */
    [[nodiscard]] std::vector<std::string> GetDependents(const std::string& assetPath) const;

    /**
     * @brief Check if asset needs reprocessing
     */
    [[nodiscard]] bool NeedsProcessing(const std::string& assetPath) const;

    /**
     * @brief Get all assets that need processing
     */
    [[nodiscard]] std::vector<std::string> GetOutdatedAssets() const;

    /**
     * @brief Rebuild dependency graph
     */
    void RebuildDependencyGraph();

    // -------------------------------------------------------------------------
    // Cache Management
    // -------------------------------------------------------------------------

    /**
     * @brief Get cache entry for asset
     */
    [[nodiscard]] const AssetCacheEntry* GetCacheEntry(const std::string& assetPath) const;

    /**
     * @brief Update cache entry
     */
    void UpdateCacheEntry(const std::string& assetPath, const AssetCacheEntry& entry);

    /**
     * @brief Invalidate cache entry
     */
    void InvalidateCache(const std::string& assetPath);

    /**
     * @brief Clear all cache entries
     */
    void ClearCache();

    /**
     * @brief Save cache to disk
     */
    bool SaveCache();

    /**
     * @brief Load cache from disk
     */
    bool LoadCache();

    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        size_t totalEntries = 0;
        size_t validEntries = 0;
        size_t invalidEntries = 0;
        size_t totalCacheSize = 0;
    };
    [[nodiscard]] CacheStats GetCacheStats() const;

    /**
     * @brief Prune invalid cache entries
     */
    void PruneCache();

    // -------------------------------------------------------------------------
    // Asset Discovery
    // -------------------------------------------------------------------------

    /**
     * @brief Scan directory for importable assets
     */
    [[nodiscard]] std::vector<std::string> ScanForAssets(const std::string& directory, bool recursive = true);

    /**
     * @brief Get asset type from path
     */
    [[nodiscard]] std::string GetAssetType(const std::string& assetPath) const;

    /**
     * @brief Check if path is an importable asset
     */
    [[nodiscard]] bool IsImportableAsset(const std::string& path) const;

    // -------------------------------------------------------------------------
    // Parallel Processing
    // -------------------------------------------------------------------------

    /**
     * @brief Set number of worker threads
     */
    void SetWorkerCount(int count);

    /**
     * @brief Get current worker count
     */
    [[nodiscard]] int GetWorkerCount() const { return m_workerCount; }

    /**
     * @brief Start worker threads
     */
    void StartWorkers();

    /**
     * @brief Stop worker threads
     */
    void StopWorkers();

    /**
     * @brief Check if workers are running
     */
    [[nodiscard]] bool AreWorkersRunning() const { return m_workersRunning; }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using AssetProcessedCallback = std::function<void(const std::string& path, bool success)>;
    using ProgressCallback = std::function<void(float progress, const std::string& currentAsset)>;
    using ErrorCallback = std::function<void(const std::string& path, const std::string& error)>;

    void SetAssetProcessedCallback(AssetProcessedCallback callback) { m_assetProcessedCallback = callback; }
    void SetProgressCallback(ProgressCallback callback) { m_progressCallback = callback; }
    void SetErrorCallback(ErrorCallback callback) { m_errorCallback = callback; }

    // -------------------------------------------------------------------------
    // Utilities
    // -------------------------------------------------------------------------

    /**
     * @brief Calculate file hash
     */
    [[nodiscard]] static uint64_t CalculateFileHash(const std::string& path);

    /**
     * @brief Calculate settings hash
     */
    [[nodiscard]] static uint64_t CalculateSettingsHash(const ImportSettingsBase& settings);

    /**
     * @brief Get output path for asset
     */
    [[nodiscard]] std::string GetOutputPath(const std::string& assetPath) const;

    /**
     * @brief Get importers
     */
    TextureImporter& GetTextureImporter() { return *m_textureImporter; }
    ModelImporter& GetModelImporter() { return *m_modelImporter; }
    AnimationImporter& GetAnimationImporter() { return *m_animationImporter; }

private:
    // Worker thread function
    void WorkerThread();

    // Process single asset (internal)
    bool ProcessAssetInternal(const std::string& assetPath, ImportProgress* progress);

    // Update dependency graph for asset
    void UpdateDependencies(const std::string& assetPath, const std::vector<AssetDependency>& deps);

    // Topological sort for dependency order
    std::vector<std::string> GetProcessingOrder(const std::vector<std::string>& assets);

    bool m_initialized = false;
    std::string m_projectRoot;
    std::string m_cacheDirectory;
    std::string m_outputDirectory;

    // Importers
    std::unique_ptr<TextureImporter> m_textureImporter;
    std::unique_ptr<ModelImporter> m_modelImporter;
    std::unique_ptr<AnimationImporter> m_animationImporter;

    // Cache
    mutable std::mutex m_cacheMutex;
    std::unordered_map<std::string, AssetCacheEntry> m_cache;

    // Dependencies
    mutable std::mutex m_depMutex;
    std::unordered_map<std::string, std::vector<AssetDependency>> m_dependencies;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_dependents;

    // Job queue
    std::mutex m_queueMutex;
    std::priority_queue<ProcessingJob> m_jobQueue;
    std::condition_variable m_queueCondition;

    // Workers
    int m_workerCount = 4;
    std::atomic<bool> m_workersRunning{false};
    std::atomic<bool> m_shutdownRequested{false};
    std::vector<std::thread> m_workers;

    // Callbacks
    AssetProcessedCallback m_assetProcessedCallback;
    ProgressCallback m_progressCallback;
    ErrorCallback m_errorCallback;
};

// ============================================================================
// Asset Manifest
// ============================================================================

/**
 * @brief Manifest of cooked assets
 */
class AssetManifest {
public:
    struct Entry {
        std::string assetId;
        std::string sourcePath;
        std::string cookedPath;
        std::string assetType;
        uint64_t cookedHash = 0;
        size_t cookedSize = 0;
        std::vector<std::string> tags;
    };

    /**
     * @brief Add entry to manifest
     */
    void AddEntry(const Entry& entry);

    /**
     * @brief Get entry by asset ID
     */
    [[nodiscard]] const Entry* GetEntry(const std::string& assetId) const;

    /**
     * @brief Get entries by type
     */
    [[nodiscard]] std::vector<const Entry*> GetEntriesByType(const std::string& type) const;

    /**
     * @brief Get entries by tag
     */
    [[nodiscard]] std::vector<const Entry*> GetEntriesByTag(const std::string& tag) const;

    /**
     * @brief Get all entries
     */
    [[nodiscard]] const std::unordered_map<std::string, Entry>& GetAllEntries() const { return m_entries; }

    /**
     * @brief Save manifest
     */
    bool Save(const std::string& path) const;

    /**
     * @brief Load manifest
     */
    bool Load(const std::string& path);

    /**
     * @brief Clear manifest
     */
    void Clear() { m_entries.clear(); }

private:
    std::unordered_map<std::string, Entry> m_entries;
};

} // namespace Nova
