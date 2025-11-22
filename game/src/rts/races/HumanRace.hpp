#pragma once

/**
 * @file HumanRace.hpp
 * @brief Human race data loader and manager for the Kingdom of Valorheim
 *
 * Features:
 * - Load all human race configurations from JSON
 * - Initialize tech tree, units, buildings, heroes
 * - Register abilities and upgrades
 * - Provide factory methods for creating human entities
 * - Handle race-specific bonuses and mechanics
 */

#include "../TechTree.hpp"
#include "../Culture.hpp"
#include "../Building.hpp"
#include "../Hero.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {

// Forward declarations
class Unit;
class Ability;
class Upgrade;

// ============================================================================
// Human Race Constants
// ============================================================================

namespace HumanRaceConstants {
    constexpr const char* RACE_ID = "humans";
    constexpr const char* RACE_NAME = "Kingdom of Valorheim";
    constexpr const char* CONFIG_PATH = "assets/configs/races/humans/";

    // Unit IDs
    constexpr const char* PEASANT = "human_peasant";
    constexpr const char* MILITIA = "human_militia";
    constexpr const char* ARCHER = "human_archer";
    constexpr const char* SCOUT = "human_scout";
    constexpr const char* FOOTMAN = "human_footman";
    constexpr const char* CROSSBOWMAN = "human_crossbowman";
    constexpr const char* KNIGHT = "human_knight";
    constexpr const char* PRIEST = "human_priest";
    constexpr const char* SIEGE_RAM = "human_siege_ram";
    constexpr const char* PALADIN = "human_paladin";
    constexpr const char* MAGE = "human_mage";
    constexpr const char* GRIFFON_RIDER = "human_griffon_rider";
    constexpr const char* CATAPULT = "human_catapult";
    constexpr const char* SPELLBREAKER = "human_spellbreaker";
    constexpr const char* CHAMPION = "human_champion";
    constexpr const char* ARCHMAGE_UNIT = "human_archmage_unit";
    constexpr const char* DRAGON_KNIGHT = "human_dragon_knight";
    constexpr const char* TREBUCHET = "human_trebuchet";

    // Hero IDs
    constexpr const char* LORD_COMMANDER = "human_lord_commander";
    constexpr const char* HIGH_PRIESTESS = "human_high_priestess";
    constexpr const char* ARCHMAGE = "human_archmage";
    constexpr const char* RANGER_CAPTAIN = "human_ranger_captain";

    // Building IDs
    constexpr const char* TOWN_HALL = "human_town_hall";
    constexpr const char* KEEP = "human_keep";
    constexpr const char* CASTLE = "human_castle";
    constexpr const char* BARRACKS = "human_barracks";
    constexpr const char* ARCHERY_RANGE = "human_archery_range";
    constexpr const char* STABLE = "human_stable";
    constexpr const char* BLACKSMITH = "human_blacksmith";
    constexpr const char* CHURCH = "human_church";
    constexpr const char* MAGE_TOWER = "human_mage_tower";
    constexpr const char* SIEGE_WORKSHOP = "human_siege_workshop";
    constexpr const char* FARM = "human_farm";
    constexpr const char* LUMBER_MILL = "human_lumber_mill";
    constexpr const char* MARKETPLACE = "human_marketplace";
    constexpr const char* GUARD_TOWER = "human_guard_tower";
    constexpr const char* CANNON_TOWER = "human_cannon_tower";
    constexpr const char* WALL_SEGMENT = "human_wall_segment";
    constexpr const char* GATE = "human_gate";
    constexpr const char* ALTAR_OF_KINGS = "human_altar_of_kings";
    constexpr const char* GRYPHON_AVIARY = "human_gryphon_aviary";
}

// ============================================================================
// Unit Template
// ============================================================================

/**
 * @brief Template data for creating human units
 */
struct HumanUnitTemplate {
    std::string id;
    std::string name;
    std::string description;
    int tier = 1;
    int ageRequirement = 1;

    // Stats
    float health = 100.0f;
    float healthRegen = 0.5f;
    float mana = 0.0f;
    float manaRegen = 0.0f;
    int armor = 0;
    int magicResist = 0;
    float moveSpeed = 5.0f;

    // Combat
    float attackDamage = 10.0f;
    float attackSpeed = 1.0f;
    float attackRange = 1.0f;
    std::string damageType = "normal";
    std::string armorType = "light";

    // Production
    std::map<std::string, int> cost;
    float buildTime = 20.0f;
    int populationCost = 1;
    std::string productionBuilding;
    std::vector<std::string> prerequisites;

    // Abilities
    std::vector<std::string> abilityIds;

    // Visuals
    std::string modelPath;
    std::string iconPath;
    std::string portraitPath;

    [[nodiscard]] nlohmann::json ToJson() const;
    static HumanUnitTemplate FromJson(const nlohmann::json& j);
};

// ============================================================================
// Building Template
// ============================================================================

/**
 * @brief Template data for creating human buildings
 */
struct HumanBuildingTemplate {
    std::string id;
    std::string name;
    std::string description;
    int tier = 1;
    int ageRequirement = 1;

    // Stats
    float maxHealth = 1000.0f;
    int armor = 5;
    int magicResist = 5;

    // Construction
    std::map<std::string, int> cost;
    float buildTime = 60.0f;
    std::vector<int> footprint = {3, 3};

    // Prerequisites
    std::vector<std::string> prerequisites;

    // Production
    std::vector<std::string> producibleUnits;
    std::vector<std::string> researchableUpgrades;

    // Upgrades
    std::string upgradesTo;
    std::string upgradesFrom;

    // Unique settings
    bool unique = false;
    int maxCount = 10;

    // Visuals
    std::string modelPath;
    std::string iconPath;

    [[nodiscard]] nlohmann::json ToJson() const;
    static HumanBuildingTemplate FromJson(const nlohmann::json& j);
};

// ============================================================================
// Hero Template
// ============================================================================

/**
 * @brief Template data for creating human heroes
 */
struct HumanHeroTemplate {
    std::string id;
    std::string name;
    std::string title;
    std::string heroClass;
    std::string primaryAttribute;
    std::string description;
    std::string lore;

    // Base Stats
    float health = 500.0f;
    float mana = 200.0f;
    float damage = 25.0f;
    int armor = 3;
    int magicResist = 15;
    float moveSpeed = 5.5f;
    float attackRange = 1.5f;
    float attackSpeed = 1.5f;

    // Attributes
    int strength = 20;
    int agility = 15;
    int intelligence = 15;

    // Growth
    float healthPerLevel = 50.0f;
    float manaPerLevel = 25.0f;
    float damagePerLevel = 2.5f;

    // Abilities (4 slots)
    std::vector<std::string> abilityIds;

    // Talents
    std::vector<std::pair<int, std::vector<std::string>>> talentChoices;

    // Production
    std::map<std::string, int> cost;
    float summonTime = 55.0f;

    // Visuals
    std::string modelPath;
    std::string portraitPath;
    std::string iconPath;
    float modelScale = 1.0f;

    [[nodiscard]] nlohmann::json ToJson() const;
    static HumanHeroTemplate FromJson(const nlohmann::json& j);
};

// ============================================================================
// Human Race Class
// ============================================================================

/**
 * @brief Main class for managing the Human race
 *
 * Handles:
 * - Loading all race configurations
 * - Creating units, buildings, and heroes
 * - Managing the tech tree
 * - Applying race-specific bonuses
 *
 * Example usage:
 * @code
 * HumanRace humans;
 * if (humans.Initialize()) {
 *     auto peasant = humans.CreateUnit(HumanRaceConstants::PEASANT);
 *     auto barracks = humans.CreateBuilding(HumanRaceConstants::BARRACKS);
 *     auto hero = humans.CreateHero(HumanRaceConstants::LORD_COMMANDER);
 * }
 * @endcode
 */
class HumanRace {
public:
    // Callbacks
    using UnitCreatedCallback = std::function<void(const std::string& unitId)>;
    using BuildingCreatedCallback = std::function<void(const std::string& buildingId)>;
    using HeroCreatedCallback = std::function<void(const std::string& heroId)>;

    HumanRace();
    ~HumanRace();

    // Non-copyable but movable
    HumanRace(const HumanRace&) = delete;
    HumanRace& operator=(const HumanRace&) = delete;
    HumanRace(HumanRace&&) noexcept;
    HumanRace& operator=(HumanRace&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the human race with all configurations
     * @param configBasePath Base path to configuration files
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool Initialize(const std::string& configBasePath = "");

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Reload all configurations
     */
    bool ReloadConfigs();

    // =========================================================================
    // Race Information
    // =========================================================================

    /**
     * @brief Get race ID
     */
    [[nodiscard]] const std::string& GetRaceId() const { return m_raceId; }

    /**
     * @brief Get race name
     */
    [[nodiscard]] const std::string& GetRaceName() const { return m_raceName; }

    /**
     * @brief Get race description
     */
    [[nodiscard]] const std::string& GetDescription() const { return m_description; }

    /**
     * @brief Get race strengths
     */
    [[nodiscard]] const std::vector<std::string>& GetStrengths() const { return m_strengths; }

    /**
     * @brief Get race weaknesses
     */
    [[nodiscard]] const std::vector<std::string>& GetWeaknesses() const { return m_weaknesses; }

    // =========================================================================
    // Unit Management
    // =========================================================================

    /**
     * @brief Get unit template by ID
     */
    [[nodiscard]] const HumanUnitTemplate* GetUnitTemplate(const std::string& unitId) const;

    /**
     * @brief Get all unit templates
     */
    [[nodiscard]] const std::unordered_map<std::string, HumanUnitTemplate>& GetAllUnitTemplates() const {
        return m_unitTemplates;
    }

    /**
     * @brief Get units by tier
     */
    [[nodiscard]] std::vector<const HumanUnitTemplate*> GetUnitsByTier(int tier) const;

    /**
     * @brief Get units available at a specific age
     */
    [[nodiscard]] std::vector<const HumanUnitTemplate*> GetUnitsForAge(int age) const;

    /**
     * @brief Check if a unit is available (prerequisites met)
     */
    [[nodiscard]] bool IsUnitAvailable(const std::string& unitId,
                                        int currentAge,
                                        const std::vector<std::string>& completedTechs,
                                        const std::vector<std::string>& builtBuildings) const;

    // =========================================================================
    // Building Management
    // =========================================================================

    /**
     * @brief Get building template by ID
     */
    [[nodiscard]] const HumanBuildingTemplate* GetBuildingTemplate(const std::string& buildingId) const;

    /**
     * @brief Get all building templates
     */
    [[nodiscard]] const std::unordered_map<std::string, HumanBuildingTemplate>& GetAllBuildingTemplates() const {
        return m_buildingTemplates;
    }

    /**
     * @brief Get buildings available at a specific age
     */
    [[nodiscard]] std::vector<const HumanBuildingTemplate*> GetBuildingsForAge(int age) const;

    /**
     * @brief Check if a building can be constructed
     */
    [[nodiscard]] bool CanBuildBuilding(const std::string& buildingId,
                                         int currentAge,
                                         const std::vector<std::string>& existingBuildings,
                                         const std::vector<std::string>& completedTechs) const;

    // =========================================================================
    // Hero Management
    // =========================================================================

    /**
     * @brief Get hero template by ID
     */
    [[nodiscard]] const HumanHeroTemplate* GetHeroTemplate(const std::string& heroId) const;

    /**
     * @brief Get all hero templates
     */
    [[nodiscard]] const std::unordered_map<std::string, HumanHeroTemplate>& GetAllHeroTemplates() const {
        return m_heroTemplates;
    }

    /**
     * @brief Get available heroes
     */
    [[nodiscard]] std::vector<const HumanHeroTemplate*> GetAvailableHeroes() const;

    // =========================================================================
    // Tech Tree
    // =========================================================================

    /**
     * @brief Get the human tech tree
     */
    [[nodiscard]] TechTree& GetTechTree() { return m_techTree; }
    [[nodiscard]] const TechTree& GetTechTree() const { return m_techTree; }

    /**
     * @brief Get age requirements
     */
    [[nodiscard]] const nlohmann::json& GetAgeConfig() const { return m_ageConfig; }

    /**
     * @brief Get upgrades configuration
     */
    [[nodiscard]] const nlohmann::json& GetUpgradesConfig() const { return m_upgradesConfig; }

    // =========================================================================
    // Abilities
    // =========================================================================

    /**
     * @brief Get ability data by ID
     */
    [[nodiscard]] const nlohmann::json* GetAbilityData(const std::string& abilityId) const;

    /**
     * @brief Get all hero abilities
     */
    [[nodiscard]] const nlohmann::json& GetHeroAbilities() const { return m_heroAbilities; }

    /**
     * @brief Get all unit abilities
     */
    [[nodiscard]] const nlohmann::json& GetUnitAbilities() const { return m_unitAbilities; }

    // =========================================================================
    // Talent Tree
    // =========================================================================

    /**
     * @brief Get talent tree configuration
     */
    [[nodiscard]] const nlohmann::json& GetTalentTree() const { return m_talentTree; }

    // =========================================================================
    // AI Profile
    // =========================================================================

    /**
     * @brief Get AI configuration
     */
    [[nodiscard]] const nlohmann::json& GetAIConfig() const { return m_aiConfig; }

    // =========================================================================
    // Visual Assets
    // =========================================================================

    /**
     * @brief Get visual assets configuration
     */
    [[nodiscard]] const nlohmann::json& GetVisualsConfig() const { return m_visualsConfig; }

    // =========================================================================
    // Race Bonuses
    // =========================================================================

    /**
     * @brief Get bonus multiplier for a stat
     */
    [[nodiscard]] float GetStatBonus(const std::string& statName) const;

    /**
     * @brief Get starting resources
     */
    [[nodiscard]] std::map<std::string, int> GetStartingResources() const;

    /**
     * @brief Get starting units
     */
    [[nodiscard]] std::vector<std::pair<std::string, int>> GetStartingUnits() const;

    /**
     * @brief Get starting buildings
     */
    [[nodiscard]] std::vector<std::pair<std::string, int>> GetStartingBuildings() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnUnitCreated(UnitCreatedCallback callback) {
        m_onUnitCreated = std::move(callback);
    }

    void SetOnBuildingCreated(BuildingCreatedCallback callback) {
        m_onBuildingCreated = std::move(callback);
    }

    void SetOnHeroCreated(HeroCreatedCallback callback) {
        m_onHeroCreated = std::move(callback);
    }

private:
    // Loading helpers
    bool LoadRaceDefinition(const std::string& path);
    bool LoadUnitConfigs(const std::string& path);
    bool LoadBuildingConfigs(const std::string& path);
    bool LoadHeroConfigs(const std::string& path);
    bool LoadTechTree(const std::string& path);
    bool LoadAbilities(const std::string& path);
    bool LoadTalentTree(const std::string& path);
    bool LoadVisuals(const std::string& path);
    bool LoadAI(const std::string& path);

    // Helper functions
    nlohmann::json LoadJsonFile(const std::string& path);
    void ParseStrengths(const nlohmann::json& j);
    void ParseWeaknesses(const nlohmann::json& j);

    // State
    bool m_initialized = false;
    std::string m_configBasePath;

    // Race info
    std::string m_raceId;
    std::string m_raceName;
    std::string m_description;
    std::vector<std::string> m_strengths;
    std::vector<std::string> m_weaknesses;
    nlohmann::json m_raceConfig;

    // Templates
    std::unordered_map<std::string, HumanUnitTemplate> m_unitTemplates;
    std::unordered_map<std::string, HumanBuildingTemplate> m_buildingTemplates;
    std::unordered_map<std::string, HumanHeroTemplate> m_heroTemplates;

    // Tech tree
    TechTree m_techTree;
    nlohmann::json m_ageConfig;
    nlohmann::json m_upgradesConfig;
    nlohmann::json m_technologiesConfig;

    // Abilities
    nlohmann::json m_heroAbilities;
    nlohmann::json m_unitAbilities;
    std::unordered_map<std::string, nlohmann::json> m_abilityLookup;

    // Talent tree
    nlohmann::json m_talentTree;

    // Visuals
    nlohmann::json m_visualsConfig;

    // AI
    nlohmann::json m_aiConfig;

    // Race bonuses
    std::unordered_map<std::string, float> m_statBonuses;

    // Callbacks
    UnitCreatedCallback m_onUnitCreated;
    BuildingCreatedCallback m_onBuildingCreated;
    HeroCreatedCallback m_onHeroCreated;
};

// ============================================================================
// Inline Implementations
// ============================================================================

inline const HumanUnitTemplate* HumanRace::GetUnitTemplate(const std::string& unitId) const {
    auto it = m_unitTemplates.find(unitId);
    return (it != m_unitTemplates.end()) ? &it->second : nullptr;
}

inline const HumanBuildingTemplate* HumanRace::GetBuildingTemplate(const std::string& buildingId) const {
    auto it = m_buildingTemplates.find(buildingId);
    return (it != m_buildingTemplates.end()) ? &it->second : nullptr;
}

inline const HumanHeroTemplate* HumanRace::GetHeroTemplate(const std::string& heroId) const {
    auto it = m_heroTemplates.find(heroId);
    return (it != m_heroTemplates.end()) ? &it->second : nullptr;
}

} // namespace RTS
} // namespace Vehement
