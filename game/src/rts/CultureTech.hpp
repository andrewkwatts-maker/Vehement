#pragma once

/**
 * @file CultureTech.hpp
 * @brief Culture-specific technology trees and research system
 *
 * Each culture has access to a unique tech tree that unlocks special abilities,
 * buildings, and upgrades. Some technologies are shared across cultures, while
 * others are exclusive to specific factions.
 */

#include "Culture.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <functional>
#include <memory>

namespace Vehement {
namespace RTS {

/**
 * @brief Technology categories for organization
 */
enum class TechCategory : uint8_t {
    Military,       ///< Combat units and weapons
    Defense,        ///< Walls, towers, fortifications
    Economy,        ///< Resource gathering and production
    Infrastructure, ///< Buildings and construction
    Special,        ///< Culture-specific unique technologies
    Count
};

/**
 * @brief Convert tech category to string
 */
inline const char* TechCategoryToString(TechCategory cat) {
    switch (cat) {
        case TechCategory::Military:        return "Military";
        case TechCategory::Defense:         return "Defense";
        case TechCategory::Economy:         return "Economy";
        case TechCategory::Infrastructure:  return "Infrastructure";
        case TechCategory::Special:         return "Special";
        default:                            return "Unknown";
    }
}

/**
 * @brief Research status for a technology
 */
enum class TechStatus : uint8_t {
    Locked,         ///< Prerequisites not met
    Available,      ///< Can be researched
    InProgress,     ///< Currently being researched
    Completed       ///< Research complete
};

/**
 * @brief Effect type that a technology can provide
 */
enum class TechEffectType : uint8_t {
    // Stat modifiers
    BonusMultiplier,    ///< Multiplies a stat (e.g., +20% damage)
    BonusFlat,          ///< Adds a flat value (e.g., +50 HP)

    // Unlocks
    UnlockBuilding,     ///< Allows construction of a building
    UnlockUnit,         ///< Allows training of a unit
    UnlockAbility,      ///< Grants a special ability

    // Special effects
    EnableFeature,      ///< Enables a gameplay feature
    ModifyMechanic,     ///< Changes how something works
};

/**
 * @brief Single effect provided by a technology
 */
struct TechEffect {
    TechEffectType type = TechEffectType::BonusMultiplier;
    std::string target;         ///< What this effect applies to
    float value = 0.0f;         ///< Numeric value (for multipliers/flat bonuses)
    std::string stringValue;    ///< String value (for unlocks)
    std::string description;    ///< Human-readable description

    /**
     * @brief Create a multiplier effect
     */
    static TechEffect Multiplier(const std::string& target, float mult, const std::string& desc) {
        TechEffect e;
        e.type = TechEffectType::BonusMultiplier;
        e.target = target;
        e.value = mult;
        e.description = desc;
        return e;
    }

    /**
     * @brief Create a flat bonus effect
     */
    static TechEffect FlatBonus(const std::string& target, float amount, const std::string& desc) {
        TechEffect e;
        e.type = TechEffectType::BonusFlat;
        e.target = target;
        e.value = amount;
        e.description = desc;
        return e;
    }

    /**
     * @brief Create a building unlock effect
     */
    static TechEffect UnlockBuilding(BuildingType building) {
        TechEffect e;
        e.type = TechEffectType::UnlockBuilding;
        e.target = BuildingTypeToString(building);
        e.stringValue = std::to_string(static_cast<int>(building));
        e.description = std::string("Unlocks ") + BuildingTypeToString(building);
        return e;
    }

    /**
     * @brief Create an ability unlock effect
     */
    static TechEffect UnlockAbility(const std::string& ability, const std::string& desc) {
        TechEffect e;
        e.type = TechEffectType::UnlockAbility;
        e.target = ability;
        e.description = desc;
        return e;
    }
};

/**
 * @brief Single node in the technology tree
 */
struct TechNode {
    std::string id;                 ///< Unique identifier
    std::string name;               ///< Display name
    std::string description;        ///< Full description
    std::string iconPath;           ///< Icon texture path

    TechCategory category = TechCategory::Military;

    // Requirements
    ResourceCost cost;              ///< Resource cost to research
    float researchTime = 30.0f;     ///< Time in seconds to complete
    std::vector<std::string> prerequisites; ///< IDs of required techs

    // Availability
    std::vector<CultureType> availableTo;   ///< Cultures that can research this
    bool isUniversal = false;               ///< Available to all cultures

    // Effects
    std::vector<TechEffect> effects;        ///< Effects when researched

    // UI positioning
    int treeColumn = 0;             ///< Column in tech tree UI
    int treeRow = 0;                ///< Row in tech tree UI

    /**
     * @brief Check if this tech is available to a culture
     */
    [[nodiscard]] bool IsAvailableTo(CultureType culture) const {
        if (isUniversal) return true;
        for (CultureType c : availableTo) {
            if (c == culture) return true;
        }
        return false;
    }
};

/**
 * @brief Player's research progress for a single technology
 */
struct TechProgress {
    std::string techId;
    TechStatus status = TechStatus::Locked;
    float progressTime = 0.0f;      ///< Time spent researching
    float totalTime = 0.0f;         ///< Total time required

    [[nodiscard]] float GetProgressPercent() const {
        return (totalTime > 0.0f) ? (progressTime / totalTime) : 0.0f;
    }
};

/**
 * @brief Complete technology tree for the RTS game
 *
 * Manages all technologies, their prerequisites, and research state.
 */
class TechTree {
public:
    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static TechTree& Instance();

    // Non-copyable
    TechTree(const TechTree&) = delete;
    TechTree& operator=(const TechTree&) = delete;

    /**
     * @brief Initialize the tech tree with all technologies
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Tech Node Access
    // =========================================================================

    /**
     * @brief Get a technology by ID
     */
    [[nodiscard]] const TechNode* GetTech(const std::string& techId) const;

    /**
     * @brief Get all technologies
     */
    [[nodiscard]] const std::vector<TechNode>& GetAllTechs() const { return m_techs; }

    /**
     * @brief Get technologies available to a specific culture
     */
    [[nodiscard]] std::vector<const TechNode*> GetTechsForCulture(CultureType culture) const;

    /**
     * @brief Get technologies in a specific category
     */
    [[nodiscard]] std::vector<const TechNode*> GetTechsByCategory(TechCategory category) const;

    /**
     * @brief Get technologies that unlock a specific building
     */
    [[nodiscard]] std::vector<const TechNode*> GetTechsUnlockingBuilding(BuildingType building) const;

    // =========================================================================
    // Prerequisite Checking
    // =========================================================================

    /**
     * @brief Check if prerequisites are met for a technology
     * @param techId Technology to check
     * @param completedTechs Set of completed technology IDs
     */
    [[nodiscard]] bool ArePrerequisitesMet(const std::string& techId,
                                           const std::unordered_set<std::string>& completedTechs) const;

    /**
     * @brief Get list of missing prerequisites
     */
    [[nodiscard]] std::vector<std::string> GetMissingPrerequisites(
        const std::string& techId,
        const std::unordered_set<std::string>& completedTechs) const;

    /**
     * @brief Get technologies that depend on a given tech
     */
    [[nodiscard]] std::vector<const TechNode*> GetDependentTechs(const std::string& techId) const;

private:
    TechTree() = default;
    ~TechTree() = default;

    // Initialization helpers
    void InitializeUniversalTechs();
    void InitializeFortressTechs();
    void InitializeBunkerTechs();
    void InitializeNomadTechs();
    void InitializeScavengerTechs();
    void InitializeMerchantTechs();
    void InitializeIndustrialTechs();
    void InitializeUndergroundTechs();
    void InitializeForestTechs();

    void AddTech(TechNode tech);

    bool m_initialized = false;
    std::vector<TechNode> m_techs;
    std::unordered_map<std::string, size_t> m_techIndex; // ID to index
};

/**
 * @brief Player-specific research manager
 *
 * Tracks research progress for a single player and handles
 * the research queue and completion callbacks.
 */
class PlayerResearch {
public:
    using ResearchCompleteCallback = std::function<void(const std::string& techId)>;

    PlayerResearch();
    explicit PlayerResearch(CultureType culture);

    /**
     * @brief Set the player's culture
     */
    void SetCulture(CultureType culture);

    /**
     * @brief Get the player's culture
     */
    [[nodiscard]] CultureType GetCulture() const noexcept { return m_culture; }

    /**
     * @brief Update research progress
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Research State
    // =========================================================================

    /**
     * @brief Get status of a technology for this player
     */
    [[nodiscard]] TechStatus GetTechStatus(const std::string& techId) const;

    /**
     * @brief Get research progress for a technology
     */
    [[nodiscard]] std::optional<TechProgress> GetProgress(const std::string& techId) const;

    /**
     * @brief Check if a technology is completed
     */
    [[nodiscard]] bool IsTechCompleted(const std::string& techId) const;

    /**
     * @brief Get all completed technologies
     */
    [[nodiscard]] const std::unordered_set<std::string>& GetCompletedTechs() const {
        return m_completedTechs;
    }

    /**
     * @brief Get currently researching technology
     */
    [[nodiscard]] std::optional<std::string> GetCurrentResearch() const;

    // =========================================================================
    // Research Actions
    // =========================================================================

    /**
     * @brief Check if a technology can be researched
     */
    [[nodiscard]] bool CanResearch(const std::string& techId) const;

    /**
     * @brief Start researching a technology
     * @return true if research started successfully
     */
    bool StartResearch(const std::string& techId);

    /**
     * @brief Cancel current research
     * @param refundPercent Percentage of resources to refund (0-1)
     * @return Resources refunded
     */
    ResourceCost CancelResearch(float refundPercent = 0.5f);

    /**
     * @brief Instantly complete current research (cheat/debug)
     */
    void CompleteCurrentResearch();

    /**
     * @brief Grant a technology without researching (scenario/script)
     */
    void GrantTech(const std::string& techId);

    // =========================================================================
    // Research Queue
    // =========================================================================

    /**
     * @brief Add technology to research queue
     */
    bool QueueResearch(const std::string& techId);

    /**
     * @brief Remove technology from queue
     */
    bool DequeueResearch(const std::string& techId);

    /**
     * @brief Get research queue
     */
    [[nodiscard]] const std::vector<std::string>& GetResearchQueue() const {
        return m_researchQueue;
    }

    /**
     * @brief Clear research queue
     */
    void ClearQueue();

    // =========================================================================
    // Effects
    // =========================================================================

    /**
     * @brief Get total bonus multiplier for a stat from all completed techs
     */
    [[nodiscard]] float GetBonusMultiplier(const std::string& stat) const;

    /**
     * @brief Get total flat bonus for a stat from all completed techs
     */
    [[nodiscard]] float GetFlatBonus(const std::string& stat) const;

    /**
     * @brief Check if a building is unlocked by research
     */
    [[nodiscard]] bool IsBuildingUnlocked(BuildingType building) const;

    /**
     * @brief Check if an ability is unlocked by research
     */
    [[nodiscard]] bool IsAbilityUnlocked(const std::string& ability) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set callback for when research completes
     */
    void SetOnResearchComplete(ResearchCompleteCallback callback) {
        m_onResearchComplete = std::move(callback);
    }

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize research state to JSON
     */
    [[nodiscard]] std::string ToJson() const;

    /**
     * @brief Deserialize research state from JSON
     */
    static PlayerResearch FromJson(const std::string& json, CultureType culture);

private:
    void UpdateResearchProgress(float deltaTime);
    void OnResearchCompleted(const std::string& techId);
    void RecalculateBonuses();

    CultureType m_culture = CultureType::Fortress;
    bool m_cultureSet = false;

    // Research state
    std::unordered_set<std::string> m_completedTechs;
    std::optional<TechProgress> m_currentResearch;
    std::vector<std::string> m_researchQueue;

    // Cached bonuses (recalculated when techs complete)
    std::unordered_map<std::string, float> m_bonusMultipliers;
    std::unordered_map<std::string, float> m_flatBonuses;
    std::unordered_set<int> m_unlockedBuildings;
    std::unordered_set<std::string> m_unlockedAbilities;

    // Callback
    ResearchCompleteCallback m_onResearchComplete;
};

// =============================================================================
// Pre-defined Technology Definitions
// =============================================================================

/**
 * @brief Universal technologies available to all cultures
 */
namespace UniversalTechs {
    // Basic economy
    constexpr const char* IMPROVED_GATHERING = "tech_improved_gathering";
    constexpr const char* ADVANCED_STORAGE = "tech_advanced_storage";
    constexpr const char* EFFICIENT_PRODUCTION = "tech_efficient_production";

    // Basic military
    constexpr const char* BASIC_WEAPONS = "tech_basic_weapons";
    constexpr const char* ARMOR_PLATING = "tech_armor_plating";
    constexpr const char* COMBAT_TRAINING = "tech_combat_training";

    // Basic defense
    constexpr const char* REINFORCED_WALLS = "tech_reinforced_walls";
    constexpr const char* TOWER_UPGRADES = "tech_tower_upgrades";
    constexpr const char* DETECTION_SYSTEMS = "tech_detection_systems";
}

/**
 * @brief Fortress-specific technologies
 */
namespace FortressTechs {
    constexpr const char* STONE_MASONRY = "tech_fortress_stone_masonry";
    constexpr const char* CASTLE_ARCHITECTURE = "tech_fortress_castle_architecture";
    constexpr const char* SIEGE_RESISTANCE = "tech_fortress_siege_resistance";
    constexpr const char* HEAVY_ARMOR = "tech_fortress_heavy_armor";
    constexpr const char* FORTIFIED_GATES = "tech_fortress_fortified_gates";
    constexpr const char* CASTLE_KEEP = "tech_fortress_castle_keep";
    constexpr const char* DEFENSIVE_AURA = "tech_fortress_defensive_aura";
}

/**
 * @brief Bunker-specific technologies
 */
namespace BunkerTechs {
    constexpr const char* AUTOMATED_TURRETS = "tech_bunker_automated_turrets";
    constexpr const char* RADAR_SYSTEMS = "tech_bunker_radar_systems";
    constexpr const char* REINFORCED_CONCRETE = "tech_bunker_reinforced_concrete";
    constexpr const char* KILL_ZONE_TACTICS = "tech_bunker_kill_zone";
    constexpr const char* EMERGENCY_LOCKDOWN = "tech_bunker_lockdown";
    constexpr const char* ADVANCED_WEAPONRY = "tech_bunker_advanced_weapons";
    constexpr const char* FIELD_MEDICINE = "tech_bunker_field_medicine";
}

/**
 * @brief Nomad-specific technologies
 */
namespace NomadTechs {
    constexpr const char* RAPID_ASSEMBLY = "tech_nomad_rapid_assembly";
    constexpr const char* MOBILE_STRUCTURES = "tech_nomad_mobile_structures";
    constexpr const char* CARAVAN_EXPERTISE = "tech_nomad_caravan_expertise";
    constexpr const char* HIT_AND_RUN = "tech_nomad_hit_and_run";
    constexpr const char* PACK_MASTERS = "tech_nomad_pack_masters";
    constexpr const char* ESCAPE_ROUTES = "tech_nomad_escape_routes";
    constexpr const char* MOBILE_WARFARE = "tech_nomad_mobile_warfare";
}

/**
 * @brief Scavenger-specific technologies
 */
namespace ScavengerTechs {
    constexpr const char* SALVAGE_EFFICIENCY = "tech_scavenger_salvage";
    constexpr const char* IMPROVISED_ARMOR = "tech_scavenger_improvised_armor";
    constexpr const char* RAPID_CONSTRUCTION = "tech_scavenger_rapid_construction";
    constexpr const char* SCRAP_RECYCLING = "tech_scavenger_recycling";
    constexpr const char* DESPERATE_MEASURES = "tech_scavenger_desperate";
    constexpr const char* HOARDER_INSTINCTS = "tech_scavenger_hoarder";
    constexpr const char* REBUILD_SURGE = "tech_scavenger_rebuild_surge";
}

/**
 * @brief Merchant-specific technologies
 */
namespace MerchantTechs {
    constexpr const char* TRADE_ROUTES = "tech_merchant_trade_routes";
    constexpr const char* BAZAAR_CONNECTIONS = "tech_merchant_bazaar";
    constexpr const char* MERCENARY_CONTRACTS = "tech_merchant_mercenaries";
    constexpr const char* DIPLOMATIC_IMMUNITY = "tech_merchant_diplomatic";
    constexpr const char* EXOTIC_GOODS = "tech_merchant_exotic_goods";
    constexpr const char* GOLD_RESERVES = "tech_merchant_gold_reserves";
    constexpr const char* TRADE_EMPIRE = "tech_merchant_trade_empire";
}

/**
 * @brief Industrial-specific technologies
 */
namespace IndustrialTechs {
    constexpr const char* ASSEMBLY_LINE = "tech_industrial_assembly_line";
    constexpr const char* AUTOMATION = "tech_industrial_automation";
    constexpr const char* MASS_PRODUCTION = "tech_industrial_mass_production";
    constexpr const char* POWER_GRID = "tech_industrial_power_grid";
    constexpr const char* FACTORY_EXPANSION = "tech_industrial_factory_expansion";
    constexpr const char* EMERGENCY_PRODUCTION = "tech_industrial_emergency";
    constexpr const char* INDUSTRIAL_REVOLUTION = "tech_industrial_revolution";
}

/**
 * @brief Underground-specific technologies
 */
namespace UndergroundTechs {
    constexpr const char* TUNNEL_NETWORK = "tech_underground_tunnels";
    constexpr const char* HIDDEN_BASES = "tech_underground_hidden_bases";
    constexpr const char* AMBUSH_TACTICS = "tech_underground_ambush";
    constexpr const char* UNDERGROUND_STORAGE = "tech_underground_storage";
    constexpr const char* COLLAPSE_TUNNELS = "tech_underground_collapse";
    constexpr const char* DEEP_EXCAVATION = "tech_underground_deep_dig";
    constexpr const char* SHADOW_WARFARE = "tech_underground_shadow_warfare";
}

/**
 * @brief Forest-specific technologies
 */
namespace ForestTechs {
    constexpr const char* CAMOUFLAGE = "tech_forest_camouflage";
    constexpr const char* AMBUSH_MASTERY = "tech_forest_ambush_mastery";
    constexpr const char* NATURES_BOUNTY = "tech_forest_bounty";
    constexpr const char* PATHFINDING = "tech_forest_pathfinding";
    constexpr const char* WOODLAND_SCOUTS = "tech_forest_scouts";
    constexpr const char* GUERRILLA_WARFARE = "tech_forest_guerrilla";
    constexpr const char* ONE_WITH_NATURE = "tech_forest_one_with_nature";
}

} // namespace RTS
} // namespace Vehement
