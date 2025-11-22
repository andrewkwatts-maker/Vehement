#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <filesystem>

namespace Vehement {

class Editor;

namespace Content {

/**
 * @brief Asset type enumeration
 */
enum class AssetType {
    Unknown,
    Unit,
    Building,
    Spell,
    Effect,
    Buff,
    Tile,
    Hero,
    Ability,
    TechTree,
    Projectile,
    Resource,
    Culture,
    Quest,
    Dialog,
    Script,
    Model,
    Texture,
    Audio,
    Animation
};

/**
 * @brief Convert AssetType to string
 */
inline const char* AssetTypeToString(AssetType type) {
    switch (type) {
        case AssetType::Unit:       return "units";
        case AssetType::Building:   return "buildings";
        case AssetType::Spell:      return "spells";
        case AssetType::Effect:     return "effects";
        case AssetType::Buff:       return "buffs";
        case AssetType::Tile:       return "tiles";
        case AssetType::Hero:       return "heroes";
        case AssetType::Ability:    return "abilities";
        case AssetType::TechTree:   return "techtrees";
        case AssetType::Projectile: return "projectiles";
        case AssetType::Resource:   return "resources";
        case AssetType::Culture:    return "cultures";
        case AssetType::Quest:      return "quests";
        case AssetType::Dialog:     return "dialogs";
        case AssetType::Script:     return "scripts";
        case AssetType::Model:      return "models";
        case AssetType::Texture:    return "textures";
        case AssetType::Audio:      return "audio";
        case AssetType::Animation:  return "animations";
        default:                    return "unknown";
    }
}

/**
 * @brief Convert string to AssetType
 */
inline AssetType StringToAssetType(const std::string& str) {
    static const std::unordered_map<std::string, AssetType> typeMap = {
        {"units", AssetType::Unit},
        {"buildings", AssetType::Building},
        {"spells", AssetType::Spell},
        {"effects", AssetType::Effect},
        {"buffs", AssetType::Buff},
        {"tiles", AssetType::Tile},
        {"heroes", AssetType::Hero},
        {"abilities", AssetType::Ability},
        {"techtrees", AssetType::TechTree},
        {"projectiles", AssetType::Projectile},
        {"resources", AssetType::Resource},
        {"cultures", AssetType::Culture},
        {"quests", AssetType::Quest},
        {"dialogs", AssetType::Dialog},
        {"scripts", AssetType::Script},
        {"models", AssetType::Model},
        {"textures", AssetType::Texture},
        {"audio", AssetType::Audio},
        {"animations", AssetType::Animation}
    };

    auto it = typeMap.find(str);
    return it != typeMap.end() ? it->second : AssetType::Unknown;
}

/**
 * @brief Validation status for assets
 */
enum class ValidationStatus {
    Unknown,
    Valid,
    Warning,
    Error
};

/**
 * @brief Asset metadata stored in the database
 */
struct AssetMetadata {
    std::string id;
    std::string name;
    std::string description;
    AssetType type = AssetType::Unknown;
    std::string filePath;
    std::string relativePath;
    std::string thumbnailPath;
    std::vector<std::string> tags;

    // File information
    size_t fileSize = 0;
    std::chrono::system_clock::time_point createdTime;
    std::chrono::system_clock::time_point modifiedTime;
    std::string checksum;

    // State
    ValidationStatus validationStatus = ValidationStatus::Unknown;
    std::string validationMessage;
    bool isDirty = false;
    bool isLoaded = false;
    bool isFavorite = false;

    // Dependencies
    std::vector<std::string> dependencies;      // Assets this asset depends on
    std::vector<std::string> dependents;        // Assets that depend on this asset

    // Search index data
    std::string searchableText;                 // Combined text for full-text search
    std::unordered_map<std::string, std::string> properties;  // Extracted properties for filtering

    // Custom metadata
    std::unordered_map<std::string, std::string> customData;
};

/**
 * @brief Search result with relevance score
 */
struct SearchResult {
    std::string assetId;
    float relevanceScore = 0.0f;
    std::vector<std::string> matchedTerms;
    std::string matchContext;  // Snippet showing match context
};

/**
 * @brief File change event
 */
struct FileChangeEvent {
    enum class Type {
        Created,
        Modified,
        Deleted,
        Renamed
    };

    Type type;
    std::string path;
    std::string oldPath;  // For rename events
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Content database configuration
 */
struct ContentDatabaseConfig {
    std::string contentRoot = "game/assets/configs";
    std::string cacheDirectory = ".content_cache";
    bool enableFileWatcher = true;
    bool enableFullTextSearch = true;
    bool enableDependencyTracking = true;
    int scanIntervalMs = 1000;
    int maxCacheSize = 1000;
};

/**
 * @brief Content Database
 *
 * Indexes and manages all content assets in the project:
 * - Scans content folders on startup
 * - Watches for file changes
 * - Extracts metadata from assets
 * - Provides full-text search
 * - Tracks dependencies between assets
 * - Validates asset integrity
 * - Manages tags and custom metadata
 */
class ContentDatabase {
public:
    explicit ContentDatabase(Editor* editor);
    ~ContentDatabase();

    // Non-copyable
    ContentDatabase(const ContentDatabase&) = delete;
    ContentDatabase& operator=(const ContentDatabase&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the database
     * @param config Database configuration
     * @return true if successful
     */
    bool Initialize(const ContentDatabaseConfig& config = {});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update (process file watcher events)
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Scanning
    // =========================================================================

    /**
     * @brief Perform full content scan
     * @param async Run scan in background thread
     */
    void ScanContent(bool async = true);

    /**
     * @brief Rescan a specific directory
     */
    void RescanDirectory(const std::string& path);

    /**
     * @brief Rescan a specific asset
     */
    void RescanAsset(const std::string& assetId);

    /**
     * @brief Check if scanning is in progress
     */
    [[nodiscard]] bool IsScanning() const { return m_scanning.load(); }

    /**
     * @brief Get scan progress (0.0 - 1.0)
     */
    [[nodiscard]] float GetScanProgress() const { return m_scanProgress.load(); }

    // =========================================================================
    // Asset Queries
    // =========================================================================

    /**
     * @brief Get all assets
     */
    [[nodiscard]] std::vector<AssetMetadata> GetAllAssets() const;

    /**
     * @brief Get asset by ID
     */
    [[nodiscard]] std::optional<AssetMetadata> GetAsset(const std::string& id) const;

    /**
     * @brief Get assets by type
     */
    [[nodiscard]] std::vector<AssetMetadata> GetAssetsByType(AssetType type) const;

    /**
     * @brief Get assets by tag
     */
    [[nodiscard]] std::vector<AssetMetadata> GetAssetsByTag(const std::string& tag) const;

    /**
     * @brief Get assets in directory
     */
    [[nodiscard]] std::vector<AssetMetadata> GetAssetsInDirectory(const std::string& path) const;

    /**
     * @brief Get favorite assets
     */
    [[nodiscard]] std::vector<AssetMetadata> GetFavorites() const;

    /**
     * @brief Get recently modified assets
     * @param count Maximum number to return
     */
    [[nodiscard]] std::vector<AssetMetadata> GetRecentAssets(size_t count = 20) const;

    /**
     * @brief Check if asset exists
     */
    [[nodiscard]] bool HasAsset(const std::string& id) const;

    /**
     * @brief Get asset count
     */
    [[nodiscard]] size_t GetAssetCount() const;

    /**
     * @brief Get asset count by type
     */
    [[nodiscard]] size_t GetAssetCountByType(AssetType type) const;

    // =========================================================================
    // Full-Text Search
    // =========================================================================

    /**
     * @brief Search assets by text query
     * @param query Search query (supports AND, OR, NOT, quotes)
     * @param maxResults Maximum results to return
     * @return Search results sorted by relevance
     */
    [[nodiscard]] std::vector<SearchResult> Search(const std::string& query, size_t maxResults = 100) const;

    /**
     * @brief Search with type filter
     */
    [[nodiscard]] std::vector<SearchResult> SearchByType(const std::string& query, AssetType type, size_t maxResults = 100) const;

    /**
     * @brief Get search suggestions
     */
    [[nodiscard]] std::vector<std::string> GetSearchSuggestions(const std::string& prefix, size_t maxSuggestions = 10) const;

    // =========================================================================
    // Tags
    // =========================================================================

    /**
     * @brief Get all tags in use
     */
    [[nodiscard]] std::vector<std::string> GetAllTags() const;

    /**
     * @brief Get tag usage count
     */
    [[nodiscard]] std::unordered_map<std::string, size_t> GetTagCounts() const;

    /**
     * @brief Add tag to asset
     */
    bool AddTag(const std::string& assetId, const std::string& tag);

    /**
     * @brief Remove tag from asset
     */
    bool RemoveTag(const std::string& assetId, const std::string& tag);

    /**
     * @brief Set all tags for asset
     */
    bool SetTags(const std::string& assetId, const std::vector<std::string>& tags);

    // =========================================================================
    // Favorites
    // =========================================================================

    /**
     * @brief Toggle favorite status
     */
    void ToggleFavorite(const std::string& assetId);

    /**
     * @brief Set favorite status
     */
    void SetFavorite(const std::string& assetId, bool favorite);

    /**
     * @brief Check if asset is favorite
     */
    [[nodiscard]] bool IsFavorite(const std::string& assetId) const;

    // =========================================================================
    // Dependencies
    // =========================================================================

    /**
     * @brief Get assets that this asset depends on
     */
    [[nodiscard]] std::vector<std::string> GetDependencies(const std::string& assetId) const;

    /**
     * @brief Get assets that depend on this asset
     */
    [[nodiscard]] std::vector<std::string> GetDependents(const std::string& assetId) const;

    /**
     * @brief Build full dependency graph for an asset
     * @param assetId Starting asset
     * @param upstream Get upstream dependencies (true) or downstream dependents (false)
     */
    [[nodiscard]] std::vector<std::string> GetDependencyTree(const std::string& assetId, bool upstream = true) const;

    /**
     * @brief Check for circular dependencies
     */
    [[nodiscard]] bool HasCircularDependency(const std::string& assetId) const;

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate a specific asset
     */
    ValidationStatus ValidateAsset(const std::string& assetId);

    /**
     * @brief Validate all assets
     */
    void ValidateAll();

    /**
     * @brief Get assets with validation errors
     */
    [[nodiscard]] std::vector<AssetMetadata> GetInvalidAssets() const;

    /**
     * @brief Get assets with warnings
     */
    [[nodiscard]] std::vector<AssetMetadata> GetAssetsWithWarnings() const;

    // =========================================================================
    // File Watching
    // =========================================================================

    /**
     * @brief Enable/disable file watching
     */
    void SetFileWatcherEnabled(bool enabled);

    /**
     * @brief Check if file watcher is enabled
     */
    [[nodiscard]] bool IsFileWatcherEnabled() const { return m_fileWatcherEnabled; }

    /**
     * @brief Get pending file change events
     */
    [[nodiscard]] std::vector<FileChangeEvent> GetPendingChanges();

    // =========================================================================
    // Metadata Management
    // =========================================================================

    /**
     * @brief Update asset metadata
     */
    bool UpdateMetadata(const std::string& assetId, const AssetMetadata& metadata);

    /**
     * @brief Set custom metadata field
     */
    bool SetCustomData(const std::string& assetId, const std::string& key, const std::string& value);

    /**
     * @brief Get custom metadata field
     */
    [[nodiscard]] std::string GetCustomData(const std::string& assetId, const std::string& key) const;

    // =========================================================================
    // Cache Management
    // =========================================================================

    /**
     * @brief Save database cache to disk
     */
    bool SaveCache();

    /**
     * @brief Load database cache from disk
     */
    bool LoadCache();

    /**
     * @brief Clear cache
     */
    void ClearCache();

    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        size_t totalAssets = 0;
        size_t cachedAssets = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        size_t cacheSize = 0;
    };
    [[nodiscard]] CacheStats GetCacheStats() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void()> OnScanComplete;
    std::function<void(const std::string&)> OnAssetAdded;
    std::function<void(const std::string&)> OnAssetRemoved;
    std::function<void(const std::string&)> OnAssetModified;
    std::function<void(const FileChangeEvent&)> OnFileChanged;

private:
    // Scanning helpers
    void ScanDirectory(const std::string& path);
    void ProcessFile(const std::filesystem::path& path);
    AssetMetadata ExtractMetadata(const std::filesystem::path& path);
    AssetType DetectAssetType(const std::filesystem::path& path);
    std::string GenerateAssetId(const std::filesystem::path& path);
    std::string ComputeChecksum(const std::filesystem::path& path);

    // Search index
    void BuildSearchIndex();
    void UpdateSearchIndex(const std::string& assetId);
    void RemoveFromSearchIndex(const std::string& assetId);
    std::vector<std::string> TokenizeQuery(const std::string& query) const;
    float ComputeRelevance(const AssetMetadata& asset, const std::vector<std::string>& tokens) const;

    // Dependency tracking
    void ExtractDependencies(const std::string& assetId);
    void UpdateDependencyGraph();

    // File watching
    void StartFileWatcher();
    void StopFileWatcher();
    void FileWatcherThread();
    void ProcessFileChange(const FileChangeEvent& event);

    // Validation
    ValidationStatus ValidateJson(const std::filesystem::path& path, std::string& message);
    ValidationStatus ValidateSchema(const std::filesystem::path& path, AssetType type, std::string& message);
    ValidationStatus ValidateReferences(const std::string& assetId, std::string& message);

    Editor* m_editor = nullptr;
    ContentDatabaseConfig m_config;
    bool m_initialized = false;

    // Asset storage
    std::unordered_map<std::string, AssetMetadata> m_assets;
    mutable std::mutex m_assetsMutex;

    // Index structures
    std::unordered_map<AssetType, std::unordered_set<std::string>> m_typeIndex;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_tagIndex;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_directoryIndex;
    std::unordered_set<std::string> m_favorites;

    // Search index (inverted index for full-text search)
    std::unordered_map<std::string, std::unordered_set<std::string>> m_searchIndex;  // term -> asset IDs
    std::unordered_map<std::string, std::unordered_map<std::string, float>> m_termFrequency;  // asset -> term -> frequency

    // Dependency graph
    std::unordered_map<std::string, std::unordered_set<std::string>> m_dependencyGraph;  // asset -> dependencies
    std::unordered_map<std::string, std::unordered_set<std::string>> m_dependentGraph;   // asset -> dependents

    // File watcher
    std::atomic<bool> m_fileWatcherEnabled{true};
    std::atomic<bool> m_fileWatcherRunning{false};
    std::thread m_fileWatcherThread;
    std::queue<FileChangeEvent> m_pendingChanges;
    std::mutex m_changesMutex;
    std::unordered_map<std::string, std::filesystem::file_time_type> m_fileTimestamps;

    // Scanning state
    std::atomic<bool> m_scanning{false};
    std::atomic<float> m_scanProgress{0.0f};
    std::thread m_scanThread;

    // Cache stats
    mutable CacheStats m_cacheStats;
};

} // namespace Content
} // namespace Vehement
