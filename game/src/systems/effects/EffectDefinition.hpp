#pragma once

#include "StatModifier.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <cstdint>

namespace Vehement {
namespace Effects {

// Forward declarations
class EffectInstance;
class EffectManager;
struct EffectTrigger;

// ============================================================================
// Effect Type
// ============================================================================

/**
 * @brief Primary classification of effects
 */
enum class EffectType : uint8_t {
    Buff,       // Positive effect on target
    Debuff,     // Negative effect on target
    Aura,       // Area effect emanating from source
    Passive,    // Always-active effect (e.g., from equipment)
    Triggered   // Effect that activates on certain conditions
};

/**
 * @brief Convert effect type to string
 */
const char* EffectTypeToString(EffectType type);

/**
 * @brief Parse effect type from string
 */
std::optional<EffectType> EffectTypeFromString(const std::string& str);

// ============================================================================
// Duration Type
// ============================================================================

/**
 * @brief How the effect's duration is tracked
 */
enum class DurationType : uint8_t {
    Permanent,  // Never expires (passive, equipment)
    Timed,      // Expires after duration seconds
    Charges,    // Expires after N uses
    Hybrid      // Expires after time OR charges, whichever first
};

/**
 * @brief Convert duration type to string
 */
const char* DurationTypeToString(DurationType type);

/**
 * @brief Parse duration type from string
 */
std::optional<DurationType> DurationTypeFromString(const std::string& str);

// ============================================================================
// Stacking Behavior
// ============================================================================

/**
 * @brief How multiple applications of the same effect interact
 */
enum class StackingMode : uint8_t {
    None,       // Cannot have multiple - refreshes existing
    Refresh,    // Refresh duration, keep same intensity
    Duration,   // Add to duration, keep same intensity
    Intensity,  // Keep duration, increase intensity/stacks
    Separate    // Each application is independent
};

/**
 * @brief Convert stacking mode to string
 */
const char* StackingModeToString(StackingMode mode);

/**
 * @brief Parse stacking mode from string
 */
std::optional<StackingMode> StackingModeFromString(const std::string& str);

// ============================================================================
// Damage Type
// ============================================================================

/**
 * @brief Type of damage for periodic effects
 */
enum class DamageType : uint8_t {
    Physical,
    Fire,
    Ice,
    Lightning,
    Poison,
    Holy,
    Dark,
    Arcane,
    Nature,
    True  // Ignores all resistance
};

/**
 * @brief Convert damage type to string
 */
const char* DamageTypeToString(DamageType type);

/**
 * @brief Parse damage type from string
 */
std::optional<DamageType> DamageTypeFromString(const std::string& str);

// ============================================================================
// Periodic Effect
// ============================================================================

/**
 * @brief Effect that occurs at regular intervals (DoT, HoT, etc.)
 */
struct PeriodicEffect {
    float interval = 1.0f;           // Seconds between ticks
    bool tickOnApply = false;        // Tick immediately when applied
    bool tickOnExpire = false;       // Tick when effect expires

    // Effect type
    enum class Type : uint8_t {
        Damage,
        Heal,
        Mana,
        Stamina,
        Custom
    } type = Type::Damage;

    // Values
    float amount = 10.0f;            // Base amount per tick
    float scaling = 0.0f;            // Scale with source stat (e.g., 0.5 = 50% of damage)
    StatType scalingStat = StatType::Damage;
    DamageType damageType = DamageType::Physical;

    // Custom script for complex periodic effects
    std::string scriptPath;
    std::string functionName;

    /**
     * @brief Load from JSON
     */
    bool LoadFromJson(const std::string& jsonStr);

    /**
     * @brief Serialize to JSON
     */
    std::string ToJson() const;
};

// ============================================================================
// Visual Indicator
// ============================================================================

/**
 * @brief Visual representation of an effect
 */
struct EffectVisual {
    std::string iconPath;            // Icon for UI
    std::string particlePath;        // Particle effect path
    std::string shaderOverride;      // Shader for visual modification
    glm::vec4 tint{1.0f};            // Color tint for entity
    float glowIntensity = 0.0f;      // Glow/outline intensity
    std::string attachPoint;         // Where to attach particles (e.g., "chest", "head")
    bool looping = true;             // Particle loops while active
    float scale = 1.0f;              // Particle scale

    /**
     * @brief Load from JSON
     */
    bool LoadFromJson(const std::string& jsonStr);

    /**
     * @brief Serialize to JSON
     */
    std::string ToJson() const;
};

// ============================================================================
// Effect Events
// ============================================================================

/**
 * @brief Script handlers for effect lifecycle events
 */
struct EffectEvents {
    std::string onApply;             // When effect is first applied
    std::string onRefresh;           // When effect is refreshed
    std::string onStack;             // When stacks are added
    std::string onTick;              // Each periodic tick
    std::string onExpire;            // When effect naturally expires
    std::string onRemove;            // When effect is forcibly removed
    std::string onDispel;            // When effect is dispelled

    /**
     * @brief Load from JSON
     */
    bool LoadFromJson(const std::string& jsonStr);

    /**
     * @brief Serialize to JSON
     */
    std::string ToJson() const;
};

// ============================================================================
// Stacking Configuration
// ============================================================================

/**
 * @brief Detailed stacking behavior configuration
 */
struct StackingConfig {
    StackingMode mode = StackingMode::Refresh;
    int maxStacks = 1;               // Maximum stack count
    float stackDurationBonus = 0.0f; // Duration added per stack (for Duration mode)
    float intensityPerStack = 1.0f;  // Multiplier per stack for stat mods
    bool separatePerSource = false;  // Each source has own stack

    /**
     * @brief Load from JSON
     */
    bool LoadFromJson(const std::string& jsonStr);

    /**
     * @brief Serialize to JSON
     */
    std::string ToJson() const;
};

// ============================================================================
// Effect Definition
// ============================================================================

/**
 * @brief Complete definition of an effect loaded from JSON
 *
 * This is the template/prototype for effects. EffectInstance is created
 * from this definition when applied to an entity.
 */
class EffectDefinition {
public:
    EffectDefinition();
    ~EffectDefinition() = default;

    // -------------------------------------------------------------------------
    // Loading
    // -------------------------------------------------------------------------

    /**
     * @brief Load definition from JSON file
     */
    bool LoadFromFile(const std::string& filePath);

    /**
     * @brief Load definition from JSON string
     */
    bool LoadFromJson(const std::string& jsonStr);

    /**
     * @brief Serialize to JSON string
     */
    std::string ToJson() const;

    /**
     * @brief Validate the definition
     * @return Vector of error messages (empty if valid)
     */
    std::vector<std::string> Validate() const;

    // -------------------------------------------------------------------------
    // Identity
    // -------------------------------------------------------------------------

    [[nodiscard]] const std::string& GetId() const { return m_id; }
    void SetId(const std::string& id) { m_id = id; }

    [[nodiscard]] const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    [[nodiscard]] const std::string& GetDescription() const { return m_description; }
    void SetDescription(const std::string& desc) { m_description = desc; }

    [[nodiscard]] EffectType GetType() const { return m_type; }
    void SetType(EffectType type) { m_type = type; }

    [[nodiscard]] const std::vector<std::string>& GetTags() const { return m_tags; }
    void AddTag(const std::string& tag) { m_tags.push_back(tag); }
    bool HasTag(const std::string& tag) const;

    // -------------------------------------------------------------------------
    // Duration
    // -------------------------------------------------------------------------

    [[nodiscard]] DurationType GetDurationType() const { return m_durationType; }
    void SetDurationType(DurationType type) { m_durationType = type; }

    [[nodiscard]] float GetBaseDuration() const { return m_baseDuration; }
    void SetBaseDuration(float duration) { m_baseDuration = duration; }

    [[nodiscard]] int GetMaxCharges() const { return m_maxCharges; }
    void SetMaxCharges(int charges) { m_maxCharges = charges; }

    // -------------------------------------------------------------------------
    // Stacking
    // -------------------------------------------------------------------------

    [[nodiscard]] const StackingConfig& GetStacking() const { return m_stacking; }
    void SetStacking(const StackingConfig& config) { m_stacking = config; }

    // -------------------------------------------------------------------------
    // Stat Modifiers
    // -------------------------------------------------------------------------

    [[nodiscard]] const std::vector<StatModifier>& GetModifiers() const { return m_modifiers; }
    void AddModifier(const StatModifier& modifier) { m_modifiers.push_back(modifier); }
    void ClearModifiers() { m_modifiers.clear(); }

    // -------------------------------------------------------------------------
    // Periodic Effects
    // -------------------------------------------------------------------------

    [[nodiscard]] const std::vector<PeriodicEffect>& GetPeriodicEffects() const { return m_periodicEffects; }
    void AddPeriodicEffect(const PeriodicEffect& effect) { m_periodicEffects.push_back(effect); }
    void ClearPeriodicEffects() { m_periodicEffects.clear(); }

    // -------------------------------------------------------------------------
    // Triggers
    // -------------------------------------------------------------------------

    [[nodiscard]] const std::vector<EffectTrigger>& GetTriggers() const { return m_triggers; }
    void AddTrigger(const EffectTrigger& trigger);
    void ClearTriggers() { m_triggers.clear(); }

    // -------------------------------------------------------------------------
    // Visuals
    // -------------------------------------------------------------------------

    [[nodiscard]] const EffectVisual& GetVisual() const { return m_visual; }
    void SetVisual(const EffectVisual& visual) { m_visual = visual; }

    // -------------------------------------------------------------------------
    // Events
    // -------------------------------------------------------------------------

    [[nodiscard]] const EffectEvents& GetEvents() const { return m_events; }
    void SetEvents(const EffectEvents& events) { m_events = events; }

    // -------------------------------------------------------------------------
    // Flags
    // -------------------------------------------------------------------------

    [[nodiscard]] bool IsDispellable() const { return m_dispellable; }
    void SetDispellable(bool dispellable) { m_dispellable = dispellable; }

    [[nodiscard]] bool IsPurgeable() const { return m_purgeable; }
    void SetPurgeable(bool purgeable) { m_purgeable = purgeable; }

    [[nodiscard]] bool IsHidden() const { return m_hidden; }
    void SetHidden(bool hidden) { m_hidden = hidden; }

    [[nodiscard]] bool IsPersistent() const { return m_persistent; }
    void SetPersistent(bool persistent) { m_persistent = persistent; }

    [[nodiscard]] int GetPriority() const { return m_priority; }
    void SetPriority(int priority) { m_priority = priority; }

    // -------------------------------------------------------------------------
    // Immunity
    // -------------------------------------------------------------------------

    [[nodiscard]] const std::vector<std::string>& GetImmunityTags() const { return m_immunityTags; }
    void AddImmunityTag(const std::string& tag) { m_immunityTags.push_back(tag); }

    // -------------------------------------------------------------------------
    // Categories for removal
    // -------------------------------------------------------------------------

    [[nodiscard]] const std::vector<std::string>& GetCategories() const { return m_categories; }
    void AddCategory(const std::string& cat) { m_categories.push_back(cat); }

    // -------------------------------------------------------------------------
    // Source Info
    // -------------------------------------------------------------------------

    [[nodiscard]] const std::string& GetSourcePath() const { return m_sourcePath; }
    [[nodiscard]] int64_t GetLastModified() const { return m_lastModified; }

private:
    // Identity
    std::string m_id;
    std::string m_name;
    std::string m_description;
    EffectType m_type = EffectType::Buff;
    std::vector<std::string> m_tags;
    std::vector<std::string> m_categories;

    // Duration
    DurationType m_durationType = DurationType::Timed;
    float m_baseDuration = 10.0f;
    int m_maxCharges = 1;

    // Stacking
    StackingConfig m_stacking;

    // Modifiers
    std::vector<StatModifier> m_modifiers;

    // Periodic
    std::vector<PeriodicEffect> m_periodicEffects;

    // Triggers
    std::vector<EffectTrigger> m_triggers;

    // Visual
    EffectVisual m_visual;

    // Events
    EffectEvents m_events;

    // Flags
    bool m_dispellable = true;
    bool m_purgeable = true;
    bool m_hidden = false;
    bool m_persistent = false;  // Survives death
    int m_priority = 0;         // Higher priority effects processed first

    // Immunity - effect grants immunity to these tags
    std::vector<std::string> m_immunityTags;

    // Source
    std::string m_sourcePath;
    int64_t m_lastModified = 0;
};

// ============================================================================
// Effect Definition Builder (Fluent API)
// ============================================================================

/**
 * @brief Builder for creating effect definitions programmatically
 */
class EffectDefinitionBuilder {
public:
    EffectDefinitionBuilder& Id(const std::string& id);
    EffectDefinitionBuilder& Name(const std::string& name);
    EffectDefinitionBuilder& Description(const std::string& desc);
    EffectDefinitionBuilder& Type(EffectType type);
    EffectDefinitionBuilder& Tag(const std::string& tag);
    EffectDefinitionBuilder& Duration(float seconds);
    EffectDefinitionBuilder& Permanent();
    EffectDefinitionBuilder& Charges(int count);
    EffectDefinitionBuilder& Stacking(StackingMode mode, int maxStacks = 1);
    EffectDefinitionBuilder& AddModifier(StatType stat, ModifierOp op, float value);
    EffectDefinitionBuilder& AddPeriodicDamage(float damage, float interval, DamageType type = DamageType::Physical);
    EffectDefinitionBuilder& AddPeriodicHeal(float amount, float interval);
    EffectDefinitionBuilder& Icon(const std::string& path);
    EffectDefinitionBuilder& Particle(const std::string& path);
    EffectDefinitionBuilder& Tint(const glm::vec4& color);
    EffectDefinitionBuilder& Dispellable(bool value = true);
    EffectDefinitionBuilder& Hidden(bool value = true);
    EffectDefinitionBuilder& Priority(int value);

    std::unique_ptr<EffectDefinition> Build();

private:
    std::unique_ptr<EffectDefinition> m_definition;
};

// Convenience function
inline EffectDefinitionBuilder DefineEffect() { return EffectDefinitionBuilder(); }

} // namespace Effects
} // namespace Vehement
