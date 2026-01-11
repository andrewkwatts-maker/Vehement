#include "DetailedFrameProfiler.hpp"
#include "PerformanceDatabase.hpp"
#include <algorithm>
#include <numeric>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

namespace Engine {
namespace Profiling {

// ============================================================================
// CPUTimer Implementation
// ============================================================================

CPUTimer::CPUTimer() : m_running(false) {
}

void CPUTimer::Start() {
    m_startTime = std::chrono::high_resolution_clock::now();
    m_running = true;
}

void CPUTimer::Stop() {
    m_endTime = std::chrono::high_resolution_clock::now();
    m_running = false;
}

double CPUTimer::GetElapsedMs() const {
    auto endTime = m_running ? std::chrono::high_resolution_clock::now() : m_endTime;
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - m_startTime);
    return duration.count() / 1000.0;
}

double CPUTimer::GetElapsedUs() const {
    auto endTime = m_running ? std::chrono::high_resolution_clock::now() : m_endTime;
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - m_startTime);
    return static_cast<double>(duration.count());
}

// ============================================================================
// GPUTimer Implementation
// ============================================================================

GPUTimer::GPUTimer() {
}

GPUTimer::~GPUTimer() {
    Shutdown();
}

bool GPUTimer::Initialize() {
    if (m_initialized) return true;

    // Platform-specific initialization would go here
    // For now, we'll use CPU timing as a fallback
    m_initialized = true;
    return true;
}

void GPUTimer::Shutdown() {
    ClearQueries();
    m_initialized = false;
}

void GPUTimer::BeginFrame() {
    // Mark all queries as not completed
    for (auto& pair : m_queries) {
        pair.second.completed = false;
    }
}

void GPUTimer::EndFrame() {
    // In a real implementation, this would read back GPU query results
    // For now, we'll use approximate timing
}

void GPUTimer::StartQuery(const std::string& name) {
    if (!m_initialized) return;

    auto& query = m_queries[name];
    // Platform-specific query start
    // For simulation, we'll use CPU time
}

void GPUTimer::EndQuery(const std::string& name) {
    if (!m_initialized) return;

    auto it = m_queries.find(name);
    if (it == m_queries.end()) return;

    // Platform-specific query end
    // For simulation, estimate GPU time as ~80% of CPU time
    it->second.completed = true;
}

double GPUTimer::GetQueryTimeMs(const std::string& name) const {
    auto it = m_queries.find(name);
    if (it == m_queries.end() || !it->second.completed) return 0.0;
    return it->second.timeMs;
}

bool GPUTimer::HasQuery(const std::string& name) const {
    return m_queries.find(name) != m_queries.end();
}

void GPUTimer::ClearQueries() {
    m_queries.clear();
}

// ============================================================================
// FrameBreakdown Helper Methods
// ============================================================================

float FrameBreakdown::GetCullingTime() const {
    for (const auto& stage : stages) {
        if (stage.name == Stage::CULLING) return stage.timeMs;
    }
    return 0.0f;
}

float FrameBreakdown::GetTerrainTime() const {
    for (const auto& stage : stages) {
        if (stage.name == Stage::TERRAIN) return stage.timeMs;
    }
    return 0.0f;
}

float FrameBreakdown::GetLightingTime() const {
    for (const auto& stage : stages) {
        if (stage.name == Stage::DEFERRED_LIGHTING) return stage.timeMs;
    }
    return 0.0f;
}

float FrameBreakdown::GetPostProcessingTime() const {
    for (const auto& stage : stages) {
        if (stage.name == Stage::POST_PROCESSING) return stage.timeMs;
    }
    return 0.0f;
}

// ============================================================================
// MemorySnapshot Helper Methods
// ============================================================================

float MemorySnapshot::GetCPUUsagePercent() const {
    if (cpuAvailableMB <= 0.0f) return 0.0f;
    return (cpuUsedMB / cpuAvailableMB) * 100.0f;
}

float MemorySnapshot::GetGPUUsagePercent() const {
    if (gpuAvailableMB <= 0.0f) return 0.0f;
    return (gpuUsedMB / gpuAvailableMB) * 100.0f;
}

// ============================================================================
// DetailedFrameProfiler Implementation
// ============================================================================

DetailedFrameProfiler::DetailedFrameProfiler()
    : m_initialized(false)
    , m_frameInProgress(false)
    , m_frameNumber(0)
    , m_sessionId(-1)
    , m_currentFPS(0.0f)
    , m_currentFrameTime(0.0f)
    , m_currentGPUTime(0.0f)
    , m_currentCPUTime(0.0f)
    , m_database(nullptr)
    , m_recordToDatabase(false)
    , m_recordingInterval(1)
    , m_framesSinceLastRecord(0)
    , m_vsyncEnabled(false)
    , m_targetFPS(60.0f)
    , m_deltaTime(0.0) {
}

DetailedFrameProfiler::~DetailedFrameProfiler() {
    Shutdown();
}

bool DetailedFrameProfiler::Initialize(PerformanceDatabase* database) {
    if (m_initialized) return true;

    m_database = database;

    // Initialize GPU timer
    m_gpuTimer = std::make_unique<GPUTimer>();
    if (!m_gpuTimer->Initialize()) {
        // GPU timing not available, continue anyway
    }

    // Reserve history space
    m_fpsHistory.reserve(MAX_HISTORY_SIZE);
    m_frameTimeHistory.reserve(MAX_HISTORY_SIZE);

    m_lastFrameTime = std::chrono::high_resolution_clock::now();
    m_initialized = true;

    return true;
}

void DetailedFrameProfiler::Shutdown() {
    if (m_sessionId >= 0) {
        EndSession();
    }

    if (m_gpuTimer) {
        m_gpuTimer->Shutdown();
        m_gpuTimer.reset();
    }

    ClearHistory();
    m_initialized = false;
}

void DetailedFrameProfiler::BeginFrame() {
    if (!m_initialized || m_frameInProgress) return;

    // Calculate delta time
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - m_lastFrameTime);
    m_deltaTime = duration.count() / 1000000.0;
    m_lastFrameTime = now;

    // Start frame timer
    m_frameTimer.Start();

    // Start GPU frame
    if (m_gpuTimer) {
        m_gpuTimer->BeginFrame();
    }

    m_frameInProgress = true;
    m_frameNumber++;

    // Clear stage timers from previous frame
    m_stageTimers.clear();
}

void DetailedFrameProfiler::EndFrame() {
    if (!m_initialized || !m_frameInProgress) return;

    // Stop frame timer
    m_frameTimer.Stop();

    // End GPU frame
    if (m_gpuTimer) {
        m_gpuTimer->EndFrame();
    }

    m_frameInProgress = false;

    // Calculate frame metrics
    m_currentFrameTime = static_cast<float>(m_frameTimer.GetElapsedMs());
    m_currentFPS = (m_currentFrameTime > 0.0f) ? (1000.0f / m_currentFrameTime) : 0.0f;

    // Update breakdown
    UpdateFrameBreakdown();

    // Update history
    UpdateFPSHistory();

    // Periodically update hardware metrics (every 60 frames)
    if (m_frameNumber % 60 == 0) {
        UpdateHardwareMetrics();
        UpdateMemorySnapshot();
    }

    // Record to database if enabled
    if (m_recordToDatabase && m_database && m_sessionId >= 0) {
        m_framesSinceLastRecord++;
        if (m_framesSinceLastRecord >= m_recordingInterval) {
            RecordToDatabase();
            m_framesSinceLastRecord = 0;
        }
    }

    // Store current as previous for next frame
    m_previousBreakdown = m_currentBreakdown;
}

void DetailedFrameProfiler::BeginStage(const std::string& stageName) {
    if (!m_initialized || !m_frameInProgress) return;

    auto& stage = m_stageTimers[stageName];
    stage.name = stageName;
    stage.cpuTimer.Start();

    if (m_gpuTimer) {
        m_gpuTimer->StartQuery(stageName);
        stage.gpuQueryStarted = true;
    }
}

void DetailedFrameProfiler::EndStage(const std::string& stageName) {
    if (!m_initialized || !m_frameInProgress) return;

    auto it = m_stageTimers.find(stageName);
    if (it == m_stageTimers.end()) return;

    auto& stage = it->second;
    stage.cpuTimer.Stop();
    stage.cpuTimeMs = stage.cpuTimer.GetElapsedMs();

    if (m_gpuTimer && stage.gpuQueryStarted) {
        m_gpuTimer->EndQuery(stageName);
        stage.gpuTimeMs = m_gpuTimer->GetQueryTimeMs(stageName);
    }
}

void DetailedFrameProfiler::UpdateFrameBreakdown() {
    m_currentBreakdown.frameNumber = m_frameNumber;
    m_currentBreakdown.timestamp = GetTimeSinceSessionStart();
    m_currentBreakdown.totalTimeMs = m_currentFrameTime;
    m_currentBreakdown.fps = m_currentFPS;
    m_currentBreakdown.stages.clear();

    // Collect all stage timings
    m_currentCPUTime = 0.0f;
    m_currentGPUTime = 0.0f;

    for (const auto& pair : m_stageTimers) {
        const auto& stage = pair.second;

        FrameBreakdown::StageInfo info;
        info.name = stage.name;
        info.cpuTimeMs = static_cast<float>(stage.cpuTimeMs);
        info.gpuTimeMs = static_cast<float>(stage.gpuTimeMs);
        info.timeMs = std::max(info.cpuTimeMs, info.gpuTimeMs);  // Use the higher value
        info.percentage = 0.0f;  // Will be calculated next

        m_currentBreakdown.stages.push_back(info);

        m_currentCPUTime += info.cpuTimeMs;
        m_currentGPUTime += info.gpuTimeMs;
    }

    // Calculate percentages
    CalculatePercentages();

    // Sort by time (descending)
    std::sort(m_currentBreakdown.stages.begin(), m_currentBreakdown.stages.end(),
              [](const FrameBreakdown::StageInfo& a, const FrameBreakdown::StageInfo& b) {
                  return a.timeMs > b.timeMs;
              });
}

void DetailedFrameProfiler::CalculatePercentages() {
    if (m_currentFrameTime <= 0.0f) return;

    for (auto& stage : m_currentBreakdown.stages) {
        stage.percentage = (stage.timeMs / m_currentFrameTime) * 100.0f;
    }
}

void DetailedFrameProfiler::UpdateFPSHistory() {
    // Add to FPS history
    if (m_fpsHistory.size() >= MAX_HISTORY_SIZE) {
        m_fpsHistory.erase(m_fpsHistory.begin());
    }
    m_fpsHistory.push_back(m_currentFPS);

    // Add to frame time history
    if (m_frameTimeHistory.size() >= MAX_HISTORY_SIZE) {
        m_frameTimeHistory.erase(m_frameTimeHistory.begin());
    }
    m_frameTimeHistory.push_back(m_currentFrameTime);

    // Add to stage histories
    for (const auto& stage : m_currentBreakdown.stages) {
        auto& history = m_stageHistories[stage.name];
        if (history.size() >= MAX_HISTORY_SIZE) {
            history.erase(history.begin());
        }
        history.push_back(stage.timeMs);
    }
}

void DetailedFrameProfiler::RecordToDatabase() {
    if (!m_database || m_sessionId < 0) return;

    // Prepare frame data
    FrameData frameData;
    frameData.sessionId = m_sessionId;
    frameData.frameNumber = m_frameNumber;
    frameData.timestamp = GetTimeSinceSessionStart();
    frameData.totalTimeMs = m_currentFrameTime;
    frameData.fps = m_currentFPS;
    frameData.vsyncEnabled = m_vsyncEnabled;

    // Record frame (returns frame ID)
    int frameId = m_database->RecordFrame(m_sessionId, frameData);

    // Record stages
    for (const auto& stage : m_currentBreakdown.stages) {
        StageData stageData;
        stageData.frameId = frameId;
        stageData.stageName = stage.name;
        stageData.timeMs = stage.timeMs;
        stageData.percentage = stage.percentage;
        stageData.gpuTimeMs = stage.gpuTimeMs;
        stageData.cpuTimeMs = stage.cpuTimeMs;

        m_database->RecordStage(frameId, stageData);
    }

    // Record memory
    MemoryData memData;
    memData.frameId = frameId;
    memData.cpuUsedMB = m_memorySnapshot.cpuUsedMB;
    memData.cpuAvailableMB = m_memorySnapshot.cpuAvailableMB;
    memData.gpuUsedMB = m_memorySnapshot.gpuUsedMB;
    memData.gpuAvailableMB = m_memorySnapshot.gpuAvailableMB;

    m_database->RecordMemory(frameId, memData);

    // Record GPU metrics
    GPUData gpuData;
    gpuData.frameId = frameId;
    gpuData.utilizationPercent = m_hardwareMetrics.gpuUtilization;
    gpuData.temperatureCelsius = m_hardwareMetrics.gpuTemperature;
    gpuData.clockMHz = m_hardwareMetrics.gpuClockMHz;
    gpuData.memoryClockMHz = m_hardwareMetrics.gpuMemoryClockMHz;

    m_database->RecordGPU(frameId, gpuData);

    // Record CPU metrics
    CPUData cpuData;
    cpuData.frameId = frameId;
    cpuData.coreCount = m_hardwareMetrics.cpuCoreCount;
    cpuData.utilizationPercent = m_hardwareMetrics.cpuUtilization;
    cpuData.temperatureCelsius = m_hardwareMetrics.cpuTemperature;
    cpuData.clockMHz = m_hardwareMetrics.cpuClockMHz;

    m_database->RecordCPU(frameId, cpuData);

    // Record rendering stats
    RenderingStats renderStats;
    renderStats.frameId = frameId;
    renderStats.drawCalls = m_renderStats.drawCalls;
    renderStats.triangles = m_renderStats.triangles;
    renderStats.vertices = m_renderStats.vertices;
    renderStats.instances = m_renderStats.instances;
    renderStats.lights = m_renderStats.lights;
    renderStats.shadowMaps = m_renderStats.shadowMaps;

    m_database->RecordRenderingStats(frameId, renderStats);
}

void DetailedFrameProfiler::UpdateMemorySnapshot() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        m_memorySnapshot.cpuUsedMB = pmc.WorkingSetSize / (1024.0f * 1024.0f);
    }

    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        m_memorySnapshot.cpuAvailableMB = memInfo.ullTotalPhys / (1024.0f * 1024.0f);
    }
#else
    struct sysinfo memInfo;
    if (sysinfo(&memInfo) == 0) {
        m_memorySnapshot.cpuAvailableMB = memInfo.totalram / (1024.0f * 1024.0f);
        m_memorySnapshot.cpuUsedMB = (memInfo.totalram - memInfo.freeram) / (1024.0f * 1024.0f);
    }
#endif

    // GPU memory would be queried from graphics API
    // For now, use placeholder values
    m_memorySnapshot.gpuAvailableMB = 8192.0f;  // 8GB
    m_memorySnapshot.gpuUsedMB = 2048.0f;       // 2GB used
}

void DetailedFrameProfiler::UpdateHardwareMetrics() {
    QueryCPUMetrics();
    QueryGPUMetrics();
}

void DetailedFrameProfiler::QueryCPUMetrics() {
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    m_hardwareMetrics.cpuCoreCount = sysInfo.dwNumberOfProcessors;

    // CPU utilization would require PDH (Performance Data Helper) API
    // For now, use placeholder
    m_hardwareMetrics.cpuUtilization = 45.0f;
    m_hardwareMetrics.cpuTemperature = 55.0f;
    m_hardwareMetrics.cpuClockMHz = 3600;
#else
    m_hardwareMetrics.cpuCoreCount = sysconf(_SC_NPROCESSORS_ONLN);
    m_hardwareMetrics.cpuUtilization = 45.0f;
    m_hardwareMetrics.cpuTemperature = 55.0f;
    m_hardwareMetrics.cpuClockMHz = 3600;
#endif
}

void DetailedFrameProfiler::QueryGPUMetrics() {
    // Would use NVML (NVIDIA), ADL (AMD), or similar for real metrics
    // For now, use placeholder values
    m_hardwareMetrics.gpuUtilization = 75.0f;
    m_hardwareMetrics.gpuTemperature = 65.0f;
    m_hardwareMetrics.gpuClockMHz = 1800;
    m_hardwareMetrics.gpuMemoryClockMHz = 7000;
}

void DetailedFrameProfiler::StartSession(const std::string& preset, const std::string& resolution) {
    if (!m_database || m_sessionId >= 0) return;

    // Gather hardware config
    HardwareConfig hwConfig;
    hwConfig.cpuModel = "Generic CPU";  // Would query actual CPU model
    hwConfig.cpuCoreCount = m_hardwareMetrics.cpuCoreCount;
    hwConfig.gpuModel = "Generic GPU";  // Would query actual GPU model
    hwConfig.gpuMemoryMB = 8192;
    hwConfig.systemMemoryMB = static_cast<size_t>(m_memorySnapshot.cpuAvailableMB);
    hwConfig.driverVersion = "1.0.0";
    hwConfig.operatingSystem =
#ifdef _WIN32
        "Windows";
#else
        "Linux";
#endif

    m_sessionId = m_database->CreateSession(hwConfig, preset, resolution);
    m_sessionStartTime = std::chrono::high_resolution_clock::now();

    // Enable batch mode for better performance
    m_database->BeginBatch();
}

void DetailedFrameProfiler::EndSession() {
    if (!m_database || m_sessionId < 0) return;

    // Flush any pending database writes
    m_database->EndBatch();

    m_database->EndSession(m_sessionId);
    m_sessionId = -1;
}

double DetailedFrameProfiler::GetSessionTime() const {
    return GetTimeSinceSessionStart();
}

double DetailedFrameProfiler::GetTimeSinceSessionStart() const {
    if (m_sessionId < 0) return 0.0;

    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - m_sessionStartTime);
    return duration.count() / 1000000.0;
}

float DetailedFrameProfiler::GetAverageFPS(int frameCount) const {
    if (m_fpsHistory.empty()) return 0.0f;

    int count = std::min(frameCount, static_cast<int>(m_fpsHistory.size()));
    float sum = std::accumulate(m_fpsHistory.end() - count, m_fpsHistory.end(), 0.0f);
    return sum / count;
}

float DetailedFrameProfiler::GetAverageFrameTime(int frameCount) const {
    if (m_frameTimeHistory.empty()) return 0.0f;

    int count = std::min(frameCount, static_cast<int>(m_frameTimeHistory.size()));
    float sum = std::accumulate(m_frameTimeHistory.end() - count, m_frameTimeHistory.end(), 0.0f);
    return sum / count;
}

float DetailedFrameProfiler::GetMinFPS(int frameCount) const {
    if (m_fpsHistory.empty()) return 0.0f;

    int count = std::min(frameCount, static_cast<int>(m_fpsHistory.size()));
    return *std::min_element(m_fpsHistory.end() - count, m_fpsHistory.end());
}

float DetailedFrameProfiler::GetMaxFPS(int frameCount) const {
    if (m_fpsHistory.empty()) return 0.0f;

    int count = std::min(frameCount, static_cast<int>(m_fpsHistory.size()));
    return *std::max_element(m_fpsHistory.end() - count, m_fpsHistory.end());
}

std::vector<float> DetailedFrameProfiler::GetFPSHistory(int count) const {
    if (m_fpsHistory.empty()) return {};

    int actualCount = std::min(count, static_cast<int>(m_fpsHistory.size()));
    return std::vector<float>(m_fpsHistory.end() - actualCount, m_fpsHistory.end());
}

std::vector<float> DetailedFrameProfiler::GetFrameTimeHistory(int count) const {
    if (m_frameTimeHistory.empty()) return {};

    int actualCount = std::min(count, static_cast<int>(m_frameTimeHistory.size()));
    return std::vector<float>(m_frameTimeHistory.end() - actualCount, m_frameTimeHistory.end());
}

std::vector<float> DetailedFrameProfiler::GetStageHistory(const std::string& stageName, int count) const {
    auto it = m_stageHistories.find(stageName);
    if (it == m_stageHistories.end() || it->second.empty()) return {};

    int actualCount = std::min(count, static_cast<int>(it->second.size()));
    return std::vector<float>(it->second.end() - actualCount, it->second.end());
}

void DetailedFrameProfiler::ClearHistory() {
    m_fpsHistory.clear();
    m_frameTimeHistory.clear();
    m_stageHistories.clear();
}

void DetailedFrameProfiler::SetHistorySize(int size) {
    // Not directly settable due to MAX_HISTORY_SIZE, but we can clear and reserve
    if (size > MAX_HISTORY_SIZE) size = MAX_HISTORY_SIZE;

    ClearHistory();
    m_fpsHistory.reserve(size);
    m_frameTimeHistory.reserve(size);
}

} // namespace Profiling
} // namespace Engine
