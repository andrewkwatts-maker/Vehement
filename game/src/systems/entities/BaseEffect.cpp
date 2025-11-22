#include "BaseEffect.hpp"
#include "../lifecycle/ObjectFactory.hpp"
#include <algorithm>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// BaseEffect Implementation
// ============================================================================

BaseEffect::BaseEffect() {
    m_components.Add<TransformComponent>();
}

BaseEffect::~BaseEffect() = default;

void BaseEffect::OnCreate(const nlohmann::json& config) {
    ScriptedLifecycle::OnCreate(config);

    m_components.SetOwner(GetHandle());
    ParseEffectConfig(config);
    m_components.InitializeAll();
}

void BaseEffect::OnTick(float deltaTime) {
    ScriptedLifecycle::OnTick(deltaTime);

    if (!m_isActive || m_isExpired) return;

    // Update position if attached to target
    if (m_attachedToTarget && m_target.IsValid()) {
        // In full implementation, get target position
    }

    // Update duration
    if (m_stats.duration > 0.0f) {
        m_remainingDuration -= deltaTime;
        if (m_remainingDuration <= 0.0f) {
            OnExpired();
            return;
        }
    }

    // Process tick
    if (m_stats.tickInterval > 0.0f) {
        m_tickTimer += deltaTime;
        while (m_tickTimer >= m_stats.tickInterval) {
            m_tickTimer -= m_stats.tickInterval;
            ProcessTick();
        }
    }

    // Update aura
    if (m_effectType == EffectType::Aura && m_stats.auraRadius > 0.0f) {
        UpdateAura(deltaTime);
    }

    // Update transform
    if (auto* transform = m_components.Get<TransformComponent>()) {
        transform->position = m_position;
    }
}

bool BaseEffect::OnEvent(const GameEvent& event) {
    if (ScriptedLifecycle::OnEvent(event)) return true;

    // Handle shield absorption
    if (m_effectType == EffectType::Shield && event.type == EventType::Damaged) {
        if (event.target == m_target && m_currentShield > 0.0f) {
            if (const auto* dmgData = event.GetData<DamageEventData>()) {
                float absorbed = std::min(m_currentShield, dmgData->amount);
                m_currentShield -= absorbed;

                // Reduce damage (would need to modify the event)
                // In full implementation, intercept and reduce damage

                if (m_currentShield <= 0.0f) {
                    Remove();
                }
                return true;
            }
        }
    }

    // Remove on target death
    if (event.type == EventType::Killed && event.target == m_target) {
        Remove();
        return true;
    }

    return false;
}

void BaseEffect::OnDestroy() {
    if (m_isActive) {
        RemoveModifiers();
    }

    m_components.Clear();
    ScriptedLifecycle::OnDestroy();
}

void BaseEffect::Apply(LifecycleHandle target, LifecycleHandle source) {
    m_target = target;
    m_source = source;
    m_remainingDuration = m_stats.duration;
    m_tickTimer = 0.0f;
    m_tickCount = 0;
    m_currentStacks = 1;
    m_isActive = true;
    m_isExpired = false;

    // Initialize shield
    if (m_effectType == EffectType::Shield) {
        m_currentShield = m_stats.shieldAmount;
    }

    // Apply modifiers
    ApplyModifiers();

    // Call hook
    OnApplied();

    // Fire event
    GameEvent event(EventType::StatusApplied, source, target);
    QueueEvent(std::move(event));

    // Callback
    if (m_onApply) {
        m_onApply(target);
    }
}

void BaseEffect::Remove() {
    if (!m_isActive) return;

    m_isActive = false;

    // Remove modifiers
    RemoveModifiers();

    // Call hook
    OnRemoved();

    // Fire event
    GameEvent event(EventType::StatusRemoved, m_source, m_target);
    QueueEvent(std::move(event));

    // Callback
    if (m_onRemove) {
        m_onRemove(m_target);
    }

    // Destroy
    auto& manager = GetGlobalLifecycleManager();
    manager.Destroy(GetHandle(), false);
}

void BaseEffect::Refresh() {
    m_remainingDuration = m_stats.duration;
    m_tickTimer = 0.0f;
}

void BaseEffect::AddStacks(int count) {
    if (count <= 0) return;

    int oldStacks = m_currentStacks;
    m_currentStacks = std::min(m_currentStacks + count, m_stats.maxStacks);

    if (m_currentStacks != oldStacks) {
        OnStacksChanged(oldStacks, m_currentStacks);
    }
}

void BaseEffect::RemoveStacks(int count) {
    if (count <= 0) return;

    int oldStacks = m_currentStacks;
    m_currentStacks = std::max(0, m_currentStacks - count);

    if (m_currentStacks != oldStacks) {
        OnStacksChanged(oldStacks, m_currentStacks);

        if (m_currentStacks <= 0) {
            Remove();
        }
    }
}

float BaseEffect::GetProgress() const {
    if (m_stats.duration <= 0.0f) return 1.0f;
    return 1.0f - (m_remainingDuration / m_stats.duration);
}

float BaseEffect::GetDamageMultiplier() const {
    float mult = m_stats.damageMultiplier;
    if (m_stacking == EffectStacking::Intensity) {
        mult = 1.0f + (mult - 1.0f) * m_currentStacks;
    }
    return mult;
}

float BaseEffect::GetSpeedMultiplier() const {
    float mult = m_stats.speedMultiplier;
    if (m_stacking == EffectStacking::Intensity) {
        mult = 1.0f + (mult - 1.0f) * m_currentStacks;
    }
    return mult;
}

float BaseEffect::GetArmorMultiplier() const {
    float mult = m_stats.armorMultiplier;
    if (m_stacking == EffectStacking::Intensity) {
        mult = 1.0f + (mult - 1.0f) * m_currentStacks;
    }
    return mult;
}

ScriptContext BaseEffect::BuildContext() const {
    ScriptContext ctx = ScriptedLifecycle::BuildContext();
    ctx.entityType = "effect";

    ctx.transform.x = m_position.x;
    ctx.transform.y = m_position.y;
    ctx.transform.z = m_position.z;

    return ctx;
}

void BaseEffect::ParseEffectConfig(const nlohmann::json& config) {
    (void)config;
    // Parse in full implementation
}

void BaseEffect::OnApplied() {
    // Fire effect started event
    GameEvent event(EventType::EffectStarted, GetHandle(), m_target);
    QueueEvent(std::move(event));
}

void BaseEffect::OnRemoved() {
    // Fire effect ended event
    GameEvent event(EventType::EffectEnded, GetHandle(), m_target);
    QueueEvent(std::move(event));
}

void BaseEffect::OnEffectTick() {
    // Override in derived classes
}

void BaseEffect::OnExpired() {
    m_isExpired = true;
    Remove();
}

void BaseEffect::OnStacksChanged(int oldStacks, int newStacks) {
    (void)oldStacks;
    (void)newStacks;
    // Reapply modifiers with new stack count
    RemoveModifiers();
    ApplyModifiers();
}

void BaseEffect::ApplyModifiers() {
    if (!m_target.IsValid()) return;

    // In a full implementation, this would modify the target's stats
    // through a stat modifier system

    // Handle CC effects
    switch (m_effectType) {
        case EffectType::Stun:
            // Prevent movement and actions
            break;
        case EffectType::Root:
            // Prevent movement only
            break;
        case EffectType::Silence:
            // Prevent abilities only
            break;
        default:
            break;
    }
}

void BaseEffect::RemoveModifiers() {
    if (!m_target.IsValid()) return;

    // Remove stat modifications
}

void BaseEffect::ProcessTick() {
    m_tickCount++;

    // DOT damage
    if (m_effectType == EffectType::DOT && m_stats.damagePerTick > 0.0f) {
        float damage = m_stats.damagePerTick * m_currentStacks;

        DamageEventData data;
        data.amount = damage;
        data.sourceHandle = m_source;
        data.targetHandle = m_target;
        data.damageType = m_stats.damageType;
        data.isDot = true;

        GameEvent event(EventType::Damaged, m_source, m_target);
        event.SetData(data);

        auto& manager = GetGlobalLifecycleManager();
        manager.SendEvent(m_target, event);
    }

    // HOT healing
    if (m_effectType == EffectType::HOT && m_stats.healPerTick > 0.0f) {
        float heal = m_stats.healPerTick * m_currentStacks;

        DamageEventData data;
        data.amount = heal;
        data.sourceHandle = m_source;
        data.targetHandle = m_target;

        GameEvent event(EventType::Healed, m_source, m_target);
        event.SetData(data);

        auto& manager = GetGlobalLifecycleManager();
        manager.SendEvent(m_target, event);
    }

    // Fire tick event
    GameEvent event(EventType::StatusTick, GetHandle(), m_target);
    QueueEvent(std::move(event));

    // Hook and callback
    OnEffectTick();

    if (m_onTick) {
        m_onTick(m_target, m_tickCount);
    }
}

void BaseEffect::UpdateAura(float deltaTime) {
    m_auraUpdateTimer += deltaTime;

    // Update aura targets every 0.5 seconds
    if (m_auraUpdateTimer >= 0.5f) {
        m_auraUpdateTimer = 0.0f;
        ApplyAuraToTargets();
    }
}

void BaseEffect::ApplyAuraToTargets() {
    auto& manager = GetGlobalLifecycleManager();

    // Find targets in range
    // In a full implementation, use spatial queries

    // For now, just clear old targets
    m_auraTargets.clear();
}

// ============================================================================
// EffectManager Implementation
// ============================================================================

LifecycleHandle EffectManager::ApplyEffect(const std::string& effectId,
                                           LifecycleHandle target,
                                           LifecycleHandle source) {
    auto& factory = GetGlobalObjectFactory();
    auto& manager = GetGlobalLifecycleManager();

    auto effect = factory.CreateFromDefinition(effectId);
    if (!effect) {
        // Try creating by type
        effect = factory.CreateByType("effect", {});
    }

    if (!effect) return LifecycleHandle::Invalid;

    auto* baseEffect = dynamic_cast<BaseEffect*>(effect.get());
    if (!baseEffect) return LifecycleHandle::Invalid;

    baseEffect->SetEffectId(effectId);
    baseEffect->Apply(target, source);

    // Transfer ownership to manager
    nlohmann::json config;
    return manager.CreateFromConfig("effect", config);
}

void EffectManager::RemoveEffect(LifecycleHandle target, const std::string& effectId) {
    auto effects = GetEffects(target);
    for (auto* effect : effects) {
        if (effect->GetEffectId() == effectId) {
            effect->Remove();
        }
    }
}

void EffectManager::RemoveAllEffects(LifecycleHandle target) {
    auto effects = GetEffects(target);
    for (auto* effect : effects) {
        effect->Remove();
    }
}

bool EffectManager::HasEffect(LifecycleHandle target, const std::string& effectId) {
    auto effects = GetEffects(target);
    for (auto* effect : effects) {
        if (effect->GetEffectId() == effectId && effect->IsActive()) {
            return true;
        }
    }
    return false;
}

std::vector<BaseEffect*> EffectManager::GetEffects(LifecycleHandle target) {
    std::vector<BaseEffect*> result;

    auto& manager = GetGlobalLifecycleManager();
    auto effects = manager.GetAllOfType<BaseEffect>();

    for (auto* effect : effects) {
        if (effect->GetTarget() == target && effect->IsActive()) {
            result.push_back(effect);
        }
    }

    return result;
}

float EffectManager::GetCombinedDamageMultiplier(LifecycleHandle target) {
    float mult = 1.0f;
    auto effects = GetEffects(target);
    for (auto* effect : effects) {
        mult *= effect->GetDamageMultiplier();
    }
    return mult;
}

float EffectManager::GetCombinedSpeedMultiplier(LifecycleHandle target) {
    float mult = 1.0f;
    auto effects = GetEffects(target);
    for (auto* effect : effects) {
        mult *= effect->GetSpeedMultiplier();
    }
    return mult;
}

float EffectManager::GetCombinedArmorMultiplier(LifecycleHandle target) {
    float mult = 1.0f;
    auto effects = GetEffects(target);
    for (auto* effect : effects) {
        mult *= effect->GetArmorMultiplier();
    }
    return mult;
}

// ============================================================================
// Factory Registration
// ============================================================================

namespace {
    struct BaseEffectRegistrar {
        BaseEffectRegistrar() {
            GetGlobalObjectFactory().RegisterType<BaseEffect>("effect");
        }
    };
    static BaseEffectRegistrar g_baseEffectRegistrar;
}

} // namespace Lifecycle
} // namespace Vehement
