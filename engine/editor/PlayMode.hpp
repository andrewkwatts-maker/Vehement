/**
 * @file PlayMode.hpp
 * @brief Play Mode / Runtime Preview system for Nova3D Editor
 *
 * Provides a seamless play/edit mode transition system that:
 * - Saves and restores scene state for non-destructive testing
 * - Supports full physics and script simulation
 * - Provides debug overlays and hot-reload capabilities
 * - Can play in viewport or separate window
 *
 * @author Nova3D Team
 * @version 1.0.0
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Scene;
class SceneNode;
class PhysicsWorld;
class Camera;
class Window;

namespace Scripting {
    class ScriptContext;
}

/**
 * @brief Current state of the play mode system
 */
enum class PlayState : uint8_t {
    Stopped = 0,    ///< Editor mode, no simulation running
    Playing,        ///< Full simulation running
    Paused,         ///< Simulation paused, can be stepped
    Stepping        ///< Single frame advance mode
};

/**
 * @brief Convert PlayState to string for display
 */
[[nodiscard]] const char* PlayStateToString(PlayState state);

/**
 * @brief Configuration settings for play mode
 */
struct PlayModeSettings {
    // Startup options
    bool startFromCurrentView = true;       ///< Use editor camera position/rotation
    bool startFromSceneCamera = false;      ///< Use scene's main camera instead

    // Simulation toggles
    bool enablePhysics = true;              ///< Enable physics simulation
    bool enableScripts = true;              ///< Enable script execution
    bool enableAudio = true;                ///< Enable audio playback
    bool enableNetworking = false;          ///< Enable networking (disabled by default)
    bool enableParticles = true;            ///< Enable particle systems
    bool enableAnimations = true;           ///< Enable skeletal/property animations

    // Time settings
    float timeScale = 1.0f;                 ///< Time multiplier (0.0 - 10.0)
    float maxDeltaTime = 0.1f;              ///< Maximum frame time cap (prevents spiral of death)
    float fixedTimestep = 1.0f / 60.0f;     ///< Physics fixed timestep

    // Debug settings
    bool showFPSCounter = true;             ///< Show FPS overlay
    bool showPhysicsDebug = false;          ///< Show physics debug visualization
    bool showScriptErrors = true;           ///< Display script errors in viewport
    bool showMemoryUsage = false;           ///< Show memory statistics
    bool showPerformanceStats = false;      ///< Show detailed performance stats

    // Window settings
    bool playInViewport = true;             ///< Play in editor viewport (vs separate window)
    bool maximizeOnPlay = false;            ///< Maximize game view when playing
    glm::ivec2 separateWindowSize{1280, 720}; ///< Size for separate game window

    // Hot reload
    bool enableScriptHotReload = true;      ///< Hot reload scripts during play
    bool enableShaderHotReload = true;      ///< Hot reload shaders during play

    /**
     * @brief Validate settings and clamp to valid ranges
     */
    void Validate() {
        timeScale = glm::clamp(timeScale, 0.0f, 10.0f);
        maxDeltaTime = glm::clamp(maxDeltaTime, 0.001f, 1.0f);
        fixedTimestep = glm::clamp(fixedTimestep, 1.0f / 240.0f, 1.0f / 10.0f);
    }
};

/**
 * @brief Debug overlay information displayed during play mode
 */
struct PlayModeDebugInfo {
    // Performance
    float fps = 0.0f;
    float frameTime = 0.0f;
    float physicsTime = 0.0f;
    float scriptTime = 0.0f;
    float renderTime = 0.0f;

    // Memory
    size_t totalMemoryMB = 0;
    size_t sceneMemoryMB = 0;
    size_t physicsMemoryMB = 0;

    // Physics stats
    size_t physicsBodyCount = 0;
    size_t physicsContactCount = 0;
    size_t physicsActiveBodyCount = 0;

    // Script stats
    size_t activeScriptCount = 0;
    size_t scriptErrorCount = 0;
    std::vector<std::string> recentScriptErrors;

    // Scene stats
    size_t sceneNodeCount = 0;
    size_t visibleNodeCount = 0;
    size_t drawCallCount = 0;
};

/**
 * @brief Error information for play mode failures
 */
struct PlayModeError {
    enum class Type {
        None,
        SceneSerializationFailed,
        PhysicsInitFailed,
        ScriptInitFailed,
        ScriptRuntimeError,
        AudioInitFailed,
        WindowCreationFailed,
        OutOfMemory,
        Unknown
    };

    Type type = Type::None;
    std::string message;
    std::string source;                     ///< File/function where error occurred
    int line = 0;                           ///< Line number if applicable
    std::chrono::system_clock::time_point timestamp;

    [[nodiscard]] bool HasError() const { return type != Type::None; }

    static PlayModeError Make(Type t, const std::string& msg, const std::string& src = "") {
        PlayModeError err;
        err.type = t;
        err.message = msg;
        err.source = src;
        err.timestamp = std::chrono::system_clock::now();
        return err;
    }
};

/**
 * @brief Callback types for play mode events
 */
using PlayModeCallback = std::function<void()>;
using PlayModeErrorCallback = std::function<void(const PlayModeError&)>;

/**
 * @brief Play Mode manager for the Nova3D Editor
 *
 * Manages the transition between edit mode and play mode, including:
 * - Scene state serialization and restoration
 * - Physics and script simulation control
 * - Debug overlay rendering
 * - Hot reload support
 *
 * Usage:
 * @code
 * auto& playMode = PlayMode::Instance();
 *
 * // Configure settings
 * PlayModeSettings settings;
 * settings.enablePhysics = true;
 * settings.timeScale = 1.0f;
 * playMode.SetSettings(settings);
 *
 * // Register callbacks
 * playMode.OnPlayStarted([]() {
 *     spdlog::info("Play mode started");
 * });
 *
 * // Start playing
 * if (!playMode.Play()) {
 *     auto error = playMode.GetLastError();
 *     spdlog::error("Failed to start: {}", error.message);
 * }
 *
 * // In game loop
 * playMode.Update(deltaTime);
 *
 * // Stop and restore scene
 * playMode.Stop();
 * @endcode
 */
class PlayMode {
public:
    /**
     * @brief Get the singleton instance
     */
    [[nodiscard]] static PlayMode& Instance();

    // Non-copyable, non-movable
    PlayMode(const PlayMode&) = delete;
    PlayMode& operator=(const PlayMode&) = delete;
    PlayMode(PlayMode&&) = delete;
    PlayMode& operator=(PlayMode&&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the play mode system
     * @param scene Pointer to the editor scene
     * @return true if initialization succeeded
     */
    bool Initialize(Scene* scene);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Play Control
    // =========================================================================

    /**
     * @brief Enter play mode
     *
     * Saves the current scene state and begins simulation.
     * @return true if play mode started successfully
     */
    bool Play();

    /**
     * @brief Pause the simulation
     *
     * Simulation is frozen but can be resumed or stepped.
     */
    void Pause();

    /**
     * @brief Resume from paused state
     */
    void Resume();

    /**
     * @brief Stop play mode and restore scene state
     *
     * All changes made during play are discarded.
     */
    void Stop();

    /**
     * @brief Advance simulation by a single frame
     *
     * Only valid when paused. Useful for debugging.
     */
    void Step();

    /**
     * @brief Toggle between play and stop states
     */
    void TogglePlayStop();

    /**
     * @brief Toggle between play and pause states
     */
    void TogglePlayPause();

    // =========================================================================
    // State Queries
    // =========================================================================

    /**
     * @brief Get current play state
     */
    [[nodiscard]] PlayState GetState() const { return m_state; }

    /**
     * @brief Check if currently in play mode (playing or paused)
     */
    [[nodiscard]] bool IsInPlayMode() const {
        return m_state != PlayState::Stopped;
    }

    /**
     * @brief Check if simulation is running (not paused)
     */
    [[nodiscard]] bool IsSimulationRunning() const {
        return m_state == PlayState::Playing || m_state == PlayState::Stepping;
    }

    /**
     * @brief Check if paused
     */
    [[nodiscard]] bool IsPaused() const {
        return m_state == PlayState::Paused;
    }

    /**
     * @brief Get elapsed play time in seconds
     */
    [[nodiscard]] float GetPlayTime() const { return m_playTime; }

    /**
     * @brief Get number of frames since play started
     */
    [[nodiscard]] uint64_t GetFrameCount() const { return m_frameCount; }

    // =========================================================================
    // Settings
    // =========================================================================

    /**
     * @brief Set play mode settings
     * @param settings New settings to apply
     */
    void SetSettings(const PlayModeSettings& settings);

    /**
     * @brief Get current settings
     */
    [[nodiscard]] const PlayModeSettings& GetSettings() const { return m_settings; }

    /**
     * @brief Get mutable reference to settings
     */
    [[nodiscard]] PlayModeSettings& GetSettingsMutable() { return m_settings; }

    /**
     * @brief Set time scale (simulation speed multiplier)
     * @param scale Time scale (0.0 to 10.0, 1.0 = normal)
     */
    void SetTimeScale(float scale);

    /**
     * @brief Get current time scale
     */
    [[nodiscard]] float GetTimeScale() const { return m_settings.timeScale; }

    // =========================================================================
    // Debug Overlays
    // =========================================================================

    /**
     * @brief Toggle FPS counter display
     */
    void ToggleFPSCounter() { m_settings.showFPSCounter = !m_settings.showFPSCounter; }

    /**
     * @brief Toggle physics debug visualization
     */
    void TogglePhysicsDebug() { m_settings.showPhysicsDebug = !m_settings.showPhysicsDebug; }

    /**
     * @brief Toggle memory usage display
     */
    void ToggleMemoryDisplay() { m_settings.showMemoryUsage = !m_settings.showMemoryUsage; }

    /**
     * @brief Get current debug information
     */
    [[nodiscard]] const PlayModeDebugInfo& GetDebugInfo() const { return m_debugInfo; }

    // =========================================================================
    // Error Handling
    // =========================================================================

    /**
     * @brief Get the last error that occurred
     */
    [[nodiscard]] const PlayModeError& GetLastError() const { return m_lastError; }

    /**
     * @brief Check if there's an active error
     */
    [[nodiscard]] bool HasError() const { return m_lastError.HasError(); }

    /**
     * @brief Clear the last error
     */
    void ClearError() { m_lastError = PlayModeError{}; }

    // =========================================================================
    // Event Callbacks
    // =========================================================================

    /**
     * @brief Register callback for when play mode starts
     */
    void OnPlayStarted(PlayModeCallback callback);

    /**
     * @brief Register callback for when play mode is paused
     */
    void OnPlayPaused(PlayModeCallback callback);

    /**
     * @brief Register callback for when play mode is resumed
     */
    void OnPlayResumed(PlayModeCallback callback);

    /**
     * @brief Register callback for when play mode stops
     */
    void OnPlayStopped(PlayModeCallback callback);

    /**
     * @brief Register callback for play mode errors
     */
    void OnPlayError(PlayModeErrorCallback callback);

    /**
     * @brief Clear all registered callbacks
     */
    void ClearCallbacks();

    // =========================================================================
    // Update (called each frame by editor)
    // =========================================================================

    /**
     * @brief Update play mode simulation
     * @param deltaTime Raw delta time from engine
     */
    void Update(float deltaTime);

    /**
     * @brief Render debug overlays
     *
     * Call after scene rendering to draw debug info on top.
     */
    void RenderDebugOverlays();

    // =========================================================================
    // Hot Reload
    // =========================================================================

    /**
     * @brief Trigger script hot reload
     *
     * Reloads all scripts while preserving state where possible.
     */
    void HotReloadScripts();

    /**
     * @brief Trigger shader hot reload
     */
    void HotReloadShaders();

    /**
     * @brief Check for file changes and auto-reload if enabled
     */
    void CheckHotReload();

    // =========================================================================
    // Editor Integration
    // =========================================================================

    /**
     * @brief Check if editor UI should be locked (during play)
     */
    [[nodiscard]] bool IsEditorLocked() const { return m_state != PlayState::Stopped; }

    /**
     * @brief Get the play indicator color (for viewport border tint)
     * @return RGBA color for play state indication
     */
    [[nodiscard]] glm::vec4 GetPlayIndicatorColor() const;

    /**
     * @brief Get play indicator text
     */
    [[nodiscard]] const char* GetPlayIndicatorText() const;

    /**
     * @brief Handle keyboard shortcuts
     * @param key GLFW key code
     * @param mods Modifier keys
     * @return true if key was handled
     */
    bool HandleKeyboardShortcut(int key, int mods);

    // =========================================================================
    // Window Management
    // =========================================================================

    /**
     * @brief Create separate game window for play mode
     * @return true if window created successfully
     */
    bool CreateGameWindow();

    /**
     * @brief Destroy separate game window
     */
    void DestroyGameWindow();

    /**
     * @brief Get the game window (nullptr if playing in viewport)
     */
    [[nodiscard]] Window* GetGameWindow() const { return m_gameWindow.get(); }

    /**
     * @brief Check if using separate game window
     */
    [[nodiscard]] bool HasSeparateGameWindow() const { return m_gameWindow != nullptr; }

    // =========================================================================
    // Dynamic Object Tracking
    // =========================================================================

    /**
     * @brief Register an object created during play mode
     *
     * These objects will be destroyed when play mode stops.
     * @param node The dynamically created node
     */
    void RegisterDynamicObject(SceneNode* node);

    /**
     * @brief Unregister a dynamic object (if destroyed during play)
     */
    void UnregisterDynamicObject(SceneNode* node);

    /**
     * @brief Get count of dynamic objects created during play
     */
    [[nodiscard]] size_t GetDynamicObjectCount() const { return m_dynamicObjects.size(); }

private:
    PlayMode();
    ~PlayMode();

    // Scene state management
    bool SaveSceneState();
    bool RestoreSceneState();
    void ClearSavedState();

    // Serialization helpers
    std::string SerializeNode(const SceneNode* node) const;
    bool DeserializeNode(SceneNode* node, const std::string& data);

    // Simulation control
    void StartSimulation();
    void StopSimulation();
    void UpdateSimulation(float deltaTime);
    void StepSimulation();

    // Physics integration
    void InitializePhysics();
    void UpdatePhysics(float deltaTime);
    void ShutdownPhysics();

    // Script integration
    void InitializeScripts();
    void UpdateScripts(float deltaTime);
    void ShutdownScripts();

    // Audio integration
    void InitializeAudio();
    void UpdateAudio(float deltaTime);
    void ShutdownAudio();
    void PauseAudio();
    void ResumeAudio();

    // Debug info collection
    void UpdateDebugInfo();
    void RenderFPSCounter();
    void RenderPhysicsDebug();
    void RenderScriptErrors();
    void RenderMemoryStats();
    void RenderPerformanceStats();
    void RenderPlayIndicator();

    // Event dispatch
    void DispatchPlayStarted();
    void DispatchPlayPaused();
    void DispatchPlayResumed();
    void DispatchPlayStopped();
    void DispatchPlayError(const PlayModeError& error);

    // Error handling
    void SetError(PlayModeError::Type type, const std::string& message,
                  const std::string& source = "");

    // Camera management
    void SaveEditorCamera();
    void RestoreEditorCamera();
    void SetupPlayCamera();

    // Dynamic object cleanup
    void CleanupDynamicObjects();

    // State
    std::atomic<PlayState> m_state{PlayState::Stopped};
    bool m_initialized = false;

    // Scene reference
    Scene* m_scene = nullptr;

    // Saved scene state (JSON serialization)
    std::string m_savedSceneState;

    // Saved editor camera state
    glm::vec3 m_savedCameraPosition{0.0f};
    glm::vec3 m_savedCameraRotation{0.0f};
    float m_savedCameraFOV = 60.0f;

    // Settings
    PlayModeSettings m_settings;

    // Timing
    float m_playTime = 0.0f;
    float m_physicsAccumulator = 0.0f;
    uint64_t m_frameCount = 0;
    std::chrono::steady_clock::time_point m_playStartTime;
    std::chrono::steady_clock::time_point m_lastFrameTime;

    // Debug info
    PlayModeDebugInfo m_debugInfo;

    // Error state
    PlayModeError m_lastError;

    // Callbacks
    std::vector<PlayModeCallback> m_onPlayStarted;
    std::vector<PlayModeCallback> m_onPlayPaused;
    std::vector<PlayModeCallback> m_onPlayResumed;
    std::vector<PlayModeCallback> m_onPlayStopped;
    std::vector<PlayModeErrorCallback> m_onPlayError;
    std::mutex m_callbackMutex;

    // Separate game window
    std::unique_ptr<Window> m_gameWindow;

    // Dynamic objects created during play
    std::unordered_set<SceneNode*> m_dynamicObjects;
    std::mutex m_dynamicObjectsMutex;

    // Physics world for simulation
    std::unique_ptr<PhysicsWorld> m_playPhysicsWorld;

    // Script context for play mode
    std::unique_ptr<Scripting::ScriptContext> m_playScriptContext;

    // Performance timing
    float m_lastPhysicsTime = 0.0f;
    float m_lastScriptTime = 0.0f;
    float m_lastRenderTime = 0.0f;

    // Hot reload tracking
    std::chrono::file_clock::time_point m_lastScriptCheckTime;
    std::chrono::file_clock::time_point m_lastShaderCheckTime;
    std::unordered_set<std::string> m_watchedScriptFiles;
    std::unordered_set<std::string> m_watchedShaderFiles;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Create default play mode settings optimized for development
 */
[[nodiscard]] PlayModeSettings CreateDevelopmentSettings();

/**
 * @brief Create play mode settings optimized for final testing
 */
[[nodiscard]] PlayModeSettings CreateReleaseSettings();

/**
 * @brief Create minimal settings for quick iteration
 */
[[nodiscard]] PlayModeSettings CreateMinimalSettings();

} // namespace Nova
