#pragma once

#include "PerformanceDatabase.hpp"
#include <string>
#include <vector>
#include <map>

namespace Engine {
namespace Profiling {

/**
 * @brief Frame time percentiles
 */
struct FrameTimePercentiles {
    float p1 = 0.0f;    // 1st percentile
    float p5 = 0.0f;    // 5th percentile
    float p50 = 0.0f;   // Median
    float p95 = 0.0f;   // 95th percentile
    float p99 = 0.0f;   // 99th percentile
    float min = 0.0f;
    float max = 0.0f;
    float mean = 0.0f;
    float stdDev = 0.0f;
};

/**
 * @brief Session comparison data
 */
struct SessionComparison {
    int sessionA;
    int sessionB;

    // FPS comparison
    float fpsA = 0.0f;
    float fpsB = 0.0f;
    float fpsDelta = 0.0f;
    float fpsPercentChange = 0.0f;

    // Frame time comparison
    float frameTimeA = 0.0f;
    float frameTimeB = 0.0f;
    float frameTimeDelta = 0.0f;
    float frameTimePercentChange = 0.0f;

    // Stage comparisons
    std::map<std::string, float> stageTimeDeltasA;
    std::map<std::string, float> stageTimeDeltasB;
    std::map<std::string, float> stageDeltas;

    // Memory comparison
    float gpuMemoryA = 0.0f;
    float gpuMemoryB = 0.0f;
    float memoryDelta = 0.0f;
};

/**
 * @brief Performance trend analysis
 */
struct PerformanceTrend {
    enum class Direction {
        IMPROVING,
        STABLE,
        DEGRADING
    };

    Direction direction = Direction::STABLE;
    float trendSlope = 0.0f;  // Positive = improving, negative = degrading
    float confidence = 0.0f;  // 0-1, how confident we are in the trend
    int sampleCount = 0;

    std::string GetDirectionString() const;
};

/**
 * @brief Bottleneck analysis
 */
struct BottleneckInfo {
    std::string stageName;
    float averageTimeMs = 0.0f;
    float averagePercent = 0.0f;
    float maxTimeMs = 0.0f;
    float minTimeMs = 0.0f;
    int occurrences = 0;
};

/**
 * @brief Frame spike information
 */
struct FrameSpike {
    int frameNumber = 0;
    float frameTimeMs = 0.0f;
    float averageFrameTimeMs = 0.0f;
    float multiplier = 0.0f;
    std::vector<std::pair<std::string, float>> stageBreakdown;
};

/**
 * @brief Stage statistics
 */
struct StageStatistics {
    std::string stageName;
    float avgTimeMs = 0.0f;
    float minTimeMs = 0.0f;
    float maxTimeMs = 0.0f;
    float avgPercent = 0.0f;
    float avgGPUTimeMs = 0.0f;
    float avgCPUTimeMs = 0.0f;
    int sampleCount = 0;
};

/**
 * @class PerformanceAnalyzer
 * @brief Advanced performance analysis and query engine
 *
 * Features:
 * - Statistical analysis (percentiles, trends, etc.)
 * - Session comparison
 * - Bottleneck identification
 * - Spike detection and analysis
 * - Performance regression detection
 */
class PerformanceAnalyzer {
public:
    PerformanceAnalyzer(PerformanceDatabase* database);
    ~PerformanceAnalyzer();

    // Statistical analysis
    FrameTimePercentiles GetPercentiles(int sessionId);
    FrameTimePercentiles GetPercentilesInTimeRange(int sessionId, double startTime, double endTime);

    // Session comparison
    SessionComparison CompareSessions(int sessionA, int sessionB);
    std::vector<SessionComparison> CompareAllSessions();

    // Trend analysis
    PerformanceTrend GetTrend(int sessionId, int windowSize = 100);
    std::vector<std::pair<int, float>> GetFPSTrend(int sessionId, int bucketSize = 100);

    // Bottleneck analysis
    std::vector<BottleneckInfo> GetBottlenecks(int sessionId, float thresholdPercent = 20.0f);
    BottleneckInfo GetWorstBottleneck(int sessionId);
    std::map<std::string, StageStatistics> GetAllStageStatistics(int sessionId);

    // Spike detection
    std::vector<FrameSpike> FindSpikes(int sessionId, float multiplier = 2.0f);
    std::vector<FrameSpike> FindWorstFrames(int sessionId, int count = 10);
    std::vector<FrameSpike> FindBestFrames(int sessionId, int count = 10);

    // Frame analysis
    FrameSpike AnalyzeFrame(int sessionId, int frameNumber);
    std::vector<int> FindFramesAboveThreshold(int sessionId, float thresholdMs);
    std::vector<int> FindFramesBelowFPS(int sessionId, float fpsThreshold);

    // Average calculations
    float GetAverageFPS(int sessionId);
    float GetAverageFPS(int sessionId, double startTime, double endTime);
    float GetAverageStageTime(int sessionId, const std::string& stageName);

    // Memory analysis
    float GetPeakGPUMemory(int sessionId);
    float GetPeakCPUMemory(int sessionId);
    float GetAverageGPUMemory(int sessionId);
    float GetAverageCPUMemory(int sessionId);
    std::vector<std::pair<int, float>> GetMemoryTrend(int sessionId);

    // GPU/CPU utilization analysis
    float GetAverageGPUUtilization(int sessionId);
    float GetAverageCPUUtilization(int sessionId);
    std::vector<std::pair<int, float>> GetGPUUtilizationTrend(int sessionId);
    std::vector<std::pair<int, float>> GetCPUUtilizationTrend(int sessionId);

    // Rendering stats analysis
    struct RenderStatsAverage {
        float avgDrawCalls = 0.0f;
        float avgTriangles = 0.0f;
        float avgVertices = 0.0f;
        float avgInstances = 0.0f;
        float avgLights = 0.0f;
        float avgShadowMaps = 0.0f;
    };

    RenderStatsAverage GetAverageRenderStats(int sessionId);

    // Performance score (0-100, higher is better)
    float CalculatePerformanceScore(int sessionId);

    // Regression detection
    bool HasPerformanceRegression(int baselineSessionId, int currentSessionId, float threshold = 5.0f);
    std::vector<std::string> GetRegressionDetails(int baselineSessionId, int currentSessionId);

    // Report generation
    std::string GenerateTextReport(int sessionId);
    std::string GenerateComparisonReport(int sessionA, int sessionB);

private:
    // Helper methods
    std::vector<float> GetFrameTimes(int sessionId);
    std::vector<float> GetFrameTimesInRange(int sessionId, double startTime, double endTime);
    float CalculateStandardDeviation(const std::vector<float>& values, float mean);
    float CalculatePercentile(std::vector<float>& values, float percentile);
    float CalculateTrendSlope(const std::vector<float>& values);

    StageStatistics CalculateStageStats(int sessionId, const std::string& stageName);

private:
    PerformanceDatabase* m_database;
};

} // namespace Profiling
} // namespace Engine
