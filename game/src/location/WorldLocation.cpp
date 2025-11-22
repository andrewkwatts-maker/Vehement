/**
 * @file WorldLocation.cpp
 * @brief GPS to game world coordinate mapping implementation
 */

#include "WorldLocation.hpp"
#include "../../../engine/location/LocationManager.hpp"
#include <cmath>
#include <iostream>

namespace Vehement {
namespace Location {

// Constants
static constexpr double EARTH_RADIUS_M = 6371000.0;
static constexpr double DEG_TO_RAD = M_PI / 180.0;
static constexpr double RAD_TO_DEG = 180.0 / M_PI;
static constexpr double METERS_PER_DEGREE_LAT = 111320.0;

WorldLocation& WorldLocation::Instance() {
    static WorldLocation instance;
    return instance;
}

void WorldLocation::Initialize(const WorldMappingConfig& config) {
    m_config = config;
    m_initialized = true;

    std::cout << "[WorldLocation] Initialized with origin: ("
              << config.origin.latitude << ", " << config.origin.longitude << ")"
              << " scale: " << config.metersPerUnit << " m/unit" << std::endl;
}

void WorldLocation::SetOrigin(const LocationCoordinate& origin) {
    m_config.origin = origin;
}

void WorldLocation::SetScale(double metersPerUnit) {
    m_config.metersPerUnit = metersPerUnit;
}

void WorldLocation::SetCoordinateSystem(CoordinateSystem system) {
    m_config.coordinateSystem = system;
}

void WorldLocation::SetWorldRotation(double degrees) {
    m_config.worldRotation = degrees;
}

glm::vec3 WorldLocation::GPSToWorld(const LocationCoordinate& gps) const {
    return GPSToWorld(gps, 0.0);
}

glm::vec2 WorldLocation::GPSToWorld2D(const LocationCoordinate& gps) const {
    glm::vec3 world3d = GPSToWorld(gps);
    return glm::vec2(world3d.x, world3d.z);
}

glm::vec3 WorldLocation::GPSToWorld(const LocationCoordinate& gps, double altitude) const {
    glm::vec2 projected;

    switch (m_config.coordinateSystem) {
        case CoordinateSystem::Mercator:
            projected = ProjectToMercator(gps);
            break;
        case CoordinateSystem::UTM:
            // Simplified UTM - for accurate results use a proper library
            projected = ProjectToCartesian(gps);
            break;
        case CoordinateSystem::Cartesian:
        default:
            projected = ProjectToCartesian(gps);
            break;
    }

    // Apply rotation if configured
    if (m_config.worldRotation != 0.0) {
        double rad = -m_config.worldRotation * DEG_TO_RAD;
        double cosR = std::cos(rad);
        double sinR = std::sin(rad);
        double newX = projected.x * cosR - projected.y * sinR;
        double newY = projected.x * sinR + projected.y * cosR;
        projected.x = static_cast<float>(newX);
        projected.y = static_cast<float>(newY);
    }

    // Apply grid snapping
    glm::vec3 result(projected.x, 0.0f, projected.y);

    if (m_config.mapAltitudeToY) {
        result.y = static_cast<float>(altitude * m_config.altitudeScale / m_config.metersPerUnit);
    }

    if (m_config.enableGridSnapping) {
        result = SnapToGrid(result);
    }

    // Apply bounds clamping
    if (m_config.enableBounds) {
        result = ClampToBounds(result);
    }

    return result;
}

LocationCoordinate WorldLocation::WorldToGPS(const glm::vec3& worldPos) const {
    double altitude;
    return WorldToGPS(worldPos, altitude);
}

LocationCoordinate WorldLocation::WorldToGPS(const glm::vec2& worldPos) const {
    return WorldToGPS(glm::vec3(worldPos.x, 0, worldPos.y));
}

LocationCoordinate WorldLocation::WorldToGPS(const glm::vec3& worldPos, double& outAltitude) const {
    glm::vec2 pos2d(worldPos.x, worldPos.z);

    // Reverse rotation
    if (m_config.worldRotation != 0.0) {
        double rad = m_config.worldRotation * DEG_TO_RAD;
        double cosR = std::cos(rad);
        double sinR = std::sin(rad);
        double newX = pos2d.x * cosR - pos2d.y * sinR;
        double newY = pos2d.x * sinR + pos2d.y * cosR;
        pos2d.x = static_cast<float>(newX);
        pos2d.y = static_cast<float>(newY);
    }

    LocationCoordinate gps;

    switch (m_config.coordinateSystem) {
        case CoordinateSystem::Mercator:
            gps = UnprojectFromMercator(pos2d);
            break;
        case CoordinateSystem::UTM:
        case CoordinateSystem::Cartesian:
        default:
            gps = UnprojectFromCartesian(pos2d);
            break;
    }

    // Extract altitude from Y
    if (m_config.mapAltitudeToY && m_config.altitudeScale != 0) {
        outAltitude = worldPos.y * m_config.metersPerUnit / m_config.altitudeScale;
    } else {
        outAltitude = 0.0;
    }

    return gps;
}

double WorldLocation::WorldDistanceMeters(const glm::vec3& a, const glm::vec3& b) const {
    glm::vec3 diff = b - a;
    double distance2d = std::sqrt(diff.x * diff.x + diff.z * diff.z);
    double distance3d = std::sqrt(distance2d * distance2d + diff.y * diff.y);
    return distance3d * m_config.metersPerUnit;
}

double WorldLocation::GPSDistance(const glm::vec3& a, const glm::vec3& b) const {
    LocationCoordinate gpsA = WorldToGPS(a);
    LocationCoordinate gpsB = WorldToGPS(b);
    return gpsA.DistanceTo(gpsB);
}

double WorldLocation::CalculateBearing(const glm::vec3& from, const glm::vec3& to) const {
    LocationCoordinate gpsFrom = WorldToGPS(from);
    LocationCoordinate gpsTo = WorldToGPS(to);
    return gpsFrom.BearingTo(gpsTo);
}

void WorldLocation::SetGridSnapping(bool enable, double gridSize) {
    m_config.enableGridSnapping = enable;
    m_config.gridSizeUnits = gridSize;
}

glm::vec3 WorldLocation::SnapToGrid(const glm::vec3& worldPos) const {
    if (!m_config.enableGridSnapping || m_config.gridSizeUnits <= 0) {
        return worldPos;
    }

    float grid = static_cast<float>(m_config.gridSizeUnits);
    return glm::vec3(
        std::round(worldPos.x / grid) * grid,
        worldPos.y, // Don't snap Y
        std::round(worldPos.z / grid) * grid
    );
}

LocationCoordinate WorldLocation::SnapGPSToGrid(const LocationCoordinate& gps) const {
    glm::vec3 world = GPSToWorld(gps);
    glm::vec3 snapped = SnapToGrid(world);
    return WorldToGPS(snapped);
}

void WorldLocation::SetBounds(const glm::vec2& min, const glm::vec2& max) {
    m_config.enableBounds = true;
    m_config.boundsMin = min;
    m_config.boundsMax = max;
}

bool WorldLocation::IsInBounds(const glm::vec3& worldPos) const {
    if (!m_config.enableBounds) return true;

    return worldPos.x >= m_config.boundsMin.x && worldPos.x <= m_config.boundsMax.x &&
           worldPos.z >= m_config.boundsMin.y && worldPos.z <= m_config.boundsMax.y;
}

bool WorldLocation::IsGPSInBounds(const LocationCoordinate& gps) const {
    return IsInBounds(GPSToWorld(gps));
}

glm::vec3 WorldLocation::ClampToBounds(const glm::vec3& worldPos) const {
    if (!m_config.enableBounds) return worldPos;

    return glm::vec3(
        std::clamp(worldPos.x, m_config.boundsMin.x, m_config.boundsMax.x),
        worldPos.y,
        std::clamp(worldPos.z, m_config.boundsMin.y, m_config.boundsMax.y)
    );
}

glm::ivec2 WorldLocation::GetTileCoords(const glm::vec3& worldPos, float tileSize) const {
    return glm::ivec2(
        static_cast<int>(std::floor(worldPos.x / tileSize)),
        static_cast<int>(std::floor(worldPos.z / tileSize))
    );
}

glm::ivec2 WorldLocation::GetTileCoordsForGPS(const LocationCoordinate& gps, float tileSize) const {
    return GetTileCoords(GPSToWorld(gps), tileSize);
}

glm::vec3 WorldLocation::GetTileCenter(const glm::ivec2& tile, float tileSize) const {
    return glm::vec3(
        (tile.x + 0.5f) * tileSize,
        0.0f,
        (tile.y + 0.5f) * tileSize
    );
}

LocationCoordinate WorldLocation::GetTileCenterGPS(const glm::ivec2& tile, float tileSize) const {
    return WorldToGPS(GetTileCenter(tile, tileSize));
}

void WorldLocation::StartTracking(std::function<void(const GameWorldPosition&)> callback) {
    if (m_tracking) {
        StopTracking();
    }

    m_trackingCallback = std::move(callback);

    Nova::Location::LocationManager::Instance().StartUpdates(
        [this](const LocationData& location) {
            GameWorldPosition pos;
            pos.gpsCoord = location.coordinate;
            pos.altitude = location.altitude;
            pos.worldPosition = GPSToWorld(location.coordinate, location.altitude);
            pos.isValid = location.IsValid();

            m_currentPosition = pos;

            if (m_trackingCallback) {
                m_trackingCallback(pos);
            }
        });

    m_tracking = true;
    std::cout << "[WorldLocation] Tracking started" << std::endl;
}

void WorldLocation::StopTracking() {
    if (!m_tracking) return;

    Nova::Location::LocationManager::Instance().StopUpdates();
    m_tracking = false;
    std::cout << "[WorldLocation] Tracking stopped" << std::endl;
}

GameWorldPosition WorldLocation::GetCurrentPosition() const {
    return m_currentPosition;
}

// === Private Projection Methods ===

glm::vec2 WorldLocation::ProjectToCartesian(const LocationCoordinate& gps) const {
    // Simple equirectangular projection
    // Good for small areas, fast computation

    double dLat = gps.latitude - m_config.origin.latitude;
    double dLon = gps.longitude - m_config.origin.longitude;

    // Convert to meters
    double metersNorth = dLat * METERS_PER_DEGREE_LAT;
    double metersEast = dLon * METERS_PER_DEGREE_LAT * std::cos(m_config.origin.latitude * DEG_TO_RAD);

    // Convert to game units
    double unitsX = metersEast / m_config.metersPerUnit;
    double unitsZ = metersNorth / m_config.metersPerUnit;

    return glm::vec2(static_cast<float>(unitsX), static_cast<float>(unitsZ));
}

glm::vec2 WorldLocation::ProjectToMercator(const LocationCoordinate& gps) const {
    // Web Mercator projection (EPSG:3857)

    auto toMercator = [](double lat, double lon) -> std::pair<double, double> {
        double x = lon * DEG_TO_RAD * EARTH_RADIUS_M;
        double y = std::log(std::tan(M_PI / 4.0 + lat * DEG_TO_RAD / 2.0)) * EARTH_RADIUS_M;
        return {x, y};
    };

    auto [originX, originY] = toMercator(m_config.origin.latitude, m_config.origin.longitude);
    auto [pointX, pointY] = toMercator(gps.latitude, gps.longitude);

    double metersX = pointX - originX;
    double metersY = pointY - originY;

    return glm::vec2(
        static_cast<float>(metersX / m_config.metersPerUnit),
        static_cast<float>(metersY / m_config.metersPerUnit)
    );
}

LocationCoordinate WorldLocation::UnprojectFromCartesian(const glm::vec2& pos) const {
    double metersEast = pos.x * m_config.metersPerUnit;
    double metersNorth = pos.y * m_config.metersPerUnit;

    double dLon = metersEast / (METERS_PER_DEGREE_LAT * std::cos(m_config.origin.latitude * DEG_TO_RAD));
    double dLat = metersNorth / METERS_PER_DEGREE_LAT;

    LocationCoordinate result;
    result.latitude = m_config.origin.latitude + dLat;
    result.longitude = m_config.origin.longitude + dLon;

    return result;
}

LocationCoordinate WorldLocation::UnprojectFromMercator(const glm::vec2& pos) const {
    auto toMercator = [](double lat, double lon) -> std::pair<double, double> {
        double x = lon * DEG_TO_RAD * EARTH_RADIUS_M;
        double y = std::log(std::tan(M_PI / 4.0 + lat * DEG_TO_RAD / 2.0)) * EARTH_RADIUS_M;
        return {x, y};
    };

    auto [originX, originY] = toMercator(m_config.origin.latitude, m_config.origin.longitude);

    double mercX = originX + pos.x * m_config.metersPerUnit;
    double mercY = originY + pos.y * m_config.metersPerUnit;

    // Inverse Mercator
    double lon = mercX / EARTH_RADIUS_M * RAD_TO_DEG;
    double lat = (2.0 * std::atan(std::exp(mercY / EARTH_RADIUS_M)) - M_PI / 2.0) * RAD_TO_DEG;

    LocationCoordinate result;
    result.latitude = lat;
    result.longitude = lon;

    return result;
}

} // namespace Location
} // namespace Vehement
