#pragma once

#include "Tile.hpp"
#include "TileMap.hpp"
#include "TileAtlas.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Nova {
    class Renderer;
    class Camera;
    class Mesh;
    class Shader;
    class Material;
}

namespace Vehement {

/**
 * @brief Render batch for tiles sharing the same texture
 */
struct TileBatch {
    TileType textureType = TileType::None;
    std::vector<glm::mat4> transforms;
    std::vector<glm::vec4> uvRects;  // UV rect per instance (minU, minV, maxU, maxV)
    bool isWallBatch = false;
    bool dirty = true;
};

/**
 * @brief Configuration for tile rendering
 */
struct TileRendererConfig {
    float groundY = 0.0f;           // Y position for ground tiles
    bool enableFrustumCulling = true;
    bool enableBatching = true;
    int maxVisibleTiles = 10000;    // Maximum tiles to render per frame
    float viewDistance = 100.0f;    // Maximum view distance for tiles
    bool renderWaterAnimated = true;
    float waterAnimationSpeed = 1.0f;
};

/**
 * @brief Animation state for animated tiles
 */
struct TileAnimationState {
    TileAnimation type = TileAnimation::None;
    float time = 0.0f;
    int currentFrame = 0;
    glm::vec2 uvOffset{0.0f};
};

/**
 * @brief Renders tile maps in 3D
 *
 * Features:
 * - Ground tiles rendered as textured quads on Y=0 plane
 * - Wall tiles extruded as 3D boxes with wall textures on sides
 * - Batch rendering by texture to minimize draw calls
 * - Frustum culling for visible tiles only
 * - Support for tile animations (water)
 */
class TileRenderer {
public:
    TileRenderer();
    ~TileRenderer();

    // Non-copyable
    TileRenderer(const TileRenderer&) = delete;
    TileRenderer& operator=(const TileRenderer&) = delete;

    /**
     * @brief Initialize the renderer
     * @param renderer The Nova engine renderer
     * @param atlas The tile atlas for textures
     * @param config Renderer configuration
     */
    bool Initialize(Nova::Renderer& renderer, TileAtlas& atlas,
                    const TileRendererConfig& config = {});

    /**
     * @brief Shutdown and cleanup resources
     */
    void Shutdown();

    /**
     * @brief Update animations
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render the tile map
     * @param tileMap The tile map to render
     * @param camera The camera for view/projection
     */
    void Render(const TileMap& tileMap, const Nova::Camera& camera);

    /**
     * @brief Render only ground tiles
     */
    void RenderGround(const TileMap& tileMap, const Nova::Camera& camera);

    /**
     * @brief Render only wall tiles
     */
    void RenderWalls(const TileMap& tileMap, const Nova::Camera& camera);

    /**
     * @brief Rebuild render batches (call after map changes)
     */
    void RebuildBatches(const TileMap& tileMap);

    /**
     * @brief Mark batches as dirty (will rebuild on next render)
     */
    void InvalidateBatches() { m_batchesDirty = true; }

    // Configuration
    void SetConfig(const TileRendererConfig& config) { m_config = config; }
    const TileRendererConfig& GetConfig() const { return m_config; }

    // Statistics
    struct RenderStats {
        uint32_t groundTilesRendered = 0;
        uint32_t wallTilesRendered = 0;
        uint32_t drawCalls = 0;
        uint32_t triangles = 0;
        uint32_t tilesculled = 0;
    };

    const RenderStats& GetStats() const { return m_stats; }

private:
    /**
     * @brief Create mesh resources
     */
    void CreateMeshes();

    /**
     * @brief Create shader resources
     */
    void CreateShaders();

    /**
     * @brief Collect visible tiles for rendering
     */
    void CollectVisibleTiles(const TileMap& tileMap, const Nova::Camera& camera);

    /**
     * @brief Build batches from visible tiles
     */
    void BuildBatchesFromVisible();

    /**
     * @brief Render a single ground tile
     */
    void RenderGroundTile(int x, int y, const Tile& tile, const Nova::Camera& camera);

    /**
     * @brief Render a single wall tile
     */
    void RenderWallTile(int x, int y, const Tile& tile, const Nova::Camera& camera);

    /**
     * @brief Check if a tile is visible in the frustum
     */
    bool IsTileVisible(int x, int y, float height, const Nova::Camera& camera) const;

    /**
     * @brief Get animation UV offset
     */
    glm::vec2 GetAnimationOffset(TileAnimation animation) const;

    Nova::Renderer* m_renderer = nullptr;
    TileAtlas* m_atlas = nullptr;
    TileRendererConfig m_config;
    bool m_initialized = false;

    // Meshes
    std::unique_ptr<Nova::Mesh> m_groundQuadMesh;
    std::unique_ptr<Nova::Mesh> m_wallBoxMesh;

    // Shaders
    std::shared_ptr<Nova::Shader> m_tileShader;

    // Batching
    std::unordered_map<TileType, TileBatch> m_groundBatches;
    std::unordered_map<TileType, TileBatch> m_wallBatches;
    bool m_batchesDirty = true;

    // Visible tile cache
    struct VisibleTile {
        int x, y;
        const Tile* tile;
        bool isWall;
    };
    std::vector<VisibleTile> m_visibleTiles;

    // Animation state
    std::unordered_map<TileAnimation, TileAnimationState> m_animationStates;
    float m_totalTime = 0.0f;

    // Current render state
    const TileMap* m_currentMap = nullptr;
    RenderStats m_stats;
};

} // namespace Vehement
