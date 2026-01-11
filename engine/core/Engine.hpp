/**
 * @file Engine.hpp
 * @brief Core engine class and application lifecycle management
 *
 * This file contains the main Engine singleton class that orchestrates all
 * engine subsystems including rendering, input, timing, and scene management.
 *
 * @section engine_usage Basic Usage
 *
 * @code{.cpp}
 * #include <engine/core/Engine.hpp>
 *
 * int main() {
 *     auto& engine = Nova::Engine::Instance();
 *
 *     Nova::Engine::InitParams params;
 *     params.configPath = "config/engine.json";
 *     params.enableImGui = true;
 *
 *     if (!engine.Initialize(params)) {
 *         return -1;
 *     }
 *
 *     Nova::Engine::ApplicationCallbacks callbacks;
 *     callbacks.onStartup = []() {
 *         // Load resources, initialize game state
 *         return true;
 *     };
 *     callbacks.onUpdate = [](float dt) {
 *         // Update game logic each frame
 *     };
 *     callbacks.onRender = []() {
 *         // Render the game world
 *     };
 *     callbacks.onImGui = []() {
 *         // Render debug UI (optional)
 *     };
 *     callbacks.onShutdown = []() {
 *         // Cleanup resources
 *     };
 *
 *     return engine.Run(std::move(callbacks));
 * }
 * @endcode
 *
 * @section engine_subsystems Subsystem Access
 *
 * Access engine subsystems through getter methods:
 * - GetWindow() - Window management and display settings
 * - GetTime() - Frame timing and delta time
 * - GetRenderer() - Graphics rendering
 * - GetInput() - Keyboard, mouse, and gamepad input
 * - GetActiveScene() - Current game scene
 *
 * @see Window, Time, Renderer, InputManager, Scene
 * @see docs/api/Engine.md for complete API documentation
 *
 * @author Nova3D Team
 * @version 1.0.0
 */

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <functional>
#include <chrono>
#include <optional>

namespace Nova {

// Forward declarations
class Window;
class Time;
class Renderer;
class InputManager;
class Scene;

/**
 * @brief Main engine class - orchestrates all subsystems
 *
 * Modern replacement for the old Application class with proper
 * resource management, event handling, and separation of concerns.
 */
class Engine {
public:
    /**
     * @brief Engine configuration passed at initialization
     */
    struct InitParams {
        std::string configPath = "config/engine.json";
        bool enableImGui = true;
        bool enableDebugDraw = true;
    };

    /**
     * @brief Application callbacks for user code
     *
     * All callbacks are optional. Use std::move when passing to Run()
     * for optimal performance with captured lambdas.
     */
    struct ApplicationCallbacks {
        std::function<bool()> onStartup;
        std::function<void(float deltaTime)> onUpdate;
        std::function<void()> onRender;
        std::function<void()> onImGui;
        std::function<void()> onShutdown;

        // Enable move semantics for efficient transfer of captured lambdas
        ApplicationCallbacks() = default;
        ApplicationCallbacks(ApplicationCallbacks&&) noexcept = default;
        ApplicationCallbacks& operator=(ApplicationCallbacks&&) noexcept = default;
        ApplicationCallbacks(const ApplicationCallbacks&) = default;
        ApplicationCallbacks& operator=(const ApplicationCallbacks&) = default;
    };

    // Singleton access
    [[nodiscard]] static Engine& Instance() noexcept;

    // Delete copy/move - singleton pattern
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    /**
     * @brief Initialize the engine with given parameters
     * @param params Initialization parameters
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool Initialize(const InitParams& params = {});

    /**
     * @brief Run the main engine loop
     * @param callbacks User application callbacks (moved for efficiency)
     * @return Exit code (0 = success, negative = error)
     */
    [[nodiscard]] int Run(ApplicationCallbacks callbacks);

    /**
     * @brief Request engine shutdown
     */
    void RequestShutdown() noexcept;

    /**
     * @brief Check if engine is running
     */
    [[nodiscard]] bool IsRunning() const noexcept { return m_running; }

    /**
     * @brief Check if engine is initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // Subsystem access - returns references, assumes engine is initialized
    [[nodiscard]] Window& GetWindow() noexcept { return *m_window; }
    [[nodiscard]] const Window& GetWindow() const noexcept { return *m_window; }

    [[nodiscard]] Time& GetTime() noexcept { return *m_time; }
    [[nodiscard]] const Time& GetTime() const noexcept { return *m_time; }

    [[nodiscard]] Renderer& GetRenderer() noexcept { return *m_renderer; }
    [[nodiscard]] const Renderer& GetRenderer() const noexcept { return *m_renderer; }

    [[nodiscard]] InputManager& GetInput() noexcept { return *m_input; }
    [[nodiscard]] const InputManager& GetInput() const noexcept { return *m_input; }

    // Scene management - may return nullptr if no scene is active
    [[nodiscard]] Scene* GetActiveScene() noexcept { return m_activeScene.get(); }
    [[nodiscard]] const Scene* GetActiveScene() const noexcept { return m_activeScene.get(); }
    void SetActiveScene(std::unique_ptr<Scene> scene);

    // Engine info - compile-time constants
    [[nodiscard]] static consteval std::string_view GetVersion() noexcept { return "1.0.0"; }
    [[nodiscard]] static consteval std::string_view GetName() noexcept { return "Nova3D"; }

private:
    Engine() = default;
    ~Engine();

    void Shutdown();
    void ProcessFrame(const ApplicationCallbacks& callbacks);
    void BeginFrame();
    void EndFrame();

    // Subsystems managed via unique_ptr for automatic cleanup
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Time> m_time;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<InputManager> m_input;
    std::unique_ptr<Scene> m_activeScene;

    // Engine state
    bool m_initialized = false;
    bool m_running = false;
    bool m_imguiEnabled = false;
    bool m_debugDrawEnabled = false;
};

} // namespace Nova
