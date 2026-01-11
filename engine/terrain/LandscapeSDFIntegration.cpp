#include "LandscapeSDFIntegration.hpp"
#include <glad/gl.h>
#include <chrono>
#include <cmath>

namespace Engine {
namespace Terrain {

// ============================================================================
// HiZBuffer Implementation
// ============================================================================

HiZBuffer::HiZBuffer(uint32_t width, uint32_t height)
    : m_texture(0)
    , m_width(width)
    , m_height(height)
    , m_mipLevels(0) {

    // Calculate mip levels
    uint32_t size = std::max(width, height);
    while (size > 1) {
        size >>= 1;
        m_mipLevels++;
    }
    m_mipLevels++;  // Include base level

    // Create texture
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexStorage2D(GL_TEXTURE_2D, m_mipLevels, GL_R32F, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

HiZBuffer::~HiZBuffer() {
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
    }
}

void HiZBuffer::GenerateFromDepth(uint32_t depthTexture) {
    // Copy depth to mip 0
    // ... implementation details

    // Generate mipmap chain (max depth reduction)
    glGenerateMipmap(GL_TEXTURE_2D);
}

// ============================================================================
// LandscapeSDFIntegration Implementation
// ============================================================================

LandscapeSDFIntegration::LandscapeSDFIntegration(const Config& config)
    : m_config(config)
    , m_terrainVAO(0)
    , m_terrainVBO(0)
    , m_terrainIBO(0)
    , m_terrainShader(0)
    , m_heightmapTexture(0)
    , m_terrainFBO(0)
    , m_terrainDepthTexture(0)
    , m_terrainColorTexture(0) {
}

LandscapeSDFIntegration::~LandscapeSDFIntegration() {
    if (m_terrainVAO) glDeleteVertexArrays(1, &m_terrainVAO);
    if (m_terrainVBO) glDeleteBuffers(1, &m_terrainVBO);
    if (m_terrainIBO) glDeleteBuffers(1, &m_terrainIBO);
    if (m_terrainShader) glDeleteProgram(m_terrainShader);
    if (m_heightmapTexture) glDeleteTextures(1, &m_heightmapTexture);
    if (m_terrainFBO) glDeleteFramebuffers(1, &m_terrainFBO);
    if (m_terrainDepthTexture) glDeleteTextures(1, &m_terrainDepthTexture);
    if (m_terrainColorTexture) glDeleteTextures(1, &m_terrainColorTexture);
}

bool LandscapeSDFIntegration::Initialize() {
    // Create terrain mesh
    std::vector<float> vertices;
    std::vector<uint32_t> indices;

    uint32_t res = m_config.terrainResolution;
    float size = m_config.terrainSize;
    float step = size / res;

    // Generate grid vertices
    for (uint32_t y = 0; y <= res; y++) {
        for (uint32_t x = 0; x <= res; x++) {
            float fx = (x / static_cast<float>(res)) * size - size * 0.5f;
            float fz = (y / static_cast<float>(res)) * size - size * 0.5f;

            vertices.push_back(fx);      // x
            vertices.push_back(0.0f);    // y (height)
            vertices.push_back(fz);      // z
            vertices.push_back(x / static_cast<float>(res));  // u
            vertices.push_back(y / static_cast<float>(res));  // v
        }
    }

    // Generate indices
    for (uint32_t y = 0; y < res; y++) {
        for (uint32_t x = 0; x < res; x++) {
            uint32_t topLeft = y * (res + 1) + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (y + 1) * (res + 1) + x;
            uint32_t bottomRight = bottomLeft + 1;

            // Triangle 1
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            // Triangle 2
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    // Create VAO/VBO/IBO
    glGenVertexArrays(1, &m_terrainVAO);
    glGenBuffers(1, &m_terrainVBO);
    glGenBuffers(1, &m_terrainIBO);

    glBindVertexArray(m_terrainVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_terrainVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_terrainIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

    // TexCoord attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);

    // Create framebuffer for terrain rendering
    glGenFramebuffers(1, &m_terrainFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_terrainFBO);

    // Color texture
    glGenTextures(1, &m_terrainColorTexture);
    glBindTexture(GL_TEXTURE_2D, m_terrainColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_config.hiZResolution, m_config.hiZResolution, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_terrainColorTexture, 0);

    // Depth texture
    glGenTextures(1, &m_terrainDepthTexture);
    glBindTexture(GL_TEXTURE_2D, m_terrainDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, m_config.hiZResolution, m_config.hiZResolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_terrainDepthTexture, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Create Hi-Z buffer
    if (m_config.enableOcclusionCulling) {
        m_hiZ = std::make_unique<HiZBuffer>(m_config.hiZResolution, m_config.hiZResolution);
    }

    // Load occlusion culling shader
    if (m_config.enableOcclusionCulling) {
        m_occlusionCullShader = std::make_unique<Graphics::ComputeShader>();
    }

    return true;
}

void LandscapeSDFIntegration::RenderTerrain(
    const Matrix4& viewMatrix,
    const Matrix4& projMatrix) {

    auto startTime = std::chrono::high_resolution_clock::now();

    // Bind terrain framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_terrainFBO);
    glViewport(0, 0, m_config.hiZResolution, m_config.hiZResolution);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Use terrain shader
    if (m_terrainShader) {
        glUseProgram(m_terrainShader);

        GLint viewLoc = glGetUniformLocation(m_terrainShader, "u_view");
        GLint projLoc = glGetUniformLocation(m_terrainShader, "u_proj");

        if (viewLoc >= 0) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, viewMatrix.m);
        if (projLoc >= 0) glUniformMatrix4fv(projLoc, 1, GL_FALSE, projMatrix.m);

        // Bind heightmap
        if (m_heightmapTexture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_heightmapTexture);
            glUniform1i(glGetUniformLocation(m_terrainShader, "u_heightmap"), 0);
        }
    }

    // Draw terrain
    glBindVertexArray(m_terrainVAO);
    glDrawElements(GL_TRIANGLES, m_config.terrainResolution * m_config.terrainResolution * 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.terrainRenderTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

void LandscapeSDFIntegration::GenerateHiZ() {
    if (!m_config.enableOcclusionCulling || !m_hiZ) {
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    m_hiZ->GenerateFromDepth(m_terrainDepthTexture);

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.hiZGenerationTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

void LandscapeSDFIntegration::CullSDFsWithTerrain(
    const std::vector<Graphics::SDFInstance>& instances,
    std::vector<uint32_t>& visibleIndices) {

    if (!m_config.enableOcclusionCulling || !m_occlusionCullShader) {
        // No culling - all visible
        visibleIndices.resize(instances.size());
        for (size_t i = 0; i < instances.size(); i++) {
            visibleIndices[i] = static_cast<uint32_t>(i);
        }
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Use compute shader for occlusion culling
    // ... implementation details

    uint32_t culledCount = 0;
    visibleIndices.clear();

    for (size_t i = 0; i < instances.size(); i++) {
        // Simple placeholder - would use Hi-Z test
        visibleIndices.push_back(static_cast<uint32_t>(i));
    }

    m_stats.sdfsCulledByTerrain = static_cast<uint32_t>(instances.size()) - static_cast<uint32_t>(visibleIndices.size());

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.occlusionCullingTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

void LandscapeSDFIntegration::RenderSDFsWithDepthTest(
    uint32_t sdfShader,
    const Matrix4& viewMatrix,
    const Matrix4& projMatrix) {

    if (!sdfShader) return;

    // Bind terrain depth for depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);  // Allow SDFs at same depth as terrain

    // Render SDFs
    glUseProgram(sdfShader);

    // Set uniforms
    GLint viewLoc = glGetUniformLocation(sdfShader, "u_view");
    GLint projLoc = glGetUniformLocation(sdfShader, "u_proj");

    if (viewLoc >= 0) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, viewMatrix.m);
    if (projLoc >= 0) glUniformMatrix4fv(projLoc, 1, GL_FALSE, projMatrix.m);

    // Bind terrain depth as texture for manual depth testing in shader
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, m_terrainDepthTexture);
    glUniform1i(glGetUniformLocation(sdfShader, "u_terrainDepth"), 10);

    // SDFs would be rendered here
}

} // namespace Terrain
} // namespace Engine
