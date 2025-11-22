#include "EffectInstance.hpp"
#include <algorithm>
#include <sstream>
#include <cmath>

namespace Vehement {
namespace Effects {

// ============================================================================
// Static Members
// ============================================================================

EffectInstance::InstanceId EffectInstance::s_nextInstanceId = 1;

// ============================================================================
// Effect Instance
// ============================================================================

EffectInstance::EffectInstance() = default;

EffectInstance::EffectInstance(const EffectDefinition* definition) {
    Initialize(definition);
}

EffectInstance::~EffectInstance() = default;

void EffectInstance::Initialize(const EffectDefinition* definition) {
    m_definition = definition;

    if (definition) {
        m_effectId = definition->GetId();
        m_totalDuration = definition->GetBaseDuration();
        m_remainingDuration = m_totalDuration;
        m_charges = definition->GetMaxCharges();

        // Initialize periodic timers
        const auto& periodicEffects = definition->GetPeriodicEffects();
        m_periodicTimers.resize(periodicEffects.size(), 0.0f);
        m_pendingTicks.resize(periodicEffects.size(), false);

        // Copy triggers for runtime state
        m_triggers = definition->GetTriggers();
    }

    m_instanceId = s_nextInstanceId++;
    m_state = EffectState::Inactive;
    m_stacks = 1;
    m_elapsedTime = 0.0f;
}

void EffectInstance::OnApply(uint32_t sourceId, uint32_t targetId) {
    m_sourceId = sourceId;
    m_targetId = targetId;
    m_state = EffectState::Active;
    m_elapsedTime = 0.0f;

    // Reset periodic timers
    for (size_t i = 0; i < m_periodicTimers.size(); ++i) {
        m_periodicTimers[i] = 0.0f;
        m_pendingTicks[i] = false;

        // Handle tick on apply
        if (m_definition) {
            const auto& periodicEffects = m_definition->GetPeriodicEffects();
            if (i < periodicEffects.size() && periodicEffects[i].tickOnApply) {
                m_pendingTicks[i] = true;
            }
        }
    }

    // Reset triggers
    for (auto& trigger : m_triggers) {
        trigger.Reset();
    }
}

void EffectInstance::Update(float deltaTime) {
    if (m_state != EffectState::Active) {
        return;
    }

    UpdateDuration(deltaTime);
    UpdatePeriodicEffects(deltaTime);
    CheckExpiration();
}

void EffectInstance::UpdateDuration(float deltaTime) {
    m_elapsedTime += deltaTime;

    if (!IsPermanent()) {
        m_remainingDuration -= deltaTime;
        if (m_remainingDuration < 0.0f) {
            m_remainingDuration = 0.0f;
        }
    }
}

void EffectInstance::UpdatePeriodicEffects(float deltaTime) {
    if (!m_definition) return;

    const auto& periodicEffects = m_definition->GetPeriodicEffects();

    for (size_t i = 0; i < periodicEffects.size() && i < m_periodicTimers.size(); ++i) {
        m_periodicTimers[i] += deltaTime;

        // Check if interval elapsed
        if (m_periodicTimers[i] >= periodicEffects[i].interval) {
            m_periodicTimers[i] -= periodicEffects[i].interval;
            m_pendingTicks[i] = true;

            // Invoke tick callback
            if (m_onTick) {
                m_onTick(this);
            }
        }
    }
}

void EffectInstance::CheckExpiration() {
    if (m_state != EffectState::Active) {
        return;
    }

    bool shouldExpire = false;

    // Check timed expiration
    if (m_definition) {
        DurationType durType = m_definition->GetDurationType();

        if (durType == DurationType::Timed && m_remainingDuration <= 0.0f) {
            shouldExpire = true;
        } else if (durType == DurationType::Charges && m_charges <= 0) {
            shouldExpire = true;
        } else if (durType == DurationType::Hybrid) {
            if (m_remainingDuration <= 0.0f || m_charges <= 0) {
                shouldExpire = true;
            }
        }
    }

    if (shouldExpire) {
        OnExpire();
    }
}

void EffectInstance::OnExpire() {
    if (m_state != EffectState::Active) {
        return;
    }

    m_state = EffectState::Expiring;

    // Handle tick on expire
    if (m_definition) {
        const auto& periodicEffects = m_definition->GetPeriodicEffects();
        for (size_t i = 0; i < periodicEffects.size() && i < m_pendingTicks.size(); ++i) {
            if (periodicEffects[i].tickOnExpire) {
                m_pendingTicks[i] = true;
            }
        }
    }

    if (m_onExpire) {
        m_onExpire(this);
    }

    m_state = EffectState::Expired;
}

void EffectInstance::OnRemove() {
    if (m_state == EffectState::Removed || m_state == EffectState::Dispelled) {
        return;
    }

    if (m_onRemove) {
        m_onRemove(this);
    }

    m_state = EffectState::Removed;
}

void EffectInstance::OnDispel() {
    if (m_state == EffectState::Removed || m_state == EffectState::Dispelled) {
        return;
    }

    if (m_onRemove) {
        m_onRemove(this);
    }

    m_state = EffectState::Dispelled;
}

void EffectInstance::OnRefresh() {
    // Refresh duration based on stacking mode
    if (m_definition) {
        StackingMode mode = m_definition->GetStacking().mode;

        switch (mode) {
            case StackingMode::None:
            case StackingMode::Refresh:
                RefreshDuration();
                break;

            case StackingMode::Duration:
                ExtendDuration(m_definition->GetBaseDuration());
                break;

            case StackingMode::Intensity:
                // Duration unchanged, stacks handled separately
                break;

            case StackingMode::Separate:
                // This shouldn't be called for separate stacking
                break;
        }
    }
}

void EffectInstance::OnStackAdded(int newStacks) {
    m_stacks = newStacks;

    // Optionally extend duration per stack
    if (m_definition) {
        float durationBonus = m_definition->GetStacking().stackDurationBonus;
        if (durationBonus > 0.0f) {
            ExtendDuration(durationBonus);
        }
    }
}

// -------------------------------------------------------------------------
// Duration
// -------------------------------------------------------------------------

bool EffectInstance::IsPermanent() const {
    return m_definition && m_definition->GetDurationType() == DurationType::Permanent;
}

void EffectInstance::SetDuration(float duration) {
    m_remainingDuration = duration;
    m_totalDuration = duration;
}

void EffectInstance::ExtendDuration(float amount) {
    m_remainingDuration += amount;
    m_totalDuration = std::max(m_totalDuration, m_remainingDuration);
}

void EffectInstance::ReduceDuration(float amount) {
    m_remainingDuration -= amount;
    if (m_remainingDuration < 0.0f) {
        m_remainingDuration = 0.0f;
    }
}

void EffectInstance::RefreshDuration() {
    if (m_definition) {
        m_remainingDuration = m_definition->GetBaseDuration();
        m_totalDuration = m_remainingDuration;
    }
}

// -------------------------------------------------------------------------
// Stacks
// -------------------------------------------------------------------------

int EffectInstance::GetMaxStacks() const {
    return m_definition ? m_definition->GetStacking().maxStacks : 1;
}

void EffectInstance::SetStacks(int stacks) {
    m_stacks = std::clamp(stacks, 0, GetMaxStacks());
}

void EffectInstance::AddStacks(int amount) {
    SetStacks(m_stacks + amount);
}

void EffectInstance::RemoveStacks(int amount) {
    SetStacks(m_stacks - amount);
}

// -------------------------------------------------------------------------
// Charges
// -------------------------------------------------------------------------

int EffectInstance::GetMaxCharges() const {
    return m_definition ? m_definition->GetMaxCharges() : 1;
}

void EffectInstance::SetCharges(int charges) {
    m_charges = std::clamp(charges, 0, GetMaxCharges());
}

void EffectInstance::ConsumeCharge() {
    if (m_charges > 0) {
        m_charges--;
    }
}

// -------------------------------------------------------------------------
// Stat Modifiers
// -------------------------------------------------------------------------

std::vector<StatModifier> EffectInstance::GetActiveModifiers() const {
    std::vector<StatModifier> result;

    if (!m_definition || m_state != EffectState::Active) {
        return result;
    }

    const auto& baseMods = m_definition->GetModifiers();
    result.reserve(baseMods.size());

    for (const auto& baseMod : baseMods) {
        StatModifier mod = baseMod;
        mod.sourceId = m_instanceId;
        mod.value = GetModifiedValue(baseMod);
        result.push_back(mod);
    }

    return result;
}

float EffectInstance::GetModifiedValue(const StatModifier& baseMod) const {
    float value = baseMod.value;

    if (m_definition && m_stacks > 1) {
        float intensityPerStack = m_definition->GetStacking().intensityPerStack;
        // Stack multiplier: 1 stack = 100%, 2 stacks = 100% + intensity, etc.
        float stackMultiplier = 1.0f + (m_stacks - 1) * (intensityPerStack - 1.0f);
        value *= stackMultiplier;
    }

    return value;
}

// -------------------------------------------------------------------------
// Periodic Effects
// -------------------------------------------------------------------------

bool EffectInstance::HasPendingTicks() const {
    for (bool pending : m_pendingTicks) {
        if (pending) return true;
    }
    return false;
}

std::vector<PeriodicEffect> EffectInstance::ConsumePendingTicks() {
    std::vector<PeriodicEffect> result;

    if (!m_definition) {
        return result;
    }

    const auto& periodicEffects = m_definition->GetPeriodicEffects();

    for (size_t i = 0; i < m_pendingTicks.size() && i < periodicEffects.size(); ++i) {
        if (m_pendingTicks[i]) {
            result.push_back(periodicEffects[i]);
            m_pendingTicks[i] = false;
        }
    }

    return result;
}

// -------------------------------------------------------------------------
// Triggers
// -------------------------------------------------------------------------

std::vector<const EffectTrigger*> EffectInstance::ProcessTriggerEvent(
    const TriggerEventData& eventData
) {
    std::vector<const EffectTrigger*> firedTriggers;

    if (m_state != EffectState::Active) {
        return firedTriggers;
    }

    for (auto& trigger : m_triggers) {
        if (!trigger.MatchesEvent(eventData.eventType, eventData.abilityId)) {
            continue;
        }

        if (!trigger.CanTrigger(eventData.currentTime)) {
            continue;
        }

        // Check threshold for health-based triggers
        if (trigger.condition == TriggerCondition::OnHealthBelow ||
            trigger.condition == TriggerCondition::OnLowHealth) {
            if (eventData.GetHealthPercent() >= trigger.threshold) {
                continue;
            }
        } else if (trigger.condition == TriggerCondition::OnHealthAbove ||
                   trigger.condition == TriggerCondition::OnFullHealth) {
            if (eventData.GetHealthPercent() <= trigger.threshold) {
                continue;
            }
        }

        if (!trigger.RollChance()) {
            continue;
        }

        trigger.OnTriggered(eventData.currentTime);
        firedTriggers.push_back(&trigger);
    }

    return firedTriggers;
}

void EffectInstance::ResetCombatTriggers() {
    for (auto& trigger : m_triggers) {
        trigger.ResetCombatTriggers();
    }
}

// -------------------------------------------------------------------------
// Flags
// -------------------------------------------------------------------------

bool EffectInstance::IsDispellable() const {
    return m_definition ? m_definition->IsDispellable() : true;
}

bool EffectInstance::IsPurgeable() const {
    return m_definition ? m_definition->IsPurgeable() : true;
}

bool EffectInstance::IsHidden() const {
    return m_definition ? m_definition->IsHidden() : false;
}

bool EffectInstance::IsPersistent() const {
    return m_definition ? m_definition->IsPersistent() : false;
}

int EffectInstance::GetPriority() const {
    return m_definition ? m_definition->GetPriority() : 0;
}

// -------------------------------------------------------------------------
// Custom Data
// -------------------------------------------------------------------------

void EffectInstance::SetCustomData(const std::string& key, float value) {
    m_customData[key] = value;
}

float EffectInstance::GetCustomData(const std::string& key, float defaultVal) const {
    auto it = m_customData.find(key);
    return (it != m_customData.end()) ? it->second : defaultVal;
}

// -------------------------------------------------------------------------
// Serialization
// -------------------------------------------------------------------------

std::string EffectInstance::SerializeState() const {
    std::ostringstream ss;
    ss << "{";
    ss << "\"effect_id\":\"" << m_effectId << "\"";
    ss << ",\"source_id\":" << m_sourceId;
    ss << ",\"target_id\":" << m_targetId;
    ss << ",\"remaining_duration\":" << m_remainingDuration;
    ss << ",\"total_duration\":" << m_totalDuration;
    ss << ",\"elapsed_time\":" << m_elapsedTime;
    ss << ",\"stacks\":" << m_stacks;
    ss << ",\"charges\":" << m_charges;
    ss << ",\"state\":" << static_cast<int>(m_state);

    // Custom data
    if (!m_customData.empty()) {
        ss << ",\"custom_data\":{";
        bool first = true;
        for (const auto& [key, value] : m_customData) {
            if (!first) ss << ",";
            ss << "\"" << key << "\":" << value;
            first = false;
        }
        ss << "}";
    }

    // Periodic timers
    if (!m_periodicTimers.empty()) {
        ss << ",\"periodic_timers\":[";
        for (size_t i = 0; i < m_periodicTimers.size(); ++i) {
            if (i > 0) ss << ",";
            ss << m_periodicTimers[i];
        }
        ss << "]";
    }

    ss << "}";
    return ss.str();
}

bool EffectInstance::DeserializeState(const std::string& data) {
    // In production, use proper JSON parsing
    // This is a simplified implementation
    return true;
}

// ============================================================================
// Effect Instance Pool
// ============================================================================

EffectInstancePool::EffectInstancePool(size_t initialSize) {
    m_pool.reserve(initialSize);
    for (size_t i = 0; i < initialSize; ++i) {
        m_pool.push_back(std::make_unique<EffectInstance>());
        m_totalCreated++;
    }
}

EffectInstancePool::~EffectInstancePool() = default;

std::unique_ptr<EffectInstance> EffectInstancePool::Acquire() {
    if (m_pool.empty()) {
        m_totalCreated++;
        m_activeCount++;
        return std::make_unique<EffectInstance>();
    }

    auto instance = std::move(m_pool.back());
    m_pool.pop_back();
    m_activeCount++;
    return instance;
}

void EffectInstancePool::Release(std::unique_ptr<EffectInstance> instance) {
    if (!instance) return;

    // Reset instance state
    instance->Initialize(nullptr);
    instance->m_state = EffectState::Inactive;
    instance->m_customData.clear();

    m_pool.push_back(std::move(instance));
    m_activeCount--;
}

} // namespace Effects
} // namespace Vehement
