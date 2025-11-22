#include "EffectTrigger.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <unordered_map>
#include <regex>

namespace Vehement {
namespace Effects {

// ============================================================================
// Type Conversion Tables
// ============================================================================

static const std::unordered_map<std::string, TriggerCondition> s_triggerCondFromString = {
    {"on_hit", TriggerCondition::OnHit},
    {"on_crit", TriggerCondition::OnCrit},
    {"on_kill", TriggerCondition::OnKill},
    {"on_assist", TriggerCondition::OnAssist},
    {"on_damage_taken", TriggerCondition::OnDamageTaken},
    {"on_crit_taken", TriggerCondition::OnCritTaken},
    {"on_heal", TriggerCondition::OnHeal},
    {"on_healed", TriggerCondition::OnHealed},
    {"on_block", TriggerCondition::OnBlock},
    {"on_dodge", TriggerCondition::OnDodge},
    {"on_parry", TriggerCondition::OnParry},
    {"on_ability_use", TriggerCondition::OnAbilityUse},
    {"on_ability_cast", TriggerCondition::OnAbilityCast},
    {"on_ability_complete", TriggerCondition::OnAbilityComplete},
    {"on_spell_cast", TriggerCondition::OnSpellCast},
    {"on_melee_attack", TriggerCondition::OnMeleeAttack},
    {"on_ranged_attack", TriggerCondition::OnRangedAttack},
    {"on_health_below", TriggerCondition::OnHealthBelow},
    {"on_health_above", TriggerCondition::OnHealthAbove},
    {"on_mana_below", TriggerCondition::OnManaBelow},
    {"on_low_health", TriggerCondition::OnLowHealth},
    {"on_full_health", TriggerCondition::OnFullHealth},
    {"on_move", TriggerCondition::OnMove},
    {"on_stand", TriggerCondition::OnStand},
    {"on_jump", TriggerCondition::OnJump},
    {"on_dash", TriggerCondition::OnDash},
    {"on_teleport", TriggerCondition::OnTeleport},
    {"on_enter_combat", TriggerCondition::OnEnterCombat},
    {"on_leave_combat", TriggerCondition::OnLeaveCombat},
    {"on_death", TriggerCondition::OnDeath},
    {"on_respawn", TriggerCondition::OnRespawn},
    {"on_level_up", TriggerCondition::OnLevelUp},
    {"on_equip_change", TriggerCondition::OnEquipChange},
    {"on_buff_applied", TriggerCondition::OnBuffApplied},
    {"on_debuff_applied", TriggerCondition::OnDebuffApplied},
    {"on_effect_removed", TriggerCondition::OnEffectRemoved},
    {"on_dispel", TriggerCondition::OnDispel},
    {"on_interval", TriggerCondition::OnInterval},
    {"on_zone_enter", TriggerCondition::OnZoneEnter},
    {"on_zone_exit", TriggerCondition::OnZoneExit},
    {"on_custom", TriggerCondition::OnCustomEvent}
};

const char* TriggerConditionToString(TriggerCondition cond) {
    switch (cond) {
        case TriggerCondition::OnHit:             return "on_hit";
        case TriggerCondition::OnCrit:            return "on_crit";
        case TriggerCondition::OnKill:            return "on_kill";
        case TriggerCondition::OnAssist:          return "on_assist";
        case TriggerCondition::OnDamageTaken:     return "on_damage_taken";
        case TriggerCondition::OnCritTaken:       return "on_crit_taken";
        case TriggerCondition::OnHeal:            return "on_heal";
        case TriggerCondition::OnHealed:          return "on_healed";
        case TriggerCondition::OnBlock:           return "on_block";
        case TriggerCondition::OnDodge:           return "on_dodge";
        case TriggerCondition::OnParry:           return "on_parry";
        case TriggerCondition::OnAbilityUse:      return "on_ability_use";
        case TriggerCondition::OnAbilityCast:     return "on_ability_cast";
        case TriggerCondition::OnAbilityComplete: return "on_ability_complete";
        case TriggerCondition::OnSpellCast:       return "on_spell_cast";
        case TriggerCondition::OnMeleeAttack:     return "on_melee_attack";
        case TriggerCondition::OnRangedAttack:    return "on_ranged_attack";
        case TriggerCondition::OnHealthBelow:     return "on_health_below";
        case TriggerCondition::OnHealthAbove:     return "on_health_above";
        case TriggerCondition::OnManaBelow:       return "on_mana_below";
        case TriggerCondition::OnLowHealth:       return "on_low_health";
        case TriggerCondition::OnFullHealth:      return "on_full_health";
        case TriggerCondition::OnMove:            return "on_move";
        case TriggerCondition::OnStand:           return "on_stand";
        case TriggerCondition::OnJump:            return "on_jump";
        case TriggerCondition::OnDash:            return "on_dash";
        case TriggerCondition::OnTeleport:        return "on_teleport";
        case TriggerCondition::OnEnterCombat:     return "on_enter_combat";
        case TriggerCondition::OnLeaveCombat:     return "on_leave_combat";
        case TriggerCondition::OnDeath:           return "on_death";
        case TriggerCondition::OnRespawn:         return "on_respawn";
        case TriggerCondition::OnLevelUp:         return "on_level_up";
        case TriggerCondition::OnEquipChange:     return "on_equip_change";
        case TriggerCondition::OnBuffApplied:     return "on_buff_applied";
        case TriggerCondition::OnDebuffApplied:   return "on_debuff_applied";
        case TriggerCondition::OnEffectRemoved:   return "on_effect_removed";
        case TriggerCondition::OnDispel:          return "on_dispel";
        case TriggerCondition::OnInterval:        return "on_interval";
        case TriggerCondition::OnZoneEnter:       return "on_zone_enter";
        case TriggerCondition::OnZoneExit:        return "on_zone_exit";
        case TriggerCondition::OnCustomEvent:     return "on_custom";
    }
    return "on_hit";
}

std::optional<TriggerCondition> TriggerConditionFromString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = s_triggerCondFromString.find(lower);
    if (it != s_triggerCondFromString.end()) {
        return it->second;
    }
    return std::nullopt;
}

static const std::unordered_map<std::string, TriggerAction> s_triggerActionFromString = {
    {"apply_effect", TriggerAction::ApplyEffect},
    {"remove_effect", TriggerAction::RemoveEffect},
    {"extend_duration", TriggerAction::ExtendDuration},
    {"reduce_duration", TriggerAction::ReduceDuration},
    {"add_stacks", TriggerAction::AddStacks},
    {"remove_stacks", TriggerAction::RemoveStacks},
    {"refresh_effect", TriggerAction::RefreshEffect},
    {"deal_damage", TriggerAction::DealDamage},
    {"damage", TriggerAction::DealDamage},
    {"heal_target", TriggerAction::HealTarget},
    {"heal", TriggerAction::HealTarget},
    {"restore_mana", TriggerAction::RestoreMana},
    {"modify_stat", TriggerAction::ModifyStat},
    {"spawn_projectile", TriggerAction::SpawnProjectile},
    {"create_aura", TriggerAction::CreateAura},
    {"teleport_target", TriggerAction::TeleportTarget},
    {"knockback_target", TriggerAction::KnockbackTarget},
    {"knockback", TriggerAction::KnockbackTarget},
    {"stun_target", TriggerAction::StunTarget},
    {"stun", TriggerAction::StunTarget},
    {"execute_script", TriggerAction::ExecuteScript},
    {"script", TriggerAction::ExecuteScript},
    {"chain_trigger", TriggerAction::ChainTrigger}
};

const char* TriggerActionToString(TriggerAction action) {
    switch (action) {
        case TriggerAction::ApplyEffect:     return "apply_effect";
        case TriggerAction::RemoveEffect:    return "remove_effect";
        case TriggerAction::ExtendDuration:  return "extend_duration";
        case TriggerAction::ReduceDuration:  return "reduce_duration";
        case TriggerAction::AddStacks:       return "add_stacks";
        case TriggerAction::RemoveStacks:    return "remove_stacks";
        case TriggerAction::RefreshEffect:   return "refresh_effect";
        case TriggerAction::DealDamage:      return "deal_damage";
        case TriggerAction::HealTarget:      return "heal_target";
        case TriggerAction::RestoreMana:     return "restore_mana";
        case TriggerAction::ModifyStat:      return "modify_stat";
        case TriggerAction::SpawnProjectile: return "spawn_projectile";
        case TriggerAction::CreateAura:      return "create_aura";
        case TriggerAction::TeleportTarget:  return "teleport_target";
        case TriggerAction::KnockbackTarget: return "knockback_target";
        case TriggerAction::StunTarget:      return "stun_target";
        case TriggerAction::ExecuteScript:   return "execute_script";
        case TriggerAction::ChainTrigger:    return "chain_trigger";
    }
    return "apply_effect";
}

std::optional<TriggerAction> TriggerActionFromString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = s_triggerActionFromString.find(lower);
    if (it != s_triggerActionFromString.end()) {
        return it->second;
    }
    return std::nullopt;
}

static const std::unordered_map<std::string, TriggerTarget> s_triggerTargetFromString = {
    {"self", TriggerTarget::Self},
    {"source", TriggerTarget::Source},
    {"attack_target", TriggerTarget::AttackTarget},
    {"target", TriggerTarget::AttackTarget},
    {"damage_source", TriggerTarget::DamageSource},
    {"attacker", TriggerTarget::DamageSource},
    {"heal_source", TriggerTarget::HealSource},
    {"healer", TriggerTarget::HealSource},
    {"nearest_enemy", TriggerTarget::NearestEnemy},
    {"nearest_ally", TriggerTarget::NearestAlly},
    {"all_nearby_enemies", TriggerTarget::AllNearbyEnemies},
    {"nearby_enemies", TriggerTarget::AllNearbyEnemies},
    {"all_nearby_allies", TriggerTarget::AllNearbyAllies},
    {"nearby_allies", TriggerTarget::AllNearbyAllies},
    {"random_enemy", TriggerTarget::RandomEnemy},
    {"random_ally", TriggerTarget::RandomAlly},
    {"kill_target", TriggerTarget::KillTarget},
    {"killed", TriggerTarget::KillTarget},
    {"custom", TriggerTarget::Custom}
};

const char* TriggerTargetToString(TriggerTarget target) {
    switch (target) {
        case TriggerTarget::Self:             return "self";
        case TriggerTarget::Source:           return "source";
        case TriggerTarget::AttackTarget:     return "attack_target";
        case TriggerTarget::DamageSource:     return "damage_source";
        case TriggerTarget::HealSource:       return "heal_source";
        case TriggerTarget::NearestEnemy:     return "nearest_enemy";
        case TriggerTarget::NearestAlly:      return "nearest_ally";
        case TriggerTarget::AllNearbyEnemies: return "all_nearby_enemies";
        case TriggerTarget::AllNearbyAllies:  return "all_nearby_allies";
        case TriggerTarget::RandomEnemy:      return "random_enemy";
        case TriggerTarget::RandomAlly:       return "random_ally";
        case TriggerTarget::KillTarget:       return "kill_target";
        case TriggerTarget::Custom:           return "custom";
    }
    return "self";
}

std::optional<TriggerTarget> TriggerTargetFromString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = s_triggerTargetFromString.find(lower);
    if (it != s_triggerTargetFromString.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ============================================================================
// JSON Helpers
// ============================================================================

namespace {

std::string ExtractJsonString(const std::string& json, const std::string& key) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(json, match, pattern)) {
        return match[1].str();
    }
    return "";
}

float ExtractJsonNumber(const std::string& json, const std::string& key, float defaultVal = 0.0f) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]*\\.?[0-9]+)");
    std::smatch match;
    if (std::regex_search(json, match, pattern)) {
        try {
            return std::stof(match[1].str());
        } catch (...) {}
    }
    return defaultVal;
}

int ExtractJsonInt(const std::string& json, const std::string& key, int defaultVal = 0) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (std::regex_search(json, match, pattern)) {
        try {
            return std::stoi(match[1].str());
        } catch (...) {}
    }
    return defaultVal;
}

} // anonymous namespace

// ============================================================================
// Effect Trigger
// ============================================================================

bool EffectTrigger::CanTrigger(float currentTime) const {
    // Check cooldown
    if (cooldown > 0.0f && (currentTime - lastTriggerTime) < cooldown) {
        return false;
    }

    // Check max triggers per combat
    if (maxTriggersPerCombat >= 0 && combatTriggerCount >= maxTriggersPerCombat) {
        return false;
    }

    // Check max triggers per effect
    if (maxTriggersPerEffect >= 0 && triggerCount >= maxTriggersPerEffect) {
        return false;
    }

    return true;
}

void EffectTrigger::OnTriggered(float currentTime) {
    lastTriggerTime = currentTime;
    triggerCount++;
    combatTriggerCount++;
}

void EffectTrigger::ResetCombatTriggers() {
    combatTriggerCount = 0;
}

void EffectTrigger::Reset() {
    lastTriggerTime = 0.0f;
    triggerCount = 0;
    combatTriggerCount = 0;
}

bool EffectTrigger::MatchesEvent(TriggerCondition eventType, const std::string& eventData) const {
    if (condition != eventType) {
        return false;
    }

    // Check event filter if set
    if (!eventFilter.empty() && eventData != eventFilter) {
        return false;
    }

    return true;
}

bool EffectTrigger::RollChance() const {
    if (chance >= 1.0f) return true;
    if (chance <= 0.0f) return false;

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng) < chance;
}

bool EffectTrigger::LoadFromJson(const std::string& jsonStr) {
    // Condition
    std::string condStr = ExtractJsonString(jsonStr, "condition");
    if (auto c = TriggerConditionFromString(condStr)) {
        condition = *c;
    }

    eventFilter = ExtractJsonString(jsonStr, "filter");
    threshold = ExtractJsonNumber(jsonStr, "threshold", 0.0f);

    // Probability and cooldown
    chance = ExtractJsonNumber(jsonStr, "chance", 1.0f);
    cooldown = ExtractJsonNumber(jsonStr, "cooldown", 0.0f);
    maxTriggersPerCombat = ExtractJsonInt(jsonStr, "max_per_combat", -1);
    maxTriggersPerEffect = ExtractJsonInt(jsonStr, "max_triggers", -1);

    // Action
    std::string actionStr = ExtractJsonString(jsonStr, "action");
    if (actionStr.empty()) {
        actionStr = ExtractJsonString(jsonStr, "effect");
        if (!actionStr.empty()) {
            // "effect" key means apply_effect with that effect ID
            action = TriggerAction::ApplyEffect;
            effectId = actionStr;
            actionStr = "apply_effect";
        }
    }
    if (auto a = TriggerActionFromString(actionStr)) {
        action = *a;
    }

    // Target
    std::string targetStr = ExtractJsonString(jsonStr, "target");
    if (auto t = TriggerTargetFromString(targetStr)) {
        target = *t;
    }

    // Action parameters
    if (effectId.empty()) {
        effectId = ExtractJsonString(jsonStr, "effect_id");
    }
    amount = ExtractJsonNumber(jsonStr, "amount", 0.0f);
    stacks = ExtractJsonInt(jsonStr, "stacks", 1);
    range = ExtractJsonNumber(jsonStr, "range", 10.0f);

    // Script
    scriptPath = ExtractJsonString(jsonStr, "script");
    functionName = ExtractJsonString(jsonStr, "function");

    return true;
}

std::string EffectTrigger::ToJson() const {
    std::ostringstream ss;
    ss << "{";
    ss << "\"condition\":\"" << TriggerConditionToString(condition) << "\"";

    if (!eventFilter.empty()) {
        ss << ",\"filter\":\"" << eventFilter << "\"";
    }

    if (threshold > 0.0f) {
        ss << ",\"threshold\":" << threshold;
    }

    if (chance < 1.0f) {
        ss << ",\"chance\":" << chance;
    }

    if (cooldown > 0.0f) {
        ss << ",\"cooldown\":" << cooldown;
    }

    if (maxTriggersPerCombat >= 0) {
        ss << ",\"max_per_combat\":" << maxTriggersPerCombat;
    }

    if (maxTriggersPerEffect >= 0) {
        ss << ",\"max_triggers\":" << maxTriggersPerEffect;
    }

    ss << ",\"action\":\"" << TriggerActionToString(action) << "\"";
    ss << ",\"target\":\"" << TriggerTargetToString(target) << "\"";

    if (!effectId.empty()) {
        ss << ",\"effect_id\":\"" << effectId << "\"";
    }

    if (amount != 0.0f) {
        ss << ",\"amount\":" << amount;
    }

    if (stacks != 1) {
        ss << ",\"stacks\":" << stacks;
    }

    if (range != 10.0f) {
        ss << ",\"range\":" << range;
    }

    if (!scriptPath.empty()) {
        ss << ",\"script\":\"" << scriptPath << "\"";
        if (!functionName.empty()) {
            ss << ",\"function\":\"" << functionName << "\"";
        }
    }

    ss << "}";
    return ss.str();
}

// ============================================================================
// Trigger Builder
// ============================================================================

TriggerBuilder& TriggerBuilder::When(TriggerCondition condition) {
    m_trigger.condition = condition;
    return *this;
}

TriggerBuilder& TriggerBuilder::Filter(const std::string& filter) {
    m_trigger.eventFilter = filter;
    return *this;
}

TriggerBuilder& TriggerBuilder::Threshold(float value) {
    m_trigger.threshold = value;
    return *this;
}

TriggerBuilder& TriggerBuilder::Chance(float probability) {
    m_trigger.chance = probability;
    return *this;
}

TriggerBuilder& TriggerBuilder::Cooldown(float seconds) {
    m_trigger.cooldown = seconds;
    return *this;
}

TriggerBuilder& TriggerBuilder::MaxPerCombat(int count) {
    m_trigger.maxTriggersPerCombat = count;
    return *this;
}

TriggerBuilder& TriggerBuilder::MaxTotal(int count) {
    m_trigger.maxTriggersPerEffect = count;
    return *this;
}

TriggerBuilder& TriggerBuilder::Action(TriggerAction action) {
    m_trigger.action = action;
    return *this;
}

TriggerBuilder& TriggerBuilder::Target(TriggerTarget target) {
    m_trigger.target = target;
    return *this;
}

TriggerBuilder& TriggerBuilder::ApplyEffect(const std::string& effectId) {
    m_trigger.action = TriggerAction::ApplyEffect;
    m_trigger.effectId = effectId;
    return *this;
}

TriggerBuilder& TriggerBuilder::RemoveEffect(const std::string& effectId) {
    m_trigger.action = TriggerAction::RemoveEffect;
    m_trigger.effectId = effectId;
    return *this;
}

TriggerBuilder& TriggerBuilder::Amount(float value) {
    m_trigger.amount = value;
    return *this;
}

TriggerBuilder& TriggerBuilder::Stacks(int count) {
    m_trigger.stacks = count;
    return *this;
}

TriggerBuilder& TriggerBuilder::Range(float radius) {
    m_trigger.range = radius;
    return *this;
}

TriggerBuilder& TriggerBuilder::Script(const std::string& path, const std::string& func) {
    m_trigger.action = TriggerAction::ExecuteScript;
    m_trigger.scriptPath = path;
    m_trigger.functionName = func;
    return *this;
}

} // namespace Effects
} // namespace Vehement
