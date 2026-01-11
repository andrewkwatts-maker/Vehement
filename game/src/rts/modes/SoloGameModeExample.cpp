/**
 * @file SoloGameModeExample.cpp
 * @brief Example usage of SoloGameMode for testing
 *
 * This file demonstrates how to use the SoloGameMode class
 * independently for testing purposes.
 */

#include "SoloGameMode.hpp"
#include "core/Engine.hpp"
#include <spdlog/spdlog.h>

namespace Vehement {

/**
 * @brief Example function showing basic SoloGameMode usage
 */
void ExampleSoloGameUsage() {
    // Create solo game mode instance
    auto soloGame = std::make_unique<SoloGameMode>();

    // Configure the game
    SoloGameConfig config;
    config.mapWidth = 128;
    config.mapHeight = 128;
    config.tileSize = 1.0f;
    config.seed = 12345;  // Use specific seed for reproducible maps

    // Resource density (percentage of tiles)
    config.treeDensity = 0.15f;   // 15% trees
    config.rockDensity = 0.08f;   // 8% rocks
    config.goldDensity = 0.03f;   // 3% gold (rare)

    // Starting resources per player
    config.startingFood = 200;
    config.startingWood = 150;
    config.startingStone = 100;
    config.startingMetal = 50;

    // Initialize the game mode
    if (!soloGame->Initialize(config)) {
        spdlog::error("Failed to initialize solo game");
        return;
    }

    // Generate the map (requires renderer)
    auto& renderer = Nova::Engine::Instance().GetRenderer();
    if (!soloGame->GenerateMap(renderer)) {
        spdlog::error("Failed to generate map");
        return;
    }

    spdlog::info("Solo game map generated successfully!");

    // Access game data
    const auto& playerSpawns = soloGame->GetPlayerSpawns();
    const auto& resourceNodes = soloGame->GetResourceNodes();

    spdlog::info("Player spawns: {}", playerSpawns.size());
    spdlog::info("Resource nodes: {}", resourceNodes.size());

    // Get player spawn positions
    for (const auto& spawn : playerSpawns) {
        glm::vec3 pos = spawn.position;
        spdlog::info("Player {} spawn: ({:.1f}, {:.1f})", spawn.playerId, pos.x, pos.z);

        // Show starting resources
        spdlog::info("  Food: {}", spawn.startingResources.GetAmount(RTS::ResourceType::Food));
        spdlog::info("  Wood: {}", spawn.startingResources.GetAmount(RTS::ResourceType::Wood));
        spdlog::info("  Stone: {}", spawn.startingResources.GetAmount(RTS::ResourceType::Stone));
        spdlog::info("  Metal: {}", spawn.startingResources.GetAmount(RTS::ResourceType::Metal));
    }

    // Count resources by type
    int treeCount = 0;
    int rockCount = 0;
    int goldCount = 0;

    for (const auto& resource : resourceNodes) {
        switch (resource.type) {
            case RTS::ResourceType::Wood:
                treeCount++;
                break;
            case RTS::ResourceType::Stone:
                rockCount++;
                break;
            case RTS::ResourceType::Metal:
                goldCount++;
                break;
            default:
                break;
        }
    }

    spdlog::info("Resource distribution:");
    spdlog::info("  Trees: {}", treeCount);
    spdlog::info("  Rocks: {}", rockCount);
    spdlog::info("  Gold deposits: {}", goldCount);

    // Cleanup
    soloGame->Shutdown();
}

/**
 * @brief Example of customizing resource placement
 */
void ExampleCustomResourcePlacement() {
    auto soloGame = std::make_unique<SoloGameMode>();

    SoloGameConfig config;
    config.mapWidth = 256;  // Larger map
    config.mapHeight = 256;

    // High resource density for testing
    config.treeDensity = 0.25f;   // 25% trees - abundant
    config.rockDensity = 0.15f;   // 15% rocks
    config.goldDensity = 0.05f;   // 5% gold - more common

    // Rich starting resources
    config.startingFood = 500;
    config.startingWood = 300;
    config.startingStone = 200;
    config.startingMetal = 100;

    if (!soloGame->Initialize(config)) {
        spdlog::error("Failed to initialize custom game");
        return;
    }

    auto& renderer = Nova::Engine::Instance().GetRenderer();
    if (!soloGame->GenerateMap(renderer)) {
        spdlog::error("Failed to generate custom map");
        return;
    }

    spdlog::info("Custom resource-rich map created!");
    spdlog::info("Total resources: {}", soloGame->GetResourceNodes().size());

    soloGame->Shutdown();
}

/**
 * @brief Example of accessing world data
 */
void ExampleWorldAccess() {
    auto soloGame = std::make_unique<SoloGameMode>();

    SoloGameConfig config;
    if (!soloGame->Initialize(config)) {
        return;
    }

    auto& renderer = Nova::Engine::Instance().GetRenderer();
    if (!soloGame->GenerateMap(renderer)) {
        return;
    }

    // Access the underlying world
    World& world = soloGame->GetWorld();

    // Get tile map
    TileMap& tileMap = world.GetTileMap();
    spdlog::info("Map size: {}x{}", tileMap.GetWidth(), tileMap.GetHeight());

    // Check specific tiles
    if (auto* tile = tileMap.GetTile(64, 64)) {
        spdlog::info("Center tile is walkable: {}", tile->isWalkable);
        spdlog::info("Center tile type: {}", GetTileTypeName(tile->type));
    }

    // Get spawn points from world
    auto spawnPoints = world.GetSpawnPoints("player_0");
    if (!spawnPoints.empty()) {
        spdlog::info("Player 0 spawn found at: ({:.1f}, {:.1f})",
            spawnPoints[0]->position.x,
            spawnPoints[0]->position.z);
    }

    soloGame->Shutdown();
}

} // namespace Vehement

// If building as standalone test
#ifdef SOLO_GAME_MODE_TEST
int main() {
    // Initialize logging
    spdlog::set_level(spdlog::level::info);

    spdlog::info("=== Solo Game Mode Examples ===");

    spdlog::info("\n--- Basic Usage ---");
    Vehement::ExampleSoloGameUsage();

    spdlog::info("\n--- Custom Resources ---");
    Vehement::ExampleCustomResourcePlacement();

    spdlog::info("\n--- World Access ---");
    Vehement::ExampleWorldAccess();

    spdlog::info("\n=== Examples Complete ===");

    return 0;
}
#endif
