#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <chrono>

namespace Nova {

// Forward declarations
class RTGIPipeline;
class Camera;
class ClusteredLightManager;

/**
 * @brief RTGI Benchmark and Profiling Tool
 *
 * Comprehensive benchmarking system for measuring and validating
 * ReSTIR + SVGF performance. Provides:
 * - Automated performance testing
 * - Quality comparisons (1 SPP vs reference)
 * - Frame time histograms
 * - GPU memory usage tracking
 * - CSV export for analysis
 */
class RTGIBenchmark {
public:
    RTGIBenchmark();
    ~RTGIBenchmark();

    // =========================================================================
    // Benchmark Configuration
    // =========================================================================

    struct Config {
        int warmupFrames = 60;          // Frames to skip before measuring
        int benchmarkFrames = 600;      // Frames to measure (10 seconds at 60 FPS)
        bool exportCSV = true;          // Export results to CSV
        std::string outputPath = "benchmark_results.csv";

        // Test scenarios
        bool testAllPresets = true;     // Test all quality presets
        bool testLightCounts = true;    // Test with varying light counts
        bool testResolutions = true;    // Test different resolutions

        // Quality validation
        bool compareWithReference = false;  // Compare with 1000 SPP reference
        std::string referencePath = "";     // Path to reference images
    };

    void SetConfig(const Config& config) { m_config = config; }
    const Config& GetConfig() const { return m_config; }

    // =========================================================================
    // Benchmarking
    // =========================================================================

    /**
     * @brief Run complete benchmark suite
     */
    void RunBenchmarkSuite(RTGIPipeline& pipeline,
                          Camera& camera,
                          ClusteredLightManager& lightManager);

    /**
     * @brief Run single benchmark scenario
     */
    void RunSingleBenchmark(const std::string& scenarioName,
                           RTGIPipeline& pipeline,
                           Camera& camera,
                           ClusteredLightManager& lightManager);

    /**
     * @brief Start frame timing
     */
    void BeginFrame();

    /**
     * @brief End frame timing
     */
    void EndFrame();

    // =========================================================================
    // Results
    // =========================================================================

    struct FrameStats {
        float frameTimeMs;
        float restirMs;
        float svgfMs;
        uint32_t lightCount;
        uint32_t effectiveSPP;
    };

    struct BenchmarkResults {
        std::string scenarioName;
        int frameCount = 0;

        // Timing stats
        float avgFrameTimeMs = 0.0f;
        float minFrameTimeMs = 999999.0f;
        float maxFrameTimeMs = 0.0f;
        float stdDevFrameTimeMs = 0.0f;

        float avgFPS = 0.0f;
        float minFPS = 0.0f;
        float maxFPS = 0.0f;

        // Percentiles
        float p50FrameTimeMs = 0.0f;  // Median
        float p95FrameTimeMs = 0.0f;
        float p99FrameTimeMs = 0.0f;

        // Breakdown
        float avgRestirMs = 0.0f;
        float avgSvgfMs = 0.0f;

        // System info
        int width = 0;
        int height = 0;
        uint32_t avgLightCount = 0;
        uint32_t effectiveSPP = 0;

        // Frame time histogram
        std::vector<float> frameTimesMs;
    };

    const std::vector<BenchmarkResults>& GetResults() const { return m_results; }
    const BenchmarkResults& GetLatestResult() const { return m_results.back(); }

    /**
     * @brief Print results to console
     */
    void PrintResults() const;

    /**
     * @brief Export results to CSV
     */
    void ExportToCSV(const std::string& filepath) const;

    /**
     * @brief Generate HTML report with charts
     */
    void GenerateHTMLReport(const std::string& filepath) const;

    // =========================================================================
    // Quality Metrics
    // =========================================================================

    /**
     * @brief Compare output with reference image
     * @return PSNR (Peak Signal-to-Noise Ratio) in dB
     */
    float CompareWithReference(uint32_t outputTexture,
                               const std::string& referencePath);

    /**
     * @brief Compute SSIM (Structural Similarity Index)
     */
    float ComputeSSIM(uint32_t outputTexture,
                     const std::string& referencePath);

private:
    // Benchmark scenarios
    void BenchmarkPreset(const std::string& presetName,
                        RTGIPipeline& pipeline,
                        Camera& camera,
                        ClusteredLightManager& lightManager);

    void BenchmarkLightCount(int lightCount,
                            RTGIPipeline& pipeline,
                            Camera& camera,
                            ClusteredLightManager& lightManager);

    void BenchmarkResolution(int width, int height,
                            RTGIPipeline& pipeline,
                            Camera& camera,
                            ClusteredLightManager& lightManager);

    // Statistics
    void ComputeStatistics(BenchmarkResults& results);
    float ComputeStdDev(const std::vector<float>& values, float mean);
    float ComputePercentile(std::vector<float> values, float percentile);

    Config m_config;
    std::vector<BenchmarkResults> m_results;

    // Current benchmark state
    bool m_benchmarking = false;
    int m_currentFrame = 0;
    BenchmarkResults m_currentResults;
    std::chrono::high_resolution_clock::time_point m_frameStart;

    std::vector<FrameStats> m_frameStats;
};

} // namespace Nova
