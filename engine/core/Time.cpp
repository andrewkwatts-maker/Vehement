#include "core/Time.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace Nova {

Time::Time()
    : m_startTime(Clock::now())
    , m_lastFrameTime(m_startTime)
    , m_currentFrameTime(m_startTime)
{
}

void Time::Update() {
    m_currentFrameTime = Clock::now();

    // Calculate delta time
    Duration elapsed = m_currentFrameTime - m_lastFrameTime;
    m_unscaledDeltaTime = elapsed.count();

    // Clamp delta time to prevent huge jumps
    m_unscaledDeltaTime = std::min(m_unscaledDeltaTime, m_maxDeltaTime);

    // Apply time scale
    m_deltaTime = m_unscaledDeltaTime * m_timeScale;

    // Update total time
    Duration totalElapsed = m_currentFrameTime - m_startTime;
    m_totalTime = totalElapsed.count();

    // Accumulate for fixed updates
    m_fixedAccumulator += m_deltaTime;

    // FPS calculation
    m_fpsTimer += m_unscaledDeltaTime;
    m_fpsFrameCount++;

    if (m_unscaledDeltaTime > 0.0f) {
        m_fps = 1.0f / m_unscaledDeltaTime;
    }

    // Calculate average FPS over 1 second
    if (m_fpsTimer >= 1.0f) {
        m_averageFPS = static_cast<float>(m_fpsFrameCount) / m_fpsTimer;
        m_fpsFrameCount = 0;
        m_fpsTimer = 0.0f;
    }

    m_frameCount++;
    m_lastFrameTime = m_currentFrameTime;
}

bool Time::ShouldFixedUpdate() {
    if (m_fixedAccumulator >= m_fixedDeltaTime) {
        m_fixedAccumulator -= m_fixedDeltaTime;
        return true;
    }
    return false;
}

// ScopedTimer implementation
ScopedTimer::ScopedTimer(const char* name)
    : m_name(name)
    , m_startTime(Time::Clock::now())
{
}

ScopedTimer::~ScopedTimer() {
    float elapsed = GetElapsedMs();
    spdlog::debug("{}: {:.3f}ms", m_name, elapsed);
}

float ScopedTimer::GetElapsedMs() const {
    auto now = Time::Clock::now();
    auto duration = std::chrono::duration<float, std::milli>(now - m_startTime);
    return duration.count();
}

// Stopwatch implementation
void Stopwatch::Start() {
    if (!m_running) {
        m_startTime = Time::Clock::now();
        m_running = true;
    }
}

void Stopwatch::Stop() {
    if (m_running) {
        auto now = Time::Clock::now();
        m_accumulated += now - m_startTime;
        m_running = false;
    }
}

void Stopwatch::Reset() {
    m_accumulated = Time::Duration{0};
    m_running = false;
}

float Stopwatch::GetElapsedSeconds() const {
    auto total = m_accumulated;
    if (m_running) {
        total += Time::Clock::now() - m_startTime;
    }
    return total.count();
}

float Stopwatch::GetElapsedMilliseconds() const {
    return GetElapsedSeconds() * 1000.0f;
}

} // namespace Nova
