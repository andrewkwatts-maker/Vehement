#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {

class Shader;
class Texture;

/**
 * @brief Individual particle data
 */
struct Particle {
    glm::vec3 position{0};
    glm::vec3 velocity{0};
    glm::vec4 color{1};
    float size = 1.0f;
    float lifetime = 0.0f;
    float maxLifetime = 1.0f;
    float rotation = 0.0f;
    float rotationSpeed = 0.0f;

    bool IsAlive() const { return lifetime < maxLifetime; }
    float GetLifeRatio() const { return lifetime / maxLifetime; }
};

/**
 * @brief Particle emitter configuration
 */
struct EmitterConfig {
    // Emission
    float emissionRate = 100.0f;      // Particles per second
    int burstCount = 0;                // Particles to emit instantly

    // Lifetime
    float lifetimeMin = 1.0f;
    float lifetimeMax = 2.0f;

    // Velocity
    glm::vec3 velocityMin{-1, 0, -1};
    glm::vec3 velocityMax{1, 5, 1};

    // Size
    float startSizeMin = 0.1f;
    float startSizeMax = 0.2f;
    float endSizeMin = 0.0f;
    float endSizeMax = 0.05f;

    // Color
    glm::vec4 startColor{1, 1, 1, 1};
    glm::vec4 endColor{1, 1, 1, 0};

    // Rotation
    float rotationMin = 0.0f;
    float rotationMax = 360.0f;
    float rotationSpeedMin = 0.0f;
    float rotationSpeedMax = 0.0f;

    // Physics
    glm::vec3 gravity{0, -9.8f, 0};
    float drag = 0.0f;
};

/**
 * @brief CPU-based particle system
 */
class ParticleSystem {
public:
    ParticleSystem();
    ~ParticleSystem();

    /**
     * @brief Initialize the particle system
     */
    bool Initialize(size_t maxParticles = 10000);

    /**
     * @brief Update all particles
     */
    void Update(float deltaTime, const glm::mat4& cameraTransform);

    /**
     * @brief Render particles
     */
    void Render(const glm::mat4& projectionView);

    /**
     * @brief Emit particles at position
     */
    void Emit(const glm::vec3& position, int count = 1);

    /**
     * @brief Emit a burst of particles
     */
    void EmitBurst(const glm::vec3& position, int count);

    /**
     * @brief Clear all particles
     */
    void Clear();

    // Configuration
    void SetConfig(const EmitterConfig& config) { m_config = config; }
    EmitterConfig& GetConfig() { return m_config; }
    const EmitterConfig& GetConfig() const { return m_config; }

    void SetTexture(std::shared_ptr<Texture> texture) { m_texture = std::move(texture); }
    void SetBlendAdditive(bool additive) { m_additiveBlend = additive; }

    // State
    void SetEmitting(bool emit) { m_emitting = emit; }
    bool IsEmitting() const { return m_emitting; }

    size_t GetActiveParticleCount() const { return m_activeCount; }
    size_t GetMaxParticles() const { return m_particles.size(); }

private:
    void EmitParticle(const glm::vec3& position);
    void SortParticles(const glm::vec3& cameraPosition);
    void UpdateBuffers();

    std::vector<Particle> m_particles;
    size_t m_activeCount = 0;

    EmitterConfig m_config;
    float m_emitAccumulator = 0.0f;
    bool m_emitting = true;
    bool m_additiveBlend = false;

    std::shared_ptr<Texture> m_texture;
    std::unique_ptr<Shader> m_shader;

    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    bool m_initialized = false;
};

} // namespace Nova
