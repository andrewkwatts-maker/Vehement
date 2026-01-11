#include "PerformanceAnalyzer.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace Engine {
namespace Profiling {

std::string PerformanceTrend::GetDirectionString() const {
    switch (direction) {
        case Direction::IMPROVING: return "Improving";
        case Direction::STABLE: return "Stable";
        case Direction::DEGRADING: return "Degrading";
        default: return "Unknown";
    }
}

PerformanceAnalyzer::PerformanceAnalyzer(PerformanceDatabase* database)
    : m_database(database) {
}

PerformanceAnalyzer::~PerformanceAnalyzer() {
}

FrameTimePercentiles PerformanceAnalyzer::GetPercentiles(int sessionId) {
    FrameTimePercentiles result;
    if (!m_database) return result;

    auto frameTimes = GetFrameTimes(sessionId);
    if (frameTimes.empty()) return result;

    // Sort for percentile calculation
    std::sort(frameTimes.begin(), frameTimes.end());

    result.min = frameTimes.front();
    result.max = frameTimes.back();
    result.p1 = CalculatePercentile(frameTimes, 0.01f);
    result.p5 = CalculatePercentile(frameTimes, 0.05f);
    result.p50 = CalculatePercentile(frameTimes, 0.50f);
    result.p95 = CalculatePercentile(frameTimes, 0.95f);
    result.p99 = CalculatePercentile(frameTimes, 0.99f);

    // Calculate mean
    result.mean = std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0f) / frameTimes.size();

    // Calculate standard deviation
    result.stdDev = CalculateStandardDeviation(frameTimes, result.mean);

    return result;
}

FrameTimePercentiles PerformanceAnalyzer::GetPercentilesInTimeRange(int sessionId, double startTime, double endTime) {
    FrameTimePercentiles result;
    if (!m_database) return result;

    auto frameTimes = GetFrameTimesInRange(sessionId, startTime, endTime);
    if (frameTimes.empty()) return result;

    std::sort(frameTimes.begin(), frameTimes.end());

    result.min = frameTimes.front();
    result.max = frameTimes.back();
    result.p1 = CalculatePercentile(frameTimes, 0.01f);
    result.p5 = CalculatePercentile(frameTimes, 0.05f);
    result.p50 = CalculatePercentile(frameTimes, 0.50f);
    result.p95 = CalculatePercentile(frameTimes, 0.95f);
    result.p99 = CalculatePercentile(frameTimes, 0.99f);
    result.mean = std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0f) / frameTimes.size();
    result.stdDev = CalculateStandardDeviation(frameTimes, result.mean);

    return result;
}

SessionComparison PerformanceAnalyzer::CompareSessions(int sessionA, int sessionB) {
    SessionComparison comparison;
    comparison.sessionA = sessionA;
    comparison.sessionB = sessionB;

    if (!m_database) return comparison;

    // Get statistics for both sessions
    auto statsA = m_database->GetStatistics(sessionA);
    auto statsB = m_database->GetStatistics(sessionB);

    // FPS comparison
    comparison.fpsA = statsA.avgFPS;
    comparison.fpsB = statsB.avgFPS;
    comparison.fpsDelta = statsB.avgFPS - statsA.avgFPS;
    if (statsA.avgFPS > 0.0f) {
        comparison.fpsPercentChange = (comparison.fpsDelta / statsA.avgFPS) * 100.0f;
    }

    // Frame time comparison
    comparison.frameTimeA = statsA.avgFrameTime;
    comparison.frameTimeB = statsB.avgFrameTime;
    comparison.frameTimeDelta = statsB.avgFrameTime - statsA.avgFrameTime;
    if (statsA.avgFrameTime > 0.0f) {
        comparison.frameTimePercentChange = (comparison.frameTimeDelta / statsA.avgFrameTime) * 100.0f;
    }

    // Stage comparisons
    auto stagesA = GetAllStageStatistics(sessionA);
    auto stagesB = GetAllStageStatistics(sessionB);

    for (const auto& pair : stagesA) {
        comparison.stageTimeDeltasA[pair.first] = pair.second.avgTimeMs;
    }

    for (const auto& pair : stagesB) {
        comparison.stageTimeDeltasB[pair.first] = pair.second.avgTimeMs;

        // Calculate delta if exists in A
        if (stagesA.find(pair.first) != stagesA.end()) {
            float delta = pair.second.avgTimeMs - stagesA[pair.first].avgTimeMs;
            comparison.stageDeltas[pair.first] = delta;
        }
    }

    // Memory comparison
    comparison.gpuMemoryA = GetPeakGPUMemory(sessionA);
    comparison.gpuMemoryB = GetPeakGPUMemory(sessionB);
    comparison.memoryDelta = comparison.gpuMemoryB - comparison.gpuMemoryA;

    return comparison;
}

PerformanceTrend PerformanceAnalyzer::GetTrend(int sessionId, int windowSize) {
    PerformanceTrend trend;
    if (!m_database) return trend;

    auto frameTimes = GetFrameTimes(sessionId);
    if (frameTimes.size() < windowSize) {
        trend.sampleCount = frameTimes.size();
        trend.direction = PerformanceTrend::Direction::STABLE;
        trend.confidence = 0.0f;
        return trend;
    }

    // Use the last 'windowSize' frames
    std::vector<float> recentFrames(frameTimes.end() - windowSize, frameTimes.end());

    trend.trendSlope = CalculateTrendSlope(recentFrames);
    trend.sampleCount = windowSize;

    // Determine direction
    // Negative slope means frame time is decreasing (performance improving)
    // Positive slope means frame time is increasing (performance degrading)
    if (trend.trendSlope < -0.01f) {
        trend.direction = PerformanceTrend::Direction::IMPROVING;
    } else if (trend.trendSlope > 0.01f) {
        trend.direction = PerformanceTrend::Direction::DEGRADING;
    } else {
        trend.direction = PerformanceTrend::Direction::STABLE;
    }

    // Confidence based on R-squared (simplified)
    trend.confidence = std::min(1.0f, std::abs(trend.trendSlope) * 10.0f);

    return trend;
}

std::vector<BottleneckInfo> PerformanceAnalyzer::GetBottlenecks(int sessionId, float thresholdPercent) {
    std::vector<BottleneckInfo> bottlenecks;
    if (!m_database) return bottlenecks;

    auto stageNames = m_database->GetBottleneckStages(sessionId, thresholdPercent);

    for (const auto& stageName : stageNames) {
        auto stats = CalculateStageStats(sessionId, stageName);

        BottleneckInfo info;
        info.stageName = stageName;
        info.averageTimeMs = stats.avgTimeMs;
        info.averagePercent = stats.avgPercent;
        info.maxTimeMs = stats.maxTimeMs;
        info.minTimeMs = stats.minTimeMs;
        info.occurrences = stats.sampleCount;

        bottlenecks.push_back(info);
    }

    // Sort by average time (descending)
    std::sort(bottlenecks.begin(), bottlenecks.end(),
              [](const BottleneckInfo& a, const BottleneckInfo& b) {
                  return a.averageTimeMs > b.averageTimeMs;
              });

    return bottlenecks;
}

BottleneckInfo PerformanceAnalyzer::GetWorstBottleneck(int sessionId) {
    auto bottlenecks = GetBottlenecks(sessionId, 0.0f);
    if (bottlenecks.empty()) {
        return BottleneckInfo();
    }
    return bottlenecks.front();
}

std::map<std::string, StageStatistics> PerformanceAnalyzer::GetAllStageStatistics(int sessionId) {
    std::map<std::string, StageStatistics> result;
    if (!m_database) return result;

    // Get a sample frame to find all stage names
    auto frames = m_database->GetFrames(sessionId, 1);
    if (frames.empty()) return result;

    auto stages = m_database->GetStages(frames[0].frameId);

    for (const auto& stage : stages) {
        result[stage.stageName] = CalculateStageStats(sessionId, stage.stageName);
    }

    return result;
}

std::vector<FrameSpike> PerformanceAnalyzer::FindSpikes(int sessionId, float multiplier) {
    std::vector<FrameSpike> spikes;
    if (!m_database) return spikes;

    auto stats = m_database->GetStatistics(sessionId);
    float threshold = stats.avgFrameTime * multiplier;

    auto spikeFrameNumbers = m_database->FindFrameSpikes(sessionId, multiplier);

    for (int frameNumber : spikeFrameNumbers) {
        spikes.push_back(AnalyzeFrame(sessionId, frameNumber));
    }

    return spikes;
}

std::vector<FrameSpike> PerformanceAnalyzer::FindWorstFrames(int sessionId, int count) {
    std::vector<FrameSpike> worstFrames;
    if (!m_database) return worstFrames;

    auto frames = m_database->GetSlowestFrames(sessionId, count);
    auto stats = m_database->GetStatistics(sessionId);

    for (const auto& frame : frames) {
        FrameSpike spike;
        spike.frameNumber = frame.frameNumber;
        spike.frameTimeMs = frame.totalTimeMs;
        spike.averageFrameTimeMs = stats.avgFrameTime;
        spike.multiplier = stats.avgFrameTime > 0.0f ? (frame.totalTimeMs / stats.avgFrameTime) : 0.0f;

        // Get stage breakdown
        auto stages = m_database->GetStages(frame.frameId);
        for (const auto& stage : stages) {
            spike.stageBreakdown.push_back({stage.stageName, stage.timeMs});
        }

        worstFrames.push_back(spike);
    }

    return worstFrames;
}

std::vector<FrameSpike> PerformanceAnalyzer::FindBestFrames(int sessionId, int count) {
    std::vector<FrameSpike> bestFrames;
    if (!m_database) return bestFrames;

    auto frames = m_database->GetFastestFrames(sessionId, count);
    auto stats = m_database->GetStatistics(sessionId);

    for (const auto& frame : frames) {
        FrameSpike spike;
        spike.frameNumber = frame.frameNumber;
        spike.frameTimeMs = frame.totalTimeMs;
        spike.averageFrameTimeMs = stats.avgFrameTime;
        spike.multiplier = stats.avgFrameTime > 0.0f ? (frame.totalTimeMs / stats.avgFrameTime) : 0.0f;

        auto stages = m_database->GetStages(frame.frameId);
        for (const auto& stage : stages) {
            spike.stageBreakdown.push_back({stage.stageName, stage.timeMs});
        }

        bestFrames.push_back(spike);
    }

    return bestFrames;
}

FrameSpike PerformanceAnalyzer::AnalyzeFrame(int sessionId, int frameNumber) {
    FrameSpike spike;
    if (!m_database) return spike;

    // Get frames and find the specific one
    QueryFilter filter;
    filter.sessionId = sessionId;
    filter.limit = 100000;

    auto frames = m_database->QueryFrames(filter);
    auto it = std::find_if(frames.begin(), frames.end(),
                          [frameNumber](const FrameData& f) { return f.frameNumber == frameNumber; });

    if (it == frames.end()) return spike;

    auto stats = m_database->GetStatistics(sessionId);

    spike.frameNumber = it->frameNumber;
    spike.frameTimeMs = it->totalTimeMs;
    spike.averageFrameTimeMs = stats.avgFrameTime;
    spike.multiplier = stats.avgFrameTime > 0.0f ? (it->totalTimeMs / stats.avgFrameTime) : 0.0f;

    // Get stage breakdown
    auto stages = m_database->GetStages(it->frameId);
    for (const auto& stage : stages) {
        spike.stageBreakdown.push_back({stage.stageName, stage.timeMs});
    }

    return spike;
}

float PerformanceAnalyzer::GetAverageFPS(int sessionId) {
    if (!m_database) return 0.0f;
    auto stats = m_database->GetStatistics(sessionId);
    return stats.avgFPS;
}

float PerformanceAnalyzer::GetAverageFPS(int sessionId, double startTime, double endTime) {
    if (!m_database) return 0.0f;
    auto stats = m_database->GetStatisticsInTimeRange(sessionId, startTime, endTime);
    return stats.avgFPS;
}

float PerformanceAnalyzer::GetAverageStageTime(int sessionId, const std::string& stageName) {
    if (!m_database) return 0.0f;
    return m_database->GetAverageStageTime(sessionId, stageName);
}

float PerformanceAnalyzer::GetPeakGPUMemory(int sessionId) {
    if (!m_database) return 0.0f;
    return m_database->GetPeakGPUMemory(sessionId);
}

float PerformanceAnalyzer::GetPeakCPUMemory(int sessionId) {
    if (!m_database) return 0.0f;
    return m_database->GetPeakCPUMemory(sessionId);
}

float PerformanceAnalyzer::CalculatePerformanceScore(int sessionId) {
    if (!m_database) return 0.0f;

    auto stats = m_database->GetStatistics(sessionId);

    // Score based on:
    // - Average FPS (40% weight)
    // - Frame time consistency/p99 (30% weight)
    // - Minimum FPS (30% weight)

    float fpsScore = std::min(100.0f, (stats.avgFPS / 60.0f) * 100.0f);
    float minFpsScore = std::min(100.0f, (stats.minFPS / 60.0f) * 100.0f);

    // Consistency score (lower p99 is better)
    float consistencyScore = 100.0f;
    if (stats.p99FrameTime > 0.0f) {
        float ratio = stats.avgFrameTime / stats.p99FrameTime;
        consistencyScore = std::min(100.0f, ratio * 100.0f);
    }

    float totalScore = (fpsScore * 0.4f) + (consistencyScore * 0.3f) + (minFpsScore * 0.3f);
    return totalScore;
}

bool PerformanceAnalyzer::HasPerformanceRegression(int baselineSessionId, int currentSessionId, float threshold) {
    if (!m_database) return false;

    auto comparison = CompareSessions(baselineSessionId, currentSessionId);

    // Check if FPS decreased by more than threshold percent
    return comparison.fpsPercentChange < -threshold;
}

std::string PerformanceAnalyzer::GenerateTextReport(int sessionId) {
    if (!m_database) return "";

    std::ostringstream report;
    auto sessionInfo = m_database->GetSessionInfo(sessionId);
    auto stats = m_database->GetStatistics(sessionId);
    auto percentiles = GetPercentiles(sessionId);
    auto bottlenecks = GetBottlenecks(sessionId, 15.0f);
    auto trend = GetTrend(sessionId);

    report << "=== Performance Report ===\n\n";
    report << "Session ID: " << sessionId << "\n";
    report << "Start Time: " << sessionInfo.startTime << "\n";
    report << "Hardware: " << sessionInfo.hardwareConfig.cpuModel << " / "
           << sessionInfo.hardwareConfig.gpuModel << "\n";
    report << "Settings: " << sessionInfo.qualityPreset << " @ " << sessionInfo.resolution << "\n\n";

    report << "--- Frame Statistics ---\n";
    report << "Total Frames: " << stats.totalFrames << "\n";
    report << "Average FPS: " << std::fixed << std::setprecision(2) << stats.avgFPS << "\n";
    report << "Min FPS: " << stats.minFPS << "\n";
    report << "Max FPS: " << stats.maxFPS << "\n";
    report << "Average Frame Time: " << stats.avgFrameTime << " ms\n\n";

    report << "--- Frame Time Percentiles ---\n";
    report << "P50 (Median): " << percentiles.p50 << " ms\n";
    report << "P95: " << percentiles.p95 << " ms\n";
    report << "P99: " << percentiles.p99 << " ms\n";
    report << "Std Dev: " << percentiles.stdDev << " ms\n\n";

    report << "--- Performance Trend ---\n";
    report << "Direction: " << trend.GetDirectionString() << "\n";
    report << "Confidence: " << (trend.confidence * 100.0f) << "%\n\n";

    if (!bottlenecks.empty()) {
        report << "--- Bottlenecks (>15% of frame time) ---\n";
        for (const auto& bottleneck : bottlenecks) {
            report << bottleneck.stageName << ": " << bottleneck.averageTimeMs << " ms ("
                   << bottleneck.averagePercent << "%)\n";
        }
        report << "\n";
    }

    report << "--- Performance Score ---\n";
    report << CalculatePerformanceScore(sessionId) << " / 100\n";

    return report.str();
}

std::vector<float> PerformanceAnalyzer::GetFrameTimes(int sessionId) {
    std::vector<float> frameTimes;
    if (!m_database) return frameTimes;

    auto frames = m_database->GetFrames(sessionId, 100000);
    for (const auto& frame : frames) {
        frameTimes.push_back(frame.totalTimeMs);
    }

    return frameTimes;
}

std::vector<float> PerformanceAnalyzer::GetFrameTimesInRange(int sessionId, double startTime, double endTime) {
    std::vector<float> frameTimes;
    if (!m_database) return frameTimes;

    auto frames = m_database->GetFramesInTimeRange(sessionId, startTime, endTime);
    for (const auto& frame : frames) {
        frameTimes.push_back(frame.totalTimeMs);
    }

    return frameTimes;
}

float PerformanceAnalyzer::CalculateStandardDeviation(const std::vector<float>& values, float mean) {
    if (values.empty()) return 0.0f;

    float variance = 0.0f;
    for (float value : values) {
        float diff = value - mean;
        variance += diff * diff;
    }
    variance /= values.size();

    return std::sqrt(variance);
}

float PerformanceAnalyzer::CalculatePercentile(std::vector<float>& values, float percentile) {
    if (values.empty()) return 0.0f;

    // Assumes values are already sorted
    size_t index = static_cast<size_t>(percentile * (values.size() - 1));
    return values[index];
}

float PerformanceAnalyzer::CalculateTrendSlope(const std::vector<float>& values) {
    if (values.size() < 2) return 0.0f;

    // Simple linear regression
    float n = static_cast<float>(values.size());
    float sumX = 0.0f, sumY = 0.0f, sumXY = 0.0f, sumX2 = 0.0f;

    for (size_t i = 0; i < values.size(); ++i) {
        float x = static_cast<float>(i);
        float y = values[i];

        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    float slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    return slope;
}

StageStatistics PerformanceAnalyzer::CalculateStageStats(int sessionId, const std::string& stageName) {
    StageStatistics stats;
    stats.stageName = stageName;

    if (!m_database) return stats;

    auto timings = m_database->GetStageTimings(sessionId, stageName, 100000);
    if (timings.empty()) return stats;

    stats.sampleCount = timings.size();
    stats.avgTimeMs = std::accumulate(timings.begin(), timings.end(), 0.0f) / timings.size();
    stats.minTimeMs = *std::min_element(timings.begin(), timings.end());
    stats.maxTimeMs = *std::max_element(timings.begin(), timings.end());

    return stats;
}

} // namespace Profiling
} // namespace Engine
