#pragma once

/**
 * @file AgeProgression.hpp
 * @brief Age-specific content, visual themes, and gameplay modifiers
 *
 * Defines what's available in each age:
 * - Buildings, units, and abilities
 * - Visual styles and UI themes
 * - Gameplay modifiers (gather rates, build speeds, etc.)
 * - Resource availability
 */

#include "TechTree.hpp"
#include "Culture.hpp"
#include "Resource.hpp"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {

// Forward declarations
class TechTree;

// ============================================================================
// Unit Types
// ============================================================================

/**
 * @brief Types of units available in the game
 */
enum class UnitType : uint8_t {
    // Basic units (all ages)
    Worker,
    Scout,

    // Stone Age
    Clubman,
    Hunter,

    // Bronze Age
    Spearman,
    Slinger,
    BronzeWarrior,

    // Iron Age
    Swordsman,
    Archer,
    HeavyInfantry,
    Cavalry,

    // Medieval Age
    Knight,
    Crossbowman,
    Pikeman,
    Trebuchet,
    BatteringRam,

    // Industrial Age
    Musketeer,
    Cannon,
    Dragoon,

    // Modern Age
    Rifleman,
    MachineGunner,
    Tank,
    APC,

    // Future Age
    PlasmaRifleman,
    HoverTank,
    BattleDrone,
    MechWarrior,

    // Special (culture-specific)
    Mercenary,
    WindRider,
    ShadowAssassin,

    COUNT
};

/**
 * @brief Get display name for unit type
 */
[[nodiscard]] inline const char* UnitTypeToString(UnitType type) {
    switch (type) {
        case UnitType::Worker:          return "Worker";
        case UnitType::Scout:           return "Scout";
        case UnitType::Clubman:         return "Clubman";
        case UnitType::Hunter:          return "Hunter";
        case UnitType::Spearman:        return "Spearman";
        case UnitType::Slinger:         return "Slinger";
        case UnitType::BronzeWarrior:   return "Bronze Warrior";
        case UnitType::Swordsman:       return "Swordsman";
        case UnitType::Archer:          return "Archer";
        case UnitType::HeavyInfantry:   return "Heavy Infantry";
        case UnitType::Cavalry:         return "Cavalry";
        case UnitType::Knight:          return "Knight";
        case UnitType::Crossbowman:     return "Crossbowman";
        case UnitType::Pikeman:         return "Pikeman";
        case UnitType::Trebuchet:       return "Trebuchet";
        case UnitType::BatteringRam:    return "Battering Ram";
        case UnitType::Musketeer:       return "Musketeer";
        case UnitType::Cannon:          return "Cannon";
        case UnitType::Dragoon:         return "Dragoon";
        case UnitType::Rifleman:        return "Rifleman";
        case UnitType::MachineGunner:   return "Machine Gunner";
        case UnitType::Tank:            return "Tank";
        case UnitType::APC:             return "APC";
        case UnitType::PlasmaRifleman:  return "Plasma Rifleman";
        case UnitType::HoverTank:       return "Hover Tank";
        case UnitType::BattleDrone:     return "Battle Drone";
        case UnitType::MechWarrior:     return "Mech Warrior";
        case UnitType::Mercenary:       return "Mercenary";
        case UnitType::WindRider:       return "Wind Rider";
        case UnitType::ShadowAssassin:  return "Shadow Assassin";
        default:                        return "Unknown";
    }
}

// ============================================================================
// Visual Styles
// ============================================================================

/**
 * @brief Building visual style per age
 */
enum class BuildingStyle : uint8_t {
    Primitive,      ///< Sticks, leaves, mud - Stone Age
    Wooden,         ///< Log cabins, wooden fences - Bronze Age
    Stone,          ///< Cut stone, mortar - Iron Age
    Medieval,       ///< Castles, towers, keeps - Medieval Age
    Brick,          ///< Brick buildings, factories - Industrial Age
    Modern,         ///< Concrete, glass, steel - Modern Age
    Futuristic,     ///< Sleek, glowing, high-tech - Future Age

    COUNT
};

[[nodiscard]] inline const char* BuildingStyleToString(BuildingStyle style) {
    switch (style) {
        case BuildingStyle::Primitive:  return "Primitive";
        case BuildingStyle::Wooden:     return "Wooden";
        case BuildingStyle::Stone:      return "Stone";
        case BuildingStyle::Medieval:   return "Medieval";
        case BuildingStyle::Brick:      return "Brick";
        case BuildingStyle::Modern:     return "Modern";
        case BuildingStyle::Futuristic: return "Futuristic";
        default:                        return "Unknown";
    }
}

/**
 * @brief Unit visual style per age
 */
enum class UnitStyle : uint8_t {
    Tribal,         ///< Animal skins, primitive weapons - Stone Age
    Bronze,         ///< Bronze armor, spears - Bronze Age
    Iron,           ///< Iron armor, swords - Iron Age
    Feudal,         ///< Plate armor, heraldry - Medieval Age
    Colonial,       ///< Uniforms, muskets - Industrial Age
    Military,       ///< Modern military gear - Modern Age
    SciFi,          ///< Power armor, energy weapons - Future Age

    COUNT
};

[[nodiscard]] inline const char* UnitStyleToString(UnitStyle style) {
    switch (style) {
        case UnitStyle::Tribal:     return "Tribal";
        case UnitStyle::Bronze:     return "Bronze";
        case UnitStyle::Iron:       return "Iron";
        case UnitStyle::Feudal:     return "Feudal";
        case UnitStyle::Colonial:   return "Colonial";
        case UnitStyle::Military:   return "Military";
        case UnitStyle::SciFi:      return "Sci-Fi";
        default:                    return "Unknown";
    }
}

/**
 * @brief UI theme colors and style for each age
 */
struct AgeUITheme {
    std::string primaryColor;       ///< Main UI color (hex)
    std::string secondaryColor;     ///< Accent color (hex)
    std::string backgroundColor;    ///< Background color (hex)
    std::string textColor;          ///< Text color (hex)
    std::string highlightColor;     ///< Highlight/selection color (hex)

    std::string fontStyle;          ///< Font family/style name
    std::string iconSet;            ///< Icon set path prefix
    std::string borderStyle;        ///< Border decoration style

    float uiScale = 1.0f;           ///< UI scale factor
    bool useAnimations = true;      ///< Enable UI animations

    [[nodiscard]] nlohmann::json ToJson() const;
    static AgeUITheme FromJson(const nlohmann::json& j);
};

// ============================================================================
// Age Content
// ============================================================================

/**
 * @brief All content available in a specific age
 *
 * Defines what buildings, units, abilities, and resources become
 * available when a player reaches this age.
 */
struct AgeContent {
    Age age = Age::Stone;

    // Available content
    std::vector<std::string> buildings;         ///< Building IDs available
    std::vector<UnitType> units;                ///< Unit types available
    std::vector<std::string> abilities;         ///< Ability IDs available
    std::vector<ResourceType> resources;        ///< Resource types available

    // Visual themes
    BuildingStyle buildingStyle = BuildingStyle::Primitive;
    UnitStyle unitStyle = UnitStyle::Tribal;
    AgeUITheme uiTheme;

    // Texture prefixes for this age
    std::string buildingTexturePrefix;
    std::string unitTexturePrefix;
    std::string effectTexturePrefix;

    // Audio themes
    std::string musicTrack;
    std::string ambientSounds;
    std::string combatSounds;

    // Gameplay modifiers (base = 1.0)
    float gatherRateMultiplier = 1.0f;          ///< Resource gathering speed
    float buildSpeedMultiplier = 1.0f;          ///< Construction speed
    float combatDamageMultiplier = 1.0f;        ///< Unit damage output
    float defenseMultiplier = 1.0f;             ///< Damage reduction
    float movementSpeedMultiplier = 1.0f;       ///< Unit movement speed
    float productionSpeedMultiplier = 1.0f;     ///< Unit/item production speed
    float healingRateMultiplier = 1.0f;         ///< Health regeneration
    float visionRangeMultiplier = 1.0f;         ///< Sight/vision range

    // Population limits
    int basePopulationCap = 20;                 ///< Base max population
    int populationPerHouse = 5;                 ///< Population per housing unit

    // Storage limits
    int baseStorageCapacity = 500;              ///< Base resource storage

    // Special mechanics unlocked
    bool canTrade = false;                      ///< Trading unlocked
    bool canResearch = false;                   ///< Research building available
    bool canBuildWalls = false;                 ///< Fortification available
    bool canBuildSiege = false;                 ///< Siege weapons available
    bool canBuildNaval = false;                 ///< Ships available
    bool canBuildAir = false;                   ///< Aircraft available

    /**
     * @brief Check if a building is available in this age
     */
    [[nodiscard]] bool HasBuilding(const std::string& buildingId) const;

    /**
     * @brief Check if a unit type is available in this age
     */
    [[nodiscard]] bool HasUnit(UnitType unit) const;

    /**
     * @brief Check if a resource is available in this age
     */
    [[nodiscard]] bool HasResource(ResourceType resource) const;

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static AgeContent FromJson(const nlohmann::json& j);
};

// ============================================================================
// Age Progression Manager
// ============================================================================

/**
 * @brief Manages age-specific content and progression
 *
 * Features:
 * - Defines what's available in each age
 * - Handles visual style transitions
 * - Calculates gameplay modifiers
 * - Manages culture-specific age variations
 *
 * Example usage:
 * @code
 * auto& manager = AgeProgressionManager::Instance();
 * manager.Initialize();
 *
 * // Get content for current age
 * const AgeContent& content = manager.GetAgeContent(Age::Iron);
 *
 * // Check if unit is available
 * if (content.HasUnit(UnitType::Cavalry)) {
 *     // Can train cavalry
 * }
 *
 * // Get modifier
 * float gatherRate = content.gatherRateMultiplier;
 * @endcode
 */
class AgeProgressionManager {
public:
    // Callback types
    using AgeTransitionCallback = std::function<void(Age from, Age to, CultureType culture)>;

    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static AgeProgressionManager& Instance();

    // Non-copyable
    AgeProgressionManager(const AgeProgressionManager&) = delete;
    AgeProgressionManager& operator=(const AgeProgressionManager&) = delete;

    /**
     * @brief Initialize age content definitions
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Age Content Access
    // =========================================================================

    /**
     * @brief Get content for a specific age
     */
    [[nodiscard]] const AgeContent& GetAgeContent(Age age) const;

    /**
     * @brief Get content for an age with culture modifications
     */
    [[nodiscard]] AgeContent GetAgeContentForCulture(Age age, CultureType culture) const;

    /**
     * @brief Get all age contents
     */
    [[nodiscard]] const std::vector<AgeContent>& GetAllAgeContents() const {
        return m_ageContents;
    }

    // =========================================================================
    // Building Availability
    // =========================================================================

    /**
     * @brief Check if building is available at an age
     */
    [[nodiscard]] bool IsBuildingAvailable(const std::string& buildingId, Age age) const;

    /**
     * @brief Get all buildings available up to an age (cumulative)
     */
    [[nodiscard]] std::vector<std::string> GetAvailableBuildings(Age age) const;

    /**
     * @brief Get buildings unlocked specifically at an age
     */
    [[nodiscard]] std::vector<std::string> GetBuildingsUnlockedAtAge(Age age) const;

    /**
     * @brief Get minimum age required for a building
     */
    [[nodiscard]] Age GetBuildingMinAge(const std::string& buildingId) const;

    // =========================================================================
    // Unit Availability
    // =========================================================================

    /**
     * @brief Check if unit is available at an age
     */
    [[nodiscard]] bool IsUnitAvailable(UnitType unit, Age age) const;

    /**
     * @brief Get all units available up to an age (cumulative)
     */
    [[nodiscard]] std::vector<UnitType> GetAvailableUnits(Age age) const;

    /**
     * @brief Get units unlocked specifically at an age
     */
    [[nodiscard]] std::vector<UnitType> GetUnitsUnlockedAtAge(Age age) const;

    /**
     * @brief Get minimum age required for a unit
     */
    [[nodiscard]] Age GetUnitMinAge(UnitType unit) const;

    // =========================================================================
    // Modifier Calculations
    // =========================================================================

    /**
     * @brief Get cumulative gather rate for an age
     */
    [[nodiscard]] float GetGatherRateModifier(Age age) const;

    /**
     * @brief Get cumulative build speed for an age
     */
    [[nodiscard]] float GetBuildSpeedModifier(Age age) const;

    /**
     * @brief Get cumulative combat damage for an age
     */
    [[nodiscard]] float GetCombatDamageModifier(Age age) const;

    /**
     * @brief Get cumulative defense for an age
     */
    [[nodiscard]] float GetDefenseModifier(Age age) const;

    /**
     * @brief Get population cap for an age
     */
    [[nodiscard]] int GetPopulationCap(Age age) const;

    /**
     * @brief Get storage capacity for an age
     */
    [[nodiscard]] int GetStorageCapacity(Age age) const;

    // =========================================================================
    // Visual Style
    // =========================================================================

    /**
     * @brief Get building style for an age
     */
    [[nodiscard]] BuildingStyle GetBuildingStyle(Age age) const;

    /**
     * @brief Get unit style for an age
     */
    [[nodiscard]] UnitStyle GetUnitStyle(Age age) const;

    /**
     * @brief Get UI theme for an age
     */
    [[nodiscard]] const AgeUITheme& GetUITheme(Age age) const;

    /**
     * @brief Get building texture prefix for an age
     */
    [[nodiscard]] std::string GetBuildingTexturePrefix(Age age) const;

    /**
     * @brief Get unit texture prefix for an age
     */
    [[nodiscard]] std::string GetUnitTexturePrefix(Age age) const;

    // =========================================================================
    // Age Transitions
    // =========================================================================

    /**
     * @brief Handle age transition (triggers visual/audio changes)
     * @param from Previous age
     * @param to New age
     * @param culture Player's culture
     */
    void OnAgeTransition(Age from, Age to, CultureType culture);

    /**
     * @brief Set callback for age transitions
     */
    void SetOnAgeTransition(AgeTransitionCallback callback) {
        m_onAgeTransition = std::move(callback);
    }

    // =========================================================================
    // Culture-Specific Content
    // =========================================================================

    /**
     * @brief Add culture-specific building to an age
     */
    void AddCultureBuilding(Age age, CultureType culture, const std::string& buildingId);

    /**
     * @brief Add culture-specific unit to an age
     */
    void AddCultureUnit(Age age, CultureType culture, UnitType unit);

    /**
     * @brief Get culture-specific modifier adjustment
     */
    [[nodiscard]] float GetCultureModifier(CultureType culture, const std::string& stat) const;

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize all age contents to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Load age contents from JSON
     */
    void FromJson(const nlohmann::json& j);

private:
    AgeProgressionManager() = default;
    ~AgeProgressionManager() = default;

    // Initialization helpers
    void InitializeStoneAge();
    void InitializeBronzeAge();
    void InitializeIronAge();
    void InitializeMedievalAge();
    void InitializeIndustrialAge();
    void InitializeModernAge();
    void InitializeFutureAge();
    void InitializeUIThemes();

    bool m_initialized = false;

    // Age content storage
    std::vector<AgeContent> m_ageContents;

    // Culture-specific additions
    struct CultureAgeAddition {
        std::vector<std::string> buildings;
        std::vector<UnitType> units;
        std::vector<std::string> abilities;
        std::map<std::string, float> modifiers;
    };
    std::map<std::pair<Age, CultureType>, CultureAgeAddition> m_cultureAdditions;

    // Callbacks
    AgeTransitionCallback m_onAgeTransition;
};

// ============================================================================
// Age Progression Helpers
// ============================================================================

/**
 * @brief Get total progression value (0.0 = Stone Age start, 1.0 = Future Age complete)
 */
[[nodiscard]] float GetProgressionValue(Age age, float ageProgress = 0.0f);

/**
 * @brief Get age from progression value
 */
[[nodiscard]] Age GetAgeFromProgressionValue(float progress);

/**
 * @brief Get color interpolated between two ages
 */
[[nodiscard]] std::string InterpolateAgeColor(
    Age fromAge, Age toAge, float t,
    const AgeProgressionManager& manager);

/**
 * @brief Calculate power level for an age (for matchmaking)
 */
[[nodiscard]] int CalculateAgePowerLevel(Age age, int techCount = 0);

} // namespace RTS
} // namespace Vehement
