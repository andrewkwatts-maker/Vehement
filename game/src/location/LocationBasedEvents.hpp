#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include "../geodata/GeoTypes.hpp"
#include "../rts/world/WorldRegion.hpp"

namespace Vehement {

/**
 * @brief Location event trigger type
 */
enum class LocationTriggerType : uint8_t {
    Enter,          // Enter area
    Exit,           // Exit area
    Proximity,      // Within range
    Dwell,          // Stay in area for duration
    Speed,          // Moving at certain speed
    TimeOfDay,      // Time-based at location
    Weather,        // Weather condition at location
    POI,            // Near point of interest
    Custom          // Custom condition
};

/**
 * @brief Location event definition
 */
struct LocationEvent {
    std::string eventId;
    std::string name;
    std::string description;
    LocationTriggerType triggerType = LocationTriggerType::Enter;

    // Location
    Geo::GeoCoordinate location;
    double triggerRadius = 100.0;       // meters
    std::string regionId;               // Optional: only trigger in this region

    // Trigger conditions
    float dwellTimeSeconds = 0.0f;      // For Dwell type
    float minSpeed = 0.0f;              // For Speed type
    float maxSpeed = 100.0f;
    int startHour = 0;                  // For TimeOfDay type
    int endHour = 24;
    std::string requiredWeather;        // For Weather type
    std::string poiCategory;            // For POI type
    std::string customCondition;        // For Custom type

    // Cooldown
    float cooldownSeconds = 3600.0f;    // 1 hour default
    bool oneTimeOnly = false;

    // Requirements
    int minPlayerLevel = 1;
    std::vector<std::string> requiredQuests;
    std::vector<std::string> requiredItems;

    // Rewards/Effects
    std::unordered_map<std::string, int> rewards;
    std::vector<std::string> spawnUnits;
    std::string triggerQuest;
    std::string triggerDialogue;
    float experienceReward = 0.0f;

    // State
    bool active = true;
    int64_t lastTriggeredTimestamp = 0;
    int triggerCount = 0;

    [[nodiscard]] nlohmann::json ToJson() const;
    static LocationEvent FromJson(const nlohmann::json& j);
};

/**
 * @brief Real-world POI integration
 */
struct RealWorldPOI {
    std::string poiId;
    std::string name;
    std::string category;       // restaurant, landmark, park, etc.
    Geo::GeoCoordinate location;
    std::string address;
    float rating = 0.0f;
    bool verified = false;

    // Game integration
    std::string gameEventId;
    std::string gameBonusType;
    float gameBonusValue = 0.0f;
    std::vector<std::string> specialRewards;

    [[nodiscard]] nlohmann::json ToJson() const;
    static RealWorldPOI FromJson(const nlohmann::json& j);
};

/**
 * @brief Time-based regional event
 */
struct RegionalTimeEvent {
    std::string eventId;
    std::string regionId;
    std::string name;
    std::string description;

    // Schedule
    int startHour = 0;
    int endHour = 24;
    std::vector<int> activeDays;  // 0=Sunday, 6=Saturday
    bool useLocalTime = true;

    // Effects
    float resourceMultiplier = 1.0f;
    float experienceMultiplier = 1.0f;
    float dangerMultiplier = 1.0f;
    std::vector<std::string> specialSpawns;
    std::string weatherOverride;

    // State
    bool currentlyActive = false;

    [[nodiscard]] nlohmann::json ToJson() const;
    static RegionalTimeEvent FromJson(const nlohmann::json& j);
};

/**
 * @brief Event trigger record
 */
struct EventTriggerRecord {
    std::string eventId;
    std::string playerId;
    int64_t timestamp = 0;
    Geo::GeoCoordinate location;
    bool rewarded = false;
    std::unordered_map<std::string, int> rewardsGiven;
};

/**
 * @brief Configuration for location-based events
 */
struct LocationEventsConfig {
    float checkInterval = 1.0f;
    double proximityCheckRadius = 1000.0;
    int maxActiveEvents = 50;
    bool enablePOIIntegration = true;
    float poiRefreshInterval = 3600.0f;
    bool enableTimeEvents = true;
    float timeEventCheckInterval = 60.0f;
};

/**
 * @brief Manager for GPS-triggered events
 */
class LocationBasedEvents {
public:
    using EventTriggeredCallback = std::function<void(const LocationEvent& event, const std::string& playerId)>;
    using POIDiscoveredCallback = std::function<void(const RealWorldPOI& poi)>;
    using TimeEventCallback = std::function<void(const RegionalTimeEvent& event, bool starting)>;

    [[nodiscard]] static LocationBasedEvents& Instance();

    LocationBasedEvents(const LocationBasedEvents&) = delete;
    LocationBasedEvents& operator=(const LocationBasedEvents&) = delete;

    [[nodiscard]] bool Initialize(const LocationEventsConfig& config = {});
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    void Update(float deltaTime);

    // ==================== Location Events ====================

    /**
     * @brief Register location event
     */
    void RegisterEvent(const LocationEvent& event);

    /**
     * @brief Unregister event
     */
    void UnregisterEvent(const std::string& eventId);

    /**
     * @brief Get event by ID
     */
    [[nodiscard]] const LocationEvent* GetEvent(const std::string& eventId) const;

    /**
     * @brief Get events near location
     */
    [[nodiscard]] std::vector<LocationEvent> GetEventsNearLocation(
        const Geo::GeoCoordinate& coord, double radiusMeters) const;

    /**
     * @brief Get events in region
     */
    [[nodiscard]] std::vector<LocationEvent> GetRegionEvents(const std::string& regionId) const;

    /**
     * @brief Check and trigger events for player position
     */
    void CheckEventTriggers(const std::string& playerId, const Geo::GeoCoordinate& position);

    /**
     * @brief Manually trigger event
     */
    bool TriggerEvent(const std::string& eventId, const std::string& playerId);

    // ==================== POI Integration ====================

    /**
     * @brief Register real-world POI
     */
    void RegisterPOI(const RealWorldPOI& poi);

    /**
     * @brief Get nearby POIs
     */
    [[nodiscard]] std::vector<RealWorldPOI> GetNearbyPOIs(
        const Geo::GeoCoordinate& coord, double radiusMeters) const;

    /**
     * @brief Get POI by category
     */
    [[nodiscard]] std::vector<RealWorldPOI> GetPOIsByCategory(const std::string& category) const;

    /**
     * @brief Check POI interaction
     */
    void CheckPOIInteraction(const std::string& playerId, const Geo::GeoCoordinate& position);

    /**
     * @brief Refresh POI data from external source
     */
    void RefreshPOIData(const Geo::GeoCoordinate& center, double radiusMeters);

    // ==================== Time-Based Events ====================

    /**
     * @brief Register time-based event
     */
    void RegisterTimeEvent(const RegionalTimeEvent& event);

    /**
     * @brief Get active time events
     */
    [[nodiscard]] std::vector<RegionalTimeEvent> GetActiveTimeEvents() const;

    /**
     * @brief Get time events for region
     */
    [[nodiscard]] std::vector<RegionalTimeEvent> GetRegionTimeEvents(const std::string& regionId) const;

    /**
     * @brief Check and update time events
     */
    void UpdateTimeEvents();

    // ==================== Enter/Exit Events ====================

    /**
     * @brief Notify region entry
     */
    void OnRegionEnter(const std::string& playerId, const std::string& regionId);

    /**
     * @brief Notify region exit
     */
    void OnRegionExit(const std::string& playerId, const std::string& regionId);

    // ==================== History ====================

    /**
     * @brief Get trigger history for player
     */
    [[nodiscard]] std::vector<EventTriggerRecord> GetPlayerTriggerHistory(
        const std::string& playerId) const;

    /**
     * @brief Check if event triggered recently
     */
    [[nodiscard]] bool HasTriggeredRecently(const std::string& eventId,
                                             const std::string& playerId) const;

    // ==================== Callbacks ====================

    void OnEventTriggered(EventTriggeredCallback callback);
    void OnPOIDiscovered(POIDiscoveredCallback callback);
    void OnTimeEvent(TimeEventCallback callback);

    // ==================== Configuration ====================

    void SetLocalPlayerId(const std::string& playerId) { m_localPlayerId = playerId; }
    [[nodiscard]] const LocationEventsConfig& GetConfig() const { return m_config; }

private:
    LocationBasedEvents() = default;
    ~LocationBasedEvents() = default;

    void ProcessEventTriggers(const std::string& playerId, const Geo::GeoCoordinate& position);
    bool CheckEventConditions(const LocationEvent& event, const std::string& playerId,
                              const Geo::GeoCoordinate& position);
    void ApplyEventRewards(const LocationEvent& event, const std::string& playerId);
    bool IsOnCooldown(const std::string& eventId, const std::string& playerId) const;
    void RecordTrigger(const std::string& eventId, const std::string& playerId,
                       const Geo::GeoCoordinate& location);

    bool m_initialized = false;
    LocationEventsConfig m_config;
    std::string m_localPlayerId;

    // Location events
    std::unordered_map<std::string, LocationEvent> m_events;
    mutable std::mutex m_eventsMutex;

    // POIs
    std::unordered_map<std::string, RealWorldPOI> m_pois;
    mutable std::mutex m_poiMutex;

    // Time events
    std::unordered_map<std::string, RegionalTimeEvent> m_timeEvents;
    mutable std::mutex m_timeEventsMutex;

    // Trigger history
    std::unordered_map<std::string, std::vector<EventTriggerRecord>> m_triggerHistory;
    mutable std::mutex m_historyMutex;

    // Dwell tracking
    std::unordered_map<std::string, std::pair<std::string, float>> m_dwellTracking;  // eventId -> (playerId, time)

    // Player region tracking
    std::unordered_map<std::string, std::string> m_playerRegions;

    // Callbacks
    std::vector<EventTriggeredCallback> m_eventCallbacks;
    std::vector<POIDiscoveredCallback> m_poiCallbacks;
    std::vector<TimeEventCallback> m_timeEventCallbacks;
    std::mutex m_callbackMutex;

    // Timers
    float m_eventCheckTimer = 0.0f;
    float m_poiRefreshTimer = 0.0f;
    float m_timeEventTimer = 0.0f;
};

} // namespace Vehement
