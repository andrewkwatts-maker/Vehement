#pragma once

#include <memory>
#include <string>
#include <functional>
#include <chrono>

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
     */
    struct ApplicationCallbacks {
        std::function<bool()> onStartup;
        std::function<void(float deltaTime)> onUpdate;
        std::function<void()> onRender;
        std::function<void()> onImGui;
        std::function<void()> onShutdown;
    };

    // Singleton access
    static Engine& Instance();

    // Delete copy/move
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    /**
     * @brief Initialize the engine with given parameters
     * @return true if initialization succeeded
     */
    bool Initialize(const InitParams& params = {});

    /**
     * @brief Run the main engine loop
     * @param callbacks User application callbacks
     * @return Exit code
     */
    int Run(const ApplicationCallbacks& callbacks);

    /**
     * @brief Request engine shutdown
     */
    void RequestShutdown();

    /**
     * @brief Check if engine is running
     */
    bool IsRunning() const { return m_running; }

    // Subsystem access
    Window& GetWindow() { return *m_window; }
    const Window& GetWindow() const { return *m_window; }

    Time& GetTime() { return *m_time; }
    const Time& GetTime() const { return *m_time; }

    Renderer& GetRenderer() { return *m_renderer; }
    const Renderer& GetRenderer() const { return *m_renderer; }

    InputManager& GetInput() { return *m_input; }
    const InputManager& GetInput() const { return *m_input; }

    Scene* GetActiveScene() { return m_activeScene.get(); }
    void SetActiveScene(std::unique_ptr<Scene> scene);

    // Engine info
    static constexpr const char* GetVersion() { return "1.0.0"; }
    static constexpr const char* GetName() { return "Nova3D"; }

private:
    Engine() = default;
    ~Engine();

    void Shutdown();
    void ProcessFrame(const ApplicationCallbacks& callbacks);
    void BeginFrame();
    void EndFrame();

    std::unique_ptr<Window> m_window;
    std::unique_ptr<Time> m_time;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<InputManager> m_input;
    std::unique_ptr<Scene> m_activeScene;

    bool m_initialized = false;
    bool m_running = false;
    bool m_imguiEnabled = false;
    bool m_debugDrawEnabled = false;
};

} // namespace Nova
