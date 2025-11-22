#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <unordered_set>

#include "Weapon.hpp"
#include "Projectile.hpp"
#include "Grenade.hpp"

namespace Vehement {

// ============================================================================
// Combat Events
// ============================================================================

/**
 * @brief Event fired when an entity takes damage
 */
struct DamageEvent {
    uint32_t targetId = 0;          // Who was hit
    uint32_t sourceId = 0;          // Who dealt damage
    float damage = 0.0f;            // Damage amount
    glm::vec3 hitPosition{0.0f};    // Where the hit occurred
    glm::vec3 hitDirection{0.0f};   // Direction of attack
    bool isHeadshot = false;        // Was it a headshot
    bool isExplosion = false;       // Explosion damage
    WeaponType weaponType = WeaponType::Glock;
};

/**
 * @brief Event fired when an entity dies
 */
struct KillEvent {
    uint32_t victimId = 0;          // Who died
    uint32_t killerId = 0;          // Who killed them
    glm::vec3 deathPosition{0.0f};  // Where they died
    WeaponType weaponType = WeaponType::Glock;
    bool isExplosion = false;
    int coinsDropped = 0;           // Coins dropped on death
};

/**
 * @brief Combat statistics for a player
 */
struct CombatStats {
    int kills = 0;
    int deaths = 0;
    int headshots = 0;
    int shotsFired = 0;
    int shotsHit = 0;
    int grenadeKills = 0;
    float damageDealt = 0.0f;
    float damageTaken = 0.0f;
    int coinsEarned = 0;

    [[nodiscard]] float GetAccuracy() const {
        return shotsFired > 0 ? static_cast<float>(shotsHit) / shotsFired : 0.0f;
    }

    [[nodiscard]] float GetKDRatio() const {
        return deaths > 0 ? static_cast<float>(kills) / deaths : static_cast<float>(kills);
    }

    void Reset() {
        kills = deaths = headshots = shotsFired = shotsHit = grenadeKills = 0;
        damageDealt = damageTaken = 0.0f;
        coinsEarned = 0;
    }
};

// ============================================================================
// Entity Interface for Combat
// ============================================================================

/**
 * @brief Interface for entities that can participate in combat
 */
class ICombatEntity {
public:
    virtual ~ICombatEntity() = default;

    [[nodiscard]] virtual uint32_t GetEntityId() const = 0;
    [[nodiscard]] virtual glm::vec3 GetPosition() const = 0;
    [[nodiscard]] virtual float GetRadius() const = 0;           // Collision radius
    [[nodiscard]] virtual float GetHeight() const = 0;           // For headshot detection
    [[nodiscard]] virtual bool IsAlive() const = 0;
    [[nodiscard]] virtual bool IsEnemy() const = 0;              // Zombie vs player

    virtual void TakeDamage(const DamageEvent& event) = 0;
    virtual void ApplyKnockback(const glm::vec3& force) = 0;
    virtual void ApplyStatusEffect(GrenadeType effectType, float duration, float strength) = 0;
};

// ============================================================================
// Collision Interface
// ============================================================================

/**
 * @brief Result of a raycast
 */
struct RaycastResult {
    bool hit = false;
    glm::vec3 hitPosition{0.0f};
    glm::vec3 hitNormal{0.0f};
    float distance = 0.0f;
    uint32_t entityId = 0;
    bool hitWorld = false;
};

/**
 * @brief Interface for collision detection (provided by game)
 */
class ICollisionProvider {
public:
    virtual ~ICollisionProvider() = default;

    /**
     * @brief Cast a ray and return the first hit
     */
    [[nodiscard]] virtual RaycastResult Raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance,
        uint32_t ignoreEntity = 0
    ) const = 0;

    /**
     * @brief Check if a point is inside world geometry
     */
    [[nodiscard]] virtual bool IsPointInWorld(const glm::vec3& point) const = 0;

    /**
     * @brief Get entities within radius
     */
    [[nodiscard]] virtual std::vector<ICombatEntity*> GetEntitiesInRadius(
        const glm::vec3& center,
        float radius
    ) const = 0;
};

// ============================================================================
// Coin Drop
// ============================================================================

/**
 * @brief Coin entity dropped by killed zombies
 */
struct CoinDrop {
    glm::vec3 position;
    int value = 10;
    float lifetime = 30.0f;         // Seconds before disappearing
    float age = 0.0f;
    float bobOffset = 0.0f;         // For bobbing animation
    bool collected = false;

    static constexpr float kCollectRadius = 2.0f;
    static constexpr float kBobSpeed = 3.0f;
    static constexpr float kBobHeight = 0.2f;
};

// ============================================================================
// Combat System
// ============================================================================

/**
 * @brief Central system managing all combat interactions
 */
class CombatSystem {
public:
    CombatSystem();
    ~CombatSystem();

    // Delete copy, allow move
    CombatSystem(const CombatSystem&) = delete;
    CombatSystem& operator=(const CombatSystem&) = delete;
    CombatSystem(CombatSystem&&) noexcept = default;
    CombatSystem& operator=(CombatSystem&&) noexcept = default;

    /**
     * @brief Initialize the combat system
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update all combat subsystems
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    /**
     * @brief Set collision provider for hit detection
     */
    void SetCollisionProvider(ICollisionProvider* provider) { m_collisionProvider = provider; }

    // -------------------------------------------------------------------------
    // Weapon Actions
    // -------------------------------------------------------------------------

    /**
     * @brief Fire a weapon from position in direction
     * @param weapon The weapon being fired
     * @param position Muzzle position
     * @param direction Fire direction (normalized)
     * @param ownerId Entity firing the weapon
     * @return true if weapon fired
     */
    bool FireWeapon(
        Weapon& weapon,
        const glm::vec3& position,
        const glm::vec3& direction,
        uint32_t ownerId
    );

    /**
     * @brief Throw a grenade
     * @param type Type of grenade
     * @param position Throw position
     * @param direction Throw direction
     * @param ownerId Entity throwing
     * @return Pointer to grenade, or nullptr if failed
     */
    Grenade* ThrowGrenade(
        GrenadeType type,
        const glm::vec3& position,
        const glm::vec3& direction,
        uint32_t ownerId
    );

    /**
     * @brief Place a claymore mine
     * @param position Placement position
     * @param facingDirection Direction the mine faces
     * @param ownerId Entity placing
     * @return Pointer to claymore, or nullptr if failed
     */
    Grenade* PlaceClaymore(
        const glm::vec3& position,
        const glm::vec3& facingDirection,
        uint32_t ownerId
    );

    // -------------------------------------------------------------------------
    // Damage Application
    // -------------------------------------------------------------------------

    /**
     * @brief Apply damage to an entity
     */
    void ApplyDamage(const DamageEvent& event);

    /**
     * @brief Apply explosion damage to all entities in radius
     */
    void ApplyExplosionDamage(
        const glm::vec3& center,
        float radius,
        float damage,
        uint32_t sourceId,
        GrenadeType grenadeType
    );

    // -------------------------------------------------------------------------
    // Coin System
    // -------------------------------------------------------------------------

    /**
     * @brief Drop coins at position
     */
    void DropCoins(const glm::vec3& position, int amount);

    /**
     * @brief Try to collect coins near position
     * @param position Collector position
     * @param collectorId ID of collecting entity
     * @return Coins collected
     */
    int CollectCoins(const glm::vec3& position, uint32_t collectorId);

    /**
     * @brief Get all coin drops for rendering
     */
    [[nodiscard]] const std::vector<CoinDrop>& GetCoinDrops() const { return m_coinDrops; }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    [[nodiscard]] ProjectilePool& GetProjectilePool() { return m_projectilePool; }
    [[nodiscard]] const ProjectilePool& GetProjectilePool() const { return m_projectilePool; }

    [[nodiscard]] GrenadePool& GetGrenadePool() { return m_grenadePool; }
    [[nodiscard]] const GrenadePool& GetGrenadePool() const { return m_grenadePool; }

    [[nodiscard]] TracerRenderer& GetTracerRenderer() { return m_tracerRenderer; }
    [[nodiscard]] BulletHoleManager& GetBulletHoleManager() { return m_bulletHoleManager; }
    [[nodiscard]] ExplosionManager& GetExplosionManager() { return m_explosionManager; }
    [[nodiscard]] AreaEffectManager& GetAreaEffectManager() { return m_areaEffectManager; }

    [[nodiscard]] CombatStats& GetPlayerStats() { return m_playerStats; }
    [[nodiscard]] const CombatStats& GetPlayerStats() const { return m_playerStats; }

    // -------------------------------------------------------------------------
    // Event Callbacks
    // -------------------------------------------------------------------------

    using DamageCallback = std::function<void(const DamageEvent&)>;
    using KillCallback = std::function<void(const KillEvent&)>;
    using CoinCallback = std::function<void(uint32_t entityId, int amount)>;

    void SetOnDamage(DamageCallback callback) { m_onDamage = std::move(callback); }
    void SetOnKill(KillCallback callback) { m_onKill = std::move(callback); }
    void SetOnCoinCollect(CoinCallback callback) { m_onCoinCollect = std::move(callback); }

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    void SetHeadshotMultiplier(float mult) { m_headshotMultiplier = mult; }
    void SetFriendlyFire(bool enabled) { m_friendlyFireEnabled = enabled; }

private:
    void UpdateProjectiles(float deltaTime);
    void UpdateGrenades(float deltaTime);
    void UpdateCoinDrops(float deltaTime);
    void UpdateAreaEffects(float deltaTime);

    void ProcessProjectileHits();
    void ProcessGrenadeExplosions();
    void CheckClaymores();

    void SpawnProjectile(
        const Weapon& weapon,
        const glm::vec3& position,
        const glm::vec3& direction,
        uint32_t ownerId
    );

    HitResult CheckProjectileCollision(Projectile& projectile);

    void NotifyKill(uint32_t victimId, uint32_t killerId, const glm::vec3& position,
                    WeaponType weaponType, bool isExplosion);

    // Subsystems
    ProjectilePool m_projectilePool;
    GrenadePool m_grenadePool;
    TracerRenderer m_tracerRenderer;
    BulletHoleManager m_bulletHoleManager;
    ExplosionManager m_explosionManager;
    AreaEffectManager m_areaEffectManager;

    // Coin drops
    std::vector<CoinDrop> m_coinDrops;
    static constexpr size_t kMaxCoinDrops = 100;

    // Stats
    CombatStats m_playerStats;

    // Configuration
    float m_headshotMultiplier = 2.0f;
    bool m_friendlyFireEnabled = false;

    // External provider
    ICollisionProvider* m_collisionProvider = nullptr;

    // Callbacks
    DamageCallback m_onDamage;
    KillCallback m_onKill;
    CoinCallback m_onCoinCollect;

    // Track processed grenades to avoid double-processing
    std::unordered_set<Grenade*> m_processedExplosions;

    bool m_initialized = false;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Calculate damage falloff based on distance
 */
inline float CalculateDamageFalloff(float distance, float maxRange, float baseDamage) {
    if (distance >= maxRange) return 0.0f;
    float falloff = 1.0f - (distance / maxRange);
    return baseDamage * falloff * falloff;  // Quadratic falloff
}

/**
 * @brief Calculate spread direction with random offset
 */
glm::vec3 ApplySpread(const glm::vec3& direction, float spread);

/**
 * @brief Calculate knockback force from hit
 */
inline glm::vec3 CalculateKnockback(const glm::vec3& hitDir, float damage, float knockbackScale = 0.1f) {
    return glm::normalize(hitDir) * damage * knockbackScale;
}

/**
 * @brief Get coin value for killing a zombie
 */
inline int GetKillCoinValue(bool isHeadshot, bool isExplosion) {
    int base = 10;
    if (isHeadshot) base += 5;
    if (isExplosion) base += 3;
    return base;
}

} // namespace Vehement
