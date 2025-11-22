#pragma once

#include <chrono>

namespace Nova {

/**
 * @brief Time management class
 *
 * Tracks delta time, total time, and FPS.
 * Replaces the old Clock class with modern chrono-based timing.
 */
class Time {
public:
    using Clock = std::chrono::high_resolution_clock;
    using Duration = std::chrono::duration<float>;
    using TimePoint = Clock::time_point;

    Time();

    /**
     * @brief Update time calculations (call once per frame)
     */
    void Update();

    /**
     * @brief Get time since last frame in seconds
     */
    float GetDeltaTime() const { return m_deltaTime; }

    /**
     * @brief Get unscaled delta time (ignores time scale)
     */
    float GetUnscaledDeltaTime() const { return m_unscaledDeltaTime; }

    /**
     * @brief Get total time since engine start in seconds
     */
    float GetTotalTime() const { return m_totalTime; }

    /**
     * @brief Get current frames per second
     */
    float GetFPS() const { return m_fps; }

    /**
     * @brief Get average FPS over last second
     */
    float GetAverageFPS() const { return m_averageFPS; }

    /**
     * @brief Get current frame number
     */
    uint64_t GetFrameCount() const { return m_frameCount; }

    /**
     * @brief Set time scale (1.0 = normal, 0.5 = half speed, etc.)
     */
    void SetTimeScale(float scale) { m_timeScale = scale; }

    /**
     * @brief Get current time scale
     */
    float GetTimeScale() const { return m_timeScale; }

    /**
     * @brief Set maximum delta time (prevents large jumps on lag)
     */
    void SetMaxDeltaTime(float maxDelta) { m_maxDeltaTime = maxDelta; }

    /**
     * @brief Get fixed timestep value
     */
    float GetFixedDeltaTime() const { return m_fixedDeltaTime; }

    /**
     * @brief Set fixed timestep for physics
     */
    void SetFixedDeltaTime(float dt) { m_fixedDeltaTime = dt; }

    /**
     * @brief Get accumulated time for fixed updates
     */
    float GetFixedAccumulator() const { return m_fixedAccumulator; }

    /**
     * @brief Check if a fixed update should occur
     */
    bool ShouldFixedUpdate();

private:
    TimePoint m_startTime;
    TimePoint m_lastFrameTime;
    TimePoint m_currentFrameTime;

    float m_deltaTime = 0.0f;
    float m_unscaledDeltaTime = 0.0f;
    float m_totalTime = 0.0f;
    float m_timeScale = 1.0f;
    float m_maxDeltaTime = 0.1f;  // 100ms max

    float m_fixedDeltaTime = 1.0f / 60.0f;
    float m_fixedAccumulator = 0.0f;

    // FPS calculation
    float m_fps = 0.0f;
    float m_averageFPS = 0.0f;
    float m_fpsAccumulator = 0.0f;
    int m_fpsFrameCount = 0;
    float m_fpsTimer = 0.0f;

    uint64_t m_frameCount = 0;
};

/**
 * @brief Scoped timer for profiling
 */
class ScopedTimer {
public:
    explicit ScopedTimer(const char* name);
    ~ScopedTimer();

    float GetElapsedMs() const;

private:
    const char* m_name;
    Time::TimePoint m_startTime;
};

/**
 * @brief Simple stopwatch for manual timing
 */
class Stopwatch {
public:
    void Start();
    void Stop();
    void Reset();

    float GetElapsedSeconds() const;
    float GetElapsedMilliseconds() const;

    bool IsRunning() const { return m_running; }

private:
    Time::TimePoint m_startTime;
    Time::Duration m_accumulated{0};
    bool m_running = false;
};

} // namespace Nova
