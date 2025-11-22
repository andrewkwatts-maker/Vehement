#include "TechModifier.hpp"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <iomanip>

namespace Vehement {
namespace TechTree {

// ============================================================================
// TargetScope
// ============================================================================

nlohmann::json TargetScope::ToJson() const {
    nlohmann::json j;

    if (type == TargetScopeType::Global && tags.empty() && !excludeHeroes) {
        return "global";
    }

    j["type"] = TargetScopeTypeToString(type);

    if (!target.empty()) {
        switch (type) {
            case TargetScopeType::UnitType:
                j["unit_type"] = target;
                break;
            case TargetScopeType::BuildingType:
                j["building_type"] = target;
                break;
            case TargetScopeType::Specific:
                j["entity_id"] = target;
                break;
            case TargetScopeType::Category:
                j["category"] = target;
                break;
            case TargetScopeType::Faction:
                j["faction"] = target;
                break;
            default:
                j["target"] = target;
                break;
        }
    }

    if (!tags.empty()) {
        j["tags"] = tags;
    }

    if (excludeHeroes) {
        j["exclude_heroes"] = true;
    }

    return j;
}

TargetScope TargetScope::FromJson(const nlohmann::json& j) {
    TargetScope scope;

    if (j.is_string()) {
        std::string scopeStr = j.get<std::string>();
        scope.type = StringToTargetScopeType(scopeStr);
        return scope;
    }

    if (j.contains("type")) {
        scope.type = StringToTargetScopeType(j["type"].get<std::string>());
    }

    if (j.contains("unit_type")) {
        scope.type = TargetScopeType::UnitType;
        scope.target = j["unit_type"].get<std::string>();
    } else if (j.contains("building_type")) {
        scope.type = TargetScopeType::BuildingType;
        scope.target = j["building_type"].get<std::string>();
    } else if (j.contains("entity_id")) {
        scope.type = TargetScopeType::Specific;
        scope.target = j["entity_id"].get<std::string>();
    } else if (j.contains("category")) {
        scope.type = TargetScopeType::Category;
        scope.target = j["category"].get<std::string>();
    } else if (j.contains("faction")) {
        scope.type = TargetScopeType::Faction;
        scope.target = j["faction"].get<std::string>();
    } else if (j.contains("target")) {
        scope.target = j["target"].get<std::string>();
    }

    if (j.contains("tags")) {
        scope.tags = j["tags"].get<std::vector<std::string>>();
    }

    if (j.contains("exclude_heroes")) {
        scope.excludeHeroes = j["exclude_heroes"].get<bool>();
    }

    return scope;
}

// ============================================================================
// ModifierCondition
// ============================================================================

nlohmann::json ModifierCondition::ToJson() const {
    if (type == ConditionType::Always && !invert) {
        return nullptr;
    }

    nlohmann::json j;
    j["type"] = ConditionTypeToString(type);

    if (threshold != 0.0f) {
        j["threshold"] = threshold;
    }

    if (!target.empty()) {
        j["target"] = target;
    }

    if (radius != 10.0f) {
        j["radius"] = radius;
    }

    if (!scriptPath.empty()) {
        j["script"] = scriptPath;
    }

    if (!buffDebuffId.empty()) {
        j["buff_debuff_id"] = buffDebuffId;
    }

    if (invert) {
        j["invert"] = true;
    }

    return j;
}

ModifierCondition ModifierCondition::FromJson(const nlohmann::json& j) {
    ModifierCondition condition;

    if (j.is_null()) {
        return condition;
    }

    if (j.is_string()) {
        condition.type = StringToConditionType(j.get<std::string>());
        return condition;
    }

    if (j.contains("type")) {
        condition.type = StringToConditionType(j["type"].get<std::string>());
    }

    if (j.contains("threshold")) {
        condition.threshold = j["threshold"].get<float>();
    }

    if (j.contains("target")) {
        condition.target = j["target"].get<std::string>();
    }

    if (j.contains("radius")) {
        condition.radius = j["radius"].get<float>();
    }

    if (j.contains("script")) {
        condition.scriptPath = j["script"].get<std::string>();
    }

    if (j.contains("buff_debuff_id")) {
        condition.buffDebuffId = j["buff_debuff_id"].get<std::string>();
    }

    if (j.contains("invert")) {
        condition.invert = j["invert"].get<bool>();
    }

    return condition;
}

// ============================================================================
// TechModifier
// ============================================================================

bool TechModifier::AppliesToEntity(const std::string& entityType,
                                    const std::vector<std::string>& entityTags,
                                    const std::string& entityId) const {
    switch (scope.type) {
        case TargetScopeType::Global:
            break;

        case TargetScopeType::UnitType:
            if (entityType != scope.target) {
                return false;
            }
            break;

        case TargetScopeType::BuildingType:
            if (entityType != scope.target) {
                return false;
            }
            break;

        case TargetScopeType::Specific:
            if (entityId != scope.target) {
                return false;
            }
            break;

        case TargetScopeType::Category: {
            bool found = false;
            for (const auto& tag : entityTags) {
                if (tag == scope.target) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
            break;
        }

        case TargetScopeType::Faction:
            // Faction check would need additional info
            break;

        case TargetScopeType::Self:
            // Self only applies to source entity
            break;

        default:
            break;
    }

    // Check additional tag requirements
    for (const auto& requiredTag : scope.tags) {
        bool found = false;
        for (const auto& tag : entityTags) {
            if (tag == requiredTag) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    // Check hero exclusion
    if (scope.excludeHeroes) {
        for (const auto& tag : entityTags) {
            if (tag == "hero" || tag == "Hero") {
                return false;
            }
        }
    }

    return true;
}

float TechModifier::Apply(float baseValue) const {
    switch (type) {
        case ModifierType::Flat:
            return baseValue + value;

        case ModifierType::Percent:
            return baseValue * (1.0f + value / 100.0f);

        case ModifierType::Multiplicative:
            return baseValue * value;

        case ModifierType::Override:
            return value;

        case ModifierType::Max:
            return std::min(baseValue, value);

        case ModifierType::Min:
            return std::max(baseValue, value);

        default:
            return baseValue;
    }
}

std::string TechModifier::GetDisplayString() const {
    std::ostringstream oss;

    switch (type) {
        case ModifierType::Flat:
            if (value >= 0) {
                oss << "+" << std::fixed << std::setprecision(0) << value;
            } else {
                oss << std::fixed << std::setprecision(0) << value;
            }
            break;

        case ModifierType::Percent:
            if (value >= 0) {
                oss << "+" << std::fixed << std::setprecision(0) << value << "%";
            } else {
                oss << std::fixed << std::setprecision(0) << value << "%";
            }
            break;

        case ModifierType::Multiplicative:
            oss << "x" << std::fixed << std::setprecision(2) << value;
            break;

        case ModifierType::Override:
            oss << "=" << std::fixed << std::setprecision(0) << value;
            break;

        case ModifierType::Max:
            oss << "max " << std::fixed << std::setprecision(0) << value;
            break;

        case ModifierType::Min:
            oss << "min " << std::fixed << std::setprecision(0) << value;
            break;

        default:
            oss << value;
            break;
    }

    oss << " " << stat;

    if (!scope.IsGlobal()) {
        oss << " (";
        switch (scope.type) {
            case TargetScopeType::UnitType:
                oss << scope.target << " units";
                break;
            case TargetScopeType::BuildingType:
                oss << scope.target << " buildings";
                break;
            case TargetScopeType::Category:
                oss << scope.target;
                break;
            default:
                oss << scope.target;
                break;
        }
        oss << ")";
    }

    if (!condition.IsAlways()) {
        oss << " [" << ConditionTypeToString(condition.type) << "]";
    }

    return oss.str();
}

nlohmann::json TechModifier::ToJson() const {
    nlohmann::json j;

    j["stat"] = stat;
    j["type"] = ModifierTypeToString(type);
    j["value"] = value;

    auto scopeJson = scope.ToJson();
    if (!scopeJson.is_null() && scopeJson != "global") {
        j["scope"] = scopeJson;
    }

    auto conditionJson = condition.ToJson();
    if (!conditionJson.is_null()) {
        j["condition"] = conditionJson;
    }

    if (stacking != StackingRule::Additive) {
        j["stacking"] = StackingRuleToString(stacking);
    }

    if (stackCap > 0.0f) {
        j["stack_cap"] = stackCap;
    }

    if (maxStacks > 0) {
        j["max_stacks"] = maxStacks;
    }

    if (priority != 100) {
        j["priority"] = priority;
    }

    if (duration > 0.0f) {
        j["duration"] = duration;
    }

    if (!description.empty()) {
        j["description"] = description;
    }

    if (!tags.empty()) {
        j["tags"] = tags;
    }

    return j;
}

TechModifier TechModifier::FromJson(const nlohmann::json& j) {
    TechModifier modifier;

    if (j.contains("id")) {
        modifier.id = j["id"].get<std::string>();
    }

    if (j.contains("source_id")) {
        modifier.sourceId = j["source_id"].get<std::string>();
    }

    if (j.contains("stat")) {
        modifier.stat = j["stat"].get<std::string>();
    }

    if (j.contains("type")) {
        modifier.type = StringToModifierType(j["type"].get<std::string>());
    }

    if (j.contains("value")) {
        modifier.value = j["value"].get<float>();
    }

    if (j.contains("scope")) {
        modifier.scope = TargetScope::FromJson(j["scope"]);
    }

    if (j.contains("condition")) {
        modifier.condition = ModifierCondition::FromJson(j["condition"]);
    }

    if (j.contains("stacking")) {
        modifier.stacking = StringToStackingRule(j["stacking"].get<std::string>());
    }

    if (j.contains("stack_cap")) {
        modifier.stackCap = j["stack_cap"].get<float>();
    }

    if (j.contains("max_stacks")) {
        modifier.maxStacks = j["max_stacks"].get<int>();
    }

    if (j.contains("priority")) {
        modifier.priority = j["priority"].get<int>();
    }

    if (j.contains("duration")) {
        modifier.duration = j["duration"].get<float>();
        modifier.isPermanent = (modifier.duration <= 0.0f);
    }

    if (j.contains("description")) {
        modifier.description = j["description"].get<std::string>();
    }

    if (j.contains("tags")) {
        modifier.tags = j["tags"].get<std::vector<std::string>>();
    }

    return modifier;
}

TechModifier TechModifier::FlatBonus(const std::string& stat, float value, TargetScope scope) {
    TechModifier mod;
    mod.stat = stat;
    mod.type = ModifierType::Flat;
    mod.value = value;
    mod.scope = std::move(scope);
    return mod;
}

TechModifier TechModifier::PercentBonus(const std::string& stat, float percent, TargetScope scope) {
    TechModifier mod;
    mod.stat = stat;
    mod.type = ModifierType::Percent;
    mod.value = percent;
    mod.scope = std::move(scope);
    return mod;
}

TechModifier TechModifier::Multiplier(const std::string& stat, float multiplier, TargetScope scope) {
    TechModifier mod;
    mod.stat = stat;
    mod.type = ModifierType::Multiplicative;
    mod.value = multiplier;
    mod.scope = std::move(scope);
    return mod;
}

// ============================================================================
// ModifierStack
// ============================================================================

void ModifierStack::AddModifier(const TechModifier& modifier) {
    m_modifiers.push_back(modifier);
    m_dirty = true;
}

bool ModifierStack::RemoveModifier(const std::string& modifierId) {
    auto it = std::find_if(m_modifiers.begin(), m_modifiers.end(),
        [&modifierId](const TechModifier& mod) {
            return mod.id == modifierId;
        });

    if (it != m_modifiers.end()) {
        m_modifiers.erase(it);
        m_dirty = true;
        return true;
    }
    return false;
}

void ModifierStack::RemoveModifiersFromSource(const std::string& sourceId) {
    auto newEnd = std::remove_if(m_modifiers.begin(), m_modifiers.end(),
        [&sourceId](const TechModifier& mod) {
            return mod.sourceId == sourceId;
        });

    if (newEnd != m_modifiers.end()) {
        m_modifiers.erase(newEnd, m_modifiers.end());
        m_dirty = true;
    }
}

void ModifierStack::Clear() {
    m_modifiers.clear();
    m_sortedModifiers.clear();
    m_dirty = true;
}

void ModifierStack::SortModifiers() const {
    if (!m_dirty) {
        return;
    }

    m_sortedModifiers.clear();
    m_sortedModifiers.reserve(m_modifiers.size());

    for (const auto& mod : m_modifiers) {
        m_sortedModifiers.push_back(&mod);
    }

    // Sort by priority, then by type
    std::sort(m_sortedModifiers.begin(), m_sortedModifiers.end(),
        [](const TechModifier* a, const TechModifier* b) {
            if (a->priority != b->priority) {
                return a->priority < b->priority;
            }
            // Flat bonuses first, then percent, then multiplicative
            return static_cast<int>(a->type) < static_cast<int>(b->type);
        });

    m_dirty = false;
}

float ModifierStack::Calculate(float baseValue,
                                const std::string& entityType,
                                const std::vector<std::string>& entityTags,
                                const std::string& entityId) const {
    SortModifiers();

    float result = baseValue;
    float flatSum = 0.0f;
    float percentSum = 0.0f;
    float multiplierProduct = 1.0f;

    // Group modifiers by stacking type
    std::unordered_map<StackingRule, std::vector<float>> stackedValues;

    for (const auto* mod : m_sortedModifiers) {
        // Check if modifier applies to this entity
        if (!mod->AppliesToEntity(entityType, entityTags, entityId)) {
            continue;
        }

        // TODO: Evaluate conditions with a condition evaluator

        switch (mod->type) {
            case ModifierType::Flat:
                flatSum += mod->value;
                break;

            case ModifierType::Percent:
                percentSum += mod->value;
                break;

            case ModifierType::Multiplicative:
                multiplierProduct *= mod->value;
                break;

            case ModifierType::Override:
                result = mod->value;
                break;

            case ModifierType::Max:
                result = std::min(result, mod->value);
                break;

            case ModifierType::Min:
                result = std::max(result, mod->value);
                break;

            default:
                break;
        }
    }

    // Apply in order: flat bonuses, then percent, then multipliers
    result = baseValue + flatSum;
    result = result * (1.0f + percentSum / 100.0f);
    result = result * multiplierProduct;

    return result;
}

std::string ModifierStack::GetCalculationBreakdown(float baseValue) const {
    SortModifiers();

    std::ostringstream oss;
    oss << "Base: " << baseValue << "\n";

    for (const auto* mod : m_sortedModifiers) {
        oss << "  " << mod->GetDisplayString() << "\n";
    }

    oss << "Final: " << Calculate(baseValue) << "\n";

    return oss.str();
}

// ============================================================================
// ModifierCollection
// ============================================================================

void ModifierCollection::AddModifier(const TechModifier& modifier) {
    m_stacks[modifier.stat].AddModifier(modifier);
}

bool ModifierCollection::RemoveModifier(const std::string& modifierId) {
    for (auto& [stat, stack] : m_stacks) {
        if (stack.RemoveModifier(modifierId)) {
            return true;
        }
    }
    return false;
}

void ModifierCollection::RemoveModifiersFromSource(const std::string& sourceId) {
    for (auto& [stat, stack] : m_stacks) {
        stack.RemoveModifiersFromSource(sourceId);
    }
}

void ModifierCollection::Clear() {
    m_stacks.clear();
}

float ModifierCollection::GetModifiedValue(const std::string& stat, float baseValue,
                                            const std::string& entityType,
                                            const std::vector<std::string>& entityTags,
                                            const std::string& entityId) const {
    auto it = m_stacks.find(stat);
    if (it == m_stacks.end()) {
        return baseValue;
    }
    return it->second.Calculate(baseValue, entityType, entityTags, entityId);
}

float ModifierCollection::GetFlatBonus(const std::string& stat) const {
    auto it = m_stacks.find(stat);
    if (it == m_stacks.end()) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (const auto& mod : it->second.GetModifiers()) {
        if (mod.type == ModifierType::Flat) {
            sum += mod.value;
        }
    }
    return sum;
}

float ModifierCollection::GetPercentBonus(const std::string& stat) const {
    auto it = m_stacks.find(stat);
    if (it == m_stacks.end()) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (const auto& mod : it->second.GetModifiers()) {
        if (mod.type == ModifierType::Percent) {
            sum += mod.value;
        }
    }
    return sum;
}

float ModifierCollection::GetMultiplier(const std::string& stat) const {
    auto it = m_stacks.find(stat);
    if (it == m_stacks.end()) {
        return 1.0f;
    }

    float product = 1.0f;
    for (const auto& mod : it->second.GetModifiers()) {
        if (mod.type == ModifierType::Multiplicative) {
            product *= mod.value;
        }
    }
    return product;
}

bool ModifierCollection::HasModifiersForStat(const std::string& stat) const {
    auto it = m_stacks.find(stat);
    return it != m_stacks.end() && !it->second.IsEmpty();
}

std::vector<std::string> ModifierCollection::GetModifiedStats() const {
    std::vector<std::string> stats;
    stats.reserve(m_stacks.size());
    for (const auto& [stat, stack] : m_stacks) {
        if (!stack.IsEmpty()) {
            stats.push_back(stat);
        }
    }
    return stats;
}

std::vector<TechModifier> ModifierCollection::GetAllModifiers() const {
    std::vector<TechModifier> all;
    for (const auto& [stat, stack] : m_stacks) {
        const auto& mods = stack.GetModifiers();
        all.insert(all.end(), mods.begin(), mods.end());
    }
    return all;
}

const ModifierStack* ModifierCollection::GetStackForStat(const std::string& stat) const {
    auto it = m_stacks.find(stat);
    return it != m_stacks.end() ? &it->second : nullptr;
}

nlohmann::json ModifierCollection::ToJson() const {
    nlohmann::json j = nlohmann::json::array();

    for (const auto& [stat, stack] : m_stacks) {
        for (const auto& mod : stack.GetModifiers()) {
            j.push_back(mod.ToJson());
        }
    }

    return j;
}

void ModifierCollection::FromJson(const nlohmann::json& j) {
    Clear();

    if (!j.is_array()) {
        return;
    }

    for (const auto& modJson : j) {
        AddModifier(TechModifier::FromJson(modJson));
    }
}

// ============================================================================
// DefaultConditionEvaluator
// ============================================================================

bool DefaultConditionEvaluator::Evaluate(const ModifierCondition& condition,
                                          const std::string& entityId) const {
    bool result = false;

    switch (condition.type) {
        case ConditionType::Always:
            result = true;
            break;

        case ConditionType::WhenInCombat:
            result = IsInCombat(entityId);
            break;

        case ConditionType::WhenNotInCombat:
            result = !IsInCombat(entityId);
            break;

        case ConditionType::WhenNearBuilding:
            result = IsNearBuilding(entityId, condition.target, condition.radius);
            break;

        case ConditionType::WhenNearUnit:
            result = IsNearUnit(entityId, condition.target, condition.radius);
            break;

        case ConditionType::WhenHealthBelow:
            result = GetHealthPercent(entityId) < condition.threshold;
            break;

        case ConditionType::WhenHealthAbove:
            result = GetHealthPercent(entityId) > condition.threshold;
            break;

        case ConditionType::WhenDay:
            result = IsDaytime();
            break;

        case ConditionType::WhenNight:
            result = !IsDaytime();
            break;

        case ConditionType::WhenInTerritory:
            result = IsInOwnTerritory(entityId);
            break;

        case ConditionType::WhenInEnemyTerritory:
            result = !IsInOwnTerritory(entityId);
            break;

        case ConditionType::WhenGarrisoned:
            result = IsGarrisoned(entityId);
            break;

        case ConditionType::WhenMoving:
            result = IsMoving(entityId);
            break;

        case ConditionType::WhenStationary:
            result = !IsMoving(entityId);
            break;

        case ConditionType::WhenBuffed:
            result = HasBuff(entityId, condition.buffDebuffId);
            break;

        case ConditionType::WhenDebuffed:
            result = HasDebuff(entityId, condition.buffDebuffId);
            break;

        case ConditionType::Custom:
            result = EvaluateCustomCondition(condition.scriptPath, entityId);
            break;

        default:
            result = true;
            break;
    }

    return condition.invert ? !result : result;
}

} // namespace TechTree
} // namespace Vehement
