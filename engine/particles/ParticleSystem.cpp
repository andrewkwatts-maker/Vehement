#include "particles/ParticleSystem.hpp"
#include "graphics/Shader.hpp"
#include "graphics/Texture.hpp"
#include "math/Random.hpp"

#include <glad/gl.h>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <spdlog/spdlog.h>

namespace Nova {

// ============================================================================
// Shader Sources
// ============================================================================

static const char* const PARTICLE_VERTEX_SHADER = R"(
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in float a_Size;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in float a_Rotation;

uniform mat4 u_ProjectionView;
uniform vec3 u_CameraRight;
uniform vec3 u_CameraUp;

out vec4 v_Color;
out vec2 v_TexCoord;

void main() {
    v_Color = a_Color;

    // Billboard quad vertices (instanced via gl_VertexID)
    const vec2 quadVertices[4] = vec2[](
        vec2(-0.5, -0.5),
        vec2( 0.5, -0.5),
        vec2( 0.5,  0.5),
        vec2(-0.5,  0.5)
    );

    int vertexID = gl_VertexID % 4;
    vec2 vertex = quadVertices[vertexID];

    // Apply rotation
    float s = sin(a_Rotation);
    float c = cos(a_Rotation);
    vec2 rotated = vec2(
        vertex.x * c - vertex.y * s,
        vertex.x * s + vertex.y * c
    );

    v_TexCoord = vertex + 0.5;

    // Billboard in world space
    vec3 worldPos = a_Position +
        u_CameraRight * rotated.x * a_Size +
        u_CameraUp * rotated.y * a_Size;

    gl_Position = u_ProjectionView * vec4(worldPos, 1.0);
}
)";

static const char* const PARTICLE_FRAGMENT_SHADER = R"(
#version 460 core

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform bool u_HasTexture;

out vec4 FragColor;

void main() {
    vec4 texColor = u_HasTexture ? texture(u_Texture, v_TexCoord) : vec4(1.0);
    FragColor = v_Color * texColor;

    // Discard nearly transparent fragments for performance
    if (FragColor.a < 0.01) {
        discard;
    }
}
)";

// ============================================================================
// Construction / Destruction
// ============================================================================

ParticleSystem::ParticleSystem() = default;

ParticleSystem::~ParticleSystem() {
    Shutdown();
}

ParticleSystem::ParticleSystem(ParticleSystem&& other) noexcept
    : m_particles(std::move(other.m_particles))
    , m_activeCount(other.m_activeCount)
    , m_maxParticles(other.m_maxParticles)
    , m_vertexBuffer(std::move(other.m_vertexBuffer))
    , m_sortIndices(std::move(other.m_sortIndices))
    , m_sortDistances(std::move(other.m_sortDistances))
    , m_config(other.m_config)
    , m_emitterPosition(other.m_emitterPosition)
    , m_emitAccumulator(other.m_emitAccumulator)
    , m_emitting(other.m_emitting)
    , m_additiveBlend(other.m_additiveBlend)
    , m_texture(std::move(other.m_texture))
    , m_shader(std::move(other.m_shader))
    , m_vao(other.m_vao)
    , m_vbo(other.m_vbo)
    , m_initialized(other.m_initialized)
{
    // Reset other's GPU handles so they don't get deleted
    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_initialized = false;
    other.m_activeCount = 0;
}

ParticleSystem& ParticleSystem::operator=(ParticleSystem&& other) noexcept {
    if (this != &other) {
        Shutdown();

        m_particles = std::move(other.m_particles);
        m_activeCount = other.m_activeCount;
        m_maxParticles = other.m_maxParticles;
        m_vertexBuffer = std::move(other.m_vertexBuffer);
        m_sortIndices = std::move(other.m_sortIndices);
        m_sortDistances = std::move(other.m_sortDistances);
        m_config = other.m_config;
        m_emitterPosition = other.m_emitterPosition;
        m_emitAccumulator = other.m_emitAccumulator;
        m_emitting = other.m_emitting;
        m_additiveBlend = other.m_additiveBlend;
        m_texture = std::move(other.m_texture);
        m_shader = std::move(other.m_shader);
        m_vao = other.m_vao;
        m_vbo = other.m_vbo;
        m_initialized = other.m_initialized;

        other.m_vao = 0;
        other.m_vbo = 0;
        other.m_initialized = false;
        other.m_activeCount = 0;
    }
    return *this;
}

// ============================================================================
// Initialization
// ============================================================================

bool ParticleSystem::Initialize(size_t maxParticles) {
    if (m_initialized) {
        spdlog::warn("ParticleSystem already initialized");
        return true;
    }

    m_maxParticles = maxParticles;

    // Allocate SoA particle data
    m_particles.Resize(maxParticles);

    // Pre-allocate vertex buffer
    m_vertexBuffer.resize(maxParticles);

    // Allocate sort buffers
    m_sortIndices.resize(maxParticles);
    m_sortDistances.resize(maxParticles);

    // Initialize sort indices
    std::iota(m_sortIndices.begin(), m_sortIndices.end(), 0u);

    // Create shader
    m_shader = std::make_unique<Shader>();
    if (!m_shader->LoadFromSource(PARTICLE_VERTEX_SHADER, PARTICLE_FRAGMENT_SHADER)) {
        spdlog::error("Failed to create particle shader");
        return false;
    }

    // Create GPU buffers
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Allocate buffer with orphaning hint for streaming updates
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(maxParticles * sizeof(ParticleVertex)),
                 nullptr,
                 GL_STREAM_DRAW);

    // Set up vertex attributes matching ParticleVertex layout
    constexpr GLsizei stride = sizeof(ParticleVertex);

    // Position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(ParticleVertex, position)));

    // Size (float)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(ParticleVertex, size)));

    // Color (vec4)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(ParticleVertex, color)));

    // Rotation (float)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(ParticleVertex, rotation)));

    glBindVertexArray(0);

    m_initialized = true;
    spdlog::debug("ParticleSystem initialized with {} max particles", maxParticles);

    return true;
}

void ParticleSystem::Shutdown() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }

    m_shader.reset();
    m_texture.reset();
    m_particles.Clear();
    m_vertexBuffer.clear();
    m_sortIndices.clear();
    m_sortDistances.clear();
    m_activeCount = 0;
    m_initialized = false;
}

// ============================================================================
// Update
// ============================================================================

void ParticleSystem::Update(float deltaTime) {
    if (m_activeCount == 0 && !m_emitting) {
        return;
    }

    // Update physics and visuals (SIMD-friendly loops)
    UpdatePhysics(deltaTime);
    UpdateVisuals(deltaTime);

    // Remove dead particles (compacts the arrays)
    RemoveDeadParticles();

    // Emit new particles based on emission rate
    if (m_emitting) {
        m_emitAccumulator += m_config.emissionRate * deltaTime;
        const int particlesToEmit = static_cast<int>(m_emitAccumulator);
        if (particlesToEmit > 0) {
            Emit(m_emitterPosition, particlesToEmit);
            m_emitAccumulator -= static_cast<float>(particlesToEmit);
        }
    }
}

void ParticleSystem::UpdatePhysics(float deltaTime) {
    // Cache frequently used values
    const float gravityX = m_config.gravity.x * deltaTime;
    const float gravityY = m_config.gravity.y * deltaTime;
    const float gravityZ = m_config.gravity.z * deltaTime;
    const float dragFactor = 1.0f - m_config.drag * deltaTime;

    // Get raw pointers for SIMD-friendly access
    float* __restrict velX = m_particles.velocityX.data();
    float* __restrict velY = m_particles.velocityY.data();
    float* __restrict velZ = m_particles.velocityZ.data();
    float* __restrict posX = m_particles.positionX.data();
    float* __restrict posY = m_particles.positionY.data();
    float* __restrict posZ = m_particles.positionZ.data();
    float* __restrict rot = m_particles.rotation.data();
    const float* __restrict rotSpeed = m_particles.rotationSpeed.data();
    float* __restrict life = m_particles.lifetime.data();

    // Update loop - structured for auto-vectorization
    const size_t count = m_activeCount;
    for (size_t i = 0; i < count; ++i) {
        // Update lifetime
        life[i] += deltaTime;

        // Apply gravity
        velX[i] += gravityX;
        velY[i] += gravityY;
        velZ[i] += gravityZ;

        // Apply drag
        velX[i] *= dragFactor;
        velY[i] *= dragFactor;
        velZ[i] *= dragFactor;

        // Update position
        posX[i] += velX[i] * deltaTime;
        posY[i] += velY[i] * deltaTime;
        posZ[i] += velZ[i] * deltaTime;

        // Update rotation
        rot[i] += rotSpeed[i] * deltaTime;
    }
}

void ParticleSystem::UpdateVisuals(float deltaTime) {
    // Cache color values
    const float startR = m_config.startColor.r;
    const float startG = m_config.startColor.g;
    const float startB = m_config.startColor.b;
    const float startA = m_config.startColor.a;
    const float endR = m_config.endColor.r;
    const float endG = m_config.endColor.g;
    const float endB = m_config.endColor.b;
    const float endA = m_config.endColor.a;

    // Get raw pointers
    const float* __restrict life = m_particles.lifetime.data();
    const float* __restrict maxLife = m_particles.maxLifetime.data();
    const float* __restrict startSz = m_particles.startSize.data();
    const float* __restrict endSz = m_particles.endSize.data();
    float* __restrict sz = m_particles.size.data();
    float* __restrict colR = m_particles.colorR.data();
    float* __restrict colG = m_particles.colorG.data();
    float* __restrict colB = m_particles.colorB.data();
    float* __restrict colA = m_particles.colorA.data();

    // Update visual properties based on lifetime ratio
    const size_t count = m_activeCount;
    for (size_t i = 0; i < count; ++i) {
        const float t = life[i] / maxLife[i];

        // Interpolate color
        colR[i] = startR + (endR - startR) * t;
        colG[i] = startG + (endG - startG) * t;
        colB[i] = startB + (endB - startB) * t;
        colA[i] = startA + (endA - startA) * t;

        // Interpolate size
        sz[i] = startSz[i] + (endSz[i] - startSz[i]) * t;
    }
}

void ParticleSystem::RemoveDeadParticles() {
    const float* life = m_particles.lifetime.data();
    const float* maxLife = m_particles.maxLifetime.data();

    // Compact array by swapping dead particles with last alive
    size_t i = 0;
    while (i < m_activeCount) {
        if (life[i] >= maxLife[i]) {
            // Particle is dead - swap with last active
            --m_activeCount;
            if (i < m_activeCount) {
                m_particles.Swap(i, m_activeCount);
            }
            // Don't increment i - check the swapped particle
        } else {
            ++i;
        }
    }
}

// ============================================================================
// Sorting
// ============================================================================

void ParticleSystem::SortByDepth(const glm::vec3& cameraPosition) {
    if (m_activeCount <= 1) {
        return;
    }

    // Calculate squared distances (avoid sqrt for performance)
    const float camX = cameraPosition.x;
    const float camY = cameraPosition.y;
    const float camZ = cameraPosition.z;
    const float* __restrict posX = m_particles.positionX.data();
    const float* __restrict posY = m_particles.positionY.data();
    const float* __restrict posZ = m_particles.positionZ.data();

    for (size_t i = 0; i < m_activeCount; ++i) {
        const float dx = posX[i] - camX;
        const float dy = posY[i] - camY;
        const float dz = posZ[i] - camZ;
        m_sortDistances[i] = dx * dx + dy * dy + dz * dz;
        m_sortIndices[i] = static_cast<uint32_t>(i);
    }

    // Sort indices by distance (back to front for proper blending)
    std::sort(m_sortIndices.begin(), m_sortIndices.begin() + m_activeCount,
        [this](uint32_t a, uint32_t b) {
            return m_sortDistances[a] > m_sortDistances[b];
        });
}

// ============================================================================
// GPU Upload
// ============================================================================

void ParticleSystem::UploadToGPU() {
    if (m_activeCount == 0) {
        return;
    }

    // Build vertex buffer using sort order
    const float* __restrict posX = m_particles.positionX.data();
    const float* __restrict posY = m_particles.positionY.data();
    const float* __restrict posZ = m_particles.positionZ.data();
    const float* __restrict colR = m_particles.colorR.data();
    const float* __restrict colG = m_particles.colorG.data();
    const float* __restrict colB = m_particles.colorB.data();
    const float* __restrict colA = m_particles.colorA.data();
    const float* __restrict sz = m_particles.size.data();
    const float* __restrict rot = m_particles.rotation.data();

    for (size_t i = 0; i < m_activeCount; ++i) {
        const uint32_t idx = m_sortIndices[i];
        ParticleVertex& v = m_vertexBuffer[i];
        v.position = glm::vec3(posX[idx], posY[idx], posZ[idx]);
        v.color = glm::vec4(colR[idx], colG[idx], colB[idx], colA[idx]);
        v.size = sz[idx];
        v.rotation = rot[idx];
    }

    // Upload to GPU using buffer orphaning for performance
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_maxParticles * sizeof(ParticleVertex)),
                 nullptr,
                 GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(m_activeCount * sizeof(ParticleVertex)),
                    m_vertexBuffer.data());
}

// ============================================================================
// Rendering
// ============================================================================

void ParticleSystem::Render(const glm::mat4& projectionView,
                            const glm::vec3& cameraRight,
                            const glm::vec3& cameraUp) {
    if (m_activeCount == 0 || !m_initialized) {
        return;
    }

    // Set blend mode
    glEnable(GL_BLEND);
    if (m_additiveBlend) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    } else {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // Disable depth writing for proper transparency
    glDepthMask(GL_FALSE);

    // Bind shader and set uniforms
    m_shader->Bind();
    m_shader->SetMat4("u_ProjectionView", projectionView);
    m_shader->SetVec3("u_CameraRight", cameraRight);
    m_shader->SetVec3("u_CameraUp", cameraUp);

    // Bind texture if available
    if (m_texture) {
        m_texture->Bind(0);
        m_shader->SetInt("u_Texture", 0);
        m_shader->SetBool("u_HasTexture", true);
    } else {
        m_shader->SetBool("u_HasTexture", false);
    }

    // Draw particles
    glBindVertexArray(m_vao);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_activeCount));
    glBindVertexArray(0);

    // Restore state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void ParticleSystem::UpdateAndRender(float deltaTime,
                                     const glm::mat4& cameraTransform,
                                     const glm::mat4& projectionView) {
    // Update particles
    Update(deltaTime);

    if (m_activeCount == 0) {
        return;
    }

    // Extract camera vectors
    const glm::vec3 cameraPos = glm::vec3(cameraTransform[3]);
    const glm::mat4 invPV = glm::inverse(projectionView);
    const glm::vec3 cameraRight = glm::normalize(glm::vec3(invPV[0]));
    const glm::vec3 cameraUp = glm::normalize(glm::vec3(invPV[1]));

    // Sort for transparency
    SortByDepth(cameraPos);

    // Upload and render
    UploadToGPU();
    Render(projectionView, cameraRight, cameraUp);
}

// ============================================================================
// Emission
// ============================================================================

void ParticleSystem::Emit(const glm::vec3& position, int count) {
    const int availableSlots = static_cast<int>(m_maxParticles - m_activeCount);
    const int actualCount = std::min(count, availableSlots);

    for (int i = 0; i < actualCount; ++i) {
        EmitSingleParticle(position);
    }
}

void ParticleSystem::EmitBurst(const glm::vec3& position, int count) {
    Emit(position, count);
}

void ParticleSystem::EmitSingleParticle(const glm::vec3& position) {
    if (m_activeCount >= m_maxParticles) {
        return;
    }

    const size_t idx = m_activeCount++;

    // Position
    m_particles.positionX[idx] = position.x;
    m_particles.positionY[idx] = position.y;
    m_particles.positionZ[idx] = position.z;

    // Velocity (randomized within config range)
    m_particles.velocityX[idx] = Random::Range(m_config.velocityMin.x, m_config.velocityMax.x);
    m_particles.velocityY[idx] = Random::Range(m_config.velocityMin.y, m_config.velocityMax.y);
    m_particles.velocityZ[idx] = Random::Range(m_config.velocityMin.z, m_config.velocityMax.z);

    // Lifetime
    m_particles.lifetime[idx] = 0.0f;
    m_particles.maxLifetime[idx] = Random::Range(m_config.lifetimeMin, m_config.lifetimeMax);

    // Size (store start and end for interpolation)
    const float startSize = Random::Range(m_config.startSizeMin, m_config.startSizeMax);
    const float endSize = Random::Range(m_config.endSizeMin, m_config.endSizeMax);
    m_particles.size[idx] = startSize;
    m_particles.startSize[idx] = startSize;
    m_particles.endSize[idx] = endSize;

    // Color (initial)
    m_particles.colorR[idx] = m_config.startColor.r;
    m_particles.colorG[idx] = m_config.startColor.g;
    m_particles.colorB[idx] = m_config.startColor.b;
    m_particles.colorA[idx] = m_config.startColor.a;

    // Rotation
    m_particles.rotation[idx] = glm::radians(Random::Range(m_config.rotationMin, m_config.rotationMax));
    m_particles.rotationSpeed[idx] = glm::radians(Random::Range(m_config.rotationSpeedMin, m_config.rotationSpeedMax));

    // Update sort index
    m_sortIndices[idx] = static_cast<uint32_t>(idx);
}

void ParticleSystem::Clear() {
    m_activeCount = 0;
    m_emitAccumulator = 0.0f;
}

} // namespace Nova
