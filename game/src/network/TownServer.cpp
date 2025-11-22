#include "TownServer.hpp"
#include <random>
#include <ctime>
#include <algorithm>

// Include engine logger if available
#if __has_include("../../engine/core/Logger.hpp")
#include "../../engine/core/Logger.hpp"
#define TOWN_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define TOWN_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#define TOWN_LOG_ERROR(...) NOVA_LOG_ERROR(__VA_ARGS__)
#else
#include <iostream>
#define TOWN_LOG_INFO(...) std::cout << "[TownServer] " << __VA_ARGS__ << std::endl
#define TOWN_LOG_WARN(...) std::cerr << "[TownServer WARN] " << __VA_ARGS__ << std::endl
#define TOWN_LOG_ERROR(...) std::cerr << "[TownServer ERROR] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

// ==================== Tile ====================

nlohmann::json Tile::ToJson() const {
    return {
        {"type", static_cast<int>(type)},
        {"variant", variant},
        {"elevation", elevation},
        {"metadata", metadata},
        {"passable", passable},
        {"zombieCleared", zombieCleared}
    };
}

Tile Tile::FromJson(const nlohmann::json& j) {
    Tile tile;
    tile.type = static_cast<TileType>(j.value("type", 0));
    tile.variant = j.value("variant", 0);
    tile.elevation = j.value("elevation", 0);
    tile.metadata = j.value("metadata", 0);
    tile.passable = j.value("passable", true);
    tile.zombieCleared = j.value("zombieCleared", false);
    return tile;
}

// ==================== TileMap ====================

TileMap::TileMap(int width, int height)
    : m_width(width), m_height(height) {
    m_tiles.resize(static_cast<size_t>(width * height));
}

const Tile& TileMap::GetTile(int x, int y) const {
    if (!InBounds(x, y)) {
        return m_emptyTile;
    }
    return m_tiles[static_cast<size_t>(y * m_width + x)];
}

void TileMap::SetTile(int x, int y, const Tile& tile) {
    if (!InBounds(x, y)) {
        return;
    }
    size_t index = static_cast<size_t>(y * m_width + x);
    m_tiles[index] = tile;
    m_dirtyTiles.emplace_back(x, y);
}

bool TileMap::InBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

void TileMap::Resize(int width, int height) {
    m_width = width;
    m_height = height;
    m_tiles.clear();
    m_tiles.resize(static_cast<size_t>(width * height));
    m_dirtyTiles.clear();
}

void TileMap::Clear() {
    std::fill(m_tiles.begin(), m_tiles.end(), Tile{});
    m_dirtyTiles.clear();
}

nlohmann::json TileMap::ToJson() const {
    nlohmann::json j;
    j["width"] = m_width;
    j["height"] = m_height;

    // Compress tile data - store as array of arrays
    nlohmann::json tilesJson = nlohmann::json::array();

    for (int y = 0; y < m_height; ++y) {
        nlohmann::json row = nlohmann::json::array();
        for (int x = 0; x < m_width; ++x) {
            const Tile& tile = GetTile(x, y);
            // Compact format: [type, variant, elevation, metadata, flags]
            // flags: bit 0 = passable, bit 1 = zombieCleared
            uint8_t flags = (tile.passable ? 1 : 0) | (tile.zombieCleared ? 2 : 0);
            row.push_back({
                static_cast<int>(tile.type),
                tile.variant,
                tile.elevation,
                tile.metadata,
                flags
            });
        }
        tilesJson.push_back(row);
    }

    j["tiles"] = tilesJson;
    return j;
}

TileMap TileMap::FromJson(const nlohmann::json& j) {
    int width = j.value("width", DEFAULT_WIDTH);
    int height = j.value("height", DEFAULT_HEIGHT);

    TileMap map(width, height);

    if (j.contains("tiles") && j["tiles"].is_array()) {
        const auto& tilesJson = j["tiles"];
        for (int y = 0; y < height && y < static_cast<int>(tilesJson.size()); ++y) {
            const auto& row = tilesJson[y];
            for (int x = 0; x < width && x < static_cast<int>(row.size()); ++x) {
                const auto& tileData = row[x];
                if (tileData.is_array() && tileData.size() >= 5) {
                    Tile tile;
                    tile.type = static_cast<TileType>(tileData[0].get<int>());
                    tile.variant = tileData[1].get<uint8_t>();
                    tile.elevation = tileData[2].get<uint8_t>();
                    tile.metadata = tileData[3].get<uint8_t>();
                    uint8_t flags = tileData[4].get<uint8_t>();
                    tile.passable = (flags & 1) != 0;
                    tile.zombieCleared = (flags & 2) != 0;
                    map.m_tiles[static_cast<size_t>(y * width + x)] = tile;
                }
            }
        }
    }

    return map;
}

std::vector<std::pair<int, int>> TileMap::GetDirtyTiles() const {
    return m_dirtyTiles;
}

void TileMap::ClearDirtyFlags() {
    m_dirtyTiles.clear();
}

// ==================== TownEntity ====================

nlohmann::json TownEntity::ToJson() const {
    return {
        {"id", id},
        {"type", type},
        {"x", x},
        {"y", y},
        {"z", z},
        {"rotation", rotation},
        {"health", health},
        {"active", active},
        {"customData", customData}
    };
}

TownEntity TownEntity::FromJson(const nlohmann::json& j) {
    TownEntity entity;
    entity.id = j.value("id", "");
    entity.type = j.value("type", "");
    entity.x = j.value("x", 0.0f);
    entity.y = j.value("y", 0.0f);
    entity.z = j.value("z", 0.0f);
    entity.rotation = j.value("rotation", 0.0f);
    entity.health = j.value("health", 100);
    entity.active = j.value("active", true);
    if (j.contains("customData")) {
        entity.customData = j["customData"];
    }
    return entity;
}

// ==================== TownServer ====================

TownServer& TownServer::Instance() {
    static TownServer instance;
    return instance;
}

bool TownServer::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Ensure FirebaseManager is ready
    if (!FirebaseManager::Instance().IsInitialized()) {
        TOWN_LOG_WARN("FirebaseManager not initialized, TownServer may not sync properly");
    }

    m_initialized = true;
    TOWN_LOG_INFO("TownServer initialized");
    return true;
}

void TownServer::Shutdown() {
    if (!m_initialized) {
        return;
    }

    DisconnectFromTown();
    m_initialized = false;
    TOWN_LOG_INFO("TownServer shutdown");
}

void TownServer::ConnectToTown(const TownInfo& town, ConnectionCallback callback) {
    if (!town.IsValid()) {
        TOWN_LOG_ERROR("Invalid town info provided");
        if (callback) {
            callback(false);
        }
        return;
    }

    ConnectToTownById(town.townId, [this, town, callback](bool success) {
        if (success) {
            m_currentTown = town;
        }
        if (callback) {
            callback(success);
        }
    });
}

void TownServer::ConnectToTownById(const std::string& townId, ConnectionCallback callback) {
    if (townId.empty()) {
        TOWN_LOG_ERROR("Empty town ID provided");
        if (callback) {
            callback(false);
        }
        return;
    }

    // Disconnect from current town first
    if (m_status != ConnectionStatus::Disconnected) {
        DisconnectFromTown();
    }

    m_currentTown.townId = townId;
    NotifyStatusChanged(ConnectionStatus::Connecting);
    TOWN_LOG_INFO("Connecting to town: " + townId);

    // Check if town exists
    TownExists(townId, [this, townId, callback](bool exists) {
        if (exists) {
            // Load existing town data
            LoadTownData(callback);
        } else {
            // Generate new town
            TOWN_LOG_INFO("Town does not exist, generating new town: " + townId);
            GenerateNewTown(m_currentTown, -1);

            // Save generated town
            SaveMapChanges();

            NotifyStatusChanged(ConnectionStatus::Connected);
            if (callback) {
                callback(true);
            }
        }
    });
}

void TownServer::DisconnectFromTown() {
    if (m_status == ConnectionStatus::Disconnected) {
        return;
    }

    TOWN_LOG_INFO("Disconnecting from town: " + m_currentTown.townId);

    StopRealtimeSync();
    RemoveListeners();

    m_currentTown = TownInfo{};
    m_townMap.Clear();

    {
        std::lock_guard<std::mutex> lock(m_entityMutex);
        m_entities.clear();
    }

    NotifyStatusChanged(ConnectionStatus::Disconnected);
}

// ==================== Map Operations ====================

void TownServer::SaveMapChanges() {
    if (m_currentTown.townId.empty()) {
        return;
    }

    auto& firebase = FirebaseManager::Instance();

    // Save entire map (for full sync)
    nlohmann::json mapData = m_townMap.ToJson();
    firebase.SetValue(GetMapPath(), mapData, [](const FirebaseManager::Result& result) {
        if (!result.success) {
            TOWN_LOG_ERROR("Failed to save map: " + result.errorMessage);
        }
    });

    m_townMap.ClearDirtyFlags();
    TOWN_LOG_INFO("Map changes saved to Firebase");
}

void TownServer::SaveTileChange(int x, int y, const Tile& tile) {
    if (m_currentTown.townId.empty()) {
        return;
    }

    m_townMap.SetTile(x, y, tile);

    // Save individual tile update
    auto& firebase = FirebaseManager::Instance();
    std::string tilePath = GetMapPath() + "/changes/" + std::to_string(x) + "_" + std::to_string(y);

    nlohmann::json tileData = {
        {"x", x},
        {"y", y},
        {"tile", tile.ToJson()},
        {"timestamp", std::time(nullptr)},
        {"changedBy", firebase.GetUserId()}
    };

    firebase.SetValue(tilePath, tileData);

    // Notify local callbacks
    MapChangeEvent event{x, y, m_townMap.GetTile(x, y), tile, firebase.GetUserId()};
    for (const auto& cb : m_mapChangeCallbacks) {
        if (cb) {
            cb(event);
        }
    }
}

void TownServer::OnMapChanged(MapChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_mapChangeCallbacks.push_back(std::move(callback));
}

// ==================== Town Generation ====================

void TownServer::GenerateNewTown(const TownInfo& town, int seed) {
    m_currentTown = town;
    GenerateTownProcedurally(seed);
    TOWN_LOG_INFO("Generated new town: " + town.townId);
}

void TownServer::TownExists(const std::string& townId, std::function<void(bool)> callback) {
    auto& firebase = FirebaseManager::Instance();
    std::string path = "towns/" + townId + "/metadata";

    firebase.GetValue(path, [callback](const nlohmann::json& data) {
        bool exists = !data.is_null() && data.contains("townId");
        if (callback) {
            callback(exists);
        }
    });
}

void TownServer::GenerateTownProcedurally(int seed) {
    if (seed < 0) {
        std::random_device rd;
        seed = static_cast<int>(rd());
    }

    std::mt19937 rng(static_cast<unsigned int>(seed));

    // Initialize map
    m_townMap.Resize(TileMap::DEFAULT_WIDTH, TileMap::DEFAULT_HEIGHT);

    // Distribution helpers
    std::uniform_int_distribution<int> tileDist(0, 100);
    std::uniform_int_distribution<int> variantDist(0, 3);

    int width = m_townMap.GetWidth();
    int height = m_townMap.GetHeight();

    // Generate basic terrain
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Tile tile;

            int roll = tileDist(rng);

            // Roads form a grid pattern
            bool isRoadX = (x % 16 == 0 || x % 16 == 1);
            bool isRoadY = (y % 16 == 0 || y % 16 == 1);

            if (isRoadX || isRoadY) {
                tile.type = TileType::Road;
                tile.passable = true;
            }
            // Buildings near roads
            else if ((isRoadX || isRoadY || (x % 16 > 2 && x % 16 < 14 && y % 16 > 2 && y % 16 < 14)) && roll < 30) {
                tile.type = TileType::Building;
                tile.passable = false;
            }
            // Trees scattered
            else if (roll < 40) {
                tile.type = TileType::Tree;
                tile.passable = false;
            }
            // Water (rare)
            else if (roll < 42) {
                tile.type = TileType::Water;
                tile.passable = false;
            }
            // Ground (default)
            else {
                tile.type = TileType::Ground;
                tile.passable = true;
            }

            tile.variant = static_cast<uint8_t>(variantDist(rng));
            m_townMap.SetTile(x, y, tile);
        }
    }

    // Add zombie spawn points
    std::uniform_int_distribution<int> spawnDist(10, width - 10);
    for (int i = 0; i < 20; ++i) {
        int x = spawnDist(rng);
        int y = spawnDist(rng);
        Tile tile = m_townMap.GetTile(x, y);
        if (tile.passable) {
            tile.type = TileType::ZombieSpawn;
            m_townMap.SetTile(x, y, tile);
        }
    }

    // Add safe zones (corners)
    for (int sy = 0; sy < 8; ++sy) {
        for (int sx = 0; sx < 8; ++sx) {
            Tile tile;
            tile.type = TileType::SafeZone;
            tile.passable = true;
            tile.zombieCleared = true;
            m_townMap.SetTile(sx, sy, tile);
            m_townMap.SetTile(width - 1 - sx, sy, tile);
            m_townMap.SetTile(sx, height - 1 - sy, tile);
            m_townMap.SetTile(width - 1 - sx, height - 1 - sy, tile);
        }
    }

    m_townMap.ClearDirtyFlags();
    TOWN_LOG_INFO("Procedural town generated with seed: " + std::to_string(seed));
}

// ==================== Entity Management ====================

std::string TownServer::SpawnEntity(const TownEntity& entity) {
    TownEntity newEntity = entity;

    if (newEntity.id.empty()) {
        // Generate unique ID
        auto& firebase = FirebaseManager::Instance();
        newEntity.id = firebase.PushValue(GetEntitiesPath(), newEntity.ToJson());
    } else {
        // Use provided ID
        auto& firebase = FirebaseManager::Instance();
        firebase.SetValue(GetEntitiesPath() + "/" + newEntity.id, newEntity.ToJson());
    }

    {
        std::lock_guard<std::mutex> lock(m_entityMutex);
        m_entities[newEntity.id] = newEntity;
    }

    // Notify callbacks
    for (const auto& cb : m_entitySpawnedCallbacks) {
        if (cb) {
            cb(newEntity);
        }
    }

    TOWN_LOG_INFO("Entity spawned: " + newEntity.id + " (" + newEntity.type + ")");
    return newEntity.id;
}

void TownServer::UpdateEntity(const TownEntity& entity) {
    if (entity.id.empty()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_entityMutex);
        m_entities[entity.id] = entity;
    }

    auto& firebase = FirebaseManager::Instance();
    firebase.SetValue(GetEntitiesPath() + "/" + entity.id, entity.ToJson());
}

void TownServer::RemoveEntity(const std::string& entityId) {
    if (entityId.empty()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_entityMutex);
        m_entities.erase(entityId);
    }

    auto& firebase = FirebaseManager::Instance();
    firebase.DeleteValue(GetEntitiesPath() + "/" + entityId);

    // Notify callbacks
    for (const auto& cb : m_entityRemovedCallbacks) {
        if (cb) {
            cb(entityId);
        }
    }

    TOWN_LOG_INFO("Entity removed: " + entityId);
}

std::vector<TownEntity> TownServer::GetEntities() const {
    std::lock_guard<std::mutex> lock(m_entityMutex);
    std::vector<TownEntity> result;
    result.reserve(m_entities.size());
    for (const auto& [id, entity] : m_entities) {
        result.push_back(entity);
    }
    return result;
}

std::optional<TownEntity> TownServer::GetEntity(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_entityMutex);
    auto it = m_entities.find(id);
    if (it != m_entities.end()) {
        return it->second;
    }
    return std::nullopt;
}

void TownServer::OnEntitySpawned(EntityCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_entitySpawnedCallbacks.push_back(std::move(callback));
}

void TownServer::OnEntityRemoved(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_entityRemovedCallbacks.push_back(std::move(callback));
}

// ==================== Real-time Sync ====================

void TownServer::StartRealtimeSync() {
    if (m_realtimeSyncActive || m_currentTown.townId.empty()) {
        return;
    }

    SetupListeners();
    m_realtimeSyncActive = true;
    TOWN_LOG_INFO("Real-time sync started for town: " + m_currentTown.townId);
}

void TownServer::StopRealtimeSync() {
    if (!m_realtimeSyncActive) {
        return;
    }

    RemoveListeners();
    m_realtimeSyncActive = false;
    TOWN_LOG_INFO("Real-time sync stopped");
}

void TownServer::Update(float deltaTime) {
    if (!m_realtimeSyncActive || m_status != ConnectionStatus::Connected) {
        return;
    }

    m_syncTimer += deltaTime;
    if (m_syncTimer >= SYNC_INTERVAL) {
        m_syncTimer = 0.0f;

        // Save any dirty map changes
        if (m_townMap.IsDirty()) {
            SaveMapChanges();
        }
    }

    // Process Firebase updates
    FirebaseManager::Instance().Update();
}

void TownServer::OnStatusChanged(StatusCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_statusCallbacks.push_back(std::move(callback));
}

// ==================== Private Helpers ====================

std::string TownServer::GetTownPath() const {
    return "towns/" + m_currentTown.townId;
}

std::string TownServer::GetMapPath() const {
    return GetTownPath() + "/map";
}

std::string TownServer::GetEntitiesPath() const {
    return GetTownPath() + "/entities";
}

std::string TownServer::GetPlayersPath() const {
    return GetTownPath() + "/players";
}

void TownServer::LoadTownData(ConnectionCallback callback) {
    auto& firebase = FirebaseManager::Instance();

    NotifyStatusChanged(ConnectionStatus::Syncing);

    // Load map data
    firebase.GetValue(GetMapPath(), [this, callback](const nlohmann::json& data) {
        if (!data.is_null()) {
            m_townMap = TileMap::FromJson(data);
            TOWN_LOG_INFO("Town map loaded: " + std::to_string(m_townMap.GetWidth()) + "x" +
                         std::to_string(m_townMap.GetHeight()));
        } else {
            // No map data, generate new
            GenerateTownProcedurally(-1);
            SaveMapChanges();
        }

        // Load entities
        FirebaseManager::Instance().GetValue(GetEntitiesPath(), [this, callback](const nlohmann::json& entityData) {
            if (entityData.is_object()) {
                std::lock_guard<std::mutex> lock(m_entityMutex);
                for (auto& [id, data] : entityData.items()) {
                    m_entities[id] = TownEntity::FromJson(data);
                }
                TOWN_LOG_INFO("Loaded " + std::to_string(m_entities.size()) + " entities");
            }

            NotifyStatusChanged(ConnectionStatus::Connected);
            if (callback) {
                callback(true);
            }
        });
    });
}

void TownServer::SetupListeners() {
    auto& firebase = FirebaseManager::Instance();

    // Listen for map changes
    m_mapListenerId = firebase.ListenToPath(GetMapPath() + "/changes",
        [this](const nlohmann::json& data) {
            HandleMapUpdate(data);
        });

    // Listen for entity changes
    m_entitiesListenerId = firebase.ListenToPath(GetEntitiesPath(),
        [this](const nlohmann::json& data) {
            HandleEntityUpdate(data);
        });
}

void TownServer::RemoveListeners() {
    auto& firebase = FirebaseManager::Instance();

    if (!m_mapListenerId.empty()) {
        firebase.StopListeningById(m_mapListenerId);
        m_mapListenerId.clear();
    }

    if (!m_entitiesListenerId.empty()) {
        firebase.StopListeningById(m_entitiesListenerId);
        m_entitiesListenerId.clear();
    }
}

void TownServer::HandleMapUpdate(const nlohmann::json& data) {
    if (data.is_null()) {
        return;
    }

    // Process tile changes
    for (auto& [key, change] : data.items()) {
        if (change.contains("x") && change.contains("y") && change.contains("tile")) {
            int x = change["x"].get<int>();
            int y = change["y"].get<int>();
            Tile oldTile = m_townMap.GetTile(x, y);
            Tile newTile = Tile::FromJson(change["tile"]);

            // Don't process our own changes
            std::string changedBy = change.value("changedBy", "");
            if (changedBy == FirebaseManager::Instance().GetUserId()) {
                continue;
            }

            m_townMap.SetTile(x, y, newTile);

            // Notify callbacks
            MapChangeEvent event{x, y, oldTile, newTile, changedBy};
            for (const auto& cb : m_mapChangeCallbacks) {
                if (cb) {
                    cb(event);
                }
            }
        }
    }
}

void TownServer::HandleEntityUpdate(const nlohmann::json& data) {
    if (!data.is_object()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_entityMutex);

    // Find removed entities
    std::vector<std::string> removedIds;
    for (const auto& [id, entity] : m_entities) {
        if (!data.contains(id)) {
            removedIds.push_back(id);
        }
    }

    // Remove deleted entities
    for (const auto& id : removedIds) {
        m_entities.erase(id);
        for (const auto& cb : m_entityRemovedCallbacks) {
            if (cb) {
                cb(id);
            }
        }
    }

    // Update/add entities
    for (auto& [id, entityData] : data.items()) {
        TownEntity entity = TownEntity::FromJson(entityData);
        bool isNew = m_entities.find(id) == m_entities.end();
        m_entities[id] = entity;

        if (isNew) {
            for (const auto& cb : m_entitySpawnedCallbacks) {
                if (cb) {
                    cb(entity);
                }
            }
        }
    }
}

void TownServer::NotifyStatusChanged(ConnectionStatus status) {
    m_status = status;

    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& cb : m_statusCallbacks) {
        if (cb) {
            cb(status);
        }
    }
}

} // namespace Vehement
