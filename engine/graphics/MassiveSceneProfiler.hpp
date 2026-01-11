#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace Engine {
namespace Graphics {

/**
 * @brief Performance sample
 */
struct PerformanceSample {
    float timeMs;
    uint64_t frameIndex;

    PerformanceSample() : timeMs(0), frameIndex(0) {}
    PerformanceSample(float t, uint64_t f) : timeMs(t), frameIndex(f) {}
};

/**
 * @brief Performance counter for tracking specific metrics
 */
class PerformanceCounter {
public:
    explicit PerformanceCounter(const std::string& name, size_t maxSamples = 120);

    void AddSample(float value, uint64_t frameIndex);
    float GetAverage() const;
    float GetMin() const;
    float GetMax() const;
    float GetLatest() const;
    const std::vector<PerformanceSample>& GetSamples() const { return m_samples; }

    const std::string& GetName() const { return m_name; }

private:
    std::string m_name;
    std::vector<PerformanceSample> m_samples;
    size_t m_maxSamples;
};

/**
 * @brief Profiler for massive scene rendering (10K+ objects, 100K+ lights)
 */
class MassiveSceneProfiler {
public:
    /**
     * @brief Profiling categories
     */
    enum class Category {
        CPUCulling,
        GPUCulling,
        LightClustering,
        GBufferPass,
        LightingPass,
        ShadowMapping,
        TerrainRendering,
        Total
    };

    MassiveSceneProfiler();
    ~MassiveSceneProfiler();

    /**
     * @brief Initialize profiler
     */
    bool Initialize();

    /**
     * @brief Begin frame profiling
     */
    void BeginFrame(uint64_t frameIndex);

    /**
     * @brief Begin profiling category
     */
    void BeginCategory(Category category);

    /**
     * @brief End profiling category
     */
    void EndCategory(Category category);

    /**
     * @brief End frame profiling
     */
    void EndFrame();

    /**
     * @brief Get counter for category
     */
    PerformanceCounter* GetCounter(Category category);

    /**
     * @brief Performance report
     */
    struct Report {
        float avgFrameTimeMs;
        float avgCPUCullingMs;
        float avgGPUCullingMs;
        float avgLightClusteringMs;
        float avgGBufferMs;
        float avgLightingMs;
        float avgShadowMs;
        float avgTerrainMs;

        float targetFPS;
        float actualFPS;
        bool isCPUBound;
        bool isGPUBound;

        std::string bottleneck;

        Report()
            : avgFrameTimeMs(0), avgCPUCullingMs(0), avgGPUCullingMs(0)
            , avgLightClusteringMs(0), avgGBufferMs(0), avgLightingMs(0)
            , avgShadowMs(0), avgTerrainMs(0)
            , targetFPS(60), actualFPS(0)
            , isCPUBound(false), isGPUBound(false) {}
    };

    /**
     * @brief Generate performance report
     */
    Report GenerateReport() const;

    /**
     * @brief Print report to console
     */
    void PrintReport() const;

    /**
     * @brief Export report to file
     */
    bool ExportReport(const std::string& filename) const;

    /**
     * @brief Reset all counters
     */
    void Reset();

private:
    std::string CategoryToString(Category category) const;
    void DetectBottleneck(Report& report) const;

    uint64_t m_currentFrame;
    std::unordered_map<Category, std::unique_ptr<PerformanceCounter>> m_counters;

    // GPU query objects per category
    std::unordered_map<Category, uint32_t> m_queryObjects;

    // CPU timing
    std::unordered_map<Category, std::chrono::high_resolution_clock::time_point> m_categoryStartTimes;

    // Frame timing
    std::chrono::high_resolution_clock::time_point m_frameStartTime;
    PerformanceCounter m_frameTimes;
};

} // namespace Graphics
} // namespace Engine
