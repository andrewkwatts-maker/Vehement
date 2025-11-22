#include "CinematicPlayer.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace RTS {
namespace Campaign {

CinematicPlayer::CinematicPlayer() = default;
CinematicPlayer::~CinematicPlayer() = default;

CinematicPlayer& CinematicPlayer::Instance() {
    static CinematicPlayer instance;
    return instance;
}

bool CinematicPlayer::Initialize() {
    if (m_initialized) return true;

    m_config = CinematicPlaybackConfig();
    m_state = CinematicPlayerState::Idle;
    m_initialized = true;
    return true;
}

void CinematicPlayer::Shutdown() {
    Stop();
    while (!m_queue.empty()) {
        m_queue.pop();
    }
    m_initialized = false;
}

void CinematicPlayer::SetConfig(const CinematicPlaybackConfig& config) {
    m_config = config;
}

void CinematicPlayer::Play(Cinematic* cinematic) {
    if (!cinematic) return;

    Stop();
    m_currentCinematic = cinematic;
    StartCinematic();
}

void CinematicPlayer::Play(const std::string& /*cinematicId*/) {
    // TODO: Look up cinematic by ID from campaign manager
}

void CinematicPlayer::PlayFromFile(const std::string& /*jsonPath*/) {
    // TODO: Load cinematic from file and play
}

void CinematicPlayer::Queue(Cinematic* cinematic) {
    if (!cinematic) return;
    m_queue.push(cinematic);

    if (m_state == CinematicPlayerState::Idle) {
        PlayNextInQueue();
    }
}

void CinematicPlayer::Queue(const std::string& /*cinematicId*/) {
    // TODO: Look up cinematic by ID and queue
}

void CinematicPlayer::Pause() {
    if (m_state == CinematicPlayerState::Playing) {
        m_state = CinematicPlayerState::Paused;
        if (m_currentCinematic) {
            m_currentCinematic->Pause();
        }
    }
}

void CinematicPlayer::Resume() {
    if (m_state == CinematicPlayerState::Paused) {
        m_state = CinematicPlayerState::Playing;
        if (m_currentCinematic) {
            m_currentCinematic->Resume();
        }
    }
}

void CinematicPlayer::Skip() {
    if (!m_currentCinematic || !m_currentCinematic->canSkip) return;

    if (m_onSkip) {
        m_onSkip();
    }

    m_currentCinematic->Skip();
    EndCinematic();
}

void CinematicPlayer::Stop() {
    if (m_currentCinematic) {
        m_currentCinematic->Stop();
        m_currentCinematic = nullptr;
    }
    m_state = CinematicPlayerState::Idle;
    m_playbackTime = 0.0f;
    m_currentDialog = nullptr;
    m_letterboxAmount = 0.0f;
    StopAllAudio();
}

void CinematicPlayer::Update(float deltaTime) {
    if (!m_initialized) return;

    switch (m_state) {
        case CinematicPlayerState::Playing:
            UpdatePlayback(deltaTime);
            break;
        case CinematicPlayerState::Paused:
            // Only update skip UI when paused
            UpdateSkip(deltaTime);
            break;
        case CinematicPlayerState::Transitioning:
            UpdateLetterbox(deltaTime);
            break;
        default:
            break;
    }

    UpdateLetterbox(deltaTime);
}

void CinematicPlayer::Render() {
    if (!m_initialized) return;

    if (m_letterboxAmount > 0.0f) {
        RenderLetterbox();
    }

    if (m_currentDialog && m_config.enableSubtitles) {
        RenderSubtitles();
    }

    if (m_showSkipPrompt && m_config.enableSkipPrompt) {
        RenderSkipPrompt();
    }
}

float CinematicPlayer::GetProgress() const {
    if (!m_currentCinematic) return 0.0f;
    return m_currentCinematic->GetProgress();
}

float CinematicPlayer::GetCurrentTime() const {
    return m_playbackTime;
}

float CinematicPlayer::GetTotalDuration() const {
    if (!m_currentCinematic) return 0.0f;
    return m_currentCinematic->totalDuration;
}

int32_t CinematicPlayer::GetCurrentSceneIndex() const {
    if (!m_currentCinematic) return -1;
    return m_currentCinematic->currentSceneIndex;
}

const CinematicScene* CinematicPlayer::GetCurrentScene() const {
    if (!m_currentCinematic) return nullptr;
    return m_currentCinematic->GetCurrentScene();
}

void CinematicPlayer::SetCameraOverride(const CinematicPosition& position) {
    m_hasCameraOverride = true;
    m_cameraOverride = position;
}

void CinematicPlayer::ClearCameraOverride() {
    m_hasCameraOverride = false;
}

std::string CinematicPlayer::GetCurrentSubtitle() const {
    if (!m_currentDialog) return "";

    // Return text up to current character for typewriter effect
    if (m_dialogCharIndex < m_currentDialog->text.length()) {
        return m_currentDialog->text.substr(0, m_dialogCharIndex);
    }
    return m_currentDialog->text;
}

float CinematicPlayer::GetSubtitleProgress() const {
    if (!m_currentDialog || m_currentDialog->text.empty()) return 1.0f;
    return static_cast<float>(m_dialogCharIndex) / static_cast<float>(m_currentDialog->text.length());
}

void CinematicPlayer::AdvanceDialog() {
    if (!m_currentDialog) return;

    // If typewriter not complete, complete it
    if (m_dialogCharIndex < m_currentDialog->text.length()) {
        m_dialogCharIndex = m_currentDialog->text.length();
    } else {
        // Otherwise end current dialog
        EndDialog();
    }
}

void CinematicPlayer::SetMasterVolume(float volume) {
    m_masterVolume = std::clamp(volume, 0.0f, 1.0f);
}

void CinematicPlayer::SetVoiceVolume(float volume) {
    m_voiceVolume = std::clamp(volume, 0.0f, 1.0f);
}

void CinematicPlayer::SetMusicVolume(float volume) {
    m_musicVolume = std::clamp(volume, 0.0f, 1.0f);
}

void CinematicPlayer::SetSFXVolume(float volume) {
    m_sfxVolume = std::clamp(volume, 0.0f, 1.0f);
}

void CinematicPlayer::BeginSkipHold() {
    m_isSkipHeld = true;
}

void CinematicPlayer::EndSkipHold() {
    m_isSkipHeld = false;
    m_skipHoldProgress = 0.0f;
}

void CinematicPlayer::UpdatePlayback(float deltaTime) {
    if (!m_currentCinematic) return;

    m_playbackTime += deltaTime;
    m_currentCinematic->Update(deltaTime);

    UpdateCamera(deltaTime);
    UpdateDialog(deltaTime);
    UpdateAudio(deltaTime);
    UpdateSkip(deltaTime);

    ProcessCurrentScene();

    if (m_currentCinematic->IsComplete()) {
        EndCinematic();
    }
}

void CinematicPlayer::UpdateCamera(float /*deltaTime*/) {
    if (m_hasCameraOverride) {
        m_interpolatedCamera.position = m_cameraOverride;
        m_interpolatedCamera.isValid = true;
        return;
    }

    const auto* scene = GetCurrentScene();
    if (!scene) return;

    float sceneTime = m_playbackTime - scene->startTime;
    InterpolateCamera(scene->camera, sceneTime);
}

void CinematicPlayer::UpdateDialog(float deltaTime) {
    if (!m_currentDialog) return;

    m_dialogTime += deltaTime;

    // Typewriter effect - advance characters
    float charsPerSecond = 30.0f;
    size_t targetChars = static_cast<size_t>(m_dialogTime * charsPerSecond);
    m_dialogCharIndex = std::min(targetChars, m_currentDialog->text.length());

    // Check if dialog duration exceeded
    if (m_dialogTime >= m_currentDialog->duration) {
        EndDialog();
    }
}

void CinematicPlayer::UpdateLetterbox(float deltaTime) {
    if (!m_config.enableLetterbox) {
        m_letterboxAmount = 0.0f;
        return;
    }

    // Animate letterbox
    float targetAmount = (m_state == CinematicPlayerState::Playing ||
                          m_state == CinematicPlayerState::Paused)
                         ? m_config.letterboxHeight : 0.0f;

    float speed = 2.0f;
    if (m_letterboxAmount < targetAmount) {
        m_letterboxAmount = std::min(m_letterboxAmount + speed * deltaTime, targetAmount);
    } else if (m_letterboxAmount > targetAmount) {
        m_letterboxAmount = std::max(m_letterboxAmount - speed * deltaTime, targetAmount);
    }
}

void CinematicPlayer::UpdateSkip(float deltaTime) {
    // Show skip prompt after delay
    m_showSkipPrompt = m_currentCinematic &&
                       m_currentCinematic->canSkip &&
                       m_playbackTime >= m_config.skipPromptDelay;

    // Handle skip hold
    if (m_isSkipHeld && m_showSkipPrompt) {
        m_skipHoldProgress += deltaTime / m_skipHoldDuration;
        if (m_skipHoldProgress >= 1.0f) {
            Skip();
            m_skipHoldProgress = 0.0f;
        }
    }
}

void CinematicPlayer::UpdateAudio(float /*deltaTime*/) {
    // TODO: Update audio playback, handle fades
}

void CinematicPlayer::StartCinematic() {
    if (!m_currentCinematic) return;

    m_state = CinematicPlayerState::Playing;
    m_playbackTime = 0.0f;
    m_targetLetterboxAmount = m_config.letterboxHeight;

    m_currentCinematic->Start();

    if (m_config.muteGameAudio) {
        // TODO: Mute game audio
    }

    if (m_onStart) {
        m_onStart();
    }
}

void CinematicPlayer::EndCinematic() {
    m_state = CinematicPlayerState::Finished;

    if (m_onEnd) {
        m_onEnd();
    }

    m_currentCinematic = nullptr;
    m_targetLetterboxAmount = 0.0f;

    PlayNextInQueue();
}

void CinematicPlayer::PlayNextInQueue() {
    if (m_queue.empty()) {
        m_state = CinematicPlayerState::Idle;
        return;
    }

    m_currentCinematic = m_queue.front();
    m_queue.pop();
    StartCinematic();
}

void CinematicPlayer::ProcessCurrentScene() {
    const auto* scene = GetCurrentScene();
    if (!scene) return;

    float sceneTime = m_playbackTime - scene->startTime;

    // Check for dialog triggers
    for (const auto& dialog : scene->dialogs) {
        if (sceneTime >= dialog.startTime &&
            sceneTime < dialog.startTime + 0.1f && // Small window to avoid re-triggering
            &dialog != m_currentDialog) {
            StartDialog(dialog);
        }
    }

    // Check for audio cue triggers
    for (const auto& cue : scene->audioCues) {
        if (sceneTime >= cue.startTime &&
            sceneTime < cue.startTime + 0.1f) {
            PlayAudioCue(cue);
        }
    }
}

void CinematicPlayer::InterpolateCamera(const CameraMovement& movement, float sceneTime) {
    if (movement.keyframes.empty()) return;

    // Find surrounding keyframes
    const CameraKeyframe* prevFrame = &movement.keyframes[0];
    const CameraKeyframe* nextFrame = &movement.keyframes[0];

    for (size_t i = 0; i < movement.keyframes.size() - 1; ++i) {
        if (movement.keyframes[i].time <= sceneTime &&
            movement.keyframes[i + 1].time > sceneTime) {
            prevFrame = &movement.keyframes[i];
            nextFrame = &movement.keyframes[i + 1];
            break;
        }
    }

    // Interpolation factor
    float t = 0.0f;
    if (nextFrame->time > prevFrame->time) {
        t = (sceneTime - prevFrame->time) / (nextFrame->time - prevFrame->time);
    }

    // Apply easing
    // TODO: Implement different easing types
    t = t * t * (3.0f - 2.0f * t); // Smoothstep

    // Interpolate position
    m_interpolatedCamera.position.x = prevFrame->position.x + t * (nextFrame->position.x - prevFrame->position.x);
    m_interpolatedCamera.position.y = prevFrame->position.y + t * (nextFrame->position.y - prevFrame->position.y);
    m_interpolatedCamera.position.z = prevFrame->position.z + t * (nextFrame->position.z - prevFrame->position.z);
    m_interpolatedCamera.position.pitch = prevFrame->position.pitch + t * (nextFrame->position.pitch - prevFrame->position.pitch);
    m_interpolatedCamera.position.yaw = prevFrame->position.yaw + t * (nextFrame->position.yaw - prevFrame->position.yaw);
    m_interpolatedCamera.position.fov = prevFrame->position.fov + t * (nextFrame->position.fov - prevFrame->position.fov);
    m_interpolatedCamera.isValid = true;
}

void CinematicPlayer::RenderLetterbox() {
    // TODO: Render top and bottom letterbox bars
}

void CinematicPlayer::RenderSubtitles() {
    // TODO: Render subtitle text
}

void CinematicPlayer::RenderSkipPrompt() {
    // TODO: Render skip prompt UI
}

void CinematicPlayer::StartDialog(const CinematicDialog& dialog) {
    m_currentDialog = &dialog;
    m_dialogTime = 0.0f;
    m_dialogCharIndex = 0;

    if (m_onDialogStart) {
        m_onDialogStart(dialog);
    }

    // TODO: Play voiceover if available
}

void CinematicPlayer::EndDialog() {
    if (m_onDialogEnd) {
        m_onDialogEnd();
    }
    m_currentDialog = nullptr;
}

void CinematicPlayer::PlayAudioCue(const AudioCue& /*cue*/) {
    // TODO: Play audio through audio system
}

void CinematicPlayer::StopAllAudio() {
    // TODO: Stop all cinematic audio
}

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
