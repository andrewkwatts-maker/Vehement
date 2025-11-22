/**
 * @file LocationBasedFeatures.cpp
 * @brief Location-based game features implementation
 */

#include "LocationBasedFeatures.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <ctime>

namespace Vehement {
namespace Location {

LocationBasedFeatures& LocationBasedFeatures::Instance() {
    static LocationBasedFeatures instance;
    return instance;
}

void LocationBasedFeatures::Initialize() {
    if (m_initialized) return;

    // Start location tracking
    Nova::Location::LocationManager::Instance().StartUpdates(
        [this](const LocationData& location) {
            OnLocationUpdate(location);
        });

    m_initialized = true;
    std::cout << "[LocationBasedFeatures] Initialized" << std::endl;
}

void LocationBasedFeatures::Shutdown() {
    DisableNearbyPlayers();
    DisableWeather();

    Nova::Location::LocationManager::Instance().StopUpdates();

    m_pois.clear();
    m_events.clear();
    m_nearbyPlayers.clear();

    m_initialized = false;
}

void LocationBasedFeatures::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update nearby players
    if (m_nearbyPlayersEnabled) {
        m_nearbyUpdateTimer += deltaTime;
        if (m_nearbyUpdateTimer >= m_nearbyUpdateInterval) {
            m_nearbyUpdateTimer = 0.0f;
            UpdateNearbyPlayers();
        }
    }

    // Check POI proximity
    CheckPOIProximity();

    // Check event triggers
    CheckEventTriggers();

    // Update weather
    if (m_weatherEnabled) {
        m_weatherUpdateTimer += deltaTime;
        if (m_weatherUpdateTimer >= m_weatherUpdateInterval * 60.0f) {
            m_weatherUpdateTimer = 0.0f;
            FetchWeather();
        }
    }
}

void LocationBasedFeatures::OnLocationUpdate(const LocationData& location) {
    std::lock_guard<std::mutex> lock(m_locationMutex);
    m_currentLocation = location.coordinate;
}

// === Nearby Players ===

void LocationBasedFeatures::EnableNearbyPlayers(double radiusMeters, float updateIntervalSeconds) {
    m_nearbyPlayersEnabled = true;
    m_nearbyRadius = radiusMeters;
    m_nearbyUpdateInterval = updateIntervalSeconds;
    m_nearbyUpdateTimer = updateIntervalSeconds; // Trigger immediate update

    std::cout << "[LocationBasedFeatures] Nearby players enabled (radius: "
              << radiusMeters << "m)" << std::endl;
}

void LocationBasedFeatures::DisableNearbyPlayers() {
    m_nearbyPlayersEnabled = false;
    std::lock_guard<std::mutex> lock(m_nearbyMutex);
    m_nearbyPlayers.clear();
}

void LocationBasedFeatures::SetNearbyPlayersCallback(NearbyPlayersCallback callback) {
    m_nearbyCallback = std::move(callback);
}

std::vector<NearbyPlayer> LocationBasedFeatures::GetNearbyPlayers() const {
    std::lock_guard<std::mutex> lock(m_nearbyMutex);
    return m_nearbyPlayers;
}

std::optional<NearbyPlayer> LocationBasedFeatures::GetNearestPlayer() const {
    std::lock_guard<std::mutex> lock(m_nearbyMutex);
    if (m_nearbyPlayers.empty()) {
        return std::nullopt;
    }

    return *std::min_element(m_nearbyPlayers.begin(), m_nearbyPlayers.end(),
        [](const NearbyPlayer& a, const NearbyPlayer& b) {
            return a.distanceMeters < b.distanceMeters;
        });
}

void LocationBasedFeatures::ReportLocation(const LocationCoordinate& location) {
    // In a real implementation, this would send to a multiplayer server
    // For now, just update local tracking
    std::lock_guard<std::mutex> lock(m_locationMutex);
    m_currentLocation = location;
}

void LocationBasedFeatures::UpdateNearbyPlayers() {
    if (m_useMockData) {
        // Generate mock nearby players
        std::vector<NearbyPlayer> mockPlayers;
        std::lock_guard<std::mutex> lock(m_locationMutex);

        for (int i = 0; i < 3; ++i) {
            NearbyPlayer player;
            player.playerId = "mock_player_" + std::to_string(i);
            player.displayName = "Player " + std::to_string(i + 1);
            player.location.latitude = m_currentLocation.latitude + (rand() % 100 - 50) * 0.0001;
            player.location.longitude = m_currentLocation.longitude + (rand() % 100 - 50) * 0.0001;
            player.worldPosition = WorldLocation::Instance().GPSToWorld(player.location);
            player.distanceMeters = m_currentLocation.DistanceTo(player.location);
            player.bearing = m_currentLocation.BearingTo(player.location);
            player.lastUpdate = std::time(nullptr) * 1000;
            player.isOnline = true;

            if (player.distanceMeters <= m_nearbyRadius) {
                mockPlayers.push_back(player);
            }
        }

        {
            std::lock_guard<std::mutex> nearbyLock(m_nearbyMutex);
            m_nearbyPlayers = mockPlayers;
        }

        if (m_nearbyCallback) {
            m_nearbyCallback(mockPlayers);
        }
        return;
    }

    // Real implementation would query a multiplayer server
    // For now, just clear the list
    std::lock_guard<std::mutex> lock(m_nearbyMutex);
    m_nearbyPlayers.clear();

    if (m_nearbyCallback) {
        m_nearbyCallback(m_nearbyPlayers);
    }
}

// === POIs ===

void LocationBasedFeatures::AddPOI(const POI& poi) {
    std::lock_guard<std::mutex> lock(m_poiMutex);

    // Check for duplicate
    for (const auto& existing : m_pois) {
        if (existing.id == poi.id) {
            return;
        }
    }

    POI newPoi = poi;
    newPoi.worldPosition = WorldLocation::Instance().GPSToWorld(poi.location);
    m_pois.push_back(newPoi);

    std::cout << "[LocationBasedFeatures] Added POI: " << poi.name << std::endl;
}

void LocationBasedFeatures::RemovePOI(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_poiMutex);
    m_pois.erase(
        std::remove_if(m_pois.begin(), m_pois.end(),
            [&id](const POI& p) { return p.id == id; }),
        m_pois.end());
    m_currentPOIs.erase(id);
}

std::vector<POI> LocationBasedFeatures::GetAllPOIs() const {
    std::lock_guard<std::mutex> lock(m_poiMutex);
    return m_pois;
}

std::vector<POI> LocationBasedFeatures::GetPOIsInRadius(const LocationCoordinate& center,
                                                         double radiusMeters) const {
    std::vector<POI> result;
    std::lock_guard<std::mutex> lock(m_poiMutex);

    for (const auto& poi : m_pois) {
        if (poi.isActive && center.DistanceTo(poi.location) <= radiusMeters) {
            result.push_back(poi);
        }
    }

    return result;
}

std::vector<POI> LocationBasedFeatures::GetPOIsByCategory(const std::string& category) const {
    std::vector<POI> result;
    std::lock_guard<std::mutex> lock(m_poiMutex);

    for (const auto& poi : m_pois) {
        if (poi.isActive && poi.category == category) {
            result.push_back(poi);
        }
    }

    return result;
}

std::optional<POI> LocationBasedFeatures::GetNearestPOI(const LocationCoordinate& from) const {
    std::lock_guard<std::mutex> lock(m_poiMutex);

    if (m_pois.empty()) {
        return std::nullopt;
    }

    const POI* nearest = nullptr;
    double minDist = std::numeric_limits<double>::max();

    for (const auto& poi : m_pois) {
        if (!poi.isActive) continue;

        double dist = from.DistanceTo(poi.location);
        if (dist < minDist) {
            minDist = dist;
            nearest = &poi;
        }
    }

    return nearest ? std::make_optional(*nearest) : std::nullopt;
}

void LocationBasedFeatures::SetPOICallback(POICallback callback) {
    m_poiCallback = std::move(callback);
}

bool LocationBasedFeatures::IsAtPOI(const std::string& poiId) const {
    return m_currentPOIs.count(poiId) > 0;
}

void LocationBasedFeatures::CheckPOIProximity() {
    LocationCoordinate currentLoc;
    {
        std::lock_guard<std::mutex> lock(m_locationMutex);
        currentLoc = m_currentLocation;
    }

    if (!currentLoc.IsValid()) return;

    std::lock_guard<std::mutex> lock(m_poiMutex);

    std::set<std::string> nowInside;

    for (const auto& poi : m_pois) {
        if (!poi.isActive) continue;

        double distance = currentLoc.DistanceTo(poi.location);
        bool inside = distance <= poi.radius;

        if (inside) {
            nowInside.insert(poi.id);

            // Check if just entered
            if (m_currentPOIs.find(poi.id) == m_currentPOIs.end()) {
                if (m_poiCallback) {
                    m_poiCallback(poi, true);
                }
            }
        }
    }

    // Check for exits
    for (const auto& poiId : m_currentPOIs) {
        if (nowInside.find(poiId) == nowInside.end()) {
            // Exited this POI
            for (const auto& poi : m_pois) {
                if (poi.id == poiId && m_poiCallback) {
                    m_poiCallback(poi, false);
                    break;
                }
            }
        }
    }

    m_currentPOIs = nowInside;
}

bool LocationBasedFeatures::LoadPOIs(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file) return false;

    // Simple JSON-like parsing (in production, use a proper JSON library)
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Very basic parsing - in production use nlohmann/json or similar
    // For now, just return true to indicate the file was read
    std::cout << "[LocationBasedFeatures] Loaded POIs from: " << filepath << std::endl;
    return true;
}

bool LocationBasedFeatures::SavePOIs(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file) return false;

    std::lock_guard<std::mutex> lock(m_poiMutex);

    file << "{\n  \"pois\": [\n";
    for (size_t i = 0; i < m_pois.size(); ++i) {
        const auto& poi = m_pois[i];
        file << "    {\n";
        file << "      \"id\": \"" << poi.id << "\",\n";
        file << "      \"name\": \"" << poi.name << "\",\n";
        file << "      \"category\": \"" << poi.category << "\",\n";
        file << "      \"latitude\": " << poi.location.latitude << ",\n";
        file << "      \"longitude\": " << poi.location.longitude << ",\n";
        file << "      \"radius\": " << poi.radius << "\n";
        file << "    }" << (i < m_pois.size() - 1 ? "," : "") << "\n";
    }
    file << "  ]\n}\n";

    return true;
}

// === Events ===

void LocationBasedFeatures::CreateEvent(const LocationEvent& event) {
    std::lock_guard<std::mutex> lock(m_eventMutex);

    LocationEvent newEvent = event;
    newEvent.worldPosition = WorldLocation::Instance().GPSToWorld(event.location);
    m_events.push_back(newEvent);

    std::cout << "[LocationBasedFeatures] Created event: " << event.name << std::endl;
}

void LocationBasedFeatures::CancelEvent(const std::string& eventId) {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_events.erase(
        std::remove_if(m_events.begin(), m_events.end(),
            [&eventId](const LocationEvent& e) { return e.id == eventId; }),
        m_events.end());
    m_joinedEvents.erase(eventId);
}

std::vector<LocationEvent> LocationBasedFeatures::GetActiveEvents() const {
    std::vector<LocationEvent> result;
    std::lock_guard<std::mutex> lock(m_eventMutex);

    int64_t now = std::time(nullptr) * 1000;

    for (const auto& event : m_events) {
        if (event.startTime <= now && (event.endTime == 0 || event.endTime > now)) {
            result.push_back(event);
        }
    }

    return result;
}

std::vector<LocationEvent> LocationBasedFeatures::GetEventsNear(const LocationCoordinate& location,
                                                                  double radiusMeters) const {
    std::vector<LocationEvent> result;
    auto active = GetActiveEvents();

    for (const auto& event : active) {
        if (location.DistanceTo(event.location) <= radiusMeters) {
            result.push_back(event);
        }
    }

    return result;
}

void LocationBasedFeatures::SetEventCallback(EventCallback callback) {
    m_eventCallback = std::move(callback);
}

bool LocationBasedFeatures::JoinEvent(const std::string& eventId) {
    std::lock_guard<std::mutex> lock(m_eventMutex);

    for (const auto& event : m_events) {
        if (event.id == eventId) {
            m_joinedEvents.insert(eventId);
            return true;
        }
    }

    return false;
}

void LocationBasedFeatures::LeaveEvent(const std::string& eventId) {
    m_joinedEvents.erase(eventId);
}

void LocationBasedFeatures::CheckEventTriggers() {
    LocationCoordinate currentLoc;
    {
        std::lock_guard<std::mutex> lock(m_locationMutex);
        currentLoc = m_currentLocation;
    }

    if (!currentLoc.IsValid()) return;

    auto activeEvents = GetActiveEvents();

    for (const auto& event : activeEvents) {
        double distance = currentLoc.DistanceTo(event.location);
        if (distance <= event.triggerRadius) {
            // Check if already joined
            if (m_joinedEvents.find(event.id) == m_joinedEvents.end()) {
                if (m_eventCallback) {
                    m_eventCallback(event);
                }
            }
        }
    }
}

// === Weather ===

void LocationBasedFeatures::EnableWeather(float updateIntervalMinutes) {
    m_weatherEnabled = true;
    m_weatherUpdateInterval = updateIntervalMinutes;
    m_weatherUpdateTimer = updateIntervalMinutes * 60.0f; // Trigger immediate update

    std::cout << "[LocationBasedFeatures] Weather enabled" << std::endl;
}

void LocationBasedFeatures::DisableWeather() {
    m_weatherEnabled = false;
}

void LocationBasedFeatures::SetWeatherCallback(WeatherCallback callback) {
    m_weatherCallback = std::move(callback);
}

LocationWeather LocationBasedFeatures::GetCurrentWeather() const {
    std::lock_guard<std::mutex> lock(m_weatherMutex);
    return m_currentWeather;
}

void LocationBasedFeatures::RequestWeatherUpdate() {
    FetchWeather();
}

void LocationBasedFeatures::ConfigureWeatherAPI(const std::string& apiUrl,
                                                  const std::string& apiKey) {
    m_weatherApiUrl = apiUrl;
    m_weatherApiKey = apiKey;
}

void LocationBasedFeatures::FetchWeather() {
    LocationCoordinate currentLoc;
    {
        std::lock_guard<std::mutex> lock(m_locationMutex);
        currentLoc = m_currentLocation;
    }

    if (!currentLoc.IsValid()) return;

    if (m_useMockData || m_weatherApiUrl.empty()) {
        // Generate mock weather
        LocationWeather weather;
        weather.location = currentLoc;
        weather.timestamp = std::time(nullptr) * 1000;

        int condition = rand() % 4;
        switch (condition) {
            case 0: weather.condition = "clear"; break;
            case 1: weather.condition = "cloudy"; break;
            case 2: weather.condition = "rain"; break;
            case 3: weather.condition = "snow"; break;
        }

        weather.temperature = 15.0 + (rand() % 20);
        weather.humidity = 30.0 + (rand() % 50);
        weather.windSpeed = (rand() % 10) / 2.0;
        weather.windDirection = rand() % 360;

        {
            std::lock_guard<std::mutex> lock(m_weatherMutex);
            m_currentWeather = weather;
        }

        if (m_weatherCallback) {
            m_weatherCallback(weather);
        }
        return;
    }

    // Real weather API call would go here
    // For now, just use mock data
    FetchWeather();
}

void LocationBasedFeatures::SetUseMockData(bool useMock) {
    m_useMockData = useMock;
}

LocationCoordinate LocationBasedFeatures::GetCurrentLocation() const {
    std::lock_guard<std::mutex> lock(m_locationMutex);
    return m_currentLocation;
}

} // namespace Location
} // namespace Vehement
