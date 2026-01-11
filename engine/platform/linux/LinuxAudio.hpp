#pragma once

/**
 * @file LinuxAudio.hpp
 * @brief Linux audio backend with PulseAudio and ALSA support
 *
 * Provides audio output for Linux platforms using:
 * - PulseAudio (preferred, modern Linux desktops)
 * - ALSA (fallback, direct hardware access)
 */

#ifdef NOVA_PLATFORM_LINUX

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstdint>

namespace Nova {

/**
 * @brief Audio backend type
 */
enum class LinuxAudioBackend {
    None,
    PulseAudio,
    ALSA
};

/**
 * @brief Audio format specification
 */
struct AudioFormat {
    uint32_t sampleRate = 44100;
    uint32_t channels = 2;
    uint32_t bitsPerSample = 16;

    [[nodiscard]] uint32_t GetBytesPerFrame() const {
        return channels * (bitsPerSample / 8);
    }

    [[nodiscard]] uint32_t GetBytesPerSecond() const {
        return sampleRate * GetBytesPerFrame();
    }
};

/**
 * @brief Audio device information
 */
struct AudioDeviceInfo {
    std::string name;
    std::string description;
    bool isDefault = false;
    uint32_t maxChannels = 2;
    std::vector<uint32_t> supportedSampleRates;
};

/**
 * @brief Audio buffer callback type
 * @param buffer Output buffer to fill
 * @param frames Number of frames to generate
 * @param userData User data pointer
 */
using AudioCallback = std::function<void(float* buffer, uint32_t frames, void* userData)>;

/**
 * @brief Linux audio manager
 *
 * Provides audio playback using PulseAudio (primary) or ALSA (fallback).
 * Supports real-time audio callback for game audio mixing.
 */
class LinuxAudio {
public:
    LinuxAudio();
    ~LinuxAudio();

    // Non-copyable
    LinuxAudio(const LinuxAudio&) = delete;
    LinuxAudio& operator=(const LinuxAudio&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Check if PulseAudio is available
     */
    static bool IsPulseAudioAvailable();

    /**
     * @brief Check if ALSA is available
     */
    static bool IsALSAAvailable();

    /**
     * @brief Initialize audio system
     * @param preferredBackend Preferred backend (will fallback if unavailable)
     * @return true if initialization succeeded
     */
    bool Initialize(LinuxAudioBackend preferredBackend = LinuxAudioBackend::PulseAudio);

    /**
     * @brief Shutdown audio system
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Get current backend
     */
    [[nodiscard]] LinuxAudioBackend GetBackend() const { return m_backend; }

    // =========================================================================
    // Device Enumeration
    // =========================================================================

    /**
     * @brief Get list of available output devices
     */
    [[nodiscard]] std::vector<AudioDeviceInfo> GetOutputDevices() const;

    /**
     * @brief Get default output device name
     */
    [[nodiscard]] std::string GetDefaultOutputDevice() const;

    /**
     * @brief Set output device
     * @param deviceName Device name (empty for default)
     * @return true if device was set successfully
     */
    bool SetOutputDevice(const std::string& deviceName);

    // =========================================================================
    // Stream Management
    // =========================================================================

    /**
     * @brief Open audio stream with callback
     * @param format Audio format
     * @param callback Audio callback function
     * @param userData User data passed to callback
     * @param bufferFrames Buffer size in frames (0 for default)
     * @return true if stream opened successfully
     */
    bool OpenStream(const AudioFormat& format, AudioCallback callback,
                    void* userData = nullptr, uint32_t bufferFrames = 0);

    /**
     * @brief Close audio stream
     */
    void CloseStream();

    /**
     * @brief Check if stream is open
     */
    [[nodiscard]] bool IsStreamOpen() const { return m_streamOpen; }

    /**
     * @brief Start audio playback
     */
    bool Start();

    /**
     * @brief Stop audio playback
     */
    void Stop();

    /**
     * @brief Check if audio is playing
     */
    [[nodiscard]] bool IsPlaying() const { return m_playing; }

    /**
     * @brief Pause audio playback
     */
    void Pause();

    /**
     * @brief Resume audio playback
     */
    void Resume();

    // =========================================================================
    // Simple Playback API (for sound effects)
    // =========================================================================

    /**
     * @brief Play a buffer of audio samples
     * @param samples Audio samples (interleaved, float -1.0 to 1.0)
     * @param numSamples Number of samples (total, not per channel)
     * @param format Audio format of samples
     */
    void PlayBuffer(const float* samples, uint32_t numSamples, const AudioFormat& format);

    /**
     * @brief Play audio from file (WAV, OGG, etc.)
     * @param filename Path to audio file
     * @param loop Loop playback
     * @return Sound handle for control, or 0 on failure
     */
    uint32_t PlayFile(const std::string& filename, bool loop = false);

    /**
     * @brief Stop a playing sound
     * @param handle Sound handle from PlayFile
     */
    void StopSound(uint32_t handle);

    /**
     * @brief Set volume for a sound
     * @param handle Sound handle
     * @param volume Volume (0.0 to 1.0)
     */
    void SetSoundVolume(uint32_t handle, float volume);

    // =========================================================================
    // Volume Control
    // =========================================================================

    /**
     * @brief Set master volume
     * @param volume Volume (0.0 to 1.0)
     */
    void SetMasterVolume(float volume);

    /**
     * @brief Get master volume
     */
    [[nodiscard]] float GetMasterVolume() const { return m_masterVolume; }

    /**
     * @brief Mute/unmute audio
     */
    void SetMuted(bool muted);

    /**
     * @brief Check if muted
     */
    [[nodiscard]] bool IsMuted() const { return m_muted; }

    // =========================================================================
    // Latency Information
    // =========================================================================

    /**
     * @brief Get current audio latency in milliseconds
     */
    [[nodiscard]] float GetLatency() const;

    /**
     * @brief Get current buffer underrun count
     */
    [[nodiscard]] uint32_t GetUnderrunCount() const { return m_underrunCount; }

private:
    // Backend-specific implementation
    class PulseAudioImpl;
    class ALSAImpl;

    std::unique_ptr<PulseAudioImpl> m_pulseImpl;
    std::unique_ptr<ALSAImpl> m_alsaImpl;

    // State
    LinuxAudioBackend m_backend = LinuxAudioBackend::None;
    bool m_initialized = false;
    std::atomic<bool> m_streamOpen{false};
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_muted{false};
    std::atomic<float> m_masterVolume{1.0f};
    std::atomic<uint32_t> m_underrunCount{0};

    // Audio format
    AudioFormat m_format;
    AudioCallback m_callback;
    void* m_userData = nullptr;

    // Audio thread
    std::thread m_audioThread;
    std::atomic<bool> m_threadRunning{false};
    std::mutex m_mutex;

    // Simple playback tracking
    std::atomic<uint32_t> m_nextSoundHandle{1};

    // Internal methods
    bool InitializePulseAudio();
    bool InitializeALSA();
    void AudioThreadFunc();
};

} // namespace Nova

#endif // NOVA_PLATFORM_LINUX
