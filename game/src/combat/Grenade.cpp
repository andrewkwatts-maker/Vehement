#include "Grenade.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {

// ============================================================================
// Grenade Implementation
// ============================================================================

Grenade::Grenade() = default;

void Grenade::InitializeThrown(
    const glm::vec3& position,
    const glm::vec3& direction,
    GrenadeType type,
    uint32_t ownerId
) {
    m_position = position;
    m_type = type;
    m_ownerId = ownerId;
    m_state = GrenadeState::InFlight;
    m_stats = DefaultGrenadeStats::GetStats(type);

    // Calculate throw velocity with arc
    glm::vec3 throwDir = glm::normalize(direction);
    // Add upward component for arc
    throwDir.y += 0.3f;
    throwDir = glm::normalize(throwDir);
    m_velocity = throwDir * m_stats.throwSpeed;

    m_facingDirection = glm::normalize(glm::vec3(direction.x, 0.0f, direction.z));
    m_fuseTimer = m_stats.fuseTime;
    m_explosionTimer = 0.0f;
    m_lifetime = 0.0f;
    m_onGround = false;
    m_triggered = false;
}

void Grenade::InitializePlaced(
    const glm::vec3& position,
    const glm::vec3& facingDirection,
    uint32_t ownerId
) {
    m_position = position;
    m_type = GrenadeType::Claymore;
    m_ownerId = ownerId;
    m_state = GrenadeState::Armed;  // Immediately armed when placed
    m_stats = DefaultGrenadeStats::GetClaymoreStats();

    m_velocity = glm::vec3(0.0f);
    m_facingDirection = glm::normalize(glm::vec3(facingDirection.x, 0.0f, facingDirection.z));
    m_fuseTimer = m_stats.fuseTime;  // Time to explode after trigger
    m_explosionTimer = 0.0f;
    m_lifetime = 0.0f;
    m_onGround = true;
    m_triggered = false;
}

void Grenade::Update(float deltaTime) {
    if (m_state == GrenadeState::Expired) {
        return;
    }

    m_lifetime += deltaTime;

    switch (m_state) {
        case GrenadeState::InFlight: {
            // Apply gravity
            m_velocity.y -= kGravity * deltaTime;

            // Apply air drag
            m_velocity *= kAirDrag;

            // Move
            m_position += m_velocity * deltaTime;

            // Check if landed (simple ground check at y=0)
            if (m_position.y <= 0.0f) {
                m_position.y = 0.0f;
                m_onGround = true;
                TransitionToArmed();
            }

            // Update fuse
            m_fuseTimer -= deltaTime;
            if (m_fuseTimer <= 0.0f) {
                Explode();
            }
            break;
        }

        case GrenadeState::Armed: {
            // Apply ground friction if moving
            if (m_onGround && glm::length(m_velocity) > 0.01f) {
                m_velocity *= kGroundFriction;
                m_position += m_velocity * deltaTime;
            }

            // For claymores, wait for trigger
            if (m_type == GrenadeType::Claymore) {
                if (m_triggered) {
                    m_fuseTimer -= deltaTime;
                    if (m_fuseTimer <= 0.0f) {
                        Explode();
                    }
                }
                // Otherwise wait indefinitely
            } else {
                // Regular grenades count down
                m_fuseTimer -= deltaTime;
                if (m_fuseTimer <= 0.0f) {
                    Explode();
                }
            }
            break;
        }

        case GrenadeState::Exploding: {
            m_explosionTimer += deltaTime;
            if (m_explosionTimer >= kExplosionDuration) {
                TransitionToExpired();
            }
            break;
        }

        case GrenadeState::Expired:
            // Do nothing
            break;
    }
}

void Grenade::Trigger() {
    if (m_type == GrenadeType::Claymore && m_state == GrenadeState::Armed && !m_triggered) {
        m_triggered = true;
        m_fuseTimer = m_stats.fuseTime;  // Start countdown
    }
}

bool Grenade::ShouldExplode() const {
    return m_state == GrenadeState::Armed && m_fuseTimer <= 0.0f;
}

void Grenade::Explode() {
    if (m_state != GrenadeState::Exploding && m_state != GrenadeState::Expired) {
        TransitionToExploding();
    }
}

void Grenade::TransitionToArmed() {
    m_state = GrenadeState::Armed;
}

void Grenade::TransitionToExploding() {
    m_state = GrenadeState::Exploding;
    m_explosionTimer = 0.0f;
}

void Grenade::TransitionToExpired() {
    m_state = GrenadeState::Expired;
}

float Grenade::GetFuseProgress() const {
    if (m_type == GrenadeType::Claymore && !m_triggered) {
        return 0.0f;  // Not counting down yet
    }
    float totalFuse = DefaultGrenadeStats::GetStats(m_type).fuseTime;
    return 1.0f - (m_fuseTimer / totalFuse);
}

void Grenade::Bounce(const glm::vec3& normal, float bounciness) {
    // Reflect velocity
    m_velocity = m_velocity - 2.0f * glm::dot(m_velocity, normal) * normal;

    // Apply bounciness
    m_velocity *= bounciness;

    // Stop if velocity is very low
    if (glm::length(m_velocity) < 0.5f) {
        m_velocity = glm::vec3(0.0f);
    }
}

const char* Grenade::GetTexturePath() const {
    using namespace WeaponTextures;

    switch (m_type) {
        case GrenadeType::Frag:
            return kGrenadeGreen;
        case GrenadeType::Flash:
            return kFlashNade;
        case GrenadeType::Stun:
            return kStunNade;
        case GrenadeType::Smoke:
            return kGrenadeGrey;
        case GrenadeType::Incendiary:
            return kGrenadeRed;
        case GrenadeType::Claymore:
            return kClaymore;
        default:
            return kGrenadeGreen;
    }
}

bool Grenade::IsInDetectionCone(const glm::vec3& targetPos) const {
    // Only claymores have detection cones
    if (m_type != GrenadeType::Claymore) {
        return false;
    }

    glm::vec3 toTarget = targetPos - m_position;
    float distance = glm::length(toTarget);

    // Check distance
    if (distance > m_triggerRadius || distance < 0.1f) {
        return false;
    }

    // Check angle
    toTarget = glm::normalize(toTarget);
    float dot = glm::dot(toTarget, m_facingDirection);
    float angleRad = std::acos(dot);
    float angleDeg = glm::degrees(angleRad);

    return angleDeg <= m_detectionAngle;
}

// ============================================================================
// GrenadePool Implementation
// ============================================================================

GrenadePool::GrenadePool(size_t maxGrenades)
    : m_maxGrenades(maxGrenades)
{
    m_grenades.reserve(maxGrenades);
}

Grenade* GrenadePool::FindInactive() {
    for (auto& grenade : m_grenades) {
        if (grenade.IsExpired()) {
            return &grenade;
        }
    }
    return nullptr;
}

Grenade* GrenadePool::ThrowGrenade(
    const glm::vec3& position,
    const glm::vec3& direction,
    GrenadeType type,
    uint32_t ownerId
) {
    // Try to reuse inactive grenade
    Grenade* grenade = FindInactive();

    if (!grenade && m_grenades.size() < m_maxGrenades) {
        m_grenades.emplace_back();
        grenade = &m_grenades.back();
    }

    if (grenade) {
        grenade->InitializeThrown(position, direction, type, ownerId);
        m_activeCount++;
        return grenade;
    }

    return nullptr;
}

Grenade* GrenadePool::PlaceClaymore(
    const glm::vec3& position,
    const glm::vec3& facing,
    uint32_t ownerId
) {
    // Try to reuse inactive grenade
    Grenade* grenade = FindInactive();

    if (!grenade && m_grenades.size() < m_maxGrenades) {
        m_grenades.emplace_back();
        grenade = &m_grenades.back();
    }

    if (grenade) {
        grenade->InitializePlaced(position, facing, ownerId);
        m_activeCount++;
        return grenade;
    }

    return nullptr;
}

void GrenadePool::Update(float deltaTime) {
    m_activeCount = 0;

    for (auto& grenade : m_grenades) {
        if (!grenade.IsExpired()) {
            grenade.Update(deltaTime);
            if (!grenade.IsExpired()) {
                m_activeCount++;
            }
        }
    }
}

std::vector<Grenade*> GrenadePool::GetExplodingGrenades() {
    std::vector<Grenade*> exploding;

    for (auto& grenade : m_grenades) {
        if (grenade.IsExploding()) {
            exploding.push_back(&grenade);
        }
    }

    return exploding;
}

void GrenadePool::Clear() {
    m_grenades.clear();
    m_activeCount = 0;
}

// ============================================================================
// ExplosionManager Implementation
// ============================================================================

void ExplosionManager::CreateExplosion(
    const glm::vec3& position,
    float radius,
    GrenadeType type
) {
    // Remove oldest if at capacity
    if (m_explosions.size() >= kMaxExplosions) {
        m_explosions.erase(m_explosions.begin());
    }

    ExplosionEffect explosion;
    explosion.position = position;
    explosion.radius = radius;
    explosion.type = type;
    explosion.lifetime = 0.0f;
    explosion.active = true;

    // Set duration based on type
    switch (type) {
        case GrenadeType::Frag:
            explosion.maxLifetime = 0.5f;
            break;
        case GrenadeType::Flash:
            explosion.maxLifetime = 0.3f;
            break;
        case GrenadeType::Stun:
            explosion.maxLifetime = 0.4f;
            break;
        case GrenadeType::Incendiary:
            explosion.maxLifetime = 0.8f;
            break;
        case GrenadeType::Claymore:
            explosion.maxLifetime = 0.4f;
            break;
        default:
            explosion.maxLifetime = 0.5f;
            break;
    }

    m_explosions.push_back(explosion);
}

void ExplosionManager::Update(float deltaTime) {
    for (auto& explosion : m_explosions) {
        if (explosion.active) {
            explosion.lifetime += deltaTime;
            if (explosion.lifetime >= explosion.maxLifetime) {
                explosion.active = false;
            }
        }
    }

    // Remove inactive
    m_explosions.erase(
        std::remove_if(m_explosions.begin(), m_explosions.end(),
            [](const ExplosionEffect& e) { return !e.active; }),
        m_explosions.end()
    );
}

// ============================================================================
// AreaEffectManager Implementation
// ============================================================================

void AreaEffectManager::CreateEffect(const Grenade& grenade) {
    // Only certain grenade types create persistent effects
    GrenadeType type = grenade.GetType();
    if (type != GrenadeType::Smoke &&
        type != GrenadeType::Incendiary &&
        type != GrenadeType::Stun &&
        type != GrenadeType::Flash) {
        return;
    }

    // Remove oldest if at capacity
    if (m_effects.size() >= kMaxAreaEffects) {
        m_effects.erase(m_effects.begin());
    }

    const GrenadeStats& stats = grenade.GetStats();

    AreaEffect effect;
    effect.position = grenade.GetPosition();
    effect.radius = stats.radius;
    effect.sourceType = type;
    effect.duration = stats.effectDuration;
    effect.timeRemaining = stats.effectDuration;
    effect.ownerId = grenade.GetOwnerId();
    effect.active = true;

    // Set effect properties based on type
    switch (type) {
        case GrenadeType::Incendiary:
            effect.damagePerSecond = stats.damage;
            break;
        case GrenadeType::Stun:
            effect.slowAmount = stats.effectStrength;
            break;
        default:
            break;
    }

    m_effects.push_back(effect);
}

void AreaEffectManager::Update(float deltaTime) {
    for (auto& effect : m_effects) {
        if (effect.active) {
            effect.timeRemaining -= deltaTime;
            if (effect.timeRemaining <= 0.0f) {
                effect.active = false;
            }
        }
    }

    // Remove inactive
    m_effects.erase(
        std::remove_if(m_effects.begin(), m_effects.end(),
            [](const AreaEffect& e) { return !e.active; }),
        m_effects.end()
    );
}

float AreaEffectManager::GetDamageAtPosition(const glm::vec3& position) const {
    float totalDPS = 0.0f;

    for (const auto& effect : m_effects) {
        if (!effect.active || effect.damagePerSecond <= 0.0f) {
            continue;
        }

        float distance = glm::length(position - effect.position);
        if (distance <= effect.radius) {
            // Linear falloff from center
            float falloff = 1.0f - (distance / effect.radius);
            totalDPS += effect.damagePerSecond * falloff;
        }
    }

    return totalDPS;
}

float AreaEffectManager::GetSlowAtPosition(const glm::vec3& position) const {
    float maxSlow = 0.0f;

    for (const auto& effect : m_effects) {
        if (!effect.active || effect.slowAmount <= 0.0f) {
            continue;
        }

        float distance = glm::length(position - effect.position);
        if (distance <= effect.radius) {
            // Linear falloff from center
            float falloff = 1.0f - (distance / effect.radius);
            float slow = effect.slowAmount * falloff;
            maxSlow = std::max(maxSlow, slow);
        }
    }

    // Return speed multiplier (1.0 = full speed, 0.0 = stopped)
    return 1.0f - maxSlow;
}

float AreaEffectManager::GetFlashAtPosition(const glm::vec3& position) const {
    float maxFlash = 0.0f;

    for (const auto& effect : m_effects) {
        if (!effect.active || effect.sourceType != GrenadeType::Flash) {
            continue;
        }

        float distance = glm::length(position - effect.position);
        if (distance <= effect.radius) {
            // Sharp falloff for flash
            float falloff = 1.0f - std::pow(distance / effect.radius, 2.0f);
            // Also fade over time
            float timeFade = effect.timeRemaining / effect.duration;
            maxFlash = std::max(maxFlash, falloff * timeFade);
        }
    }

    return maxFlash;
}

} // namespace Vehement
