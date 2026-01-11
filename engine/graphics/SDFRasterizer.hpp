#pragma once

#include "RenderBackend.hpp"
#include "Framebuffer.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Nova {

// Forward declarations
class Shader;
class Texture;

/**
 * @brief Axis-aligned bounding box for tile culling
 */
struct TileAABB {
    glm::vec3 minWorld{0.0f};
    glm::vec3 maxWorld{0.0f};
    glm::vec2 screenMin{0.0f};
    glm::vec2 screenMax{0.0f};
    std::vector<uint32_t> sdfObjectIndices;
    bool isEmpty = true;
};

/**
 * @brief SDF object data for GPU
 */
struct SDFObjectGPU {
    glm::mat4 transform;
    glm::mat4 inverseTransform;
    glm::vec4 bounds;           // xyz = center, w = radius
    glm::vec4 material;         // xyz = color, w = roughness
    glm::vec4 params;           // x = type, y = blend, z = metallic, w = emission

    // SDF type identifiers
    static constexpr int TYPE_SPHERE = 0;
    static constexpr int TYPE_BOX = 1;
    static constexpr int TYPE_TORUS = 2;
    static constexpr int TYPE_CYLINDER = 3;
    static constexpr int TYPE_CAPSULE = 4;
};

/**
 * @brief Tile data for GPU dispatch
 */
struct TileData {
    glm::ivec2 tileCoord;       // Tile X, Y coordinate
    uint32_t objectCount;        // Number of SDF objects in tile
    uint32_t objectOffset;       // Offset into object index buffer
};

/**
 * @brief SDF-first compute-based rasterizer
 *
 * Uses compute shaders for screen-space tile-based raymarching.
 * No RTX required - works on AMD, Intel, and older NVIDIA cards.
 *
 * Key features:
 * - Tile-based rendering (8x8 or 16x16 tiles)
 * - Per-tile bounding box culling
 * - Early-out optimization for empty tiles
 * - Writes depth buffer for Z-testing with polygons
 * - PBR material evaluation during raymarching
 */
class SDFRasterizer : public IRenderBackend {
public:
    SDFRasterizer();
    ~SDFRasterizer() override;

    // Non-copyable
    SDFRasterizer(const SDFRasterizer&) = delete;
    SDFRasterizer& operator=(const SDFRasterizer&) = delete;

    // IRenderBackend implementation
    bool Initialize(int width, int height) override;
    void Shutdown() override;
    void Resize(int width, int height) override;
    void BeginFrame(const Camera& camera) override;
    void EndFrame() override;
    void Render(const Scene& scene, const Camera& camera) override;
    void SetQualitySettings(const QualitySettings& settings) override;
    const QualitySettings& GetQualitySettings() const override { return m_settings; }
    const RenderStats& GetStats() const override { return m_stats; }
    bool SupportsFeature(RenderFeature feature) const override;
    const char* GetName() const override { return "SDF Rasterizer (Compute)"; }
    std::shared_ptr<Texture> GetOutputColor() const override;
    std::shared_ptr<Texture> GetOutputDepth() const override;
    void SetDebugMode(bool enabled) override { m_debugMode = enabled; }

    /**
     * @brief Add SDF object to scene
     * @param object SDF object data
     * @return Object ID for future reference
     */
    uint32_t AddSDFObject(const SDFObjectGPU& object);

    /**
     * @brief Remove SDF object
     */
    void RemoveSDFObject(uint32_t objectId);

    /**
     * @brief Update SDF object transform
     */
    void UpdateSDFObject(uint32_t objectId, const glm::mat4& transform);

    /**
     * @brief Clear all SDF objects
     */
    void ClearSDFObjects();

    /**
     * @brief Get number of active tiles
     */
    uint32_t GetActiveTileCount() const { return m_activeTileCount; }

    /**
     * @brief Get tile grid dimensions
     */
    glm::ivec2 GetTileGridSize() const { return m_tileGridSize; }

private:
    /**
     * @brief Build tile AABBs and cull SDF objects
     */
    void BuildTileBounds(const Camera& camera);

    /**
     * @brief Upload tile and object data to GPU
     */
    void UploadTileData();

    /**
     * @brief Dispatch compute shader for raymarching
     */
    void DispatchRaymarch();

    /**
     * @brief Render debug visualization
     */
    void RenderDebugVisualization();

    /**
     * @brief Create GPU buffers
     */
    bool CreateBuffers();

    /**
     * @brief Create compute shader programs
     */
    bool CreateShaders();

    /**
     * @brief Compute world-space AABB for screen tile
     */
    TileAABB ComputeTileAABB(int tileX, int tileY, const Camera& camera) const;

    /**
     * @brief Test if SDF object intersects tile AABB
     */
    bool TestSDFIntersectsTile(const SDFObjectGPU& object, const TileAABB& tile) const;

    /**
     * @brief Update performance statistics
     */
    void UpdateStats();

    // Settings and state
    QualitySettings m_settings;
    RenderStats m_stats;
    bool m_debugMode = false;
    bool m_initialized = false;

    // Render targets
    std::unique_ptr<Framebuffer> m_framebuffer;
    std::shared_ptr<Texture> m_colorTexture;
    std::shared_ptr<Texture> m_depthTexture;

    // Compute shaders
    std::shared_ptr<Shader> m_raymarchShader;
    std::shared_ptr<Shader> m_tileCullShader;
    std::shared_ptr<Shader> m_debugShader;

    // SDF scene data
    std::vector<SDFObjectGPU> m_sdfObjects;
    std::unordered_map<uint32_t, size_t> m_objectIdToIndex;
    uint32_t m_nextObjectId = 1;

    // Tile data
    std::vector<TileAABB> m_tileAABBs;
    std::vector<TileData> m_activeTiles;
    std::vector<uint32_t> m_tileObjectIndices;
    glm::ivec2 m_tileGridSize{0, 0};
    uint32_t m_activeTileCount = 0;

    // GPU buffers
    uint32_t m_sdfObjectBuffer = 0;
    uint32_t m_tileDataBuffer = 0;
    uint32_t m_tileObjectIndexBuffer = 0;
    uint32_t m_statsBuffer = 0;

    // Camera data
    glm::mat4 m_viewMatrix{1.0f};
    glm::mat4 m_projMatrix{1.0f};
    glm::mat4 m_viewProjMatrix{1.0f};
    glm::mat4 m_invViewProjMatrix{1.0f};
    glm::vec3 m_cameraPosition{0.0f};
    glm::vec3 m_cameraForward{0.0f, 0.0f, -1.0f};

    // Timing
    uint32_t m_gpuQueryStart = 0;
    uint32_t m_gpuQueryEnd = 0;
    std::chrono::high_resolution_clock::time_point m_frameStartTime;

    // Frame counter for statistics
    uint32_t m_frameCount = 0;
    float m_accumulatedTime = 0.0f;
};

} // namespace Nova
