#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include "WorldRegion.hpp"

namespace Vehement {

/**
 * @brief Portal activation status
 */
enum class PortalStatus : uint8_t {
    Inactive,       // Portal not activated
    Activating,     // Portal warming up
    Active,         // Portal ready for travel
    Cooldown,       // Portal on cooldown
    Disabled,       // Portal permanently disabled
    Locked,         // Portal requires unlock
    Unstable        // Portal has random behavior
};

const char* PortalStatusToString(PortalStatus status);
PortalStatus PortalStatusFromString(const std::string& str);

/**
 * @brief Visual effect type for portals
 */
enum class PortalVisualType : uint8_t {
    Standard,       // Default swirling portal
    Fire,           // Flame portal
    Ice,            // Frozen portal
    Shadow,         // Dark/void portal
    Nature,         // Living/organic portal
    Tech,           // Mechanical/sci-fi portal
    Celestial,      // Divine/heavenly portal
    Infernal,       // Demonic portal
    Ancient,        // Runic/mystical portal
    Dimensional     // Reality-warping portal
};

const char* PortalVisualTypeToString(PortalVisualType type);
PortalVisualType PortalVisualTypeFromString(const std::string& str);

/**
 * @brief Requirements to activate/use a portal
 */
struct PortalRequirements {
    int minLevel = 1;
    int maxLevel = 100;
    std::vector<std::string> requiredQuests;      // Must have completed
    std::vector<std::string> requiredItems;       // Must possess
    std::unordered_map<std::string, int> resourceCost;  // One-time cost
    std::unordered_map<std::string, int> maintenanceCost;  // Per-use cost
    std::vector<std::string> requiredFactions;    // Must belong to faction
    std::vector<std::string> bannedFactions;      // Cannot use if in faction
    bool requiresGroupLeader = false;
    int minGroupSize = 1;
    int maxGroupSize = 100;
    float cooldownSeconds = 0.0f;                 // Personal cooldown
    int64_t availableAfterTimestamp = 0;          // Time-gated
    int64_t availableUntilTimestamp = 0;          // Limited time
    std::vector<std::string> requiredAchievements;
    std::string requiredRegionControl;            // Must control destination region

    [[nodiscard]] nlohmann::json ToJson() const;
    static PortalRequirements FromJson(const nlohmann::json& j);
};

/**
 * @brief Visual and audio configuration for portal
 */
struct PortalVisuals {
    PortalVisualType type = PortalVisualType::Standard;
    glm::vec3 primaryColor{0.5f, 0.5f, 1.0f};
    glm::vec3 secondaryColor{0.3f, 0.3f, 0.8f};
    float scale = 1.0f;
    float rotationSpeed = 1.0f;
    float pulseFrequency = 1.0f;
    float particleDensity = 1.0f;
    std::string customModel;
    std::string customTexture;
    std::string idleAnimation;
    std::string activateAnimation;
    std::string deactivateAnimation;
    std::string travelAnimation;
    std::string ambientSound;
    std::string activateSound;
    std::string travelSound;
    std::string arrivalSound;
    float soundRadius = 50.0f;
    bool emitsLight = true;
    float lightRadius = 20.0f;
    glm::vec3 lightColor{0.6f, 0.6f, 1.0f};

    [[nodiscard]] nlohmann::json ToJson() const;
    static PortalVisuals FromJson(const nlohmann::json& j);
};

/**
 * @brief Travel configuration
 */
struct TravelConfig {
    float baseTravelTime = 10.0f;           // Seconds
    float distanceTimeMultiplier = 0.001f;  // Additional time per km
    int maxUnitsPerTrip = 50;
    int maxResourcesPerTrip = 10000;
    float unitCapacityMultiplier = 1.0f;
    bool allowCombatUnits = true;
    bool allowCivilianUnits = true;
    bool allowHeroes = true;
    bool preserveFormation = true;
    float encounterChance = 0.0f;           // Chance of travel encounter
    std::vector<std::string> possibleEncounters;
    bool canBeInterrupted = false;
    float interruptionChance = 0.0f;

    [[nodiscard]] nlohmann::json ToJson() const;
    static TravelConfig FromJson(const nlohmann::json& j);
};

/**
 * @brief Traveler currently in transit
 */
struct PortalTraveler {
    std::string travelerId;
    std::string playerId;
    std::string sourceRegionId;
    std::string destinationRegionId;
    std::string sourcePortalId;
    std::string destinationPortalId;
    int64_t departureTime = 0;
    int64_t arrivalTime = 0;
    float progress = 0.0f;                  // 0-1
    std::vector<std::string> unitIds;
    std::unordered_map<std::string, int> resources;
    bool interrupted = false;
    std::string encounterId;

    [[nodiscard]] nlohmann::json ToJson() const;
    static PortalTraveler FromJson(const nlohmann::json& j);
};

/**
 * @brief Portal gate entity representing a travel point
 */
struct PortalGate {
    // Identity
    std::string id;
    std::string name;
    std::string description;
    std::string regionId;

    // Location
    Geo::GeoCoordinate gpsLocation;
    glm::vec3 worldPosition{0.0f};          // Local world position
    float rotation = 0.0f;                  // Facing direction

    // Destination
    std::string destinationRegionId;
    std::string destinationPortalId;
    bool bidirectional = true;

    // Status
    PortalStatus status = PortalStatus::Active;
    float activationProgress = 0.0f;        // For activating state
    float cooldownRemaining = 0.0f;
    int64_t lastUsedTimestamp = 0;

    // Configuration
    PortalRequirements requirements;
    PortalVisuals visuals;
    TravelConfig travelConfig;

    // Capacity
    int currentCapacity = 0;                // Current travelers in transit
    int maxCapacity = 10;                   // Max concurrent travelers
    float congestionLevel = 0.0f;           // 0-1, affects travel time

    // Statistics
    int totalUses = 0;
    int uniqueUsers = 0;
    int64_t createdTimestamp = 0;

    // Travelers
    std::vector<PortalTraveler> inTransit;

    // Special flags
    bool isOneWay = false;
    bool isHidden = false;
    bool isTemporary = false;
    int64_t expirationTimestamp = 0;        // For temporary portals
    bool isBossPortal = false;              // Leads to boss area
    bool isEventPortal = false;             // Event-specific
    bool requiresKey = false;
    std::string keyItemId;

    /**
     * @brief Check if player can use this portal
     */
    [[nodiscard]] bool CanPlayerUse(int playerLevel,
                                     const std::vector<std::string>& completedQuests,
                                     const std::vector<std::string>& inventory,
                                     const std::string& factionId) const;

    /**
     * @brief Calculate travel time for given distance
     */
    [[nodiscard]] float CalculateTravelTime(double distanceKm) const;

    /**
     * @brief Check if portal has capacity for more travelers
     */
    [[nodiscard]] bool HasCapacity(int unitCount = 1) const;

    /**
     * @brief Get effective cooldown considering congestion
     */
    [[nodiscard]] float GetEffectiveCooldown() const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static PortalGate FromJson(const nlohmann::json& j);
};

/**
 * @brief Portal network edge for pathfinding
 */
struct PortalNetworkEdge {
    std::string sourcePortalId;
    std::string targetPortalId;
    std::string sourceRegionId;
    std::string targetRegionId;
    float baseTravelTime = 0.0f;
    float currentTravelTime = 0.0f;         // Including congestion
    bool bidirectional = true;
    bool active = true;
    int minLevel = 1;

    [[nodiscard]] nlohmann::json ToJson() const;
    static PortalNetworkEdge FromJson(const nlohmann::json& j);
};

/**
 * @brief Portal travel path
 */
struct TravelPath {
    std::vector<std::string> portalIds;
    std::vector<std::string> regionIds;
    float totalTravelTime = 0.0f;
    float totalDistance = 0.0f;
    std::unordered_map<std::string, int> totalResourceCost;
    int requiredLevel = 1;
    std::vector<std::string> requiredQuests;
    bool valid = false;
    std::string invalidReason;
};

/**
 * @brief Configuration for portal system
 */
struct PortalConfig {
    float defaultCooldown = 60.0f;
    float defaultTravelTime = 30.0f;
    int defaultMaxCapacity = 10;
    float congestionThreshold = 0.7f;
    float congestionPenalty = 1.5f;
    float activationTime = 5.0f;
    bool allowCrossRegionTravel = true;
    float encounterBaseChance = 0.05f;
    float maxTravelTimeSeconds = 300.0f;
};

/**
 * @brief Manager for portal gates
 */
class PortalManager {
public:
    using PortalChangedCallback = std::function<void(const PortalGate& portal)>;
    using TravelStartCallback = std::function<void(const PortalTraveler& traveler)>;
    using TravelCompleteCallback = std::function<void(const PortalTraveler& traveler)>;
    using EncounterCallback = std::function<void(const PortalTraveler& traveler, const std::string& encounterId)>;

    [[nodiscard]] static PortalManager& Instance();

    PortalManager(const PortalManager&) = delete;
    PortalManager& operator=(const PortalManager&) = delete;

    /**
     * @brief Initialize portal system
     */
    [[nodiscard]] bool Initialize(const PortalConfig& config = {});
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Update portal system
     */
    void Update(float deltaTime);

    // ==================== Portal Queries ====================

    /**
     * @brief Get portal by ID
     */
    [[nodiscard]] const PortalGate* GetPortal(const std::string& portalId) const;

    /**
     * @brief Get all portals
     */
    [[nodiscard]] std::vector<const PortalGate*> GetAllPortals() const;

    /**
     * @brief Get portals in region
     */
    [[nodiscard]] std::vector<const PortalGate*> GetPortalsInRegion(const std::string& regionId) const;

    /**
     * @brief Get active portals accessible to player
     */
    [[nodiscard]] std::vector<const PortalGate*> GetAccessiblePortals(
        const std::string& playerId, int playerLevel,
        const std::vector<std::string>& completedQuests) const;

    /**
     * @brief Find nearest portal to GPS coordinate
     */
    [[nodiscard]] const PortalGate* FindNearestPortal(const Geo::GeoCoordinate& coord) const;

    /**
     * @brief Find portals leading to region
     */
    [[nodiscard]] std::vector<const PortalGate*> FindPortalsToRegion(const std::string& regionId) const;

    // ==================== Portal Management ====================

    /**
     * @brief Register a portal
     */
    bool RegisterPortal(const PortalGate& portal);

    /**
     * @brief Update portal
     */
    bool UpdatePortal(const PortalGate& portal);

    /**
     * @brief Remove portal
     */
    bool RemovePortal(const std::string& portalId);

    /**
     * @brief Load portals from config
     */
    bool LoadPortalsFromConfig(const std::string& configPath);

    /**
     * @brief Set portal status
     */
    void SetPortalStatus(const std::string& portalId, PortalStatus status);

    /**
     * @brief Activate a portal
     */
    bool ActivatePortal(const std::string& portalId, const std::string& playerId);

    /**
     * @brief Deactivate a portal
     */
    void DeactivatePortal(const std::string& portalId);

    // ==================== Travel ====================

    /**
     * @brief Start travel through portal
     * @return Travel ID or empty string on failure
     */
    std::string StartTravel(const std::string& portalId,
                            const std::string& playerId,
                            const std::vector<std::string>& unitIds,
                            const std::unordered_map<std::string, int>& resources);

    /**
     * @brief Cancel in-progress travel
     */
    bool CancelTravel(const std::string& travelId);

    /**
     * @brief Get travel status
     */
    [[nodiscard]] const PortalTraveler* GetTravelStatus(const std::string& travelId) const;

    /**
     * @brief Get all travelers for player
     */
    [[nodiscard]] std::vector<PortalTraveler> GetPlayerTravelers(const std::string& playerId) const;

    // ==================== Pathfinding ====================

    /**
     * @brief Find optimal path between regions
     */
    [[nodiscard]] TravelPath FindPath(const std::string& sourceRegionId,
                                       const std::string& destRegionId,
                                       int playerLevel,
                                       const std::vector<std::string>& completedQuests) const;

    /**
     * @brief Find all paths between regions
     */
    [[nodiscard]] std::vector<TravelPath> FindAllPaths(const std::string& sourceRegionId,
                                                        const std::string& destRegionId,
                                                        int maxPaths = 5) const;

    /**
     * @brief Check if regions are connected
     */
    [[nodiscard]] bool AreRegionsConnected(const std::string& regionA,
                                            const std::string& regionB) const;

    /**
     * @brief Get network edges
     */
    [[nodiscard]] std::vector<PortalNetworkEdge> GetNetworkEdges() const;

    // ==================== Synchronization ====================

    void SyncToServer();
    void LoadFromServer();
    void ListenForChanges();
    void StopListening();

    // ==================== Callbacks ====================

    void OnPortalChanged(PortalChangedCallback callback);
    void OnTravelStarted(TravelStartCallback callback);
    void OnTravelCompleted(TravelCompleteCallback callback);
    void OnEncounter(EncounterCallback callback);

    // ==================== Configuration ====================

    void SetLocalPlayerId(const std::string& playerId) { m_localPlayerId = playerId; }
    [[nodiscard]] const PortalConfig& GetConfig() const { return m_config; }
    void SetConfig(const PortalConfig& config) { m_config = config; }

private:
    PortalManager() = default;
    ~PortalManager() = default;

    void UpdateTravelers(float deltaTime);
    void UpdateCooldowns(float deltaTime);
    void UpdateActivations(float deltaTime);
    void ProcessTravelCompletion(PortalTraveler& traveler);
    void ProcessEncounter(PortalTraveler& traveler);
    void RebuildNetworkGraph();
    std::string GenerateTravelId();

    bool m_initialized = false;
    PortalConfig m_config;
    std::string m_localPlayerId;

    std::unordered_map<std::string, PortalGate> m_portals;
    mutable std::mutex m_portalsMutex;

    std::unordered_map<std::string, PortalTraveler> m_travelers;
    std::mutex m_travelersMutex;

    std::vector<PortalNetworkEdge> m_networkEdges;
    std::unordered_map<std::string, std::vector<size_t>> m_regionToEdges;
    mutable std::mutex m_networkMutex;
    bool m_networkDirty = true;

    std::vector<PortalChangedCallback> m_portalCallbacks;
    std::vector<TravelStartCallback> m_travelStartCallbacks;
    std::vector<TravelCompleteCallback> m_travelCompleteCallbacks;
    std::vector<EncounterCallback> m_encounterCallbacks;
    std::mutex m_callbackMutex;

    uint64_t m_nextTravelId = 1;
};

} // namespace Vehement
