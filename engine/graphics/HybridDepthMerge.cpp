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

    // Create temporary depth buffer with GL_DEPTH_COMPONENT32F format
    m_tempDepth = std::make_shared<Texture>();
    if (!m_tempDepth->Create(width, height, TextureFormat::Depth, nullptr)) {
        spdlog::error("Failed to create temporary depth texture");
        return false;
    }

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

    // Resize temporary depth buffer - delete and recreate with new dimensions
    if (m_tempDepth) {
        m_tempDepth->Cleanup();
        if (!m_tempDepth->Create(width, height, TextureFormat::Depth, nullptr)) {
            spdlog::error("Failed to recreate temporary depth texture on resize");
        }
    }
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

    // Create inline depth copy shader - simple pass-through that optionally uses min
    const std::string depthCopySource = R"(
#version 450 core
layout(local_size_x = 8, local_size_y = 8) in;

layout(r32f, binding = 0) uniform readonly image2D u_source;
layout(r32f, binding = 1) uniform image2D u_dest;

uniform ivec2 u_resolution;
uniform int u_useMin;

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (pixel.x >= u_resolution.x || pixel.y >= u_resolution.y) return;

    float srcDepth = imageLoad(u_source, pixel).r;
    if (u_useMin == 1) {
        float dstDepth = imageLoad(u_dest, pixel).r;
        imageStore(u_dest, pixel, vec4(min(srcDepth, dstDepth)));
    } else {
        imageStore(u_dest, pixel, vec4(srcDepth));
    }
}
)";
    m_depthCopyShader = std::make_shared<Shader>();
    if (!m_depthCopyShader->LoadComputeShader(depthCopySource)) {
        spdlog::warn("Failed to compile depth copy shader");
    }

    // Create inline depth clear shader
    const std::string depthClearSource = R"(
#version 450 core
layout(local_size_x = 8, local_size_y = 8) in;

layout(r32f, binding = 0) uniform writeonly image2D u_depth;

uniform ivec2 u_resolution;
uniform float u_clearValue;

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (pixel.x >= u_resolution.x || pixel.y >= u_resolution.y) return;

    imageStore(u_depth, pixel, vec4(u_clearValue));
}
)";
    m_depthClearShader = std::make_shared<Shader>();
    if (!m_depthClearShader->LoadComputeShader(depthClearSource)) {
        spdlog::warn("Failed to compile depth clear shader");
    }

    // Create inline depth init shader for raymarch initialization
    const std::string depthInitSource = R"(
#version 450 core
layout(local_size_x = 8, local_size_y = 8) in;

layout(r32f, binding = 0) uniform writeonly image2D u_depth;

uniform ivec2 u_resolution;
uniform float u_farPlane;

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (pixel.x >= u_resolution.x || pixel.y >= u_resolution.y) return;

    // Initialize depth to far plane for raymarch
    imageStore(u_depth, pixel, vec4(u_farPlane));
}
)";
    m_depthInitShader = std::make_shared<Shader>();
    if (!m_depthInitShader->LoadComputeShader(depthInitSource)) {
        spdlog::warn("Failed to compile depth init shader");
    }

    return true;
}

bool HybridDepthMerge::CreateBuffers() {
    // No additional buffers needed - we use image textures directly
    return true;
}

} // namespace Nova
