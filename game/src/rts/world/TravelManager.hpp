#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <queue>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include "WorldRegion.hpp"
#include "PortalGate.hpp"

namespace Vehement {

/**
 * @brief Travel state for a player/army
 */
enum class TravelState : uint8_t {
    Idle,           // Not traveling
    Preparing,      // Gathering units for travel
    InTransit,      // Currently traveling
    Arriving,       // Arriving at destination
    Interrupted,    // Travel interrupted
    Failed          // Travel failed
};

/**
 * @brief Travel encounter during transit
 */
struct TravelEncounter {
    std::string encounterId;
    std::string encounterType;  // ambush, merchant, event, portal_storm
    std::string name;
    std::string description;
    float travelProgressTrigger = 0.5f;
    bool mandatory = false;
    bool combat = false;
    std::vector<std::string> enemySpawns;
    std::unordered_map<std::string, int> rewards;
    std::unordered_map<std::string, int> costs;
    float delaySeconds = 0.0f;

    [[nodiscard]] nlohmann::json ToJson() const;
    static TravelEncounter FromJson(const nlohmann::json& j);
};

/**
 * @brief Resource transfer limits
 */
struct TransferLimits {
    int maxUnitsPerTrip = 100;
    int maxResourceTypesPerTrip = 10;
    std::unordered_map<std::string, int> maxResourceAmounts;
    float carryCapacityPerUnit = 100.0f;
    bool allowHeroes = true;
    bool allowSiegeUnits = false;
    bool allowBuildings = false;

    [[nodiscard]] nlohmann::json ToJson() const;
    static TransferLimits FromJson(const nlohmann::json& j);
};

/**
 * @brief Active travel session
 */
struct TravelSession {
    std::string sessionId;
    std::string playerId;
    TravelState state = TravelState::Idle;

    // Route info
    std::string sourceRegionId;
    std::string destinationRegionId;
    std::vector<std::string> portalPath;
    int currentPortalIndex = 0;

    // Cargo
    std::vector<std::string> unitIds;
    std::vector<std::string> heroIds;
    std::unordered_map<std::string, int> resources;

    // Timing
    int64_t startTimestamp = 0;
    int64_t estimatedArrival = 0;
    float progress = 0.0f;
    float totalTravelTime = 0.0f;
    float elapsedTime = 0.0f;

    // Encounters
    std::vector<TravelEncounter> pendingEncounters;
    TravelEncounter* activeEncounter = nullptr;
    bool encounterResolved = false;

    // Status
    bool cancelled = false;
    std::string failureReason;
    int unitsLost = 0;
    std::unordered_map<std::string, int> resourcesLost;

    [[nodiscard]] nlohmann::json ToJson() const;
    static TravelSession FromJson(const nlohmann::json& j);
};

/**
 * @brief Travel request parameters
 */
struct TravelRequest {
    std::string playerId;
    std::string sourceRegionId;
    std::string destinationRegionId;
    std::vector<std::string> unitIds;
    std::vector<std::string> heroIds;
    std::unordered_map<std::string, int> resources;
    bool useShortestPath = true;
    bool allowDangerousRegions = false;
    int maxDangerLevel = 5;

    [[nodiscard]] nlohmann::json ToJson() const;
    static TravelRequest FromJson(const nlohmann::json& j);
};

/**
 * @brief Travel result
 */
struct TravelResult {
    bool success = false;
    std::string sessionId;
    std::string errorMessage;
    float estimatedTime = 0.0f;
    std::unordered_map<std::string, int> totalCost;
    std::vector<std::string> warnings;
};

/**
 * @brief Configuration for travel manager
 */
struct TravelManagerConfig {
    float baseTravelSpeed = 100.0f;  // km/h equivalent
    float encounterCheckInterval = 5.0f;
    float maxTravelTimeHours = 24.0f;
    bool allowCancelDuringTravel = true;
    float cancelPenaltyPercent = 10.0f;
    bool instantTravelForPremium = false;
    TransferLimits defaultLimits;
};

/**
 * @brief Manager for travel between regions
 */
class TravelManager {
public:
    using TravelStartCallback = std::function<void(const TravelSession& session)>;
    using TravelProgressCallback = std::function<void(const TravelSession& session)>;
    using TravelCompleteCallback = std::function<void(const TravelSession& session)>;
    using EncounterCallback = std::function<void(TravelSession& session, TravelEncounter& encounter)>;

    [[nodiscard]] static TravelManager& Instance();

    TravelManager(const TravelManager&) = delete;
    TravelManager& operator=(const TravelManager&) = delete;

    [[nodiscard]] bool Initialize(const TravelManagerConfig& config = {});
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    void Update(float deltaTime);

    // ==================== Travel Operations ====================

    /**
     * @brief Request travel between regions
     */
    [[nodiscard]] TravelResult RequestTravel(const TravelRequest& request);

    /**
     * @brief Cancel active travel
     */
    bool CancelTravel(const std::string& sessionId);

    /**
     * @brief Get travel session
     */
    [[nodiscard]] const TravelSession* GetSession(const std::string& sessionId) const;

    /**
     * @brief Get all sessions for player
     */
    [[nodiscard]] std::vector<TravelSession> GetPlayerSessions(const std::string& playerId) const;

    /**
     * @brief Get active session count
     */
    [[nodiscard]] int GetActiveSessionCount() const;

    // ==================== Travel Validation ====================

    /**
     * @brief Validate travel request
     */
    [[nodiscard]] TravelResult ValidateTravel(const TravelRequest& request) const;

    /**
     * @brief Calculate travel cost
     */
    [[nodiscard]] std::unordered_map<std::string, int> CalculateTravelCost(
        const std::string& sourceRegion,
        const std::string& destRegion,
        int unitCount) const;

    /**
     * @brief Estimate travel time
     */
    [[nodiscard]] float EstimateTravelTime(
        const std::string& sourceRegion,
        const std::string& destRegion) const;

    /**
     * @brief Check if player can afford travel
     */
    [[nodiscard]] bool CanAffordTravel(
        const std::string& playerId,
        const std::unordered_map<std::string, int>& cost) const;

    // ==================== Transfer Limits ====================

    /**
     * @brief Get transfer limits for portal
     */
    [[nodiscard]] TransferLimits GetTransferLimits(const std::string& portalId) const;

    /**
     * @brief Set custom transfer limits for portal
     */
    void SetTransferLimits(const std::string& portalId, const TransferLimits& limits);

    /**
     * @brief Check if cargo fits within limits
     */
    [[nodiscard]] bool ValidateCargo(
        const std::vector<std::string>& units,
        const std::unordered_map<std::string, int>& resources,
        const TransferLimits& limits) const;

    // ==================== Encounters ====================

    /**
     * @brief Resolve encounter choice
     */
    void ResolveEncounter(const std::string& sessionId, const std::string& choice);

    /**
     * @brief Skip encounter (if allowed)
     */
    bool SkipEncounter(const std::string& sessionId);

    /**
     * @brief Get available encounter choices
     */
    [[nodiscard]] std::vector<std::string> GetEncounterChoices(const std::string& sessionId) const;

    // ==================== Region Loading ====================

    /**
     * @brief Preload destination region
     */
    void PreloadRegion(const std::string& regionId);

    /**
     * @brief Unload region
     */
    void UnloadRegion(const std::string& regionId);

    /**
     * @brief Check if region is loaded
     */
    [[nodiscard]] bool IsRegionLoaded(const std::string& regionId) const;

    /**
     * @brief Get region load progress
     */
    [[nodiscard]] float GetRegionLoadProgress(const std::string& regionId) const;

    // ==================== Callbacks ====================

    void OnTravelStarted(TravelStartCallback callback);
    void OnTravelProgress(TravelProgressCallback callback);
    void OnTravelCompleted(TravelCompleteCallback callback);
    void OnEncounter(EncounterCallback callback);

    // ==================== Configuration ====================

    [[nodiscard]] const TravelManagerConfig& GetConfig() const { return m_config; }
    void SetConfig(const TravelManagerConfig& config) { m_config = config; }

private:
    TravelManager() = default;
    ~TravelManager() = default;

    void UpdateSessions(float deltaTime);
    void ProcessArrival(TravelSession& session);
    void TriggerEncounter(TravelSession& session);
    void DeductTravelCost(const std::string& playerId, const std::unordered_map<std::string, int>& cost);
    std::string GenerateSessionId();
    std::vector<TravelEncounter> GenerateEncounters(const TravelSession& session);

    bool m_initialized = false;
    TravelManagerConfig m_config;

    std::unordered_map<std::string, TravelSession> m_sessions;
    mutable std::mutex m_sessionsMutex;

    std::unordered_map<std::string, TransferLimits> m_portalLimits;
    mutable std::mutex m_limitsMutex;

    std::unordered_set<std::string> m_loadedRegions;
    std::unordered_map<std::string, float> m_loadingProgress;
    mutable std::mutex m_loadingMutex;

    std::vector<TravelStartCallback> m_startCallbacks;
    std::vector<TravelProgressCallback> m_progressCallbacks;
    std::vector<TravelCompleteCallback> m_completeCallbacks;
    std::vector<EncounterCallback> m_encounterCallbacks;
    std::mutex m_callbackMutex;

    uint64_t m_nextSessionId = 1;
};

} // namespace Vehement
