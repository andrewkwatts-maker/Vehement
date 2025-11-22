/**
 * @file Geofence.hpp
 * @brief Software geofencing system
 */

#pragma once

#include "../platform/LocationService.hpp"
#include <vector>
#include <map>
#include <functional>
#include <chrono>
#include <mutex>

namespace Nova {
namespace Location {

using namespace Platform;

/**
 * @brief Polygon geofence region
 */
struct PolygonRegion {
    std::string identifier;
    std::vector<LocationCoordinate> vertices;   ///< Polygon vertices (clockwise)
    bool notifyOnEntry = true;
    bool notifyOnExit = true;
    bool notifyOnDwell = false;
    int dwellTimeMs = 30000;

    /**
     * @brief Check if point is inside polygon (ray casting)
     */
    [[nodiscard]] bool ContainsPoint(const LocationCoordinate& point) const;

    /**
     * @brief Get bounding box of polygon
     */
    [[nodiscard]] std::pair<LocationCoordinate, LocationCoordinate> GetBounds() const;

    /**
     * @brief Get center of polygon
     */
    [[nodiscard]] LocationCoordinate GetCenter() const;

    /**
     * @brief Get area in square meters
     */
    [[nodiscard]] double GetArea() const;
};

/**
 * @brief Geofence trigger event with additional context
 */
struct GeofenceEventData {
    std::string regionId;
    GeofenceEvent event;
    LocationData location;
    int64_t timestamp;
    int64_t dwellTime = 0;      ///< Time spent in region (for dwell events)
    double distanceFromEdge;     ///< Distance to region boundary
};

/**
 * @brief Geofence configuration
 */
struct GeofenceConfig {
    int updateIntervalMs = 1000;            ///< How often to check geofences
    double hysteresisMeters = 5.0;          ///< Buffer to prevent rapid enter/exit
    bool enableDwellDetection = true;       ///< Track time spent in regions
    bool persistGeofences = true;           ///< Save geofences to disk
    std::string persistPath = "geofences.dat";
};

using GeofenceEventCallback = std::function<void(const GeofenceEventData& event)>;

/**
 * @brief Software geofencing system
 *
 * Features:
 * - Circular regions (via platform or software)
 * - Polygon regions (software-only)
 * - Enter/exit/dwell detection
 * - Persistent geofences
 * - Multiple callbacks per region
 */
class GeofenceManager {
public:
    /**
     * @brief Get singleton instance
     */
    static GeofenceManager& Instance();

    // Delete copy/move
    GeofenceManager(const GeofenceManager&) = delete;
    GeofenceManager& operator=(const GeofenceManager&) = delete;

    /**
     * @brief Initialize the geofence manager
     */
    void Initialize(const GeofenceConfig& config = {});

    /**
     * @brief Shutdown the manager
     */
    void Shutdown();

    // === Circular Regions ===

    /**
     * @brief Add a circular geofence
     * @return true if added successfully
     */
    bool AddCircularRegion(const GeofenceRegion& region);

    /**
     * @brief Add circular region with callback
     */
    bool AddCircularRegion(const GeofenceRegion& region, GeofenceEventCallback callback);

    // === Polygon Regions ===

    /**
     * @brief Add a polygon geofence
     */
    bool AddPolygonRegion(const PolygonRegion& region);

    /**
     * @brief Add polygon region with callback
     */
    bool AddPolygonRegion(const PolygonRegion& region, GeofenceEventCallback callback);

    // === Region Management ===

    /**
     * @brief Remove a region by ID
     */
    void RemoveRegion(const std::string& identifier);

    /**
     * @brief Remove all regions
     */
    void RemoveAllRegions();

    /**
     * @brief Get all circular regions
     */
    [[nodiscard]] std::vector<GeofenceRegion> GetCircularRegions() const;

    /**
     * @brief Get all polygon regions
     */
    [[nodiscard]] std::vector<PolygonRegion> GetPolygonRegions() const;

    /**
     * @brief Check if region exists
     */
    [[nodiscard]] bool HasRegion(const std::string& identifier) const;

    /**
     * @brief Get region count
     */
    [[nodiscard]] size_t GetRegionCount() const;

    // === Callbacks ===

    /**
     * @brief Set global callback for all geofence events
     */
    void SetGlobalCallback(GeofenceEventCallback callback);

    /**
     * @brief Add callback for specific region
     */
    void AddRegionCallback(const std::string& identifier, GeofenceEventCallback callback);

    /**
     * @brief Remove callbacks for region
     */
    void RemoveRegionCallbacks(const std::string& identifier);

    // === State Query ===

    /**
     * @brief Check if currently inside a region
     */
    [[nodiscard]] bool IsInsideRegion(const std::string& identifier) const;

    /**
     * @brief Get regions containing a point
     */
    [[nodiscard]] std::vector<std::string> GetRegionsContaining(
        const LocationCoordinate& point) const;

    /**
     * @brief Get distance to nearest region
     */
    [[nodiscard]] double GetDistanceToNearestRegion(
        const LocationCoordinate& point,
        std::string* outRegionId = nullptr) const;

    /**
     * @brief Get current dwell time for region
     * @return Dwell time in ms, or -1 if not inside region
     */
    [[nodiscard]] int64_t GetDwellTime(const std::string& identifier) const;

    // === Manual Update ===

    /**
     * @brief Manually update with location
     */
    void Update(const LocationData& location);

    /**
     * @brief Start automatic updates from LocationManager
     */
    void StartAutoUpdates();

    /**
     * @brief Stop automatic updates
     */
    void StopAutoUpdates();

    // === Persistence ===

    /**
     * @brief Save geofences to file
     */
    bool SaveToFile(const std::string& filepath = "") const;

    /**
     * @brief Load geofences from file
     */
    bool LoadFromFile(const std::string& filepath = "");

private:
    GeofenceManager() = default;
    ~GeofenceManager() = default;

    void CheckGeofences(const LocationData& location);
    void TriggerEvent(const std::string& regionId, GeofenceEvent event,
                      const LocationData& location, int64_t dwellTime = 0);
    double DistanceToCircularRegion(const LocationCoordinate& point,
                                     const GeofenceRegion& region) const;
    double DistanceToPolygonRegion(const LocationCoordinate& point,
                                    const PolygonRegion& region) const;

    // Configuration
    GeofenceConfig m_config;
    bool m_initialized = false;
    bool m_autoUpdating = false;

    // Regions
    std::vector<GeofenceRegion> m_circularRegions;
    std::vector<PolygonRegion> m_polygonRegions;
    mutable std::mutex m_regionMutex;

    // State tracking
    struct RegionState {
        bool inside = false;
        int64_t enterTime = 0;
        bool dwellTriggered = false;
    };
    std::map<std::string, RegionState> m_regionStates;

    // Callbacks
    GeofenceEventCallback m_globalCallback;
    std::map<std::string, std::vector<GeofenceEventCallback>> m_regionCallbacks;
    mutable std::mutex m_callbackMutex;

    // Last known location
    LocationData m_lastLocation;
};

} // namespace Location
} // namespace Nova
