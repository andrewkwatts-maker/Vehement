#include "AudioPreview.hpp"
#include "ModernUI.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <cmath>
#include <algorithm>

AudioPreview::AudioPreview() = default;
AudioPreview::~AudioPreview() {
    Stop();
}

void AudioPreview::Open(const std::string& assetPath) {
    m_assetPath = assetPath;

    // Extract filename
    std::filesystem::path path(assetPath);
    m_audioName = path.filename().string();

    LoadAudio();
}

void AudioPreview::Render(bool* isOpen) {
    if (!isOpen || !*isOpen) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);

    std::string windowTitle = "Audio Preview - " + m_audioName;

    if (ImGui::Begin(windowTitle.c_str(), isOpen, ImGuiWindowFlags_MenuBar)) {
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Export As...")) {
                    // TODO: Export dialog
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Close")) {
                    *isOpen = false;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        if (!m_isLoaded) {
            ImVec2 windowSize = ImGui::GetContentRegionAvail();
            ImGui::SetCursorPos(ImVec2(
                windowSize.x * 0.5f - 50,
                windowSize.y * 0.5f - 10
            ));
            ImGui::TextDisabled("No audio loaded");
        } else {
            // Waveform visualization
            if (ImGui::BeginChild("Waveform", ImVec2(0, 200), true)) {
                RenderWaveform();
            }
            ImGui::EndChild();

            ImGui::Spacing();

            // Playback controls
            RenderControls();

            ImGui::Spacing();
            ModernUI::GradientSeparator();
            ImGui::Spacing();

            // Properties
            RenderProperties();
        }
    }
    ImGui::End();
}

void AudioPreview::Update(float deltaTime) {
    if (m_playbackState == PlaybackState::Playing && m_isLoaded) {
        m_currentTime += deltaTime;

        if (m_currentTime >= m_duration) {
            if (m_loop) {
                m_currentTime = 0.0f;
            } else {
                Stop();
            }
        }
    }
}

void AudioPreview::RenderWaveform() {
    if (m_waveformData.empty()) {
        ImGui::TextDisabled("No waveform data");
        return;
    }

    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Background
    drawList->AddRectFilled(
        canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(20, 20, 30, 255)
    );

    // Center line
    float centerY = canvasPos.y + canvasSize.y * 0.5f;
    drawList->AddLine(
        ImVec2(canvasPos.x, centerY),
        ImVec2(canvasPos.x + canvasSize.x, centerY),
        IM_COL32(80, 80, 100, 100),
        1.0f
    );

    // Waveform
    const ImU32 waveColor = IM_COL32(100, 200, 255, 255);
    const float amplitude = canvasSize.y * 0.4f;

    float xStep = canvasSize.x / static_cast<float>(m_waveformData.size() - 1);

    for (size_t i = 0; i < m_waveformData.size() - 1; ++i) {
        float x1 = canvasPos.x + i * xStep;
        float x2 = canvasPos.x + (i + 1) * xStep;
        float y1 = centerY - m_waveformData[i] * amplitude;
        float y2 = centerY - m_waveformData[i + 1] * amplitude;

        drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), waveColor, 1.5f);
    }

    // Playback position indicator
    if (m_duration > 0.0f) {
        float playbackX = canvasPos.x + (m_currentTime / m_duration) * canvasSize.x;
        drawList->AddLine(
            ImVec2(playbackX, canvasPos.y),
            ImVec2(playbackX, canvasPos.y + canvasSize.y),
            IM_COL32(255, 200, 0, 200),
            2.0f
        );
    }

    // Handle clicking to seek
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("WaveformButton", canvasSize);

    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 mousePos = ImGui::GetMousePos();
        float clickX = mousePos.x - canvasPos.x;
        float position = std::clamp(clickX / canvasSize.x, 0.0f, 1.0f);
        Seek(position * m_duration);
    }
}

void AudioPreview::RenderControls() {
    ImGui::BeginGroup();

    // Play/Pause/Stop buttons
    float buttonSize = 60.0f;
    ImVec2 btnSize(buttonSize, buttonSize);

    ImGui::BeginGroup();

    if (m_playbackState == PlaybackState::Playing) {
        if (ModernUI::GlowButton("||", btnSize)) {
            Pause();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Pause");
        }
    } else {
        if (ModernUI::GlowButton(">", btnSize)) {
            Play();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Play");
        }
    }

    ImGui::SameLine();

    if (ModernUI::GlowButton("[]", btnSize)) {
        Stop();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Stop");
    }

    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 20.0f);

    // Time display and seek bar
    ImGui::BeginGroup();

    // Time display
    std::string timeStr = FormatTime(m_currentTime) + " / " + FormatTime(m_duration);
    ImGui::Text("%s", timeStr.c_str());

    // Seek bar
    float seekPosition = m_duration > 0.0f ? m_currentTime / m_duration : 0.0f;
    float seekValue = seekPosition;

    ImGui::SetNextItemWidth(300);
    if (ImGui::SliderFloat("##Seek", &seekValue, 0.0f, 1.0f, "")) {
        Seek(seekValue * m_duration);
    }

    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 20.0f);

    // Volume and loop controls
    ImGui::BeginGroup();

    ImGui::Text("Volume");
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("##Volume", &m_volume, 0.0f, 1.0f, "%.2f");

    ImGui::Checkbox("Loop", &m_loop);

    ImGui::EndGroup();

    ImGui::EndGroup();
}

void AudioPreview::RenderProperties() {
    ModernUI::GradientHeader("Audio Information", ImGuiTreeNodeFlags_DefaultOpen);

    ImGui::Indent();

    ModernUI::CompactStat("File", m_audioName.c_str());

    ModernUI::CompactStat("Format", m_format.c_str());

    char sampleRateStr[32];
    snprintf(sampleRateStr, sizeof(sampleRateStr), "%d Hz", m_sampleRate);
    ModernUI::CompactStat("Sample Rate", sampleRateStr);

    char channelStr[32];
    if (m_channels == 1) {
        snprintf(channelStr, sizeof(channelStr), "Mono");
    } else if (m_channels == 2) {
        snprintf(channelStr, sizeof(channelStr), "Stereo");
    } else {
        snprintf(channelStr, sizeof(channelStr), "%d Channels", m_channels);
    }
    ModernUI::CompactStat("Channels", channelStr);

    char bitrateStr[32];
    snprintf(bitrateStr, sizeof(bitrateStr), "%d kbps", m_bitrate);
    ModernUI::CompactStat("Bitrate", bitrateStr);

    ModernUI::CompactStat("Duration", FormatTime(m_duration).c_str());

    char sizeStr[32];
    if (m_fileSize < 1024) {
        snprintf(sizeStr, sizeof(sizeStr), "%zu B", m_fileSize);
    } else if (m_fileSize < 1024 * 1024) {
        snprintf(sizeStr, sizeof(sizeStr), "%.2f KB", m_fileSize / 1024.0);
    } else {
        snprintf(sizeStr, sizeof(sizeStr), "%.2f MB", m_fileSize / (1024.0 * 1024.0));
    }
    ModernUI::CompactStat("File Size", sizeStr);

    ImGui::Unindent();
}

void AudioPreview::LoadAudio() {
    spdlog::info("AudioPreview: Loading audio '{}'", m_assetPath);

    // TODO: Actual audio loading using engine's audio system
    // For now, simulate loading

    try {
        std::filesystem::path path(m_assetPath);

        if (!std::filesystem::exists(path)) {
            spdlog::error("AudioPreview: File does not exist: '{}'", m_assetPath);
            return;
        }

        m_fileSize = std::filesystem::file_size(path);

        // Simulate audio properties
        m_sampleRate = 44100;
        m_channels = 2;
        m_bitrate = 320;
        m_duration = 125.5f;
        m_format = "MP3";

        // Generate fake waveform data
        m_waveformData.clear();
        m_waveformData.reserve(WAVEFORM_SAMPLES);

        for (int i = 0; i < WAVEFORM_SAMPLES; ++i) {
            float t = static_cast<float>(i) / WAVEFORM_SAMPLES;

            // Create a pseudo-random but visually pleasing waveform
            float value = 0.0f;
            value += std::sin(t * 20.0f) * 0.3f;
            value += std::sin(t * 50.0f) * 0.2f;
            value += std::sin(t * 100.0f) * 0.1f;
            value *= (1.0f - t * 0.3f); // Fade out

            m_waveformData.push_back(value);
        }

        m_isLoaded = true;
        m_currentTime = 0.0f;
        m_playbackState = PlaybackState::Stopped;

        spdlog::info("AudioPreview: Audio loaded successfully");

    } catch (const std::exception& e) {
        spdlog::error("AudioPreview: Failed to load audio: {}", e.what());
        m_isLoaded = false;
    }
}

void AudioPreview::Play() {
    if (!m_isLoaded) {
        return;
    }

    if (m_playbackState == PlaybackState::Stopped) {
        m_currentTime = 0.0f;
    }

    m_playbackState = PlaybackState::Playing;
    spdlog::debug("AudioPreview: Playing");

    // TODO: Start actual audio playback
}

void AudioPreview::Pause() {
    if (!m_isLoaded) {
        return;
    }

    m_playbackState = PlaybackState::Paused;
    spdlog::debug("AudioPreview: Paused");

    // TODO: Pause actual audio playback
}

void AudioPreview::Stop() {
    if (!m_isLoaded) {
        return;
    }

    m_playbackState = PlaybackState::Stopped;
    m_currentTime = 0.0f;
    spdlog::debug("AudioPreview: Stopped");

    // TODO: Stop actual audio playback
}

void AudioPreview::Seek(float position) {
    if (!m_isLoaded) {
        return;
    }

    m_currentTime = std::clamp(position, 0.0f, m_duration);
    spdlog::debug("AudioPreview: Seek to {:.2f}", m_currentTime);

    // TODO: Seek in actual audio playback
}

std::string AudioPreview::FormatTime(float seconds) const {
    int totalSeconds = static_cast<int>(seconds);
    int minutes = totalSeconds / 60;
    int secs = totalSeconds % 60;

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, secs);
    return std::string(buffer);
}

std::string AudioPreview::GetEditorName() const {
    return "Audio Preview - " + m_audioName;
}

bool AudioPreview::IsDirty() const {
    return m_isDirty;
}

void AudioPreview::Save() {
    // Audio preview is read-only, no save needed
    m_isDirty = false;
}

std::string AudioPreview::GetAssetPath() const {
    return m_assetPath;
}
