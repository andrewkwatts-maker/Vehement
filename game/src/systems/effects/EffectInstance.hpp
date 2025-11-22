#pragma once

#include "EffectDefinition.hpp"
#include "EffectTrigger.hpp"
#include "StatModifier.hpp"
#include <memory>
#include <functional>
#include <cstdint>

namespace Vehement {
namespace Effects {

// Forward declarations
class EffectContainer;
class EffectManager;

// ============================================================================
// Effect Instance State
// ============================================================================

/**
 * @brief Current state of an effect instance
 */
enum class EffectState : uint8_t {
    Inactive,       // Not yet applied
    Active,         // Currently active
    Expiring,       // About to expire (for transition effects)
    Expired,        // Duration ended
    Removed,        // Forcibly removed
    Dispelled       // Removed by dispel
};

// ============================================================================
// Effect Instance
// ============================================================================

/**
 * @brief Runtime instance of an effect applied to an entity
 *
 * Created from an EffectDefinition when an effect is applied to a target.
 * Tracks duration, stacks, periodic ticks, and trigger state.
 */
class EffectInstance {
public:
    using InstanceId = uint32_t;
    static constexpr InstanceId INVALID_ID = 0;

    EffectInstance();
    explicit EffectInstance(const EffectDefinition* definition);
    ~EffectInstance();

    // Non-copyable, movable
    EffectInstance(const EffectInstance&) = delete;
    EffectInstance& operator=(const EffectInstance&) = delete;
    EffectInstance(EffectInstance&&) noexcept = default;
    EffectInstance& operator=(EffectInstance&&) noexcept = default;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize the instance from a definition
     */
    void Initialize(const EffectDefinition* definition);

    /**
     * @brief Called when effect is first applied
     */
    void OnApply(uint32_t sourceId, uint32_t targetId);

    /**
     * @brief Update the effect instance
     * @param deltaTime Time since last update in seconds
     */
    void Update(float deltaTime);

    /**
     * @brief Called when effect expires naturally
     */
    void OnExpire();

    /**
     * @brief Called when effect is forcibly removed
     */
    void OnRemove();

    /**
     * @brief Called when effect is dispelled
     */
    void OnDispel();

    /**
     * @brief Called when effect is refreshed (same effect reapplied)
     */
    void OnRefresh();

    /**
     * @brief Called when stacks are added
     */
    void OnStackAdded(int newStacks);

    // -------------------------------------------------------------------------
    // Identity
    // -------------------------------------------------------------------------

    [[nodiscard]] InstanceId GetId() const { return m_instanceId; }
    [[nodiscard]] const std::string& GetEffectId() const { return m_effectId; }
    [[nodiscard]] const EffectDefinition* GetDefinition() const { return m_definition; }
    [[nodiscard]] EffectState GetState() const { return m_state; }
    [[nodiscard]] bool IsActive() const { return m_state == EffectState::Active; }

    // -------------------------------------------------------------------------
    // Source and Target
    // -------------------------------------------------------------------------

    [[nodiscard]] uint32_t GetSourceId() const { return m_sourceId; }
    [[nodiscard]] uint32_t GetTargetId() const { return m_targetId; }

    void SetSourceId(uint32_t id) { m_sourceId = id; }
    void SetTargetId(uint32_t id) { m_targetId = id; }

    // -------------------------------------------------------------------------
    // Duration
    // -------------------------------------------------------------------------

    [[nodiscard]] float GetRemainingDuration() const { return m_remainingDuration; }
    [[nodiscard]] float GetElapsedTime() const { return m_elapsedTime; }
    [[nodiscard]] float GetTotalDuration() const { return m_totalDuration; }
    [[nodiscard]] float GetDurationPercent() const {
        return m_totalDuration > 0.0f ? m_remainingDuration / m_totalDuration : 0.0f;
    }

    [[nodiscard]] bool IsExpired() const {
        return m_state == EffectState::Expired ||
               m_state == EffectState::Removed ||
               m_state == EffectState::Dispelled;
    }

    [[nodiscard]] bool IsPermanent() const;

    void SetDuration(float duration);
    void ExtendDuration(float amount);
    void ReduceDuration(float amount);
    void RefreshDuration();

    // -------------------------------------------------------------------------
    // Stacks
    // -------------------------------------------------------------------------

    [[nodiscard]] int GetStacks() const { return m_stacks; }
    [[nodiscard]] int GetMaxStacks() const;

    void SetStacks(int stacks);
    void AddStacks(int amount);
    void RemoveStacks(int amount);

    // -------------------------------------------------------------------------
    // Charges
    // -------------------------------------------------------------------------

    [[nodiscard]] int GetCharges() const { return m_charges; }
    [[nodiscard]] int GetMaxCharges() const;

    void SetCharges(int charges);
    void ConsumeCharge();

    // -------------------------------------------------------------------------
    // Stat Modifiers
    // -------------------------------------------------------------------------

    /**
     * @brief Get all active stat modifiers from this effect
     * @return Vector of modifiers with proper source IDs set
     */
    [[nodiscard]] std::vector<StatModifier> GetActiveModifiers() const;

    /**
     * @brief Get effective modifier value considering stacks
     */
    [[nodiscard]] float GetModifiedValue(const StatModifier& baseMod) const;

    // -------------------------------------------------------------------------
    // Periodic Effects
    // -------------------------------------------------------------------------

    /**
     * @brief Check if any periodic effects are ready to tick
     */
    [[nodiscard]] bool HasPendingTicks() const;

    /**
     * @brief Get and reset pending tick data
     * @return Vector of periodic effects ready to apply
     */
    std::vector<PeriodicEffect> ConsumePendingTicks();

    // -------------------------------------------------------------------------
    // Triggers
    // -------------------------------------------------------------------------

    /**
     * @brief Process a trigger event
     * @return Triggers that fired
     */
    std::vector<const EffectTrigger*> ProcessTriggerEvent(
        const TriggerEventData& eventData
    );

    /**
     * @brief Reset combat trigger counts
     */
    void ResetCombatTriggers();

    // -------------------------------------------------------------------------
    // Event Callbacks
    // -------------------------------------------------------------------------

    using EffectCallback = std::function<void(EffectInstance*)>;

    void SetOnTick(EffectCallback callback) { m_onTick = std::move(callback); }
    void SetOnExpire(EffectCallback callback) { m_onExpire = std::move(callback); }
    void SetOnRemove(EffectCallback callback) { m_onRemove = std::move(callback); }

    // -------------------------------------------------------------------------
    // Flags
    // -------------------------------------------------------------------------

    [[nodiscard]] bool IsDispellable() const;
    [[nodiscard]] bool IsPurgeable() const;
    [[nodiscard]] bool IsHidden() const;
    [[nodiscard]] bool IsPersistent() const;
    [[nodiscard]] int GetPriority() const;

    // -------------------------------------------------------------------------
    // Custom Data
    // -------------------------------------------------------------------------

    /**
     * @brief Store custom runtime data
     */
    void SetCustomData(const std::string& key, float value);
    float GetCustomData(const std::string& key, float defaultVal = 0.0f) const;

    /**
     * @brief Get all custom data for script access
     */
    const std::unordered_map<std::string, float>& GetAllCustomData() const { return m_customData; }

    // -------------------------------------------------------------------------
    // Serialization
    // -------------------------------------------------------------------------

    /**
     * @brief Serialize state for save/load
     */
    std::string SerializeState() const;

    /**
     * @brief Restore state from serialized data
     */
    bool DeserializeState(const std::string& data);

private:
    void UpdateDuration(float deltaTime);
    void UpdatePeriodicEffects(float deltaTime);
    void CheckExpiration();

    // Identity
    InstanceId m_instanceId = INVALID_ID;
    std::string m_effectId;
    const EffectDefinition* m_definition = nullptr;
    EffectState m_state = EffectState::Inactive;

    // Source and target
    uint32_t m_sourceId = 0;
    uint32_t m_targetId = 0;

    // Duration tracking
    float m_remainingDuration = 0.0f;
    float m_totalDuration = 0.0f;
    float m_elapsedTime = 0.0f;

    // Stacking
    int m_stacks = 1;

    // Charges
    int m_charges = 1;

    // Periodic effect timers
    std::vector<float> m_periodicTimers;
    std::vector<bool> m_pendingTicks;

    // Trigger state (copies from definition for runtime modification)
    std::vector<EffectTrigger> m_triggers;

    // Event callbacks
    EffectCallback m_onTick;
    EffectCallback m_onExpire;
    EffectCallback m_onRemove;

    // Custom runtime data
    std::unordered_map<std::string, float> m_customData;

    // Instance ID generator
    static InstanceId s_nextInstanceId;

    friend class EffectManager;
    friend class EffectContainer;
};

// ============================================================================
// Effect Instance Pool
// ============================================================================

/**
 * @brief Object pool for effect instances to reduce allocations
 */
class EffectInstancePool {
public:
    EffectInstancePool(size_t initialSize = 64);
    ~EffectInstancePool();

    /**
     * @brief Acquire an effect instance from the pool
     */
    std::unique_ptr<EffectInstance> Acquire();

    /**
     * @brief Return an effect instance to the pool
     */
    void Release(std::unique_ptr<EffectInstance> instance);

    /**
     * @brief Get current pool statistics
     */
    size_t GetPooledCount() const { return m_pool.size(); }
    size_t GetActiveCount() const { return m_activeCount; }
    size_t GetTotalCreated() const { return m_totalCreated; }

private:
    std::vector<std::unique_ptr<EffectInstance>> m_pool;
    size_t m_activeCount = 0;
    size_t m_totalCreated = 0;
};

} // namespace Effects
} // namespace Vehement
