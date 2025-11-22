#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <functional>
#include "Weapon.hpp"

namespace Vehement {

// ============================================================================
// Grenade Types
// ============================================================================

/**
 * @brief Type of grenade/throwable
 */
enum class GrenadeType {
    Frag,           // Explosive fragmentation
    Flash,          // Flashbang - blinds enemies
    Stun,           // Stun/concussion - slows enemies
    Smoke,          // Smoke grenade - visual cover
    Incendiary,     // Fire damage over time
    Claymore        // Placeable proximity mine
};

// ============================================================================
// Grenade State
// ============================================================================

/**
 * @brief Current state of grenade
 */
enum class GrenadeState {
    InFlight,       // Currently being thrown
    Armed,          // Landed/placed and counting down (or waiting for trigger)
    Exploding,      // Currently exploding (for visual effects)
    Expired         // Done, should be removed
};

// ============================================================================
// Grenade Stats
// ============================================================================

/**
 * @brief Stats that define grenade behavior
 */
struct GrenadeStats {
    float damage = 100.0f;          // Direct hit damage
    float radius = 10.0f;           // Explosion radius
    float fuseTime = 3.0f;          // Seconds until detonation
    float throwSpeed = 15.0f;       // Initial throw velocity
    float effectDuration = 5.0f;    // Duration of effect (stun, flash, etc.)
    float effectStrength = 1.0f;    // Intensity of effect (0-1)

    // Builder-style setters
    GrenadeStats& SetDamage(float d) { damage = d; return *this; }
    GrenadeStats& SetRadius(float r) { radius = r; return *this; }
    GrenadeStats& SetFuseTime(float t) { fuseTime = t; return *this; }
    GrenadeStats& SetThrowSpeed(float s) { throwSpeed = s; return *this; }
    GrenadeStats& SetEffectDuration(float d) { effectDuration = d; return *this; }
    GrenadeStats& SetEffectStrength(float s) { effectStrength = s; return *this; }
};

// ============================================================================
// Default Grenade Stats
// ============================================================================

namespace DefaultGrenadeStats {

    inline GrenadeStats GetFragStats() {
        return GrenadeStats()
            .SetDamage(150.0f)
            .SetRadius(12.0f)
            .SetFuseTime(3.0f)
            .SetThrowSpeed(20.0f)
            .SetEffectDuration(0.0f)
            .SetEffectStrength(0.0f);
    }

    inline GrenadeStats GetFlashStats() {
        return GrenadeStats()
            .SetDamage(0.0f)
            .SetRadius(15.0f)
            .SetFuseTime(2.0f)
            .SetThrowSpeed(18.0f)
            .SetEffectDuration(5.0f)      // 5 seconds of blindness
            .SetEffectStrength(1.0f);
    }

    inline GrenadeStats GetStunStats() {
        return GrenadeStats()
            .SetDamage(25.0f)
            .SetRadius(10.0f)
            .SetFuseTime(2.5f)
            .SetThrowSpeed(18.0f)
            .SetEffectDuration(4.0f)      // 4 seconds of slow
            .SetEffectStrength(0.5f);     // 50% speed reduction
    }

    inline GrenadeStats GetSmokeStats() {
        return GrenadeStats()
            .SetDamage(0.0f)
            .SetRadius(8.0f)
            .SetFuseTime(1.5f)
            .SetThrowSpeed(15.0f)
            .SetEffectDuration(10.0f)     // 10 seconds of smoke
            .SetEffectStrength(1.0f);
    }

    inline GrenadeStats GetIncendiaryStats() {
        return GrenadeStats()
            .SetDamage(20.0f)             // DPS
            .SetRadius(8.0f)
            .SetFuseTime(2.0f)
            .SetThrowSpeed(16.0f)
            .SetEffectDuration(7.0f)      // 7 seconds of fire
            .SetEffectStrength(1.0f);
    }

    inline GrenadeStats GetClaymoreStats() {
        return GrenadeStats()
            .SetDamage(200.0f)
            .SetRadius(8.0f)
            .SetFuseTime(0.5f)            // Time after trigger
            .SetThrowSpeed(0.0f)          // Placed, not thrown
            .SetEffectDuration(0.0f)
            .SetEffectStrength(0.0f);
    }

    inline GrenadeStats GetStats(GrenadeType type) {
        switch (type) {
            case GrenadeType::Frag:       return GetFragStats();
            case GrenadeType::Flash:      return GetFlashStats();
            case GrenadeType::Stun:       return GetStunStats();
            case GrenadeType::Smoke:      return GetSmokeStats();
            case GrenadeType::Incendiary: return GetIncendiaryStats();
            case GrenadeType::Claymore:   return GetClaymoreStats();
            default:                      return GetFragStats();
        }
    }
}

// ============================================================================
// Explosion Effect Data
// ============================================================================

/**
 * @brief Data for explosion visual/audio effects
 */
struct ExplosionEffect {
    glm::vec3 position;
    float radius;
    GrenadeType type;
    float lifetime = 0.0f;
    float maxLifetime = 1.0f;
    bool active = true;

    [[nodiscard]] float GetProgress() const {
        return lifetime / maxLifetime;
    }
};

// ============================================================================
// Grenade Entity
// ============================================================================

/**
 * @brief Individual grenade/throwable entity
 */
class Grenade {
public:
    Grenade();
    ~Grenade() = default;

    /**
     * @brief Initialize as thrown grenade
     * @param position Starting position
     * @param direction Throw direction (will be normalized)
     * @param type Type of grenade
     * @param ownerId ID of thrower
     */
    void InitializeThrown(
        const glm::vec3& position,
        const glm::vec3& direction,
        GrenadeType type,
        uint32_t ownerId
    );

    /**
     * @brief Initialize as placed mine (claymore)
     * @param position Placement position
     * @param facingDirection Direction the mine faces
     * @param ownerId ID of placer
     */
    void InitializePlaced(
        const glm::vec3& position,
        const glm::vec3& facingDirection,
        uint32_t ownerId
    );

    /**
     * @brief Update grenade physics and state
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    /**
     * @brief Trigger explosion (for claymores when enemy enters)
     */
    void Trigger();

    /**
     * @brief Check if grenade should explode
     */
    [[nodiscard]] bool ShouldExplode() const;

    /**
     * @brief Mark as exploded
     */
    void Explode();

    // State checks
    [[nodiscard]] bool IsActive() const { return m_state != GrenadeState::Expired; }
    [[nodiscard]] bool IsInFlight() const { return m_state == GrenadeState::InFlight; }
    [[nodiscard]] bool IsArmed() const { return m_state == GrenadeState::Armed; }
    [[nodiscard]] bool IsExploding() const { return m_state == GrenadeState::Exploding; }
    [[nodiscard]] bool IsExpired() const { return m_state == GrenadeState::Expired; }
    [[nodiscard]] bool IsTriggered() const { return m_triggered; }

    // Getters
    [[nodiscard]] const glm::vec3& GetPosition() const { return m_position; }
    [[nodiscard]] const glm::vec3& GetVelocity() const { return m_velocity; }
    [[nodiscard]] const glm::vec3& GetFacingDirection() const { return m_facingDirection; }
    [[nodiscard]] GrenadeType GetType() const { return m_type; }
    [[nodiscard]] GrenadeState GetState() const { return m_state; }
    [[nodiscard]] const GrenadeStats& GetStats() const { return m_stats; }
    [[nodiscard]] uint32_t GetOwnerId() const { return m_ownerId; }
    [[nodiscard]] float GetFuseRemaining() const { return m_fuseTimer; }
    [[nodiscard]] float GetFuseProgress() const;

    // Collision/physics
    void SetPosition(const glm::vec3& pos) { m_position = pos; }
    void SetVelocity(const glm::vec3& vel) { m_velocity = vel; }
    void SetOnGround(bool grounded) { m_onGround = grounded; }
    [[nodiscard]] bool IsOnGround() const { return m_onGround; }

    // Bounce off surfaces
    void Bounce(const glm::vec3& normal, float bounciness = 0.4f);

    // Texture paths
    [[nodiscard]] const char* GetTexturePath() const;

    // Claymore specific
    [[nodiscard]] float GetTriggerRadius() const { return m_triggerRadius; }
    [[nodiscard]] float GetDetectionAngle() const { return m_detectionAngle; }
    [[nodiscard]] bool IsInDetectionCone(const glm::vec3& targetPos) const;

private:
    void TransitionToArmed();
    void TransitionToExploding();
    void TransitionToExpired();

    glm::vec3 m_position{0.0f};
    glm::vec3 m_velocity{0.0f};
    glm::vec3 m_facingDirection{0.0f, 0.0f, 1.0f};

    GrenadeType m_type = GrenadeType::Frag;
    GrenadeState m_state = GrenadeState::InFlight;
    GrenadeStats m_stats;

    uint32_t m_ownerId = 0;

    float m_fuseTimer = 3.0f;
    float m_explosionTimer = 0.0f;
    float m_lifetime = 0.0f;

    bool m_onGround = false;
    bool m_triggered = false;

    // Claymore detection
    float m_triggerRadius = 5.0f;
    float m_detectionAngle = 60.0f;  // Degrees

    // Physics constants
    static constexpr float kGravity = 20.0f;
    static constexpr float kGroundFriction = 0.9f;
    static constexpr float kAirDrag = 0.99f;
    static constexpr float kExplosionDuration = 0.5f;
};

// ============================================================================
// Grenade Pool
// ============================================================================

/**
 * @brief Object pool for grenade management
 */
class GrenadePool {
public:
    static constexpr size_t kDefaultPoolSize = 50;

    GrenadePool(size_t maxGrenades = kDefaultPoolSize);
    ~GrenadePool() = default;

    /**
     * @brief Throw a grenade
     */
    Grenade* ThrowGrenade(
        const glm::vec3& position,
        const glm::vec3& direction,
        GrenadeType type,
        uint32_t ownerId
    );

    /**
     * @brief Place a claymore
     */
    Grenade* PlaceClaymore(
        const glm::vec3& position,
        const glm::vec3& facing,
        uint32_t ownerId
    );

    /**
     * @brief Update all active grenades
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    /**
     * @brief Get all active grenades for collision/rendering
     */
    [[nodiscard]] const std::vector<Grenade>& GetGrenades() const { return m_grenades; }
    [[nodiscard]] std::vector<Grenade>& GetGrenades() { return m_grenades; }

    /**
     * @brief Get grenades that are ready to explode
     */
    [[nodiscard]] std::vector<Grenade*> GetExplodingGrenades();

    /**
     * @brief Get number of active grenades
     */
    [[nodiscard]] size_t GetActiveCount() const { return m_activeCount; }

    /**
     * @brief Clear all grenades
     */
    void Clear();

private:
    Grenade* FindInactive();

    std::vector<Grenade> m_grenades;
    size_t m_activeCount = 0;
    size_t m_maxGrenades;
};

// ============================================================================
// Explosion Manager
// ============================================================================

/**
 * @brief Manages explosion effects (visual/audio)
 */
class ExplosionManager {
public:
    static constexpr size_t kMaxExplosions = 20;

    ExplosionManager() = default;
    ~ExplosionManager() = default;

    /**
     * @brief Create explosion effect at position
     */
    void CreateExplosion(const glm::vec3& position, float radius, GrenadeType type);

    /**
     * @brief Update all explosion effects
     */
    void Update(float deltaTime);

    /**
     * @brief Get active explosions for rendering
     */
    [[nodiscard]] const std::vector<ExplosionEffect>& GetExplosions() const { return m_explosions; }

    /**
     * @brief Clear all explosions
     */
    void Clear() { m_explosions.clear(); }

private:
    std::vector<ExplosionEffect> m_explosions;
};

// ============================================================================
// Persistent Area Effects
// ============================================================================

/**
 * @brief Area effect from grenades (fire, smoke, etc.)
 */
struct AreaEffect {
    glm::vec3 position;
    float radius;
    GrenadeType sourceType;
    float duration;
    float timeRemaining;
    uint32_t ownerId;
    bool active = true;

    // Effect properties
    float damagePerSecond = 0.0f;   // For incendiary
    float slowAmount = 0.0f;         // For stun (0-1)

    [[nodiscard]] float GetProgress() const {
        return 1.0f - (timeRemaining / duration);
    }
};

class AreaEffectManager {
public:
    static constexpr size_t kMaxAreaEffects = 30;

    AreaEffectManager() = default;
    ~AreaEffectManager() = default;

    /**
     * @brief Create area effect from grenade
     */
    void CreateEffect(const Grenade& grenade);

    /**
     * @brief Update all area effects
     */
    void Update(float deltaTime);

    /**
     * @brief Check if position is in any damaging area
     * @return DPS at that position (0 if none)
     */
    [[nodiscard]] float GetDamageAtPosition(const glm::vec3& position) const;

    /**
     * @brief Check if position is in any slowing area
     * @return Slow multiplier (1.0 if none)
     */
    [[nodiscard]] float GetSlowAtPosition(const glm::vec3& position) const;

    /**
     * @brief Check if position is in flash range
     * @return Flash intensity (0 if none)
     */
    [[nodiscard]] float GetFlashAtPosition(const glm::vec3& position) const;

    /**
     * @brief Get all active effects for rendering
     */
    [[nodiscard]] const std::vector<AreaEffect>& GetEffects() const { return m_effects; }

    /**
     * @brief Clear all effects
     */
    void Clear() { m_effects.clear(); }

private:
    std::vector<AreaEffect> m_effects;
};

} // namespace Vehement
