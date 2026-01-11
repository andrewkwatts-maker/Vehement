#include "particles/ParticleSystem.hpp"
#include "math/Random.hpp"
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace Nova {

// ============================================================================
// Emitter Configuration Presets
// ============================================================================

namespace EmitterPresets {

/**
 * @brief Create a fire effect emitter configuration
 */
EmitterConfig Fire() {
    return EmitterConfig()
        .SetEmissionRate(150.0f)
        .SetLifetime(0.5f, 1.5f)
        .SetVelocity(glm::vec3(-0.3f, 2.0f, -0.3f), glm::vec3(0.3f, 4.0f, 0.3f))
        .SetStartSize(0.3f, 0.5f)
        .SetEndSize(0.0f, 0.1f)
        .SetStartColor(glm::vec4(1.0f, 0.8f, 0.2f, 1.0f))
        .SetEndColor(glm::vec4(1.0f, 0.2f, 0.0f, 0.0f))
        .SetGravity(glm::vec3(0.0f, 1.0f, 0.0f))
        .SetDrag(0.5f);
}

/**
 * @brief Create a smoke effect emitter configuration
 */
EmitterConfig Smoke() {
    return EmitterConfig()
        .SetEmissionRate(50.0f)
        .SetLifetime(2.0f, 4.0f)
        .SetVelocity(glm::vec3(-0.5f, 1.0f, -0.5f), glm::vec3(0.5f, 2.0f, 0.5f))
        .SetStartSize(0.2f, 0.4f)
        .SetEndSize(1.0f, 2.0f)
        .SetStartColor(glm::vec4(0.3f, 0.3f, 0.3f, 0.8f))
        .SetEndColor(glm::vec4(0.5f, 0.5f, 0.5f, 0.0f))
        .SetGravity(glm::vec3(0.0f, 0.5f, 0.0f))
        .SetDrag(0.8f);
}

/**
 * @brief Create a spark/explosion effect emitter configuration
 */
EmitterConfig Sparks() {
    return EmitterConfig()
        .SetEmissionRate(0.0f)  // Use burst mode
        .SetBurstCount(50)
        .SetLifetime(0.3f, 0.8f)
        .SetVelocity(glm::vec3(-5.0f, -5.0f, -5.0f), glm::vec3(5.0f, 5.0f, 5.0f))
        .SetStartSize(0.05f, 0.1f)
        .SetEndSize(0.0f, 0.02f)
        .SetStartColor(glm::vec4(1.0f, 1.0f, 0.5f, 1.0f))
        .SetEndColor(glm::vec4(1.0f, 0.5f, 0.0f, 0.0f))
        .SetGravity(glm::vec3(0.0f, -9.8f, 0.0f))
        .SetDrag(0.1f);
}

/**
 * @brief Create a water fountain effect emitter configuration
 */
EmitterConfig Fountain() {
    return EmitterConfig()
        .SetEmissionRate(200.0f)
        .SetLifetime(1.0f, 2.0f)
        .SetVelocity(glm::vec3(-1.0f, 8.0f, -1.0f), glm::vec3(1.0f, 12.0f, 1.0f))
        .SetStartSize(0.1f, 0.15f)
        .SetEndSize(0.05f, 0.1f)
        .SetStartColor(glm::vec4(0.6f, 0.8f, 1.0f, 0.8f))
        .SetEndColor(glm::vec4(0.8f, 0.9f, 1.0f, 0.0f))
        .SetGravity(glm::vec3(0.0f, -9.8f, 0.0f))
        .SetDrag(0.2f);
}

/**
 * @brief Create a snow effect emitter configuration
 */
EmitterConfig Snow() {
    return EmitterConfig()
        .SetEmissionRate(100.0f)
        .SetLifetime(5.0f, 10.0f)
        .SetVelocity(glm::vec3(-0.5f, -1.0f, -0.5f), glm::vec3(0.5f, -0.5f, 0.5f))
        .SetStartSize(0.05f, 0.1f)
        .SetEndSize(0.05f, 0.1f)
        .SetStartColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f))
        .SetEndColor(glm::vec4(1.0f, 1.0f, 1.0f, 0.0f))
        .SetGravity(glm::vec3(0.0f, -0.5f, 0.0f))
        .SetRotationSpeed(-45.0f, 45.0f)
        .SetDrag(0.3f);
}

/**
 * @brief Create a magic/sparkle effect emitter configuration
 */
EmitterConfig Magic() {
    return EmitterConfig()
        .SetEmissionRate(75.0f)
        .SetLifetime(0.5f, 1.5f)
        .SetVelocity(glm::vec3(-2.0f, -2.0f, -2.0f), glm::vec3(2.0f, 2.0f, 2.0f))
        .SetStartSize(0.1f, 0.2f)
        .SetEndSize(0.0f, 0.0f)
        .SetStartColor(glm::vec4(0.8f, 0.4f, 1.0f, 1.0f))
        .SetEndColor(glm::vec4(0.2f, 0.8f, 1.0f, 0.0f))
        .SetGravity(glm::vec3(0.0f, 0.0f, 0.0f))
        .SetRotation(0.0f, 360.0f)
        .SetRotationSpeed(-180.0f, 180.0f)
        .SetDrag(0.5f);
}

/**
 * @brief Create a dust/debris effect emitter configuration
 */
EmitterConfig Dust() {
    return EmitterConfig()
        .SetEmissionRate(30.0f)
        .SetLifetime(2.0f, 4.0f)
        .SetVelocity(glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f))
        .SetStartSize(0.05f, 0.15f)
        .SetEndSize(0.1f, 0.3f)
        .SetStartColor(glm::vec4(0.6f, 0.5f, 0.4f, 0.6f))
        .SetEndColor(glm::vec4(0.6f, 0.5f, 0.4f, 0.0f))
        .SetGravity(glm::vec3(0.0f, -0.5f, 0.0f))
        .SetDrag(0.9f);
}

} // namespace EmitterPresets

// ============================================================================
// Emission Shape Utilities
// ============================================================================

namespace EmissionShapes {

/**
 * @brief Generate a random point on a sphere surface
 */
glm::vec3 OnSphere(float radius) {
    const float theta = Random::Range(0.0f, glm::two_pi<float>());
    const float phi = std::acos(Random::Range(-1.0f, 1.0f));
    const float sinPhi = std::sin(phi);
    return glm::vec3(
        radius * sinPhi * std::cos(theta),
        radius * sinPhi * std::sin(theta),
        radius * std::cos(phi)
    );
}

/**
 * @brief Generate a random point inside a sphere
 */
glm::vec3 InSphere(float radius) {
    // Cube root for uniform distribution
    const float r = radius * std::cbrt(Random::Value());
    return glm::normalize(OnSphere(1.0f)) * r;
}

/**
 * @brief Generate a random point on a circle (XZ plane)
 */
glm::vec3 OnCircle(float radius) {
    const float angle = Random::Range(0.0f, glm::two_pi<float>());
    return glm::vec3(
        radius * std::cos(angle),
        0.0f,
        radius * std::sin(angle)
    );
}

/**
 * @brief Generate a random point inside a circle (XZ plane)
 */
glm::vec3 InCircle(float radius) {
    const float r = radius * std::sqrt(Random::Value());
    const float angle = Random::Range(0.0f, glm::two_pi<float>());
    return glm::vec3(
        r * std::cos(angle),
        0.0f,
        r * std::sin(angle)
    );
}

/**
 * @brief Generate a random point inside a box
 */
glm::vec3 InBox(const glm::vec3& halfExtents) {
    return glm::vec3(
        Random::Range(-halfExtents.x, halfExtents.x),
        Random::Range(-halfExtents.y, halfExtents.y),
        Random::Range(-halfExtents.z, halfExtents.z)
    );
}

/**
 * @brief Generate a random point on a cone surface
 * @param height Cone height
 * @param angle Cone angle in radians
 */
glm::vec3 OnCone(float height, float angle) {
    const float azimuth = Random::Range(0.0f, glm::two_pi<float>());
    const float t = Random::Value();
    const float radius = t * height * std::tan(angle);
    return glm::vec3(
        radius * std::cos(azimuth),
        t * height,
        radius * std::sin(azimuth)
    );
}

/**
 * @brief Generate a random direction within a cone
 * @param direction Base direction (normalized)
 * @param angle Maximum angle from direction in radians
 */
glm::vec3 DirectionInCone(const glm::vec3& direction, float angle) {
    // Generate random point in cone around +Y
    const float cosAngle = std::cos(angle);
    const float z = Random::Range(cosAngle, 1.0f);
    const float phi = Random::Range(0.0f, glm::two_pi<float>());
    const float sinTheta = std::sqrt(1.0f - z * z);

    glm::vec3 localDir(
        sinTheta * std::cos(phi),
        z,
        sinTheta * std::sin(phi)
    );

    // Rotate to align with desired direction
    if (std::abs(direction.y - 1.0f) < 0.001f) {
        return localDir;
    }
    if (std::abs(direction.y + 1.0f) < 0.001f) {
        return -localDir;
    }

    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    const glm::vec3 right = glm::normalize(glm::cross(up, direction));
    const glm::vec3 forward = glm::cross(direction, right);

    return localDir.x * right + localDir.y * direction + localDir.z * forward;
}

} // namespace EmissionShapes

} // namespace Nova
