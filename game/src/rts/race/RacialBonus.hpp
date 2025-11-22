#pragma once

/**
 * @file RacialBonus.hpp
 * @brief Racial passive bonuses for RTS races
 */

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {
namespace Race {

// ============================================================================
// Bonus Types
// ============================================================================

enum class BonusType : uint8_t {
    GatherRate = 0,     ///< Resource gathering speed
    BuildSpeed,         ///< Construction speed
    UnitStat,           ///< Unit stat modifier
    BuildingStat,       ///< Building stat modifier
    SpellEnhancement,   ///< Spell power/efficiency
    UniqueAbility,      ///< Special race ability
    EconomyBoost,       ///< Economic bonuses
    MilitaryBoost,      ///< Combat bonuses
    ResearchBoost,      ///< Research speed
    StartingBonus,      ///< Game start advantages

    COUNT
};

[[nodiscard]] inline const char* BonusTypeToString(BonusType t) {
    switch (t) {
        case BonusType::GatherRate: return "GatherRate";
        case BonusType::BuildSpeed: return "BuildSpeed";
        case BonusType::UnitStat: return "UnitStat";
        case BonusType::BuildingStat: return "BuildingStat";
        case BonusType::SpellEnhancement: return "SpellEnhancement";
        case BonusType::UniqueAbility: return "UniqueAbility";
        case BonusType::EconomyBoost: return "EconomyBoost";
        case BonusType::MilitaryBoost: return "MilitaryBoost";
        case BonusType::ResearchBoost: return "ResearchBoost";
        case BonusType::StartingBonus: return "StartingBonus";
        default: return "Unknown";
    }
}

// ============================================================================
// Bonus Effect
// ============================================================================

struct BonusEffect {
    std::string target;             ///< What is affected (stat name, resource type, etc.)
    float value = 0.0f;             ///< Modifier value (multiplier or flat)
    bool isMultiplier = true;       ///< true = multiply, false = flat add
    std::string condition;          ///< Condition for activation (optional)

    [[nodiscard]] nlohmann::json ToJson() const;
    static BonusEffect FromJson(const nlohmann::json& j);
};

// ============================================================================
// Racial Bonus
// ============================================================================

struct RacialBonus {
    // Identity
    std::string id;
    std::string name;
    std::string description;
    std::string iconPath;

    // Classification
    BonusType type = BonusType::UnitStat;
    bool isPassive = true;          ///< Always active
    bool isUnique = false;          ///< Only this race can have it

    // Effects
    std::vector<BonusEffect> effects;

    // Activation
    std::string activationCondition;    ///< When does it activate
    std::string deactivationCondition;  ///< When does it turn off
    float cooldown = 0.0f;              ///< For active abilities
    float duration = 0.0f;              ///< Effect duration (0 = permanent)

    // Requirements
    int requiredAge = 0;
    std::string requiredTech;

    // Balance
    int pointCost = 5;              ///< Cost in race design points
    float powerRating = 1.0f;

    // Tags
    std::vector<std::string> tags;

    // =========================================================================
    // Methods
    // =========================================================================

    [[nodiscard]] float ApplyBonus(float baseValue, const std::string& targetStat) const;
    [[nodiscard]] bool IsApplicable(const std::string& targetStat) const;
    [[nodiscard]] bool Validate() const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static RacialBonus FromJson(const nlohmann::json& j);

    bool SaveToFile(const std::string& filepath) const;
    bool LoadFromFile(const std::string& filepath);
};

// ============================================================================
// Racial Bonus Registry
// ============================================================================

class RacialBonusRegistry {
public:
    [[nodiscard]] static RacialBonusRegistry& Instance();

    bool Initialize();
    void Shutdown();

    bool RegisterBonus(const RacialBonus& bonus);
    [[nodiscard]] const RacialBonus* GetBonus(const std::string& id) const;
    [[nodiscard]] std::vector<const RacialBonus*> GetAllBonuses() const;
    [[nodiscard]] std::vector<const RacialBonus*> GetByType(BonusType type) const;

    int LoadFromDirectory(const std::string& directory);

private:
    RacialBonusRegistry() = default;
    bool m_initialized = false;
    std::map<std::string, RacialBonus> m_bonuses;
    void InitializeBuiltInBonuses();
};

// ============================================================================
// Built-in Racial Bonuses
// ============================================================================

[[nodiscard]] RacialBonus CreateGatherSpeedBonus();
[[nodiscard]] RacialBonus CreateBuildSpeedBonus();
[[nodiscard]] RacialBonus CreateInfantryDamageBonus();
[[nodiscard]] RacialBonus CreateCavalrySpeedBonus();
[[nodiscard]] RacialBonus CreateMagicPowerBonus();
[[nodiscard]] RacialBonus CreateDefenseBonus();
[[nodiscard]] RacialBonus CreateResearchSpeedBonus();
[[nodiscard]] RacialBonus CreateStartingResourcesBonus();

} // namespace Race
} // namespace RTS
} // namespace Vehement
