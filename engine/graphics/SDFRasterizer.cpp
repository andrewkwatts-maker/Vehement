#include "SDFRasterizer.hpp"
#include "core/Camera.hpp"
#include "scene/Scene.hpp"
#include "graphics/Shader.hpp"
#include "graphics/Texture.hpp"
#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace Nova {

SDFRasterizer::SDFRasterizer() = default;

SDFRasterizer::~SDFRasterizer() {
    Shutdown();
}

bool SDFRasterizer::Initialize(int width, int height) {
    if (m_initialized) {
        spdlog::warn("SDFRasterizer already initialized");
        return true;
    }

    spdlog::info("Initializing SDF Rasterizer ({}x{})", width, height);

    // Check compute shader support
    GLint maxComputeWorkGroupCount[3];
    GLint maxComputeWorkGroupSize[3];
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maxComputeWorkGroupCount[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &maxComputeWorkGroupCount[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxComputeWorkGroupSize[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxComputeWorkGroupSize[1]);

    spdlog::info("Compute shader support: Work group count: {}x{}, Work group size: {}x{}",
                 maxComputeWorkGroupCount[0], maxComputeWorkGroupCount[1],
                 maxComputeWorkGroupSize[0], maxComputeWorkGroupSize[1]);

    // Set default quality settings
    m_settings.renderWidth = width;
    m_settings.renderHeight = height;
    m_settings.sdfTileSize = 16;
    m_settings.maxRaymarchSteps = 128;
    m_settings.sdfRayEpsilon = 0.001f;

    // Calculate tile grid
    m_tileGridSize.x = (width + m_settings.sdfTileSize - 1) / m_settings.sdfTileSize;
    m_tileGridSize.y = (height + m_settings.sdfTileSize - 1) / m_settings.sdfTileSize;
    spdlog::info("Tile grid: {}x{} (tile size: {})", m_tileGridSize.x, m_tileGridSize.y, m_settings.sdfTileSize);

    // Create framebuffer and textures
    m_framebuffer = std::make_unique<Framebuffer>();
    if (!m_framebuffer->Create(width, height, 1, true)) {
        spdlog::error("Failed to create SDF framebuffer");
        return false;
    }

    m_colorTexture = m_framebuffer->GetColorAttachment(0);
    m_depthTexture = m_framebuffer->GetDepthAttachment();

    // Create GPU buffers
    if (!CreateBuffers()) {
        spdlog::error("Failed to create GPU buffers");
        return false;
    }

    // Create compute shaders
    if (!CreateShaders()) {
        spdlog::error("Failed to create compute shaders");
        return false;
    }

    // Create GPU queries for timing
    glGenQueries(1, &m_gpuQueryStart);
    glGenQueries(1, &m_gpuQueryEnd);

    // Pre-allocate tile array
    m_tileAABBs.resize(m_tileGridSize.x * m_tileGridSize.y);

    m_initialized = true;
    spdlog::info("SDF Rasterizer initialized successfully");
    return true;
}

void SDFRasterizer::Shutdown() {
    if (!m_initialized) return;

    spdlog::info("Shutting down SDF Rasterizer");

    // Delete GPU buffers
    if (m_sdfObjectBuffer) glDeleteBuffers(1, &m_sdfObjectBuffer);
    if (m_tileDataBuffer) glDeleteBuffers(1, &m_tileDataBuffer);
    if (m_tileObjectIndexBuffer) glDeleteBuffers(1, &m_tileObjectIndexBuffer);
    if (m_statsBuffer) glDeleteBuffers(1, &m_statsBuffer);

    // Delete queries
    if (m_gpuQueryStart) glDeleteQueries(1, &m_gpuQueryStart);
    if (m_gpuQueryEnd) glDeleteQueries(1, &m_gpuQueryEnd);

    // Clear data
    m_sdfObjects.clear();
    m_objectIdToIndex.clear();
    m_tileAABBs.clear();
    m_activeTiles.clear();
    m_tileObjectIndices.clear();

    m_framebuffer.reset();
    m_colorTexture.reset();
    m_depthTexture.reset();
    m_raymarchShader.reset();
    m_tileCullShader.reset();
    m_debugShader.reset();

    m_initialized = false;
}

void SDFRasterizer::Resize(int width, int height) {
    if (!m_initialized) return;

    spdlog::info("Resizing SDF Rasterizer to {}x{}", width, height);

    m_settings.renderWidth = width;
    m_settings.renderHeight = height;

    // Recalculate tile grid
    m_tileGridSize.x = (width + m_settings.sdfTileSize - 1) / m_settings.sdfTileSize;
    m_tileGridSize.y = (height + m_settings.sdfTileSize - 1) / m_settings.sdfTileSize;

    // Resize framebuffer
    m_framebuffer->Resize(width, height);

    // Reallocate tile array
    m_tileAABBs.clear();
    m_tileAABBs.resize(m_tileGridSize.x * m_tileGridSize.y);

    spdlog::info("New tile grid: {}x{}", m_tileGridSize.x, m_tileGridSize.y);
}

void SDFRasterizer::BeginFrame(const Camera& camera) {
    m_frameStartTime = std::chrono::high_resolution_clock::now();
    m_stats.Reset();

    // Update camera matrices
    m_viewMatrix = camera.GetViewMatrix();
    m_projMatrix = camera.GetProjectionMatrix();
    m_viewProjMatrix = m_projMatrix * m_viewMatrix;
    m_invViewProjMatrix = glm::inverse(m_viewProjMatrix);
    m_cameraPosition = camera.GetPosition();
    m_cameraForward = camera.GetForward();

    // Start GPU timing
    glQueryCounter(m_gpuQueryStart, GL_TIMESTAMP);
}

void SDFRasterizer::EndFrame() {
    // End GPU timing
    glQueryCounter(m_gpuQueryEnd, GL_TIMESTAMP);

    // Update statistics
    UpdateStats();

    m_frameCount++;
}

void SDFRasterizer::Render(const Scene& scene, const Camera& camera) {
    if (!m_initialized) return;

    ScopedTimer timer(m_stats.sdfPassMs);

    // Build tile bounds and cull objects
    BuildTileBounds(camera);

    // Upload data to GPU
    UploadTileData();

    // Dispatch raymarching compute shader
    DispatchRaymarch();

    // Debug visualization if enabled
    if (m_debugMode) {
        RenderDebugVisualization();
    }
}

void SDFRasterizer::SetQualitySettings(const QualitySettings& settings) {
    m_settings = settings;

    // Update tile size if changed
    if (settings.sdfTileSize != m_settings.sdfTileSize) {
        m_tileGridSize.x = (m_settings.renderWidth + settings.sdfTileSize - 1) / settings.sdfTileSize;
        m_tileGridSize.y = (m_settings.renderHeight + settings.sdfTileSize - 1) / settings.sdfTileSize;
        m_tileAABBs.clear();
        m_tileAABBs.resize(m_tileGridSize.x * m_tileGridSize.y);
    }
}

bool SDFRasterizer::SupportsFeature(RenderFeature feature) const {
    switch (feature) {
        case RenderFeature::SDFRendering:
        case RenderFeature::ComputeShaders:
        case RenderFeature::TileBasedCulling:
        case RenderFeature::PBRShading:
        case RenderFeature::DepthInterleaving:
            return true;
        case RenderFeature::PolygonRendering:
        case RenderFeature::HybridRendering:
        case RenderFeature::RTXRaytracing:
            return false;
        default:
            return false;
    }
}

std::shared_ptr<Texture> SDFRasterizer::GetOutputColor() const {
    return m_colorTexture;
}

std::shared_ptr<Texture> SDFRasterizer::GetOutputDepth() const {
    return m_depthTexture;
}

uint32_t SDFRasterizer::AddSDFObject(const SDFObjectGPU& object) {
    uint32_t id = m_nextObjectId++;
    m_objectIdToIndex[id] = m_sdfObjects.size();
    m_sdfObjects.push_back(object);
    return id;
}

void SDFRasterizer::RemoveSDFObject(uint32_t objectId) {
    auto it = m_objectIdToIndex.find(objectId);
    if (it == m_objectIdToIndex.end()) return;

    size_t index = it->second;

    // Swap with last and pop
    if (index < m_sdfObjects.size() - 1) {
        std::swap(m_sdfObjects[index], m_sdfObjects.back());
        // Update index map for swapped object
        for (auto& pair : m_objectIdToIndex) {
            if (pair.second == m_sdfObjects.size() - 1) {
                pair.second = index;
                break;
            }
        }
    }

    m_sdfObjects.pop_back();
    m_objectIdToIndex.erase(it);
}

void SDFRasterizer::UpdateSDFObject(uint32_t objectId, const glm::mat4& transform) {
    auto it = m_objectIdToIndex.find(objectId);
    if (it == m_objectIdToIndex.end()) return;

    m_sdfObjects[it->second].transform = transform;
    m_sdfObjects[it->second].inverseTransform = glm::inverse(transform);
}

void SDFRasterizer::ClearSDFObjects() {
    m_sdfObjects.clear();
    m_objectIdToIndex.clear();
    m_nextObjectId = 1;
}

void SDFRasterizer::BuildTileBounds(const Camera& camera) {
    ScopedTimer timer(m_stats.cpuTimeMs);

    // Clear previous frame data
    m_activeTiles.clear();
    m_tileObjectIndices.clear();
    m_activeTileCount = 0;

    const int tileSize = m_settings.sdfTileSize;
    const int width = m_settings.renderWidth;
    const int height = m_settings.renderHeight;

    // Build AABBs and cull for each tile
    for (int ty = 0; ty < m_tileGridSize.y; ++ty) {
        for (int tx = 0; tx < m_tileGridSize.x; ++tx) {
            TileAABB& tile = m_tileAABBs[ty * m_tileGridSize.x + tx];

            // Compute tile AABB in world space
            tile = ComputeTileAABB(tx, ty, camera);
            tile.sdfObjectIndices.clear();

            // Cull SDF objects against tile
            for (uint32_t i = 0; i < m_sdfObjects.size(); ++i) {
                if (TestSDFIntersectsTile(m_sdfObjects[i], tile)) {
                    tile.sdfObjectIndices.push_back(i);
                }
            }

            // Add to active tiles if not empty
            if (!tile.sdfObjectIndices.empty()) {
                TileData tileData;
                tileData.tileCoord = glm::ivec2(tx, ty);
                tileData.objectCount = static_cast<uint32_t>(tile.sdfObjectIndices.size());
                tileData.objectOffset = static_cast<uint32_t>(m_tileObjectIndices.size());

                m_activeTiles.push_back(tileData);
                m_tileObjectIndices.insert(m_tileObjectIndices.end(),
                                          tile.sdfObjectIndices.begin(),
                                          tile.sdfObjectIndices.end());
                m_activeTileCount++;
            }
        }
    }

    // Update statistics
    m_stats.tilesProcessed = m_tileGridSize.x * m_tileGridSize.y;
    m_stats.tilesCulled = m_stats.tilesProcessed - m_activeTileCount;
}

void SDFRasterizer::UploadTileData() {
    // Upload SDF objects
    if (!m_sdfObjects.empty()) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_sdfObjectBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     m_sdfObjects.size() * sizeof(SDFObjectGPU),
                     m_sdfObjects.data(),
                     GL_DYNAMIC_DRAW);
    }

    // Upload tile data
    if (!m_activeTiles.empty()) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_tileDataBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     m_activeTiles.size() * sizeof(TileData),
                     m_activeTiles.data(),
                     GL_DYNAMIC_DRAW);
    }

    // Upload tile object indices
    if (!m_tileObjectIndices.empty()) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_tileObjectIndexBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     m_tileObjectIndices.size() * sizeof(uint32_t),
                     m_tileObjectIndices.data(),
                     GL_DYNAMIC_DRAW);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void SDFRasterizer::DispatchRaymarch() {
    if (m_activeTileCount == 0) {
        // Clear framebuffer if no tiles
        m_framebuffer->Bind();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }

    // Bind compute shader
    m_raymarchShader->Bind();

    // Bind output textures
    glBindImageTexture(0, m_colorTexture->GetID(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glBindImageTexture(1, m_depthTexture->GetID(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);

    // Bind SSBOs
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_sdfObjectBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_tileDataBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_tileObjectIndexBuffer);

    // Set uniforms
    m_raymarchShader->SetMat4("u_viewProj", m_viewProjMatrix);
    m_raymarchShader->SetMat4("u_invViewProj", m_invViewProjMatrix);
    m_raymarchShader->SetVec3("u_cameraPos", m_cameraPosition);
    m_raymarchShader->SetVec3("u_cameraForward", m_cameraForward);
    m_raymarchShader->SetInt("u_maxSteps", m_settings.maxRaymarchSteps);
    m_raymarchShader->SetFloat("u_epsilon", m_settings.sdfRayEpsilon);
    m_raymarchShader->SetInt("u_tileSize", m_settings.sdfTileSize);
    m_raymarchShader->SetIVec2("u_resolution", glm::ivec2(m_settings.renderWidth, m_settings.renderHeight));
    m_raymarchShader->SetInt("u_enableShadows", m_settings.sdfEnableShadows ? 1 : 0);
    m_raymarchShader->SetInt("u_enableAO", m_settings.sdfEnableAO ? 1 : 0);
    m_raymarchShader->SetFloat("u_aoRadius", m_settings.sdfAORadius);
    m_raymarchShader->SetInt("u_aoSamples", m_settings.sdfAOSamples);

    // Dispatch compute shader (one thread per tile)
    const int tileSize = m_settings.sdfTileSize;
    int numGroupsX = (m_settings.renderWidth + tileSize - 1) / tileSize;
    int numGroupsY = (m_settings.renderHeight + tileSize - 1) / tileSize;

    glDispatchCompute(numGroupsX, numGroupsY, 1);

    // Memory barrier to ensure writes complete
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    m_stats.computeDispatches++;
    m_stats.sdfObjectsRendered = static_cast<uint32_t>(m_sdfObjects.size());
}

void SDFRasterizer::RenderDebugVisualization() {
    // TODO: Implement debug visualization
    // - Draw tile boundaries
    // - Show culled vs active tiles
    // - Display depth buffer
    // - Show per-tile object counts
}

bool SDFRasterizer::CreateBuffers() {
    // Create SSBO for SDF objects
    glGenBuffers(1, &m_sdfObjectBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_sdfObjectBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 1024 * sizeof(SDFObjectGPU), nullptr, GL_DYNAMIC_DRAW);

    // Create SSBO for tile data
    glGenBuffers(1, &m_tileDataBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_tileDataBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4096 * sizeof(TileData), nullptr, GL_DYNAMIC_DRAW);

    // Create SSBO for tile object indices
    glGenBuffers(1, &m_tileObjectIndexBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_tileObjectIndexBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 16384 * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

    // Create SSBO for statistics
    glGenBuffers(1, &m_statsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_statsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 256, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        spdlog::error("OpenGL error creating buffers: {}", error);
        return false;
    }

    return true;
}

bool SDFRasterizer::CreateShaders() {
    // Load raymarch compute shader
    m_raymarchShader = std::make_shared<Shader>();
    if (!m_raymarchShader->LoadCompute("assets/shaders/sdf_rasterize_tile.comp")) {
        spdlog::error("Failed to load raymarch compute shader");
        return false;
    }

    return true;
}

TileAABB SDFRasterizer::ComputeTileAABB(int tileX, int tileY, const Camera& camera) const {
    TileAABB aabb;

    const int tileSize = m_settings.sdfTileSize;
    const int width = m_settings.renderWidth;
    const int height = m_settings.renderHeight;

    // Screen-space bounds
    aabb.screenMin = glm::vec2(tileX * tileSize, tileY * tileSize);
    aabb.screenMax = glm::vec2(std::min((tileX + 1) * tileSize, width),
                               std::min((tileY + 1) * tileSize, height));

    // Convert screen corners to NDC
    glm::vec2 corners[4] = {
        glm::vec2(aabb.screenMin.x, aabb.screenMin.y),
        glm::vec2(aabb.screenMax.x, aabb.screenMin.y),
        glm::vec2(aabb.screenMin.x, aabb.screenMax.y),
        glm::vec2(aabb.screenMax.x, aabb.screenMax.y)
    };

    // Initialize bounds to camera position
    aabb.minWorld = m_cameraPosition;
    aabb.maxWorld = m_cameraPosition;

    // Raymarch from camera through tile corners to approximate world-space bounds
    const float maxDist = camera.GetFarPlane();

    for (int i = 0; i < 4; ++i) {
        // Screen to NDC
        glm::vec2 ndc = (corners[i] / glm::vec2(width, height)) * 2.0f - 1.0f;
        ndc.y = -ndc.y; // Flip Y

        // NDC to world ray
        glm::vec4 clipNear = glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
        glm::vec4 clipFar = glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);

        glm::vec4 worldNear = m_invViewProjMatrix * clipNear;
        glm::vec4 worldFar = m_invViewProjMatrix * clipFar;

        worldNear /= worldNear.w;
        worldFar /= worldFar.w;

        glm::vec3 rayOrigin = glm::vec3(worldNear);
        glm::vec3 rayDir = glm::normalize(glm::vec3(worldFar) - rayOrigin);

        // Extend ray to far plane
        glm::vec3 farPoint = rayOrigin + rayDir * maxDist;

        // Expand AABB
        aabb.minWorld = glm::min(aabb.minWorld, farPoint);
        aabb.maxWorld = glm::max(aabb.maxWorld, farPoint);
    }

    aabb.isEmpty = false;
    return aabb;
}

bool SDFRasterizer::TestSDFIntersectsTile(const SDFObjectGPU& object, const TileAABB& tile) const {
    // Get object center and radius from bounds
    glm::vec3 center = glm::vec3(object.bounds);
    float radius = object.bounds.w;

    // Simple sphere-AABB intersection test
    glm::vec3 closest = glm::clamp(center, tile.minWorld, tile.maxWorld);
    float distSq = glm::dot(closest - center, closest - center);

    return distSq <= radius * radius;
}

void SDFRasterizer::UpdateStats() {
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
