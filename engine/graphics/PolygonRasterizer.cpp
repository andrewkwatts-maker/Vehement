#include "PolygonRasterizer.hpp"
#include "scene/Camera.hpp"
#include "scene/Scene.hpp"
#include "graphics/Shader.hpp"
#include "graphics/Texture.hpp"
#include "graphics/Mesh.hpp"
#include "graphics/Material.hpp"
#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace Nova {

PolygonRasterizer::PolygonRasterizer() = default;

PolygonRasterizer::~PolygonRasterizer() {
    Shutdown();
}

bool PolygonRasterizer::Initialize(int width, int height) {
    if (m_initialized) {
        spdlog::warn("PolygonRasterizer already initialized");
        return true;
    }

    spdlog::info("Initializing Polygon Rasterizer ({}x{})", width, height);

    // Set default quality settings
    m_settings.renderWidth = width;
    m_settings.renderHeight = height;
    m_settings.shadowMapSize = 2048;
    m_settings.cascadeCount = 4;
    m_settings.enableMSAA = false;
    m_settings.msaaSamples = 4;

    // Create framebuffer and textures
    m_framebuffer = std::make_unique<Framebuffer>();
    if (!m_framebuffer->Create(width, height, 1, true)) {
        spdlog::error("Failed to create polygon framebuffer");
        return false;
    }

    m_colorTexture = m_framebuffer->GetColorAttachment(0);
    m_depthTexture = m_framebuffer->GetDepthAttachment();

    // Create instance buffers
    if (!CreateInstanceBuffers()) {
        spdlog::error("Failed to create instance buffers");
        return false;
    }

    // Create shaders
    if (!CreateShaders()) {
        spdlog::error("Failed to create shaders");
        return false;
    }

    // Create shadow maps
    if (!CreateShadowMaps()) {
        spdlog::error("Failed to create shadow maps");
        return false;
    }

    // Create GPU queries for timing
    glGenQueries(1, &m_gpuQueryStart);
    glGenQueries(1, &m_gpuQueryEnd);

    // Setup cascade splits (logarithmic distribution)
    m_cascadeSplits.resize(m_settings.cascadeCount);
    for (int i = 0; i < m_settings.cascadeCount; ++i) {
        float t = static_cast<float>(i + 1) / m_settings.cascadeCount;
        m_cascadeSplits[i] = 0.1f + (1000.0f - 0.1f) * (t * t); // Quadratic distribution
    }

    m_initialized = true;
    spdlog::info("Polygon Rasterizer initialized successfully");
    return true;
}

void PolygonRasterizer::Shutdown() {
    if (!m_initialized) return;

    spdlog::info("Shutting down Polygon Rasterizer");

    // Delete instance buffers
    if (m_instanceBuffer) glDeleteBuffers(1, &m_instanceBuffer);
    if (m_instanceVAO) glDeleteVertexArrays(1, &m_instanceVAO);

    // Delete queries
    if (m_gpuQueryStart) glDeleteQueries(1, &m_gpuQueryStart);
    if (m_gpuQueryEnd) glDeleteQueries(1, &m_gpuQueryEnd);

    // Clear data
    m_opaqueBatches.clear();
    m_transparentBatches.clear();

    m_framebuffer.reset();
    m_colorTexture.reset();
    m_depthTexture.reset();
    m_shadowMapFramebuffers.clear();
    m_pbrShader.reset();
    m_shadowShader.reset();
    m_instancedShader.reset();

    m_initialized = false;
}

void PolygonRasterizer::Resize(int width, int height) {
    if (!m_initialized) return;

    spdlog::info("Resizing Polygon Rasterizer to {}x{}", width, height);

    m_settings.renderWidth = width;
    m_settings.renderHeight = height;

    // Resize framebuffer
    m_framebuffer->Resize(width, height);
}

void PolygonRasterizer::BeginFrame(const Camera& camera) {
    m_frameStartTime = std::chrono::high_resolution_clock::now();
    m_stats.Reset();

    // Update camera matrices
    m_viewMatrix = camera.GetViewMatrix();
    m_projMatrix = camera.GetProjectionMatrix();
    m_viewProjMatrix = m_projMatrix * m_viewMatrix;
    m_cameraPosition = camera.GetPosition();

    // Setup shadow cascades
    SetupShadowCascades(camera);

    // Start GPU timing
    glQueryCounter(m_gpuQueryStart, GL_TIMESTAMP);

    // Clear submissions from previous frame
    ClearSubmissions();
}

void PolygonRasterizer::EndFrame() {
    // End GPU timing
    glQueryCounter(m_gpuQueryEnd, GL_TIMESTAMP);

    // Update statistics
    UpdateStats();

    m_frameCount++;
}

void PolygonRasterizer::Render(const Scene& scene, const Camera& camera) {
    if (!m_initialized) return;

    ScopedTimer timer(m_stats.frameTimeMs);

    // Build render batches
    BuildBatches();

    // Render shadow maps
    if (m_settings.shadowMapSize > 0) {
        RenderShadows();
    }

    // Bind main framebuffer
    m_framebuffer->Bind();
    glViewport(0, 0, m_settings.renderWidth, m_settings.renderHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Render opaque geometry
    {
        ScopedTimer polygonTimer(m_stats.polygonPassMs);
        RenderOpaque();
    }

    // Render transparent geometry (back-to-front sorted)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        RenderTransparent();

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    Framebuffer::Unbind();
}

void PolygonRasterizer::SetQualitySettings(const QualitySettings& settings) {
    bool shadowMapChanged = (settings.shadowMapSize != m_settings.shadowMapSize ||
                            settings.cascadeCount != m_settings.cascadeCount);

    m_settings = settings;

    // Recreate shadow maps if size changed
    if (shadowMapChanged && m_initialized) {
        CreateShadowMaps();
    }
}

bool PolygonRasterizer::SupportsFeature(RenderFeature feature) const {
    switch (feature) {
        case RenderFeature::PolygonRendering:
        case RenderFeature::PBRShading:
        case RenderFeature::ShadowMapping:
        case RenderFeature::ClusteredLighting:
        case RenderFeature::DepthInterleaving:
            return true;
        case RenderFeature::SDFRendering:
        case RenderFeature::HybridRendering:
        case RenderFeature::RTXRaytracing:
        case RenderFeature::ComputeShaders:
        case RenderFeature::TileBasedCulling:
            return false;
        default:
            return false;
    }
}

std::shared_ptr<Texture> PolygonRasterizer::GetOutputColor() const {
    return m_colorTexture;
}

std::shared_ptr<Texture> PolygonRasterizer::GetOutputDepth() const {
    return m_depthTexture;
}

void PolygonRasterizer::SubmitMesh(const std::shared_ptr<Mesh>& mesh,
                                   const std::shared_ptr<Material>& material,
                                   const glm::mat4& transform) {
    PolygonBatch batch;
    batch.mesh = mesh;
    batch.material = material;
    batch.transforms.push_back(transform);
    batch.instanceCount = 1;
    batch.isInstanced = false;

    // Sort into opaque or transparent
    if (material && material->GetAlpha() < 1.0f) {
        m_transparentBatches.push_back(batch);
    } else {
        m_opaqueBatches.push_back(batch);
    }
}

void PolygonRasterizer::SubmitInstanced(const std::shared_ptr<Mesh>& mesh,
                                        const std::shared_ptr<Material>& material,
                                        const std::vector<glm::mat4>& transforms) {
    if (transforms.empty()) return;

    PolygonBatch batch;
    batch.mesh = mesh;
    batch.material = material;
    batch.transforms = transforms;
    batch.instanceCount = static_cast<uint32_t>(transforms.size());
    batch.isInstanced = true;

    // Instanced meshes are always opaque for simplicity
    m_opaqueBatches.push_back(batch);
}

void PolygonRasterizer::ClearSubmissions() {
    m_opaqueBatches.clear();
    m_transparentBatches.clear();
}

void PolygonRasterizer::BuildBatches() {
    // Batches are already built during submission
    // In a more sophisticated system, we would merge batches with same mesh/material here
    m_stats.drawCalls = static_cast<uint32_t>(m_opaqueBatches.size() + m_transparentBatches.size());
}

void PolygonRasterizer::RenderOpaque() {
    for (const auto& batch : m_opaqueBatches) {
        RenderBatch(batch);
    }

    m_stats.polygonObjectsRendered += static_cast<uint32_t>(m_opaqueBatches.size());
}

void PolygonRasterizer::RenderTransparent() {
    // Sort transparent batches back-to-front by distance to camera
    // This is essential for correct alpha blending (painter's algorithm)
    std::sort(m_transparentBatches.begin(), m_transparentBatches.end(),
        [this](const PolygonBatch& a, const PolygonBatch& b) {
            // Calculate distance from camera to each batch's center
            // For instanced batches, use the first transform as representative
            glm::vec3 posA = glm::vec3(a.transforms[0][3]); // Extract position from transform
            glm::vec3 posB = glm::vec3(b.transforms[0][3]);

            // Calculate squared distance to camera (avoid sqrt for performance)
            float distA = glm::dot(posA - m_cameraPosition, posA - m_cameraPosition);
            float distB = glm::dot(posB - m_cameraPosition, posB - m_cameraPosition);

            // Sort back-to-front (farther objects first)
            return distA > distB;
        });

    for (const auto& batch : m_transparentBatches) {
        RenderBatch(batch);
    }

    m_stats.polygonObjectsRendered += static_cast<uint32_t>(m_transparentBatches.size());
}

void PolygonRasterizer::RenderShadows() {
    ScopedTimer timer(m_stats.cpuTimeMs);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glCullFace(GL_FRONT); // Reduce peter-panning

    for (int cascade = 0; cascade < m_settings.cascadeCount; ++cascade) {
        // Bind shadow map framebuffer
        m_shadowMapFramebuffers[cascade]->Bind();
        glViewport(0, 0, m_settings.shadowMapSize, m_settings.shadowMapSize);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Bind shadow shader
        m_shadowShader->Bind();
        m_shadowShader->SetMat4("u_lightViewProj", m_shadowViewProj[cascade]);

        // Render all opaque batches
        for (const auto& batch : m_opaqueBatches) {
            if (!batch.mesh) continue;

            // Set model matrix
            if (batch.isInstanced) {
                // For shadow pass, we skip true GPU instancing for simplicity
                // This ensures shadow maps work correctly without needing a separate instanced shadow shader
                // Performance note: For large instance counts, consider implementing instanced shadow rendering
                // by uploading transforms to a UBO/SSBO and using gl_InstanceID in the shadow vertex shader
                for (const auto& transform : batch.transforms) {
                    m_shadowShader->SetMat4("u_model", transform);
                    batch.mesh->Draw();
                    m_stats.drawCalls++;
                }
            } else {
                m_shadowShader->SetMat4("u_model", batch.transforms[0]);
                batch.mesh->Draw();
                m_stats.drawCalls++;
            }
        }
    }

    glCullFace(GL_BACK);
    Framebuffer::Unbind();
}

void PolygonRasterizer::SetupShadowCascades(const Camera& camera) {
    // Calculate shadow view-projection matrices for each cascade
    m_shadowViewProj.resize(m_settings.cascadeCount);

    // Directional light direction (sun)
    glm::vec3 lightDir = glm::normalize(glm::vec3(-0.3f, -1.0f, -0.2f));

    // Get camera properties for frustum calculation
    const glm::mat4& viewMatrix = camera.GetView();
    const glm::mat4& projMatrix = camera.GetProjection();
    glm::mat4 invViewProj = glm::inverse(projMatrix * viewMatrix);

    for (int i = 0; i < m_settings.cascadeCount; ++i) {
        float nearPlane = (i == 0) ? camera.GetNearPlane() : m_cascadeSplits[i - 1];
        float farPlane = m_cascadeSplits[i];

        // Calculate cascade frustum split in normalized depth (0-1)
        float nearSplit = (nearPlane - camera.GetNearPlane()) /
                         (camera.GetFarPlane() - camera.GetNearPlane());
        float farSplit = (farPlane - camera.GetNearPlane()) /
                        (camera.GetFarPlane() - camera.GetNearPlane());

        // Calculate 8 frustum corners in world space for this cascade
        // NDC corners: x,y in [-1,1], z from nearSplit to farSplit
        glm::vec3 frustumCorners[8] = {
            // Near plane corners
            glm::vec3(-1.0f, -1.0f, nearSplit * 2.0f - 1.0f),
            glm::vec3( 1.0f, -1.0f, nearSplit * 2.0f - 1.0f),
            glm::vec3( 1.0f,  1.0f, nearSplit * 2.0f - 1.0f),
            glm::vec3(-1.0f,  1.0f, nearSplit * 2.0f - 1.0f),
            // Far plane corners
            glm::vec3(-1.0f, -1.0f, farSplit * 2.0f - 1.0f),
            glm::vec3( 1.0f, -1.0f, farSplit * 2.0f - 1.0f),
            glm::vec3( 1.0f,  1.0f, farSplit * 2.0f - 1.0f),
            glm::vec3(-1.0f,  1.0f, farSplit * 2.0f - 1.0f),
        };

        // Transform frustum corners to world space
        glm::vec3 frustumCenter(0.0f);
        for (int j = 0; j < 8; ++j) {
            glm::vec4 worldCorner = invViewProj * glm::vec4(frustumCorners[j], 1.0f);
            frustumCorners[j] = glm::vec3(worldCorner) / worldCorner.w;
            frustumCenter += frustumCorners[j];
        }
        frustumCenter /= 8.0f;

        // Calculate the radius of the bounding sphere for this cascade
        float radius = 0.0f;
        for (int j = 0; j < 8; ++j) {
            float distance = glm::length(frustumCorners[j] - frustumCenter);
            radius = glm::max(radius, distance);
        }

        // Round up to reduce shadow swimming when camera moves
        radius = std::ceil(radius * 16.0f) / 16.0f;

        // Create light view matrix looking at frustum center
        glm::vec3 lightPos = frustumCenter - lightDir * radius;
        glm::mat4 lightView = glm::lookAt(lightPos, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));

        // Create orthographic projection that encompasses the cascade frustum
        glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius,
                                        0.1f, radius * 2.0f + 10.0f);

        // Stabilize shadow map to reduce shimmer when camera rotates
        // by snapping to texel boundaries
        glm::mat4 shadowMatrix = lightProj * lightView;
        glm::vec4 shadowOrigin = shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        shadowOrigin *= static_cast<float>(m_settings.shadowMapSize) / 2.0f;

        glm::vec4 roundedOrigin = glm::round(shadowOrigin);
        glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
        roundOffset *= 2.0f / static_cast<float>(m_settings.shadowMapSize);
        roundOffset.z = 0.0f;
        roundOffset.w = 0.0f;

        lightProj[3] += roundOffset;

        m_shadowViewProj[i] = lightProj * lightView;
    }
}

void PolygonRasterizer::RenderBatch(const PolygonBatch& batch) {
    if (!batch.mesh) return;

    // Select shader
    std::shared_ptr<Shader> shader = batch.isInstanced ? m_instancedShader : m_pbrShader;
    shader->Bind();

    // Set camera uniforms
    shader->SetMat4("u_view", m_viewMatrix);
    shader->SetMat4("u_projection", m_projMatrix);
    shader->SetMat4("u_viewProjection", m_viewProjMatrix);
    shader->SetVec3("u_cameraPos", m_cameraPosition);

    // Set shadow uniforms
    for (int i = 0; i < m_settings.cascadeCount && i < static_cast<int>(m_shadowViewProj.size()); ++i) {
        std::string uniformName = "u_shadowViewProj[" + std::to_string(i) + "]";
        shader->SetMat4(uniformName.c_str(), m_shadowViewProj[i]);
    }

    // Setup material
    if (batch.material) {
        SetupMaterial(*batch.material);
    }

    // Render instances
    if (batch.isInstanced && batch.instanceCount > 1) {
        // Upload instance transforms
        glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                       batch.instanceCount * sizeof(glm::mat4),
                       batch.transforms.data());

        // Draw instanced
        batch.mesh->DrawInstanced(batch.instanceCount);
        m_stats.drawCalls++;
        m_stats.trianglesRendered += batch.mesh->GetTriangleCount() * batch.instanceCount;
    } else {
        // Single instance
        shader->SetMat4("u_model", batch.transforms[0]);
        batch.mesh->Draw();
        m_stats.drawCalls++;
        m_stats.trianglesRendered += batch.mesh->GetTriangleCount();
    }
}

void PolygonRasterizer::SetupMaterial(const Material& material) {
    // Bind the material which sets up its shader, textures, and uniforms
    material.Bind();

    // Get the currently bound shader to set additional PBR uniforms
    // The Material::Bind() already activates the shader, so we can set uniforms directly
    auto shader = material.GetShaderPtr();
    if (!shader || !shader->IsValid()) {
        return;
    }

    // Bind shadow maps to texture units 8-11 (reserving 0-7 for material textures)
    // This allows the fragment shader to sample shadows from all cascades
    for (int i = 0; i < m_settings.cascadeCount && i < static_cast<int>(m_shadowMapFramebuffers.size()); ++i) {
        auto shadowDepth = m_shadowMapFramebuffers[i]->GetDepthAttachment();
        if (shadowDepth) {
            glActiveTexture(GL_TEXTURE8 + i);
            glBindTexture(GL_TEXTURE_2D, shadowDepth->GetID());

            // Set sampler uniform for this cascade
            std::string samplerName = "u_shadowMap[" + std::to_string(i) + "]";
            shader->SetInt(samplerName, 8 + i);
        }
    }

    // Set cascade split distances for shader to select correct cascade
    shader->SetFloatArray("u_cascadeSplits", m_cascadeSplits.data(),
                         static_cast<int>(m_cascadeSplits.size()));
    shader->SetInt("u_cascadeCount", m_settings.cascadeCount);

    // Reset active texture to unit 0 for subsequent material texture bindings
    glActiveTexture(GL_TEXTURE0);
}

bool PolygonRasterizer::CreateInstanceBuffers() {
    // Create instance buffer for transform matrices
    glGenBuffers(1, &m_instanceBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 m_maxInstances * sizeof(glm::mat4),
                 nullptr,
                 GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        spdlog::error("OpenGL error creating instance buffers: {}", error);
        return false;
    }

    return true;
}

bool PolygonRasterizer::CreateShaders() {
    // Load PBR shader
    m_pbrShader = std::make_shared<Shader>();
    if (!m_pbrShader->Load("assets/shaders/vertex/pbr.vert",
                          "assets/shaders/fragment/pbr.frag")) {
        spdlog::warn("Failed to load PBR shader, using fallback");
        // In production, we'd create a minimal fallback shader here
    }

    // Load shadow shader
    m_shadowShader = std::make_shared<Shader>();
    if (!m_shadowShader->Load("assets/shaders/vertex/shadow.vert",
                             "assets/shaders/fragment/shadow.frag")) {
        spdlog::warn("Failed to load shadow shader");
    }

    // Load instanced shader
    m_instancedShader = std::make_shared<Shader>();
    if (!m_instancedShader->Load("assets/shaders/vertex/instanced.vert",
                                "assets/shaders/fragment/pbr.frag")) {
        spdlog::warn("Failed to load instanced shader");
    }

    return true;
}

bool PolygonRasterizer::CreateShadowMaps() {
    m_shadowMapFramebuffers.clear();
    m_shadowMapFramebuffers.resize(m_settings.cascadeCount);

    for (int i = 0; i < m_settings.cascadeCount; ++i) {
        m_shadowMapFramebuffers[i] = std::make_unique<Framebuffer>();
        if (!m_shadowMapFramebuffers[i]->Create(m_settings.shadowMapSize,
                                               m_settings.shadowMapSize,
                                               0, // No color attachments
                                               true)) {
            spdlog::error("Failed to create shadow map framebuffer {}", i);
            return false;
        }
    }

    return true;
}

void PolygonRasterizer::UpdateStats() {
    // Calculate frame time
    auto frameEndTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> frameDuration = frameEndTime - m_frameStartTime;
    m_stats.frameTimeMs = frameDuration.count();

    // Get GPU time
    GLuint64 startTime, endTime;
    glGetQueryObjectui64v(m_gpuQueryStart, GL_QUERY_RESULT, &startTime);
    glGetQueryObjectui64v(m_gpuQueryEnd, GL_QUERY_RESULT, &endTime);
    m_stats.gpuTimeMs = (endTime - startTime) / 1000000.0f; // Convert ns to ms

    // Calculate FPS
    m_accumulatedTime += m_stats.frameTimeMs;
    if (m_accumulatedTime >= 1000.0f) {
        m_stats.fps = static_cast<int>(m_frameCount * 1000.0f / m_accumulatedTime);
        m_frameCount = 0;
        m_accumulatedTime = 0.0f;
    }
}

} // namespace Nova
