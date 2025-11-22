#pragma once

#include "Tile.hpp"
#include "TileMap.hpp"
#include "TileAtlas.hpp"
#include "Voxel3DMap.hpp"
#include "HexGrid.hpp"
#include "WorldConfig.hpp"
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
 * @brief Render batch for 3D voxels
 */
struct VoxelBatch {
    TileType textureType = TileType::None;
    std::vector<glm::mat4> transforms;
    std::vector<glm::vec4> uvRects;
    int zLevel = 0;                  // Z level for layer sorting
    bool isTransparent = false;      // Render in transparent pass
    bool dirty = true;
};

/**
 * @brief Render batch for hex tiles
 */
struct HexBatch {
    TileType textureType = TileType::None;
    std::vector<glm::mat4> transforms;
    std::vector<HexCoord> hexCoords;
    int zLevel = 0;
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

    // 3D/Voxel rendering settings
    bool enable3DRendering = true;
    int maxVisibleZLevels = 8;      // Z levels above/below camera to render
    bool renderFloors = true;
    bool renderCeilings = true;
    bool renderWalls = true;
    float voxelLODDistance = 32.0f; // Distance for voxel LOD

    // Hex grid rendering settings
    bool useHexGrid = true;
    bool renderHexOutlines = false;  // Debug: render hex borders
    float hexOutlineWidth = 0.02f;
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
 * @brief Renders tile maps in 3D with support for voxels and hex grids
 *
 * Features:
 * - Ground tiles rendered as textured quads on Y=0 plane
 * - Wall tiles extruded as 3D boxes with wall textures on sides
 * - Full 3D voxel rendering with multi-story buildings
 * - Hexagonal grid support with proper hex tile rendering
 * - Batch rendering by texture to minimize draw calls
 * - Frustum culling for visible tiles only
 * - Support for tile animations (water)
 * - Z-level based rendering for multi-story structures
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

    // ========== 2D TileMap Rendering (Legacy) ==========

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

    // ========== 3D Voxel Rendering ==========

    /**
     * @brief Render a 3D voxel map
     * @param voxelMap The voxel map to render
     * @param camera The camera for view/projection
     */
    void RenderVoxelMap(const Voxel3DMap& voxelMap, const Nova::Camera& camera);

    /**
     * @brief Render a specific Z level of the voxel map
     * @param voxelMap The voxel map
     * @param camera The camera
     * @param zLevel The Z level to render
     */
    void RenderVoxelLayer(const Voxel3DMap& voxelMap, const Nova::Camera& camera, int zLevel);

    /**
     * @brief Render voxel map floors (horizontal surfaces)
     */
    void RenderVoxelFloors(const Voxel3DMap& voxelMap, const Nova::Camera& camera);

    /**
     * @brief Render voxel map walls (vertical surfaces)
     */
    void RenderVoxelWalls(const Voxel3DMap& voxelMap, const Nova::Camera& camera);

    /**
     * @brief Rebuild voxel render batches
     */
    void RebuildVoxelBatches(const Voxel3DMap& voxelMap);

    /**
     * @brief Rebuild batches for a specific region (optimization)
     */
    void RebuildVoxelBatchesRegion(const Voxel3DMap& voxelMap,
                                   const glm::ivec3& min, const glm::ivec3& max);

    // ========== Hex Grid Rendering ==========

    /**
     * @brief Render hex grid using voxel map data
     * @param voxelMap The voxel map (with hex grid)
     * @param camera The camera
     */
    void RenderHexGrid(const Voxel3DMap& voxelMap, const Nova::Camera& camera);

    /**
     * @brief Render a single hex tile
     * @param hex The hex coordinate
     * @param zLevel The Z level
     * @param voxel The voxel data
     * @param hexGrid The hex grid for coordinate conversion
     * @param camera The camera
     */
    void RenderHexTile(const HexCoord& hex, int zLevel, const Voxel& voxel,
                       const HexGrid& hexGrid, const Nova::Camera& camera);

    /**
     * @brief Render hex outlines for debugging
     */
    void RenderHexOutlines(const Voxel3DMap& voxelMap, const Nova::Camera& camera);

    /**
     * @brief Create hex mesh (6-sided polygon)
     */
    void CreateHexMesh();

    // ========== Common ==========

    /**
     * @brief Mark batches as dirty (will rebuild on next render)
     */
    void InvalidateBatches() { m_batchesDirty = true; m_voxelBatchesDirty = true; }

    /**
     * @brief Invalidate only voxel batches
     */
    void InvalidateVoxelBatches() { m_voxelBatchesDirty = true; }

    // Configuration
    void SetConfig(const TileRendererConfig& config) { m_config = config; }
    const TileRendererConfig& GetConfig() const { return m_config; }

    /**
     * @brief Set the current camera Z level for layer-based rendering
     */
    void SetCameraZLevel(int zLevel) { m_cameraZLevel = zLevel; }
    int GetCameraZLevel() const { return m_cameraZLevel; }

    // Statistics
    struct RenderStats {
        uint32_t groundTilesRendered = 0;
        uint32_t wallTilesRendered = 0;
        uint32_t voxelsRendered = 0;
        uint32_t hexTilesRendered = 0;
        uint32_t drawCalls = 0;
        uint32_t triangles = 0;
        uint32_t tilesculled = 0;
        uint32_t zLevelsRendered = 0;
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

    // ========== 3D/Voxel Private Methods ==========

    /**
     * @brief Collect visible voxels for rendering
     */
    void CollectVisibleVoxels(const Voxel3DMap& voxelMap, const Nova::Camera& camera);

    /**
     * @brief Build voxel batches from visible voxels
     */
    void BuildVoxelBatchesFromVisible(const Voxel3DMap& voxelMap);

    /**
     * @brief Render a single voxel
     */
    void RenderVoxel(const glm::ivec3& pos, const Voxel& voxel,
                     const Voxel3DMap& voxelMap, const Nova::Camera& camera);

    /**
     * @brief Check if a voxel is visible in the frustum
     */
    bool IsVoxelVisible(const glm::ivec3& pos, const Voxel3DMap& voxelMap,
                        const Nova::Camera& camera) const;

    /**
     * @brief Get visible Z level range based on camera position
     */
    std::pair<int, int> GetVisibleZRange(const Nova::Camera& camera, int maxZ) const;

    /**
     * @brief Create voxel cube mesh with proper face culling info
     */
    void CreateVoxelMesh();

    /**
     * @brief Determine which faces of a voxel need rendering
     */
    uint8_t GetVisibleVoxelFaces(const glm::ivec3& pos, const Voxel3DMap& voxelMap) const;

    Nova::Renderer* m_renderer = nullptr;
    TileAtlas* m_atlas = nullptr;
    TileRendererConfig m_config;
    bool m_initialized = false;

    // Meshes
    std::unique_ptr<Nova::Mesh> m_groundQuadMesh;
    std::unique_ptr<Nova::Mesh> m_wallBoxMesh;
    std::unique_ptr<Nova::Mesh> m_hexMesh;           // Hex tile mesh
    std::unique_ptr<Nova::Mesh> m_hexOutlineMesh;    // Hex outline for debug
    std::unique_ptr<Nova::Mesh> m_voxelMesh;         // Full voxel cube

    // Shaders
    std::shared_ptr<Nova::Shader> m_tileShader;
    std::shared_ptr<Nova::Shader> m_voxelShader;

    // 2D Tile Batching
    std::unordered_map<TileType, TileBatch> m_groundBatches;
    std::unordered_map<TileType, TileBatch> m_wallBatches;
    bool m_batchesDirty = true;

    // 3D Voxel Batching (organized by Z level and texture)
    std::unordered_map<int, std::unordered_map<TileType, VoxelBatch>> m_voxelBatchesByLevel;
    std::unordered_map<TileType, VoxelBatch> m_transparentVoxelBatches;  // Separate for alpha sorting
    bool m_voxelBatchesDirty = true;

    // Hex Batching
    std::unordered_map<int, std::unordered_map<TileType, HexBatch>> m_hexBatchesByLevel;

    // Visible tile cache (2D)
    struct VisibleTile {
        int x, y;
        const Tile* tile;
        bool isWall;
    };
    std::vector<VisibleTile> m_visibleTiles;

    // Visible voxel cache (3D)
    struct VisibleVoxel {
        glm::ivec3 pos;
        const Voxel* voxel;
        uint8_t visibleFaces;  // Bitfield of which faces are visible
        float distanceToCamera;
    };
    std::vector<VisibleVoxel> m_visibleVoxels;

    // Animation state
    std::unordered_map<TileAnimation, TileAnimationState> m_animationStates;
    float m_totalTime = 0.0f;

    // Current render state
    const TileMap* m_currentMap = nullptr;
    const Voxel3DMap* m_currentVoxelMap = nullptr;
    int m_cameraZLevel = 0;
    RenderStats m_stats;
};

} // namespace Vehement
