#pragma once

/**
 * @file RTXSupport.hpp
 * @brief Hardware ray tracing capability detection and management
 *
 * Detects and manages hardware ray tracing support for:
 * - NVIDIA RTX (DirectX Raytracing / DXR)
 * - Vulkan Ray Tracing (VK_KHR_ray_tracing_pipeline)
 * - OpenGL with NV_ray_tracing extension
 *
 * This enables 3-5x performance improvement for path tracing on RTX GPUs.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace Nova {

/**
 * @brief Ray tracing API backend
 */
enum class RayTracingAPI {
    None,                   // No ray tracing support
    OpenGL_NV_ray_tracing, // NVIDIA OpenGL extension
    Vulkan_KHR,            // Vulkan KHR ray tracing
    DirectX_Raytracing,    // DXR (DirectX 12)
};

/**
 * @brief Ray tracing tier/version
 */
enum class RayTracingTier {
    None = 0,
    Tier_1_0 = 10,  // Basic ray tracing
    Tier_1_1 = 11,  // Inline ray tracing, additional features
    Tier_1_2 = 12,  // Enhanced performance features
};

/**
 * @brief Hardware ray tracing capabilities
 */
struct RTXCapabilities {
    // General support
    bool hasRayTracing = false;
    bool hasInlineRayTracing = false;       // SM 6.5+, Vulkan 1.2+
    bool hasMeshShaders = false;
    bool hasVariableRateShading = false;

    // Ray tracing features
    bool hasRayQuery = false;               // Inline ray tracing in any shader
    bool hasRayMotionBlur = false;
    bool hasRayTracingMaintenance1 = false;

    // Performance features
    bool hasOpacityMicromap = false;        // RTX 4000 series
    bool hasDisplacementMicromap = false;   // RTX 4000 series
    bool hasShaderExecutionReordering = false; // SER for better occupancy

    // Limits
    int maxRecursionDepth = 0;              // Typically 31
    int maxRayGenerationThreads = 0;        // Typically 1048576
    int maxInstanceCount = 0;               // Max instances in TLAS
    int maxGeometryCount = 0;               // Max geometries per BLAS
    uint64_t maxAccelerationStructureSize = 0;

    // Memory requirements
    uint64_t scratchBufferAlignment = 0;
    uint64_t shaderGroupHandleSize = 0;
    uint64_t shaderGroupBaseAlignment = 0;

    // API info
    RayTracingAPI api = RayTracingAPI::None;
    RayTracingTier tier = RayTracingTier::None;
    std::string apiVersion;

    // Device info
    std::string deviceName;
    std::string vendorName;
    uint32_t driverVersion = 0;

    // Extensions available
    std::vector<std::string> extensions;

    /**
     * @brief Check if a specific extension is supported
     */
    [[nodiscard]] bool HasExtension(const std::string& ext) const;

    /**
     * @brief Get human-readable capability summary
     */
    [[nodiscard]] std::string ToString() const;
};

/**
 * @brief Performance metrics for ray tracing
 */
struct RTXPerformanceMetrics {
    // Timing (in milliseconds)
    double totalFrameTime = 0.0;
    double accelerationBuildTime = 0.0;
    double accelerationUpdateTime = 0.0;
    double rayTracingTime = 0.0;
    double shadingTime = 0.0;
    double denoisingTime = 0.0;

    // Ray statistics
    uint64_t totalRaysCast = 0;
    uint64_t primaryRays = 0;
    uint64_t shadowRays = 0;
    uint64_t secondaryRays = 0;
    uint64_t aoRays = 0;

    // Acceleration structure stats
    uint32_t blasCount = 0;
    uint32_t tlasInstanceCount = 0;
    uint64_t totalASMemory = 0;         // Bytes
    uint64_t scratchMemoryUsed = 0;     // Bytes

    // Performance metrics
    double raysPerSecond = 0.0;
    double speedupVsCompute = 1.0;      // Speedup compared to compute shader

    /**
     * @brief Reset all metrics
     */
    void Reset();

    /**
     * @brief Calculate derived metrics
     */
    void Calculate();

    /**
     * @brief Get formatted string of metrics
     */
    [[nodiscard]] std::string ToString() const;
};

/**
 * @brief RTX Support Manager
 *
 * Singleton class for detecting and managing hardware ray tracing support.
 * Automatically detects the best available ray tracing API.
 */
class RTXSupport {
public:
    /**
     * @brief Get singleton instance
     */
    static RTXSupport& Get();

    /**
     * @brief Initialize ray tracing support detection
     * @return true if ray tracing is available
     */
    bool Initialize();

    /**
     * @brief Shutdown ray tracing support
     */
    void Shutdown();

    /**
     * @brief Check if ray tracing is available
     */
    [[nodiscard]] static bool IsAvailable();

    /**
     * @brief Get ray tracing tier
     */
    [[nodiscard]] static RayTracingTier GetRayTracingTier();

    /**
     * @brief Get ray tracing API
     */
    [[nodiscard]] static RayTracingAPI GetRayTracingAPI();

    /**
     * @brief Query full capabilities
     */
    [[nodiscard]] static RTXCapabilities QueryCapabilities();

    /**
     * @brief Get current capabilities (cached)
     */
    [[nodiscard]] const RTXCapabilities& GetCapabilities() const { return m_capabilities; }

    /**
     * @brief Check if specific feature is supported
     */
    [[nodiscard]] bool HasFeature(const std::string& feature) const;

    /**
     * @brief Get/Set performance metrics
     */
    [[nodiscard]] RTXPerformanceMetrics& GetMetrics() { return m_metrics; }
    [[nodiscard]] const RTXPerformanceMetrics& GetMetrics() const { return m_metrics; }

    /**
     * @brief Enable/disable ray tracing (runtime toggle)
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return m_enabled && m_initialized; }

    /**
     * @brief Log capabilities to console
     */
    void LogCapabilities() const;

    /**
     * @brief Benchmark ray tracing performance
     * @return Estimated rays per second
     */
    double BenchmarkPerformance();

private:
    RTXSupport() = default;
    ~RTXSupport() = default;

    // Non-copyable, non-movable
    RTXSupport(const RTXSupport&) = delete;
    RTXSupport& operator=(const RTXSupport&) = delete;
    RTXSupport(RTXSupport&&) = delete;
    RTXSupport& operator=(RTXSupport&&) = delete;

    // Detection methods for different APIs
    bool DetectOpenGLRayTracing();
    bool DetectVulkanRayTracing();
    bool DetectDirectXRayTracing();

    // Query capabilities for each API
    void QueryOpenGLCapabilities();
    void QueryVulkanCapabilities();
    void QueryDirectXCapabilities();

    bool m_initialized = false;
    bool m_enabled = true;
    RTXCapabilities m_capabilities;
    RTXPerformanceMetrics m_metrics;
};

/**
 * @brief RAII helper for RTX performance measurements
 */
class RTXScopedTimer {
public:
    RTXScopedTimer(double& outTime);
    ~RTXScopedTimer();

private:
    double& m_outTime;
    double m_startTime;
};

} // namespace Nova
