#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

namespace Nova {

/**
 * @brief Time management class
 *
 * Tracks delta time, total time, and FPS.
 * Uses std::chrono for precise, type-safe timing.
 */
class Time {
public:
    using Clock = std::chrono::steady_clock;  // steady_clock is more appropriate for game timing
    using Duration = std::chrono::duration<float>;
    using TimePoint = Clock::time_point;

    /**
     * @brief Construct Time manager and initialize timing
     */
    Time() noexcept;

    // Rule of five - default implementations are fine
    ~Time() = default;
    Time(const Time&) = default;
    Time& operator=(const Time&) = default;
    Time(Time&&) noexcept = default;
    Time& operator=(Time&&) noexcept = default;

    /**
     * @brief Update time calculations (call once per frame)
     */
    void Update() noexcept;

    /**
     * @brief Get time since last frame in seconds
     */
    [[nodiscard]] float GetDeltaTime() const noexcept { return m_deltaTime; }

    /**
     * @brief Get unscaled delta time (ignores time scale)
     */
    [[nodiscard]] float GetUnscaledDeltaTime() const noexcept { return m_unscaledDeltaTime; }

    /**
     * @brief Get total time since engine start in seconds
     */
    [[nodiscard]] float GetTotalTime() const noexcept { return m_totalTime; }

    /**
     * @brief Get current frames per second (instantaneous)
     */
    [[nodiscard]] float GetFPS() const noexcept { return m_fps; }

    /**
     * @brief Get average FPS over last second (smoothed)
     */
    [[nodiscard]] float GetAverageFPS() const noexcept { return m_averageFPS; }

    /**
     * @brief Get current frame number
     */
    [[nodiscard]] std::uint64_t GetFrameCount() const noexcept { return m_frameCount; }

    /**
     * @brief Set time scale (1.0 = normal, 0.5 = half speed, 2.0 = double speed)
     * @param scale Time scale factor (clamped to >= 0)
     */
    void SetTimeScale(float scale) noexcept { m_timeScale = (scale >= 0.0f) ? scale : 0.0f; }

    /**
     * @brief Get current time scale
     */
    [[nodiscard]] float GetTimeScale() const noexcept { return m_timeScale; }

    /**
     * @brief Set maximum delta time (prevents large jumps on lag/breakpoints)
     * @param maxDelta Maximum delta in seconds
     */
    void SetMaxDeltaTime(float maxDelta) noexcept { m_maxDeltaTime = maxDelta; }

    /**
     * @brief Get maximum delta time
     */
    [[nodiscard]] float GetMaxDeltaTime() const noexcept { return m_maxDeltaTime; }

    /**
     * @brief Get fixed timestep value for physics
     */
    [[nodiscard]] float GetFixedDeltaTime() const noexcept { return m_fixedDeltaTime; }

    /**
     * @brief Set fixed timestep for physics
     * @param dt Fixed timestep in seconds
     */
    void SetFixedDeltaTime(float dt) noexcept { m_fixedDeltaTime = dt; }

    /**
     * @brief Get accumulated time for fixed updates
     */
    [[nodiscard]] float GetFixedAccumulator() const noexcept { return m_fixedAccumulator; }

    /**
     * @brief Check if a fixed update should occur and consume accumulator
     * @return true if fixed update should run
     */
    [[nodiscard]] bool ShouldFixedUpdate() noexcept;

    /**
     * @brief Get interpolation factor for rendering between fixed updates
     * @return Alpha value [0, 1] for interpolation
     */
    [[nodiscard]] float GetFixedAlpha() const noexcept {
        return m_fixedDeltaTime > 0.0f ? m_fixedAccumulator / m_fixedDeltaTime : 0.0f;
    }

private:
    TimePoint m_startTime;
    TimePoint m_lastFrameTime;
    TimePoint m_currentFrameTime;

    float m_deltaTime = 0.0f;
    float m_unscaledDeltaTime = 0.0f;
    float m_totalTime = 0.0f;
    float m_timeScale = 1.0f;
    float m_maxDeltaTime = 0.1f;  // 100ms max (prevents spiral of death)

    float m_fixedDeltaTime = 1.0f / 60.0f;  // 60 Hz physics
    float m_fixedAccumulator = 0.0f;

    // FPS calculation with smoothing
    float m_fps = 0.0f;
    float m_averageFPS = 0.0f;
    int m_fpsFrameCount = 0;
    float m_fpsTimer = 0.0f;

    std::uint64_t m_frameCount = 0;
};

/**
 * @brief RAII scoped timer for profiling code blocks
 *
 * Usage:
 *   {
 *       ScopedTimer timer("MyFunction");
 *       // ... code to time ...
 *   } // Logs elapsed time on destruction
 */
class ScopedTimer {
public:
    /**
     * @brief Start timing with a label
     * @param name Label for the timer (must outlive the ScopedTimer)
     */
    explicit ScopedTimer(std::string_view name) noexcept;

    /**
     * @brief Stop timing and log the result
     */
    ~ScopedTimer();

    // Non-copyable, non-movable - scoped resource
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

    /**
     * @brief Get elapsed time since construction
     */
    [[nodiscard]] float GetElapsedMs() const noexcept;

private:
    std::string_view m_name;
    Time::TimePoint m_startTime;
};

/**
 * @brief Manual stopwatch for timing operations
 *
 * Unlike ScopedTimer, can be started/stopped multiple times
 * and accumulates total time.
 */
class Stopwatch {
public:
    Stopwatch() noexcept = default;

    /**
     * @brief Start or resume timing
     */
    void Start() noexcept;

    /**
     * @brief Pause timing (can be resumed with Start)
     */
    void Stop() noexcept;

    /**
     * @brief Reset accumulated time and stop timing
     */
    void Reset() noexcept;

    /**
     * @brief Restart from zero (equivalent to Reset + Start)
     */
    void Restart() noexcept;

    /**
     * @brief Get total elapsed time in seconds
     */
    [[nodiscard]] float GetElapsedSeconds() const noexcept;

    /**
     * @brief Get total elapsed time in milliseconds
     */
    [[nodiscard]] float GetElapsedMilliseconds() const noexcept;

    /**
     * @brief Check if stopwatch is currently running
     */
    [[nodiscard]] bool IsRunning() const noexcept { return m_running; }

private:
    Time::TimePoint m_startTime{};
    Time::Duration m_accumulated{0};
    bool m_running = false;
};

} // namespace Nova
