#pragma once

/**
 * @file Profiler.hpp
 * @brief High-performance profiling system for Nova3D engine
 *
 * Features:
 * - Scoped timing with RAII (PROFILE_SCOPE macro)
 * - Hierarchical profiling (nested scopes with parent-child relationships)
 * - GPU timing queries using OpenGL timer queries
 * - Frame time tracking with history
 * - Memory usage tracking
 * - Statistics: min, max, average, percentiles (50th, 95th, 99th)
 * - CSV export for external analysis
 * - ImGui integration for real-time visualization
 *
 * @section usage Usage Example
 * @code
 * void Render() {
 *     NOVA_PROFILE_FRAME_BEGIN();
 *
 *     {
 *         NOVA_PROFILE_SCOPE("Scene Rendering");
 *         {
 *             NOVA_PROFILE_SCOPE("Culling");
 *             // Culling code
 *         }
 *         {
 *             NOVA_PROFILE_GPU_SCOPE("Draw Calls");
 *             // GPU rendering
 *         }
 *     }
 *
 *     NOVA_PROFILE_FRAME_END();
 * }
 * @endcode
 */

#include <chrono>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <array>
#include <algorithm>
#include <numeric>
#include <cstdio>
#include <fstream>
#include <deque>
#include <memory>
#include <cstdint>

namespace Nova {

// Forward declarations
class ProfilerWindow;

// =============================================================================
// Constants
// =============================================================================

constexpr size_t PROFILER_FRAME_HISTORY_SIZE = 300;   // ~5 seconds at 60fps
constexpr size_t PROFILER_SAMPLE_HISTORY_SIZE = 1000; // Samples for percentile calculation
constexpr size_t PROFILER_MAX_GPU_QUERIES = 64;
constexpr size_t PROFILER_MAX_HIERARCHY_DEPTH = 32;

// =============================================================================
// High-Precision Timer
// =============================================================================

/**
 * @brief High-precision timer for profiling
 */
class ProfileTimer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::duration<double, std::milli>;

    ProfileTimer() : m_start(Clock::now()) {}

    void Reset() noexcept { m_start = Clock::now(); }

    [[nodiscard]] double ElapsedMs() const noexcept {
        return Duration(Clock::now() - m_start).count();
    }

    [[nodiscard]] double ElapsedUs() const noexcept {
        return std::chrono::duration<double, std::micro>(Clock::now() - m_start).count();
    }

    [[nodiscard]] double ElapsedNs() const noexcept {
        return std::chrono::duration<double, std::nano>(Clock::now() - m_start).count();
    }

    [[nodiscard]] TimePoint GetStartTime() const noexcept { return m_start; }

private:
    TimePoint m_start;
};

// =============================================================================
// Profiler Statistics
// =============================================================================

/**
 * @brief Extended statistics for a profiled section
 *
 * Tracks min, max, average, and percentile timings over a rolling window
 * of samples for more detailed performance analysis.
 */
struct ProfilerStats {
    std::string name;
    uint32_t depth = 0;              // Hierarchy depth (0 = root)
    std::string parentName;          // Parent scope name (empty for root)

    // Timing statistics
    double totalMs = 0.0;
    double minMs = std::numeric_limits<double>::max();
    double maxMs = 0.0;
    double avgMs = 0.0;
    double lastMs = 0.0;             // Most recent sample
    uint64_t callCount = 0;

    // Percentiles (calculated from sample history)
    double p50Ms = 0.0;              // Median
    double p95Ms = 0.0;              // 95th percentile
    double p99Ms = 0.0;              // 99th percentile
    double stdDevMs = 0.0;           // Standard deviation

    // Rolling sample history for percentile calculation
    std::deque<double> sampleHistory;

    void AddSample(double ms) {
        lastMs = ms;
        totalMs += ms;
        minMs = std::min(minMs, ms);
        maxMs = std::max(maxMs, ms);
        ++callCount;
        avgMs = totalMs / static_cast<double>(callCount);

        // Maintain rolling history
        sampleHistory.push_back(ms);
        if (sampleHistory.size() > PROFILER_SAMPLE_HISTORY_SIZE) {
            sampleHistory.pop_front();
        }

        // Update percentiles periodically (every 100 samples to avoid overhead)
        if (callCount % 100 == 0 || sampleHistory.size() <= 10) {
            UpdatePercentiles();
        }
    }

    void UpdatePercentiles() {
        if (sampleHistory.empty()) return;

        std::vector<double> sorted(sampleHistory.begin(), sampleHistory.end());
        std::sort(sorted.begin(), sorted.end());

        size_t n = sorted.size();
        p50Ms = sorted[n / 2];
        p95Ms = sorted[std::min(static_cast<size_t>(n * 0.95), n - 1)];
        p99Ms = sorted[std::min(static_cast<size_t>(n * 0.99), n - 1)];

        // Calculate standard deviation
        double mean = avgMs;
        double sumSqDiff = 0.0;
        for (double sample : sampleHistory) {
            double diff = sample - mean;
            sumSqDiff += diff * diff;
        }
        stdDevMs = std::sqrt(sumSqDiff / static_cast<double>(n));
    }

    void Reset() {
        totalMs = 0.0;
        minMs = std::numeric_limits<double>::max();
        maxMs = 0.0;
        avgMs = 0.0;
        lastMs = 0.0;
        callCount = 0;
        p50Ms = p95Ms = p99Ms = stdDevMs = 0.0;
        sampleHistory.clear();
    }
};

// =============================================================================
// Frame Statistics
// =============================================================================

/**
 * @brief Per-frame statistics snapshot
 */
struct FrameStats {
    uint64_t frameNumber = 0;
    double frameTimeMs = 0.0;
    double cpuTimeMs = 0.0;
    double gpuTimeMs = 0.0;
    double fps = 0.0;

    // Memory statistics (in bytes)
    size_t totalMemoryUsed = 0;
    size_t peakMemoryUsed = 0;
    size_t gpuMemoryUsed = 0;

    // Hierarchical scope timings for this frame
    std::vector<std::pair<std::string, double>> scopeTimings;
};

// =============================================================================
// Memory Tracker
// =============================================================================

/**
 * @brief Tracks memory allocations and usage
 */
class MemoryTracker {
public:
    struct MemoryStats {
        size_t currentBytes = 0;
        size_t peakBytes = 0;
        uint64_t totalAllocations = 0;
        uint64_t totalDeallocations = 0;
        size_t gpuMemoryBytes = 0;
    };

    static MemoryTracker& Instance() {
        static MemoryTracker instance;
        return instance;
    }

    void RecordAllocation(size_t bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.currentBytes += bytes;
        m_stats.totalAllocations++;
        if (m_stats.currentBytes > m_stats.peakBytes) {
            m_stats.peakBytes = m_stats.currentBytes;
        }
    }

    void RecordDeallocation(size_t bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (bytes <= m_stats.currentBytes) {
            m_stats.currentBytes -= bytes;
        }
        m_stats.totalDeallocations++;
    }

    void SetGPUMemory(size_t bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.gpuMemoryBytes = bytes;
    }

    [[nodiscard]] MemoryStats GetStats() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stats;
    }

    void Reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats = MemoryStats{};
    }

private:
    MemoryTracker() = default;
    mutable std::mutex m_mutex;
    MemoryStats m_stats;
};

// =============================================================================
// GPU Profiler
// =============================================================================

/**
 * @brief GPU timing query manager using OpenGL timer queries
 *
 * Uses GL_TIME_ELAPSED queries with double-buffering to avoid stalls.
 */
class GPUProfiler {
public:
    struct GPUQuery {
        uint32_t queryId[2] = {0, 0};  // Double-buffered queries
        std::string name;
        double timeMs = 0.0;
        bool active = false;
        int currentBuffer = 0;
    };

    static GPUProfiler& Instance() {
        static GPUProfiler instance;
        return instance;
    }

    /**
     * @brief Initialize GPU profiling (creates OpenGL query objects)
     * @return true if initialization succeeded
     */
    bool Initialize();

    /**
     * @brief Shutdown GPU profiling (deletes query objects)
     */
    void Shutdown();

    /**
     * @brief Check if GPU profiler is initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Begin a GPU timing scope
     * @param name Identifier for this GPU timing section
     * @return Query index, or -1 if no queries available
     */
    int BeginQuery(std::string_view name);

    /**
     * @brief End the current GPU timing scope
     */
    void EndQuery();

    /**
     * @brief Collect timing results from completed queries
     * Call this once per frame after all rendering is complete
     */
    void CollectResults();

    /**
     * @brief Get results for a specific query by name
     */
    [[nodiscard]] double GetQueryTime(std::string_view name) const;

    /**
     * @brief Get all GPU query results
     */
    [[nodiscard]] std::vector<std::pair<std::string, double>> GetAllResults() const;

    /**
     * @brief Get total GPU time for the current frame
     */
    [[nodiscard]] double GetTotalGPUTime() const noexcept { return m_totalGPUTime; }

    /**
     * @brief Begin frame - swap query buffers
     */
    void BeginFrame();

    /**
     * @brief End frame - finalize GPU timing
     */
    void EndFrame();

private:
    GPUProfiler() = default;
    ~GPUProfiler();

    GPUProfiler(const GPUProfiler&) = delete;
    GPUProfiler& operator=(const GPUProfiler&) = delete;

    std::array<GPUQuery, PROFILER_MAX_GPU_QUERIES> m_queries;
    int m_activeQueryIndex = -1;
    int m_nextQuerySlot = 0;
    int m_currentFrame = 0;
    double m_totalGPUTime = 0.0;
    bool m_initialized = false;
    mutable std::mutex m_mutex;
};

// =============================================================================
// Hierarchical Scope Tracking
// =============================================================================

/**
 * @brief Tracks the current profiling scope hierarchy per thread
 */
class ScopeStack {
public:
    struct ScopeInfo {
        std::string name;
        ProfileTimer timer;
        uint32_t depth;
    };

    void Push(std::string_view name) {
        ScopeInfo info;
        info.name = std::string(name);
        info.depth = static_cast<uint32_t>(m_stack.size());
        m_stack.push_back(std::move(info));
    }

    ScopeInfo Pop() {
        if (m_stack.empty()) {
            return ScopeInfo{};
        }
        ScopeInfo info = std::move(m_stack.back());
        m_stack.pop_back();
        return info;
    }

    [[nodiscard]] bool IsEmpty() const noexcept { return m_stack.empty(); }
    [[nodiscard]] size_t Depth() const noexcept { return m_stack.size(); }

    [[nodiscard]] std::string GetCurrentPath() const {
        std::string path;
        for (const auto& scope : m_stack) {
            if (!path.empty()) path += "/";
            path += scope.name;
        }
        return path;
    }

    [[nodiscard]] std::string GetParentName() const {
        if (m_stack.size() < 2) return "";
        return m_stack[m_stack.size() - 2].name;
    }

private:
    std::vector<ScopeInfo> m_stack;
};

// =============================================================================
// Main Profiler Class
// =============================================================================

/**
 * @brief Thread-safe hierarchical profiler for measuring code performance
 *
 * The Profiler is a singleton that provides:
 * - Scoped timing with automatic hierarchy tracking
 * - GPU timing via OpenGL timer queries
 * - Frame time history and statistics
 * - Memory usage tracking
 * - CSV export for external analysis
 */
class Profiler {
public:
    /**
     * @brief RAII scope marker for automatic hierarchical timing
     */
    class ProfileScope {
    public:
        ProfileScope(Profiler& profiler, std::string_view name, bool gpuScope = false);
        ~ProfileScope();

        // Non-copyable, movable
        ProfileScope(const ProfileScope&) = delete;
        ProfileScope& operator=(const ProfileScope&) = delete;
        ProfileScope(ProfileScope&& other) noexcept;
        ProfileScope& operator=(ProfileScope&&) = delete;

        [[nodiscard]] double GetElapsedMs() const { return m_timer.ElapsedMs(); }

    private:
        Profiler* m_profiler;
        std::string m_name;
        std::string m_parentName;
        uint32_t m_depth;
        ProfileTimer m_timer;
        bool m_gpuScope;
        int m_gpuQueryIndex;
    };

    /**
     * @brief Get singleton instance
     */
    static Profiler& Instance() {
        static Profiler instance;
        return instance;
    }

    // Delete copy/move - singleton pattern
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;
    Profiler(Profiler&&) = delete;
    Profiler& operator=(Profiler&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialize the profiler (creates GPU queries, etc.)
     */
    bool Initialize();

    /**
     * @brief Shutdown the profiler
     */
    void Shutdown();

    /**
     * @brief Enable/disable profiling globally
     */
    void SetEnabled(bool enabled) noexcept { m_enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }

    // =========================================================================
    // Frame Markers
    // =========================================================================

    /**
     * @brief Mark the beginning of a frame
     */
    void BeginFrame();

    /**
     * @brief Mark the end of a frame
     */
    void EndFrame();

    /**
     * @brief Get current frame number
     */
    [[nodiscard]] uint64_t GetFrameCount() const noexcept { return m_frameCount; }

    // =========================================================================
    // Scoped Profiling
    // =========================================================================

    /**
     * @brief Begin a profiling scope (returns RAII marker)
     * @param name Identifier for this scope
     * @return RAII scope marker that automatically records timing on destruction
     */
    [[nodiscard]] ProfileScope BeginScope(std::string_view name) {
        return ProfileScope(*this, name, false);
    }

    /**
     * @brief Begin a GPU profiling scope (returns RAII marker)
     * @param name Identifier for this GPU scope
     * @return RAII scope marker that records both CPU and GPU timing
     */
    [[nodiscard]] ProfileScope BeginGPUScope(std::string_view name) {
        return ProfileScope(*this, name, true);
    }

    /**
     * @brief Record a timing sample directly
     */
    void RecordSample(std::string_view name, double milliseconds,
                      uint32_t depth = 0, std::string_view parent = "");

    // =========================================================================
    // Statistics Access
    // =========================================================================

    /**
     * @brief Get stats for a specific scope
     * @param name Scope name to query
     * @return Pointer to stats, or nullptr if not found
     */
    [[nodiscard]] const ProfilerStats* GetScopeStats(std::string_view name) const;

    /**
     * @brief Get all scope statistics sorted by total time
     */
    [[nodiscard]] std::vector<ProfilerStats> GetAllScopeStats() const;

    /**
     * @brief Get hierarchical scope statistics (tree structure)
     */
    [[nodiscard]] std::vector<ProfilerStats> GetHierarchicalStats() const;

    /**
     * @brief Get frame statistics for the last N frames
     */
    [[nodiscard]] const std::deque<FrameStats>& GetFrameHistory() const { return m_frameHistory; }

    /**
     * @brief Get the most recent frame statistics
     */
    [[nodiscard]] FrameStats GetLastFrameStats() const;

    /**
     * @brief Get average FPS over the frame history
     */
    [[nodiscard]] double GetAverageFPS() const;

    /**
     * @brief Get average frame time in milliseconds
     */
    [[nodiscard]] double GetAverageFrameTime() const;

    // =========================================================================
    // Memory Statistics
    // =========================================================================

    /**
     * @brief Get current memory statistics
     */
    [[nodiscard]] MemoryTracker::MemoryStats GetMemoryStats() const {
        return MemoryTracker::Instance().GetStats();
    }

    // =========================================================================
    // Export
    // =========================================================================

    /**
     * @brief Generate a text report of profiling statistics
     */
    [[nodiscard]] std::string GenerateReport() const;

    /**
     * @brief Export profiling data to CSV file
     * @param filename Path to output CSV file
     * @return true if export succeeded
     */
    bool ExportCSV(const std::string& filename) const;

    /**
     * @brief Export frame history to CSV
     */
    bool ExportFrameHistoryCSV(const std::string& filename) const;

    /**
     * @brief Save report to text file
     */
    bool SaveReport(const std::string& filename) const;

    // =========================================================================
    // Reset
    // =========================================================================

    /**
     * @brief Reset all statistics (keeps scope names)
     */
    void ResetStats();

    /**
     * @brief Clear all data including scope names
     */
    void Clear();

private:
    Profiler();
    ~Profiler();

    // Thread-local scope stack for hierarchy tracking
    ScopeStack& GetThreadScopeStack();

    // Internal methods
    void EndScopeInternal(const std::string& name, double milliseconds,
                          uint32_t depth, const std::string& parent);
    void UpdateFrameStats();

    // Statistics storage
    std::unordered_map<std::string, ProfilerStats> m_scopeStats;
    mutable std::mutex m_statsMutex;

    // Frame tracking
    ProfileTimer m_frameTimer;
    std::deque<FrameStats> m_frameHistory;
    std::array<double, PROFILER_FRAME_HISTORY_SIZE> m_recentFrameTimes;
    size_t m_frameTimeIndex = 0;
    uint64_t m_frameCount = 0;
    FrameStats m_currentFrameStats;

    // State
    std::atomic<bool> m_enabled{true};
    bool m_initialized = false;

    // Thread-local storage for scope stacks
    std::unordered_map<std::thread::id, std::unique_ptr<ScopeStack>> m_threadStacks;
    std::mutex m_threadStacksMutex;
};

// =============================================================================
// Profiler Window (ImGui)
// =============================================================================

/**
 * @brief ImGui window for real-time profiler visualization
 */
class ProfilerWindow {
public:
    ProfilerWindow();
    ~ProfilerWindow() = default;

    /**
     * @brief Initialize the profiler window
     */
    bool Initialize();

    /**
     * @brief Render the profiler window
     * Call this within your ImGui rendering loop
     */
    void Render();

    /**
     * @brief Show/hide the window
     */
    void Show() { m_visible = true; }
    void Hide() { m_visible = false; }
    void Toggle() { m_visible = !m_visible; }
    [[nodiscard]] bool IsVisible() const noexcept { return m_visible; }
    void SetVisible(bool visible) { m_visible = visible; }

    // Configuration
    void SetUpdateInterval(float seconds) { m_updateInterval = seconds; }
    void SetGraphHeight(float height) { m_graphHeight = height; }
    void SetShowGPU(bool show) { m_showGPU = show; }
    void SetShowMemory(bool show) { m_showMemory = show; }
    void SetShowHierarchy(bool show) { m_showHierarchy = show; }

private:
    void RenderFrameTimeGraph();
    void RenderScopeHierarchy();
    void RenderMemoryStats();
    void RenderGPUStats();
    void RenderStatisticsTable();
    void RenderControlsToolbar();

    bool m_visible = false;
    float m_updateInterval = 0.1f;  // Update stats every 100ms
    float m_graphHeight = 100.0f;
    float m_lastUpdateTime = 0.0f;

    bool m_showGPU = true;
    bool m_showMemory = true;
    bool m_showHierarchy = true;
    bool m_pauseUpdates = false;
    bool m_showPercentiles = true;

    // Cached data for display (updated at m_updateInterval)
    std::vector<ProfilerStats> m_cachedStats;
    std::vector<float> m_frameTimeGraphData;
    std::vector<float> m_fpsGraphData;

    // Graph settings
    float m_graphTimeScale = 16.67f;  // Default to 60fps scale (16.67ms)
    int m_selectedScopeIndex = -1;
};

} // namespace Nova

// =============================================================================
// Profiling Macros
// =============================================================================

// Enable profiling by default (can be disabled via compile flag -DNOVA_PROFILE_ENABLED=0)
#ifndef NOVA_PROFILE_ENABLED
#define NOVA_PROFILE_ENABLED 1
#endif

#if NOVA_PROFILE_ENABLED

/**
 * @brief Profile a scope (measures until end of current block)
 */
#define NOVA_PROFILE_SCOPE(name) \
    auto NOVA_CONCAT(_nova_profile_scope_, __LINE__) = ::Nova::Profiler::Instance().BeginScope(name)

/**
 * @brief Profile a GPU scope (measures both CPU and GPU time)
 */
#define NOVA_PROFILE_GPU_SCOPE(name) \
    auto NOVA_CONCAT(_nova_profile_gpu_scope_, __LINE__) = ::Nova::Profiler::Instance().BeginGPUScope(name)

/**
 * @brief Profile a function (uses function name)
 */
#define NOVA_PROFILE_FUNCTION() \
    NOVA_PROFILE_SCOPE(__FUNCTION__)

/**
 * @brief Record a timing sample directly
 */
#define NOVA_PROFILE_SAMPLE(name, ms) \
    ::Nova::Profiler::Instance().RecordSample(name, ms)

/**
 * @brief Frame markers
 */
#define NOVA_PROFILE_FRAME_BEGIN() \
    ::Nova::Profiler::Instance().BeginFrame()

#define NOVA_PROFILE_FRAME_END() \
    ::Nova::Profiler::Instance().EndFrame()

/**
 * @brief GPU query markers (manual, prefer NOVA_PROFILE_GPU_SCOPE)
 */
#define NOVA_PROFILE_GPU_BEGIN(name) \
    ::Nova::GPUProfiler::Instance().BeginQuery(name)

#define NOVA_PROFILE_GPU_END() \
    ::Nova::GPUProfiler::Instance().EndQuery()

// Helper macro for unique variable names
#define NOVA_CONCAT_IMPL(a, b) a##b
#define NOVA_CONCAT(a, b) NOVA_CONCAT_IMPL(a, b)

#else // NOVA_PROFILE_ENABLED == 0

// No-op versions when profiling is disabled
#define NOVA_PROFILE_SCOPE(name) ((void)0)
#define NOVA_PROFILE_GPU_SCOPE(name) ((void)0)
#define NOVA_PROFILE_FUNCTION() ((void)0)
#define NOVA_PROFILE_SAMPLE(name, ms) ((void)0)
#define NOVA_PROFILE_FRAME_BEGIN() ((void)0)
#define NOVA_PROFILE_FRAME_END() ((void)0)
#define NOVA_PROFILE_GPU_BEGIN(name) ((void)0)
#define NOVA_PROFILE_GPU_END() ((void)0)

#endif // NOVA_PROFILE_ENABLED
