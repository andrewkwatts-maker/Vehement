#include "SDFPerformanceProfiler.hpp"
#include <glad/gl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace Nova {

SDFPerformanceProfiler::SDFPerformanceProfiler() {
    m_frameHistory.resize(HISTORY_SIZE);
}

SDFPerformanceProfiler::~SDFPerformanceProfiler() {
    Shutdown();
}

// =============================================================================
// Initialization
// =============================================================================

bool SDFPerformanceProfiler::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Create GPU timing queries
    if (!CreateTimingQueries()) {
        return false;
    }

    // Initialize debug visualization texture (1920x1080 RGBA16F)
    glGenTextures(1, &m_debugTexture);
    glBindTexture(GL_TEXTURE_2D, m_debugTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1920, 1080, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create FBO for debug rendering
    glGenFramebuffers(1, &m_debugFBO);

    if (m_debugTexture == 0 || m_debugFBO == 0) {
        Shutdown();
        return false;
    }

    m_initialized = true;
    return true;
}

void SDFPerformanceProfiler::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Delete timing queries
    for (auto& query : m_timingQueries) {
        if (query.queryID != 0) {
            glDeleteQueries(1, &query.queryID);
            query.queryID = 0;
        }
    }
    m_timingQueries.clear();

    // Delete debug resources
    if (m_debugTexture != 0) {
        glDeleteTextures(1, &m_debugTexture);
        m_debugTexture = 0;
    }

    if (m_debugFBO != 0) {
        glDeleteFramebuffers(1, &m_debugFBO);
        m_debugFBO = 0;
    }

    m_initialized = false;
}

// =============================================================================
// GPU Timing
// =============================================================================

bool SDFPerformanceProfiler::CreateTimingQueries() {
    m_timingQueries.resize(MAX_QUERIES);

    for (int i = 0; i < MAX_QUERIES; ++i) {
        glGenQueries(1, &m_timingQueries[i].queryID);
        if (m_timingQueries[i].queryID == 0) {
            return false;
        }
        m_timingQueries[i].active = false;
    }

    return true;
}

void SDFPerformanceProfiler::BeginGPUPass(const std::string& passName) {
    if (!m_initialized) {
        return;
    }

    // Find free query
    for (int i = 0; i < MAX_QUERIES; ++i) {
        if (!m_timingQueries[i].active) {
            m_currentQueryIndex = i;
            m_timingQueries[i].name = passName;
            m_timingQueries[i].active = true;

            glBeginQuery(GL_TIME_ELAPSED, m_timingQueries[i].queryID);
            return;
        }
    }

    // No free queries available
    m_currentQueryIndex = -1;
}

void SDFPerformanceProfiler::EndGPUPass() {
    if (!m_initialized || m_currentQueryIndex < 0) {
        return;
    }

    glEndQuery(GL_TIME_ELAPSED);
    m_currentQueryIndex = -1;
}

void SDFPerformanceProfiler::CollectGPUTimings() {
    if (!m_initialized) {
        return;
    }

    // Collect results from all active queries
    for (auto& query : m_timingQueries) {
        if (!query.active) {
            continue;
        }

        // Check if query result is available
        GLint available = 0;
        glGetQueryObjectiv(query.queryID, GL_QUERY_RESULT_AVAILABLE, &available);

        if (available) {
            GLuint64 timeNs = 0;
            glGetQueryObjectui64v(query.queryID, GL_QUERY_RESULT, &timeNs);

            query.timeMs = static_cast<float>(timeNs) / 1000000.0f;
            query.active = false;

            // Update corresponding performance data field
            if (query.name == "Culling") {
                m_currentData.cullingTimeMs = query.timeMs;
            } else if (query.name == "Raymarching") {
                m_currentData.raymarchTimeMs = query.timeMs;
            } else if (query.name == "Temporal") {
                m_currentData.temporalTimeMs = query.timeMs;
            } else if (query.name == "Reconstruction") {
                m_currentData.reconstructionTimeMs = query.timeMs;
            }
        }
    }

    // Calculate total SDF render time
    m_currentData.sdfRenderTimeMs =
        m_currentData.cullingTimeMs +
        m_currentData.raymarchTimeMs +
        m_currentData.temporalTimeMs +
        m_currentData.reconstructionTimeMs;
}

// =============================================================================
// Statistics Collection
// =============================================================================

void SDFPerformanceProfiler::UpdateFrameStats(const SDFPerformanceData& data) {
    m_currentData = data;
    m_currentData.frameNumber = m_frameCounter++;

    // Calculate derived statistics
    if (m_currentData.totalPixels > 0) {
        m_currentData.reprojectionRate =
            static_cast<float>(m_currentData.reprojectedPixels) /
            static_cast<float>(m_currentData.totalPixels);
    }

    if (m_currentData.tracedPixels > 0 && m_currentData.totalRaySteps > 0) {
        m_currentData.avgStepsPerRay =
            static_cast<float>(m_currentData.totalRaySteps) /
            static_cast<float>(m_currentData.tracedPixels);
    }

    if (m_currentData.cachedInstances > 0) {
        uint32_t totalCacheAccess = m_currentData.brickCacheHits + m_currentData.brickCacheMisses;
        if (totalCacheAccess > 0) {
            m_currentData.cacheHitRate =
                static_cast<float>(m_currentData.brickCacheHits) /
                static_cast<float>(totalCacheAccess);
        }
    }

    // Store in history
    m_frameHistory[m_historyIndex] = m_currentData;
    m_historyIndex = (m_historyIndex + 1) % HISTORY_SIZE;

    // Update averaged statistics
    UpdateAveragedStats();

    // Update adaptive quality if enabled
    if (m_adaptiveQualityEnabled) {
        m_currentQualityScale = CalculateQualityScale(
            m_currentData.totalFrameTimeMs,
            m_targetFrameTimeMs
        );
        m_currentData.qualityScale = m_currentQualityScale;
    }

    // Update debug visualization
    if (m_visualizationMode != SDFVisualizationMode::None) {
        UpdateDebugVisualization();
    }
}

void SDFPerformanceProfiler::UpdateAveragedStats() {
    // Calculate rolling average over frame history
    SDFPerformanceData sum = {};
    int validFrames = std::min(static_cast<int>(m_frameCounter), HISTORY_SIZE);

    for (int i = 0; i < validFrames; ++i) {
        const auto& frame = m_frameHistory[i];
        sum.totalFrameTimeMs += frame.totalFrameTimeMs;
        sum.sdfRenderTimeMs += frame.sdfRenderTimeMs;
        sum.cullingTimeMs += frame.cullingTimeMs;
        sum.raymarchTimeMs += frame.raymarchTimeMs;
        sum.temporalTimeMs += frame.temporalTimeMs;
        sum.reconstructionTimeMs += frame.reconstructionTimeMs;
        sum.avgStepsPerRay += frame.avgStepsPerRay;
        sum.reprojectionRate += frame.reprojectionRate;
        sum.cacheHitRate += frame.cacheHitRate;
    }

    if (validFrames > 0) {
        float invCount = 1.0f / static_cast<float>(validFrames);
        m_averagedData.totalFrameTimeMs = sum.totalFrameTimeMs * invCount;
        m_averagedData.sdfRenderTimeMs = sum.sdfRenderTimeMs * invCount;
        m_averagedData.cullingTimeMs = sum.cullingTimeMs * invCount;
        m_averagedData.raymarchTimeMs = sum.raymarchTimeMs * invCount;
        m_averagedData.temporalTimeMs = sum.temporalTimeMs * invCount;
        m_averagedData.reconstructionTimeMs = sum.reconstructionTimeMs * invCount;
        m_averagedData.avgStepsPerRay = sum.avgStepsPerRay * invCount;
        m_averagedData.reprojectionRate = sum.reprojectionRate * invCount;
        m_averagedData.cacheHitRate = sum.cacheHitRate * invCount;
    }

    // Copy non-averaged fields from current frame
    m_averagedData.totalInstances = m_currentData.totalInstances;
    m_averagedData.visibleInstances = m_currentData.visibleInstances;
    m_averagedData.culledInstances = m_currentData.culledInstances;
    m_averagedData.qualityScale = m_currentData.qualityScale;
}

// =============================================================================
// Adaptive Quality Scaling
// =============================================================================

float SDFPerformanceProfiler::GetRecommendedQualityScale() const {
    return m_currentQualityScale;
}

float SDFPerformanceProfiler::CalculateQualityScale(float currentFrameTime, float targetFrameTime) {
    // PID-like controller for smooth quality scaling
    const float kP = 0.02f;   // Proportional gain
    const float kD = 0.01f;   // Derivative gain

    float error = targetFrameTime - currentFrameTime;
    float derivative = error - m_qualityScaleVelocity;

    // Calculate adjustment
    float adjustment = kP * error + kD * derivative;

    // Update velocity
    m_qualityScaleVelocity = error;

    // Apply adjustment with clamping
    float newScale = m_currentQualityScale + adjustment;
    newScale = std::clamp(newScale, 0.25f, 1.0f);  // 25% to 100%

    // Quantize to common resolutions for stability
    if (newScale > 0.9f) {
        return 1.0f;  // Full resolution
    } else if (newScale > 0.65f) {
        return 0.75f;  // 75%
    } else if (newScale > 0.4f) {
        return 0.5f;   // Half resolution
    } else {
        return 0.25f;  // Quarter resolution
    }
}

void SDFPerformanceProfiler::UpdateDebugVisualization() {
    // NOTE: In a full implementation, this would render debug overlays
    // to the debug texture based on m_visualizationMode
    // For now, this is a stub
}

// =============================================================================
// Reporting
// =============================================================================

std::string SDFPerformanceProfiler::GetPerformanceSummary() const {
    std::ostringstream oss;

    oss << std::fixed << std::setprecision(2);
    oss << "=== SDF Performance Summary ===\n";
    oss << "Frame Time: " << m_averagedData.totalFrameTimeMs << " ms ";
    oss << "(" << (1000.0f / m_averagedData.totalFrameTimeMs) << " FPS)\n";
    oss << "SDF Render: " << m_averagedData.sdfRenderTimeMs << " ms\n";
    oss << "  - Culling: " << m_averagedData.cullingTimeMs << " ms\n";
    oss << "  - Raymarching: " << m_averagedData.raymarchTimeMs << " ms\n";
    oss << "  - Temporal: " << m_averagedData.temporalTimeMs << " ms\n";
    oss << "  - Reconstruction: " << m_averagedData.reconstructionTimeMs << " ms\n";
    oss << "\nInstances: " << m_currentData.visibleInstances << " / "
        << m_currentData.totalInstances << " visible\n";
    oss << "Culled: " << m_currentData.culledInstances << "\n";
    oss << "Avg Steps/Ray: " << m_averagedData.avgStepsPerRay << "\n";
    oss << "Reprojection Rate: " << (m_averagedData.reprojectionRate * 100.0f) << "%\n";
    oss << "Cache Hit Rate: " << (m_averagedData.cacheHitRate * 100.0f) << "%\n";
    oss << "Quality Scale: " << (m_currentData.qualityScale * 100.0f) << "%\n";

    return oss.str();
}

void SDFPerformanceProfiler::PrintPerformanceReport() const {
    std::cout << GetPerformanceSummary() << std::endl;
}

bool SDFPerformanceProfiler::ExportToCSV(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    // CSV header
    file << "Frame,TotalTime,SDFTime,Culling,Raymarching,Temporal,Reconstruction,";
    file << "Instances,Visible,Culled,AvgSteps,ReprojRate,CacheHitRate,QualityScale\n";

    // Export frame history
    int validFrames = std::min(static_cast<int>(m_frameCounter), HISTORY_SIZE);
    for (int i = 0; i < validFrames; ++i) {
        const auto& frame = m_frameHistory[i];

        file << frame.frameNumber << ","
             << frame.totalFrameTimeMs << ","
             << frame.sdfRenderTimeMs << ","
             << frame.cullingTimeMs << ","
             << frame.raymarchTimeMs << ","
             << frame.temporalTimeMs << ","
             << frame.reconstructionTimeMs << ","
             << frame.totalInstances << ","
             << frame.visibleInstances << ","
             << frame.culledInstances << ","
             << frame.avgStepsPerRay << ","
             << frame.reprojectionRate << ","
             << frame.cacheHitRate << ","
             << frame.qualityScale << "\n";
    }

    file.close();
    return true;
}

} // namespace Nova
