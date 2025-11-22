#pragma once

/**
 * @file RaceDefinition.hpp
 * @brief Core race structure for RTS game
 *
 * Defines the complete data structure for a playable race including
 * point allocation, archetypes, bonuses, tech trees, and campaign data.
 */

#include "PointAllocation.hpp"
#include "../TechTree.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {
namespace Race {

// Forward declarations
struct UnitArchetype;
struct BuildingArchetype;
struct HeroArchetype;
struct SpellArchetype;
struct RacialBonus;
struct RaceTechTree;

// ============================================================================
// Race Theme
// ============================================================================

/**
 * @brief Visual and thematic style of a race
 */
enum class RaceTheme : uint8_t {
    Fantasy = 0,    ///< Medieval fantasy (elves, dwarves, etc.)
    SciFi,          ///< Science fiction (robots, aliens, etc.)
    Horror,         ///< Dark horror (undead, demons, etc.)
    Nature,         ///< Nature-based (animals, elementals, etc.)
    Steampunk,      ///< Steam-powered technology
    Mystical,       ///< Magical and ethereal
    Tribal,         ///< Primitive tribal societies
    Imperial,       ///< Advanced empires

    COUNT
};

[[nodiscard]] inline const char* RaceThemeToString(RaceTheme theme) {
    switch (theme) {
        case RaceTheme::Fantasy:   return "Fantasy";
        case RaceTheme::SciFi:     return "SciFi";
        case RaceTheme::Horror:    return "Horror";
        case RaceTheme::Nature:    return "Nature";
        case RaceTheme::Steampunk: return "Steampunk";
        case RaceTheme::Mystical:  return "Mystical";
        case RaceTheme::Tribal:    return "Tribal";
        case RaceTheme::Imperial:  return "Imperial";
        default:                   return "Unknown";
    }
}

[[nodiscard]] inline RaceTheme StringToRaceTheme(const std::string& str) {
    if (str == "Fantasy" || str == "fantasy") return RaceTheme::Fantasy;
    if (str == "SciFi" || str == "scifi") return RaceTheme::SciFi;
    if (str == "Horror" || str == "horror") return RaceTheme::Horror;
    if (str == "Nature" || str == "nature") return RaceTheme::Nature;
    if (str == "Steampunk" || str == "steampunk") return RaceTheme::Steampunk;
    if (str == "Mystical" || str == "mystical") return RaceTheme::Mystical;
    if (str == "Tribal" || str == "tribal") return RaceTheme::Tribal;
    if (str == "Imperial" || str == "imperial") return RaceTheme::Imperial;
    return RaceTheme::Fantasy;
}

// ============================================================================
// Campaign Information
// ============================================================================

/**
 * @brief Campaign-related data for a race
 */
struct CampaignInfo {
    std::string campaignId;             ///< Campaign identifier
    std::string storyDescription;       ///< Lore and backstory
    std::string homeworld;              ///< Race's home location
    std::vector<std::string> allies;    ///< Allied race IDs
    std::vector<std::string> enemies;   ///< Enemy race IDs

    int campaignMissionCount = 0;       ///< Number of campaign missions
    int difficulty = 3;                 ///< Default difficulty (1-5)

    // Voice and dialogue
    std::string voicePackId;            ///< Voice pack for units
    std::string dialogueSetId;          ///< Dialogue set for campaign

    [[nodiscard]] nlohmann::json ToJson() const;
    static CampaignInfo FromJson(const nlohmann::json& j);
};

// ============================================================================
// Visual Style
// ============================================================================

/**
 * @brief Visual customization for a race
 */
struct RaceVisualStyle {
    std::string iconPath;               ///< Race icon
    std::string bannerPath;             ///< Race banner/flag
    std::string portraitPath;           ///< Leader portrait
    std::string backgroundPath;         ///< Selection screen background

    // Color scheme
    std::string primaryColor;           ///< Primary color (hex)
    std::string secondaryColor;         ///< Secondary color (hex)
    std::string accentColor;            ///< Accent color (hex)

    // UI customization
    std::string uiSkinId;               ///< UI skin identifier
    std::string minimapColor;           ///< Color on minimap

    // Music and ambience
    std::string musicTheme;             ///< Background music
    std::string ambientSound;           ///< Ambient sounds

    [[nodiscard]] nlohmann::json ToJson() const;
    static RaceVisualStyle FromJson(const nlohmann::json& j);
};

// ============================================================================
// Starting Configuration
// ============================================================================

/**
 * @brief Starting resources and units for a race
 */
struct StartingConfig {
    // Resources
    int startingGold = 500;
    int startingWood = 300;
    int startingStone = 200;
    int startingFood = 100;
    int startingMetal = 0;

    // Units
    std::vector<std::pair<std::string, int>> startingUnits;  ///< (unitId, count)

    // Buildings
    std::vector<std::string> startingBuildings;  ///< Building IDs

    // Tech
    std::vector<std::string> startingTechs;      ///< Pre-researched techs
    Age startingAge = Age::Stone;                ///< Starting age

    // Population
    int startingPopCap = 10;
    int startingPopulation = 0;

    [[nodiscard]] nlohmann::json ToJson() const;
    static StartingConfig FromJson(const nlohmann::json& j);
};

// ============================================================================
// Race Restrictions
// ============================================================================

/**
 * @brief Restrictions and limitations for a race
 */
struct RaceRestrictions {
    // Buildings that cannot be built
    std::vector<std::string> forbiddenBuildings;

    // Units that cannot be trained
    std::vector<std::string> forbiddenUnits;

    // Techs that cannot be researched
    std::vector<std::string> forbiddenTechs;

    // Resource restrictions
    bool canGatherWood = true;
    bool canGatherStone = true;
    bool canGatherGold = true;
    bool canGatherFood = true;
    bool canGatherMetal = true;

    // Gameplay restrictions
    bool canTrade = true;
    bool canAlly = true;
    int maxHeroes = 3;
    int maxAge = static_cast<int>(Age::Future);

    [[nodiscard]] nlohmann::json ToJson() const;
    static RaceRestrictions FromJson(const nlohmann::json& j);
};

// ============================================================================
// Main Race Definition
// ============================================================================

/**
 * @brief Complete definition of a playable race
 *
 * Contains all data needed to define a balanced, playable race including
 * point allocation, unit/building/hero/spell archetypes, bonuses, and
 * campaign information.
 *
 * Example usage:
 * @code
 * RaceDefinition race;
 * race.id = "humans";
 * race.name = "Human Empire";
 * race.description = "Versatile and adaptable...";
 * race.theme = RaceTheme::Fantasy;
 *
 * // Configure point allocation
 * race.allocation.military = 30;
 * race.allocation.economy = 25;
 * race.allocation.magic = 20;
 * race.allocation.technology = 25;
 *
 * // Add archetypes
 * race.unitArchetypes = {"worker", "infantry_melee", "infantry_ranged"};
 * race.buildingArchetypes = {"main_hall", "barracks", "archery_range"};
 *
 * // Validate and save
 * if (race.Validate()) {
 *     race.SaveToFile("configs/races/humans.json");
 * }
 * @endcode
 */
struct RaceDefinition {
    // =========================================================================
    // Identity
    // =========================================================================

    std::string id;                     ///< Unique identifier (e.g., "humans")
    std::string name;                   ///< Display name (e.g., "Human Empire")
    std::string shortName;              ///< Short name for UI (e.g., "Humans")
    std::string description;            ///< Full description
    RaceTheme theme = RaceTheme::Fantasy;

    // =========================================================================
    // Point Allocation
    // =========================================================================

    int totalPoints = 100;              ///< Total balance points
    PointAllocation allocation;         ///< Point distribution

    // =========================================================================
    // Archetypes
    // =========================================================================

    std::vector<std::string> unitArchetypes;      ///< Available unit types
    std::vector<std::string> buildingArchetypes;  ///< Available building types
    std::vector<std::string> heroArchetypes;      ///< Available hero types
    std::vector<std::string> spellArchetypes;     ///< Available spell types

    // =========================================================================
    // Bonuses
    // =========================================================================

    std::vector<std::string> bonusIds;            ///< Racial bonus IDs
    std::map<std::string, float> statModifiers;   ///< Direct stat modifications

    // =========================================================================
    // Tech Tree
    // =========================================================================

    std::string techTreeId;             ///< Tech tree configuration ID
    std::vector<std::string> uniqueTechs;  ///< Race-unique technologies

    // =========================================================================
    // Campaign
    // =========================================================================

    CampaignInfo campaign;

    // =========================================================================
    // Visual Style
    // =========================================================================

    RaceVisualStyle visualStyle;

    // =========================================================================
    // Starting Configuration
    // =========================================================================

    StartingConfig startingConfig;

    // =========================================================================
    // Restrictions
    // =========================================================================

    RaceRestrictions restrictions;

    // =========================================================================
    // Metadata
    // =========================================================================

    std::string author;                 ///< Race creator
    std::string version;                ///< Version string
    int64_t createdTimestamp = 0;       ///< Creation timestamp
    int64_t modifiedTimestamp = 0;      ///< Last modification timestamp
    bool isBuiltIn = false;             ///< True for built-in races
    bool isEnabled = true;              ///< True if race is playable

    // =========================================================================
    // Methods
    // =========================================================================

    /**
     * @brief Validate the race definition
     * @return true if valid
     */
    [[nodiscard]] bool Validate() const;

    /**
     * @brief Get validation errors
     */
    [[nodiscard]] std::vector<std::string> GetValidationErrors() const;

    /**
     * @brief Calculate overall power level
     * @return Power level (100 = balanced)
     */
    [[nodiscard]] float CalculatePowerLevel() const;

    /**
     * @brief Get balance score
     */
    [[nodiscard]] BalanceScore GetBalanceScore() const;

    /**
     * @brief Check if a unit archetype is available
     */
    [[nodiscard]] bool HasUnitArchetype(const std::string& archetypeId) const;

    /**
     * @brief Check if a building archetype is available
     */
    [[nodiscard]] bool HasBuildingArchetype(const std::string& archetypeId) const;

    /**
     * @brief Check if a hero archetype is available
     */
    [[nodiscard]] bool HasHeroArchetype(const std::string& archetypeId) const;

    /**
     * @brief Check if a spell archetype is available
     */
    [[nodiscard]] bool HasSpellArchetype(const std::string& archetypeId) const;

    /**
     * @brief Get effective stat modifier
     * @param statName Name of the stat
     * @return Modifier value (1.0 = no change)
     */
    [[nodiscard]] float GetStatModifier(const std::string& statName) const;

    /**
     * @brief Apply allocation bonuses to stat modifiers
     */
    void ApplyAllocationBonuses();

    // =========================================================================
    // Serialization
    // =========================================================================

    [[nodiscard]] nlohmann::json ToJson() const;
    static RaceDefinition FromJson(const nlohmann::json& j);

    /**
     * @brief Save to file
     */
    bool SaveToFile(const std::string& filepath) const;

    /**
     * @brief Load from file
     */
    bool LoadFromFile(const std::string& filepath);
};

// ============================================================================
// Race Registry
// ============================================================================

/**
 * @brief Registry for managing all race definitions
 */
class RaceRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static RaceRegistry& Instance();

    // Delete copy/move
    RaceRegistry(const RaceRegistry&) = delete;
    RaceRegistry& operator=(const RaceRegistry&) = delete;

    /**
     * @brief Initialize the registry
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Register a race definition
     */
    bool RegisterRace(const RaceDefinition& race);

    /**
     * @brief Unregister a race
     */
    bool UnregisterRace(const std::string& raceId);

    /**
     * @brief Get a race definition
     */
    [[nodiscard]] const RaceDefinition* GetRace(const std::string& raceId) const;

    /**
     * @brief Get all registered races
     */
    [[nodiscard]] std::vector<const RaceDefinition*> GetAllRaces() const;

    /**
     * @brief Get races by theme
     */
    [[nodiscard]] std::vector<const RaceDefinition*> GetRacesByTheme(RaceTheme theme) const;

    /**
     * @brief Get enabled races only
     */
    [[nodiscard]] std::vector<const RaceDefinition*> GetEnabledRaces() const;

    /**
     * @brief Load races from directory
     */
    int LoadRacesFromDirectory(const std::string& directory);

    /**
     * @brief Save all races to directory
     */
    int SaveRacesToDirectory(const std::string& directory) const;

    /**
     * @brief Create a new race from template
     */
    [[nodiscard]] RaceDefinition CreateFromTemplate(const std::string& templateName) const;

    /**
     * @brief Get available templates
     */
    [[nodiscard]] std::vector<std::string> GetTemplateNames() const;

    /**
     * @brief Validate all registered races
     */
    [[nodiscard]] std::map<std::string, std::vector<std::string>> ValidateAllRaces() const;

private:
    RaceRegistry() = default;

    bool m_initialized = false;
    std::map<std::string, RaceDefinition> m_races;
    std::map<std::string, RaceDefinition> m_templates;
};

// ============================================================================
// Built-in Race Templates
// ============================================================================

/**
 * @brief Create Human race template
 */
[[nodiscard]] RaceDefinition CreateHumanRace();

/**
 * @brief Create Orc race template
 */
[[nodiscard]] RaceDefinition CreateOrcRace();

/**
 * @brief Create Elf race template
 */
[[nodiscard]] RaceDefinition CreateElfRace();

/**
 * @brief Create Undead race template
 */
[[nodiscard]] RaceDefinition CreateUndeadRace();

/**
 * @brief Create Dwarf race template
 */
[[nodiscard]] RaceDefinition CreateDwarfRace();

/**
 * @brief Create blank race template for custom races
 */
[[nodiscard]] RaceDefinition CreateBlankRace();

} // namespace Race
} // namespace RTS
} // namespace Vehement
