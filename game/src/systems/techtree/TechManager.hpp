#pragma once

/**
 * @file TechManager.hpp
 * @brief Central registry and state manager for tech trees
 *
 * Manages:
 * - Loading tech trees from JSON files
 * - Tracking researched techs per player
 * - Applying/removing modifiers
 * - Querying unlocked content
 * - Persistence save/load
 */

#include "TechTree.hpp"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace TechTree {

// ============================================================================
// Tech Manager Callbacks
// ============================================================================

/**
 * @brief Callback types for tech manager events
 */
using TechUnlockedCallback = std::function<void(const std::string& playerId,
                                                 const std::string& techId,
                                                 const TechNode& tech)>;

using TechLostCallback = std::function<void(const std::string& playerId,
                                            const std::string& techId)>;

using ModifiersChangedCallback = std::function<void(const std::string& playerId)>;

// ============================================================================
// Player Tech State
// ============================================================================

/**
 * @brief Complete tech state for a single player
 */
struct PlayerTechState {
    std::string playerId;
    std::string culture;
    TechAge currentAge = TechAge::Stone;

    std::vector<std::unique_ptr<PlayerTechTree>> trees;
    ModifierCollection activeModifiers;

    std::unordered_set<std::string> unlockedBuildings;
    std::unordered_set<std::string> unlockedUnits;
    std::unordered_set<std::string> unlockedAbilities;
    std::unordered_set<std::string> unlockedUpgrades;
    std::unordered_set<std::string> unlockedSpells;
    std::unordered_set<std::string> unlockedFeatures;

    // Statistics
    int totalTechsResearched = 0;
    int totalTechsLost = 0;
    float totalResearchTime = 0.0f;
    TechAge highestAgeReached = TechAge::Stone;

    [[nodiscard]] nlohmann::json ToJson() const;
    void FromJson(const nlohmann::json& j, const std::vector<TechTreeDef*>& treeDefs);
};

// ============================================================================
// Tech Manager
// ============================================================================

/**
 * @brief Central manager for all tech tree functionality
 *
 * Handles:
 * - Loading and registering tech tree definitions
 * - Managing player research state
 * - Applying and removing tech modifiers
 * - Querying unlocked content
 * - Persistence
 *
 * Example usage:
 * @code
 * auto& manager = TechManager::Instance();
 *
 * // Load tech trees
 * manager.LoadTechTree("assets/configs/techtrees/universal_tree.json");
 * manager.LoadTechTree("assets/configs/techtrees/fortress_tree.json");
 *
 * // Initialize player
 * manager.InitializePlayer("player_1", "fortress");
 *
 * // Research a tech
 * if (manager.CanResearch("player_1", "tech_iron_weapons", context)) {
 *     manager.StartResearch("player_1", "tech_iron_weapons", context);
 * }
 *
 * // Update each frame
 * manager.Update(deltaTime);
 *
 * // Check unlocks
 * if (manager.IsBuildingUnlocked("player_1", "barracks")) {
 *     // Can build barracks
 * }
 *
 * // Get modified stat
 * float damage = manager.GetModifiedStat("player_1", "damage", 100.0f);
 * @endcode
 */
class TechManager {
public:
    /**
     * @brief Get singleton instance
     */
    static TechManager& Instance();

    // Non-copyable
    TechManager(const TechManager&) = delete;
    TechManager& operator=(const TechManager&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the tech manager
     */
    void Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Tech Tree Loading
    // =========================================================================

    /**
     * @brief Load a tech tree from a JSON file
     * @return true if loaded successfully
     */
    bool LoadTechTree(const std::string& filePath);

    /**
     * @brief Load all tech trees from a directory
     * @return Number of trees loaded
     */
    int LoadTechTreesFromDirectory(const std::string& directory);

    /**
     * @brief Register a tech tree definition
     */
    void RegisterTechTree(std::unique_ptr<TechTreeDef> treeDef);

    /**
     * @brief Get a tech tree definition by ID
     */
    [[nodiscard]] const TechTreeDef* GetTechTree(const std::string& treeId) const;

    /**
     * @brief Get all registered tech tree IDs
     */
    [[nodiscard]] std::vector<std::string> GetTechTreeIds() const;

    /**
     * @brief Get tech trees available to a culture
     */
    [[nodiscard]] std::vector<const TechTreeDef*> GetTreesForCulture(const std::string& culture) const;

    // =========================================================================
    // Player Management
    // =========================================================================

    /**
     * @brief Initialize tech state for a player
     * @param playerId Unique player identifier
     * @param culture Player's culture (for culture-specific trees)
     */
    void InitializePlayer(const std::string& playerId, const std::string& culture);

    /**
     * @brief Remove a player's tech state
     */
    void RemovePlayer(const std::string& playerId);

    /**
     * @brief Check if a player exists
     */
    [[nodiscard]] bool HasPlayer(const std::string& playerId) const;

    /**
     * @brief Get a player's tech state
     */
    [[nodiscard]] const PlayerTechState* GetPlayerState(const std::string& playerId) const;

    /**
     * @brief Get player's current age
     */
    [[nodiscard]] TechAge GetPlayerAge(const std::string& playerId) const;

    /**
     * @brief Set player's current age
     */
    void SetPlayerAge(const std::string& playerId, TechAge age);

    // =========================================================================
    // Research Queries
    // =========================================================================

    /**
     * @brief Check if player has researched a tech
     */
    [[nodiscard]] bool HasTech(const std::string& playerId, const std::string& techId) const;

    /**
     * @brief Check if player can research a tech
     */
    [[nodiscard]] bool CanResearch(const std::string& playerId, const std::string& techId,
                                    const IRequirementContext& context) const;

    /**
     * @brief Get research status for a tech
     */
    [[nodiscard]] ResearchStatus GetTechStatus(const std::string& playerId,
                                                const std::string& techId) const;

    /**
     * @brief Get all researched techs for a player
     */
    [[nodiscard]] std::vector<std::string> GetResearchedTechs(const std::string& playerId) const;

    /**
     * @brief Get available techs for a player
     */
    [[nodiscard]] std::vector<std::string> GetAvailableTechs(const std::string& playerId,
                                                             const IRequirementContext& context) const;

    /**
     * @brief Get a tech node by ID (searches all trees)
     */
    [[nodiscard]] const TechNode* GetTechNode(const std::string& techId) const;

    // =========================================================================
    // Research Actions
    // =========================================================================

    /**
     * @brief Start researching a tech
     */
    bool StartResearch(const std::string& playerId, const std::string& techId,
                       const IRequirementContext& context);

    /**
     * @brief Update research progress for all players
     */
    void Update(float deltaTime);

    /**
     * @brief Complete current research for a player
     */
    void CompleteCurrentResearch(const std::string& playerId);

    /**
     * @brief Cancel current research
     */
    std::map<std::string, int> CancelResearch(const std::string& playerId, float refundPercent = 0.5f);

    /**
     * @brief Grant a tech to a player instantly
     */
    void GrantTech(const std::string& playerId, const std::string& techId);

    /**
     * @brief Remove a researched tech from a player
     */
    bool LoseTech(const std::string& playerId, const std::string& techId);

    // =========================================================================
    // Research Queue
    // =========================================================================

    bool QueueResearch(const std::string& playerId, const std::string& techId);
    bool DequeueResearch(const std::string& playerId, const std::string& techId);
    void ClearResearchQueue(const std::string& playerId);

    [[nodiscard]] std::vector<std::string> GetResearchQueue(const std::string& playerId) const;
    [[nodiscard]] bool IsQueued(const std::string& playerId, const std::string& techId) const;

    // =========================================================================
    // Current Research
    // =========================================================================

    [[nodiscard]] std::string GetCurrentResearch(const std::string& playerId) const;
    [[nodiscard]] float GetCurrentResearchProgress(const std::string& playerId) const;
    [[nodiscard]] float GetCurrentResearchRemainingTime(const std::string& playerId) const;
    [[nodiscard]] bool IsResearching(const std::string& playerId) const;

    // =========================================================================
    // Unlock Queries
    // =========================================================================

    [[nodiscard]] bool IsBuildingUnlocked(const std::string& playerId, const std::string& buildingId) const;
    [[nodiscard]] bool IsUnitUnlocked(const std::string& playerId, const std::string& unitId) const;
    [[nodiscard]] bool IsAbilityUnlocked(const std::string& playerId, const std::string& abilityId) const;
    [[nodiscard]] bool IsUpgradeUnlocked(const std::string& playerId, const std::string& upgradeId) const;
    [[nodiscard]] bool IsSpellUnlocked(const std::string& playerId, const std::string& spellId) const;
    [[nodiscard]] bool IsFeatureUnlocked(const std::string& playerId, const std::string& featureId) const;

    [[nodiscard]] std::vector<std::string> GetUnlockedBuildings(const std::string& playerId) const;
    [[nodiscard]] std::vector<std::string> GetUnlockedUnits(const std::string& playerId) const;
    [[nodiscard]] std::vector<std::string> GetUnlockedAbilities(const std::string& playerId) const;

    // =========================================================================
    // Modifier Queries
    // =========================================================================

    /**
     * @brief Get modified stat value for a player
     */
    [[nodiscard]] float GetModifiedStat(const std::string& playerId, const std::string& stat,
                                         float baseValue, const std::string& entityType = "",
                                         const std::vector<std::string>& entityTags = {},
                                         const std::string& entityId = "") const;

    /**
     * @brief Get flat bonus for a stat
     */
    [[nodiscard]] float GetStatFlatBonus(const std::string& playerId, const std::string& stat) const;

    /**
     * @brief Get percent bonus for a stat
     */
    [[nodiscard]] float GetStatPercentBonus(const std::string& playerId, const std::string& stat) const;

    /**
     * @brief Get all active modifiers for a player
     */
    [[nodiscard]] std::vector<TechModifier> GetActiveModifiers(const std::string& playerId) const;

    // =========================================================================
    // Research Speed
    // =========================================================================

    /**
     * @brief Set global research speed multiplier for a player
     */
    void SetResearchSpeedMultiplier(const std::string& playerId, float multiplier);

    /**
     * @brief Get research speed multiplier
     */
    [[nodiscard]] float GetResearchSpeedMultiplier(const std::string& playerId) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnTechUnlocked(TechUnlockedCallback callback) { m_onTechUnlocked = std::move(callback); }
    void SetOnTechLost(TechLostCallback callback) { m_onTechLost = std::move(callback); }
    void SetOnModifiersChanged(ModifiersChangedCallback callback) { m_onModifiersChanged = std::move(callback); }

    // =========================================================================
    // Persistence
    // =========================================================================

    /**
     * @brief Save player tech state to JSON
     */
    [[nodiscard]] nlohmann::json SavePlayerState(const std::string& playerId) const;

    /**
     * @brief Load player tech state from JSON
     */
    void LoadPlayerState(const std::string& playerId, const nlohmann::json& j);

    /**
     * @brief Save all state to JSON
     */
    [[nodiscard]] nlohmann::json SaveAllState() const;

    /**
     * @brief Load all state from JSON
     */
    void LoadAllState(const nlohmann::json& j);

    /**
     * @brief Save state to file
     */
    bool SaveToFile(const std::string& filePath) const;

    /**
     * @brief Load state from file
     */
    bool LoadFromFile(const std::string& filePath);

private:
    TechManager() = default;
    ~TechManager() = default;

    void OnTechResearched(const std::string& playerId, const std::string& techId);
    void OnTechLost(const std::string& playerId, const std::string& techId);
    void RebuildUnlocksAndModifiers(const std::string& playerId);
    void ApplyTechModifiers(PlayerTechState& state, const TechNode& tech);
    void RemoveTechModifiers(PlayerTechState& state, const TechNode& tech);

    bool m_initialized = false;

    // Tech tree definitions
    std::unordered_map<std::string, std::unique_ptr<TechTreeDef>> m_techTrees;
    std::unordered_map<std::string, std::string> m_techToTree; // techId -> treeId

    // Player states
    std::unordered_map<std::string, std::unique_ptr<PlayerTechState>> m_playerStates;

    // Research speed multipliers
    std::unordered_map<std::string, float> m_researchSpeedMultipliers;

    // Callbacks
    TechUnlockedCallback m_onTechUnlocked;
    TechLostCallback m_onTechLost;
    ModifiersChangedCallback m_onModifiersChanged;
};

// ============================================================================
// Script Interface
// ============================================================================

/**
 * @brief Script-friendly interface for tech tree system
 *
 * Provides simplified functions for use in Python scripts.
 */
namespace TechScript {

    /**
     * @brief Check if player has researched a tech
     */
    bool HasTech(const std::string& playerId, const std::string& techId);

    /**
     * @brief Grant a tech to a player
     */
    void GrantTech(const std::string& playerId, const std::string& techId);

    /**
     * @brief Remove a tech from a player
     */
    void RevokeTech(const std::string& playerId, const std::string& techId);

    /**
     * @brief Get modified stat value
     */
    float GetModifiedStat(const std::string& playerId, const std::string& stat, float baseValue);

    /**
     * @brief Check if building is unlocked
     */
    bool IsBuildingUnlocked(const std::string& playerId, const std::string& buildingId);

    /**
     * @brief Check if unit is unlocked
     */
    bool IsUnitUnlocked(const std::string& playerId, const std::string& unitId);

    /**
     * @brief Get player's current age
     */
    int GetPlayerAge(const std::string& playerId);

    /**
     * @brief Set player's age
     */
    void SetPlayerAge(const std::string& playerId, int age);

} // namespace TechScript

} // namespace TechTree
} // namespace Vehement
