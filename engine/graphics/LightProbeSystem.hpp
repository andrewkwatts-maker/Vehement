#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <memory>
#include <vector>
#include <array>
#include <string>
#include <functional>

namespace Nova {

// Forward declarations
class Shader;
class Texture;
class Framebuffer;
class Renderer;
class RadianceCascade;
class Camera;

// ============================================================================
// Spherical Harmonics Types
// ============================================================================

/**
 * @brief Spherical harmonics order for light probe encoding
 *
 * L0 (1 coeff)  - Constant ambient light
 * L1 (4 coeffs) - Linear directional variation
 * L2 (9 coeffs) - Quadratic detail (recommended for diffuse GI)
 * L3 (16 coeffs) - High-frequency detail
 */
enum class SHOrder : int {
    L0 = 1,
    L1 = 4,
    L2 = 9,
    L3 = 16
};

/**
 * @brief Get number of SH coefficients for given order
 */
constexpr int GetSHCoeffCount(SHOrder order) {
    return static_cast<int>(order);
}

/**
 * @brief Spherical Harmonics coefficients (RGB per band)
 *
 * Storage layout for L2:
 * [0]    - L0 (constant)
 * [1-3]  - L1 (linear: x, y, z)
 * [4-8]  - L2 (quadratic: xy, yz, z^2, xz, x^2-y^2)
 */
struct SHCoefficients {
    std::array<glm::vec3, 16> coeffs;  // Max L3, typically use L2 (9)
    int order = 9;  // L2 by default

    SHCoefficients() {
        for (auto& c : coeffs) {
            c = glm::vec3(0.0f);
        }
    }

    void Clear() {
        for (auto& c : coeffs) {
            c = glm::vec3(0.0f);
        }
    }

    SHCoefficients operator+(const SHCoefficients& other) const {
        SHCoefficients result;
        result.order = order;
        for (int i = 0; i < order; ++i) {
            result.coeffs[i] = coeffs[i] + other.coeffs[i];
        }
        return result;
    }

    SHCoefficients operator*(float scalar) const {
        SHCoefficients result;
        result.order = order;
        for (int i = 0; i < order; ++i) {
            result.coeffs[i] = coeffs[i] * scalar;
        }
        return result;
    }

    SHCoefficients& operator+=(const SHCoefficients& other) {
        for (int i = 0; i < order; ++i) {
            coeffs[i] += other.coeffs[i];
        }
        return *this;
    }

    SHCoefficients& operator*=(float scalar) {
        for (int i = 0; i < order; ++i) {
            coeffs[i] *= scalar;
        }
        return *this;
    }
};

// ============================================================================
// Light Probe Structure
// ============================================================================

/**
 * @brief Single light probe with position, SH coefficients, and metadata
 */
struct LightProbe {
    glm::vec3 position{0.0f};               // World position
    SHCoefficients irradiance;               // SH-encoded diffuse irradiance

    // Validity and state
    float validity = 0.0f;                   // 0 = invalid, 1 = fully valid
    float updatePriority = 0.0f;             // Priority for update scheduling
    bool needsUpdate = true;                 // Flag for update queue
    bool isOccluded = false;                 // Inside geometry (invalid position)

    // Visibility information for occlusion handling
    std::array<float, 8> cornerVisibility;   // Visibility to 8 corners for interpolation

    // Temporal data
    SHCoefficients previousIrradiance;       // For temporal blending
    int framesSinceUpdate = 0;

    LightProbe() {
        cornerVisibility.fill(1.0f);
    }

    void Reset() {
        irradiance.Clear();
        previousIrradiance.Clear();
        validity = 0.0f;
        updatePriority = 0.0f;
        needsUpdate = true;
        isOccluded = false;
        framesSinceUpdate = 0;
        cornerVisibility.fill(1.0f);
    }
};

/**
 * @brief GPU-aligned light probe data for shader consumption
 */
struct alignas(16) GPULightProbe {
    glm::vec4 positionAndValidity;           // xyz = position, w = validity
    glm::vec4 sh0;                           // L0 constant (RGB) + padding
    glm::vec4 sh1_r;                         // L1 red channel + L2 first
    glm::vec4 sh1_g;                         // L1 green channel + L2 first
    glm::vec4 sh1_b;                         // L1 blue channel + L2 first
    glm::vec4 sh2_rg;                        // L2 remaining red/green
    glm::vec4 sh2_b_occlusion;               // L2 remaining blue + occlusion data
};

// ============================================================================
// Probe Grid Configuration
// ============================================================================

/**
 * @brief Axis-aligned bounding box for probe placement
 */
struct AABB {
    glm::vec3 min{-50.0f};
    glm::vec3 max{50.0f};

    glm::vec3 Center() const { return (min + max) * 0.5f; }
    glm::vec3 Size() const { return max - min; }
    glm::vec3 Extents() const { return Size() * 0.5f; }

    bool Contains(const glm::vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }
};

/**
 * @brief Configuration for automatic probe grid placement
 */
struct ProbeGridConfig {
    // Spatial bounds
    AABB bounds;
    glm::vec3 spacing{2.0f};                 // Distance between probes

    // Quality settings
    SHOrder shOrder = SHOrder::L2;
    int raysPerProbe = 256;                  // Rays for raytraced updates
    int maxBounces = 2;                      // GI bounce count

    // Update settings
    int maxProbesPerFrame = 64;              // Budget for real-time updates
    float temporalBlend = 0.9f;              // Blend factor with history (0=no history, 1=all history)
    float updateRadius = 30.0f;              // Update probes within this radius of camera
    float priorityDecay = 0.1f;              // Priority reduction per frame

    // Visibility settings
    float visibilityBias = 0.05f;            // Offset for visibility rays
    float normalBias = 0.1f;                 // Normal offset for sampling
    bool enableOcclusionCulling = true;      // Cull probes inside geometry

    // Hybrid GI settings
    bool enableRadianceCascadeBlend = true;  // Blend with RadianceCascade
    float cascadeBlendDistance = 20.0f;      // Distance for cascade blending
    float cascadeBlendFalloff = 5.0f;        // Falloff for smooth transition
};

// ============================================================================
// Light Probe System
// ============================================================================

/**
 * @brief RTGI Light Probe System for Real-Time Global Illumination
 *
 * Implements a complete light probe system for diffuse GI:
 * - Spherical harmonics (L2) encoding for efficient diffuse lighting
 * - Automatic and manual probe placement
 * - Real-time probe updates using raytraced samples
 * - Trilinear interpolation with visibility-aware blending
 * - Integration with RadianceCascade for hybrid GI
 * - Compute shader-based parallel probe updates
 *
 * Performance Targets:
 * - Probe update: <0.5ms for 64 probes/frame at 256 rays/probe
 * - Sampling: <0.1ms for GI lookup in deferred shading pass
 * - Memory: ~256 bytes per probe (L2 SH + metadata)
 *
 * Usage:
 * @code
 * LightProbeSystem probeSystem;
 * probeSystem.Initialize();
 *
 * // Place probes automatically
 * probeSystem.PlaceProbes(bounds, glm::vec3(3.0f));
 *
 * // Each frame
 * probeSystem.UpdateProbes(camera, deltaTime);
 *
 * // Sample GI in shader or CPU
 * glm::vec3 gi = probeSystem.SampleGI(surfacePos, surfaceNormal);
 * @endcode
 */
class LightProbeSystem {
public:
    LightProbeSystem();
    ~LightProbeSystem();

    // Non-copyable, moveable
    LightProbeSystem(const LightProbeSystem&) = delete;
    LightProbeSystem& operator=(const LightProbeSystem&) = delete;
    LightProbeSystem(LightProbeSystem&&) noexcept;
    LightProbeSystem& operator=(LightProbeSystem&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the light probe system
     * @param config Configuration for probe placement and updates
     * @return true if initialization succeeded
     */
    bool Initialize(const ProbeGridConfig& config = {});

    /**
     * @brief Shutdown and release all resources
     */
    void Shutdown();

    /**
     * @brief Check if system is initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Reinitialize with new configuration
     */
    bool Reinitialize(const ProbeGridConfig& config);

    // =========================================================================
    // Probe Placement
    // =========================================================================

    /**
     * @brief Place probes automatically in a regular grid
     * @param bounds Axis-aligned bounding box for probe placement
     * @param spacing Distance between probes (uniform or per-axis)
     * @return Number of probes placed
     */
    int PlaceProbes(const AABB& bounds, const glm::vec3& spacing);
    int PlaceProbes(const AABB& bounds, float uniformSpacing);

    /**
     * @brief Place a single probe manually
     * @param position World position for the probe
     * @return Index of the placed probe, or -1 if placement failed
     */
    int PlaceProbeManual(const glm::vec3& position);

    /**
     * @brief Remove probe at index
     */
    void RemoveProbe(int index);

    /**
     * @brief Remove all probes
     */
    void ClearProbes();

    /**
     * @brief Optimize probe placement based on scene geometry
     * Removes occluded probes and adjusts positions to valid locations
     * @param raycastFunc Function to test ray-scene intersection
     */
    using RaycastFunc = std::function<bool(const glm::vec3& origin, const glm::vec3& direction, float maxDist, glm::vec3& hitPos, glm::vec3& hitNormal)>;
    void OptimizeProbes(RaycastFunc raycastFunc);

    // =========================================================================
    // Probe Updates
    // =========================================================================

    /**
     * @brief Update probes for the current frame
     * Prioritizes probes near camera and recently invalidated probes
     * @param camera Current camera for priority calculation
     * @param deltaTime Frame delta time for temporal blending
     */
    void UpdateProbes(const Camera& camera, float deltaTime);

    /**
     * @brief Update probes using compute shader (GPU-accelerated)
     * @param camera Current camera
     * @param deltaTime Frame delta time
     */
    void UpdateProbesGPU(const Camera& camera, float deltaTime);

    /**
     * @brief Force update of all probes (for baking)
     * @param progressCallback Optional callback for progress reporting
     */
    void BakeAllProbes(std::function<void(float progress)> progressCallback = nullptr);

    /**
     * @brief Invalidate probes in a region (for dynamic scene changes)
     * @param bounds Region to invalidate
     */
    void InvalidateRegion(const AABB& bounds);

    /**
     * @brief Invalidate single probe
     */
    void InvalidateProbe(int index);

    // =========================================================================
    // GI Sampling
    // =========================================================================

    /**
     * @brief Sample global illumination at a world position
     * Uses trilinear interpolation with visibility weighting
     * @param position World position to sample
     * @param normal Surface normal for SH evaluation
     * @return Irradiance (RGB) from diffuse GI
     */
    [[nodiscard]] glm::vec3 SampleGI(const glm::vec3& position, const glm::vec3& normal) const;

    /**
     * @brief Sample GI with explicit blend weights output
     * @param position World position
     * @param normal Surface normal
     * @param outProbeIndices Indices of contributing probes (8 max)
     * @param outWeights Blend weights for each probe
     * @return Irradiance (RGB)
     */
    [[nodiscard]] glm::vec3 SampleGIDetailed(const glm::vec3& position, const glm::vec3& normal,
                                              std::array<int, 8>& outProbeIndices,
                                              std::array<float, 8>& outWeights) const;

    /**
     * @brief Evaluate SH irradiance for a given normal direction
     * @param sh Spherical harmonics coefficients
     * @param normal Direction to evaluate
     * @return Irradiance (RGB)
     */
    [[nodiscard]] static glm::vec3 EvaluateSH(const SHCoefficients& sh, const glm::vec3& normal);

    /**
     * @brief Project radiance samples to SH coefficients
     * @param samples Radiance samples (RGB)
     * @param directions Sample directions (unit vectors)
     * @param numSamples Number of samples
     * @param outSH Output SH coefficients
     */
    static void ProjectToSH(const glm::vec3* samples, const glm::vec3* directions,
                           int numSamples, SHCoefficients& outSH);

    // =========================================================================
    // RadianceCascade Integration
    // =========================================================================

    /**
     * @brief Set RadianceCascade for hybrid GI blending
     * @param cascade Shared pointer to RadianceCascade system
     */
    void SetRadianceCascade(std::shared_ptr<RadianceCascade> cascade);

    /**
     * @brief Sample hybrid GI (probes + cascade blend)
     * @param position World position
     * @param normal Surface normal
     * @param distanceFromCamera Distance for blend factor calculation
     * @return Blended irradiance from probes and cascade
     */
    [[nodiscard]] glm::vec3 SampleHybridGI(const glm::vec3& position, const glm::vec3& normal,
                                           float distanceFromCamera) const;

    // =========================================================================
    // GPU Integration
    // =========================================================================

    /**
     * @brief Upload probe data to GPU buffer
     */
    void UploadToGPU();

    /**
     * @brief Bind probe buffer for shader access
     * @param binding SSBO binding point
     */
    void BindForRendering(uint32_t binding = 5);

    /**
     * @brief Unbind probe buffer
     */
    void UnbindFromRendering();

    /**
     * @brief Set shader uniforms for probe sampling
     */
    void SetShaderUniforms(Shader& shader);

    /**
     * @brief Get GPU probe buffer handle
     */
    [[nodiscard]] uint32_t GetProbeBuffer() const { return m_probeSSBO; }

    /**
     * @brief Get grid info texture (for shader grid lookup)
     */
    [[nodiscard]] uint32_t GetGridInfoTexture() const { return m_gridInfoTexture; }

    // =========================================================================
    // Accessors
    // =========================================================================

    /**
     * @brief Get probe by index
     */
    [[nodiscard]] LightProbe* GetProbe(int index);
    [[nodiscard]] const LightProbe* GetProbe(int index) const;

    /**
     * @brief Get probe at grid coordinates
     */
    [[nodiscard]] LightProbe* GetProbeAtGrid(const glm::ivec3& gridCoord);
    [[nodiscard]] const LightProbe* GetProbeAtGrid(const glm::ivec3& gridCoord) const;

    /**
     * @brief Get nearest probe to position
     */
    [[nodiscard]] int GetNearestProbeIndex(const glm::vec3& position) const;

    /**
     * @brief Get all probes
     */
    [[nodiscard]] const std::vector<LightProbe>& GetProbes() const { return m_probes; }

    /**
     * @brief Get probe count
     */
    [[nodiscard]] int GetProbeCount() const { return static_cast<int>(m_probes.size()); }

    /**
     * @brief Get grid dimensions
     */
    [[nodiscard]] const glm::ivec3& GetGridDimensions() const { return m_gridDimensions; }

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const ProbeGridConfig& GetConfig() const { return m_config; }

    /**
     * @brief Update configuration (some changes require Reinitialize)
     */
    void SetConfig(const ProbeGridConfig& config);

    /**
     * @brief Get bounds of probe grid
     */
    [[nodiscard]] const AABB& GetBounds() const { return m_config.bounds; }

    // =========================================================================
    // Debug Visualization
    // =========================================================================

    /**
     * @brief Debug visualization mode
     */
    enum class DebugView {
        None,                   // No visualization
        ProbePositions,         // Show probe spheres
        ProbeValidity,          // Color by validity
        SHBands,               // Visualize SH band contributions
        Interpolation,          // Show interpolation weights
        OccludedProbes,        // Highlight occluded probes
        UpdatePriority          // Color by update priority
    };

    /**
     * @brief Enable debug visualization
     */
    void SetDebugView(DebugView view) { m_debugView = view; }
    [[nodiscard]] DebugView GetDebugView() const { return m_debugView; }

    /**
     * @brief Render debug visualization
     * @param renderer Renderer for debug drawing
     */
    void RenderDebugVisualization(Renderer* renderer);

    /**
     * @brief Render debug SH spheres at probe positions
     * @param renderer Renderer for debug drawing
     * @param probeIndex Specific probe to visualize (-1 for all)
     */
    void RenderDebugSHSpheres(Renderer* renderer, int probeIndex = -1);

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        int totalProbes = 0;
        int validProbes = 0;
        int occludedProbes = 0;
        int probesUpdatedThisFrame = 0;
        int probesPendingUpdate = 0;

        float updateTimeMs = 0.0f;
        float uploadTimeMs = 0.0f;
        float sampleTimeMs = 0.0f;

        size_t gpuMemoryBytes = 0;
    };

    [[nodiscard]] const Stats& GetStats() const { return m_stats; }
    void ResetStats();

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    // Grid operations
    void BuildGrid();
    [[nodiscard]] glm::ivec3 WorldToGrid(const glm::vec3& worldPos) const;
    [[nodiscard]] glm::vec3 GridToWorld(const glm::ivec3& gridCoord) const;
    [[nodiscard]] int GridToIndex(const glm::ivec3& gridCoord) const;
    [[nodiscard]] glm::ivec3 IndexToGrid(int index) const;
    [[nodiscard]] bool IsValidGridCoord(const glm::ivec3& gridCoord) const;

    // Probe update
    void UpdateSingleProbe(int probeIndex);
    void CalculateUpdatePriorities(const glm::vec3& cameraPos);
    void SortProbesByPriority(std::vector<int>& probeIndices);

    // SH operations
    [[nodiscard]] std::array<float, 16> EvaluateSHBasis(const glm::vec3& direction) const;
    void GenerateSampleDirections(std::vector<glm::vec3>& directions, int numSamples) const;

    // Interpolation
    struct InterpolationData {
        std::array<int, 8> probeIndices;
        std::array<float, 8> weights;
        glm::vec3 cellMin;
        glm::vec3 cellMax;
    };
    [[nodiscard]] InterpolationData GetInterpolationData(const glm::vec3& position) const;
    [[nodiscard]] float CalculateVisibilityWeight(int probeIndex, const glm::vec3& samplePos) const;

    // GPU resources
    bool InitializeShaders();
    bool InitializeBuffers();
    void CleanupResources();
    void ConvertToGPUFormat(std::vector<GPULightProbe>& gpuProbes) const;

    // =========================================================================
    // Member Variables
    // =========================================================================

    bool m_initialized = false;
    ProbeGridConfig m_config;

    // Probe storage
    std::vector<LightProbe> m_probes;
    glm::ivec3 m_gridDimensions{0};
    glm::vec3 m_gridOrigin{0.0f};
    glm::vec3 m_invSpacing{1.0f};

    // GPU resources
    uint32_t m_probeSSBO = 0;
    uint32_t m_gridInfoTexture = 0;
    uint32_t m_updateComputeSSBO = 0;
    bool m_gpuDataDirty = true;

    // Shaders
    std::unique_ptr<Shader> m_probeUpdateShader;     // Compute shader for probe raytracing
    std::unique_ptr<Shader> m_probeSampleShader;     // Fragment shader for GI sampling
    std::unique_ptr<Shader> m_debugVisualizationShader;

    // Hybrid GI
    std::shared_ptr<RadianceCascade> m_radianceCascade;

    // Raycast callback for CPU updates
    RaycastFunc m_raycastFunc;

    // Debug
    DebugView m_debugView = DebugView::None;

    // Statistics
    Stats m_stats;
    uint32_t m_frameCount = 0;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Generate uniformly distributed directions on sphere (Fibonacci lattice)
 */
void GenerateFibonacciSphereDirections(std::vector<glm::vec3>& directions, int numSamples);

/**
 * @brief Generate cosine-weighted hemisphere directions for diffuse sampling
 */
void GenerateCosineWeightedDirections(std::vector<glm::vec3>& directions, int numSamples,
                                      const glm::vec3& normal);

/**
 * @brief SH rotation matrix for rotating SH coefficients
 */
void RotateSH(const SHCoefficients& input, const glm::mat3& rotation, SHCoefficients& output);

/**
 * @brief Convolve SH with cosine lobe for diffuse irradiance
 */
void ConvolveSHCosine(SHCoefficients& sh);

} // namespace Nova
