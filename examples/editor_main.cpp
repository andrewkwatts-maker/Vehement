#include "core/Engine.hpp"
#include "core/Window.hpp"
#include "StandaloneEditor.hpp"
#include "scene/FlyCamera.hpp"
#include "graphics/Renderer.hpp"

#include <spdlog/spdlog.h>

/**
 * @brief Entry point for the Nova3D Level Editor
 *
 * This executable launches directly into the StandaloneEditor,
 * skipping the game menu entirely. This provides a dedicated
 * editor experience for content creation.
 */
int main(int argc, char* argv[]) {
    // Initialize logging
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Starting Nova3D Level Editor");

    // Get engine instance
    Nova::Engine& engine = Nova::Engine::Instance();

    // Initialize engine
    Nova::Engine::InitParams params;
    params.configPath = "assets/config/engine.json";
    params.enableImGui = true;
    params.enableDebugDraw = true;

    if (!engine.Initialize(params)) {
        spdlog::critical("Failed to initialize engine");
        return -1;
    }

    // Create standalone editor
    StandaloneEditor editor;

    // Create camera for editor
    std::unique_ptr<Nova::FlyCamera> camera = std::make_unique<Nova::FlyCamera>();
    camera->SetPerspective(45.0f, engine.GetWindow().GetAspectRatio(), 0.1f, 1000.0f);
    camera->LookAt(glm::vec3(10, 10, 10), glm::vec3(0, 0, 0));

    // Set up callbacks
    Nova::Engine::ApplicationCallbacks callbacks;

    callbacks.onStartup = [&editor]() {
        return editor.Initialize();
    };

    callbacks.onUpdate = [&editor, &camera, &engine](float deltaTime) {
        // Update camera
        auto& input = engine.GetInput();
        camera->Update(input, deltaTime);

        // Toggle cursor lock with Tab
        if (input.IsKeyPressed(Nova::Key::Tab)) {
            bool locked = !input.IsCursorLocked();
            input.SetCursorLocked(locked);
        }

        // Update editor
        editor.Update(deltaTime);
    };

    callbacks.onRender = [&editor, &camera, &engine]() {
        auto& renderer = engine.GetRenderer();

        // Set camera for rendering
        renderer.SetCamera(camera.get());

        // Render editor 3D content
        editor.Render3D(renderer, *camera);
    };

    callbacks.onImGui = [&editor]() {
        // Render editor UI
        editor.RenderUI();
    };

    callbacks.onShutdown = [&editor]() {
        editor.Shutdown();
    };

    // Run engine with editor callbacks
    int result = engine.Run(callbacks);

    spdlog::info("Nova3D Level Editor finished with code {}", result);
    return result;
}
