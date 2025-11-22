#pragma once

#include "ContentDatabase.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace Nova {
    class Renderer;
    class Texture;
    class Model;
    class Scene;
    class Framebuffer;
}

namespace Vehement {

class Editor;

namespace Content {

/**
 * @brief Thumbnail size presets
 */
enum class ThumbnailSize {
    Small = 64,
    Medium = 128,
    Large = 256,
    ExtraLarge = 512
};

/**
 * @brief Thumbnail format
 */
enum class ThumbnailFormat {
    PNG,
    JPEG,
    WebP
};

/**
 * @brief Thumbnail generation request
 */
struct ThumbnailRequest {
    std::string assetId;
    std::string assetPath;
    AssetType assetType;
    ThumbnailSize size = ThumbnailSize::Medium;
    ThumbnailFormat format = ThumbnailFormat::PNG;
    int priority = 0;  // Higher = process first
    std::function<void(const std::string&)> callback;  // Called with thumbnail path

    bool operator<(const ThumbnailRequest& other) const {
        return priority < other.priority;  // For priority queue (lower priority = later)
    }
};

/**
 * @brief Cached thumbnail entry
 */
struct ThumbnailCacheEntry {
    std::string assetId;
    std::string thumbnailPath;
    ThumbnailSize size;
    std::chrono::system_clock::time_point generatedTime;
    std::chrono::system_clock::time_point sourceModifiedTime;
    bool valid = true;
};

/**
 * @brief Thumbnail generator configuration
 */
struct ThumbnailGeneratorConfig {
    std::string cacheDirectory = ".thumbnails";
    ThumbnailSize defaultSize = ThumbnailSize::Medium;
    ThumbnailFormat format = ThumbnailFormat::PNG;
    int jpegQuality = 85;
    int maxCacheSize = 500 * 1024 * 1024;  // 500 MB
    int maxCacheEntries = 10000;
    int workerThreads = 2;
    bool generateOnStartup = false;
    bool useGPU = true;
};

/**
 * @brief Thumbnail Generator
 *
 * Generates preview thumbnails for all asset types:
 * - Renders 3D models to thumbnails
 * - Generates icons for config types
 * - Caches thumbnails to disk
 * - Background generation thread
 * - Placeholder while loading
 */
class ThumbnailGenerator {
public:
    explicit ThumbnailGenerator(Editor* editor);
    ~ThumbnailGenerator();

    // Non-copyable
    ThumbnailGenerator(const ThumbnailGenerator&) = delete;
    ThumbnailGenerator& operator=(const ThumbnailGenerator&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the thumbnail generator
     */
    bool Initialize(const ThumbnailGeneratorConfig& config = {});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update (process completed thumbnails)
     */
    void Update(float deltaTime);

    // =========================================================================
    // Thumbnail Generation
    // =========================================================================

    /**
     * @brief Request thumbnail generation
     * @param assetId Asset ID
     * @param callback Called when thumbnail is ready
     * @return Request ID for tracking
     */
    uint64_t RequestThumbnail(const std::string& assetId,
                              std::function<void(const std::string&)> callback = nullptr);

    /**
     * @brief Request thumbnail with options
     */
    uint64_t RequestThumbnail(const ThumbnailRequest& request);

    /**
     * @brief Generate thumbnail synchronously
     * @param assetId Asset ID
     * @param size Thumbnail size
     * @return Path to generated thumbnail, or empty on failure
     */
    std::string GenerateThumbnail(const std::string& assetId, ThumbnailSize size = ThumbnailSize::Medium);

    /**
     * @brief Generate thumbnails for all assets
     * @param async Run in background
     */
    void GenerateAllThumbnails(bool async = true);

    /**
     * @brief Cancel pending request
     */
    void CancelRequest(uint64_t requestId);

    /**
     * @brief Cancel all pending requests
     */
    void CancelAllRequests();

    // =========================================================================
    // Cache Management
    // =========================================================================

    /**
     * @brief Get cached thumbnail path
     * @return Path to thumbnail, or placeholder path if not cached
     */
    std::string GetThumbnailPath(const std::string& assetId, ThumbnailSize size = ThumbnailSize::Medium);

    /**
     * @brief Check if thumbnail is cached and valid
     */
    bool HasValidThumbnail(const std::string& assetId, ThumbnailSize size = ThumbnailSize::Medium);

    /**
     * @brief Invalidate cached thumbnail
     */
    void InvalidateThumbnail(const std::string& assetId);

    /**
     * @brief Invalidate all thumbnails
     */
    void InvalidateAllThumbnails();

    /**
     * @brief Clear thumbnail cache
     */
    void ClearCache();

    /**
     * @brief Trim cache to size limit
     */
    void TrimCache();

    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        size_t totalEntries = 0;
        size_t validEntries = 0;
        size_t cacheSize = 0;
        size_t hitCount = 0;
        size_t missCount = 0;
        size_t generatedCount = 0;
    };
    [[nodiscard]] CacheStats GetCacheStats() const;

    // =========================================================================
    // Placeholder Management
    // =========================================================================

    /**
     * @brief Get placeholder thumbnail for asset type
     */
    std::string GetPlaceholder(AssetType type, ThumbnailSize size = ThumbnailSize::Medium);

    /**
     * @brief Get loading placeholder
     */
    std::string GetLoadingPlaceholder(ThumbnailSize size = ThumbnailSize::Medium);

    /**
     * @brief Get error placeholder
     */
    std::string GetErrorPlaceholder(ThumbnailSize size = ThumbnailSize::Medium);

    // =========================================================================
    // Custom Renderers
    // =========================================================================

    /**
     * @brief Custom thumbnail renderer function
     * @param assetPath Path to asset file
     * @param outputPath Path to write thumbnail
     * @param size Thumbnail size
     * @return true if successful
     */
    using CustomRenderer = std::function<bool(const std::string& assetPath,
                                               const std::string& outputPath,
                                               int size)>;

    /**
     * @brief Register custom renderer for asset type
     */
    void RegisterRenderer(AssetType type, CustomRenderer renderer);

    /**
     * @brief Unregister custom renderer
     */
    void UnregisterRenderer(AssetType type);

    // =========================================================================
    // Status
    // =========================================================================

    /**
     * @brief Get pending request count
     */
    [[nodiscard]] size_t GetPendingCount() const;

    /**
     * @brief Check if generator is busy
     */
    [[nodiscard]] bool IsBusy() const { return m_pendingCount.load() > 0; }

    /**
     * @brief Get generation progress (0.0 - 1.0)
     */
    [[nodiscard]] float GetProgress() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&, const std::string&)> OnThumbnailReady;  // assetId, path
    std::function<void(const std::string&, const std::string&)> OnThumbnailFailed; // assetId, error
    std::function<void()> OnAllThumbnailsGenerated;

private:
    // Worker thread management
    void StartWorkerThreads();
    void StopWorkerThreads();
    void WorkerThread();

    // Generation methods
    bool GenerateModelThumbnail(const std::string& assetPath, const std::string& outputPath, int size);
    bool GenerateTextureThumbnail(const std::string& assetPath, const std::string& outputPath, int size);
    bool GenerateConfigThumbnail(const std::string& assetPath, AssetType type, const std::string& outputPath, int size);
    bool GenerateIconThumbnail(AssetType type, const std::string& outputPath, int size);

    // Icon generation
    void GenerateUnitIcon(unsigned char* pixels, int size);
    void GenerateBuildingIcon(unsigned char* pixels, int size);
    void GenerateSpellIcon(unsigned char* pixels, int size);
    void GenerateEffectIcon(unsigned char* pixels, int size);
    void GenerateTileIcon(unsigned char* pixels, int size);
    void GenerateHeroIcon(unsigned char* pixels, int size);
    void GenerateAbilityIcon(unsigned char* pixels, int size);
    void GenerateTechTreeIcon(unsigned char* pixels, int size);
    void GenerateDefaultIcon(unsigned char* pixels, int size);

    // Path helpers
    std::string GetCachePath(const std::string& assetId, ThumbnailSize size) const;
    std::string GetAssetSourcePath(const std::string& assetId) const;
    bool EnsureCacheDirectory();

    // Image processing
    bool SaveThumbnail(const unsigned char* pixels, int width, int height, const std::string& path);
    bool ResizeImage(const unsigned char* src, int srcW, int srcH,
                     unsigned char* dst, int dstW, int dstH);

    Editor* m_editor = nullptr;
    ThumbnailGeneratorConfig m_config;
    bool m_initialized = false;

    // Request queue
    std::priority_queue<ThumbnailRequest> m_requestQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::atomic<uint64_t> m_nextRequestId{1};
    std::atomic<size_t> m_pendingCount{0};
    std::atomic<size_t> m_totalRequests{0};
    std::atomic<size_t> m_completedRequests{0};

    // Worker threads
    std::vector<std::thread> m_workerThreads;
    std::atomic<bool> m_running{false};

    // Cache
    std::unordered_map<std::string, ThumbnailCacheEntry> m_cache;  // Key: assetId_size
    mutable std::mutex m_cacheMutex;
    mutable CacheStats m_cacheStats;

    // Custom renderers
    std::unordered_map<AssetType, CustomRenderer> m_customRenderers;

    // Placeholders (pre-generated)
    std::unordered_map<AssetType, std::string> m_placeholders;
    std::string m_loadingPlaceholder;
    std::string m_errorPlaceholder;

    // Completed thumbnails (for main thread callbacks)
    struct CompletedThumbnail {
        std::string assetId;
        std::string path;
        bool success;
        std::string error;
        std::function<void(const std::string&)> callback;
    };
    std::queue<CompletedThumbnail> m_completedThumbnails;
    std::mutex m_completedMutex;

    // Rendering resources (for GPU rendering)
    std::unique_ptr<Nova::Framebuffer> m_framebuffer;
    std::unique_ptr<Nova::Scene> m_previewScene;
};

} // namespace Content
} // namespace Vehement
