#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace Nova {

/**
 * @brief GPU timing query for measuring pass performance
 */
struct GPUTimingQuery {
    uint32_t queryID = 0;
    std::string name;
    float timeMs = 0.0f;
    bool active = false;
};

/**
 * @brief Render pass statistics
 */
struct SDFPassStats {
    std::string passName;
    float gpuTimeMs = 0.0f;
    uint32_t pixelsProcessed = 0;
    uint32_t rayMarchSteps = 0;
    float avgStepsPerPixel = 0.0f;
};

/**
 * @brief Performance profiling data
 */
struct SDFPerformanceData {
    // Overall timing
    float totalFrameTimeMs = 0.0f;
    float sdfRenderTimeMs = 0.0f;
    float cullingTimeMs = 0.0f;
    float raymarchTimeMs = 0.0f;
    float temporalTimeMs = 0.0f;
    float reconstructionTimeMs = 0.0f;

    // Render statistics
    uint32_t totalPixels = 0;
    uint32_t tracedPixels = 0;
    uint32_t reprojectedPixels = 0;
    float reprojectionRate = 0.0f;

    // Raymarching statistics
    uint64_t totalRaySteps = 0;
    float avgStepsPerRay = 0.0f;
    uint32_t maxStepsPerRay = 0;

    // Instance statistics
    uint32_t totalInstances = 0;
    uint32_t visibleInstances = 0;
    uint32_t culledInstances = 0;

    // Cache statistics
    uint32_t cachedInstances = 0;
    uint32_t brickCacheHits = 0;
    uint32_t brickCacheMisses = 0;
    float cacheHitRate = 0.0f;

    // Quality metrics
    float targetFrameTimeMs = 16.67f;  // 60 FPS
    float qualityScale = 1.0f;         // Current quality scaling (1.0 = full, 0.5 = half-res)
    bool adaptiveQuality = true;

    // Frame counter
    uint32_t frameNumber = 0;
};

/**
 * @brief Visualization mode for performance profiling
 */
enum class SDFVisualizationMode {
    None = 0,
    StepCountHeatmap,        // Show raymarching step count
    OccupancyHeatmap,        // Show tile occupancy
    OverdrawVisualization,   // Show pixel overdraw
    LODVisualization,        // Show LOD levels
    CacheVisualization,      // Show brick cache usage
    TimingHeatmap            // Show per-tile timing
};

/**
 * @brief SDF Performance Profiler
 *
 * Comprehensive GPU performance profiling for SDF rendering:
 * - GPU timing queries per pass
 * - Raymarching statistics (steps, convergence)
 * - Tile occupancy analysis
 * - Cache hit rate tracking
 * - Automatic quality scaling to maintain target framerate
 * - Debug visualizations (heatmaps, overdraw, LOD)
 *
 * Performance:
 * - Sub-100us profiling overhead
 * - Frame-by-frame tracking
 * - Rolling average for stability
 */
class SDFPerformanceProfiler {
public:
    SDFPerformanceProfiler();
    ~SDFPerformanceProfiler();

    // Non-copyable
    SDFPerformanceProfiler(const SDFPerformanceProfiler&) = delete;
    SDFPerformanceProfiler& operator=(const SDFPerformanceProfiler&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize profiler
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // GPU Timing
    // =========================================================================

    /**
     * @brief Begin timing a GPU pass
     * @param passName Name of the pass
     */
    void BeginGPUPass(const std::string& passName);

    /**
     * @brief End timing current GPU pass
     */
    void EndGPUPass();

    /**
     * @brief Collect GPU timing results (call once per frame after rendering)
     */
    void CollectGPUTimings();

    // =========================================================================
    // Statistics Collection
    // =========================================================================

    /**
     * @brief Update frame statistics
     */
    void UpdateFrameStats(const SDFPerformanceData& data);

    /**
     * @brief Get current performance data
     */
    const SDFPerformanceData& GetPerformanceData() const { return m_currentData; }

    /**
     * @brief Get averaged performance data (smoothed over N frames)
     */
    const SDFPerformanceData& GetAveragedData() const { return m_averagedData; }

    // =========================================================================
    // Adaptive Quality Scaling
    // =========================================================================

    /**
     * @brief Enable/disable adaptive quality scaling
     */
    void SetAdaptiveQuality(bool enabled) { m_adaptiveQualityEnabled = enabled; }

    /**
     * @brief Set target frame time in milliseconds
     */
    void SetTargetFrameTime(float targetMs) { m_targetFrameTimeMs = targetMs; }

    /**
     * @brief Get recommended quality scale based on performance
     * @return Quality scale factor (0.5 = half-res, 1.0 = full-res, etc.)
     */
    float GetRecommendedQualityScale() const;

    // =========================================================================
    // Visualization
    // =========================================================================

    /**
     * @brief Set visualization mode
     */
    void SetVisualizationMode(SDFVisualizationMode mode) { m_visualizationMode = mode; }

    /**
     * @brief Get current visualization mode
     */
    SDFVisualizationMode GetVisualizationMode() const { return m_visualizationMode; }

    /**
     * @brief Generate debug visualization texture
     * @return Texture ID (0 if disabled)
     */
    uint32_t GetVisualizationTexture() const { return m_debugTexture; }

    // =========================================================================
    // Reporting
    // =========================================================================

    /**
     * @brief Get performance summary string
     */
    std::string GetPerformanceSummary() const;

    /**
     * @brief Print performance report to console
     */
    void PrintPerformanceReport() const;

    /**
     * @brief Export performance data to CSV
     */
    bool ExportToCSV(const std::string& filename) const;

private:
    // Create GPU timing queries
    bool CreateTimingQueries();

    // Update averaged statistics
    void UpdateAveragedStats();

    // Calculate quality scale adjustment
    float CalculateQualityScale(float currentFrameTime, float targetFrameTime);

    // Create debug visualization
    void UpdateDebugVisualization();

    bool m_initialized = false;

    // GPU timing queries
    std::vector<GPUTimingQuery> m_timingQueries;
    int m_currentQueryIndex = -1;
    static constexpr int MAX_QUERIES = 16;

    // Performance data
    SDFPerformanceData m_currentData;
    SDFPerformanceData m_averagedData;

    // Frame history for averaging (ring buffer)
    static constexpr int HISTORY_SIZE = 60;
    std::vector<SDFPerformanceData> m_frameHistory;
    int m_historyIndex = 0;

    // Adaptive quality
    bool m_adaptiveQualityEnabled = true;
    float m_targetFrameTimeMs = 16.67f;  // 60 FPS
    float m_currentQualityScale = 1.0f;
    float m_qualityScaleVelocity = 0.0f;  // For smooth transitions

    // Visualization
    SDFVisualizationMode m_visualizationMode = SDFVisualizationMode::None;
    uint32_t m_debugTexture = 0;
    uint32_t m_debugFBO = 0;

    // Frame counter
    uint32_t m_frameCounter = 0;
};

} // namespace Nova
