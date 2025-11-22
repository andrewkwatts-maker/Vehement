#pragma once

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

namespace Nova {

/**
 * @brief High-precision timer for profiling
 */
class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::duration<double, std::milli>;

    Timer() : m_start(Clock::now()) {}

    void Reset() { m_start = Clock::now(); }

    [[nodiscard]] double ElapsedMs() const {
        return Duration(Clock::now() - m_start).count();
    }

    [[nodiscard]] double ElapsedUs() const {
        return std::chrono::duration<double, std::micro>(Clock::now() - m_start).count();
    }

    [[nodiscard]] double ElapsedNs() const {
        return std::chrono::duration<double, std::nano>(Clock::now() - m_start).count();
    }

private:
    TimePoint m_start;
};

/**
 * @brief Statistics for a profiled section
 */
struct ProfileStats {
    std::string name;
    double totalMs = 0.0;
    double minMs = std::numeric_limits<double>::max();
    double maxMs = 0.0;
    double avgMs = 0.0;
    uint64_t callCount = 0;

    void AddSample(double ms) {
        totalMs += ms;
        minMs = std::min(minMs, ms);
        maxMs = std::max(maxMs, ms);
        ++callCount;
        avgMs = totalMs / callCount;
    }

    void Reset() {
        totalMs = 0.0;
        minMs = std::numeric_limits<double>::max();
        maxMs = 0.0;
        avgMs = 0.0;
        callCount = 0;
    }
};

/**
 * @brief Thread-safe profiler for measuring code performance
 *
 * Usage:
 * @code
 * // Manual timing
 * NOVA_PROFILE_SCOPE("MyFunction");
 *
 * // Or use the timer directly
 * {
 *     auto scope = Profiler::Instance().BeginScope("Rendering");
 *     // ... render code ...
 * } // Automatically ends and records
 *
 * // Query results
 * auto& stats = Profiler::Instance().GetStats("Rendering");
 * @endcode
 */
class Profiler {
public:
    /**
     * @brief RAII scope marker for automatic timing
     */
    class ScopeMarker {
    public:
        ScopeMarker(Profiler& profiler, std::string_view name)
            : m_profiler(&profiler), m_name(name) {}

        ~ScopeMarker() {
            if (m_profiler) {
                m_profiler->EndScope(m_name, m_timer.ElapsedMs());
            }
        }

        // Non-copyable, movable
        ScopeMarker(const ScopeMarker&) = delete;
        ScopeMarker& operator=(const ScopeMarker&) = delete;
        ScopeMarker(ScopeMarker&& other) noexcept
            : m_profiler(other.m_profiler), m_name(other.m_name), m_timer(other.m_timer) {
            other.m_profiler = nullptr;
        }
        ScopeMarker& operator=(ScopeMarker&&) = delete;

    private:
        Profiler* m_profiler;
        std::string_view m_name;
        Timer m_timer;
    };

    /**
     * @brief Get singleton instance
     */
    static Profiler& Instance() {
        static Profiler instance;
        return instance;
    }

    // Delete copy/move
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;
    Profiler(Profiler&&) = delete;
    Profiler& operator=(Profiler&&) = delete;

    /**
     * @brief Enable/disable profiling globally
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }

    /**
     * @brief Begin a profiling scope (returns RAII marker)
     */
    [[nodiscard]] ScopeMarker BeginScope(std::string_view name) {
        return ScopeMarker(*this, name);
    }

    /**
     * @brief Record a timing sample directly
     */
    void RecordSample(std::string_view name, double milliseconds) {
        if (!m_enabled) return;

        std::lock_guard lock(m_mutex);
        auto& stats = m_stats[std::string(name)];
        if (stats.name.empty()) {
            stats.name = std::string(name);
        }
        stats.AddSample(milliseconds);
    }

    /**
     * @brief Get stats for a specific section
     */
    [[nodiscard]] const ProfileStats* GetStats(std::string_view name) const {
        std::lock_guard lock(m_mutex);
        auto it = m_stats.find(std::string(name));
        return (it != m_stats.end()) ? &it->second : nullptr;
    }

    /**
     * @brief Get all stats sorted by total time
     */
    [[nodiscard]] std::vector<ProfileStats> GetAllStats() const {
        std::lock_guard lock(m_mutex);
        std::vector<ProfileStats> result;
        result.reserve(m_stats.size());

        for (const auto& [name, stats] : m_stats) {
            result.push_back(stats);
        }

        std::sort(result.begin(), result.end(),
                  [](const ProfileStats& a, const ProfileStats& b) {
                      return a.totalMs > b.totalMs;
                  });

        return result;
    }

    /**
     * @brief Reset all statistics
     */
    void Reset() {
        std::lock_guard lock(m_mutex);
        for (auto& [name, stats] : m_stats) {
            stats.Reset();
        }
    }

    /**
     * @brief Clear all statistics
     */
    void Clear() {
        std::lock_guard lock(m_mutex);
        m_stats.clear();
    }

    /**
     * @brief Generate a text report
     */
    [[nodiscard]] std::string GenerateReport() const {
        auto stats = GetAllStats();
        std::string report;
        report.reserve(4096);

        report += "=== Performance Profile Report ===\n\n";
        report += "Section                          | Total (ms) | Avg (ms) | Min (ms) | Max (ms) | Calls\n";
        report += "---------------------------------|------------|----------|----------|----------|--------\n";

        for (const auto& s : stats) {
            char line[256];
            std::snprintf(line, sizeof(line),
                         "%-32s | %10.2f | %8.3f | %8.3f | %8.3f | %6llu\n",
                         s.name.c_str(), s.totalMs, s.avgMs, s.minMs, s.maxMs,
                         static_cast<unsigned long long>(s.callCount));
            report += line;
        }

        return report;
    }

    /**
     * @brief Save report to file
     */
    bool SaveReport(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file) return false;
        file << GenerateReport();
        return true;
    }

    /**
     * @brief Frame boundary marker for per-frame stats
     */
    void BeginFrame() {
        m_frameTimer.Reset();
        ++m_frameCount;
    }

    void EndFrame() {
        double frameMs = m_frameTimer.ElapsedMs();
        RecordSample("Frame", frameMs);

        // Track rolling average FPS
        m_recentFrameTimes[m_frameTimeIndex] = frameMs;
        m_frameTimeIndex = (m_frameTimeIndex + 1) % FRAME_HISTORY_SIZE;
    }

    [[nodiscard]] double GetAverageFPS() const {
        double sum = 0.0;
        for (double t : m_recentFrameTimes) {
            sum += t;
        }
        double avgMs = sum / FRAME_HISTORY_SIZE;
        return (avgMs > 0.0) ? (1000.0 / avgMs) : 0.0;
    }

    [[nodiscard]] uint64_t GetFrameCount() const { return m_frameCount; }

private:
    Profiler() {
        m_recentFrameTimes.fill(16.67);  // Default to ~60 FPS
    }

    void EndScope(std::string_view name, double milliseconds) {
        RecordSample(name, milliseconds);
    }

    static constexpr size_t FRAME_HISTORY_SIZE = 120;

    std::unordered_map<std::string, ProfileStats> m_stats;
    mutable std::mutex m_mutex;
    std::atomic<bool> m_enabled{true};

    Timer m_frameTimer;
    std::array<double, FRAME_HISTORY_SIZE> m_recentFrameTimes;
    size_t m_frameTimeIndex = 0;
    uint64_t m_frameCount = 0;
};

/**
 * @brief GPU timing query wrapper (placeholder for actual GPU profiling)
 */
class GPUProfiler {
public:
    struct GPUTimestamp {
        uint32_t queryId;
        std::string name;
        double startMs;
        double endMs;
    };

    static GPUProfiler& Instance() {
        static GPUProfiler instance;
        return instance;
    }

    void BeginQuery(std::string_view name) {
        // Would call glBeginQuery or equivalent
        m_currentQuery = std::string(name);
        m_queryStart.Reset();
    }

    void EndQuery() {
        // Would call glEndQuery or equivalent
        double elapsed = m_queryStart.ElapsedMs();
        Profiler::Instance().RecordSample("GPU_" + m_currentQuery, elapsed);
    }

private:
    GPUProfiler() = default;
    std::string m_currentQuery;
    Timer m_queryStart;
};

} // namespace Nova

// =============================================================================
// Profiling Macros
// =============================================================================

#ifdef NOVA_PROFILE_ENABLED

/**
 * @brief Profile a scope (measures until end of current block)
 */
#define NOVA_PROFILE_SCOPE(name) \
    auto _nova_profile_scope_##__LINE__ = ::Nova::Profiler::Instance().BeginScope(name)

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
 * @brief GPU query markers
 */
#define NOVA_PROFILE_GPU_BEGIN(name) \
    ::Nova::GPUProfiler::Instance().BeginQuery(name)

#define NOVA_PROFILE_GPU_END() \
    ::Nova::GPUProfiler::Instance().EndQuery()

#else

// No-op versions when profiling is disabled
#define NOVA_PROFILE_SCOPE(name) ((void)0)
#define NOVA_PROFILE_FUNCTION() ((void)0)
#define NOVA_PROFILE_SAMPLE(name, ms) ((void)0)
#define NOVA_PROFILE_FRAME_BEGIN() ((void)0)
#define NOVA_PROFILE_FRAME_END() ((void)0)
#define NOVA_PROFILE_GPU_BEGIN(name) ((void)0)
#define NOVA_PROFILE_GPU_END() ((void)0)

#endif

// Always enable profiling by default (can be disabled via compile flag)
#ifndef NOVA_PROFILE_ENABLED
#define NOVA_PROFILE_ENABLED 1
#endif
