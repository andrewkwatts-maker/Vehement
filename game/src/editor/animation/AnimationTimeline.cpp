#include "AnimationTimeline.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {

AnimationTimeline::AnimationTimeline() = default;
AnimationTimeline::~AnimationTimeline() = default;

void AnimationTimeline::Initialize(const Config& config) {
    m_config = config;
    m_frameRate = config.defaultFrameRate;
    m_initialized = true;
}

// ============================================================================
// Playback Control
// ============================================================================

void AnimationTimeline::Play() {
    if (m_playbackState != PlaybackState::Playing) {
        m_playbackState = PlaybackState::Playing;

        // If at end in non-looping mode, restart
        if (m_currentTime >= m_duration && m_loopMode == LoopMode::Once) {
            m_currentTime = 0.0f;
        }

        if (OnPlaybackStarted) {
            OnPlaybackStarted();
        }
    }
}

void AnimationTimeline::Pause() {
    if (m_playbackState == PlaybackState::Playing) {
        m_playbackState = PlaybackState::Paused;

        if (OnPlaybackPaused) {
            OnPlaybackPaused();
        }
    }
}

void AnimationTimeline::Stop() {
    m_playbackState = PlaybackState::Stopped;
    m_currentTime = 0.0f;
    m_playingForward = true;

    if (OnPlaybackStopped) {
        OnPlaybackStopped();
    }

    if (OnTimeChanged) {
        OnTimeChanged(m_currentTime);
    }
}

void AnimationTimeline::TogglePlayPause() {
    if (m_playbackState == PlaybackState::Playing) {
        Pause();
    } else {
        Play();
    }
}

// ============================================================================
// Time Control
// ============================================================================

void AnimationTimeline::Update(float deltaTime) {
    if (m_playbackState == PlaybackState::Playing) {
        UpdatePlayback(deltaTime);
    }
}

void AnimationTimeline::UpdatePlayback(float deltaTime) {
    float previousTime = m_currentTime;

    // Calculate time step based on speed and direction
    float timeStep = deltaTime * m_playbackSpeed;
    if (!m_playingForward) {
        timeStep = -timeStep;
    }

    m_currentTime += timeStep;

    // Get effective range
    float rangeStart = m_usePlayRange ? m_playRangeStart : 0.0f;
    float rangeEnd = m_usePlayRange ? m_playRangeEnd : m_duration;

    // Handle looping
    switch (m_loopMode) {
        case LoopMode::Once:
            if (m_currentTime >= rangeEnd) {
                m_currentTime = rangeEnd;
                Pause();
            } else if (m_currentTime <= rangeStart) {
                m_currentTime = rangeStart;
                Pause();
            }
            break;

        case LoopMode::Loop:
            if (m_currentTime >= rangeEnd) {
                m_currentTime = rangeStart + std::fmod(m_currentTime - rangeStart, rangeEnd - rangeStart);
                if (OnLooped) {
                    OnLooped();
                }
            } else if (m_currentTime < rangeStart) {
                m_currentTime = rangeEnd - std::fmod(rangeStart - m_currentTime, rangeEnd - rangeStart);
                if (OnLooped) {
                    OnLooped();
                }
            }
            break;

        case LoopMode::PingPong:
            if (m_currentTime >= rangeEnd) {
                m_currentTime = rangeEnd - (m_currentTime - rangeEnd);
                m_playingForward = false;
                if (OnLooped) {
                    OnLooped();
                }
            } else if (m_currentTime <= rangeStart) {
                m_currentTime = rangeStart + (rangeStart - m_currentTime);
                m_playingForward = true;
                if (OnLooped) {
                    OnLooped();
                }
            }
            break;

        case LoopMode::ClampForever:
            m_currentTime = std::clamp(m_currentTime, rangeStart, rangeEnd);
            break;
    }

    // Fire events
    FireEvents(previousTime, m_currentTime);

    // Trigger audio
    TriggerAudio(previousTime, m_currentTime);

    // Sample animation
    if (m_keyframeEditor) {
        m_keyframeEditor->SampleAnimation(m_currentTime);
    }

    if (OnTimeChanged) {
        OnTimeChanged(m_currentTime);
    }
}

void AnimationTimeline::SetCurrentTime(float time) {
    float previousTime = m_currentTime;
    m_currentTime = std::clamp(time, 0.0f, m_duration);

    // Sample animation
    if (m_keyframeEditor) {
        m_keyframeEditor->SampleAnimation(m_currentTime);
    }

    if (OnTimeChanged && std::abs(previousTime - m_currentTime) > 0.0001f) {
        OnTimeChanged(m_currentTime);
    }
}

void AnimationTimeline::GoToFrame(int frame) {
    SetCurrentTime(static_cast<float>(frame) / m_frameRate);
}

int AnimationTimeline::GetCurrentFrame() const {
    return static_cast<int>(std::round(m_currentTime * m_frameRate));
}

void AnimationTimeline::StepForward() {
    GoToFrame(GetCurrentFrame() + 1);
}

void AnimationTimeline::StepBackward() {
    GoToFrame(std::max(0, GetCurrentFrame() - 1));
}

void AnimationTimeline::GoToStart() {
    SetCurrentTime(m_usePlayRange ? m_playRangeStart : 0.0f);
}

void AnimationTimeline::GoToEnd() {
    SetCurrentTime(m_usePlayRange ? m_playRangeEnd : m_duration);
}

void AnimationTimeline::GoToNextKeyframe() {
    if (!m_keyframeEditor) return;

    float nextTime = m_duration;

    for (const auto& track : m_keyframeEditor->GetTracks()) {
        for (const auto& kf : track.keyframes) {
            if (kf.time > m_currentTime + 0.0001f && kf.time < nextTime) {
                nextTime = kf.time;
            }
        }
    }

    SetCurrentTime(nextTime);
}

void AnimationTimeline::GoToPreviousKeyframe() {
    if (!m_keyframeEditor) return;

    float prevTime = 0.0f;

    for (const auto& track : m_keyframeEditor->GetTracks()) {
        for (const auto& kf : track.keyframes) {
            if (kf.time < m_currentTime - 0.0001f && kf.time > prevTime) {
                prevTime = kf.time;
            }
        }
    }

    SetCurrentTime(prevTime);
}

void AnimationTimeline::GoToNextMarker() {
    float nextTime = m_duration;

    for (const auto& marker : m_timeMarkers) {
        if (marker.time > m_currentTime + 0.0001f && marker.time < nextTime) {
            nextTime = marker.time;
        }
    }

    for (const auto& marker : m_eventMarkers) {
        if (marker.time > m_currentTime + 0.0001f && marker.time < nextTime) {
            nextTime = marker.time;
        }
    }

    SetCurrentTime(nextTime);
}

void AnimationTimeline::GoToPreviousMarker() {
    float prevTime = 0.0f;

    for (const auto& marker : m_timeMarkers) {
        if (marker.time < m_currentTime - 0.0001f && marker.time > prevTime) {
            prevTime = marker.time;
        }
    }

    for (const auto& marker : m_eventMarkers) {
        if (marker.time < m_currentTime - 0.0001f && marker.time > prevTime) {
            prevTime = marker.time;
        }
    }

    SetCurrentTime(prevTime);
}

// ============================================================================
// Duration
// ============================================================================

void AnimationTimeline::SetDuration(float duration) {
    m_duration = std::max(0.001f, duration);

    if (m_currentTime > m_duration) {
        SetCurrentTime(m_duration);
    }

    if (m_keyframeEditor) {
        m_keyframeEditor->SetDuration(m_duration);
    }
}

void AnimationTimeline::SetFrameRate(float fps) {
    m_frameRate = std::max(1.0f, fps);

    if (m_keyframeEditor) {
        m_keyframeEditor->SetFrameRate(m_frameRate);
    }
}

int AnimationTimeline::GetTotalFrames() const {
    return static_cast<int>(std::ceil(m_duration * m_frameRate));
}

// ============================================================================
// Playback Settings
// ============================================================================

void AnimationTimeline::SetPlayRange(float start, float end) {
    m_playRangeStart = std::max(0.0f, std::min(start, end));
    m_playRangeEnd = std::min(m_duration, std::max(start, end));
    m_usePlayRange = true;
}

void AnimationTimeline::ClearPlayRange() {
    m_usePlayRange = false;
    m_playRangeStart = 0.0f;
    m_playRangeEnd = m_duration;
}

// ============================================================================
// Time Markers
// ============================================================================

TimeMarker* AnimationTimeline::AddTimeMarker(const std::string& name, float time) {
    // Check for existing marker
    for (auto& marker : m_timeMarkers) {
        if (marker.name == name) {
            marker.time = time;
            return &marker;
        }
    }

    TimeMarker marker;
    marker.name = name;
    marker.time = std::clamp(time, 0.0f, m_duration);
    m_timeMarkers.push_back(marker);

    // Sort by time
    std::sort(m_timeMarkers.begin(), m_timeMarkers.end(),
        [](const TimeMarker& a, const TimeMarker& b) { return a.time < b.time; });

    return &m_timeMarkers.back();
}

void AnimationTimeline::RemoveTimeMarker(const std::string& name) {
    m_timeMarkers.erase(
        std::remove_if(m_timeMarkers.begin(), m_timeMarkers.end(),
            [&name](const TimeMarker& m) { return m.name == name; }),
        m_timeMarkers.end()
    );
}

TimeMarker* AnimationTimeline::GetTimeMarker(const std::string& name) {
    for (auto& marker : m_timeMarkers) {
        if (marker.name == name) {
            return &marker;
        }
    }
    return nullptr;
}

void AnimationTimeline::MoveTimeMarker(const std::string& name, float newTime) {
    if (auto* marker = GetTimeMarker(name)) {
        if (!marker->locked) {
            marker->time = std::clamp(newTime, 0.0f, m_duration);

            // Re-sort
            std::sort(m_timeMarkers.begin(), m_timeMarkers.end(),
                [](const TimeMarker& a, const TimeMarker& b) { return a.time < b.time; });
        }
    }
}

// ============================================================================
// Event Markers
// ============================================================================

EventMarker* AnimationTimeline::AddEventMarker(const std::string& name, float time) {
    EventMarker marker;
    marker.name = name;
    marker.time = std::clamp(time, 0.0f, m_duration);
    m_eventMarkers.push_back(marker);

    std::sort(m_eventMarkers.begin(), m_eventMarkers.end(),
        [](const EventMarker& a, const EventMarker& b) { return a.time < b.time; });

    return &m_eventMarkers.back();
}

void AnimationTimeline::RemoveEventMarker(const std::string& name) {
    m_eventMarkers.erase(
        std::remove_if(m_eventMarkers.begin(), m_eventMarkers.end(),
            [&name](const EventMarker& m) { return m.name == name; }),
        m_eventMarkers.end()
    );
}

EventMarker* AnimationTimeline::GetEventMarker(const std::string& name) {
    for (auto& marker : m_eventMarkers) {
        if (marker.name == name) {
            return &marker;
        }
    }
    return nullptr;
}

std::vector<const EventMarker*> AnimationTimeline::GetEventsInRange(float start, float end) const {
    std::vector<const EventMarker*> events;

    for (const auto& marker : m_eventMarkers) {
        if (marker.time >= start && marker.time <= end) {
            events.push_back(&marker);
        }
    }

    return events;
}

// ============================================================================
// Audio Markers
// ============================================================================

AudioMarker* AnimationTimeline::AddAudioMarker(const std::string& name, const std::string& audioFile, float time) {
    AudioMarker marker;
    marker.name = name;
    marker.audioFile = audioFile;
    marker.time = std::clamp(time, 0.0f, m_duration);
    m_audioMarkers.push_back(marker);

    std::sort(m_audioMarkers.begin(), m_audioMarkers.end(),
        [](const AudioMarker& a, const AudioMarker& b) { return a.time < b.time; });

    return &m_audioMarkers.back();
}

void AnimationTimeline::RemoveAudioMarker(const std::string& name) {
    m_audioMarkers.erase(
        std::remove_if(m_audioMarkers.begin(), m_audioMarkers.end(),
            [&name](const AudioMarker& m) { return m.name == name; }),
        m_audioMarkers.end()
    );
}

AudioMarker* AnimationTimeline::GetAudioMarker(const std::string& name) {
    for (auto& marker : m_audioMarkers) {
        if (marker.name == name) {
            return &marker;
        }
    }
    return nullptr;
}

// ============================================================================
// View Settings
// ============================================================================

void AnimationTimeline::ZoomIn() {
    SetZoom(m_viewSettings.zoom * 1.2f);
}

void AnimationTimeline::ZoomOut() {
    SetZoom(m_viewSettings.zoom / 1.2f);
}

void AnimationTimeline::SetZoom(float zoom) {
    m_viewSettings.zoom = std::clamp(zoom, m_config.minZoom, m_config.maxZoom);
}

void AnimationTimeline::ZoomToFit() {
    // Would need viewport width to calculate properly
    // For now, set zoom to show full animation
    m_viewSettings.zoom = 1.0f;
    m_viewSettings.scrollOffset = 0.0f;
}

void AnimationTimeline::Scroll(float amount) {
    m_viewSettings.scrollOffset = std::max(0.0f, m_viewSettings.scrollOffset + amount);
}

void AnimationTimeline::CenterOnTime(float time) {
    // Would need viewport width for proper calculation
    m_viewSettings.scrollOffset = std::max(0.0f, time - 0.5f / m_viewSettings.zoom);
}

// ============================================================================
// Scrubbing
// ============================================================================

void AnimationTimeline::BeginScrub(float time) {
    m_isScrubbing = true;
    m_preScrubState = m_playbackState;

    if (m_playbackState == PlaybackState::Playing) {
        m_playbackState = PlaybackState::Paused;
    }

    SetCurrentTime(time);
}

void AnimationTimeline::UpdateScrub(float time) {
    if (m_isScrubbing) {
        SetCurrentTime(time);
    }
}

void AnimationTimeline::EndScrub() {
    m_isScrubbing = false;

    // Optionally restore playback state
    // m_playbackState = m_preScrubState;
}

// ============================================================================
// Coordinate Conversion
// ============================================================================

float AnimationTimeline::ScreenToTime(float screenX) const {
    return (screenX / (m_viewSettings.pixelsPerSecond * m_viewSettings.zoom)) + m_viewSettings.scrollOffset;
}

float AnimationTimeline::TimeToScreen(float time) const {
    return (time - m_viewSettings.scrollOffset) * m_viewSettings.pixelsPerSecond * m_viewSettings.zoom;
}

float AnimationTimeline::SnapToGrid(float time) const {
    if (!m_viewSettings.snapToGrid) {
        return time;
    }

    float gridSize = m_viewSettings.gridSubdivision / m_viewSettings.zoom;
    return std::round(time / gridSize) * gridSize;
}

// ============================================================================
// Private Methods
// ============================================================================

void AnimationTimeline::FireEvents(float previousTime, float currentTime) {
    if (!OnEventTriggered) return;

    // Handle wrap-around for looping
    if (currentTime < previousTime && m_loopMode == LoopMode::Loop) {
        // Fire events from previousTime to end, then from start to currentTime
        float rangeEnd = m_usePlayRange ? m_playRangeEnd : m_duration;
        float rangeStart = m_usePlayRange ? m_playRangeStart : 0.0f;

        for (const auto& marker : m_eventMarkers) {
            if ((marker.time > previousTime && marker.time <= rangeEnd) ||
                (marker.time >= rangeStart && marker.time <= currentTime)) {
                OnEventTriggered(marker);
            }
        }
    } else {
        float minTime = std::min(previousTime, currentTime);
        float maxTime = std::max(previousTime, currentTime);

        for (const auto& marker : m_eventMarkers) {
            if (marker.time > minTime && marker.time <= maxTime) {
                OnEventTriggered(marker);
            }
        }
    }
}

void AnimationTimeline::TriggerAudio(float previousTime, float currentTime) {
    if (!m_audioEnabled || !OnAudioTriggered) return;

    float minTime = std::min(previousTime, currentTime);
    float maxTime = std::max(previousTime, currentTime);

    for (const auto& marker : m_audioMarkers) {
        if (marker.time > minTime && marker.time <= maxTime) {
            OnAudioTriggered(marker);
        }
    }
}

} // namespace Vehement
