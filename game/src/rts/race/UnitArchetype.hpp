#pragma once

/**
 * @file UnitArchetype.hpp
 * @brief Unit template definitions for RTS races
 *
 * Defines all unit archetypes that races can use including workers,
 * infantry, ranged, cavalry, siege, naval, air, and special units.
 */

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {
namespace Race {

// ============================================================================
// Unit Categories
// ============================================================================

/**
 * @brief Primary unit category
 */
enum class UnitCategory : uint8_t {
    Worker = 0,     ///< Resource gatherers and builders
    Infantry,       ///< Ground melee units
    Ranged,         ///< Ground ranged units
    Cavalry,        ///< Mounted units
    Siege,          ///< Siege weapons
    Naval,          ///< Water units
    Air,            ///< Flying units
    Special,        ///< Unique/special units

    COUNT
};

[[nodiscard]] inline const char* UnitCategoryToString(UnitCategory cat) {
    switch (cat) {
        case UnitCategory::Worker:   return "Worker";
        case UnitCategory::Infantry: return "Infantry";
        case UnitCategory::Ranged:   return "Ranged";
        case UnitCategory::Cavalry:  return "Cavalry";
        case UnitCategory::Siege:    return "Siege";
        case UnitCategory::Naval:    return "Naval";
        case UnitCategory::Air:      return "Air";
        case UnitCategory::Special:  return "Special";
        default:                     return "Unknown";
    }
}

// ============================================================================
// Unit Subtypes
// ============================================================================

/**
 * @brief Specific unit subtypes within categories
 */
enum class UnitSubtype : uint8_t {
    // Worker subtypes
    Harvester = 0,
    Builder,
    Scout,

    // Infantry subtypes
    Melee,
    Pike,
    Shield,
    Berserker,

    // Ranged subtypes
    Archer,
    Gunner,
    Caster,
    Thrower,

    // Cavalry subtypes
    Light,
    Heavy,
    Chariot,
    BeastRider,

    // Siege subtypes
    Catapult,
    Ram,
    Tower,
    Cannon,

    // Naval subtypes
    Transport,
    Warship,
    Submarine,
    Carrier,

    // Air subtypes
    AirScout,
    Fighter,
    Bomber,
    Transport_Air,

    // Special subtypes
    Assassin,
    Healer,
    Summoner,
    Commander,

    COUNT
};

[[nodiscard]] const char* UnitSubtypeToString(UnitSubtype subtype);
[[nodiscard]] UnitSubtype StringToUnitSubtype(const std::string& str);

// ============================================================================
// Base Unit Stats
// ============================================================================

/**
 * @brief Base statistics for a unit
 */
struct UnitBaseStats {
    // Combat stats
    int health = 100;
    int maxHealth = 100;
    int armor = 0;
    int magicResist = 0;
    int damage = 10;
    float attackSpeed = 1.0f;       ///< Attacks per second
    float attackRange = 1.0f;       ///< Attack range in tiles

    // Movement stats
    float moveSpeed = 4.0f;         ///< Tiles per second
    float turnSpeed = 180.0f;       ///< Degrees per second

    // Vision stats
    float visionRange = 8.0f;       ///< Vision range in tiles
    float detectionRange = 4.0f;    ///< Detection range for stealth

    // Resource stats (for workers)
    int carryCapacity = 0;
    float gatherSpeed = 1.0f;
    float buildSpeed = 1.0f;

    // Population
    int populationCost = 1;

    [[nodiscard]] nlohmann::json ToJson() const;
    static UnitBaseStats FromJson(const nlohmann::json& j);
};

// ============================================================================
// Unit Cost
// ============================================================================

/**
 * @brief Cost to produce a unit
 */
struct UnitCost {
    int gold = 0;
    int wood = 0;
    int stone = 0;
    int food = 0;
    int metal = 0;
    float buildTime = 10.0f;        ///< Time to produce in seconds

    [[nodiscard]] int GetTotalCost() const {
        return gold + wood + stone + food + metal;
    }

    [[nodiscard]] nlohmann::json ToJson() const;
    static UnitCost FromJson(const nlohmann::json& j);
};

// ============================================================================
// Unit Abilities
// ============================================================================

/**
 * @brief Ability reference for a unit
 */
struct UnitAbilityRef {
    std::string abilityId;
    int unlockLevel = 1;
    bool isPassive = false;

    [[nodiscard]] nlohmann::json ToJson() const;
    static UnitAbilityRef FromJson(const nlohmann::json& j);
};

// ============================================================================
// Unit Archetype Definition
// ============================================================================

/**
 * @brief Complete unit archetype definition
 *
 * Defines a template for a unit type that can be customized per race.
 * Archetypes provide base stats, costs, and abilities that races
 * can modify through their point allocation and bonuses.
 *
 * Example archetypes:
 * - Worker (harvester, builder)
 * - Infantry (melee, pike, shield)
 * - Ranged (archer, gunner, caster)
 * - Cavalry (light, heavy, chariot)
 * - Siege (catapult, ram, tower)
 * - Naval (transport, warship, submarine)
 * - Air (scout, fighter, bomber)
 * - Special (assassin, healer, summoner)
 */
struct UnitArchetype {
    // Identity
    std::string id;                     ///< Unique identifier
    std::string name;                   ///< Display name
    std::string description;            ///< Unit description
    std::string iconPath;               ///< Icon asset path

    // Classification
    UnitCategory category = UnitCategory::Infantry;
    UnitSubtype subtype = UnitSubtype::Melee;

    // Base stats (before race modifiers)
    UnitBaseStats baseStats;

    // Cost to produce
    UnitCost cost;

    // Requirements
    std::string requiredBuilding;       ///< Building needed to produce
    std::string requiredTech;           ///< Tech needed to unlock
    int requiredAge = 0;                ///< Minimum age (0 = Stone)

    // Abilities
    std::vector<UnitAbilityRef> abilities;
    std::vector<std::string> passiveEffects;

    // Combat properties
    std::string attackType;             ///< "melee", "ranged", "magic"
    std::string damageType;             ///< "physical", "magic", "siege", "pierce"
    std::string projectileId;           ///< Projectile for ranged units
    bool canAttackAir = false;
    bool canAttackGround = true;

    // Movement properties
    std::string movementType;           ///< "ground", "fly", "swim", "amphibious"
    bool canClimb = false;
    bool canBurrow = false;

    // Special flags
    bool isHero = false;
    bool isBuilding = false;
    bool isSummoned = false;
    bool isDetector = false;
    bool isStealthed = false;
    bool canGather = false;
    bool canBuild = false;
    bool canRepair = false;
    bool canHeal = false;

    // Upgrade paths
    std::vector<std::string> upgradesTo;
    std::string upgradesFrom;

    // Visual
    std::string modelPath;
    std::string animationSet;
    float modelScale = 1.0f;

    // Audio
    std::string selectSound;
    std::string moveSound;
    std::string attackSound;
    std::string deathSound;

    // Balance
    int pointCost = 5;                  ///< Cost in race design points
    float powerRating = 1.0f;           ///< Relative power for balance

    // Tags for filtering
    std::vector<std::string> tags;

    // =========================================================================
    // Methods
    // =========================================================================

    /**
     * @brief Calculate effective stats with modifiers
     */
    [[nodiscard]] UnitBaseStats CalculateStats(
        const std::map<std::string, float>& modifiers) const;

    /**
     * @brief Get DPS (damage per second)
     */
    [[nodiscard]] float GetDPS() const {
        return baseStats.damage * baseStats.attackSpeed;
    }

    /**
     * @brief Get cost efficiency (power / cost)
     */
    [[nodiscard]] float GetCostEfficiency() const {
        int totalCost = cost.GetTotalCost();
        return (totalCost > 0) ? powerRating / totalCost : 0.0f;
    }

    /**
     * @brief Validate the archetype
     */
    [[nodiscard]] bool Validate() const;

    /**
     * @brief Get validation errors
     */
    [[nodiscard]] std::vector<std::string> GetValidationErrors() const;

    // =========================================================================
    // Serialization
    // =========================================================================

    [[nodiscard]] nlohmann::json ToJson() const;
    static UnitArchetype FromJson(const nlohmann::json& j);

    bool SaveToFile(const std::string& filepath) const;
    bool LoadFromFile(const std::string& filepath);
};

// ============================================================================
// Unit Archetype Registry
// ============================================================================

/**
 * @brief Registry for all unit archetypes
 */
class UnitArchetypeRegistry {
public:
    [[nodiscard]] static UnitArchetypeRegistry& Instance();

    bool Initialize();
    void Shutdown();

    bool RegisterArchetype(const UnitArchetype& archetype);
    bool UnregisterArchetype(const std::string& id);

    [[nodiscard]] const UnitArchetype* GetArchetype(const std::string& id) const;
    [[nodiscard]] std::vector<const UnitArchetype*> GetAllArchetypes() const;
    [[nodiscard]] std::vector<const UnitArchetype*> GetByCategory(UnitCategory category) const;
    [[nodiscard]] std::vector<const UnitArchetype*> GetBySubtype(UnitSubtype subtype) const;

    int LoadFromDirectory(const std::string& directory);

private:
    UnitArchetypeRegistry() = default;

    bool m_initialized = false;
    std::map<std::string, UnitArchetype> m_archetypes;

    void InitializeBuiltInArchetypes();
};

// ============================================================================
// Built-in Unit Archetypes
// ============================================================================

// Worker archetypes
[[nodiscard]] UnitArchetype CreateWorkerArchetype();
[[nodiscard]] UnitArchetype CreateBuilderArchetype();

// Infantry archetypes
[[nodiscard]] UnitArchetype CreateInfantryMeleeArchetype();
[[nodiscard]] UnitArchetype CreateInfantryPikeArchetype();
[[nodiscard]] UnitArchetype CreateInfantryShieldArchetype();
[[nodiscard]] UnitArchetype CreateInfantryBerserkerArchetype();

// Ranged archetypes
[[nodiscard]] UnitArchetype CreateRangedArcherArchetype();
[[nodiscard]] UnitArchetype CreateRangedGunnerArchetype();
[[nodiscard]] UnitArchetype CreateRangedCasterArchetype();

// Cavalry archetypes
[[nodiscard]] UnitArchetype CreateCavalryLightArchetype();
[[nodiscard]] UnitArchetype CreateCavalryHeavyArchetype();
[[nodiscard]] UnitArchetype CreateCavalryChariotArchetype();

// Siege archetypes
[[nodiscard]] UnitArchetype CreateSiegeCatapultArchetype();
[[nodiscard]] UnitArchetype CreateSiegeRamArchetype();
[[nodiscard]] UnitArchetype CreateSiegeTowerArchetype();

// Naval archetypes
[[nodiscard]] UnitArchetype CreateNavalTransportArchetype();
[[nodiscard]] UnitArchetype CreateNavalWarshipArchetype();
[[nodiscard]] UnitArchetype CreateNavalSubmarineArchetype();

// Air archetypes
[[nodiscard]] UnitArchetype CreateAirScoutArchetype();
[[nodiscard]] UnitArchetype CreateAirFighterArchetype();
[[nodiscard]] UnitArchetype CreateAirBomberArchetype();

// Special archetypes
[[nodiscard]] UnitArchetype CreateSpecialAssassinArchetype();
[[nodiscard]] UnitArchetype CreateSpecialHealerArchetype();
[[nodiscard]] UnitArchetype CreateSpecialSummonerArchetype();

} // namespace Race
} // namespace RTS
} // namespace Vehement
