#include "Ability.hpp"
#include "Hero.hpp"
#include "../entities/Entity.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace RTS {

// ============================================================================
// AbilityBehavior Base
// ============================================================================

bool AbilityBehavior::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!context.caster) {
        return false;
    }

    // Check mana cost
    const auto& levelData = data.GetLevelData(context.abilityLevel);
    if (context.caster->GetMana() < levelData.manaCost) {
        return false;
    }

    // Check if target is required
    if (data.requiresTarget && !context.targetUnit) {
        if (data.targetType == TargetType::Unit) {
            return false;
        }
    }

    // Check range for targeted abilities
    if (data.targetType != TargetType::None && levelData.range > 0.0f) {
        float distance = glm::length(context.targetPoint - context.caster->GetPosition());
        if (distance > levelData.range) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// Rally Ability (Warlord)
// ============================================================================

AbilityCastResult RallyAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (!context.caster) {
        result.failReason = "No caster";
        return result;
    }

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Apply buff to all allies in radius
    // In a real implementation, this would query the entity manager
    // for all friendly units in radius and apply the Might buff

    result.success = true;
    result.unitsAffected = 1; // At minimum, affects the caster

    // Cost mana
    context.caster->ConsumeMana(levelData.manaCost);

    return result;
}

void RallyAbility::Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) {
    // Rally is a toggle ability - drain mana over time
    if (data.type == AbilityType::Toggle) {
        const auto& levelData = data.GetLevelData(context.abilityLevel);
        float manaDrain = levelData.manaCost * 0.1f * deltaTime; // 10% of cost per second

        if (context.caster && context.caster->GetMana() >= manaDrain) {
            context.caster->ConsumeMana(manaDrain);
        }
    }
}

void RallyAbility::OnEnd(const AbilityCastContext& context, const AbilityData& data) {
    // Remove buff from all affected allies
    // Implementation would remove the Might status effect
}

// ============================================================================
// Inspire Ability (Commander)
// ============================================================================

AbilityCastResult InspireAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (!context.caster) {
        result.failReason = "No caster";
        return result;
    }

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Inspire grants movement speed and cooldown reduction to allies
    // Effect strength determines the percentage bonus
    // Duration determines how long the buff lasts

    result.success = true;
    result.unitsAffected = 1;

    context.caster->ConsumeMana(levelData.manaCost);

    return result;
}

// ============================================================================
// Fortify Ability (Engineer)
// ============================================================================

AbilityCastResult FortifyAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (!context.caster) {
        result.failReason = "No caster";
        return result;
    }

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Fortify increases armor and max health of target
    // Can target buildings for massive bonus
    // Can target units for smaller bonus

    if (context.targetUnit) {
        // Apply fortified status to target
        result.success = true;
        result.unitsAffected = 1;
        result.affectedEntities.push_back(context.targetUnit->GetId());
    } else {
        // Self-cast
        result.success = true;
        result.unitsAffected = 1;
    }

    context.caster->ConsumeMana(levelData.manaCost);

    return result;
}

// ============================================================================
// Shadowstep Ability (Scout)
// ============================================================================

bool ShadowstepAbility::CanCast(const AbilityCastContext& context, const AbilityData& data) const {
    if (!AbilityBehavior::CanCast(context, data)) {
        return false;
    }

    // Additional check: target point must be on valid terrain
    // In real implementation, would check collision/pathfinding

    return true;
}

AbilityCastResult ShadowstepAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (!context.caster) {
        result.failReason = "No caster";
        return result;
    }

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Teleport hero to target location
    context.caster->SetPosition(context.targetPoint);

    // Grant brief stealth (invisibility)
    // Duration based on ability level
    context.caster->ApplyStatusEffect(StatusEffect::Invisible, levelData.duration);

    result.success = true;
    result.unitsAffected = 1;

    context.caster->ConsumeMana(levelData.manaCost);

    return result;
}

// ============================================================================
// Market Mastery Ability (Merchant)
// ============================================================================

AbilityCastResult MarketMasteryAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    // Market Mastery is a passive ability
    // It's always active and provides gold bonuses
    // The Execute function is called when leveling up to apply the new bonus

    result.success = true;

    return result;
}

// ============================================================================
// Warcry Ability (Ultimate)
// ============================================================================

AbilityCastResult WarcryAbility::Execute(const AbilityCastContext& context, const AbilityData& data) {
    AbilityCastResult result;

    if (!context.caster) {
        result.failReason = "No caster";
        return result;
    }

    const auto& levelData = data.GetLevelData(context.abilityLevel);

    // Warcry deals damage in a large radius and stuns enemies
    // This is the Warlord's ultimate ability

    // In real implementation:
    // 1. Query all enemies in radius around caster
    // 2. Deal damage to each
    // 3. Apply stun status effect
    // 4. Play visual/audio effects

    result.success = true;
    result.damageDealt = levelData.damage; // Would be multiplied by targets hit

    context.caster->ConsumeMana(levelData.manaCost);

    return result;
}

// ============================================================================
// AbilityManager Implementation
// ============================================================================

void AbilityManager::Initialize() {
    RegisterDefaultAbilities();
}

const AbilityData* AbilityManager::GetAbility(int id) const {
    if (id >= 0 && id < static_cast<int>(m_abilities.size())) {
        return &m_abilities[id];
    }
    return nullptr;
}

AbilityBehavior* AbilityManager::GetBehavior(int id) {
    auto it = m_behaviors.find(id);
    if (it != m_behaviors.end()) {
        return it->second.get();
    }
    return nullptr;
}

void AbilityManager::RegisterBehavior(int abilityId, std::unique_ptr<AbilityBehavior> behavior) {
    m_behaviors[abilityId] = std::move(behavior);
}

std::vector<const AbilityData*> AbilityManager::GetAbilitiesForClass(int heroClassId) const {
    std::vector<const AbilityData*> result;
    // Return abilities based on class
    // This would be determined by the class definition
    return result;
}

void AbilityManager::RegisterDefaultAbilities() {
    m_abilities.clear();
    m_behaviors.clear();

    // =========================================================================
    // RALLY (Warlord Q)
    // =========================================================================
    {
        AbilityData rally;
        rally.id = AbilityId::RALLY;
        rally.name = "Rally";
        rally.description = "Inspire nearby allies to fight harder, increasing their damage and attack speed.";
        rally.iconPath = "rts/icons/rally.png";
        rally.type = AbilityType::Toggle;
        rally.targetType = TargetType::None;
        rally.effects = {AbilityEffect::Buff};
        rally.appliesStatus = StatusEffect::Might;
        rally.requiredLevel = 1;
        rally.maxLevel = 4;
        rally.canTargetSelf = true;

        rally.levelData = {
            {.damage = 0, .duration = 0, .radius = 8.0f, .manaCost = 25.0f, .cooldown = 0, .range = 0, .effectStrength = 0.10f},
            {.damage = 0, .duration = 0, .radius = 10.0f, .manaCost = 25.0f, .cooldown = 0, .range = 0, .effectStrength = 0.15f},
            {.damage = 0, .duration = 0, .radius = 12.0f, .manaCost = 25.0f, .cooldown = 0, .range = 0, .effectStrength = 0.20f},
            {.damage = 0, .duration = 0, .radius = 14.0f, .manaCost = 25.0f, .cooldown = 0, .range = 0, .effectStrength = 0.25f}
        };

        rally.castSound = "rts/sounds/rally_cast.wav";
        rally.castEffect = "rts/effects/rally_aura.vfx";

        m_abilities.push_back(rally);
        m_behaviors[AbilityId::RALLY] = std::make_unique<RallyAbility>();
    }

    // =========================================================================
    // INSPIRE (Commander Q)
    // =========================================================================
    {
        AbilityData inspire;
        inspire.id = AbilityId::INSPIRE;
        inspire.name = "Inspire";
        inspire.description = "Grant allied units increased movement speed and reduced ability cooldowns.";
        inspire.iconPath = "rts/icons/inspire.png";
        inspire.type = AbilityType::Active;
        inspire.targetType = TargetType::Area;
        inspire.effects = {AbilityEffect::Buff};
        inspire.appliesStatus = StatusEffect::Haste;
        inspire.requiredLevel = 1;
        inspire.maxLevel = 4;

        inspire.levelData = {
            {.damage = 0, .duration = 10.0f, .radius = 10.0f, .manaCost = 50.0f, .cooldown = 20.0f, .range = 0, .effectStrength = 0.15f},
            {.damage = 0, .duration = 12.0f, .radius = 12.0f, .manaCost = 60.0f, .cooldown = 20.0f, .range = 0, .effectStrength = 0.20f},
            {.damage = 0, .duration = 14.0f, .radius = 14.0f, .manaCost = 70.0f, .cooldown = 20.0f, .range = 0, .effectStrength = 0.25f},
            {.damage = 0, .duration = 16.0f, .radius = 16.0f, .manaCost = 80.0f, .cooldown = 20.0f, .range = 0, .effectStrength = 0.30f}
        };

        inspire.castSound = "rts/sounds/inspire_cast.wav";
        inspire.impactEffect = "rts/effects/inspire_buff.vfx";

        m_abilities.push_back(inspire);
        m_behaviors[AbilityId::INSPIRE] = std::make_unique<InspireAbility>();
    }

    // =========================================================================
    // FORTIFY (Engineer Q)
    // =========================================================================
    {
        AbilityData fortify;
        fortify.id = AbilityId::FORTIFY;
        fortify.name = "Fortify";
        fortify.description = "Reinforce a structure or unit, greatly increasing armor and maximum health.";
        fortify.iconPath = "rts/icons/fortify.png";
        fortify.type = AbilityType::Active;
        fortify.targetType = TargetType::Unit;
        fortify.effects = {AbilityEffect::Buff, AbilityEffect::Shield};
        fortify.appliesStatus = StatusEffect::Fortified;
        fortify.requiredLevel = 1;
        fortify.maxLevel = 4;
        fortify.requiresTarget = true;
        fortify.canTargetEnemy = false;

        fortify.levelData = {
            {.damage = 0, .duration = 20.0f, .radius = 0, .manaCost = 40.0f, .cooldown = 15.0f, .range = 8.0f, .effectStrength = 0.25f},
            {.damage = 0, .duration = 25.0f, .radius = 0, .manaCost = 55.0f, .cooldown = 15.0f, .range = 10.0f, .effectStrength = 0.35f},
            {.damage = 0, .duration = 30.0f, .radius = 0, .manaCost = 70.0f, .cooldown = 15.0f, .range = 12.0f, .effectStrength = 0.45f},
            {.damage = 0, .duration = 35.0f, .radius = 0, .manaCost = 85.0f, .cooldown = 15.0f, .range = 14.0f, .effectStrength = 0.55f}
        };

        fortify.castSound = "rts/sounds/fortify_cast.wav";
        fortify.impactEffect = "rts/effects/fortify_shield.vfx";

        m_abilities.push_back(fortify);
        m_behaviors[AbilityId::FORTIFY] = std::make_unique<FortifyAbility>();
    }

    // =========================================================================
    // SHADOWSTEP (Scout Q)
    // =========================================================================
    {
        AbilityData shadowstep;
        shadowstep.id = AbilityId::SHADOWSTEP;
        shadowstep.name = "Shadowstep";
        shadowstep.description = "Teleport to target location and become invisible briefly.";
        shadowstep.iconPath = "rts/icons/shadowstep.png";
        shadowstep.type = AbilityType::Active;
        shadowstep.targetType = TargetType::Point;
        shadowstep.effects = {AbilityEffect::Teleport, AbilityEffect::Stealth};
        shadowstep.appliesStatus = StatusEffect::Invisible;
        shadowstep.requiredLevel = 1;
        shadowstep.maxLevel = 4;
        shadowstep.canTargetGround = true;

        shadowstep.levelData = {
            {.damage = 0, .duration = 2.0f, .radius = 0, .manaCost = 60.0f, .cooldown = 12.0f, .range = 10.0f, .effectStrength = 0},
            {.damage = 0, .duration = 3.0f, .radius = 0, .manaCost = 70.0f, .cooldown = 11.0f, .range = 12.0f, .effectStrength = 0},
            {.damage = 0, .duration = 4.0f, .radius = 0, .manaCost = 80.0f, .cooldown = 10.0f, .range = 14.0f, .effectStrength = 0},
            {.damage = 0, .duration = 5.0f, .radius = 0, .manaCost = 90.0f, .cooldown = 9.0f, .range = 16.0f, .effectStrength = 0}
        };

        shadowstep.castSound = "rts/sounds/shadowstep_cast.wav";
        shadowstep.castEffect = "rts/effects/shadowstep_vanish.vfx";
        shadowstep.impactEffect = "rts/effects/shadowstep_appear.vfx";

        m_abilities.push_back(shadowstep);
        m_behaviors[AbilityId::SHADOWSTEP] = std::make_unique<ShadowstepAbility>();
    }

    // =========================================================================
    // MARKET MASTERY (Merchant Q) - Passive
    // =========================================================================
    {
        AbilityData marketMastery;
        marketMastery.id = AbilityId::MARKET_MASTERY;
        marketMastery.name = "Market Mastery";
        marketMastery.description = "Passive: Increases gold gained from all sources.";
        marketMastery.iconPath = "rts/icons/market_mastery.png";
        marketMastery.type = AbilityType::Passive;
        marketMastery.targetType = TargetType::None;
        marketMastery.effects = {AbilityEffect::ResourceGain};
        marketMastery.requiredLevel = 1;
        marketMastery.maxLevel = 4;

        marketMastery.levelData = {
            {.damage = 0, .duration = 0, .radius = 0, .manaCost = 0, .cooldown = 0, .range = 0, .effectStrength = 0.10f},
            {.damage = 0, .duration = 0, .radius = 0, .manaCost = 0, .cooldown = 0, .range = 0, .effectStrength = 0.20f},
            {.damage = 0, .duration = 0, .radius = 0, .manaCost = 0, .cooldown = 0, .range = 0, .effectStrength = 0.30f},
            {.damage = 0, .duration = 0, .radius = 0, .manaCost = 0, .cooldown = 0, .range = 0, .effectStrength = 0.40f}
        };

        m_abilities.push_back(marketMastery);
        m_behaviors[AbilityId::MARKET_MASTERY] = std::make_unique<MarketMasteryAbility>();
    }

    // =========================================================================
    // WARCRY (Warlord R - Ultimate)
    // =========================================================================
    {
        AbilityData warcry;
        warcry.id = AbilityId::WARCRY;
        warcry.name = "Warcry";
        warcry.description = "Let out a devastating battle cry that damages and stuns all enemies in a large area.";
        warcry.iconPath = "rts/icons/warcry.png";
        warcry.type = AbilityType::Active;
        warcry.targetType = TargetType::None;
        warcry.effects = {AbilityEffect::Damage, AbilityEffect::Stun};
        warcry.appliesStatus = StatusEffect::Stunned;
        warcry.requiredLevel = 6;  // Ultimate ability requires level 6
        warcry.maxLevel = 3;

        warcry.levelData = {
            {.damage = 100.0f, .duration = 1.5f, .radius = 12.0f, .manaCost = 100.0f, .cooldown = 90.0f, .range = 0, .effectStrength = 0},
            {.damage = 175.0f, .duration = 2.0f, .radius = 14.0f, .manaCost = 150.0f, .cooldown = 80.0f, .range = 0, .effectStrength = 0},
            {.damage = 250.0f, .duration = 2.5f, .radius = 16.0f, .manaCost = 200.0f, .cooldown = 70.0f, .range = 0, .effectStrength = 0}
        };

        warcry.castSound = "rts/sounds/warcry_cast.wav";
        warcry.impactSound = "rts/sounds/warcry_impact.wav";
        warcry.castEffect = "rts/effects/warcry_shockwave.vfx";

        m_abilities.push_back(warcry);
        m_behaviors[AbilityId::WARCRY] = std::make_unique<WarcryAbility>();
    }
}

} // namespace RTS
} // namespace Vehement
