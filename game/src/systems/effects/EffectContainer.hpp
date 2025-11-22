#pragma once

#include "EffectInstance.hpp"
#include "StatModifier.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <optional>

namespace Vehement {
namespace Effects {

// Forward declarations
class EffectManager;
class EffectDefinition;

// ============================================================================
// Effect Query Results
// ============================================================================

/**
 * @brief Result of effect query operations
 */
struct EffectQuery {
    std::vector<EffectInstance*> results;

    [[nodiscard]] bool Empty() const { return results.empty(); }
    [[nodiscard]] size_t Count() const { return results.size(); }
    [[nodiscard]] EffectInstance* First() const {
        return results.empty() ? nullptr : results.front();
    }
};

// ============================================================================
// Effect Application Result
// ============================================================================

/**
 * @brief Result of applying an effect
 */
struct EffectApplicationResult {
    enum class Status {
        Applied,        // New effect applied
        Refreshed,      // Existing effect refreshed
        Stacked,        // Added stack to existing
        Immune,         // Target is immune
        MaxStacks,      // Already at max stacks
        Failed          // Failed for other reason
    };

    Status status = Status::Failed;
    EffectInstance* instance = nullptr;
    std::string message;

    [[nodiscard]] bool Success() const {
        return status == Status::Applied ||
               status == Status::Refreshed ||
               status == Status::Stacked;
    }
};

// ============================================================================
// Effect Container
// ============================================================================

/**
 * @brief Component that holds all effects on an entity
 *
 * Manages active effects, calculates combined stat modifications,
 * tracks immunities, and handles effect lifecycle events.
 */
class EffectContainer {
public:
    EffectContainer();
    explicit EffectContainer(uint32_t ownerId);
    ~EffectContainer();

    // Non-copyable, movable
    EffectContainer(const EffectContainer&) = delete;
    EffectContainer& operator=(const EffectContainer&) = delete;
    EffectContainer(EffectContainer&&) noexcept = default;
    EffectContainer& operator=(EffectContainer&&) noexcept = default;

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    /**
     * @brief Set owner entity ID
     */
    void SetOwnerId(uint32_t id) { m_ownerId = id; }
    [[nodiscard]] uint32_t GetOwnerId() const { return m_ownerId; }

    /**
     * @brief Set effect manager reference for creating instances
     */
    void SetEffectManager(EffectManager* manager) { m_effectManager = manager; }

    // -------------------------------------------------------------------------
    // Effect Application
    // -------------------------------------------------------------------------

    /**
     * @brief Apply an effect to this container
     * @param definition Effect definition to apply
     * @param sourceId Who is applying the effect
     * @return Application result
     */
    EffectApplicationResult ApplyEffect(
        const EffectDefinition* definition,
        uint32_t sourceId
    );

    /**
     * @brief Apply an effect by ID
     */
    EffectApplicationResult ApplyEffect(
        const std::string& effectId,
        uint32_t sourceId
    );

    /**
     * @brief Apply a pre-created effect instance
     */
    EffectApplicationResult ApplyInstance(
        std::unique_ptr<EffectInstance> instance
    );

    // -------------------------------------------------------------------------
    // Effect Removal
    // -------------------------------------------------------------------------

    /**
     * @brief Remove a specific effect instance
     */
    bool RemoveEffect(EffectInstance::InstanceId instanceId);

    /**
     * @brief Remove all instances of an effect by definition ID
     */
    int RemoveEffectById(const std::string& effectId);

    /**
     * @brief Remove effects by source entity
     */
    int RemoveEffectsBySource(uint32_t sourceId);

    /**
     * @brief Remove effects by tag
     */
    int RemoveEffectsByTag(const std::string& tag);

    /**
     * @brief Remove effects by category
     */
    int RemoveEffectsByCategory(const std::string& category);

    /**
     * @brief Remove all buffs
     */
    int RemoveAllBuffs();

    /**
     * @brief Remove all debuffs
     */
    int RemoveAllDebuffs();

    /**
     * @brief Dispel effects (respects dispellable flag)
     */
    int DispelEffects(int maxDispels = -1, bool buffsOnly = false, bool debuffsOnly = false);

    /**
     * @brief Purge effects (respects purgeable flag)
     */
    int PurgeEffects(int maxPurges = -1);

    /**
     * @brief Clear all effects
     */
    void ClearAll();

    // -------------------------------------------------------------------------
    // Querying
    // -------------------------------------------------------------------------

    /**
     * @brief Get all active effects
     */
    [[nodiscard]] const std::vector<std::unique_ptr<EffectInstance>>& GetEffects() const {
        return m_effects;
    }

    /**
     * @brief Check if any effect with ID is active
     */
    [[nodiscard]] bool HasEffect(const std::string& effectId) const;

    /**
     * @brief Check if any effect with tag is active
     */
    [[nodiscard]] bool HasEffectWithTag(const std::string& tag) const;

    /**
     * @brief Get effect by instance ID
     */
    [[nodiscard]] EffectInstance* GetEffect(EffectInstance::InstanceId instanceId) const;

    /**
     * @brief Get first effect by definition ID
     */
    [[nodiscard]] EffectInstance* GetEffectById(const std::string& effectId) const;

    /**
     * @brief Query effects by type
     */
    [[nodiscard]] EffectQuery QueryByType(EffectType type) const;

    /**
     * @brief Query effects by tag
     */
    [[nodiscard]] EffectQuery QueryByTag(const std::string& tag) const;

    /**
     * @brief Query effects by source
     */
    [[nodiscard]] EffectQuery QueryBySource(uint32_t sourceId) const;

    /**
     * @brief Query effects matching predicate
     */
    [[nodiscard]] EffectQuery Query(
        std::function<bool(const EffectInstance*)> predicate
    ) const;

    /**
     * @brief Get total effect count
     */
    [[nodiscard]] size_t GetEffectCount() const { return m_effects.size(); }

    /**
     * @brief Get buff count
     */
    [[nodiscard]] size_t GetBuffCount() const;

    /**
     * @brief Get debuff count
     */
    [[nodiscard]] size_t GetDebuffCount() const;

    // -------------------------------------------------------------------------
    // Update
    // -------------------------------------------------------------------------

    /**
     * @brief Update all effects
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Process trigger events for all effects
     * @return Vector of fired triggers with their effect instances
     */
    std::vector<std::pair<EffectInstance*, const EffectTrigger*>> ProcessTriggers(
        const TriggerEventData& eventData
    );

    /**
     * @brief Reset combat state (for combat triggers)
     */
    void ResetCombat();

    // -------------------------------------------------------------------------
    // Stat Calculation
    // -------------------------------------------------------------------------

    /**
     * @brief Calculate final stat value with all modifiers applied
     * @param stat The stat to calculate
     * @param baseValue The base value before modifiers
     * @param context Context for conditional modifiers
     * @return Final modified value
     */
    [[nodiscard]] float CalculateStat(
        StatType stat,
        float baseValue,
        const std::unordered_map<std::string, float>& context = {}
    ) const;

    /**
     * @brief Get all active modifiers for a stat
     */
    [[nodiscard]] std::vector<StatModifier> GetModifiersForStat(StatType stat) const;

    /**
     * @brief Get all active modifiers from all effects
     */
    [[nodiscard]] std::vector<StatModifier> GetAllModifiers() const;

    /**
     * @brief Mark stat cache as dirty
     */
    void InvalidateStatCache();

    // -------------------------------------------------------------------------
    // Immunity
    // -------------------------------------------------------------------------

    /**
     * @brief Add immunity to an effect tag
     */
    void AddImmunity(const std::string& tag);

    /**
     * @brief Remove immunity to an effect tag
     */
    void RemoveImmunity(const std::string& tag);

    /**
     * @brief Check if immune to an effect tag
     */
    [[nodiscard]] bool IsImmuneTo(const std::string& tag) const;

    /**
     * @brief Check if immune to a specific effect
     */
    [[nodiscard]] bool IsImmuneToEffect(const EffectDefinition* definition) const;

    /**
     * @brief Get all current immunities
     */
    [[nodiscard]] const std::unordered_set<std::string>& GetImmunities() const {
        return m_immunities;
    }

    // -------------------------------------------------------------------------
    // Periodic Effects
    // -------------------------------------------------------------------------

    /**
     * @brief Consume and return all pending periodic effect ticks
     */
    std::vector<std::pair<EffectInstance*, PeriodicEffect>> ConsumePendingTicks();

    // -------------------------------------------------------------------------
    // Events
    // -------------------------------------------------------------------------

    using EffectEventCallback = std::function<void(EffectInstance*, const std::string&)>;

    void SetOnEffectApplied(EffectEventCallback callback) {
        m_onEffectApplied = std::move(callback);
    }

    void SetOnEffectRemoved(EffectEventCallback callback) {
        m_onEffectRemoved = std::move(callback);
    }

    void SetOnEffectExpired(EffectEventCallback callback) {
        m_onEffectExpired = std::move(callback);
    }

    // -------------------------------------------------------------------------
    // Serialization
    // -------------------------------------------------------------------------

    /**
     * @brief Serialize all effect states
     */
    std::string SerializeState() const;

    /**
     * @brief Restore effect states (requires effect manager)
     */
    bool DeserializeState(const std::string& data);

private:
    void CleanupExpiredEffects();
    void UpdateImmunities();
    EffectInstance* FindExistingEffect(const std::string& effectId, uint32_t sourceId);

    // Owner entity
    uint32_t m_ownerId = 0;

    // Effect manager for lookups
    EffectManager* m_effectManager = nullptr;

    // Active effects
    std::vector<std::unique_ptr<EffectInstance>> m_effects;

    // Index for quick lookup
    std::unordered_multimap<std::string, EffectInstance*> m_effectsByDefinitionId;

    // Immunities
    std::unordered_set<std::string> m_immunities;
    std::unordered_map<std::string, int> m_immunityRefCounts;  // From effect-granted immunities

    // Stat cache
    mutable bool m_statCacheDirty = true;
    mutable std::unordered_map<StatType, std::vector<StatModifier>> m_cachedModifiers;

    // Event callbacks
    EffectEventCallback m_onEffectApplied;
    EffectEventCallback m_onEffectRemoved;
    EffectEventCallback m_onEffectExpired;
};

} // namespace Effects
} // namespace Vehement
