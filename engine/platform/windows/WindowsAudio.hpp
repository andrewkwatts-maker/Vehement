#pragma once

/**
 * @file WindowsAudio.hpp
 * @brief Windows audio using WASAPI
 *
 * Features:
 * - Audio device enumeration
 * - Output stream (playback)
 * - Input stream (microphone)
 * - Spatial audio support (Windows Sonic)
 * - Low latency exclusive mode
 */

#ifdef NOVA_PLATFORM_WINDOWS

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>
#include <atomic>

namespace Nova {

/**
 * @brief Audio device information
 */
struct AudioDeviceInfo {
    std::string id;
    std::string name;
    bool isDefault = false;
    bool isInput = false;
    int channels = 2;
    int sampleRate = 48000;
};

/**
 * @brief Audio format
 */
struct AudioFormat {
    int channels = 2;
    int sampleRate = 48000;
    int bitsPerSample = 16;
    bool floatFormat = false;
};

/**
 * @brief Audio stream configuration
 */
struct AudioStreamConfig {
    AudioFormat format;
    int bufferSizeFrames = 1024;  // Buffer size in frames
    bool exclusiveMode = false;   // Low latency exclusive mode
    bool eventDriven = true;      // Use event-driven mode
    std::string deviceId;         // Empty for default device
};

/**
 * @brief Audio callback for filling/reading buffers
 * @param data Pointer to audio buffer
 * @param frames Number of frames to fill/read
 */
using AudioCallback = std::function<void(float* data, int frames)>;

/**
 * @brief Windows audio output stream (WASAPI)
 */
class WindowsAudioOutput {
public:
    WindowsAudioOutput();
    ~WindowsAudioOutput();

    /**
     * @brief Initialize with configuration
     */
    bool Initialize(const AudioStreamConfig& config);

    /**
     * @brief Start playback
     */
    bool Start();

    /**
     * @brief Stop playback
     */
    void Stop();

    /**
     * @brief Check if playing
     */
    [[nodiscard]] bool IsPlaying() const { return m_playing; }

    /**
     * @brief Set audio callback
     */
    void SetCallback(AudioCallback callback) { m_callback = std::move(callback); }

    /**
     * @brief Get buffer size in frames
     */
    [[nodiscard]] int GetBufferSize() const { return m_bufferSize; }

    /**
     * @brief Get latency in milliseconds
     */
    [[nodiscard]] float GetLatency() const;

    /**
     * @brief Get current format
     */
    [[nodiscard]] const AudioFormat& GetFormat() const { return m_format; }

    /**
     * @brief Set volume (0.0 - 1.0)
     */
    void SetVolume(float volume);

    /**
     * @brief Get volume
     */
    [[nodiscard]] float GetVolume() const { return m_volume; }

private:
    void AudioThread();

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    AudioFormat m_format;
    AudioCallback m_callback;
    int m_bufferSize = 1024;
    float m_volume = 1.0f;
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_running{false};
};

/**
 * @brief Windows audio input stream (WASAPI)
 */
class WindowsAudioInput {
public:
    WindowsAudioInput();
    ~WindowsAudioInput();

    /**
     * @brief Initialize with configuration
     */
    bool Initialize(const AudioStreamConfig& config);

    /**
     * @brief Start recording
     */
    bool Start();

    /**
     * @brief Stop recording
     */
    void Stop();

    /**
     * @brief Check if recording
     */
    [[nodiscard]] bool IsRecording() const { return m_recording; }

    /**
     * @brief Set audio callback
     */
    void SetCallback(AudioCallback callback) { m_callback = std::move(callback); }

    /**
     * @brief Get buffer size in frames
     */
    [[nodiscard]] int GetBufferSize() const { return m_bufferSize; }

    /**
     * @brief Get current format
     */
    [[nodiscard]] const AudioFormat& GetFormat() const { return m_format; }

private:
    void AudioThread();

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    AudioFormat m_format;
    AudioCallback m_callback;
    int m_bufferSize = 1024;
    std::atomic<bool> m_recording{false};
    std::atomic<bool> m_running{false};
};

/**
 * @brief Audio device manager
 */
class WindowsAudioDevices {
public:
    /**
     * @brief Get list of output devices
     */
    static std::vector<AudioDeviceInfo> GetOutputDevices();

    /**
     * @brief Get list of input devices
     */
    static std::vector<AudioDeviceInfo> GetInputDevices();

    /**
     * @brief Get default output device
     */
    static AudioDeviceInfo GetDefaultOutputDevice();

    /**
     * @brief Get default input device
     */
    static AudioDeviceInfo GetDefaultInputDevice();

    /**
     * @brief Check if spatial audio is available
     */
    static bool IsSpatialAudioAvailable();
};

} // namespace Nova

#endif // NOVA_PLATFORM_WINDOWS
