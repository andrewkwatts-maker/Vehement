#pragma once

#include "../lifecycle/ILifecycle.hpp"
#include "../lifecycle/GameEvent.hpp"
#include "../lifecycle/ComponentLifecycle.hpp"
#include "../lifecycle/ScriptedLifecycle.hpp"
#include <glm/glm.hpp>
#include <string>
#include <functional>
#include <vector>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// Effect Type
// ============================================================================

/**
 * @brief Effect classification
 */
enum class EffectType : uint8_t {
    Visual,         // Pure visual (particles, etc)
    Buff,           // Positive stat modifier
    Debuff,         // Negative stat modifier
    Aura,           // Area effect
    DOT,            // Damage over time
    HOT,            // Heal over time
    Shield,         // Damage absorption
    Stun,           // Prevents action
    Slow,           // Movement reduction
    Root,           // Prevents movement
    Silence,        // Prevents abilities
    Knockback,      // Movement displacement

    Custom = 255
};

inline const char* EffectTypeToString(EffectType type) {
    switch (type) {
        case EffectType::Visual:    return "Visual";
        case EffectType::Buff:      return "Buff";
        case EffectType::Debuff:    return "Debuff";
        case EffectType::Aura:      return "Aura";
        case EffectType::DOT:       return "DOT";
        case EffectType::HOT:       return "HOT";
        case EffectType::Shield:    return "Shield";
        case EffectType::Stun:      return "Stun";
        case EffectType::Slow:      return "Slow";
        case EffectType::Root:      return "Root";
        case EffectType::Silence:   return "Silence";
        case EffectType::Knockback: return "Knockback";
        case EffectType::Custom:    return "Custom";
        default:                    return "Unknown";
    }
}

// ============================================================================
// Effect Stacking
// ============================================================================

/**
 * @brief How effects stack
 */
enum class EffectStacking : uint8_t {
    None,           // Only one instance allowed
    Duration,       // Refresh duration
    Intensity,      // Stack intensity
    Independent     // Each instance independent
};

// ============================================================================
// Effect Stats
// ============================================================================

/**
 * @brief Effect statistics and modifiers
 */
struct EffectStats {
    // Duration
    float duration = 5.0f;          // Total duration (-1 for infinite)
    float tickInterval = 0.5f;      // Time between ticks
    int maxStacks = 1;

    // Modifiers (multiplicative)
    float damageMultiplier = 1.0f;
    float speedMultiplier = 1.0f;
    float armorMultiplier = 1.0f;
    float attackSpeedMultiplier = 1.0f;

    // Flat modifiers
    float damageBonus = 0.0f;
    float speedBonus = 0.0f;
    float armorBonus = 0.0f;
    float healthBonus = 0.0f;

    // DOT/HOT
    float damagePerTick = 0.0f;
    float healPerTick = 0.0f;
    std::string damageType = "magic";

    // Shield
    float shieldAmount = 0.0f;

    // Aura
    float auraRadius = 0.0f;
    bool affectsAllies = true;
    bool affectsEnemies = false;

    // Visual
    std::string particleEffect;
    std::string soundEffect;
    glm::vec4 tintColor{1.0f};
    float scale = 1.0f;
};

// ============================================================================
// Effect Instance Data
// ============================================================================

/**
 * @brief Runtime instance data for applied effect
 */
struct EffectInstance {
    LifecycleHandle effectHandle;
    LifecycleHandle sourceHandle;   // Who applied it
    LifecycleHandle targetHandle;   // Who has it

    float remainingDuration = 0.0f;
    float tickTimer = 0.0f;
    int currentStacks = 1;

    bool isPermanent = false;
    bool isExpired = false;
};

// ============================================================================
// BaseEffect
// ============================================================================

/**
 * @brief Base class for all visual and gameplay effects
 *
 * Provides:
 * - Timed effects with duration
 * - Stat modifiers (buffs/debuffs)
 * - Damage/heal over time
 * - Aura effects
 * - Visual effects integration
 * - Script hooks
 *
 * JSON Config:
 * {
 *   "id": "effect_burning",
 *   "type": "effect",
 *   "effect_type": "DOT",
 *   "stats": {
 *     "duration": 5,
 *     "tick_interval": 1,
 *     "damage_per_tick": 10,
 *     "damage_type": "fire"
 *   },
 *   "visual": {
 *     "particle": "fire_particles",
 *     "tint": [1, 0.5, 0, 1]
 *   }
 * }
 */
class BaseEffect : public ScriptedLifecycle {
public:
    BaseEffect();
    ~BaseEffect() override;

    // =========================================================================
    // ILifecycle Implementation
    // =========================================================================

    void OnCreate(const nlohmann::json& config) override;
    void OnTick(float deltaTime) override;
    bool OnEvent(const GameEvent& event) override;
    void OnDestroy() override;

    [[nodiscard]] const char* GetTypeName() const override { return "BaseEffect"; }

    // =========================================================================
    // Effect Properties
    // =========================================================================

    [[nodiscard]] EffectType GetEffectType() const { return m_effectType; }
    void SetEffectType(EffectType type) { m_effectType = type; }

    [[nodiscard]] EffectStacking GetStacking() const { return m_stacking; }
    void SetStacking(EffectStacking stacking) { m_stacking = stacking; }

    [[nodiscard]] EffectStats& GetStats() { return m_stats; }
    [[nodiscard]] const EffectStats& GetStats() const { return m_stats; }

    [[nodiscard]] const std::string& GetEffectId() const { return m_effectId; }
    void SetEffectId(const std::string& id) { m_effectId = id; }

    // =========================================================================
    // Application
    // =========================================================================

    /**
     * @brief Apply effect to target
     */
    void Apply(LifecycleHandle target, LifecycleHandle source = LifecycleHandle::Invalid);

    /**
     * @brief Remove effect
     */
    void Remove();

    /**
     * @brief Refresh duration
     */
    void Refresh();

    /**
     * @brief Add stacks
     */
    void AddStacks(int count);

    /**
     * @brief Remove stacks
     */
    void RemoveStacks(int count);

    // =========================================================================
    // State Access
    // =========================================================================

    [[nodiscard]] LifecycleHandle GetTarget() const { return m_target; }
    [[nodiscard]] LifecycleHandle GetSource() const { return m_source; }

    [[nodiscard]] float GetRemainingDuration() const { return m_remainingDuration; }
    [[nodiscard]] float GetProgress() const;

    [[nodiscard]] int GetCurrentStacks() const { return m_currentStacks; }
    [[nodiscard]] bool IsActive() const { return m_isActive; }
    [[nodiscard]] bool IsExpired() const { return m_isExpired; }

    // =========================================================================
    // Position (for positional effects)
    // =========================================================================

    [[nodiscard]] const glm::vec3& GetPosition() const { return m_position; }
    void SetPosition(const glm::vec3& pos) { m_position = pos; }

    [[nodiscard]] bool IsAttachedToTarget() const { return m_attachedToTarget; }
    void SetAttachedToTarget(bool attached) { m_attachedToTarget = attached; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    using ApplyCallback = std::function<void(LifecycleHandle target)>;
    void SetOnApply(ApplyCallback callback) { m_onApply = std::move(callback); }

    using RemoveCallback = std::function<void(LifecycleHandle target)>;
    void SetOnRemove(RemoveCallback callback) { m_onRemove = std::move(callback); }

    using TickCallback = std::function<void(LifecycleHandle target, int tickNum)>;
    void SetOnTick(TickCallback callback) { m_onTick = std::move(callback); }

    // =========================================================================
    // Stat Modifier Calculation
    // =========================================================================

    /**
     * @brief Calculate total modifier for a stat
     */
    [[nodiscard]] float GetDamageMultiplier() const;
    [[nodiscard]] float GetSpeedMultiplier() const;
    [[nodiscard]] float GetArmorMultiplier() const;

    // =========================================================================
    // Script Context Override
    // =========================================================================

    [[nodiscard]] ScriptContext BuildContext() const override;

protected:
    virtual void ParseEffectConfig(const nlohmann::json& config);

    // Lifecycle hooks
    virtual void OnApplied();
    virtual void OnRemoved();
    virtual void OnEffectTick();
    virtual void OnExpired();
    virtual void OnStacksChanged(int oldStacks, int newStacks);

    // Effect application
    void ApplyModifiers();
    void RemoveModifiers();
    void ProcessTick();

    // Aura handling
    void UpdateAura(float deltaTime);
    void ApplyAuraToTargets();

    EffectType m_effectType = EffectType::Visual;
    EffectStacking m_stacking = EffectStacking::Duration;
    EffectStats m_stats;

    std::string m_effectId;

    LifecycleHandle m_target = LifecycleHandle::Invalid;
    LifecycleHandle m_source = LifecycleHandle::Invalid;

    glm::vec3 m_position{0.0f};
    bool m_attachedToTarget = true;

    float m_remainingDuration = 0.0f;
    float m_tickTimer = 0.0f;
    int m_tickCount = 0;
    int m_currentStacks = 1;

    bool m_isActive = false;
    bool m_isExpired = false;

    // Shield tracking
    float m_currentShield = 0.0f;

    // Callbacks
    ApplyCallback m_onApply;
    RemoveCallback m_onRemove;
    TickCallback m_onTick;

    // Aura targets
    std::vector<LifecycleHandle> m_auraTargets;
    float m_auraUpdateTimer = 0.0f;

    ComponentContainer m_components;
};

// ============================================================================
// Effect Manager Helper
// ============================================================================

/**
 * @brief Helper for managing effects on an entity
 */
class EffectManager {
public:
    /**
     * @brief Apply effect to target
     */
    static LifecycleHandle ApplyEffect(const std::string& effectId,
                                       LifecycleHandle target,
                                       LifecycleHandle source = LifecycleHandle::Invalid);

    /**
     * @brief Remove effect from target
     */
    static void RemoveEffect(LifecycleHandle target, const std::string& effectId);

    /**
     * @brief Remove all effects from target
     */
    static void RemoveAllEffects(LifecycleHandle target);

    /**
     * @brief Check if target has effect
     */
    static bool HasEffect(LifecycleHandle target, const std::string& effectId);

    /**
     * @brief Get all effects on target
     */
    static std::vector<BaseEffect*> GetEffects(LifecycleHandle target);

    /**
     * @brief Calculate combined stat modifiers from all effects
     */
    static float GetCombinedDamageMultiplier(LifecycleHandle target);
    static float GetCombinedSpeedMultiplier(LifecycleHandle target);
    static float GetCombinedArmorMultiplier(LifecycleHandle target);
};

} // namespace Lifecycle
} // namespace Vehement
