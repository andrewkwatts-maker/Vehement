#pragma once

#include "InstanceData.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Nova {

/**
 * @brief Manages instance data and archetype configurations
 *
 * Handles loading/saving of instance data, archetype configs, and merging
 * instance overrides with base archetype properties to produce effective
 * configurations.
 */
class InstanceManager {
public:
    InstanceManager() = default;
    ~InstanceManager() = default;

    // Non-copyable
    InstanceManager(const InstanceManager&) = delete;
    InstanceManager& operator=(const InstanceManager&) = delete;

    /**
     * @brief Initialize the instance manager
     * @param archetypeDirectory Directory containing archetype JSON files
     * @param instanceDirectory Directory for saving instance JSON files
     */
    bool Initialize(const std::string& archetypeDirectory, const std::string& instanceDirectory);

    /**
     * @brief Load an instance from file
     * @param path Path to instance JSON file
     * @return Instance data, or empty instance on failure
     */
    InstanceData LoadInstance(const std::string& path);

    /**
     * @brief Save an instance to file
     * @param path Path to save instance JSON
     * @param instance Instance data to save
     * @return True on success
     */
    bool SaveInstance(const std::string& path, const InstanceData& instance);

    /**
     * @brief Save instance to default location based on map name
     * @param mapName Name of the current map
     * @param instance Instance data to save
     * @return True on success
     */
    bool SaveInstanceToMap(const std::string& mapName, const InstanceData& instance);

    /**
     * @brief Load all instances for a map
     * @param mapName Name of the map
     * @return Vector of instance data
     */
    std::vector<InstanceData> LoadMapInstances(const std::string& mapName);

    /**
     * @brief Load archetype configuration
     * @param archetypeId Archetype identifier (e.g., "humans.units.footman")
     * @return JSON configuration, or empty object if not found
     */
    nlohmann::json LoadArchetype(const std::string& archetypeId);

    /**
     * @brief Apply instance overrides to base archetype config
     * @param baseConfig Base archetype configuration
     * @param overrides Instance-specific overrides
     * @return Merged configuration
     */
    nlohmann::json ApplyOverrides(const nlohmann::json& baseConfig, const nlohmann::json& overrides);

    /**
     * @brief Get effective configuration for an instance
     * @param instance Instance data
     * @return Full resolved configuration with overrides applied
     */
    nlohmann::json GetEffectiveConfig(const InstanceData& instance);

    /**
     * @brief Register an instance in memory
     * @param instance Instance data to register
     */
    void RegisterInstance(const InstanceData& instance);

    /**
     * @brief Unregister an instance from memory
     * @param instanceId Instance ID to unregister
     */
    void UnregisterInstance(const std::string& instanceId);

    /**
     * @brief Get registered instance by ID
     * @param instanceId Instance ID
     * @return Pointer to instance data, or nullptr if not found
     */
    InstanceData* GetInstance(const std::string& instanceId);

    /**
     * @brief Get all registered instances
     */
    const std::unordered_map<std::string, InstanceData>& GetAllInstances() const {
        return m_instances;
    }

    /**
     * @brief Clear all registered instances
     */
    void ClearInstances() {
        m_instances.clear();
        m_dirtyInstances.clear();
    }

    /**
     * @brief Mark instance as dirty (needs saving)
     */
    void MarkDirty(const std::string& instanceId);

    /**
     * @brief Check if instance is dirty
     */
    bool IsDirty(const std::string& instanceId) const;

    /**
     * @brief Get all dirty instances
     */
    std::vector<std::string> GetDirtyInstances() const;

    /**
     * @brief Save all dirty instances
     * @param mapName Name of current map
     * @return Number of instances saved
     */
    int SaveDirtyInstances(const std::string& mapName);

    /**
     * @brief Create a new instance from archetype
     * @param archetypeId Archetype identifier
     * @param position Initial position
     * @return New instance data
     */
    InstanceData CreateInstance(const std::string& archetypeId, const glm::vec3& position = glm::vec3(0.0f));

    /**
     * @brief Get archetype directory path
     */
    const std::string& GetArchetypeDirectory() const { return m_archetypeDirectory; }

    /**
     * @brief Get instance directory path
     */
    const std::string& GetInstanceDirectory() const { return m_instanceDirectory; }

    /**
     * @brief List all available archetypes
     * @return Vector of archetype IDs
     */
    std::vector<std::string> ListArchetypes() const;

private:
    /**
     * @brief Convert archetype ID to file path
     * @param archetypeId Archetype ID (e.g., "humans.units.footman")
     * @return File path
     */
    std::string ArchetypeIdToPath(const std::string& archetypeId) const;

    /**
     * @brief Get instance file path for a map
     * @param mapName Map name
     * @param instanceId Instance ID
     * @return File path
     */
    std::string GetInstancePath(const std::string& mapName, const std::string& instanceId) const;

    /**
     * @brief Recursively merge JSON objects
     */
    void MergeJson(nlohmann::json& target, const nlohmann::json& source);

    /**
     * @brief Cache archetype configs for performance
     */
    std::unordered_map<std::string, nlohmann::json> m_archetypeCache;

    /**
     * @brief Active instances in memory
     */
    std::unordered_map<std::string, InstanceData> m_instances;

    /**
     * @brief Set of dirty instance IDs
     */
    std::unordered_set<std::string> m_dirtyInstances;

    /**
     * @brief Directory containing archetype JSON files
     */
    std::string m_archetypeDirectory = "assets/config/";

    /**
     * @brief Directory for instance JSON files
     */
    std::string m_instanceDirectory = "assets/maps/";
};

} // namespace Nova
