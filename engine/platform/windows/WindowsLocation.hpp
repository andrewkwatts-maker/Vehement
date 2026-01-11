/**
 * @file WindowsLocation.hpp
 * @brief Windows location service using Windows.Devices.Geolocation
 */

#pragma once

#include "../LocationService.hpp"
#include <mutex>
#include <atomic>
#include <thread>

namespace Nova {
namespace Platform {

/**
 * @brief Windows location service using Windows.Devices.Geolocation API
 *
 * Features:
 * - Windows Location API (Windows 8+)
 * - Privacy settings integration
 * - IP-based fallback
 */
class WindowsLocationService : public ILocationService {
public:
    WindowsLocationService();
    ~WindowsLocationService() override;

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

private:
    void InitializeWinRT();
    void ShutdownWinRT();
    void UpdateThread();
    void CheckGeofences(const LocationData& location);
    void IPGeolocationFallback();

    // WinRT handles (void* to avoid including Windows headers)
    void* m_geolocator = nullptr;
    void* m_positionToken = nullptr;

    // State
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_updating{false};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_rejectMockLocations{false};
    std::atomic<bool> m_useIPFallback{false};
    LocationAccuracy m_desiredAccuracy = LocationAccuracy::Best;
    double m_distanceFilter = 0.0;
    int64_t m_updateInterval = 1000;
    LocationData m_lastLocation;
    std::string m_lastError;
    mutable std::mutex m_mutex;

    // Update thread
    std::thread m_updateThread;
    std::condition_variable m_updateCondition;

    // Callbacks
    LocationCallback m_locationCallback;
    LocationErrorCallback m_errorCallback;
    AuthorizationCallback m_authCallback;
    std::map<std::string, GeofenceCallback> m_geofenceCallbacks;

    // Monitored regions
    std::vector<GeofenceRegion> m_monitoredRegions;
    std::map<std::string, bool> m_regionState;
};

} // namespace Platform
} // namespace Nova
