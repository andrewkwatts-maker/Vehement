/**
 * @file Geofence.cpp
 * @brief Software geofencing implementation
 */

#include "Geofence.hpp"
#include "LocationManager.hpp"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace Nova {
namespace Location {

// === PolygonRegion Implementation ===

bool PolygonRegion::ContainsPoint(const LocationCoordinate& point) const {
    if (vertices.size() < 3) return false;

    // Ray casting algorithm
    bool inside = false;
    size_t j = vertices.size() - 1;

    for (size_t i = 0; i < vertices.size(); ++i) {
        if (((vertices[i].latitude > point.latitude) != (vertices[j].latitude > point.latitude)) &&
            (point.longitude < (vertices[j].longitude - vertices[i].longitude) *
             (point.latitude - vertices[i].latitude) /
             (vertices[j].latitude - vertices[i].latitude) + vertices[i].longitude)) {
            inside = !inside;
        }
        j = i;
    }

    return inside;
}

std::pair<LocationCoordinate, LocationCoordinate> PolygonRegion::GetBounds() const {
    if (vertices.empty()) {
        return {{0, 0}, {0, 0}};
    }

    LocationCoordinate sw = vertices[0];
    LocationCoordinate ne = vertices[0];

    for (const auto& v : vertices) {
        sw.latitude = std::min(sw.latitude, v.latitude);
        sw.longitude = std::min(sw.longitude, v.longitude);
        ne.latitude = std::max(ne.latitude, v.latitude);
        ne.longitude = std::max(ne.longitude, v.longitude);
    }

    return {sw, ne};
}

LocationCoordinate PolygonRegion::GetCenter() const {
    if (vertices.empty()) return {0, 0};

    double sumLat = 0, sumLon = 0;
    for (const auto& v : vertices) {
        sumLat += v.latitude;
        sumLon += v.longitude;
    }

    return {sumLat / vertices.size(), sumLon / vertices.size()};
}

double PolygonRegion::GetArea() const {
    if (vertices.size() < 3) return 0.0;

    // Shoelace formula with spherical approximation
    double area = 0.0;
    size_t j = vertices.size() - 1;

    for (size_t i = 0; i < vertices.size(); ++i) {
        area += (vertices[j].longitude + vertices[i].longitude) *
                (vertices[j].latitude - vertices[i].latitude);
        j = i;
    }

    // Convert to square meters (approximate at equator)
    constexpr double DEG_TO_M = 111320.0;
    return std::abs(area) * 0.5 * DEG_TO_M * DEG_TO_M;
}

// === GeofenceManager Implementation ===

GeofenceManager& GeofenceManager::Instance() {
    static GeofenceManager instance;
    return instance;
}

void GeofenceManager::Initialize(const GeofenceConfig& config) {
    if (m_initialized) return;

    m_config = config;

    // Load persisted geofences
    if (config.persistGeofences) {
        LoadFromFile(config.persistPath);
    }

    m_initialized = true;
    std::cout << "[Geofence] Manager initialized" << std::endl;
}

void GeofenceManager::Shutdown() {
    StopAutoUpdates();

    // Save geofences if configured
    if (m_config.persistGeofences) {
        SaveToFile(m_config.persistPath);
    }

    RemoveAllRegions();
    m_initialized = false;
}

bool GeofenceManager::AddCircularRegion(const GeofenceRegion& region) {
    return AddCircularRegion(region, nullptr);
}

bool GeofenceManager::AddCircularRegion(const GeofenceRegion& region,
                                          GeofenceEventCallback callback) {
    if (region.identifier.empty() || !region.center.IsValid()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_regionMutex);

        // Check for duplicate
        for (const auto& r : m_circularRegions) {
            if (r.identifier == region.identifier) {
                return false;
            }
        }

        m_circularRegions.push_back(region);
        m_regionStates[region.identifier] = RegionState{};
    }

    if (callback) {
        AddRegionCallback(region.identifier, std::move(callback));
    }

    // Check initial state
    if (m_lastLocation.IsValid()) {
        bool inside = region.ContainsPoint(m_lastLocation.coordinate);
        m_regionStates[region.identifier].inside = inside;
        if (inside) {
            m_regionStates[region.identifier].enterTime =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
        }
    }

    std::cout << "[Geofence] Added circular region: " << region.identifier << std::endl;
    return true;
}

bool GeofenceManager::AddPolygonRegion(const PolygonRegion& region) {
    return AddPolygonRegion(region, nullptr);
}

bool GeofenceManager::AddPolygonRegion(const PolygonRegion& region,
                                         GeofenceEventCallback callback) {
    if (region.identifier.empty() || region.vertices.size() < 3) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_regionMutex);

        // Check for duplicate
        for (const auto& r : m_polygonRegions) {
            if (r.identifier == region.identifier) {
                return false;
            }
        }

        m_polygonRegions.push_back(region);
        m_regionStates[region.identifier] = RegionState{};
    }

    if (callback) {
        AddRegionCallback(region.identifier, std::move(callback));
    }

    // Check initial state
    if (m_lastLocation.IsValid()) {
        bool inside = region.ContainsPoint(m_lastLocation.coordinate);
        m_regionStates[region.identifier].inside = inside;
        if (inside) {
            m_regionStates[region.identifier].enterTime =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
        }
    }

    std::cout << "[Geofence] Added polygon region: " << region.identifier << std::endl;
    return true;
}

void GeofenceManager::RemoveRegion(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(m_regionMutex);

    m_circularRegions.erase(
        std::remove_if(m_circularRegions.begin(), m_circularRegions.end(),
            [&](const GeofenceRegion& r) { return r.identifier == identifier; }),
        m_circularRegions.end());

    m_polygonRegions.erase(
        std::remove_if(m_polygonRegions.begin(), m_polygonRegions.end(),
            [&](const PolygonRegion& r) { return r.identifier == identifier; }),
        m_polygonRegions.end());

    m_regionStates.erase(identifier);
    RemoveRegionCallbacks(identifier);
}

void GeofenceManager::RemoveAllRegions() {
    std::lock_guard<std::mutex> lock(m_regionMutex);
    m_circularRegions.clear();
    m_polygonRegions.clear();
    m_regionStates.clear();

    std::lock_guard<std::mutex> cbLock(m_callbackMutex);
    m_regionCallbacks.clear();
}

std::vector<GeofenceRegion> GeofenceManager::GetCircularRegions() const {
    std::lock_guard<std::mutex> lock(m_regionMutex);
    return m_circularRegions;
}

std::vector<PolygonRegion> GeofenceManager::GetPolygonRegions() const {
    std::lock_guard<std::mutex> lock(m_regionMutex);
    return m_polygonRegions;
}

bool GeofenceManager::HasRegion(const std::string& identifier) const {
    std::lock_guard<std::mutex> lock(m_regionMutex);

    for (const auto& r : m_circularRegions) {
        if (r.identifier == identifier) return true;
    }
    for (const auto& r : m_polygonRegions) {
        if (r.identifier == identifier) return true;
    }

    return false;
}

size_t GeofenceManager::GetRegionCount() const {
    std::lock_guard<std::mutex> lock(m_regionMutex);
    return m_circularRegions.size() + m_polygonRegions.size();
}

void GeofenceManager::SetGlobalCallback(GeofenceEventCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_globalCallback = std::move(callback);
}

void GeofenceManager::AddRegionCallback(const std::string& identifier,
                                          GeofenceEventCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_regionCallbacks[identifier].push_back(std::move(callback));
}

void GeofenceManager::RemoveRegionCallbacks(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_regionCallbacks.erase(identifier);
}

bool GeofenceManager::IsInsideRegion(const std::string& identifier) const {
    std::lock_guard<std::mutex> lock(m_regionMutex);
    auto it = m_regionStates.find(identifier);
    return it != m_regionStates.end() && it->second.inside;
}

std::vector<std::string> GeofenceManager::GetRegionsContaining(
    const LocationCoordinate& point) const {

    std::vector<std::string> result;
    std::lock_guard<std::mutex> lock(m_regionMutex);

    for (const auto& region : m_circularRegions) {
        if (region.ContainsPoint(point)) {
            result.push_back(region.identifier);
        }
    }

    for (const auto& region : m_polygonRegions) {
        if (region.ContainsPoint(point)) {
            result.push_back(region.identifier);
        }
    }

    return result;
}

double GeofenceManager::GetDistanceToNearestRegion(const LocationCoordinate& point,
                                                     std::string* outRegionId) const {
    double minDistance = std::numeric_limits<double>::max();
    std::string nearestId;

    std::lock_guard<std::mutex> lock(m_regionMutex);

    for (const auto& region : m_circularRegions) {
        double dist = DistanceToCircularRegion(point, region);
        if (dist < minDistance) {
            minDistance = dist;
            nearestId = region.identifier;
        }
    }

    for (const auto& region : m_polygonRegions) {
        double dist = DistanceToPolygonRegion(point, region);
        if (dist < minDistance) {
            minDistance = dist;
            nearestId = region.identifier;
        }
    }

    if (outRegionId) {
        *outRegionId = nearestId;
    }

    return minDistance == std::numeric_limits<double>::max() ? -1 : minDistance;
}

int64_t GeofenceManager::GetDwellTime(const std::string& identifier) const {
    std::lock_guard<std::mutex> lock(m_regionMutex);

    auto it = m_regionStates.find(identifier);
    if (it == m_regionStates.end() || !it->second.inside) {
        return -1;
    }

    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return now - it->second.enterTime;
}

void GeofenceManager::Update(const LocationData& location) {
    if (!location.IsValid()) return;

    m_lastLocation = location;
    CheckGeofences(location);
}

void GeofenceManager::StartAutoUpdates() {
    if (m_autoUpdating) return;

    LocationManager::Instance().StartUpdates([this](const LocationData& location) {
        Update(location);
    });

    m_autoUpdating = true;
    std::cout << "[Geofence] Auto updates started" << std::endl;
}

void GeofenceManager::StopAutoUpdates() {
    if (!m_autoUpdating) return;

    LocationManager::Instance().StopUpdates();
    m_autoUpdating = false;
}

bool GeofenceManager::SaveToFile(const std::string& filepath) const {
    std::string path = filepath.empty() ? m_config.persistPath : filepath;

    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    std::lock_guard<std::mutex> lock(m_regionMutex);

    // Write circular regions
    uint32_t circularCount = static_cast<uint32_t>(m_circularRegions.size());
    file.write(reinterpret_cast<char*>(&circularCount), sizeof(circularCount));

    for (const auto& r : m_circularRegions) {
        uint32_t idLen = static_cast<uint32_t>(r.identifier.size());
        file.write(reinterpret_cast<char*>(&idLen), sizeof(idLen));
        file.write(r.identifier.c_str(), idLen);
        file.write(reinterpret_cast<const char*>(&r.center.latitude), sizeof(double));
        file.write(reinterpret_cast<const char*>(&r.center.longitude), sizeof(double));
        file.write(reinterpret_cast<const char*>(&r.radiusMeters), sizeof(double));
        file.write(reinterpret_cast<const char*>(&r.notifyOnEntry), sizeof(bool));
        file.write(reinterpret_cast<const char*>(&r.notifyOnExit), sizeof(bool));
        file.write(reinterpret_cast<const char*>(&r.notifyOnDwell), sizeof(bool));
        file.write(reinterpret_cast<const char*>(&r.dwellTimeMs), sizeof(int));
    }

    // Write polygon regions
    uint32_t polygonCount = static_cast<uint32_t>(m_polygonRegions.size());
    file.write(reinterpret_cast<char*>(&polygonCount), sizeof(polygonCount));

    for (const auto& r : m_polygonRegions) {
        uint32_t idLen = static_cast<uint32_t>(r.identifier.size());
        file.write(reinterpret_cast<char*>(&idLen), sizeof(idLen));
        file.write(r.identifier.c_str(), idLen);

        uint32_t vertexCount = static_cast<uint32_t>(r.vertices.size());
        file.write(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));

        for (const auto& v : r.vertices) {
            file.write(reinterpret_cast<const char*>(&v.latitude), sizeof(double));
            file.write(reinterpret_cast<const char*>(&v.longitude), sizeof(double));
        }

        file.write(reinterpret_cast<const char*>(&r.notifyOnEntry), sizeof(bool));
        file.write(reinterpret_cast<const char*>(&r.notifyOnExit), sizeof(bool));
        file.write(reinterpret_cast<const char*>(&r.notifyOnDwell), sizeof(bool));
        file.write(reinterpret_cast<const char*>(&r.dwellTimeMs), sizeof(int));
    }

    return true;
}

bool GeofenceManager::LoadFromFile(const std::string& filepath) {
    std::string path = filepath.empty() ? m_config.persistPath : filepath;

    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    try {
        std::lock_guard<std::mutex> lock(m_regionMutex);

        m_circularRegions.clear();
        m_polygonRegions.clear();
        m_regionStates.clear();

        // Read circular regions
        uint32_t circularCount;
        file.read(reinterpret_cast<char*>(&circularCount), sizeof(circularCount));

        for (uint32_t i = 0; i < circularCount; ++i) {
            GeofenceRegion r;

            uint32_t idLen;
            file.read(reinterpret_cast<char*>(&idLen), sizeof(idLen));
            r.identifier.resize(idLen);
            file.read(&r.identifier[0], idLen);

            file.read(reinterpret_cast<char*>(&r.center.latitude), sizeof(double));
            file.read(reinterpret_cast<char*>(&r.center.longitude), sizeof(double));
            file.read(reinterpret_cast<char*>(&r.radiusMeters), sizeof(double));
            file.read(reinterpret_cast<char*>(&r.notifyOnEntry), sizeof(bool));
            file.read(reinterpret_cast<char*>(&r.notifyOnExit), sizeof(bool));
            file.read(reinterpret_cast<char*>(&r.notifyOnDwell), sizeof(bool));
            file.read(reinterpret_cast<char*>(&r.dwellTimeMs), sizeof(int));

            m_circularRegions.push_back(r);
            m_regionStates[r.identifier] = RegionState{};
        }

        // Read polygon regions
        uint32_t polygonCount;
        file.read(reinterpret_cast<char*>(&polygonCount), sizeof(polygonCount));

        for (uint32_t i = 0; i < polygonCount; ++i) {
            PolygonRegion r;

            uint32_t idLen;
            file.read(reinterpret_cast<char*>(&idLen), sizeof(idLen));
            r.identifier.resize(idLen);
            file.read(&r.identifier[0], idLen);

            uint32_t vertexCount;
            file.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));

            r.vertices.resize(vertexCount);
            for (uint32_t j = 0; j < vertexCount; ++j) {
                file.read(reinterpret_cast<char*>(&r.vertices[j].latitude), sizeof(double));
                file.read(reinterpret_cast<char*>(&r.vertices[j].longitude), sizeof(double));
            }

            file.read(reinterpret_cast<char*>(&r.notifyOnEntry), sizeof(bool));
            file.read(reinterpret_cast<char*>(&r.notifyOnExit), sizeof(bool));
            file.read(reinterpret_cast<char*>(&r.notifyOnDwell), sizeof(bool));
            file.read(reinterpret_cast<char*>(&r.dwellTimeMs), sizeof(int));

            m_polygonRegions.push_back(r);
            m_regionStates[r.identifier] = RegionState{};
        }

        return true;
    } catch (...) {
        return false;
    }
}

// === Private Implementation ===

void GeofenceManager::CheckGeofences(const LocationData& location) {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::lock_guard<std::mutex> lock(m_regionMutex);

    // Check circular regions
    for (const auto& region : m_circularRegions) {
        double distance = location.coordinate.DistanceTo(region.center);
        bool wasInside = m_regionStates[region.identifier].inside;

        // Apply hysteresis
        double effectiveRadius = region.radiusMeters;
        if (wasInside) {
            effectiveRadius += m_config.hysteresisMeters;
        } else {
            effectiveRadius -= m_config.hysteresisMeters;
        }

        bool isInside = distance <= effectiveRadius;

        if (isInside && !wasInside) {
            // Enter event
            m_regionStates[region.identifier].inside = true;
            m_regionStates[region.identifier].enterTime = now;
            m_regionStates[region.identifier].dwellTriggered = false;

            if (region.notifyOnEntry) {
                TriggerEvent(region.identifier, GeofenceEvent::Enter, location);
            }
        } else if (!isInside && wasInside) {
            // Exit event
            m_regionStates[region.identifier].inside = false;

            if (region.notifyOnExit) {
                int64_t dwellTime = now - m_regionStates[region.identifier].enterTime;
                TriggerEvent(region.identifier, GeofenceEvent::Exit, location, dwellTime);
            }
        } else if (isInside && region.notifyOnDwell && m_config.enableDwellDetection) {
            // Check for dwell
            auto& state = m_regionStates[region.identifier];
            if (!state.dwellTriggered) {
                int64_t dwellTime = now - state.enterTime;
                if (dwellTime >= region.dwellTimeMs) {
                    state.dwellTriggered = true;
                    TriggerEvent(region.identifier, GeofenceEvent::Dwell, location, dwellTime);
                }
            }
        }
    }

    // Check polygon regions
    for (const auto& region : m_polygonRegions) {
        bool wasInside = m_regionStates[region.identifier].inside;
        bool isInside = region.ContainsPoint(location.coordinate);

        if (isInside && !wasInside) {
            m_regionStates[region.identifier].inside = true;
            m_regionStates[region.identifier].enterTime = now;
            m_regionStates[region.identifier].dwellTriggered = false;

            if (region.notifyOnEntry) {
                TriggerEvent(region.identifier, GeofenceEvent::Enter, location);
            }
        } else if (!isInside && wasInside) {
            m_regionStates[region.identifier].inside = false;

            if (region.notifyOnExit) {
                int64_t dwellTime = now - m_regionStates[region.identifier].enterTime;
                TriggerEvent(region.identifier, GeofenceEvent::Exit, location, dwellTime);
            }
        } else if (isInside && region.notifyOnDwell && m_config.enableDwellDetection) {
            auto& state = m_regionStates[region.identifier];
            if (!state.dwellTriggered) {
                int64_t dwellTime = now - state.enterTime;
                if (dwellTime >= region.dwellTimeMs) {
                    state.dwellTriggered = true;
                    TriggerEvent(region.identifier, GeofenceEvent::Dwell, location, dwellTime);
                }
            }
        }
    }
}

void GeofenceManager::TriggerEvent(const std::string& regionId, GeofenceEvent event,
                                     const LocationData& location, int64_t dwellTime) {
    GeofenceEventData eventData;
    eventData.regionId = regionId;
    eventData.event = event;
    eventData.location = location;
    eventData.timestamp = location.timestamp;
    eventData.dwellTime = dwellTime;

    // Calculate distance from edge (simplified)
    eventData.distanceFromEdge = 0; // Would need full implementation

    // Call global callback
    GeofenceEventCallback globalCb;
    std::vector<GeofenceEventCallback> regionCbs;

    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        globalCb = m_globalCallback;
        auto it = m_regionCallbacks.find(regionId);
        if (it != m_regionCallbacks.end()) {
            regionCbs = it->second;
        }
    }

    if (globalCb) {
        globalCb(eventData);
    }

    for (const auto& cb : regionCbs) {
        if (cb) cb(eventData);
    }
}

double GeofenceManager::DistanceToCircularRegion(const LocationCoordinate& point,
                                                   const GeofenceRegion& region) const {
    double distance = point.DistanceTo(region.center);
    return std::max(0.0, distance - region.radiusMeters);
}

double GeofenceManager::DistanceToPolygonRegion(const LocationCoordinate& point,
                                                  const PolygonRegion& region) const {
    if (region.ContainsPoint(point)) {
        return 0.0;
    }

    // Simple approximation: distance to center
    return point.DistanceTo(region.GetCenter());
}

} // namespace Location
} // namespace Nova
