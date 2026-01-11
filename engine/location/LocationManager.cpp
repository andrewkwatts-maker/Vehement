/**
 * @file LocationManager.cpp
 * @brief High-level location manager implementation
 */

#include "LocationManager.hpp"
#include <cmath>
#include <numeric>
#include <iostream>

namespace Nova {
namespace Location {

// Earth radius in meters
static constexpr double EARTH_RADIUS_M = 6371000.0;

LocationManager& LocationManager::Instance() {
    static LocationManager instance;
    return instance;
}

void LocationManager::Initialize(const LocationManagerConfig& config) {
    if (m_initialized) {
        std::cout << "[LocationManager] Already initialized" << std::endl;
        return;
    }

    m_config = config;

    // Create platform-specific location service
    m_service = Platform::CreateLocationService();

    if (m_service) {
        // Set up error callback
        m_service->SetErrorCallback([](LocationError error, const std::string& message) {
            std::cerr << "[LocationManager] Error: " << message << std::endl;
        });
    }

    m_initialized = true;
    std::cout << "[LocationManager] Initialized" << std::endl;
}

void LocationManager::Shutdown() {
    if (!m_initialized) return;

    StopUpdates();

    m_service.reset();
    m_history.clear();
    m_totalDistance = 0.0;

    m_initialized = false;
    std::cout << "[LocationManager] Shutdown" << std::endl;
}

bool LocationManager::RequestPermission(bool alwaysAccess) {
    if (!m_service) return false;
    return m_service->RequestPermission(alwaysAccess);
}

bool LocationManager::HasPermission() const {
    if (!m_service) return false;
    return m_service->HasPermission();
}

void LocationManager::GetLocation(LocationCallback callback, int64_t maxAgeMs) {
    if (!callback) return;

    // Check cache first
    if (maxAgeMs > 0) {
        auto cached = GetCachedLocation(maxAgeMs);
        if (cached) {
            callback(*cached);
            return;
        }
    }

    // Request fresh location
    if (m_service) {
        m_service->RequestSingleUpdate(
            [this, callback](const LocationData& location) {
                OnLocationUpdate(location);
                callback(location);
            },
            [callback](LocationError error, const std::string& message) {
                std::cerr << "[LocationManager] Location request failed: " << message << std::endl;
                callback(LocationData{}); // Return invalid location
            }
        );
    } else {
        callback(LocationData{});
    }
}

std::optional<LocationData> LocationManager::GetCachedLocation(int64_t maxAgeMs) const {
    if (!m_cachedLocation.IsValid()) {
        return std::nullopt;
    }

    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_cacheTime).count();

    if (age <= maxAgeMs) {
        return m_cachedLocation;
    }

    return std::nullopt;
}

void LocationManager::StartUpdates(LocationCallback callback) {
    if (!m_service) {
        std::cerr << "[LocationManager] No platform service available" << std::endl;
        return;
    }

    m_userCallback = std::move(callback);

    m_service->StartUpdates([this](const LocationData& location) {
        OnLocationUpdate(location);
        if (m_userCallback) {
            m_userCallback(location);
        }
    });
}

void LocationManager::StopUpdates() {
    if (m_service) {
        m_service->StopUpdates();
    }
    m_userCallback = nullptr;
}

bool LocationManager::IsUpdating() const {
    return m_service && m_service->IsUpdating();
}

// === Static calculation methods ===

double LocationManager::CalculateDistance(const LocationCoordinate& from,
                                           const LocationCoordinate& to) {
    return from.DistanceTo(to);
}

double LocationManager::CalculateBearing(const LocationCoordinate& from,
                                          const LocationCoordinate& to) {
    return from.BearingTo(to);
}

LocationCoordinate LocationManager::CalculateDestination(const LocationCoordinate& from,
                                                          double bearingDegrees,
                                                          double distanceMeters) {
    double lat1 = from.latitude * M_PI / 180.0;
    double lon1 = from.longitude * M_PI / 180.0;
    double brng = bearingDegrees * M_PI / 180.0;
    double d = distanceMeters / EARTH_RADIUS_M;

    double lat2 = std::asin(std::sin(lat1) * std::cos(d) +
                           std::cos(lat1) * std::sin(d) * std::cos(brng));

    double lon2 = lon1 + std::atan2(std::sin(brng) * std::sin(d) * std::cos(lat1),
                                     std::cos(d) - std::sin(lat1) * std::sin(lat2));

    LocationCoordinate result;
    result.latitude = lat2 * 180.0 / M_PI;
    result.longitude = std::fmod(lon2 * 180.0 / M_PI + 540.0, 360.0) - 180.0;

    return result;
}

LocationCoordinate LocationManager::CalculateMidpoint(const LocationCoordinate& a,
                                                       const LocationCoordinate& b) {
    double lat1 = a.latitude * M_PI / 180.0;
    double lon1 = a.longitude * M_PI / 180.0;
    double lat2 = b.latitude * M_PI / 180.0;
    double dLon = (b.longitude - a.longitude) * M_PI / 180.0;

    double bx = std::cos(lat2) * std::cos(dLon);
    double by = std::cos(lat2) * std::sin(dLon);

    double lat3 = std::atan2(std::sin(lat1) + std::sin(lat2),
                             std::sqrt((std::cos(lat1) + bx) * (std::cos(lat1) + bx) + by * by));
    double lon3 = lon1 + std::atan2(by, std::cos(lat1) + bx);

    LocationCoordinate result;
    result.latitude = lat3 * 180.0 / M_PI;
    result.longitude = std::fmod(lon3 * 180.0 / M_PI + 540.0, 360.0) - 180.0;

    return result;
}

bool LocationManager::IsPointInBounds(const LocationCoordinate& point,
                                       const LocationCoordinate& sw,
                                       const LocationCoordinate& ne) {
    bool latInRange = point.latitude >= sw.latitude && point.latitude <= ne.latitude;

    bool lonInRange;
    if (sw.longitude <= ne.longitude) {
        // Normal case
        lonInRange = point.longitude >= sw.longitude && point.longitude <= ne.longitude;
    } else {
        // Crosses antimeridian
        lonInRange = point.longitude >= sw.longitude || point.longitude <= ne.longitude;
    }

    return latInRange && lonInRange;
}

// === Movement detection ===

MovementState LocationManager::GetMovementState() const {
    return m_movementState;
}

double LocationManager::GetEstimatedSpeed() const {
    return m_estimatedSpeed;
}

double LocationManager::GetEstimatedHeading() const {
    return m_estimatedHeading;
}

void LocationManager::SetMovementCallback(MovementCallback callback) {
    m_movementCallback = std::move(callback);
}

// === History access ===

std::vector<LocationHistoryEntry> LocationManager::GetHistory() const {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    return std::vector<LocationHistoryEntry>(m_history.begin(), m_history.end());
}

double LocationManager::GetTotalDistance() const {
    return m_totalDistance;
}

void LocationManager::ClearHistory() {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    m_history.clear();
    m_totalDistance = 0.0;
}

// === Settings ===

void LocationManager::SetAccuracy(LocationAccuracy accuracy) {
    if (m_service) {
        m_service->SetDesiredAccuracy(accuracy);
    }
}

void LocationManager::SetDistanceFilter(double meters) {
    if (m_service) {
        m_service->SetDistanceFilter(meters);
    }
}

// === Private methods ===

void LocationManager::OnLocationUpdate(const LocationData& location) {
    if (!location.IsValid()) return;

    // Update cache
    m_cachedLocation = location;
    m_cacheTime = std::chrono::steady_clock::now();

    // Calculate distance from previous location
    double distance = 0.0;
    double speed = location.speed >= 0 ? location.speed : 0.0;

    {
        std::lock_guard<std::mutex> lock(m_historyMutex);

        if (!m_history.empty()) {
            const auto& prev = m_history.back();
            distance = location.coordinate.DistanceTo(prev.location.coordinate);

            // Filter out GPS jitter for stationary detection
            if (distance < m_config.minDistanceUpdateM) {
                // Update timestamp but don't add new entry
                return;
            }

            // Estimate speed if not provided
            if (m_config.enableSpeedEstimation && speed <= 0) {
                int64_t timeDiff = location.timestamp - prev.timestamp;
                if (timeDiff > 0) {
                    speed = (distance * 1000.0) / timeDiff; // m/s
                }
            }

            // Estimate heading
            m_estimatedHeading = location.coordinate.latitude != prev.location.coordinate.latitude ||
                                 location.coordinate.longitude != prev.location.coordinate.longitude
                ? prev.location.coordinate.BearingTo(location.coordinate)
                : m_estimatedHeading;
        }

        // Add to history
        LocationHistoryEntry entry;
        entry.location = location;
        entry.timestamp = location.timestamp;
        entry.distanceFromPrevious = distance;
        entry.speedEstimate = speed;

        m_history.push_back(entry);
        m_totalDistance += distance;

        // Trim history if needed
        while (m_history.size() > static_cast<size_t>(m_config.historyMaxSize)) {
            m_history.pop_front();
        }
    }

    m_estimatedSpeed = speed;

    // Update movement state
    if (m_config.enableMovementDetection) {
        UpdateMovementState();
    }
}

void LocationManager::UpdateMovementState() {
    MovementState oldState = m_movementState;
    MovementState newState = EstimateMovementFromSpeed(m_estimatedSpeed);

    // Additional check for stationary - must be still for some time
    if (newState == MovementState::Stationary) {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        if (m_history.size() >= 3) {
            // Check if we've moved significantly in the last N entries
            double recentDistance = 0.0;
            int64_t recentTime = 0;
            auto it = m_history.rbegin();
            auto start = it->timestamp;

            for (int i = 0; i < 5 && it != m_history.rend(); ++i, ++it) {
                recentDistance += it->distanceFromPrevious;
                recentTime = start - it->timestamp;
            }

            if (recentDistance > m_config.stationaryThresholdM ||
                recentTime < m_config.stationaryTimeMs) {
                newState = MovementState::Walking; // Not truly stationary yet
            }
        }
    }

    m_movementState = newState;

    // Notify if state changed
    if (oldState != newState && m_movementCallback) {
        m_movementCallback(newState, oldState);
    }
}

MovementState LocationManager::EstimateMovementFromSpeed(double speed) const {
    if (speed < 0.5) return MovementState::Stationary;      // < 1.8 km/h
    if (speed < 2.5) return MovementState::Walking;         // < 9 km/h
    if (speed < 6.0) return MovementState::Running;         // < 21.6 km/h
    if (speed < 40.0) return MovementState::Driving;        // < 144 km/h
    return MovementState::HighSpeed;
}

} // namespace Location
} // namespace Nova
