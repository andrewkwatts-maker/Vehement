#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace Nova {

// Forward declarations
class Shader;
class SDFModel;

/**
 * @brief Spherical Harmonics coefficient count
 * L0 = 1, L1 = 4, L2 = 9 coefficients
 */
enum class SHOrder {
    L0 = 1,   // Constant (ambient)
    L1 = 4,   // Linear (directional)
    L2 = 9    // Quadratic (full lighting)
};

/**
 * @brief Single light probe with SH coefficients
 */
struct LightProbe {
    glm::vec3 position;
    std::vector<glm::vec3> shCoefficients;  // SH coefficients (RGB per coefficient)
    float radius;                            // Influence radius
    bool baked;                             // Is this probe baked or dynamic?

    LightProbe() : position(0.0f), radius(5.0f), baked(false) {}
};

/**
 * @brief Configuration for light probe system
 */
struct LightProbeConfig {
    // Grid settings
    glm::vec3 gridOrigin{0.0f};
    glm::vec3 gridSize{100.0f, 50.0f, 100.0f};
    glm::vec3 gridSpacing{5.0f};          // Distance between probes

    // Quality
    SHOrder shOrder = SHOrder::L2;
    int raysPerProbe = 256;               // For baking
    int maxBounces = 2;                   // GI bounces

    // Dynamic updates
    bool enableDynamicUpdates = true;
    int probesPerFrame = 8;               // Dynamic probes updated per frame
    float updateRadius = 20.0f;           // Update probes within this radius of dynamic objects

    // Rendering
    bool useGPUBaking = true;             // Use compute shader for baking
    float blendDistance = 2.0f;           // Distance for smooth probe blending
};

/**
 * @brief SDF Light Probe Grid
 *
 * Implements light probe system optimized for SDF rendering:
 * - Spherical Harmonics (L2) for diffuse lighting
 * - GPU-accelerated SDF raytracing for baking
 * - Dynamic probe updates for moving objects
 * - Smooth trilinear interpolation between probes
 * - Integration with clustered lighting
 *
 * Features:
 * - Fast GI for SDF models
 * - Multi-bounce indirect lighting
 * - Supports dynamic scenes
 * - <1ms per frame for updates
 * - Seamless blend with direct lighting
 */
class SDFLightProbeGrid {
public:
    SDFLightProbeGrid();
    ~SDFLightProbeGrid();

    // Non-copyable
    SDFLightProbeGrid(const SDFLightProbeGrid&) = delete;
    SDFLightProbeGrid& operator=(const SDFLightProbeGrid&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize light probe grid
     */
    bool Initialize(const LightProbeConfig& config);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Reconfigure grid
     */
    bool Reconfigure(const LightProbeConfig& config);

    // =========================================================================
    // Baking
    // =========================================================================

    /**
     * @brief Bake all probes from static SDF models
     * @param staticModels List of static SDF models
     * @return True if baking succeeded
     */
    bool BakeProbes(const std::vector<const SDFModel*>& staticModels);

    /**
     * @brief Bake single probe
     */
    bool BakeProbe(int probeIndex, const std::vector<const SDFModel*>& models);

    /**
     * @brief Update dynamic probes near moving objects
     */
    void UpdateDynamicProbes(const std::vector<const SDFModel*>& dynamicModels);

    /**
     * @brief Clear all probe data
     */
    void ClearProbes();

    // =========================================================================
    // Sampling
    // =========================================================================

    /**
     * @brief Sample irradiance at world position
     * @param position World position
     * @param normal Surface normal (for SH evaluation)
     * @return Irradiance (RGB)
     */
    glm::vec3 SampleIrradiance(const glm::vec3& position, const glm::vec3& normal) const;

    /**
     * @brief Get nearest probe index
     */
    int GetNearestProbeIndex(const glm::vec3& position) const;

    /**
     * @brief Get probe at grid coordinates
     */
    LightProbe* GetProbe(const glm::ivec3& gridCoord);
    const LightProbe* GetProbe(const glm::ivec3& gridCoord) const;

    /**
     * @brief Get probe by index
     */
    LightProbe* GetProbeByIndex(int index);
    const LightProbe* GetProbeByIndex(int index) const;

    // =========================================================================
    // GPU Integration
    // =========================================================================

    /**
     * @brief Upload probe data to GPU
     */
    void UploadToGPU();

    /**
     * @brief Bind probe buffer for rendering
     */
    void BindForRendering(uint32_t binding = 4);

    /**
     * @brief Set shader uniforms
     */
    void SetShaderUniforms(Shader& shader);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     */
    const LightProbeConfig& GetConfig() const { return m_config; }

    /**
     * @brief Get grid dimensions
     */
    glm::ivec3 GetGridDimensions() const { return m_gridDim; }

    /**
     * @brief Get total probe count
     */
    int GetProbeCount() const { return static_cast<int>(m_probes.size()); }

    /**
     * @brief Enable/disable debug visualization
     */
    void SetDebugVisualization(bool enabled) { m_debugVisualization = enabled; }

    /**
     * @brief Render debug visualization
     */
    void RenderDebugVisualization();

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        int totalProbes = 0;
        int bakedProbes = 0;
        int dynamicProbes = 0;
        float bakingTimeMs = 0.0f;
        float updateTimeMs = 0.0f;
        int probesUpdatedThisFrame = 0;
    };

    const Stats& GetStats() const { return m_stats; }

private:
    // Build grid structure
    void BuildGrid();

    // World position to grid coordinates
    glm::ivec3 WorldToGrid(const glm::vec3& worldPos) const;

    // Grid coordinates to world position
    glm::vec3 GridToWorld(const glm::ivec3& gridCoord) const;

    // Grid coordinates to linear index
    int GridToIndex(const glm::ivec3& gridCoord) const;

    // Linear index to grid coordinates
    glm::ivec3 IndexToGrid(int index) const;

    // Check if grid coordinates are valid
    bool IsValidGridCoord(const glm::ivec3& gridCoord) const;

    // Trace ray through SDF scene
    bool TraceRay(const glm::vec3& origin, const glm::vec3& direction,
                 const std::vector<const SDFModel*>& models,
                 float& hitDistance, glm::vec3& hitColor) const;

    // Evaluate SH basis
    std::vector<float> EvaluateSHBasis(const glm::vec3& direction) const;

    // Project lighting to SH
    void ProjectToSH(const std::vector<glm::vec3>& samples,
                    const std::vector<glm::vec3>& directions,
                    std::vector<glm::vec3>& outCoefficients) const;

    // Evaluate SH for given normal
    glm::vec3 EvaluateSH(const std::vector<glm::vec3>& coefficients,
                        const glm::vec3& normal) const;

    // Trilinear interpolation
    glm::vec3 TrilinearInterpolate(const glm::vec3& position, const glm::vec3& normal) const;

    bool m_initialized = false;
    LightProbeConfig m_config;

    // Grid structure
    glm::ivec3 m_gridDim;                // Grid dimensions (probe count per axis)
    std::vector<LightProbe> m_probes;    // All probes in grid

    // GPU resources
    uint32_t m_probeSSBO = 0;            // Probe data buffer
    bool m_gpuDataDirty = true;

    // Shaders
    std::unique_ptr<Shader> m_bakingShader;  // Compute shader for GPU baking
    std::unique_ptr<Shader> m_debugShader;   // Debug visualization

    // State
    int m_nextDynamicProbeIndex = 0;     // For round-robin updates

    // Statistics
    Stats m_stats;

    // Debug
    bool m_debugVisualization = false;
};

} // namespace Nova
