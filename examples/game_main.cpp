#include "core/Engine.hpp"
#include "RTSApplication.hpp"

#include <spdlog/spdlog.h>

/**
 * @brief Entry point for the Nova3D RTS Game
 *
 * This executable launches directly into the RTSApplication game menu.
 * The level editor is not accessible from this executable - use the
 * separate nova_editor executable for content creation.
 */
int main(int argc, char* argv[]) {
    // Initialize logging
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Starting Nova3D RTS Game");

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

    // Create RTS application
    RTSApplication rtsApp;

    // Set up callbacks
    Nova::Engine::ApplicationCallbacks callbacks;

    callbacks.onStartup = [&rtsApp]() {
        return rtsApp.Initialize();
    };

    callbacks.onUpdate = [&rtsApp](float deltaTime) {
        rtsApp.Update(deltaTime);
    };

    callbacks.onRender = [&rtsApp]() {
        rtsApp.Render();
    };

    callbacks.onImGui = [&rtsApp]() {
        rtsApp.OnImGui();
    };

    callbacks.onShutdown = [&rtsApp]() {
        rtsApp.Shutdown();
    };

    // Run engine
    int result = engine.Run(callbacks);

    spdlog::info("Nova3D RTS Game finished with code {}", result);
    return result;
}
