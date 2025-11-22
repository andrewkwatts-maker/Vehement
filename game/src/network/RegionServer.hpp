#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <nlohmann/json.hpp>
#include "../rts/world/WorldRegion.hpp"

namespace Vehement {

/**
 * @brief Region instance state
 */
enum class RegionInstanceState : uint8_t {
    Offline,        // Not running
    Starting,       // Booting up
    Online,         // Running and accepting players
    Busy,           // Running but not accepting new players
    Maintenance,    // Undergoing maintenance
    Shutting Down   // Shutting down
};

/**
 * @brief Player session in region
 */
struct RegionPlayerSession {
    std::string sessionId;
    std::string playerId;
    std::string regionId;
    int64_t joinTimestamp = 0;
    int64_t lastActivity = 0;
    Geo::GeoCoordinate lastPosition;
    bool active = true;
    std::string transferringTo;  // Region ID if transferring

    [[nodiscard]] nlohmann::json ToJson() const;
    static RegionPlayerSession FromJson(const nlohmann::json& j);
};

/**
 * @brief Region instance information
 */
struct RegionInstance {
    std::string instanceId;
    std::string regionId;
    RegionInstanceState state = RegionInstanceState::Offline;
    std::string hostAddress;
    int port = 0;
    int playerCount = 0;
    int maxPlayers = 100;
    float loadPercent = 0.0f;
    int64_t startedTimestamp = 0;
    int64_t lastHeartbeat = 0;
    std::unordered_map<std::string, RegionPlayerSession> sessions;

    [[nodiscard]] nlohmann::json ToJson() const;
    static RegionInstance FromJson(const nlohmann::json& j);
};

/**
 * @brief Player transfer request
 */
struct PlayerTransferRequest {
    std::string requestId;
    std::string playerId;
    std::string sourceRegionId;
    std::string sourceInstanceId;
    std::string destinationRegionId;
    std::string destinationInstanceId;
    int64_t requestTimestamp = 0;
    int64_t completedTimestamp = 0;
    bool approved = false;
    bool completed = false;
    std::string failureReason;
    nlohmann::json playerState;  // Serialized player state

    [[nodiscard]] nlohmann::json ToJson() const;
    static PlayerTransferRequest FromJson(const nlohmann::json& j);
};

/**
 * @brief Region server configuration
 */
struct RegionServerConfig {
    int defaultMaxPlayers = 100;
    float heartbeatInterval = 10.0f;
    float sessionTimeout = 300.0f;
    float loadBalanceThreshold = 80.0f;
    int maxInstancesPerRegion = 5;
    bool autoScaling = true;
    float scaleUpThreshold = 70.0f;
    float scaleDownThreshold = 30.0f;
    int minInstances = 1;
};

/**
 * @brief Region server manager for hosting region instances
 */
class RegionServer {
public:
    using PlayerJoinCallback = std::function<void(const RegionPlayerSession& session)>;
    using PlayerLeaveCallback = std::function<void(const RegionPlayerSession& session)>;
    using TransferCallback = std::function<void(const PlayerTransferRequest& request)>;
    using StateChangeCallback = std::function<void(const RegionInstance& instance)>;

    [[nodiscard]] static RegionServer& Instance();

    RegionServer(const RegionServer&) = delete;
    RegionServer& operator=(const RegionServer&) = delete;

    [[nodiscard]] bool Initialize(const RegionServerConfig& config = {});
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    void Update(float deltaTime);

    // ==================== Instance Management ====================

    /**
     * @brief Start a region instance
     */
    std::string StartInstance(const std::string& regionId);

    /**
     * @brief Stop a region instance
     */
    bool StopInstance(const std::string& instanceId);

    /**
     * @brief Get instance by ID
     */
    [[nodiscard]] const RegionInstance* GetInstance(const std::string& instanceId) const;

    /**
     * @brief Get instances for region
     */
    [[nodiscard]] std::vector<RegionInstance> GetRegionInstances(const std::string& regionId) const;

    /**
     * @brief Get best instance for new player
     */
    [[nodiscard]] std::string GetBestInstance(const std::string& regionId) const;

    /**
     * @brief Get all active instances
     */
    [[nodiscard]] std::vector<RegionInstance> GetAllInstances() const;

    /**
     * @brief Set instance state
     */
    void SetInstanceState(const std::string& instanceId, RegionInstanceState state);

    // ==================== Player Sessions ====================

    /**
     * @brief Add player to instance
     */
    std::string JoinInstance(const std::string& instanceId, const std::string& playerId);

    /**
     * @brief Remove player from instance
     */
    void LeaveInstance(const std::string& instanceId, const std::string& playerId);

    /**
     * @brief Get player session
     */
    [[nodiscard]] const RegionPlayerSession* GetPlayerSession(const std::string& playerId) const;

    /**
     * @brief Update player activity
     */
    void UpdatePlayerActivity(const std::string& playerId, const Geo::GeoCoordinate& position);

    /**
     * @brief Get players in instance
     */
    [[nodiscard]] std::vector<RegionPlayerSession> GetInstancePlayers(const std::string& instanceId) const;

    // ==================== Player Transfer ====================

    /**
     * @brief Request player transfer
     */
    std::string RequestTransfer(const std::string& playerId, const std::string& destinationRegionId,
                                const nlohmann::json& playerState);

    /**
     * @brief Approve transfer
     */
    bool ApproveTransfer(const std::string& requestId, const std::string& destinationInstanceId);

    /**
     * @brief Complete transfer
     */
    bool CompleteTransfer(const std::string& requestId);

    /**
     * @brief Cancel transfer
     */
    void CancelTransfer(const std::string& requestId, const std::string& reason);

    /**
     * @brief Get pending transfers
     */
    [[nodiscard]] std::vector<PlayerTransferRequest> GetPendingTransfers() const;

    // ==================== State Persistence ====================

    /**
     * @brief Save region state
     */
    void SaveRegionState(const std::string& regionId, const nlohmann::json& state);

    /**
     * @brief Load region state
     */
    [[nodiscard]] nlohmann::json LoadRegionState(const std::string& regionId) const;

    /**
     * @brief Save player state
     */
    void SavePlayerState(const std::string& playerId, const nlohmann::json& state);

    /**
     * @brief Load player state
     */
    [[nodiscard]] nlohmann::json LoadPlayerState(const std::string& playerId) const;

    // ==================== Load Balancing ====================

    /**
     * @brief Get current load for region
     */
    [[nodiscard]] float GetRegionLoad(const std::string& regionId) const;

    /**
     * @brief Check if region needs scaling
     */
    [[nodiscard]] bool NeedsScaling(const std::string& regionId) const;

    /**
     * @brief Balance load across instances
     */
    void BalanceLoad(const std::string& regionId);

    // ==================== Callbacks ====================

    void OnPlayerJoin(PlayerJoinCallback callback);
    void OnPlayerLeave(PlayerLeaveCallback callback);
    void OnTransfer(TransferCallback callback);
    void OnStateChange(StateChangeCallback callback);

    // ==================== Configuration ====================

    [[nodiscard]] const RegionServerConfig& GetConfig() const { return m_config; }

private:
    RegionServer() = default;
    ~RegionServer() = default;

    void UpdateHeartbeats(float deltaTime);
    void UpdateSessionTimeouts(float deltaTime);
    void CheckAutoScaling();
    void CleanupExpiredSessions();
    std::string GenerateInstanceId();
    std::string GenerateSessionId();
    std::string GenerateTransferId();

    bool m_initialized = false;
    RegionServerConfig m_config;
    std::atomic<bool> m_running{false};

    // Instances
    std::unordered_map<std::string, RegionInstance> m_instances;
    mutable std::mutex m_instancesMutex;

    // Player to instance mapping
    std::unordered_map<std::string, std::string> m_playerInstances;
    mutable std::mutex m_playersMutex;

    // Transfer requests
    std::unordered_map<std::string, PlayerTransferRequest> m_transfers;
    mutable std::mutex m_transfersMutex;

    // Persisted state
    std::unordered_map<std::string, nlohmann::json> m_regionStates;
    std::unordered_map<std::string, nlohmann::json> m_playerStates;
    mutable std::mutex m_stateMutex;

    // Callbacks
    std::vector<PlayerJoinCallback> m_joinCallbacks;
    std::vector<PlayerLeaveCallback> m_leaveCallbacks;
    std::vector<TransferCallback> m_transferCallbacks;
    std::vector<StateChangeCallback> m_stateCallbacks;
    std::mutex m_callbackMutex;

    // Timers
    float m_heartbeatTimer = 0.0f;
    float m_sessionTimer = 0.0f;
    float m_scaleTimer = 0.0f;

    uint64_t m_nextInstanceId = 1;
    uint64_t m_nextSessionId = 1;
    uint64_t m_nextTransferId = 1;
};

} // namespace Vehement
