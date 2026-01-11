#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include "../math/Vector3.hpp"
#include "../math/Vector4.hpp"
#include "../math/Matrix4.hpp"
#include "GPUDrivenRenderer.hpp"

namespace Engine {
namespace Graphics {

/**
 * @brief Light types supported by the system
 */
enum class LightType : uint32_t {
    Point = 0,
    Spot = 1,
    Directional = 2,
    Area = 3,
    EmissiveMesh = 4
};

/**
 * @brief GPU light structure (aligned for GPU)
 */
struct alignas(16) GPULight {
    Vector4 position;           // xyz = position, w = range
    Vector4 direction;          // xyz = direction, w = spotAngle
    Vector4 color;              // rgb = color, a = intensity
    Vector4 attenuation;        // x = constant, y = linear, z = quadratic, w = type
    Vector4 extra;              // Area light params or emissive mesh ID

    GPULight()
        : position(0, 0, 0, 100.0f)
        , direction(0, -1, 0, 0.0f)
        , color(1, 1, 1, 1.0f)
        , attenuation(1.0f, 0.09f, 0.032f, 0.0f)
        , extra(0, 0, 0, 0) {}
};

/**
 * @brief CPU-side light representation
 */
struct Light {
    LightType type;
    Vector3 position;
    Vector3 direction;
    Vector3 color;
    float intensity;
    float range;
    float spotAngle;
    float innerSpotAngle;

    // Area light properties
    Vector3 areaSize;

    // Attenuation
    float constantAttenuation;
    float linearAttenuation;
    float quadraticAttenuation;

    // Shadow properties
    bool castsShadows;
    uint32_t shadowMapIndex;

    // Emissive mesh
    uint32_t meshID;

    Light()
        : type(LightType::Point)
        , position(0, 0, 0)
        , direction(0, -1, 0)
        , color(1, 1, 1)
        , intensity(1.0f)
        , range(100.0f)
        , spotAngle(45.0f)
        , innerSpotAngle(30.0f)
        , areaSize(1, 1, 0)
        , constantAttenuation(1.0f)
        , linearAttenuation(0.09f)
        , quadraticAttenuation(0.032f)
        , castsShadows(false)
        , shadowMapIndex(0)
        , meshID(0) {}

    GPULight ToGPULight() const;
};

/**
 * @brief Light cluster for spatial partitioning
 */
struct LightCluster {
    uint32_t lightCount;
    uint32_t lightIndices[256];  // First 256 lights inline
    uint32_t overflowHead;        // Linked list for overflow
    uint32_t padding;

    LightCluster() : lightCount(0), overflowHead(0), padding(0) {
        for (int i = 0; i < 256; i++) {
            lightIndices[i] = 0;
        }
    }
};

/**
 * @brief Light overflow node for linked list
 */
struct LightOverflowNode {
    uint32_t lightIndex;
    uint32_t next;  // 0 = end of list

    LightOverflowNode() : lightIndex(0), next(0) {}
};

/**
 * @brief Cluster bounds (AABB)
 */
struct ClusterAABB {
    Vector3 min;
    Vector3 max;

    ClusterAABB() : min(0, 0, 0), max(0, 0, 0) {}

    bool Intersects(const Light& light) const;
};

/**
 * @brief Shadow map atlas for 256+ shadow maps
 */
class ShadowMapAtlas {
public:
    ShadowMapAtlas(uint32_t size = 16384);
    ~ShadowMapAtlas();

    /**
     * @brief Allocate shadow map slot
     * @return Shadow map index, or -1 if full
     */
    int32_t AllocateSlot();

    /**
     * @brief Free shadow map slot
     */
    void FreeSlot(uint32_t index);

    /**
     * @brief Get texture handle
     */
    uint32_t GetTexture() const { return m_texture; }

    /**
     * @brief Get slot viewport
     */
    struct SlotViewport {
        uint32_t x, y, width, height;
    };
    SlotViewport GetSlotViewport(uint32_t index) const;

    /**
     * @brief Get max slots
     */
    uint32_t GetMaxSlots() const { return m_maxSlots; }

private:
    uint32_t m_texture;
    uint32_t m_fbo;
    uint32_t m_size;
    uint32_t m_slotSize;
    uint32_t m_maxSlots;
    std::vector<bool> m_allocatedSlots;
};

/**
 * @brief Clustered lighting system with support for 100,000+ lights
 * Uses expanded cluster grid and overflow handling
 */
class ClusteredLightingExpanded {
public:
    /**
     * @brief Configuration for clustered lighting
     */
    struct Config {
        uint32_t clusterGridX;      // Clusters in X (default: 32)
        uint32_t clusterGridY;      // Clusters in Y (default: 18)
        uint32_t clusterGridZ;      // Clusters in Z (default: 48)
        uint32_t maxLights;          // Maximum lights (default: 131072)
        uint32_t maxLightsPerCluster; // Inline capacity (default: 256)
        uint32_t overflowPoolSize;   // Overflow node pool (default: 1048576)
        bool enableShadows;          // Enable shadow mapping
        uint32_t shadowAtlasSize;    // Shadow atlas resolution
        uint32_t maxShadowCasters;   // Max shadow casting lights

        Config()
            : clusterGridX(32)
            , clusterGridY(18)
            , clusterGridZ(48)
            , maxLights(131072)
            , maxLightsPerCluster(256)
            , overflowPoolSize(1048576)
            , enableShadows(true)
            , shadowAtlasSize(16384)
            , maxShadowCasters(256) {}
    };

    explicit ClusteredLightingExpanded(const Config& config = Config());
    ~ClusteredLightingExpanded();

    /**
     * @brief Initialize system resources
     */
    bool Initialize();

    /**
     * @brief Add light to system
     * @return Light index, or -1 if full
     */
    int32_t AddLight(const Light& light);

    /**
     * @brief Update light properties
     */
    void UpdateLight(uint32_t index, const Light& light);

    /**
     * @brief Remove light
     */
    void RemoveLight(uint32_t index);

    /**
     * @brief Clear all lights
     */
    void ClearLights();

    /**
     * @brief Update cluster assignment (call before rendering)
     * Runs GPU compute shader to assign lights to clusters
     */
    void UpdateClusters(const Matrix4& viewMatrix, const Matrix4& projMatrix);

    /**
     * @brief Get total cluster count
     */
    uint32_t GetClusterCount() const {
        return m_config.clusterGridX * m_config.clusterGridY * m_config.clusterGridZ;
    }

    /**
     * @brief Get cluster index from 3D position
     */
    uint32_t GetClusterIndex(const Vector3& worldPos) const;

    /**
     * @brief Get cluster AABB
     */
    ClusterAABB GetClusterAABB(uint32_t clusterIndex) const;

    /**
     * @brief Get cluster AABB from 3D grid position
     */
    ClusterAABB GetClusterAABBFromGrid(uint32_t x, uint32_t y, uint32_t z) const;

    /**
     * @brief Bind lighting buffers for rendering
     */
    void BindLightingBuffers();

    /**
     * @brief Get light buffer
     */
    GPUBuffer* GetLightBuffer() { return m_lightBuffer.get(); }

    /**
     * @brief Get cluster buffer
     */
    GPUBuffer* GetClusterBuffer() { return m_clusterBuffer.get(); }

    /**
     * @brief Get overflow buffer
     */
    GPUBuffer* GetOverflowBuffer() { return m_overflowBuffer.get(); }

    /**
     * @brief Get shadow atlas
     */
    ShadowMapAtlas* GetShadowAtlas() { return m_shadowAtlas.get(); }

    /**
     * @brief Performance statistics
     */
    struct Stats {
        uint32_t totalLights;
        uint32_t activeLights;
        uint32_t shadowCastingLights;
        uint32_t clustersWithOverflow;
        uint32_t maxLightsInCluster;
        float clusterUpdateTimeMs;
        float lightUploadTimeMs;

        Stats()
            : totalLights(0), activeLights(0), shadowCastingLights(0)
            , clustersWithOverflow(0), maxLightsInCluster(0)
            , clusterUpdateTimeMs(0), lightUploadTimeMs(0) {}
    };

    Stats GetStats() const { return m_stats; }
    void ResetStats();

private:
    /**
     * @brief Create GPU buffers
     */
    bool CreateBuffers();

    /**
     * @brief Load compute shaders
     */
    bool LoadShaders();

    /**
     * @brief Reset cluster data
     */
    void ResetClusters();

    /**
     * @brief CPU-side cluster assignment (fallback)
     */
    void UpdateClustersCPU(const Matrix4& viewMatrix, const Matrix4& projMatrix);

    /**
     * @brief Calculate cluster bounds in view space
     */
    ClusterAABB CalculateClusterBounds(uint32_t x, uint32_t y, uint32_t z,
                                       const Matrix4& viewMatrix,
                                       const Matrix4& projMatrix) const;

    Config m_config;

    // Light data
    std::vector<Light> m_lights;
    std::vector<GPULight> m_gpuLights;
    std::vector<uint32_t> m_freeLightIndices;

    // Cluster data
    std::vector<LightCluster> m_clusters;
    std::vector<LightOverflowNode> m_overflowPool;
    uint32_t m_overflowCounter;

    // GPU Buffers
    std::unique_ptr<GPUBuffer> m_lightBuffer;
    std::unique_ptr<GPUBuffer> m_clusterBuffer;
    std::unique_ptr<GPUBuffer> m_overflowBuffer;
    std::unique_ptr<GPUBuffer> m_clusterBoundsBuffer;

    // Compute shaders
    std::unique_ptr<ComputeShader> m_clusterAssignShader;
    std::unique_ptr<ComputeShader> m_clusterResetShader;

    // Shadow mapping
    std::unique_ptr<ShadowMapAtlas> m_shadowAtlas;

    // Performance tracking
    Stats m_stats;
    uint32_t m_queryObject;

    // Camera frustum info
    float m_nearPlane;
    float m_farPlane;
    float m_fov;
};

/**
 * @brief Light importance sampler
 * Prioritizes lights for shadow mapping and detailed calculations
 */
class LightImportanceSampler {
public:
    /**
     * @brief Calculate light importance
     * Higher values = more important
     */
    static float CalculateImportance(const Light& light, const Vector3& viewPos);

    /**
     * @brief Sort lights by importance
     */
    static void SortByImportance(std::vector<Light>& lights, const Vector3& viewPos);

    /**
     * @brief Select top N most important lights
     */
    static std::vector<uint32_t> SelectTopLights(
        const std::vector<Light>& lights,
        const Vector3& viewPos,
        uint32_t count
    );
};

} // namespace Graphics
} // namespace Engine
