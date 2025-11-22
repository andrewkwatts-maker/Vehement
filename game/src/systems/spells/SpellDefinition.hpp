#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>
#include <functional>
#include <glm/glm.hpp>
#include <cstdint>

namespace Vehement {
namespace Spells {

// Forward declarations
class SpellEffect;
class SpellVisuals;
class SpellTargeting;

// ============================================================================
// Targeting Types
// ============================================================================

/**
 * @brief All supported spell targeting modes
 */
enum class TargetingMode : uint8_t {
    Self,           // Cast on caster only
    Single,         // Single target selection
    PassiveRadius,  // Passive aura around caster
    AOE,            // Area of effect at target location
    Line,           // Line from caster to target point
    Cone,           // Cone emanating from caster
    Projectile,     // Launches projectile toward target
    Chain           // Bounces between targets
};

/**
 * @brief Target priority for auto-targeting
 */
enum class TargetPriority : uint8_t {
    Nearest,        // Closest target first
    Farthest,       // Farthest target first
    LowestHealth,   // Lowest HP target
    HighestHealth,  // Highest HP target
    HighestThreat,  // Highest threat/aggro
    Random          // Random selection
};

/**
 * @brief Filter options for valid targets
 */
enum class FactionFilter : uint8_t {
    All,            // Any faction
    Friendly,       // Same faction as caster
    Enemy,          // Hostile to caster
    Neutral,        // Neutral faction only
    Self            // Only the caster
};

// ============================================================================
// Cost Structure
// ============================================================================

/**
 * @brief Resource costs for casting a spell
 */
struct SpellCost {
    float mana = 0.0f;
    float health = 0.0f;
    float stamina = 0.0f;
    float rage = 0.0f;
    float energy = 0.0f;

    // Custom resource costs (key = resource name)
    std::unordered_map<std::string, float> customResources;

    bool IsZero() const {
        return mana == 0.0f && health == 0.0f && stamina == 0.0f &&
               rage == 0.0f && energy == 0.0f && customResources.empty();
    }
};

// ============================================================================
// Timing Configuration
// ============================================================================

/**
 * @brief Timing parameters for spell casting
 */
struct SpellTiming {
    float castTime = 0.0f;          // Time to cast (0 = instant)
    float channelDuration = 0.0f;   // Time to channel (0 = not channeled)
    float cooldown = 0.0f;          // Time before spell can be cast again

    // Charge system
    int maxCharges = 1;             // Maximum stored charges
    float chargeRechargeTime = 0.0f; // Time to regain one charge

    // Global cooldown interaction
    bool triggersGCD = true;        // Does this spell trigger global cooldown
    bool affectedByGCD = true;      // Is this spell affected by global cooldown
    float gcdDuration = 1.5f;       // Global cooldown duration

    // Haste scaling
    bool castTimeScalesWithHaste = true;
    bool cooldownScalesWithHaste = false;
    bool channelScalesWithHaste = true;
};

// ============================================================================
// Scaling Configuration
// ============================================================================

/**
 * @brief Stat scaling for spell effects
 */
struct SpellScaling {
    std::string stat;               // Stat name (spell_power, attack_power, etc.)
    float coefficient = 0.0f;       // Multiplier for the stat
    float levelScaling = 0.0f;      // Additional scaling per caster level

    // Cap settings
    float minValue = 0.0f;
    float maxValue = std::numeric_limits<float>::max();
};

// ============================================================================
// Event Script References
// ============================================================================

/**
 * @brief Script references for spell events
 */
struct SpellEventScripts {
    std::string onCastStart;        // Called when cast begins
    std::string onCastComplete;     // Called when cast finishes
    std::string onCastInterrupt;    // Called when cast is interrupted
    std::string onChannelTick;      // Called each channel tick
    std::string onHit;              // Called when spell hits target
    std::string onCrit;             // Called on critical hit
    std::string onKill;             // Called when spell kills target
    std::string onMiss;             // Called when spell misses
    std::string onReflect;          // Called when spell is reflected
    std::string onAbsorb;           // Called when spell is absorbed
};

// ============================================================================
// Spell Flags
// ============================================================================

/**
 * @brief Boolean flags for spell behavior
 */
struct SpellFlags {
    bool canCrit = true;            // Can this spell critically hit
    bool canMiss = false;           // Can this spell miss
    bool canBeReflected = true;     // Can this spell be reflected
    bool canBeInterrupted = true;   // Can casting be interrupted
    bool canBeSilenced = true;      // Does silence prevent casting
    bool requiresLineOfSight = true; // Must have LoS to target
    bool requiresFacing = true;     // Must face target
    bool canCastWhileMoving = false; // Can cast while moving
    bool canCastWhileCasting = false; // Can cast during another cast
    bool isPassive = false;         // Passive spell (always active)
    bool isToggle = false;          // Toggle on/off spell
    bool isAura = false;            // Aura effect
    bool breaksOnDamage = false;    // Effect breaks when taking damage
    bool breaksOnMovement = false;  // Effect breaks when moving
    bool ignoresArmor = false;      // Damage ignores armor
    bool ignoresResistance = false; // Damage ignores resistance
};

// ============================================================================
// Requirements
// ============================================================================

/**
 * @brief Requirements to cast the spell
 */
struct SpellRequirements {
    int minLevel = 1;               // Minimum caster level
    std::string requiredWeapon;     // Required weapon type (empty = any)
    std::vector<std::string> requiredBuffs; // Buffs required to cast
    std::vector<std::string> forbiddenBuffs; // Buffs that prevent casting
    std::string requiredStance;     // Required stance (empty = any)
    bool requiresCombat = false;    // Must be in combat
    bool requiresNotCombat = false; // Must be out of combat
    bool requiresStealth = false;   // Must be stealthed
    float minHealth = 0.0f;         // Minimum health percentage
    float maxHealth = 100.0f;       // Maximum health percentage
};

// ============================================================================
// Spell Definition
// ============================================================================

/**
 * @brief Complete spell definition loaded from JSON
 *
 * This class represents a spell template that can be instantiated
 * for actual casting. All configuration comes from JSON files.
 */
class SpellDefinition {
public:
    SpellDefinition() = default;
    ~SpellDefinition() = default;

    // Disable copy, allow move
    SpellDefinition(const SpellDefinition&) = delete;
    SpellDefinition& operator=(const SpellDefinition&) = delete;
    SpellDefinition(SpellDefinition&&) noexcept = default;
    SpellDefinition& operator=(SpellDefinition&&) noexcept = default;

    // =========================================================================
    // JSON Serialization
    // =========================================================================

    /**
     * @brief Load spell definition from JSON file
     */
    bool LoadFromFile(const std::string& filePath);

    /**
     * @brief Load spell definition from JSON string
     */
    bool LoadFromString(const std::string& jsonString);

    /**
     * @brief Save spell definition to JSON file
     */
    bool SaveToFile(const std::string& filePath) const;

    /**
     * @brief Serialize to JSON string
     */
    std::string ToJsonString() const;

    /**
     * @brief Validate the spell definition
     */
    bool Validate(std::vector<std::string>& errors) const;

    // =========================================================================
    // Lifecycle Hooks
    // =========================================================================

    using LifecycleCallback = std::function<void(class SpellInstance&)>;

    /**
     * @brief Called when spell instance is created
     */
    void OnCreate(class SpellInstance& instance) const;

    /**
     * @brief Called every frame while spell is active
     */
    void OnTick(class SpellInstance& instance, float deltaTime) const;

    /**
     * @brief Called when spell instance is destroyed
     */
    void OnDestroy(class SpellInstance& instance) const;

    // =========================================================================
    // Accessors
    // =========================================================================

    [[nodiscard]] const std::string& GetId() const { return m_id; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }
    [[nodiscard]] const std::string& GetDescription() const { return m_description; }
    [[nodiscard]] const std::string& GetIconPath() const { return m_iconPath; }
    [[nodiscard]] const std::string& GetSchool() const { return m_school; }
    [[nodiscard]] const std::vector<std::string>& GetTags() const { return m_tags; }

    [[nodiscard]] TargetingMode GetTargetingMode() const { return m_targetingMode; }
    [[nodiscard]] const SpellTiming& GetTiming() const { return m_timing; }
    [[nodiscard]] const SpellCost& GetCost() const { return m_cost; }
    [[nodiscard]] const SpellFlags& GetFlags() const { return m_flags; }
    [[nodiscard]] const SpellRequirements& GetRequirements() const { return m_requirements; }
    [[nodiscard]] const SpellEventScripts& GetScripts() const { return m_scripts; }

    [[nodiscard]] const std::vector<std::shared_ptr<SpellEffect>>& GetEffects() const { return m_effects; }
    [[nodiscard]] const std::shared_ptr<SpellTargeting>& GetTargeting() const { return m_targeting; }
    [[nodiscard]] const std::shared_ptr<SpellVisuals>& GetVisuals() const { return m_visuals; }

    [[nodiscard]] float GetRange() const { return m_range; }
    [[nodiscard]] float GetMinRange() const { return m_minRange; }

    [[nodiscard]] const std::string& GetSourcePath() const { return m_sourcePath; }
    [[nodiscard]] int64_t GetLastModified() const { return m_lastModified; }

    // =========================================================================
    // Mutators (for programmatic creation)
    // =========================================================================

    void SetId(const std::string& id) { m_id = id; }
    void SetName(const std::string& name) { m_name = name; }
    void SetDescription(const std::string& desc) { m_description = desc; }
    void SetIconPath(const std::string& path) { m_iconPath = path; }
    void SetSchool(const std::string& school) { m_school = school; }
    void SetTags(const std::vector<std::string>& tags) { m_tags = tags; }

    void SetTargetingMode(TargetingMode mode) { m_targetingMode = mode; }
    void SetTiming(const SpellTiming& timing) { m_timing = timing; }
    void SetCost(const SpellCost& cost) { m_cost = cost; }
    void SetFlags(const SpellFlags& flags) { m_flags = flags; }
    void SetRequirements(const SpellRequirements& reqs) { m_requirements = reqs; }
    void SetScripts(const SpellEventScripts& scripts) { m_scripts = scripts; }

    void SetRange(float range) { m_range = range; }
    void SetMinRange(float minRange) { m_minRange = minRange; }

    void AddEffect(std::shared_ptr<SpellEffect> effect) { m_effects.push_back(std::move(effect)); }
    void SetTargeting(std::shared_ptr<SpellTargeting> targeting) { m_targeting = std::move(targeting); }
    void SetVisuals(std::shared_ptr<SpellVisuals> visuals) { m_visuals = std::move(visuals); }

private:
    // Parse JSON sections
    void ParseIdentity(const std::string& json);
    void ParseTargeting(const std::string& json);
    void ParseTiming(const std::string& json);
    void ParseCost(const std::string& json);
    void ParseEffects(const std::string& json);
    void ParseFlags(const std::string& json);
    void ParseRequirements(const std::string& json);
    void ParseScripts(const std::string& json);
    void ParseVisuals(const std::string& json);

    // Identity
    std::string m_id;
    std::string m_name;
    std::string m_description;
    std::string m_iconPath;
    std::string m_school;           // fire, frost, nature, etc.
    std::vector<std::string> m_tags;

    // Core configuration
    TargetingMode m_targetingMode = TargetingMode::Single;
    SpellTiming m_timing;
    SpellCost m_cost;
    SpellFlags m_flags;
    SpellRequirements m_requirements;
    SpellEventScripts m_scripts;

    // Range
    float m_range = 30.0f;
    float m_minRange = 0.0f;

    // Effects
    std::vector<std::shared_ptr<SpellEffect>> m_effects;

    // Targeting configuration
    std::shared_ptr<SpellTargeting> m_targeting;

    // Visual configuration
    std::shared_ptr<SpellVisuals> m_visuals;

    // Lifecycle callbacks (set from scripts)
    LifecycleCallback m_onCreate;
    LifecycleCallback m_onTick;
    LifecycleCallback m_onDestroy;

    // Source tracking for hot-reload
    std::string m_sourcePath;
    int64_t m_lastModified = 0;
};

// ============================================================================
// Spell Instance
// ============================================================================

/**
 * @brief Runtime instance of a spell being cast
 */
class SpellInstance {
public:
    enum class State : uint8_t {
        Created,        // Just created
        Casting,        // Cast in progress
        Channeling,     // Channel in progress
        Traveling,      // Projectile traveling
        Active,         // Effect is active
        Completed,      // Spell finished
        Interrupted,    // Cast was interrupted
        Failed          // Cast failed
    };

    SpellInstance(const SpellDefinition* definition, uint32_t casterId);
    ~SpellInstance() = default;

    // =========================================================================
    // State Management
    // =========================================================================

    [[nodiscard]] State GetState() const { return m_state; }
    [[nodiscard]] bool IsActive() const { return m_state >= State::Casting && m_state <= State::Active; }
    [[nodiscard]] bool IsComplete() const { return m_state >= State::Completed; }

    void SetState(State state) { m_state = state; }

    // =========================================================================
    // Timing
    // =========================================================================

    [[nodiscard]] float GetCastProgress() const;
    [[nodiscard]] float GetChannelProgress() const;
    [[nodiscard]] float GetRemainingCastTime() const { return m_remainingCastTime; }
    [[nodiscard]] float GetRemainingChannelTime() const { return m_remainingChannelTime; }

    void SetRemainingCastTime(float time) { m_remainingCastTime = time; }
    void SetRemainingChannelTime(float time) { m_remainingChannelTime = time; }

    // =========================================================================
    // Targeting
    // =========================================================================

    [[nodiscard]] uint32_t GetCasterId() const { return m_casterId; }
    [[nodiscard]] uint32_t GetTargetId() const { return m_targetId; }
    [[nodiscard]] const glm::vec3& GetTargetPosition() const { return m_targetPosition; }
    [[nodiscard]] const glm::vec3& GetTargetDirection() const { return m_targetDirection; }
    [[nodiscard]] const std::vector<uint32_t>& GetHitTargets() const { return m_hitTargets; }

    void SetTargetId(uint32_t id) { m_targetId = id; }
    void SetTargetPosition(const glm::vec3& pos) { m_targetPosition = pos; }
    void SetTargetDirection(const glm::vec3& dir) { m_targetDirection = dir; }
    void AddHitTarget(uint32_t id) { m_hitTargets.push_back(id); }

    // =========================================================================
    // Definition Access
    // =========================================================================

    [[nodiscard]] const SpellDefinition* GetDefinition() const { return m_definition; }

    // =========================================================================
    // Custom Data Storage
    // =========================================================================

    void SetCustomData(const std::string& key, const std::variant<int, float, std::string, glm::vec3>& value) {
        m_customData[key] = value;
    }

    template<typename T>
    std::optional<T> GetCustomData(const std::string& key) const {
        auto it = m_customData.find(key);
        if (it != m_customData.end()) {
            if (auto* val = std::get_if<T>(&it->second)) {
                return *val;
            }
        }
        return std::nullopt;
    }

private:
    const SpellDefinition* m_definition = nullptr;
    State m_state = State::Created;

    // Timing
    float m_remainingCastTime = 0.0f;
    float m_remainingChannelTime = 0.0f;
    float m_totalCastTime = 0.0f;
    float m_totalChannelTime = 0.0f;

    // Targeting
    uint32_t m_casterId = 0;
    uint32_t m_targetId = 0;
    glm::vec3 m_targetPosition{0.0f};
    glm::vec3 m_targetDirection{0.0f, 0.0f, 1.0f};
    std::vector<uint32_t> m_hitTargets;

    // Custom runtime data
    std::unordered_map<std::string, std::variant<int, float, std::string, glm::vec3>> m_customData;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Convert TargetingMode to string
 */
const char* TargetingModeToString(TargetingMode mode);

/**
 * @brief Parse TargetingMode from string
 */
TargetingMode StringToTargetingMode(const std::string& str);

/**
 * @brief Convert TargetPriority to string
 */
const char* TargetPriorityToString(TargetPriority priority);

/**
 * @brief Parse TargetPriority from string
 */
TargetPriority StringToTargetPriority(const std::string& str);

} // namespace Spells
} // namespace Vehement
