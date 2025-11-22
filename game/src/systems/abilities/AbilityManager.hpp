#pragma once

#include "AbilityDefinition.hpp"
#include "AbilityInstance.hpp"

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <queue>
#include <glm/glm.hpp>

namespace Vehement {

// Forward declarations
class Entity;

namespace Abilities {

// Forward declarations
class AbilityDefinition;
class AbilityInstance;

// ============================================================================
// Ability Events
// ============================================================================

/**
 * @brief Event types for ability system
 */
enum class AbilityEventType : uint8_t {
    CastStart,
    CastComplete,
    CastFailed,
    Channeling,
    ChannelInterrupt,
    ChannelComplete,
    Hit,
    Miss,
    Kill,
    Cooldown,
    CooldownComplete,
    LevelUp,
    Toggle,
    ChargeUsed,
    ChargeRestored
};

/**
 * @brief Ability event data
 */
struct AbilityEvent {
    AbilityEventType type = AbilityEventType::CastStart;
    uint32_t casterId = 0;
    uint32_t targetId = 0;
    std::string abilityId;
    int abilityLevel = 0;
    glm::vec3 position{0.0f};
    float value = 0.0f;         // Damage, healing, etc.
    float gameTime = 0.0f;
};

// ============================================================================
// Cast Validation
// ============================================================================

/**
 * @brief Reasons why a cast might fail
 */
enum class CastFailReason : uint8_t {
    None,
    NotLearned,
    OnCooldown,
    NotEnoughMana,
    NotEnoughHealth,
    NoCharges,
    Silenced,
    Stunned,
    Rooted,            // For movement abilities
    OutOfRange,
    InvalidTarget,
    NoTarget,
    Channeling,
    Dead,
    Disabled,
    Custom
};

/**
 * @brief Result of cast validation
 */
struct CastValidation {
    bool canCast = false;
    CastFailReason reason = CastFailReason::None;
    std::string customReason;

    static CastValidation Success() {
        return {true, CastFailReason::None, ""};
    }

    static CastValidation Failure(CastFailReason reason) {
        return {false, reason, ""};
    }

    static CastValidation CustomFailure(const std::string& message) {
        return {false, CastFailReason::Custom, message};
    }
};

// ============================================================================
// Targeting Resolution
// ============================================================================

/**
 * @brief Resolved targeting data for ability execution
 */
struct ResolvedTarget {
    bool valid = false;
    std::vector<uint32_t> targets;  // Entity IDs
    glm::vec3 point{0.0f};
    glm::vec3 direction{0.0f, 0.0f, 1.0f};
    float effectRadius = 0.0f;
};

// ============================================================================
// Ability Execution Context
// ============================================================================

/**
 * @brief Full context for ability execution
 */
struct AbilityExecutionContext {
    AbilityInstance* ability = nullptr;
    uint32_t casterId = 0;
    Entity* casterEntity = nullptr;
    ResolvedTarget targets;
    AbilityCastContext castContext;
    float deltaTime = 0.0f;
};

// ============================================================================
// Effect Application
// ============================================================================

/**
 * @brief Effect to apply from ability
 */
struct AbilityEffect {
    enum class Type {
        Damage,
        Heal,
        Buff,
        Debuff,
        Stun,
        Slow,
        Silence,
        Root,
        Knockback,
        Teleport,
        Summon,
        Dispel,
        Shield,
        Lifesteal,
        ManaBurn,
        ManaRestore,
        Custom
    };

    Type type = Type::Damage;
    float value = 0.0f;
    float duration = 0.0f;
    DamageType damageType = DamageType::Magical;
    std::string customEffectId;

    // Source tracking
    uint32_t sourceId = 0;
    std::string sourceAbilityId;
    int sourceLevel = 0;
};

// ============================================================================
// Ability Manager
// ============================================================================

/**
 * @brief Central manager for ability execution
 *
 * Handles:
 * - Cast validation (mana, cooldown, silence, etc.)
 * - Targeting resolution
 * - Effect application
 * - Event callbacks
 */
class AbilityManager {
public:
    using EventCallback = std::function<void(const AbilityEvent&)>;
    using ValidateCallback = std::function<CastValidation(const AbilityCastContext&, const AbilityInstance&)>;
    using ExecuteCallback = std::function<AbilityCastResult(AbilityExecutionContext&)>;
    using EffectCallback = std::function<void(Entity*, const AbilityEffect&)>;

    static AbilityManager& Instance();

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize ability manager
     */
    void Initialize();

    /**
     * @brief Shutdown ability manager
     */
    void Shutdown();

    // =========================================================================
    // Cast Validation
    // =========================================================================

    /**
     * @brief Validate if ability can be cast
     * @param caster Entity casting
     * @param ability Ability instance
     * @param context Cast context (target, etc.)
     * @return Validation result
     */
    CastValidation ValidateCast(Entity* caster, AbilityInstance& ability,
                                const AbilityCastContext& context);

    /**
     * @brief Check if caster has enough mana
     */
    bool HasEnoughMana(Entity* caster, const AbilityInstance& ability) const;

    /**
     * @brief Check if ability is in range of target
     */
    bool IsInRange(Entity* caster, const AbilityCastContext& context,
                   const AbilityInstance& ability) const;

    /**
     * @brief Check if target is valid for ability
     */
    bool IsValidTarget(Entity* caster, Entity* target,
                       const AbilityDefinition& definition) const;

    // =========================================================================
    // Targeting Resolution
    // =========================================================================

    /**
     * @brief Resolve targets for ability
     * @param caster Entity casting
     * @param ability Ability instance
     * @param context Cast context
     * @return Resolved targets
     */
    ResolvedTarget ResolveTargets(Entity* caster, const AbilityInstance& ability,
                                  const AbilityCastContext& context);

    /**
     * @brief Find units in area
     */
    std::vector<uint32_t> FindUnitsInArea(const glm::vec3& center, float radius,
                                           TargetFlag flags, uint32_t casterId = 0);

    /**
     * @brief Find units in cone
     */
    std::vector<uint32_t> FindUnitsInCone(const glm::vec3& origin, const glm::vec3& direction,
                                           float angle, float range, TargetFlag flags);

    /**
     * @brief Find units in line
     */
    std::vector<uint32_t> FindUnitsInLine(const glm::vec3& start, const glm::vec3& end,
                                           float width, TargetFlag flags);

    // =========================================================================
    // Ability Execution
    // =========================================================================

    /**
     * @brief Cast ability
     * @param caster Entity casting
     * @param ability Ability instance
     * @param context Cast context
     * @return Cast result
     */
    AbilityCastResult CastAbility(Entity* caster, AbilityInstance& ability,
                                  const AbilityCastContext& context);

    /**
     * @brief Execute ability effects
     */
    void ExecuteAbility(AbilityExecutionContext& context);

    /**
     * @brief Process channeling tick
     */
    void ProcessChannel(Entity* caster, AbilityInstance& ability, float deltaTime);

    /**
     * @brief Cancel ability cast/channel
     */
    void CancelAbility(Entity* caster, AbilityInstance& ability);

    // =========================================================================
    // Effect Application
    // =========================================================================

    /**
     * @brief Apply effect to target
     */
    void ApplyEffect(Entity* target, const AbilityEffect& effect);

    /**
     * @brief Apply damage
     */
    float ApplyDamage(Entity* source, Entity* target, float damage, DamageType type,
                      const std::string& abilityId = "");

    /**
     * @brief Apply healing
     */
    float ApplyHealing(Entity* source, Entity* target, float amount,
                       const std::string& abilityId = "");

    /**
     * @brief Apply status effect
     */
    void ApplyStatusEffect(Entity* target, const std::string& effectId,
                           float duration, float strength, uint32_t sourceId);

    /**
     * @brief Remove status effect
     */
    void RemoveStatusEffect(Entity* target, const std::string& effectId);

    // =========================================================================
    // Event System
    // =========================================================================

    /**
     * @brief Register event callback
     */
    void RegisterEventCallback(AbilityEventType type, EventCallback callback);

    /**
     * @brief Fire ability event
     */
    void FireEvent(const AbilityEvent& event);

    /**
     * @brief Get pending events
     */
    std::vector<AbilityEvent> GetPendingEvents();

    /**
     * @brief Clear event queue
     */
    void ClearEvents();

    // =========================================================================
    // Custom Handlers
    // =========================================================================

    /**
     * @brief Register custom validation handler for ability
     */
    void RegisterValidationHandler(const std::string& abilityId, ValidateCallback callback);

    /**
     * @brief Register custom execution handler for ability
     */
    void RegisterExecutionHandler(const std::string& abilityId, ExecuteCallback callback);

    /**
     * @brief Register custom effect handler
     */
    void RegisterEffectHandler(AbilityEffect::Type type, EffectCallback callback);

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Get cooldown reduction for entity
     */
    float GetCooldownReduction(Entity* entity) const;

    /**
     * @brief Get mana cost reduction for entity
     */
    float GetManaCostReduction(Entity* entity) const;

    /**
     * @brief Get cast range bonus for entity
     */
    float GetCastRangeBonus(Entity* entity) const;

    /**
     * @brief Get spell amplification for entity
     */
    float GetSpellAmplification(Entity* entity) const;

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update ability manager
     */
    void Update(float deltaTime);

private:
    AbilityManager() = default;

    void ConsumeMana(Entity* caster, float amount);
    float CalculateFinalDamage(float baseDamage, DamageType type,
                                Entity* source, Entity* target);

    // Event callbacks by type
    std::unordered_map<AbilityEventType, std::vector<EventCallback>> m_eventCallbacks;

    // Custom handlers
    std::unordered_map<std::string, ValidateCallback> m_validateHandlers;
    std::unordered_map<std::string, ExecuteCallback> m_executeHandlers;
    std::unordered_map<AbilityEffect::Type, EffectCallback> m_effectHandlers;

    // Event queue
    std::queue<AbilityEvent> m_eventQueue;

    // Active channeling abilities
    struct ActiveChannel {
        Entity* caster = nullptr;
        AbilityInstance* ability = nullptr;
        AbilityCastContext context;
    };
    std::vector<ActiveChannel> m_activeChannels;

    bool m_initialized = false;
};

// ============================================================================
// Helper Functions
// ============================================================================

[[nodiscard]] std::string CastFailReasonToString(CastFailReason reason);
[[nodiscard]] std::string AbilityEventTypeToString(AbilityEventType type);

} // namespace Abilities
} // namespace Vehement
