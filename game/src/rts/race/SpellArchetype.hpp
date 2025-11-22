#pragma once

/**
 * @file SpellArchetype.hpp
 * @brief Spell template definitions for RTS races
 *
 * Defines spell archetypes: damage, healing, buff, debuff, summon, utility, ultimate.
 */

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {
namespace Race {

// ============================================================================
// Spell Categories
// ============================================================================

enum class SpellCategory : uint8_t {
    Damage = 0,     ///< Single, AOE, DOT
    Healing,        ///< Single, AOE, HOT
    Buff,           ///< Attack, defense, speed
    Debuff,         ///< Slow, weaken, silence
    Summon,         ///< Units, structures, elementals
    Utility,        ///< Teleport, reveal, dispel
    Ultimate,       ///< Meteor, resurrection, mind control

    COUNT
};

[[nodiscard]] inline const char* SpellCategoryToString(SpellCategory c) {
    switch (c) {
        case SpellCategory::Damage: return "Damage";
        case SpellCategory::Healing: return "Healing";
        case SpellCategory::Buff: return "Buff";
        case SpellCategory::Debuff: return "Debuff";
        case SpellCategory::Summon: return "Summon";
        case SpellCategory::Utility: return "Utility";
        case SpellCategory::Ultimate: return "Ultimate";
        default: return "Unknown";
    }
}

// ============================================================================
// Spell Target Type
// ============================================================================

enum class SpellTargetType : uint8_t {
    Self = 0,
    SingleAlly,
    SingleEnemy,
    SingleUnit,
    AlliedArea,
    EnemyArea,
    AllArea,
    Ground,
    None,           ///< No target (auto-cast)

    COUNT
};

[[nodiscard]] inline const char* SpellTargetTypeToString(SpellTargetType t) {
    switch (t) {
        case SpellTargetType::Self: return "Self";
        case SpellTargetType::SingleAlly: return "SingleAlly";
        case SpellTargetType::SingleEnemy: return "SingleEnemy";
        case SpellTargetType::SingleUnit: return "SingleUnit";
        case SpellTargetType::AlliedArea: return "AlliedArea";
        case SpellTargetType::EnemyArea: return "EnemyArea";
        case SpellTargetType::AllArea: return "AllArea";
        case SpellTargetType::Ground: return "Ground";
        case SpellTargetType::None: return "None";
        default: return "Unknown";
    }
}

// ============================================================================
// Spell Effect
// ============================================================================

struct SpellEffect {
    std::string effectType;         ///< "damage", "heal", "buff", etc.
    std::string statAffected;       ///< Stat to modify
    float baseValue = 0.0f;         ///< Base effect value
    float scalingFactor = 0.0f;     ///< Intelligence/stat scaling
    float duration = 0.0f;          ///< Effect duration (0 = instant)
    float tickRate = 1.0f;          ///< For DOT/HOT effects
    std::string appliedEffect;      ///< Status effect ID to apply

    [[nodiscard]] nlohmann::json ToJson() const;
    static SpellEffect FromJson(const nlohmann::json& j);
};

// ============================================================================
// Spell Archetype
// ============================================================================

struct SpellArchetype {
    // Identity
    std::string id;
    std::string name;
    std::string description;
    std::string iconPath;

    // Classification
    SpellCategory category = SpellCategory::Damage;
    SpellTargetType targetType = SpellTargetType::SingleEnemy;

    // Cost and timing
    float manaCost = 50.0f;
    float cooldown = 10.0f;
    float castTime = 0.5f;          ///< Channel time
    float range = 10.0f;
    float radius = 0.0f;            ///< AOE radius (0 = single target)

    // Effects
    std::vector<SpellEffect> effects;

    // Summon properties
    std::string summonUnitId;
    int summonCount = 1;
    float summonDuration = 60.0f;

    // Requirements
    std::string requiredBuilding;
    std::string requiredTech;
    int requiredAge = 0;

    // Upgrades
    bool canUpgrade = true;
    int maxLevel = 3;
    float manaCostPerLevel = 10.0f;
    float effectPerLevel = 0.2f;    ///< +20% per level

    // Visual/Audio
    std::string castEffect;
    std::string impactEffect;
    std::string projectileId;
    std::string castSound;
    std::string impactSound;

    // Balance
    int pointCost = 5;
    float powerRating = 1.0f;

    // Tags
    std::vector<std::string> tags;

    // =========================================================================
    // Methods
    // =========================================================================

    [[nodiscard]] float CalculateEffectValue(int level, float casterStat = 0.0f) const;
    [[nodiscard]] float CalculateManaCost(int level) const;
    [[nodiscard]] float CalculateCooldown(int level, float cdrBonus = 0.0f) const;

    [[nodiscard]] bool Validate() const;
    [[nodiscard]] std::vector<std::string> GetValidationErrors() const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static SpellArchetype FromJson(const nlohmann::json& j);

    bool SaveToFile(const std::string& filepath) const;
    bool LoadFromFile(const std::string& filepath);
};

// ============================================================================
// Spell Archetype Registry
// ============================================================================

class SpellArchetypeRegistry {
public:
    [[nodiscard]] static SpellArchetypeRegistry& Instance();

    bool Initialize();
    void Shutdown();

    bool RegisterArchetype(const SpellArchetype& archetype);
    [[nodiscard]] const SpellArchetype* GetArchetype(const std::string& id) const;
    [[nodiscard]] std::vector<const SpellArchetype*> GetAllArchetypes() const;
    [[nodiscard]] std::vector<const SpellArchetype*> GetByCategory(SpellCategory cat) const;

    int LoadFromDirectory(const std::string& directory);

private:
    SpellArchetypeRegistry() = default;
    bool m_initialized = false;
    std::map<std::string, SpellArchetype> m_archetypes;
    void InitializeBuiltInArchetypes();
};

// ============================================================================
// Built-in Spell Archetypes
// ============================================================================

// Damage spells
[[nodiscard]] SpellArchetype CreateDamageSingleArchetype();
[[nodiscard]] SpellArchetype CreateDamageAOEArchetype();
[[nodiscard]] SpellArchetype CreateDamageDOTArchetype();

// Healing spells
[[nodiscard]] SpellArchetype CreateHealingSingleArchetype();
[[nodiscard]] SpellArchetype CreateHealingAOEArchetype();
[[nodiscard]] SpellArchetype CreateHealingHOTArchetype();

// Buff spells
[[nodiscard]] SpellArchetype CreateBuffAttackArchetype();
[[nodiscard]] SpellArchetype CreateBuffDefenseArchetype();
[[nodiscard]] SpellArchetype CreateBuffSpeedArchetype();

// Debuff spells
[[nodiscard]] SpellArchetype CreateDebuffSlowArchetype();
[[nodiscard]] SpellArchetype CreateDebuffWeakenArchetype();
[[nodiscard]] SpellArchetype CreateDebuffSilenceArchetype();

// Summon spells
[[nodiscard]] SpellArchetype CreateSummonUnitsArchetype();
[[nodiscard]] SpellArchetype CreateSummonStructureArchetype();
[[nodiscard]] SpellArchetype CreateSummonElementalArchetype();

// Utility spells
[[nodiscard]] SpellArchetype CreateUtilityTeleportArchetype();
[[nodiscard]] SpellArchetype CreateUtilityRevealArchetype();
[[nodiscard]] SpellArchetype CreateUtilityDispelArchetype();

// Ultimate spells
[[nodiscard]] SpellArchetype CreateUltimateMeteorArchetype();
[[nodiscard]] SpellArchetype CreateUltimateResurrectionArchetype();
[[nodiscard]] SpellArchetype CreateUltimateMindControlArchetype();

} // namespace Race
} // namespace RTS
} // namespace Vehement
