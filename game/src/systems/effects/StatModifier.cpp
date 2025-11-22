#include "StatModifier.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace Vehement {
namespace Effects {

// ============================================================================
// Stat Type Conversion
// ============================================================================

static const std::unordered_map<std::string, StatType> s_statTypeFromString = {
    {"health", StatType::Health},
    {"max_health", StatType::MaxHealth},
    {"mana", StatType::Mana},
    {"max_mana", StatType::MaxMana},
    {"stamina", StatType::Stamina},
    {"max_stamina", StatType::MaxStamina},
    {"damage", StatType::Damage},
    {"physical_damage", StatType::PhysicalDamage},
    {"magic_damage", StatType::MagicDamage},
    {"fire_damage", StatType::FireDamage},
    {"ice_damage", StatType::IceDamage},
    {"lightning_damage", StatType::LightningDamage},
    {"poison_damage", StatType::PoisonDamage},
    {"holy_damage", StatType::HolyDamage},
    {"dark_damage", StatType::DarkDamage},
    {"armor", StatType::Armor},
    {"physical_resist", StatType::PhysicalResist},
    {"magic_resist", StatType::MagicResist},
    {"fire_resist", StatType::FireResist},
    {"ice_resist", StatType::IceResist},
    {"lightning_resist", StatType::LightningResist},
    {"poison_resist", StatType::PoisonResist},
    {"attack_speed", StatType::AttackSpeed},
    {"cast_speed", StatType::CastSpeed},
    {"crit_chance", StatType::CritChance},
    {"crit_multiplier", StatType::CritMultiplier},
    {"accuracy", StatType::Accuracy},
    {"armor_penetration", StatType::ArmorPenetration},
    {"move_speed", StatType::MoveSpeed},
    {"speed", StatType::MoveSpeed},  // Alias
    {"jump_height", StatType::JumpHeight},
    {"dodge", StatType::Dodge},
    {"health_regen", StatType::HealthRegen},
    {"mana_regen", StatType::ManaRegen},
    {"stamina_regen", StatType::StaminaRegen},
    {"lifesteal", StatType::Lifesteal},
    {"mana_leech", StatType::ManaLeech},
    {"cooldown_reduction", StatType::CooldownReduction},
    {"range", StatType::Range},
    {"area_of_effect", StatType::AreaOfEffect},
    {"aoe", StatType::AreaOfEffect},  // Alias
    {"duration", StatType::Duration},
    {"threat_modifier", StatType::ThreatModifier},
    {"threat", StatType::ThreatModifier},  // Alias
    {"experience_gain", StatType::ExperienceGain},
    {"xp_gain", StatType::ExperienceGain},  // Alias
    {"gold_find", StatType::GoldFind},
    {"loot_chance", StatType::LootChance}
};

static const char* s_statTypeStrings[] = {
    "health", "max_health", "mana", "max_mana", "stamina", "max_stamina",
    "damage", "physical_damage", "magic_damage", "fire_damage", "ice_damage",
    "lightning_damage", "poison_damage", "holy_damage", "dark_damage",
    "armor", "physical_resist", "magic_resist", "fire_resist", "ice_resist",
    "lightning_resist", "poison_resist",
    "attack_speed", "cast_speed", "crit_chance", "crit_multiplier",
    "accuracy", "armor_penetration",
    "move_speed", "jump_height", "dodge",
    "health_regen", "mana_regen", "stamina_regen", "lifesteal", "mana_leech",
    "cooldown_reduction", "range", "area_of_effect", "duration", "threat_modifier",
    "experience_gain", "gold_find", "loot_chance"
};

const char* StatTypeToString(StatType type) {
    size_t index = static_cast<size_t>(type);
    if (index < static_cast<size_t>(StatType::Count)) {
        return s_statTypeStrings[index];
    }
    return "unknown";
}

std::optional<StatType> StatTypeFromString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = s_statTypeFromString.find(lower);
    if (it != s_statTypeFromString.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ============================================================================
// Modifier Operation Conversion
// ============================================================================

static const std::unordered_map<std::string, ModifierOp> s_modifierOpFromString = {
    {"add", ModifierOp::Add},
    {"flat", ModifierOp::Add},
    {"+", ModifierOp::Add},
    {"multiply", ModifierOp::Multiply},
    {"mult", ModifierOp::Multiply},
    {"*", ModifierOp::Multiply},
    {"percent", ModifierOp::Percent},
    {"%", ModifierOp::Percent},
    {"set", ModifierOp::Set},
    {"=", ModifierOp::Set},
    {"min", ModifierOp::Min},
    {"max", ModifierOp::Max}
};

const char* ModifierOpToString(ModifierOp op) {
    switch (op) {
        case ModifierOp::Add:      return "add";
        case ModifierOp::Multiply: return "multiply";
        case ModifierOp::Percent:  return "percent";
        case ModifierOp::Set:      return "set";
        case ModifierOp::Min:      return "min";
        case ModifierOp::Max:      return "max";
    }
    return "add";
}

std::optional<ModifierOp> ModifierOpFromString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = s_modifierOpFromString.find(lower);
    if (it != s_modifierOpFromString.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ============================================================================
// Condition Type Conversion
// ============================================================================

static const std::unordered_map<std::string, ConditionType> s_conditionTypeFromString = {
    {"none", ConditionType::None},
    {"health_below", ConditionType::HealthBelow},
    {"health_above", ConditionType::HealthAbove},
    {"mana_below", ConditionType::ManaBelow},
    {"mana_above", ConditionType::ManaAbove},
    {"stamina_below", ConditionType::StaminaBelow},
    {"in_combat", ConditionType::InCombat},
    {"out_of_combat", ConditionType::OutOfCombat},
    {"moving", ConditionType::MovingSpeed},
    {"stationary", ConditionType::Stationary},
    {"has_buff", ConditionType::HasBuff},
    {"has_debuff", ConditionType::HasDebuff},
    {"target_health_below", ConditionType::TargetHealthBelow},
    {"enemies_nearby", ConditionType::EnemiesNearby},
    {"allies_nearby", ConditionType::AlliesNearby},
    {"time_of_day", ConditionType::TimeOfDay},
    {"is_stealthed", ConditionType::IsStealthed},
    {"is_mounted", ConditionType::IsMounted},
    {"full_health", ConditionType::HasFullHealth},
    {"recently_damaged", ConditionType::RecentlyDamaged},
    {"killed_recently", ConditionType::KilledRecently}
};

const char* ConditionTypeToString(ConditionType type) {
    switch (type) {
        case ConditionType::None:             return "none";
        case ConditionType::HealthBelow:      return "health_below";
        case ConditionType::HealthAbove:      return "health_above";
        case ConditionType::ManaBelow:        return "mana_below";
        case ConditionType::ManaAbove:        return "mana_above";
        case ConditionType::StaminaBelow:     return "stamina_below";
        case ConditionType::InCombat:         return "in_combat";
        case ConditionType::OutOfCombat:      return "out_of_combat";
        case ConditionType::MovingSpeed:      return "moving";
        case ConditionType::Stationary:       return "stationary";
        case ConditionType::HasBuff:          return "has_buff";
        case ConditionType::HasDebuff:        return "has_debuff";
        case ConditionType::TargetHealthBelow: return "target_health_below";
        case ConditionType::EnemiesNearby:    return "enemies_nearby";
        case ConditionType::AlliesNearby:     return "allies_nearby";
        case ConditionType::TimeOfDay:        return "time_of_day";
        case ConditionType::IsStealthed:      return "is_stealthed";
        case ConditionType::IsMounted:        return "is_mounted";
        case ConditionType::HasFullHealth:    return "full_health";
        case ConditionType::RecentlyDamaged:  return "recently_damaged";
        case ConditionType::KilledRecently:   return "killed_recently";
    }
    return "none";
}

std::optional<ConditionType> ConditionTypeFromString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = s_conditionTypeFromString.find(lower);
    if (it != s_conditionTypeFromString.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ============================================================================
// Modifier Condition
// ============================================================================

bool ModifierCondition::Evaluate(const std::unordered_map<std::string, float>& context) const {
    bool result = false;

    switch (type) {
        case ConditionType::None:
            result = true;
            break;

        case ConditionType::HealthBelow: {
            auto it = context.find("health_percent");
            result = (it != context.end()) && (it->second < threshold);
            break;
        }

        case ConditionType::HealthAbove: {
            auto it = context.find("health_percent");
            result = (it != context.end()) && (it->second > threshold);
            break;
        }

        case ConditionType::ManaBelow: {
            auto it = context.find("mana_percent");
            result = (it != context.end()) && (it->second < threshold);
            break;
        }

        case ConditionType::ManaAbove: {
            auto it = context.find("mana_percent");
            result = (it != context.end()) && (it->second > threshold);
            break;
        }

        case ConditionType::StaminaBelow: {
            auto it = context.find("stamina_percent");
            result = (it != context.end()) && (it->second < threshold);
            break;
        }

        case ConditionType::InCombat: {
            auto it = context.find("in_combat");
            result = (it != context.end()) && (it->second > 0.5f);
            break;
        }

        case ConditionType::OutOfCombat: {
            auto it = context.find("in_combat");
            result = (it == context.end()) || (it->second < 0.5f);
            break;
        }

        case ConditionType::MovingSpeed: {
            auto it = context.find("current_speed");
            result = (it != context.end()) && (it->second > threshold);
            break;
        }

        case ConditionType::Stationary: {
            auto it = context.find("current_speed");
            result = (it == context.end()) || (it->second < 0.1f);
            break;
        }

        case ConditionType::HasBuff: {
            auto it = context.find("has_buff_" + parameter);
            result = (it != context.end()) && (it->second > 0.5f);
            break;
        }

        case ConditionType::HasDebuff: {
            auto it = context.find("has_debuff_" + parameter);
            result = (it != context.end()) && (it->second > 0.5f);
            break;
        }

        case ConditionType::TargetHealthBelow: {
            auto it = context.find("target_health_percent");
            result = (it != context.end()) && (it->second < threshold);
            break;
        }

        case ConditionType::EnemiesNearby: {
            auto it = context.find("enemies_nearby");
            result = (it != context.end()) && (it->second >= threshold);
            break;
        }

        case ConditionType::AlliesNearby: {
            auto it = context.find("allies_nearby");
            result = (it != context.end()) && (it->second >= threshold);
            break;
        }

        case ConditionType::IsStealthed: {
            auto it = context.find("is_stealthed");
            result = (it != context.end()) && (it->second > 0.5f);
            break;
        }

        case ConditionType::IsMounted: {
            auto it = context.find("is_mounted");
            result = (it != context.end()) && (it->second > 0.5f);
            break;
        }

        case ConditionType::HasFullHealth: {
            auto it = context.find("health_percent");
            result = (it != context.end()) && (it->second >= 99.9f);
            break;
        }

        case ConditionType::RecentlyDamaged: {
            auto it = context.find("last_damage_time");
            result = (it != context.end()) && (it->second < threshold);
            break;
        }

        case ConditionType::KilledRecently: {
            auto it = context.find("last_kill_time");
            result = (it != context.end()) && (it->second < threshold);
            break;
        }

        default:
            result = true;
            break;
    }

    return inverted ? !result : result;
}

bool ModifierCondition::LoadFromJson(const std::string& jsonStr) {
    // Simple JSON parsing for condition
    // In production, use a proper JSON library
    return true;
}

std::string ModifierCondition::ToJson() const {
    std::ostringstream ss;
    ss << "{";
    ss << "\"type\":\"" << ConditionTypeToString(type) << "\"";
    ss << ",\"threshold\":" << threshold;
    if (!parameter.empty()) {
        ss << ",\"parameter\":\"" << parameter << "\"";
    }
    if (inverted) {
        ss << ",\"inverted\":true";
    }
    ss << "}";
    return ss.str();
}

// ============================================================================
// Stat Modifier
// ============================================================================

float StatModifier::Apply(float baseValue, float currentValue) const {
    switch (operation) {
        case ModifierOp::Add:
            return currentValue + value;

        case ModifierOp::Multiply:
            return currentValue * value;

        case ModifierOp::Percent:
            return currentValue + (baseValue * value / 100.0f);

        case ModifierOp::Set:
            return value;

        case ModifierOp::Min:
            return std::max(currentValue, value);

        case ModifierOp::Max:
            return std::min(currentValue, value);
    }
    return currentValue;
}

bool StatModifier::ShouldApply(const std::unordered_map<std::string, float>& context) const {
    if (!condition.has_value()) {
        return true;
    }
    return condition->Evaluate(context);
}

bool StatModifier::LoadFromJson(const std::string& jsonStr) {
    // JSON parsing would go here
    return true;
}

std::string StatModifier::ToJson() const {
    std::ostringstream ss;
    ss << "{";
    ss << "\"stat\":\"" << StatTypeToString(stat) << "\"";
    ss << ",\"op\":\"" << ModifierOpToString(operation) << "\"";
    ss << ",\"value\":" << value;
    if (priority != 0) {
        ss << ",\"priority\":" << priority;
    }
    if (sourceId != 0) {
        ss << ",\"source_id\":" << sourceId;
    }
    if (!sourceTag.empty()) {
        ss << ",\"source_tag\":\"" << sourceTag << "\"";
    }
    if (condition.has_value()) {
        ss << ",\"condition\":" << condition->ToJson();
    }
    ss << "}";
    return ss.str();
}

// ============================================================================
// Stat Block
// ============================================================================

StatBlock::StatBlock() {
    // Initialize default stats
    m_baseStats[StatType::Health] = 100.0f;
    m_baseStats[StatType::MaxHealth] = 100.0f;
    m_baseStats[StatType::Mana] = 100.0f;
    m_baseStats[StatType::MaxMana] = 100.0f;
    m_baseStats[StatType::Stamina] = 100.0f;
    m_baseStats[StatType::MaxStamina] = 100.0f;
    m_baseStats[StatType::Damage] = 10.0f;
    m_baseStats[StatType::Armor] = 0.0f;
    m_baseStats[StatType::AttackSpeed] = 1.0f;
    m_baseStats[StatType::MoveSpeed] = 5.0f;
    m_baseStats[StatType::CritChance] = 5.0f;
    m_baseStats[StatType::CritMultiplier] = 200.0f;

    m_finalStats = m_baseStats;
}

float StatBlock::GetBaseStat(StatType stat) const {
    auto it = m_baseStats.find(stat);
    return (it != m_baseStats.end()) ? it->second : 0.0f;
}

void StatBlock::SetBaseStat(StatType stat, float value) {
    m_baseStats[stat] = value;
    m_dirty = true;
}

float StatBlock::GetFinalStat(StatType stat) const {
    auto it = m_finalStats.find(stat);
    return (it != m_finalStats.end()) ? it->second : GetBaseStat(stat);
}

void StatBlock::AddModifier(const StatModifier& modifier) {
    m_modifiers.push_back(modifier);
    std::sort(m_modifiers.begin(), m_modifiers.end());
    m_dirty = true;
}

void StatBlock::RemoveModifiersBySource(uint32_t sourceId) {
    m_modifiers.erase(
        std::remove_if(m_modifiers.begin(), m_modifiers.end(),
            [sourceId](const StatModifier& mod) {
                return mod.sourceId == sourceId;
            }),
        m_modifiers.end()
    );
    m_dirty = true;
}

void StatBlock::RemoveModifiersByTag(const std::string& tag) {
    m_modifiers.erase(
        std::remove_if(m_modifiers.begin(), m_modifiers.end(),
            [&tag](const StatModifier& mod) {
                return mod.sourceTag == tag;
            }),
        m_modifiers.end()
    );
    m_dirty = true;
}

void StatBlock::ClearAllModifiers() {
    m_modifiers.clear();
    m_dirty = true;
}

void StatBlock::Recalculate(const std::unordered_map<std::string, float>& context) {
    if (!m_dirty) return;

    // Reset final stats to base
    m_finalStats = m_baseStats;

    // Group modifiers by stat and priority
    // Apply in order: flat adds, percent adds, multipliers, set/min/max

    for (size_t i = 0; i < static_cast<size_t>(StatType::Count); ++i) {
        CalculateStat(static_cast<StatType>(i), context);
    }

    m_dirty = false;
}

void StatBlock::CalculateStat(StatType stat, const std::unordered_map<std::string, float>& context) {
    float baseValue = GetBaseStat(stat);
    float currentValue = baseValue;

    // Collect applicable modifiers for this stat
    std::vector<const StatModifier*> applicableModifiers;
    for (const auto& mod : m_modifiers) {
        if (mod.stat == stat && mod.ShouldApply(context)) {
            applicableModifiers.push_back(&mod);
        }
    }

    // Apply in order: Add, Percent, Multiply, Set/Min/Max
    // First pass: Add
    for (const auto* mod : applicableModifiers) {
        if (mod->operation == ModifierOp::Add) {
            currentValue = mod->Apply(baseValue, currentValue);
        }
    }

    // Second pass: Percent
    for (const auto* mod : applicableModifiers) {
        if (mod->operation == ModifierOp::Percent) {
            currentValue = mod->Apply(baseValue, currentValue);
        }
    }

    // Third pass: Multiply
    for (const auto* mod : applicableModifiers) {
        if (mod->operation == ModifierOp::Multiply) {
            currentValue = mod->Apply(baseValue, currentValue);
        }
    }

    // Fourth pass: Set/Min/Max (these override)
    for (const auto* mod : applicableModifiers) {
        if (mod->operation == ModifierOp::Set ||
            mod->operation == ModifierOp::Min ||
            mod->operation == ModifierOp::Max) {
            currentValue = mod->Apply(baseValue, currentValue);
        }
    }

    m_finalStats[stat] = currentValue;
}

bool StatBlock::LoadFromJson(const std::string& jsonStr) {
    // JSON parsing for base stats
    return true;
}

std::string StatBlock::ToJson() const {
    std::ostringstream ss;
    ss << "{";
    bool first = true;
    for (const auto& [stat, value] : m_baseStats) {
        if (!first) ss << ",";
        first = false;
        ss << "\"" << StatTypeToString(stat) << "\":" << value;
    }
    ss << "}";
    return ss.str();
}

// ============================================================================
// Stat Modifier Builder
// ============================================================================

StatModifierBuilder& StatModifierBuilder::Stat(StatType stat) {
    m_modifier.stat = stat;
    return *this;
}

StatModifierBuilder& StatModifierBuilder::Operation(ModifierOp op) {
    m_modifier.operation = op;
    return *this;
}

StatModifierBuilder& StatModifierBuilder::Value(float val) {
    m_modifier.value = val;
    return *this;
}

StatModifierBuilder& StatModifierBuilder::Priority(int pri) {
    m_modifier.priority = pri;
    return *this;
}

StatModifierBuilder& StatModifierBuilder::Source(uint32_t id) {
    m_modifier.sourceId = id;
    return *this;
}

StatModifierBuilder& StatModifierBuilder::Tag(const std::string& tag) {
    m_modifier.sourceTag = tag;
    return *this;
}

StatModifierBuilder& StatModifierBuilder::When(ConditionType condition, float threshold) {
    if (!m_modifier.condition.has_value()) {
        m_modifier.condition = ModifierCondition{};
    }
    m_modifier.condition->type = condition;
    m_modifier.condition->threshold = threshold;
    return *this;
}

StatModifierBuilder& StatModifierBuilder::WithParam(const std::string& param) {
    if (!m_modifier.condition.has_value()) {
        m_modifier.condition = ModifierCondition{};
    }
    m_modifier.condition->parameter = param;
    return *this;
}

StatModifierBuilder& StatModifierBuilder::Inverted(bool inv) {
    if (!m_modifier.condition.has_value()) {
        m_modifier.condition = ModifierCondition{};
    }
    m_modifier.condition->inverted = inv;
    return *this;
}

} // namespace Effects
} // namespace Vehement
