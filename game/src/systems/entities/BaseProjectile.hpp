#pragma once

#include "../lifecycle/ILifecycle.hpp"
#include "../lifecycle/GameEvent.hpp"
#include "../lifecycle/ComponentLifecycle.hpp"
#include "../lifecycle/ScriptedLifecycle.hpp"
#include <glm/glm.hpp>
#include <string>
#include <functional>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// Projectile Type
// ============================================================================

/**
 * @brief Projectile movement type
 */
enum class ProjectileType : uint8_t {
    Linear,         // Straight line
    Homing,         // Tracks target
    Parabolic,      // Arc trajectory
    Instant,        // Instant hit (hitscan)
    Bouncing,       // Bounces off surfaces
    Seeking         // Seeks nearest enemy
};

// ============================================================================
// Projectile Stats
// ============================================================================

/**
 * @brief Projectile statistics
 */
struct ProjectileStats {
    // Movement
    float speed = 20.0f;
    float acceleration = 0.0f;      // For homing
    float turnRate = 180.0f;        // Degrees/sec for homing
    float gravity = 0.0f;           // For parabolic

    // Damage
    float damage = 10.0f;
    float explosionRadius = 0.0f;   // 0 = no explosion
    float knockback = 0.0f;
    std::string damageType = "physical";

    // Lifetime
    float maxLifetime = 5.0f;
    float maxDistance = 100.0f;

    // Collision
    float radius = 0.1f;
    bool piercing = false;          // Pass through enemies
    int maxPierceCount = 1;         // How many to pierce
    int bounceCount = 0;            // Bounces before destroyed

    // Visual
    float scale = 1.0f;
    bool hasTrail = true;
};

// ============================================================================
// BaseProjectile
// ============================================================================

/**
 * @brief Base class for all projectiles
 *
 * Provides:
 * - Various trajectory types
 * - Collision detection
 * - Damage dealing
 * - Explosion/AOE effects
 * - Script hooks
 *
 * JSON Config:
 * {
 *   "id": "projectile_arrow",
 *   "type": "projectile",
 *   "projectile_type": "Linear",
 *   "stats": {
 *     "speed": 30,
 *     "damage": 15,
 *     "max_lifetime": 3
 *   },
 *   "lifecycle": {
 *     "tick_group": "Physics"
 *   }
 * }
 */
class BaseProjectile : public ScriptedLifecycle {
public:
    BaseProjectile();
    ~BaseProjectile() override;

    // =========================================================================
    // ILifecycle Implementation
    // =========================================================================

    void OnCreate(const nlohmann::json& config) override;
    void OnTick(float deltaTime) override;
    bool OnEvent(const GameEvent& event) override;
    void OnDestroy() override;

    [[nodiscard]] const char* GetTypeName() const override { return "BaseProjectile"; }

    // =========================================================================
    // Projectile Properties
    // =========================================================================

    [[nodiscard]] ProjectileType GetProjectileType() const { return m_projectileType; }
    void SetProjectileType(ProjectileType type) { m_projectileType = type; }

    [[nodiscard]] ProjectileStats& GetStats() { return m_stats; }
    [[nodiscard]] const ProjectileStats& GetStats() const { return m_stats; }

    // =========================================================================
    // Launch Configuration
    // =========================================================================

    /**
     * @brief Launch projectile
     */
    void Launch(const glm::vec3& position, const glm::vec3& direction);

    /**
     * @brief Launch at specific target
     */
    void LaunchAt(const glm::vec3& position, const glm::vec3& targetPos);

    /**
     * @brief Launch at target entity (for homing)
     */
    void LaunchAtTarget(const glm::vec3& position, LifecycleHandle target);

    /**
     * @brief Set source (who fired this)
     */
    void SetSource(LifecycleHandle source) { m_source = source; }
    [[nodiscard]] LifecycleHandle GetSource() const { return m_source; }

    /**
     * @brief Set target (for homing)
     */
    void SetTarget(LifecycleHandle target) { m_target = target; }
    [[nodiscard]] LifecycleHandle GetTarget() const { return m_target; }

    // =========================================================================
    // Position/Movement
    // =========================================================================

    [[nodiscard]] const glm::vec3& GetPosition() const { return m_position; }
    void SetPosition(const glm::vec3& pos) { m_position = pos; }

    [[nodiscard]] const glm::vec3& GetVelocity() const { return m_velocity; }
    [[nodiscard]] const glm::vec3& GetDirection() const { return m_direction; }

    [[nodiscard]] float GetDistanceTraveled() const { return m_distanceTraveled; }
    [[nodiscard]] float GetLifetime() const { return m_lifetime; }

    // =========================================================================
    // Team
    // =========================================================================

    [[nodiscard]] int GetTeamId() const { return m_teamId; }
    void SetTeamId(int teamId) { m_teamId = teamId; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    using HitCallback = std::function<void(LifecycleHandle target, const glm::vec3& hitPos)>;
    void SetOnHit(HitCallback callback) { m_onHit = std::move(callback); }

    using ExpireCallback = std::function<void()>;
    void SetOnExpire(ExpireCallback callback) { m_onExpire = std::move(callback); }

    // =========================================================================
    // Script Context Override
    // =========================================================================

    [[nodiscard]] ScriptContext BuildContext() const override;

protected:
    virtual void ParseProjectileConfig(const nlohmann::json& config);

    // Movement updates
    virtual void UpdateLinear(float deltaTime);
    virtual void UpdateHoming(float deltaTime);
    virtual void UpdateParabolic(float deltaTime);

    // Collision
    virtual bool CheckCollisions();
    virtual void OnHit(LifecycleHandle target, const glm::vec3& hitPos);
    virtual void OnExpire();
    virtual void Explode();

    // Helpers
    void DealDamage(LifecycleHandle target);
    void ApplyKnockback(LifecycleHandle target, const glm::vec3& direction);

    ProjectileType m_projectileType = ProjectileType::Linear;
    ProjectileStats m_stats;

    glm::vec3 m_position{0.0f};
    glm::vec3 m_velocity{0.0f};
    glm::vec3 m_direction{0.0f, 0.0f, 1.0f};
    glm::vec3 m_startPosition{0.0f};

    LifecycleHandle m_source = LifecycleHandle::Invalid;
    LifecycleHandle m_target = LifecycleHandle::Invalid;

    float m_lifetime = 0.0f;
    float m_distanceTraveled = 0.0f;
    int m_pierceCount = 0;
    int m_bounceCount = 0;
    int m_teamId = 0;

    bool m_launched = false;
    std::vector<LifecycleHandle> m_hitTargets; // Track piercing

    HitCallback m_onHit;
    ExpireCallback m_onExpire;

    ComponentContainer m_components;
};

} // namespace Lifecycle
} // namespace Vehement
