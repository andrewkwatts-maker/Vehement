#pragma once

#include "AssetEditor.hpp"
#include <imgui.h>
#include <vector>
#include <memory>

namespace Nova {
    class AudioBuffer;
    class AudioSource;
}

/**
 * @brief Audio file preview and editor
 *
 * Features:
 * - Waveform visualization
 * - Play/Pause/Stop controls
 * - Seek bar for scrubbing
 * - Volume control
 * - Loop toggle
 * - Display audio properties (sample rate, channels, bitrate, duration)
 * - Time display (current / total)
 */
class AudioPreview : public IAssetEditor {
public:
    AudioPreview();
    ~AudioPreview() override;

    void Open(const std::string& assetPath) override;
    void Render(bool* isOpen) override;
    std::string GetEditorName() const override;
    bool IsDirty() const override;
    void Save() override;
    std::string GetAssetPath() const override;

    /**
     * @brief Update audio playback (should be called regularly)
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

private:
    void RenderWaveform();
    void RenderControls();
    void RenderProperties();
    void LoadAudio();
    void Play();
    void Pause();
    void Stop();
    void Seek(float position);
    std::string FormatTime(float seconds) const;

    std::string m_assetPath;
    std::string m_audioName;
    bool m_isDirty = false;
    bool m_isLoaded = false;

    // Audio properties
    int m_sampleRate = 0;
    int m_channels = 0;
    int m_bitrate = 0;
    float m_duration = 0.0f;
    std::string m_format = "Unknown";
    size_t m_fileSize = 0;

    // Playback state
    enum class PlaybackState {
        Stopped,
        Playing,
        Paused
    };
    PlaybackState m_playbackState = PlaybackState::Stopped;
    float m_currentTime = 0.0f;
    float m_volume = 1.0f;
    bool m_loop = false;

    // Waveform data (simplified for visualization)
    std::vector<float> m_waveformData;
    static constexpr int WAVEFORM_SAMPLES = 1000;

    // UI state
    bool m_isDraggingSeek = false;

    // Audio engine integration
    std::shared_ptr<Nova::AudioBuffer> m_audioBuffer;
    std::shared_ptr<Nova::AudioSource> m_audioSource;
};
