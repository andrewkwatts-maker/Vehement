#pragma once

#include "SpellDefinition.hpp"
#include "SpellEffect.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <unordered_map>
#include <queue>

namespace Vehement {
namespace Spells {

// Forward declarations
class SpellManager;
class SpellInstance;

// ============================================================================
// Cast State Machine
// ============================================================================

/**
 * @brief Current state of the spell caster
 */
enum class CastState : uint8_t {
    Idle,               // Not casting anything
    Casting,            // Cast in progress
    Channeling,         // Channel in progress
    OnGlobalCooldown,   // GCD active
    Interrupted,        // Cast was just interrupted
    Failed              // Cast failed
};

/**
 * @brief Reason why a cast failed
 */
enum class CastFailReason : uint8_t {
    None,               // No failure
    NotEnoughMana,
    NotEnoughHealth,
    NotEnoughStamina,
    NotEnoughResource,
    OnCooldown,
    OnGlobalCooldown,
    OutOfRange,
    InvalidTarget,
    NoLineOfSight,
    NotFacingTarget,
    Moving,
    AlreadyCasting,
    Silenced,
    Stunned,
    Dead,
    RequirementNotMet,
    SpellNotKnown,
    NoChargesAvailable
};

// ============================================================================
// Spell Slot
// ============================================================================

/**
 * @brief A slot containing a spell assignment
 */
struct SpellSlot {
    std::string spellId;            // ID of assigned spell
    int slotIndex = -1;             // Slot number (0-based)

    // Cooldown tracking
    float cooldownRemaining = 0.0f;
    float cooldownTotal = 0.0f;

    // Charge tracking
    int currentCharges = 0;
    int maxCharges = 1;
    float chargeRechargeRemaining = 0.0f;

    // State
    bool isReady = true;
    bool isKnown = false;

    [[nodiscard]] float GetCooldownProgress() const {
        return cooldownTotal > 0 ? (cooldownTotal - cooldownRemaining) / cooldownTotal : 1.0f;
    }

    [[nodiscard]] bool HasCharges() const {
        return currentCharges > 0;
    }
};

// ============================================================================
// Interrupt Info
// ============================================================================

/**
 * @brief Information about an interrupt event
 */
struct InterruptInfo {
    uint32_t sourceId = 0;          // Who interrupted
    float lockoutDuration = 0.0f;   // How long locked out
    std::string schoolLocked;       // Which spell school is locked
    std::string interruptSpellId;   // What spell caused interrupt
};

// ============================================================================
// Cast Event Data
// ============================================================================

/**
 * @brief Data passed to cast event callbacks
 */
struct CastEventData {
    const SpellDefinition* spell = nullptr;
    SpellInstance* instance = nullptr;
    uint32_t casterId = 0;
    uint32_t targetId = 0;
    glm::vec3 targetPosition{0.0f};
    glm::vec3 targetDirection{0.0f};
    float castTime = 0.0f;
    float elapsed = 0.0f;
    bool wasCrit = false;
    float amount = 0.0f;
    CastFailReason failReason = CastFailReason::None;
    InterruptInfo interruptInfo;
};

// ============================================================================
// Spell Caster Component
// ============================================================================

/**
 * @brief Entity component that enables spell casting
 *
 * This component manages spell slots, cooldowns, cast state,
 * and handles the casting flow for an entity.
 */
class SpellCaster {
public:
    SpellCaster(uint32_t entityId);
    ~SpellCaster();

    // Disable copy, allow move
    SpellCaster(const SpellCaster&) = delete;
    SpellCaster& operator=(const SpellCaster&) = delete;
    SpellCaster(SpellCaster&&) noexcept = default;
    SpellCaster& operator=(SpellCaster&&) noexcept = default;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the spell caster
     * @param maxSlots Maximum number of spell slots
     */
    void Initialize(int maxSlots = 10);

    /**
     * @brief Set reference to spell manager
     */
    void SetSpellManager(SpellManager* manager) { m_spellManager = manager; }

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update the caster each frame
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    // =========================================================================
    // Spell Slot Management
    // =========================================================================

    /**
     * @brief Assign a spell to a slot
     * @param slotIndex Slot to assign to
     * @param spellId ID of spell to assign
     * @return true if successful
     */
    bool AssignSpell(int slotIndex, const std::string& spellId);

    /**
     * @brief Clear a spell slot
     */
    void ClearSlot(int slotIndex);

    /**
     * @brief Get spell in a slot
     */
    std::optional<SpellSlot> GetSlot(int slotIndex) const;

    /**
     * @brief Get all spell slots
     */
    const std::vector<SpellSlot>& GetSlots() const { return m_slots; }

    /**
     * @brief Find slot containing a spell
     * @return Slot index or -1 if not found
     */
    int FindSpellSlot(const std::string& spellId) const;

    /**
     * @brief Learn a spell (makes it available)
     */
    void LearnSpell(const std::string& spellId);

    /**
     * @brief Unlearn a spell
     */
    void UnlearnSpell(const std::string& spellId);

    /**
     * @brief Check if a spell is known
     */
    bool KnowsSpell(const std::string& spellId) const;

    // =========================================================================
    // Casting
    // =========================================================================

    /**
     * @brief Start casting a spell by slot
     * @param slotIndex Slot to cast from
     * @param targetId Target entity (or 0 for self/ground target)
     * @param targetPosition Target position for ground-targeted spells
     * @param targetDirection Direction for directional spells
     * @return Cast result
     */
    struct CastResult {
        bool success = false;
        CastFailReason failReason = CastFailReason::None;
        std::string failMessage;
        SpellInstance* instance = nullptr;
    };

    CastResult CastSpell(
        int slotIndex,
        uint32_t targetId = 0,
        const glm::vec3& targetPosition = glm::vec3(0.0f),
        const glm::vec3& targetDirection = glm::vec3(0.0f, 0.0f, 1.0f)
    );

    /**
     * @brief Start casting a spell by ID
     */
    CastResult CastSpellById(
        const std::string& spellId,
        uint32_t targetId = 0,
        const glm::vec3& targetPosition = glm::vec3(0.0f),
        const glm::vec3& targetDirection = glm::vec3(0.0f, 0.0f, 1.0f)
    );

    /**
     * @brief Check if a spell can be cast
     */
    CastResult CanCastSpell(
        const std::string& spellId,
        uint32_t targetId,
        const glm::vec3& targetPosition
    ) const;

    /**
     * @brief Cancel the current cast
     */
    void CancelCast();

    /**
     * @brief Interrupt the current cast
     * @param info Information about the interrupt
     */
    void InterruptCast(const InterruptInfo& info);

    // =========================================================================
    // Cooldown Management
    // =========================================================================

    /**
     * @brief Start cooldown for a spell
     */
    void StartCooldown(const std::string& spellId, float duration);

    /**
     * @brief Get remaining cooldown for a spell
     */
    float GetCooldownRemaining(const std::string& spellId) const;

    /**
     * @brief Check if spell is on cooldown
     */
    bool IsOnCooldown(const std::string& spellId) const;

    /**
     * @brief Reset cooldown for a spell
     */
    void ResetCooldown(const std::string& spellId);

    /**
     * @brief Reset all cooldowns
     */
    void ResetAllCooldowns();

    /**
     * @brief Reduce cooldown by amount
     */
    void ReduceCooldown(const std::string& spellId, float amount);

    /**
     * @brief Apply cooldown reduction multiplier (1.0 = no change, 0.5 = half cooldowns)
     */
    void SetCooldownMultiplier(float multiplier) { m_cooldownMultiplier = multiplier; }

    // =========================================================================
    // Global Cooldown
    // =========================================================================

    /**
     * @brief Start the global cooldown
     */
    void StartGlobalCooldown(float duration);

    /**
     * @brief Get remaining GCD
     */
    float GetGlobalCooldownRemaining() const { return m_gcdRemaining; }

    /**
     * @brief Check if on GCD
     */
    bool IsOnGlobalCooldown() const { return m_gcdRemaining > 0.0f; }

    // =========================================================================
    // State Queries
    // =========================================================================

    [[nodiscard]] CastState GetState() const { return m_state; }
    [[nodiscard]] bool IsCasting() const { return m_state == CastState::Casting; }
    [[nodiscard]] bool IsChanneling() const { return m_state == CastState::Channeling; }
    [[nodiscard]] bool IsIdle() const { return m_state == CastState::Idle; }
    [[nodiscard]] bool CanCast() const;

    [[nodiscard]] uint32_t GetEntityId() const { return m_entityId; }
    [[nodiscard]] const SpellInstance* GetCurrentCast() const { return m_currentCast.get(); }
    [[nodiscard]] float GetCastProgress() const;
    [[nodiscard]] float GetChannelProgress() const;

    // =========================================================================
    // School Lockouts
    // =========================================================================

    /**
     * @brief Lock a spell school (from interrupt)
     */
    void LockSchool(const std::string& school, float duration);

    /**
     * @brief Check if a school is locked
     */
    bool IsSchoolLocked(const std::string& school) const;

    /**
     * @brief Get school lockout remaining
     */
    float GetSchoolLockout(const std::string& school) const;

    // =========================================================================
    // Active Effects
    // =========================================================================

    /**
     * @brief Add an active effect to the caster
     */
    void AddActiveEffect(std::unique_ptr<ActiveEffect> effect);

    /**
     * @brief Remove an active effect
     */
    void RemoveActiveEffect(const std::string& effectId);

    /**
     * @brief Check if caster has an effect
     */
    bool HasActiveEffect(const std::string& effectId) const;

    /**
     * @brief Get active effect by ID
     */
    const ActiveEffect* GetActiveEffect(const std::string& effectId) const;

    /**
     * @brief Get all active effects
     */
    const std::vector<std::unique_ptr<ActiveEffect>>& GetActiveEffects() const { return m_activeEffects; }

    /**
     * @brief Dispel effects matching criteria
     * @return Number of effects dispelled
     */
    int DispelEffects(bool buffs, bool debuffs, int maxDispel);

    // =========================================================================
    // Event Callbacks
    // =========================================================================

    using CastEventCallback = std::function<void(const CastEventData&)>;

    void SetOnCastStart(CastEventCallback callback) { m_onCastStart = std::move(callback); }
    void SetOnCastComplete(CastEventCallback callback) { m_onCastComplete = std::move(callback); }
    void SetOnCastInterrupt(CastEventCallback callback) { m_onCastInterrupt = std::move(callback); }
    void SetOnSpellHit(CastEventCallback callback) { m_onSpellHit = std::move(callback); }
    void SetOnSpellMiss(CastEventCallback callback) { m_onSpellMiss = std::move(callback); }
    void SetOnCooldownStart(CastEventCallback callback) { m_onCooldownStart = std::move(callback); }

    // =========================================================================
    // Resource Queries (set externally)
    // =========================================================================

    using ResourceQueryFunc = std::function<float()>;
    using ResourceConsumeFunc = std::function<bool(float amount)>;

    void SetManaQuery(ResourceQueryFunc query, ResourceConsumeFunc consume) {
        m_getMana = std::move(query);
        m_consumeMana = std::move(consume);
    }

    void SetHealthQuery(ResourceQueryFunc query, ResourceConsumeFunc consume) {
        m_getHealth = std::move(query);
        m_consumeHealth = std::move(consume);
    }

    void SetStaminaQuery(ResourceQueryFunc query, ResourceConsumeFunc consume) {
        m_getStamina = std::move(query);
        m_consumeStamina = std::move(consume);
    }

    void SetCustomResourceQuery(
        const std::string& name,
        ResourceQueryFunc query,
        ResourceConsumeFunc consume
    ) {
        m_customResourceQueries[name] = std::move(query);
        m_customResourceConsume[name] = std::move(consume);
    }

    // =========================================================================
    // Position/Facing Queries (set externally)
    // =========================================================================

    using PositionQueryFunc = std::function<glm::vec3()>;
    using FacingQueryFunc = std::function<glm::vec3()>;
    using MovingQueryFunc = std::function<bool()>;

    void SetPositionQuery(PositionQueryFunc query) { m_getPosition = std::move(query); }
    void SetFacingQuery(FacingQueryFunc query) { m_getFacing = std::move(query); }
    void SetMovingQuery(MovingQueryFunc query) { m_isMoving = std::move(query); }

    // =========================================================================
    // Status Queries (set externally)
    // =========================================================================

    using StatusQueryFunc = std::function<bool()>;

    void SetSilencedQuery(StatusQueryFunc query) { m_isSilenced = std::move(query); }
    void SetStunnedQuery(StatusQueryFunc query) { m_isStunned = std::move(query); }
    void SetDeadQuery(StatusQueryFunc query) { m_isDead = std::move(query); }

private:
    // Internal methods
    void UpdateCasting(float deltaTime);
    void UpdateChanneling(float deltaTime);
    void UpdateCooldowns(float deltaTime);
    void UpdateActiveEffects(float deltaTime);
    void UpdateSchoolLockouts(float deltaTime);

    bool ValidateTarget(const SpellDefinition* spell, uint32_t targetId, const glm::vec3& targetPos) const;
    bool ConsumeResources(const SpellCost& cost);
    bool HasResources(const SpellCost& cost) const;
    bool CheckRequirements(const SpellRequirements& reqs) const;

    void CompleteCast();
    void FailCast(CastFailReason reason);

    void FireEvent(CastEventCallback& callback, const CastEventData& data);

    // Identity
    uint32_t m_entityId = 0;

    // State
    CastState m_state = CastState::Idle;
    std::unique_ptr<SpellInstance> m_currentCast;

    // Spell slots
    std::vector<SpellSlot> m_slots;
    std::unordered_set<std::string> m_knownSpells;

    // Cooldowns
    std::unordered_map<std::string, float> m_cooldowns;
    float m_gcdRemaining = 0.0f;
    float m_cooldownMultiplier = 1.0f;

    // School lockouts
    std::unordered_map<std::string, float> m_schoolLockouts;

    // Active effects (buffs/debuffs)
    std::vector<std::unique_ptr<ActiveEffect>> m_activeEffects;

    // Event callbacks
    CastEventCallback m_onCastStart;
    CastEventCallback m_onCastComplete;
    CastEventCallback m_onCastInterrupt;
    CastEventCallback m_onSpellHit;
    CastEventCallback m_onSpellMiss;
    CastEventCallback m_onCooldownStart;

    // Resource queries
    ResourceQueryFunc m_getMana;
    ResourceConsumeFunc m_consumeMana;
    ResourceQueryFunc m_getHealth;
    ResourceConsumeFunc m_consumeHealth;
    ResourceQueryFunc m_getStamina;
    ResourceConsumeFunc m_consumeStamina;
    std::unordered_map<std::string, ResourceQueryFunc> m_customResourceQueries;
    std::unordered_map<std::string, ResourceConsumeFunc> m_customResourceConsume;

    // Position/facing queries
    PositionQueryFunc m_getPosition;
    FacingQueryFunc m_getFacing;
    MovingQueryFunc m_isMoving;

    // Status queries
    StatusQueryFunc m_isSilenced;
    StatusQueryFunc m_isStunned;
    StatusQueryFunc m_isDead;

    // External reference
    SpellManager* m_spellManager = nullptr;

    // Configuration
    int m_maxSlots = 10;
    bool m_initialized = false;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Convert CastState to string
 */
const char* CastStateToString(CastState state);

/**
 * @brief Convert CastFailReason to string
 */
const char* CastFailReasonToString(CastFailReason reason);

/**
 * @brief Get user-friendly message for cast failure
 */
std::string GetCastFailMessage(CastFailReason reason);

} // namespace Spells
} // namespace Vehement
