#include "HybridDepthMerge.hpp"
#include "graphics/Shader.hpp"
#include "graphics/Texture.hpp"
#include <glad/gl.h>
#include <spdlog/spdlog.h>

namespace Nova {

HybridDepthMerge::HybridDepthMerge() = default;

HybridDepthMerge::~HybridDepthMerge() {
    Shutdown();
}

bool HybridDepthMerge::Initialize(int width, int height) {
    if (m_initialized) {
        spdlog::warn("HybridDepthMerge already initialized");
        return true;
    }

    spdlog::info("Initializing HybridDepthMerge ({}x{})", width, height);

    m_width = width;
    m_height = height;

    // Create compute shaders
    if (!CreateShaders()) {
        spdlog::error("Failed to create depth merge shaders");
        return false;
    }

    // Create temporary depth buffer
    m_tempDepth = std::make_shared<Texture>();
    // TODO: Create depth texture with appropriate format

    m_initialized = true;
    spdlog::info("HybridDepthMerge initialized successfully");
    return true;
}

void HybridDepthMerge::Shutdown() {
    if (!m_initialized) return;

    spdlog::info("Shutting down HybridDepthMerge");

    m_depthMergeShader.reset();
    m_depthCopyShader.reset();
    m_depthClearShader.reset();
    m_depthInitShader.reset();
    m_tempDepth.reset();

    m_initialized = false;
}

void HybridDepthMerge::Resize(int width, int height) {
    if (!m_initialized) return;

    spdlog::info("Resizing HybridDepthMerge to {}x{}", width, height);

    m_width = width;
    m_height = height;

    // Resize temporary depth buffer
    // TODO: Recreate m_tempDepth with new dimensions
}

void HybridDepthMerge::PrepareSDFPass(DepthMergeMode mode) {
    switch (mode) {
        case DepthMergeMode::SDFFirst:
            // Clear depth to far plane - SDF will write first
            // Polygon pass will use this depth for early-Z
            break;

        case DepthMergeMode::PolygonFirst:
            // Depth already contains polygon depths
            // SDF raymarch will read this for early termination
            break;

        case DepthMergeMode::Interleaved:
            // Initialize depth for atomic min operations
            break;
    }
}

void HybridDepthMerge::PreparePolygonPass(DepthMergeMode mode) {
    switch (mode) {
        case DepthMergeMode::SDFFirst:
            // Depth contains SDF depths
            // Enable depth testing to use SDF depth for early-Z
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_TRUE);
            break;

        case DepthMergeMode::PolygonFirst:
            // Clear depth - polygon will write first
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);
            break;

        case DepthMergeMode::Interleaved:
            // Both passes write atomically
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_TRUE);
            break;
    }
}

void HybridDepthMerge::MergeDepthBuffers(const std::shared_ptr<Texture>& sdfDepth,
                                        const std::shared_ptr<Texture>& polygonDepth,
                                        const std::shared_ptr<Texture>& output) {
    if (!m_initialized || !m_depthMergeShader) return;
    if (!sdfDepth || !polygonDepth || !output) return;

    // Bind compute shader
    m_depthMergeShader->Bind();

    // Bind input textures
    glBindImageTexture(0, sdfDepth->GetID(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
    glBindImageTexture(1, polygonDepth->GetID(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
    glBindImageTexture(2, output->GetID(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    // Set uniforms
    m_depthMergeShader->SetIVec2("u_resolution", glm::ivec2(m_width, m_height));
    m_depthMergeShader->SetFloat("u_depthBias", m_depthBias);
    m_depthMergeShader->SetInt("u_mode", static_cast<int>(m_mode));

    // Dispatch compute shader (8x8 work groups)
    int numGroupsX = (m_width + 7) / 8;
    int numGroupsY = (m_height + 7) / 8;
    glDispatchCompute(numGroupsX, numGroupsY, 1);

    // Memory barrier
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void HybridDepthMerge::CopyDepth(const std::shared_ptr<Texture>& source,
                                const std::shared_ptr<Texture>& dest,
                                bool useMin) {
    if (!m_initialized || !m_depthCopyShader) return;
    if (!source || !dest) return;

    // Bind compute shader
    m_depthCopyShader->Bind();

    // Bind textures
    glBindImageTexture(0, source->GetID(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
    glBindImageTexture(1, dest->GetID(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);

    // Set uniforms
    m_depthCopyShader->SetIVec2("u_resolution", glm::ivec2(m_width, m_height));
    m_depthCopyShader->SetInt("u_useMin", useMin ? 1 : 0);

    // Dispatch
    int numGroupsX = (m_width + 7) / 8;
    int numGroupsY = (m_height + 7) / 8;
    glDispatchCompute(numGroupsX, numGroupsY, 1);

    // Memory barrier
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void HybridDepthMerge::ClearDepth(const std::shared_ptr<Texture>& depth) {
    if (!m_initialized || !m_depthClearShader) return;
    if (!depth) return;

    // Bind compute shader
    m_depthClearShader->Bind();

    // Bind texture
    glBindImageTexture(0, depth->GetID(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    // Set uniforms
    m_depthClearShader->SetIVec2("u_resolution", glm::ivec2(m_width, m_height));
    m_depthClearShader->SetFloat("u_clearValue", 1.0f); // Far plane

    // Dispatch
    int numGroupsX = (m_width + 7) / 8;
    int numGroupsY = (m_height + 7) / 8;
    glDispatchCompute(numGroupsX, numGroupsY, 1);

    // Memory barrier
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void HybridDepthMerge::InitializeDepthForRaymarch(const std::shared_ptr<Texture>& depth, float farPlane) {
    if (!m_initialized || !m_depthInitShader) return;
    if (!depth) return;

    // Bind compute shader
    m_depthInitShader->Bind();

    // Bind texture
    glBindImageTexture(0, depth->GetID(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    // Set uniforms
    m_depthInitShader->SetIVec2("u_resolution", glm::ivec2(m_width, m_height));
    m_depthInitShader->SetFloat("u_farPlane", farPlane);

    // Dispatch
    int numGroupsX = (m_width + 7) / 8;
    int numGroupsY = (m_height + 7) / 8;
    glDispatchCompute(numGroupsX, numGroupsY, 1);

    // Memory barrier
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

bool HybridDepthMerge::CreateShaders() {
    // Load depth merge shader
    m_depthMergeShader = std::make_shared<Shader>();
    if (!m_depthMergeShader->LoadCompute("assets/shaders/depth_merge.comp")) {
        spdlog::warn("Failed to load depth merge compute shader");
        // Continue - we can still function with limited capability
    }

    // Load depth copy shader (inline simple shader)
    m_depthCopyShader = std::make_shared<Shader>();
    // TODO: Load or create simple depth copy shader

    // Load depth clear shader
    m_depthClearShader = std::make_shared<Shader>();
    // TODO: Load or create depth clear shader

    // Load depth init shader
    m_depthInitShader = std::make_shared<Shader>();
    // TODO: Load or create depth init shader

    return true;
}

bool HybridDepthMerge::CreateBuffers() {
    // No additional buffers needed - we use image textures directly
    return true;
}

} // namespace Nova
