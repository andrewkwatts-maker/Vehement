#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <core/Engine.hpp>
#include <core/Logger.hpp>

#include "core/Game.hpp"
#include "core/GameConfig.hpp"

/**
 * @brief Parse command line arguments
 */
struct CommandLineArgs {
    std::string configPath = "config/game.json";
    std::string levelPath;
    bool enableMultiplayer = false;
    bool enableGPS = false;
    bool startInEditor = false;
    bool showHelp = false;

    static CommandLineArgs Parse(int argc, char* argv[]) {
        CommandLineArgs args;

        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                args.showHelp = true;
            } else if (arg == "-c" || arg == "--config") {
                if (i + 1 < argc) {
                    args.configPath = argv[++i];
                }
            } else if (arg == "-l" || arg == "--level") {
                if (i + 1 < argc) {
                    args.levelPath = argv[++i];
                }
            } else if (arg == "-m" || arg == "--multiplayer") {
                args.enableMultiplayer = true;
            } else if (arg == "-g" || arg == "--gps") {
                args.enableGPS = true;
            } else if (arg == "-e" || arg == "--editor") {
                args.startInEditor = true;
            }
        }

        return args;
    }

    static void PrintHelp() {
        std::cout << "Vehement2 - Zombie Survival Game\n";
        std::cout << "================================\n\n";
        std::cout << "Usage: vehement2 [options]\n\n";
        std::cout << "Options:\n";
        std::cout << "  -h, --help          Show this help message\n";
        std::cout << "  -c, --config PATH   Path to game configuration file\n";
        std::cout << "  -l, --level PATH    Path to level file to load\n";
        std::cout << "  -m, --multiplayer   Enable multiplayer mode\n";
        std::cout << "  -g, --gps           Enable GPS location tracking\n";
        std::cout << "  -e, --editor        Start in level editor mode\n";
        std::cout << "\n";
        std::cout << "Version: " << Vehement::Config::GameVersion << "\n";
        std::cout << "Engine:  " << Nova::Engine::GetName() << " " << Nova::Engine::GetVersion() << "\n";
    }
};

/**
 * @brief Main entry point for Vehement2 game
 */
int main(int argc, char* argv[]) {
    // Parse command line arguments
    auto args = CommandLineArgs::Parse(argc, argv);

    if (args.showHelp) {
        CommandLineArgs::PrintHelp();
        return EXIT_SUCCESS;
    }

    // Get engine instance
    auto& engine = Nova::Engine::Instance();

    // Initialize engine
    Nova::Engine::InitParams engineParams;
    engineParams.configPath = "config/engine.json";
    engineParams.enableImGui = true;
    engineParams.enableDebugDraw = true;

    if (!engine.Initialize(engineParams)) {
        std::cerr << "Failed to initialize Nova3D engine\n";
        return EXIT_FAILURE;
    }

    Nova::Logger::Info("[Vehement] {} v{} starting...",
        Vehement::Config::GameName,
        Vehement::Config::GameVersion);
    Nova::Logger::Info("[Vehement] Running on {} v{}",
        Nova::Engine::GetName(),
        Nova::Engine::GetVersion());

    // Create game instance
    auto game = std::make_unique<Vehement::Game>(engine);

    // Initialize game
    Vehement::GameInitParams gameParams;
    gameParams.configPath = args.configPath;
    gameParams.levelPath = args.levelPath;
    gameParams.enableMultiplayer = args.enableMultiplayer;
    gameParams.enableGPS = args.enableGPS;
    gameParams.startInEditor = args.startInEditor;

    auto initResult = game->Initialize(gameParams);
    if (!initResult) {
        Nova::Logger::Error("[Vehement] Failed to initialize game");
        return EXIT_FAILURE;
    }

    // Setup engine callbacks
    Nova::Engine::ApplicationCallbacks callbacks;

    callbacks.onStartup = []() -> bool {
        Nova::Logger::Info("[Vehement] Application startup");
        return true;
    };

    callbacks.onUpdate = [&game](float deltaTime) {
        game->Update(deltaTime);
    };

    callbacks.onRender = [&game]() {
        game->Render();
    };

    callbacks.onImGui = [&game]() {
        game->RenderImGui();
    };

    callbacks.onShutdown = [&game]() {
        Nova::Logger::Info("[Vehement] Application shutdown");
        game->Shutdown();
    };

    // Run the game
    Nova::Logger::Info("[Vehement] Entering main loop");
    int exitCode = engine.Run(std::move(callbacks));

    // Cleanup
    game.reset();

    Nova::Logger::Info("[Vehement] Exiting with code {}", exitCode);
    return exitCode;
}
