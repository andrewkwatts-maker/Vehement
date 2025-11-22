#include "core/Engine.hpp"
#include "DemoApplication.hpp"

#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    // Initialize logging
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Starting Nova3D Engine Demo");

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

    // Create demo application
    DemoApplication demo;

    // Set up callbacks
    Nova::Engine::ApplicationCallbacks callbacks;

    callbacks.onStartup = [&demo]() {
        return demo.Initialize();
    };

    callbacks.onUpdate = [&demo](float deltaTime) {
        demo.Update(deltaTime);
    };

    callbacks.onRender = [&demo]() {
        demo.Render();
    };

    callbacks.onImGui = [&demo]() {
        demo.OnImGui();
    };

    callbacks.onShutdown = [&demo]() {
        demo.Shutdown();
    };

    // Run engine
    int result = engine.Run(callbacks);

    spdlog::info("Nova3D Engine Demo finished with code {}", result);
    return result;
}
