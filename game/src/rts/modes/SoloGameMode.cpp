#include "SoloGameMode.hpp"
#include "../../pcg/PCGContext.hpp"
#include <spdlog/spdlog.h>
#include <random>
#include <ctime>

namespace Vehement {

SoloGameMode::SoloGameMode()
    : m_world(std::make_unique<World>())
    , m_terrainGenerator(std::make_unique<TerrainGenerator>())
    , m_entitySpawner(std::make_unique<EntitySpawner>())
{
}

SoloGameMode::~SoloGameMode() {
    Shutdown();
}

bool SoloGameMode::Initialize(const SoloGameConfig& config) {
    if (m_initialized) {
        spdlog::warn("SoloGameMode already initialized");
        return true;
    }

    spdlog::info("Initializing Solo Game Mode");
    m_config = config;

    // Initialize random number generator
    if (m_config.seed == 0) {
        m_config.seed = static_cast<uint64_t>(std::time(nullptr));
    }
    m_rng.seed(static_cast<unsigned int>(m_config.seed));

    spdlog::info("Solo Game seed: {}", m_config.seed);

    m_initialized = true;
    return true;
}

bool SoloGameMode::GenerateMap(Nova::Renderer& renderer) {
    if (!m_initialized) {
        spdlog::error("SoloGameMode not initialized");
        return false;
    }

    if (m_mapGenerated) {
        spdlog::warn("Map already generated");
        return true;
    }

    spdlog::info("Generating 1v1 map: {}x{}", m_config.mapWidth, m_config.mapHeight);

    // Initialize world
    WorldConfig worldConfig;
    worldConfig.mapWidth = m_config.mapWidth;
    worldConfig.mapHeight = m_config.mapHeight;
    worldConfig.tileSize = m_config.tileSize;
    worldConfig.tileSizeXY = m_config.tileSize;
    worldConfig.useHexGrid = false;  // Use rectangular grid for RTS
    worldConfig.enableChunks = false;

    if (!m_world->Initialize(renderer, worldConfig)) {
        spdlog::error("Failed to initialize world");
        return false;
    }

    // Generate terrain
    if (m_config.generateTerrain) {
        GenerateTerrain();
    }

    // Setup player spawn points (must be before resource placement)
    SetupPlayerSpawns();

    // Place resources
    if (m_config.generateResources) {
        PlaceResources();
    }

    m_mapGenerated = true;
    spdlog::info("Map generation complete");
    return true;
}

void SoloGameMode::GenerateTerrain() {
    spdlog::info("Generating flat terrain with variation");

    auto& tileMap = m_world->GetTileMap();

    // Fill with grass base
    Tile grassTile = Tile::Ground(TileType::GroundGrass1);
    tileMap.Fill(grassTile);

    // Add some variation with dirt patches using noise
    std::uniform_real_distribution<float> noiseDist(0.0f, 1.0f);
    std::uniform_int_distribution<int> tileDist(0, 1);

    for (int y = 0; y < m_config.mapHeight; ++y) {
        for (int x = 0; x < m_config.mapWidth; ++x) {
            float noiseValue = noiseDist(m_rng);

            // 20% chance of dirt
            if (noiseValue < 0.20f) {
                Tile dirtTile = Tile::Ground(TileType::GroundDirt);
                tileMap.SetTile(x, y, dirtTile);
            }
            // 5% chance of alternate grass texture
            else if (noiseValue < 0.25f) {
                Tile grassTile2 = Tile::Ground(TileType::GroundGrass2);
                tileMap.SetTile(x, y, grassTile2);
            }
        }
    }

    spdlog::info("Terrain generation complete");
}

void SoloGameMode::SetupPlayerSpawns() {
    spdlog::info("Setting up player spawn points");

    // Calculate spawn positions on opposite sides of the map
    int centerX = m_config.mapWidth / 2;
    int centerY = m_config.mapHeight / 2;

    // Player 1 spawn (human) - bottom left area
    glm::vec3 player1Pos = glm::vec3(
        m_config.mapWidth * 0.2f * m_config.tileSize,
        0.0f,
        m_config.mapHeight * 0.2f * m_config.tileSize
    );

    // Player 2 spawn (AI) - top right area
    glm::vec3 player2Pos = glm::vec3(
        m_config.mapWidth * 0.8f * m_config.tileSize,
        0.0f,
        m_config.mapHeight * 0.8f * m_config.tileSize
    );

    // Verify minimum distance
    float distance = glm::distance(player1Pos, player2Pos);
    if (distance < m_config.minPlayerDistance) {
        spdlog::warn("Player spawn points too close: {} < {}", distance, m_config.minPlayerDistance);
    }

    // Setup player 1 spawn (Human)
    PlayerSpawn player1Spawn;
    player1Spawn.playerId = 0;
    player1Spawn.position = player1Pos;
    player1Spawn.radius = 5.0f;

    // Initialize starting resources
    player1Spawn.startingResources.Set(RTS::ResourceType::Food, m_config.startingFood);
    player1Spawn.startingResources.Set(RTS::ResourceType::Wood, m_config.startingWood);
    player1Spawn.startingResources.Set(RTS::ResourceType::Stone, m_config.startingStone);
    player1Spawn.startingResources.Set(RTS::ResourceType::Metal, m_config.startingMetal);
    player1Spawn.startingResources.Set(RTS::ResourceType::Coins, m_config.startingCoins);

    // Set reasonable capacities
    player1Spawn.startingResources.SetCapacity(RTS::ResourceType::Food, 1000);
    player1Spawn.startingResources.SetCapacity(RTS::ResourceType::Wood, 1000);
    player1Spawn.startingResources.SetCapacity(RTS::ResourceType::Stone, 1000);
    player1Spawn.startingResources.SetCapacity(RTS::ResourceType::Metal, 500);
    player1Spawn.startingResources.SetCapacity(RTS::ResourceType::Coins, 10000);

    // Add starting units and buildings
    player1Spawn.startingUnits.push_back("worker");
    player1Spawn.startingUnits.push_back("worker");
    player1Spawn.startingUnits.push_back("worker");  // 3 starting workers
    player1Spawn.startingBuildings.push_back("town_hall");

    m_playerSpawns.push_back(player1Spawn);

    // Setup player 2 spawn (AI)
    PlayerSpawn player2Spawn = player1Spawn;  // Copy config
    player2Spawn.playerId = 1;
    player2Spawn.position = player2Pos;

    m_playerSpawns.push_back(player2Spawn);

    // Add spawn points to world
    for (const auto& spawn : m_playerSpawns) {
        SpawnPoint worldSpawn;
        worldSpawn.position = spawn.position;
        worldSpawn.radius = spawn.radius;
        worldSpawn.tag = "player_" + std::to_string(spawn.playerId);
        worldSpawn.enabled = true;
        m_world->AddSpawnPoint(worldSpawn);
    }

    spdlog::info("Player 1 spawn: ({:.1f}, {:.1f})", player1Pos.x, player1Pos.z);
    spdlog::info("Player 2 spawn: ({:.1f}, {:.1f})", player2Pos.x, player2Pos.z);
}

void SoloGameMode::PlaceResources() {
    spdlog::info("Placing resources on map");

    int totalWalkableTiles = 0;
    auto& tileMap = m_world->GetTileMap();

    // Count walkable tiles
    for (int y = 0; y < m_config.mapHeight; ++y) {
        for (int x = 0; x < m_config.mapWidth; ++x) {
            if (tileMap.IsWalkable(x, y)) {
                totalWalkableTiles++;
            }
        }
    }

    // Calculate resource counts
    int treeCount = static_cast<int>(totalWalkableTiles * m_config.treeDensity);
    int rockCount = static_cast<int>(totalWalkableTiles * m_config.rockDensity);
    int goldCount = static_cast<int>(totalWalkableTiles * m_config.goldDensity);

    spdlog::info("Placing {} trees, {} rocks, {} gold deposits", treeCount, rockCount, goldCount);

    // Place starting resources near player spawns
    for (const auto& spawn : m_playerSpawns) {
        PlaceStartingResources(spawn.position);
    }

    // Place trees across the map
    for (int i = 0; i < treeCount; ++i) {
        glm::ivec2 pos = GetRandomWalkablePosition();
        if (IsValidResourcePosition(pos.x, pos.y)) {
            ResourceNode tree;
            tree.position = tileMap.TileToWorld(pos.x, pos.y);
            tree.type = RTS::ResourceType::Wood;
            tree.amount = 500;  // 500 wood per tree
            tree.gatherRate = 10.0f;  // 10 wood per second
            tree.entityType = "tree";
            m_resourceNodes.push_back(tree);

            // Mark tile as occupied with forest texture
            Tile forestTile = Tile::Ground(TileType::GroundForest1);
            tileMap.SetTile(pos.x, pos.y, forestTile);
        }
    }

    // Place rocks (stone)
    for (int i = 0; i < rockCount; ++i) {
        glm::ivec2 pos = GetRandomWalkablePosition();
        if (IsValidResourcePosition(pos.x, pos.y)) {
            ResourceNode rock;
            rock.position = tileMap.TileToWorld(pos.x, pos.y);
            rock.type = RTS::ResourceType::Stone;
            rock.amount = 400;  // 400 stone per rock
            rock.gatherRate = 8.0f;  // 8 stone per second
            rock.entityType = "rock";
            m_resourceNodes.push_back(rock);
        }
    }

    // Place gold deposits (metal)
    for (int i = 0; i < goldCount; ++i) {
        glm::ivec2 pos = GetRandomWalkablePosition();
        if (IsValidResourcePosition(pos.x, pos.y)) {
            ResourceNode gold;
            gold.position = tileMap.TileToWorld(pos.x, pos.y);
            gold.type = RTS::ResourceType::Metal;
            gold.amount = 1000;  // 1000 metal per deposit
            gold.gatherRate = 15.0f;  // 15 metal per second
            gold.entityType = "gold_deposit";
            m_resourceNodes.push_back(gold);
        }
    }

    spdlog::info("Placed {} total resource nodes", m_resourceNodes.size());
}

void SoloGameMode::PlaceStartingResources(const glm::vec3& spawnPos) {
    spdlog::info("Placing starting resources near spawn ({:.1f}, {:.1f})", spawnPos.x, spawnPos.z);

    // Place resource clusters near spawn
    // Trees cluster
    PlaceResourceCluster(spawnPos, RTS::ResourceType::Wood, 8, 15.0f);

    // Rocks cluster
    PlaceResourceCluster(spawnPos, RTS::ResourceType::Stone, 5, 12.0f);

    // Gold deposit (1-2 near spawn)
    PlaceResourceCluster(spawnPos, RTS::ResourceType::Metal, 2, 20.0f);
}

void SoloGameMode::PlaceResourceCluster(const glm::vec3& center, RTS::ResourceType type,
                                         int count, float radius) {
    auto& tileMap = m_world->GetTileMap();
    glm::ivec2 centerTile = tileMap.WorldToTile(center);

    int radiusTiles = static_cast<int>(radius / m_config.tileSize);
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
    std::uniform_real_distribution<float> radiusDist(radius * 0.3f, radius);

    for (int i = 0; i < count; ++i) {
        float angle = angleDist(m_rng);
        float dist = radiusDist(m_rng);

        int offsetX = static_cast<int>(std::cos(angle) * dist / m_config.tileSize);
        int offsetY = static_cast<int>(std::sin(angle) * dist / m_config.tileSize);

        int tileX = centerTile.x + offsetX;
        int tileY = centerTile.y + offsetY;

        if (tileMap.IsInBounds(tileX, tileY) && IsValidResourcePosition(tileX, tileY)) {
            ResourceNode node;
            node.position = tileMap.TileToWorld(tileX, tileY);
            node.type = type;

            // Set properties based on type
            switch (type) {
                case RTS::ResourceType::Wood:
                    node.amount = 500;
                    node.gatherRate = 10.0f;
                    node.entityType = "tree";
                    break;
                case RTS::ResourceType::Stone:
                    node.amount = 400;
                    node.gatherRate = 8.0f;
                    node.entityType = "rock";
                    break;
                case RTS::ResourceType::Metal:
                    node.amount = 1000;
                    node.gatherRate = 15.0f;
                    node.entityType = "gold_deposit";
                    break;
                default:
                    node.amount = 100;
                    node.gatherRate = 5.0f;
                    node.entityType = "resource";
                    break;
            }

            m_resourceNodes.push_back(node);
        }
    }
}

bool SoloGameMode::IsValidResourcePosition(int x, int y) const {
    auto& tileMap = m_world->GetTileMap();

    // Check if in bounds and walkable
    if (!tileMap.IsInBounds(x, y) || !tileMap.IsWalkable(x, y)) {
        return false;
    }

    // Check minimum distance from player spawns
    glm::vec3 pos = tileMap.TileToWorld(x, y);
    float minSpawnDist = 8.0f * m_config.tileSize;  // 8 tiles minimum

    for (const auto& spawn : m_playerSpawns) {
        float dist = glm::distance(glm::vec2(pos.x, pos.z), glm::vec2(spawn.position.x, spawn.position.z));
        if (dist < minSpawnDist) {
            return false;  // Too close to spawn
        }
    }

    // Check minimum distance from other resources
    float minResourceDist = 2.0f * m_config.tileSize;  // 2 tiles minimum

    for (const auto& resource : m_resourceNodes) {
        float dist = glm::distance(glm::vec2(pos.x, pos.z), glm::vec2(resource.position.x, resource.position.z));
        if (dist < minResourceDist) {
            return false;  // Too close to another resource
        }
    }

    return true;
}

glm::ivec2 SoloGameMode::GetRandomWalkablePosition() const {
    auto& tileMap = m_world->GetTileMap();
    std::uniform_int_distribution<int> xDist(0, m_config.mapWidth - 1);
    std::uniform_int_distribution<int> yDist(0, m_config.mapHeight - 1);

    // Try up to 100 times to find a walkable position
    for (int attempt = 0; attempt < 100; ++attempt) {
        int x = xDist(m_rng);
        int y = yDist(m_rng);

        if (tileMap.IsWalkable(x, y)) {
            return glm::ivec2(x, y);
        }
    }

    // Fallback to center if no walkable position found
    return glm::ivec2(m_config.mapWidth / 2, m_config.mapHeight / 2);
}

float SoloGameMode::TileDistance(int x1, int y1, int x2, int y2) const {
    int dx = x2 - x1;
    int dy = y2 - y1;
    return std::sqrt(static_cast<float>(dx * dx + dy * dy));
}

void SoloGameMode::Update(float deltaTime) {
    if (!m_initialized || !m_mapGenerated) {
        return;
    }

    m_gameTime += deltaTime;

    // Update world (handles entity movement, collisions, spawns)
    m_world->Update(deltaTime);

    // Update resource node respawning
    for (auto& node : m_resourceNodes) {
        if (node.amount <= 0) {
            // Resource depleted - could implement respawn timer here
            // For now, depleted resources stay depleted
        }
    }

    // Entity AI and game logic is handled by the World's entity update callback
    // and the individual entity Update() methods called from World::UpdateEntities()
}

void SoloGameMode::Render(const Nova::Camera& camera) {
    if (!m_initialized || !m_mapGenerated) {
        return;
    }

    // Render world (tile map and terrain)
    m_world->Render(camera);

    // Resource nodes are rendered as tile textures (forest/rock tiles)
    // set during PlaceResources(). Additional 3D models or sprites for
    // individual trees/rocks would be rendered by the entity system.

    // Units and buildings are managed as Entity objects in the World.
    // They are rendered via World::GetEntities() which returns all
    // active entities. The caller with access to a Nova::Renderer
    // should iterate and call Entity::Render() for each.
    // This is typically done at the Game/Application level where both
    // the Camera and Renderer are available.
}

glm::vec3 SoloGameMode::GetPlayerSpawnPosition(int playerId) const {
    for (const auto& spawn : m_playerSpawns) {
        if (spawn.playerId == playerId) {
            return spawn.position;
        }
    }
    return glm::vec3(0.0f);
}

void SoloGameMode::Shutdown() {
    if (!m_initialized) {
        return;
    }

    spdlog::info("Shutting down Solo Game Mode");

    if (m_world) {
        m_world->Shutdown();
    }

    m_playerSpawns.clear();
    m_resourceNodes.clear();
    m_mapGenerated = false;
    m_initialized = false;
}

} // namespace Vehement
