#include "AbilityInstance.hpp"
#include "AbilityDefinition.hpp"

#include <algorithm>
#include <sstream>

namespace Vehement {
namespace Abilities {

// ============================================================================
// AbilityInstance Implementation
// ============================================================================

AbilityInstance::AbilityInstance() = default;

AbilityInstance::AbilityInstance(const std::string& definitionId) {
    Initialize(definitionId);
}

AbilityInstance::AbilityInstance(std::shared_ptr<AbilityDefinition> definition) {
    Initialize(definition);
}

AbilityInstance::~AbilityInstance() = default;

bool AbilityInstance::Initialize(const std::string& definitionId) {
    auto definition = AbilityDefinitionRegistry::Instance().Get(definitionId);
    if (!definition) return false;
    return Initialize(definition);
}

bool AbilityInstance::Initialize(std::shared_ptr<AbilityDefinition> definition) {
    if (!definition) return false;

    m_definition = definition;
    m_definitionId = definition->GetId();
    m_state = AbilityState::NotLearned;
    m_currentLevel = 0;

    // Initialize charges from level 1 data
    const auto& levelData = definition->GetDataForLevel(1);
    m_maxCharges = levelData.charges;
    m_charges = m_maxCharges;
    m_chargeRestoreTime = levelData.chargeRestoreTime;

    return true;
}

void AbilityInstance::Reset() {
    m_currentLevel = 0;
    m_state = AbilityState::NotLearned;
    m_cooldownRemaining = 0.0f;
    m_cooldownTotal = 0.0f;
    m_charges = m_maxCharges;
    m_chargeRestoreTimer = 0.0f;
    m_isToggled = false;
    m_isAutocast = false;
    m_channelTimeRemaining = 0.0f;
    m_channelDuration = 0.0f;
}

// ============================================================================
// Level
// ============================================================================

int AbilityInstance::GetMaxLevel() const {
    if (!m_definition) return 4;
    return m_definition->GetMaxLevel();
}

bool AbilityInstance::LevelUp() {
    if (IsMaxLevel()) return false;

    int oldLevel = m_currentLevel;
    m_currentLevel++;

    // Update state if this is first level
    if (oldLevel == 0) {
        m_state = AbilityState::Ready;
    }

    // Update charges from new level
    if (m_definition) {
        const auto& levelData = m_definition->GetDataForLevel(m_currentLevel);
        if (levelData.charges > m_maxCharges) {
            m_charges += (levelData.charges - m_maxCharges);
            m_maxCharges = levelData.charges;
        }
        m_chargeRestoreTime = levelData.chargeRestoreTime;
    }

    if (m_onLevelUp) {
        m_onLevelUp(*this, m_currentLevel);
    }

    return true;
}

void AbilityInstance::SetLevel(int level) {
    int maxLevel = GetMaxLevel();
    m_currentLevel = std::clamp(level, 0, maxLevel);

    if (m_currentLevel > 0 && m_state == AbilityState::NotLearned) {
        m_state = AbilityState::Ready;
    } else if (m_currentLevel == 0) {
        m_state = AbilityState::NotLearned;
    }
}

// ============================================================================
// State
// ============================================================================

bool AbilityInstance::IsReady() const {
    if (!IsLearned()) return false;
    if (m_state == AbilityState::Disabled) return false;
    if (m_state == AbilityState::Channeling) return false;

    // Check cooldown
    if (m_cooldownRemaining > 0.0f) return false;

    // Check charges (if charge-based)
    if (m_maxCharges > 0 && m_charges <= 0) return false;

    return true;
}

void AbilityInstance::SetDisabled(bool disabled) {
    if (disabled) {
        if (m_state == AbilityState::Channeling) {
            InterruptChannel();
        }
        m_state = AbilityState::Disabled;
    } else {
        if (m_state == AbilityState::Disabled) {
            m_state = IsLearned() ? (m_cooldownRemaining > 0.0f ? AbilityState::OnCooldown : AbilityState::Ready)
                                  : AbilityState::NotLearned;
        }
    }
}

// ============================================================================
// Cooldown
// ============================================================================

float AbilityInstance::GetCooldownPercent() const {
    if (m_cooldownTotal <= 0.0f) return 0.0f;
    return m_cooldownRemaining / m_cooldownTotal;
}

void AbilityInstance::StartCooldown(float duration) {
    if (duration <= 0.0f && m_definition) {
        duration = GetCooldown();
    }

    m_cooldownTotal = duration;
    m_cooldownRemaining = duration;

    if (m_state != AbilityState::Disabled && m_state != AbilityState::NotLearned) {
        m_state = AbilityState::OnCooldown;
    }
}

void AbilityInstance::ResetCooldown() {
    m_cooldownRemaining = 0.0f;

    if (m_state == AbilityState::OnCooldown) {
        m_state = AbilityState::Ready;
        if (m_onCooldownComplete) {
            m_onCooldownComplete(*this);
        }
    }
}

void AbilityInstance::ReduceCooldown(float amount) {
    if (amount <= 0.0f) return;

    m_cooldownRemaining = std::max(0.0f, m_cooldownRemaining - amount);

    if (m_cooldownRemaining <= 0.0f && m_state == AbilityState::OnCooldown) {
        m_state = AbilityState::Ready;
        if (m_onCooldownComplete) {
            m_onCooldownComplete(*this);
        }
    }
}

void AbilityInstance::RefreshCooldown() {
    m_cooldownRemaining = m_cooldownTotal;
}

// ============================================================================
// Charges
// ============================================================================

bool AbilityInstance::UseCharge() {
    if (m_maxCharges == 0) return true; // Not charge-based
    if (m_charges <= 0) return false;

    m_charges--;

    // Start charge restore if not already running
    if (m_chargeRestoreTimer <= 0.0f && m_charges < m_maxCharges) {
        m_chargeRestoreTimer = m_chargeRestoreTime;
    }

    return true;
}

void AbilityInstance::AddCharges(int count) {
    m_charges = std::min(m_charges + count, m_maxCharges);
}

void AbilityInstance::SetCharges(int count) {
    m_charges = std::clamp(count, 0, m_maxCharges);
}

// ============================================================================
// Toggle
// ============================================================================

bool AbilityInstance::Toggle() {
    if (!m_definition || m_definition->GetType() != AbilityType::Toggle) {
        return m_isToggled;
    }

    m_isToggled = !m_isToggled;
    m_state = m_isToggled ? AbilityState::Active : AbilityState::Ready;

    return m_isToggled;
}

void AbilityInstance::SetToggled(bool toggled) {
    m_isToggled = toggled;
    if (m_definition && m_definition->GetType() == AbilityType::Toggle) {
        m_state = toggled ? AbilityState::Active : AbilityState::Ready;
    }
}

// ============================================================================
// Autocast
// ============================================================================

bool AbilityInstance::ToggleAutocast() {
    if (!m_definition || m_definition->GetType() != AbilityType::Autocast) {
        return m_isAutocast;
    }

    m_isAutocast = !m_isAutocast;
    return m_isAutocast;
}

void AbilityInstance::SetAutocast(bool autocast) {
    m_isAutocast = autocast;
}

// ============================================================================
// Channeling
// ============================================================================

float AbilityInstance::GetChannelProgress() const {
    if (m_channelDuration <= 0.0f) return 1.0f;
    return 1.0f - (m_channelTimeRemaining / m_channelDuration);
}

void AbilityInstance::StartChanneling(float duration) {
    m_channelDuration = duration;
    m_channelTimeRemaining = duration;
    m_state = AbilityState::Channeling;
}

void AbilityInstance::InterruptChannel() {
    if (m_state != AbilityState::Channeling) return;

    m_channelTimeRemaining = 0.0f;
    m_channelDuration = 0.0f;
    m_state = AbilityState::Ready;

    // Start cooldown on interrupt
    StartCooldown();
}

// ============================================================================
// Current Level Data
// ============================================================================

const AbilityLevelData& AbilityInstance::GetCurrentLevelData() const {
    static AbilityLevelData empty;
    if (!m_definition || m_currentLevel <= 0) return empty;
    return m_definition->GetDataForLevel(m_currentLevel);
}

float AbilityInstance::GetDamage() const {
    return GetCurrentLevelData().damage;
}

float AbilityInstance::GetHealing() const {
    return GetCurrentLevelData().healing;
}

float AbilityInstance::GetManaCost() const {
    return GetCurrentLevelData().manaCost;
}

float AbilityInstance::GetCooldown() const {
    return GetCurrentLevelData().cooldown;
}

float AbilityInstance::GetDuration() const {
    return GetCurrentLevelData().duration;
}

float AbilityInstance::GetCastRange() const {
    return GetCurrentLevelData().castRange;
}

float AbilityInstance::GetEffectRadius() const {
    return GetCurrentLevelData().effectRadius;
}

// ============================================================================
// Update
// ============================================================================

void AbilityInstance::Update(float deltaTime) {
    UpdateCooldown(deltaTime);
    UpdateCharges(deltaTime);
    UpdateChannel(deltaTime);
}

void AbilityInstance::UpdateCooldown(float deltaTime) {
    if (m_cooldownRemaining <= 0.0f) return;

    m_cooldownRemaining -= deltaTime;

    if (m_cooldownRemaining <= 0.0f) {
        m_cooldownRemaining = 0.0f;

        if (m_state == AbilityState::OnCooldown) {
            m_state = AbilityState::Ready;

            if (m_onCooldownComplete) {
                m_onCooldownComplete(*this);
            }
        }
    }
}

void AbilityInstance::UpdateCharges(float deltaTime) {
    if (m_maxCharges == 0) return; // Not charge-based
    if (m_charges >= m_maxCharges) return; // Full charges

    if (m_chargeRestoreTimer > 0.0f) {
        m_chargeRestoreTimer -= deltaTime;

        if (m_chargeRestoreTimer <= 0.0f) {
            m_charges++;

            // Start next charge restore if needed
            if (m_charges < m_maxCharges) {
                m_chargeRestoreTimer = m_chargeRestoreTime;
            } else {
                m_chargeRestoreTimer = 0.0f;
            }
        }
    }
}

void AbilityInstance::UpdateChannel(float deltaTime) {
    if (m_state != AbilityState::Channeling) return;

    m_channelTimeRemaining -= deltaTime;

    if (m_channelTimeRemaining <= 0.0f) {
        m_channelTimeRemaining = 0.0f;
        m_channelDuration = 0.0f;
        m_state = AbilityState::Ready;

        // Start cooldown when channel completes
        StartCooldown();
    }
}

// ============================================================================
// Serialization
// ============================================================================

std::string AbilityInstance::ToJson() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"definition_id\": \"" << m_definitionId << "\",\n";
    ss << "  \"level\": " << m_currentLevel << ",\n";
    ss << "  \"cooldown_remaining\": " << m_cooldownRemaining << ",\n";
    ss << "  \"charges\": " << m_charges << ",\n";
    ss << "  \"toggled\": " << (m_isToggled ? "true" : "false") << ",\n";
    ss << "  \"autocast\": " << (m_isAutocast ? "true" : "false") << "\n";
    ss << "}";
    return ss.str();
}

bool AbilityInstance::FromJson(const std::string& json) {
    // Implementation would parse JSON and restore state
    return false;
}

} // namespace Abilities
} // namespace Vehement
