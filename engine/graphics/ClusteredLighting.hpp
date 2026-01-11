#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace Nova {

// Forward declarations
class Shader;
class Camera;

/**
 * @brief Light types supported by clustered lighting
 */
enum class LightType {
    Point = 0,
    Spot = 1,
    Directional = 2
};

/**
 * @brief GPU-aligned light data structure
 */
struct alignas(16) GPULight {
    glm::vec4 position;        // xyz = position, w = range
    glm::vec4 direction;       // xyz = direction, w = inner cone angle
    glm::vec4 color;           // rgb = color, a = intensity
    glm::vec4 params;          // x = outer cone angle, y = type, z = enabled, w = padding
};

/**
 * @brief Cluster data structure (CPU side)
 */
struct Cluster {
    glm::vec3 minAABB;
    glm::vec3 maxAABB;
    uint32_t lightCount;
    std::vector<uint32_t> lightIndices;
};

/**
 * @brief Clustered Forward Rendering
 *
 * Implements clustered lighting for efficient rendering of thousands of lights.
 * Divides view frustum into 3D grid of clusters, culls lights per cluster,
 * and allows fragment shader to only evaluate visible lights.
 *
 * Features:
 * - Supports 10,000+ lights with minimal overhead
 * - Compute shader-based light culling
 * - Point, spot, and directional light support
 * - Configurable cluster grid dimensions
 * - Sub-3ms performance for 1000 lights at 1080p
 */
class ClusteredLightManager {
public:
    ClusteredLightManager();
    ~ClusteredLightManager();

    // Non-copyable
    ClusteredLightManager(const ClusteredLightManager&) = delete;
    ClusteredLightManager& operator=(const ClusteredLightManager&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize clustered lighting system
     * @param width Viewport width
     * @param height Viewport height
     * @param gridX Number of clusters in X (default: 16)
     * @param gridY Number of clusters in Y (default: 9)
     * @param gridZ Number of clusters in Z/depth (default: 24)
     */
    bool Initialize(int width, int height, int gridX = 16, int gridY = 9, int gridZ = 24);

    /**
     * @brief Shutdown and cleanup resources
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Resize for new viewport dimensions
     */
    void Resize(int width, int height);

    // =========================================================================
    // Light Management
    // =========================================================================

    /**
     * @brief Clear all lights
     */
    void ClearLights();

    /**
     * @brief Add point light
     * @return Light index
     */
    uint32_t AddPointLight(const glm::vec3& position, const glm::vec3& color,
                          float intensity, float range);

    /**
     * @brief Add spot light
     * @return Light index
     */
    uint32_t AddSpotLight(const glm::vec3& position, const glm::vec3& direction,
                         const glm::vec3& color, float intensity, float range,
                         float innerAngle, float outerAngle);

    /**
     * @brief Add directional light
     * @return Light index
     */
    uint32_t AddDirectionalLight(const glm::vec3& direction, const glm::vec3& color,
                                 float intensity);

    /**
     * @brief Update existing light
     */
    void UpdateLight(uint32_t index, const GPULight& light);

    /**
     * @brief Remove light
     */
    void RemoveLight(uint32_t index);

    /**
     * @brief Get light count
     */
    uint32_t GetLightCount() const { return static_cast<uint32_t>(m_lights.size()); }

    /**
     * @brief Get light at index
     */
    GPULight& GetLight(uint32_t index) { return m_lights[index]; }
    const GPULight& GetLight(uint32_t index) const { return m_lights[index]; }

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Update cluster grid and cull lights
     * @param camera Camera for view frustum calculation
     */
    void UpdateClusters(const Camera& camera);

    /**
     * @brief Bind cluster and light buffers for rendering
     * @param clusterBinding SSBO binding point for clusters (default: 1)
     * @param lightBinding SSBO binding point for lights (default: 2)
     * @param lightIndexBinding SSBO binding point for light indices (default: 3)
     */
    void BindForRendering(uint32_t clusterBinding = 1, uint32_t lightBinding = 2,
                         uint32_t lightIndexBinding = 3);

    /**
     * @brief Set shader uniforms for clustered lighting
     */
    void SetShaderUniforms(Shader& shader);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get cluster grid dimensions
     */
    glm::ivec3 GetGridDimensions() const { return m_gridDim; }

    /**
     * @brief Get viewport dimensions
     */
    glm::ivec2 GetViewportSize() const { return glm::ivec2(m_viewportWidth, m_viewportHeight); }

    /**
     * @brief Get near and far plane distances
     */
    float GetNearPlane() const { return m_nearPlane; }
    float GetFarPlane() const { return m_farPlane; }

    /**
     * @brief Set near and far plane distances
     */
    void SetDepthRange(float nearPlane, float farPlane);

    /**
     * @brief Enable/disable debug visualization
     */
    void SetDebugVisualization(bool enabled) { m_debugVisualization = enabled; }
    bool IsDebugVisualizationEnabled() const { return m_debugVisualization; }

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        uint32_t totalLights = 0;
        uint32_t totalClusters = 0;
        uint32_t activeClusters = 0;    // Clusters with at least one light
        uint32_t avgLightsPerCluster = 0;
        uint32_t maxLightsPerCluster = 0;
        float cullingTimeMs = 0.0f;
    };

    const Stats& GetStats() const { return m_stats; }

private:
    // Build cluster grid in view space
    void BuildClusterGrid(const Camera& camera);

    // Calculate cluster AABB in view space
    void CalculateClusterAABB(int x, int y, int z, glm::vec3& minAABB, glm::vec3& maxAABB);

    // Upload lights to GPU
    void UploadLights();

    // Perform light culling using compute shader
    void CullLights(const Camera& camera);

    // Transform AABB to world space
    void TransformAABB(const glm::mat4& transform, const glm::vec3& minAABB,
                      const glm::vec3& maxAABB, glm::vec3& outMin, glm::vec3& outMax);

    // Check sphere-AABB intersection
    bool SphereAABBIntersect(const glm::vec3& center, float radius,
                            const glm::vec3& aabbMin, const glm::vec3& aabbMax);

    bool m_initialized = false;

    // Grid configuration
    glm::ivec3 m_gridDim;           // Cluster grid dimensions (x, y, z)
    int m_viewportWidth = 0;
    int m_viewportHeight = 0;
    float m_nearPlane = 0.1f;
    float m_farPlane = 100.0f;

    // Lights
    std::vector<GPULight> m_lights;
    std::vector<uint32_t> m_freeLightIndices;

    // Clusters (CPU side for debugging)
    std::vector<Cluster> m_clusters;

    // GPU buffers
    uint32_t m_clusterSSBO = 0;         // Cluster data (light count + offset)
    uint32_t m_lightSSBO = 0;           // Light data
    uint32_t m_lightIndexSSBO = 0;      // Compact light index list
    uint32_t m_atomicCounterBuffer = 0; // For atomic operations in compute shader

    // Compute shader for light culling
    std::unique_ptr<Shader> m_cullingShader;

    // Statistics
    Stats m_stats;

    // Debug
    bool m_debugVisualization = false;

    // Maximum lights per cluster (configurable)
    static constexpr uint32_t MAX_LIGHTS_PER_CLUSTER = 256;
    static constexpr uint32_t MAX_TOTAL_LIGHT_INDICES = 1024 * 1024; // 1M indices
};

} // namespace Nova
