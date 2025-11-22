/**
 * @file LocationBasedFeatures.hpp
 * @brief Location-based game features
 */

#pragma once

#include "WorldLocation.hpp"
#include "../../../engine/location/LocationManager.hpp"
#include "../../../engine/location/Geofence.hpp"
#include <vector>
#include <map>
#include <functional>

namespace Vehement {
namespace Location {

using namespace Nova::Platform;
using namespace Nova::Location;

/**
 * @brief Nearby player information
 */
struct NearbyPlayer {
    std::string playerId;
    std::string displayName;
    LocationCoordinate location;
    glm::vec3 worldPosition;
    double distanceMeters;
    double bearing;
    int64_t lastUpdate;
    bool isOnline = true;
};

/**
 * @brief Point of Interest
 */
struct POI {
    std::string id;
    std::string name;
    std::string category;       ///< e.g., "shop", "landmark", "event"
    std::string description;
    LocationCoordinate location;
    glm::vec3 worldPosition;
    double radius = 10.0;       ///< Interaction radius in meters
    std::string iconPath;
    std::map<std::string, std::string> metadata;
    bool isActive = true;
};

/**
 * @brief Location-based event
 */
struct LocationEvent {
    std::string id;
    std::string name;
    std::string type;           ///< e.g., "spawn", "treasure", "battle"
    LocationCoordinate location;
    glm::vec3 worldPosition;
    double triggerRadius;
    int64_t startTime;
    int64_t endTime;
    int maxParticipants = -1;   ///< -1 = unlimited
    std::map<std::string, std::string> eventData;
};

/**
 * @brief Weather data for location
 */
struct LocationWeather {
    std::string condition;      ///< e.g., "clear", "rain", "snow"
    double temperature;         ///< Celsius
    double humidity;            ///< 0-100%
    double windSpeed;           ///< m/s
    double windDirection;       ///< degrees
    int64_t timestamp;
    LocationCoordinate location;
};

// Callback types
using NearbyPlayersCallback = std::function<void(const std::vector<NearbyPlayer>& players)>;
using POICallback = std::function<void(const POI& poi, bool entered)>;
using EventCallback = std::function<void(const LocationEvent& event)>;
using WeatherCallback = std::function<void(const LocationWeather& weather)>;

/**
 * @brief Location-based features manager
 *
 * Provides:
 * - Nearby player discovery
 * - Point of Interest system
 * - Location-based events
 * - Weather based on location
 */
class LocationBasedFeatures {
public:
    /**
     * @brief Get singleton instance
     */
    static LocationBasedFeatures& Instance();

    // Delete copy/move
    LocationBasedFeatures(const LocationBasedFeatures&) = delete;
    LocationBasedFeatures& operator=(const LocationBasedFeatures&) = delete;

    /**
     * @brief Initialize the system
     */
    void Initialize();

    /**
     * @brief Shutdown the system
     */
    void Shutdown();

    /**
     * @brief Update (call each frame or periodically)
     */
    void Update(float deltaTime);

    // === Nearby Players ===

    /**
     * @brief Enable nearby player discovery
     * @param radiusMeters Search radius
     * @param updateIntervalSeconds How often to refresh
     */
    void EnableNearbyPlayers(double radiusMeters, float updateIntervalSeconds = 5.0f);

    /**
     * @brief Disable nearby player discovery
     */
    void DisableNearbyPlayers();

    /**
     * @brief Set callback for nearby player updates
     */
    void SetNearbyPlayersCallback(NearbyPlayersCallback callback);

    /**
     * @brief Get current nearby players
     */
    [[nodiscard]] std::vector<NearbyPlayer> GetNearbyPlayers() const;

    /**
     * @brief Get nearest player
     */
    [[nodiscard]] std::optional<NearbyPlayer> GetNearestPlayer() const;

    /**
     * @brief Report own location (for multiplayer)
     */
    void ReportLocation(const LocationCoordinate& location);

    // === Points of Interest ===

    /**
     * @brief Add a Point of Interest
     */
    void AddPOI(const POI& poi);

    /**
     * @brief Remove a POI
     */
    void RemovePOI(const std::string& id);

    /**
     * @brief Get all POIs
     */
    [[nodiscard]] std::vector<POI> GetAllPOIs() const;

    /**
     * @brief Get POIs in radius
     */
    [[nodiscard]] std::vector<POI> GetPOIsInRadius(const LocationCoordinate& center,
                                                    double radiusMeters) const;

    /**
     * @brief Get POIs by category
     */
    [[nodiscard]] std::vector<POI> GetPOIsByCategory(const std::string& category) const;

    /**
     * @brief Get nearest POI
     */
    [[nodiscard]] std::optional<POI> GetNearestPOI(const LocationCoordinate& from) const;

    /**
     * @brief Set callback for POI enter/exit
     */
    void SetPOICallback(POICallback callback);

    /**
     * @brief Check if player is at a POI
     */
    [[nodiscard]] bool IsAtPOI(const std::string& poiId) const;

    /**
     * @brief Load POIs from JSON file
     */
    bool LoadPOIs(const std::string& filepath);

    /**
     * @brief Save POIs to JSON file
     */
    bool SavePOIs(const std::string& filepath) const;

    // === Location Events ===

    /**
     * @brief Create a location-based event
     */
    void CreateEvent(const LocationEvent& event);

    /**
     * @brief Cancel an event
     */
    void CancelEvent(const std::string& eventId);

    /**
     * @brief Get active events
     */
    [[nodiscard]] std::vector<LocationEvent> GetActiveEvents() const;

    /**
     * @brief Get events near location
     */
    [[nodiscard]] std::vector<LocationEvent> GetEventsNear(const LocationCoordinate& location,
                                                           double radiusMeters) const;

    /**
     * @brief Set callback for event triggers
     */
    void SetEventCallback(EventCallback callback);

    /**
     * @brief Join an event
     */
    bool JoinEvent(const std::string& eventId);

    /**
     * @brief Leave an event
     */
    void LeaveEvent(const std::string& eventId);

    // === Weather ===

    /**
     * @brief Enable weather updates
     * @param updateIntervalMinutes How often to fetch weather
     */
    void EnableWeather(float updateIntervalMinutes = 30.0f);

    /**
     * @brief Disable weather updates
     */
    void DisableWeather();

    /**
     * @brief Set weather callback
     */
    void SetWeatherCallback(WeatherCallback callback);

    /**
     * @brief Get current weather
     */
    [[nodiscard]] LocationWeather GetCurrentWeather() const;

    /**
     * @brief Request weather update
     */
    void RequestWeatherUpdate();

    /**
     * @brief Set weather API configuration
     */
    void ConfigureWeatherAPI(const std::string& apiUrl, const std::string& apiKey);

    // === Configuration ===

    /**
     * @brief Set whether to use mock data
     */
    void SetUseMockData(bool useMock);

    /**
     * @brief Get current player location
     */
    [[nodiscard]] LocationCoordinate GetCurrentLocation() const;

private:
    LocationBasedFeatures() = default;
    ~LocationBasedFeatures() = default;

    void UpdateNearbyPlayers();
    void CheckPOIProximity();
    void CheckEventTriggers();
    void FetchWeather();
    void OnLocationUpdate(const LocationData& location);

    bool m_initialized = false;
    bool m_useMockData = false;

    // Current location
    LocationCoordinate m_currentLocation;
    std::mutex m_locationMutex;

    // Nearby players
    bool m_nearbyPlayersEnabled = false;
    double m_nearbyRadius = 1000.0;
    float m_nearbyUpdateInterval = 5.0f;
    float m_nearbyUpdateTimer = 0.0f;
    std::vector<NearbyPlayer> m_nearbyPlayers;
    NearbyPlayersCallback m_nearbyCallback;
    std::mutex m_nearbyMutex;

    // POIs
    std::vector<POI> m_pois;
    std::set<std::string> m_currentPOIs; // POIs player is currently in
    POICallback m_poiCallback;
    std::mutex m_poiMutex;

    // Events
    std::vector<LocationEvent> m_events;
    std::set<std::string> m_joinedEvents;
    EventCallback m_eventCallback;
    std::mutex m_eventMutex;

    // Weather
    bool m_weatherEnabled = false;
    float m_weatherUpdateInterval = 30.0f;
    float m_weatherUpdateTimer = 0.0f;
    LocationWeather m_currentWeather;
    WeatherCallback m_weatherCallback;
    std::string m_weatherApiUrl;
    std::string m_weatherApiKey;
    std::mutex m_weatherMutex;
};

} // namespace Location
} // namespace Vehement
