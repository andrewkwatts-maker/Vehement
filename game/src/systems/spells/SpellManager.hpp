#pragma once

#include "SpellDefinition.hpp"
#include "SpellTargeting.hpp"
#include "SpellEffect.hpp"
#include "SpellVisuals.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <filesystem>

namespace Vehement {
namespace Spells {

// Forward declarations
class SpellCaster;
class SpellInstance;

// ============================================================================
// Spell Registry Entry
// ============================================================================

/**
 * @brief Entry in the spell registry with metadata
 */
struct SpellRegistryEntry {
    std::shared_ptr<SpellDefinition> definition;
    std::string filePath;
    int64_t lastModified = 0;
    bool isLoaded = false;
    std::vector<std::string> validationErrors;
};

// ============================================================================
// Spell Category
// ============================================================================

/**
 * @brief Category for organizing spells
 */
struct SpellCategory {
    std::string id;
    std::string name;
    std::string iconPath;
    std::vector<std::string> spellIds;
    int sortOrder = 0;
};

// ============================================================================
// Hot Reload Event
// ============================================================================

/**
 * @brief Event data for hot reload notifications
 */
struct HotReloadEvent {
    enum class Type { Added, Modified, Removed };

    Type type;
    std::string spellId;
    std::string filePath;
    bool success = true;
    std::string errorMessage;
};

// ============================================================================
// Spell Manager
// ============================================================================

/**
 * @brief Central registry and factory for spell definitions
 *
 * Responsibilities:
 * - Load spell definitions from JSON files
 * - Maintain spell registry
 * - Create spell instances
 * - Support hot-reload of spell definitions
 * - Validate spell configurations
 */
class SpellManager {
public:
    /**
     * @brief Get singleton instance
     */
    static SpellManager& Instance();

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the spell manager
     * @param spellConfigPath Path to spell configuration directory
     * @return true if initialization successful
     */
    bool Initialize(const std::string& spellConfigPath);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if manager is initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Loading
    // =========================================================================

    /**
     * @brief Load all spell definitions from configured path
     * @return Number of spells loaded
     */
    int LoadAllSpells();

    /**
     * @brief Load a single spell from file
     * @param filePath Path to spell JSON file
     * @return true if loaded successfully
     */
    bool LoadSpell(const std::string& filePath);

    /**
     * @brief Load spell from JSON string
     * @param id Spell ID
     * @param jsonString JSON content
     * @return true if loaded successfully
     */
    bool LoadSpellFromString(const std::string& id, const std::string& jsonString);

    /**
     * @brief Reload a specific spell
     * @param spellId ID of spell to reload
     * @return true if reloaded successfully
     */
    bool ReloadSpell(const std::string& spellId);

    /**
     * @brief Unload a spell
     */
    void UnloadSpell(const std::string& spellId);

    // =========================================================================
    // Queries
    // =========================================================================

    /**
     * @brief Get spell definition by ID
     * @return Spell definition or nullptr if not found
     */
    const SpellDefinition* GetSpell(const std::string& spellId) const;

    /**
     * @brief Get spell definition (mutable)
     */
    SpellDefinition* GetSpellMutable(const std::string& spellId);

    /**
     * @brief Check if spell exists
     */
    bool HasSpell(const std::string& spellId) const;

    /**
     * @brief Get all spell IDs
     */
    std::vector<std::string> GetAllSpellIds() const;

    /**
     * @brief Get spells by tag
     */
    std::vector<const SpellDefinition*> GetSpellsByTag(const std::string& tag) const;

    /**
     * @brief Get spells by school
     */
    std::vector<const SpellDefinition*> GetSpellsBySchool(const std::string& school) const;

    /**
     * @brief Get spells by targeting mode
     */
    std::vector<const SpellDefinition*> GetSpellsByTargetingMode(TargetingMode mode) const;

    /**
     * @brief Search spells by name (partial match)
     */
    std::vector<const SpellDefinition*> SearchSpells(const std::string& query) const;

    /**
     * @brief Get number of loaded spells
     */
    [[nodiscard]] size_t GetSpellCount() const { return m_spells.size(); }

    // =========================================================================
    // Categories
    // =========================================================================

    /**
     * @brief Register a spell category
     */
    void RegisterCategory(const SpellCategory& category);

    /**
     * @brief Get spell category
     */
    const SpellCategory* GetCategory(const std::string& categoryId) const;

    /**
     * @brief Get all categories
     */
    std::vector<const SpellCategory*> GetAllCategories() const;

    /**
     * @brief Get spells in category
     */
    std::vector<const SpellDefinition*> GetSpellsInCategory(const std::string& categoryId) const;

    // =========================================================================
    // Instance Creation
    // =========================================================================

    /**
     * @brief Create a new spell instance for casting
     * @param spellId ID of spell to instantiate
     * @param casterId ID of casting entity
     * @return Unique spell instance
     */
    std::unique_ptr<SpellInstance> CreateInstance(
        const std::string& spellId,
        uint32_t casterId
    );

    /**
     * @brief Create spell instance with targeting
     */
    std::unique_ptr<SpellInstance> CreateInstance(
        const std::string& spellId,
        uint32_t casterId,
        uint32_t targetId,
        const glm::vec3& targetPosition,
        const glm::vec3& targetDirection
    );

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate a spell definition
     * @param spell Spell to validate
     * @param errors Output error messages
     * @return true if valid
     */
    bool ValidateSpell(const SpellDefinition& spell, std::vector<std::string>& errors) const;

    /**
     * @brief Validate all loaded spells
     * @return Map of spell ID to validation errors
     */
    std::unordered_map<std::string, std::vector<std::string>> ValidateAllSpells() const;

    /**
     * @brief Get validation errors for a spell
     */
    std::vector<std::string> GetValidationErrors(const std::string& spellId) const;

    // =========================================================================
    // Hot Reload
    // =========================================================================

    /**
     * @brief Enable hot reload watching
     */
    void EnableHotReload();

    /**
     * @brief Disable hot reload watching
     */
    void DisableHotReload();

    /**
     * @brief Check if hot reload is enabled
     */
    [[nodiscard]] bool IsHotReloadEnabled() const { return m_hotReloadEnabled; }

    /**
     * @brief Poll for file changes (call each frame if hot reload enabled)
     */
    void PollFileChanges();

    /**
     * @brief Set callback for hot reload events
     */
    using HotReloadCallback = std::function<void(const HotReloadEvent&)>;
    void SetHotReloadCallback(HotReloadCallback callback) { m_hotReloadCallback = std::move(callback); }

    // =========================================================================
    // Script Integration
    // =========================================================================

    /**
     * @brief Register Python script handler
     */
    using ScriptExecutor = std::function<bool(
        const std::string& scriptPath,
        const std::string& function,
        SpellInstance& instance
    )>;

    void SetScriptExecutor(ScriptExecutor executor) { m_scriptExecutor = std::move(executor); }

    /**
     * @brief Execute a spell script
     */
    bool ExecuteScript(
        const std::string& scriptPath,
        const std::string& function,
        SpellInstance& instance
    );

    // =========================================================================
    // Effect Registry
    // =========================================================================

    /**
     * @brief Register a shared effect definition
     */
    void RegisterEffect(const std::string& effectId, std::shared_ptr<SpellEffect> effect);

    /**
     * @brief Get a registered effect
     */
    std::shared_ptr<SpellEffect> GetEffect(const std::string& effectId) const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get spell config directory path
     */
    [[nodiscard]] const std::string& GetConfigPath() const { return m_configPath; }

    /**
     * @brief Set global cooldown duration
     */
    void SetGlobalCooldownDuration(float duration) { m_globalCooldownDuration = duration; }
    [[nodiscard]] float GetGlobalCooldownDuration() const { return m_globalCooldownDuration; }

private:
    SpellManager() = default;
    ~SpellManager() = default;

    // Disable copy
    SpellManager(const SpellManager&) = delete;
    SpellManager& operator=(const SpellManager&) = delete;

    // Internal methods
    void ScanDirectory(const std::string& path);
    void ProcessJsonFile(const std::string& filePath);
    bool ParseSpellJson(const std::string& jsonContent, SpellDefinition& spell);
    void UpdateFileModificationTimes();
    void NotifyHotReload(const HotReloadEvent& event);

    // Spell registry
    std::unordered_map<std::string, SpellRegistryEntry> m_spells;

    // Categories
    std::unordered_map<std::string, SpellCategory> m_categories;

    // Shared effects (can be referenced by multiple spells)
    std::unordered_map<std::string, std::shared_ptr<SpellEffect>> m_sharedEffects;

    // Configuration
    std::string m_configPath;
    float m_globalCooldownDuration = 1.5f;

    // Hot reload
    bool m_hotReloadEnabled = false;
    std::unordered_map<std::string, int64_t> m_fileModTimes;
    HotReloadCallback m_hotReloadCallback;

    // Script execution
    ScriptExecutor m_scriptExecutor;

    // State
    bool m_initialized = false;
};

// ============================================================================
// JSON Schema
// ============================================================================

/**
 * @brief Get the JSON schema for spell definitions
 */
std::string GetSpellJsonSchema();

/**
 * @brief Validate JSON against spell schema
 */
bool ValidateSpellJson(const std::string& json, std::vector<std::string>& errors);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Get file modification time
 */
int64_t GetFileModificationTime(const std::string& filePath);

/**
 * @brief Check if file has been modified since timestamp
 */
bool FileWasModified(const std::string& filePath, int64_t lastKnownTime);

/**
 * @brief List all JSON files in directory
 */
std::vector<std::string> ListJsonFiles(const std::string& directory);

} // namespace Spells
} // namespace Vehement
