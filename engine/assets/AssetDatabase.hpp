#pragma once

#include "JsonAssetSerializer.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <functional>
#include <mutex>

namespace Nova {

/**
 * @brief Asset import settings
 */
struct AssetImportSettings {
    bool generateThumbnail = true;
    bool validateOnImport = true;
    bool autoMigrate = true;
    bool trackDependencies = true;
};

/**
 * @brief Asset reference for dependency tracking
 */
struct AssetReference {
    std::string uuid;
    AssetType type;
    std::string path;
    bool isLoaded = false;
    time_t lastModified = 0;
};

/**
 * @brief Asset hot-reload event
 */
struct AssetReloadEvent {
    std::string uuid;
    AssetType type;
    std::string path;
    std::shared_ptr<JsonAsset> newAsset;
};

/**
 * @brief Asset registry and management database
 *
 * Features:
 * - Registry of all assets (JSON + binary)
 * - Asset UUID tracking
 * - Dependency graph
 * - Asset import queue
 * - Asset hot-reload manager
 * - Integration with JsonAssetSerializer
 * - Asset search by type/tags/name
 */
class AssetDatabase {
public:
    AssetDatabase();
    ~AssetDatabase();

    /**
     * @brief Initialize database
     */
    void Initialize(const std::string& projectRoot);

    /**
     * @brief Shutdown database
     */
    void Shutdown();

    /**
     * @brief Register asset
     */
    bool RegisterAsset(std::shared_ptr<JsonAsset> asset);

    /**
     * @brief Unregister asset
     */
    void UnregisterAsset(const std::string& uuid);

    /**
     * @brief Get asset by UUID
     */
    std::shared_ptr<JsonAsset> GetAsset(const std::string& uuid);

    /**
     * @brief Get asset by path
     */
    std::shared_ptr<JsonAsset> GetAssetByPath(const std::string& path);

    /**
     * @brief Check if asset exists
     */
    bool HasAsset(const std::string& uuid) const;

    /**
     * @brief Check if path is registered
     */
    bool HasPath(const std::string& path) const;

    /**
     * @brief Get all assets of type
     */
    std::vector<std::shared_ptr<JsonAsset>> GetAssetsByType(AssetType type);

    /**
     * @brief Get all assets with tag
     */
    std::vector<std::shared_ptr<JsonAsset>> GetAssetsByTag(const std::string& tag);

    /**
     * @brief Search assets by name
     */
    std::vector<std::shared_ptr<JsonAsset>> SearchByName(const std::string& query);

    /**
     * @brief Get all asset UUIDs
     */
    std::vector<std::string> GetAllAssetUUIDs() const;

    /**
     * @brief Import asset from file
     */
    bool ImportAsset(const std::string& filePath, const AssetImportSettings& settings = {});

    /**
     * @brief Reimport asset
     */
    bool ReimportAsset(const std::string& uuid);

    /**
     * @brief Import directory recursively
     */
    void ImportDirectory(const std::string& directory, bool recursive = true);

    /**
     * @brief Export asset to file
     */
    bool ExportAsset(const std::string& uuid, const std::string& filePath);

    /**
     * @brief Get asset dependencies
     */
    std::vector<std::string> GetDependencies(const std::string& uuid);

    /**
     * @brief Get assets that depend on this asset
     */
    std::vector<std::string> GetDependents(const std::string& uuid);

    /**
     * @brief Build dependency graph
     */
    void BuildDependencyGraph();

    /**
     * @brief Validate all assets
     */
    std::vector<ValidationResult> ValidateAll();

    /**
     * @brief Enable/disable hot-reloading
     */
    void SetHotReloadEnabled(bool enabled);
    bool IsHotReloadEnabled() const { return m_hotReloadEnabled; }

    /**
     * @brief Update hot-reload system (call every frame)
     */
    void Update();

    /**
     * @brief Register hot-reload callback
     */
    void RegisterReloadCallback(std::function<void(const AssetReloadEvent&)> callback);

    /**
     * @brief Save database index to disk
     */
    bool SaveIndex();

    /**
     * @brief Load database index from disk
     */
    bool LoadIndex();

    /**
     * @brief Get database statistics
     */
    struct DatabaseStats {
        size_t totalAssets = 0;
        size_t loadedAssets = 0;
        std::unordered_map<AssetType, size_t> assetsByType;
        size_t totalDependencies = 0;
        size_t reloadCount = 0;
        size_t importQueue = 0;
    };
    DatabaseStats GetStatistics() const;

    /**
     * @brief Clear all cached assets
     */
    void ClearCache();

    /**
     * @brief Get serializer
     */
    JsonAssetSerializer& GetSerializer() { return m_serializer; }

private:
    std::string m_projectRoot;
    JsonAssetSerializer m_serializer;

    // Asset registry
    std::unordered_map<std::string, std::shared_ptr<JsonAsset>> m_assets;
    std::unordered_map<std::string, std::string> m_pathToUuid;
    std::unordered_map<std::string, AssetReference> m_references;

    // Dependency graph
    std::unordered_map<std::string, std::unordered_set<std::string>> m_dependencies;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_dependents;

    // Hot-reload system
    bool m_hotReloadEnabled = true;
    std::unordered_map<std::string, time_t> m_fileModificationTimes;
    std::vector<std::function<void(const AssetReloadEvent&)>> m_reloadCallbacks;

    // Import queue
    struct ImportTask {
        std::string filePath;
        AssetImportSettings settings;
    };
    std::vector<ImportTask> m_importQueue;

    // Thread safety
    mutable std::mutex m_mutex;

    /**
     * @brief Check if file has been modified
     */
    bool HasFileChanged(const std::string& filePath);

    /**
     * @brief Get file modification time
     */
    time_t GetFileModificationTime(const std::string& filePath) const;

    /**
     * @brief Reload changed assets
     */
    void ReloadChangedAssets();

    /**
     * @brief Add to dependency graph
     */
    void AddDependency(const std::string& assetUuid, const std::string& dependencyUuid);

    /**
     * @brief Remove from dependency graph
     */
    void RemoveDependency(const std::string& assetUuid, const std::string& dependencyUuid);

    /**
     * @brief Notify reload callbacks
     */
    void NotifyReload(const AssetReloadEvent& event);
};

/**
 * @brief Global asset database singleton
 */
class AssetDatabaseManager {
public:
    static AssetDatabaseManager& Instance();

    void Initialize(const std::string& projectRoot);
    void Shutdown();

    AssetDatabase& GetDatabase() { return m_database; }
    const AssetDatabase& GetDatabase() const { return m_database; }

private:
    AssetDatabaseManager() = default;
    AssetDatabase m_database;
};

} // namespace Nova
