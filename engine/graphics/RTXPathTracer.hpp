#pragma once

/**
 * @file RTXPathTracer.hpp
 * @brief Modern RTX-accelerated path tracer with SOLID architecture
 *
 * Complete rewrite implementing:
 * - IRayTracingBackend interface for multiple API support (DXR 1.1, Vulkan RT, Compute fallback)
 * - AccelerationStructureManager for BLAS/TLAS management
 * - ShaderBindingTableBuilder for SBT construction
 * - RayGenShader, MissShader, HitShader abstractions
 * - Inline ray tracing for hybrid SDF/polygon rendering
 * - Ray query integration for SDF evaluation
 * - SVGF/NRD denoiser integration
 * - Compute-based fallback for non-RTX hardware
 *
 * Target: <2ms per frame at 1080p (500+ FPS)
 */

#include "RTXSupport.hpp"
#include "RTXAccelerationStructure.hpp"
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Camera;
class SDFModel;
class Texture;
class Shader;
class IRayTracingBackend;
class DenoiserIntegration;

/**
 * @brief Path tracing render settings
 */
struct PathTracingSettings {
    // Quality
    int maxBounces = 4;
    int samplesPerPixel = 1;
    bool enableAccumulation = true;

    // Features
    bool enableShadows = true;
    bool enableGlobalIllumination = true;
    bool enableAmbientOcclusion = true;
    float aoRadius = 1.0f;

    // Lighting
    glm::vec3 lightDirection{0.5f, -1.0f, 0.5f};
    glm::vec3 lightColor{1.0f, 1.0f, 1.0f};
    float lightIntensity = 1.0f;

    // Background
    glm::vec3 backgroundColor{0.1f, 0.1f, 0.15f};
    bool useEnvironmentMap = false;

    // Performance
    float maxDistance = 1000.0f;
    bool enableDenoise = false;

    // Ray query settings for hybrid rendering
    bool enableInlineRayTracing = true;
    bool enableRayQueryForSDF = true;
};

/**
 * @brief Path tracer statistics
 */
struct PathTracerStats {
    // Timing (milliseconds)
    double totalFrameTime = 0.0;
    double accelerationUpdateTime = 0.0;
    double rayTracingTime = 0.0;
    double denoisingTime = 0.0;

    // Ray counts
    uint64_t primaryRays = 0;
    uint64_t shadowRays = 0;
    uint64_t secondaryRays = 0;

    // Accumulation
    uint32_t accumulatedFrames = 0;

    void Reset() { *this = PathTracerStats{}; }
};

/**
 * @brief RTX-accelerated path tracer with SOLID architecture
 *
 * Architecture:
 * - Single Responsibility: Each backend handles one ray tracing API
 * - Open/Closed: New backends can be added without modifying core
 * - Liskov Substitution: All backends implement IRayTracingBackend
 * - Interface Segregation: Separate interfaces for RT, denoising, AS management
 * - Dependency Inversion: Core depends on abstractions, not concrete implementations
 *
 * Features:
 * - Hardware ray tracing using RTX cores (DXR 1.1 / Vulkan RT)
 * - Compute shader fallback for non-RTX hardware
 * - Bottom-level and top-level acceleration structures
 * - Multi-bounce global illumination
 * - Real-time shadows and ambient occlusion
 * - Temporal accumulation for noise reduction
 * - SVGF/NRD denoising integration
 * - Inline ray tracing for hybrid SDF/polygon rendering
 * - Ray query support for SDF evaluation
 */
class RTXPathTracer {
public:
    RTXPathTracer();
    ~RTXPathTracer();

    // Non-copyable
    RTXPathTracer(const RTXPathTracer&) = delete;
    RTXPathTracer& operator=(const RTXPathTracer&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize RTX path tracer
     *
     * Automatically selects the best available backend:
     * 1. Hardware RTX (DXR 1.1 / Vulkan RT) if available
     * 2. Compute shader path tracing as fallback
     *
     * @param width Render target width
     * @param height Render target height
     * @return true if initialization succeeded
     */
    bool Initialize(int width, int height);

    /**
     * @brief Shutdown and cleanup all resources
     */
    void Shutdown();

    /**
     * @brief Check if path tracer is initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Check if using hardware ray tracing
     */
    [[nodiscard]] bool IsUsingHardwareRT() const;

    /**
     * @brief Check if inline ray tracing is supported
     */
    [[nodiscard]] bool SupportsInlineRayTracing() const;

    /**
     * @brief Get the active backend name
     */
    [[nodiscard]] const char* GetBackendName() const;

    // =========================================================================
    // Scene Management
    // =========================================================================

    /**
     * @brief Build acceleration structures from scene
     *
     * Creates BLAS for each model and TLAS for the scene.
     * Automatically uses appropriate build settings based on
     * hardware capabilities.
     *
     * @param models SDF models to render
     * @param transforms Transform for each model
     */
    void BuildScene(const std::vector<const SDFModel*>& models,
                    const std::vector<glm::mat4>& transforms);

    /**
     * @brief Update scene (for dynamic objects)
     *
     * Performs fast TLAS update without rebuilding BLAS.
     * Use when only transforms change, not geometry.
     *
     * @param transforms New transforms for each model
     */
    void UpdateScene(const std::vector<glm::mat4>& transforms);

    /**
     * @brief Clear scene and reset acceleration structures
     */
    void ClearScene();

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render frame using hardware ray tracing
     *
     * Pipeline:
     * 1. Update camera and settings uniforms
     * 2. Dispatch ray tracing (hardware or compute)
     * 3. Apply denoising if enabled
     * 4. Return output texture
     *
     * @param camera Camera to render from
     * @return Output texture with rendered image
     */
    uint32_t Render(const Camera& camera);

    /**
     * @brief Render to specific framebuffer
     *
     * Renders and blits result to target framebuffer.
     *
     * @param camera Camera to render from
     * @param framebuffer Target framebuffer (0 for default)
     */
    void RenderToFramebuffer(const Camera& camera, uint32_t framebuffer);

    /**
     * @brief Reset temporal accumulation
     *
     * Call when camera moves or scene changes significantly.
     * Also resets denoiser temporal history.
     */
    void ResetAccumulation();

    /**
     * @brief Resize render targets
     *
     * Recreates all render targets and resets accumulation.
     *
     * @param width New width
     * @param height New height
     */
    void Resize(int width, int height);

    // =========================================================================
    // Settings
    // =========================================================================

    [[nodiscard]] PathTracingSettings& GetSettings() { return m_settings; }
    [[nodiscard]] const PathTracingSettings& GetSettings() const { return m_settings; }

    void SetSettings(const PathTracingSettings& settings) {
        m_settings = settings;
        ResetAccumulation();
    }

    // =========================================================================
    // Denoising
    // =========================================================================

    /**
     * @brief Enable or disable denoising
     */
    void SetDenoiseEnabled(bool enabled);

    /**
     * @brief Check if denoising is enabled
     */
    [[nodiscard]] bool IsDenoiseEnabled() const;

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] const PathTracerStats& GetStats() const { return m_stats; }
    void ResetStats() { m_stats.Reset(); }

    /**
     * @brief Get performance metrics (rays per second)
     */
    [[nodiscard]] double GetRaysPerSecond() const;

    /**
     * @brief Get speedup compared to compute shader baseline
     */
    [[nodiscard]] double GetSpeedupFactor() const { return m_speedupFactor; }

    // =========================================================================
    // Environment
    // =========================================================================

    void SetEnvironmentMap(std::shared_ptr<Texture> envMap);
    [[nodiscard]] std::shared_ptr<Texture> GetEnvironmentMap() const { return m_environmentMap; }

    // =========================================================================
    // Output
    // =========================================================================

    [[nodiscard]] uint32_t GetOutputTexture() const { return m_outputTexture; }
    [[nodiscard]] int GetWidth() const { return m_width; }
    [[nodiscard]] int GetHeight() const { return m_height; }

private:
    // Initialization helpers
    bool InitializeRayTracingPipeline();
    bool InitializeShaderBindingTable();
    bool InitializeRenderTargets();

    // Shader compilation
    bool CompileRayTracingShaders();
    uint32_t CompileShader(const std::string& path, uint32_t shaderType);

    // Rendering helpers
    void UpdateUniforms(const Camera& camera);
    void DispatchRays();

    // State
    bool m_initialized = false;
    int m_width = 1920;
    int m_height = 1080;

    // Modern SOLID architecture components
    std::unique_ptr<IRayTracingBackend> m_backend;
    std::unique_ptr<DenoiserIntegration> m_denoiser;

    // Legacy RTX components (for backward compatibility)
    std::unique_ptr<RTXAccelerationStructure> m_accelerationStructure;

    // Ray tracing pipeline (legacy)
    uint32_t m_rtPipeline = 0;
    uint32_t m_rayGenShader = 0;
    uint32_t m_closestHitShader = 0;
    uint32_t m_missShader = 0;
    uint32_t m_shadowMissShader = 0;
    uint32_t m_shadowAnyHitShader = 0;

    // Shader binding table
    uint32_t m_sbtBuffer = 0;
    size_t m_sbtSize = 0;

    // Render targets
    uint32_t m_accumulationTexture = 0;  // RGBA32F for accumulation
    uint32_t m_outputTexture = 0;        // RGBA8 for display

    // Uniform buffers
    uint32_t m_cameraUBO = 0;
    uint32_t m_settingsUBO = 0;
    uint32_t m_environmentSettingsUBO = 0;

    // Scene data
    uint64_t m_tlasHandle = 0;
    std::vector<uint64_t> m_blasHandles;

    // Settings
    PathTracingSettings m_settings;
    std::shared_ptr<Texture> m_environmentMap;

    // Statistics
    PathTracerStats m_stats;
    uint32_t m_frameCount = 0;
    double m_speedupFactor = 3.5; // Typical speedup vs compute shader

    // Temporal data for motion vectors and TAA
    glm::mat4 m_prevViewProjInverse{1.0f};
    glm::vec2 m_prevJitter{0.0f};

    // Cache for camera movement detection
    glm::vec3 m_lastCameraPos{0.0f};
    glm::vec3 m_lastCameraDir{0.0f};
};

} // namespace Nova
