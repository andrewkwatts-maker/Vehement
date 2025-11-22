#pragma once

#include "TileModel.hpp"
#include "TileModelManager.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {
    class Renderer;
    class Camera;
    class Shader;
    class Framebuffer;
}

namespace Vehement {

// Forward declarations
class TileMap;
class TileAtlas;

/**
 * @brief Voxel data for a single cell in the 3D map
 */
struct Voxel3D {
    std::string modelId;            // ID of the tile model to use
    glm::vec3 position;             // World position
    glm::vec3 rotation;             // Euler rotation (degrees)
    glm::vec3 scale = glm::vec3(1.0f);
    glm::vec4 color = glm::vec4(1.0f);  // Tint color

    // Properties
    bool isSolid = true;
    bool isVisible = true;
    bool castsShadow = true;
    bool receivesShadow = true;

    // Lighting
    float ambientOcclusion = 1.0f;  // 0-1, local AO factor
    glm::vec3 emissiveColor = glm::vec3(0.0f);
    float emissiveStrength = 0.0f;

    // Animation
    float animationTime = 0.0f;
    int animationFrame = 0;

    /**
     * @brief Get the model transform matrix
     */
    glm::mat4 GetTransform() const;
};

/**
 * @brief 3D voxel map storage
 */
class Voxel3DMap {
public:
    Voxel3DMap();
    ~Voxel3DMap();

    /**
     * @brief Initialize map with dimensions
     * @param width X dimension
     * @param height Y dimension (floors/levels)
     * @param depth Z dimension
     * @param cellSize World size of each cell
     */
    void Initialize(int width, int height, int depth, float cellSize = 1.0f);

    /**
     * @brief Clear all voxels
     */
    void Clear();

    /**
     * @brief Get voxel at grid position
     * @return Pointer to voxel or nullptr if empty/out of bounds
     */
    Voxel3D* GetVoxel(int x, int y, int z);
    const Voxel3D* GetVoxel(int x, int y, int z) const;

    /**
     * @brief Set voxel at grid position
     * @return true if set successfully
     */
    bool SetVoxel(int x, int y, int z, const Voxel3D& voxel);

    /**
     * @brief Remove voxel at position
     */
    void RemoveVoxel(int x, int y, int z);

    /**
     * @brief Check if position is in bounds
     */
    bool IsInBounds(int x, int y, int z) const;

    /**
     * @brief Convert world position to grid position
     */
    glm::ivec3 WorldToGrid(const glm::vec3& worldPos) const;

    /**
     * @brief Convert grid position to world position (center of cell)
     */
    glm::vec3 GridToWorld(int x, int y, int z) const;

    /**
     * @brief Get voxel at world position
     */
    Voxel3D* GetVoxelAtWorld(const glm::vec3& worldPos);

    // Dimensions
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetDepth() const { return m_depth; }
    float GetCellSize() const { return m_cellSize; }

    /**
     * @brief Get all voxels on a specific floor/Y level
     */
    std::vector<Voxel3D*> GetVoxelsOnFloor(int floorY);

    /**
     * @brief Get voxel count
     */
    size_t GetVoxelCount() const { return m_voxelCount; }

    /**
     * @brief Iterate over all voxels
     */
    template<typename Func>
    void ForEachVoxel(Func&& func) {
        for (auto& [key, voxel] : m_voxels) {
            int x = static_cast<int>((key >> 0) & 0xFFFF);
            int y = static_cast<int>((key >> 16) & 0xFFFF);
            int z = static_cast<int>((key >> 32) & 0xFFFF);
            func(x, y, z, voxel);
        }
    }

    /**
     * @brief Mark a region as dirty (needs re-rendering)
     */
    void MarkDirty(int x, int y, int z, int w = 1, int h = 1, int d = 1);

    /**
     * @brief Check if map needs rebuilding
     */
    bool IsDirty() const { return m_dirty; }

    /**
     * @brief Clear dirty flag
     */
    void ClearDirty() { m_dirty = false; }

private:
    /**
     * @brief Get key for voxel position
     */
    uint64_t GetKey(int x, int y, int z) const {
        return (static_cast<uint64_t>(x) & 0xFFFF) |
               ((static_cast<uint64_t>(y) & 0xFFFF) << 16) |
               ((static_cast<uint64_t>(z) & 0xFFFF) << 32);
    }

    int m_width = 0;
    int m_height = 0;
    int m_depth = 0;
    float m_cellSize = 1.0f;

    std::unordered_map<uint64_t, Voxel3D> m_voxels;
    size_t m_voxelCount = 0;
    bool m_dirty = true;
};

/**
 * @brief Floor visibility settings
 */
struct FloorVisibility {
    int floorY = 0;
    float alpha = 1.0f;         // Transparency (0 = invisible, 1 = opaque)
    bool visible = true;
    bool highlighted = false;   // Draw with highlight effect
};

/**
 * @brief Render configuration for voxel renderer
 */
struct Voxel3DRendererConfig {
    // Rendering options
    bool enableFrustumCulling = true;
    bool enableOcclusionCulling = false;
    bool enableInstancing = true;
    bool enableShadows = true;
    bool enableAmbientOcclusion = true;

    // Floor rendering
    bool enableFloorFading = true;     // Fade floors above player
    float floorFadeDistance = 2.0f;    // Floors above this are faded
    float floorFadeAlpha = 0.3f;       // Alpha for faded floors

    // View distance
    float maxRenderDistance = 100.0f;
    int maxVisibleVoxels = 50000;

    // Quality
    int shadowMapSize = 2048;
    int maxBatchSize = 1000;

    // Debug
    bool showBoundingBoxes = false;
    bool showFloorBoundaries = false;
    bool wireframeMode = false;
};

/**
 * @brief Renders 3D voxel worlds with floor-based visibility
 *
 * Features:
 * - Floor-based rendering with transparency for multi-level buildings
 * - Instanced rendering for performance
 * - Frustum and occlusion culling
 * - Shadow mapping
 * - Ambient occlusion
 */
class Voxel3DRenderer {
public:
    Voxel3DRenderer();
    ~Voxel3DRenderer();

    // Non-copyable
    Voxel3DRenderer(const Voxel3DRenderer&) = delete;
    Voxel3DRenderer& operator=(const Voxel3DRenderer&) = delete;

    /**
     * @brief Initialize the renderer
     * @param renderer Engine renderer
     * @param modelManager Tile model manager
     * @param config Renderer configuration
     * @return true if initialized successfully
     */
    bool Initialize(Nova::Renderer& renderer, TileModelManager& modelManager,
                    const Voxel3DRendererConfig& config = {});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update renderer state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // ========== Main Rendering ==========

    /**
     * @brief Render the entire voxel map
     * @param map The voxel map to render
     * @param camera View camera
     * @param currentFloor The floor the player is currently on
     */
    void Render(const Voxel3DMap& map, const Nova::Camera& camera, int currentFloor);

    /**
     * @brief Render only a specific floor
     * @param map The voxel map
     * @param camera View camera
     * @param floorY Floor level to render
     * @param abovePlayer Whether this floor is above the player
     */
    void RenderFloor(const Voxel3DMap& map, const Nova::Camera& camera, int floorY, bool abovePlayer);

    /**
     * @brief Render shadow pass
     * @param map The voxel map
     * @param lightDirection Direction of directional light
     */
    void RenderShadows(const Voxel3DMap& map, const glm::vec3& lightDirection);

    // ========== Floor Visibility ==========

    /**
     * @brief Set visibility/transparency for a floor
     * @param floor Floor level
     * @param alpha Transparency (0-1)
     */
    void SetFloorVisibility(int floor, float alpha);

    /**
     * @brief Hide all floors above a certain level
     * @param floor Maximum visible floor (inclusive)
     */
    void HideFloorsAbove(int floor);

    /**
     * @brief Show all floors
     */
    void ShowAllFloors();

    /**
     * @brief Get floor visibility settings
     * @param floor Floor level
     */
    FloorVisibility GetFloorVisibility(int floor) const;

    /**
     * @brief Set the current floor (affects automatic visibility)
     * @param floor Current floor level
     */
    void SetCurrentFloor(int floor);

    /**
     * @brief Get current floor
     */
    int GetCurrentFloor() const { return m_currentFloor; }

    // ========== Culling ==========

    /**
     * @brief Check if a voxel is visible (within frustum)
     * @param voxel The voxel to check
     * @param camera View camera
     * @return true if voxel is visible
     */
    bool IsVoxelVisible(const Voxel3D& voxel, const Nova::Camera& camera) const;

    /**
     * @brief Check if a voxel is occluded by other voxels
     * @param map The voxel map
     * @param x, y, z Voxel position
     * @return true if completely occluded (not visible)
     */
    bool IsVoxelOccluded(const Voxel3DMap& map, int x, int y, int z) const;

    // ========== Batch Rendering ==========

    /**
     * @brief Begin collecting voxels for batch rendering
     */
    void BeginBatch();

    /**
     * @brief Add voxel to current batch
     * @param voxel The voxel to add
     */
    void AddToBatch(const Voxel3D& voxel);

    /**
     * @brief End batch and render all collected voxels
     */
    void EndBatch();

    /**
     * @brief Force render current batch
     */
    void FlushBatch();

    // ========== Configuration ==========

    /**
     * @brief Set renderer configuration
     */
    void SetConfig(const Voxel3DRendererConfig& config);

    /**
     * @brief Get current configuration
     */
    const Voxel3DRendererConfig& GetConfig() const { return m_config; }

    /**
     * @brief Set custom shader for voxel rendering
     */
    void SetShader(std::shared_ptr<Nova::Shader> shader);

    /**
     * @brief Set shadow shader
     */
    void SetShadowShader(std::shared_ptr<Nova::Shader> shader);

    // ========== Statistics ==========

    struct RenderStats {
        uint32_t voxelsRendered = 0;
        uint32_t voxelsCulled = 0;
        uint32_t drawCalls = 0;
        uint32_t triangles = 0;
        uint32_t floorsRendered = 0;
        float renderTimeMs = 0.0f;
    };

    /**
     * @brief Get render statistics
     */
    const RenderStats& GetStats() const { return m_stats; }

    /**
     * @brief Reset frame statistics
     */
    void ResetStats();

private:
    /**
     * @brief Create shader resources
     */
    void CreateShaders();

    /**
     * @brief Create shadow map framebuffer
     */
    void CreateShadowMap();

    /**
     * @brief Collect visible voxels for rendering
     */
    void CollectVisibleVoxels(const Voxel3DMap& map, const Nova::Camera& camera);

    /**
     * @brief Sort voxels for optimal rendering
     */
    void SortVoxelsForRendering();

    /**
     * @brief Apply floor visibility settings
     */
    void ApplyFloorVisibility(int floorY, float& outAlpha, bool& outVisible);

    /**
     * @brief Calculate light space matrix for shadows
     */
    glm::mat4 CalculateLightSpaceMatrix(const glm::vec3& lightDir, const glm::vec3& sceneCenter, float sceneRadius);

    Nova::Renderer* m_renderer = nullptr;
    TileModelManager* m_modelManager = nullptr;
    Voxel3DRendererConfig m_config;
    bool m_initialized = false;

    // Shaders
    std::shared_ptr<Nova::Shader> m_voxelShader;
    std::shared_ptr<Nova::Shader> m_shadowShader;
    std::shared_ptr<Nova::Shader> m_transparentShader;

    // Shadow mapping
    std::unique_ptr<Nova::Framebuffer> m_shadowFramebuffer;
    glm::mat4 m_lightSpaceMatrix;

    // Floor visibility
    std::unordered_map<int, FloorVisibility> m_floorVisibility;
    int m_currentFloor = 0;

    // Batch rendering
    struct BatchData {
        std::string modelId;
        std::vector<glm::mat4> transforms;
        std::vector<glm::vec4> colors;
        float alpha = 1.0f;
    };
    std::unordered_map<std::string, BatchData> m_batches;
    bool m_batchActive = false;

    // Visible voxels cache
    struct VisibleVoxel {
        const Voxel3D* voxel;
        int floorY;
        float distanceToCamera;
    };
    std::vector<VisibleVoxel> m_visibleVoxels;

    // Statistics
    RenderStats m_stats;

    // Timing
    float m_totalTime = 0.0f;
};

/**
 * @brief Helper class for building voxel worlds procedurally
 */
class Voxel3DBuilder {
public:
    Voxel3DBuilder(Voxel3DMap& map, TileModelManager& modelManager);

    /**
     * @brief Place a single voxel
     */
    void PlaceVoxel(int x, int y, int z, const std::string& modelId,
                    const glm::vec3& rotation = glm::vec3(0),
                    const glm::vec4& color = glm::vec4(1));

    /**
     * @brief Fill a box region with voxels
     */
    void FillBox(int x1, int y1, int z1, int x2, int y2, int z2,
                 const std::string& modelId,
                 const glm::vec4& color = glm::vec4(1));

    /**
     * @brief Create a hollow box (walls only)
     */
    void HollowBox(int x1, int y1, int z1, int x2, int y2, int z2,
                   const std::string& wallModelId,
                   const std::string& floorModelId = "",
                   const std::string& ceilingModelId = "");

    /**
     * @brief Create a floor plane
     */
    void CreateFloor(int y, int x1, int z1, int x2, int z2,
                     const std::string& modelId,
                     const glm::vec4& color = glm::vec4(1));

    /**
     * @brief Create walls around a perimeter
     */
    void CreateWalls(int y, int height, int x1, int z1, int x2, int z2,
                     const std::string& modelId);

    /**
     * @brief Create a staircase
     */
    void CreateStairs(int startX, int startY, int startZ,
                      int direction, int steps, const std::string& modelId);

    /**
     * @brief Create a ramp
     */
    void CreateRamp(int x1, int y1, int z1, int x2, int y2, int z2,
                    const std::string& modelId);

    /**
     * @brief Create a cylindrical column
     */
    void CreateColumn(int x, int z, int y1, int y2,
                      const std::string& modelId);

    /**
     * @brief Create a hex tile at hex grid coordinates
     */
    void PlaceHexTile(int hexQ, int hexR, int y,
                      const std::string& modelId, float hexRadius = 1.0f);

    /**
     * @brief Fill a hex region
     */
    void FillHexRegion(int centerQ, int centerR, int radius, int y,
                       const std::string& modelId, float hexRadius = 1.0f);

private:
    Voxel3DMap& m_map;
    TileModelManager& m_modelManager;
};

} // namespace Vehement
