#include "CombatSystem.hpp"
#include <algorithm>
#include <cmath>
#include <random>

namespace Vehement {

// ============================================================================
// Helper Functions
// ============================================================================

// Thread-local random engine for spread calculation
static std::mt19937& GetRandomEngine() {
    static thread_local std::mt19937 engine(std::random_device{}());
    return engine;
}

glm::vec3 ApplySpread(const glm::vec3& direction, float spread) {
    if (spread <= 0.0f) {
        return direction;
    }

    std::uniform_real_distribution<float> dist(-spread, spread);
    auto& engine = GetRandomEngine();

    // Generate random offset angles
    float offsetX = dist(engine);
    float offsetY = dist(engine);

    // Create perpendicular vectors
    glm::vec3 right = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
    if (glm::length(right) < 0.001f) {
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    glm::vec3 up = glm::normalize(glm::cross(right, direction));

    // Apply offset
    glm::vec3 spreadDir = direction + right * offsetX + up * offsetY;
    return glm::normalize(spreadDir);
}

// ============================================================================
// CombatSystem Implementation
// ============================================================================

CombatSystem::CombatSystem()
    : m_projectilePool(500)
    , m_grenadePool(50)
{
}

CombatSystem::~CombatSystem() {
    Shutdown();
}

bool CombatSystem::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Initialize tracer renderer
    if (!m_tracerRenderer.Initialize()) {
        return false;
    }

    m_playerStats.Reset();
    m_coinDrops.reserve(kMaxCoinDrops);

    m_initialized = true;
    return true;
}

void CombatSystem::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_tracerRenderer.Shutdown();
    m_projectilePool.Clear();
    m_grenadePool.Clear();
    m_bulletHoleManager.Clear();
    m_explosionManager.Clear();
    m_areaEffectManager.Clear();
    m_coinDrops.clear();

    m_initialized = false;
}

void CombatSystem::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update all subsystems
    UpdateProjectiles(deltaTime);
    UpdateGrenades(deltaTime);
    UpdateCoinDrops(deltaTime);
    UpdateAreaEffects(deltaTime);

    // Process hits and explosions
    ProcessProjectileHits();
    ProcessGrenadeExplosions();
    CheckClaymores();

    // Update visual effects
    m_bulletHoleManager.Update(deltaTime);
    m_explosionManager.Update(deltaTime);
}

void CombatSystem::UpdateProjectiles(float deltaTime) {
    m_projectilePool.Update(deltaTime);

    // Clear tracer renderer for new frame
    m_tracerRenderer.Clear();

    // Add tracers for active projectiles
    for (const auto& proj : m_projectilePool.GetProjectiles()) {
        if (proj.IsActive()) {
            glm::vec4 color = TracerRenderer::GetTracerColor(WeaponType::Glock);  // Default
            m_tracerRenderer.AddTracer(proj.GetTracerStart(), proj.GetTracerEnd(), color);
        }
    }
}

void CombatSystem::UpdateGrenades(float deltaTime) {
    m_grenadePool.Update(deltaTime);

    // Clear processed explosions set each frame
    m_processedExplosions.clear();
}

void CombatSystem::UpdateCoinDrops(float deltaTime) {
    for (auto& coin : m_coinDrops) {
        if (!coin.collected) {
            coin.age += deltaTime;
            // Bobbing animation
            coin.bobOffset = std::sin(coin.age * CoinDrop::kBobSpeed) * CoinDrop::kBobHeight;
        }
    }

    // Remove collected or expired coins
    m_coinDrops.erase(
        std::remove_if(m_coinDrops.begin(), m_coinDrops.end(),
            [](const CoinDrop& c) {
                return c.collected || c.age >= c.lifetime;
            }),
        m_coinDrops.end()
    );
}

void CombatSystem::UpdateAreaEffects(float deltaTime) {
    m_areaEffectManager.Update(deltaTime);
}

void CombatSystem::ProcessProjectileHits() {
    if (!m_collisionProvider) {
        return;
    }

    for (auto& proj : m_projectilePool.GetProjectiles()) {
        if (!proj.IsActive()) {
            continue;
        }

        HitResult hit = CheckProjectileCollision(proj);

        if (hit.hit) {
            if (hit.isWall) {
                // Create bullet hole
                m_bulletHoleManager.AddBulletHole(hit.hitPosition, hit.hitNormal);
                proj.Destroy();
            } else if (hit.isEnemy) {
                // Apply damage
                DamageEvent event;
                event.targetId = hit.entityId;
                event.sourceId = proj.GetOwnerId();
                event.damage = proj.GetDamage();
                event.hitPosition = hit.hitPosition;
                event.hitDirection = proj.GetDirection();
                event.weaponType = WeaponType::Glock;  // TODO: Track weapon type in projectile

                // Check for headshot (hit in upper 20% of entity)
                // This would need height information from entity

                ApplyDamage(event);

                // Update stats
                m_playerStats.shotsHit++;

                // Check if bullet should continue (penetration)
                if (!proj.ProcessHit()) {
                    proj.Destroy();
                }
            }
        }
    }
}

HitResult CombatSystem::CheckProjectileCollision(Projectile& projectile) {
    HitResult result;

    if (!m_collisionProvider) {
        return result;
    }

    glm::vec3 start = projectile.GetPreviousPosition();
    glm::vec3 end = projectile.GetPosition();
    glm::vec3 dir = end - start;
    float dist = glm::length(dir);

    if (dist < 0.001f) {
        return result;
    }

    dir = glm::normalize(dir);

    RaycastResult rayHit = m_collisionProvider->Raycast(
        start, dir, dist, projectile.GetOwnerId()
    );

    if (rayHit.hit) {
        result.hit = true;
        result.hitPosition = rayHit.hitPosition;
        result.hitNormal = rayHit.hitNormal;
        result.distance = rayHit.distance;
        result.entityId = rayHit.entityId;
        result.isWall = rayHit.hitWorld;
        result.isEnemy = rayHit.entityId != 0 && !rayHit.hitWorld;
    }

    return result;
}

void CombatSystem::ProcessGrenadeExplosions() {
    auto exploding = m_grenadePool.GetExplodingGrenades();

    for (Grenade* grenade : exploding) {
        // Skip if already processed
        if (m_processedExplosions.count(grenade) > 0) {
            continue;
        }
        m_processedExplosions.insert(grenade);

        const GrenadeStats& stats = grenade->GetStats();
        glm::vec3 pos = grenade->GetPosition();

        // Create visual explosion
        m_explosionManager.CreateExplosion(pos, stats.radius, grenade->GetType());

        // Apply damage based on type
        switch (grenade->GetType()) {
            case GrenadeType::Frag:
            case GrenadeType::Claymore:
                ApplyExplosionDamage(pos, stats.radius, stats.damage,
                                    grenade->GetOwnerId(), grenade->GetType());
                break;

            case GrenadeType::Flash:
            case GrenadeType::Stun:
            case GrenadeType::Smoke:
            case GrenadeType::Incendiary:
                // Create area effect
                m_areaEffectManager.CreateEffect(*grenade);
                // Apply initial damage for stun/incendiary
                if (stats.damage > 0.0f) {
                    ApplyExplosionDamage(pos, stats.radius, stats.damage,
                                        grenade->GetOwnerId(), grenade->GetType());
                }
                break;
        }
    }
}

void CombatSystem::CheckClaymores() {
    if (!m_collisionProvider) {
        return;
    }

    for (auto& grenade : m_grenadePool.GetGrenades()) {
        if (grenade.GetType() != GrenadeType::Claymore ||
            !grenade.IsArmed() ||
            grenade.IsTriggered()) {
            continue;
        }

        // Check for enemies in detection cone
        auto entities = m_collisionProvider->GetEntitiesInRadius(
            grenade.GetPosition(),
            grenade.GetTriggerRadius()
        );

        for (auto* entity : entities) {
            if (!entity->IsEnemy() || !entity->IsAlive()) {
                continue;
            }

            if (grenade.IsInDetectionCone(entity->GetPosition())) {
                grenade.Trigger();
                break;
            }
        }
    }
}

// -------------------------------------------------------------------------
// Weapon Actions
// -------------------------------------------------------------------------

bool CombatSystem::FireWeapon(
    Weapon& weapon,
    const glm::vec3& position,
    const glm::vec3& direction,
    uint32_t ownerId
) {
    if (!weapon.Fire()) {
        return false;
    }

    // Update stats
    m_playerStats.shotsFired++;

    // Spawn projectile
    SpawnProjectile(weapon, position, direction, ownerId);

    return true;
}

void CombatSystem::SpawnProjectile(
    const Weapon& weapon,
    const glm::vec3& position,
    const glm::vec3& direction,
    uint32_t ownerId
) {
    const WeaponStats& stats = weapon.GetStats();

    // Apply spread
    glm::vec3 spreadDir = ApplySpread(direction, stats.spread);

    // Create projectile
    Projectile* proj = m_projectilePool.Spawn(
        position,
        spreadDir,
        stats.bulletSpeed,
        stats.damage,
        stats.penetration,
        ownerId
    );

    if (proj) {
        proj->SetMaxRange(stats.range);

        // Set projectile type based on weapon
        if (weapon.GetType() == WeaponType::Sniper) {
            proj->SetType(ProjectileType::SniperRound);
        }
    }
}

Grenade* CombatSystem::ThrowGrenade(
    GrenadeType type,
    const glm::vec3& position,
    const glm::vec3& direction,
    uint32_t ownerId
) {
    return m_grenadePool.ThrowGrenade(position, direction, type, ownerId);
}

Grenade* CombatSystem::PlaceClaymore(
    const glm::vec3& position,
    const glm::vec3& facingDirection,
    uint32_t ownerId
) {
    return m_grenadePool.PlaceClaymore(position, facingDirection, ownerId);
}

// -------------------------------------------------------------------------
// Damage Application
// -------------------------------------------------------------------------

void CombatSystem::ApplyDamage(const DamageEvent& event) {
    if (!m_collisionProvider) {
        return;
    }

    // Get entity
    auto entities = m_collisionProvider->GetEntitiesInRadius(event.hitPosition, 0.1f);
    ICombatEntity* target = nullptr;

    for (auto* entity : entities) {
        if (entity->GetEntityId() == event.targetId) {
            target = entity;
            break;
        }
    }

    if (!target) {
        return;
    }

    // Apply headshot multiplier
    DamageEvent modifiedEvent = event;
    if (event.isHeadshot) {
        modifiedEvent.damage *= m_headshotMultiplier;
        m_playerStats.headshots++;
    }

    // Update stats
    m_playerStats.damageDealt += modifiedEvent.damage;

    // Apply damage to entity
    target->TakeDamage(modifiedEvent);

    // Apply knockback
    glm::vec3 knockback = CalculateKnockback(event.hitDirection, modifiedEvent.damage);
    target->ApplyKnockback(knockback);

    // Fire callback
    if (m_onDamage) {
        m_onDamage(modifiedEvent);
    }

    // Check if killed
    if (!target->IsAlive()) {
        NotifyKill(event.targetId, event.sourceId, event.hitPosition,
                  event.weaponType, event.isExplosion);
    }
}

void CombatSystem::ApplyExplosionDamage(
    const glm::vec3& center,
    float radius,
    float damage,
    uint32_t sourceId,
    GrenadeType grenadeType
) {
    if (!m_collisionProvider) {
        return;
    }

    auto entities = m_collisionProvider->GetEntitiesInRadius(center, radius);

    for (auto* entity : entities) {
        if (!entity->IsAlive()) {
            continue;
        }

        // Check friendly fire
        if (!m_friendlyFireEnabled && !entity->IsEnemy() &&
            entity->GetEntityId() != sourceId) {
            continue;
        }

        float distance = glm::length(entity->GetPosition() - center);
        float actualDamage = CalculateDamageFalloff(distance, radius, damage);

        if (actualDamage <= 0.0f) {
            continue;
        }

        DamageEvent event;
        event.targetId = entity->GetEntityId();
        event.sourceId = sourceId;
        event.damage = actualDamage;
        event.hitPosition = entity->GetPosition();
        event.hitDirection = glm::normalize(entity->GetPosition() - center);
        event.isExplosion = true;

        // Map grenade type to weapon type for stats
        switch (grenadeType) {
            case GrenadeType::Frag:
                event.weaponType = WeaponType::Grenade;
                break;
            case GrenadeType::Claymore:
                event.weaponType = WeaponType::Claymore;
                break;
            default:
                event.weaponType = WeaponType::Grenade;
                break;
        }

        // Apply damage
        entity->TakeDamage(event);

        // Update stats
        m_playerStats.damageDealt += actualDamage;

        // Apply knockback (stronger for explosions)
        glm::vec3 knockback = CalculateKnockback(event.hitDirection, actualDamage, 0.3f);
        entity->ApplyKnockback(knockback);

        // Apply status effects
        switch (grenadeType) {
            case GrenadeType::Flash:
                entity->ApplyStatusEffect(GrenadeType::Flash,
                    DefaultGrenadeStats::GetFlashStats().effectDuration,
                    DefaultGrenadeStats::GetFlashStats().effectStrength);
                break;
            case GrenadeType::Stun:
                entity->ApplyStatusEffect(GrenadeType::Stun,
                    DefaultGrenadeStats::GetStunStats().effectDuration,
                    DefaultGrenadeStats::GetStunStats().effectStrength);
                break;
            default:
                break;
        }

        // Fire callback
        if (m_onDamage) {
            m_onDamage(event);
        }

        // Check if killed
        if (!entity->IsAlive()) {
            NotifyKill(event.targetId, sourceId, entity->GetPosition(),
                      event.weaponType, true);
        }
    }
}

void CombatSystem::NotifyKill(
    uint32_t victimId,
    uint32_t killerId,
    const glm::vec3& position,
    WeaponType weaponType,
    bool isExplosion
) {
    // Update stats
    m_playerStats.kills++;
    if (isExplosion) {
        m_playerStats.grenadeKills++;
    }

    // Calculate coin drop
    int coins = GetKillCoinValue(false, isExplosion);  // TODO: Track headshots properly
    m_playerStats.coinsEarned += coins;

    // Drop coins
    DropCoins(position, coins);

    // Fire callback
    if (m_onKill) {
        KillEvent event;
        event.victimId = victimId;
        event.killerId = killerId;
        event.deathPosition = position;
        event.weaponType = weaponType;
        event.isExplosion = isExplosion;
        event.coinsDropped = coins;

        m_onKill(event);
    }
}

// -------------------------------------------------------------------------
// Coin System
// -------------------------------------------------------------------------

void CombatSystem::DropCoins(const glm::vec3& position, int amount) {
    // Remove oldest if at capacity
    while (m_coinDrops.size() >= kMaxCoinDrops) {
        m_coinDrops.erase(m_coinDrops.begin());
    }

    // Spread coins slightly
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
    auto& engine = GetRandomEngine();

    CoinDrop coin;
    coin.position = position + glm::vec3(dist(engine), 0.0f, dist(engine));
    coin.value = amount;
    coin.lifetime = 30.0f;
    coin.age = 0.0f;
    coin.bobOffset = 0.0f;
    coin.collected = false;

    m_coinDrops.push_back(coin);
}

int CombatSystem::CollectCoins(const glm::vec3& position, uint32_t collectorId) {
    int totalCollected = 0;

    for (auto& coin : m_coinDrops) {
        if (coin.collected) {
            continue;
        }

        float distance = glm::length(glm::vec3(coin.position.x, position.y, coin.position.z) -
                                     glm::vec3(position.x, position.y, position.z));

        if (distance <= CoinDrop::kCollectRadius) {
            coin.collected = true;
            totalCollected += coin.value;

            // Fire callback
            if (m_onCoinCollect) {
                m_onCoinCollect(collectorId, coin.value);
            }
        }
    }

    return totalCollected;
}

} // namespace Vehement
