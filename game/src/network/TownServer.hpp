#pragma once

#include "GPSLocation.hpp"
#include "FirebaseManager.hpp"
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <nlohmann/json.hpp>

namespace Vehement {

/**
 * @brief Tile types for the town map
 */
enum class TileType : uint8_t {
    Empty = 0,
    Ground = 1,
    Road = 2,
    Building = 3,
    Water = 4,
    Tree = 5,
    ZombieSpawn = 6,
    SafeZone = 7,
    Barrier = 8,
    Custom = 255
};

/**
 * @brief Single tile in the town map
 */
struct Tile {
    TileType type = TileType::Empty;
    uint8_t variant = 0;          ///< Visual variant for same type
    uint8_t elevation = 0;        ///< Height level
    uint8_t metadata = 0;         ///< Custom data
    bool passable = true;         ///< Can entities walk through
    bool zombieCleared = false;   ///< Has this tile been cleared of zombies

    /**
     * @brief Serialize tile to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize tile from JSON
     */
    static Tile FromJson(const nlohmann::json& j);
};

/**
 * @brief 2D tile map for a town
 */
class TileMap {
public:
    static constexpr int DEFAULT_WIDTH = 256;
    static constexpr int DEFAULT_HEIGHT = 256;

    TileMap() = default;
    TileMap(int width, int height);

    /**
     * @brief Get tile at coordinates
     * @return Tile reference (returns empty tile if out of bounds)
     */
    [[nodiscard]] const Tile& GetTile(int x, int y) const;

    /**
     * @brief Set tile at coordinates
     */
    void SetTile(int x, int y, const Tile& tile);

    /**
     * @brief Get map dimensions
     */
    [[nodiscard]] int GetWidth() const { return m_width; }
    [[nodiscard]] int GetHeight() const { return m_height; }

    /**
     * @brief Check if coordinates are within bounds
     */
    [[nodiscard]] bool InBounds(int x, int y) const;

    /**
     * @brief Resize the map (clears existing data)
     */
    void Resize(int width, int height);

    /**
     * @brief Clear all tiles
     */
    void Clear();

    /**
     * @brief Serialize entire map to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize map from JSON
     */
    static TileMap FromJson(const nlohmann::json& j);

    /**
     * @brief Get dirty regions for partial updates
     */
    [[nodiscard]] std::vector<std::pair<int, int>> GetDirtyTiles() const;

    /**
     * @brief Mark all tiles as clean (synced)
     */
    void ClearDirtyFlags();

    /**
     * @brief Check if map has unsaved changes
     */
    [[nodiscard]] bool IsDirty() const { return !m_dirtyTiles.empty(); }

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<Tile> m_tiles;
    std::vector<std::pair<int, int>> m_dirtyTiles;
    mutable Tile m_emptyTile;
};

/**
 * @brief Entity in the town (zombie, item, etc.)
 */
struct TownEntity {
    std::string id;
    std::string type;             ///< "zombie", "item", "npc", etc.
    float x = 0, y = 0, z = 0;    ///< Position
    float rotation = 0;           ///< Y-axis rotation
    int health = 100;
    bool active = true;
    nlohmann::json customData;    ///< Type-specific data

    [[nodiscard]] nlohmann::json ToJson() const;
    static TownEntity FromJson(const nlohmann::json& j);
};

/**
 * @brief Town data management via Firebase
 *
 * Manages:
 * - Loading/saving town map data from Firebase
 * - Generating procedural towns
 * - Real-time synchronization of town state
 * - Entity management within towns
 *
 * Firebase paths:
 * - /towns/{townId}/metadata - town info
 * - /towns/{townId}/map - tile map data
 * - /towns/{townId}/entities - shared entities
 * - /towns/{townId}/players - connected players
 */
class TownServer {
public:
    /**
     * @brief Town connection status
     */
    enum class ConnectionStatus {
        Disconnected,
        Connecting,
        Connected,
        Syncing,
        Error
    };

    /**
     * @brief Map change event
     */
    struct MapChangeEvent {
        int x, y;
        Tile oldTile;
        Tile newTile;
        std::string changedBy;  ///< Player ID who made the change
    };

    // Callback types
    using ConnectionCallback = std::function<void(bool success)>;
    using MapChangeCallback = std::function<void(const MapChangeEvent& event)>;
    using EntityCallback = std::function<void(const TownEntity& entity)>;
    using StatusCallback = std::function<void(ConnectionStatus status)>;

    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static TownServer& Instance();

    // Delete copy/move
    TownServer(const TownServer&) = delete;
    TownServer& operator=(const TownServer&) = delete;

    /**
     * @brief Initialize town server
     * @return true if successful
     */
    [[nodiscard]] bool Initialize();

    /**
     * @brief Shutdown and disconnect
     */
    void Shutdown();

    /**
     * @brief Connect to a town by GPS location
     * @param town Town information
     * @param callback Called when connection complete
     */
    void ConnectToTown(const TownInfo& town, ConnectionCallback callback);

    /**
     * @brief Connect to a town by ID
     * @param townId Town identifier
     * @param callback Called when connection complete
     */
    void ConnectToTownById(const std::string& townId, ConnectionCallback callback);

    /**
     * @brief Disconnect from current town
     */
    void DisconnectFromTown();

    /**
     * @brief Get the current town info
     */
    [[nodiscard]] const TownInfo& GetCurrentTown() const { return m_currentTown; }

    /**
     * @brief Check if connected to a town
     */
    [[nodiscard]] bool IsConnected() const { return m_status == ConnectionStatus::Connected; }

    /**
     * @brief Get connection status
     */
    [[nodiscard]] ConnectionStatus GetStatus() const { return m_status; }

    // ==================== Map Operations ====================

    /**
     * @brief Get the current town's map data
     * @return Reference to tile map (may be empty if not connected)
     */
    [[nodiscard]] TileMap& GetTownMap() { return m_townMap; }
    [[nodiscard]] const TileMap& GetTownMap() const { return m_townMap; }

    /**
     * @brief Save map changes to Firebase
     */
    void SaveMapChanges();

    /**
     * @brief Save specific tile change
     * @param x Tile X coordinate
     * @param y Tile Y coordinate
     * @param tile New tile data
     */
    void SaveTileChange(int x, int y, const Tile& tile);

    /**
     * @brief Register callback for map changes
     */
    void OnMapChanged(MapChangeCallback callback);

    // ==================== Town Generation ====================

    /**
     * @brief Generate a new random town
     * @param town Town info (for metadata)
     * @param seed Random seed (use -1 for random)
     */
    void GenerateNewTown(const TownInfo& town, int seed = -1);

    /**
     * @brief Check if town exists in database
     * @param townId Town identifier
     * @param callback Called with result
     */
    void TownExists(const std::string& townId, std::function<void(bool exists)> callback);

    // ==================== Entity Management ====================

    /**
     * @brief Spawn an entity in the town
     * @param entity Entity to spawn
     * @return Entity ID
     */
    std::string SpawnEntity(const TownEntity& entity);

    /**
     * @brief Update an entity's state
     * @param entity Updated entity data
     */
    void UpdateEntity(const TownEntity& entity);

    /**
     * @brief Remove an entity
     * @param entityId Entity to remove
     */
    void RemoveEntity(const std::string& entityId);

    /**
     * @brief Get all entities in town
     */
    [[nodiscard]] std::vector<TownEntity> GetEntities() const;

    /**
     * @brief Get entity by ID
     */
    [[nodiscard]] std::optional<TownEntity> GetEntity(const std::string& id) const;

    /**
     * @brief Register callback for entity spawned
     */
    void OnEntitySpawned(EntityCallback callback);

    /**
     * @brief Register callback for entity removed
     */
    void OnEntityRemoved(std::function<void(const std::string& id)> callback);

    // ==================== Real-time Sync ====================

    /**
     * @brief Start real-time synchronization
     */
    void StartRealtimeSync();

    /**
     * @brief Stop real-time synchronization
     */
    void StopRealtimeSync();

    /**
     * @brief Check if real-time sync is active
     */
    [[nodiscard]] bool IsRealtimeSyncActive() const { return m_realtimeSyncActive; }

    /**
     * @brief Process updates (call from game loop)
     */
    void Update(float deltaTime);

    /**
     * @brief Register status change callback
     */
    void OnStatusChanged(StatusCallback callback);

private:
    TownServer() = default;
    ~TownServer() = default;

    // Firebase paths
    [[nodiscard]] std::string GetTownPath() const;
    [[nodiscard]] std::string GetMapPath() const;
    [[nodiscard]] std::string GetEntitiesPath() const;
    [[nodiscard]] std::string GetPlayersPath() const;

    // Internal operations
    void LoadTownData(ConnectionCallback callback);
    void SetupListeners();
    void RemoveListeners();
    void HandleMapUpdate(const nlohmann::json& data);
    void HandleEntityUpdate(const nlohmann::json& data);
    void GenerateTownProcedurally(int seed);
    void NotifyStatusChanged(ConnectionStatus status);

    // State
    TownInfo m_currentTown;
    TileMap m_townMap;
    std::unordered_map<std::string, TownEntity> m_entities;
    ConnectionStatus m_status = ConnectionStatus::Disconnected;
    bool m_realtimeSyncActive = false;
    bool m_initialized = false;

    // Listeners
    std::string m_mapListenerId;
    std::string m_entitiesListenerId;

    // Callbacks
    std::vector<MapChangeCallback> m_mapChangeCallbacks;
    std::vector<EntityCallback> m_entitySpawnedCallbacks;
    std::vector<std::function<void(const std::string&)>> m_entityRemovedCallbacks;
    std::vector<StatusCallback> m_statusCallbacks;

    // Sync timing
    float m_syncTimer = 0.0f;
    static constexpr float SYNC_INTERVAL = 1.0f; // Seconds between syncs

    // Mutexes
    mutable std::mutex m_entityMutex;
    std::mutex m_callbackMutex;
};

} // namespace Vehement
