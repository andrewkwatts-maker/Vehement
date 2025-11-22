/**
 * @file MockServices.hpp
 * @brief Mock implementations for testing engine services
 */

#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <glm/glm.hpp>
#include <filesystem>
#include <optional>

namespace Nova {
namespace Test {

// =============================================================================
// MockFileSystem
// =============================================================================

/**
 * @brief Mock file system for testing file I/O without disk access
 */
class MockFileSystem {
public:
    // File operations
    MOCK_METHOD(bool, Exists, (const std::string& path), (const));
    MOCK_METHOD(bool, IsDirectory, (const std::string& path), (const));
    MOCK_METHOD(bool, IsFile, (const std::string& path), (const));
    MOCK_METHOD(std::string, ReadFile, (const std::string& path), (const));
    MOCK_METHOD(bool, WriteFile, (const std::string& path, const std::string& content), ());
    MOCK_METHOD(bool, DeleteFile, (const std::string& path), ());
    MOCK_METHOD(bool, CreateDirectory, (const std::string& path), ());
    MOCK_METHOD(std::vector<std::string>, ListDirectory, (const std::string& path), (const));
    MOCK_METHOD(size_t, GetFileSize, (const std::string& path), (const));
    MOCK_METHOD(uint64_t, GetModificationTime, (const std::string& path), (const));

    // Helper to add virtual files for testing
    void AddVirtualFile(const std::string& path, const std::string& content) {
        m_virtualFiles[path] = content;
        ON_CALL(*this, Exists(path)).WillByDefault(::testing::Return(true));
        ON_CALL(*this, IsFile(path)).WillByDefault(::testing::Return(true));
        ON_CALL(*this, ReadFile(path)).WillByDefault(::testing::Return(content));
        ON_CALL(*this, GetFileSize(path)).WillByDefault(::testing::Return(content.size()));
    }

    void AddVirtualDirectory(const std::string& path, const std::vector<std::string>& files) {
        ON_CALL(*this, Exists(path)).WillByDefault(::testing::Return(true));
        ON_CALL(*this, IsDirectory(path)).WillByDefault(::testing::Return(true));
        ON_CALL(*this, ListDirectory(path)).WillByDefault(::testing::Return(files));
    }

    void Reset() {
        m_virtualFiles.clear();
    }

private:
    std::unordered_map<std::string, std::string> m_virtualFiles;
};

// =============================================================================
// MockNetwork
// =============================================================================

/**
 * @brief Mock network service for testing multiplayer/Firebase without real connections
 */
class MockNetwork {
public:
    // Connection management
    MOCK_METHOD(bool, Connect, (const std::string& host, int port), ());
    MOCK_METHOD(void, Disconnect, (), ());
    MOCK_METHOD(bool, IsConnected, (), (const));

    // Data transfer
    MOCK_METHOD(bool, Send, (const std::vector<uint8_t>& data), ());
    MOCK_METHOD(std::vector<uint8_t>, Receive, (), ());
    MOCK_METHOD(void, SetReceiveCallback, (std::function<void(const std::vector<uint8_t>&)>), ());

    // HTTP-like operations (for Firebase REST)
    MOCK_METHOD(std::string, Get, (const std::string& url), ());
    MOCK_METHOD(std::string, Post, (const std::string& url, const std::string& body), ());
    MOCK_METHOD(std::string, Put, (const std::string& url, const std::string& body), ());
    MOCK_METHOD(bool, Delete, (const std::string& url), ());

    // Latency simulation
    void SetSimulatedLatency(float ms) { m_simulatedLatency = ms; }
    void SetPacketLoss(float rate) { m_packetLossRate = rate; }

    // Queue a message to be received
    void QueueReceivedData(const std::vector<uint8_t>& data) {
        m_receivedDataQueue.push_back(data);
    }

    void Reset() {
        m_receivedDataQueue.clear();
        m_simulatedLatency = 0.0f;
        m_packetLossRate = 0.0f;
    }

private:
    std::vector<std::vector<uint8_t>> m_receivedDataQueue;
    float m_simulatedLatency = 0.0f;
    float m_packetLossRate = 0.0f;
};

// =============================================================================
// MockAudio
// =============================================================================

/**
 * @brief Mock audio service for testing sound playback without actual audio output
 */
class MockAudio {
public:
    // Initialization
    MOCK_METHOD(bool, Initialize, (), ());
    MOCK_METHOD(void, Shutdown, (), ());

    // Sound playback
    MOCK_METHOD(uint32_t, PlaySound, (const std::string& name, float volume, bool loop), ());
    MOCK_METHOD(void, StopSound, (uint32_t handle), ());
    MOCK_METHOD(void, SetVolume, (uint32_t handle, float volume), ());
    MOCK_METHOD(void, SetPitch, (uint32_t handle, float pitch), ());
    MOCK_METHOD(bool, IsPlaying, (uint32_t handle), (const));

    // 3D audio
    MOCK_METHOD(void, SetListenerPosition, (const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up), ());
    MOCK_METHOD(void, SetSourcePosition, (uint32_t handle, const glm::vec3& position), ());

    // Music
    MOCK_METHOD(void, PlayMusic, (const std::string& name, float fadeTime), ());
    MOCK_METHOD(void, StopMusic, (float fadeTime), ());
    MOCK_METHOD(void, SetMusicVolume, (float volume), ());

    // Helpers for tracking played sounds in tests
    struct PlayedSound {
        std::string name;
        float volume;
        bool looping;
        uint32_t handle;
    };

    const std::vector<PlayedSound>& GetPlayedSounds() const { return m_playedSounds; }

    void TrackSound(const std::string& name, float volume, bool loop, uint32_t handle) {
        m_playedSounds.push_back({name, volume, loop, handle});
    }

    void Reset() {
        m_playedSounds.clear();
    }

private:
    std::vector<PlayedSound> m_playedSounds;
};

// =============================================================================
// MockRenderer
// =============================================================================

/**
 * @brief Mock renderer for testing graphics operations without GPU
 */
class MockRenderer {
public:
    // Initialization
    MOCK_METHOD(bool, Initialize, (int width, int height), ());
    MOCK_METHOD(void, Shutdown, (), ());
    MOCK_METHOD(void, Resize, (int width, int height), ());

    // Frame operations
    MOCK_METHOD(void, BeginFrame, (), ());
    MOCK_METHOD(void, EndFrame, (), ());
    MOCK_METHOD(void, Clear, (float r, float g, float b, float a), ());

    // Rendering
    MOCK_METHOD(void, DrawMesh, (uint32_t meshId, const glm::mat4& transform), ());
    MOCK_METHOD(void, DrawSprite, (uint32_t textureId, const glm::vec2& position, const glm::vec2& size), ());
    MOCK_METHOD(void, DrawText, (const std::string& text, const glm::vec2& position, float size), ());
    MOCK_METHOD(void, DrawLine, (const glm::vec3& start, const glm::vec3& end, const glm::vec4& color), ());

    // Resource management
    MOCK_METHOD(uint32_t, LoadTexture, (const std::string& path), ());
    MOCK_METHOD(uint32_t, LoadMesh, (const std::string& path), ());
    MOCK_METHOD(void, UnloadTexture, (uint32_t id), ());
    MOCK_METHOD(void, UnloadMesh, (uint32_t id), ());

    // State
    MOCK_METHOD(void, SetViewMatrix, (const glm::mat4& view), ());
    MOCK_METHOD(void, SetProjectionMatrix, (const glm::mat4& projection), ());

    // Draw call tracking for tests
    struct DrawCall {
        enum Type { Mesh, Sprite, Text, Line } type;
        glm::mat4 transform;
        uint32_t resourceId;
    };

    const std::vector<DrawCall>& GetDrawCalls() const { return m_drawCalls; }
    size_t GetDrawCallCount() const { return m_drawCalls.size(); }

    void TrackDrawCall(DrawCall::Type type, uint32_t resourceId, const glm::mat4& transform = glm::mat4(1.0f)) {
        m_drawCalls.push_back({type, transform, resourceId});
    }

    void Reset() {
        m_drawCalls.clear();
    }

private:
    std::vector<DrawCall> m_drawCalls;
};

// =============================================================================
// MockInput
// =============================================================================

/**
 * @brief Mock input service for testing input handling
 */
class MockInput {
public:
    // Keyboard
    MOCK_METHOD(bool, IsKeyDown, (int key), (const));
    MOCK_METHOD(bool, IsKeyPressed, (int key), (const));
    MOCK_METHOD(bool, IsKeyReleased, (int key), (const));

    // Mouse
    MOCK_METHOD(bool, IsMouseButtonDown, (int button), (const));
    MOCK_METHOD(bool, IsMouseButtonPressed, (int button), (const));
    MOCK_METHOD(bool, IsMouseButtonReleased, (int button), (const));
    MOCK_METHOD(glm::vec2, GetMousePosition, (), (const));
    MOCK_METHOD(glm::vec2, GetMouseDelta, (), (const));
    MOCK_METHOD(float, GetScrollDelta, (), (const));

    // Gamepad
    MOCK_METHOD(bool, IsGamepadConnected, (int index), (const));
    MOCK_METHOD(float, GetGamepadAxis, (int index, int axis), (const));
    MOCK_METHOD(bool, IsGamepadButtonDown, (int index, int button), (const));

    // Touch (mobile)
    MOCK_METHOD(int, GetTouchCount, (), (const));
    MOCK_METHOD(glm::vec2, GetTouchPosition, (int index), (const));

    // Simulate input for tests
    void SimulateKeyPress(int key) {
        m_pressedKeys.insert(key);
        m_downKeys.insert(key);
    }

    void SimulateKeyRelease(int key) {
        m_releasedKeys.insert(key);
        m_downKeys.erase(key);
    }

    void SimulateMouseMove(const glm::vec2& position) {
        m_mouseDelta = position - m_mousePosition;
        m_mousePosition = position;
    }

    void SimulateMouseButton(int button, bool down) {
        if (down) {
            m_pressedMouseButtons.insert(button);
            m_downMouseButtons.insert(button);
        } else {
            m_releasedMouseButtons.insert(button);
            m_downMouseButtons.erase(button);
        }
    }

    void Update() {
        m_pressedKeys.clear();
        m_releasedKeys.clear();
        m_pressedMouseButtons.clear();
        m_releasedMouseButtons.clear();
        m_mouseDelta = glm::vec2(0.0f);
    }

    void Reset() {
        m_downKeys.clear();
        m_pressedKeys.clear();
        m_releasedKeys.clear();
        m_mousePosition = glm::vec2(0.0f);
        m_mouseDelta = glm::vec2(0.0f);
        m_downMouseButtons.clear();
        m_pressedMouseButtons.clear();
        m_releasedMouseButtons.clear();
    }

private:
    std::set<int> m_downKeys;
    std::set<int> m_pressedKeys;
    std::set<int> m_releasedKeys;
    glm::vec2 m_mousePosition{0.0f};
    glm::vec2 m_mouseDelta{0.0f};
    std::set<int> m_downMouseButtons;
    std::set<int> m_pressedMouseButtons;
    std::set<int> m_releasedMouseButtons;
};

// =============================================================================
// MockServiceRegistry
// =============================================================================

/**
 * @brief Central registry for all mock services
 */
class MockServiceRegistry {
public:
    static MockServiceRegistry& Instance() {
        static MockServiceRegistry instance;
        return instance;
    }

    void Initialize() {
        m_fileSystem = std::make_unique<::testing::NiceMock<MockFileSystem>>();
        m_network = std::make_unique<::testing::NiceMock<MockNetwork>>();
        m_audio = std::make_unique<::testing::NiceMock<MockAudio>>();
        m_renderer = std::make_unique<::testing::NiceMock<MockRenderer>>();
        m_input = std::make_unique<::testing::NiceMock<MockInput>>();
    }

    void Shutdown() {
        m_fileSystem.reset();
        m_network.reset();
        m_audio.reset();
        m_renderer.reset();
        m_input.reset();
    }

    void Reset() {
        if (m_fileSystem) m_fileSystem->Reset();
        if (m_network) m_network->Reset();
        if (m_audio) m_audio->Reset();
        if (m_renderer) m_renderer->Reset();
        if (m_input) m_input->Reset();
    }

    MockFileSystem& FileSystem() { return *m_fileSystem; }
    MockNetwork& Network() { return *m_network; }
    MockAudio& Audio() { return *m_audio; }
    MockRenderer& Renderer() { return *m_renderer; }
    MockInput& Input() { return *m_input; }

private:
    MockServiceRegistry() = default;

    std::unique_ptr<::testing::NiceMock<MockFileSystem>> m_fileSystem;
    std::unique_ptr<::testing::NiceMock<MockNetwork>> m_network;
    std::unique_ptr<::testing::NiceMock<MockAudio>> m_audio;
    std::unique_ptr<::testing::NiceMock<MockRenderer>> m_renderer;
    std::unique_ptr<::testing::NiceMock<MockInput>> m_input;
};

// =============================================================================
// Convenience Macros
// =============================================================================

#define MOCK_FS() Nova::Test::MockServiceRegistry::Instance().FileSystem()
#define MOCK_NET() Nova::Test::MockServiceRegistry::Instance().Network()
#define MOCK_AUDIO() Nova::Test::MockServiceRegistry::Instance().Audio()
#define MOCK_RENDERER() Nova::Test::MockServiceRegistry::Instance().Renderer()
#define MOCK_INPUT() Nova::Test::MockServiceRegistry::Instance().Input()

} // namespace Test
} // namespace Nova
