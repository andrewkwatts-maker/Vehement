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
    m_tileShader.reset();
    m_groundBatches.clear();
    m_wallBatches.clear();
    m_visibleTiles.clear();
    m_initialized = false;
}

void TileRenderer::CreateMeshes() {
    // Create ground quad mesh (1x1 quad on XZ plane)
    m_groundQuadMesh = Nova::Mesh::CreatePlane(1.0f, 1.0f, 1, 1);

    // Create wall box mesh (1x1x1 cube)
    m_wallBoxMesh = Nova::Mesh::CreateCube(1.0f);
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

} // namespace Vehement
