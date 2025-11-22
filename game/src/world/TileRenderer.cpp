#include "TileRenderer.hpp"
#include "../../engine/graphics/Renderer.hpp"
#include "../../engine/graphics/Mesh.hpp"
#include "../../engine/graphics/Shader.hpp"
#include "../../engine/graphics/ShaderManager.hpp"
#include "../../engine/graphics/Material.hpp"
#include "../../engine/scene/Camera.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {

// Face visibility flags for voxel rendering
enum VoxelFace : uint8_t {
    FACE_NONE   = 0,
    FACE_TOP    = 1 << 0,
    FACE_BOTTOM = 1 << 1,
    FACE_NORTH  = 1 << 2,
    FACE_SOUTH  = 1 << 3,
    FACE_EAST   = 1 << 4,
    FACE_WEST   = 1 << 5,
    FACE_ALL    = 0x3F
};

TileRenderer::TileRenderer() = default;

TileRenderer::~TileRenderer() {
    Shutdown();
}

bool TileRenderer::Initialize(Nova::Renderer& renderer, TileAtlas& atlas,
                              const TileRendererConfig& config) {
    m_renderer = &renderer;
    m_atlas = &atlas;
    m_config = config;

    CreateMeshes();
    CreateShaders();

    // Initialize animation states
    m_animationStates[TileAnimation::Water] = {TileAnimation::Water, 0.0f, 0, {0.0f, 0.0f}};
    m_animationStates[TileAnimation::Flicker] = {TileAnimation::Flicker, 0.0f, 0, {0.0f, 0.0f}};
    m_animationStates[TileAnimation::Scroll] = {TileAnimation::Scroll, 0.0f, 0, {0.0f, 0.0f}};

    m_initialized = true;
    return true;
}

void TileRenderer::Shutdown() {
    m_groundQuadMesh.reset();
    m_wallBoxMesh.reset();
    m_hexMesh.reset();
    m_hexOutlineMesh.reset();
    m_voxelMesh.reset();
    m_tileShader.reset();
    m_voxelShader.reset();
    m_groundBatches.clear();
    m_wallBatches.clear();
    m_voxelBatchesByLevel.clear();
    m_transparentVoxelBatches.clear();
    m_hexBatchesByLevel.clear();
    m_visibleTiles.clear();
    m_visibleVoxels.clear();
    m_initialized = false;
}

void TileRenderer::CreateMeshes() {
    // Create ground quad mesh (1x1 quad on XZ plane)
    m_groundQuadMesh = Nova::Mesh::CreatePlane(1.0f, 1.0f, 1, 1);

    // Create wall box mesh (1x1x1 cube)
    m_wallBoxMesh = Nova::Mesh::CreateCube(1.0f);

    // Create hex mesh
    CreateHexMesh();

    // Create voxel mesh
    CreateVoxelMesh();
}

void TileRenderer::CreateHexMesh() {
    // Create a hexagonal mesh (6-sided polygon)
    // The hex is oriented with pointy-top by default
    // Vertices are arranged around the center

    std::vector<float> vertices;
    std::vector<uint32_t> indices;

    // Center vertex
    vertices.insert(vertices.end(), {0.0f, 0.0f, 0.0f});  // position
    vertices.insert(vertices.end(), {0.0f, 1.0f, 0.0f});  // normal (up)
    vertices.insert(vertices.end(), {0.5f, 0.5f});        // UV center

    // 6 corner vertices
    for (int i = 0; i < 6; ++i) {
        float angle = glm::radians(30.0f + 60.0f * static_cast<float>(i)); // Pointy-top starts at 30 degrees
        float x = std::cos(angle);
        float z = std::sin(angle);

        // Position
        vertices.insert(vertices.end(), {x, 0.0f, z});
        // Normal
        vertices.insert(vertices.end(), {0.0f, 1.0f, 0.0f});
        // UV (mapped to hex shape)
        vertices.insert(vertices.end(), {0.5f + x * 0.5f, 0.5f + z * 0.5f});
    }

    // 6 triangles from center to each edge
    for (int i = 0; i < 6; ++i) {
        indices.push_back(0);           // Center
        indices.push_back(i + 1);       // Current corner
        indices.push_back((i % 6) + 2 > 6 ? 1 : (i % 6) + 2); // Next corner (wrap)
    }

    // Fix the last triangle
    indices[indices.size() - 1] = 1;

    // Create mesh using engine's mesh creation
    m_hexMesh = Nova::Mesh::CreateCube(1.0f); // Placeholder - real implementation would use vertex data

    // Create outline mesh for debugging
    m_hexOutlineMesh = Nova::Mesh::CreateCube(1.0f); // Placeholder
}

void TileRenderer::CreateVoxelMesh() {
    // Create a unit cube mesh for voxel rendering
    // This is similar to the wall box but with UVs set up for voxel texturing
    m_voxelMesh = Nova::Mesh::CreateCube(1.0f);
}

void TileRenderer::CreateShaders() {
    // Use the renderer's shader manager to load/create tile shader
    // For now, we'll use a basic textured shader
    // In a full implementation, this would be a custom tile shader

    // The shader will be set up when rendering
    // We rely on the engine's built-in shaders for now
}

void TileRenderer::Update(float deltaTime) {
    m_totalTime += deltaTime;

    // Update water animation
    auto& waterAnim = m_animationStates[TileAnimation::Water];
    waterAnim.time += deltaTime * m_config.waterAnimationSpeed;
    // Create a gentle wave effect
    waterAnim.uvOffset.x = std::sin(waterAnim.time * 0.5f) * 0.02f;
    waterAnim.uvOffset.y = std::cos(waterAnim.time * 0.3f) * 0.02f;

    // Update flicker animation
    auto& flickerAnim = m_animationStates[TileAnimation::Flicker];
    flickerAnim.time += deltaTime;
    // Random flicker effect
    flickerAnim.currentFrame = static_cast<int>(flickerAnim.time * 10.0f) % 3;

    // Update scroll animation
    auto& scrollAnim = m_animationStates[TileAnimation::Scroll];
    scrollAnim.time += deltaTime;
    scrollAnim.uvOffset.x = std::fmod(scrollAnim.time * 0.2f, 1.0f);
}

void TileRenderer::Render(const TileMap& tileMap, const Nova::Camera& camera) {
    if (!m_initialized || !m_renderer) return;

    m_currentMap = &tileMap;
    m_stats = RenderStats{};

    // Collect visible tiles
    CollectVisibleTiles(tileMap, camera);

    // Rebuild batches if needed
    if (m_batchesDirty || tileMap.IsDirty()) {
        BuildBatchesFromVisible();
        m_batchesDirty = false;
    }

    // Render ground tiles first (opaque)
    RenderGround(tileMap, camera);

    // Render walls on top
    RenderWalls(tileMap, camera);
}

void TileRenderer::RenderGround(const TileMap& tileMap, const Nova::Camera& camera) {
    if (!m_initialized || !m_renderer || !m_groundQuadMesh) return;

    m_renderer->SetDepthTest(true);
    m_renderer->SetDepthWrite(true);
    m_renderer->SetCulling(false);  // Quads visible from both sides

    // Render each batch
    for (const auto& [tileType, batch] : m_groundBatches) {
        if (batch.transforms.empty()) continue;

        // Bind texture for this batch
        m_atlas->BindTexture(tileType, 0);

        // Render each tile in the batch
        for (size_t i = 0; i < batch.transforms.size(); ++i) {
            m_renderer->DrawMesh(*m_groundQuadMesh,
                               m_renderer->GetShaderManager().GetShader("basic"),
                               batch.transforms[i]);
            m_stats.groundTilesRendered++;
            m_stats.drawCalls++;
            m_stats.triangles += 2;
        }
    }
}

void TileRenderer::RenderWalls(const TileMap& tileMap, const Nova::Camera& camera) {
    if (!m_initialized || !m_renderer || !m_wallBoxMesh) return;

    m_renderer->SetDepthTest(true);
    m_renderer->SetDepthWrite(true);
    m_renderer->SetCulling(true, true);  // Back face culling for solid walls

    // Render each wall batch
    for (const auto& [tileType, batch] : m_wallBatches) {
        if (batch.transforms.empty()) continue;

        // Bind texture for this batch
        m_atlas->BindTexture(tileType, 0);

        // Render each wall in the batch
        for (size_t i = 0; i < batch.transforms.size(); ++i) {
            m_renderer->DrawMesh(*m_wallBoxMesh,
                               m_renderer->GetShaderManager().GetShader("basic"),
                               batch.transforms[i]);
            m_stats.wallTilesRendered++;
            m_stats.drawCalls++;
            m_stats.triangles += 12;  // 6 faces * 2 triangles
        }
    }
}

void TileRenderer::RebuildBatches(const TileMap& tileMap) {
    m_currentMap = &tileMap;
    m_groundBatches.clear();
    m_wallBatches.clear();

    // Iterate over all tiles and build batches
    tileMap.ForEachTile([this](int x, int y, const Tile& tile) {
        if (tile.type == TileType::None) return;

        float tileSize = m_currentMap->GetTileSize();
        glm::vec3 worldPos = m_currentMap->TileToWorld(x, y);

        if (tile.isWall) {
            // Add to wall batch
            TileType sideTexture = tile.GetSideTexture();
            auto& batch = m_wallBatches[sideTexture];
            batch.textureType = sideTexture;
            batch.isWallBatch = true;

            // Create transform for wall (scaled cube)
            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(
                worldPos.x,
                m_config.groundY + tile.wallHeight * 0.5f,
                worldPos.z
            ));
            transform = glm::scale(transform, glm::vec3(
                tileSize,
                tile.wallHeight,
                tileSize
            ));

            batch.transforms.push_back(transform);
        } else {
            // Add to ground batch
            auto& batch = m_groundBatches[tile.type];
            batch.textureType = tile.type;
            batch.isWallBatch = false;

            // Create transform for ground quad
            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(
                worldPos.x,
                m_config.groundY + 0.001f,  // Slight offset to prevent z-fighting
                worldPos.z
            ));

            // Rotate to lie flat on XZ plane (quad is created on XY by default)
            transform = glm::rotate(transform, glm::radians(-90.0f), glm::vec3(1, 0, 0));

            // Apply tile rotation if any
            if (tile.rotation != 0) {
                transform = glm::rotate(transform,
                    glm::radians(static_cast<float>(tile.rotation)),
                    glm::vec3(0, 0, 1));
            }

            transform = glm::scale(transform, glm::vec3(tileSize, tileSize, 1.0f));

            batch.transforms.push_back(transform);
        }
    });

    m_batchesDirty = false;
}

void TileRenderer::CollectVisibleTiles(const TileMap& tileMap, const Nova::Camera& camera) {
    m_visibleTiles.clear();

    if (!m_config.enableFrustumCulling) {
        // No culling - add all tiles
        tileMap.ForEachTile([this](int x, int y, const Tile& tile) {
            if (tile.type != TileType::None) {
                m_visibleTiles.push_back({x, y, &tile, tile.isWall});
            }
        });
        return;
    }

    // Get camera position for distance culling
    glm::vec3 camPos = camera.GetPosition();

    // Calculate visible tile range based on view distance
    glm::ivec2 camTile = tileMap.WorldToTile(camPos.x, camPos.z);
    float tileSize = tileMap.GetTileSize();
    int tileRange = static_cast<int>(std::ceil(m_config.viewDistance / tileSize)) + 1;

    int minX = std::max(0, camTile.x - tileRange);
    int maxX = std::min(tileMap.GetWidth() - 1, camTile.x + tileRange);
    int minY = std::max(0, camTile.y - tileRange);
    int maxY = std::min(tileMap.GetHeight() - 1, camTile.y + tileRange);

    // Collect tiles in range
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const Tile* tile = tileMap.GetTile(x, y);
            if (!tile || tile->type == TileType::None) continue;

            float height = tile->isWall ? tile->wallHeight : 0.0f;

            if (IsTileVisible(x, y, height, camera)) {
                m_visibleTiles.push_back({x, y, tile, tile->isWall});
            } else {
                m_stats.tilesculled++;
            }

            // Check max visible tiles limit
            if (m_visibleTiles.size() >= static_cast<size_t>(m_config.maxVisibleTiles)) {
                return;
            }
        }
    }
}

void TileRenderer::BuildBatchesFromVisible() {
    m_groundBatches.clear();
    m_wallBatches.clear();

    if (!m_currentMap) return;

    float tileSize = m_currentMap->GetTileSize();

    for (const auto& visible : m_visibleTiles) {
        const Tile& tile = *visible.tile;
        glm::vec3 worldPos = m_currentMap->TileToWorld(visible.x, visible.y);

        if (visible.isWall) {
            // Add to wall batch
            TileType sideTexture = tile.GetSideTexture();
            auto& batch = m_wallBatches[sideTexture];
            batch.textureType = sideTexture;
            batch.isWallBatch = true;

            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(
                worldPos.x,
                m_config.groundY + tile.wallHeight * 0.5f,
                worldPos.z
            ));
            transform = glm::scale(transform, glm::vec3(
                tileSize,
                tile.wallHeight,
                tileSize
            ));

            batch.transforms.push_back(transform);
        } else {
            // Add to ground batch
            auto& batch = m_groundBatches[tile.type];
            batch.textureType = tile.type;
            batch.isWallBatch = false;

            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(
                worldPos.x,
                m_config.groundY + 0.001f,
                worldPos.z
            ));

            // Rotate to lie flat on XZ plane
            transform = glm::rotate(transform, glm::radians(-90.0f), glm::vec3(1, 0, 0));

            if (tile.rotation != 0) {
                transform = glm::rotate(transform,
                    glm::radians(static_cast<float>(tile.rotation)),
                    glm::vec3(0, 0, 1));
            }

            transform = glm::scale(transform, glm::vec3(tileSize, tileSize, 1.0f));

            batch.transforms.push_back(transform);
        }
    }
}

bool TileRenderer::IsTileVisible(int x, int y, float height, const Nova::Camera& camera) const {
    if (!m_currentMap) return false;

    glm::vec3 worldPos = m_currentMap->TileToWorld(x, y);
    float tileSize = m_currentMap->GetTileSize();

    // Create bounding sphere for the tile
    float radius = tileSize * 0.707f;  // Half diagonal
    if (height > 0.0f) {
        // For walls, expand the sphere to include height
        worldPos.y = m_config.groundY + height * 0.5f;
        radius = std::sqrt(tileSize * tileSize * 0.5f + height * height * 0.25f);
    }

    // Use camera's frustum culling
    return camera.IsInFrustum(worldPos, radius);
}

glm::vec2 TileRenderer::GetAnimationOffset(TileAnimation animation) const {
    auto it = m_animationStates.find(animation);
    if (it != m_animationStates.end()) {
        return it->second.uvOffset;
    }
    return glm::vec2(0.0f);
}

void TileRenderer::RenderGroundTile(int x, int y, const Tile& tile,
                                    const Nova::Camera& camera) {
    if (!m_groundQuadMesh || !m_currentMap) return;

    float tileSize = m_currentMap->GetTileSize();
    glm::vec3 worldPos = m_currentMap->TileToWorld(x, y);

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, glm::vec3(worldPos.x, m_config.groundY, worldPos.z));
    transform = glm::rotate(transform, glm::radians(-90.0f), glm::vec3(1, 0, 0));

    if (tile.rotation != 0) {
        transform = glm::rotate(transform,
            glm::radians(static_cast<float>(tile.rotation)),
            glm::vec3(0, 0, 1));
    }

    transform = glm::scale(transform, glm::vec3(tileSize, tileSize, 1.0f));

    m_atlas->BindTexture(tile.type, 0);
    m_renderer->DrawMesh(*m_groundQuadMesh,
                        m_renderer->GetShaderManager().GetShader("basic"),
                        transform);
}

void TileRenderer::RenderWallTile(int x, int y, const Tile& tile,
                                  const Nova::Camera& camera) {
    if (!m_wallBoxMesh || !m_currentMap) return;

    float tileSize = m_currentMap->GetTileSize();
    glm::vec3 worldPos = m_currentMap->TileToWorld(x, y);

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, glm::vec3(
        worldPos.x,
        m_config.groundY + tile.wallHeight * 0.5f,
        worldPos.z
    ));
    transform = glm::scale(transform, glm::vec3(tileSize, tile.wallHeight, tileSize));

    // Use side texture for walls
    TileType sideTexture = tile.GetSideTexture();
    m_atlas->BindTexture(sideTexture, 0);

    m_renderer->DrawMesh(*m_wallBoxMesh,
                        m_renderer->GetShaderManager().GetShader("basic"),
                        transform);
}

// ============================================================================
// 3D Voxel Rendering
// ============================================================================

void TileRenderer::RenderVoxelMap(const Voxel3DMap& voxelMap, const Nova::Camera& camera) {
    if (!m_initialized || !m_renderer) return;

    m_currentVoxelMap = &voxelMap;
    m_stats = RenderStats{};

    // Determine camera Z level from Y position
    const auto& config = voxelMap.GetConfig();
    m_cameraZLevel = static_cast<int>(camera.GetPosition().y / config.tileSizeZ);

    // Get visible Z range
    auto [minZ, maxZ] = GetVisibleZRange(camera, config.maxHeight);

    // Collect visible voxels
    CollectVisibleVoxels(voxelMap, camera);

    // Rebuild batches if needed
    if (m_voxelBatchesDirty || voxelMap.IsDirty()) {
        BuildVoxelBatchesFromVisible(voxelMap);
        m_voxelBatchesDirty = false;
    }

    // Render based on grid type
    if (voxelMap.IsHexGrid()) {
        RenderHexGrid(voxelMap, camera);
    } else {
        // Render floors first (opaque)
        if (m_config.renderFloors) {
            RenderVoxelFloors(voxelMap, camera);
        }

        // Render walls
        if (m_config.renderWalls) {
            RenderVoxelWalls(voxelMap, camera);
        }
    }

    m_stats.zLevelsRendered = maxZ - minZ + 1;
}

void TileRenderer::RenderVoxelLayer(const Voxel3DMap& voxelMap, const Nova::Camera& camera, int zLevel) {
    if (!m_initialized || !m_renderer || !m_voxelMesh) return;

    const auto& config = voxelMap.GetConfig();

    m_renderer->SetDepthTest(true);
    m_renderer->SetDepthWrite(true);
    m_renderer->SetCulling(true, true);

    // Get batches for this Z level
    auto levelIt = m_voxelBatchesByLevel.find(zLevel);
    if (levelIt == m_voxelBatchesByLevel.end()) return;

    for (const auto& [tileType, batch] : levelIt->second) {
        if (batch.transforms.empty()) continue;

        m_atlas->BindTexture(tileType, 0);

        for (size_t i = 0; i < batch.transforms.size(); ++i) {
            m_renderer->DrawMesh(*m_voxelMesh,
                               m_renderer->GetShaderManager().GetShader("basic"),
                               batch.transforms[i]);
            m_stats.voxelsRendered++;
            m_stats.drawCalls++;
            m_stats.triangles += 12;
        }
    }
}

void TileRenderer::RenderVoxelFloors(const Voxel3DMap& voxelMap, const Nova::Camera& camera) {
    if (!m_initialized || !m_renderer || !m_groundQuadMesh) return;

    const auto& config = voxelMap.GetConfig();

    m_renderer->SetDepthTest(true);
    m_renderer->SetDepthWrite(true);
    m_renderer->SetCulling(false);

    auto [minZ, maxZ] = GetVisibleZRange(camera, config.maxHeight);

    // Render floors for each visible Z level
    for (int z = minZ; z <= maxZ; ++z) {
        for (const auto& visible : m_visibleVoxels) {
            if (visible.pos.z != z) continue;
            if (!visible.voxel->isFloor) continue;

            glm::vec3 worldPos = voxelMap.VoxelToWorldCenter(visible.pos);

            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(
                worldPos.x,
                worldPos.y - config.tileSizeZ * 0.5f + 0.001f, // Top of voxel below
                worldPos.z
            ));
            transform = glm::rotate(transform, glm::radians(-90.0f), glm::vec3(1, 0, 0));
            transform = glm::scale(transform, glm::vec3(config.tileSizeXY, config.tileSizeXY, 1.0f));

            m_atlas->BindTexture(visible.voxel->type, 0);
            m_renderer->DrawMesh(*m_groundQuadMesh,
                               m_renderer->GetShaderManager().GetShader("basic"),
                               transform);
            m_stats.groundTilesRendered++;
            m_stats.drawCalls++;
            m_stats.triangles += 2;
        }
    }
}

void TileRenderer::RenderVoxelWalls(const Voxel3DMap& voxelMap, const Nova::Camera& camera) {
    if (!m_initialized || !m_renderer || !m_voxelMesh) return;

    const auto& config = voxelMap.GetConfig();

    m_renderer->SetDepthTest(true);
    m_renderer->SetDepthWrite(true);
    m_renderer->SetCulling(true, true);

    auto [minZ, maxZ] = GetVisibleZRange(camera, config.maxHeight);

    // Render solid voxels for each visible Z level
    for (int z = minZ; z <= maxZ; ++z) {
        for (const auto& visible : m_visibleVoxels) {
            if (visible.pos.z != z) continue;
            if (!visible.voxel->isSolid) continue;

            glm::vec3 worldPos = voxelMap.VoxelToWorldCenter(visible.pos);

            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, worldPos);
            transform = glm::scale(transform, glm::vec3(
                config.tileSizeXY,
                config.tileSizeZ,
                config.tileSizeXY
            ));

            m_atlas->BindTexture(visible.voxel->type, 0);
            m_renderer->DrawMesh(*m_voxelMesh,
                               m_renderer->GetShaderManager().GetShader("basic"),
                               transform);
            m_stats.wallTilesRendered++;
            m_stats.drawCalls++;
            m_stats.triangles += 12;
        }
    }
}

void TileRenderer::RebuildVoxelBatches(const Voxel3DMap& voxelMap) {
    m_voxelBatchesByLevel.clear();
    m_transparentVoxelBatches.clear();

    const auto& config = voxelMap.GetConfig();

    // Build batches from all non-empty voxels
    voxelMap.ForEachVoxel([this, &voxelMap, &config](const glm::ivec3& pos, const Voxel& voxel) {
        glm::vec3 worldPos = voxelMap.VoxelToWorldCenter(pos);

        if (voxel.isTransparent) {
            // Add to transparent batch
            auto& batch = m_transparentVoxelBatches[voxel.type];
            batch.textureType = voxel.type;
            batch.isTransparent = true;

            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, worldPos);
            transform = glm::scale(transform, glm::vec3(
                config.tileSizeXY, config.tileSizeZ, config.tileSizeXY));
            batch.transforms.push_back(transform);
        } else {
            // Add to level-based batch
            auto& levelBatches = m_voxelBatchesByLevel[pos.z];
            auto& batch = levelBatches[voxel.type];
            batch.textureType = voxel.type;
            batch.zLevel = pos.z;

            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, worldPos);
            transform = glm::scale(transform, glm::vec3(
                config.tileSizeXY, config.tileSizeZ, config.tileSizeXY));
            batch.transforms.push_back(transform);
        }
    });

    m_voxelBatchesDirty = false;
}

void TileRenderer::RebuildVoxelBatchesRegion(const Voxel3DMap& voxelMap,
                                             const glm::ivec3& min, const glm::ivec3& max) {
    // For now, just rebuild all batches
    // A more optimized version would only update the affected region
    RebuildVoxelBatches(voxelMap);
}

void TileRenderer::CollectVisibleVoxels(const Voxel3DMap& voxelMap, const Nova::Camera& camera) {
    m_visibleVoxels.clear();

    const auto& config = voxelMap.GetConfig();
    glm::vec3 camPos = camera.GetPosition();

    // Calculate visible range based on view distance
    int tileRangeXY = static_cast<int>(std::ceil(m_config.viewDistance / config.tileSizeXY)) + 1;

    glm::ivec3 camVoxel = voxelMap.WorldToVoxel(camPos);

    int minX = std::max(0, camVoxel.x - tileRangeXY);
    int maxX = std::min(config.mapWidth - 1, camVoxel.x + tileRangeXY);
    int minY = std::max(0, camVoxel.y - tileRangeXY);
    int maxY = std::min(config.mapHeight - 1, camVoxel.y + tileRangeXY);

    auto [minZ, maxZ] = GetVisibleZRange(camera, config.maxHeight);

    // Collect visible voxels
    for (int z = minZ; z <= maxZ; ++z) {
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                glm::ivec3 pos(x, y, z);
                const Voxel& voxel = voxelMap.GetVoxel(pos);

                if (voxel.IsEmpty()) continue;

                if (!m_config.enableFrustumCulling || IsVoxelVisible(pos, voxelMap, camera)) {
                    VisibleVoxel visible;
                    visible.pos = pos;
                    visible.voxel = &voxel;
                    visible.visibleFaces = GetVisibleVoxelFaces(pos, voxelMap);

                    glm::vec3 worldPos = voxelMap.VoxelToWorldCenter(pos);
                    visible.distanceToCamera = glm::length(worldPos - camPos);

                    m_visibleVoxels.push_back(visible);
                } else {
                    m_stats.tilesculled++;
                }

                if (m_visibleVoxels.size() >= static_cast<size_t>(m_config.maxVisibleTiles)) {
                    return;
                }
            }
        }
    }

    // Sort transparent voxels by distance (back to front)
    std::sort(m_visibleVoxels.begin(), m_visibleVoxels.end(),
        [](const VisibleVoxel& a, const VisibleVoxel& b) {
            // Sort opaque first, then by distance
            if (a.voxel->isTransparent != b.voxel->isTransparent) {
                return !a.voxel->isTransparent;
            }
            if (a.voxel->isTransparent) {
                return a.distanceToCamera > b.distanceToCamera; // Back to front for transparent
            }
            return a.distanceToCamera < b.distanceToCamera; // Front to back for opaque
        });
}

void TileRenderer::BuildVoxelBatchesFromVisible(const Voxel3DMap& voxelMap) {
    m_voxelBatchesByLevel.clear();
    m_transparentVoxelBatches.clear();

    const auto& config = voxelMap.GetConfig();

    for (const auto& visible : m_visibleVoxels) {
        glm::vec3 worldPos = voxelMap.VoxelToWorldCenter(visible.pos);

        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, worldPos);
        transform = glm::scale(transform, glm::vec3(
            config.tileSizeXY, config.tileSizeZ, config.tileSizeXY));

        if (visible.voxel->isTransparent) {
            auto& batch = m_transparentVoxelBatches[visible.voxel->type];
            batch.textureType = visible.voxel->type;
            batch.isTransparent = true;
            batch.transforms.push_back(transform);
        } else {
            auto& levelBatches = m_voxelBatchesByLevel[visible.pos.z];
            auto& batch = levelBatches[visible.voxel->type];
            batch.textureType = visible.voxel->type;
            batch.zLevel = visible.pos.z;
            batch.transforms.push_back(transform);
        }
    }
}

void TileRenderer::RenderVoxel(const glm::ivec3& pos, const Voxel& voxel,
                               const Voxel3DMap& voxelMap, const Nova::Camera& camera) {
    if (!m_voxelMesh) return;

    const auto& config = voxelMap.GetConfig();
    glm::vec3 worldPos = voxelMap.VoxelToWorldCenter(pos);

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, worldPos);
    transform = glm::scale(transform, glm::vec3(
        config.tileSizeXY, config.tileSizeZ, config.tileSizeXY));

    m_atlas->BindTexture(voxel.type, 0);
    m_renderer->DrawMesh(*m_voxelMesh,
                        m_renderer->GetShaderManager().GetShader("basic"),
                        transform);
}

bool TileRenderer::IsVoxelVisible(const glm::ivec3& pos, const Voxel3DMap& voxelMap,
                                  const Nova::Camera& camera) const {
    glm::vec3 worldPos = voxelMap.VoxelToWorldCenter(pos);
    const auto& config = voxelMap.GetConfig();

    float radius = std::sqrt(config.tileSizeXY * config.tileSizeXY +
                            config.tileSizeZ * config.tileSizeZ) * 0.5f;

    return camera.IsInFrustum(worldPos, radius);
}

std::pair<int, int> TileRenderer::GetVisibleZRange(const Nova::Camera& camera, int maxZ) const {
    int minZ = std::max(0, m_cameraZLevel - m_config.maxVisibleZLevels);
    int maxZLevel = std::min(maxZ - 1, m_cameraZLevel + m_config.maxVisibleZLevels);
    return {minZ, maxZLevel};
}

uint8_t TileRenderer::GetVisibleVoxelFaces(const glm::ivec3& pos, const Voxel3DMap& voxelMap) const {
    uint8_t faces = FACE_NONE;

    // Check each neighbor - if neighbor is empty or transparent, face is visible
    static const glm::ivec3 offsets[] = {
        {0, 1, 0},   // Top
        {0, -1, 0},  // Bottom
        {0, 0, -1},  // North
        {0, 0, 1},   // South
        {1, 0, 0},   // East
        {-1, 0, 0}   // West
    };

    static const uint8_t faceFlags[] = {
        FACE_TOP, FACE_BOTTOM, FACE_NORTH, FACE_SOUTH, FACE_EAST, FACE_WEST
    };

    for (int i = 0; i < 6; ++i) {
        glm::ivec3 neighborPos = pos + offsets[i];

        if (!voxelMap.IsInBounds(neighborPos)) {
            faces |= faceFlags[i];
            continue;
        }

        const Voxel& neighbor = voxelMap.GetVoxel(neighborPos);
        if (neighbor.IsEmpty() || neighbor.isTransparent) {
            faces |= faceFlags[i];
        }
    }

    return faces;
}

// ============================================================================
// Hex Grid Rendering
// ============================================================================

void TileRenderer::RenderHexGrid(const Voxel3DMap& voxelMap, const Nova::Camera& camera) {
    if (!m_initialized || !m_renderer) return;

    const auto& config = voxelMap.GetConfig();
    const HexGrid& hexGrid = voxelMap.GetHexGrid();

    auto [minZ, maxZ] = GetVisibleZRange(camera, config.maxHeight);

    m_renderer->SetDepthTest(true);
    m_renderer->SetDepthWrite(true);

    // Render hex tiles for each visible Z level
    for (int z = minZ; z <= maxZ; ++z) {
        for (const auto& visible : m_visibleVoxels) {
            if (visible.pos.z != z) continue;

            HexCoord hex = voxelMap.VoxelToHex(visible.pos);
            RenderHexTile(hex, z, *visible.voxel, hexGrid, camera);
        }
    }

    // Render hex outlines if enabled
    if (m_config.renderHexOutlines) {
        RenderHexOutlines(voxelMap, camera);
    }
}

void TileRenderer::RenderHexTile(const HexCoord& hex, int zLevel, const Voxel& voxel,
                                 const HexGrid& hexGrid, const Nova::Camera& camera) {
    if (!m_hexMesh || !m_currentVoxelMap) return;

    const auto& config = m_currentVoxelMap->GetConfig();

    glm::vec2 worldXZ = hexGrid.HexToWorld(hex);
    float worldY = static_cast<float>(zLevel) * config.tileSizeZ;

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, glm::vec3(worldXZ.x, worldY, worldXZ.y));
    transform = glm::scale(transform, glm::vec3(hexGrid.GetHexSize()));

    if (voxel.isSolid) {
        // Render as a hex column
        transform = glm::scale(transform, glm::vec3(1.0f, config.tileSizeZ / hexGrid.GetHexSize(), 1.0f));
        m_atlas->BindTexture(voxel.type, 0);
        m_renderer->DrawMesh(*m_wallBoxMesh, // Using cube for now
                            m_renderer->GetShaderManager().GetShader("basic"),
                            transform);
        m_stats.hexTilesRendered++;
        m_stats.drawCalls++;
        m_stats.triangles += 12;
    } else if (voxel.isFloor) {
        // Render as flat hex tile
        m_renderer->SetCulling(false);
        m_atlas->BindTexture(voxel.type, 0);
        m_renderer->DrawMesh(*m_hexMesh,
                            m_renderer->GetShaderManager().GetShader("basic"),
                            transform);
        m_stats.hexTilesRendered++;
        m_stats.drawCalls++;
        m_stats.triangles += 6;
    }
}

void TileRenderer::RenderHexOutlines(const Voxel3DMap& voxelMap, const Nova::Camera& camera) {
    // Debug rendering of hex grid outlines
    // Would use line rendering for the hex borders
    // Implementation depends on engine's line rendering capabilities
}

} // namespace Vehement
