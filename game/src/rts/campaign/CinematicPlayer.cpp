#include "CinematicPlayer.hpp"
#include "CampaignManager.hpp"
#include "engine/audio/AudioEngine.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <memory>

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

void CinematicPlayer::Play(const std::string& cinematicId) {
    // Look up cinematic by ID from campaign manager's current campaign
    auto& campaignMgr = CampaignManager::Instance();
    if (auto* campaign = campaignMgr.GetCurrentCampaign()) {
        if (auto* cinematic = campaign->GetCinematic(cinematicId)) {
            Play(cinematic);
        }
    }
}

void CinematicPlayer::PlayFromFile(const std::string& jsonPath) {
    // Load cinematic from file and play
    auto cinematic = CinematicFactory::CreateFromJson(jsonPath);
    if (cinematic) {
        // Store the unique_ptr and play the raw pointer
        m_loadedCinematic = std::move(cinematic);
        Play(m_loadedCinematic.get());
    }
}

void CinematicPlayer::Queue(Cinematic* cinematic) {
    if (!cinematic) return;
    m_queue.push(cinematic);

    if (m_state == CinematicPlayerState::Idle) {
        PlayNextInQueue();
    }
}

void CinematicPlayer::Queue(const std::string& cinematicId) {
    // Look up cinematic by ID from campaign manager and queue
    auto& campaignMgr = CampaignManager::Instance();
    if (auto* campaign = campaignMgr.GetCurrentCampaign()) {
        if (auto* cinematic = campaign->GetCinematic(cinematicId)) {
            Queue(cinematic);
        }
    }
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

void CinematicPlayer::UpdateAudio(float deltaTime) {
    auto& audio = Nova::AudioEngine::Instance();

    // Update music volume based on current cinematic settings
    if (m_currentCinematic && !m_currentCinematic->backgroundMusic.empty()) {
        float targetVolume = m_currentCinematic->musicVolume * m_musicVolume * m_masterVolume;

        // Handle fade in
        if (m_currentCinematic->fadeInMusic && m_playbackTime < 2.0f) {
            float fadeProgress = m_playbackTime / 2.0f;
            audio.SetMusicVolume(targetVolume * fadeProgress);
        }
        // Handle fade out near end
        else if (m_currentCinematic->fadeOutMusic) {
            float timeRemaining = m_currentCinematic->totalDuration - m_playbackTime;
            if (timeRemaining < 2.0f && timeRemaining > 0.0f) {
                float fadeProgress = timeRemaining / 2.0f;
                audio.SetMusicVolume(targetVolume * fadeProgress);
            } else {
                audio.SetMusicVolume(targetVolume);
            }
        } else {
            audio.SetMusicVolume(targetVolume);
        }
    }

    // Update voice and SFX volumes
    // These are managed through the audio bus system
    if (auto* voiceBus = audio.GetBus("voice")) {
        voiceBus->SetVolume(m_voiceVolume * m_masterVolume);
    }
    if (auto* sfxBus = audio.GetBus("sfx")) {
        sfxBus->SetVolume(m_sfxVolume * m_masterVolume);
    }

    (void)deltaTime;  // Mark as intentionally unused for now
}

void CinematicPlayer::StartCinematic() {
    if (!m_currentCinematic) return;

    m_state = CinematicPlayerState::Playing;
    m_playbackTime = 0.0f;
    m_targetLetterboxAmount = m_config.letterboxHeight;

    m_currentCinematic->Start();

    if (m_config.muteGameAudio) {
        // Mute game audio buses (ambient, game SFX) but keep cinematic audio
        auto& audio = Nova::AudioEngine::Instance();
        if (auto* ambientBus = audio.GetBus("ambient")) {
            ambientBus->SetMuted(true);
        }
        if (auto* gameSfxBus = audio.GetBus("game_sfx")) {
            gameSfxBus->SetMuted(true);
        }
        m_wasGameAudioMuted = true;
    }

    if (m_onStart) {
        m_onStart();
    }
}

void CinematicPlayer::EndCinematic() {
    m_state = CinematicPlayerState::Finished;

    // Restore game audio if we muted it
    if (m_wasGameAudioMuted) {
        auto& audio = Nova::AudioEngine::Instance();
        if (auto* ambientBus = audio.GetBus("ambient")) {
            ambientBus->SetMuted(false);
        }
        if (auto* gameSfxBus = audio.GetBus("game_sfx")) {
            gameSfxBus->SetMuted(false);
        }
        m_wasGameAudioMuted = false;
    }

    // Stop cinematic audio
    StopAllAudio();

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

    // Apply easing based on keyframe easing type
    const std::string& easingType = prevFrame->easingType;
    if (easingType == "linear") {
        // Linear - no change to t
    } else if (easingType == "ease-in" || easingType == "ease_in") {
        // Ease in - quadratic
        t = t * t;
    } else if (easingType == "ease-out" || easingType == "ease_out") {
        // Ease out - inverse quadratic
        t = t * (2.0f - t);
    } else if (easingType == "ease-in-out" || easingType == "ease_in_out") {
        // Ease in-out - smoothstep
        t = t * t * (3.0f - 2.0f * t);
    } else if (easingType == "ease-in-cubic" || easingType == "ease_in_cubic") {
        // Cubic ease in
        t = t * t * t;
    } else if (easingType == "ease-out-cubic" || easingType == "ease_out_cubic") {
        // Cubic ease out
        float f = t - 1.0f;
        t = f * f * f + 1.0f;
    } else if (easingType == "ease-in-out-cubic" || easingType == "ease_in_out_cubic") {
        // Cubic ease in-out
        if (t < 0.5f) {
            t = 4.0f * t * t * t;
        } else {
            float f = 2.0f * t - 2.0f;
            t = 0.5f * f * f * f + 1.0f;
        }
    } else if (easingType == "bounce") {
        // Bounce effect at end
        if (t < (1.0f / 2.75f)) {
            t = 7.5625f * t * t;
        } else if (t < (2.0f / 2.75f)) {
            t -= (1.5f / 2.75f);
            t = 7.5625f * t * t + 0.75f;
        } else if (t < (2.5f / 2.75f)) {
            t -= (2.25f / 2.75f);
            t = 7.5625f * t * t + 0.9375f;
        } else {
            t -= (2.625f / 2.75f);
            t = 7.5625f * t * t + 0.984375f;
        }
    } else {
        // Default to smoothstep
        t = t * t * (3.0f - 2.0f * t);
    }

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
    // Render top and bottom letterbox bars using ImGui
    ImGuiIO& io = ImGui::GetIO();
    float screenWidth = io.DisplaySize.x;
    float screenHeight = io.DisplaySize.y;
    float barHeight = screenHeight * m_letterboxAmount;

    ImU32 barColor = IM_COL32(0, 0, 0, 255);

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    // Top bar
    drawList->AddRectFilled(
        ImVec2(0.0f, 0.0f),
        ImVec2(screenWidth, barHeight),
        barColor
    );

    // Bottom bar
    drawList->AddRectFilled(
        ImVec2(0.0f, screenHeight - barHeight),
        ImVec2(screenWidth, screenHeight),
        barColor
    );
}

void CinematicPlayer::RenderSubtitles() {
    if (!m_currentDialog || !m_currentDialog->showSubtitle) return;

    ImGuiIO& io = ImGui::GetIO();
    float screenWidth = io.DisplaySize.x;
    float screenHeight = io.DisplaySize.y;

    std::string subtitle = GetCurrentSubtitle();
    if (subtitle.empty()) return;

    // Calculate subtitle position (bottom center, above letterbox)
    float subtitleY = screenHeight - (screenHeight * m_letterboxAmount) - 60.0f;
    float padding = 20.0f;
    float maxWidth = screenWidth * 0.8f;

    // Style for subtitle background
    ImU32 bgColor = IM_COL32(0, 0, 0, 180);
    ImU32 textColor = IM_COL32(255, 255, 255, 255);

    // Get text size
    ImVec2 textSize = ImGui::CalcTextSize(subtitle.c_str(), nullptr, false, maxWidth);

    // Calculate box position (centered)
    float boxX = (screenWidth - textSize.x - padding * 2) * 0.5f;
    float boxY = subtitleY - textSize.y - padding;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Draw background
    drawList->AddRectFilled(
        ImVec2(boxX, boxY),
        ImVec2(boxX + textSize.x + padding * 2, boxY + textSize.y + padding * 2),
        bgColor,
        5.0f  // Rounded corners
    );

    // Draw character name if available
    if (!m_currentDialog->characterName.empty()) {
        ImU32 nameColor = IM_COL32(255, 220, 100, 255);
        std::string nameText = m_currentDialog->characterName + ":";
        ImVec2 nameSize = ImGui::CalcTextSize(nameText.c_str());
        drawList->AddText(
            ImVec2(boxX + padding, boxY - nameSize.y - 5.0f),
            nameColor,
            nameText.c_str()
        );
    }

    // Draw subtitle text
    drawList->AddText(
        nullptr,
        0.0f,
        ImVec2(boxX + padding, boxY + padding),
        textColor,
        subtitle.c_str(),
        nullptr,
        maxWidth
    );
}

void CinematicPlayer::RenderSkipPrompt() {
    ImGuiIO& io = ImGui::GetIO();
    float screenWidth = io.DisplaySize.x;

    // Position in bottom-right corner
    float promptX = screenWidth - 200.0f;
    float promptY = 50.0f;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Draw skip prompt text
    const char* skipText = "Hold [SPACE] to Skip";
    ImU32 textColor = IM_COL32(255, 255, 255, 200);
    drawList->AddText(ImVec2(promptX, promptY), textColor, skipText);

    // Draw progress bar if holding
    if (m_isSkipHeld && m_skipHoldProgress > 0.0f) {
        float barWidth = 150.0f;
        float barHeight = 4.0f;
        float barY = promptY + 20.0f;

        // Background
        drawList->AddRectFilled(
            ImVec2(promptX, barY),
            ImVec2(promptX + barWidth, barY + barHeight),
            IM_COL32(50, 50, 50, 200),
            2.0f
        );

        // Progress fill
        float fillWidth = barWidth * m_skipHoldProgress;
        drawList->AddRectFilled(
            ImVec2(promptX, barY),
            ImVec2(promptX + fillWidth, barY + barHeight),
            IM_COL32(255, 255, 255, 255),
            2.0f
        );
    }
}

void CinematicPlayer::StartDialog(const CinematicDialog& dialog) {
    m_currentDialog = &dialog;
    m_dialogTime = 0.0f;
    m_dialogCharIndex = 0;

    if (m_onDialogStart) {
        m_onDialogStart(dialog);
    }

    // Play voiceover if available
    if (!dialog.voiceoverFile.empty()) {
        auto& audio = Nova::AudioEngine::Instance();
        auto voiceBuffer = audio.LoadSound(dialog.voiceoverFile);
        if (voiceBuffer) {
            m_currentVoiceover = audio.Play2D(voiceBuffer, m_voiceVolume * m_masterVolume);
            if (m_currentVoiceover) {
                m_currentVoiceover->SetOutputBus("voice");
            }
        }
    }
}

void CinematicPlayer::EndDialog() {
    if (m_onDialogEnd) {
        m_onDialogEnd();
    }
    m_currentDialog = nullptr;
}

void CinematicPlayer::PlayAudioCue(const AudioCue& cue) {
    if (cue.audioFile.empty()) return;

    auto& audio = Nova::AudioEngine::Instance();

    if (cue.isMusic) {
        // Play as music (streaming)
        float volume = cue.volume * m_musicVolume * m_masterVolume;
        audio.PlayMusic(cue.audioFile, volume, cue.loop);
    } else {
        // Play as sound effect
        auto buffer = audio.LoadSound(cue.audioFile);
        if (buffer) {
            float volume = cue.volume * m_sfxVolume * m_masterVolume;
            auto source = audio.Play2D(buffer, volume);
            if (source) {
                source->SetLooping(cue.loop);
                if (!cue.channel.empty()) {
                    source->SetOutputBus(cue.channel);
                } else {
                    source->SetOutputBus("sfx");
                }
                // Track active SFX for cleanup
                m_activeSfx.push_back(source);
            }
        }
    }
}

void CinematicPlayer::StopAllAudio() {
    auto& audio = Nova::AudioEngine::Instance();

    // Stop background music
    audio.StopMusic();

    // Stop current voiceover
    if (m_currentVoiceover) {
        m_currentVoiceover->Stop();
        m_currentVoiceover.reset();
    }

    // Stop all active SFX
    for (auto& sfx : m_activeSfx) {
        if (sfx) {
            sfx->Stop();
        }
    }
    m_activeSfx.clear();
}

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
