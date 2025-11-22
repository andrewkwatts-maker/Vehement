#include "Projectile.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {

// ============================================================================
// Projectile Implementation
// ============================================================================

Projectile::Projectile() = default;

void Projectile::Initialize(
    const glm::vec3& position,
    const glm::vec3& direction,
    float speed,
    float damage,
    int penetration,
    uint32_t ownerId
) {
    m_position = position;
    m_previousPosition = position;
    m_direction = glm::normalize(direction);
    m_speed = speed;
    m_velocity = m_direction * m_speed;
    m_damage = damage;
    m_penetration = penetration;
    m_ownerId = ownerId;

    m_distanceTraveled = 0.0f;
    m_lifetime = 0.0f;
    m_active = true;
}

void Projectile::Update(float deltaTime) {
    if (!m_active) return;

    // Store previous position for collision detection
    m_previousPosition = m_position;

    // Move projectile
    glm::vec3 displacement = m_velocity * deltaTime;
    m_position += displacement;

    // Track distance
    float distThisFrame = glm::length(displacement);
    m_distanceTraveled += distThisFrame;

    // Update lifetime
    m_lifetime += deltaTime;

    // Check if should destroy
    if (ShouldDestroy()) {
        m_active = false;
    }
}

bool Projectile::ShouldDestroy() const {
    // Exceeded max range
    if (m_distanceTraveled >= m_maxRange) {
        return true;
    }

    // Exceeded max lifetime
    if (m_lifetime >= m_maxLifetime) {
        return true;
    }

    // No penetration left
    if (m_penetration <= 0) {
        return true;
    }

    return false;
}

bool Projectile::ProcessHit() {
    m_penetration--;

    // Reduce damage after penetration (30% reduction per hit)
    m_damage *= 0.7f;

    return m_penetration > 0;
}

glm::vec3 Projectile::GetTracerStart() const {
    // Tracer starts behind current position
    return m_position - m_direction * m_tracerLength;
}

glm::vec3 Projectile::GetTracerEnd() const {
    return m_position;
}

// ============================================================================
// ProjectilePool Implementation
// ============================================================================

ProjectilePool::ProjectilePool(size_t maxProjectiles)
    : m_maxProjectiles(maxProjectiles)
{
    m_projectiles.reserve(maxProjectiles);
}

Projectile* ProjectilePool::Spawn(
    const glm::vec3& position,
    const glm::vec3& direction,
    float speed,
    float damage,
    int penetration,
    uint32_t ownerId
) {
    // First, try to find an inactive projectile to reuse
    for (auto& proj : m_projectiles) {
        if (!proj.IsActive()) {
            proj.Initialize(position, direction, speed, damage, penetration, ownerId);
            m_activeCount++;
            return &proj;
        }
    }

    // If no inactive found, add new if under limit
    if (m_projectiles.size() < m_maxProjectiles) {
        m_projectiles.emplace_back();
        auto& proj = m_projectiles.back();
        proj.Initialize(position, direction, speed, damage, penetration, ownerId);
        m_activeCount++;
        return &proj;
    }

    // Pool is full
    return nullptr;
}

void ProjectilePool::Update(float deltaTime) {
    m_activeCount = 0;

    for (auto& proj : m_projectiles) {
        if (proj.IsActive()) {
            proj.Update(deltaTime);
            if (proj.IsActive()) {
                m_activeCount++;
            }
        }
    }
}

void ProjectilePool::Clear() {
    m_projectiles.clear();
    m_activeCount = 0;
}

void ProjectilePool::Compact() {
    // Remove all inactive projectiles
    m_projectiles.erase(
        std::remove_if(m_projectiles.begin(), m_projectiles.end(),
            [](const Projectile& p) { return !p.IsActive(); }),
        m_projectiles.end()
    );

    m_activeCount = m_projectiles.size();
}

// ============================================================================
// TracerRenderer Implementation
// ============================================================================

TracerRenderer::TracerRenderer() {
    m_vertices.reserve(kMaxTracers * 2);  // 2 vertices per tracer
}

TracerRenderer::~TracerRenderer() {
    Shutdown();
}

bool TracerRenderer::Initialize() {
    // Note: Actual OpenGL initialization would happen here
    // For now, we just mark as initialized

    m_initialized = true;
    return true;
}

void TracerRenderer::Shutdown() {
    if (m_vao != 0) {
        // glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo != 0) {
        // glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    m_initialized = false;
}

void TracerRenderer::AddTracer(
    const glm::vec3& start,
    const glm::vec3& end,
    const glm::vec4& color,
    float width
) {
    if (m_vertices.size() >= kMaxTracers * 2) {
        return;  // Buffer full
    }

    TracerVertex v1, v2;
    v1.position = start;
    v1.color = color;
    v1.width = width;

    v2.position = end;
    v2.color = glm::vec4(color.r, color.g, color.b, 0.0f);  // Fade at end
    v2.width = width * 0.5f;  // Taper

    m_vertices.push_back(v1);
    m_vertices.push_back(v2);
}

void TracerRenderer::Render(const glm::mat4& viewProjection) {
    if (m_vertices.empty()) return;

    // Note: Actual rendering would use OpenGL/shader here
    // This is a placeholder for the render implementation

    /*
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    m_vertices.size() * sizeof(TracerVertex),
                    m_vertices.data());

    // Use line shader with viewProjection
    m_shader->Use();
    m_shader->SetMat4("u_ViewProjection", viewProjection);

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertices.size()));
    glBindVertexArray(0);
    */
}

void TracerRenderer::Clear() {
    m_vertices.clear();
}

glm::vec4 TracerRenderer::GetTracerColor(WeaponType type) {
    switch (type) {
        case WeaponType::Glock:
            return glm::vec4(1.0f, 0.9f, 0.5f, 1.0f);   // Yellow
        case WeaponType::AK47:
            return glm::vec4(1.0f, 0.6f, 0.2f, 1.0f);   // Orange
        case WeaponType::Sniper:
            return glm::vec4(0.3f, 1.0f, 0.3f, 1.0f);   // Green
        default:
            return glm::vec4(1.0f, 1.0f, 0.5f, 1.0f);   // Default yellow
    }
}

// ============================================================================
// BulletHoleManager Implementation
// ============================================================================

void BulletHoleManager::AddBulletHole(
    const glm::vec3& position,
    const glm::vec3& normal,
    float size
) {
    // If at capacity, remove oldest
    if (m_bulletHoles.size() >= kMaxBulletHoles) {
        m_bulletHoles.erase(m_bulletHoles.begin());
    }

    BulletHole hole;
    hole.position = position;
    hole.normal = normal;
    hole.size = size;
    hole.lifetime = 10.0f;
    hole.fadeTime = 2.0f;
    hole.age = 0.0f;

    m_bulletHoles.push_back(hole);
}

void BulletHoleManager::Update(float deltaTime) {
    for (auto& hole : m_bulletHoles) {
        hole.age += deltaTime;
    }

    // Remove expired bullet holes
    m_bulletHoles.erase(
        std::remove_if(m_bulletHoles.begin(), m_bulletHoles.end(),
            [](const BulletHole& h) {
                return h.age >= h.lifetime + h.fadeTime;
            }),
        m_bulletHoles.end()
    );
}

} // namespace Vehement
