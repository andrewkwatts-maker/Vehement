/**
 * @file AssetThumbnailCache.hpp
 * @brief Automatic thumbnail generation and caching system for game assets
 *
 * Monitors asset files for changes and automatically generates/updates thumbnails.
 * Integrates with content browser for real-time icon display.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>
#include <memory>
#include <queue>
#include <mutex>
#include <atomic>
#include "../graphics/Texture.hpp"
#include "../sdf/SDFModel.hpp"
#include "../scene/Camera.hpp"

namespace fs = std::filesystem;

namespace Nova {

class Framebuffer;
class SDFRenderer;

/**
 * @brief Asset type for thumbnail generation strategy
 */
enum class ThumbnailAssetType {
    Static,      // Static 3D model - render single frame
    Animated,    // Animated model - render rotating preview
    Unit,        // Game unit - render with idle animation
    Building,    // Game building - render with ambient animation
    Texture,     // 2D texture - load directly
    Unknown      // Unsupported format
};

/**
 * @brief Thumbnail generation request
 */
struct ThumbnailRequest {
    std::string assetPath;
    std::string outputPath;
    int size = 256;
    bool forceRegenerate = false;
    ThumbnailAssetType assetType = ThumbnailAssetType::Unknown;
    uint64_t fileTimestamp = 0;
    int priority = 0;  // Higher priority processed first

    bool operator<(const ThumbnailRequest& other) const {
        return priority < other.priority; // Reverse for max-heap
    }
};

/**
 * @brief Cached thumbnail metadata
 */
struct ThumbnailCache {
    std::shared_ptr<Texture> texture;
    std::string assetPath;
    std::string thumbnailPath;
    uint64_t assetTimestamp = 0;
    uint64_t thumbnailTimestamp = 0;
    int size = 256;
    bool isValid = false;
    bool isGenerating = false;
};

/**
 * @brief Asset thumbnail generation and caching system
 *
 * Features:
 * - Automatic thumbnail generation on asset creation/modification
 * - Background queue processing to avoid blocking main thread
 * - File system monitoring for asset changes
 * - Intelligent caching with timestamp validation
 * - Priority queue for UI-visible assets
 * - Support for static and animated assets
 */
class AssetThumbnailCache {
public:
    AssetThumbnailCache();
    ~AssetThumbnailCache();

    // Non-copyable
    AssetThumbnailCache(const AssetThumbnailCache&) = delete;
    AssetThumbnailCache& operator=(const AssetThumbnailCache&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize thumbnail system
     * @param cacheDirectory Directory to store generated thumbnails
     * @param assetDirectory Root directory for game assets
     */
    bool Initialize(const std::string& cacheDirectory, const std::string& assetDirectory);

    /**
     * @brief Shutdown thumbnail system and save cache manifest
     */
    void Shutdown();

    // =========================================================================
    // Thumbnail Access
    // =========================================================================

    /**
     * @brief Get thumbnail for asset (generates if needed)
     * @param assetPath Path to asset file
     * @param size Desired thumbnail size in pixels
     * @param priority Request priority (0-10, higher = more urgent)
     * @return Texture pointer (may be placeholder while generating)
     */
    std::shared_ptr<Texture> GetThumbnail(const std::string& assetPath, int size = 256, int priority = 5);

    /**
     * @brief Check if thumbnail exists and is up-to-date
     */
    bool HasValidThumbnail(const std::string& assetPath) const;

    /**
     * @brief Force regeneration of thumbnail
     */
    void InvalidateThumbnail(const std::string& assetPath);

    /**
     * @brief Invalidate all thumbnails for assets in directory
     */
    void InvalidateDirectory(const std::string& directory);

    // =========================================================================
    // Background Processing
    // =========================================================================

    /**
     * @brief Process thumbnail generation queue (call from main thread)
     * @param maxTimeMs Maximum time to spend processing (milliseconds)
     * @return Number of thumbnails generated
     */
    int ProcessQueue(float maxTimeMs = 16.0f);

    /**
     * @brief Check if queue has pending requests
     */
    bool HasPendingRequests() const { return !m_requestQueue.empty(); }

    /**
     * @brief Get number of pending requests
     */
    size_t GetPendingCount() const { return m_requestQueue.size(); }

    // =========================================================================
    // Asset Monitoring
    // =========================================================================

    /**
     * @brief Scan asset directory for new/modified files
     */
    void ScanForChanges();

    /**
     * @brief Register asset file for monitoring
     */
    void WatchAsset(const std::string& assetPath);

    /**
     * @brief Unregister asset file from monitoring
     */
    void UnwatchAsset(const std::string& assetPath);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set thumbnail size presets
     */
    void SetSizePresets(const std::vector<int>& sizes) { m_sizePresets = sizes; }

    /**
     * @brief Get thumbnail cache directory
     */
    const std::string& GetCacheDirectory() const { return m_cacheDirectory; }

    /**
     * @brief Get asset directory
     */
    const std::string& GetAssetDirectory() const { return m_assetDirectory; }

    /**
     * @brief Enable/disable automatic background generation
     */
    void SetAutoGenerate(bool enabled) { m_autoGenerate = enabled; }

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    /**
     * @brief Detect asset type from file
     */
    ThumbnailAssetType DetectAssetType(const std::string& assetPath);

    /**
     * @brief Generate cache key for asset
     */
    std::string GetCacheKey(const std::string& assetPath, int size) const;

    /**
     * @brief Get thumbnail output path
     */
    std::string GetThumbnailPath(const std::string& assetPath, int size) const;

    /**
     * @brief Generate thumbnail for asset
     */
    bool GenerateThumbnail(const ThumbnailRequest& request);

    /**
     * @brief Render 3D asset to thumbnail
     */
    bool RenderSDFThumbnail(const std::string& assetPath, const std::string& outputPath,
                            int size, ThumbnailAssetType type);

    /**
     * @brief Copy 2D texture to thumbnail
     */
    bool CopyTextureThumbnail(const std::string& assetPath, const std::string& outputPath, int size);

    /**
     * @brief Load asset SDF model
     */
    std::unique_ptr<SDFModel> LoadAssetModel(const std::string& assetPath);

    /**
     * @brief Create placeholder texture for loading state
     */
    std::shared_ptr<Texture> CreatePlaceholderTexture(int size);

    /**
     * @brief Load cache manifest from disk
     */
    bool LoadCacheManifest();

    /**
     * @brief Save cache manifest to disk
     */
    bool SaveCacheManifest();

    /**
     * @brief Get file modification timestamp
     */
    uint64_t GetFileTimestamp(const std::string& path) const;

    // =========================================================================
    // Member Variables
    // =========================================================================

    std::string m_cacheDirectory;
    std::string m_assetDirectory;

    // Cache storage
    std::unordered_map<std::string, ThumbnailCache> m_cache;
    mutable std::mutex m_cacheMutex;

    // Request queue
    std::priority_queue<ThumbnailRequest> m_requestQueue;
    mutable std::mutex m_queueMutex;

    // Rendering resources
    std::unique_ptr<SDFRenderer> m_renderer;
    std::shared_ptr<Framebuffer> m_framebuffer;
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_autoGenerate{true};

    // Configuration
    std::vector<int> m_sizePresets{64, 128, 256, 512};
    std::shared_ptr<Texture> m_placeholderTexture;

    // File monitoring
    std::unordered_map<std::string, uint64_t> m_watchedFiles;
};

} // namespace Nova
