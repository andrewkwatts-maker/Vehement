/**
 * @file IOSLocation.hpp
 * @brief iOS Core Location service implementation
 */

#pragma once

#include "../LocationService.hpp"
#include <mutex>
#include <atomic>

// Forward declaration for Objective-C types
#ifdef __OBJC__
@class CLLocationManager;
@class NovaLocationDelegate;
#else
typedef void CLLocationManager;
typedef void NovaLocationDelegate;
#endif

namespace Nova {
namespace Platform {

/**
 * @brief iOS location service using Core Location framework
 *
 * Features:
 * - CLLocationManager for GPS/WiFi/Cellular location
 * - Permission handling (WhenInUse/Always)
 * - Significant location changes (battery efficient)
 * - Region monitoring (geofencing)
 * - iBeacon ranging
 * - Visit monitoring
 */
class IOSLocationService : public ILocationService {
public:
    IOSLocationService();
    ~IOSLocationService() override;

    // === Permission Management ===
    bool RequestPermission(bool alwaysAccess = false) override;
    [[nodiscard]] bool HasPermission() const override;
    [[nodiscard]] LocationAuthorizationStatus GetAuthorizationStatus() const override;
    void SetAuthorizationCallback(AuthorizationCallback callback) override;

    // === Location Updates ===
    void StartUpdates(LocationCallback callback) override;
    void StopUpdates() override;
    [[nodiscard]] bool IsUpdating() const override;
    void RequestSingleUpdate(LocationCallback callback,
                             LocationErrorCallback errorCallback = nullptr) override;
    [[nodiscard]] LocationData GetLastKnown() const override;

    // === Accuracy Settings ===
    [[nodiscard]] bool IsHighAccuracyAvailable() const override;
    void SetDesiredAccuracy(LocationAccuracy accuracy) override;
    [[nodiscard]] LocationAccuracy GetDesiredAccuracy() const override;
    void SetDistanceFilter(double meters) override;
    void SetUpdateInterval(int64_t milliseconds) override;

    // === Background Location ===
    [[nodiscard]] bool IsBackgroundLocationAvailable() const override;
    void SetBackgroundUpdatesEnabled(bool enable) override;
    void StartSignificantLocationChanges(LocationCallback callback) override;
    void StopSignificantLocationChanges() override;

    // === Geofencing ===
    [[nodiscard]] bool IsGeofencingSupported() const override;
    bool StartMonitoringRegion(const GeofenceRegion& region,
                                GeofenceCallback callback) override;
    void StopMonitoringRegion(const std::string& identifier) override;
    void StopMonitoringAllRegions() override;
    [[nodiscard]] std::vector<GeofenceRegion> GetMonitoredRegions() const override;

    // === Activity Recognition ===
    [[nodiscard]] bool IsActivityRecognitionAvailable() const override;
    void StartActivityUpdates(ActivityCallback callback) override;
    void StopActivityUpdates() override;

    // === Platform Info ===
    [[nodiscard]] std::string GetServiceName() const override;
    [[nodiscard]] bool AreLocationServicesEnabled() const override;
    void OpenLocationSettings() override;

    // === Mock Location Detection ===
    [[nodiscard]] bool AreMockLocationsAllowed() const override;
    void SetRejectMockLocations(bool reject) override;

    // === Error Handling ===
    void SetErrorCallback(LocationErrorCallback callback) override;
    [[nodiscard]] std::string GetLastError() const override;

    // === iOS-specific features ===

    /**
     * @brief Start monitoring for visits (battery efficient)
     */
    void StartVisitMonitoring(std::function<void(const LocationData&, bool isDeparture)> callback);

    /**
     * @brief Stop visit monitoring
     */
    void StopVisitMonitoring();

    /**
     * @brief Start heading updates (compass)
     */
    void StartHeadingUpdates(std::function<void(double heading, double accuracy)> callback);

    /**
     * @brief Stop heading updates
     */
    void StopHeadingUpdates();

    /**
     * @brief Start ranging for iBeacons
     * @param uuid Beacon UUID
     * @param callback Called when beacons are ranged
     */
    void StartBeaconRanging(const std::string& uuid,
        std::function<void(const std::string& uuid, int major, int minor, double distance)> callback);

    /**
     * @brief Stop ranging iBeacons
     * @param uuid Beacon UUID to stop ranging
     */
    void StopBeaconRanging(const std::string& uuid);

    // === Delegate callbacks (called from Objective-C) ===
    void OnLocationUpdate(double latitude, double longitude, double altitude,
                          double hAccuracy, double vAccuracy, double speed,
                          double course, int64_t timestamp, bool isSimulated);
    void OnAuthorizationChange(int status);
    void OnLocationError(int errorCode, const std::string& message);
    void OnRegionEnter(const std::string& identifier);
    void OnRegionExit(const std::string& identifier);
    void OnVisit(double latitude, double longitude, int64_t arrivalTime,
                 int64_t departureTime);
    void OnHeadingUpdate(double magneticHeading, double trueHeading, double accuracy);
    void OnBeaconRanged(const std::string& uuid, int major, int minor,
                        double accuracy, int proximity);

private:
    void UpdateAccuracySettings();
    ActivityType EstimateActivityFromSpeed(double speed) const;

    // Core Location manager and delegate
    CLLocationManager* m_locationManager = nullptr;
    NovaLocationDelegate* m_delegate = nullptr;

    // State
    std::atomic<bool> m_updating{false};
    std::atomic<bool> m_significantChanges{false};
    std::atomic<bool> m_backgroundEnabled{false};
    std::atomic<bool> m_rejectMockLocations{false};
    std::atomic<bool> m_visitMonitoring{false};
    std::atomic<bool> m_headingUpdates{false};
    LocationAccuracy m_desiredAccuracy = LocationAccuracy::Best;
    double m_distanceFilter = 0.0;
    LocationData m_lastLocation;
    std::string m_lastError;
    mutable std::mutex m_mutex;

    // Callbacks
    LocationCallback m_locationCallback;
    LocationErrorCallback m_errorCallback;
    AuthorizationCallback m_authCallback;
    LocationCallback m_significantCallback;
    std::map<std::string, GeofenceCallback> m_geofenceCallbacks;
    ActivityCallback m_activityCallback;
    std::function<void(const LocationData&, bool)> m_visitCallback;
    std::function<void(double, double)> m_headingCallback;
    std::map<std::string, std::function<void(const std::string&, int, int, double)>> m_beaconCallbacks;

    // Monitored regions
    std::vector<GeofenceRegion> m_monitoredRegions;
};

} // namespace Platform
} // namespace Nova
