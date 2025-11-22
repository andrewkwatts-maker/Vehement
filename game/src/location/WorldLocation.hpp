/**
 * @file WorldLocation.hpp
 * @brief GPS to game world coordinate mapping
 */

#pragma once

#include "../../../engine/platform/LocationService.hpp"
#include <glm/glm.hpp>
#include <string>
#include <functional>

namespace Vehement {
namespace Location {

using namespace Nova::Platform;

/**
 * @brief Coordinate system types for game world mapping
 */
enum class CoordinateSystem {
    Cartesian,          ///< Standard X/Y grid centered on origin
    Mercator,           ///< Web Mercator projection (like web maps)
    UTM,                ///< Universal Transverse Mercator
    Custom              ///< Custom projection defined by user
};

/**
 * @brief World mapping configuration
 */
struct WorldMappingConfig {
    CoordinateSystem coordinateSystem = CoordinateSystem::Cartesian;

    // Origin point (GPS coordinates that map to world (0,0))
    LocationCoordinate origin;

    // Scale factor (meters per game unit)
    double metersPerUnit = 1.0;

    // Grid snapping
    bool enableGridSnapping = false;
    double gridSizeUnits = 1.0;

    // Bounds (optional, for clamping)
    bool enableBounds = false;
    glm::vec2 boundsMin{-10000, -10000};
    glm::vec2 boundsMax{10000, 10000};

    // Y-axis mapping (for 3D games)
    bool mapAltitudeToY = true;
    double altitudeScale = 1.0;

    // Rotation (degrees, clockwise from north)
    double worldRotation = 0.0;
};

/**
 * @brief Game world location with GPS backing
 */
struct GameWorldPosition {
    glm::vec3 worldPosition;        ///< Position in game world coordinates
    LocationCoordinate gpsCoord;    ///< Original GPS coordinates
    double altitude = 0.0;          ///< Altitude in meters
    bool isValid = false;
};

/**
 * @brief GPS to game world coordinate mapping
 *
 * Converts between GPS coordinates and game world coordinates.
 * Supports various projection systems and configurable origin/scale.
 */
class WorldLocation {
public:
    /**
     * @brief Get singleton instance
     */
    static WorldLocation& Instance();

    // Delete copy/move
    WorldLocation(const WorldLocation&) = delete;
    WorldLocation& operator=(const WorldLocation&) = delete;

    /**
     * @brief Initialize with mapping configuration
     */
    void Initialize(const WorldMappingConfig& config);

    /**
     * @brief Set the world origin (GPS coordinates for world 0,0)
     */
    void SetOrigin(const LocationCoordinate& origin);

    /**
     * @brief Get current origin
     */
    [[nodiscard]] LocationCoordinate GetOrigin() const { return m_config.origin; }

    /**
     * @brief Set meters per game unit scale
     */
    void SetScale(double metersPerUnit);

    /**
     * @brief Get current scale
     */
    [[nodiscard]] double GetScale() const { return m_config.metersPerUnit; }

    /**
     * @brief Set coordinate system
     */
    void SetCoordinateSystem(CoordinateSystem system);

    /**
     * @brief Set world rotation
     */
    void SetWorldRotation(double degrees);

    // === Coordinate Conversion ===

    /**
     * @brief Convert GPS coordinates to game world position
     */
    [[nodiscard]] glm::vec3 GPSToWorld(const LocationCoordinate& gps) const;

    /**
     * @brief Convert GPS coordinates to game world position (2D)
     */
    [[nodiscard]] glm::vec2 GPSToWorld2D(const LocationCoordinate& gps) const;

    /**
     * @brief Convert GPS with altitude to world position
     */
    [[nodiscard]] glm::vec3 GPSToWorld(const LocationCoordinate& gps, double altitude) const;

    /**
     * @brief Convert game world position to GPS coordinates
     */
    [[nodiscard]] LocationCoordinate WorldToGPS(const glm::vec3& worldPos) const;

    /**
     * @brief Convert game world position to GPS (2D)
     */
    [[nodiscard]] LocationCoordinate WorldToGPS(const glm::vec2& worldPos) const;

    /**
     * @brief Convert game world position to GPS with altitude
     * @param worldPos World position
     * @param outAltitude Output altitude in meters
     */
    [[nodiscard]] LocationCoordinate WorldToGPS(const glm::vec3& worldPos, double& outAltitude) const;

    // === Distance and Direction ===

    /**
     * @brief Calculate distance between two world positions in meters
     */
    [[nodiscard]] double WorldDistanceMeters(const glm::vec3& a, const glm::vec3& b) const;

    /**
     * @brief Calculate GPS distance between two world positions
     */
    [[nodiscard]] double GPSDistance(const glm::vec3& a, const glm::vec3& b) const;

    /**
     * @brief Calculate bearing from one position to another (degrees, 0=North)
     */
    [[nodiscard]] double CalculateBearing(const glm::vec3& from, const glm::vec3& to) const;

    // === Grid Snapping ===

    /**
     * @brief Enable/disable grid snapping
     */
    void SetGridSnapping(bool enable, double gridSize = 1.0);

    /**
     * @brief Snap world position to grid
     */
    [[nodiscard]] glm::vec3 SnapToGrid(const glm::vec3& worldPos) const;

    /**
     * @brief Snap GPS coordinate to grid
     */
    [[nodiscard]] LocationCoordinate SnapGPSToGrid(const LocationCoordinate& gps) const;

    // === Bounds ===

    /**
     * @brief Set world bounds
     */
    void SetBounds(const glm::vec2& min, const glm::vec2& max);

    /**
     * @brief Check if position is within bounds
     */
    [[nodiscard]] bool IsInBounds(const glm::vec3& worldPos) const;

    /**
     * @brief Check if GPS coordinate is within bounds
     */
    [[nodiscard]] bool IsGPSInBounds(const LocationCoordinate& gps) const;

    /**
     * @brief Clamp position to bounds
     */
    [[nodiscard]] glm::vec3 ClampToBounds(const glm::vec3& worldPos) const;

    // === Tile/Chunk Mapping ===

    /**
     * @brief Get tile/chunk coordinates for world position
     * @param worldPos World position
     * @param tileSize Size of tiles in world units
     */
    [[nodiscard]] glm::ivec2 GetTileCoords(const glm::vec3& worldPos, float tileSize) const;

    /**
     * @brief Get tile/chunk coordinates for GPS position
     */
    [[nodiscard]] glm::ivec2 GetTileCoordsForGPS(const LocationCoordinate& gps, float tileSize) const;

    /**
     * @brief Get world position for tile center
     */
    [[nodiscard]] glm::vec3 GetTileCenter(const glm::ivec2& tile, float tileSize) const;

    /**
     * @brief Get GPS coordinates for tile center
     */
    [[nodiscard]] LocationCoordinate GetTileCenterGPS(const glm::ivec2& tile, float tileSize) const;

    // === Current Location ===

    /**
     * @brief Start tracking current GPS location
     */
    void StartTracking(std::function<void(const GameWorldPosition&)> callback);

    /**
     * @brief Stop tracking
     */
    void StopTracking();

    /**
     * @brief Get current tracked position
     */
    [[nodiscard]] GameWorldPosition GetCurrentPosition() const;

    /**
     * @brief Check if tracking is active
     */
    [[nodiscard]] bool IsTracking() const { return m_tracking; }

    // === Configuration Access ===

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const WorldMappingConfig& GetConfig() const { return m_config; }

private:
    WorldLocation() = default;
    ~WorldLocation() = default;

    // Projection helpers
    glm::vec2 ProjectToCartesian(const LocationCoordinate& gps) const;
    glm::vec2 ProjectToMercator(const LocationCoordinate& gps) const;
    LocationCoordinate UnprojectFromCartesian(const glm::vec2& pos) const;
    LocationCoordinate UnprojectFromMercator(const glm::vec2& pos) const;

    WorldMappingConfig m_config;
    bool m_initialized = false;
    bool m_tracking = false;

    GameWorldPosition m_currentPosition;
    std::function<void(const GameWorldPosition&)> m_trackingCallback;
};

} // namespace Location
} // namespace Vehement
