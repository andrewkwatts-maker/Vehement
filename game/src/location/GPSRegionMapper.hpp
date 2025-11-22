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
 * @brief GPS accuracy level
 */
enum class GPSAccuracy : uint8_t {
    Unknown,
    Low,        // >100m accuracy
    Medium,     // 10-100m accuracy
    High,       // 1-10m accuracy
    Precise     // <1m accuracy (RTK)
};

/**
 * @brief GPS position update
 */
struct GPSPosition {
    Geo::GeoCoordinate coordinate;
    double altitude = 0.0;      // meters
    double accuracy = 0.0;      // meters
    double speed = 0.0;         // m/s
    double heading = 0.0;       // degrees
    int64_t timestamp = 0;
    GPSAccuracy accuracyLevel = GPSAccuracy::Unknown;
    bool valid = false;

    [[nodiscard]] nlohmann::json ToJson() const;
    static GPSPosition FromJson(const nlohmann::json& j);
};

/**
 * @brief Region mapping result
 */
struct RegionMappingResult {
    std::string regionId;
    std::string regionName;
    double distanceToCenter = 0.0;  // meters
    double distanceToBorder = 0.0;  // meters
    bool insideRegion = false;
    float confidence = 0.0f;        // 0-1

    [[nodiscard]] nlohmann::json ToJson() const;
    static RegionMappingResult FromJson(const nlohmann::json& j);
};

/**
 * @brief Nearby portal info
 */
struct NearbyPortal {
    std::string portalId;
    std::string portalName;
    std::string destinationRegionId;
    double distance = 0.0;          // meters
    double bearing = 0.0;           // degrees from north
    bool active = true;
    bool accessible = true;

    [[nodiscard]] nlohmann::json ToJson() const;
    static NearbyPortal FromJson(const nlohmann::json& j);
};

/**
 * @brief GPS mapper configuration
 */
struct GPSMapperConfig {
    float updateInterval = 1.0f;
    double regionSearchRadius = 50000.0;  // meters
    double portalSearchRadius = 5000.0;   // meters
    int maxNearbyPortals = 10;
    bool interpolatePosition = true;
    float interpolationSpeed = 5.0f;
    double boundaryMargin = 100.0;  // meters for boundary detection
};

/**
 * @brief Maps GPS coordinates to game regions
 */
class GPSRegionMapper {
public:
    using PositionCallback = std::function<void(const GPSPosition& position)>;
    using RegionChangeCallback = std::function<void(const std::string& oldRegion, const std::string& newRegion)>;
    using PortalNearbyCallback = std::function<void(const NearbyPortal& portal)>;
    using BoundaryCallback = std::function<void(const std::string& regionId, bool entering)>;

    [[nodiscard]] static GPSRegionMapper& Instance();

    GPSRegionMapper(const GPSRegionMapper&) = delete;
    GPSRegionMapper& operator=(const GPSRegionMapper&) = delete;

    [[nodiscard]] bool Initialize(const GPSMapperConfig& config = {});
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    void Update(float deltaTime);

    // ==================== GPS Position ====================

    /**
     * @brief Update GPS position from device
     */
    void UpdateGPSPosition(const GPSPosition& position);

    /**
     * @brief Get current GPS position
     */
    [[nodiscard]] GPSPosition GetCurrentPosition() const;

    /**
     * @brief Get interpolated position
     */
    [[nodiscard]] Geo::GeoCoordinate GetInterpolatedPosition() const;

    /**
     * @brief Check if GPS is available
     */
    [[nodiscard]] bool IsGPSAvailable() const { return m_gpsAvailable; }

    /**
     * @brief Get GPS accuracy level
     */
    [[nodiscard]] GPSAccuracy GetAccuracyLevel() const;

    // ==================== Region Mapping ====================

    /**
     * @brief Map coordinate to region
     */
    [[nodiscard]] RegionMappingResult MapToRegion(const Geo::GeoCoordinate& coord) const;

    /**
     * @brief Get current region ID
     */
    [[nodiscard]] std::string GetCurrentRegionId() const;

    /**
     * @brief Get regions near coordinate
     */
    [[nodiscard]] std::vector<RegionMappingResult> GetNearbyRegions(
        const Geo::GeoCoordinate& coord, double radiusMeters) const;

    /**
     * @brief Check if coordinate is in any region
     */
    [[nodiscard]] bool IsInRegion(const Geo::GeoCoordinate& coord) const;

    /**
     * @brief Calculate distance to region border
     */
    [[nodiscard]] double GetDistanceToRegionBorder(const std::string& regionId,
                                                    const Geo::GeoCoordinate& coord) const;

    // ==================== Portal Finding ====================

    /**
     * @brief Find nearest portal
     */
    [[nodiscard]] NearbyPortal FindNearestPortal(const Geo::GeoCoordinate& coord) const;

    /**
     * @brief Find nearby portals
     */
    [[nodiscard]] std::vector<NearbyPortal> FindNearbyPortals(
        const Geo::GeoCoordinate& coord, double radiusMeters) const;

    /**
     * @brief Get portals to specific region
     */
    [[nodiscard]] std::vector<NearbyPortal> FindPortalsToRegion(
        const Geo::GeoCoordinate& from, const std::string& targetRegionId) const;

    // ==================== Distance Calculations ====================

    /**
     * @brief Calculate distance between coordinates
     */
    [[nodiscard]] double CalculateDistance(const Geo::GeoCoordinate& from,
                                            const Geo::GeoCoordinate& to) const;

    /**
     * @brief Calculate bearing between coordinates
     */
    [[nodiscard]] double CalculateBearing(const Geo::GeoCoordinate& from,
                                           const Geo::GeoCoordinate& to) const;

    /**
     * @brief Convert GPS to local world coordinates
     */
    [[nodiscard]] glm::vec3 GPSToWorldPosition(const Geo::GeoCoordinate& coord,
                                                const std::string& regionId) const;

    /**
     * @brief Convert local world to GPS coordinates
     */
    [[nodiscard]] Geo::GeoCoordinate WorldPositionToGPS(const glm::vec3& worldPos,
                                                         const std::string& regionId) const;

    // ==================== Movement Tracking ====================

    /**
     * @brief Get movement speed
     */
    [[nodiscard]] double GetCurrentSpeed() const;

    /**
     * @brief Get movement heading
     */
    [[nodiscard]] double GetCurrentHeading() const;

    /**
     * @brief Get total distance traveled
     */
    [[nodiscard]] double GetDistanceTraveled() const { return m_totalDistance; }

    /**
     * @brief Reset distance counter
     */
    void ResetDistanceCounter();

    // ==================== Callbacks ====================

    void OnPositionUpdated(PositionCallback callback);
    void OnRegionChanged(RegionChangeCallback callback);
    void OnPortalNearby(PortalNearbyCallback callback);
    void OnBoundaryApproach(BoundaryCallback callback);

    // ==================== Configuration ====================

    [[nodiscard]] const GPSMapperConfig& GetConfig() const { return m_config; }
    void SetConfig(const GPSMapperConfig& config) { m_config = config; }

private:
    GPSRegionMapper() = default;
    ~GPSRegionMapper() = default;

    void UpdateInterpolation(float deltaTime);
    void CheckRegionChange();
    void CheckNearbyPortals();
    void CheckBoundaries();
    GPSAccuracy DetermineAccuracyLevel(double accuracy) const;

    bool m_initialized = false;
    GPSMapperConfig m_config;
    bool m_gpsAvailable = false;

    // Position state
    GPSPosition m_currentPosition;
    GPSPosition m_previousPosition;
    Geo::GeoCoordinate m_interpolatedPosition;
    mutable std::mutex m_positionMutex;

    // Region state
    std::string m_currentRegionId;
    std::vector<NearbyPortal> m_nearbyPortals;
    mutable std::mutex m_regionMutex;

    // Movement tracking
    double m_totalDistance = 0.0;
    double m_currentSpeed = 0.0;
    double m_currentHeading = 0.0;

    // Callbacks
    std::vector<PositionCallback> m_positionCallbacks;
    std::vector<RegionChangeCallback> m_regionChangeCallbacks;
    std::vector<PortalNearbyCallback> m_portalNearbyCallbacks;
    std::vector<BoundaryCallback> m_boundaryCallbacks;
    std::mutex m_callbackMutex;

    // Timers
    float m_updateTimer = 0.0f;
    float m_portalCheckTimer = 0.0f;

    // Portal notification cooldown
    std::unordered_map<std::string, int64_t> m_portalNotificationTimes;
};

} // namespace Vehement
