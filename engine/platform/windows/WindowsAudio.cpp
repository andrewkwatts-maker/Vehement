/**
 * @file WindowsAudio.cpp
 * @brief WASAPI audio implementation
 */

#include "WindowsAudio.hpp"

#ifdef NOVA_PLATFORM_WINDOWS

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <thread>
#include <mutex>

#pragma comment(lib, "ole32.lib")

namespace Nova {

// =============================================================================
// COM Helper
// =============================================================================

class COMInitializer {
public:
    COMInitializer() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        m_initialized = SUCCEEDED(hr) || hr == S_FALSE;
    }
    ~COMInitializer() {
        if (m_initialized) {
            CoUninitialize();
        }
    }
    bool IsInitialized() const { return m_initialized; }
private:
    bool m_initialized = false;
};

// =============================================================================
// WindowsAudioOutput Implementation
// =============================================================================

struct WindowsAudioOutput::Impl {
    IMMDevice* device = nullptr;
    IAudioClient* audioClient = nullptr;
    IAudioRenderClient* renderClient = nullptr;
    HANDLE bufferEvent = nullptr;
    std::thread audioThread;
    WAVEFORMATEX* mixFormat = nullptr;
    UINT32 bufferFrameCount = 0;
};

WindowsAudioOutput::WindowsAudioOutput()
    : m_impl(std::make_unique<Impl>()) {}

WindowsAudioOutput::~WindowsAudioOutput() {
    Stop();

    if (m_impl->renderClient) {
        m_impl->renderClient->Release();
    }
    if (m_impl->audioClient) {
        m_impl->audioClient->Release();
    }
    if (m_impl->device) {
        m_impl->device->Release();
    }
    if (m_impl->mixFormat) {
        CoTaskMemFree(m_impl->mixFormat);
    }
    if (m_impl->bufferEvent) {
        CloseHandle(m_impl->bufferEvent);
    }
}

bool WindowsAudioOutput::Initialize(const AudioStreamConfig& config) {
    COMInitializer com;
    if (!com.IsInitialized()) {
        return false;
    }

    // Get device enumerator
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                   CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                   reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) {
        return false;
    }

    // Get default output device
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_impl->device);
    enumerator->Release();
    if (FAILED(hr)) {
        return false;
    }

    // Activate audio client
    hr = m_impl->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                   nullptr, reinterpret_cast<void**>(&m_impl->audioClient));
    if (FAILED(hr)) {
        return false;
    }

    // Get mix format
    hr = m_impl->audioClient->GetMixFormat(&m_impl->mixFormat);
    if (FAILED(hr)) {
        return false;
    }

    // Update format info
    m_format.channels = m_impl->mixFormat->nChannels;
    m_format.sampleRate = m_impl->mixFormat->nSamplesPerSec;
    m_format.bitsPerSample = m_impl->mixFormat->wBitsPerSample;
    m_format.floatFormat = (m_impl->mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                            (m_impl->mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                             reinterpret_cast<WAVEFORMATEXTENSIBLE*>(m_impl->mixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT));

    // Calculate buffer duration (100ns units)
    REFERENCE_TIME bufferDuration = static_cast<REFERENCE_TIME>(
        (10000.0 * 1000 / m_format.sampleRate * config.bufferSizeFrames) + 0.5);

    // Create event for buffer notifications
    m_impl->bufferEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_impl->bufferEvent) {
        return false;
    }

    // Initialize audio client
    DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    if (config.eventDriven) {
        streamFlags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    }

    hr = m_impl->audioClient->Initialize(
        config.exclusiveMode ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED,
        streamFlags,
        bufferDuration,
        config.exclusiveMode ? bufferDuration : 0,
        m_impl->mixFormat,
        nullptr);

    if (FAILED(hr)) {
        return false;
    }

    // Set event handle
    hr = m_impl->audioClient->SetEventHandle(m_impl->bufferEvent);
    if (FAILED(hr)) {
        return false;
    }

    // Get buffer size
    hr = m_impl->audioClient->GetBufferSize(&m_impl->bufferFrameCount);
    if (FAILED(hr)) {
        return false;
    }
    m_bufferSize = static_cast<int>(m_impl->bufferFrameCount);

    // Get render client
    hr = m_impl->audioClient->GetService(__uuidof(IAudioRenderClient),
                                          reinterpret_cast<void**>(&m_impl->renderClient));
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

bool WindowsAudioOutput::Start() {
    if (m_playing || !m_impl->audioClient) {
        return false;
    }

    // Start the audio thread
    m_running = true;
    m_impl->audioThread = std::thread(&WindowsAudioOutput::AudioThread, this);

    // Start the audio client
    HRESULT hr = m_impl->audioClient->Start();
    if (FAILED(hr)) {
        m_running = false;
        if (m_impl->audioThread.joinable()) {
            m_impl->audioThread.join();
        }
        return false;
    }

    m_playing = true;
    return true;
}

void WindowsAudioOutput::Stop() {
    if (!m_playing) {
        return;
    }

    m_playing = false;
    m_running = false;

    // Signal the event to wake up the thread
    if (m_impl->bufferEvent) {
        SetEvent(m_impl->bufferEvent);
    }

    if (m_impl->audioThread.joinable()) {
        m_impl->audioThread.join();
    }

    if (m_impl->audioClient) {
        m_impl->audioClient->Stop();
        m_impl->audioClient->Reset();
    }
}

void WindowsAudioOutput::AudioThread() {
    COMInitializer com;

    while (m_running) {
        // Wait for buffer event
        DWORD result = WaitForSingleObject(m_impl->bufferEvent, 2000);
        if (result != WAIT_OBJECT_0 || !m_running) {
            break;
        }

        // Get buffer padding (how much is already queued)
        UINT32 paddingFrames = 0;
        m_impl->audioClient->GetCurrentPadding(&paddingFrames);

        // Calculate available frames
        UINT32 availableFrames = m_impl->bufferFrameCount - paddingFrames;
        if (availableFrames == 0) {
            continue;
        }

        // Get buffer
        BYTE* buffer = nullptr;
        HRESULT hr = m_impl->renderClient->GetBuffer(availableFrames, &buffer);
        if (FAILED(hr)) {
            continue;
        }

        // Fill buffer via callback
        if (m_callback && buffer) {
            m_callback(reinterpret_cast<float*>(buffer), static_cast<int>(availableFrames));

            // Apply volume
            if (m_volume < 1.0f) {
                float* samples = reinterpret_cast<float*>(buffer);
                int totalSamples = availableFrames * m_format.channels;
                for (int i = 0; i < totalSamples; ++i) {
                    samples[i] *= m_volume;
                }
            }
        } else {
            // Fill with silence
            memset(buffer, 0, availableFrames * m_format.channels * sizeof(float));
        }

        // Release buffer
        m_impl->renderClient->ReleaseBuffer(availableFrames, 0);
    }
}

float WindowsAudioOutput::GetLatency() const {
    if (!m_impl->audioClient) {
        return 0.0f;
    }

    REFERENCE_TIME latency = 0;
    m_impl->audioClient->GetStreamLatency(&latency);
    return static_cast<float>(latency) / 10000.0f;  // Convert to ms
}

void WindowsAudioOutput::SetVolume(float volume) {
    m_volume = std::max(0.0f, std::min(1.0f, volume));
}

// =============================================================================
// WindowsAudioInput Implementation
// =============================================================================

struct WindowsAudioInput::Impl {
    IMMDevice* device = nullptr;
    IAudioClient* audioClient = nullptr;
    IAudioCaptureClient* captureClient = nullptr;
    HANDLE bufferEvent = nullptr;
    std::thread audioThread;
    WAVEFORMATEX* mixFormat = nullptr;
    UINT32 bufferFrameCount = 0;
};

WindowsAudioInput::WindowsAudioInput()
    : m_impl(std::make_unique<Impl>()) {}

WindowsAudioInput::~WindowsAudioInput() {
    Stop();

    if (m_impl->captureClient) {
        m_impl->captureClient->Release();
    }
    if (m_impl->audioClient) {
        m_impl->audioClient->Release();
    }
    if (m_impl->device) {
        m_impl->device->Release();
    }
    if (m_impl->mixFormat) {
        CoTaskMemFree(m_impl->mixFormat);
    }
    if (m_impl->bufferEvent) {
        CloseHandle(m_impl->bufferEvent);
    }
}

bool WindowsAudioInput::Initialize(const AudioStreamConfig& config) {
    COMInitializer com;
    if (!com.IsInitialized()) {
        return false;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                   CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                   reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) {
        return false;
    }

    hr = enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &m_impl->device);
    enumerator->Release();
    if (FAILED(hr)) {
        return false;
    }

    hr = m_impl->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                   nullptr, reinterpret_cast<void**>(&m_impl->audioClient));
    if (FAILED(hr)) {
        return false;
    }

    hr = m_impl->audioClient->GetMixFormat(&m_impl->mixFormat);
    if (FAILED(hr)) {
        return false;
    }

    m_format.channels = m_impl->mixFormat->nChannels;
    m_format.sampleRate = m_impl->mixFormat->nSamplesPerSec;
    m_format.bitsPerSample = m_impl->mixFormat->wBitsPerSample;

    REFERENCE_TIME bufferDuration = static_cast<REFERENCE_TIME>(
        (10000.0 * 1000 / m_format.sampleRate * config.bufferSizeFrames) + 0.5);

    m_impl->bufferEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_impl->bufferEvent) {
        return false;
    }

    hr = m_impl->audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        bufferDuration,
        0,
        m_impl->mixFormat,
        nullptr);

    if (FAILED(hr)) {
        return false;
    }

    hr = m_impl->audioClient->SetEventHandle(m_impl->bufferEvent);
    if (FAILED(hr)) {
        return false;
    }

    hr = m_impl->audioClient->GetBufferSize(&m_impl->bufferFrameCount);
    if (FAILED(hr)) {
        return false;
    }
    m_bufferSize = static_cast<int>(m_impl->bufferFrameCount);

    hr = m_impl->audioClient->GetService(__uuidof(IAudioCaptureClient),
                                          reinterpret_cast<void**>(&m_impl->captureClient));
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

bool WindowsAudioInput::Start() {
    if (m_recording || !m_impl->audioClient) {
        return false;
    }

    m_running = true;
    m_impl->audioThread = std::thread(&WindowsAudioInput::AudioThread, this);

    HRESULT hr = m_impl->audioClient->Start();
    if (FAILED(hr)) {
        m_running = false;
        if (m_impl->audioThread.joinable()) {
            m_impl->audioThread.join();
        }
        return false;
    }

    m_recording = true;
    return true;
}

void WindowsAudioInput::Stop() {
    if (!m_recording) {
        return;
    }

    m_recording = false;
    m_running = false;

    if (m_impl->bufferEvent) {
        SetEvent(m_impl->bufferEvent);
    }

    if (m_impl->audioThread.joinable()) {
        m_impl->audioThread.join();
    }

    if (m_impl->audioClient) {
        m_impl->audioClient->Stop();
        m_impl->audioClient->Reset();
    }
}

void WindowsAudioInput::AudioThread() {
    COMInitializer com;

    while (m_running) {
        DWORD result = WaitForSingleObject(m_impl->bufferEvent, 2000);
        if (result != WAIT_OBJECT_0 || !m_running) {
            break;
        }

        UINT32 packetLength = 0;
        while (SUCCEEDED(m_impl->captureClient->GetNextPacketSize(&packetLength)) && packetLength > 0) {
            BYTE* buffer = nullptr;
            UINT32 framesAvailable = 0;
            DWORD flags = 0;

            HRESULT hr = m_impl->captureClient->GetBuffer(&buffer, &framesAvailable, &flags, nullptr, nullptr);
            if (FAILED(hr)) {
                break;
            }

            if (m_callback && buffer && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                m_callback(reinterpret_cast<float*>(buffer), static_cast<int>(framesAvailable));
            }

            m_impl->captureClient->ReleaseBuffer(framesAvailable);
        }
    }
}

// =============================================================================
// WindowsAudioDevices
// =============================================================================

static std::string WideToUtf8(const wchar_t* wstr) {
    if (!wstr) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], size, nullptr, nullptr);
    return result;
}

std::vector<AudioDeviceInfo> WindowsAudioDevices::GetOutputDevices() {
    std::vector<AudioDeviceInfo> devices;
    COMInitializer com;
    if (!com.IsInitialized()) return devices;

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                   CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                   reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) return devices;

    IMMDeviceCollection* collection = nullptr;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
    if (SUCCEEDED(hr)) {
        UINT count = 0;
        collection->GetCount(&count);

        for (UINT i = 0; i < count; ++i) {
            IMMDevice* device = nullptr;
            if (SUCCEEDED(collection->Item(i, &device))) {
                AudioDeviceInfo info;
                info.isInput = false;

                LPWSTR id = nullptr;
                if (SUCCEEDED(device->GetId(&id))) {
                    info.id = WideToUtf8(id);
                    CoTaskMemFree(id);
                }

                IPropertyStore* props = nullptr;
                if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
                    PROPVARIANT name;
                    PropVariantInit(&name);
                    if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &name))) {
                        info.name = WideToUtf8(name.pwszVal);
                        PropVariantClear(&name);
                    }
                    props->Release();
                }

                devices.push_back(info);
                device->Release();
            }
        }
        collection->Release();
    }
    enumerator->Release();

    return devices;
}

std::vector<AudioDeviceInfo> WindowsAudioDevices::GetInputDevices() {
    std::vector<AudioDeviceInfo> devices;
    COMInitializer com;
    if (!com.IsInitialized()) return devices;

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                   CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                   reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) return devices;

    IMMDeviceCollection* collection = nullptr;
    hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
    if (SUCCEEDED(hr)) {
        UINT count = 0;
        collection->GetCount(&count);

        for (UINT i = 0; i < count; ++i) {
            IMMDevice* device = nullptr;
            if (SUCCEEDED(collection->Item(i, &device))) {
                AudioDeviceInfo info;
                info.isInput = true;

                LPWSTR id = nullptr;
                if (SUCCEEDED(device->GetId(&id))) {
                    info.id = WideToUtf8(id);
                    CoTaskMemFree(id);
                }

                IPropertyStore* props = nullptr;
                if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
                    PROPVARIANT name;
                    PropVariantInit(&name);
                    if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &name))) {
                        info.name = WideToUtf8(name.pwszVal);
                        PropVariantClear(&name);
                    }
                    props->Release();
                }

                devices.push_back(info);
                device->Release();
            }
        }
        collection->Release();
    }
    enumerator->Release();

    return devices;
}

AudioDeviceInfo WindowsAudioDevices::GetDefaultOutputDevice() {
    auto devices = GetOutputDevices();
    if (!devices.empty()) {
        devices[0].isDefault = true;
        return devices[0];
    }
    return {};
}

AudioDeviceInfo WindowsAudioDevices::GetDefaultInputDevice() {
    auto devices = GetInputDevices();
    if (!devices.empty()) {
        devices[0].isDefault = true;
        return devices[0];
    }
    return {};
}

bool WindowsAudioDevices::IsSpatialAudioAvailable() {
    // Check for Windows Sonic / Dolby Atmos support
    // This would require checking IAudioClient2::IsOffloadCapable or similar
    return IsWindows10OrGreater();
}

} // namespace Nova

#endif // NOVA_PLATFORM_WINDOWS
