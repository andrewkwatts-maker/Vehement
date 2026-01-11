#include "MassiveSceneProfiler.hpp"
#include <glad/gl.h>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iostream>
#include <iomanip>

namespace Engine {
namespace Graphics {

// ============================================================================
// PerformanceCounter Implementation
// ============================================================================

PerformanceCounter::PerformanceCounter(const std::string& name, size_t maxSamples)
    : m_name(name)
    , m_maxSamples(maxSamples) {
    m_samples.reserve(maxSamples);
}

void PerformanceCounter::AddSample(float value, uint64_t frameIndex) {
    m_samples.push_back(PerformanceSample(value, frameIndex));

    if (m_samples.size() > m_maxSamples) {
        m_samples.erase(m_samples.begin());
    }
}

float PerformanceCounter::GetAverage() const {
    if (m_samples.empty()) return 0.0f;

    float sum = 0.0f;
    for (const auto& sample : m_samples) {
        sum += sample.timeMs;
    }
    return sum / m_samples.size();
}

float PerformanceCounter::GetMin() const {
    if (m_samples.empty()) return 0.0f;

    float minVal = m_samples[0].timeMs;
    for (const auto& sample : m_samples) {
        minVal = std::min(minVal, sample.timeMs);
    }
    return minVal;
}

float PerformanceCounter::GetMax() const {
    if (m_samples.empty()) return 0.0f;

    float maxVal = m_samples[0].timeMs;
    for (const auto& sample : m_samples) {
        maxVal = std::max(maxVal, sample.timeMs);
    }
    return maxVal;
}

float PerformanceCounter::GetLatest() const {
    if (m_samples.empty()) return 0.0f;
    return m_samples.back().timeMs;
}

// ============================================================================
// MassiveSceneProfiler Implementation
// ============================================================================

MassiveSceneProfiler::MassiveSceneProfiler()
    : m_currentFrame(0)
    , m_frameTimes("FrameTime", 120) {
}

MassiveSceneProfiler::~MassiveSceneProfiler() {
    // Delete GPU queries
    for (auto& pair : m_queryObjects) {
        if (pair.second) {
            glDeleteQueries(1, &pair.second);
        }
    }
}

bool MassiveSceneProfiler::Initialize() {
    // Create performance counters for each category
    m_counters[Category::CPUCulling] = std::make_unique<PerformanceCounter>("CPU Culling", 120);
    m_counters[Category::GPUCulling] = std::make_unique<PerformanceCounter>("GPU Culling", 120);
    m_counters[Category::LightClustering] = std::make_unique<PerformanceCounter>("Light Clustering", 120);
    m_counters[Category::GBufferPass] = std::make_unique<PerformanceCounter>("G-Buffer Pass", 120);
    m_counters[Category::LightingPass] = std::make_unique<PerformanceCounter>("Lighting Pass", 120);
    m_counters[Category::ShadowMapping] = std::make_unique<PerformanceCounter>("Shadow Mapping", 120);
    m_counters[Category::TerrainRendering] = std::make_unique<PerformanceCounter>("Terrain Rendering", 120);
    m_counters[Category::Total] = std::make_unique<PerformanceCounter>("Total Frame", 120);

    // Create GPU query objects for each category
    for (auto& pair : m_counters) {
        uint32_t query;
        glGenQueries(1, &query);
        m_queryObjects[pair.first] = query;
    }

    return true;
}

void MassiveSceneProfiler::BeginFrame(uint64_t frameIndex) {
    m_currentFrame = frameIndex;
    m_frameStartTime = std::chrono::high_resolution_clock::now();
}

void MassiveSceneProfiler::BeginCategory(Category category) {
    m_categoryStartTimes[category] = std::chrono::high_resolution_clock::now();

    // Start GPU query if available
    auto it = m_queryObjects.find(category);
    if (it != m_queryObjects.end() && it->second) {
        glBeginQuery(GL_TIME_ELAPSED, it->second);
    }
}

void MassiveSceneProfiler::EndCategory(Category category) {
    // End GPU query
    auto queryIt = m_queryObjects.find(category);
    if (queryIt != m_queryObjects.end() && queryIt->second) {
        glEndQuery(GL_TIME_ELAPSED);
    }

    // Calculate CPU time
    auto it = m_categoryStartTimes.find(category);
    if (it != m_categoryStartTimes.end()) {
        auto endTime = std::chrono::high_resolution_clock::now();
        float cpuTimeMs = std::chrono::duration<float, std::milli>(endTime - it->second).count();

        // Read GPU query result if available
        GLint available = 0;
        if (queryIt != m_queryObjects.end() && queryIt->second) {
            glGetQueryObjectiv(queryIt->second, GL_QUERY_RESULT_AVAILABLE, &available);

            if (available) {
                GLuint64 gpuTime = 0;
                glGetQueryObjectui64v(queryIt->second, GL_QUERY_RESULT, &gpuTime);
                float gpuTimeMs = gpuTime / 1000000.0f;

                // Use max of CPU and GPU time
                float timeMs = std::max(cpuTimeMs, gpuTimeMs);

                auto counterIt = m_counters.find(category);
                if (counterIt != m_counters.end()) {
                    counterIt->second->AddSample(timeMs, m_currentFrame);
                }
            } else {
                // GPU query not ready, use CPU time
                auto counterIt = m_counters.find(category);
                if (counterIt != m_counters.end()) {
                    counterIt->second->AddSample(cpuTimeMs, m_currentFrame);
                }
            }
        } else {
            // No GPU query, use CPU time
            auto counterIt = m_counters.find(category);
            if (counterIt != m_counters.end()) {
                counterIt->second->AddSample(cpuTimeMs, m_currentFrame);
            }
        }
    }
}

void MassiveSceneProfiler::EndFrame() {
    auto endTime = std::chrono::high_resolution_clock::now();
    float frameTimeMs = std::chrono::duration<float, std::milli>(endTime - m_frameStartTime).count();

    m_frameTimes.AddSample(frameTimeMs, m_currentFrame);

    // Also record in Total category
    auto it = m_counters.find(Category::Total);
    if (it != m_counters.end()) {
        it->second->AddSample(frameTimeMs, m_currentFrame);
    }
}

PerformanceCounter* MassiveSceneProfiler::GetCounter(Category category) {
    auto it = m_counters.find(category);
    if (it != m_counters.end()) {
        return it->second.get();
    }
    return nullptr;
}

MassiveSceneProfiler::Report MassiveSceneProfiler::GenerateReport() const {
    Report report;

    // Get averages
    report.avgFrameTimeMs = m_frameTimes.GetAverage();
    report.avgCPUCullingMs = m_counters.at(Category::CPUCulling)->GetAverage();
    report.avgGPUCullingMs = m_counters.at(Category::GPUCulling)->GetAverage();
    report.avgLightClusteringMs = m_counters.at(Category::LightClustering)->GetAverage();
    report.avgGBufferMs = m_counters.at(Category::GBufferPass)->GetAverage();
    report.avgLightingMs = m_counters.at(Category::LightingPass)->GetAverage();
    report.avgShadowMs = m_counters.at(Category::ShadowMapping)->GetAverage();
    report.avgTerrainMs = m_counters.at(Category::TerrainRendering)->GetAverage();

    // Calculate FPS
    if (report.avgFrameTimeMs > 0) {
        report.actualFPS = 1000.0f / report.avgFrameTimeMs;
    }

    report.targetFPS = 60.0f;

    // Detect bottleneck
    const_cast<MassiveSceneProfiler*>(this)->DetectBottleneck(report);

    return report;
}

void MassiveSceneProfiler::DetectBottleneck(Report& report) const {
    // Find the most expensive category
    struct CategoryTime {
        std::string name;
        float timeMs;
    };

    std::vector<CategoryTime> times;
    times.push_back({"CPU Culling", report.avgCPUCullingMs});
    times.push_back({"GPU Culling", report.avgGPUCullingMs});
    times.push_back({"Light Clustering", report.avgLightClusteringMs});
    times.push_back({"G-Buffer Pass", report.avgGBufferMs});
    times.push_back({"Lighting Pass", report.avgLightingMs});
    times.push_back({"Shadow Mapping", report.avgShadowMs});
    times.push_back({"Terrain Rendering", report.avgTerrainMs});

    std::sort(times.begin(), times.end(),
        [](const CategoryTime& a, const CategoryTime& b) {
            return a.timeMs > b.timeMs;
        }
    );

    if (!times.empty()) {
        report.bottleneck = times[0].name;
    }

    // Determine if CPU or GPU bound
    float cpuTotal = report.avgCPUCullingMs;
    float gpuTotal = report.avgGPUCullingMs + report.avgGBufferMs +
                     report.avgLightingMs + report.avgShadowMs + report.avgTerrainMs;

    if (cpuTotal > gpuTotal * 1.2f) {
        report.isCPUBound = true;
    } else if (gpuTotal > cpuTotal * 1.2f) {
        report.isGPUBound = true;
    }
}

void MassiveSceneProfiler::PrintReport() const {
    Report report = GenerateReport();

    std::cout << "\n=== Massive Scene Performance Report ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);

    std::cout << "\nFrame Timing:" << std::endl;
    std::cout << "  Average Frame Time: " << report.avgFrameTimeMs << " ms" << std::endl;
    std::cout << "  Target FPS:         " << report.targetFPS << std::endl;
    std::cout << "  Actual FPS:         " << report.actualFPS << std::endl;

    std::cout << "\nCulling:" << std::endl;
    std::cout << "  CPU Culling:        " << report.avgCPUCullingMs << " ms" << std::endl;
    std::cout << "  GPU Culling:        " << report.avgGPUCullingMs << " ms" << std::endl;

    std::cout << "\nLighting:" << std::endl;
    std::cout << "  Light Clustering:   " << report.avgLightClusteringMs << " ms" << std::endl;
    std::cout << "  Lighting Pass:      " << report.avgLightingMs << " ms" << std::endl;
    std::cout << "  Shadow Mapping:     " << report.avgShadowMs << " ms" << std::endl;

    std::cout << "\nRendering:" << std::endl;
    std::cout << "  G-Buffer Pass:      " << report.avgGBufferMs << " ms" << std::endl;
    std::cout << "  Terrain Rendering:  " << report.avgTerrainMs << " ms" << std::endl;

    std::cout << "\nBottleneck Analysis:" << std::endl;
    std::cout << "  Primary Bottleneck: " << report.bottleneck << std::endl;
    std::cout << "  CPU Bound:          " << (report.isCPUBound ? "Yes" : "No") << std::endl;
    std::cout << "  GPU Bound:          " << (report.isGPUBound ? "Yes" : "No") << std::endl;

    std::cout << "\n========================================\n" << std::endl;
}

bool MassiveSceneProfiler::ExportReport(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    Report report = GenerateReport();

    file << "Massive Scene Performance Report\n";
    file << "================================\n\n";

    file << "Frame Timing:\n";
    file << "  Average Frame Time: " << report.avgFrameTimeMs << " ms\n";
    file << "  Actual FPS:         " << report.actualFPS << "\n\n";

    file << "Culling:\n";
    file << "  CPU Culling:        " << report.avgCPUCullingMs << " ms\n";
    file << "  GPU Culling:        " << report.avgGPUCullingMs << " ms\n\n";

    file << "Lighting:\n";
    file << "  Light Clustering:   " << report.avgLightClusteringMs << " ms\n";
    file << "  Lighting Pass:      " << report.avgLightingMs << " ms\n";
    file << "  Shadow Mapping:     " << report.avgShadowMs << " ms\n\n";

    file << "Rendering:\n";
    file << "  G-Buffer Pass:      " << report.avgGBufferMs << " ms\n";
    file << "  Terrain Rendering:  " << report.avgTerrainMs << " ms\n\n";

    file << "Bottleneck:           " << report.bottleneck << "\n";

    file.close();
    return true;
}

void MassiveSceneProfiler::Reset() {
    for (auto& pair : m_counters) {
        // Can't easily reset, but samples will age out
    }
}

std::string MassiveSceneProfiler::CategoryToString(Category category) const {
    switch (category) {
        case Category::CPUCulling: return "CPU Culling";
        case Category::GPUCulling: return "GPU Culling";
        case Category::LightClustering: return "Light Clustering";
        case Category::GBufferPass: return "G-Buffer Pass";
        case Category::LightingPass: return "Lighting Pass";
        case Category::ShadowMapping: return "Shadow Mapping";
        case Category::TerrainRendering: return "Terrain Rendering";
        case Category::Total: return "Total Frame";
        default: return "Unknown";
    }
}

} // namespace Graphics
} // namespace Engine
