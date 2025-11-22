#include "AbilityManager.hpp"

#include <algorithm>
#include <cmath>

namespace Vehement {
namespace Abilities {

// ============================================================================
// AbilityManager Implementation
// ============================================================================

AbilityManager& AbilityManager::Instance() {
    static AbilityManager instance;
    return instance;
}

void AbilityManager::Initialize() {
    if (m_initialized) return;

    // Register default effect handlers
    RegisterEffectHandler(AbilityEffect::Type::Damage, [this](Entity* target, const AbilityEffect& effect) {
        // Default damage handling would go here
    });

    RegisterEffectHandler(AbilityEffect::Type::Heal, [this](Entity* target, const AbilityEffect& effect) {
        // Default healing handling would go here
    });

    m_initialized = true;
}

void AbilityManager::Shutdown() {
    m_eventCallbacks.clear();
    m_validateHandlers.clear();
    m_executeHandlers.clear();
    m_effectHandlers.clear();
    m_activeChannels.clear();
    while (!m_eventQueue.empty()) m_eventQueue.pop();
    m_initialized = false;
}

// ============================================================================
// Cast Validation
// ============================================================================

CastValidation AbilityManager::ValidateCast(Entity* caster, AbilityInstance& ability,
                                             const AbilityCastContext& context) {
    if (!caster) {
        return CastValidation::CustomFailure("No caster");
    }

    // Check if ability is learned
    if (!ability.IsLearned()) {
        return CastValidation::Failure(CastFailReason::NotLearned);
    }

    // Check cooldown
    if (ability.IsOnCooldown()) {
        return CastValidation::Failure(CastFailReason::OnCooldown);
    }

    // Check charges
    if (!ability.HasCharges()) {
        return CastValidation::Failure(CastFailReason::NoCharges);
    }

    // Check disabled state
    if (ability.IsDisabled()) {
        return CastValidation::Failure(CastFailReason::Disabled);
    }

    // Check already channeling
    if (ability.IsChanneling()) {
        return CastValidation::Failure(CastFailReason::Channeling);
    }

    // Check mana
    if (!HasEnoughMana(caster, ability)) {
        return CastValidation::Failure(CastFailReason::NotEnoughMana);
    }

    // Check range (for targeted abilities)
    if (!IsInRange(caster, context, ability)) {
        return CastValidation::Failure(CastFailReason::OutOfRange);
    }

    // Custom validation handler
    auto it = m_validateHandlers.find(ability.GetDefinitionId());
    if (it != m_validateHandlers.end()) {
        auto result = it->second(context, ability);
        if (!result.canCast) {
            return result;
        }
    }

    return CastValidation::Success();
}

bool AbilityManager::HasEnoughMana(Entity* caster, const AbilityInstance& ability) const {
    if (!caster) return false;

    float manaCost = ability.GetManaCost();
    float reduction = GetManaCostReduction(caster);
    float finalCost = manaCost * (1.0f - reduction);

    // Would check caster's current mana here
    // For now, assume we have enough
    return true;
}

bool AbilityManager::IsInRange(Entity* caster, const AbilityCastContext& context,
                                const AbilityInstance& ability) const {
    if (!caster) return false;

    auto definition = ability.GetDefinition();
    if (!definition) return true;

    // No target abilities don't need range check
    if (definition->GetTargetingType() == TargetingType::None) {
        return true;
    }

    float castRange = ability.GetCastRange();
    float rangeBonus = GetCastRangeBonus(caster);
    float finalRange = castRange + rangeBonus;

    // Would calculate distance to target here
    // For now, assume in range
    return true;
}

bool AbilityManager::IsValidTarget(Entity* caster, Entity* target,
                                    const AbilityDefinition& definition) const {
    if (!target) return false;

    TargetFlag flags = definition.GetTargetFlags();

    // Check target type flags
    // Would check entity type against flags here

    return true;
}

// ============================================================================
// Targeting Resolution
// ============================================================================

ResolvedTarget AbilityManager::ResolveTargets(Entity* caster, const AbilityInstance& ability,
                                               const AbilityCastContext& context) {
    ResolvedTarget result;
    result.point = context.targetPoint;
    result.direction = context.direction;

    auto definition = ability.GetDefinition();
    if (!definition) {
        return result;
    }

    const auto& levelData = ability.GetCurrentLevelData();
    result.effectRadius = levelData.effectRadius;

    switch (definition->GetTargetingType()) {
        case TargetingType::None:
            // Self-cast, target is caster
            result.targets.push_back(context.casterId);
            result.valid = true;
            break;

        case TargetingType::Unit:
            // Single unit target
            if (context.targetId != 0) {
                result.targets.push_back(context.targetId);
                result.valid = true;
            }
            break;

        case TargetingType::Point:
            // Point target, no units
            result.valid = true;
            break;

        case TargetingType::Area:
            // Area effect, find units in radius
            result.targets = FindUnitsInArea(context.targetPoint, levelData.effectRadius,
                                              definition->GetTargetFlags(), context.casterId);
            result.valid = true;
            break;

        case TargetingType::Cone:
            // Cone shaped area
            result.targets = FindUnitsInCone(context.targetPoint, context.direction,
                                              60.0f, levelData.effectRadius,
                                              definition->GetTargetFlags());
            result.valid = true;
            break;

        case TargetingType::Line:
            // Line from caster
            {
                glm::vec3 end = context.targetPoint + context.direction * levelData.travelDistance;
                result.targets = FindUnitsInLine(context.targetPoint, end,
                                                  levelData.width, definition->GetTargetFlags());
            }
            result.valid = true;
            break;

        default:
            result.valid = true;
            break;
    }

    // Limit targets if needed
    if (levelData.maxTargets > 0 && result.targets.size() > static_cast<size_t>(levelData.maxTargets)) {
        result.targets.resize(levelData.maxTargets);
    }

    return result;
}

std::vector<uint32_t> AbilityManager::FindUnitsInArea(const glm::vec3& center, float radius,
                                                        TargetFlag flags, uint32_t casterId) {
    std::vector<uint32_t> result;

    // Would query entity manager for units in radius
    // For now, return empty

    return result;
}

std::vector<uint32_t> AbilityManager::FindUnitsInCone(const glm::vec3& origin, const glm::vec3& direction,
                                                        float angle, float range, TargetFlag flags) {
    std::vector<uint32_t> result;

    // Would query entity manager for units in cone
    // For now, return empty

    return result;
}

std::vector<uint32_t> AbilityManager::FindUnitsInLine(const glm::vec3& start, const glm::vec3& end,
                                                        float width, TargetFlag flags) {
    std::vector<uint32_t> result;

    // Would query entity manager for units in line
    // For now, return empty

    return result;
}

// ============================================================================
// Ability Execution
// ============================================================================

AbilityCastResult AbilityManager::CastAbility(Entity* caster, AbilityInstance& ability,
                                               const AbilityCastContext& context) {
    // Validate cast
    auto validation = ValidateCast(caster, ability, context);
    if (!validation.canCast) {
        AbilityCastResult result;
        result.success = false;
        result.failReason = validation.customReason.empty()
            ? CastFailReasonToString(validation.reason)
            : validation.customReason;

        // Fire failed event
        AbilityEvent event;
        event.type = AbilityEventType::CastFailed;
        event.casterId = context.casterId;
        event.abilityId = ability.GetDefinitionId();
        event.abilityLevel = ability.GetLevel();
        FireEvent(event);

        return result;
    }

    // Resolve targets
    auto targets = ResolveTargets(caster, ability, context);

    // Consume resources
    float manaCost = ability.GetManaCost() * (1.0f - GetManaCostReduction(caster));
    ConsumeMana(caster, manaCost);

    // Use charge if applicable
    ability.UseCharge();

    // Fire cast start event
    AbilityEvent startEvent;
    startEvent.type = AbilityEventType::CastStart;
    startEvent.casterId = context.casterId;
    startEvent.abilityId = ability.GetDefinitionId();
    startEvent.abilityLevel = ability.GetLevel();
    startEvent.position = context.targetPoint;
    FireEvent(startEvent);

    // Build execution context
    AbilityExecutionContext execContext;
    execContext.ability = &ability;
    execContext.casterId = context.casterId;
    execContext.casterEntity = caster;
    execContext.targets = targets;
    execContext.castContext = context;

    // Check for custom execution handler
    auto it = m_executeHandlers.find(ability.GetDefinitionId());
    if (it != m_executeHandlers.end()) {
        auto result = it->second(execContext);

        // Start cooldown
        float cooldown = ability.GetCooldown() * (1.0f - GetCooldownReduction(caster));
        ability.StartCooldown(cooldown);

        // Fire cast complete event
        AbilityEvent completeEvent;
        completeEvent.type = AbilityEventType::CastComplete;
        completeEvent.casterId = context.casterId;
        completeEvent.abilityId = ability.GetDefinitionId();
        completeEvent.abilityLevel = ability.GetLevel();
        completeEvent.value = result.damageDealt;
        FireEvent(completeEvent);

        return result;
    }

    // Default execution
    ExecuteAbility(execContext);

    // Start cooldown
    float cooldown = ability.GetCooldown() * (1.0f - GetCooldownReduction(caster));
    ability.StartCooldown(cooldown);

    // Fire cast complete event
    AbilityEvent completeEvent;
    completeEvent.type = AbilityEventType::CastComplete;
    completeEvent.casterId = context.casterId;
    completeEvent.abilityId = ability.GetDefinitionId();
    completeEvent.abilityLevel = ability.GetLevel();
    FireEvent(completeEvent);

    AbilityCastResult result;
    result.success = true;
    result.targetsHit = static_cast<int>(targets.targets.size());
    result.affectedEntities = targets.targets;
    result.actualCooldown = cooldown;
    result.actualManaCost = manaCost;

    return result;
}

void AbilityManager::ExecuteAbility(AbilityExecutionContext& context) {
    if (!context.ability) return;

    auto definition = context.ability->GetDefinition();
    if (!definition) return;

    const auto& levelData = context.ability->GetCurrentLevelData();

    // Apply damage to all targets
    if (levelData.damage > 0.0f) {
        float spellAmp = GetSpellAmplification(context.casterEntity);
        float finalDamage = levelData.damage * (1.0f + spellAmp);

        for (uint32_t targetId : context.targets.targets) {
            // Would apply damage to target entity here
            // ApplyDamage(context.casterEntity, targetEntity, finalDamage, levelData.damageType);

            AbilityEvent hitEvent;
            hitEvent.type = AbilityEventType::Hit;
            hitEvent.casterId = context.casterId;
            hitEvent.targetId = targetId;
            hitEvent.abilityId = context.ability->GetDefinitionId();
            hitEvent.value = finalDamage;
            FireEvent(hitEvent);
        }
    }

    // Apply healing
    if (levelData.healing > 0.0f) {
        for (uint32_t targetId : context.targets.targets) {
            // Would apply healing to target entity here
        }
    }

    // Apply status effects
    if (levelData.stunDuration > 0.0f) {
        for (uint32_t targetId : context.targets.targets) {
            ApplyStatusEffect(nullptr, "stunned", levelData.stunDuration, 1.0f, context.casterId);
        }
    }

    if (levelData.slowPercent > 0.0f) {
        for (uint32_t targetId : context.targets.targets) {
            ApplyStatusEffect(nullptr, "slowed", levelData.duration, levelData.slowPercent, context.casterId);
        }
    }
}

void AbilityManager::ProcessChannel(Entity* caster, AbilityInstance& ability, float deltaTime) {
    if (!ability.IsChanneling()) return;

    // Update channel time
    ability.Update(deltaTime);

    // Fire channeling event
    AbilityEvent event;
    event.type = AbilityEventType::Channeling;
    event.abilityId = ability.GetDefinitionId();
    event.value = ability.GetChannelProgress();
    FireEvent(event);

    // Check if channel complete
    if (!ability.IsChanneling()) {
        AbilityEvent completeEvent;
        completeEvent.type = AbilityEventType::ChannelComplete;
        completeEvent.abilityId = ability.GetDefinitionId();
        FireEvent(completeEvent);
    }
}

void AbilityManager::CancelAbility(Entity* caster, AbilityInstance& ability) {
    if (ability.IsChanneling()) {
        ability.InterruptChannel();

        AbilityEvent event;
        event.type = AbilityEventType::ChannelInterrupt;
        event.abilityId = ability.GetDefinitionId();
        FireEvent(event);
    }
}

// ============================================================================
// Effect Application
// ============================================================================

void AbilityManager::ApplyEffect(Entity* target, const AbilityEffect& effect) {
    auto it = m_effectHandlers.find(effect.type);
    if (it != m_effectHandlers.end()) {
        it->second(target, effect);
    }
}

float AbilityManager::ApplyDamage(Entity* source, Entity* target, float damage, DamageType type,
                                   const std::string& abilityId) {
    if (!target) return 0.0f;

    float finalDamage = CalculateFinalDamage(damage, type, source, target);

    // Would apply damage to entity here

    return finalDamage;
}

float AbilityManager::ApplyHealing(Entity* source, Entity* target, float amount,
                                    const std::string& abilityId) {
    if (!target) return 0.0f;

    // Would apply healing to entity here

    return amount;
}

void AbilityManager::ApplyStatusEffect(Entity* target, const std::string& effectId,
                                        float duration, float strength, uint32_t sourceId) {
    // Would apply status effect to entity here
}

void AbilityManager::RemoveStatusEffect(Entity* target, const std::string& effectId) {
    // Would remove status effect from entity here
}

// ============================================================================
// Event System
// ============================================================================

void AbilityManager::RegisterEventCallback(AbilityEventType type, EventCallback callback) {
    m_eventCallbacks[type].push_back(std::move(callback));
}

void AbilityManager::FireEvent(const AbilityEvent& event) {
    // Queue event
    m_eventQueue.push(event);

    // Fire callbacks immediately
    auto it = m_eventCallbacks.find(event.type);
    if (it != m_eventCallbacks.end()) {
        for (const auto& callback : it->second) {
            callback(event);
        }
    }
}

std::vector<AbilityEvent> AbilityManager::GetPendingEvents() {
    std::vector<AbilityEvent> events;
    while (!m_eventQueue.empty()) {
        events.push_back(m_eventQueue.front());
        m_eventQueue.pop();
    }
    return events;
}

void AbilityManager::ClearEvents() {
    while (!m_eventQueue.empty()) m_eventQueue.pop();
}

// ============================================================================
// Custom Handlers
// ============================================================================

void AbilityManager::RegisterValidationHandler(const std::string& abilityId, ValidateCallback callback) {
    m_validateHandlers[abilityId] = std::move(callback);
}

void AbilityManager::RegisterExecutionHandler(const std::string& abilityId, ExecuteCallback callback) {
    m_executeHandlers[abilityId] = std::move(callback);
}

void AbilityManager::RegisterEffectHandler(AbilityEffect::Type type, EffectCallback callback) {
    m_effectHandlers[type] = std::move(callback);
}

// ============================================================================
// Utility
// ============================================================================

float AbilityManager::GetCooldownReduction(Entity* entity) const {
    // Would query entity for CDR stat
    return 0.0f;
}

float AbilityManager::GetManaCostReduction(Entity* entity) const {
    // Would query entity for mana cost reduction stat
    return 0.0f;
}

float AbilityManager::GetCastRangeBonus(Entity* entity) const {
    // Would query entity for cast range bonus
    return 0.0f;
}

float AbilityManager::GetSpellAmplification(Entity* entity) const {
    // Would query entity for spell amp stat
    return 0.0f;
}

void AbilityManager::ConsumeMana(Entity* caster, float amount) {
    // Would deduct mana from entity
}

float AbilityManager::CalculateFinalDamage(float baseDamage, DamageType type,
                                            Entity* source, Entity* target) {
    // Would calculate damage reduction from armor/magic resist
    return baseDamage;
}

void AbilityManager::Update(float deltaTime) {
    // Update active channels
    for (auto& channel : m_activeChannels) {
        ProcessChannel(channel.caster, *channel.ability, deltaTime);
    }

    // Remove completed channels
    m_activeChannels.erase(
        std::remove_if(m_activeChannels.begin(), m_activeChannels.end(),
            [](const ActiveChannel& c) { return !c.ability->IsChanneling(); }),
        m_activeChannels.end()
    );
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string CastFailReasonToString(CastFailReason reason) {
    switch (reason) {
        case CastFailReason::None: return "None";
        case CastFailReason::NotLearned: return "Ability not learned";
        case CastFailReason::OnCooldown: return "Ability is on cooldown";
        case CastFailReason::NotEnoughMana: return "Not enough mana";
        case CastFailReason::NotEnoughHealth: return "Not enough health";
        case CastFailReason::NoCharges: return "No charges remaining";
        case CastFailReason::Silenced: return "Cannot cast while silenced";
        case CastFailReason::Stunned: return "Cannot cast while stunned";
        case CastFailReason::Rooted: return "Cannot cast while rooted";
        case CastFailReason::OutOfRange: return "Target is out of range";
        case CastFailReason::InvalidTarget: return "Invalid target";
        case CastFailReason::NoTarget: return "No target selected";
        case CastFailReason::Channeling: return "Already channeling";
        case CastFailReason::Dead: return "Cannot cast while dead";
        case CastFailReason::Disabled: return "Ability is disabled";
        case CastFailReason::Custom: return "Custom failure";
    }
    return "Unknown";
}

std::string AbilityEventTypeToString(AbilityEventType type) {
    switch (type) {
        case AbilityEventType::CastStart: return "cast_start";
        case AbilityEventType::CastComplete: return "cast_complete";
        case AbilityEventType::CastFailed: return "cast_failed";
        case AbilityEventType::Channeling: return "channeling";
        case AbilityEventType::ChannelInterrupt: return "channel_interrupt";
        case AbilityEventType::ChannelComplete: return "channel_complete";
        case AbilityEventType::Hit: return "hit";
        case AbilityEventType::Miss: return "miss";
        case AbilityEventType::Kill: return "kill";
        case AbilityEventType::Cooldown: return "cooldown";
        case AbilityEventType::CooldownComplete: return "cooldown_complete";
        case AbilityEventType::LevelUp: return "level_up";
        case AbilityEventType::Toggle: return "toggle";
        case AbilityEventType::ChargeUsed: return "charge_used";
        case AbilityEventType::ChargeRestored: return "charge_restored";
    }
    return "unknown";
}

} // namespace Abilities
} // namespace Vehement
