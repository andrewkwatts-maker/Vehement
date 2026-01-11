#include "SDFDeferredLighting.hpp"
#include <glad/gl.h>
#include <cstring>
#include <chrono>

namespace Engine {
namespace Graphics {

// ============================================================================
// SDFDeferredLighting Implementation
// ============================================================================

SDFDeferredLighting::SDFDeferredLighting(uint32_t width, uint32_t height)
    : m_width(width)
    , m_height(height)
    , m_outputFBO(0)
    , m_outputTexture(0)
    , m_gbufferShader(0)
    , m_lightingShader(0)
    , m_blendShader(0)
    , m_quadVAO(0)
    , m_quadVBO(0) {

    m_queryObjects[0] = 0;
    m_queryObjects[1] = 0;
}

SDFDeferredLighting::~SDFDeferredLighting() {
    // Cleanup G-buffer
    if (m_gbuffer.fbo) glDeleteFramebuffers(1, &m_gbuffer.fbo);
    if (m_gbuffer.albedoTexture) glDeleteTextures(1, &m_gbuffer.albedoTexture);
    if (m_gbuffer.normalTexture) glDeleteTextures(1, &m_gbuffer.normalTexture);
    if (m_gbuffer.materialTexture) glDeleteTextures(1, &m_gbuffer.materialTexture);
    if (m_gbuffer.depthTexture) glDeleteTextures(1, &m_gbuffer.depthTexture);

    // Cleanup output
    if (m_outputFBO) glDeleteFramebuffers(1, &m_outputFBO);
    if (m_outputTexture) glDeleteTextures(1, &m_outputTexture);

    // Cleanup shaders
    if (m_gbufferShader) glDeleteProgram(m_gbufferShader);
    if (m_lightingShader) glDeleteProgram(m_lightingShader);
    if (m_blendShader) glDeleteProgram(m_blendShader);

    // Cleanup quad
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);

    // Cleanup queries
    if (m_queryObjects[0]) glDeleteQueries(2, m_queryObjects);
}

bool SDFDeferredLighting::Initialize() {
    if (!CreateGBuffer()) {
        return false;
    }

    if (!CreateOutputBuffer()) {
        return false;
    }

    if (!LoadShaders()) {
        return false;
    }

    // Create fullscreen quad
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    // Create query objects
    glGenQueries(2, m_queryObjects);

    return true;
}

bool SDFDeferredLighting::CreateGBuffer() {
    // Create framebuffer
    glGenFramebuffers(1, &m_gbuffer.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_gbuffer.fbo);

    // Albedo + metallic texture
    glGenTextures(1, &m_gbuffer.albedoTexture);
    glBindTexture(GL_TEXTURE_2D, m_gbuffer.albedoTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_gbuffer.albedoTexture, 0);

    // Normal + roughness texture
    glGenTextures(1, &m_gbuffer.normalTexture);
    glBindTexture(GL_TEXTURE_2D, m_gbuffer.normalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_gbuffer.normalTexture, 0);

    // Material properties texture
    glGenTextures(1, &m_gbuffer.materialTexture);
    glBindTexture(GL_TEXTURE_2D, m_gbuffer.materialTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_gbuffer.materialTexture, 0);

    // Depth texture
    glGenTextures(1, &m_gbuffer.depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_gbuffer.depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_gbuffer.depthTexture, 0);

    // Specify draw buffers
    GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, drawBuffers);

    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_gbuffer.width = m_width;
    m_gbuffer.height = m_height;

    return true;
}

bool SDFDeferredLighting::CreateOutputBuffer() {
    // Create output framebuffer for lighting result
    glGenFramebuffers(1, &m_outputFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);

    // Output color texture
    glGenTextures(1, &m_outputTexture);
    glBindTexture(GL_TEXTURE_2D, m_outputTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_outputTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

bool SDFDeferredLighting::LoadShaders() {
    // G-buffer shader would be loaded from file
    // For now, create placeholder programs
    m_gbufferShader = glCreateProgram();
    m_lightingShader = glCreateProgram();
    m_blendShader = glCreateProgram();

    return true;
}

void SDFDeferredLighting::Resize(uint32_t width, uint32_t height) {
    if (m_width == width && m_height == height) {
        return;
    }

    m_width = width;
    m_height = height;

    // Recreate buffers
    if (m_gbuffer.albedoTexture) glDeleteTextures(1, &m_gbuffer.albedoTexture);
    if (m_gbuffer.normalTexture) glDeleteTextures(1, &m_gbuffer.normalTexture);
    if (m_gbuffer.materialTexture) glDeleteTextures(1, &m_gbuffer.materialTexture);
    if (m_gbuffer.depthTexture) glDeleteTextures(1, &m_gbuffer.depthTexture);
    if (m_outputTexture) glDeleteTextures(1, &m_outputTexture);

    CreateGBuffer();
    CreateOutputBuffer();
}

void SDFDeferredLighting::BeginGBufferPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_gbuffer.fbo);
    glViewport(0, 0, m_width, m_height);

    // Clear G-buffer
    ClearGBuffer();

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    // Start timing
    glBeginQuery(GL_TIME_ELAPSED, m_queryObjects[0]);
}

void SDFDeferredLighting::EndGBufferPass() {
    glEndQuery(GL_TIME_ELAPSED);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Read query result
    GLint available = 0;
    glGetQueryObjectiv(m_queryObjects[0], GL_QUERY_RESULT_AVAILABLE, &available);
    if (available) {
        GLuint64 timeElapsed = 0;
        glGetQueryObjectui64v(m_queryObjects[0], GL_QUERY_RESULT, &timeElapsed);
        m_stats.gbufferPassTimeMs = timeElapsed / 1000000.0f;
    }
}

void SDFDeferredLighting::RenderSDFsToGBuffer(
    uint32_t sdfShader,
    const Matrix4& viewMatrix,
    const Matrix4& projMatrix) {

    if (!sdfShader) return;

    glUseProgram(sdfShader);

    // Set uniforms
    GLint viewLoc = glGetUniformLocation(sdfShader, "u_view");
    GLint projLoc = glGetUniformLocation(sdfShader, "u_proj");
    GLint maxStepsLoc = glGetUniformLocation(sdfShader, "u_maxSteps");
    GLint maxDistLoc = glGetUniformLocation(sdfShader, "u_maxDistance");
    GLint thresholdLoc = glGetUniformLocation(sdfShader, "u_hitThreshold");

    if (viewLoc >= 0) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, viewMatrix.m);
    if (projLoc >= 0) glUniformMatrix4fv(projLoc, 1, GL_FALSE, projMatrix.m);
    if (maxStepsLoc >= 0) glUniform1i(maxStepsLoc, m_raymarchSettings.maxSteps);
    if (maxDistLoc >= 0) glUniform1f(maxDistLoc, m_raymarchSettings.maxDistance);
    if (thresholdLoc >= 0) glUniform1f(thresholdLoc, m_raymarchSettings.hitThreshold);

    // Render fullscreen quad
    RenderFullscreenQuad();
}

void SDFDeferredLighting::ExecuteLightingPass(
    ClusteredLightingExpanded* lighting,
    const Matrix4& viewMatrix,
    const Matrix4& projMatrix,
    const Vector3& cameraPos) {

    glBeginQuery(GL_TIME_ELAPSED, m_queryObjects[1]);

    // Bind output framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_outputFBO);
    glViewport(0, 0, m_width, m_height);

    glDisable(GL_DEPTH_TEST);

    // Use lighting shader
    glUseProgram(m_lightingShader);

    // Bind G-buffer textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_gbuffer.albedoTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_gbuffer.normalTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_gbuffer.materialTexture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_gbuffer.depthTexture);

    // Set samplers
    glUniform1i(glGetUniformLocation(m_lightingShader, "g_albedo"), 0);
    glUniform1i(glGetUniformLocation(m_lightingShader, "g_normal"), 1);
    glUniform1i(glGetUniformLocation(m_lightingShader, "g_material"), 2);
    glUniform1i(glGetUniformLocation(m_lightingShader, "g_depth"), 3);

    // Set camera uniforms
    glUniform3f(glGetUniformLocation(m_lightingShader, "u_cameraPos"),
               cameraPos.x, cameraPos.y, cameraPos.z);
    glUniformMatrix4fv(glGetUniformLocation(m_lightingShader, "u_invView"), 1, GL_FALSE, viewMatrix.m);
    glUniformMatrix4fv(glGetUniformLocation(m_lightingShader, "u_invProj"), 1, GL_FALSE, projMatrix.m);

    // Bind lighting buffers
    if (lighting) {
        lighting->BindLightingBuffers();
    }

    // Render fullscreen quad
    RenderFullscreenQuad();

    glEndQuery(GL_TIME_ELAPSED);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Read query result
    GLint available = 0;
    glGetQueryObjectiv(m_queryObjects[1], GL_QUERY_RESULT_AVAILABLE, &available);
    if (available) {
        GLuint64 timeElapsed = 0;
        glGetQueryObjectui64v(m_queryObjects[1], GL_QUERY_RESULT, &timeElapsed);
        m_stats.lightingPassTimeMs = timeElapsed / 1000000.0f;
    }
}

void SDFDeferredLighting::ClearGBuffer() {
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    glClearBufferfv(GL_COLOR, 0, clearColor);
    glClearBufferfv(GL_COLOR, 1, clearColor);
    glClearBufferfv(GL_COLOR, 2, clearColor);

    const float clearDepth = 1.0f;
    glClearBufferfv(GL_DEPTH, 0, &clearDepth);
}

void SDFDeferredLighting::BlendWithRasterized(uint32_t rasterDepthTexture) {
    if (!m_blendShader || !rasterDepthTexture) return;

    glUseProgram(m_blendShader);

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_gbuffer.depthTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, rasterDepthTexture);

    // Perform depth-based blending
    // Implementation would composite based on depth values
}

void SDFDeferredLighting::RenderFullscreenQuad() {
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void SDFDeferredLighting::ResetStats() {
    m_stats = Stats();
}

// ============================================================================
// HybridSDFRenderer Implementation
// ============================================================================

HybridSDFRenderer::HybridSDFRenderer(uint32_t width, uint32_t height)
    : m_width(width)
    , m_height(height)
    , m_rasterFBO(0)
    , m_rasterColorTexture(0)
    , m_rasterDepthTexture(0)
    , m_compositeFBO(0)
    , m_compositeTexture(0)
    , m_compositeShader(0) {

    m_sdfDeferred = std::make_unique<SDFDeferredLighting>(width, height);
}

HybridSDFRenderer::~HybridSDFRenderer() {
    if (m_rasterFBO) glDeleteFramebuffers(1, &m_rasterFBO);
    if (m_rasterColorTexture) glDeleteTextures(1, &m_rasterColorTexture);
    if (m_rasterDepthTexture) glDeleteTextures(1, &m_rasterDepthTexture);
    if (m_compositeFBO) glDeleteFramebuffers(1, &m_compositeFBO);
    if (m_compositeTexture) glDeleteTextures(1, &m_compositeTexture);
    if (m_compositeShader) glDeleteProgram(m_compositeShader);
}

bool HybridSDFRenderer::Initialize() {
    if (!m_sdfDeferred->Initialize()) {
        return false;
    }

    // Create rasterized geometry buffers
    glGenFramebuffers(1, &m_rasterFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_rasterFBO);

    glGenTextures(1, &m_rasterColorTexture);
    glBindTexture(GL_TEXTURE_2D, m_rasterColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_rasterColorTexture, 0);

    glGenTextures(1, &m_rasterDepthTexture);
    glBindTexture(GL_TEXTURE_2D, m_rasterDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_rasterDepthTexture, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Create composite buffer
    glGenFramebuffers(1, &m_compositeFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_compositeFBO);

    glGenTextures(1, &m_compositeTexture);
    glBindTexture(GL_TEXTURE_2D, m_compositeTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_compositeTexture, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

void HybridSDFRenderer::BeginFrame() {
    // Clear rasterized buffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_rasterFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void HybridSDFRenderer::RenderRasterizedGeometry(const Matrix4& viewMatrix, const Matrix4& projMatrix) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_rasterFBO);
    glEnable(GL_DEPTH_TEST);

    // User would render their rasterized geometry here

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void HybridSDFRenderer::RenderSDFs(uint32_t sdfShader, const Matrix4& viewMatrix, const Matrix4& projMatrix) {
    m_sdfDeferred->BeginGBufferPass();
    m_sdfDeferred->RenderSDFsToGBuffer(sdfShader, viewMatrix, projMatrix);
    m_sdfDeferred->EndGBufferPass();
}

void HybridSDFRenderer::ApplyLighting(
    ClusteredLightingExpanded* lighting,
    const Matrix4& viewMatrix,
    const Matrix4& projMatrix,
    const Vector3& cameraPos) {

    // Apply lighting to SDFs
    m_sdfDeferred->ExecuteLightingPass(lighting, viewMatrix, projMatrix, cameraPos);

    // Composite rasterized and SDF results
    glBindFramebuffer(GL_FRAMEBUFFER, m_compositeFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    // Blend based on depth
    m_sdfDeferred->BlendWithRasterized(m_rasterDepthTexture);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void HybridSDFRenderer::EndFrame() {
    // Final composite is ready
}

uint32_t HybridSDFRenderer::GetOutputTexture() const {
    return m_compositeTexture;
}

} // namespace Graphics
} // namespace Engine
