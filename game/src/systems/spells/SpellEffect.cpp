#include "SpellEffect.hpp"
#include <algorithm>
#include <sstream>
#include <random>
#include <cmath>

namespace Vehement {
namespace Spells {

// ============================================================================
// String Conversion Functions
// ============================================================================

const char* EffectTypeToString(EffectType type) {
    switch (type) {
        case EffectType::Damage: return "damage";
        case EffectType::Heal: return "heal";
        case EffectType::DamageOverTime: return "dot";
        case EffectType::HealOverTime: return "hot";
        case EffectType::AbsorbDamage: return "absorb";
        case EffectType::Stun: return "stun";
        case EffectType::Root: return "root";
        case EffectType::Silence: return "silence";
        case EffectType::Disarm: return "disarm";
        case EffectType::Slow: return "slow";
        case EffectType::Fear: return "fear";
        case EffectType::Charm: return "charm";
        case EffectType::Sleep: return "sleep";
        case EffectType::Knockback: return "knockback";
        case EffectType::Pull: return "pull";
        case EffectType::Grip: return "grip";
        case EffectType::Buff: return "buff";
        case EffectType::Debuff: return "debuff";
        case EffectType::StatModifier: return "stat_modifier";
        case EffectType::Aura: return "aura";
        case EffectType::Teleport: return "teleport";
        case EffectType::Dash: return "dash";
        case EffectType::Leap: return "leap";
        case EffectType::Summon: return "summon";
        case EffectType::Transform: return "transform";
        case EffectType::Dispel: return "dispel";
        case EffectType::Interrupt: return "interrupt";
        case EffectType::Resurrect: return "resurrect";
        case EffectType::ResourceRestore: return "resource_restore";
        case EffectType::ResourceDrain: return "resource_drain";
        case EffectType::Script: return "script";
        case EffectType::Trigger: return "trigger";
        case EffectType::Chain: return "chain";
        default: return "unknown";
    }
}

EffectType StringToEffectType(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "damage") return EffectType::Damage;
    if (lower == "heal") return EffectType::Heal;
    if (lower == "dot" || lower == "damage_over_time") return EffectType::DamageOverTime;
    if (lower == "hot" || lower == "heal_over_time") return EffectType::HealOverTime;
    if (lower == "absorb") return EffectType::AbsorbDamage;
    if (lower == "stun") return EffectType::Stun;
    if (lower == "root") return EffectType::Root;
    if (lower == "silence") return EffectType::Silence;
    if (lower == "disarm") return EffectType::Disarm;
    if (lower == "slow") return EffectType::Slow;
    if (lower == "fear") return EffectType::Fear;
    if (lower == "charm") return EffectType::Charm;
    if (lower == "sleep") return EffectType::Sleep;
    if (lower == "knockback") return EffectType::Knockback;
    if (lower == "pull") return EffectType::Pull;
    if (lower == "grip") return EffectType::Grip;
    if (lower == "buff") return EffectType::Buff;
    if (lower == "debuff") return EffectType::Debuff;
    if (lower == "stat_modifier" || lower == "stat") return EffectType::StatModifier;
    if (lower == "aura") return EffectType::Aura;
    if (lower == "teleport") return EffectType::Teleport;
    if (lower == "dash") return EffectType::Dash;
    if (lower == "leap") return EffectType::Leap;
    if (lower == "summon") return EffectType::Summon;
    if (lower == "transform") return EffectType::Transform;
    if (lower == "dispel") return EffectType::Dispel;
    if (lower == "interrupt") return EffectType::Interrupt;
    if (lower == "resurrect") return EffectType::Resurrect;
    if (lower == "resource_restore") return EffectType::ResourceRestore;
    if (lower == "resource_drain") return EffectType::ResourceDrain;
    if (lower == "script") return EffectType::Script;
    if (lower == "trigger") return EffectType::Trigger;
    if (lower == "chain") return EffectType::Chain;

    return EffectType::Damage;
}

const char* EffectTimingToString(EffectTiming timing) {
    switch (timing) {
        case EffectTiming::Instant: return "instant";
        case EffectTiming::OverTime: return "over_time";
        case EffectTiming::Delayed: return "delayed";
        case EffectTiming::OnInterval: return "on_interval";
        case EffectTiming::OnExpire: return "on_expire";
        case EffectTiming::OnRemove: return "on_remove";
        case EffectTiming::OnStack: return "on_stack";
        case EffectTiming::Channeled: return "channeled";
        default: return "instant";
    }
}

EffectTiming StringToEffectTiming(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "instant") return EffectTiming::Instant;
    if (lower == "over_time" || lower == "overtime") return EffectTiming::OverTime;
    if (lower == "delayed") return EffectTiming::Delayed;
    if (lower == "on_interval" || lower == "interval") return EffectTiming::OnInterval;
    if (lower == "on_expire" || lower == "expire") return EffectTiming::OnExpire;
    if (lower == "on_remove" || lower == "remove") return EffectTiming::OnRemove;
    if (lower == "on_stack" || lower == "stack") return EffectTiming::OnStack;
    if (lower == "channeled") return EffectTiming::Channeled;

    return EffectTiming::Instant;
}

const char* StackingRuleToString(StackingRule rule) {
    switch (rule) {
        case StackingRule::Refresh: return "refresh";
        case StackingRule::Stack: return "stack";
        case StackingRule::Replace: return "replace";
        case StackingRule::Ignore: return "ignore";
        case StackingRule::Pandemic: return "pandemic";
        case StackingRule::Highest: return "highest";
        case StackingRule::Lowest: return "lowest";
        case StackingRule::Separate: return "separate";
        default: return "refresh";
    }
}

StackingRule StringToStackingRule(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "refresh") return StackingRule::Refresh;
    if (lower == "stack") return StackingRule::Stack;
    if (lower == "replace") return StackingRule::Replace;
    if (lower == "ignore") return StackingRule::Ignore;
    if (lower == "pandemic") return StackingRule::Pandemic;
    if (lower == "highest") return StackingRule::Highest;
    if (lower == "lowest") return StackingRule::Lowest;
    if (lower == "separate") return StackingRule::Separate;

    return StackingRule::Refresh;
}

const char* DamageTypeToString(DamageType type) {
    switch (type) {
        case DamageType::Physical: return "physical";
        case DamageType::Fire: return "fire";
        case DamageType::Frost: return "frost";
        case DamageType::Nature: return "nature";
        case DamageType::Arcane: return "arcane";
        case DamageType::Shadow: return "shadow";
        case DamageType::Holy: return "holy";
        case DamageType::Lightning: return "lightning";
        case DamageType::Poison: return "poison";
        case DamageType::Bleed: return "bleed";
        case DamageType::Pure: return "pure";
        default: return "physical";
    }
}

DamageType StringToDamageType(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "physical") return DamageType::Physical;
    if (lower == "fire") return DamageType::Fire;
    if (lower == "frost" || lower == "ice" || lower == "cold") return DamageType::Frost;
    if (lower == "nature") return DamageType::Nature;
    if (lower == "arcane") return DamageType::Arcane;
    if (lower == "shadow") return DamageType::Shadow;
    if (lower == "holy" || lower == "light") return DamageType::Holy;
    if (lower == "lightning") return DamageType::Lightning;
    if (lower == "poison") return DamageType::Poison;
    if (lower == "bleed") return DamageType::Bleed;
    if (lower == "pure" || lower == "true") return DamageType::Pure;

    return DamageType::Physical;
}

const char* TriggerConditionToString(TriggerCondition condition) {
    switch (condition) {
        case TriggerCondition::None: return "none";
        case TriggerCondition::OnCrit: return "on_crit";
        case TriggerCondition::OnKill: return "on_kill";
        case TriggerCondition::OnLowHealth: return "on_low_health";
        case TriggerCondition::OnHighHealth: return "on_high_health";
        case TriggerCondition::OnMiss: return "on_miss";
        case TriggerCondition::OnResist: return "on_resist";
        case TriggerCondition::OnDispel: return "on_dispel";
        case TriggerCondition::OnExpire: return "on_expire";
        case TriggerCondition::OnTargetCast: return "on_target_cast";
        case TriggerCondition::OnTargetMove: return "on_target_move";
        case TriggerCondition::OnTargetAttack: return "on_target_attack";
        case TriggerCondition::OnDamageTaken: return "on_damage_taken";
        case TriggerCondition::OnHealReceived: return "on_heal_received";
        case TriggerCondition::OnResourceSpent: return "on_resource_spent";
        case TriggerCondition::OnCombo: return "on_combo";
        case TriggerCondition::CustomScript: return "custom";
        default: return "none";
    }
}

TriggerCondition StringToTriggerCondition(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "on_crit") return TriggerCondition::OnCrit;
    if (lower == "on_kill") return TriggerCondition::OnKill;
    if (lower == "on_low_health") return TriggerCondition::OnLowHealth;
    if (lower == "on_high_health") return TriggerCondition::OnHighHealth;
    if (lower == "on_miss") return TriggerCondition::OnMiss;
    if (lower == "on_resist") return TriggerCondition::OnResist;
    if (lower == "on_dispel") return TriggerCondition::OnDispel;
    if (lower == "on_expire") return TriggerCondition::OnExpire;
    if (lower == "on_target_cast") return TriggerCondition::OnTargetCast;
    if (lower == "on_target_move") return TriggerCondition::OnTargetMove;
    if (lower == "on_target_attack") return TriggerCondition::OnTargetAttack;
    if (lower == "on_damage_taken") return TriggerCondition::OnDamageTaken;
    if (lower == "on_heal_received") return TriggerCondition::OnHealReceived;
    if (lower == "on_resource_spent") return TriggerCondition::OnResourceSpent;
    if (lower == "on_combo") return TriggerCondition::OnCombo;
    if (lower == "custom") return TriggerCondition::CustomScript;

    return TriggerCondition::None;
}

// ============================================================================
// JSON Parsing Helpers
// ============================================================================

namespace {

std::string ExtractString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return "";

    size_t valueStart = json.find('"', colonPos);
    if (valueStart == std::string::npos) return "";

    size_t valueEnd = json.find('"', valueStart + 1);
    if (valueEnd == std::string::npos) return "";

    return json.substr(valueStart + 1, valueEnd - valueStart - 1);
}

float ExtractFloat(const std::string& json, const std::string& key, float defaultVal = 0.0f) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return defaultVal;

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return defaultVal;

    size_t valueStart = colonPos + 1;
    while (valueStart < json.size() && std::isspace(json[valueStart])) valueStart++;

    size_t valueEnd = valueStart;
    while (valueEnd < json.size() && (std::isdigit(json[valueEnd]) ||
           json[valueEnd] == '.' || json[valueEnd] == '-')) valueEnd++;

    if (valueStart == valueEnd) return defaultVal;

    try {
        return std::stof(json.substr(valueStart, valueEnd - valueStart));
    } catch (...) {
        return defaultVal;
    }
}

int ExtractInt(const std::string& json, const std::string& key, int defaultVal = 0) {
    return static_cast<int>(ExtractFloat(json, key, static_cast<float>(defaultVal)));
}

bool ExtractBool(const std::string& json, const std::string& key, bool defaultVal = false) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return defaultVal;

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return defaultVal;

    size_t valueStart = colonPos + 1;
    while (valueStart < json.size() && std::isspace(json[valueStart])) valueStart++;

    if (json.compare(valueStart, 4, "true") == 0) return true;
    if (json.compare(valueStart, 5, "false") == 0) return false;

    return defaultVal;
}

std::string ExtractObject(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t braceStart = json.find('{', keyPos);
    if (braceStart == std::string::npos) return "";

    int braceCount = 1;
    size_t braceEnd = braceStart + 1;
    while (braceEnd < json.size() && braceCount > 0) {
        if (json[braceEnd] == '{') braceCount++;
        else if (json[braceEnd] == '}') braceCount--;
        braceEnd++;
    }

    return json.substr(braceStart, braceEnd - braceStart);
}

} // anonymous namespace

// ============================================================================
// EffectScaling Implementation
// ============================================================================

float EffectScaling::Calculate(float base, float statValue, int casterLevel, int targetLevel) const {
    float scaled = base + (statValue * coefficient);
    scaled += levelBonus * casterLevel;

    int levelDiff = targetLevel - casterLevel;
    if (levelDiff > 0) {
        scaled -= targetLevelPenalty * levelDiff;
    }

    return std::clamp(scaled, minValue, maxValue);
}

// ============================================================================
// SpellEffect Implementation
// ============================================================================

bool SpellEffect::LoadFromJson(const std::string& jsonString) {
    m_id = ExtractString(jsonString, "id");
    m_type = StringToEffectType(ExtractString(jsonString, "type"));
    m_timing = StringToEffectTiming(ExtractString(jsonString, "timing"));
    m_stackingRule = StringToStackingRule(ExtractString(jsonString, "stacking"));
    m_damageType = StringToDamageType(ExtractString(jsonString, "damage_type"));

    m_baseAmount = ExtractFloat(jsonString, "amount", 0.0f);
    m_duration = ExtractFloat(jsonString, "duration", 0.0f);
    m_tickInterval = ExtractFloat(jsonString, "tick_interval", 1.0f);
    m_delay = ExtractFloat(jsonString, "delay", 0.0f);
    m_maxStacks = ExtractInt(jsonString, "max_stacks", 1);
    m_stackValue = ExtractFloat(jsonString, "stack_value", 0.0f);

    m_critChance = ExtractFloat(jsonString, "crit_chance", 0.0f);
    m_critMultiplier = ExtractFloat(jsonString, "crit_multiplier", 2.0f);

    m_customScript = ExtractString(jsonString, "script");
    m_description = ExtractString(jsonString, "description");
    m_iconOverride = ExtractString(jsonString, "icon");

    // Parse scaling
    std::string scalingJson = ExtractObject(jsonString, "scaling");
    if (!scalingJson.empty()) {
        m_scaling.stat = ExtractString(scalingJson, "stat");
        m_scaling.coefficient = ExtractFloat(scalingJson, "coefficient", 0.0f);
        m_scaling.levelBonus = ExtractFloat(scalingJson, "level_bonus", 0.0f);
        m_scaling.targetLevelPenalty = ExtractFloat(scalingJson, "target_level_penalty", 0.0f);
        m_scaling.minValue = ExtractFloat(scalingJson, "min", 0.0f);
        m_scaling.maxValue = ExtractFloat(scalingJson, "max", std::numeric_limits<float>::max());
    }

    // Parse type-specific configs based on effect type
    switch (m_type) {
        case EffectType::Summon: {
            std::string summonJson = ExtractObject(jsonString, "summon");
            if (!summonJson.empty()) {
                SummonConfig cfg;
                cfg.unitId = ExtractString(summonJson, "unit_id");
                cfg.count = ExtractInt(summonJson, "count", 1);
                cfg.duration = ExtractFloat(summonJson, "duration", 0.0f);
                cfg.inheritFaction = ExtractBool(summonJson, "inherit_faction", true);
                cfg.spawnRadius = ExtractFloat(summonJson, "spawn_radius", 2.0f);
                summonConfig = cfg;
            }
            break;
        }

        case EffectType::Teleport:
        case EffectType::Dash:
        case EffectType::Leap: {
            std::string moveJson = ExtractObject(jsonString, "movement");
            if (!moveJson.empty()) {
                MovementConfig cfg;
                cfg.distance = ExtractFloat(moveJson, "distance", 10.0f);
                cfg.speed = ExtractFloat(moveJson, "speed", 20.0f);
                cfg.towardTarget = ExtractBool(moveJson, "toward_target", true);
                cfg.throughWalls = ExtractBool(moveJson, "through_walls", false);
                cfg.arrivalEffect = ExtractString(moveJson, "arrival_effect");
                movementConfig = cfg;
            }
            break;
        }

        case EffectType::Knockback:
        case EffectType::Pull:
        case EffectType::Grip: {
            std::string dispJson = ExtractObject(jsonString, "displacement");
            if (!dispJson.empty()) {
                DisplacementConfig cfg;
                cfg.distance = ExtractFloat(dispJson, "distance", 5.0f);
                cfg.speed = ExtractFloat(dispJson, "speed", 15.0f);
                cfg.scalesWithDistance = ExtractBool(dispJson, "scales_with_distance", false);
                cfg.knocksUp = ExtractBool(dispJson, "knocks_up", false);
                cfg.knockUpHeight = ExtractFloat(dispJson, "knock_up_height", 2.0f);
                displacementConfig = cfg;
            }
            break;
        }

        case EffectType::Dispel: {
            std::string dispelJson = ExtractObject(jsonString, "dispel");
            if (!dispelJson.empty()) {
                DispelConfig cfg;
                cfg.dispelBuffs = ExtractBool(dispelJson, "buffs", false);
                cfg.dispelDebuffs = ExtractBool(dispelJson, "debuffs", true);
                cfg.maxDispelled = ExtractInt(dispelJson, "max", 1);
                cfg.dispelMagic = ExtractBool(dispelJson, "magic", true);
                cfg.dispelCurse = ExtractBool(dispelJson, "curse", false);
                cfg.dispelPoison = ExtractBool(dispelJson, "poison", false);
                cfg.dispelDisease = ExtractBool(dispelJson, "disease", false);
                dispelConfig = cfg;
            }
            break;
        }

        case EffectType::ResourceRestore:
        case EffectType::ResourceDrain: {
            std::string resJson = ExtractObject(jsonString, "resource");
            if (!resJson.empty()) {
                ResourceConfig cfg;
                cfg.resourceType = ExtractString(resJson, "type");
                cfg.amount = ExtractFloat(resJson, "amount", 0.0f);
                cfg.percentage = ExtractBool(resJson, "percentage", false);
                cfg.drain = (m_type == EffectType::ResourceDrain);
                cfg.drainEfficiency = ExtractFloat(resJson, "efficiency", 1.0f);
                resourceConfig = cfg;
            }
            break;
        }

        default:
            break;
    }

    return true;
}

std::string SpellEffect::ToJsonString() const {
    std::ostringstream json;
    json << "{\n";

    if (!m_id.empty()) {
        json << "  \"id\": \"" << m_id << "\",\n";
    }

    json << "  \"type\": \"" << EffectTypeToString(m_type) << "\",\n";
    json << "  \"timing\": \"" << EffectTimingToString(m_timing) << "\",\n";
    json << "  \"amount\": " << m_baseAmount << ",\n";

    if (m_duration > 0) {
        json << "  \"duration\": " << m_duration << ",\n";
        json << "  \"tick_interval\": " << m_tickInterval << ",\n";
    }

    if (m_maxStacks > 1) {
        json << "  \"max_stacks\": " << m_maxStacks << ",\n";
        json << "  \"stacking\": \"" << StackingRuleToString(m_stackingRule) << "\",\n";
    }

    if (!m_scaling.stat.empty()) {
        json << "  \"scaling\": {\n";
        json << "    \"stat\": \"" << m_scaling.stat << "\",\n";
        json << "    \"coefficient\": " << m_scaling.coefficient << "\n";
        json << "  },\n";
    }

    json << "  \"damage_type\": \"" << DamageTypeToString(m_damageType) << "\"\n";
    json << "}";

    return json.str();
}

bool SpellEffect::Validate(std::vector<std::string>& errors) const {
    bool valid = true;

    if (m_baseAmount < 0 && m_type != EffectType::Heal && m_type != EffectType::HealOverTime) {
        // Negative damage is allowed for healing effects
    }

    if (m_duration < 0) {
        errors.push_back("Effect duration cannot be negative");
        valid = false;
    }

    if (m_tickInterval <= 0 && (m_type == EffectType::DamageOverTime || m_type == EffectType::HealOverTime)) {
        errors.push_back("Tick interval must be positive for over-time effects");
        valid = false;
    }

    if (m_maxStacks < 1) {
        errors.push_back("Max stacks must be at least 1");
        valid = false;
    }

    if (m_critChance < 0 || m_critChance > 1) {
        errors.push_back("Crit chance must be between 0 and 1");
        valid = false;
    }

    return valid;
}

SpellEffect::ApplicationResult SpellEffect::Apply(
    const SpellInstance& instance,
    uint32_t casterId,
    uint32_t targetId,
    StatQueryFunc casterStats,
    StatQueryFunc targetStats
) const {
    ApplicationResult result;
    result.success = true;

    // Calculate final amount
    float amount = CalculateAmount(casterId, targetId, casterStats, targetStats);

    // Check for critical hit
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> critRoll(0.0f, 1.0f);

    float totalCritChance = m_critChance;
    if (casterStats) {
        totalCritChance += casterStats(casterId, "crit_chance");
    }

    if (critRoll(gen) < totalCritChance) {
        result.wasCrit = true;
        amount *= m_critMultiplier;
    }

    result.amount = amount;

    return result;
}

float SpellEffect::CalculateAmount(
    uint32_t casterId,
    uint32_t targetId,
    StatQueryFunc casterStats,
    StatQueryFunc targetStats
) const {
    float base = m_baseAmount;

    // Add stack scaling
    base += m_stackValue * (m_currentStacks > 0 ? m_currentStacks - 1 : 0);

    // Apply stat scaling
    if (!m_scaling.stat.empty() && casterStats) {
        float statValue = casterStats(casterId, m_scaling.stat);
        int casterLevel = static_cast<int>(casterStats(casterId, "level"));
        int targetLevel = targetStats ? static_cast<int>(targetStats(targetId, "level")) : casterLevel;

        base = m_scaling.Calculate(base, statValue, casterLevel, targetLevel);
    }

    return base;
}

bool SpellEffect::CheckTriggerCondition(
    TriggerCondition condition,
    const ApplicationResult& result,
    uint32_t casterId,
    uint32_t targetId,
    StatQueryFunc statQuery
) const {
    switch (condition) {
        case TriggerCondition::None:
            return true;

        case TriggerCondition::OnCrit:
            return result.wasCrit;

        case TriggerCondition::OnKill:
            // Would need to check if target died - handled externally
            return false;

        case TriggerCondition::OnLowHealth:
            if (statQuery) {
                float health = statQuery(targetId, "health");
                float maxHealth = statQuery(targetId, "max_health");
                return (health / maxHealth) < 0.35f;
            }
            return false;

        case TriggerCondition::OnHighHealth:
            if (statQuery) {
                float health = statQuery(targetId, "health");
                float maxHealth = statQuery(targetId, "max_health");
                return (health / maxHealth) > 0.90f;
            }
            return false;

        case TriggerCondition::OnMiss:
            return !result.success && !result.wasResisted;

        case TriggerCondition::OnResist:
            return result.wasResisted;

        default:
            return false;
    }
}

void SpellEffect::UpdatePeriodic(float deltaTime, TickCallback tickCallback) {
    if (m_tickInterval <= 0) return;

    m_periodicAccumulator += deltaTime;

    while (m_periodicAccumulator >= m_tickInterval) {
        m_periodicAccumulator -= m_tickInterval;

        // Calculate tick amount
        float tickAmount = m_baseAmount;
        tickAmount += m_stackValue * (m_currentStacks > 0 ? m_currentStacks - 1 : 0);

        // For DoT/HoT, amount is per tick
        tickCallback(tickAmount);
    }
}

std::pair<float, int> SpellEffect::HandleStacking(float existingDuration, int existingStacks) const {
    float newDuration = m_duration;
    int newStacks = existingStacks;

    switch (m_stackingRule) {
        case StackingRule::Refresh:
            // Reset duration, keep stacks
            newDuration = m_duration;
            break;

        case StackingRule::Stack:
            // Add stack, refresh duration
            newDuration = m_duration;
            newStacks = std::min(existingStacks + 1, m_maxStacks);
            break;

        case StackingRule::Replace:
            // Replace entirely
            newDuration = m_duration;
            newStacks = 1;
            break;

        case StackingRule::Ignore:
            // Don't change anything
            newDuration = existingDuration;
            break;

        case StackingRule::Pandemic:
            // Add remaining duration (up to 150% of base)
            {
                float maxDuration = m_duration * 1.5f;
                newDuration = std::min(m_duration + existingDuration, maxDuration);
            }
            break;

        case StackingRule::Highest:
        case StackingRule::Lowest:
            // Handled by comparing effect values externally
            newDuration = m_duration;
            break;

        case StackingRule::Separate:
            // Each application is independent - handled externally
            newDuration = m_duration;
            newStacks = 1;
            break;
    }

    return {newDuration, newStacks};
}

// ============================================================================
// ActiveEffect Implementation
// ============================================================================

ActiveEffect::ActiveEffect(const SpellEffect* effect, uint32_t casterId, uint32_t targetId)
    : m_effect(effect)
    , m_casterId(casterId)
    , m_targetId(targetId)
{
    if (effect) {
        m_remainingDuration = effect->GetDuration();
        m_totalDuration = effect->GetDuration();
    }
}

bool ActiveEffect::Update(float deltaTime) {
    if (m_remainingDuration <= 0.0f) {
        if (m_onExpire) {
            m_onExpire(*this);
        }
        return false;
    }

    m_remainingDuration -= deltaTime;

    // Handle periodic ticks
    if (m_effect && m_effect->GetTickInterval() > 0) {
        m_tickAccumulator += deltaTime;

        while (m_tickAccumulator >= m_effect->GetTickInterval()) {
            m_tickAccumulator -= m_effect->GetTickInterval();

            float tickAmount = m_effect->GetBaseAmount();
            tickAmount += m_effect->GetStackValue() * (m_stacks - 1);

            if (m_onTick) {
                m_onTick(*this, tickAmount);
            }
        }
    }

    return m_remainingDuration > 0.0f;
}

int ActiveEffect::AddStack() {
    if (m_effect) {
        m_stacks = std::min(m_stacks + 1, m_effect->GetMaxStacks());
    }
    return m_stacks;
}

int ActiveEffect::RemoveStack() {
    m_stacks = std::max(1, m_stacks - 1);
    return m_stacks;
}

void ActiveEffect::Refresh() {
    if (m_effect) {
        m_remainingDuration = m_effect->GetDuration();
    }
}

void ActiveEffect::ApplyPandemic(float newDuration, float maxPandemicBonus) {
    float bonus = std::min(m_remainingDuration, maxPandemicBonus);
    m_remainingDuration = newDuration + bonus;
    m_totalDuration = m_remainingDuration;
}

// ============================================================================
// SpellEffectFactory Implementation
// ============================================================================

SpellEffectFactory& SpellEffectFactory::Instance() {
    static SpellEffectFactory instance;
    return instance;
}

std::shared_ptr<SpellEffect> SpellEffectFactory::CreateFromJson(const std::string& jsonString) {
    auto effect = std::make_shared<SpellEffect>();
    if (effect->LoadFromJson(jsonString)) {
        return effect;
    }
    return nullptr;
}

std::shared_ptr<SpellEffect> SpellEffectFactory::Create(EffectType type) {
    auto effect = std::make_shared<SpellEffect>();
    effect->SetType(type);
    return effect;
}

} // namespace Spells
} // namespace Vehement
