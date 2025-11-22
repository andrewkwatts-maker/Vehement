#pragma once

#include "KeyframeEditor.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace Vehement {

/**
 * @brief Playback state
 */
enum class PlaybackState {
    Stopped,
    Playing,
    Paused
};

/**
 * @brief Loop mode
 */
enum class LoopMode {
    Once,           // Play once and stop
    Loop,           // Loop continuously
    PingPong,       // Play forward then backward
    ClampForever    // Clamp to end and stay there
};

/**
 * @brief Time marker on timeline
 */
struct TimeMarker {
    std::string name;
    float time = 0.0f;
    glm::vec4 color{1.0f, 0.5f, 0.0f, 1.0f};
    bool locked = false;
};

/**
 * @brief Animation event marker
 */
struct EventMarker {
    std::string name;
    float time = 0.0f;
    std::string functionName;
    std::string parameter;
    glm::vec4 color{0.0f, 0.8f, 0.2f, 1.0f};
};

/**
 * @brief Audio sync marker
 */
struct AudioMarker {
    std::string name;
    std::string audioFile;
    float time = 0.0f;
    float duration = 0.0f;
    float volume = 1.0f;
    bool loop = false;
    glm::vec4 color{0.2f, 0.5f, 1.0f, 1.0f};
};

/**
 * @brief Timeline view settings
 */
struct TimelineViewSettings {
    float zoom = 1.0f;
    float scrollOffset = 0.0f;
    float pixelsPerSecond = 100.0f;
    bool showFrameNumbers = true;
    bool showTimeValues = true;
    bool showKeyframes = true;
    bool showEvents = true;
    bool showAudioMarkers = true;
    bool snapToGrid = true;
    float gridSubdivision = 0.1f;  // Grid line every 0.1 seconds at default zoom
};

/**
 * @brief Animation timeline control
 *
 * Features:
 * - Play/pause/stop controls
 * - Frame scrubbing
 * - Loop modes
 * - Playback speed
 * - Time markers
 * - Event markers
 * - Audio sync markers
 */
class AnimationTimeline {
public:
    struct Config {
        float minZoom = 0.1f;
        float maxZoom = 10.0f;
        float defaultFrameRate = 30.0f;
        glm::vec4 playheadColor{1.0f, 0.2f, 0.2f, 1.0f};
        glm::vec4 timelineBackground{0.15f, 0.15f, 0.18f, 1.0f};
        glm::vec4 gridColor{0.25f, 0.25f, 0.3f, 1.0f};
    };

    AnimationTimeline();
    ~AnimationTimeline();

    /**
     * @brief Initialize timeline
     */
    void Initialize(const Config& config = {});

    /**
     * @brief Set keyframe editor reference
     */
    void SetKeyframeEditor(KeyframeEditor* editor) { m_keyframeEditor = editor; }

    // =========================================================================
    // Playback Control
    // =========================================================================

    /**
     * @brief Start playback
     */
    void Play();

    /**
     * @brief Pause playback
     */
    void Pause();

    /**
     * @brief Stop and reset to start
     */
    void Stop();

    /**
     * @brief Toggle play/pause
     */
    void TogglePlayPause();

    /**
     * @brief Check if playing
     */
    [[nodiscard]] bool IsPlaying() const { return m_playbackState == PlaybackState::Playing; }

    /**
     * @brief Check if paused
     */
    [[nodiscard]] bool IsPaused() const { return m_playbackState == PlaybackState::Paused; }

    /**
     * @brief Check if stopped
     */
    [[nodiscard]] bool IsStopped() const { return m_playbackState == PlaybackState::Stopped; }

    /**
     * @brief Get playback state
     */
    [[nodiscard]] PlaybackState GetPlaybackState() const { return m_playbackState; }

    // =========================================================================
    // Time Control
    // =========================================================================

    /**
     * @brief Update timeline (call each frame)
     */
    void Update(float deltaTime);

    /**
     * @brief Set current time
     */
    void SetCurrentTime(float time);

    /**
     * @brief Get current time
     */
    [[nodiscard]] float GetCurrentTime() const { return m_currentTime; }

    /**
     * @brief Go to frame
     */
    void GoToFrame(int frame);

    /**
     * @brief Get current frame
     */
    [[nodiscard]] int GetCurrentFrame() const;

    /**
     * @brief Step forward one frame
     */
    void StepForward();

    /**
     * @brief Step backward one frame
     */
    void StepBackward();

    /**
     * @brief Go to start
     */
    void GoToStart();

    /**
     * @brief Go to end
     */
    void GoToEnd();

    /**
     * @brief Go to next keyframe
     */
    void GoToNextKeyframe();

    /**
     * @brief Go to previous keyframe
     */
    void GoToPreviousKeyframe();

    /**
     * @brief Go to next marker
     */
    void GoToNextMarker();

    /**
     * @brief Go to previous marker
     */
    void GoToPreviousMarker();

    // =========================================================================
    // Duration
    // =========================================================================

    /**
     * @brief Set animation duration
     */
    void SetDuration(float duration);

    /**
     * @brief Get animation duration
     */
    [[nodiscard]] float GetDuration() const { return m_duration; }

    /**
     * @brief Set frame rate
     */
    void SetFrameRate(float fps);

    /**
     * @brief Get frame rate
     */
    [[nodiscard]] float GetFrameRate() const { return m_frameRate; }

    /**
     * @brief Get total frame count
     */
    [[nodiscard]] int GetTotalFrames() const;

    // =========================================================================
    // Playback Settings
    // =========================================================================

    /**
     * @brief Set playback speed
     */
    void SetPlaybackSpeed(float speed) { m_playbackSpeed = speed; }

    /**
     * @brief Get playback speed
     */
    [[nodiscard]] float GetPlaybackSpeed() const { return m_playbackSpeed; }

    /**
     * @brief Set loop mode
     */
    void SetLoopMode(LoopMode mode) { m_loopMode = mode; }

    /**
     * @brief Get loop mode
     */
    [[nodiscard]] LoopMode GetLoopMode() const { return m_loopMode; }

    /**
     * @brief Set play range (for looping specific section)
     */
    void SetPlayRange(float start, float end);

    /**
     * @brief Clear play range
     */
    void ClearPlayRange();

    /**
     * @brief Check if play range is set
     */
    [[nodiscard]] bool HasPlayRange() const { return m_usePlayRange; }

    /**
     * @brief Get play range
     */
    [[nodiscard]] std::pair<float, float> GetPlayRange() const {
        return {m_playRangeStart, m_playRangeEnd};
    }

    // =========================================================================
    // Time Markers
    // =========================================================================

    /**
     * @brief Add time marker
     */
    TimeMarker* AddTimeMarker(const std::string& name, float time);

    /**
     * @brief Remove time marker
     */
    void RemoveTimeMarker(const std::string& name);

    /**
     * @brief Get time marker
     */
    [[nodiscard]] TimeMarker* GetTimeMarker(const std::string& name);

    /**
     * @brief Get all time markers
     */
    [[nodiscard]] const std::vector<TimeMarker>& GetTimeMarkers() const { return m_timeMarkers; }

    /**
     * @brief Move time marker
     */
    void MoveTimeMarker(const std::string& name, float newTime);

    // =========================================================================
    // Event Markers
    // =========================================================================

    /**
     * @brief Add event marker
     */
    EventMarker* AddEventMarker(const std::string& name, float time);

    /**
     * @brief Remove event marker
     */
    void RemoveEventMarker(const std::string& name);

    /**
     * @brief Get event marker
     */
    [[nodiscard]] EventMarker* GetEventMarker(const std::string& name);

    /**
     * @brief Get all event markers
     */
    [[nodiscard]] const std::vector<EventMarker>& GetEventMarkers() const { return m_eventMarkers; }

    /**
     * @brief Get events in time range
     */
    [[nodiscard]] std::vector<const EventMarker*> GetEventsInRange(float start, float end) const;

    // =========================================================================
    // Audio Markers
    // =========================================================================

    /**
     * @brief Add audio marker
     */
    AudioMarker* AddAudioMarker(const std::string& name, const std::string& audioFile, float time);

    /**
     * @brief Remove audio marker
     */
    void RemoveAudioMarker(const std::string& name);

    /**
     * @brief Get audio marker
     */
    [[nodiscard]] AudioMarker* GetAudioMarker(const std::string& name);

    /**
     * @brief Get all audio markers
     */
    [[nodiscard]] const std::vector<AudioMarker>& GetAudioMarkers() const { return m_audioMarkers; }

    /**
     * @brief Enable/disable audio playback
     */
    void SetAudioEnabled(bool enabled) { m_audioEnabled = enabled; }

    /**
     * @brief Check if audio is enabled
     */
    [[nodiscard]] bool IsAudioEnabled() const { return m_audioEnabled; }

    // =========================================================================
    // View Settings
    // =========================================================================

    /**
     * @brief Get view settings
     */
    [[nodiscard]] TimelineViewSettings& GetViewSettings() { return m_viewSettings; }
    [[nodiscard]] const TimelineViewSettings& GetViewSettings() const { return m_viewSettings; }

    /**
     * @brief Zoom in
     */
    void ZoomIn();

    /**
     * @brief Zoom out
     */
    void ZoomOut();

    /**
     * @brief Set zoom level
     */
    void SetZoom(float zoom);

    /**
     * @brief Zoom to fit animation
     */
    void ZoomToFit();

    /**
     * @brief Scroll timeline
     */
    void Scroll(float amount);

    /**
     * @brief Center view on time
     */
    void CenterOnTime(float time);

    // =========================================================================
    // Scrubbing
    // =========================================================================

    /**
     * @brief Begin scrubbing
     */
    void BeginScrub(float time);

    /**
     * @brief Update scrub position
     */
    void UpdateScrub(float time);

    /**
     * @brief End scrubbing
     */
    void EndScrub();

    /**
     * @brief Check if scrubbing
     */
    [[nodiscard]] bool IsScrubbing() const { return m_isScrubbing; }

    // =========================================================================
    // Coordinate Conversion
    // =========================================================================

    /**
     * @brief Convert screen X to time
     */
    [[nodiscard]] float ScreenToTime(float screenX) const;

    /**
     * @brief Convert time to screen X
     */
    [[nodiscard]] float TimeToScreen(float time) const;

    /**
     * @brief Snap time to grid
     */
    [[nodiscard]] float SnapToGrid(float time) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(float)> OnTimeChanged;
    std::function<void()> OnPlaybackStarted;
    std::function<void()> OnPlaybackStopped;
    std::function<void()> OnPlaybackPaused;
    std::function<void()> OnLooped;
    std::function<void(const EventMarker&)> OnEventTriggered;
    std::function<void(const AudioMarker&)> OnAudioTriggered;

private:
    void UpdatePlayback(float deltaTime);
    void FireEvents(float previousTime, float currentTime);
    void TriggerAudio(float previousTime, float currentTime);

    Config m_config;
    KeyframeEditor* m_keyframeEditor = nullptr;

    // Playback state
    PlaybackState m_playbackState = PlaybackState::Stopped;
    float m_currentTime = 0.0f;
    float m_duration = 1.0f;
    float m_frameRate = 30.0f;
    float m_playbackSpeed = 1.0f;
    LoopMode m_loopMode = LoopMode::Loop;
    bool m_playingForward = true;  // For ping-pong mode

    // Play range
    bool m_usePlayRange = false;
    float m_playRangeStart = 0.0f;
    float m_playRangeEnd = 1.0f;

    // Markers
    std::vector<TimeMarker> m_timeMarkers;
    std::vector<EventMarker> m_eventMarkers;
    std::vector<AudioMarker> m_audioMarkers;

    // Audio
    bool m_audioEnabled = true;

    // View
    TimelineViewSettings m_viewSettings;

    // Scrubbing
    bool m_isScrubbing = false;
    PlaybackState m_preScrubState = PlaybackState::Stopped;

    bool m_initialized = false;
};

} // namespace Vehement
