/**
 * @file LocationService.hpp
 * @brief Cross-platform GPS/Location service interface
 *
 * Provides a unified interface for accessing location services across
 * all supported platforms (Android, iOS, Linux, Windows, macOS).
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <cmath>

namespace Nova {
namespace Platform {

/**
 * @brief Location accuracy levels
 */
enum class LocationAccuracy {
    BestForNavigation,  ///< Highest accuracy, uses all sensors, highest battery usage
    Best,               ///< Best accuracy balanced with performance
    NearestTenMeters,   ///< Accurate to ~10 meters
    HundredMeters,      ///< Accurate to ~100 meters
    Kilometer,          ///< Accurate to ~1km
    ThreeKilometers,    ///< Accurate to ~3km (lowest battery usage)
    Passive             ///< No active GPS, only receives updates from other apps
};

/**
 * @brief Location authorization status
 */
enum class LocationAuthorizationStatus {
    NotDetermined,      ///< User has not been asked yet
    Restricted,         ///< Location services restricted (parental controls, etc.)
    Denied,             ///< User explicitly denied permission
    AuthorizedAlways,   ///< Authorized for background and foreground use
    AuthorizedWhenInUse ///< Authorized only while app is in foreground
};

/**
 * @brief Location error codes
 */
enum class LocationError {
    None,
    PermissionDenied,
    LocationDisabled,
    NetworkUnavailable,
    Timeout,
    Unknown,
    NotSupported,
    MockLocationDetected,
    AccuracyNotMet
};

/**
 * @brief GPS coordinate data
 */
struct LocationCoordinate {
    double latitude = 0.0;   ///< Latitude in degrees (-90 to 90)
    double longitude = 0.0;  ///< Longitude in degrees (-180 to 180)

    /**
     * @brief Check if coordinates are valid
     */
    [[nodiscard]] bool IsValid() const noexcept {
        return latitude >= -90.0 && latitude <= 90.0 &&
               longitude >= -180.0 && longitude <= 180.0 &&
               (latitude != 0.0 || longitude != 0.0);
    }

    /**
     * @brief Calculate distance to another point using Haversine formula
     * @param other Target coordinates
     * @return Distance in meters
     */
    [[nodiscard]] double DistanceTo(const LocationCoordinate& other) const {
        constexpr double EARTH_RADIUS_M = 6371000.0;

        double lat1Rad = latitude * M_PI / 180.0;
        double lat2Rad = other.latitude * M_PI / 180.0;
        double deltaLat = (other.latitude - latitude) * M_PI / 180.0;
        double deltaLon = (other.longitude - longitude) * M_PI / 180.0;

        double a = std::sin(deltaLat / 2) * std::sin(deltaLat / 2) +
                   std::cos(lat1Rad) * std::cos(lat2Rad) *
                   std::sin(deltaLon / 2) * std::sin(deltaLon / 2);

        double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

        return EARTH_RADIUS_M * c;
    }

    /**
     * @brief Calculate bearing to another point
     * @param other Target coordinates
     * @return Bearing in degrees (0-360, 0=North)
     */
    [[nodiscard]] double BearingTo(const LocationCoordinate& other) const {
        double lat1Rad = latitude * M_PI / 180.0;
        double lat2Rad = other.latitude * M_PI / 180.0;
        double deltaLon = (other.longitude - longitude) * M_PI / 180.0;

        double y = std::sin(deltaLon) * std::cos(lat2Rad);
        double x = std::cos(lat1Rad) * std::sin(lat2Rad) -
                   std::sin(lat1Rad) * std::cos(lat2Rad) * std::cos(deltaLon);

        double bearing = std::atan2(y, x) * 180.0 / M_PI;
        return std::fmod(bearing + 360.0, 360.0);
    }

    bool operator==(const LocationCoordinate& other) const {
        constexpr double EPSILON = 0.0000001;
        return std::abs(latitude - other.latitude) < EPSILON &&
               std::abs(longitude - other.longitude) < EPSILON;
    }

    bool operator!=(const LocationCoordinate& other) const {
        return !(*this == other);
    }
};

/**
 * @brief Complete location data including metadata
 */
struct LocationData {
    LocationCoordinate coordinate;          ///< GPS coordinates
    double altitude = 0.0;                  ///< Altitude in meters
    double horizontalAccuracy = -1.0;       ///< Horizontal accuracy in meters (-1 = unknown)
    double verticalAccuracy = -1.0;         ///< Vertical accuracy in meters (-1 = unknown)
    double speed = -1.0;                    ///< Speed in m/s (-1 = unknown)
    double course = -1.0;                   ///< Direction of travel in degrees (-1 = unknown)
    int64_t timestamp = 0;                  ///< Unix timestamp in milliseconds
    bool isMockLocation = false;            ///< True if this is a simulated/mock location
    std::string provider;                   ///< Location provider name (GPS, Network, etc.)

    /**
     * @brief Check if location data is valid
     */
    [[nodiscard]] bool IsValid() const noexcept {
        return coordinate.IsValid() && horizontalAccuracy >= 0;
    }

    /**
     * @brief Get age of this location data
     */
    [[nodiscard]] int64_t GetAgeMs() const {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return now - timestamp;
    }
};

/**
 * @brief Geofence region definition
 */
struct GeofenceRegion {
    std::string identifier;             ///< Unique region identifier
    LocationCoordinate center;          ///< Center of the region
    double radiusMeters = 100.0;        ///< Radius in meters
    bool notifyOnEntry = true;          ///< Trigger on entering region
    bool notifyOnExit = true;           ///< Trigger on exiting region
    bool notifyOnDwell = false;         ///< Trigger after dwelling for dwellTime
    int dwellTimeMs = 30000;            ///< Time to dwell before trigger (ms)

    /**
     * @brief Check if a point is inside this region
     */
    [[nodiscard]] bool ContainsPoint(const LocationCoordinate& point) const {
        return center.DistanceTo(point) <= radiusMeters;
    }
};

/**
 * @brief Geofence event types
 */
enum class GeofenceEvent {
    Enter,      ///< Device entered the region
    Exit,       ///< Device exited the region
    Dwell       ///< Device has been in region for dwell time
};

/**
 * @brief Activity recognition types
 */
enum class ActivityType {
    Unknown,
    Stationary,
    Walking,
    Running,
    Cycling,
    Automotive,
    Flying
};

/**
 * @brief Detected activity information
 */
struct ActivityData {
    ActivityType type = ActivityType::Unknown;
    float confidence = 0.0f;    ///< Confidence level 0-1
    int64_t timestamp = 0;      ///< When activity was detected
};

// Callback type definitions
using LocationCallback = std::function<void(const LocationData& location)>;
using LocationErrorCallback = std::function<void(LocationError error, const std::string& message)>;
using AuthorizationCallback = std::function<void(LocationAuthorizationStatus status)>;
using GeofenceCallback = std::function<void(const GeofenceRegion& region, GeofenceEvent event)>;
using ActivityCallback = std::function<void(const ActivityData& activity)>;

/**
 * @brief Cross-platform location service interface
 *
 * Implementations:
 * - AndroidLocationService (Android)
 * - IOSLocationService (iOS)
 * - LinuxLocationService (Linux/Desktop)
 * - WindowsLocationService (Windows)
 * - MacOSLocationService (macOS)
 */
class ILocationService {
public:
    virtual ~ILocationService() = default;

    // === Permission Management ===

    /**
     * @brief Request location permission from the user
     * @param alwaysAccess Request "always" (background) access if true
     * @return true if permission request was initiated
     */
    virtual bool RequestPermission(bool alwaysAccess = false) = 0;

    /**
     * @brief Check if we have location permission
     */
    [[nodiscard]] virtual bool HasPermission() const = 0;

    /**
     * @brief Get current authorization status
     */
    [[nodiscard]] virtual LocationAuthorizationStatus GetAuthorizationStatus() const = 0;

    /**
     * @brief Set callback for authorization changes
     */
    virtual void SetAuthorizationCallback(AuthorizationCallback callback) = 0;

    // === Location Updates ===

    /**
     * @brief Start continuous location updates
     * @param callback Called whenever location updates
     */
    virtual void StartUpdates(LocationCallback callback) = 0;

    /**
     * @brief Stop continuous location updates
     */
    virtual void StopUpdates() = 0;

    /**
     * @brief Check if location updates are currently active
     */
    [[nodiscard]] virtual bool IsUpdating() const = 0;

    /**
     * @brief Request a single location update
     * @param callback Called with location result
     * @param errorCallback Called on error
     */
    virtual void RequestSingleUpdate(LocationCallback callback,
                                      LocationErrorCallback errorCallback = nullptr) = 0;

    /**
     * @brief Get the last known location
     * @return Last known location or empty if none available
     */
    [[nodiscard]] virtual LocationData GetLastKnown() const = 0;

    // === Accuracy Settings ===

    /**
     * @brief Check if high accuracy mode is available
     */
    [[nodiscard]] virtual bool IsHighAccuracyAvailable() const = 0;

    /**
     * @brief Set desired location accuracy
     * @param accuracy Desired accuracy level
     */
    virtual void SetDesiredAccuracy(LocationAccuracy accuracy) = 0;

    /**
     * @brief Get current accuracy setting
     */
    [[nodiscard]] virtual LocationAccuracy GetDesiredAccuracy() const = 0;

    /**
     * @brief Set minimum distance filter for updates
     * @param meters Minimum distance between updates (0 = all updates)
     */
    virtual void SetDistanceFilter(double meters) = 0;

    /**
     * @brief Set minimum time interval between updates
     * @param milliseconds Minimum interval (0 = fastest possible)
     */
    virtual void SetUpdateInterval(int64_t milliseconds) = 0;

    // === Background Location ===

    /**
     * @brief Check if background location is available
     */
    [[nodiscard]] virtual bool IsBackgroundLocationAvailable() const = 0;

    /**
     * @brief Enable/disable background location updates
     * @param enable Enable background updates
     */
    virtual void SetBackgroundUpdatesEnabled(bool enable) = 0;

    /**
     * @brief Start monitoring significant location changes (battery efficient)
     * @param callback Called on significant location change
     */
    virtual void StartSignificantLocationChanges(LocationCallback callback) = 0;

    /**
     * @brief Stop monitoring significant location changes
     */
    virtual void StopSignificantLocationChanges() = 0;

    // === Geofencing ===

    /**
     * @brief Check if geofencing is supported
     */
    [[nodiscard]] virtual bool IsGeofencingSupported() const = 0;

    /**
     * @brief Start monitoring a geofence region
     * @param region Region to monitor
     * @param callback Called on geofence events
     * @return true if monitoring started successfully
     */
    virtual bool StartMonitoringRegion(const GeofenceRegion& region,
                                        GeofenceCallback callback) = 0;

    /**
     * @brief Stop monitoring a geofence region
     * @param identifier Region identifier to stop monitoring
     */
    virtual void StopMonitoringRegion(const std::string& identifier) = 0;

    /**
     * @brief Stop monitoring all geofence regions
     */
    virtual void StopMonitoringAllRegions() = 0;

    /**
     * @brief Get list of currently monitored regions
     */
    [[nodiscard]] virtual std::vector<GeofenceRegion> GetMonitoredRegions() const = 0;

    // === Activity Recognition ===

    /**
     * @brief Check if activity recognition is available
     */
    [[nodiscard]] virtual bool IsActivityRecognitionAvailable() const = 0;

    /**
     * @brief Start activity recognition updates
     * @param callback Called when activity changes
     */
    virtual void StartActivityUpdates(ActivityCallback callback) = 0;

    /**
     * @brief Stop activity recognition updates
     */
    virtual void StopActivityUpdates() = 0;

    // === Platform Info ===

    /**
     * @brief Get platform-specific location service name
     */
    [[nodiscard]] virtual std::string GetServiceName() const = 0;

    /**
     * @brief Check if location services are enabled system-wide
     */
    [[nodiscard]] virtual bool AreLocationServicesEnabled() const = 0;

    /**
     * @brief Open system location settings (platform-specific)
     */
    virtual void OpenLocationSettings() = 0;

    // === Mock Location Detection ===

    /**
     * @brief Check if mock locations are allowed
     */
    [[nodiscard]] virtual bool AreMockLocationsAllowed() const = 0;

    /**
     * @brief Set whether to reject mock locations
     * @param reject If true, mock locations will be rejected
     */
    virtual void SetRejectMockLocations(bool reject) = 0;

    // === Error Handling ===

    /**
     * @brief Set error callback for location errors
     */
    virtual void SetErrorCallback(LocationErrorCallback callback) = 0;

    /**
     * @brief Get last error message
     */
    [[nodiscard]] virtual std::string GetLastError() const = 0;
};

/**
 * @brief Factory function to create platform-specific location service
 */
std::unique_ptr<ILocationService> CreateLocationService();

/**
 * @brief Helper class for location service management
 */
class LocationServiceManager {
public:
    /**
     * @brief Get the singleton instance
     */
    static LocationServiceManager& Instance();

    /**
     * @brief Initialize the location service
     */
    void Initialize();

    /**
     * @brief Shutdown the location service
     */
    void Shutdown();

    /**
     * @brief Get the platform location service
     */
    ILocationService* GetService() { return m_service.get(); }
    const ILocationService* GetService() const { return m_service.get(); }

    /**
     * @brief Check if service is initialized
     */
    bool IsInitialized() const { return m_service != nullptr; }

private:
    LocationServiceManager() = default;
    std::unique_ptr<ILocationService> m_service;
};

} // namespace Platform
} // namespace Nova
