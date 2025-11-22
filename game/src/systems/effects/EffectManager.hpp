#pragma once

#include "EffectDefinition.hpp"
#include "EffectInstance.hpp"
#include "EffectContainer.hpp"
#include "AuraEffect.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <filesystem>

namespace Vehement {
namespace Effects {

// ============================================================================
// Effect Manager Configuration
// ============================================================================

/**
 * @brief Configuration for the effect manager
 */
struct EffectManagerConfig {
    std::string effectsPath = "assets/configs/effects/";
    bool enableHotReload = true;
    float hotReloadCheckInterval = 2.0f;  // Seconds between file checks
    size_t instancePoolSize = 128;
    bool logWarnings = true;
    bool strictValidation = false;
};

// ============================================================================
// Effect Manager
// ============================================================================

/**
 * @brief Central manager for effect definitions and instances
 *
 * Responsibilities:
 * - Load and store effect definitions from JSON
 * - Create effect instances
 * - Hot-reload definitions during development
 * - Track global effect statistics
 */
class EffectManager {
public:
    EffectManager();
    ~EffectManager();

    // Non-copyable
    EffectManager(const EffectManager&) = delete;
    EffectManager& operator=(const EffectManager&) = delete;

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize the effect manager
     */
    bool Initialize(const EffectManagerConfig& config = {});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // -------------------------------------------------------------------------
    // Loading
    // -------------------------------------------------------------------------

    /**
     * @brief Load all effect definitions from directory
     */
    int LoadEffectsFromDirectory(const std::string& path);

    /**
     * @brief Load a single effect definition from file
     */
    bool LoadEffectFromFile(const std::string& filePath);

    /**
     * @brief Load effect definition from JSON string
     */
    bool LoadEffectFromJson(const std::string& jsonStr, const std::string& sourcePath = "");

    /**
     * @brief Register a programmatically created definition
     */
    bool RegisterDefinition(std::unique_ptr<EffectDefinition> definition);

    /**
     * @brief Unload an effect definition
     */
    bool UnloadEffect(const std::string& effectId);

    /**
     * @brief Unload all effect definitions
     */
    void UnloadAll();

    // -------------------------------------------------------------------------
    // Hot Reload
    // -------------------------------------------------------------------------

    /**
     * @brief Update hot reload (call each frame)
     */
    void UpdateHotReload(float deltaTime);

    /**
     * @brief Force reload of a specific effect
     */
    bool ReloadEffect(const std::string& effectId);

    /**
     * @brief Force reload of all effects
     */
    int ReloadAll();

    /**
     * @brief Enable/disable hot reload
     */
    void SetHotReloadEnabled(bool enabled) { m_config.enableHotReload = enabled; }

    // -------------------------------------------------------------------------
    // Definition Access
    // -------------------------------------------------------------------------

    /**
     * @brief Get effect definition by ID
     */
    [[nodiscard]] const EffectDefinition* GetDefinition(const std::string& effectId) const;

    /**
     * @brief Check if effect exists
     */
    [[nodiscard]] bool HasEffect(const std::string& effectId) const;

    /**
     * @brief Get all effect IDs
     */
    [[nodiscard]] std::vector<std::string> GetAllEffectIds() const;

    /**
     * @brief Get effects by type
     */
    [[nodiscard]] std::vector<const EffectDefinition*> GetEffectsByType(EffectType type) const;

    /**
     * @brief Get effects with tag
     */
    [[nodiscard]] std::vector<const EffectDefinition*> GetEffectsByTag(const std::string& tag) const;

    /**
     * @brief Query effects matching predicate
     */
    [[nodiscard]] std::vector<const EffectDefinition*> QueryEffects(
        std::function<bool(const EffectDefinition*)> predicate
    ) const;

    // -------------------------------------------------------------------------
    // Instance Creation
    // -------------------------------------------------------------------------

    /**
     * @brief Create an effect instance
     */
    std::unique_ptr<EffectInstance> CreateInstance(const std::string& effectId);

    /**
     * @brief Create an effect instance from definition
     */
    std::unique_ptr<EffectInstance> CreateInstance(const EffectDefinition* definition);

    /**
     * @brief Get instance pool
     */
    EffectInstancePool& GetInstancePool() { return m_instancePool; }

    // -------------------------------------------------------------------------
    // Container Management
    // -------------------------------------------------------------------------

    /**
     * @brief Create an effect container for an entity
     */
    std::unique_ptr<EffectContainer> CreateContainer(uint32_t ownerId);

    /**
     * @brief Register a container with the manager
     */
    void RegisterContainer(EffectContainer* container);

    /**
     * @brief Unregister a container
     */
    void UnregisterContainer(EffectContainer* container);

    /**
     * @brief Get container for entity
     */
    [[nodiscard]] EffectContainer* GetContainer(uint32_t ownerId) const;

    // -------------------------------------------------------------------------
    // Aura Management
    // -------------------------------------------------------------------------

    /**
     * @brief Get aura manager
     */
    AuraManager& GetAuraManager() { return m_auraManager; }
    [[nodiscard]] const AuraManager& GetAuraManager() const { return m_auraManager; }

    // -------------------------------------------------------------------------
    // Global Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Update all managed containers
     */
    void Update(float deltaTime);

    /**
     * @brief Apply effect to entity
     * @param effectId Effect to apply
     * @param sourceId Source entity
     * @param targetId Target entity
     * @return Application result
     */
    EffectApplicationResult ApplyEffect(
        const std::string& effectId,
        uint32_t sourceId,
        uint32_t targetId
    );

    /**
     * @brief Remove effect from entity
     */
    bool RemoveEffect(const std::string& effectId, uint32_t targetId);

    /**
     * @brief Process trigger event for all containers
     */
    void ProcessGlobalTrigger(const TriggerEventData& eventData);

    // -------------------------------------------------------------------------
    // Statistics
    // -------------------------------------------------------------------------

    /**
     * @brief Get total loaded definitions
     */
    [[nodiscard]] size_t GetDefinitionCount() const { return m_definitions.size(); }

    /**
     * @brief Get total active instances
     */
    [[nodiscard]] size_t GetActiveInstanceCount() const;

    /**
     * @brief Get statistics
     */
    struct Statistics {
        size_t definitionsLoaded = 0;
        size_t activeInstances = 0;
        size_t activeContainers = 0;
        size_t activeAuras = 0;
        size_t reloadsPerformed = 0;
        size_t instancesCreated = 0;
    };

    [[nodiscard]] Statistics GetStatistics() const;

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using ReloadCallback = std::function<void(const std::string& effectId)>;

    void SetOnEffectReloaded(ReloadCallback callback) {
        m_onEffectReloaded = std::move(callback);
    }

    // -------------------------------------------------------------------------
    // Validation
    // -------------------------------------------------------------------------

    /**
     * @brief Validate all loaded definitions
     * @return Map of effect ID to error messages
     */
    std::unordered_map<std::string, std::vector<std::string>> ValidateAll() const;

    /**
     * @brief Check for missing references (effects that reference non-existent effects)
     */
    std::vector<std::string> FindMissingReferences() const;

private:
    void CheckHotReload();
    void ProcessReload(const std::string& effectId, const std::string& filePath);

    // Configuration
    EffectManagerConfig m_config;
    bool m_initialized = false;

    // Definitions
    std::unordered_map<std::string, std::unique_ptr<EffectDefinition>> m_definitions;
    std::unordered_map<std::string, std::string> m_definitionFilePaths;
    std::unordered_map<std::string, int64_t> m_fileModificationTimes;

    // Containers (weak references)
    std::unordered_map<uint32_t, EffectContainer*> m_containers;

    // Instance pool
    EffectInstancePool m_instancePool;

    // Aura manager
    AuraManager m_auraManager;

    // Hot reload
    float m_hotReloadTimer = 0.0f;

    // Statistics
    mutable Statistics m_statistics;

    // Callbacks
    ReloadCallback m_onEffectReloaded;
};

// ============================================================================
// Global Effect Manager Access
// ============================================================================

/**
 * @brief Get the global effect manager instance
 */
EffectManager& GetEffectManager();

/**
 * @brief Set the global effect manager instance
 */
void SetEffectManager(EffectManager* manager);

} // namespace Effects
} // namespace Vehement
