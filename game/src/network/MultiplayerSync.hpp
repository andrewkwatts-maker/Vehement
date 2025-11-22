#pragma once

#include "FirebaseManager.hpp"
#include "Matchmaking.hpp"
#include "TownServer.hpp"
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <chrono>

namespace Vehement {

/**
 * @brief Networked transform for entities
 */
struct NetworkTransform {
    float x = 0, y = 0, z = 0;     ///< Position
    float rotX = 0, rotY = 0, rotZ = 0; ///< Rotation (Euler angles)
    float velX = 0, velY = 0, velZ = 0; ///< Velocity (for prediction)
    int64_t timestamp = 0;         ///< Server timestamp

    [[nodiscard]] nlohmann::json ToJson() const;
    static NetworkTransform FromJson(const nlohmann::json& j);

    /**
     * @brief Interpolate between two transforms
     */
    [[nodiscard]] static NetworkTransform Lerp(const NetworkTransform& a,
                                                const NetworkTransform& b,
                                                float t);
};

/**
 * @brief Zombie network state
 */
struct ZombieNetState {
    std::string id;
    NetworkTransform transform;
    int health = 100;
    bool isDead = false;
    std::string targetPlayerId;    ///< Which player zombie is targeting
    std::string lastDamagedBy;     ///< Player who last damaged this zombie

    enum class State {
        Idle,
        Roaming,
        Chasing,
        Attacking,
        Dead
    } state = State::Idle;

    [[nodiscard]] nlohmann::json ToJson() const;
    static ZombieNetState FromJson(const nlohmann::json& j);
};

/**
 * @brief Player network state
 */
struct PlayerNetState {
    std::string oderId;
    NetworkTransform transform;
    int health = 100;
    bool isDead = false;
    int score = 0;
    std::string currentWeapon;
    bool isShooting = false;
    bool isReloading = false;

    [[nodiscard]] nlohmann::json ToJson() const;
    static PlayerNetState FromJson(const nlohmann::json& j);
};

/**
 * @brief Map edit network event
 */
struct MapEditEvent {
    int tileX;
    int tileY;
    Tile newTile;
    std::string editedBy;          ///< Player who made the edit
    int64_t timestamp;

    [[nodiscard]] nlohmann::json ToJson() const;
    static MapEditEvent FromJson(const nlohmann::json& j);
};

/**
 * @brief Game event for synchronization
 */
struct GameEvent {
    enum class Type {
        PlayerSpawned,
        PlayerDied,
        PlayerRespawned,
        ZombieSpawned,
        ZombieDied,
        ZombieTargetChanged,
        DamageDealt,
        ItemPickedUp,
        WeaponFired,
        MapEdited,
        TileCleared,
        Custom
    } type;

    std::string sourceId;          ///< Entity that caused the event
    std::string targetId;          ///< Entity affected by the event
    nlohmann::json data;           ///< Event-specific data
    int64_t timestamp;

    [[nodiscard]] nlohmann::json ToJson() const;
    static GameEvent FromJson(const nlohmann::json& j);
};

/**
 * @brief Real-time multiplayer state synchronization
 *
 * Handles:
 * - Player position/state synchronization
 * - Zombie state synchronization
 * - Map edit synchronization
 * - Conflict resolution (server authoritative)
 * - Interpolation for smooth movement
 * - Latency compensation
 *
 * Sync strategy:
 * - Players: High frequency (10-20 Hz), interpolation
 * - Zombies: Medium frequency (5-10 Hz), owned by host
 * - Map edits: Event-based, conflict resolution
 * - Items: Event-based, first-come-first-served
 */
class MultiplayerSync {
public:
    /**
     * @brief Synchronization configuration
     */
    struct Config {
        // Update rates
        float playerSyncRate = 10.0f;     ///< Player updates per second
        float zombieSyncRate = 5.0f;      ///< Zombie updates per second
        float eventSyncRate = 20.0f;      ///< Event processing rate

        // Interpolation
        float interpolationDelay = 0.1f;   ///< Seconds of delay for interpolation
        int maxInterpolationStates = 20;   ///< Max states to store for interpolation

        // Authority
        bool hostAuthoritative = true;     ///< Host controls zombie spawning/state
        bool conflictResolutionByTimestamp = true; ///< Use timestamp for conflicts

        // Limits
        int maxZombiesPerTown = 100;       ///< Maximum zombies in town
        int maxEventsPerSecond = 50;       ///< Rate limit for events
    };

    // Callback types
    using PlayerStateCallback = std::function<void(const PlayerNetState& state)>;
    using ZombieStateCallback = std::function<void(const ZombieNetState& state)>;
    using MapEditCallback = std::function<void(const MapEditEvent& edit)>;
    using GameEventCallback = std::function<void(const GameEvent& event)>;

    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static MultiplayerSync& Instance();

    // Delete copy/move
    MultiplayerSync(const MultiplayerSync&) = delete;
    MultiplayerSync& operator=(const MultiplayerSync&) = delete;

    /**
     * @brief Initialize multiplayer sync
     * @param config Sync configuration
     * @return true if successful
     */
    [[nodiscard]] bool Initialize(const Config& config = {});

    /**
     * @brief Shutdown sync system
     */
    void Shutdown();

    /**
     * @brief Start synchronization for current town
     */
    void StartSync();

    /**
     * @brief Stop synchronization
     */
    void StopSync();

    /**
     * @brief Check if sync is active
     */
    [[nodiscard]] bool IsSyncing() const { return m_syncing; }

    /**
     * @brief Process updates (call from game loop)
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    // ==================== Player Sync ====================

    /**
     * @brief Update local player state
     * @param state Current player state
     */
    void SetLocalPlayerState(const PlayerNetState& state);

    /**
     * @brief Get remote player's interpolated state
     * @param oderId Player ID
     * @return Interpolated state or nullopt if not found
     */
    [[nodiscard]] std::optional<PlayerNetState> GetPlayerState(const std::string& oderId) const;

    /**
     * @brief Get all remote player states (interpolated)
     */
    [[nodiscard]] std::vector<PlayerNetState> GetAllPlayerStates() const;

    /**
     * @brief Register callback for player state updates
     */
    void OnPlayerStateChanged(PlayerStateCallback callback);

    // ==================== Zombie Sync ====================

    /**
     * @brief Update zombie state (host only)
     * @param state Zombie state
     */
    void SetZombieState(const ZombieNetState& state);

    /**
     * @brief Get zombie's interpolated state
     * @param zombieId Zombie ID
     * @return Interpolated state or nullopt if not found
     */
    [[nodiscard]] std::optional<ZombieNetState> GetZombieState(const std::string& zombieId) const;

    /**
     * @brief Get all zombie states (interpolated)
     */
    [[nodiscard]] std::vector<ZombieNetState> GetAllZombieStates() const;

    /**
     * @brief Spawn a zombie (host only)
     * @param zombie Initial zombie state
     * @return Zombie ID
     */
    std::string SpawnZombie(const ZombieNetState& zombie);

    /**
     * @brief Kill a zombie
     * @param zombieId Zombie to kill
     * @param killedBy Player who killed it
     */
    void KillZombie(const std::string& zombieId, const std::string& killedBy);

    /**
     * @brief Damage a zombie
     * @param zombieId Zombie to damage
     * @param damage Damage amount
     * @param damagedBy Player who dealt damage
     */
    void DamageZombie(const std::string& zombieId, int damage, const std::string& damagedBy);

    /**
     * @brief Register callback for zombie state updates
     */
    void OnZombieStateChanged(ZombieStateCallback callback);

    /**
     * @brief Check if local player is the host (controls zombies)
     */
    [[nodiscard]] bool IsHost() const;

    // ==================== Map Sync ====================

    /**
     * @brief Send a map edit
     * @param x Tile X
     * @param y Tile Y
     * @param newTile New tile data
     */
    void SendMapEdit(int x, int y, const Tile& newTile);

    /**
     * @brief Register callback for map edits
     */
    void OnMapEdited(MapEditCallback callback);

    // ==================== Game Events ====================

    /**
     * @brief Send a game event
     * @param event Event to send
     */
    void SendEvent(const GameEvent& event);

    /**
     * @brief Register callback for game events
     */
    void OnGameEvent(GameEventCallback callback);

    // ==================== Latency & Stats ====================

    /**
     * @brief Get estimated latency to server
     */
    [[nodiscard]] float GetLatency() const { return m_latency; }

    /**
     * @brief Get current server time estimate
     */
    [[nodiscard]] int64_t GetServerTime() const;

    /**
     * @brief Get sync statistics
     */
    struct SyncStats {
        int playerUpdatesPerSecond = 0;
        int zombieUpdatesPerSecond = 0;
        int eventsPerSecond = 0;
        int bytesUpPerSecond = 0;
        int bytesDownPerSecond = 0;
        float averageLatency = 0.0f;
    };
    [[nodiscard]] SyncStats GetStats() const;

private:
    MultiplayerSync() = default;
    ~MultiplayerSync() = default;

    // Firebase paths
    [[nodiscard]] std::string GetPlayersPath() const;
    [[nodiscard]] std::string GetZombiesPath() const;
    [[nodiscard]] std::string GetEventsPath() const;
    [[nodiscard]] std::string GetMapEditsPath() const;

    // Internal operations
    void SetupListeners();
    void RemoveListeners();
    void SendPlayerUpdate();
    void SendZombieUpdates();
    void ProcessRemoteStates();
    void ProcessEvents();
    void HandlePlayerUpdate(const std::string& oderId, const nlohmann::json& data);
    void HandleZombieUpdate(const std::string& zombieId, const nlohmann::json& data);
    void HandleMapEdit(const nlohmann::json& data);
    void HandleGameEvent(const nlohmann::json& data);
    void UpdateLatencyEstimate();

    // Interpolation helpers
    template<typename T>
    T InterpolateState(const std::deque<std::pair<int64_t, T>>& history, int64_t targetTime) const;

    // Configuration
    Config m_config;
    bool m_initialized = false;
    bool m_syncing = false;

    // Local state
    PlayerNetState m_localPlayerState;
    std::string m_currentTownId;

    // Remote player states with history for interpolation
    struct PlayerHistory {
        std::deque<std::pair<int64_t, PlayerNetState>> states;
        PlayerNetState interpolated;
    };
    std::unordered_map<std::string, PlayerHistory> m_playerStates;
    mutable std::mutex m_playerMutex;

    // Zombie states (host authoritative)
    struct ZombieHistory {
        std::deque<std::pair<int64_t, ZombieNetState>> states;
        ZombieNetState interpolated;
    };
    std::unordered_map<std::string, ZombieHistory> m_zombieStates;
    mutable std::mutex m_zombieMutex;

    // Event queue
    std::deque<GameEvent> m_pendingEvents;
    std::mutex m_eventMutex;

    // Map edit queue
    std::deque<MapEditEvent> m_pendingMapEdits;
    std::mutex m_mapEditMutex;

    // Listeners
    std::string m_playersListenerId;
    std::string m_zombiesListenerId;
    std::string m_eventsListenerId;
    std::string m_mapEditsListenerId;

    // Callbacks
    std::vector<PlayerStateCallback> m_playerStateCallbacks;
    std::vector<ZombieStateCallback> m_zombieStateCallbacks;
    std::vector<MapEditCallback> m_mapEditCallbacks;
    std::vector<GameEventCallback> m_gameEventCallbacks;
    std::mutex m_callbackMutex;

    // Timing
    float m_playerSyncTimer = 0.0f;
    float m_zombieSyncTimer = 0.0f;
    float m_latency = 0.0f;
    int64_t m_serverTimeOffset = 0;

    // Statistics
    int m_playerUpdates = 0;
    int m_zombieUpdates = 0;
    int m_eventsProcessed = 0;
    float m_statsTimer = 0.0f;
    SyncStats m_stats;
};

/**
 * @brief Conflict resolver for concurrent edits
 */
class ConflictResolver {
public:
    /**
     * @brief Resolution strategy
     */
    enum class Strategy {
        FirstWins,        ///< First update wins
        LastWins,         ///< Last update wins (default)
        HostWins,         ///< Host's update always wins
        HighestHealth,    ///< For health, lower value wins (damage applied)
        Merge             ///< Attempt to merge changes
    };

    /**
     * @brief Resolve conflict between two zombie states
     * @param local Local state
     * @param remote Remote state
     * @param strategy Resolution strategy
     * @return Resolved state
     */
    static ZombieNetState ResolveZombieConflict(const ZombieNetState& local,
                                                 const ZombieNetState& remote,
                                                 Strategy strategy = Strategy::LastWins);

    /**
     * @brief Resolve conflict between two map edits
     * @param local Local edit
     * @param remote Remote edit
     * @param strategy Resolution strategy
     * @return Resolved edit
     */
    static MapEditEvent ResolveMapEditConflict(const MapEditEvent& local,
                                                const MapEditEvent& remote,
                                                Strategy strategy = Strategy::LastWins);
};

} // namespace Vehement
