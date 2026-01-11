#pragma once

/**
 * @file HybridPathTracer.hpp
 * @brief Hybrid path tracer with automatic RTX/compute shader switching
 *
 * Automatically selects the best available rendering path:
 * 1. RTX hardware ray tracing (fastest, requires RTX GPU)
 * 2. Compute shader path tracing (fallback for non-RTX GPUs)
 *
 * Provides seamless API regardless of which backend is active.
 */

#include "RTXPathTracer.hpp"
#include "SDFRenderer.hpp"
#include <memory>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Camera;
class SDFModel;

/**
 * @brief Rendering backend type
 */
enum class PathTracerBackend {
    None,
    RTX_Hardware,       // Hardware ray tracing (NVIDIA RTX, AMD RDNA2+)
    Compute_Shader,     // Software ray tracing via compute shader
};

/**
 * @brief Convert backend enum to string
 */
constexpr const char* PathTracerBackendToString(PathTracerBackend backend) noexcept {
    switch (backend) {
        case PathTracerBackend::None: return "None";
        case PathTracerBackend::RTX_Hardware: return "RTX Hardware";
        case PathTracerBackend::Compute_Shader: return "Compute Shader";
        default: return "Unknown";
    }
}

/**
 * @brief Hybrid path tracer configuration
 */
struct HybridPathTracerConfig {
    // Backend selection
    bool preferRTX = true;              // Try RTX first if available
    bool allowFallback = true;          // Fall back to compute if RTX unavailable

    // Performance thresholds for automatic backend switching
    bool enableAutoSwitch = false;      // Automatically switch based on performance
    double minRTXSpeedup = 1.5;        // Minimum speedup to justify RTX
    double targetFrameTime = 8.33;     // Target frame time (ms) for 120 FPS

    // Quality presets
    bool matchQualityAcrossBackends = true;  // Use same quality settings
};

/**
 * @brief Hybrid path tracer performance comparison
 */
struct PathTracerComparison {
    // Timing (milliseconds)
    double rtxFrameTime = 0.0;
    double computeFrameTime = 0.0;

    // Speedup
    double speedupFactor = 1.0;         // RTX vs Compute

    // Quality metrics
    uint32_t rtxSamples = 0;
    uint32_t computeSamples = 0;

    // Memory usage
    size_t rtxMemoryMB = 0;
    size_t computeMemoryMB = 0;

    [[nodiscard]] double GetSpeedup() const {
        if (computeFrameTime <= 0.0) return 1.0;
        return computeFrameTime / rtxFrameTime;
    }

    [[nodiscard]] std::string ToString() const;
};

/**
 * @brief Hybrid Path Tracer
 *
 * Intelligently switches between hardware ray tracing and compute shader
 * path tracing based on GPU capabilities and performance requirements.
 *
 * Features:
 * - Automatic backend detection and selection
 * - Seamless fallback for non-RTX GPUs
 * - Performance comparison and benchmarking
 * - Unified API regardless of backend
 * - Runtime backend switching (experimental)
 *
 * Performance expectations:
 * - RTX: 1.5ms per frame (666 FPS) @ 1080p
 * - Compute: 5.5ms per frame (182 FPS) @ 1080p
 * - Speedup: 3.6x
 */
class HybridPathTracer {
public:
    HybridPathTracer();
    ~HybridPathTracer();

    // Non-copyable
    HybridPathTracer(const HybridPathTracer&) = delete;
    HybridPathTracer& operator=(const HybridPathTracer&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize hybrid path tracer
     * @param width Render target width
     * @param height Render target height
     * @param config Configuration for backend selection
     * @return true if at least one backend initialized successfully
     */
    bool Initialize(int width, int height, const HybridPathTracerConfig& config = {});

    /**
     * @brief Shutdown and cleanup both backends
     */
    void Shutdown();

    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Backend Management
    // =========================================================================

    /**
     * @brief Get active backend
     */
    [[nodiscard]] PathTracerBackend GetActiveBackend() const { return m_activeBackend; }

    /**
     * @brief Check which backends are available
     */
    [[nodiscard]] bool IsRTXAvailable() const { return m_rtxAvailable; }
    [[nodiscard]] bool IsComputeAvailable() const { return m_computeAvailable; }

    /**
     * @brief Manually switch backend (if available)
     * @return true if switch was successful
     */
    bool SwitchBackend(PathTracerBackend backend);

    /**
     * @brief Check if currently using hardware ray tracing
     */
    [[nodiscard]] bool IsUsingHardwareRT() const {
        return m_activeBackend == PathTracerBackend::RTX_Hardware;
    }

    // =========================================================================
    // Scene Management
    // =========================================================================

    /**
     * @brief Build scene from SDF models
     */
    void BuildScene(const std::vector<const SDFModel*>& models,
                    const std::vector<glm::mat4>& transforms);

    /**
     * @brief Update scene transforms
     */
    void UpdateScene(const std::vector<glm::mat4>& transforms);

    /**
     * @brief Clear scene
     */
    void ClearScene();

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render frame using active backend
     * @return Output texture ID
     */
    uint32_t Render(const Camera& camera);

    /**
     * @brief Render to specific framebuffer
     */
    void RenderToFramebuffer(const Camera& camera, uint32_t framebuffer);

    /**
     * @brief Reset accumulation
     */
    void ResetAccumulation();

    /**
     * @brief Resize render targets
     */
    void Resize(int width, int height);

    // =========================================================================
    // Settings (unified across backends)
    // =========================================================================

    /**
     * @brief Get/Set path tracing settings
     * Automatically synced to active backend
     */
    [[nodiscard]] PathTracingSettings& GetSettings() { return m_settings; }
    [[nodiscard]] const PathTracingSettings& GetSettings() const { return m_settings; }

    void SetSettings(const PathTracingSettings& settings);

    /**
     * @brief Apply quality preset
     */
    void ApplyQualityPreset(const std::string& preset);

    // =========================================================================
    // Statistics & Performance
    // =========================================================================

    /**
     * @brief Get statistics from active backend
     */
    [[nodiscard]] PathTracerStats GetStats() const;

    /**
     * @brief Get performance comparison between backends
     * Requires running benchmark first
     */
    [[nodiscard]] const PathTracerComparison& GetComparison() const { return m_comparison; }

    /**
     * @brief Benchmark both backends (if available)
     * @param frames Number of frames to test
     * @return Performance comparison results
     */
    PathTracerComparison Benchmark(int frames = 100);

    /**
     * @brief Get current frame time (ms)
     */
    [[nodiscard]] double GetFrameTime() const;

    /**
     * @brief Get rays per second
     */
    [[nodiscard]] double GetRaysPerSecond() const;

    // =========================================================================
    // Configuration
    // =========================================================================

    [[nodiscard]] const HybridPathTracerConfig& GetConfig() const { return m_config; }
    void SetConfig(const HybridPathTracerConfig& config) { m_config = config; }

    // =========================================================================
    // Environment
    // =========================================================================

    void SetEnvironmentMap(std::shared_ptr<Texture> envMap);
    [[nodiscard]] std::shared_ptr<Texture> GetEnvironmentMap() const { return m_environmentMap; }

    // =========================================================================
    // Direct Backend Access (advanced)
    // =========================================================================

    [[nodiscard]] RTXPathTracer* GetRTXPathTracer() { return m_rtxPathTracer.get(); }
    [[nodiscard]] SDFRenderer* GetComputeRenderer() { return m_computeRenderer.get(); }

    // =========================================================================
    // Diagnostics
    // =========================================================================

    /**
     * @brief Log backend information and capabilities
     */
    void LogBackendInfo() const;

    /**
     * @brief Get recommended backend for current hardware
     */
    [[nodiscard]] PathTracerBackend GetRecommendedBackend() const;

private:
    // Initialization helpers
    bool InitializeRTX();
    bool InitializeCompute();
    void SelectInitialBackend();

    // Settings sync
    void SyncSettingsToBackends();
    PathTracingSettings ConvertSDFSettingsToPathTracing(const SDFRenderSettings& sdfSettings);
    SDFRenderSettings ConvertPathTracingToSDFSettings(const PathTracingSettings& ptSettings);

    // State
    bool m_initialized = false;
    int m_width = 1920;
    int m_height = 1080;

    // Configuration
    HybridPathTracerConfig m_config;

    // Backend availability
    bool m_rtxAvailable = false;
    bool m_computeAvailable = false;
    PathTracerBackend m_activeBackend = PathTracerBackend::None;

    // Backend implementations
    std::unique_ptr<RTXPathTracer> m_rtxPathTracer;
    std::unique_ptr<SDFRenderer> m_computeRenderer;    // Uses compute shader for SDF ray marching

    // Unified settings
    PathTracingSettings m_settings;
    std::shared_ptr<Texture> m_environmentMap;

    // Performance comparison
    PathTracerComparison m_comparison;
    bool m_hasComparisonData = false;

    // Cached scene data
    std::vector<const SDFModel*> m_cachedModels;
    std::vector<glm::mat4> m_cachedTransforms;
};

} // namespace Nova
