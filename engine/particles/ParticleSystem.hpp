#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <glm/glm.hpp>

namespace Nova {

class Shader;
class Texture;

// ============================================================================
// Constants
// ============================================================================

namespace ParticleConstants {
    constexpr size_t kDefaultMaxParticles = 10000;
    constexpr size_t kMinPoolSize = 256;
    constexpr float kDefaultEmissionRate = 100.0f;
    constexpr float kDefaultGravity = -9.8f;
    constexpr float kDefaultLifetimeMin = 1.0f;
    constexpr float kDefaultLifetimeMax = 2.0f;
}

// ============================================================================
// Emitter Configuration
// ============================================================================

/**
 * @brief Particle emitter configuration
 *
 * Defines all parameters for particle emission and behavior.
 * Use builder-style setters for convenient configuration.
 */
struct EmitterConfig {
    // Emission parameters
    float emissionRate = ParticleConstants::kDefaultEmissionRate;
    int burstCount = 0;

    // Lifetime range
    float lifetimeMin = ParticleConstants::kDefaultLifetimeMin;
    float lifetimeMax = ParticleConstants::kDefaultLifetimeMax;

    // Velocity range (per axis)
    glm::vec3 velocityMin{-1.0f, 0.0f, -1.0f};
    glm::vec3 velocityMax{1.0f, 5.0f, 1.0f};

    // Size over lifetime
    float startSizeMin = 0.1f;
    float startSizeMax = 0.2f;
    float endSizeMin = 0.0f;
    float endSizeMax = 0.05f;

    // Color over lifetime
    glm::vec4 startColor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 endColor{1.0f, 1.0f, 1.0f, 0.0f};

    // Rotation parameters
    float rotationMin = 0.0f;
    float rotationMax = 360.0f;
    float rotationSpeedMin = 0.0f;
    float rotationSpeedMax = 0.0f;

    // Physics parameters
    glm::vec3 gravity{0.0f, ParticleConstants::kDefaultGravity, 0.0f};
    float drag = 0.0f;

    // Builder-style setters for convenient configuration
    EmitterConfig& SetEmissionRate(float rate) { emissionRate = rate; return *this; }
    EmitterConfig& SetBurstCount(int count) { burstCount = count; return *this; }
    EmitterConfig& SetLifetime(float min, float max) { lifetimeMin = min; lifetimeMax = max; return *this; }
    EmitterConfig& SetVelocity(const glm::vec3& min, const glm::vec3& max) { velocityMin = min; velocityMax = max; return *this; }
    EmitterConfig& SetStartSize(float min, float max) { startSizeMin = min; startSizeMax = max; return *this; }
    EmitterConfig& SetEndSize(float min, float max) { endSizeMin = min; endSizeMax = max; return *this; }
    EmitterConfig& SetStartColor(const glm::vec4& color) { startColor = color; return *this; }
    EmitterConfig& SetEndColor(const glm::vec4& color) { endColor = color; return *this; }
    EmitterConfig& SetRotation(float min, float max) { rotationMin = min; rotationMax = max; return *this; }
    EmitterConfig& SetRotationSpeed(float min, float max) { rotationSpeedMin = min; rotationSpeedMax = max; return *this; }
    EmitterConfig& SetGravity(const glm::vec3& g) { gravity = g; return *this; }
    EmitterConfig& SetDrag(float d) { drag = d; return *this; }
};

// ============================================================================
// Particle Data - SoA Layout for Cache Efficiency
// ============================================================================

/**
 * @brief Struct-of-Arrays particle data for cache-efficient updates
 *
 * SoA layout allows for better SIMD utilization and cache locality
 * when iterating over individual particle properties.
 */
struct ParticleData {
    // Position and velocity (updated together)
    std::vector<float> positionX;
    std::vector<float> positionY;
    std::vector<float> positionZ;
    std::vector<float> velocityX;
    std::vector<float> velocityY;
    std::vector<float> velocityZ;

    // Lifetime data
    std::vector<float> lifetime;
    std::vector<float> maxLifetime;

    // Visual properties
    std::vector<float> colorR;
    std::vector<float> colorG;
    std::vector<float> colorB;
    std::vector<float> colorA;
    std::vector<float> size;
    std::vector<float> startSize;
    std::vector<float> endSize;

    // Rotation
    std::vector<float> rotation;
    std::vector<float> rotationSpeed;

    /**
     * @brief Resize all arrays to accommodate maxParticles
     */
    void Resize(size_t maxParticles) {
        positionX.resize(maxParticles);
        positionY.resize(maxParticles);
        positionZ.resize(maxParticles);
        velocityX.resize(maxParticles);
        velocityY.resize(maxParticles);
        velocityZ.resize(maxParticles);
        lifetime.resize(maxParticles);
        maxLifetime.resize(maxParticles);
        colorR.resize(maxParticles);
        colorG.resize(maxParticles);
        colorB.resize(maxParticles);
        colorA.resize(maxParticles);
        size.resize(maxParticles);
        startSize.resize(maxParticles);
        endSize.resize(maxParticles);
        rotation.resize(maxParticles);
        rotationSpeed.resize(maxParticles);
    }

    /**
     * @brief Clear all data (reset sizes to 0)
     */
    void Clear() {
        positionX.clear();
        positionY.clear();
        positionZ.clear();
        velocityX.clear();
        velocityY.clear();
        velocityZ.clear();
        lifetime.clear();
        maxLifetime.clear();
        colorR.clear();
        colorG.clear();
        colorB.clear();
        colorA.clear();
        size.clear();
        startSize.clear();
        endSize.clear();
        rotation.clear();
        rotationSpeed.clear();
    }

    /**
     * @brief Swap particle at index i with particle at index j
     */
    void Swap(size_t i, size_t j) {
        std::swap(positionX[i], positionX[j]);
        std::swap(positionY[i], positionY[j]);
        std::swap(positionZ[i], positionZ[j]);
        std::swap(velocityX[i], velocityX[j]);
        std::swap(velocityY[i], velocityY[j]);
        std::swap(velocityZ[i], velocityZ[j]);
        std::swap(lifetime[i], lifetime[j]);
        std::swap(maxLifetime[i], maxLifetime[j]);
        std::swap(colorR[i], colorR[j]);
        std::swap(colorG[i], colorG[j]);
        std::swap(colorB[i], colorB[j]);
        std::swap(colorA[i], colorA[j]);
        std::swap(size[i], size[j]);
        std::swap(startSize[i], startSize[j]);
        std::swap(endSize[i], endSize[j]);
        std::swap(rotation[i], rotation[j]);
        std::swap(rotationSpeed[i], rotationSpeed[j]);
    }
};

// ============================================================================
// GPU Vertex Data
// ============================================================================

/**
 * @brief Vertex data for GPU rendering (packed for efficient upload)
 */
struct alignas(16) ParticleVertex {
    glm::vec3 position;
    float size;
    glm::vec4 color;
    float rotation;
    float _padding[3]; // Align to 48 bytes for GPU
};

// ============================================================================
// Particle System
// ============================================================================

/**
 * @brief CPU-based particle system with SoA data layout
 *
 * Features:
 * - Struct-of-Arrays (SoA) data layout for cache efficiency
 * - Pre-allocated vertex buffer to avoid per-frame allocations
 * - Configurable emitter with builder-style API
 * - Depth-sorted rendering for proper transparency
 */
class ParticleSystem {
public:
    ParticleSystem();
    ~ParticleSystem();

    // Prevent copying (OpenGL resources)
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    // Allow moving
    ParticleSystem(ParticleSystem&& other) noexcept;
    ParticleSystem& operator=(ParticleSystem&& other) noexcept;

    /**
     * @brief Initialize the particle system
     * @param maxParticles Maximum number of concurrent particles
     * @return true if initialization succeeded
     */
    bool Initialize(size_t maxParticles = ParticleConstants::kDefaultMaxParticles);

    /**
     * @brief Shutdown and release resources
     */
    void Shutdown();

    /**
     * @brief Update all particles (physics and lifetime)
     * @param deltaTime Time since last frame in seconds
     */
    void Update(float deltaTime);

    /**
     * @brief Sort particles by distance to camera (for proper transparency)
     * @param cameraPosition World-space camera position
     */
    void SortByDepth(const glm::vec3& cameraPosition);

    /**
     * @brief Upload particle data to GPU
     */
    void UploadToGPU();

    /**
     * @brief Render particles
     * @param projectionView Combined projection-view matrix
     * @param cameraRight Camera right vector for billboarding
     * @param cameraUp Camera up vector for billboarding
     */
    void Render(const glm::mat4& projectionView,
                const glm::vec3& cameraRight,
                const glm::vec3& cameraUp);

    /**
     * @brief Combined update and render (convenience method)
     * @param deltaTime Time since last frame
     * @param cameraTransform Camera world transform matrix
     * @param projectionView Combined projection-view matrix
     */
    void UpdateAndRender(float deltaTime,
                         const glm::mat4& cameraTransform,
                         const glm::mat4& projectionView);

    /**
     * @brief Emit particles at a specific position
     * @param position World-space emission position
     * @param count Number of particles to emit
     */
    void Emit(const glm::vec3& position, int count = 1);

    /**
     * @brief Emit a burst of particles
     * @param position World-space emission position
     * @param count Number of particles in burst
     */
    void EmitBurst(const glm::vec3& position, int count);

    /**
     * @brief Clear all active particles
     */
    void Clear();

    // Configuration
    void SetConfig(const EmitterConfig& config) { m_config = config; }
    EmitterConfig& GetConfig() { return m_config; }
    const EmitterConfig& GetConfig() const { return m_config; }

    void SetTexture(std::shared_ptr<Texture> texture) { m_texture = std::move(texture); }
    void SetBlendAdditive(bool additive) { m_additiveBlend = additive; }
    void SetEmitterPosition(const glm::vec3& pos) { m_emitterPosition = pos; }

    // Emission control
    void SetEmitting(bool emit) { m_emitting = emit; }
    bool IsEmitting() const { return m_emitting; }
    void ResetEmissionAccumulator() { m_emitAccumulator = 0.0f; }

    // State queries
    size_t GetActiveParticleCount() const { return m_activeCount; }
    size_t GetMaxParticles() const { return m_maxParticles; }
    bool IsInitialized() const { return m_initialized; }

    // Direct access to particle data (for advanced usage)
    const ParticleData& GetParticleData() const { return m_particles; }

private:
    void EmitSingleParticle(const glm::vec3& position);
    void RemoveDeadParticles();
    void UpdatePhysics(float deltaTime);
    void UpdateVisuals(float deltaTime);

    // Particle data (SoA layout)
    ParticleData m_particles;
    size_t m_activeCount = 0;
    size_t m_maxParticles = 0;

    // Pre-allocated vertex buffer (avoids per-frame allocation)
    std::vector<ParticleVertex> m_vertexBuffer;

    // Sort indices (for depth sorting without moving particle data)
    std::vector<uint32_t> m_sortIndices;
    std::vector<float> m_sortDistances;

    // Configuration
    EmitterConfig m_config;
    glm::vec3 m_emitterPosition{0.0f};
    float m_emitAccumulator = 0.0f;
    bool m_emitting = true;
    bool m_additiveBlend = false;

    // GPU resources
    std::shared_ptr<Texture> m_texture;
    std::unique_ptr<Shader> m_shader;
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    bool m_initialized = false;
};

} // namespace Nova
