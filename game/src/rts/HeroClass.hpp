#pragma once

#include <string>
#include <array>
#include <cstdint>

namespace Vehement {
namespace RTS {

/**
 * @brief Hero class/specialization types
 *
 * Each class has unique strengths, abilities, and playstyles.
 * Like Warcraft 3 heroes, the class determines the hero's role
 * in combat, economy, and army composition.
 */
enum class HeroClass : uint8_t {
    Warlord,    // Combat focused, high strength, rally abilities
    Commander,  // Support focused, buffs, larger command radius
    Engineer,   // Building focused, faster construction, repair
    Scout,      // Exploration focused, larger vision, stealth
    Merchant,   // Economy focused, better trade, resource bonuses

    COUNT
};

/**
 * @brief Get string name for hero class
 */
inline const char* HeroClassToString(HeroClass heroClass) {
    switch (heroClass) {
        case HeroClass::Warlord:   return "Warlord";
        case HeroClass::Commander: return "Commander";
        case HeroClass::Engineer:  return "Engineer";
        case HeroClass::Scout:     return "Scout";
        case HeroClass::Merchant:  return "Merchant";
        default:                   return "Unknown";
    }
}

/**
 * @brief Get detailed description for hero class
 */
inline const char* HeroClassDescription(HeroClass heroClass) {
    switch (heroClass) {
        case HeroClass::Warlord:
            return "A fierce warrior who leads from the front. "
                   "Excels in direct combat with powerful offensive abilities. "
                   "Troops nearby fight harder inspired by the Warlord's presence.";
        case HeroClass::Commander:
            return "A tactical genius who enhances allied units. "
                   "Provides powerful buffs and can command units from greater distances. "
                   "Essential for coordinating large army movements.";
        case HeroClass::Engineer:
            return "A master builder who accelerates construction. "
                   "Can repair damaged buildings and units, and builds defenses faster. "
                   "Unlocks advanced fortification blueprints.";
        case HeroClass::Scout:
            return "A stealthy pathfinder with enhanced vision. "
                   "Can detect hidden enemies and move unseen through enemy territory. "
                   "Provides critical intelligence and ambush capabilities.";
        case HeroClass::Merchant:
            return "A savvy trader who maximizes resource gains. "
                   "Generates passive income and reduces building costs. "
                   "Can access special market trades and rare items.";
        default:
            return "Unknown hero class.";
    }
}

/**
 * @brief Base stats for heroes
 */
struct HeroStats {
    float strength = 10.0f;      // Physical power, health, melee damage
    float agility = 10.0f;       // Attack speed, movement speed, dodge
    float intelligence = 10.0f; // Mana pool, ability damage, cooldown reduction

    // Derived values
    [[nodiscard]] float GetBonusHealth() const { return strength * 20.0f; }
    [[nodiscard]] float GetBonusMeleeDamage() const { return strength * 2.0f; }
    [[nodiscard]] float GetAttackSpeedBonus() const { return agility * 0.02f; }
    [[nodiscard]] float GetMoveSpeedBonus() const { return agility * 0.01f; }
    [[nodiscard]] float GetDodgeChance() const { return agility * 0.005f; }
    [[nodiscard]] float GetBonusMana() const { return intelligence * 15.0f; }
    [[nodiscard]] float GetAbilityDamageBonus() const { return intelligence * 1.5f; }
    [[nodiscard]] float GetCooldownReduction() const { return intelligence * 0.005f; }

    HeroStats operator+(const HeroStats& other) const {
        return { strength + other.strength,
                 agility + other.agility,
                 intelligence + other.intelligence };
    }

    HeroStats operator*(float scalar) const {
        return { strength * scalar, agility * scalar, intelligence * scalar };
    }
};

/**
 * @brief Per-level stat gains for each class
 */
struct ClassStatGains {
    float strengthPerLevel;
    float agilityPerLevel;
    float intelligencePerLevel;
};

/**
 * @brief Passive bonuses provided by hero class
 */
struct ClassPassives {
    // Combat
    float damageBonus = 0.0f;           // % bonus damage
    float armorBonus = 0.0f;            // Flat armor bonus
    float healthRegenBonus = 0.0f;      // HP/sec bonus

    // Command
    float commandRadiusBonus = 0.0f;    // % bonus command radius
    float auraRadiusBonus = 0.0f;       // % bonus aura radius
    float allyBuffStrength = 0.0f;      // % stronger ally buffs

    // Building
    float buildSpeedBonus = 0.0f;       // % faster construction
    float repairSpeedBonus = 0.0f;      // % faster repair
    float buildingHealthBonus = 0.0f;   // % bonus building health

    // Scouting
    float visionRangeBonus = 0.0f;      // % bonus vision
    float stealthDetection = 0.0f;      // Detection radius
    float moveSpeedBonus = 0.0f;        // % bonus move speed

    // Economy
    float resourceGatherBonus = 0.0f;   // % bonus resources
    float tradePriceBonus = 0.0f;       // % better trade prices
    float passiveIncomeBonus = 0.0f;    // Gold/minute bonus
};

/**
 * @brief Complete class definition with all stats and bonuses
 */
struct HeroClassDefinition {
    HeroClass type;
    std::string name;
    std::string description;

    // Starting stats at level 1
    HeroStats baseStats;

    // Stat growth per level
    ClassStatGains statGains;

    // Class passive bonuses
    ClassPassives passives;

    // Base values
    float baseHealth = 300.0f;
    float baseMana = 100.0f;
    float baseArmor = 2.0f;
    float baseCommandRadius = 15.0f;
    float baseAuraRadius = 8.0f;
    float baseVisionRange = 12.0f;

    // Ability slots (ability IDs, -1 for empty/locked)
    std::array<int, 4> startingAbilities = {-1, -1, -1, -1};

    // Visual/Audio
    std::string texturePath;
    std::string portraitPath;
    std::string selectSound;
    std::string attackSound;
};

/**
 * @brief Registry of all hero class definitions
 */
class HeroClassRegistry {
public:
    /**
     * @brief Get the singleton instance
     */
    static HeroClassRegistry& Instance() {
        static HeroClassRegistry instance;
        return instance;
    }

    /**
     * @brief Get class definition by type
     */
    const HeroClassDefinition& GetClass(HeroClass type) const {
        return m_classes[static_cast<size_t>(type)];
    }

    /**
     * @brief Get all class definitions
     */
    const std::array<HeroClassDefinition, static_cast<size_t>(HeroClass::COUNT)>& GetAllClasses() const {
        return m_classes;
    }

private:
    HeroClassRegistry() {
        InitializeClasses();
    }

    void InitializeClasses() {
        // =====================================================================
        // WARLORD - Combat focused
        // =====================================================================
        m_classes[static_cast<size_t>(HeroClass::Warlord)] = {
            .type = HeroClass::Warlord,
            .name = "Warlord",
            .description = HeroClassDescription(HeroClass::Warlord),
            .baseStats = {
                .strength = 25.0f,      // High strength
                .agility = 15.0f,       // Medium agility
                .intelligence = 10.0f   // Low intelligence
            },
            .statGains = {
                .strengthPerLevel = 3.5f,
                .agilityPerLevel = 1.5f,
                .intelligencePerLevel = 1.0f
            },
            .passives = {
                .damageBonus = 0.15f,           // +15% damage
                .armorBonus = 3.0f,             // +3 armor
                .healthRegenBonus = 2.0f,       // +2 HP/sec
                .allyBuffStrength = 0.10f       // +10% ally buff strength
            },
            .baseHealth = 400.0f,
            .baseMana = 75.0f,
            .baseArmor = 4.0f,
            .baseCommandRadius = 12.0f,
            .baseAuraRadius = 10.0f,
            .baseVisionRange = 10.0f,
            .startingAbilities = {0, -1, -1, -1},  // Rally
            .texturePath = "rts/heroes/warlord.png",
            .portraitPath = "rts/portraits/warlord.png",
            .selectSound = "rts/sounds/warlord_select.wav",
            .attackSound = "rts/sounds/warlord_attack.wav"
        };

        // =====================================================================
        // COMMANDER - Support focused
        // =====================================================================
        m_classes[static_cast<size_t>(HeroClass::Commander)] = {
            .type = HeroClass::Commander,
            .name = "Commander",
            .description = HeroClassDescription(HeroClass::Commander),
            .baseStats = {
                .strength = 18.0f,
                .agility = 12.0f,
                .intelligence = 20.0f   // High intelligence
            },
            .statGains = {
                .strengthPerLevel = 2.0f,
                .agilityPerLevel = 1.5f,
                .intelligencePerLevel = 2.5f
            },
            .passives = {
                .commandRadiusBonus = 0.50f,    // +50% command radius
                .auraRadiusBonus = 0.30f,       // +30% aura radius
                .allyBuffStrength = 0.25f       // +25% ally buff strength
            },
            .baseHealth = 325.0f,
            .baseMana = 150.0f,
            .baseArmor = 2.0f,
            .baseCommandRadius = 20.0f,         // Large command radius
            .baseAuraRadius = 12.0f,
            .baseVisionRange = 12.0f,
            .startingAbilities = {1, -1, -1, -1},  // Inspire
            .texturePath = "rts/heroes/commander.png",
            .portraitPath = "rts/portraits/commander.png",
            .selectSound = "rts/sounds/commander_select.wav",
            .attackSound = "rts/sounds/commander_attack.wav"
        };

        // =====================================================================
        // ENGINEER - Building focused
        // =====================================================================
        m_classes[static_cast<size_t>(HeroClass::Engineer)] = {
            .type = HeroClass::Engineer,
            .name = "Engineer",
            .description = HeroClassDescription(HeroClass::Engineer),
            .baseStats = {
                .strength = 15.0f,
                .agility = 12.0f,
                .intelligence = 23.0f
            },
            .statGains = {
                .strengthPerLevel = 1.5f,
                .agilityPerLevel = 1.5f,
                .intelligencePerLevel = 3.0f
            },
            .passives = {
                .buildSpeedBonus = 0.40f,       // +40% build speed
                .repairSpeedBonus = 0.50f,      // +50% repair speed
                .buildingHealthBonus = 0.20f    // +20% building health
            },
            .baseHealth = 300.0f,
            .baseMana = 175.0f,
            .baseArmor = 2.0f,
            .baseCommandRadius = 15.0f,
            .baseAuraRadius = 8.0f,
            .baseVisionRange = 10.0f,
            .startingAbilities = {2, -1, -1, -1},  // Fortify
            .texturePath = "rts/heroes/engineer.png",
            .portraitPath = "rts/portraits/engineer.png",
            .selectSound = "rts/sounds/engineer_select.wav",
            .attackSound = "rts/sounds/engineer_attack.wav"
        };

        // =====================================================================
        // SCOUT - Exploration focused
        // =====================================================================
        m_classes[static_cast<size_t>(HeroClass::Scout)] = {
            .type = HeroClass::Scout,
            .name = "Scout",
            .description = HeroClassDescription(HeroClass::Scout),
            .baseStats = {
                .strength = 12.0f,
                .agility = 25.0f,       // High agility
                .intelligence = 13.0f
            },
            .statGains = {
                .strengthPerLevel = 1.5f,
                .agilityPerLevel = 3.5f,
                .intelligencePerLevel = 1.0f
            },
            .passives = {
                .moveSpeedBonus = 0.25f,        // +25% move speed
                .visionRangeBonus = 0.40f,      // +40% vision range
                .stealthDetection = 10.0f       // 10 unit detection radius
            },
            .baseHealth = 275.0f,
            .baseMana = 100.0f,
            .baseArmor = 1.0f,
            .baseCommandRadius = 12.0f,
            .baseAuraRadius = 6.0f,
            .baseVisionRange = 18.0f,           // Large vision
            .startingAbilities = {3, -1, -1, -1},  // Shadowstep
            .texturePath = "rts/heroes/scout.png",
            .portraitPath = "rts/portraits/scout.png",
            .selectSound = "rts/sounds/scout_select.wav",
            .attackSound = "rts/sounds/scout_attack.wav"
        };

        // =====================================================================
        // MERCHANT - Economy focused
        // =====================================================================
        m_classes[static_cast<size_t>(HeroClass::Merchant)] = {
            .type = HeroClass::Merchant,
            .name = "Merchant",
            .description = HeroClassDescription(HeroClass::Merchant),
            .baseStats = {
                .strength = 12.0f,
                .agility = 15.0f,
                .intelligence = 23.0f
            },
            .statGains = {
                .strengthPerLevel = 1.0f,
                .agilityPerLevel = 2.0f,
                .intelligencePerLevel = 3.0f
            },
            .passives = {
                .resourceGatherBonus = 0.20f,   // +20% resources
                .tradePriceBonus = 0.15f,       // +15% better prices
                .passiveIncomeBonus = 10.0f     // +10 gold/minute
            },
            .baseHealth = 275.0f,
            .baseMana = 150.0f,
            .baseArmor = 1.0f,
            .baseCommandRadius = 15.0f,
            .baseAuraRadius = 10.0f,
            .baseVisionRange = 10.0f,
            .startingAbilities = {4, -1, -1, -1},  // Market Mastery
            .texturePath = "rts/heroes/merchant.png",
            .portraitPath = "rts/portraits/merchant.png",
            .selectSound = "rts/sounds/merchant_select.wav",
            .attackSound = "rts/sounds/merchant_attack.wav"
        };
    }

    std::array<HeroClassDefinition, static_cast<size_t>(HeroClass::COUNT)> m_classes;
};

/**
 * @brief Helper to get primary stat for a class
 */
inline const char* GetPrimaryStat(HeroClass heroClass) {
    switch (heroClass) {
        case HeroClass::Warlord:   return "Strength";
        case HeroClass::Commander: return "Intelligence";
        case HeroClass::Engineer:  return "Intelligence";
        case HeroClass::Scout:     return "Agility";
        case HeroClass::Merchant:  return "Intelligence";
        default:                   return "Strength";
    }
}

/**
 * @brief Get recommended playstyle for class
 */
inline const char* GetPlaystyleHint(HeroClass heroClass) {
    switch (heroClass) {
        case HeroClass::Warlord:
            return "Lead your troops into battle. Stay on the frontlines.";
        case HeroClass::Commander:
            return "Position behind your army. Keep buffs active on allies.";
        case HeroClass::Engineer:
            return "Focus on base building. Repair during sieges.";
        case HeroClass::Scout:
            return "Explore the map. Provide vision and pick off stragglers.";
        case HeroClass::Merchant:
            return "Secure resource nodes. Trade often for maximum profit.";
        default:
            return "Play to your strengths.";
    }
}

} // namespace RTS
} // namespace Vehement
