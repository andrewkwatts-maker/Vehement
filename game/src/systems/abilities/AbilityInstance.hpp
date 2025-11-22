#pragma once

#include "AbilityDefinition.hpp"

#include <string>
#include <memory>
#include <functional>
#include <optional>
#include <glm/glm.hpp>

namespace Vehement {
namespace Abilities {

// Forward declarations
class AbilityDefinition;

// ============================================================================
// Ability State
// ============================================================================

/**
 * @brief Current state of an ability
 */
enum class AbilityState : uint8_t {
    Ready,          // Can be cast
    OnCooldown,     // Waiting for cooldown
    Channeling,     // Currently channeling
    Active,         // Toggle is active
    Disabled,       // Cannot be used (silenced, etc.)
    NotLearned      // Not yet learned
};

// ============================================================================
// Cast Context
// ============================================================================

/**
 * @brief Context for ability casting
 */
struct AbilityCastContext {
    uint32_t casterId = 0;          // Entity casting the ability
    uint32_t targetId = 0;          // Target entity (if unit target)
    glm::vec3 targetPoint{0.0f};    // Target point (if point target)
    glm::vec3 direction{0.0f, 0.0f, 1.0f}; // Cast direction
    float castTime = 0.0f;          // Time spent casting
    int abilityLevel = 1;           // Current ability level
    bool isAutocast = false;        // Was this autocast
    float gameTime = 0.0f;          // Current game time
};

// ============================================================================
// Cast Result
// ============================================================================

/**
 * @brief Result of attempting to cast an ability
 */
struct AbilityCastResult {
    bool success = false;
    std::string failReason;

    // Stats from cast
    float damageDealt = 0.0f;
    float healingDone = 0.0f;
    int targetsHit = 0;
    std::vector<uint32_t> affectedEntities;

    // For debugging
    float actualCooldown = 0.0f;
    float actualManaCost = 0.0f;

    static AbilityCastResult Success() {
        AbilityCastResult result;
        result.success = true;
        return result;
    }

    static AbilityCastResult Failure(const std::string& reason) {
        AbilityCastResult result;
        result.success = false;
        result.failReason = reason;
        return result;
    }
};

// ============================================================================
// Ability Instance
// ============================================================================

/**
 * @brief Runtime state of an ability instance
 *
 * Represents a single instance of an ability on a hero:
 * - Current level
 * - Cooldown remaining
 * - Charges (for charge-based abilities)
 * - Toggle state
 * - Autocast state
 */
class AbilityInstance {
public:
    using CastCallback = std::function<void(AbilityInstance&, const AbilityCastResult&)>;
    using CooldownCallback = std::function<void(AbilityInstance&)>;
    using LevelCallback = std::function<void(AbilityInstance&, int newLevel)>;

    AbilityInstance();
    explicit AbilityInstance(const std::string& definitionId);
    explicit AbilityInstance(std::shared_ptr<AbilityDefinition> definition);
    ~AbilityInstance();

    // =========================================================================
    // Initialization
    // =========================================================================

    bool Initialize(const std::string& definitionId);
    bool Initialize(std::shared_ptr<AbilityDefinition> definition);
    void Reset();

    [[nodiscard]] std::shared_ptr<AbilityDefinition> GetDefinition() const { return m_definition; }
    [[nodiscard]] const std::string& GetDefinitionId() const { return m_definitionId; }

    // =========================================================================
    // Level
    // =========================================================================

    [[nodiscard]] int GetLevel() const noexcept { return m_currentLevel; }
    [[nodiscard]] int GetMaxLevel() const;
    [[nodiscard]] bool IsLearned() const { return m_currentLevel > 0; }
    [[nodiscard]] bool IsMaxLevel() const { return m_currentLevel >= GetMaxLevel(); }

    /**
     * @brief Level up the ability
     * @return true if leveled up successfully
     */
    bool LevelUp();

    /**
     * @brief Set ability level directly
     */
    void SetLevel(int level);

    // =========================================================================
    // State
    // =========================================================================

    [[nodiscard]] AbilityState GetState() const noexcept { return m_state; }
    [[nodiscard]] bool IsReady() const;
    [[nodiscard]] bool IsOnCooldown() const { return m_state == AbilityState::OnCooldown; }
    [[nodiscard]] bool IsChanneling() const { return m_state == AbilityState::Channeling; }
    [[nodiscard]] bool IsActive() const { return m_state == AbilityState::Active; }
    [[nodiscard]] bool IsDisabled() const { return m_state == AbilityState::Disabled; }

    void SetDisabled(bool disabled);

    // =========================================================================
    // Cooldown
    // =========================================================================

    [[nodiscard]] float GetCooldownRemaining() const noexcept { return m_cooldownRemaining; }
    [[nodiscard]] float GetCooldownTotal() const noexcept { return m_cooldownTotal; }
    [[nodiscard]] float GetCooldownPercent() const;

    /**
     * @brief Start cooldown
     * @param duration Cooldown duration (0 = use default from definition)
     */
    void StartCooldown(float duration = 0.0f);

    /**
     * @brief Reset cooldown to zero
     */
    void ResetCooldown();

    /**
     * @brief Reduce cooldown by amount
     */
    void ReduceCooldown(float amount);

    /**
     * @brief Refresh cooldown (restart from beginning)
     */
    void RefreshCooldown();

    // =========================================================================
    // Charges
    // =========================================================================

    [[nodiscard]] int GetCharges() const noexcept { return m_charges; }
    [[nodiscard]] int GetMaxCharges() const noexcept { return m_maxCharges; }
    [[nodiscard]] bool HasCharges() const { return m_maxCharges == 0 || m_charges > 0; }
    [[nodiscard]] float GetChargeRestoreTime() const noexcept { return m_chargeRestoreTimer; }

    /**
     * @brief Use a charge
     * @return true if charge was used
     */
    bool UseCharge();

    /**
     * @brief Add charges
     */
    void AddCharges(int count);

    /**
     * @brief Set charge count
     */
    void SetCharges(int count);

    // =========================================================================
    // Toggle
    // =========================================================================

    [[nodiscard]] bool IsToggled() const noexcept { return m_isToggled; }

    /**
     * @brief Toggle ability on/off
     * @return New toggle state
     */
    bool Toggle();

    /**
     * @brief Set toggle state directly
     */
    void SetToggled(bool toggled);

    // =========================================================================
    // Autocast
    // =========================================================================

    [[nodiscard]] bool IsAutocast() const noexcept { return m_isAutocast; }

    /**
     * @brief Toggle autocast
     * @return New autocast state
     */
    bool ToggleAutocast();

    /**
     * @brief Set autocast state
     */
    void SetAutocast(bool autocast);

    // =========================================================================
    // Channeling
    // =========================================================================

    [[nodiscard]] float GetChannelTimeRemaining() const noexcept { return m_channelTimeRemaining; }
    [[nodiscard]] float GetChannelDuration() const noexcept { return m_channelDuration; }
    [[nodiscard]] float GetChannelProgress() const;

    /**
     * @brief Start channeling
     */
    void StartChanneling(float duration);

    /**
     * @brief Interrupt channeling
     */
    void InterruptChannel();

    // =========================================================================
    // Current Level Data
    // =========================================================================

    /**
     * @brief Get data for current level
     */
    [[nodiscard]] const AbilityLevelData& GetCurrentLevelData() const;

    /**
     * @brief Get specific value for current level
     */
    [[nodiscard]] float GetDamage() const;
    [[nodiscard]] float GetHealing() const;
    [[nodiscard]] float GetManaCost() const;
    [[nodiscard]] float GetCooldown() const;
    [[nodiscard]] float GetDuration() const;
    [[nodiscard]] float GetCastRange() const;
    [[nodiscard]] float GetEffectRadius() const;

    // =========================================================================
    // Owner
    // =========================================================================

    [[nodiscard]] uint32_t GetOwnerId() const noexcept { return m_ownerId; }
    void SetOwnerId(uint32_t id) { m_ownerId = id; }

    [[nodiscard]] int GetSlot() const noexcept { return m_slot; }
    void SetSlot(int slot) { m_slot = slot; }

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update ability state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnCast(CastCallback callback) { m_onCast = std::move(callback); }
    void SetOnCooldownComplete(CooldownCallback callback) { m_onCooldownComplete = std::move(callback); }
    void SetOnLevelUp(LevelCallback callback) { m_onLevelUp = std::move(callback); }

    // =========================================================================
    // Serialization
    // =========================================================================

    [[nodiscard]] std::string ToJson() const;
    bool FromJson(const std::string& json);

private:
    void UpdateCooldown(float deltaTime);
    void UpdateCharges(float deltaTime);
    void UpdateChannel(float deltaTime);

    // Definition
    std::shared_ptr<AbilityDefinition> m_definition;
    std::string m_definitionId;

    // Owner
    uint32_t m_ownerId = 0;
    int m_slot = 0;

    // Level
    int m_currentLevel = 0;

    // State
    AbilityState m_state = AbilityState::NotLearned;

    // Cooldown
    float m_cooldownRemaining = 0.0f;
    float m_cooldownTotal = 0.0f;

    // Charges
    int m_charges = 0;
    int m_maxCharges = 0;
    float m_chargeRestoreTimer = 0.0f;
    float m_chargeRestoreTime = 0.0f;

    // Toggle
    bool m_isToggled = false;

    // Autocast
    bool m_isAutocast = false;

    // Channeling
    float m_channelTimeRemaining = 0.0f;
    float m_channelDuration = 0.0f;

    // Callbacks
    CastCallback m_onCast;
    CooldownCallback m_onCooldownComplete;
    LevelCallback m_onLevelUp;
};

} // namespace Abilities
} // namespace Vehement
