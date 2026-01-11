/**
 * @file MacOSLocation.hpp
 * @brief macOS Core Location service implementation
 */

#pragma once

#include "../LocationService.hpp"
#include <mutex>
#include <atomic>

// Forward declaration for Objective-C types
#ifdef __OBJC__
@class CLLocationManager;
@class NovaMacLocationDelegate;
#else
typedef void CLLocationManager;
typedef void NovaMacLocationDelegate;
#endif

namespace Nova {
namespace Platform {

/**
 * @brief macOS location service using Core Location framework
 *
 * Features:
 * - CLLocationManager for WiFi/Cellular location
 * - Permission handling with authorization
 * - Significant location changes
 * - Region monitoring (geofencing)
 */
class MacOSLocationService : public ILocationService {
public:
    MacOSLocationService();
    ~MacOSLocationService() override;

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

    // === Delegate callbacks ===
    void OnLocationUpdate(double latitude, double longitude, double altitude,
                          double hAccuracy, double vAccuracy, double speed,
                          double course, int64_t timestamp);
    void OnAuthorizationChange(int status);
    void OnLocationError(int errorCode, const std::string& message);
    void OnRegionEnter(const std::string& identifier);
    void OnRegionExit(const std::string& identifier);

private:
    void UpdateAccuracySettings();

    // Core Location manager and delegate
    CLLocationManager* m_locationManager = nullptr;
    NovaMacLocationDelegate* m_delegate = nullptr;

    // State
    std::atomic<bool> m_updating{false};
    std::atomic<bool> m_significantChanges{false};
    std::atomic<bool> m_rejectMockLocations{false};
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

    // Monitored regions
    std::vector<GeofenceRegion> m_monitoredRegions;
};

} // namespace Platform
} // namespace Nova
