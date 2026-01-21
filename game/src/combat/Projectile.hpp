#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include "Weapon.hpp"

namespace Vehement {

// ============================================================================
// Projectile Types
// ============================================================================

/**
 * @brief Type of projectile
 */
enum class ProjectileType {
    Bullet,         // Standard bullet (pistol, rifle)
    SniperRound,    // High-velocity penetrating round
    Tracer,         // Visible tracer round (every Nth bullet)
};

// ============================================================================
// Hit Result
// ============================================================================

/**
 * @brief Result of a projectile hit
 */
struct HitResult {
    bool hit = false;
    glm::vec3 hitPosition{0.0f};
    glm::vec3 hitNormal{0.0f};
    float distance = 0.0f;
    uint32_t entityId = 0;          // ID of hit entity (0 = world/wall)
    bool isEnemy = false;           // Was the hit target an enemy
    bool isWall = false;            // Hit world geometry
    bool penetrated = false;        // Did bullet pass through
};

// ============================================================================
// Projectile
// ============================================================================

/**
 * @brief Individual projectile entity
 */
class Projectile {
public:
    Projectile();
    ~Projectile() = default;

    /**
     * @brief Initialize projectile with spawn data
     * @param position Starting position
     * @param direction Normalized direction
     * @param speed Velocity magnitude
     * @param damage Damage on hit
     * @param penetration Number of enemies to penetrate
     * @param ownerId ID of entity that fired (player ID)
     */
    void Initialize(
        const glm::vec3& position,
        const glm::vec3& direction,
        float speed,
        float damage,
        int penetration,
        uint32_t ownerId
    );

    /**
     * @brief Update projectile position
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    /**
     * @brief Check if projectile should be destroyed
     */
    [[nodiscard]] bool ShouldDestroy() const;

    /**
     * @brief Mark projectile for destruction
     */
    void Destroy() { m_active = false; }

    /**
     * @brief Process a hit (reduce penetration, apply damage)
     * @return true if bullet should continue (has penetration left)
     */
    bool ProcessHit();

    // Getters
    [[nodiscard]] const glm::vec3& GetPosition() const { return m_position; }
    [[nodiscard]] const glm::vec3& GetPreviousPosition() const { return m_previousPosition; }
    [[nodiscard]] const glm::vec3& GetVelocity() const { return m_velocity; }
    [[nodiscard]] const glm::vec3& GetDirection() const { return m_direction; }
    [[nodiscard]] float GetSpeed() const { return m_speed; }
    [[nodiscard]] float GetDamage() const { return m_damage; }
    [[nodiscard]] int GetPenetration() const { return m_penetration; }
    [[nodiscard]] uint32_t GetOwnerId() const { return m_ownerId; }
    [[nodiscard]] float GetDistanceTraveled() const { return m_distanceTraveled; }
    [[nodiscard]] float GetMaxRange() const { return m_maxRange; }
    [[nodiscard]] float GetLifetime() const { return m_lifetime; }
    [[nodiscard]] bool IsActive() const { return m_active; }
    [[nodiscard]] ProjectileType GetType() const { return m_type; }
    [[nodiscard]] WeaponType GetWeaponType() const { return m_weaponType; }

    // Setters
    void SetType(ProjectileType type) { m_type = type; }
    void SetMaxRange(float range) { m_maxRange = range; }
    void SetPosition(const glm::vec3& pos) { m_position = pos; }
    void SetWeaponType(WeaponType type) { m_weaponType = type; }

    // Tracer rendering info
    [[nodiscard]] glm::vec3 GetTracerStart() const;
    [[nodiscard]] glm::vec3 GetTracerEnd() const;
    [[nodiscard]] float GetTracerLength() const { return m_tracerLength; }
    void SetTracerLength(float length) { m_tracerLength = length; }

private:
    glm::vec3 m_position{0.0f};
    glm::vec3 m_previousPosition{0.0f};
    glm::vec3 m_velocity{0.0f};
    glm::vec3 m_direction{0.0f, 0.0f, 1.0f};

    float m_speed = 500.0f;
    float m_damage = 10.0f;
    int m_penetration = 1;
    uint32_t m_ownerId = 0;

    float m_distanceTraveled = 0.0f;
    float m_maxRange = 1000.0f;
    float m_lifetime = 0.0f;
    float m_maxLifetime = 5.0f;

    float m_tracerLength = 10.0f;

    ProjectileType m_type = ProjectileType::Bullet;
    WeaponType m_weaponType = WeaponType::Glock;
    bool m_active = true;
};

// ============================================================================
// Projectile Pool
// ============================================================================

/**
 * @brief Object pool for efficient projectile management
 */
class ProjectilePool {
public:
    static constexpr size_t kDefaultPoolSize = 500;

    ProjectilePool(size_t maxProjectiles = kDefaultPoolSize);
    ~ProjectilePool() = default;

    /**
     * @brief Spawn a new projectile
     * @return Pointer to projectile, or nullptr if pool is full
     */
    Projectile* Spawn(
        const glm::vec3& position,
        const glm::vec3& direction,
        float speed,
        float damage,
        int penetration,
        uint32_t ownerId
    );

    /**
     * @brief Update all active projectiles
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    /**
     * @brief Get all active projectiles for collision/rendering
     */
    [[nodiscard]] const std::vector<Projectile>& GetProjectiles() const { return m_projectiles; }
    [[nodiscard]] std::vector<Projectile>& GetProjectiles() { return m_projectiles; }

    /**
     * @brief Get number of active projectiles
     */
    [[nodiscard]] size_t GetActiveCount() const { return m_activeCount; }

    /**
     * @brief Clear all projectiles
     */
    void Clear();

    /**
     * @brief Remove inactive projectiles (defragment pool)
     */
    void Compact();

private:
    std::vector<Projectile> m_projectiles;
    size_t m_activeCount = 0;
    size_t m_maxProjectiles;
};

// ============================================================================
// Tracer Renderer
// ============================================================================

/**
 * @brief Handles rendering of projectile tracers
 */
struct TracerVertex {
    glm::vec3 position;
    glm::vec4 color;
    float width;
};

class TracerRenderer {
public:
    TracerRenderer();
    ~TracerRenderer();

    /**
     * @brief Initialize renderer resources
     */
    bool Initialize();

    /**
     * @brief Shutdown and release resources
     */
    void Shutdown();

    /**
     * @brief Add tracer line for rendering
     */
    void AddTracer(const glm::vec3& start, const glm::vec3& end,
                   const glm::vec4& color = glm::vec4(1.0f, 0.9f, 0.5f, 1.0f),
                   float width = 0.05f);

    /**
     * @brief Render all queued tracers
     */
    void Render(const glm::mat4& viewProjection);

    /**
     * @brief Clear tracer queue (call after rendering)
     */
    void Clear();

    /**
     * @brief Set tracer color based on weapon type
     */
    static glm::vec4 GetTracerColor(WeaponType type);

private:
    std::vector<TracerVertex> m_vertices;

    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    bool m_initialized = false;

    static constexpr size_t kMaxTracers = 1000;
};

// ============================================================================
// Bullet Hole Decal (for wall hits)
// ============================================================================

struct BulletHole {
    glm::vec3 position;
    glm::vec3 normal;
    float size = 0.1f;
    float lifetime = 10.0f;     // Seconds before fade
    float fadeTime = 2.0f;      // Fade duration
    float age = 0.0f;
};

class BulletHoleManager {
public:
    static constexpr size_t kMaxBulletHoles = 200;

    BulletHoleManager() = default;
    ~BulletHoleManager() = default;

    /**
     * @brief Add a bullet hole at position
     */
    void AddBulletHole(const glm::vec3& position, const glm::vec3& normal, float size = 0.1f);

    /**
     * @brief Update bullet hole lifetimes
     */
    void Update(float deltaTime);

    /**
     * @brief Get bullet holes for rendering
     */
    [[nodiscard]] const std::vector<BulletHole>& GetBulletHoles() const { return m_bulletHoles; }

    /**
     * @brief Clear all bullet holes
     */
    void Clear() { m_bulletHoles.clear(); }

private:
    std::vector<BulletHole> m_bulletHoles;
};

} // namespace Vehement
