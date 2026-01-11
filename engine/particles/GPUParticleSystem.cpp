#include "particles/ParticleSystem.hpp"
#include "graphics/Shader.hpp"
#include "graphics/Texture.hpp"
#include "math/Random.hpp"

#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <glm/gtc/type_ptr.hpp>

namespace Nova {

// ============================================================================
// GPU Particle System
// ============================================================================
//
// This implementation uses compute shaders for particle simulation,
// enabling handling of hundreds of thousands of particles efficiently.
// The design uses double-buffered SSBOs for particle data to allow
// concurrent read/write operations.

namespace GPUParticleConstants {
    constexpr size_t kDefaultMaxParticles = 100000;
    constexpr uint32_t kWorkGroupSize = 256;
}

// ============================================================================
// Compute Shader Source
// ============================================================================

static const char* const GPU_PARTICLE_COMPUTE_SHADER = R"(
#version 460 core

layout(local_size_x = 256) in;

struct Particle {
    vec4 positionLife;   // xyz = position, w = lifetime
    vec4 velocityMaxLife; // xyz = velocity, w = maxLifetime
    vec4 color;
    vec4 sizeRotation;   // x = size, y = startSize, z = endSize, w = rotation
    float rotationSpeed;
    float _pad0;
    float _pad1;
    float _pad2;
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

layout(std430, binding = 1) buffer AliveCountBuffer {
    uint aliveCount;
    uint emitCount;
    uint _pad0;
    uint _pad1;
};

uniform float u_DeltaTime;
uniform vec3 u_Gravity;
uniform float u_Drag;
uniform vec4 u_StartColor;
uniform vec4 u_EndColor;

void main() {
    uint index = gl_GlobalInvocationID.x;

    if (index >= aliveCount) {
        return;
    }

    Particle p = particles[index];

    // Update lifetime
    p.positionLife.w += u_DeltaTime;

    // Check if dead
    if (p.positionLife.w >= p.velocityMaxLife.w) {
        // Mark as dead by setting maxLifetime to 0
        // Dead particles will be removed in a separate pass
        p.velocityMaxLife.w = 0.0;
        particles[index] = p;
        return;
    }

    // Apply physics
    vec3 velocity = p.velocityMaxLife.xyz;
    velocity += u_Gravity * u_DeltaTime;
    velocity *= (1.0 - u_Drag * u_DeltaTime);
    p.velocityMaxLife.xyz = velocity;

    // Update position
    p.positionLife.xyz += velocity * u_DeltaTime;

    // Update rotation
    p.sizeRotation.w += p.rotationSpeed * u_DeltaTime;

    // Calculate life ratio and interpolate visuals
    float t = p.positionLife.w / p.velocityMaxLife.w;

    // Interpolate color
    p.color = mix(u_StartColor, u_EndColor, t);

    // Interpolate size
    p.sizeRotation.x = mix(p.sizeRotation.y, p.sizeRotation.z, t);

    particles[index] = p;
}
)";

// ============================================================================
// Render Shader Sources
// ============================================================================

static const char* const GPU_PARTICLE_VERTEX_SHADER = R"(
#version 460 core

struct Particle {
    vec4 positionLife;
    vec4 velocityMaxLife;
    vec4 color;
    vec4 sizeRotation;
    float rotationSpeed;
    float _pad0;
    float _pad1;
    float _pad2;
};

layout(std430, binding = 0) readonly buffer ParticleBuffer {
    Particle particles[];
};

uniform mat4 u_ProjectionView;
uniform vec3 u_CameraRight;
uniform vec3 u_CameraUp;

out vec4 v_Color;
out vec2 v_TexCoord;

void main() {
    uint particleIndex = gl_VertexID / 4;
    uint vertexIndex = gl_VertexID % 4;

    Particle p = particles[particleIndex];

    // Skip dead particles
    if (p.velocityMaxLife.w <= 0.0) {
        gl_Position = vec4(0.0);
        v_Color = vec4(0.0);
        return;
    }

    v_Color = p.color;

    // Billboard quad vertices
    const vec2 quadVertices[4] = vec2[](
        vec2(-0.5, -0.5),
        vec2( 0.5, -0.5),
        vec2( 0.5,  0.5),
        vec2(-0.5,  0.5)
    );

    vec2 vertex = quadVertices[vertexIndex];

    // Apply rotation
    float s = sin(p.sizeRotation.w);
    float c = cos(p.sizeRotation.w);
    vec2 rotated = vec2(
        vertex.x * c - vertex.y * s,
        vertex.x * s + vertex.y * c
    );

    v_TexCoord = vertex + 0.5;

    // Billboard in world space
    float size = p.sizeRotation.x;
    vec3 worldPos = p.positionLife.xyz +
        u_CameraRight * rotated.x * size +
        u_CameraUp * rotated.y * size;

    gl_Position = u_ProjectionView * vec4(worldPos, 1.0);
}
)";

static const char* const GPU_PARTICLE_FRAGMENT_SHADER = R"(
#version 460 core

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform bool u_HasTexture;

out vec4 FragColor;

void main() {
    vec4 texColor = u_HasTexture ? texture(u_Texture, v_TexCoord) : vec4(1.0);
    FragColor = v_Color * texColor;

    if (FragColor.a < 0.01) {
        discard;
    }
}
)";

// ============================================================================
// GPUParticleSystem Class
// ============================================================================

/**
 * @brief GPU-accelerated particle system using compute shaders
 *
 * This class provides a high-performance particle system that runs entirely
 * on the GPU, suitable for very large particle counts (100k+).
 */
class GPUParticleSystem {
public:
    GPUParticleSystem() = default;
    ~GPUParticleSystem() { Shutdown(); }

    // Non-copyable
    GPUParticleSystem(const GPUParticleSystem&) = delete;
    GPUParticleSystem& operator=(const GPUParticleSystem&) = delete;

    /**
     * @brief Initialize the GPU particle system
     */
    bool Initialize(size_t maxParticles = GPUParticleConstants::kDefaultMaxParticles) {
        if (m_initialized) {
            return true;
        }

        m_maxParticles = maxParticles;

        // Create compute shader
        m_computeShader = std::make_unique<Shader>();
        if (!m_computeShader->LoadComputeShader(GPU_PARTICLE_COMPUTE_SHADER)) {
            spdlog::error("Failed to create GPU particle compute shader");
            return false;
        }

        // Create render shader
        m_renderShader = std::make_unique<Shader>();
        if (!m_renderShader->LoadFromSource(GPU_PARTICLE_VERTEX_SHADER, GPU_PARTICLE_FRAGMENT_SHADER)) {
            spdlog::error("Failed to create GPU particle render shader");
            return false;
        }

        // Create particle SSBO
        glGenBuffers(1, &m_particleSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     static_cast<GLsizeiptr>(maxParticles * sizeof(GPUParticle)),
                     nullptr,
                     GL_DYNAMIC_DRAW);

        // Create counter buffer
        glGenBuffers(1, &m_counterBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_counterBuffer);
        uint32_t initialCounters[4] = {0, 0, 0, 0};
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(initialCounters), initialCounters, GL_DYNAMIC_DRAW);

        // Create VAO for rendering (empty, we use gl_VertexID)
        glGenVertexArrays(1, &m_vao);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        m_initialized = true;
        spdlog::info("GPUParticleSystem initialized with {} max particles", maxParticles);
        return true;
    }

    /**
     * @brief Shutdown and release resources
     */
    void Shutdown() {
        if (m_particleSSBO != 0) {
            glDeleteBuffers(1, &m_particleSSBO);
            m_particleSSBO = 0;
        }
        if (m_counterBuffer != 0) {
            glDeleteBuffers(1, &m_counterBuffer);
            m_counterBuffer = 0;
        }
        if (m_vao != 0) {
            glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        }
        m_computeShader.reset();
        m_renderShader.reset();
        m_initialized = false;
    }

    /**
     * @brief Update particles using compute shader
     */
    void Update(float deltaTime) {
        if (!m_initialized || m_activeCount == 0) {
            return;
        }

        // Bind compute shader
        m_computeShader->Bind();

        // Set uniforms
        m_computeShader->SetFloat("u_DeltaTime", deltaTime);
        m_computeShader->SetVec3("u_Gravity", m_config.gravity);
        m_computeShader->SetFloat("u_Drag", m_config.drag);
        m_computeShader->SetVec4("u_StartColor", m_config.startColor);
        m_computeShader->SetVec4("u_EndColor", m_config.endColor);

        // Bind SSBOs
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_particleSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_counterBuffer);

        // Dispatch compute
        const uint32_t numGroups = (static_cast<uint32_t>(m_activeCount) + GPUParticleConstants::kWorkGroupSize - 1)
                                   / GPUParticleConstants::kWorkGroupSize;
        glDispatchCompute(numGroups, 1, 1);

        // Memory barrier before rendering
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    }

    /**
     * @brief Render particles
     */
    void Render(const glm::mat4& projectionView, const glm::vec3& cameraRight, const glm::vec3& cameraUp) {
        if (!m_initialized || m_activeCount == 0) {
            return;
        }

        // Set blend mode
        glEnable(GL_BLEND);
        if (m_additiveBlend) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        } else {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        glDepthMask(GL_FALSE);

        // Bind render shader
        m_renderShader->Bind();
        m_renderShader->SetMat4("u_ProjectionView", projectionView);
        m_renderShader->SetVec3("u_CameraRight", cameraRight);
        m_renderShader->SetVec3("u_CameraUp", cameraUp);

        // Bind texture
        if (m_texture) {
            m_texture->Bind(0);
            m_renderShader->SetInt("u_Texture", 0);
            m_renderShader->SetBool("u_HasTexture", true);
        } else {
            m_renderShader->SetBool("u_HasTexture", false);
        }

        // Bind particle SSBO for vertex shader access
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_particleSSBO);

        // Draw quads (4 vertices per particle)
        glBindVertexArray(m_vao);
        glDrawArrays(GL_QUADS, 0, static_cast<GLsizei>(m_activeCount * 4));
        glBindVertexArray(0);

        // Restore state
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    /**
     * @brief Emit particles (CPU-side, uploads to GPU)
     */
    void Emit(const glm::vec3& position, int count) {
        if (!m_initialized) {
            return;
        }

        const size_t availableSlots = m_maxParticles - m_activeCount;
        const size_t actualCount = std::min(static_cast<size_t>(count), availableSlots);

        if (actualCount == 0) {
            return;
        }

        // Generate particles on CPU and upload
        std::vector<GPUParticle> newParticles(actualCount);
        for (size_t i = 0; i < actualCount; ++i) {
            GPUParticle& p = newParticles[i];
            p.positionLife = glm::vec4(position, 0.0f);
            p.velocityMaxLife = glm::vec4(
                Random::Range(m_config.velocityMin.x, m_config.velocityMax.x),
                Random::Range(m_config.velocityMin.y, m_config.velocityMax.y),
                Random::Range(m_config.velocityMin.z, m_config.velocityMax.z),
                Random::Range(m_config.lifetimeMin, m_config.lifetimeMax)
            );
            p.color = m_config.startColor;
            const float startSize = Random::Range(m_config.startSizeMin, m_config.startSizeMax);
            const float endSize = Random::Range(m_config.endSizeMin, m_config.endSizeMax);
            p.sizeRotation = glm::vec4(
                startSize,
                startSize,
                endSize,
                glm::radians(Random::Range(m_config.rotationMin, m_config.rotationMax))
            );
            p.rotationSpeed = glm::radians(Random::Range(m_config.rotationSpeedMin, m_config.rotationSpeedMax));
        }

        // Upload to SSBO
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        static_cast<GLintptr>(m_activeCount * sizeof(GPUParticle)),
                        static_cast<GLsizeiptr>(actualCount * sizeof(GPUParticle)),
                        newParticles.data());

        m_activeCount += actualCount;

        // Update counter
        uint32_t count32 = static_cast<uint32_t>(m_activeCount);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_counterBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &count32);
    }

    void SetConfig(const EmitterConfig& config) { m_config = config; }
    EmitterConfig& GetConfig() { return m_config; }

    void SetTexture(std::shared_ptr<Texture> texture) { m_texture = std::move(texture); }
    void SetBlendAdditive(bool additive) { m_additiveBlend = additive; }

    size_t GetActiveParticleCount() const { return m_activeCount; }
    size_t GetMaxParticles() const { return m_maxParticles; }

private:
    // GPU particle structure (matches shader layout)
    struct GPUParticle {
        glm::vec4 positionLife;    // xyz = position, w = lifetime
        glm::vec4 velocityMaxLife; // xyz = velocity, w = maxLifetime
        glm::vec4 color;
        glm::vec4 sizeRotation;    // x = size, y = startSize, z = endSize, w = rotation
        float rotationSpeed;
        float _pad0;
        float _pad1;
        float _pad2;
    };
    static_assert(sizeof(GPUParticle) == 80, "GPUParticle must be 80 bytes for GPU alignment");

    std::unique_ptr<Shader> m_computeShader;
    std::unique_ptr<Shader> m_renderShader;
    std::shared_ptr<Texture> m_texture;

    uint32_t m_particleSSBO = 0;
    uint32_t m_counterBuffer = 0;
    uint32_t m_vao = 0;

    EmitterConfig m_config;
    size_t m_maxParticles = 0;
    size_t m_activeCount = 0;
    bool m_additiveBlend = false;
    bool m_initialized = false;
};

} // namespace Nova
