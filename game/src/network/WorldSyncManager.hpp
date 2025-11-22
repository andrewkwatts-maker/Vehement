#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <queue>
#include <nlohmann/json.hpp>
#include "../rts/world/WorldRegion.hpp"

namespace Vehement {

/**
 * @brief Sync operation type
 */
enum class SyncOperationType : uint8_t {
    RegionUpdate,
    PortalUpdate,
    PlayerPosition,
    FactionControl,
    WorldEvent,
    BattleStart,
    BattleEnd,
    ResourceChange,
    ControlPointUpdate
};

/**
 * @brief Sync priority levels
 */
enum class SyncPriority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

/**
 * @brief Pending sync operation
 */
struct SyncOperation {
    std::string operationId;
    SyncOperationType type;
    SyncPriority priority = SyncPriority::Normal;
    std::string targetId;  // Region/portal/player ID
    nlohmann::json data;
    int64_t timestamp = 0;
    int retryCount = 0;
    int maxRetries = 3;
    bool acknowledged = false;

    [[nodiscard]] nlohmann::json ToJson() const;
    static SyncOperation FromJson(const nlohmann::json& j);
};

/**
 * @brief Cross-region battle information
 */
struct CrossRegionBattle {
    std::string battleId;
    std::string regionId;
    std::vector<std::string> attackerPlayerIds;
    std::vector<std::string> defenderPlayerIds;
    int attackerFaction = -1;
    int defenderFaction = -1;
    Geo::GeoCoordinate location;
    int64_t startTimestamp = 0;
    int64_t endTimestamp = 0;
    bool active = true;
    std::string winnerId;
    std::unordered_map<std::string, int> casualties;

    [[nodiscard]] nlohmann::json ToJson() const;
    static CrossRegionBattle FromJson(const nlohmann::json& j);
};

/**
 * @brief World event broadcast
 */
struct WorldEventBroadcast {
    std::string eventId;
    std::string eventType;
    std::string title;
    std::string message;
    std::vector<std::string> affectedRegions;
    int64_t timestamp = 0;
    int64_t expiresAt = 0;
    SyncPriority priority = SyncPriority::Normal;
    bool global = false;

    [[nodiscard]] nlohmann::json ToJson() const;
    static WorldEventBroadcast FromJson(const nlohmann::json& j);
};

/**
 * @brief Configuration for world sync
 */
struct WorldSyncConfig {
    float positionSyncInterval = 5.0f;
    float regionSyncInterval = 30.0f;
    float stateSyncInterval = 10.0f;
    int maxPendingOperations = 100;
    int maxRetries = 3;
    float retryDelaySeconds = 5.0f;
    bool compressData = true;
    bool enableBatching = true;
    int batchSize = 10;
};

/**
 * @brief Manager for synchronizing world state across network
 */
class WorldSyncManager {
public:
    using SyncCompleteCallback = std::function<void(const SyncOperation& op, bool success)>;
    using BattleCallback = std::function<void(const CrossRegionBattle& battle)>;
    using EventCallback = std::function<void(const WorldEventBroadcast& event)>;
    using ConnectionCallback = std::function<void(bool connected)>;

    [[nodiscard]] static WorldSyncManager& Instance();

    WorldSyncManager(const WorldSyncManager&) = delete;
    WorldSyncManager& operator=(const WorldSyncManager&) = delete;

    [[nodiscard]] bool Initialize(const WorldSyncConfig& config = {});
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    void Update(float deltaTime);

    // ==================== Region Sync ====================

    /**
     * @brief Sync region ownership to server
     */
    void SyncRegionOwnership(const std::string& regionId, int factionId, const std::string& playerId);

    /**
     * @brief Sync region state
     */
    void SyncRegionState(const std::string& regionId, const nlohmann::json& state);

    /**
     * @brief Request region data from server
     */
    void RequestRegionData(const std::string& regionId);

    /**
     * @brief Subscribe to region updates
     */
    void SubscribeToRegion(const std::string& regionId);

    /**
     * @brief Unsubscribe from region updates
     */
    void UnsubscribeFromRegion(const std::string& regionId);

    // ==================== Portal Sync ====================

    /**
     * @brief Sync portal state
     */
    void SyncPortalState(const std::string& portalId, const nlohmann::json& state);

    /**
     * @brief Notify portal usage
     */
    void NotifyPortalUsage(const std::string& portalId, const std::string& playerId);

    // ==================== Player Position Sync ====================

    /**
     * @brief Sync local player position
     */
    void SyncPlayerPosition(const std::string& playerId, const std::string& regionId,
                            const Geo::GeoCoordinate& position);

    /**
     * @brief Get nearby players from server
     */
    void RequestNearbyPlayers(const std::string& regionId, const Geo::GeoCoordinate& center,
                              double radiusKm);

    // ==================== Battle Sync ====================

    /**
     * @brief Start cross-region battle
     */
    std::string StartBattle(const std::string& regionId, const std::vector<std::string>& attackers,
                            const std::vector<std::string>& defenders, const Geo::GeoCoordinate& location);

    /**
     * @brief Update battle state
     */
    void UpdateBattle(const std::string& battleId, const nlohmann::json& update);

    /**
     * @brief End battle
     */
    void EndBattle(const std::string& battleId, const std::string& winnerId);

    /**
     * @brief Get active battles in region
     */
    [[nodiscard]] std::vector<CrossRegionBattle> GetActiveBattles(const std::string& regionId) const;

    // ==================== World Events ====================

    /**
     * @brief Broadcast world event
     */
    void BroadcastEvent(const WorldEventBroadcast& event);

    /**
     * @brief Get pending events
     */
    [[nodiscard]] std::vector<WorldEventBroadcast> GetPendingEvents() const;

    /**
     * @brief Acknowledge event
     */
    void AcknowledgeEvent(const std::string& eventId);

    // ==================== Sync Status ====================

    /**
     * @brief Get pending operation count
     */
    [[nodiscard]] int GetPendingOperationCount() const;

    /**
     * @brief Check if fully synced
     */
    [[nodiscard]] bool IsFullySynced() const;

    /**
     * @brief Force sync all pending
     */
    void ForceSync();

    /**
     * @brief Get sync latency
     */
    [[nodiscard]] float GetSyncLatency() const { return m_lastLatency; }

    /**
     * @brief Check connection status
     */
    [[nodiscard]] bool IsConnected() const { return m_connected; }

    // ==================== Callbacks ====================

    void OnSyncComplete(SyncCompleteCallback callback);
    void OnBattle(BattleCallback callback);
    void OnWorldEvent(EventCallback callback);
    void OnConnectionChanged(ConnectionCallback callback);

    // ==================== Configuration ====================

    void SetLocalPlayerId(const std::string& playerId) { m_localPlayerId = playerId; }
    [[nodiscard]] const WorldSyncConfig& GetConfig() const { return m_config; }

private:
    WorldSyncManager() = default;
    ~WorldSyncManager() = default;

    void ProcessSyncQueue();
    void ProcessIncomingData(const nlohmann::json& data);
    void RetrySyncOperation(SyncOperation& op);
    void CleanupAcknowledged();
    std::string GenerateOperationId();

    bool m_initialized = false;
    WorldSyncConfig m_config;
    std::string m_localPlayerId;
    bool m_connected = false;
    float m_lastLatency = 0.0f;

    // Pending operations queue
    std::priority_queue<SyncOperation, std::vector<SyncOperation>,
        std::function<bool(const SyncOperation&, const SyncOperation&)>> m_syncQueue;
    std::unordered_map<std::string, SyncOperation> m_pendingOps;
    mutable std::mutex m_queueMutex;

    // Active battles
    std::unordered_map<std::string, CrossRegionBattle> m_activeBattles;
    mutable std::mutex m_battlesMutex;

    // Pending events
    std::vector<WorldEventBroadcast> m_pendingEvents;
    mutable std::mutex m_eventsMutex;

    // Subscriptions
    std::unordered_set<std::string> m_subscribedRegions;
    std::mutex m_subsMutex;

    // Callbacks
    std::vector<SyncCompleteCallback> m_syncCallbacks;
    std::vector<BattleCallback> m_battleCallbacks;
    std::vector<EventCallback> m_eventCallbacks;
    std::vector<ConnectionCallback> m_connectionCallbacks;
    std::mutex m_callbackMutex;

    // Timers
    float m_positionTimer = 0.0f;
    float m_regionTimer = 0.0f;
    float m_stateTimer = 0.0f;

    uint64_t m_nextOpId = 1;
    uint64_t m_nextBattleId = 1;
};

} // namespace Vehement
