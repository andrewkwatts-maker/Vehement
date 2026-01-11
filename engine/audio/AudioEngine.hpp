#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <queue>
#include <mutex>
#include <atomic>
#include <glm/glm.hpp>

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================

class AudioSource;
class AudioBuffer;
class SoundBank;
class AudioBus;

// ============================================================================
// Audio Constants
// ============================================================================

namespace AudioConstants {
    constexpr int kDefaultSampleRate = 44100;
    constexpr int kDefaultBufferSize = 4096;
    constexpr int kMaxVoices = 128;
    constexpr int kMaxBuses = 16;
    constexpr float kSpeedOfSound = 343.0f; // m/s
    constexpr float kDefaultRolloffFactor = 1.0f;
    constexpr float kDefaultReferenceDistance = 1.0f;
    constexpr float kDefaultMaxDistance = 100.0f;
}

// ============================================================================
// Audio Format
// ============================================================================

/**
 * @brief Supported audio formats
 */
enum class AudioFormat : uint8_t {
    Unknown = 0,
    WAV,
    OGG,
    MP3,
    FLAC
};

/**
 * @brief Audio channel configuration
 */
enum class AudioChannels : uint8_t {
    Mono = 1,
    Stereo = 2,
    Surround51 = 6,
    Surround71 = 8
};

// ============================================================================
// 3D Audio Attenuation Models
// ============================================================================

/**
 * @brief Distance attenuation models for 3D audio
 */
enum class AttenuationModel : uint8_t {
    None,           ///< No distance attenuation
    Linear,         ///< Linear falloff
    Inverse,        ///< Inverse distance (realistic)
    InverseClamped, ///< Inverse with clamped minimum
    Exponential     ///< Exponential falloff
};

// ============================================================================
// Audio Effect Types
// ============================================================================

/**
 * @brief Types of audio effects for effect buses
 */
enum class AudioEffectType : uint8_t {
    None = 0,
    Reverb,
    Delay,
    EQ,
    Compressor,
    LowPass,
    HighPass,
    Chorus,
    Distortion
};

/**
 * @brief Reverb preset configurations
 */
enum class ReverbPreset : uint8_t {
    None,
    SmallRoom,
    MediumRoom,
    LargeRoom,
    Hall,
    Cathedral,
    Cave,
    Arena,
    Forest,
    Underwater
};

// ============================================================================
// Audio Effect Parameters
// ============================================================================

/**
 * @brief Reverb effect parameters
 */
struct ReverbParams {
    float roomSize = 0.5f;      ///< 0.0 to 1.0
    float damping = 0.5f;       ///< High frequency damping
    float wetLevel = 0.33f;     ///< Wet signal level
    float dryLevel = 0.4f;      ///< Dry signal level
    float width = 1.0f;         ///< Stereo width
    float preDelay = 0.02f;     ///< Pre-delay in seconds
    float decayTime = 1.5f;     ///< Decay time in seconds
};

/**
 * @brief Delay effect parameters
 */
struct DelayParams {
    float delayTime = 0.25f;    ///< Delay time in seconds
    float feedback = 0.3f;      ///< Feedback amount (0-1)
    float wetLevel = 0.5f;      ///< Wet signal level
    float dryLevel = 1.0f;      ///< Dry signal level
    bool stereo = true;         ///< Stereo ping-pong delay
};

/**
 * @brief Equalizer parameters (3-band)
 */
struct EQParams {
    float lowGain = 1.0f;       ///< Low frequency gain
    float midGain = 1.0f;       ///< Mid frequency gain
    float highGain = 1.0f;      ///< High frequency gain
    float lowFreq = 200.0f;     ///< Low/mid crossover frequency
    float highFreq = 4000.0f;   ///< Mid/high crossover frequency
};

/**
 * @brief Compressor parameters
 */
struct CompressorParams {
    float threshold = -20.0f;   ///< Threshold in dB
    float ratio = 4.0f;         ///< Compression ratio
    float attack = 0.01f;       ///< Attack time in seconds
    float release = 0.1f;       ///< Release time in seconds
    float makeupGain = 0.0f;    ///< Makeup gain in dB
};

// ============================================================================
// Audio Occlusion
// ============================================================================

/**
 * @brief Audio occlusion data for spatial audio
 */
struct AudioOcclusion {
    float directOcclusion = 0.0f;    ///< Direct path occlusion (0 = none, 1 = full)
    float reverbOcclusion = 0.0f;    ///< Reverb send occlusion
    float lowPassCutoff = 22000.0f;  ///< Low-pass filter frequency when occluded
    bool enabled = false;

    /**
     * @brief Calculate occlusion from ray cast results
     * @param hitCount Number of occluding surfaces hit
     * @param maxHits Maximum expected hits
     */
    void CalculateFromRayCast(int hitCount, int maxHits = 5) {
        if (hitCount <= 0) {
            directOcclusion = 0.0f;
            lowPassCutoff = 22000.0f;
        } else {
            directOcclusion = std::min(1.0f, static_cast<float>(hitCount) / maxHits);
            lowPassCutoff = 22000.0f - (directOcclusion * 18000.0f); // Range: 22000 -> 4000 Hz
        }
        reverbOcclusion = directOcclusion * 0.5f;
        enabled = hitCount > 0;
    }
};

// ============================================================================
// Audio Listener
// ============================================================================

/**
 * @brief 3D audio listener (usually attached to camera)
 */
struct AudioListener {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float gain = 1.0f;
};

// ============================================================================
// Audio Buffer
// ============================================================================

/**
 * @brief Raw audio data buffer
 */
class AudioBuffer {
public:
    AudioBuffer() = default;
    ~AudioBuffer();

    // Non-copyable
    AudioBuffer(const AudioBuffer&) = delete;
    AudioBuffer& operator=(const AudioBuffer&) = delete;

    // Movable
    AudioBuffer(AudioBuffer&& other) noexcept;
    AudioBuffer& operator=(AudioBuffer&& other) noexcept;

    /**
     * @brief Load audio data from file
     * @param path Path to audio file
     * @return true if loaded successfully
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief Load audio data from memory
     * @param data Audio data
     * @param size Size in bytes
     * @param format Audio format
     * @param channels Number of channels
     * @param sampleRate Sample rate in Hz
     */
    bool LoadFromMemory(const void* data, size_t size,
                        AudioFormat format,
                        AudioChannels channels,
                        int sampleRate);

    /**
     * @brief Release audio data
     */
    void Release();

    // Accessors
    [[nodiscard]] uint32_t GetHandle() const { return m_handle; }
    [[nodiscard]] AudioFormat GetFormat() const { return m_format; }
    [[nodiscard]] AudioChannels GetChannels() const { return m_channels; }
    [[nodiscard]] int GetSampleRate() const { return m_sampleRate; }
    [[nodiscard]] float GetDuration() const { return m_duration; }
    [[nodiscard]] size_t GetSize() const { return m_size; }
    [[nodiscard]] bool IsLoaded() const { return m_handle != 0; }
    [[nodiscard]] bool IsStreaming() const { return m_streaming; }

    const std::string& GetPath() const { return m_path; }

private:
    uint32_t m_handle = 0;
    std::string m_path;
    AudioFormat m_format = AudioFormat::Unknown;
    AudioChannels m_channels = AudioChannels::Mono;
    int m_sampleRate = AudioConstants::kDefaultSampleRate;
    float m_duration = 0.0f;
    size_t m_size = 0;
    bool m_streaming = false;
};

// ============================================================================
// Audio Source
// ============================================================================

/**
 * @brief Audio source for playing sounds
 *
 * Supports both 2D and 3D positional audio with various parameters
 * for controlling playback and spatialization.
 */
class AudioSource {
public:
    AudioSource();
    ~AudioSource();

    // Non-copyable
    AudioSource(const AudioSource&) = delete;
    AudioSource& operator=(const AudioSource&) = delete;

    // Movable
    AudioSource(AudioSource&& other) noexcept;
    AudioSource& operator=(AudioSource&& other) noexcept;

    /**
     * @brief Initialize the audio source
     */
    bool Initialize();

    /**
     * @brief Release resources
     */
    void Release();

    // =========== Playback Control ===========

    /**
     * @brief Play the attached buffer
     */
    void Play();

    /**
     * @brief Pause playback
     */
    void Pause();

    /**
     * @brief Stop playback and reset position
     */
    void Stop();

    /**
     * @brief Check if currently playing
     */
    [[nodiscard]] bool IsPlaying() const;

    /**
     * @brief Check if paused
     */
    [[nodiscard]] bool IsPaused() const;

    /**
     * @brief Check if stopped
     */
    [[nodiscard]] bool IsStopped() const;

    // =========== Buffer ===========

    /**
     * @brief Set the audio buffer to play
     */
    void SetBuffer(std::shared_ptr<AudioBuffer> buffer);

    /**
     * @brief Get the attached buffer
     */
    [[nodiscard]] std::shared_ptr<AudioBuffer> GetBuffer() const { return m_buffer; }

    // =========== Basic Properties ===========

    /**
     * @brief Set volume (0.0 to 1.0)
     */
    void SetVolume(float volume);
    [[nodiscard]] float GetVolume() const { return m_volume; }

    /**
     * @brief Set pitch (0.5 to 2.0 typically)
     */
    void SetPitch(float pitch);
    [[nodiscard]] float GetPitch() const { return m_pitch; }

    /**
     * @brief Set looping
     */
    void SetLooping(bool loop);
    [[nodiscard]] bool IsLooping() const { return m_looping; }

    /**
     * @brief Set playback position in seconds
     */
    void SetPlaybackPosition(float seconds);
    [[nodiscard]] float GetPlaybackPosition() const;

    // =========== 3D Properties ===========

    /**
     * @brief Enable/disable 3D spatialization
     */
    void Set3D(bool enable);
    [[nodiscard]] bool Is3D() const { return m_is3D; }

    /**
     * @brief Set world position
     */
    void SetPosition(const glm::vec3& position);
    [[nodiscard]] const glm::vec3& GetPosition() const { return m_position; }

    /**
     * @brief Set velocity for Doppler effect
     */
    void SetVelocity(const glm::vec3& velocity);
    [[nodiscard]] const glm::vec3& GetVelocity() const { return m_velocity; }

    /**
     * @brief Set direction (for directional sources)
     */
    void SetDirection(const glm::vec3& direction);
    [[nodiscard]] const glm::vec3& GetDirection() const { return m_direction; }

    // =========== Attenuation ===========

    /**
     * @brief Set distance attenuation model
     */
    void SetAttenuationModel(AttenuationModel model);
    [[nodiscard]] AttenuationModel GetAttenuationModel() const { return m_attenuationModel; }

    /**
     * @brief Set reference distance (distance at which volume is 100%)
     */
    void SetReferenceDistance(float distance);
    [[nodiscard]] float GetReferenceDistance() const { return m_referenceDistance; }

    /**
     * @brief Set maximum distance (beyond which sound is silent or at minimum)
     */
    void SetMaxDistance(float distance);
    [[nodiscard]] float GetMaxDistance() const { return m_maxDistance; }

    /**
     * @brief Set rolloff factor
     */
    void SetRolloffFactor(float factor);
    [[nodiscard]] float GetRolloffFactor() const { return m_rolloffFactor; }

    // =========== Occlusion ===========

    /**
     * @brief Set audio occlusion parameters
     */
    void SetOcclusion(const AudioOcclusion& occlusion);
    [[nodiscard]] const AudioOcclusion& GetOcclusion() const { return m_occlusion; }

    // =========== Bus Routing ===========

    /**
     * @brief Set output bus for mixing
     */
    void SetOutputBus(const std::string& busName);
    [[nodiscard]] const std::string& GetOutputBus() const { return m_outputBus; }

    // =========== Cone (Directional) ===========

    /**
     * @brief Set inner cone angle (full volume within this angle)
     */
    void SetInnerConeAngle(float degrees);
    [[nodiscard]] float GetInnerConeAngle() const { return m_innerConeAngle; }

    /**
     * @brief Set outer cone angle (attenuated outside inner, silent outside outer)
     */
    void SetOuterConeAngle(float degrees);
    [[nodiscard]] float GetOuterConeAngle() const { return m_outerConeAngle; }

    /**
     * @brief Set gain outside outer cone
     */
    void SetOuterConeGain(float gain);
    [[nodiscard]] float GetOuterConeGain() const { return m_outerConeGain; }

    // Internal
    [[nodiscard]] uint32_t GetHandle() const { return m_handle; }

private:
    void UpdateOpenALProperties();

    uint32_t m_handle = 0;
    std::shared_ptr<AudioBuffer> m_buffer;

    // Basic properties
    float m_volume = 1.0f;
    float m_pitch = 1.0f;
    bool m_looping = false;

    // 3D properties
    bool m_is3D = false;
    glm::vec3 m_position{0.0f};
    glm::vec3 m_velocity{0.0f};
    glm::vec3 m_direction{0.0f, 0.0f, -1.0f};

    // Attenuation
    AttenuationModel m_attenuationModel = AttenuationModel::InverseClamped;
    float m_referenceDistance = AudioConstants::kDefaultReferenceDistance;
    float m_maxDistance = AudioConstants::kDefaultMaxDistance;
    float m_rolloffFactor = AudioConstants::kDefaultRolloffFactor;

    // Cone
    float m_innerConeAngle = 360.0f;
    float m_outerConeAngle = 360.0f;
    float m_outerConeGain = 0.0f;

    // Occlusion
    AudioOcclusion m_occlusion;

    // Bus routing
    std::string m_outputBus = "master";
};

// ============================================================================
// Audio Bus (Effect Chain)
// ============================================================================

/**
 * @brief Audio bus for mixing and effects
 */
class AudioBus {
public:
    AudioBus(const std::string& name);
    ~AudioBus();

    /**
     * @brief Set bus volume
     */
    void SetVolume(float volume);
    [[nodiscard]] float GetVolume() const { return m_volume; }

    /**
     * @brief Mute/unmute the bus
     */
    void SetMuted(bool muted);
    [[nodiscard]] bool IsMuted() const { return m_muted; }

    /**
     * @brief Add a reverb effect
     */
    void AddReverb(const ReverbParams& params);

    /**
     * @brief Add a delay effect
     */
    void AddDelay(const DelayParams& params);

    /**
     * @brief Add an equalizer
     */
    void AddEQ(const EQParams& params);

    /**
     * @brief Add a compressor
     */
    void AddCompressor(const CompressorParams& params);

    /**
     * @brief Apply a reverb preset
     */
    void SetReverbPreset(ReverbPreset preset);

    /**
     * @brief Clear all effects
     */
    void ClearEffects();

    /**
     * @brief Set parent bus (for hierarchical mixing)
     */
    void SetParentBus(const std::string& parentName);

    [[nodiscard]] const std::string& GetName() const { return m_name; }

private:
    std::string m_name;
    std::string m_parentBus;
    float m_volume = 1.0f;
    bool m_muted = false;

    // Effect slots
    std::vector<std::pair<AudioEffectType, uint32_t>> m_effects;
    ReverbParams m_reverbParams;
    DelayParams m_delayParams;
    EQParams m_eqParams;
    CompressorParams m_compressorParams;
};

// ============================================================================
// Audio Streaming
// ============================================================================

/**
 * @brief Streaming audio source for large files (music, ambience)
 */
class AudioStream {
public:
    AudioStream();
    ~AudioStream();

    /**
     * @brief Open a file for streaming
     */
    bool Open(const std::string& path);

    /**
     * @brief Close the stream
     */
    void Close();

    /**
     * @brief Update stream buffers (call frequently)
     */
    void Update();

    /**
     * @brief Playback control
     */
    void Play();
    void Pause();
    void Stop();

    /**
     * @brief Set volume
     */
    void SetVolume(float volume);
    [[nodiscard]] float GetVolume() const { return m_volume; }

    /**
     * @brief Set looping
     */
    void SetLooping(bool loop);
    [[nodiscard]] bool IsLooping() const { return m_looping; }

    /**
     * @brief Check playback state
     */
    [[nodiscard]] bool IsPlaying() const;
    [[nodiscard]] bool IsPaused() const;

    /**
     * @brief Get playback progress (0.0 to 1.0)
     */
    [[nodiscard]] float GetProgress() const;

    /**
     * @brief Seek to position (0.0 to 1.0)
     */
    void Seek(float position);

private:
    static constexpr int kNumStreamBuffers = 4;
    static constexpr int kStreamBufferSize = 65536;

    uint32_t m_source = 0;
    uint32_t m_buffers[kNumStreamBuffers] = {0};

    std::string m_path;
    float m_volume = 1.0f;
    bool m_looping = false;

    // File handle for streaming
    void* m_fileHandle = nullptr;
    size_t m_totalSamples = 0;
    size_t m_currentSample = 0;
};

// ============================================================================
// Audio Engine
// ============================================================================

/**
 * @brief Main audio engine class
 *
 * Provides complete audio management including:
 * - 3D positional audio
 * - Audio mixing with effect buses
 * - Streaming for music
 * - Audio occlusion support
 * - Compressed format support (OGG, MP3, FLAC)
 *
 * Usage:
 * @code
 * auto& audio = AudioEngine::Instance();
 * audio.Initialize();
 *
 * // Load a sound
 * auto buffer = audio.LoadSound("explosion.ogg");
 *
 * // Play 3D sound
 * auto source = audio.Play3D(buffer, position);
 *
 * // Update listener
 * audio.SetListenerPosition(cameraPos, cameraForward, cameraUp);
 *
 * // Update each frame
 * audio.Update(deltaTime);
 * @endcode
 */
class AudioEngine {
public:
    /**
     * @brief Get singleton instance
     */
    static AudioEngine& Instance();

    // Non-copyable, non-movable
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) = delete;
    AudioEngine& operator=(AudioEngine&&) = delete;

    /**
     * @brief Initialize the audio engine
     * @return true if initialization succeeded
     */
    bool Initialize();

    /**
     * @brief Shutdown and release all resources
     */
    void Shutdown();

    /**
     * @brief Update audio system (call each frame)
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========== Sound Loading ===========

    /**
     * @brief Load a sound into memory
     * @param path Path to audio file
     * @return Shared pointer to audio buffer, nullptr on failure
     */
    std::shared_ptr<AudioBuffer> LoadSound(const std::string& path);

    /**
     * @brief Preload multiple sounds
     */
    void PreloadSounds(const std::vector<std::string>& paths);

    /**
     * @brief Unload a sound
     */
    void UnloadSound(const std::string& path);

    /**
     * @brief Clear all loaded sounds
     */
    void ClearSounds();

    // =========== Playback ===========

    /**
     * @brief Play a 2D sound (non-positional)
     * @param buffer Audio buffer to play
     * @param volume Volume (0.0 to 1.0)
     * @param pitch Pitch multiplier
     * @return Audio source handle
     */
    std::shared_ptr<AudioSource> Play2D(std::shared_ptr<AudioBuffer> buffer,
                                         float volume = 1.0f,
                                         float pitch = 1.0f);

    /**
     * @brief Play a 3D positional sound
     * @param buffer Audio buffer to play
     * @param position World position
     * @param volume Volume (0.0 to 1.0)
     * @return Audio source handle
     */
    std::shared_ptr<AudioSource> Play3D(std::shared_ptr<AudioBuffer> buffer,
                                         const glm::vec3& position,
                                         float volume = 1.0f);

    /**
     * @brief Play a sound by name (from loaded sounds)
     */
    std::shared_ptr<AudioSource> PlaySound(const std::string& name,
                                            float volume = 1.0f);

    /**
     * @brief Play a sound with full configuration
     */
    std::shared_ptr<AudioSource> PlayConfigured(std::shared_ptr<AudioBuffer> buffer,
                                                 const glm::vec3& position,
                                                 float volume,
                                                 float pitch,
                                                 bool loop,
                                                 const std::string& bus);

    /**
     * @brief Stop all sounds
     */
    void StopAll();

    /**
     * @brief Pause all sounds
     */
    void PauseAll();

    /**
     * @brief Resume all paused sounds
     */
    void ResumeAll();

    // =========== Streaming ===========

    /**
     * @brief Create a streaming audio source (for music)
     */
    std::shared_ptr<AudioStream> CreateStream(const std::string& path);

    /**
     * @brief Play background music (streaming)
     */
    void PlayMusic(const std::string& path, float volume = 1.0f, bool loop = true);

    /**
     * @brief Stop current music
     */
    void StopMusic();

    /**
     * @brief Set music volume
     */
    void SetMusicVolume(float volume);

    /**
     * @brief Crossfade to new music
     */
    void CrossfadeMusic(const std::string& path, float duration = 2.0f);

    // =========== Listener ===========

    /**
     * @brief Set listener (camera) position and orientation
     */
    void SetListenerTransform(const glm::vec3& position,
                              const glm::vec3& forward,
                              const glm::vec3& up);

    /**
     * @brief Set listener velocity (for Doppler effect)
     */
    void SetListenerVelocity(const glm::vec3& velocity);

    /**
     * @brief Set master listener gain
     */
    void SetListenerGain(float gain);

    /**
     * @brief Get current listener
     */
    [[nodiscard]] const AudioListener& GetListener() const { return m_listener; }

    // =========== Buses ===========

    /**
     * @brief Create an audio bus
     */
    AudioBus* CreateBus(const std::string& name);

    /**
     * @brief Get a bus by name
     */
    AudioBus* GetBus(const std::string& name);

    /**
     * @brief Set master volume
     */
    void SetMasterVolume(float volume);
    [[nodiscard]] float GetMasterVolume() const { return m_masterVolume; }

    /**
     * @brief Mute/unmute all audio
     */
    void SetMuted(bool muted);
    [[nodiscard]] bool IsMuted() const { return m_muted; }

    // =========== Global Settings ===========

    /**
     * @brief Set global Doppler factor
     */
    void SetDopplerFactor(float factor);
    [[nodiscard]] float GetDopplerFactor() const { return m_dopplerFactor; }

    /**
     * @brief Set speed of sound for Doppler calculations
     */
    void SetSpeedOfSound(float speed);
    [[nodiscard]] float GetSpeedOfSound() const { return m_speedOfSound; }

    /**
     * @brief Set global distance model
     */
    void SetDistanceModel(AttenuationModel model);

    // =========== Occlusion ===========

    /**
     * @brief Enable/disable automatic occlusion calculation
     */
    void SetOcclusionEnabled(bool enabled);
    [[nodiscard]] bool IsOcclusionEnabled() const { return m_occlusionEnabled; }

    /**
     * @brief Set occlusion ray cast callback
     * @param callback Function that returns number of hits between listener and source
     */
    using OcclusionCallback = std::function<int(const glm::vec3& from, const glm::vec3& to)>;
    void SetOcclusionCallback(OcclusionCallback callback);

    // =========== Statistics ===========

    /**
     * @brief Get number of active voices
     */
    [[nodiscard]] size_t GetActiveVoiceCount() const;

    /**
     * @brief Get number of loaded sounds
     */
    [[nodiscard]] size_t GetLoadedSoundCount() const { return m_buffers.size(); }

    /**
     * @brief Get total memory used by audio buffers
     */
    [[nodiscard]] size_t GetMemoryUsage() const;

private:
    AudioEngine();
    ~AudioEngine();

    void UpdateOcclusion();
    void CleanupFinishedSources();
    std::shared_ptr<AudioSource> AcquireSource();
    void ReturnSource(std::shared_ptr<AudioSource> source);

    // OpenAL context
    void* m_device = nullptr;
    void* m_context = nullptr;

    // Listener
    AudioListener m_listener;

    // Sound buffers
    std::unordered_map<std::string, std::shared_ptr<AudioBuffer>> m_buffers;

    // Active sources
    std::vector<std::shared_ptr<AudioSource>> m_activeSources;
    std::vector<std::shared_ptr<AudioSource>> m_sourcePool;
    std::mutex m_sourceMutex;

    // Buses
    std::unordered_map<std::string, std::unique_ptr<AudioBus>> m_buses;

    // Music streaming
    std::shared_ptr<AudioStream> m_currentMusic;
    std::shared_ptr<AudioStream> m_fadingMusic;
    float m_musicFadeTime = 0.0f;
    float m_musicFadeDuration = 0.0f;
    float m_musicVolume = 1.0f;

    // Global settings
    float m_masterVolume = 1.0f;
    float m_dopplerFactor = 1.0f;
    float m_speedOfSound = AudioConstants::kSpeedOfSound;
    bool m_muted = false;
    bool m_initialized = false;

    // Occlusion
    bool m_occlusionEnabled = false;
    OcclusionCallback m_occlusionCallback;
};

} // namespace Nova
