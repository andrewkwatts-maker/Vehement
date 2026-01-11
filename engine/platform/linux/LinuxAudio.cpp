/**
 * @file LinuxAudio.cpp
 * @brief Linux audio implementation with PulseAudio and ALSA support
 */

#include "LinuxAudio.hpp"

#ifdef NOVA_PLATFORM_LINUX

#include <cstring>
#include <cmath>
#include <iostream>
#include <dlfcn.h>
#include <algorithm>

// PulseAudio headers (dynamically loaded)
typedef struct pa_simple pa_simple;
typedef struct pa_sample_spec pa_sample_spec;
typedef struct pa_buffer_attr pa_buffer_attr;

// PulseAudio sample format
enum pa_sample_format {
    PA_SAMPLE_U8,
    PA_SAMPLE_ALAW,
    PA_SAMPLE_ULAW,
    PA_SAMPLE_S16LE,
    PA_SAMPLE_S16BE,
    PA_SAMPLE_FLOAT32LE,
    PA_SAMPLE_FLOAT32BE,
    PA_SAMPLE_S32LE,
    PA_SAMPLE_S32BE,
    PA_SAMPLE_S24LE,
    PA_SAMPLE_S24BE,
    PA_SAMPLE_S24_32LE,
    PA_SAMPLE_S24_32BE,
    PA_SAMPLE_MAX,
    PA_SAMPLE_INVALID = -1
};

// PulseAudio stream direction
enum pa_stream_direction {
    PA_STREAM_NODIRECTION,
    PA_STREAM_PLAYBACK,
    PA_STREAM_RECORD,
    PA_STREAM_UPLOAD
};

// ALSA headers (dynamically loaded)
typedef struct _snd_pcm snd_pcm_t;
typedef struct _snd_pcm_hw_params snd_pcm_hw_params_t;

// ALSA formats
enum snd_pcm_format_t {
    SND_PCM_FORMAT_S16_LE = 2,
    SND_PCM_FORMAT_S32_LE = 10,
    SND_PCM_FORMAT_FLOAT_LE = 14
};

// ALSA access modes
enum snd_pcm_access_t {
    SND_PCM_ACCESS_MMAP_INTERLEAVED = 0,
    SND_PCM_ACCESS_MMAP_NONINTERLEAVED,
    SND_PCM_ACCESS_MMAP_COMPLEX,
    SND_PCM_ACCESS_RW_INTERLEAVED,
    SND_PCM_ACCESS_RW_NONINTERLEAVED
};

// ALSA stream types
enum snd_pcm_stream_t {
    SND_PCM_STREAM_PLAYBACK = 0,
    SND_PCM_STREAM_CAPTURE
};

namespace Nova {

// =============================================================================
// PulseAudio Implementation
// =============================================================================

class LinuxAudio::PulseAudioImpl {
public:
    PulseAudioImpl() = default;
    ~PulseAudioImpl() { Shutdown(); }

    bool Initialize() {
        // Load PulseAudio library
        m_lib = dlopen("libpulse-simple.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!m_lib) {
            m_lib = dlopen("libpulse-simple.so", RTLD_NOW | RTLD_LOCAL);
        }
        if (!m_lib) {
            std::cerr << "LinuxAudio: Failed to load PulseAudio library\n";
            return false;
        }

        // Load function pointers
        m_pa_simple_new = reinterpret_cast<decltype(m_pa_simple_new)>(
            dlsym(m_lib, "pa_simple_new"));
        m_pa_simple_free = reinterpret_cast<decltype(m_pa_simple_free)>(
            dlsym(m_lib, "pa_simple_free"));
        m_pa_simple_write = reinterpret_cast<decltype(m_pa_simple_write)>(
            dlsym(m_lib, "pa_simple_write"));
        m_pa_simple_drain = reinterpret_cast<decltype(m_pa_simple_drain)>(
            dlsym(m_lib, "pa_simple_drain"));
        m_pa_simple_get_latency = reinterpret_cast<decltype(m_pa_simple_get_latency)>(
            dlsym(m_lib, "pa_simple_get_latency"));
        m_pa_strerror = reinterpret_cast<decltype(m_pa_strerror)>(
            dlsym(m_lib, "pa_strerror"));

        if (!m_pa_simple_new || !m_pa_simple_free || !m_pa_simple_write) {
            std::cerr << "LinuxAudio: Failed to load PulseAudio functions\n";
            dlclose(m_lib);
            m_lib = nullptr;
            return false;
        }

        return true;
    }

    void Shutdown() {
        CloseStream();
        if (m_lib) {
            dlclose(m_lib);
            m_lib = nullptr;
        }
    }

    bool OpenStream(const AudioFormat& format, uint32_t bufferFrames) {
        if (m_stream) {
            CloseStream();
        }

        // Set up sample spec
        pa_sample_spec ss;
        ss.format = PA_SAMPLE_FLOAT32LE;  // We'll convert to float
        ss.rate = format.sampleRate;
        ss.channels = static_cast<uint8_t>(format.channels);

        // Buffer attributes for low latency
        pa_buffer_attr ba;
        uint32_t bytesPerFrame = format.channels * sizeof(float);
        uint32_t bufferSize = bufferFrames > 0 ? bufferFrames * bytesPerFrame :
                              (format.sampleRate / 20) * bytesPerFrame;  // ~50ms default

        ba.maxlength = static_cast<uint32_t>(-1);  // Maximum buffer size
        ba.tlength = bufferSize;                    // Target buffer length
        ba.prebuf = static_cast<uint32_t>(-1);     // Start threshold
        ba.minreq = static_cast<uint32_t>(-1);     // Minimum request
        ba.fragsize = static_cast<uint32_t>(-1);   // Fragment size (for recording)

        int error;
        m_stream = m_pa_simple_new(
            nullptr,                    // Server (NULL = default)
            "Nova3D Engine",           // Application name
            PA_STREAM_PLAYBACK,        // Stream direction
            nullptr,                    // Device (NULL = default)
            "Game Audio",              // Stream name
            &ss,                       // Sample spec
            nullptr,                   // Channel map (NULL = default)
            &ba,                       // Buffer attributes
            &error                     // Error code
        );

        if (!m_stream) {
            const char* errStr = m_pa_strerror ? m_pa_strerror(error) : "Unknown error";
            std::cerr << "LinuxAudio: Failed to open PulseAudio stream: " << errStr << "\n";
            return false;
        }

        m_format = format;
        return true;
    }

    void CloseStream() {
        if (m_stream) {
            if (m_pa_simple_drain) {
                int error;
                m_pa_simple_drain(m_stream, &error);
            }
            m_pa_simple_free(m_stream);
            m_stream = nullptr;
        }
    }

    bool Write(const float* buffer, uint32_t frames) {
        if (!m_stream || !m_pa_simple_write) {
            return false;
        }

        size_t bytes = frames * m_format.channels * sizeof(float);
        int error;
        int result = m_pa_simple_write(m_stream, buffer, bytes, &error);

        if (result < 0) {
            const char* errStr = m_pa_strerror ? m_pa_strerror(error) : "Unknown error";
            std::cerr << "LinuxAudio: PulseAudio write error: " << errStr << "\n";
            return false;
        }

        return true;
    }

    float GetLatency() const {
        if (!m_stream || !m_pa_simple_get_latency) {
            return 0.0f;
        }

        int error;
        uint64_t latency = m_pa_simple_get_latency(m_stream, &error);
        return static_cast<float>(latency) / 1000.0f;  // Convert microseconds to milliseconds
    }

    bool IsValid() const { return m_lib != nullptr; }

private:
    void* m_lib = nullptr;
    pa_simple* m_stream = nullptr;
    AudioFormat m_format;

    // Function pointers
    pa_simple* (*m_pa_simple_new)(const char*, const char*, int,
                                   const char*, const char*,
                                   const pa_sample_spec*,
                                   void*, const pa_buffer_attr*,
                                   int*) = nullptr;
    void (*m_pa_simple_free)(pa_simple*) = nullptr;
    int (*m_pa_simple_write)(pa_simple*, const void*, size_t, int*) = nullptr;
    int (*m_pa_simple_drain)(pa_simple*, int*) = nullptr;
    uint64_t (*m_pa_simple_get_latency)(pa_simple*, int*) = nullptr;
    const char* (*m_pa_strerror)(int) = nullptr;
};

// =============================================================================
// ALSA Implementation
// =============================================================================

class LinuxAudio::ALSAImpl {
public:
    ALSAImpl() = default;
    ~ALSAImpl() { Shutdown(); }

    bool Initialize() {
        // Load ALSA library
        m_lib = dlopen("libasound.so.2", RTLD_NOW | RTLD_LOCAL);
        if (!m_lib) {
            m_lib = dlopen("libasound.so", RTLD_NOW | RTLD_LOCAL);
        }
        if (!m_lib) {
            std::cerr << "LinuxAudio: Failed to load ALSA library\n";
            return false;
        }

        // Load function pointers
        m_snd_pcm_open = reinterpret_cast<decltype(m_snd_pcm_open)>(
            dlsym(m_lib, "snd_pcm_open"));
        m_snd_pcm_close = reinterpret_cast<decltype(m_snd_pcm_close)>(
            dlsym(m_lib, "snd_pcm_close"));
        m_snd_pcm_hw_params_malloc = reinterpret_cast<decltype(m_snd_pcm_hw_params_malloc)>(
            dlsym(m_lib, "snd_pcm_hw_params_malloc"));
        m_snd_pcm_hw_params_free = reinterpret_cast<decltype(m_snd_pcm_hw_params_free)>(
            dlsym(m_lib, "snd_pcm_hw_params_free"));
        m_snd_pcm_hw_params_any = reinterpret_cast<decltype(m_snd_pcm_hw_params_any)>(
            dlsym(m_lib, "snd_pcm_hw_params_any"));
        m_snd_pcm_hw_params_set_access = reinterpret_cast<decltype(m_snd_pcm_hw_params_set_access)>(
            dlsym(m_lib, "snd_pcm_hw_params_set_access"));
        m_snd_pcm_hw_params_set_format = reinterpret_cast<decltype(m_snd_pcm_hw_params_set_format)>(
            dlsym(m_lib, "snd_pcm_hw_params_set_format"));
        m_snd_pcm_hw_params_set_rate_near = reinterpret_cast<decltype(m_snd_pcm_hw_params_set_rate_near)>(
            dlsym(m_lib, "snd_pcm_hw_params_set_rate_near"));
        m_snd_pcm_hw_params_set_channels = reinterpret_cast<decltype(m_snd_pcm_hw_params_set_channels)>(
            dlsym(m_lib, "snd_pcm_hw_params_set_channels"));
        m_snd_pcm_hw_params_set_buffer_size_near = reinterpret_cast<decltype(m_snd_pcm_hw_params_set_buffer_size_near)>(
            dlsym(m_lib, "snd_pcm_hw_params_set_buffer_size_near"));
        m_snd_pcm_hw_params = reinterpret_cast<decltype(m_snd_pcm_hw_params)>(
            dlsym(m_lib, "snd_pcm_hw_params"));
        m_snd_pcm_prepare = reinterpret_cast<decltype(m_snd_pcm_prepare)>(
            dlsym(m_lib, "snd_pcm_prepare"));
        m_snd_pcm_writei = reinterpret_cast<decltype(m_snd_pcm_writei)>(
            dlsym(m_lib, "snd_pcm_writei"));
        m_snd_pcm_drain = reinterpret_cast<decltype(m_snd_pcm_drain)>(
            dlsym(m_lib, "snd_pcm_drain"));
        m_snd_pcm_recover = reinterpret_cast<decltype(m_snd_pcm_recover)>(
            dlsym(m_lib, "snd_pcm_recover"));
        m_snd_pcm_delay = reinterpret_cast<decltype(m_snd_pcm_delay)>(
            dlsym(m_lib, "snd_pcm_delay"));
        m_snd_strerror = reinterpret_cast<decltype(m_snd_strerror)>(
            dlsym(m_lib, "snd_strerror"));

        if (!m_snd_pcm_open || !m_snd_pcm_close || !m_snd_pcm_writei) {
            std::cerr << "LinuxAudio: Failed to load ALSA functions\n";
            dlclose(m_lib);
            m_lib = nullptr;
            return false;
        }

        return true;
    }

    void Shutdown() {
        CloseStream();
        if (m_lib) {
            dlclose(m_lib);
            m_lib = nullptr;
        }
    }

    bool OpenStream(const AudioFormat& format, uint32_t bufferFrames) {
        if (m_pcm) {
            CloseStream();
        }

        // Open default device
        int err = m_snd_pcm_open(&m_pcm, m_deviceName.empty() ? "default" : m_deviceName.c_str(),
                                  SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0) {
            std::cerr << "LinuxAudio: Failed to open ALSA device: " << GetError(err) << "\n";
            return false;
        }

        // Allocate hardware params
        snd_pcm_hw_params_t* hwParams = nullptr;
        m_snd_pcm_hw_params_malloc(&hwParams);
        m_snd_pcm_hw_params_any(m_pcm, hwParams);

        // Set parameters
        m_snd_pcm_hw_params_set_access(m_pcm, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
        m_snd_pcm_hw_params_set_format(m_pcm, hwParams, SND_PCM_FORMAT_FLOAT_LE);

        unsigned int rate = format.sampleRate;
        m_snd_pcm_hw_params_set_rate_near(m_pcm, hwParams, &rate, nullptr);
        m_snd_pcm_hw_params_set_channels(m_pcm, hwParams, format.channels);

        // Set buffer size
        unsigned long frames = bufferFrames > 0 ? bufferFrames : format.sampleRate / 20;
        m_snd_pcm_hw_params_set_buffer_size_near(m_pcm, hwParams, &frames);

        // Apply parameters
        err = m_snd_pcm_hw_params(m_pcm, hwParams);
        m_snd_pcm_hw_params_free(hwParams);

        if (err < 0) {
            std::cerr << "LinuxAudio: Failed to set ALSA parameters: " << GetError(err) << "\n";
            m_snd_pcm_close(m_pcm);
            m_pcm = nullptr;
            return false;
        }

        // Prepare device
        err = m_snd_pcm_prepare(m_pcm);
        if (err < 0) {
            std::cerr << "LinuxAudio: Failed to prepare ALSA device: " << GetError(err) << "\n";
            m_snd_pcm_close(m_pcm);
            m_pcm = nullptr;
            return false;
        }

        m_format = format;
        return true;
    }

    void CloseStream() {
        if (m_pcm) {
            m_snd_pcm_drain(m_pcm);
            m_snd_pcm_close(m_pcm);
            m_pcm = nullptr;
        }
    }

    bool Write(const float* buffer, uint32_t frames) {
        if (!m_pcm || !m_snd_pcm_writei) {
            return false;
        }

        long written = m_snd_pcm_writei(m_pcm, buffer, frames);

        if (written < 0) {
            // Try to recover from error (underrun, etc.)
            written = m_snd_pcm_recover(m_pcm, static_cast<int>(written), 0);
            if (written < 0) {
                std::cerr << "LinuxAudio: ALSA write error: " << GetError(static_cast<int>(written)) << "\n";
                return false;
            }
            m_underruns++;
        }

        return true;
    }

    float GetLatency() const {
        if (!m_pcm || !m_snd_pcm_delay) {
            return 0.0f;
        }

        long frames;
        int err = m_snd_pcm_delay(m_pcm, &frames);
        if (err < 0) {
            return 0.0f;
        }

        return (static_cast<float>(frames) / static_cast<float>(m_format.sampleRate)) * 1000.0f;
    }

    void SetDevice(const std::string& device) { m_deviceName = device; }
    uint32_t GetUnderruns() const { return m_underruns; }
    bool IsValid() const { return m_lib != nullptr; }

private:
    const char* GetError(int err) const {
        return m_snd_strerror ? m_snd_strerror(err) : "Unknown error";
    }

    void* m_lib = nullptr;
    snd_pcm_t* m_pcm = nullptr;
    AudioFormat m_format;
    std::string m_deviceName;
    uint32_t m_underruns = 0;

    // Function pointers
    int (*m_snd_pcm_open)(snd_pcm_t**, const char*, int, int) = nullptr;
    int (*m_snd_pcm_close)(snd_pcm_t*) = nullptr;
    int (*m_snd_pcm_hw_params_malloc)(snd_pcm_hw_params_t**) = nullptr;
    void (*m_snd_pcm_hw_params_free)(snd_pcm_hw_params_t*) = nullptr;
    int (*m_snd_pcm_hw_params_any)(snd_pcm_t*, snd_pcm_hw_params_t*) = nullptr;
    int (*m_snd_pcm_hw_params_set_access)(snd_pcm_t*, snd_pcm_hw_params_t*, int) = nullptr;
    int (*m_snd_pcm_hw_params_set_format)(snd_pcm_t*, snd_pcm_hw_params_t*, int) = nullptr;
    int (*m_snd_pcm_hw_params_set_rate_near)(snd_pcm_t*, snd_pcm_hw_params_t*, unsigned int*, int*) = nullptr;
    int (*m_snd_pcm_hw_params_set_channels)(snd_pcm_t*, snd_pcm_hw_params_t*, unsigned int) = nullptr;
    int (*m_snd_pcm_hw_params_set_buffer_size_near)(snd_pcm_t*, snd_pcm_hw_params_t*, unsigned long*) = nullptr;
    int (*m_snd_pcm_hw_params)(snd_pcm_t*, snd_pcm_hw_params_t*) = nullptr;
    int (*m_snd_pcm_prepare)(snd_pcm_t*) = nullptr;
    long (*m_snd_pcm_writei)(snd_pcm_t*, const void*, unsigned long) = nullptr;
    int (*m_snd_pcm_drain)(snd_pcm_t*) = nullptr;
    int (*m_snd_pcm_recover)(snd_pcm_t*, int, int) = nullptr;
    int (*m_snd_pcm_delay)(snd_pcm_t*, long*) = nullptr;
    const char* (*m_snd_strerror)(int) = nullptr;
};

// =============================================================================
// LinuxAudio Implementation
// =============================================================================

LinuxAudio::LinuxAudio() = default;

LinuxAudio::~LinuxAudio() {
    Shutdown();
}

bool LinuxAudio::IsPulseAudioAvailable() {
    void* lib = dlopen("libpulse-simple.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        lib = dlopen("libpulse-simple.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (lib) {
        dlclose(lib);
        return true;
    }
    return false;
}

bool LinuxAudio::IsALSAAvailable() {
    void* lib = dlopen("libasound.so.2", RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        lib = dlopen("libasound.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (lib) {
        dlclose(lib);
        return true;
    }
    return false;
}

bool LinuxAudio::Initialize(LinuxAudioBackend preferredBackend) {
    if (m_initialized) {
        return true;
    }

    // Try preferred backend first
    if (preferredBackend == LinuxAudioBackend::PulseAudio) {
        if (InitializePulseAudio()) {
            m_backend = LinuxAudioBackend::PulseAudio;
            m_initialized = true;
            std::cout << "LinuxAudio: Initialized with PulseAudio\n";
            return true;
        }
        // Fallback to ALSA
        if (InitializeALSA()) {
            m_backend = LinuxAudioBackend::ALSA;
            m_initialized = true;
            std::cout << "LinuxAudio: Initialized with ALSA (PulseAudio fallback)\n";
            return true;
        }
    } else if (preferredBackend == LinuxAudioBackend::ALSA) {
        if (InitializeALSA()) {
            m_backend = LinuxAudioBackend::ALSA;
            m_initialized = true;
            std::cout << "LinuxAudio: Initialized with ALSA\n";
            return true;
        }
        // Fallback to PulseAudio
        if (InitializePulseAudio()) {
            m_backend = LinuxAudioBackend::PulseAudio;
            m_initialized = true;
            std::cout << "LinuxAudio: Initialized with PulseAudio (ALSA fallback)\n";
            return true;
        }
    }

    std::cerr << "LinuxAudio: Failed to initialize any audio backend\n";
    return false;
}

bool LinuxAudio::InitializePulseAudio() {
    m_pulseImpl = std::make_unique<PulseAudioImpl>();
    if (!m_pulseImpl->Initialize()) {
        m_pulseImpl.reset();
        return false;
    }
    return true;
}

bool LinuxAudio::InitializeALSA() {
    m_alsaImpl = std::make_unique<ALSAImpl>();
    if (!m_alsaImpl->Initialize()) {
        m_alsaImpl.reset();
        return false;
    }
    return true;
}

void LinuxAudio::Shutdown() {
    Stop();
    CloseStream();

    m_pulseImpl.reset();
    m_alsaImpl.reset();

    m_backend = LinuxAudioBackend::None;
    m_initialized = false;
}

std::vector<AudioDeviceInfo> LinuxAudio::GetOutputDevices() const {
    std::vector<AudioDeviceInfo> devices;

    // Add default device
    AudioDeviceInfo defaultDevice;
    defaultDevice.name = "default";
    defaultDevice.description = "Default Audio Device";
    defaultDevice.isDefault = true;
    defaultDevice.maxChannels = 8;
    defaultDevice.supportedSampleRates = {44100, 48000, 88200, 96000, 192000};
    devices.push_back(defaultDevice);

    return devices;
}

std::string LinuxAudio::GetDefaultOutputDevice() const {
    return "default";
}

bool LinuxAudio::SetOutputDevice(const std::string& deviceName) {
    if (m_alsaImpl) {
        m_alsaImpl->SetDevice(deviceName);
    }
    return true;
}

bool LinuxAudio::OpenStream(const AudioFormat& format, AudioCallback callback,
                            void* userData, uint32_t bufferFrames) {
    if (!m_initialized) {
        return false;
    }

    m_format = format;
    m_callback = std::move(callback);
    m_userData = userData;

    bool success = false;
    if (m_backend == LinuxAudioBackend::PulseAudio && m_pulseImpl) {
        success = m_pulseImpl->OpenStream(format, bufferFrames);
    } else if (m_backend == LinuxAudioBackend::ALSA && m_alsaImpl) {
        success = m_alsaImpl->OpenStream(format, bufferFrames);
    }

    if (success) {
        m_streamOpen = true;
    }

    return success;
}

void LinuxAudio::CloseStream() {
    Stop();

    if (m_pulseImpl) {
        m_pulseImpl->CloseStream();
    }
    if (m_alsaImpl) {
        m_alsaImpl->CloseStream();
    }

    m_streamOpen = false;
    m_callback = nullptr;
    m_userData = nullptr;
}

bool LinuxAudio::Start() {
    if (!m_streamOpen || m_playing) {
        return false;
    }

    m_threadRunning = true;
    m_playing = true;

    m_audioThread = std::thread(&LinuxAudio::AudioThreadFunc, this);

    return true;
}

void LinuxAudio::Stop() {
    if (!m_playing) {
        return;
    }

    m_threadRunning = false;
    m_playing = false;

    if (m_audioThread.joinable()) {
        m_audioThread.join();
    }
}

void LinuxAudio::Pause() {
    m_playing = false;
}

void LinuxAudio::Resume() {
    if (m_streamOpen && m_threadRunning) {
        m_playing = true;
    }
}

void LinuxAudio::AudioThreadFunc() {
    const uint32_t bufferFrames = m_format.sampleRate / 50;  // ~20ms buffer
    std::vector<float> buffer(bufferFrames * m_format.channels);

    while (m_threadRunning) {
        if (!m_playing) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Generate audio via callback
        if (m_callback) {
            m_callback(buffer.data(), bufferFrames, m_userData);
        } else {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
        }

        // Apply master volume and mute
        if (m_muted) {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
        } else {
            float volume = m_masterVolume.load();
            for (auto& sample : buffer) {
                sample *= volume;
            }
        }

        // Write to backend
        bool success = false;
        if (m_backend == LinuxAudioBackend::PulseAudio && m_pulseImpl) {
            success = m_pulseImpl->Write(buffer.data(), bufferFrames);
        } else if (m_backend == LinuxAudioBackend::ALSA && m_alsaImpl) {
            success = m_alsaImpl->Write(buffer.data(), bufferFrames);
        }

        if (!success) {
            m_underrunCount++;
        }
    }
}

void LinuxAudio::PlayBuffer(const float* samples, uint32_t numSamples, const AudioFormat& format) {
    (void)format;  // Use stream format

    if (!m_streamOpen) {
        return;
    }

    uint32_t frames = numSamples / m_format.channels;

    if (m_backend == LinuxAudioBackend::PulseAudio && m_pulseImpl) {
        m_pulseImpl->Write(samples, frames);
    } else if (m_backend == LinuxAudioBackend::ALSA && m_alsaImpl) {
        m_alsaImpl->Write(samples, frames);
    }
}

uint32_t LinuxAudio::PlayFile(const std::string& filename, bool loop) {
    (void)filename;
    (void)loop;
    // This would require audio file loading (e.g., using libsndfile)
    // For now, return a placeholder handle
    return m_nextSoundHandle++;
}

void LinuxAudio::StopSound(uint32_t handle) {
    (void)handle;
    // Would stop the sound with given handle
}

void LinuxAudio::SetSoundVolume(uint32_t handle, float volume) {
    (void)handle;
    (void)volume;
    // Would set volume for specific sound
}

void LinuxAudio::SetMasterVolume(float volume) {
    m_masterVolume = std::clamp(volume, 0.0f, 1.0f);
}

void LinuxAudio::SetMuted(bool muted) {
    m_muted = muted;
}

float LinuxAudio::GetLatency() const {
    if (m_backend == LinuxAudioBackend::PulseAudio && m_pulseImpl) {
        return m_pulseImpl->GetLatency();
    } else if (m_backend == LinuxAudioBackend::ALSA && m_alsaImpl) {
        return m_alsaImpl->GetLatency();
    }
    return 0.0f;
}

} // namespace Nova

#endif // NOVA_PLATFORM_LINUX
