#pragma once

#include "SpellDefinition.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <functional>

namespace Vehement {
namespace Spells {

// Forward declarations
class SpellInstance;

// ============================================================================
// Target Filter Configuration
// ============================================================================

/**
 * @brief Unit type filter for targeting
 */
enum class UnitTypeFilter : uint8_t {
    Any,            // Any unit type
    Player,         // Player characters only
    NPC,            // NPCs only
    Monster,        // Monsters/enemies only
    Summon,         // Summoned units only
    Building,       // Buildings/structures
    Destructible    // Destructible objects
};

/**
 * @brief Complete filter configuration for spell targets
 */
struct TargetFilter {
    // Faction filtering
    FactionFilter factionFilter = FactionFilter::All;
    bool canTargetSelf = true;

    // Unit type filtering
    std::vector<UnitTypeFilter> allowedTypes;
    std::vector<UnitTypeFilter> excludedTypes;

    // State filtering
    bool mustBeAlive = true;
    bool canTargetDead = false;
    bool canTargetInvisible = false;
    bool canTargetInvulnerable = false;
    bool mustBeInCombat = false;
    bool mustBeOutOfCombat = false;

    // Buff/debuff filtering
    std::vector<std::string> requiredBuffs;     // Target must have these
    std::vector<std::string> excludedBuffs;     // Target must NOT have these
    std::vector<std::string> requiredDebuffs;
    std::vector<std::string> excludedDebuffs;

    // Health filtering
    float minHealthPercent = 0.0f;
    float maxHealthPercent = 100.0f;

    // Custom filter script
    std::string customFilterScript;
};

// ============================================================================
// Targeting Preview Data
// ============================================================================

/**
 * @brief Visual data for targeting preview/indicator
 */
struct TargetingPreview {
    // Shape type for preview
    enum class Shape : uint8_t {
        None,
        Circle,         // AOE circle
        Rectangle,      // Line/rectangle
        Cone,           // Cone shape
        Ring,           // Ring/donut shape
        Arrow           // Direction indicator
    };

    Shape shape = Shape::None;

    // Color (RGBA)
    glm::vec4 validColor{0.0f, 1.0f, 0.0f, 0.3f};
    glm::vec4 invalidColor{1.0f, 0.0f, 0.0f, 0.3f};
    glm::vec4 maxRangeColor{1.0f, 1.0f, 0.0f, 0.2f};

    // Display options
    bool showRange = true;
    bool showAOE = true;
    bool showTargets = true;
    bool pulseAnimation = true;
    float pulseSpeed = 2.0f;

    // Custom indicator
    std::string customIndicatorModel;
    std::string customIndicatorTexture;
};

// ============================================================================
// Projectile Configuration
// ============================================================================

/**
 * @brief Configuration for projectile-based spells
 */
struct ProjectileConfig {
    float speed = 20.0f;                // Units per second
    float acceleration = 0.0f;          // Speed change per second
    float maxSpeed = 100.0f;            // Maximum speed cap
    float turnRate = 0.0f;              // Degrees per second (0 = no homing)
    float gravity = 0.0f;               // Gravity applied to projectile

    // Collision
    float radius = 0.5f;                // Collision radius
    bool piercing = false;              // Can hit multiple targets
    int maxPierceCount = 1;             // Maximum pierce count
    float pierceDamageFalloff = 0.2f;   // Damage reduction per pierce

    // Behavior
    bool homingEnabled = false;         // Tracks target
    float homingAcquireRange = 5.0f;    // Range to acquire new target
    bool explodeOnImpact = false;       // Creates AOE on hit
    float explosionRadius = 0.0f;       // Explosion radius

    // Lifetime
    float maxLifetime = 10.0f;          // Maximum flight time
    float maxRange = 100.0f;            // Maximum travel distance

    // Visual
    std::string modelPath;
    std::string trailEffect;
    float trailLength = 2.0f;
};

// ============================================================================
// Chain Configuration
// ============================================================================

/**
 * @brief Configuration for chain spells
 */
struct ChainConfig {
    int maxBounces = 3;                 // Maximum number of bounces
    float bounceRange = 10.0f;          // Max range to next target
    float damagePerBounce = 0.0f;       // Flat damage change per bounce
    float damageMultiplierPerBounce = 0.9f; // Multiplicative change per bounce
    float bounceDelay = 0.1f;           // Time between bounces
    bool canHitSameTarget = false;      // Can return to previous targets
    bool requiresLoS = true;            // Requires line of sight to bounce

    // Priority for bounce targets
    TargetPriority bouncePriority = TargetPriority::Nearest;
};

// ============================================================================
// Ground Targeting Configuration
// ============================================================================

/**
 * @brief Configuration for ground-targeted spells
 */
struct GroundTargetConfig {
    bool enabled = true;
    bool snapToTerrain = true;          // Snap to terrain height
    bool requiresWalkable = false;      // Must be on walkable terrain
    bool showGroundIndicator = true;    // Show targeting circle
    float indicatorRadius = 1.0f;       // Radius of indicator
    float maxHeightDifference = 10.0f;  // Max height diff from caster
};

// ============================================================================
// Spell Targeting System
// ============================================================================

/**
 * @brief Complete targeting configuration for a spell
 *
 * Handles all targeting modes: Self, Single, PassiveRadius, AOE, Line, Cone, Projectile, Chain
 */
class SpellTargeting {
public:
    SpellTargeting() = default;
    ~SpellTargeting() = default;

    // =========================================================================
    // JSON Serialization
    // =========================================================================

    /**
     * @brief Load targeting config from JSON string
     */
    bool LoadFromJson(const std::string& jsonString);

    /**
     * @brief Serialize targeting config to JSON string
     */
    std::string ToJsonString() const;

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate targeting configuration
     */
    bool Validate(std::vector<std::string>& errors) const;

    // =========================================================================
    // Target Acquisition
    // =========================================================================

    /**
     * @brief Find valid targets for the spell
     * @param instance The spell instance being cast
     * @param casterPosition Position of the caster
     * @param targetPosition Target point (for AOE, line, etc.)
     * @param targetDirection Direction for cone/line spells
     * @param entityQuery Function to query entities in area
     * @return List of valid target entity IDs
     */
    using EntityQueryFunc = std::function<std::vector<uint32_t>(
        const glm::vec3& center, float radius
    )>;

    std::vector<uint32_t> FindTargets(
        const SpellInstance& instance,
        const glm::vec3& casterPosition,
        const glm::vec3& targetPosition,
        const glm::vec3& targetDirection,
        EntityQueryFunc entityQuery
    ) const;

    /**
     * @brief Check if a specific entity is a valid target
     */
    using EntityValidationFunc = std::function<bool(uint32_t entityId, const TargetFilter& filter)>;

    bool IsValidTarget(
        uint32_t entityId,
        uint32_t casterId,
        EntityValidationFunc validateFunc
    ) const;

    /**
     * @brief Get targeting preview data for UI
     */
    TargetingPreview GetPreviewData(
        const glm::vec3& casterPosition,
        const glm::vec3& targetPosition,
        const glm::vec3& targetDirection
    ) const;

    // =========================================================================
    // Mode-Specific Targeting
    // =========================================================================

    /**
     * @brief Get targets in AOE radius
     */
    std::vector<uint32_t> GetAOETargets(
        const glm::vec3& center,
        float radius,
        EntityQueryFunc entityQuery,
        EntityValidationFunc validateFunc
    ) const;

    /**
     * @brief Get targets in line
     */
    std::vector<uint32_t> GetLineTargets(
        const glm::vec3& start,
        const glm::vec3& end,
        float width,
        EntityQueryFunc entityQuery,
        EntityValidationFunc validateFunc
    ) const;

    /**
     * @brief Get targets in cone
     */
    std::vector<uint32_t> GetConeTargets(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float range,
        float angle,
        EntityQueryFunc entityQuery,
        EntityValidationFunc validateFunc
    ) const;

    /**
     * @brief Get next chain target
     */
    std::optional<uint32_t> GetNextChainTarget(
        const glm::vec3& currentPosition,
        const std::vector<uint32_t>& alreadyHit,
        EntityQueryFunc entityQuery,
        EntityValidationFunc validateFunc
    ) const;

    // =========================================================================
    // Accessors
    // =========================================================================

    [[nodiscard]] TargetingMode GetMode() const { return m_mode; }
    [[nodiscard]] float GetRange() const { return m_range; }
    [[nodiscard]] float GetMinRange() const { return m_minRange; }
    [[nodiscard]] float GetRadius() const { return m_radius; }
    [[nodiscard]] float GetAngle() const { return m_angle; }
    [[nodiscard]] float GetWidth() const { return m_width; }
    [[nodiscard]] int GetMaxTargets() const { return m_maxTargets; }
    [[nodiscard]] TargetPriority GetPriority() const { return m_priority; }
    [[nodiscard]] const TargetFilter& GetFilter() const { return m_filter; }
    [[nodiscard]] const ProjectileConfig& GetProjectile() const { return m_projectile; }
    [[nodiscard]] const ChainConfig& GetChain() const { return m_chain; }
    [[nodiscard]] const GroundTargetConfig& GetGroundTarget() const { return m_groundTarget; }
    [[nodiscard]] const TargetingPreview& GetPreview() const { return m_preview; }

    // =========================================================================
    // Mutators
    // =========================================================================

    void SetMode(TargetingMode mode) { m_mode = mode; }
    void SetRange(float range) { m_range = range; }
    void SetMinRange(float minRange) { m_minRange = minRange; }
    void SetRadius(float radius) { m_radius = radius; }
    void SetAngle(float angle) { m_angle = angle; }
    void SetWidth(float width) { m_width = width; }
    void SetMaxTargets(int max) { m_maxTargets = max; }
    void SetPriority(TargetPriority priority) { m_priority = priority; }
    void SetFilter(const TargetFilter& filter) { m_filter = filter; }
    void SetProjectile(const ProjectileConfig& config) { m_projectile = config; }
    void SetChain(const ChainConfig& config) { m_chain = config; }
    void SetGroundTarget(const GroundTargetConfig& config) { m_groundTarget = config; }
    void SetPreview(const TargetingPreview& preview) { m_preview = preview; }

private:
    // Core settings
    TargetingMode m_mode = TargetingMode::Single;
    float m_range = 30.0f;              // Maximum targeting range
    float m_minRange = 0.0f;            // Minimum targeting range
    float m_radius = 0.0f;              // AOE radius / passive radius
    float m_angle = 90.0f;              // Cone angle in degrees
    float m_width = 2.0f;               // Line width
    int m_maxTargets = 1;               // Maximum targets hit
    TargetPriority m_priority = TargetPriority::Nearest;

    // Filter configuration
    TargetFilter m_filter;

    // Mode-specific configs
    ProjectileConfig m_projectile;
    ChainConfig m_chain;
    GroundTargetConfig m_groundTarget;

    // Preview configuration
    TargetingPreview m_preview;

    // Helper methods
    void SortTargetsByPriority(
        std::vector<uint32_t>& targets,
        const glm::vec3& referencePoint,
        std::function<glm::vec3(uint32_t)> getPosition,
        std::function<float(uint32_t)> getHealth,
        std::function<float(uint32_t)> getThreat
    ) const;

    bool IsInCone(
        const glm::vec3& origin,
        const glm::vec3& direction,
        const glm::vec3& point,
        float range,
        float angle
    ) const;

    bool IsInLine(
        const glm::vec3& start,
        const glm::vec3& end,
        const glm::vec3& point,
        float width
    ) const;
};

// ============================================================================
// Targeting Utility Functions
// ============================================================================

/**
 * @brief Calculate distance between two points (2D, ignoring Y)
 */
float GetHorizontalDistance(const glm::vec3& a, const glm::vec3& b);

/**
 * @brief Calculate angle between two directions
 */
float GetAngleBetween(const glm::vec3& dir1, const glm::vec3& dir2);

/**
 * @brief Check if point is within range
 */
bool IsInRange(const glm::vec3& origin, const glm::vec3& target, float minRange, float maxRange);

/**
 * @brief Get direction from origin to target
 */
glm::vec3 GetDirection(const glm::vec3& origin, const glm::vec3& target);

/**
 * @brief Convert UnitTypeFilter to string
 */
const char* UnitTypeFilterToString(UnitTypeFilter filter);

/**
 * @brief Parse UnitTypeFilter from string
 */
UnitTypeFilter StringToUnitTypeFilter(const std::string& str);

} // namespace Spells
} // namespace Vehement
