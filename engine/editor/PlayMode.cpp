/**
 * @file PlayMode.cpp
 * @brief Play Mode / Runtime Preview system implementation
 */

#include "PlayMode.hpp"
#include "../scene/Scene.hpp"
#include "../scene/SceneNode.hpp"
#include "../scene/Camera.hpp"
#include "../physics/PhysicsWorld.hpp"
#include "../scripting/ScriptContext.hpp"
#include "../audio/AudioEngine.hpp"
#include "../core/Window.hpp"
#include "../core/Logger.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Nova {

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// PlayState String Conversion
// ============================================================================

const char* PlayStateToString(PlayState state) {
    switch (state) {
        case PlayState::Stopped:  return "Stopped";
        case PlayState::Playing:  return "Playing";
        case PlayState::Paused:   return "Paused";
        case PlayState::Stepping: return "Stepping";
        default:                  return "Unknown";
    }
}

// ============================================================================
// PlayMode Implementation
// ============================================================================

PlayMode& PlayMode::Instance() {
    static PlayMode instance;
    return instance;
}

PlayMode::PlayMode()
    : m_lastScriptCheckTime(std::chrono::file_clock::now())
    , m_lastShaderCheckTime(std::chrono::file_clock::now())
{
    // Initialize with development settings by default
    m_settings = CreateDevelopmentSettings();
}

PlayMode::~PlayMode() {
    if (m_initialized) {
        Shutdown();
    }
}

// ============================================================================
// Initialization
// ============================================================================

bool PlayMode::Initialize(Scene* scene) {
    if (m_initialized) {
        LOG_WARN("PlayMode already initialized");
        return true;
    }

    if (!scene) {
        SetError(PlayModeError::Type::Unknown, "Scene pointer is null", "PlayMode::Initialize");
        return false;
    }

    m_scene = scene;
    m_initialized = true;

    LOG_INFO("PlayMode system initialized");
    return true;
}

void PlayMode::Shutdown() {
    if (m_state != PlayState::Stopped) {
        Stop();
    }

    ClearCallbacks();
    ClearSavedState();
    m_scene = nullptr;
    m_initialized = false;

    LOG_INFO("PlayMode system shutdown");
}

// ============================================================================
// Play Control
// ============================================================================

bool PlayMode::Play() {
    if (!m_initialized) {
        SetError(PlayModeError::Type::Unknown, "PlayMode not initialized", "PlayMode::Play");
        return false;
    }

    if (m_state != PlayState::Stopped) {
        LOG_WARN("Already in play mode");
        return true;
    }

    LOG_INFO("Entering play mode...");

    // Save current scene state
    if (!SaveSceneState()) {
        SetError(PlayModeError::Type::SceneSerializationFailed,
                 "Failed to save scene state before play", "PlayMode::Play");
        return false;
    }

    // Save editor camera
    SaveEditorCamera();

    // Create separate window if configured
    if (!m_settings.playInViewport) {
        if (!CreateGameWindow()) {
            RestoreSceneState();
            return false;
        }
    }

    // Start simulation subsystems
    StartSimulation();

    // Update state
    m_state = PlayState::Playing;
    m_playTime = 0.0f;
    m_frameCount = 0;
    m_physicsAccumulator = 0.0f;
    m_playStartTime = std::chrono::steady_clock::now();
    m_lastFrameTime = m_playStartTime;

    // Clear any previous errors
    ClearError();

    // Dispatch event
    DispatchPlayStarted();

    LOG_INFO("Play mode started");
    return true;
}

void PlayMode::Pause() {
    if (m_state != PlayState::Playing) {
        return;
    }

    m_state = PlayState::Paused;

    // Pause audio
    if (m_settings.enableAudio) {
        PauseAudio();
    }

    DispatchPlayPaused();
    LOG_DEBUG("Play mode paused");
}

void PlayMode::Resume() {
    if (m_state != PlayState::Paused) {
        return;
    }

    m_state = PlayState::Playing;
    m_lastFrameTime = std::chrono::steady_clock::now();

    // Resume audio
    if (m_settings.enableAudio) {
        ResumeAudio();
    }

    DispatchPlayResumed();
    LOG_DEBUG("Play mode resumed");
}

void PlayMode::Stop() {
    if (m_state == PlayState::Stopped) {
        return;
    }

    LOG_INFO("Stopping play mode...");

    // Stop simulation
    StopSimulation();

    // Cleanup dynamic objects first
    CleanupDynamicObjects();

    // Destroy game window if created
    if (m_gameWindow) {
        DestroyGameWindow();
    }

    // Restore scene state
    if (!RestoreSceneState()) {
        LOG_ERROR("Failed to restore scene state after play");
        // Continue anyway - better to have broken state than crash
    }

    // Restore editor camera
    RestoreEditorCamera();

    // Update state
    m_state = PlayState::Stopped;

    DispatchPlayStopped();
    LOG_INFO("Play mode stopped, scene restored");
}

void PlayMode::Step() {
    if (m_state == PlayState::Stopped) {
        // Start playing first, then immediately pause
        if (!Play()) {
            return;
        }
        Pause();
    }

    if (m_state != PlayState::Paused) {
        return;
    }

    // Set stepping state temporarily
    m_state = PlayState::Stepping;

    // Advance one frame
    StepSimulation();

    // Return to paused
    m_state = PlayState::Paused;

    LOG_DEBUG("Stepped one frame");
}

void PlayMode::TogglePlayStop() {
    if (m_state == PlayState::Stopped) {
        Play();
    } else {
        Stop();
    }
}

void PlayMode::TogglePlayPause() {
    if (m_state == PlayState::Playing) {
        Pause();
    } else if (m_state == PlayState::Paused) {
        Resume();
    } else if (m_state == PlayState::Stopped) {
        Play();
    }
}

// ============================================================================
// Settings
// ============================================================================

void PlayMode::SetSettings(const PlayModeSettings& settings) {
    m_settings = settings;
    m_settings.Validate();
}

void PlayMode::SetTimeScale(float scale) {
    m_settings.timeScale = glm::clamp(scale, 0.0f, 10.0f);
}

// ============================================================================
// Update Loop
// ============================================================================

void PlayMode::Update(float deltaTime) {
    if (m_state == PlayState::Stopped) {
        return;
    }

    // Check for hot reload if enabled
    if (m_settings.enableScriptHotReload || m_settings.enableShaderHotReload) {
        CheckHotReload();
    }

    if (m_state == PlayState::Playing) {
        // Apply time scale and cap
        float scaledDelta = deltaTime * m_settings.timeScale;
        scaledDelta = std::min(scaledDelta, m_settings.maxDeltaTime);

        // Update simulation
        UpdateSimulation(scaledDelta);

        // Update play time
        m_playTime += scaledDelta;
        m_frameCount++;
    }

    // Update debug info regardless of pause state
    UpdateDebugInfo();
}

void PlayMode::UpdateSimulation(float deltaTime) {
    auto frameStart = std::chrono::steady_clock::now();

    // Update physics with fixed timestep
    if (m_settings.enablePhysics) {
        auto physicsStart = std::chrono::steady_clock::now();
        UpdatePhysics(deltaTime);
        auto physicsEnd = std::chrono::steady_clock::now();
        m_lastPhysicsTime = std::chrono::duration<float, std::milli>(physicsEnd - physicsStart).count();
    }

    // Update scripts
    if (m_settings.enableScripts) {
        auto scriptStart = std::chrono::steady_clock::now();
        UpdateScripts(deltaTime);
        auto scriptEnd = std::chrono::steady_clock::now();
        m_lastScriptTime = std::chrono::duration<float, std::milli>(scriptEnd - scriptStart).count();
    }

    // Update audio
    if (m_settings.enableAudio) {
        UpdateAudio(deltaTime);
    }

    // Update scene (animations, etc.)
    if (m_scene) {
        m_scene->Update(deltaTime);
    }

    m_lastFrameTime = std::chrono::steady_clock::now();
}

void PlayMode::StepSimulation() {
    // Use fixed timestep for stepping
    UpdateSimulation(m_settings.fixedTimestep);
}

// ============================================================================
// Physics Integration
// ============================================================================

void PlayMode::InitializePhysics() {
    if (!m_settings.enablePhysics) {
        return;
    }

    LOG_DEBUG("Initializing play mode physics");

    // Create physics world with default config
    PhysicsWorldConfig config;
    config.fixedTimestep = m_settings.fixedTimestep;
    m_playPhysicsWorld = std::make_unique<PhysicsWorld>(config);

    // FUTURE: Copy physics bodies from scene to play world
}

void PlayMode::UpdatePhysics(float deltaTime) {
    if (!m_playPhysicsWorld) {
        return;
    }

    // Accumulate time for fixed timestep
    m_physicsAccumulator += deltaTime;

    // Run fixed steps
    int steps = 0;
    const int maxSteps = 8;  // Prevent spiral of death

    while (m_physicsAccumulator >= m_settings.fixedTimestep && steps < maxSteps) {
        m_playPhysicsWorld->FixedStep();
        m_physicsAccumulator -= m_settings.fixedTimestep;
        steps++;
    }

    // Clamp accumulator to prevent buildup
    if (m_physicsAccumulator > m_settings.fixedTimestep) {
        m_physicsAccumulator = m_settings.fixedTimestep;
    }
}

void PlayMode::ShutdownPhysics() {
    if (m_playPhysicsWorld) {
        m_playPhysicsWorld->Clear();
        m_playPhysicsWorld.reset();
    }
}

// ============================================================================
// Script Integration
// ============================================================================

void PlayMode::InitializeScripts() {
    if (!m_settings.enableScripts) {
        return;
    }

    LOG_DEBUG("Initializing play mode scripts");

    m_playScriptContext = std::make_unique<Scripting::ScriptContext>();

    // FUTURE: Initialize scripts from scene
}

void PlayMode::UpdateScripts(float deltaTime) {
    if (!m_playScriptContext) {
        return;
    }

    m_playScriptContext->Update(deltaTime);
}

void PlayMode::ShutdownScripts() {
    if (m_playScriptContext) {
        m_playScriptContext.reset();
    }
}

// ============================================================================
// Audio Integration
// ============================================================================

void PlayMode::InitializeAudio() {
    if (!m_settings.enableAudio) {
        return;
    }

    LOG_DEBUG("Initializing play mode audio");
    // Audio engine is a singleton, just ensure it's initialized
    auto& audio = AudioEngine::Instance();
    if (!audio.IsInitialized()) {
        audio.Initialize();
    }
}

void PlayMode::UpdateAudio(float deltaTime) {
    if (!m_settings.enableAudio) {
        return;
    }

    auto& audio = AudioEngine::Instance();
    audio.Update(deltaTime);
}

void PlayMode::ShutdownAudio() {
    // Don't shutdown the audio engine, just stop all sounds
    auto& audio = AudioEngine::Instance();
    audio.StopAll();
}

void PlayMode::PauseAudio() {
    auto& audio = AudioEngine::Instance();
    audio.PauseAll();
}

void PlayMode::ResumeAudio() {
    auto& audio = AudioEngine::Instance();
    audio.ResumeAll();
}

// ============================================================================
// Simulation Control
// ============================================================================

void PlayMode::StartSimulation() {
    LOG_DEBUG("Starting simulation subsystems");

    InitializePhysics();
    InitializeScripts();
    InitializeAudio();

    SetupPlayCamera();
}

void PlayMode::StopSimulation() {
    LOG_DEBUG("Stopping simulation subsystems");

    ShutdownAudio();
    ShutdownScripts();
    ShutdownPhysics();
}

// ============================================================================
// Scene State Management
// ============================================================================

bool PlayMode::SaveSceneState() {
    if (!m_scene) {
        return false;
    }

    LOG_DEBUG("Saving scene state before play");

    try {
        json sceneJson;

        // Serialize scene metadata
        sceneJson["name"] = m_scene->GetName();
        sceneJson["version"] = 1;

        // Serialize all nodes recursively
        json nodesJson = json::array();
        if (m_scene->GetRoot()) {
            std::function<void(const SceneNode*, json&)> serializeNode =
                [&serializeNode](const SceneNode* node, json& parentArray) {
                    json nodeJson;
                    nodeJson["name"] = node->GetName();

                    // Position, rotation, scale
                    const auto& pos = node->GetPosition();
                    nodeJson["position"] = {pos.x, pos.y, pos.z};

                    const auto& rot = node->GetRotation();
                    nodeJson["rotation"] = {rot.w, rot.x, rot.y, rot.z};

                    const auto& scale = node->GetScale();
                    nodeJson["scale"] = {scale.x, scale.y, scale.z};

                    nodeJson["visible"] = node->IsVisible();

                    // Serialize children
                    json childrenJson = json::array();
                    for (const auto& child : node->GetChildren()) {
                        serializeNode(child.get(), childrenJson);
                    }
                    nodeJson["children"] = childrenJson;

                    parentArray.push_back(nodeJson);
                };

            serializeNode(m_scene->GetRoot(), nodesJson);
        }

        sceneJson["nodes"] = nodesJson;

        // Store as string
        m_savedSceneState = sceneJson.dump();

        LOG_DEBUG("Scene state saved ({} bytes)", m_savedSceneState.size());
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to serialize scene state: {}", e.what());
        return false;
    }
}

bool PlayMode::RestoreSceneState() {
    if (m_savedSceneState.empty()) {
        LOG_WARN("No saved scene state to restore");
        return false;
    }

    LOG_DEBUG("Restoring scene state after play");

    try {
        json sceneJson = json::parse(m_savedSceneState);

        // Restore nodes recursively
        if (m_scene->GetRoot() && sceneJson.contains("nodes")) {
            const auto& nodesJson = sceneJson["nodes"];

            std::function<void(SceneNode*, const json&)> restoreNode =
                [&restoreNode](SceneNode* node, const json& nodeJson) {
                    // Restore transform
                    if (nodeJson.contains("position")) {
                        const auto& p = nodeJson["position"];
                        node->SetPosition({p[0].get<float>(), p[1].get<float>(), p[2].get<float>()});
                    }

                    if (nodeJson.contains("rotation")) {
                        const auto& r = nodeJson["rotation"];
                        node->SetRotation(glm::quat{
                            r[0].get<float>(), r[1].get<float>(),
                            r[2].get<float>(), r[3].get<float>()
                        });
                    }

                    if (nodeJson.contains("scale")) {
                        const auto& s = nodeJson["scale"];
                        node->SetScale({s[0].get<float>(), s[1].get<float>(), s[2].get<float>()});
                    }

                    if (nodeJson.contains("visible")) {
                        node->SetVisible(nodeJson["visible"].get<bool>());
                    }

                    // Restore children
                    if (nodeJson.contains("children")) {
                        const auto& childrenJson = nodeJson["children"];
                        const auto& children = node->GetChildren();

                        for (size_t i = 0; i < std::min(childrenJson.size(), children.size()); ++i) {
                            restoreNode(children[i].get(), childrenJson[i]);
                        }
                    }
                };

            if (!nodesJson.empty()) {
                restoreNode(m_scene->GetRoot(), nodesJson[0]);
            }
        }

        LOG_DEBUG("Scene state restored");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to restore scene state: {}", e.what());
        return false;
    }
}

void PlayMode::ClearSavedState() {
    m_savedSceneState.clear();
}

// ============================================================================
// Camera Management
// ============================================================================

void PlayMode::SaveEditorCamera() {
    if (!m_scene) return;

    auto* camera = m_scene->GetCamera();
    if (!camera) return;

    m_savedCameraPosition = camera->GetPosition();
    m_savedCameraRotation = glm::vec3(camera->GetPitch(), camera->GetYaw(), 0.0f);
    m_savedCameraFOV = camera->GetFOV();

    LOG_DEBUG("Editor camera state saved (pos: {:.2f}, {:.2f}, {:.2f})",
              m_savedCameraPosition.x, m_savedCameraPosition.y, m_savedCameraPosition.z);
}

void PlayMode::RestoreEditorCamera() {
    if (!m_scene) return;

    auto* camera = m_scene->GetCamera();
    if (!camera) return;

    camera->SetPosition(m_savedCameraPosition);
    camera->SetRotation(m_savedCameraRotation.x, m_savedCameraRotation.y);

    LOG_DEBUG("Editor camera state restored");
}

void PlayMode::SetupPlayCamera() {
    if (!m_scene) return;

    if (m_settings.startFromCurrentView) {
        // Keep current editor camera view
        LOG_DEBUG("Using editor camera for play");
    } else if (m_settings.startFromSceneCamera) {
        // FUTURE: Switch to scene's designated play camera
        LOG_DEBUG("Using scene camera for play");
    }
}

// ============================================================================
// Dynamic Object Tracking
// ============================================================================

void PlayMode::RegisterDynamicObject(SceneNode* node) {
    if (!node) return;

    std::lock_guard<std::mutex> lock(m_dynamicObjectsMutex);
    m_dynamicObjects.insert(node);
}

void PlayMode::UnregisterDynamicObject(SceneNode* node) {
    if (!node) return;

    std::lock_guard<std::mutex> lock(m_dynamicObjectsMutex);
    m_dynamicObjects.erase(node);
}

void PlayMode::CleanupDynamicObjects() {
    std::lock_guard<std::mutex> lock(m_dynamicObjectsMutex);

    LOG_DEBUG("Cleaning up {} dynamic objects", m_dynamicObjects.size());

    for (SceneNode* node : m_dynamicObjects) {
        // Remove from scene hierarchy
        if (node && node->GetParent()) {
            node->GetParent()->RemoveChild(node);
        }
    }

    m_dynamicObjects.clear();
}

// ============================================================================
// Debug Overlays
// ============================================================================

void PlayMode::UpdateDebugInfo() {
    auto now = std::chrono::steady_clock::now();
    float frameTime = std::chrono::duration<float, std::milli>(now - m_lastFrameTime).count();

    m_debugInfo.frameTime = frameTime;
    m_debugInfo.fps = frameTime > 0.0f ? 1000.0f / frameTime : 0.0f;
    m_debugInfo.physicsTime = m_lastPhysicsTime;
    m_debugInfo.scriptTime = m_lastScriptTime;

    // Physics stats
    if (m_playPhysicsWorld) {
        const auto& stats = m_playPhysicsWorld->GetStats();
        m_debugInfo.physicsBodyCount = stats.bodyCount;
        m_debugInfo.physicsActiveBodyCount = stats.activeBodyCount;
        m_debugInfo.physicsContactCount = stats.contactCount;
    }

    // Scene stats
    if (m_scene) {
        m_debugInfo.sceneNodeCount = m_scene->GetNodeCount();
    }

    // Script stats
    if (m_playScriptContext) {
        const auto& metrics = m_playScriptContext->GetMetrics();
        m_debugInfo.activeScriptCount = metrics.apiCallCount;
    }
}

void PlayMode::RenderDebugOverlays() {
    if (m_state == PlayState::Stopped) {
        return;
    }

    // Play indicator border/tint
    RenderPlayIndicator();

    if (m_settings.showFPSCounter) {
        RenderFPSCounter();
    }

    if (m_settings.showPhysicsDebug) {
        RenderPhysicsDebug();
    }

    if (m_settings.showScriptErrors && !m_debugInfo.recentScriptErrors.empty()) {
        RenderScriptErrors();
    }

    if (m_settings.showMemoryUsage) {
        RenderMemoryStats();
    }

    if (m_settings.showPerformanceStats) {
        RenderPerformanceStats();
    }
}

void PlayMode::RenderFPSCounter() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 120, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);

    ImGui::Begin("PlayMode FPS", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("FPS: %.1f", m_debugInfo.fps);
    ImGui::Text("Frame: %.2f ms", m_debugInfo.frameTime);

    if (m_state == PlayState::Paused) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "PAUSED");
    }

    ImGui::End();
}

void PlayMode::RenderPhysicsDebug() {
    if (!m_playPhysicsWorld) {
        return;
    }

    // Draw physics debug visualization
    m_playPhysicsWorld->DebugRender();

    // Physics info window
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(10, io.DisplaySize.y - 100), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);

    ImGui::Begin("Physics Debug", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Bodies: %zu", m_debugInfo.physicsBodyCount);
    ImGui::Text("Active: %zu", m_debugInfo.physicsActiveBodyCount);
    ImGui::Text("Contacts: %zu", m_debugInfo.physicsContactCount);
    ImGui::Text("Time: %.2f ms", m_debugInfo.physicsTime);

    ImGui::End();
}

void PlayMode::RenderScriptErrors() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.8f);

    ImGui::Begin("Script Errors", nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Script Errors:");
    ImGui::Separator();

    for (const auto& error : m_debugInfo.recentScriptErrors) {
        ImGui::TextWrapped("%s", error.c_str());
    }

    if (ImGui::Button("Clear")) {
        m_debugInfo.recentScriptErrors.clear();
    }

    ImGui::End();
}

void PlayMode::RenderMemoryStats() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 150, 80), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);

    ImGui::Begin("Memory", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Total: %zu MB", m_debugInfo.totalMemoryMB);
    ImGui::Text("Scene: %zu MB", m_debugInfo.sceneMemoryMB);
    ImGui::Text("Physics: %zu MB", m_debugInfo.physicsMemoryMB);

    ImGui::End();
}

void PlayMode::RenderPerformanceStats() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 200, 150), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);

    ImGui::Begin("Performance", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Physics: %.2f ms", m_debugInfo.physicsTime);
    ImGui::Text("Scripts: %.2f ms", m_debugInfo.scriptTime);
    ImGui::Text("Render:  %.2f ms", m_debugInfo.renderTime);
    ImGui::Separator();
    ImGui::Text("Nodes: %zu", m_debugInfo.sceneNodeCount);
    ImGui::Text("Visible: %zu", m_debugInfo.visibleNodeCount);
    ImGui::Text("Draw Calls: %zu", m_debugInfo.drawCallCount);

    ImGui::End();
}

void PlayMode::RenderPlayIndicator() {
    // Draw colored border around viewport to indicate play mode
    glm::vec4 color = GetPlayIndicatorColor();

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    float borderWidth = 3.0f;
    ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(
        ImVec4(color.r, color.g, color.b, color.a));

    // Top border
    drawList->AddRectFilled(
        ImVec2(0, 0),
        ImVec2(io.DisplaySize.x, borderWidth),
        borderColor);

    // Bottom border
    drawList->AddRectFilled(
        ImVec2(0, io.DisplaySize.y - borderWidth),
        ImVec2(io.DisplaySize.x, io.DisplaySize.y),
        borderColor);

    // Left border
    drawList->AddRectFilled(
        ImVec2(0, 0),
        ImVec2(borderWidth, io.DisplaySize.y),
        borderColor);

    // Right border
    drawList->AddRectFilled(
        ImVec2(io.DisplaySize.x - borderWidth, 0),
        ImVec2(io.DisplaySize.x, io.DisplaySize.y),
        borderColor);

    // Play state indicator text
    const char* stateText = GetPlayIndicatorText();
    ImVec2 textSize = ImGui::CalcTextSize(stateText);
    ImVec2 textPos((io.DisplaySize.x - textSize.x) / 2.0f, borderWidth + 5.0f);

    drawList->AddText(textPos, borderColor, stateText);
}

glm::vec4 PlayMode::GetPlayIndicatorColor() const {
    switch (m_state) {
        case PlayState::Playing:
            return {0.2f, 0.8f, 0.2f, 1.0f};  // Green
        case PlayState::Paused:
            return {1.0f, 0.8f, 0.0f, 1.0f};  // Yellow
        case PlayState::Stepping:
            return {0.0f, 0.6f, 1.0f, 1.0f};  // Blue
        default:
            return {0.0f, 0.0f, 0.0f, 0.0f};  // Transparent
    }
}

const char* PlayMode::GetPlayIndicatorText() const {
    switch (m_state) {
        case PlayState::Playing:
            return "PLAYING";
        case PlayState::Paused:
            return "PAUSED";
        case PlayState::Stepping:
            return "STEPPING";
        default:
            return "";
    }
}

// ============================================================================
// Event Callbacks
// ============================================================================

void PlayMode::OnPlayStarted(PlayModeCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_onPlayStarted.push_back(std::move(callback));
}

void PlayMode::OnPlayPaused(PlayModeCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_onPlayPaused.push_back(std::move(callback));
}

void PlayMode::OnPlayResumed(PlayModeCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_onPlayResumed.push_back(std::move(callback));
}

void PlayMode::OnPlayStopped(PlayModeCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_onPlayStopped.push_back(std::move(callback));
}

void PlayMode::OnPlayError(PlayModeErrorCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_onPlayError.push_back(std::move(callback));
}

void PlayMode::ClearCallbacks() {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_onPlayStarted.clear();
    m_onPlayPaused.clear();
    m_onPlayResumed.clear();
    m_onPlayStopped.clear();
    m_onPlayError.clear();
}

void PlayMode::DispatchPlayStarted() {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_onPlayStarted) {
        if (callback) callback();
    }
}

void PlayMode::DispatchPlayPaused() {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_onPlayPaused) {
        if (callback) callback();
    }
}

void PlayMode::DispatchPlayResumed() {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_onPlayResumed) {
        if (callback) callback();
    }
}

void PlayMode::DispatchPlayStopped() {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_onPlayStopped) {
        if (callback) callback();
    }
}

void PlayMode::DispatchPlayError(const PlayModeError& error) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_onPlayError) {
        if (callback) callback(error);
    }
}

// ============================================================================
// Error Handling
// ============================================================================

void PlayMode::SetError(PlayModeError::Type type, const std::string& message,
                        const std::string& source) {
    m_lastError = PlayModeError::Make(type, message, source);
    LOG_ERROR("PlayMode error: {} ({})", message, source);
    DispatchPlayError(m_lastError);
}

// ============================================================================
// Keyboard Shortcuts
// ============================================================================

bool PlayMode::HandleKeyboardShortcut(int key, int mods) {
    // Escape key always stops play mode
    if (key == GLFW_KEY_ESCAPE && m_state != PlayState::Stopped) {
        Stop();
        return true;
    }

    // F5: Toggle play/stop
    if (key == GLFW_KEY_F5) {
        TogglePlayStop();
        return true;
    }

    // F6: Pause/Resume
    if (key == GLFW_KEY_F6 && m_state != PlayState::Stopped) {
        TogglePlayPause();
        return true;
    }

    // F7 or F10: Step frame
    if ((key == GLFW_KEY_F7 || key == GLFW_KEY_F10) &&
        (m_state == PlayState::Paused || m_state == PlayState::Stopped)) {
        Step();
        return true;
    }

    // P: Toggle physics debug (when playing)
    if (key == GLFW_KEY_P && (mods & GLFW_MOD_CONTROL) && m_state != PlayState::Stopped) {
        TogglePhysicsDebug();
        return true;
    }

    return false;
}

// ============================================================================
// Hot Reload
// ============================================================================

void PlayMode::HotReloadScripts() {
    if (m_state == PlayState::Stopped) {
        return;
    }

    LOG_INFO("Hot reloading scripts...");

    // Save script state
    // FUTURE: Implement script state preservation

    // Reinitialize scripts
    ShutdownScripts();
    InitializeScripts();

    LOG_INFO("Scripts hot reloaded");
}

void PlayMode::HotReloadShaders() {
    if (m_state == PlayState::Stopped) {
        return;
    }

    LOG_INFO("Hot reloading shaders...");

    // FUTURE: Implement shader hot reload
    // This would typically trigger a recompile of all shader programs

    LOG_INFO("Shaders hot reloaded");
}

void PlayMode::CheckHotReload() {
    auto now = std::chrono::file_clock::now();

    // Check every 500ms
    auto checkInterval = std::chrono::milliseconds(500);

    if (m_settings.enableScriptHotReload) {
        if (now - m_lastScriptCheckTime > checkInterval) {
            m_lastScriptCheckTime = now;

            // FUTURE: Check watched script files for modifications
            // For each modified file, trigger HotReloadScripts()
        }
    }

    if (m_settings.enableShaderHotReload) {
        if (now - m_lastShaderCheckTime > checkInterval) {
            m_lastShaderCheckTime = now;

            // FUTURE: Check watched shader files for modifications
            // For each modified file, trigger HotReloadShaders()
        }
    }
}

// ============================================================================
// Window Management
// ============================================================================

bool PlayMode::CreateGameWindow() {
    LOG_INFO("Creating separate game window ({}x{})",
             m_settings.separateWindowSize.x, m_settings.separateWindowSize.y);

    // FUTURE: Implement separate window creation
    // This would create a new GLFW window for game rendering

    SetError(PlayModeError::Type::WindowCreationFailed,
             "Separate game window not yet implemented", "PlayMode::CreateGameWindow");
    return false;
}

void PlayMode::DestroyGameWindow() {
    if (m_gameWindow) {
        LOG_INFO("Destroying game window");
        m_gameWindow.reset();
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

PlayModeSettings CreateDevelopmentSettings() {
    PlayModeSettings settings;
    settings.startFromCurrentView = true;
    settings.enablePhysics = true;
    settings.enableScripts = true;
    settings.enableAudio = true;
    settings.enableNetworking = false;
    settings.timeScale = 1.0f;
    settings.maxDeltaTime = 0.1f;
    settings.showFPSCounter = true;
    settings.showPhysicsDebug = false;
    settings.showScriptErrors = true;
    settings.showMemoryUsage = false;
    settings.playInViewport = true;
    settings.enableScriptHotReload = true;
    settings.enableShaderHotReload = true;
    return settings;
}

PlayModeSettings CreateReleaseSettings() {
    PlayModeSettings settings;
    settings.startFromCurrentView = false;
    settings.startFromSceneCamera = true;
    settings.enablePhysics = true;
    settings.enableScripts = true;
    settings.enableAudio = true;
    settings.enableNetworking = true;
    settings.timeScale = 1.0f;
    settings.maxDeltaTime = 0.05f;
    settings.showFPSCounter = false;
    settings.showPhysicsDebug = false;
    settings.showScriptErrors = false;
    settings.showMemoryUsage = false;
    settings.playInViewport = false;
    settings.maximizeOnPlay = true;
    settings.enableScriptHotReload = false;
    settings.enableShaderHotReload = false;
    return settings;
}

PlayModeSettings CreateMinimalSettings() {
    PlayModeSettings settings;
    settings.startFromCurrentView = true;
    settings.enablePhysics = false;
    settings.enableScripts = false;
    settings.enableAudio = false;
    settings.enableNetworking = false;
    settings.timeScale = 1.0f;
    settings.maxDeltaTime = 0.1f;
    settings.showFPSCounter = true;
    settings.showPhysicsDebug = false;
    settings.showScriptErrors = false;
    settings.showMemoryUsage = false;
    settings.playInViewport = true;
    settings.enableScriptHotReload = false;
    settings.enableShaderHotReload = false;
    return settings;
}

} // namespace Nova
