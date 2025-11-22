#pragma once

#include "EntityConfig.hpp"
#include "UnitConfig.hpp"
#include "BuildingConfig.hpp"
#include "TileConfig.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <filesystem>
#include <mutex>

namespace Vehement {
namespace Config {

// ============================================================================
// Config Query
// ============================================================================

/**
 * @brief Query parameters for searching configs
 */
struct ConfigQuery {
    std::string type;                        // Filter by config type
    std::vector<std::string> tags;           // Filter by tags (AND)
    std::vector<std::string> anyTags;        // Filter by tags (OR)
    std::string namePattern;                 // Regex pattern for name
    std::string idPattern;                   // Regex pattern for ID

    bool operator==(const ConfigQuery& other) const {
        return type == other.type && tags == other.tags &&
               anyTags == other.anyTags && namePattern == other.namePattern &&
               idPattern == other.idPattern;
    }
};

// ============================================================================
// Config Change Event
// ============================================================================

/**
 * @brief Event fired when a config changes
 */
struct ConfigChangeEvent {
    enum class Type {
        Added,
        Modified,
        Removed
    };

    Type changeType;
    std::string configId;
    std::string configPath;
    std::shared_ptr<EntityConfig> config;    // nullptr for Removed
};

using ConfigChangeCallback = std::function<void(const ConfigChangeEvent&)>;

// ============================================================================
// Config Registry
// ============================================================================

/**
 * @brief Central registry for all entity configurations
 *
 * Features:
 * - Load all configs from a directory recursively
 * - Hot-reload support for development
 * - Validation of config schemas
 * - Dependency resolution for inheritance
 * - Query by ID, type, or tags
 */
class ConfigRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static ConfigRegistry& Instance();

    // Prevent copying
    ConfigRegistry(const ConfigRegistry&) = delete;
    ConfigRegistry& operator=(const ConfigRegistry&) = delete;

    // =========================================================================
    // Loading
    // =========================================================================

    /**
     * @brief Load all configs from a directory recursively
     * @param rootPath Root directory to scan
     * @param extensions File extensions to load (default: .json)
     * @return Number of configs loaded
     */
    size_t LoadDirectory(const std::string& rootPath,
                        const std::vector<std::string>& extensions = {".json"});

    /**
     * @brief Load a single config file
     * @param filePath Path to the config file
     * @return true if loaded successfully
     */
    bool LoadFile(const std::string& filePath);

    /**
     * @brief Reload a specific config from disk
     * @param configId ID of config to reload
     * @return true if reloaded successfully
     */
    bool ReloadConfig(const std::string& configId);

    /**
     * @brief Reload all configs from their source files
     * @return Number of configs reloaded
     */
    size_t ReloadAll();

    /**
     * @brief Unload all configs
     */
    void Clear();

    // =========================================================================
    // Hot Reload
    // =========================================================================

    /**
     * @brief Enable hot-reload watching on a directory
     * @param rootPath Directory to watch
     * @param pollIntervalMs How often to check for changes (ms)
     */
    void EnableHotReload(const std::string& rootPath, int pollIntervalMs = 1000);

    /**
     * @brief Disable hot-reload watching
     */
    void DisableHotReload();

    /**
     * @brief Check for and apply any file changes
     * Call this periodically if not using automatic watching
     * @return Number of configs that changed
     */
    size_t CheckForChanges();

    /**
     * @brief Is hot-reload enabled?
     */
    [[nodiscard]] bool IsHotReloadEnabled() const { return m_hotReloadEnabled; }

    // =========================================================================
    // Retrieval
    // =========================================================================

    /**
     * @brief Get config by ID
     * @param id Config ID
     * @return Config or nullptr if not found
     */
    [[nodiscard]] std::shared_ptr<EntityConfig> Get(const std::string& id) const;

    /**
     * @brief Get config by ID with type checking
     * @param id Config ID
     * @return Typed config or nullptr
     */
    template<typename T>
    [[nodiscard]] std::shared_ptr<T> GetAs(const std::string& id) const {
        auto config = Get(id);
        return config ? std::dynamic_pointer_cast<T>(config) : nullptr;
    }

    /**
     * @brief Check if config exists
     * @param id Config ID
     */
    [[nodiscard]] bool Has(const std::string& id) const;

    /**
     * @brief Get all configs of a specific type
     * @param type Config type (e.g., "unit", "building", "tile")
     */
    [[nodiscard]] std::vector<std::shared_ptr<EntityConfig>> GetByType(
        const std::string& type) const;

    /**
     * @brief Get all configs matching a query
     * @param query Query parameters
     */
    [[nodiscard]] std::vector<std::shared_ptr<EntityConfig>> Query(
        const ConfigQuery& query) const;

    /**
     * @brief Get all configs with a specific tag
     * @param tag Tag to search for
     */
    [[nodiscard]] std::vector<std::shared_ptr<EntityConfig>> GetByTag(
        const std::string& tag) const;

    /**
     * @brief Get all registered config IDs
     */
    [[nodiscard]] std::vector<std::string> GetAllIds() const;

    /**
     * @brief Get all registered config types
     */
    [[nodiscard]] std::vector<std::string> GetAllTypes() const;

    /**
     * @brief Get count of configs
     */
    [[nodiscard]] size_t GetCount() const { return m_configs.size(); }

    // =========================================================================
    // Convenience Type-Specific Accessors
    // =========================================================================

    [[nodiscard]] std::shared_ptr<UnitConfig> GetUnit(const std::string& id) const {
        return GetAs<UnitConfig>(id);
    }

    [[nodiscard]] std::shared_ptr<BuildingConfig> GetBuilding(const std::string& id) const {
        return GetAs<BuildingConfig>(id);
    }

    [[nodiscard]] std::shared_ptr<TileConfig> GetTile(const std::string& id) const {
        return GetAs<TileConfig>(id);
    }

    [[nodiscard]] std::vector<std::shared_ptr<UnitConfig>> GetAllUnits() const;
    [[nodiscard]] std::vector<std::shared_ptr<BuildingConfig>> GetAllBuildings() const;
    [[nodiscard]] std::vector<std::shared_ptr<TileConfig>> GetAllTiles() const;

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate all loaded configs
     * @return Map of config ID to validation results
     */
    [[nodiscard]] std::unordered_map<std::string, ValidationResult> ValidateAll() const;

    /**
     * @brief Validate a specific config
     * @param id Config ID
     * @return Validation result
     */
    [[nodiscard]] ValidationResult ValidateConfig(const std::string& id) const;

    /**
     * @brief Check for circular dependencies in inheritance
     * @return List of config IDs involved in circular dependencies
     */
    [[nodiscard]] std::vector<std::string> FindCircularDependencies() const;

    // =========================================================================
    // Registration and Modification
    // =========================================================================

    /**
     * @brief Register a config programmatically
     * @param config Config to register
     * @return true if registered (false if ID already exists)
     */
    bool Register(std::shared_ptr<EntityConfig> config);

    /**
     * @brief Unregister a config by ID
     * @param id Config ID
     * @return true if removed
     */
    bool Unregister(const std::string& id);

    // =========================================================================
    // Change Notifications
    // =========================================================================

    /**
     * @brief Subscribe to config change events
     * @param callback Function to call on changes
     * @return Subscription ID for unsubscribing
     */
    int Subscribe(ConfigChangeCallback callback);

    /**
     * @brief Unsubscribe from config changes
     * @param subscriptionId ID from Subscribe()
     */
    void Unsubscribe(int subscriptionId);

    // =========================================================================
    // Inheritance Resolution
    // =========================================================================

    /**
     * @brief Resolve inheritance for all configs
     * Must be called after loading to apply base configs
     */
    void ResolveInheritance();

    /**
     * @brief Get inheritance chain for a config
     * @param id Config ID
     * @return List of IDs from root to this config
     */
    [[nodiscard]] std::vector<std::string> GetInheritanceChain(
        const std::string& id) const;

    // =========================================================================
    // Schema Management
    // =========================================================================

    /**
     * @brief Register a schema for validation
     * @param schema Schema definition
     */
    void RegisterSchema(const ConfigSchemaDefinition& schema);

    /**
     * @brief Get registered schema
     * @param schemaId Schema ID
     */
    [[nodiscard]] const ConfigSchemaDefinition* GetSchema(
        const std::string& schemaId) const;

private:
    ConfigRegistry() = default;
    ~ConfigRegistry() = default;

    // Internal helpers
    std::shared_ptr<EntityConfig> CreateConfigForFile(const std::string& filePath);
    std::string DetermineConfigType(const std::string& jsonContent);
    void NotifyChange(const ConfigChangeEvent& event);
    bool ResolveInheritanceFor(const std::string& id,
                               std::unordered_set<std::string>& visited);

    // Config storage
    std::unordered_map<std::string, std::shared_ptr<EntityConfig>> m_configs;
    std::unordered_map<std::string, std::string> m_pathToId;   // file path -> config ID

    // Type indexing
    std::unordered_map<std::string, std::unordered_set<std::string>> m_typeIndex;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_tagIndex;

    // Hot reload
    bool m_hotReloadEnabled = false;
    std::string m_watchPath;
    int m_pollInterval = 1000;
    std::unordered_map<std::string, int64_t> m_fileTimestamps;

    // Change notifications
    std::unordered_map<int, ConfigChangeCallback> m_subscribers;
    int m_nextSubscriberId = 1;

    // Schemas
    std::unordered_map<std::string, ConfigSchemaDefinition> m_schemas;

    // Thread safety
    mutable std::mutex m_mutex;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * @brief Get the global config registry
 */
inline ConfigRegistry& Configs() {
    return ConfigRegistry::Instance();
}

/**
 * @brief Get a unit config by ID
 */
inline std::shared_ptr<UnitConfig> GetUnitConfig(const std::string& id) {
    return ConfigRegistry::Instance().GetUnit(id);
}

/**
 * @brief Get a building config by ID
 */
inline std::shared_ptr<BuildingConfig> GetBuildingConfig(const std::string& id) {
    return ConfigRegistry::Instance().GetBuilding(id);
}

/**
 * @brief Get a tile config by ID
 */
inline std::shared_ptr<TileConfig> GetTileConfig(const std::string& id) {
    return ConfigRegistry::Instance().GetTile(id);
}

} // namespace Config
} // namespace Vehement
