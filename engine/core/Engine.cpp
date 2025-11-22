#include "core/Engine.hpp"
#include "core/Window.hpp"
#include "core/Time.hpp"
#include "core/Logger.hpp"
#include "graphics/Renderer.hpp"
#include "input/InputManager.hpp"
#include "scene/Scene.hpp"
#include "config/Config.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

namespace Nova {

Engine& Engine::Instance() {
    static Engine instance;
    return instance;
}

Engine::~Engine() {
    if (m_initialized) {
        Shutdown();
    }
}

bool Engine::Initialize(const InitParams& params) {
    if (m_initialized) {
        spdlog::warn("Engine already initialized");
        return true;
    }

    spdlog::info("Initializing Nova3D Engine v{}", GetVersion());

    // Load configuration
    auto& config = Config::Instance();
    if (!config.Load(params.configPath)) {
        spdlog::warn("Using default configuration");
    }

    // Initialize GLFW
    if (!glfwInit()) {
        spdlog::critical("Failed to initialize GLFW");
        return false;
    }

    // Create window
    m_window = std::make_unique<Window>();
    if (!m_window->Create()) {
        spdlog::critical("Failed to create window");
        glfwTerminate();
        return false;
    }

    // Initialize OpenGL loader
    if (!gladLoadGL(glfwGetProcAddress)) {
        spdlog::critical("Failed to initialize GLAD");
        return false;
    }

    spdlog::info("OpenGL Version: {}.{}", GLVersion.major, GLVersion.minor);
    spdlog::info("OpenGL Renderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    spdlog::info("OpenGL Vendor: {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));

    // Initialize subsystems
    m_time = std::make_unique<Time>();

    m_input = std::make_unique<InputManager>();
    m_input->Initialize(m_window->GetHandle());

    m_renderer = std::make_unique<Renderer>();
    if (!m_renderer->Initialize()) {
        spdlog::critical("Failed to initialize renderer");
        return false;
    }

    // Initialize ImGui
    m_imguiEnabled = params.enableImGui;
    if (m_imguiEnabled) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(m_window->GetHandle(), true);
        ImGui_ImplOpenGL3_Init("#version 460");

        spdlog::info("ImGui initialized");
    }

    m_debugDrawEnabled = params.enableDebugDraw;
    m_initialized = true;

    spdlog::info("Engine initialization complete");
    return true;
}

int Engine::Run(const ApplicationCallbacks& callbacks) {
    if (!m_initialized) {
        spdlog::error("Engine not initialized");
        return -1;
    }

    // Call user startup
    if (callbacks.onStartup && !callbacks.onStartup()) {
        spdlog::error("Application startup failed");
        return -1;
    }

    m_running = true;

    // Main loop
    while (m_running && !m_window->ShouldClose()) {
        ProcessFrame(callbacks);
    }

    // Call user shutdown
    if (callbacks.onShutdown) {
        callbacks.onShutdown();
    }

    return 0;
}

void Engine::ProcessFrame(const ApplicationCallbacks& callbacks) {
    BeginFrame();

    // Update time
    m_time->Update();
    float deltaTime = m_time->GetDeltaTime();

    // Poll input
    m_input->Update();

    // User update
    if (callbacks.onUpdate) {
        callbacks.onUpdate(deltaTime);
    }

    // Update active scene
    if (m_activeScene) {
        m_activeScene->Update(deltaTime);
    }

    // Begin rendering
    m_renderer->BeginFrame();

    // User render
    if (callbacks.onRender) {
        callbacks.onRender();
    }

    // Render active scene
    if (m_activeScene) {
        m_activeScene->Render(*m_renderer);
    }

    // Debug draw
    if (m_debugDrawEnabled) {
        m_renderer->RenderDebug();
    }

    // ImGui
    if (m_imguiEnabled) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (callbacks.onImGui) {
            callbacks.onImGui();
        }

        // Engine debug UI
        if (m_debugDrawEnabled) {
            ImGui::Begin("Engine Stats");
            ImGui::Text("FPS: %.1f", m_time->GetFPS());
            ImGui::Text("Frame Time: %.3f ms", m_time->GetDeltaTime() * 1000.0f);
            ImGui::Text("Total Time: %.2f s", m_time->GetTotalTime());
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    m_renderer->EndFrame();
    EndFrame();
}

void Engine::BeginFrame() {
    glfwPollEvents();
}

void Engine::EndFrame() {
    m_window->SwapBuffers();
}

void Engine::RequestShutdown() {
    m_running = false;
}

void Engine::SetActiveScene(std::unique_ptr<Scene> scene) {
    m_activeScene = std::move(scene);
}

void Engine::Shutdown() {
    spdlog::info("Shutting down engine");

    // Cleanup ImGui
    if (m_imguiEnabled) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    // Cleanup subsystems
    m_activeScene.reset();
    m_renderer.reset();
    m_input.reset();
    m_time.reset();
    m_window.reset();

    glfwTerminate();
    m_initialized = false;

    spdlog::info("Engine shutdown complete");
}

} // namespace Nova
