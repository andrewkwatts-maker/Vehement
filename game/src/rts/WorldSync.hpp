#pragma once

#include "PersistentWorld.hpp"
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <deque>
#include <unordered_map>
#include <chrono>
#include <glm/glm.hpp>

namespace Vehement {

// Forward declarations
class FirebaseManager;

/**
 * @brief Nearby player information for multiplayer
 */
struct NearbyPlayer {
    std::string playerId;
    std::string displayName;
    glm::vec2 position;
    float distance;                 // Distance from local player
    bool isOnline;
    int64_t lastSeen;               // Timestamp
    int level;
    int territorySize;

    [[nodiscard]] nlohmann::json ToJson() const;
    static NearbyPlayer FromJson(const nlohmann::json& j);
};

/**
 * @brief Batched resource update for efficient sync
 */
struct ResourceUpdate {
    std::string playerId;
    ResourceStock resources;
    int64_t timestamp;

    [[nodiscard]] nlohmann::json ToJson() const;
    static ResourceUpdate FromJson(const nlohmann::json& j);
};

/**
 * @brief Building change event for sync
 */
struct BuildingChangeEvent {
    enum class Type {
        Placed,
        Destroyed,
        Upgraded,
        Damaged,
        Repaired
    };

    Type type;
    std::string playerId;
    Building building;
    int64_t timestamp;

    [[nodiscard]] nlohmann::json ToJson() const;
    static BuildingChangeEvent FromJson(const nlohmann::json& j);
};

/**
 * @brief Worker assignment change for sync
 */
struct WorkerAssignmentEvent {
    std::string playerId;
    int workerId;
    WorkerJob job;
    int buildingId;
    int64_t timestamp;

    [[nodiscard]] nlohmann::json ToJson() const;
    static WorkerAssignmentEvent FromJson(const nlohmann::json& j);
};

/**
 * @brief World sync configuration
 */
struct WorldSyncConfig {
    // Sync rates
    float heroPositionSyncRate = 5.0f;      // Sync hero position X times per second
    float resourceSyncRate = 0.2f;          // Sync resources every 5 seconds
    float buildingSyncRate = 1.0f;          // Building updates per second
    float nearbyPlayerSyncRate = 0.5f;      // Check nearby players twice per second

    // Distance thresholds
    float nearbyPlayerRadius = 1000.0f;     // Meters/units to consider "nearby"
    float positionUpdateThreshold = 1.0f;   // Min distance moved to send update

    // Batching
    int maxResourceBatchSize = 10;          // Max resource updates per batch
    int maxBuildingEventsPerSync = 5;       // Max building events per sync

    // Retry
    int maxRetryAttempts = 3;
    float retryDelay = 1.0f;                // Seconds between retries
};

/**
 * @brief Real-time world synchronization with Firebase
 *
 * Handles:
 * - Hero position synchronization (real-time for multiplayer visibility)
 * - Building placement/destruction sync
 * - Resource batching for efficient updates
 * - Worker assignment sync
 * - Nearby player detection and tracking
 * - World event broadcasting/listening
 *
 * Sync strategy:
 * - Hero position: High frequency, delta compression
 * - Buildings: Event-based (only on changes)
 * - Resources: Batched, periodic
 * - Workers: Event-based (only on assignment changes)
 */
class WorldSync {
public:
    /**
     * @brief Callback types
     */
    using NearbyPlayerCallback = std::function<void(const std::vector<NearbyPlayer>& players)>;
    using BuildingEventCallback = std::function<void(const BuildingChangeEvent& event)>;
    using WorldEventCallback = std::function<void(const WorldEvent& event)>;

    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static WorldSync& Instance();

    // Non-copyable
    WorldSync(const WorldSync&) = delete;
    WorldSync& operator=(const WorldSync&) = delete;

    /**
     * @brief Initialize world sync system
     * @param config Sync configuration
     * @return true if successful
     */
    [[nodiscard]] bool Initialize(const WorldSyncConfig& config = {});

    /**
     * @brief Shutdown sync system
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Start synchronization
     * @param playerId Local player ID
     * @param region Geographic region for syncing
     */
    void StartSync(const std::string& playerId, const std::string& region);

    /**
     * @brief Stop synchronization
     */
    void StopSync();

    /**
     * @brief Check if sync is active
     */
    [[nodiscard]] bool IsSyncing() const noexcept { return m_syncing; }

    /**
     * @brief Update sync (call from game loop)
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    // ==================== Hero Position Sync ====================

    /**
     * @brief Sync hero position to server
     * @param pos Current hero position
     * @param forceUpdate Force update even if below threshold
     */
    void SyncHeroPosition(const glm::vec2& pos, bool forceUpdate = false);

    /**
     * @brief Set hero rotation for sync
     * @param rotation Hero rotation in radians
     */
    void SyncHeroRotation(float rotation);

    /**
     * @brief Set hero online status
     * @param online Is player online
     */
    void SyncHeroOnlineStatus(bool online);

    // ==================== Building Sync ====================

    /**
     * @brief Sync a newly placed building
     * @param building The building that was placed
     */
    void SyncBuildingPlaced(const Building& building);

    /**
     * @brief Sync a destroyed building
     * @param buildingId ID of destroyed building
     */
    void SyncBuildingDestroyed(int buildingId);

    /**
     * @brief Sync a building upgrade
     * @param buildingId ID of upgraded building
     * @param newLevel New level
     */
    void SyncBuildingUpgraded(int buildingId, int newLevel);

    /**
     * @brief Sync building damage
     * @param buildingId ID of damaged building
     * @param newHealth New health value
     */
    void SyncBuildingDamaged(int buildingId, int newHealth);

    /**
     * @brief Register callback for building events from other players
     */
    void OnBuildingEvent(BuildingEventCallback callback);

    // ==================== Resource Sync ====================

    /**
     * @brief Sync resource changes (batched for efficiency)
     * @param resources Current resource stock
     */
    void SyncResources(const ResourceStock& resources);

    /**
     * @brief Force immediate resource sync
     */
    void FlushResourceSync();

    // ==================== Worker Sync ====================

    /**
     * @brief Sync worker assignment change
     * @param workerId Worker ID
     * @param job New job
     * @param buildingId Building assigned to (-1 for none)
     */
    void SyncWorkerAssignment(int workerId, WorkerJob job, int buildingId);

    // ==================== Nearby Players ====================

    /**
     * @brief Start listening for nearby players
     */
    void ListenForNearbyPlayers();

    /**
     * @brief Stop listening for nearby players
     */
    void StopListeningForNearbyPlayers();

    /**
     * @brief Get current nearby players
     */
    [[nodiscard]] std::vector<NearbyPlayer> GetNearbyPlayers() const;

    /**
     * @brief Register callback for nearby player updates
     */
    void OnNearbyPlayersChanged(NearbyPlayerCallback callback);

    // ==================== World Events ====================

    /**
     * @brief Start listening for world events
     */
    void ListenForWorldEvents();

    /**
     * @brief Stop listening for world events
     */
    void StopListeningForWorldEvents();

    /**
     * @brief Broadcast a world event
     * @param event Event to broadcast
     */
    void BroadcastWorldEvent(const WorldEvent& event);

    /**
     * @brief Register callback for world events
     */
    void OnWorldEvent(WorldEventCallback callback);

    // ==================== Territory Sync ====================

    /**
     * @brief Sync territory claim
     * @param tiles Tiles claimed
     */
    void SyncTerritoryClaim(const std::vector<glm::ivec2>& tiles);

    /**
     * @brief Sync territory strength
     * @param strength Current territory strength
     */
    void SyncTerritoryStrength(float strength);

    // ==================== Statistics ====================

    /**
     * @brief Get sync statistics
     */
    struct SyncStats {
        int positionUpdatesPerSecond = 0;
        int resourceSyncsPerSecond = 0;
        int buildingEventsPerSecond = 0;
        int bytesUpPerSecond = 0;
        int bytesDownPerSecond = 0;
        float averageLatency = 0.0f;
        int nearbyPlayersCount = 0;
        int pendingUpdates = 0;
    };
    [[nodiscard]] SyncStats GetStats() const;

    /**
     * @brief Get current latency estimate
     */
    [[nodiscard]] float GetLatency() const { return m_latency; }

private:
    WorldSync() = default;
    ~WorldSync() = default;

    // Firebase paths
    [[nodiscard]] std::string GetHeroPositionPath() const;
    [[nodiscard]] std::string GetBuildingsPath() const;
    [[nodiscard]] std::string GetResourcesPath() const;
    [[nodiscard]] std::string GetWorkersPath() const;
    [[nodiscard]] std::string GetRegionPlayersPath() const;
    [[nodiscard]] std::string GetWorldEventsPath() const;
    [[nodiscard]] std::string GetTerritoryPath() const;

    // Internal sync operations
    void SetupListeners();
    void RemoveListeners();
    void ProcessPendingUpdates();
    void UpdateNearbyPlayers();
    void HandlePlayerUpdate(const std::string& playerId, const nlohmann::json& data);
    void HandleBuildingEvent(const nlohmann::json& data);
    void HandleWorldEvent(const nlohmann::json& data);
    void UpdateLatencyEstimate();

    // Delta compression for positions
    [[nodiscard]] bool ShouldSendPositionUpdate(const glm::vec2& pos) const;

    bool m_initialized = false;
    bool m_syncing = false;
    WorldSyncConfig m_config;

    // Player info
    std::string m_playerId;
    std::string m_region;
    glm::vec2 m_lastSyncedPosition{0.0f};
    float m_lastSyncedRotation = 0.0f;

    // Sync timers
    float m_positionSyncTimer = 0.0f;
    float m_resourceSyncTimer = 0.0f;
    float m_nearbyPlayerTimer = 0.0f;

    // Pending updates queue
    struct PendingUpdate {
        enum class Type { Position, Building, Resource, Worker, Territory, Event };
        Type type;
        nlohmann::json data;
        int retryCount = 0;
    };
    std::deque<PendingUpdate> m_pendingUpdates;
    std::mutex m_pendingMutex;

    // Nearby players cache
    std::unordered_map<std::string, NearbyPlayer> m_nearbyPlayers;
    mutable std::mutex m_nearbyMutex;

    // Building events queue
    std::deque<BuildingChangeEvent> m_buildingEvents;
    std::mutex m_buildingMutex;

    // Listener IDs
    std::string m_playersListenerId;
    std::string m_buildingsListenerId;
    std::string m_eventsListenerId;

    // Callbacks
    std::vector<NearbyPlayerCallback> m_nearbyCallbacks;
    std::vector<BuildingEventCallback> m_buildingCallbacks;
    std::vector<WorldEventCallback> m_worldEventCallbacks;
    std::mutex m_callbackMutex;

    // Resource batching
    ResourceStock m_pendingResourceUpdate;
    bool m_resourceUpdatePending = false;

    // Statistics
    float m_latency = 0.0f;
    int m_positionUpdates = 0;
    int m_resourceSyncs = 0;
    int m_buildingEventCount = 0;
    float m_statsTimer = 0.0f;
    SyncStats m_stats;
};

} // namespace Vehement
