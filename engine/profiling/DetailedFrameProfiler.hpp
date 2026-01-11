#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <array>

namespace Engine {
namespace Profiling {

// Forward declarations
class PerformanceDatabase;

// GPU query handle (platform-specific)
#ifdef _WIN32
using GPUQueryHandle = void*;  // D3D12/Vulkan query pool handle
#else
using GPUQueryHandle = unsigned int;  // OpenGL query object
#endif

/**
 * @brief Stage names for profiling
 */
namespace Stage {
    constexpr const char* CULLING = "Culling";
    constexpr const char* TERRAIN = "Terrain";
    constexpr const char* SDF_GBUFFER = "SDF_GBuffer";
    constexpr const char* LIGHT_ASSIGNMENT = "Light_Assignment";
    constexpr const char* DEFERRED_LIGHTING = "Deferred_Lighting";
    constexpr const char* POST_PROCESSING = "Post_Processing";
    constexpr const char* UI_RENDERING = "UI_Rendering";
    constexpr const char* OVERHEAD = "Overhead";
    constexpr const char* VSYNC_WAIT = "VSync_Wait";
}

/**
 * @brief High-resolution timer for CPU profiling
 */
class CPUTimer {
public:
    CPUTimer();

    void Start();
    void Stop();
    double GetElapsedMs() const;
    double GetElapsedUs() const;
    bool IsRunning() const { return m_running; }

private:
    std::chrono::high_resolution_clock::time_point m_startTime;
    std::chrono::high_resolution_clock::time_point m_endTime;
    bool m_running = false;
};

/**
 * @brief GPU timer using platform-specific queries
 */
class GPUTimer {
public:
    GPUTimer();
    ~GPUTimer();

    bool Initialize();
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    void StartQuery(const std::string& name);
    void EndQuery(const std::string& name);

    double GetQueryTimeMs(const std::string& name) const;
    bool HasQuery(const std::string& name) const;

    void ClearQueries();

private:
    struct QueryPair {
        GPUQueryHandle startQuery;
        GPUQueryHandle endQuery;
        double timeMs = 0.0;
        bool completed = false;
    };

    std::unordered_map<std::string, QueryPair> m_queries;
    bool m_initialized = false;

#ifdef _WIN32
    void* m_queryHeap = nullptr;  // D3D12 query heap
#endif
};

/**
 * @brief Frame breakdown data
 */
struct FrameBreakdown {
    struct StageInfo {
        std::string name;
        float timeMs = 0.0f;
        float percentage = 0.0f;
        float gpuTimeMs = 0.0f;
        float cpuTimeMs = 0.0f;
    };

    int frameNumber = 0;
    double timestamp = 0.0;
    float totalTimeMs = 0.0f;
    float fps = 0.0f;
    std::vector<StageInfo> stages;

    // Quick accessors for common stages
    float GetCullingTime() const;
    float GetTerrainTime() const;
    float GetLightingTime() const;
    float GetPostProcessingTime() const;
};

/**
 * @brief Memory tracking data
 */
struct MemorySnapshot {
    float cpuUsedMB = 0.0f;
    float cpuAvailableMB = 0.0f;
    float gpuUsedMB = 0.0f;
    float gpuAvailableMB = 0.0f;

    float GetCPUUsagePercent() const;
    float GetGPUUsagePercent() const;
};

/**
 * @brief Hardware metrics
 */
struct HardwareMetrics {
    // GPU
    float gpuUtilization = 0.0f;
    float gpuTemperature = 0.0f;
    int gpuClockMHz = 0;
    int gpuMemoryClockMHz = 0;

    // CPU
    int cpuCoreCount = 0;
    float cpuUtilization = 0.0f;
    float cpuTemperature = 0.0f;
    int cpuClockMHz = 0;
};

/**
 * @brief Rendering statistics
 */
struct RenderStats {
    int drawCalls = 0;
    int triangles = 0;
    int vertices = 0;
    int instances = 0;
    int lights = 0;
    int shadowMaps = 0;
};

/**
 * @class DetailedFrameProfiler
 * @brief Comprehensive frame profiler with CPU/GPU timing and database integration
 *
 * Features:
 * - Per-stage CPU and GPU timing
 * - Real-time frame breakdown
 * - Hardware monitoring
 * - Memory tracking
 * - Database integration for historical analysis
 * - Thread-safe operation
 */
class DetailedFrameProfiler {
public:
    DetailedFrameProfiler();
    ~DetailedFrameProfiler();

    // Initialization
    bool Initialize(PerformanceDatabase* database = nullptr);
    void Shutdown();

    // Frame lifecycle
    void BeginFrame();
    void EndFrame();

    // Stage timing
    void BeginStage(const std::string& stageName);
    void EndStage(const std::string& stageName);

    // Scoped stage timing helper
    class ScopedStage {
    public:
        ScopedStage(DetailedFrameProfiler* profiler, const std::string& name)
            : m_profiler(profiler), m_name(name) {
            if (m_profiler) m_profiler->BeginStage(m_name);
        }
        ~ScopedStage() {
            if (m_profiler) m_profiler->EndStage(m_name);
        }
    private:
        DetailedFrameProfiler* m_profiler;
        std::string m_name;
    };

    // Data accessors
    const FrameBreakdown& GetCurrentBreakdown() const { return m_currentBreakdown; }
    const FrameBreakdown& GetPreviousBreakdown() const { return m_previousBreakdown; }

    float GetCurrentFPS() const { return m_currentFPS; }
    float GetCurrentFrameTime() const { return m_currentFrameTime; }
    float GetCurrentGPUTime() const { return m_currentGPUTime; }
    float GetCurrentCPUTime() const { return m_currentCPUTime; }

    // Statistics
    float GetAverageFPS(int frameCount = 60) const;
    float GetAverageFrameTime(int frameCount = 60) const;
    float GetMinFPS(int frameCount = 60) const;
    float GetMaxFPS(int frameCount = 60) const;

    // History access
    std::vector<float> GetFPSHistory(int count = 1000) const;
    std::vector<float> GetFrameTimeHistory(int count = 1000) const;
    std::vector<float> GetStageHistory(const std::string& stageName, int count = 1000) const;

    // Memory tracking
    void UpdateMemorySnapshot();
    const MemorySnapshot& GetMemorySnapshot() const { return m_memorySnapshot; }

    // Hardware monitoring
    void UpdateHardwareMetrics();
    const HardwareMetrics& GetHardwareMetrics() const { return m_hardwareMetrics; }

    // Rendering stats
    void SetRenderStats(const RenderStats& stats) { m_renderStats = stats; }
    const RenderStats& GetRenderStats() const { return m_renderStats; }

    // Database recording
    void EnableDatabaseRecording(bool enable) { m_recordToDatabase = enable; }
    bool IsDatabaseRecordingEnabled() const { return m_recordToDatabase; }
    void SetRecordingInterval(int frames) { m_recordingInterval = frames; }

    // Session management
    void StartSession(const std::string& preset, const std::string& resolution);
    void EndSession();
    bool IsSessionActive() const { return m_sessionId >= 0; }
    int GetSessionId() const { return m_sessionId; }

    // History management
    void ClearHistory();
    void SetHistorySize(int size);
    int GetHistorySize() const { return static_cast<int>(m_fpsHistory.size()); }

    // Configuration
    void SetVSyncEnabled(bool enabled) { m_vsyncEnabled = enabled; }
    bool IsVSyncEnabled() const { return m_vsyncEnabled; }

    void SetTargetFPS(float fps) { m_targetFPS = fps; }
    float GetTargetFPS() const { return m_targetFPS; }

    // Helpers
    int GetCurrentFrameNumber() const { return m_frameNumber; }
    double GetSessionTime() const;

private:
    // Stage tracking
    struct StageTimer {
        std::string name;
        CPUTimer cpuTimer;
        bool gpuQueryStarted = false;
        double cpuTimeMs = 0.0;
        double gpuTimeMs = 0.0;
    };

    // Internal methods
    void UpdateFrameBreakdown();
    void RecordToDatabase();
    void UpdateFPSHistory();
    void CalculatePercentages();

    // Platform-specific hardware queries
    void QueryGPUMetrics();
    void QueryCPUMetrics();
    void QueryMemoryUsage();

    // Timing helpers
    double GetTimeSinceSessionStart() const;

private:
    // Core state
    bool m_initialized = false;
    bool m_frameInProgress = false;
    int m_frameNumber = 0;

    // Session
    int m_sessionId = -1;
    std::chrono::high_resolution_clock::time_point m_sessionStartTime;

    // Timers
    CPUTimer m_frameTimer;
    std::unique_ptr<GPUTimer> m_gpuTimer;
    std::unordered_map<std::string, StageTimer> m_stageTimers;

    // Current frame data
    FrameBreakdown m_currentBreakdown;
    FrameBreakdown m_previousBreakdown;

    float m_currentFPS = 0.0f;
    float m_currentFrameTime = 0.0f;
    float m_currentGPUTime = 0.0f;
    float m_currentCPUTime = 0.0f;

    // Memory and hardware
    MemorySnapshot m_memorySnapshot;
    HardwareMetrics m_hardwareMetrics;
    RenderStats m_renderStats;

    // History
    std::vector<float> m_fpsHistory;
    std::vector<float> m_frameTimeHistory;
    std::unordered_map<std::string, std::vector<float>> m_stageHistories;

    static constexpr int MAX_HISTORY_SIZE = 10000;

    // Database integration
    PerformanceDatabase* m_database = nullptr;
    bool m_recordToDatabase = false;
    int m_recordingInterval = 1;  // Record every N frames
    int m_framesSinceLastRecord = 0;

    // Configuration
    bool m_vsyncEnabled = false;
    float m_targetFPS = 60.0f;

    // Frame timing
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    double m_deltaTime = 0.0;
};

/**
 * @brief Scoped profiling helper macro
 */
#define PROFILE_STAGE(profiler, stageName) \
    Engine::Profiling::DetailedFrameProfiler::ScopedStage __scopedStage##__LINE__(profiler, stageName)

} // namespace Profiling
} // namespace Engine
