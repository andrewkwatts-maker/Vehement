#include "audio/AudioEngine.hpp"
#include "core/Logger.hpp"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>
#include <sndfile.h>
#include <vorbis/vorbisfile.h>

#include <fstream>
#include <cstring>
#include <algorithm>

namespace Nova {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

ALenum GetALFormat(AudioChannels channels, int bitsPerSample) {
    if (channels == AudioChannels::Mono) {
        return (bitsPerSample == 16) ? AL_FORMAT_MONO16 : AL_FORMAT_MONO8;
    } else if (channels == AudioChannels::Stereo) {
        return (bitsPerSample == 16) ? AL_FORMAT_STEREO16 : AL_FORMAT_STEREO8;
    }
    return AL_FORMAT_MONO16;
}

bool CheckALError(const char* operation) {
    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        const char* errorStr = "Unknown error";
        switch (error) {
            case AL_INVALID_NAME: errorStr = "Invalid name"; break;
            case AL_INVALID_ENUM: errorStr = "Invalid enum"; break;
            case AL_INVALID_VALUE: errorStr = "Invalid value"; break;
            case AL_INVALID_OPERATION: errorStr = "Invalid operation"; break;
            case AL_OUT_OF_MEMORY: errorStr = "Out of memory"; break;
        }
        LOG_ERROR("OpenAL error in {}: {}", operation, errorStr);
        return false;
    }
    return true;
}

} // anonymous namespace

// ============================================================================
// AudioBuffer Implementation
// ============================================================================

AudioBuffer::~AudioBuffer() {
    Release();
}

AudioBuffer::AudioBuffer(AudioBuffer&& other) noexcept
    : m_handle(other.m_handle)
    , m_path(std::move(other.m_path))
    , m_format(other.m_format)
    , m_channels(other.m_channels)
    , m_sampleRate(other.m_sampleRate)
    , m_duration(other.m_duration)
    , m_size(other.m_size)
    , m_streaming(other.m_streaming)
{
    other.m_handle = 0;
}

AudioBuffer& AudioBuffer::operator=(AudioBuffer&& other) noexcept {
    if (this != &other) {
        Release();
        m_handle = other.m_handle;
        m_path = std::move(other.m_path);
        m_format = other.m_format;
        m_channels = other.m_channels;
        m_sampleRate = other.m_sampleRate;
        m_duration = other.m_duration;
        m_size = other.m_size;
        m_streaming = other.m_streaming;
        other.m_handle = 0;
    }
    return *this;
}

bool AudioBuffer::LoadFromFile(const std::string& path) {
    SF_INFO sfInfo;
    std::memset(&sfInfo, 0, sizeof(sfInfo));

    SNDFILE* sndFile = sf_open(path.c_str(), SFM_READ, &sfInfo);
    if (!sndFile) {
        LOG_ERROR("Failed to open audio file: {}", path);
        return false;
    }

    // Determine format
    m_channels = (sfInfo.channels == 1) ? AudioChannels::Mono : AudioChannels::Stereo;
    m_sampleRate = sfInfo.samplerate;
    m_duration = static_cast<float>(sfInfo.frames) / static_cast<float>(sfInfo.samplerate);

    // Read data as 16-bit PCM
    const size_t numSamples = sfInfo.frames * sfInfo.channels;
    std::vector<int16_t> samples(numSamples);

    sf_count_t readCount = sf_read_short(sndFile, samples.data(), static_cast<sf_count_t>(numSamples));
    sf_close(sndFile);

    if (readCount <= 0) {
        LOG_ERROR("Failed to read audio data from: {}", path);
        return false;
    }

    // Create OpenAL buffer
    alGenBuffers(1, &m_handle);
    if (!CheckALError("alGenBuffers")) {
        return false;
    }

    ALenum format = GetALFormat(m_channels, 16);
    alBufferData(m_handle, format, samples.data(),
                 static_cast<ALsizei>(numSamples * sizeof(int16_t)),
                 m_sampleRate);

    if (!CheckALError("alBufferData")) {
        alDeleteBuffers(1, &m_handle);
        m_handle = 0;
        return false;
    }

    m_path = path;
    m_size = numSamples * sizeof(int16_t);

    // Determine format from extension
    if (path.find(".ogg") != std::string::npos || path.find(".OGG") != std::string::npos) {
        m_format = AudioFormat::OGG;
    } else if (path.find(".mp3") != std::string::npos || path.find(".MP3") != std::string::npos) {
        m_format = AudioFormat::MP3;
    } else if (path.find(".flac") != std::string::npos || path.find(".FLAC") != std::string::npos) {
        m_format = AudioFormat::FLAC;
    } else {
        m_format = AudioFormat::WAV;
    }

    LOG_INFO("Loaded audio: {} ({:.2f}s, {} Hz, {} channels)",
             path, m_duration, m_sampleRate,
             static_cast<int>(m_channels));

    return true;
}

bool AudioBuffer::LoadFromMemory(const void* data, size_t size,
                                  AudioFormat format,
                                  AudioChannels channels,
                                  int sampleRate) {
    if (!data || size == 0) {
        return false;
    }

    alGenBuffers(1, &m_handle);
    if (!CheckALError("alGenBuffers")) {
        return false;
    }

    ALenum alFormat = GetALFormat(channels, 16);
    alBufferData(m_handle, alFormat, data, static_cast<ALsizei>(size), sampleRate);

    if (!CheckALError("alBufferData")) {
        alDeleteBuffers(1, &m_handle);
        m_handle = 0;
        return false;
    }

    m_format = format;
    m_channels = channels;
    m_sampleRate = sampleRate;
    m_size = size;

    // Calculate duration based on format
    int bytesPerSample = 2 * static_cast<int>(channels);
    m_duration = static_cast<float>(size) / (sampleRate * bytesPerSample);

    return true;
}

void AudioBuffer::Release() {
    if (m_handle != 0) {
        alDeleteBuffers(1, &m_handle);
        m_handle = 0;
    }
    m_path.clear();
    m_duration = 0.0f;
    m_size = 0;
}

// ============================================================================
// AudioSource Implementation
// ============================================================================

AudioSource::AudioSource() = default;

AudioSource::~AudioSource() {
    Release();
}

AudioSource::AudioSource(AudioSource&& other) noexcept
    : m_handle(other.m_handle)
    , m_buffer(std::move(other.m_buffer))
    , m_volume(other.m_volume)
    , m_pitch(other.m_pitch)
    , m_looping(other.m_looping)
    , m_is3D(other.m_is3D)
    , m_position(other.m_position)
    , m_velocity(other.m_velocity)
    , m_direction(other.m_direction)
    , m_attenuationModel(other.m_attenuationModel)
    , m_referenceDistance(other.m_referenceDistance)
    , m_maxDistance(other.m_maxDistance)
    , m_rolloffFactor(other.m_rolloffFactor)
    , m_innerConeAngle(other.m_innerConeAngle)
    , m_outerConeAngle(other.m_outerConeAngle)
    , m_outerConeGain(other.m_outerConeGain)
    , m_occlusion(other.m_occlusion)
    , m_outputBus(std::move(other.m_outputBus))
{
    other.m_handle = 0;
}

AudioSource& AudioSource::operator=(AudioSource&& other) noexcept {
    if (this != &other) {
        Release();
        m_handle = other.m_handle;
        m_buffer = std::move(other.m_buffer);
        m_volume = other.m_volume;
        m_pitch = other.m_pitch;
        m_looping = other.m_looping;
        m_is3D = other.m_is3D;
        m_position = other.m_position;
        m_velocity = other.m_velocity;
        m_direction = other.m_direction;
        m_attenuationModel = other.m_attenuationModel;
        m_referenceDistance = other.m_referenceDistance;
        m_maxDistance = other.m_maxDistance;
        m_rolloffFactor = other.m_rolloffFactor;
        m_innerConeAngle = other.m_innerConeAngle;
        m_outerConeAngle = other.m_outerConeAngle;
        m_outerConeGain = other.m_outerConeGain;
        m_occlusion = other.m_occlusion;
        m_outputBus = std::move(other.m_outputBus);
        other.m_handle = 0;
    }
    return *this;
}

bool AudioSource::Initialize() {
    if (m_handle != 0) {
        return true;
    }

    alGenSources(1, &m_handle);
    if (!CheckALError("alGenSources")) {
        return false;
    }

    UpdateOpenALProperties();
    return true;
}

void AudioSource::Release() {
    if (m_handle != 0) {
        alSourceStop(m_handle);
        alDeleteSources(1, &m_handle);
        m_handle = 0;
    }
    m_buffer.reset();
}

void AudioSource::Play() {
    if (m_handle != 0) {
        alSourcePlay(m_handle);
    }
}

void AudioSource::Pause() {
    if (m_handle != 0) {
        alSourcePause(m_handle);
    }
}

void AudioSource::Stop() {
    if (m_handle != 0) {
        alSourceStop(m_handle);
    }
}

bool AudioSource::IsPlaying() const {
    if (m_handle == 0) return false;
    ALint state;
    alGetSourcei(m_handle, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

bool AudioSource::IsPaused() const {
    if (m_handle == 0) return false;
    ALint state;
    alGetSourcei(m_handle, AL_SOURCE_STATE, &state);
    return state == AL_PAUSED;
}

bool AudioSource::IsStopped() const {
    if (m_handle == 0) return true;
    ALint state;
    alGetSourcei(m_handle, AL_SOURCE_STATE, &state);
    return state == AL_STOPPED || state == AL_INITIAL;
}

void AudioSource::SetBuffer(std::shared_ptr<AudioBuffer> buffer) {
    m_buffer = std::move(buffer);
    if (m_handle != 0 && m_buffer) {
        alSourcei(m_handle, AL_BUFFER, static_cast<ALint>(m_buffer->GetHandle()));
    }
}

void AudioSource::SetVolume(float volume) {
    m_volume = std::clamp(volume, 0.0f, 1.0f);
    if (m_handle != 0) {
        float effectiveVolume = m_volume * (1.0f - m_occlusion.directOcclusion);
        alSourcef(m_handle, AL_GAIN, effectiveVolume);
    }
}

void AudioSource::SetPitch(float pitch) {
    m_pitch = std::clamp(pitch, 0.1f, 4.0f);
    if (m_handle != 0) {
        alSourcef(m_handle, AL_PITCH, m_pitch);
    }
}

void AudioSource::SetLooping(bool loop) {
    m_looping = loop;
    if (m_handle != 0) {
        alSourcei(m_handle, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    }
}

void AudioSource::SetPlaybackPosition(float seconds) {
    if (m_handle != 0) {
        alSourcef(m_handle, AL_SEC_OFFSET, seconds);
    }
}

float AudioSource::GetPlaybackPosition() const {
    if (m_handle == 0) return 0.0f;
    ALfloat seconds;
    alGetSourcef(m_handle, AL_SEC_OFFSET, &seconds);
    return seconds;
}

void AudioSource::Set3D(bool enable) {
    m_is3D = enable;
    if (m_handle != 0) {
        alSourcei(m_handle, AL_SOURCE_RELATIVE, enable ? AL_FALSE : AL_TRUE);
        if (!enable) {
            alSource3f(m_handle, AL_POSITION, 0.0f, 0.0f, 0.0f);
        } else {
            alSource3f(m_handle, AL_POSITION, m_position.x, m_position.y, m_position.z);
        }
    }
}

void AudioSource::SetPosition(const glm::vec3& position) {
    m_position = position;
    if (m_handle != 0 && m_is3D) {
        alSource3f(m_handle, AL_POSITION, position.x, position.y, position.z);
    }
}

void AudioSource::SetVelocity(const glm::vec3& velocity) {
    m_velocity = velocity;
    if (m_handle != 0) {
        alSource3f(m_handle, AL_VELOCITY, velocity.x, velocity.y, velocity.z);
    }
}

void AudioSource::SetDirection(const glm::vec3& direction) {
    m_direction = direction;
    if (m_handle != 0) {
        alSource3f(m_handle, AL_DIRECTION, direction.x, direction.y, direction.z);
    }
}

void AudioSource::SetAttenuationModel(AttenuationModel model) {
    m_attenuationModel = model;
    // OpenAL uses global distance model, individual sources can override with rolloff
}

void AudioSource::SetReferenceDistance(float distance) {
    m_referenceDistance = std::max(0.0f, distance);
    if (m_handle != 0) {
        alSourcef(m_handle, AL_REFERENCE_DISTANCE, m_referenceDistance);
    }
}

void AudioSource::SetMaxDistance(float distance) {
    m_maxDistance = std::max(0.0f, distance);
    if (m_handle != 0) {
        alSourcef(m_handle, AL_MAX_DISTANCE, m_maxDistance);
    }
}

void AudioSource::SetRolloffFactor(float factor) {
    m_rolloffFactor = std::max(0.0f, factor);
    if (m_handle != 0) {
        alSourcef(m_handle, AL_ROLLOFF_FACTOR, m_rolloffFactor);
    }
}

void AudioSource::SetOcclusion(const AudioOcclusion& occlusion) {
    m_occlusion = occlusion;
    // Apply occlusion through volume and filter
    SetVolume(m_volume);  // Recalculates with occlusion

    // Apply low-pass filter if EFX is available
    // This would require EFX extension setup
}

void AudioSource::SetOutputBus(const std::string& busName) {
    m_outputBus = busName;
}

void AudioSource::SetInnerConeAngle(float degrees) {
    m_innerConeAngle = std::clamp(degrees, 0.0f, 360.0f);
    if (m_handle != 0) {
        alSourcef(m_handle, AL_CONE_INNER_ANGLE, m_innerConeAngle);
    }
}

void AudioSource::SetOuterConeAngle(float degrees) {
    m_outerConeAngle = std::clamp(degrees, 0.0f, 360.0f);
    if (m_handle != 0) {
        alSourcef(m_handle, AL_CONE_OUTER_ANGLE, m_outerConeAngle);
    }
}

void AudioSource::SetOuterConeGain(float gain) {
    m_outerConeGain = std::clamp(gain, 0.0f, 1.0f);
    if (m_handle != 0) {
        alSourcef(m_handle, AL_CONE_OUTER_GAIN, m_outerConeGain);
    }
}

void AudioSource::UpdateOpenALProperties() {
    if (m_handle == 0) return;

    alSourcef(m_handle, AL_GAIN, m_volume);
    alSourcef(m_handle, AL_PITCH, m_pitch);
    alSourcei(m_handle, AL_LOOPING, m_looping ? AL_TRUE : AL_FALSE);

    if (m_is3D) {
        alSourcei(m_handle, AL_SOURCE_RELATIVE, AL_FALSE);
        alSource3f(m_handle, AL_POSITION, m_position.x, m_position.y, m_position.z);
        alSource3f(m_handle, AL_VELOCITY, m_velocity.x, m_velocity.y, m_velocity.z);
        alSource3f(m_handle, AL_DIRECTION, m_direction.x, m_direction.y, m_direction.z);
    } else {
        alSourcei(m_handle, AL_SOURCE_RELATIVE, AL_TRUE);
        alSource3f(m_handle, AL_POSITION, 0.0f, 0.0f, 0.0f);
    }

    alSourcef(m_handle, AL_REFERENCE_DISTANCE, m_referenceDistance);
    alSourcef(m_handle, AL_MAX_DISTANCE, m_maxDistance);
    alSourcef(m_handle, AL_ROLLOFF_FACTOR, m_rolloffFactor);

    alSourcef(m_handle, AL_CONE_INNER_ANGLE, m_innerConeAngle);
    alSourcef(m_handle, AL_CONE_OUTER_ANGLE, m_outerConeAngle);
    alSourcef(m_handle, AL_CONE_OUTER_GAIN, m_outerConeGain);
}

// ============================================================================
// AudioBus Implementation
// ============================================================================

AudioBus::AudioBus(const std::string& name) : m_name(name) {}

AudioBus::~AudioBus() {
    ClearEffects();
}

void AudioBus::SetVolume(float volume) {
    m_volume = std::clamp(volume, 0.0f, 1.0f);
}

void AudioBus::SetMuted(bool muted) {
    m_muted = muted;
}

void AudioBus::AddReverb(const ReverbParams& params) {
    m_reverbParams = params;
    m_effects.emplace_back(AudioEffectType::Reverb, 0);  // Handle would be EFX effect
}

void AudioBus::AddDelay(const DelayParams& params) {
    m_delayParams = params;
    m_effects.emplace_back(AudioEffectType::Delay, 0);
}

void AudioBus::AddEQ(const EQParams& params) {
    m_eqParams = params;
    m_effects.emplace_back(AudioEffectType::EQ, 0);
}

void AudioBus::AddCompressor(const CompressorParams& params) {
    m_compressorParams = params;
    m_effects.emplace_back(AudioEffectType::Compressor, 0);
}

void AudioBus::SetReverbPreset(ReverbPreset preset) {
    switch (preset) {
        case ReverbPreset::SmallRoom:
            m_reverbParams = {0.25f, 0.8f, 0.3f, 0.7f, 0.8f, 0.01f, 0.5f};
            break;
        case ReverbPreset::MediumRoom:
            m_reverbParams = {0.5f, 0.6f, 0.35f, 0.6f, 0.9f, 0.02f, 1.0f};
            break;
        case ReverbPreset::LargeRoom:
            m_reverbParams = {0.75f, 0.5f, 0.4f, 0.5f, 1.0f, 0.03f, 1.5f};
            break;
        case ReverbPreset::Hall:
            m_reverbParams = {0.8f, 0.4f, 0.45f, 0.4f, 1.0f, 0.04f, 2.5f};
            break;
        case ReverbPreset::Cathedral:
            m_reverbParams = {0.95f, 0.3f, 0.5f, 0.3f, 1.0f, 0.05f, 4.0f};
            break;
        case ReverbPreset::Cave:
            m_reverbParams = {0.9f, 0.2f, 0.6f, 0.2f, 0.7f, 0.1f, 3.0f};
            break;
        case ReverbPreset::Arena:
            m_reverbParams = {0.85f, 0.35f, 0.5f, 0.35f, 1.0f, 0.08f, 5.0f};
            break;
        case ReverbPreset::Forest:
            m_reverbParams = {0.3f, 0.9f, 0.2f, 0.8f, 1.0f, 0.02f, 0.8f};
            break;
        case ReverbPreset::Underwater:
            m_reverbParams = {0.7f, 0.1f, 0.7f, 0.2f, 0.5f, 0.15f, 2.0f};
            break;
        default:
            m_reverbParams = {};
            break;
    }
}

void AudioBus::ClearEffects() {
    m_effects.clear();
}

void AudioBus::SetParentBus(const std::string& parentName) {
    m_parentBus = parentName;
}

// ============================================================================
// AudioStream Implementation
// ============================================================================

AudioStream::AudioStream() = default;

AudioStream::~AudioStream() {
    Close();
}

bool AudioStream::Open(const std::string& path) {
    // Open file with libsndfile
    SF_INFO sfInfo;
    std::memset(&sfInfo, 0, sizeof(sfInfo));

    m_fileHandle = sf_open(path.c_str(), SFM_READ, &sfInfo);
    if (!m_fileHandle) {
        LOG_ERROR("Failed to open audio stream: {}", path);
        return false;
    }

    m_path = path;
    m_totalSamples = sfInfo.frames;
    m_currentSample = 0;

    // Create OpenAL source and buffers
    alGenSources(1, &m_source);
    alGenBuffers(kNumStreamBuffers, m_buffers);

    if (!CheckALError("Create stream buffers")) {
        Close();
        return false;
    }

    alSourcef(m_source, AL_GAIN, m_volume);

    // Pre-fill buffers
    for (int i = 0; i < kNumStreamBuffers; ++i) {
        // Read and queue buffer
        std::vector<int16_t> data(kStreamBufferSize);
        sf_count_t read = sf_read_short(static_cast<SNDFILE*>(m_fileHandle),
                                        data.data(), kStreamBufferSize);
        if (read > 0) {
            ALenum format = (sfInfo.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
            alBufferData(m_buffers[i], format, data.data(),
                        static_cast<ALsizei>(read * sizeof(int16_t)), sfInfo.samplerate);
            alSourceQueueBuffers(m_source, 1, &m_buffers[i]);
        }
    }

    LOG_INFO("Opened audio stream: {}", path);
    return true;
}

void AudioStream::Close() {
    if (m_source != 0) {
        alSourceStop(m_source);
        alSourcei(m_source, AL_BUFFER, 0);
        alDeleteSources(1, &m_source);
        m_source = 0;
    }

    if (m_buffers[0] != 0) {
        alDeleteBuffers(kNumStreamBuffers, m_buffers);
        std::memset(m_buffers, 0, sizeof(m_buffers));
    }

    if (m_fileHandle) {
        sf_close(static_cast<SNDFILE*>(m_fileHandle));
        m_fileHandle = nullptr;
    }

    m_path.clear();
}

void AudioStream::Update() {
    if (m_source == 0 || !m_fileHandle) return;

    ALint processed;
    alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);

    while (processed-- > 0) {
        ALuint buffer;
        alSourceUnqueueBuffers(m_source, 1, &buffer);

        // Read more data
        std::vector<int16_t> data(kStreamBufferSize);
        sf_count_t read = sf_read_short(static_cast<SNDFILE*>(m_fileHandle),
                                        data.data(), kStreamBufferSize);

        if (read > 0) {
            SF_INFO info;
            sf_command(static_cast<SNDFILE*>(m_fileHandle), SFC_GET_CURRENT_SF_INFO, &info, sizeof(info));
            ALenum format = (info.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
            alBufferData(buffer, format, data.data(),
                        static_cast<ALsizei>(read * sizeof(int16_t)), info.samplerate);
            alSourceQueueBuffers(m_source, 1, &buffer);
            m_currentSample += read / info.channels;
        } else if (m_looping) {
            // Loop back to start
            sf_seek(static_cast<SNDFILE*>(m_fileHandle), 0, SEEK_SET);
            m_currentSample = 0;
        }
    }

    // Restart if stopped but should be playing
    ALint state;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    if (state == AL_STOPPED) {
        ALint queued;
        alGetSourcei(m_source, AL_BUFFERS_QUEUED, &queued);
        if (queued > 0) {
            alSourcePlay(m_source);
        }
    }
}

void AudioStream::Play() {
    if (m_source != 0) {
        alSourcePlay(m_source);
    }
}

void AudioStream::Pause() {
    if (m_source != 0) {
        alSourcePause(m_source);
    }
}

void AudioStream::Stop() {
    if (m_source != 0) {
        alSourceStop(m_source);
    }
    if (m_fileHandle) {
        sf_seek(static_cast<SNDFILE*>(m_fileHandle), 0, SEEK_SET);
        m_currentSample = 0;
    }
}

void AudioStream::SetVolume(float volume) {
    m_volume = std::clamp(volume, 0.0f, 1.0f);
    if (m_source != 0) {
        alSourcef(m_source, AL_GAIN, m_volume);
    }
}

void AudioStream::SetLooping(bool loop) {
    m_looping = loop;
}

bool AudioStream::IsPlaying() const {
    if (m_source == 0) return false;
    ALint state;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

bool AudioStream::IsPaused() const {
    if (m_source == 0) return false;
    ALint state;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    return state == AL_PAUSED;
}

float AudioStream::GetProgress() const {
    if (m_totalSamples == 0) return 0.0f;
    return static_cast<float>(m_currentSample) / m_totalSamples;
}

void AudioStream::Seek(float position) {
    if (m_fileHandle) {
        sf_count_t frame = static_cast<sf_count_t>(position * m_totalSamples);
        sf_seek(static_cast<SNDFILE*>(m_fileHandle), frame, SEEK_SET);
        m_currentSample = frame;
    }
}

// ============================================================================
// AudioEngine Implementation
// ============================================================================

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    Shutdown();
}

AudioEngine& AudioEngine::Instance() {
    static AudioEngine instance;
    return instance;
}

bool AudioEngine::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Open default device
    m_device = alcOpenDevice(nullptr);
    if (!m_device) {
        LOG_ERROR("Failed to open audio device");
        return false;
    }

    // Create context
    m_context = alcCreateContext(static_cast<ALCdevice*>(m_device), nullptr);
    if (!m_context) {
        LOG_ERROR("Failed to create audio context");
        alcCloseDevice(static_cast<ALCdevice*>(m_device));
        m_device = nullptr;
        return false;
    }

    alcMakeContextCurrent(static_cast<ALCcontext*>(m_context));

    // Set default distance model
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

    // Set Doppler parameters
    alDopplerFactor(m_dopplerFactor);
    alSpeedOfSound(m_speedOfSound);

    // Create default buses
    CreateBus("master");
    CreateBus("sfx");
    CreateBus("music");
    CreateBus("voice");
    CreateBus("ambient");

    // Pre-allocate source pool
    m_sourcePool.reserve(AudioConstants::kMaxVoices);
    for (int i = 0; i < AudioConstants::kMaxVoices / 2; ++i) {
        auto source = std::make_shared<AudioSource>();
        if (source->Initialize()) {
            m_sourcePool.push_back(std::move(source));
        }
    }

    m_initialized = true;
    LOG_INFO("AudioEngine initialized");

    return true;
}

void AudioEngine::Shutdown() {
    if (!m_initialized) return;

    StopAll();

    m_currentMusic.reset();
    m_fadingMusic.reset();

    m_activeSources.clear();
    m_sourcePool.clear();
    m_buffers.clear();
    m_buses.clear();

    if (m_context) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(static_cast<ALCcontext*>(m_context));
        m_context = nullptr;
    }

    if (m_device) {
        alcCloseDevice(static_cast<ALCdevice*>(m_device));
        m_device = nullptr;
    }

    m_initialized = false;
    LOG_INFO("AudioEngine shutdown");
}

void AudioEngine::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update streaming music
    if (m_currentMusic) {
        m_currentMusic->Update();
    }

    // Handle music crossfade
    if (m_fadingMusic) {
        m_musicFadeTime += deltaTime;
        float t = std::min(1.0f, m_musicFadeTime / m_musicFadeDuration);

        // Fade out old music
        if (m_currentMusic) {
            m_currentMusic->SetVolume(m_musicVolume * (1.0f - t));
            m_currentMusic->Update();
        }

        // Fade in new music
        m_fadingMusic->SetVolume(m_musicVolume * t);
        m_fadingMusic->Update();

        if (t >= 1.0f) {
            if (m_currentMusic) {
                m_currentMusic->Stop();
            }
            m_currentMusic = std::move(m_fadingMusic);
            m_fadingMusic.reset();
            m_currentMusic->SetVolume(m_musicVolume);
        }
    }

    // Update occlusion
    if (m_occlusionEnabled && m_occlusionCallback) {
        UpdateOcclusion();
    }

    // Cleanup finished sources
    CleanupFinishedSources();
}

std::shared_ptr<AudioBuffer> AudioEngine::LoadSound(const std::string& path) {
    // Check if already loaded
    auto it = m_buffers.find(path);
    if (it != m_buffers.end()) {
        return it->second;
    }

    // Load new buffer
    auto buffer = std::make_shared<AudioBuffer>();
    if (!buffer->LoadFromFile(path)) {
        return nullptr;
    }

    m_buffers[path] = buffer;
    return buffer;
}

void AudioEngine::PreloadSounds(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        LoadSound(path);
    }
}

void AudioEngine::UnloadSound(const std::string& path) {
    m_buffers.erase(path);
}

void AudioEngine::ClearSounds() {
    m_buffers.clear();
}

std::shared_ptr<AudioSource> AudioEngine::Play2D(std::shared_ptr<AudioBuffer> buffer,
                                                   float volume, float pitch) {
    if (!buffer) return nullptr;

    auto source = AcquireSource();
    if (!source) return nullptr;

    source->Set3D(false);
    source->SetBuffer(buffer);
    source->SetVolume(volume * m_masterVolume);
    source->SetPitch(pitch);
    source->SetLooping(false);
    source->Play();

    return source;
}

std::shared_ptr<AudioSource> AudioEngine::Play3D(std::shared_ptr<AudioBuffer> buffer,
                                                   const glm::vec3& position,
                                                   float volume) {
    if (!buffer) return nullptr;

    auto source = AcquireSource();
    if (!source) return nullptr;

    source->Set3D(true);
    source->SetBuffer(buffer);
    source->SetPosition(position);
    source->SetVolume(volume * m_masterVolume);
    source->SetLooping(false);
    source->Play();

    return source;
}

std::shared_ptr<AudioSource> AudioEngine::PlaySound(const std::string& name, float volume) {
    auto it = m_buffers.find(name);
    if (it != m_buffers.end()) {
        return Play2D(it->second, volume);
    }
    return nullptr;
}

std::shared_ptr<AudioSource> AudioEngine::PlayConfigured(std::shared_ptr<AudioBuffer> buffer,
                                                          const glm::vec3& position,
                                                          float volume, float pitch,
                                                          bool loop, const std::string& bus) {
    if (!buffer) return nullptr;

    auto source = AcquireSource();
    if (!source) return nullptr;

    source->Set3D(true);
    source->SetBuffer(buffer);
    source->SetPosition(position);
    source->SetVolume(volume * m_masterVolume);
    source->SetPitch(pitch);
    source->SetLooping(loop);
    source->SetOutputBus(bus);
    source->Play();

    return source;
}

void AudioEngine::StopAll() {
    std::lock_guard lock(m_sourceMutex);
    for (auto& source : m_activeSources) {
        source->Stop();
    }
}

void AudioEngine::PauseAll() {
    std::lock_guard lock(m_sourceMutex);
    for (auto& source : m_activeSources) {
        if (source->IsPlaying()) {
            source->Pause();
        }
    }
}

void AudioEngine::ResumeAll() {
    std::lock_guard lock(m_sourceMutex);
    for (auto& source : m_activeSources) {
        if (source->IsPaused()) {
            source->Play();
        }
    }
}

std::shared_ptr<AudioStream> AudioEngine::CreateStream(const std::string& path) {
    auto stream = std::make_shared<AudioStream>();
    if (!stream->Open(path)) {
        return nullptr;
    }
    return stream;
}

void AudioEngine::PlayMusic(const std::string& path, float volume, bool loop) {
    if (m_currentMusic) {
        m_currentMusic->Stop();
    }

    m_currentMusic = CreateStream(path);
    if (m_currentMusic) {
        m_musicVolume = volume;
        m_currentMusic->SetVolume(volume);
        m_currentMusic->SetLooping(loop);
        m_currentMusic->Play();
    }
}

void AudioEngine::StopMusic() {
    if (m_currentMusic) {
        m_currentMusic->Stop();
        m_currentMusic.reset();
    }
}

void AudioEngine::SetMusicVolume(float volume) {
    m_musicVolume = std::clamp(volume, 0.0f, 1.0f);
    if (m_currentMusic) {
        m_currentMusic->SetVolume(m_musicVolume);
    }
}

void AudioEngine::CrossfadeMusic(const std::string& path, float duration) {
    m_fadingMusic = CreateStream(path);
    if (m_fadingMusic) {
        m_fadingMusic->SetVolume(0.0f);
        m_fadingMusic->SetLooping(true);
        m_fadingMusic->Play();
        m_musicFadeTime = 0.0f;
        m_musicFadeDuration = duration;
    }
}

void AudioEngine::SetListenerTransform(const glm::vec3& position,
                                        const glm::vec3& forward,
                                        const glm::vec3& up) {
    m_listener.position = position;
    m_listener.forward = forward;
    m_listener.up = up;

    alListener3f(AL_POSITION, position.x, position.y, position.z);

    ALfloat orientation[6] = {
        forward.x, forward.y, forward.z,
        up.x, up.y, up.z
    };
    alListenerfv(AL_ORIENTATION, orientation);
}

void AudioEngine::SetListenerVelocity(const glm::vec3& velocity) {
    m_listener.velocity = velocity;
    alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
}

void AudioEngine::SetListenerGain(float gain) {
    m_listener.gain = gain;
    alListenerf(AL_GAIN, gain);
}

AudioBus* AudioEngine::CreateBus(const std::string& name) {
    auto [it, inserted] = m_buses.try_emplace(name, std::make_unique<AudioBus>(name));
    return it->second.get();
}

AudioBus* AudioEngine::GetBus(const std::string& name) {
    auto it = m_buses.find(name);
    return (it != m_buses.end()) ? it->second.get() : nullptr;
}

void AudioEngine::SetMasterVolume(float volume) {
    m_masterVolume = std::clamp(volume, 0.0f, 1.0f);
    alListenerf(AL_GAIN, m_muted ? 0.0f : m_masterVolume);
}

void AudioEngine::SetMuted(bool muted) {
    m_muted = muted;
    alListenerf(AL_GAIN, muted ? 0.0f : m_masterVolume);
}

void AudioEngine::SetDopplerFactor(float factor) {
    m_dopplerFactor = std::max(0.0f, factor);
    alDopplerFactor(m_dopplerFactor);
}

void AudioEngine::SetSpeedOfSound(float speed) {
    m_speedOfSound = std::max(1.0f, speed);
    alSpeedOfSound(m_speedOfSound);
}

void AudioEngine::SetDistanceModel(AttenuationModel model) {
    ALenum alModel = AL_INVERSE_DISTANCE_CLAMPED;
    switch (model) {
        case AttenuationModel::None:
            alModel = AL_NONE;
            break;
        case AttenuationModel::Linear:
            alModel = AL_LINEAR_DISTANCE;
            break;
        case AttenuationModel::Inverse:
            alModel = AL_INVERSE_DISTANCE;
            break;
        case AttenuationModel::InverseClamped:
            alModel = AL_INVERSE_DISTANCE_CLAMPED;
            break;
        case AttenuationModel::Exponential:
            alModel = AL_EXPONENT_DISTANCE;
            break;
    }
    alDistanceModel(alModel);
}

void AudioEngine::SetOcclusionEnabled(bool enabled) {
    m_occlusionEnabled = enabled;
}

void AudioEngine::SetOcclusionCallback(OcclusionCallback callback) {
    m_occlusionCallback = std::move(callback);
}

size_t AudioEngine::GetActiveVoiceCount() const {
    return m_activeSources.size();
}

size_t AudioEngine::GetMemoryUsage() const {
    size_t total = 0;
    for (const auto& [name, buffer] : m_buffers) {
        total += buffer->GetSize();
    }
    return total;
}

void AudioEngine::UpdateOcclusion() {
    std::lock_guard lock(m_sourceMutex);

    for (auto& source : m_activeSources) {
        if (source->Is3D() && source->IsPlaying()) {
            int hits = m_occlusionCallback(m_listener.position, source->GetPosition());
            AudioOcclusion occlusion;
            occlusion.CalculateFromRayCast(hits);
            source->SetOcclusion(occlusion);
        }
    }
}

void AudioEngine::CleanupFinishedSources() {
    std::lock_guard lock(m_sourceMutex);

    auto it = m_activeSources.begin();
    while (it != m_activeSources.end()) {
        if ((*it)->IsStopped() && !(*it)->IsLooping()) {
            ReturnSource(*it);
            it = m_activeSources.erase(it);
        } else {
            ++it;
        }
    }
}

std::shared_ptr<AudioSource> AudioEngine::AcquireSource() {
    std::lock_guard lock(m_sourceMutex);

    std::shared_ptr<AudioSource> source;

    if (!m_sourcePool.empty()) {
        source = std::move(m_sourcePool.back());
        m_sourcePool.pop_back();
    } else if (m_activeSources.size() < AudioConstants::kMaxVoices) {
        source = std::make_shared<AudioSource>();
        if (!source->Initialize()) {
            return nullptr;
        }
    } else {
        // Steal oldest non-looping source
        for (auto& s : m_activeSources) {
            if (!s->IsLooping()) {
                s->Stop();
                source = s;
                break;
            }
        }
        if (!source) {
            return nullptr;
        }
    }

    m_activeSources.push_back(source);
    return source;
}

void AudioEngine::ReturnSource(std::shared_ptr<AudioSource> source) {
    if (source && m_sourcePool.size() < AudioConstants::kMaxVoices) {
        source->Stop();
        source->SetBuffer(nullptr);
        m_sourcePool.push_back(std::move(source));
    }
}

} // namespace Nova
