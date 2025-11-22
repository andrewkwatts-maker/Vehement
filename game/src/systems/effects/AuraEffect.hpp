#pragma once

#include "EffectDefinition.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_set>
#include <functional>
#include <memory>
#include <cstdint>

namespace Vehement {
namespace Effects {

// Forward declarations
class EffectContainer;
class EffectManager;

// ============================================================================
// Aura Shape
// ============================================================================

/**
 * @brief Shape of the aura area
 */
enum class AuraShape : uint8_t {
    Circle,         // Circular radius around source
    Cone,           // Cone in facing direction
    Rectangle,      // Rectangular area
    Ring,           // Hollow ring (outer/inner radius)
    Line            // Line from source
};

/**
 * @brief Convert aura shape to string
 */
const char* AuraShapeToString(AuraShape shape);

/**
 * @brief Parse aura shape from string
 */
std::optional<AuraShape> AuraShapeFromString(const std::string& str);

// ============================================================================
// Aura Target Filter
// ============================================================================

/**
 * @brief Which entities the aura affects
 */
enum class AuraTargetFilter : uint8_t {
    Allies,         // Friendly entities only
    Enemies,        // Hostile entities only
    Both,           // All entities
    SelfOnly,       // Only the aura source
    AlliesNoSelf,   // Allies excluding self
    EnemiesNoSelf   // Enemies excluding self (for debuff auras)
};

/**
 * @brief Convert target filter to string
 */
const char* AuraTargetFilterToString(AuraTargetFilter filter);

/**
 * @brief Parse target filter from string
 */
std::optional<AuraTargetFilter> AuraTargetFilterFromString(const std::string& str);

// ============================================================================
// Aura Configuration
// ============================================================================

/**
 * @brief Configuration for an aura effect
 */
struct AuraConfig {
    // Shape and size
    AuraShape shape = AuraShape::Circle;
    float radius = 10.0f;            // Main radius
    float innerRadius = 0.0f;        // For ring shape
    float coneAngle = 60.0f;         // For cone shape (degrees)
    float width = 5.0f;              // For rectangle/line
    float length = 10.0f;            // For rectangle/line

    // Targeting
    AuraTargetFilter targetFilter = AuraTargetFilter::Allies;
    int maxTargets = -1;             // -1 for unlimited

    // Timing
    float pulseInterval = 1.0f;      // Seconds between pulses
    bool pulseOnEnter = true;        // Apply immediately on enter
    bool removeOnExit = true;        // Remove effect on exit

    // Effect to apply
    std::string applyEffectId;       // Effect applied to targets in aura

    // Movement
    bool followsSource = true;       // Aura moves with source
    glm::vec3 offset{0.0f};          // Offset from source position

    // Visuals
    std::string visualEffect;        // Visual effect for aura area
    bool showRange = false;          // Show range indicator

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
// Aura Instance
// ============================================================================

/**
 * @brief Runtime instance of an active aura
 */
class AuraInstance {
public:
    using AuraId = uint32_t;
    static constexpr AuraId INVALID_ID = 0;

    AuraInstance();
    explicit AuraInstance(const EffectDefinition* definition);
    ~AuraInstance();

    // Non-copyable, movable
    AuraInstance(const AuraInstance&) = delete;
    AuraInstance& operator=(const AuraInstance&) = delete;
    AuraInstance(AuraInstance&&) noexcept = default;
    AuraInstance& operator=(AuraInstance&&) noexcept = default;

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize from effect definition
     */
    void Initialize(const EffectDefinition* definition, const AuraConfig& config);

    /**
     * @brief Activate the aura
     */
    void Activate(uint32_t sourceId, const glm::vec3& position);

    /**
     * @brief Deactivate and cleanup
     */
    void Deactivate();

    // -------------------------------------------------------------------------
    // Update
    // -------------------------------------------------------------------------

    /**
     * @brief Update aura state
     * @param deltaTime Time since last update
     * @param sourcePosition Current position of source (if following)
     */
    void Update(float deltaTime, const glm::vec3& sourcePosition);

    /**
     * @brief Check if pulse is ready
     */
    [[nodiscard]] bool IsPulseReady() const;

    /**
     * @brief Consume pulse timer
     */
    void ConsumePulse();

    // -------------------------------------------------------------------------
    // Target Management
    // -------------------------------------------------------------------------

    /**
     * @brief Check if entity is in aura range
     */
    [[nodiscard]] bool IsInRange(const glm::vec3& entityPosition, float entityRadius = 0.0f) const;

    /**
     * @brief Get entities that entered the aura this frame
     */
    [[nodiscard]] const std::vector<uint32_t>& GetNewEntities() const { return m_enteredThisFrame; }

    /**
     * @brief Get entities that exited the aura this frame
     */
    [[nodiscard]] const std::vector<uint32_t>& GetExitedEntities() const { return m_exitedThisFrame; }

    /**
     * @brief Get all entities currently in aura
     */
    [[nodiscard]] const std::unordered_set<uint32_t>& GetEntitiesInRange() const { return m_entitiesInRange; }

    /**
     * @brief Update entity presence
     */
    void UpdateEntityPresence(uint32_t entityId, bool inRange);

    /**
     * @brief Clear frame tracking
     */
    void ClearFrameTracking();

    // -------------------------------------------------------------------------
    // Properties
    // -------------------------------------------------------------------------

    [[nodiscard]] AuraId GetId() const { return m_auraId; }
    [[nodiscard]] uint32_t GetSourceId() const { return m_sourceId; }
    [[nodiscard]] const glm::vec3& GetPosition() const { return m_position; }
    [[nodiscard]] const AuraConfig& GetConfig() const { return m_config; }
    [[nodiscard]] const EffectDefinition* GetDefinition() const { return m_definition; }
    [[nodiscard]] bool IsActive() const { return m_active; }

    void SetPosition(const glm::vec3& pos) { m_position = pos; }
    void SetFacingDirection(const glm::vec3& dir) { m_facingDirection = glm::normalize(dir); }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using EntityCallback = std::function<void(uint32_t entityId, AuraInstance*)>;

    void SetOnEntityEnter(EntityCallback callback) { m_onEntityEnter = std::move(callback); }
    void SetOnEntityExit(EntityCallback callback) { m_onEntityExit = std::move(callback); }
    void SetOnPulse(EntityCallback callback) { m_onPulse = std::move(callback); }

private:
    bool CheckCircleIntersection(const glm::vec3& entityPos, float entityRadius) const;
    bool CheckConeIntersection(const glm::vec3& entityPos, float entityRadius) const;
    bool CheckRectangleIntersection(const glm::vec3& entityPos, float entityRadius) const;
    bool CheckRingIntersection(const glm::vec3& entityPos, float entityRadius) const;
    bool CheckLineIntersection(const glm::vec3& entityPos, float entityRadius) const;

    // Identity
    AuraId m_auraId = INVALID_ID;
    const EffectDefinition* m_definition = nullptr;

    // Configuration
    AuraConfig m_config;

    // State
    bool m_active = false;
    uint32_t m_sourceId = 0;
    glm::vec3 m_position{0.0f};
    glm::vec3 m_facingDirection{0.0f, 0.0f, 1.0f};

    // Timing
    float m_pulseTimer = 0.0f;

    // Entity tracking
    std::unordered_set<uint32_t> m_entitiesInRange;
    std::vector<uint32_t> m_enteredThisFrame;
    std::vector<uint32_t> m_exitedThisFrame;

    // Callbacks
    EntityCallback m_onEntityEnter;
    EntityCallback m_onEntityExit;
    EntityCallback m_onPulse;

    // ID generator
    static AuraId s_nextAuraId;
};

// ============================================================================
// Aura Manager
// ============================================================================

/**
 * @brief Manages all active auras in the game
 */
class AuraManager {
public:
    AuraManager();
    ~AuraManager();

    /**
     * @brief Set effect manager for applying effects
     */
    void SetEffectManager(EffectManager* manager) { m_effectManager = manager; }

    // -------------------------------------------------------------------------
    // Aura Creation
    // -------------------------------------------------------------------------

    /**
     * @brief Create an aura from effect definition
     */
    AuraInstance* CreateAura(
        const EffectDefinition* definition,
        uint32_t sourceId,
        const glm::vec3& position
    );

    /**
     * @brief Create an aura with custom config
     */
    AuraInstance* CreateAura(
        const AuraConfig& config,
        uint32_t sourceId,
        const glm::vec3& position
    );

    /**
     * @brief Remove an aura by ID
     */
    bool RemoveAura(AuraInstance::AuraId auraId);

    /**
     * @brief Remove all auras from a source
     */
    int RemoveAurasBySource(uint32_t sourceId);

    // -------------------------------------------------------------------------
    // Update
    // -------------------------------------------------------------------------

    /**
     * @brief Update all auras
     * @param deltaTime Time since last update
     * @param entityPositions Map of entity ID to position
     * @param entityFactions Map of entity ID to faction (for targeting)
     */
    void Update(
        float deltaTime,
        const std::unordered_map<uint32_t, glm::vec3>& entityPositions,
        const std::unordered_map<uint32_t, int>& entityFactions
    );

    // -------------------------------------------------------------------------
    // Queries
    // -------------------------------------------------------------------------

    /**
     * @brief Get all auras affecting an entity
     */
    std::vector<AuraInstance*> GetAurasAffecting(uint32_t entityId) const;

    /**
     * @brief Get all auras from a source
     */
    std::vector<AuraInstance*> GetAurasFromSource(uint32_t sourceId) const;

    /**
     * @brief Check if entity is in any aura
     */
    bool IsInAnyAura(uint32_t entityId) const;

    /**
     * @brief Get aura by ID
     */
    AuraInstance* GetAura(AuraInstance::AuraId auraId) const;

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using AuraEffectCallback = std::function<void(
        AuraInstance* aura,
        uint32_t targetId,
        const std::string& effectId
    )>;

    void SetOnApplyEffect(AuraEffectCallback callback) {
        m_onApplyEffect = std::move(callback);
    }

    void SetOnRemoveEffect(AuraEffectCallback callback) {
        m_onRemoveEffect = std::move(callback);
    }

private:
    void ProcessAuraTargets(
        AuraInstance* aura,
        const std::unordered_map<uint32_t, glm::vec3>& entityPositions,
        const std::unordered_map<uint32_t, int>& entityFactions
    );

    bool PassesTargetFilter(
        AuraTargetFilter filter,
        uint32_t sourceId,
        uint32_t targetId,
        int sourceFaction,
        int targetFaction
    ) const;

    EffectManager* m_effectManager = nullptr;
    std::vector<std::unique_ptr<AuraInstance>> m_auras;
    std::unordered_map<uint32_t, int> m_sourceFactions;

    AuraEffectCallback m_onApplyEffect;
    AuraEffectCallback m_onRemoveEffect;
};

} // namespace Effects
} // namespace Vehement
