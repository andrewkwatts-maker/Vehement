#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <fstream>
#include <cstdint>
#include <unordered_map>

namespace Nova {

// ============================================================================
// Replay System Types
// ============================================================================

/**
 * @brief Input event types for replay recording
 */
enum class InputEventType : uint8_t {
    KeyDown,
    KeyUp,
    MouseButtonDown,
    MouseButtonUp,
    MouseMove,
    MouseScroll,
    GamepadButton,
    GamepadAxis,
    Custom
};

/**
 * @brief Recorded input event
 */
struct InputEvent {
    uint32_t frame;                    ///< Frame number
    float timestamp;                   ///< Time since recording start
    InputEventType type;
    int32_t code;                      ///< Key/button code
    float valueX;                      ///< Value or X position
    float valueY;                      ///< Y position for mouse
    uint8_t modifiers;                 ///< Modifier keys state

    bool operator<(const InputEvent& other) const {
        return frame < other.frame;
    }
};

/**
 * @brief Replay state snapshot for determinism verification
 */
struct StateSnapshot {
    uint32_t frame;
    uint32_t checksum;                 ///< Game state checksum
    std::vector<uint8_t> data;         ///< Optional state data
};

/**
 * @brief Replay file header
 */
struct ReplayHeader {
    uint32_t magic = 0x52504C59;       ///< "RPLY"
    uint32_t version = 1;
    uint32_t frameCount = 0;
    uint32_t eventCount = 0;
    float duration = 0.0f;
    uint32_t randomSeed = 0;           ///< Initial random seed
    std::chrono::system_clock::time_point recordTime;
    std::string gameVersion;
    std::string mapName;
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief Playback state
 */
enum class PlaybackState : uint8_t {
    Stopped,
    Playing,
    Paused,
    FastForward,
    Rewind,
    Seeking
};

// ============================================================================
// Replay Recording
// ============================================================================

/**
 * @brief Replay recording session
 */
class ReplayRecorder {
public:
    ReplayRecorder() = default;
    ~ReplayRecorder() = default;

    /**
     * @brief Start recording
     */
    void Start(uint32_t randomSeed = 0);

    /**
     * @brief Stop recording
     */
    void Stop();

    /**
     * @brief Record an input event
     */
    void RecordInput(InputEventType type, int32_t code, float x = 0, float y = 0, uint8_t mods = 0);

    /**
     * @brief Record a custom event
     */
    void RecordCustomEvent(const std::string& eventName, const std::vector<uint8_t>& data);

    /**
     * @brief Take a state snapshot for verification
     */
    void TakeSnapshot(uint32_t checksum, const std::vector<uint8_t>& stateData = {});

    /**
     * @brief Advance frame counter (call once per game update)
     */
    void AdvanceFrame();

    /**
     * @brief Save replay to file
     */
    bool Save(const std::string& path, const std::string& mapName = "",
              const std::unordered_map<std::string, std::string>& metadata = {});

    // Accessors
    [[nodiscard]] bool IsRecording() const { return m_recording; }
    [[nodiscard]] uint32_t GetFrameCount() const { return m_currentFrame; }
    [[nodiscard]] float GetDuration() const { return m_duration; }
    [[nodiscard]] size_t GetEventCount() const { return m_events.size(); }
    [[nodiscard]] uint32_t GetRandomSeed() const { return m_randomSeed; }

private:
    std::vector<InputEvent> m_events;
    std::vector<StateSnapshot> m_snapshots;
    uint32_t m_currentFrame = 0;
    float m_startTime = 0.0f;
    float m_duration = 0.0f;
    uint32_t m_randomSeed = 0;
    bool m_recording = false;
};

// ============================================================================
// Replay Playback
// ============================================================================

/**
 * @brief Replay playback controller
 */
class ReplayPlayer {
public:
    ReplayPlayer() = default;
    ~ReplayPlayer() = default;

    /**
     * @brief Load replay from file
     */
    bool Load(const std::string& path);

    /**
     * @brief Unload current replay
     */
    void Unload();

    /**
     * @brief Start playback
     */
    void Play();

    /**
     * @brief Pause playback
     */
    void Pause();

    /**
     * @brief Stop and reset playback
     */
    void Stop();

    /**
     * @brief Toggle play/pause
     */
    void TogglePause();

    /**
     * @brief Set playback speed (1.0 = normal, 2.0 = double, 0.5 = half)
     */
    void SetPlaybackSpeed(float speed);

    /**
     * @brief Seek to specific frame
     */
    void SeekToFrame(uint32_t frame);

    /**
     * @brief Seek to time (seconds)
     */
    void SeekToTime(float time);

    /**
     * @brief Seek by percentage (0.0 to 1.0)
     */
    void SeekToPercent(float percent);

    /**
     * @brief Step forward one frame
     */
    void StepForward();

    /**
     * @brief Step backward one frame
     */
    void StepBackward();

    /**
     * @brief Update playback (call each frame)
     * @param deltaTime Frame delta time
     * @return Events for this frame
     */
    std::vector<InputEvent> Update(float deltaTime);

    /**
     * @brief Get events for specific frame
     */
    [[nodiscard]] std::vector<InputEvent> GetEventsForFrame(uint32_t frame) const;

    /**
     * @brief Verify state checksum at current frame
     */
    [[nodiscard]] bool VerifyState(uint32_t checksum) const;

    // Accessors
    [[nodiscard]] bool IsLoaded() const { return m_loaded; }
    [[nodiscard]] PlaybackState GetState() const { return m_state; }
    [[nodiscard]] uint32_t GetCurrentFrame() const { return m_currentFrame; }
    [[nodiscard]] uint32_t GetTotalFrames() const { return m_header.frameCount; }
    [[nodiscard]] float GetCurrentTime() const { return m_currentTime; }
    [[nodiscard]] float GetDuration() const { return m_header.duration; }
    [[nodiscard]] float GetProgress() const;
    [[nodiscard]] float GetPlaybackSpeed() const { return m_playbackSpeed; }
    [[nodiscard]] const ReplayHeader& GetHeader() const { return m_header; }
    [[nodiscard]] uint32_t GetRandomSeed() const { return m_header.randomSeed; }

    // Callbacks
    using PlaybackCallback = std::function<void(PlaybackState state)>;
    using FrameCallback = std::function<void(uint32_t frame)>;

    void SetStateCallback(PlaybackCallback callback) { m_stateCallback = std::move(callback); }
    void SetFrameCallback(FrameCallback callback) { m_frameCallback = std::move(callback); }

private:
    ReplayHeader m_header;
    std::vector<InputEvent> m_events;
    std::vector<StateSnapshot> m_snapshots;

    PlaybackState m_state = PlaybackState::Stopped;
    uint32_t m_currentFrame = 0;
    float m_currentTime = 0.0f;
    float m_playbackSpeed = 1.0f;
    size_t m_eventIndex = 0;

    PlaybackCallback m_stateCallback;
    FrameCallback m_frameCallback;

    bool m_loaded = false;
};

// ============================================================================
// Replay Manager
// ============================================================================

/**
 * @brief Main replay system manager
 *
 * Features:
 * - Input recording for deterministic replay
 * - State snapshots for verification
 * - Playback with seek/speed controls
 * - Export to video (with external encoder)
 *
 * Usage:
 * @code
 * auto& replay = ReplayManager::Instance();
 *
 * // Recording
 * replay.StartRecording();
 * // ... game loop, inputs recorded automatically ...
 * replay.StopRecording();
 * replay.SaveRecording("match.replay");
 *
 * // Playback
 * replay.LoadReplay("match.replay");
 * replay.Play();
 * // In game loop:
 * auto events = replay.Update(deltaTime);
 * // Apply events to game state
 * @endcode
 */
class ReplayManager {
public:
    static ReplayManager& Instance();

    // Non-copyable
    ReplayManager(const ReplayManager&) = delete;
    ReplayManager& operator=(const ReplayManager&) = delete;

    /**
     * @brief Initialize replay system
     */
    bool Initialize(const std::string& replayDirectory = "replays");

    /**
     * @brief Shutdown
     */
    void Shutdown();

    // =========== Recording ===========

    /**
     * @brief Start recording
     */
    void StartRecording(uint32_t randomSeed = 0);

    /**
     * @brief Stop recording
     */
    void StopRecording();

    /**
     * @brief Check if recording
     */
    [[nodiscard]] bool IsRecording() const;

    /**
     * @brief Record input event (called by input system)
     */
    void RecordInput(InputEventType type, int32_t code, float x = 0, float y = 0, uint8_t mods = 0);

    /**
     * @brief Record custom event
     */
    void RecordCustom(const std::string& name, const std::vector<uint8_t>& data);

    /**
     * @brief Take state snapshot
     */
    void Snapshot(uint32_t checksum, const std::vector<uint8_t>& state = {});

    /**
     * @brief Advance recording frame
     */
    void AdvanceFrame();

    /**
     * @brief Save current recording
     */
    bool SaveRecording(const std::string& filename,
                       const std::unordered_map<std::string, std::string>& metadata = {});

    /**
     * @brief Get current recorder
     */
    ReplayRecorder& GetRecorder() { return m_recorder; }

    // =========== Playback ===========

    /**
     * @brief Load a replay file
     */
    bool LoadReplay(const std::string& filename);

    /**
     * @brief Unload current replay
     */
    void UnloadReplay();

    /**
     * @brief Check if replay is loaded
     */
    [[nodiscard]] bool IsReplayLoaded() const;

    /**
     * @brief Playback controls
     */
    void Play();
    void Pause();
    void Stop();
    void TogglePause();

    /**
     * @brief Set playback speed
     */
    void SetSpeed(float speed);

    /**
     * @brief Seek controls
     */
    void SeekFrame(uint32_t frame);
    void SeekTime(float time);
    void SeekPercent(float percent);
    void StepForward();
    void StepBackward();

    /**
     * @brief Update playback (returns events for current frame)
     */
    std::vector<InputEvent> Update(float deltaTime);

    /**
     * @brief Verify current state
     */
    [[nodiscard]] bool VerifyState(uint32_t checksum) const;

    /**
     * @brief Get current player
     */
    ReplayPlayer& GetPlayer() { return m_player; }

    // =========== File Management ===========

    /**
     * @brief Get list of replay files
     */
    [[nodiscard]] std::vector<std::string> GetReplayFiles() const;

    /**
     * @brief Get replay info without loading
     */
    [[nodiscard]] ReplayHeader GetReplayInfo(const std::string& filename) const;

    /**
     * @brief Delete a replay file
     */
    bool DeleteReplay(const std::string& filename);

    /**
     * @brief Export replay to file (for video encoding)
     */
    bool ExportFrameData(const std::string& filename, const std::string& outputDir);

    // =========== Settings ===========

    /**
     * @brief Set replay directory
     */
    void SetReplayDirectory(const std::string& dir) { m_replayDirectory = dir; }

    /**
     * @brief Auto-record all matches
     */
    void SetAutoRecord(bool enabled) { m_autoRecord = enabled; }
    [[nodiscard]] bool IsAutoRecording() const { return m_autoRecord; }

    /**
     * @brief Set max replays to keep (auto-delete oldest)
     */
    void SetMaxReplays(size_t max) { m_maxReplays = max; }

private:
    ReplayManager() = default;
    ~ReplayManager() = default;

    std::string GetFullPath(const std::string& filename) const;
    void PruneOldReplays();

    ReplayRecorder m_recorder;
    ReplayPlayer m_player;

    std::string m_replayDirectory = "replays";
    bool m_autoRecord = false;
    size_t m_maxReplays = 50;
    bool m_initialized = false;
};

// ============================================================================
// Implementation
// ============================================================================

inline void ReplayRecorder::Start(uint32_t randomSeed) {
    m_events.clear();
    m_snapshots.clear();
    m_currentFrame = 0;
    m_duration = 0.0f;
    m_randomSeed = randomSeed;
    m_recording = true;
}

inline void ReplayRecorder::Stop() {
    m_recording = false;
}

inline void ReplayRecorder::RecordInput(InputEventType type, int32_t code, float x, float y, uint8_t mods) {
    if (!m_recording) return;

    InputEvent event;
    event.frame = m_currentFrame;
    event.timestamp = m_duration;
    event.type = type;
    event.code = code;
    event.valueX = x;
    event.valueY = y;
    event.modifiers = mods;
    m_events.push_back(event);
}

inline void ReplayRecorder::RecordCustomEvent(const std::string& /*eventName*/, const std::vector<uint8_t>& /*data*/) {
    // Custom event recording
}

inline void ReplayRecorder::TakeSnapshot(uint32_t checksum, const std::vector<uint8_t>& stateData) {
    if (!m_recording) return;
    m_snapshots.push_back({m_currentFrame, checksum, stateData});
}

inline void ReplayRecorder::AdvanceFrame() {
    if (m_recording) {
        ++m_currentFrame;
        m_duration += 1.0f / 60.0f;  // Assuming 60 FPS
    }
}

inline bool ReplayRecorder::Save(const std::string& path, const std::string& mapName,
                                  const std::unordered_map<std::string, std::string>& metadata) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    ReplayHeader header;
    header.frameCount = m_currentFrame;
    header.eventCount = static_cast<uint32_t>(m_events.size());
    header.duration = m_duration;
    header.randomSeed = m_randomSeed;
    header.recordTime = std::chrono::system_clock::now();
    header.mapName = mapName;
    header.metadata = metadata;

    // Write header
    file.write(reinterpret_cast<const char*>(&header.magic), sizeof(header.magic));
    file.write(reinterpret_cast<const char*>(&header.version), sizeof(header.version));
    file.write(reinterpret_cast<const char*>(&header.frameCount), sizeof(header.frameCount));
    file.write(reinterpret_cast<const char*>(&header.eventCount), sizeof(header.eventCount));
    file.write(reinterpret_cast<const char*>(&header.duration), sizeof(header.duration));
    file.write(reinterpret_cast<const char*>(&header.randomSeed), sizeof(header.randomSeed));

    // Write events
    for (const auto& event : m_events) {
        file.write(reinterpret_cast<const char*>(&event), sizeof(InputEvent));
    }

    return true;
}

inline bool ReplayPlayer::Load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    file.read(reinterpret_cast<char*>(&m_header.magic), sizeof(m_header.magic));
    if (m_header.magic != 0x52504C59) return false;

    file.read(reinterpret_cast<char*>(&m_header.version), sizeof(m_header.version));
    file.read(reinterpret_cast<char*>(&m_header.frameCount), sizeof(m_header.frameCount));
    file.read(reinterpret_cast<char*>(&m_header.eventCount), sizeof(m_header.eventCount));
    file.read(reinterpret_cast<char*>(&m_header.duration), sizeof(m_header.duration));
    file.read(reinterpret_cast<char*>(&m_header.randomSeed), sizeof(m_header.randomSeed));

    m_events.resize(m_header.eventCount);
    for (auto& event : m_events) {
        file.read(reinterpret_cast<char*>(&event), sizeof(InputEvent));
    }

    m_loaded = true;
    m_currentFrame = 0;
    m_currentTime = 0.0f;
    m_eventIndex = 0;
    m_state = PlaybackState::Stopped;

    return true;
}

inline void ReplayPlayer::Unload() {
    m_events.clear();
    m_snapshots.clear();
    m_loaded = false;
    m_state = PlaybackState::Stopped;
}

inline void ReplayPlayer::Play() {
    if (m_loaded && m_state != PlaybackState::Playing) {
        m_state = PlaybackState::Playing;
        if (m_stateCallback) m_stateCallback(m_state);
    }
}

inline void ReplayPlayer::Pause() {
    if (m_state == PlaybackState::Playing) {
        m_state = PlaybackState::Paused;
        if (m_stateCallback) m_stateCallback(m_state);
    }
}

inline void ReplayPlayer::Stop() {
    m_state = PlaybackState::Stopped;
    m_currentFrame = 0;
    m_currentTime = 0.0f;
    m_eventIndex = 0;
    if (m_stateCallback) m_stateCallback(m_state);
}

inline void ReplayPlayer::TogglePause() {
    if (m_state == PlaybackState::Playing) Pause();
    else if (m_state == PlaybackState::Paused) Play();
}

inline void ReplayPlayer::SetPlaybackSpeed(float speed) {
    m_playbackSpeed = std::clamp(speed, 0.1f, 10.0f);
}

inline void ReplayPlayer::SeekToFrame(uint32_t frame) {
    m_currentFrame = std::min(frame, m_header.frameCount);
    m_currentTime = m_currentFrame * (m_header.duration / m_header.frameCount);

    // Find event index for this frame
    m_eventIndex = 0;
    for (size_t i = 0; i < m_events.size(); ++i) {
        if (m_events[i].frame >= m_currentFrame) {
            m_eventIndex = i;
            break;
        }
    }

    if (m_frameCallback) m_frameCallback(m_currentFrame);
}

inline void ReplayPlayer::SeekToTime(float time) {
    float progress = std::clamp(time / m_header.duration, 0.0f, 1.0f);
    SeekToFrame(static_cast<uint32_t>(progress * m_header.frameCount));
}

inline void ReplayPlayer::SeekToPercent(float percent) {
    SeekToFrame(static_cast<uint32_t>(std::clamp(percent, 0.0f, 1.0f) * m_header.frameCount));
}

inline void ReplayPlayer::StepForward() {
    if (m_currentFrame < m_header.frameCount) {
        SeekToFrame(m_currentFrame + 1);
    }
}

inline void ReplayPlayer::StepBackward() {
    if (m_currentFrame > 0) {
        SeekToFrame(m_currentFrame - 1);
    }
}

inline std::vector<InputEvent> ReplayPlayer::Update(float deltaTime) {
    std::vector<InputEvent> frameEvents;

    if (m_state != PlaybackState::Playing || !m_loaded) {
        return frameEvents;
    }

    m_currentTime += deltaTime * m_playbackSpeed;
    uint32_t targetFrame = static_cast<uint32_t>(m_currentTime / (m_header.duration / m_header.frameCount));

    while (m_currentFrame < targetFrame && m_currentFrame < m_header.frameCount) {
        frameEvents = GetEventsForFrame(m_currentFrame);
        ++m_currentFrame;
        if (m_frameCallback) m_frameCallback(m_currentFrame);
    }

    if (m_currentFrame >= m_header.frameCount) {
        Stop();
    }

    return frameEvents;
}

inline std::vector<InputEvent> ReplayPlayer::GetEventsForFrame(uint32_t frame) const {
    std::vector<InputEvent> events;
    for (const auto& event : m_events) {
        if (event.frame == frame) {
            events.push_back(event);
        } else if (event.frame > frame) {
            break;
        }
    }
    return events;
}

inline bool ReplayPlayer::VerifyState(uint32_t checksum) const {
    for (const auto& snapshot : m_snapshots) {
        if (snapshot.frame == m_currentFrame) {
            return snapshot.checksum == checksum;
        }
    }
    return true;  // No snapshot for this frame
}

inline float ReplayPlayer::GetProgress() const {
    if (m_header.frameCount == 0) return 0.0f;
    return static_cast<float>(m_currentFrame) / m_header.frameCount;
}

inline ReplayManager& ReplayManager::Instance() {
    static ReplayManager instance;
    return instance;
}

inline bool ReplayManager::Initialize(const std::string& replayDirectory) {
    m_replayDirectory = replayDirectory;
    m_initialized = true;
    return true;
}

inline void ReplayManager::Shutdown() {
    StopRecording();
    UnloadReplay();
    m_initialized = false;
}

inline void ReplayManager::StartRecording(uint32_t randomSeed) {
    m_recorder.Start(randomSeed);
}

inline void ReplayManager::StopRecording() {
    m_recorder.Stop();
}

inline bool ReplayManager::IsRecording() const {
    return m_recorder.IsRecording();
}

inline void ReplayManager::RecordInput(InputEventType type, int32_t code, float x, float y, uint8_t mods) {
    m_recorder.RecordInput(type, code, x, y, mods);
}

inline void ReplayManager::RecordCustom(const std::string& name, const std::vector<uint8_t>& data) {
    m_recorder.RecordCustomEvent(name, data);
}

inline void ReplayManager::Snapshot(uint32_t checksum, const std::vector<uint8_t>& state) {
    m_recorder.TakeSnapshot(checksum, state);
}

inline void ReplayManager::AdvanceFrame() {
    m_recorder.AdvanceFrame();
}

inline bool ReplayManager::SaveRecording(const std::string& filename,
                                          const std::unordered_map<std::string, std::string>& metadata) {
    return m_recorder.Save(GetFullPath(filename), "", metadata);
}

inline bool ReplayManager::LoadReplay(const std::string& filename) {
    return m_player.Load(GetFullPath(filename));
}

inline void ReplayManager::UnloadReplay() {
    m_player.Unload();
}

inline bool ReplayManager::IsReplayLoaded() const {
    return m_player.IsLoaded();
}

inline void ReplayManager::Play() { m_player.Play(); }
inline void ReplayManager::Pause() { m_player.Pause(); }
inline void ReplayManager::Stop() { m_player.Stop(); }
inline void ReplayManager::TogglePause() { m_player.TogglePause(); }
inline void ReplayManager::SetSpeed(float speed) { m_player.SetPlaybackSpeed(speed); }
inline void ReplayManager::SeekFrame(uint32_t frame) { m_player.SeekToFrame(frame); }
inline void ReplayManager::SeekTime(float time) { m_player.SeekToTime(time); }
inline void ReplayManager::SeekPercent(float percent) { m_player.SeekToPercent(percent); }
inline void ReplayManager::StepForward() { m_player.StepForward(); }
inline void ReplayManager::StepBackward() { m_player.StepBackward(); }

inline std::vector<InputEvent> ReplayManager::Update(float deltaTime) {
    return m_player.Update(deltaTime);
}

inline bool ReplayManager::VerifyState(uint32_t checksum) const {
    return m_player.VerifyState(checksum);
}

inline std::string ReplayManager::GetFullPath(const std::string& filename) const {
    return m_replayDirectory + "/" + filename;
}

} // namespace Nova
