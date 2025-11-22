#include "Voxel3DRenderer.hpp"
#include "TileMap.hpp"
#include "TileAtlas.hpp"
#include "../../../engine/graphics/Renderer.hpp"
#include "../../../engine/graphics/Shader.hpp"
#include "../../../engine/graphics/Framebuffer.hpp"
#include "../../../engine/core/Logger.hpp"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace Vehement {

// ============================================================================
// Voxel3D Implementation
// ============================================================================

glm::mat4 Voxel3D::GetTransform() const {
    glm::mat4 transform(1.0f);

    // Translate
    transform = glm::translate(transform, position);

    // Rotate (YXZ order for intuitive control)
    transform = glm::rotate(transform, glm::radians(rotation.y), glm::vec3(0, 1, 0));
    transform = glm::rotate(transform, glm::radians(rotation.x), glm::vec3(1, 0, 0));
    transform = glm::rotate(transform, glm::radians(rotation.z), glm::vec3(0, 0, 1));

    // Scale
    transform = glm::scale(transform, scale);

    return transform;
}

// ============================================================================
// Voxel3DMap Implementation
// ============================================================================

Voxel3DMap::Voxel3DMap() = default;

Voxel3DMap::~Voxel3DMap() {
    Clear();
}

void Voxel3DMap::Initialize(int width, int height, int depth, float cellSize) {
    Clear();

    m_width = width;
    m_height = height;
    m_depth = depth;
    m_cellSize = cellSize;
    m_dirty = true;
}

void Voxel3DMap::Clear() {
    m_voxels.clear();
    m_voxelCount = 0;
    m_dirty = true;
}

Voxel3D* Voxel3DMap::GetVoxel(int x, int y, int z) {
    if (!IsInBounds(x, y, z)) {
        return nullptr;
    }

    auto it = m_voxels.find(GetKey(x, y, z));
    if (it != m_voxels.end()) {
        return &it->second;
    }
    return nullptr;
}

const Voxel3D* Voxel3DMap::GetVoxel(int x, int y, int z) const {
    if (!IsInBounds(x, y, z)) {
        return nullptr;
    }

    auto it = m_voxels.find(GetKey(x, y, z));
    if (it != m_voxels.end()) {
        return &it->second;
    }
    return nullptr;
}

bool Voxel3DMap::SetVoxel(int x, int y, int z, const Voxel3D& voxel) {
    if (!IsInBounds(x, y, z)) {
        return false;
    }

    uint64_t key = GetKey(x, y, z);
    bool isNew = m_voxels.find(key) == m_voxels.end();

    Voxel3D v = voxel;
    v.position = GridToWorld(x, y, z);

    m_voxels[key] = v;

    if (isNew) {
        m_voxelCount++;
    }

    m_dirty = true;
    return true;
}

void Voxel3DMap::RemoveVoxel(int x, int y, int z) {
    if (!IsInBounds(x, y, z)) {
        return;
    }

    auto it = m_voxels.find(GetKey(x, y, z));
    if (it != m_voxels.end()) {
        m_voxels.erase(it);
        m_voxelCount--;
        m_dirty = true;
    }
}

bool Voxel3DMap::IsInBounds(int x, int y, int z) const {
    return x >= 0 && x < m_width &&
           y >= 0 && y < m_height &&
           z >= 0 && z < m_depth;
}

glm::ivec3 Voxel3DMap::WorldToGrid(const glm::vec3& worldPos) const {
    return glm::ivec3(
        static_cast<int>(std::floor(worldPos.x / m_cellSize)),
        static_cast<int>(std::floor(worldPos.y / m_cellSize)),
        static_cast<int>(std::floor(worldPos.z / m_cellSize))
    );
}

glm::vec3 Voxel3DMap::GridToWorld(int x, int y, int z) const {
    return glm::vec3(
        (x + 0.5f) * m_cellSize,
        (y + 0.5f) * m_cellSize,
        (z + 0.5f) * m_cellSize
    );
}

Voxel3D* Voxel3DMap::GetVoxelAtWorld(const glm::vec3& worldPos) {
    glm::ivec3 grid = WorldToGrid(worldPos);
    return GetVoxel(grid.x, grid.y, grid.z);
}

std::vector<Voxel3D*> Voxel3DMap::GetVoxelsOnFloor(int floorY) {
    std::vector<Voxel3D*> result;

    for (auto& [key, voxel] : m_voxels) {
        int y = static_cast<int>((key >> 16) & 0xFFFF);
        if (y == floorY) {
            result.push_back(&voxel);
        }
    }

    return result;
}

void Voxel3DMap::MarkDirty(int x, int y, int z, int w, int h, int d) {
    m_dirty = true;
    // In a more advanced implementation, we could track dirty regions
}

// ============================================================================
// Voxel3DRenderer Implementation
// ============================================================================

Voxel3DRenderer::Voxel3DRenderer() = default;

Voxel3DRenderer::~Voxel3DRenderer() {
    Shutdown();
}

bool Voxel3DRenderer::Initialize(Nova::Renderer& renderer, TileModelManager& modelManager,
                                  const Voxel3DRendererConfig& config) {
    if (m_initialized) {
        return true;
    }

    m_renderer = &renderer;
    m_modelManager = &modelManager;
    m_config = config;

    // Create shaders
    CreateShaders();

    // Create shadow map if enabled
    if (m_config.enableShadows) {
        CreateShadowMap();
    }

    m_initialized = true;
    return true;
}

void Voxel3DRenderer::Shutdown() {
    m_voxelShader.reset();
    m_shadowShader.reset();
    m_transparentShader.reset();
    m_shadowFramebuffer.reset();

    m_batches.clear();
    m_visibleVoxels.clear();
    m_floorVisibility.clear();

    m_initialized = false;
}

void Voxel3DRenderer::Update(float deltaTime) {
    m_totalTime += deltaTime;

    // Update any animated voxels
    // (handled in batch rendering via shader uniforms)
}

void Voxel3DRenderer::Render(const Voxel3DMap& map, const Nova::Camera& camera, int currentFloor) {
    if (!m_initialized) return;

    auto startTime = std::chrono::high_resolution_clock::now();

    m_currentFloor = currentFloor;
    ResetStats();

    // Collect visible voxels
    CollectVisibleVoxels(map, camera);

    // Sort for optimal rendering (opaque front-to-back, transparent back-to-front)
    SortVoxelsForRendering();

    // Render shadow pass first if enabled
    if (m_config.enableShadows && m_shadowShader) {
        // Assume directional light from above-front
        glm::vec3 lightDir = glm::normalize(glm::vec3(0.3f, -1.0f, 0.3f));
        RenderShadows(map, lightDir);
    }

    // Begin batch rendering
    BeginBatch();

    // Render each visible voxel
    for (const auto& visibleVoxel : m_visibleVoxels) {
        const Voxel3D* voxel = visibleVoxel.voxel;
        if (!voxel || !voxel->isVisible) continue;

        // Check floor visibility
        float alpha = 1.0f;
        bool visible = true;
        ApplyFloorVisibility(visibleVoxel.floorY, alpha, visible);

        if (!visible || alpha <= 0.0f) {
            m_stats.voxelsCulled++;
            continue;
        }

        // Add to batch with modified color alpha
        Voxel3D modifiedVoxel = *voxel;
        modifiedVoxel.color.a *= alpha;

        AddToBatch(modifiedVoxel);
        m_stats.voxelsRendered++;
    }

    // End batch and render
    EndBatch();

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.renderTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

void Voxel3DRenderer::RenderFloor(const Voxel3DMap& map, const Nova::Camera& camera, int floorY, bool abovePlayer) {
    if (!m_initialized) return;

    // Get voxels on this floor
    auto voxels = const_cast<Voxel3DMap&>(map).GetVoxelsOnFloor(floorY);

    float alpha = 1.0f;
    if (abovePlayer && m_config.enableFloorFading) {
        int floorsAbove = floorY - m_currentFloor;
        if (floorsAbove > 0) {
            if (floorsAbove > static_cast<int>(m_config.floorFadeDistance)) {
                alpha = m_config.floorFadeAlpha;
            } else {
                float t = static_cast<float>(floorsAbove) / m_config.floorFadeDistance;
                alpha = glm::mix(1.0f, m_config.floorFadeAlpha, t);
            }
        }
    }

    // Check custom floor visibility
    auto it = m_floorVisibility.find(floorY);
    if (it != m_floorVisibility.end()) {
        if (!it->second.visible) return;
        alpha *= it->second.alpha;
    }

    BeginBatch();

    for (Voxel3D* voxel : voxels) {
        if (!voxel || !voxel->isVisible) continue;

        if (!IsVoxelVisible(*voxel, camera)) {
            continue;
        }

        Voxel3D modifiedVoxel = *voxel;
        modifiedVoxel.color.a *= alpha;

        AddToBatch(modifiedVoxel);
        m_stats.voxelsRendered++;
    }

    EndBatch();
    m_stats.floorsRendered++;
}

void Voxel3DRenderer::RenderShadows(const Voxel3DMap& map, const glm::vec3& lightDirection) {
    if (!m_shadowFramebuffer || !m_shadowShader) return;

    // Calculate scene bounds for shadow map
    glm::vec3 sceneCenter(
        map.GetWidth() * map.GetCellSize() * 0.5f,
        map.GetHeight() * map.GetCellSize() * 0.5f,
        map.GetDepth() * map.GetCellSize() * 0.5f
    );
    float sceneRadius = glm::length(sceneCenter);

    m_lightSpaceMatrix = CalculateLightSpaceMatrix(lightDirection, sceneCenter, sceneRadius);

    // Bind shadow framebuffer
    m_shadowFramebuffer->Bind();
    glViewport(0, 0, m_config.shadowMapSize, m_config.shadowMapSize);
    glClear(GL_DEPTH_BUFFER_BIT);

    // Enable front-face culling for shadow pass (reduces peter-panning)
    glCullFace(GL_FRONT);

    // Render all shadow-casting voxels
    // Use the shadow shader
    // ... (simplified - full implementation would iterate and render)

    glCullFace(GL_BACK);
    m_shadowFramebuffer->Unbind();
}

void Voxel3DRenderer::SetFloorVisibility(int floor, float alpha) {
    FloorVisibility& vis = m_floorVisibility[floor];
    vis.floorY = floor;
    vis.alpha = glm::clamp(alpha, 0.0f, 1.0f);
    vis.visible = alpha > 0.0f;
}

void Voxel3DRenderer::HideFloorsAbove(int floor) {
    // Mark all floors above as invisible
    for (auto& [floorY, vis] : m_floorVisibility) {
        if (floorY > floor) {
            vis.visible = false;
        }
    }
}

void Voxel3DRenderer::ShowAllFloors() {
    for (auto& [floorY, vis] : m_floorVisibility) {
        vis.visible = true;
        vis.alpha = 1.0f;
    }
}

FloorVisibility Voxel3DRenderer::GetFloorVisibility(int floor) const {
    auto it = m_floorVisibility.find(floor);
    if (it != m_floorVisibility.end()) {
        return it->second;
    }

    // Default visibility
    FloorVisibility defaultVis;
    defaultVis.floorY = floor;
    return defaultVis;
}

void Voxel3DRenderer::SetCurrentFloor(int floor) {
    m_currentFloor = floor;
}

bool Voxel3DRenderer::IsVoxelVisible(const Voxel3D& voxel, const Nova::Camera& camera) const {
    if (!m_config.enableFrustumCulling) {
        return true;
    }

    // Simple distance check first
    glm::vec3 cameraPos = camera.GetPosition();
    float distance = glm::length(voxel.position - cameraPos);

    if (distance > m_config.maxRenderDistance) {
        return false;
    }

    // Frustum culling would go here (checking against camera frustum planes)
    // Simplified: just use distance check for now

    return true;
}

bool Voxel3DRenderer::IsVoxelOccluded(const Voxel3DMap& map, int x, int y, int z) const {
    if (!m_config.enableOcclusionCulling) {
        return false;
    }

    // Check if all 6 neighbors are solid
    bool occluded = true;

    const glm::ivec3 neighbors[6] = {
        {1, 0, 0}, {-1, 0, 0},
        {0, 1, 0}, {0, -1, 0},
        {0, 0, 1}, {0, 0, -1}
    };

    for (const auto& offset : neighbors) {
        const Voxel3D* neighbor = map.GetVoxel(x + offset.x, y + offset.y, z + offset.z);
        if (!neighbor || !neighbor->isSolid) {
            occluded = false;
            break;
        }
    }

    return occluded;
}

void Voxel3DRenderer::BeginBatch() {
    m_batches.clear();
    m_batchActive = true;
}

void Voxel3DRenderer::AddToBatch(const Voxel3D& voxel) {
    if (!m_batchActive || voxel.modelId.empty()) return;

    auto& batch = m_batches[voxel.modelId];
    batch.modelId = voxel.modelId;
    batch.transforms.push_back(voxel.GetTransform());
    batch.colors.push_back(voxel.color);

    // Use minimum alpha for batch (for transparency sorting)
    if (batch.transforms.size() == 1) {
        batch.alpha = voxel.color.a;
    } else {
        batch.alpha = std::min(batch.alpha, voxel.color.a);
    }
}

void Voxel3DRenderer::EndBatch() {
    if (!m_batchActive) return;

    FlushBatch();
    m_batchActive = false;
}

void Voxel3DRenderer::FlushBatch() {
    if (!m_modelManager) return;

    // Render opaque batches first
    for (auto& [modelId, batch] : m_batches) {
        if (batch.alpha >= 1.0f) {
            m_modelManager->RenderInstanced(modelId, batch.transforms, batch.colors);
            m_stats.drawCalls++;
        }
    }

    // Then render transparent batches (sorted back-to-front)
    for (auto& [modelId, batch] : m_batches) {
        if (batch.alpha < 1.0f) {
            // Enable alpha blending
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            m_modelManager->RenderInstanced(modelId, batch.transforms, batch.colors);
            m_stats.drawCalls++;

            glDisable(GL_BLEND);
        }
    }

    m_batches.clear();
}

void Voxel3DRenderer::SetConfig(const Voxel3DRendererConfig& config) {
    m_config = config;

    // Recreate shadow map if size changed
    if (m_config.enableShadows && m_shadowFramebuffer) {
        // Check if size changed and recreate if needed
    }
}

void Voxel3DRenderer::SetShader(std::shared_ptr<Nova::Shader> shader) {
    m_voxelShader = shader;
}

void Voxel3DRenderer::SetShadowShader(std::shared_ptr<Nova::Shader> shader) {
    m_shadowShader = shader;
}

void Voxel3DRenderer::ResetStats() {
    m_stats = RenderStats{};
}

void Voxel3DRenderer::CreateShaders() {
    // In a full implementation, we would load shaders from files
    // For now, this is a placeholder

    // The shaders would handle:
    // - Basic PBR/Phong lighting
    // - Instanced rendering via vertex attributes
    // - Shadow mapping
    // - Per-instance color tinting
    // - Transparency/alpha blending
}

void Voxel3DRenderer::CreateShadowMap() {
    m_shadowFramebuffer = std::make_unique<Nova::Framebuffer>();
    m_shadowFramebuffer->CreateDepthOnly(m_config.shadowMapSize, m_config.shadowMapSize);
}

void Voxel3DRenderer::CollectVisibleVoxels(const Voxel3DMap& map, const Nova::Camera& camera) {
    m_visibleVoxels.clear();
    m_visibleVoxels.reserve(m_config.maxVisibleVoxels);

    glm::vec3 cameraPos = camera.GetPosition();

    // Iterate all voxels in the map
    const_cast<Voxel3DMap&>(map).ForEachVoxel([&](int x, int y, int z, Voxel3D& voxel) {
        if (!voxel.isVisible) return;

        // Frustum culling
        if (!IsVoxelVisible(voxel, camera)) {
            m_stats.voxelsCulled++;
            return;
        }

        // Occlusion culling
        if (IsVoxelOccluded(map, x, y, z)) {
            m_stats.voxelsCulled++;
            return;
        }

        // Add to visible list
        VisibleVoxel vis;
        vis.voxel = &voxel;
        vis.floorY = y;
        vis.distanceToCamera = glm::length(voxel.position - cameraPos);

        m_visibleVoxels.push_back(vis);

        // Limit visible voxels
        if (m_visibleVoxels.size() >= static_cast<size_t>(m_config.maxVisibleVoxels)) {
            return;
        }
    });
}

void Voxel3DRenderer::SortVoxelsForRendering() {
    // Sort by:
    // 1. Opaque first, then transparent
    // 2. Opaque: front-to-back (for early-Z rejection)
    // 3. Transparent: back-to-front (for correct blending)

    std::sort(m_visibleVoxels.begin(), m_visibleVoxels.end(),
        [](const VisibleVoxel& a, const VisibleVoxel& b) {
            bool aOpaque = a.voxel->color.a >= 1.0f;
            bool bOpaque = b.voxel->color.a >= 1.0f;

            if (aOpaque != bOpaque) {
                return aOpaque;  // Opaque first
            }

            if (aOpaque) {
                return a.distanceToCamera < b.distanceToCamera;  // Front to back
            } else {
                return a.distanceToCamera > b.distanceToCamera;  // Back to front
            }
        });
}

void Voxel3DRenderer::ApplyFloorVisibility(int floorY, float& outAlpha, bool& outVisible) {
    outAlpha = 1.0f;
    outVisible = true;

    // Check custom visibility first
    auto it = m_floorVisibility.find(floorY);
    if (it != m_floorVisibility.end()) {
        outVisible = it->second.visible;
        outAlpha = it->second.alpha;

        if (!outVisible) return;
    }

    // Apply automatic floor fading for floors above current
    if (m_config.enableFloorFading && floorY > m_currentFloor) {
        int floorsAbove = floorY - m_currentFloor;

        if (floorsAbove > static_cast<int>(m_config.floorFadeDistance)) {
            outAlpha *= m_config.floorFadeAlpha;
        } else {
            float t = static_cast<float>(floorsAbove) / m_config.floorFadeDistance;
            outAlpha *= glm::mix(1.0f, m_config.floorFadeAlpha, t);
        }
    }
}

glm::mat4 Voxel3DRenderer::CalculateLightSpaceMatrix(const glm::vec3& lightDir,
                                                      const glm::vec3& sceneCenter,
                                                      float sceneRadius) {
    // Create orthographic projection for directional light
    float orthoSize = sceneRadius * 1.5f;

    glm::mat4 lightProjection = glm::ortho(
        -orthoSize, orthoSize,
        -orthoSize, orthoSize,
        0.1f, sceneRadius * 3.0f
    );

    // Look at scene center from light direction
    glm::vec3 lightPos = sceneCenter - lightDir * sceneRadius;
    glm::mat4 lightView = glm::lookAt(lightPos, sceneCenter, glm::vec3(0, 1, 0));

    return lightProjection * lightView;
}

// ============================================================================
// Voxel3DBuilder Implementation
// ============================================================================

Voxel3DBuilder::Voxel3DBuilder(Voxel3DMap& map, TileModelManager& modelManager)
    : m_map(map)
    , m_modelManager(modelManager)
{
}

void Voxel3DBuilder::PlaceVoxel(int x, int y, int z, const std::string& modelId,
                                const glm::vec3& rotation, const glm::vec4& color) {
    Voxel3D voxel;
    voxel.modelId = modelId;
    voxel.rotation = rotation;
    voxel.color = color;

    m_map.SetVoxel(x, y, z, voxel);
}

void Voxel3DBuilder::FillBox(int x1, int y1, int z1, int x2, int y2, int z2,
                             const std::string& modelId, const glm::vec4& color) {
    int minX = std::min(x1, x2);
    int maxX = std::max(x1, x2);
    int minY = std::min(y1, y2);
    int maxY = std::max(y1, y2);
    int minZ = std::min(z1, z2);
    int maxZ = std::max(z1, z2);

    for (int y = minY; y <= maxY; ++y) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                PlaceVoxel(x, y, z, modelId, glm::vec3(0), color);
            }
        }
    }
}

void Voxel3DBuilder::HollowBox(int x1, int y1, int z1, int x2, int y2, int z2,
                               const std::string& wallModelId,
                               const std::string& floorModelId,
                               const std::string& ceilingModelId) {
    int minX = std::min(x1, x2);
    int maxX = std::max(x1, x2);
    int minY = std::min(y1, y2);
    int maxY = std::max(y1, y2);
    int minZ = std::min(z1, z2);
    int maxZ = std::max(z1, z2);

    for (int y = minY; y <= maxY; ++y) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                bool isEdge = (x == minX || x == maxX ||
                               y == minY || y == maxY ||
                               z == minZ || z == maxZ);

                if (isEdge) {
                    std::string model = wallModelId;

                    // Use floor model for bottom
                    if (y == minY && !floorModelId.empty()) {
                        model = floorModelId;
                    }
                    // Use ceiling model for top
                    else if (y == maxY && !ceilingModelId.empty()) {
                        model = ceilingModelId;
                    }

                    PlaceVoxel(x, y, z, model);
                }
            }
        }
    }
}

void Voxel3DBuilder::CreateFloor(int y, int x1, int z1, int x2, int z2,
                                  const std::string& modelId, const glm::vec4& color) {
    int minX = std::min(x1, x2);
    int maxX = std::max(x1, x2);
    int minZ = std::min(z1, z2);
    int maxZ = std::max(z1, z2);

    for (int z = minZ; z <= maxZ; ++z) {
        for (int x = minX; x <= maxX; ++x) {
            PlaceVoxel(x, y, z, modelId, glm::vec3(0), color);
        }
    }
}

void Voxel3DBuilder::CreateWalls(int y, int height, int x1, int z1, int x2, int z2,
                                  const std::string& modelId) {
    int minX = std::min(x1, x2);
    int maxX = std::max(x1, x2);
    int minZ = std::min(z1, z2);
    int maxZ = std::max(z1, z2);

    for (int h = 0; h < height; ++h) {
        int currentY = y + h;

        // Front wall (minZ)
        for (int x = minX; x <= maxX; ++x) {
            PlaceVoxel(x, currentY, minZ, modelId);
        }

        // Back wall (maxZ)
        for (int x = minX; x <= maxX; ++x) {
            PlaceVoxel(x, currentY, maxZ, modelId);
        }

        // Left wall (minX), excluding corners
        for (int z = minZ + 1; z < maxZ; ++z) {
            PlaceVoxel(minX, currentY, z, modelId);
        }

        // Right wall (maxX), excluding corners
        for (int z = minZ + 1; z < maxZ; ++z) {
            PlaceVoxel(maxX, currentY, z, modelId);
        }
    }
}

void Voxel3DBuilder::CreateStairs(int startX, int startY, int startZ,
                                   int direction, int steps, const std::string& modelId) {
    // Direction: 0=+X, 1=-X, 2=+Z, 3=-Z
    glm::ivec3 dir(0);
    float rotation = 0.0f;

    switch (direction) {
        case 0: dir = glm::ivec3(1, 0, 0); rotation = 90.0f; break;
        case 1: dir = glm::ivec3(-1, 0, 0); rotation = -90.0f; break;
        case 2: dir = glm::ivec3(0, 0, 1); rotation = 0.0f; break;
        case 3: dir = glm::ivec3(0, 0, -1); rotation = 180.0f; break;
    }

    for (int i = 0; i < steps; ++i) {
        int x = startX + dir.x * i;
        int y = startY + i;
        int z = startZ + dir.z * i;

        PlaceVoxel(x, y, z, modelId, glm::vec3(0, rotation, 0));
    }
}

void Voxel3DBuilder::CreateRamp(int x1, int y1, int z1, int x2, int y2, int z2,
                                 const std::string& modelId) {
    // Create a simple linear ramp
    int dx = x2 - x1;
    int dy = y2 - y1;
    int dz = z2 - z1;

    int steps = std::max({std::abs(dx), std::abs(dz)});
    if (steps == 0) return;

    float stepX = static_cast<float>(dx) / steps;
    float stepY = static_cast<float>(dy) / steps;
    float stepZ = static_cast<float>(dz) / steps;

    for (int i = 0; i <= steps; ++i) {
        int x = x1 + static_cast<int>(stepX * i);
        int y = y1 + static_cast<int>(stepY * i);
        int z = z1 + static_cast<int>(stepZ * i);

        // Calculate rotation based on direction
        float rotation = std::atan2(static_cast<float>(dz), static_cast<float>(dx));

        PlaceVoxel(x, y, z, modelId, glm::vec3(0, glm::degrees(rotation), 0));
    }
}

void Voxel3DBuilder::CreateColumn(int x, int z, int y1, int y2, const std::string& modelId) {
    int minY = std::min(y1, y2);
    int maxY = std::max(y1, y2);

    for (int y = minY; y <= maxY; ++y) {
        PlaceVoxel(x, y, z, modelId);
    }
}

void Voxel3DBuilder::PlaceHexTile(int hexQ, int hexR, int y,
                                   const std::string& modelId, float hexRadius) {
    // Convert hex coordinates to world coordinates
    // Using axial coordinates (q, r)
    float x = hexRadius * (3.0f / 2.0f * hexQ);
    float z = hexRadius * (std::sqrt(3.0f) / 2.0f * hexQ + std::sqrt(3.0f) * hexR);

    // Convert to grid coordinates
    float cellSize = m_map.GetCellSize();
    int gridX = static_cast<int>(std::round(x / cellSize));
    int gridZ = static_cast<int>(std::round(z / cellSize));

    PlaceVoxel(gridX, y, gridZ, modelId);
}

void Voxel3DBuilder::FillHexRegion(int centerQ, int centerR, int radius, int y,
                                    const std::string& modelId, float hexRadius) {
    // Fill hexagonal region using cube coordinates
    for (int q = -radius; q <= radius; ++q) {
        int r1 = std::max(-radius, -q - radius);
        int r2 = std::min(radius, -q + radius);

        for (int r = r1; r <= r2; ++r) {
            PlaceHexTile(centerQ + q, centerR + r, y, modelId, hexRadius);
        }
    }
}

} // namespace Vehement
