#pragma once

#include "Cinematic.hpp"
#include <string>
#include <memory>
#include <queue>
#include <functional>
#include <vector>

// Forward declarations
namespace Nova {
    class AudioSource;
}

namespace Vehement {
namespace RTS {
namespace Campaign {

/**
 * @brief Player state for cinematic playback
 */
enum class CinematicPlayerState : uint8_t {
    Idle,
    Loading,
    Playing,
    Paused,
    Transitioning,
    Finished
};

/**
 * @brief Configuration for cinematic playback
 */
struct CinematicPlaybackConfig {
    bool enableLetterbox = true;
    float letterboxHeight = 0.15f;      ///< Height as percentage of screen
    bool enableSubtitles = true;
    float subtitleFontSize = 24.0f;
    std::string subtitleFont;
    bool enableSkipPrompt = true;
    float skipPromptDelay = 3.0f;
    bool pauseOnFocusLoss = true;
    bool muteGameAudio = true;
    float transitionDuration = 0.5f;
    std::string defaultTransition;
};

/**
 * @brief Interpolated camera state for smooth playback
 */
struct InterpolatedCamera {
    CinematicPosition position;
    CinematicPosition target;
    float fov = 60.0f;
    bool isValid = false;
};

/**
 * @brief Cinematic player for playing sequences
 */
class CinematicPlayer {
public:
    CinematicPlayer();
    ~CinematicPlayer();

    // Singleton access
    [[nodiscard]] static CinematicPlayer& Instance();

    // Delete copy/move
    CinematicPlayer(const CinematicPlayer&) = delete;
    CinematicPlayer& operator=(const CinematicPlayer&) = delete;
    CinematicPlayer(CinematicPlayer&&) = delete;
    CinematicPlayer& operator=(CinematicPlayer&&) = delete;

    // Initialization
    bool Initialize();
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // Configuration
    void SetConfig(const CinematicPlaybackConfig& config);
    [[nodiscard]] const CinematicPlaybackConfig& GetConfig() const { return m_config; }

    // Playback control
    void Play(Cinematic* cinematic);
    void Play(const std::string& cinematicId);
    void PlayFromFile(const std::string& jsonPath);
    void Queue(Cinematic* cinematic);
    void Queue(const std::string& cinematicId);
    void Pause();
    void Resume();
    void Skip();
    void Stop();

    // Update
    void Update(float deltaTime);
    void Render();

    // State queries
    [[nodiscard]] CinematicPlayerState GetState() const { return m_state; }
    [[nodiscard]] bool IsPlaying() const { return m_state == CinematicPlayerState::Playing; }
    [[nodiscard]] bool IsPaused() const { return m_state == CinematicPlayerState::Paused; }
    [[nodiscard]] bool IsFinished() const { return m_state == CinematicPlayerState::Finished; }
    [[nodiscard]] bool HasActivecinematic() const { return m_currentCinematic != nullptr; }

    // Current cinematic info
    [[nodiscard]] Cinematic* GetCurrentCinematic() { return m_currentCinematic; }
    [[nodiscard]] const Cinematic* GetCurrentCinematic() const { return m_currentCinematic; }
    [[nodiscard]] float GetProgress() const;
    [[nodiscard]] float GetCurrentTime() const;
    [[nodiscard]] float GetTotalDuration() const;
    [[nodiscard]] int32_t GetCurrentSceneIndex() const;
    [[nodiscard]] const CinematicScene* GetCurrentScene() const;

    // Camera
    [[nodiscard]] InterpolatedCamera GetInterpolatedCamera() const { return m_interpolatedCamera; }
    void SetCameraOverride(const CinematicPosition& position);
    void ClearCameraOverride();

    // Dialog
    [[nodiscard]] const CinematicDialog* GetCurrentDialog() const { return m_currentDialog; }
    [[nodiscard]] bool HasActiveDialog() const { return m_currentDialog != nullptr; }
    void AdvanceDialog();

    // Subtitle display
    [[nodiscard]] std::string GetCurrentSubtitle() const;
    [[nodiscard]] float GetSubtitleProgress() const;

    // Events
    void SetOnStart(std::function<void()> callback) { m_onStart = callback; }
    void SetOnEnd(std::function<void()> callback) { m_onEnd = callback; }
    void SetOnSkip(std::function<void()> callback) { m_onSkip = callback; }
    void SetOnSceneChange(std::function<void(int)> callback) { m_onSceneChange = callback; }
    void SetOnDialogStart(std::function<void(const CinematicDialog&)> callback) { m_onDialogStart = callback; }
    void SetOnDialogEnd(std::function<void()> callback) { m_onDialogEnd = callback; }

    // Audio control
    void SetMasterVolume(float volume);
    void SetVoiceVolume(float volume);
    void SetMusicVolume(float volume);
    void SetSFXVolume(float volume);

    // Letterbox
    [[nodiscard]] float GetLetterboxAmount() const { return m_letterboxAmount; }
    [[nodiscard]] bool IsLetterboxActive() const { return m_letterboxAmount > 0.0f; }

    // Skip UI
    [[nodiscard]] bool IsSkipPromptVisible() const { return m_showSkipPrompt; }
    [[nodiscard]] float GetSkipHoldProgress() const { return m_skipHoldProgress; }
    void BeginSkipHold();
    void EndSkipHold();

private:
    bool m_initialized = false;
    CinematicPlaybackConfig m_config;
    CinematicPlayerState m_state = CinematicPlayerState::Idle;

    // Current playback
    Cinematic* m_currentCinematic = nullptr;
    std::unique_ptr<Cinematic> m_loadedCinematic;  // Owned cinematic from file load
    std::queue<Cinematic*> m_queue;
    float m_playbackTime = 0.0f;

    // Camera
    InterpolatedCamera m_interpolatedCamera;
    bool m_hasCameraOverride = false;
    CinematicPosition m_cameraOverride;

    // Dialog
    const CinematicDialog* m_currentDialog = nullptr;
    float m_dialogTime = 0.0f;
    size_t m_dialogCharIndex = 0;

    // Letterbox
    float m_letterboxAmount = 0.0f;
    float m_targetLetterboxAmount = 0.0f;

    // Skip
    bool m_showSkipPrompt = false;
    bool m_isSkipHeld = false;
    float m_skipHoldProgress = 0.0f;
    float m_skipHoldDuration = 1.0f;

    // Audio
    float m_masterVolume = 1.0f;
    float m_voiceVolume = 1.0f;
    float m_musicVolume = 0.8f;
    float m_sfxVolume = 1.0f;
    bool m_wasGameAudioMuted = false;
    std::shared_ptr<Nova::AudioSource> m_currentVoiceover;
    std::vector<std::shared_ptr<Nova::AudioSource>> m_activeSfx;

    // Callbacks
    std::function<void()> m_onStart;
    std::function<void()> m_onEnd;
    std::function<void()> m_onSkip;
    std::function<void(int)> m_onSceneChange;
    std::function<void(const CinematicDialog&)> m_onDialogStart;
    std::function<void()> m_onDialogEnd;

    // Internal methods
    void UpdatePlayback(float deltaTime);
    void UpdateCamera(float deltaTime);
    void UpdateDialog(float deltaTime);
    void UpdateLetterbox(float deltaTime);
    void UpdateSkip(float deltaTime);
    void UpdateAudio(float deltaTime);

    void StartCinematic();
    void EndCinematic();
    void PlayNextInQueue();

    void ProcessCurrentScene();
    void InterpolateCamera(const CameraMovement& movement, float sceneTime);

    void RenderLetterbox();
    void RenderSubtitles();
    void RenderSkipPrompt();

    void StartDialog(const CinematicDialog& dialog);
    void EndDialog();

    void PlayAudioCue(const AudioCue& cue);
    void StopAllAudio();
};

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
