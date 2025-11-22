#include "BaseProjectile.hpp"
#include "../lifecycle/ObjectFactory.hpp"
#include <algorithm>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// BaseProjectile Implementation
// ============================================================================

BaseProjectile::BaseProjectile() {
    m_components.Add<TransformComponent>();
}

BaseProjectile::~BaseProjectile() = default;

void BaseProjectile::OnCreate(const nlohmann::json& config) {
    ScriptedLifecycle::OnCreate(config);

    m_components.SetOwner(GetHandle());
    ParseProjectileConfig(config);
    m_components.InitializeAll();
}

void BaseProjectile::OnTick(float deltaTime) {
    ScriptedLifecycle::OnTick(deltaTime);

    if (!m_launched) return;

    // Update lifetime
    m_lifetime += deltaTime;
    if (m_lifetime >= m_stats.maxLifetime) {
        OnExpire();
        return;
    }

    // Update position based on type
    glm::vec3 oldPosition = m_position;

    switch (m_projectileType) {
        case ProjectileType::Linear:
            UpdateLinear(deltaTime);
            break;
        case ProjectileType::Homing:
        case ProjectileType::Seeking:
            UpdateHoming(deltaTime);
            break;
        case ProjectileType::Parabolic:
            UpdateParabolic(deltaTime);
            break;
        case ProjectileType::Instant:
            // Handled in Launch
            break;
        case ProjectileType::Bouncing:
            UpdateLinear(deltaTime);
            break;
    }

    // Update distance traveled
    m_distanceTraveled += glm::length(m_position - oldPosition);
    if (m_distanceTraveled >= m_stats.maxDistance) {
        OnExpire();
        return;
    }

    // Update transform component
    if (auto* transform = m_components.Get<TransformComponent>()) {
        transform->position = m_position;

        // Face movement direction
        if (glm::length(m_direction) > 0.001f) {
            transform->rotation.y = glm::degrees(atan2(m_direction.x, m_direction.z));
        }
    }

    // Check collisions
    if (CheckCollisions()) {
        return; // Projectile was destroyed by collision
    }
}

bool BaseProjectile::OnEvent(const GameEvent& event) {
    return ScriptedLifecycle::OnEvent(event);
}

void BaseProjectile::OnDestroy() {
    m_components.Clear();
    ScriptedLifecycle::OnDestroy();
}

void BaseProjectile::Launch(const glm::vec3& position, const glm::vec3& direction) {
    m_position = position;
    m_startPosition = position;
    m_direction = glm::normalize(direction);
    m_velocity = m_direction * m_stats.speed;
    m_launched = true;
    m_lifetime = 0.0f;
    m_distanceTraveled = 0.0f;
    m_pierceCount = 0;
    m_bounceCount = 0;
    m_hitTargets.clear();

    // Update transform
    if (auto* transform = m_components.Get<TransformComponent>()) {
        transform->position = m_position;
    }

    // Fire launch event
    GameEvent event(EventType::Launched, GetHandle());
    QueueEvent(std::move(event));

    // Handle instant hit
    if (m_projectileType == ProjectileType::Instant) {
        // Raycast and hit immediately
        // In a full implementation, this would use physics raycast
        OnExpire();
    }
}

void BaseProjectile::LaunchAt(const glm::vec3& position, const glm::vec3& targetPos) {
    glm::vec3 direction = targetPos - position;

    if (m_projectileType == ProjectileType::Parabolic) {
        // Calculate parabolic trajectory
        float distance = glm::length(glm::vec2(direction.x, direction.z));
        float heightDiff = targetPos.y - position.y;

        // Simple parabolic calculation
        float time = distance / m_stats.speed;
        direction.y = (heightDiff + 0.5f * m_stats.gravity * time * time) / time;
    }

    Launch(position, direction);
}

void BaseProjectile::LaunchAtTarget(const glm::vec3& position, LifecycleHandle target) {
    m_target = target;

    // Get target position
    auto& manager = GetGlobalLifecycleManager();
    if (auto* targetObj = manager.Get(target)) {
        // Get target transform
        // In a full implementation, access target's position
        glm::vec3 targetPos = position + glm::vec3(0, 0, 5); // Placeholder
        LaunchAt(position, targetPos);
    } else {
        Launch(position, glm::vec3(0, 0, 1));
    }
}

ScriptContext BaseProjectile::BuildContext() const {
    ScriptContext ctx = ScriptedLifecycle::BuildContext();
    ctx.entityType = "projectile";

    ctx.transform.x = m_position.x;
    ctx.transform.y = m_position.y;
    ctx.transform.z = m_position.z;

    return ctx;
}

void BaseProjectile::ParseProjectileConfig(const nlohmann::json& config) {
    (void)config;
    // Parse config in full implementation
}

void BaseProjectile::UpdateLinear(float deltaTime) {
    m_velocity = m_direction * m_stats.speed;
    m_position += m_velocity * deltaTime;
}

void BaseProjectile::UpdateHoming(float deltaTime) {
    // Get target position
    glm::vec3 targetPos = m_position + m_direction * 10.0f; // Default forward

    if (m_target.IsValid()) {
        auto& manager = GetGlobalLifecycleManager();
        if (auto* targetObj = manager.Get(m_target)) {
            // In full implementation, get actual target position
        }
    }

    // Calculate desired direction
    glm::vec3 desiredDir = glm::normalize(targetPos - m_position);

    // Smoothly turn towards target
    float turnAmount = glm::radians(m_stats.turnRate) * deltaTime;
    float currentAngle = atan2(m_direction.x, m_direction.z);
    float targetAngle = atan2(desiredDir.x, desiredDir.z);

    float angleDiff = targetAngle - currentAngle;
    // Normalize to -PI to PI
    while (angleDiff > 3.14159f) angleDiff -= 2 * 3.14159f;
    while (angleDiff < -3.14159f) angleDiff += 2 * 3.14159f;

    float turn = std::clamp(angleDiff, -turnAmount, turnAmount);
    float newAngle = currentAngle + turn;

    m_direction = glm::vec3(sin(newAngle), 0, cos(newAngle));

    // Apply acceleration
    float currentSpeed = glm::length(m_velocity);
    currentSpeed += m_stats.acceleration * deltaTime;
    currentSpeed = std::min(currentSpeed, m_stats.speed * 2.0f);

    m_velocity = m_direction * currentSpeed;
    m_position += m_velocity * deltaTime;
}

void BaseProjectile::UpdateParabolic(float deltaTime) {
    // Apply gravity
    m_velocity.y -= m_stats.gravity * deltaTime;

    m_position += m_velocity * deltaTime;

    // Update direction to match velocity
    if (glm::length(m_velocity) > 0.01f) {
        m_direction = glm::normalize(m_velocity);
    }

    // Check ground collision
    if (m_position.y <= 0.0f) {
        m_position.y = 0.0f;
        OnHit(LifecycleHandle::Invalid, m_position);
    }
}

bool BaseProjectile::CheckCollisions() {
    // In a full implementation, use physics collision detection
    // For now, simple distance check against all potential targets

    auto& manager = GetGlobalLifecycleManager();
    auto objects = manager.GetAll();

    for (auto* obj : objects) {
        // Skip self and source
        if (obj->GetHandle() == GetHandle()) continue;
        if (obj->GetHandle() == m_source) continue;

        // Skip already hit (for piercing)
        if (std::find(m_hitTargets.begin(), m_hitTargets.end(), obj->GetHandle())
            != m_hitTargets.end()) {
            continue;
        }

        // Check distance (simplified collision)
        // In full implementation, use proper collision shapes
        float collisionDist = m_stats.radius + 0.5f; // Assume 0.5 target radius

        // Get target position (would be from transform component)
        glm::vec3 targetPos = m_position + glm::vec3(100, 100, 100); // Placeholder

        if (glm::length(m_position - targetPos) < collisionDist) {
            OnHit(obj->GetHandle(), m_position);

            if (!m_stats.piercing || m_pierceCount >= m_stats.maxPierceCount) {
                return true; // Projectile destroyed
            }
        }
    }

    return false;
}

void BaseProjectile::OnHit(LifecycleHandle target, const glm::vec3& hitPos) {
    // Track hit for piercing
    if (target.IsValid()) {
        m_hitTargets.push_back(target);
        m_pierceCount++;
    }

    // Deal damage
    if (target.IsValid()) {
        DealDamage(target);

        // Apply knockback
        if (m_stats.knockback > 0.0f) {
            ApplyKnockback(target, m_direction);
        }
    }

    // Fire hit event
    GameEvent event(EventType::ProjectileHit, GetHandle(), target);
    PositionEventData posData;
    posData.position = hitPos;
    event.SetData(posData);
    QueueEvent(std::move(event));

    // Call callback
    if (m_onHit) {
        m_onHit(target, hitPos);
    }

    // Check for explosion
    if (m_stats.explosionRadius > 0.0f) {
        Explode();
    }

    // Destroy if not piercing or bounce
    if (!m_stats.piercing && m_stats.bounceCount == 0) {
        auto& manager = GetGlobalLifecycleManager();
        manager.Destroy(GetHandle(), false);
    } else if (m_projectileType == ProjectileType::Bouncing &&
               m_bounceCount < m_stats.bounceCount) {
        // Bounce
        m_bounceCount++;
        m_direction = glm::reflect(m_direction, glm::vec3(0, 1, 0)); // Simple ground bounce
        m_velocity = m_direction * m_stats.speed;

        GameEvent bounceEvent(EventType::Bounced, GetHandle());
        QueueEvent(std::move(bounceEvent));
    }
}

void BaseProjectile::OnExpire() {
    // Fire expire event
    GameEvent event(EventType::ProjectileExpired, GetHandle());
    QueueEvent(std::move(event));

    if (m_onExpire) {
        m_onExpire();
    }

    // Destroy
    auto& manager = GetGlobalLifecycleManager();
    manager.Destroy(GetHandle(), false);
}

void BaseProjectile::Explode() {
    // Fire explosion event
    GameEvent event(EventType::Exploded, GetHandle());
    PositionEventData posData;
    posData.position = m_position;
    event.SetData(posData);
    QueueEvent(std::move(event));

    // Deal AOE damage
    auto& manager = GetGlobalLifecycleManager();
    auto objects = manager.GetAll();

    for (auto* obj : objects) {
        if (obj->GetHandle() == GetHandle()) continue;
        if (obj->GetHandle() == m_source) continue;

        // Get distance (would use actual position)
        float distance = 0.0f; // Placeholder

        if (distance <= m_stats.explosionRadius) {
            // Damage falloff based on distance
            float falloff = 1.0f - (distance / m_stats.explosionRadius);
            float damage = m_stats.damage * falloff;

            DamageEventData dmgData;
            dmgData.amount = damage;
            dmgData.sourceHandle = m_source;
            dmgData.targetHandle = obj->GetHandle();
            dmgData.damageType = m_stats.damageType;

            GameEvent dmgEvent(EventType::Damaged, m_source, obj->GetHandle());
            dmgEvent.SetData(dmgData);
            manager.SendEvent(obj->GetHandle(), dmgEvent);
        }
    }
}

void BaseProjectile::DealDamage(LifecycleHandle target) {
    DamageEventData data;
    data.amount = m_stats.damage;
    data.sourceHandle = m_source;
    data.targetHandle = target;
    data.damageType = m_stats.damageType;
    data.hitPosition = m_position;
    data.hitNormal = -m_direction;

    GameEvent event(EventType::Damaged, m_source, target);
    event.SetData(data);

    auto& manager = GetGlobalLifecycleManager();
    manager.SendEvent(target, event);
}

void BaseProjectile::ApplyKnockback(LifecycleHandle target, const glm::vec3& direction) {
    // In a full implementation, apply force to target's physics/movement
    (void)target;
    (void)direction;
}

// ============================================================================
// Factory Registration
// ============================================================================

namespace {
    struct BaseProjectileRegistrar {
        BaseProjectileRegistrar() {
            GetGlobalObjectFactory().RegisterType<BaseProjectile>("projectile");
        }
    };
    static BaseProjectileRegistrar g_baseProjectileRegistrar;
}

} // namespace Lifecycle
} // namespace Vehement
