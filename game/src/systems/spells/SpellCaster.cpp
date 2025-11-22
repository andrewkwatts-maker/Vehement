#include "SpellCaster.hpp"
#include "SpellManager.hpp"
#include <algorithm>
#include <sstream>

namespace Vehement {
namespace Spells {

// ============================================================================
// String Conversion Functions
// ============================================================================

const char* CastStateToString(CastState state) {
    switch (state) {
        case CastState::Idle: return "idle";
        case CastState::Casting: return "casting";
        case CastState::Channeling: return "channeling";
        case CastState::OnGlobalCooldown: return "on_gcd";
        case CastState::Interrupted: return "interrupted";
        case CastState::Failed: return "failed";
        default: return "unknown";
    }
}

const char* CastFailReasonToString(CastFailReason reason) {
    switch (reason) {
        case CastFailReason::None: return "none";
        case CastFailReason::NotEnoughMana: return "not_enough_mana";
        case CastFailReason::NotEnoughHealth: return "not_enough_health";
        case CastFailReason::NotEnoughStamina: return "not_enough_stamina";
        case CastFailReason::NotEnoughResource: return "not_enough_resource";
        case CastFailReason::OnCooldown: return "on_cooldown";
        case CastFailReason::OnGlobalCooldown: return "on_gcd";
        case CastFailReason::OutOfRange: return "out_of_range";
        case CastFailReason::InvalidTarget: return "invalid_target";
        case CastFailReason::NoLineOfSight: return "no_los";
        case CastFailReason::NotFacingTarget: return "not_facing";
        case CastFailReason::Moving: return "moving";
        case CastFailReason::AlreadyCasting: return "already_casting";
        case CastFailReason::Silenced: return "silenced";
        case CastFailReason::Stunned: return "stunned";
        case CastFailReason::Dead: return "dead";
        case CastFailReason::RequirementNotMet: return "requirement_not_met";
        case CastFailReason::SpellNotKnown: return "spell_not_known";
        case CastFailReason::NoChargesAvailable: return "no_charges";
        default: return "unknown";
    }
}

std::string GetCastFailMessage(CastFailReason reason) {
    switch (reason) {
        case CastFailReason::None: return "";
        case CastFailReason::NotEnoughMana: return "Not enough mana";
        case CastFailReason::NotEnoughHealth: return "Not enough health";
        case CastFailReason::NotEnoughStamina: return "Not enough stamina";
        case CastFailReason::NotEnoughResource: return "Not enough resources";
        case CastFailReason::OnCooldown: return "Spell is on cooldown";
        case CastFailReason::OnGlobalCooldown: return "On global cooldown";
        case CastFailReason::OutOfRange: return "Target is out of range";
        case CastFailReason::InvalidTarget: return "Invalid target";
        case CastFailReason::NoLineOfSight: return "No line of sight";
        case CastFailReason::NotFacingTarget: return "Not facing target";
        case CastFailReason::Moving: return "Cannot cast while moving";
        case CastFailReason::AlreadyCasting: return "Already casting";
        case CastFailReason::Silenced: return "Silenced";
        case CastFailReason::Stunned: return "Stunned";
        case CastFailReason::Dead: return "Cannot cast while dead";
        case CastFailReason::RequirementNotMet: return "Requirements not met";
        case CastFailReason::SpellNotKnown: return "Spell not known";
        case CastFailReason::NoChargesAvailable: return "No charges available";
        default: return "Cannot cast";
    }
}

// ============================================================================
// SpellCaster Implementation
// ============================================================================

SpellCaster::SpellCaster(uint32_t entityId)
    : m_entityId(entityId)
{
}

SpellCaster::~SpellCaster() = default;

void SpellCaster::Initialize(int maxSlots) {
    m_maxSlots = maxSlots;
    m_slots.resize(maxSlots);

    for (int i = 0; i < maxSlots; ++i) {
        m_slots[i].slotIndex = i;
    }

    m_initialized = true;
}

void SpellCaster::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update based on current state
    switch (m_state) {
        case CastState::Casting:
            UpdateCasting(deltaTime);
            break;

        case CastState::Channeling:
            UpdateChanneling(deltaTime);
            break;

        case CastState::OnGlobalCooldown:
            m_gcdRemaining -= deltaTime;
            if (m_gcdRemaining <= 0.0f) {
                m_gcdRemaining = 0.0f;
                m_state = CastState::Idle;
            }
            break;

        default:
            break;
    }

    // Always update cooldowns and effects
    UpdateCooldowns(deltaTime);
    UpdateActiveEffects(deltaTime);
    UpdateSchoolLockouts(deltaTime);
}

bool SpellCaster::AssignSpell(int slotIndex, const std::string& spellId) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_slots.size())) {
        return false;
    }

    if (!m_spellManager) {
        return false;
    }

    const SpellDefinition* spell = m_spellManager->GetSpell(spellId);
    if (!spell) {
        return false;
    }

    SpellSlot& slot = m_slots[slotIndex];
    slot.spellId = spellId;
    slot.isKnown = KnowsSpell(spellId);
    slot.maxCharges = spell->GetTiming().maxCharges;
    slot.currentCharges = slot.maxCharges;
    slot.cooldownRemaining = 0.0f;
    slot.cooldownTotal = spell->GetTiming().cooldown;
    slot.isReady = true;

    return true;
}

void SpellCaster::ClearSlot(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_slots.size())) {
        return;
    }

    m_slots[slotIndex] = SpellSlot{};
    m_slots[slotIndex].slotIndex = slotIndex;
}

std::optional<SpellSlot> SpellCaster::GetSlot(int slotIndex) const {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_slots.size())) {
        return std::nullopt;
    }
    return m_slots[slotIndex];
}

int SpellCaster::FindSpellSlot(const std::string& spellId) const {
    for (const auto& slot : m_slots) {
        if (slot.spellId == spellId) {
            return slot.slotIndex;
        }
    }
    return -1;
}

void SpellCaster::LearnSpell(const std::string& spellId) {
    m_knownSpells.insert(spellId);

    // Update any slots with this spell
    for (auto& slot : m_slots) {
        if (slot.spellId == spellId) {
            slot.isKnown = true;
        }
    }
}

void SpellCaster::UnlearnSpell(const std::string& spellId) {
    m_knownSpells.erase(spellId);

    // Update any slots with this spell
    for (auto& slot : m_slots) {
        if (slot.spellId == spellId) {
            slot.isKnown = false;
        }
    }
}

bool SpellCaster::KnowsSpell(const std::string& spellId) const {
    return m_knownSpells.find(spellId) != m_knownSpells.end();
}

SpellCaster::CastResult SpellCaster::CastSpell(
    int slotIndex,
    uint32_t targetId,
    const glm::vec3& targetPosition,
    const glm::vec3& targetDirection
) {
    CastResult result;

    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_slots.size())) {
        result.failReason = CastFailReason::SpellNotKnown;
        result.failMessage = "Invalid spell slot";
        return result;
    }

    const SpellSlot& slot = m_slots[slotIndex];
    if (slot.spellId.empty()) {
        result.failReason = CastFailReason::SpellNotKnown;
        result.failMessage = "No spell in slot";
        return result;
    }

    return CastSpellById(slot.spellId, targetId, targetPosition, targetDirection);
}

SpellCaster::CastResult SpellCaster::CastSpellById(
    const std::string& spellId,
    uint32_t targetId,
    const glm::vec3& targetPosition,
    const glm::vec3& targetDirection
) {
    CastResult result;

    if (!m_spellManager) {
        result.failReason = CastFailReason::SpellNotKnown;
        result.failMessage = "Spell system not initialized";
        return result;
    }

    const SpellDefinition* spell = m_spellManager->GetSpell(spellId);
    if (!spell) {
        result.failReason = CastFailReason::SpellNotKnown;
        result.failMessage = "Spell not found";
        return result;
    }

    // Check if we can cast
    result = CanCastSpell(spellId, targetId, targetPosition);
    if (!result.success) {
        return result;
    }

    // Consume resources
    if (!ConsumeResources(spell->GetCost())) {
        result.success = false;
        result.failReason = CastFailReason::NotEnoughResource;
        result.failMessage = GetCastFailMessage(CastFailReason::NotEnoughResource);
        return result;
    }

    // Create spell instance
    m_currentCast = m_spellManager->CreateInstance(spellId, m_entityId, targetId, targetPosition, targetDirection);
    if (!m_currentCast) {
        result.success = false;
        result.failReason = CastFailReason::SpellNotKnown;
        result.failMessage = "Failed to create spell instance";
        return result;
    }

    result.instance = m_currentCast.get();

    // Determine if cast is instant or has cast time
    const auto& timing = spell->GetTiming();

    if (timing.castTime <= 0.0f) {
        // Instant cast
        CompleteCast();
    } else {
        // Start casting
        m_state = CastState::Casting;
        m_currentCast->SetState(SpellInstance::State::Casting);
        m_currentCast->SetRemainingCastTime(timing.castTime);
    }

    // Start GCD if applicable
    if (timing.triggersGCD) {
        StartGlobalCooldown(timing.gcdDuration);
    }

    // Fire cast start event
    CastEventData eventData;
    eventData.spell = spell;
    eventData.instance = m_currentCast.get();
    eventData.casterId = m_entityId;
    eventData.targetId = targetId;
    eventData.targetPosition = targetPosition;
    eventData.targetDirection = targetDirection;
    eventData.castTime = timing.castTime;

    FireEvent(m_onCastStart, eventData);

    result.success = true;
    return result;
}

SpellCaster::CastResult SpellCaster::CanCastSpell(
    const std::string& spellId,
    uint32_t targetId,
    const glm::vec3& targetPosition
) const {
    CastResult result;
    result.success = false;

    // Check if dead
    if (m_isDead && m_isDead()) {
        result.failReason = CastFailReason::Dead;
        result.failMessage = GetCastFailMessage(CastFailReason::Dead);
        return result;
    }

    // Check if stunned
    if (m_isStunned && m_isStunned()) {
        result.failReason = CastFailReason::Stunned;
        result.failMessage = GetCastFailMessage(CastFailReason::Stunned);
        return result;
    }

    // Check if already casting
    if (m_state == CastState::Casting || m_state == CastState::Channeling) {
        result.failReason = CastFailReason::AlreadyCasting;
        result.failMessage = GetCastFailMessage(CastFailReason::AlreadyCasting);
        return result;
    }

    if (!m_spellManager) {
        result.failReason = CastFailReason::SpellNotKnown;
        result.failMessage = "Spell system not initialized";
        return result;
    }

    const SpellDefinition* spell = m_spellManager->GetSpell(spellId);
    if (!spell) {
        result.failReason = CastFailReason::SpellNotKnown;
        result.failMessage = GetCastFailMessage(CastFailReason::SpellNotKnown);
        return result;
    }

    const auto& flags = spell->GetFlags();
    const auto& timing = spell->GetTiming();

    // Check if silenced (and spell can be silenced)
    if (flags.canBeSilenced && m_isSilenced && m_isSilenced()) {
        result.failReason = CastFailReason::Silenced;
        result.failMessage = GetCastFailMessage(CastFailReason::Silenced);
        return result;
    }

    // Check school lockout
    if (IsSchoolLocked(spell->GetSchool())) {
        result.failReason = CastFailReason::Silenced;
        result.failMessage = "School is locked out";
        return result;
    }

    // Check GCD
    if (timing.affectedByGCD && IsOnGlobalCooldown()) {
        result.failReason = CastFailReason::OnGlobalCooldown;
        result.failMessage = GetCastFailMessage(CastFailReason::OnGlobalCooldown);
        return result;
    }

    // Check cooldown
    if (IsOnCooldown(spellId)) {
        result.failReason = CastFailReason::OnCooldown;
        result.failMessage = GetCastFailMessage(CastFailReason::OnCooldown);
        return result;
    }

    // Check charges
    int slotIndex = FindSpellSlot(spellId);
    if (slotIndex >= 0) {
        const SpellSlot& slot = m_slots[slotIndex];
        if (slot.maxCharges > 1 && slot.currentCharges <= 0) {
            result.failReason = CastFailReason::NoChargesAvailable;
            result.failMessage = GetCastFailMessage(CastFailReason::NoChargesAvailable);
            return result;
        }
    }

    // Check moving
    if (!flags.canCastWhileMoving && m_isMoving && m_isMoving()) {
        result.failReason = CastFailReason::Moving;
        result.failMessage = GetCastFailMessage(CastFailReason::Moving);
        return result;
    }

    // Check resources
    if (!HasResources(spell->GetCost())) {
        if (spell->GetCost().mana > 0 && m_getMana && m_getMana() < spell->GetCost().mana) {
            result.failReason = CastFailReason::NotEnoughMana;
        } else if (spell->GetCost().health > 0 && m_getHealth && m_getHealth() < spell->GetCost().health) {
            result.failReason = CastFailReason::NotEnoughHealth;
        } else if (spell->GetCost().stamina > 0 && m_getStamina && m_getStamina() < spell->GetCost().stamina) {
            result.failReason = CastFailReason::NotEnoughStamina;
        } else {
            result.failReason = CastFailReason::NotEnoughResource;
        }
        result.failMessage = GetCastFailMessage(result.failReason);
        return result;
    }

    // Check requirements
    if (!CheckRequirements(spell->GetRequirements())) {
        result.failReason = CastFailReason::RequirementNotMet;
        result.failMessage = GetCastFailMessage(CastFailReason::RequirementNotMet);
        return result;
    }

    // Validate target
    if (!ValidateTarget(spell, targetId, targetPosition)) {
        result.failReason = CastFailReason::InvalidTarget;
        result.failMessage = GetCastFailMessage(CastFailReason::InvalidTarget);
        return result;
    }

    result.success = true;
    return result;
}

void SpellCaster::CancelCast() {
    if (m_state != CastState::Casting && m_state != CastState::Channeling) {
        return;
    }

    if (m_currentCast) {
        m_currentCast->SetState(SpellInstance::State::Interrupted);

        CastEventData eventData;
        eventData.spell = m_currentCast->GetDefinition();
        eventData.instance = m_currentCast.get();
        eventData.casterId = m_entityId;

        FireEvent(m_onCastInterrupt, eventData);
    }

    m_currentCast.reset();
    m_state = CastState::Idle;
}

void SpellCaster::InterruptCast(const InterruptInfo& info) {
    if (m_state != CastState::Casting && m_state != CastState::Channeling) {
        return;
    }

    const SpellDefinition* spell = m_currentCast ? m_currentCast->GetDefinition() : nullptr;

    if (spell && !spell->GetFlags().canBeInterrupted) {
        return; // Cannot interrupt this spell
    }

    if (m_currentCast) {
        m_currentCast->SetState(SpellInstance::State::Interrupted);

        CastEventData eventData;
        eventData.spell = spell;
        eventData.instance = m_currentCast.get();
        eventData.casterId = m_entityId;
        eventData.interruptInfo = info;

        FireEvent(m_onCastInterrupt, eventData);
    }

    // Apply school lockout
    if (!info.schoolLocked.empty() && info.lockoutDuration > 0) {
        LockSchool(info.schoolLocked, info.lockoutDuration);
    } else if (spell && info.lockoutDuration > 0) {
        LockSchool(spell->GetSchool(), info.lockoutDuration);
    }

    m_currentCast.reset();
    m_state = CastState::Interrupted;
}

void SpellCaster::StartCooldown(const std::string& spellId, float duration) {
    float adjustedDuration = duration * m_cooldownMultiplier;
    m_cooldowns[spellId] = adjustedDuration;

    // Update slot if applicable
    int slotIndex = FindSpellSlot(spellId);
    if (slotIndex >= 0) {
        m_slots[slotIndex].cooldownRemaining = adjustedDuration;
        m_slots[slotIndex].cooldownTotal = adjustedDuration;
        m_slots[slotIndex].isReady = false;

        // Consume a charge if using charge system
        if (m_slots[slotIndex].maxCharges > 1) {
            m_slots[slotIndex].currentCharges = std::max(0, m_slots[slotIndex].currentCharges - 1);
        }
    }
}

float SpellCaster::GetCooldownRemaining(const std::string& spellId) const {
    auto it = m_cooldowns.find(spellId);
    if (it != m_cooldowns.end()) {
        return it->second;
    }
    return 0.0f;
}

bool SpellCaster::IsOnCooldown(const std::string& spellId) const {
    return GetCooldownRemaining(spellId) > 0.0f;
}

void SpellCaster::ResetCooldown(const std::string& spellId) {
    m_cooldowns.erase(spellId);

    int slotIndex = FindSpellSlot(spellId);
    if (slotIndex >= 0) {
        m_slots[slotIndex].cooldownRemaining = 0.0f;
        m_slots[slotIndex].isReady = true;
    }
}

void SpellCaster::ResetAllCooldowns() {
    m_cooldowns.clear();

    for (auto& slot : m_slots) {
        slot.cooldownRemaining = 0.0f;
        slot.isReady = true;
        slot.currentCharges = slot.maxCharges;
    }
}

void SpellCaster::ReduceCooldown(const std::string& spellId, float amount) {
    auto it = m_cooldowns.find(spellId);
    if (it != m_cooldowns.end()) {
        it->second = std::max(0.0f, it->second - amount);

        int slotIndex = FindSpellSlot(spellId);
        if (slotIndex >= 0) {
            m_slots[slotIndex].cooldownRemaining = it->second;
            if (it->second <= 0.0f) {
                m_slots[slotIndex].isReady = true;
            }
        }
    }
}

void SpellCaster::StartGlobalCooldown(float duration) {
    m_gcdRemaining = duration;
}

bool SpellCaster::CanCast() const {
    if (!m_initialized) return false;
    if (m_isDead && m_isDead()) return false;
    if (m_isStunned && m_isStunned()) return false;
    if (m_state == CastState::Casting || m_state == CastState::Channeling) return false;
    return true;
}

float SpellCaster::GetCastProgress() const {
    if (m_currentCast) {
        return m_currentCast->GetCastProgress();
    }
    return 1.0f;
}

float SpellCaster::GetChannelProgress() const {
    if (m_currentCast) {
        return m_currentCast->GetChannelProgress();
    }
    return 1.0f;
}

void SpellCaster::LockSchool(const std::string& school, float duration) {
    if (school.empty()) return;

    auto it = m_schoolLockouts.find(school);
    if (it != m_schoolLockouts.end()) {
        it->second = std::max(it->second, duration);
    } else {
        m_schoolLockouts[school] = duration;
    }
}

bool SpellCaster::IsSchoolLocked(const std::string& school) const {
    if (school.empty()) return false;

    auto it = m_schoolLockouts.find(school);
    return it != m_schoolLockouts.end() && it->second > 0.0f;
}

float SpellCaster::GetSchoolLockout(const std::string& school) const {
    auto it = m_schoolLockouts.find(school);
    if (it != m_schoolLockouts.end()) {
        return it->second;
    }
    return 0.0f;
}

void SpellCaster::AddActiveEffect(std::unique_ptr<ActiveEffect> effect) {
    m_activeEffects.push_back(std::move(effect));
}

void SpellCaster::RemoveActiveEffect(const std::string& effectId) {
    m_activeEffects.erase(
        std::remove_if(m_activeEffects.begin(), m_activeEffects.end(),
            [&effectId](const std::unique_ptr<ActiveEffect>& effect) {
                return effect->GetEffect() && effect->GetEffect()->GetId() == effectId;
            }),
        m_activeEffects.end()
    );
}

bool SpellCaster::HasActiveEffect(const std::string& effectId) const {
    for (const auto& effect : m_activeEffects) {
        if (effect->GetEffect() && effect->GetEffect()->GetId() == effectId) {
            return true;
        }
    }
    return false;
}

const ActiveEffect* SpellCaster::GetActiveEffect(const std::string& effectId) const {
    for (const auto& effect : m_activeEffects) {
        if (effect->GetEffect() && effect->GetEffect()->GetId() == effectId) {
            return effect.get();
        }
    }
    return nullptr;
}

int SpellCaster::DispelEffects(bool buffs, bool debuffs, int maxDispel) {
    int dispelled = 0;

    auto it = m_activeEffects.begin();
    while (it != m_activeEffects.end() && dispelled < maxDispel) {
        const SpellEffect* effect = (*it)->GetEffect();
        if (effect) {
            bool isBuff = (effect->GetType() == EffectType::Buff ||
                          effect->GetType() == EffectType::HealOverTime ||
                          effect->GetType() == EffectType::AbsorbDamage);
            bool isDebuff = !isBuff;

            if ((buffs && isBuff) || (debuffs && isDebuff)) {
                it = m_activeEffects.erase(it);
                dispelled++;
                continue;
            }
        }
        ++it;
    }

    return dispelled;
}

void SpellCaster::UpdateCasting(float deltaTime) {
    if (!m_currentCast) {
        m_state = CastState::Idle;
        return;
    }

    float remaining = m_currentCast->GetRemainingCastTime() - deltaTime;
    m_currentCast->SetRemainingCastTime(remaining);

    if (remaining <= 0.0f) {
        CompleteCast();
    }
}

void SpellCaster::UpdateChanneling(float deltaTime) {
    if (!m_currentCast) {
        m_state = CastState::Idle;
        return;
    }

    float remaining = m_currentCast->GetRemainingChannelTime() - deltaTime;
    m_currentCast->SetRemainingChannelTime(remaining);

    // Channel tick would be handled here
    // ...

    if (remaining <= 0.0f) {
        m_currentCast->SetState(SpellInstance::State::Completed);
        m_currentCast.reset();
        m_state = CastState::Idle;
    }
}

void SpellCaster::UpdateCooldowns(float deltaTime) {
    // Update cooldown map
    for (auto it = m_cooldowns.begin(); it != m_cooldowns.end(); ) {
        it->second -= deltaTime;
        if (it->second <= 0.0f) {
            it = m_cooldowns.erase(it);
        } else {
            ++it;
        }
    }

    // Update slots
    for (auto& slot : m_slots) {
        if (slot.cooldownRemaining > 0.0f) {
            slot.cooldownRemaining -= deltaTime;
            if (slot.cooldownRemaining <= 0.0f) {
                slot.cooldownRemaining = 0.0f;
                slot.isReady = true;
            }
        }

        // Recharge charges
        if (slot.currentCharges < slot.maxCharges && slot.maxCharges > 1) {
            slot.chargeRechargeRemaining -= deltaTime;
            if (slot.chargeRechargeRemaining <= 0.0f && m_spellManager) {
                const SpellDefinition* spell = m_spellManager->GetSpell(slot.spellId);
                if (spell) {
                    slot.currentCharges++;
                    if (slot.currentCharges < slot.maxCharges) {
                        slot.chargeRechargeRemaining = spell->GetTiming().chargeRechargeTime;
                    }
                }
            }
        }
    }
}

void SpellCaster::UpdateActiveEffects(float deltaTime) {
    // Update effects and remove expired ones
    m_activeEffects.erase(
        std::remove_if(m_activeEffects.begin(), m_activeEffects.end(),
            [deltaTime](std::unique_ptr<ActiveEffect>& effect) {
                return !effect->Update(deltaTime);
            }),
        m_activeEffects.end()
    );
}

void SpellCaster::UpdateSchoolLockouts(float deltaTime) {
    for (auto it = m_schoolLockouts.begin(); it != m_schoolLockouts.end(); ) {
        it->second -= deltaTime;
        if (it->second <= 0.0f) {
            it = m_schoolLockouts.erase(it);
        } else {
            ++it;
        }
    }
}

bool SpellCaster::ValidateTarget(const SpellDefinition* spell, uint32_t targetId, const glm::vec3& targetPos) const {
    if (!spell) return false;

    TargetingMode mode = spell->GetTargetingMode();

    switch (mode) {
        case TargetingMode::Self:
            // Self-cast always valid
            return true;

        case TargetingMode::Single:
        case TargetingMode::Projectile:
        case TargetingMode::Chain:
            // Need a valid target ID
            if (targetId == 0) return false;
            break;

        case TargetingMode::AOE:
        case TargetingMode::Line:
        case TargetingMode::Cone:
            // Ground-targeted spells need valid position
            // (Additional validation could check range, LoS, etc.)
            break;

        case TargetingMode::PassiveRadius:
            // Always valid
            return true;
    }

    // Check range
    if (m_getPosition) {
        glm::vec3 casterPos = m_getPosition();
        float range = spell->GetRange();
        float minRange = spell->GetMinRange();

        float dist = glm::length(targetPos - casterPos);
        if (dist > range || dist < minRange) {
            return false;
        }
    }

    return true;
}

bool SpellCaster::ConsumeResources(const SpellCost& cost) {
    // Verify we have enough before consuming
    if (!HasResources(cost)) {
        return false;
    }

    if (cost.mana > 0 && m_consumeMana) {
        m_consumeMana(cost.mana);
    }

    if (cost.health > 0 && m_consumeHealth) {
        m_consumeHealth(cost.health);
    }

    if (cost.stamina > 0 && m_consumeStamina) {
        m_consumeStamina(cost.stamina);
    }

    for (const auto& [resource, amount] : cost.customResources) {
        auto it = m_customResourceConsume.find(resource);
        if (it != m_customResourceConsume.end()) {
            it->second(amount);
        }
    }

    return true;
}

bool SpellCaster::HasResources(const SpellCost& cost) const {
    if (cost.mana > 0 && m_getMana && m_getMana() < cost.mana) {
        return false;
    }

    if (cost.health > 0 && m_getHealth && m_getHealth() < cost.health) {
        return false;
    }

    if (cost.stamina > 0 && m_getStamina && m_getStamina() < cost.stamina) {
        return false;
    }

    for (const auto& [resource, amount] : cost.customResources) {
        auto it = m_customResourceQueries.find(resource);
        if (it != m_customResourceQueries.end()) {
            if (it->second() < amount) {
                return false;
            }
        }
    }

    return true;
}

bool SpellCaster::CheckRequirements(const SpellRequirements& reqs) const {
    // Level check would require level query
    // Buff checks would iterate active effects
    // Combat state checks would require combat state query

    // For now, basic implementation
    return true;
}

void SpellCaster::CompleteCast() {
    if (!m_currentCast) return;

    const SpellDefinition* spell = m_currentCast->GetDefinition();
    if (!spell) {
        m_currentCast.reset();
        m_state = CastState::Idle;
        return;
    }

    // Check for channeled spell
    if (spell->GetTiming().channelDuration > 0.0f) {
        m_state = CastState::Channeling;
        m_currentCast->SetState(SpellInstance::State::Channeling);
        m_currentCast->SetRemainingChannelTime(spell->GetTiming().channelDuration);
    } else {
        m_currentCast->SetState(SpellInstance::State::Completed);

        // Fire completion event
        CastEventData eventData;
        eventData.spell = spell;
        eventData.instance = m_currentCast.get();
        eventData.casterId = m_entityId;
        eventData.targetId = m_currentCast->GetTargetId();

        FireEvent(m_onCastComplete, eventData);

        // Start cooldown
        StartCooldown(spell->GetId(), spell->GetTiming().cooldown);

        m_currentCast.reset();
        m_state = CastState::Idle;
    }
}

void SpellCaster::FailCast(CastFailReason reason) {
    if (m_currentCast) {
        m_currentCast->SetState(SpellInstance::State::Failed);
        m_currentCast.reset();
    }
    m_state = CastState::Failed;
}

void SpellCaster::FireEvent(CastEventCallback& callback, const CastEventData& data) {
    if (callback) {
        callback(data);
    }
}

} // namespace Spells
} // namespace Vehement
