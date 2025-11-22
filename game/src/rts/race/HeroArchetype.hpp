#pragma once

/**
 * @file HeroArchetype.hpp
 * @brief Hero template definitions for RTS races
 *
 * Defines hero archetypes including warriors, mages, rangers, support, and specialists.
 */

#include "UnitArchetype.hpp"
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {
namespace Race {

// ============================================================================
// Hero Classes
// ============================================================================

enum class HeroClass : uint8_t {
    Warrior = 0,    ///< Tank/damage melee
    Mage,           ///< Damage/support caster
    Ranger,         ///< Scout/sniper/beastmaster
    Support,        ///< Healer/buffer/aura
    Specialist,     ///< Siege/stealth/necromancer

    COUNT
};

[[nodiscard]] inline const char* HeroClassToString(HeroClass c) {
    switch (c) {
        case HeroClass::Warrior: return "Warrior";
        case HeroClass::Mage: return "Mage";
        case HeroClass::Ranger: return "Ranger";
        case HeroClass::Support: return "Support";
        case HeroClass::Specialist: return "Specialist";
        default: return "Unknown";
    }
}

// ============================================================================
// Hero Subclass
// ============================================================================

enum class HeroSubclass : uint8_t {
    // Warrior
    Tank = 0,
    Berserker,
    Paladin,

    // Mage
    Archmage,
    Warlock,
    Summoner,

    // Ranger
    Scout,
    Sniper,
    Beastmaster,

    // Support
    Healer,
    Buffer,
    Aura,

    // Specialist
    SiegeMaster,
    Assassin,
    Necromancer,

    COUNT
};

[[nodiscard]] const char* HeroSubclassToString(HeroSubclass s);
[[nodiscard]] HeroSubclass StringToHeroSubclass(const std::string& str);

// ============================================================================
// Hero Ability
// ============================================================================

struct HeroAbility {
    std::string abilityId;
    std::string name;
    std::string description;
    int unlockLevel = 1;
    int maxLevel = 3;
    float cooldown = 10.0f;
    float manaCost = 20.0f;
    bool isPassive = false;
    bool isUltimate = false;

    [[nodiscard]] nlohmann::json ToJson() const;
    static HeroAbility FromJson(const nlohmann::json& j);
};

// ============================================================================
// Hero Stats (extends UnitBaseStats)
// ============================================================================

struct HeroBaseStats : public UnitBaseStats {
    int strength = 20;          ///< Affects health/damage
    int agility = 20;           ///< Affects speed/attack speed
    int intelligence = 20;      ///< Affects mana/spell damage

    float mana = 100.0f;
    float maxMana = 100.0f;
    float manaRegen = 1.0f;

    float experienceGain = 1.0f;
    int startingLevel = 1;
    int maxLevel = 10;

    // Per-level gains
    float healthPerLevel = 50.0f;
    float manaPerLevel = 20.0f;
    float damagePerLevel = 3.0f;
    float armorPerLevel = 0.5f;

    int strengthPerLevel = 2;
    int agilityPerLevel = 2;
    int intelligencePerLevel = 2;

    [[nodiscard]] nlohmann::json ToJson() const;
    static HeroBaseStats FromJson(const nlohmann::json& j);
};

// ============================================================================
// Hero Archetype
// ============================================================================

struct HeroArchetype {
    // Identity
    std::string id;
    std::string name;
    std::string title;              ///< e.g., "The Brave"
    std::string description;
    std::string lore;               ///< Backstory
    std::string iconPath;
    std::string portraitPath;

    // Classification
    HeroClass heroClass = HeroClass::Warrior;
    HeroSubclass subclass = HeroSubclass::Tank;

    // Stats
    HeroBaseStats baseStats;

    // Cost
    int goldCost = 500;
    float reviveTime = 60.0f;
    int reviveCost = 250;

    // Requirements
    std::string requiredBuilding;
    std::string requiredTech;
    int requiredAge = 1;

    // Abilities (4 standard + 1 ultimate)
    std::vector<HeroAbility> abilities;
    std::string ultimateAbilityId;

    // Aura/Passive
    std::string passiveAuraId;
    float auraRadius = 0.0f;

    // Combat
    std::string attackType;
    std::string damageType;
    std::string projectileId;

    // Inventory
    int inventorySlots = 6;
    bool canUseItems = true;
    std::vector<std::string> preferredItems;

    // Special flags
    bool canRevive = true;
    bool isUnique = true;           ///< Only one per player
    bool isSummoned = false;

    // Visual
    std::string modelPath;
    std::string animationSet;
    float modelScale = 1.0f;

    // Audio
    std::string selectQuotes;
    std::string moveQuotes;
    std::string attackQuotes;
    std::string deathQuotes;

    // Balance
    int pointCost = 15;
    float powerRating = 3.0f;

    // Tags
    std::vector<std::string> tags;

    // =========================================================================
    // Methods
    // =========================================================================

    [[nodiscard]] HeroBaseStats CalculateStatsAtLevel(int level,
        const std::map<std::string, float>& modifiers = {}) const;

    [[nodiscard]] bool Validate() const;
    [[nodiscard]] std::vector<std::string> GetValidationErrors() const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static HeroArchetype FromJson(const nlohmann::json& j);

    bool SaveToFile(const std::string& filepath) const;
    bool LoadFromFile(const std::string& filepath);
};

// ============================================================================
// Hero Archetype Registry
// ============================================================================

class HeroArchetypeRegistry {
public:
    [[nodiscard]] static HeroArchetypeRegistry& Instance();

    bool Initialize();
    void Shutdown();

    bool RegisterArchetype(const HeroArchetype& archetype);
    [[nodiscard]] const HeroArchetype* GetArchetype(const std::string& id) const;
    [[nodiscard]] std::vector<const HeroArchetype*> GetAllArchetypes() const;
    [[nodiscard]] std::vector<const HeroArchetype*> GetByClass(HeroClass heroClass) const;

    int LoadFromDirectory(const std::string& directory);

private:
    HeroArchetypeRegistry() = default;
    bool m_initialized = false;
    std::map<std::string, HeroArchetype> m_archetypes;
    void InitializeBuiltInArchetypes();
};

// ============================================================================
// Built-in Hero Archetypes
// ============================================================================

// Warrior heroes
[[nodiscard]] HeroArchetype CreateWarriorTankArchetype();
[[nodiscard]] HeroArchetype CreateWarriorBerserkerArchetype();
[[nodiscard]] HeroArchetype CreateWarriorPaladinArchetype();

// Mage heroes
[[nodiscard]] HeroArchetype CreateMageArchmageArchetype();
[[nodiscard]] HeroArchetype CreateMageWarlockArchetype();
[[nodiscard]] HeroArchetype CreateMageSummonerArchetype();

// Ranger heroes
[[nodiscard]] HeroArchetype CreateRangerScoutArchetype();
[[nodiscard]] HeroArchetype CreateRangerSniperArchetype();
[[nodiscard]] HeroArchetype CreateRangerBeastmasterArchetype();

// Support heroes
[[nodiscard]] HeroArchetype CreateSupportHealerArchetype();
[[nodiscard]] HeroArchetype CreateSupportBufferArchetype();
[[nodiscard]] HeroArchetype CreateSupportAuraArchetype();

// Specialist heroes
[[nodiscard]] HeroArchetype CreateSpecialistSiegeArchetype();
[[nodiscard]] HeroArchetype CreateSpecialistAssassinArchetype();
[[nodiscard]] HeroArchetype CreateSpecialistNecromancerArchetype();

} // namespace Race
} // namespace RTS
} // namespace Vehement
