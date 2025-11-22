#include "particles/ParticleSystem.hpp"
#include "graphics/Shader.hpp"
#include "graphics/Texture.hpp"
#include "math/Random.hpp"

#include <glad/gl.h>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace Nova {

static const char* PARTICLE_VERTEX_SHADER = R"(
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in float a_Size;
layout(location = 3) in float a_Rotation;

uniform mat4 u_ProjectionView;
uniform vec3 u_CameraRight;
uniform vec3 u_CameraUp;

out vec4 v_Color;
out vec2 v_TexCoord;

void main() {
    v_Color = a_Color;

    // Billboard quad vertices
    float s = sin(a_Rotation);
    float c = cos(a_Rotation);

    vec2 quadVertices[4] = vec2[](
        vec2(-0.5, -0.5),
        vec2( 0.5, -0.5),
        vec2( 0.5,  0.5),
        vec2(-0.5,  0.5)
    );

    int vertexID = gl_VertexID % 4;
    vec2 vertex = quadVertices[vertexID];

    // Rotate
    vec2 rotated = vec2(
        vertex.x * c - vertex.y * s,
        vertex.x * s + vertex.y * c
    );

    v_TexCoord = vertex + 0.5;

    vec3 worldPos = a_Position +
        u_CameraRight * rotated.x * a_Size +
        u_CameraUp * rotated.y * a_Size;

    gl_Position = u_ProjectionView * vec4(worldPos, 1.0);
}
)";

static const char* PARTICLE_FRAGMENT_SHADER = R"(
#version 460 core

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform bool u_HasTexture;

out vec4 FragColor;

void main() {
    vec4 texColor = u_HasTexture ? texture(u_Texture, v_TexCoord) : vec4(1.0);
    FragColor = v_Color * texColor;
}
)";

ParticleSystem::ParticleSystem() = default;

ParticleSystem::~ParticleSystem() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
}

bool ParticleSystem::Initialize(size_t maxParticles) {
    m_particles.resize(maxParticles);

    // Create shader
    m_shader = std::make_unique<Shader>();
    if (!m_shader->LoadFromSource(PARTICLE_VERTEX_SHADER, PARTICLE_FRAGMENT_SHADER)) {
        spdlog::error("Failed to create particle shader");
        return false;
    }

    // Create buffers
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Allocate buffer (position + color + size + rotation per particle)
    size_t vertexSize = sizeof(glm::vec3) + sizeof(glm::vec4) + sizeof(float) * 2;
    glBufferData(GL_ARRAY_BUFFER, maxParticles * vertexSize, nullptr, GL_DYNAMIC_DRAW);

    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertexSize, (void*)0);

    // Color
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, vertexSize, (void*)sizeof(glm::vec3));

    // Size
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, vertexSize, (void*)(sizeof(glm::vec3) + sizeof(glm::vec4)));

    // Rotation
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, vertexSize, (void*)(sizeof(glm::vec3) + sizeof(glm::vec4) + sizeof(float)));

    glBindVertexArray(0);

    m_initialized = true;
    return true;
}

void ParticleSystem::Update(float deltaTime, const glm::mat4& cameraTransform) {
    // Update existing particles
    for (size_t i = 0; i < m_activeCount; ) {
        Particle& p = m_particles[i];
        p.lifetime += deltaTime;

        if (!p.IsAlive()) {
            // Swap with last active particle
            std::swap(m_particles[i], m_particles[m_activeCount - 1]);
            m_activeCount--;
            continue;
        }

        // Physics
        p.velocity += m_config.gravity * deltaTime;
        p.velocity *= (1.0f - m_config.drag * deltaTime);
        p.position += p.velocity * deltaTime;
        p.rotation += p.rotationSpeed * deltaTime;

        // Interpolate color and size
        float t = p.GetLifeRatio();
        p.color = glm::mix(m_config.startColor, m_config.endColor, t);

        float startSize = glm::mix(m_config.startSizeMin, m_config.startSizeMax, 0.5f);
        float endSize = glm::mix(m_config.endSizeMin, m_config.endSizeMax, 0.5f);
        p.size = glm::mix(startSize, endSize, t);

        i++;
    }

    // Emit new particles
    if (m_emitting) {
        m_emitAccumulator += m_config.emissionRate * deltaTime;
        while (m_emitAccumulator >= 1.0f && m_activeCount < m_particles.size()) {
            EmitParticle(glm::vec3(0));  // Override position in actual use
            m_emitAccumulator -= 1.0f;
        }
    }

    // Sort for transparency (back to front)
    glm::vec3 cameraPos = glm::vec3(cameraTransform[3]);
    SortParticles(cameraPos);
}

void ParticleSystem::Render(const glm::mat4& projectionView) {
    if (m_activeCount == 0 || !m_initialized) return;

    UpdateBuffers();

    // Set blend mode
    glEnable(GL_BLEND);
    if (m_additiveBlend) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    } else {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glDepthMask(GL_FALSE);

    m_shader->Bind();
    m_shader->SetMat4("u_ProjectionView", projectionView);

    // Extract camera vectors from projection-view matrix
    glm::mat4 invPV = glm::inverse(projectionView);
    glm::vec3 right = glm::normalize(glm::vec3(invPV[0]));
    glm::vec3 up = glm::normalize(glm::vec3(invPV[1]));
    m_shader->SetVec3("u_CameraRight", right);
    m_shader->SetVec3("u_CameraUp", up);

    if (m_texture) {
        m_texture->Bind(0);
        m_shader->SetInt("u_Texture", 0);
        m_shader->SetBool("u_HasTexture", true);
    } else {
        m_shader->SetBool("u_HasTexture", false);
    }

    glBindVertexArray(m_vao);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_activeCount));
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void ParticleSystem::Emit(const glm::vec3& position, int count) {
    for (int i = 0; i < count && m_activeCount < m_particles.size(); ++i) {
        EmitParticle(position);
    }
}

void ParticleSystem::EmitBurst(const glm::vec3& position, int count) {
    Emit(position, count);
}

void ParticleSystem::Clear() {
    m_activeCount = 0;
}

void ParticleSystem::EmitParticle(const glm::vec3& position) {
    if (m_activeCount >= m_particles.size()) return;

    Particle& p = m_particles[m_activeCount++];

    p.position = position;
    p.lifetime = 0.0f;
    p.maxLifetime = Random::Range(m_config.lifetimeMin, m_config.lifetimeMax);

    p.velocity = glm::vec3(
        Random::Range(m_config.velocityMin.x, m_config.velocityMax.x),
        Random::Range(m_config.velocityMin.y, m_config.velocityMax.y),
        Random::Range(m_config.velocityMin.z, m_config.velocityMax.z)
    );

    p.size = Random::Range(m_config.startSizeMin, m_config.startSizeMax);
    p.color = m_config.startColor;
    p.rotation = glm::radians(Random::Range(m_config.rotationMin, m_config.rotationMax));
    p.rotationSpeed = glm::radians(Random::Range(m_config.rotationSpeedMin, m_config.rotationSpeedMax));
}

void ParticleSystem::SortParticles(const glm::vec3& cameraPosition) {
    std::sort(m_particles.begin(), m_particles.begin() + m_activeCount,
        [&cameraPosition](const Particle& a, const Particle& b) {
            float distA = glm::length2(a.position - cameraPosition);
            float distB = glm::length2(b.position - cameraPosition);
            return distA > distB;  // Back to front
        });
}

void ParticleSystem::UpdateBuffers() {
    struct ParticleVertex {
        glm::vec3 position;
        glm::vec4 color;
        float size;
        float rotation;
    };

    std::vector<ParticleVertex> vertices(m_activeCount);
    for (size_t i = 0; i < m_activeCount; ++i) {
        vertices[i].position = m_particles[i].position;
        vertices[i].color = m_particles[i].color;
        vertices[i].size = m_particles[i].size;
        vertices[i].rotation = m_particles[i].rotation;
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(ParticleVertex), vertices.data());
}

} // namespace Nova
