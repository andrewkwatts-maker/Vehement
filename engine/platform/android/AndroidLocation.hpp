/**
 * @file AndroidLocation.hpp
 * @brief Android GPS/Location service implementation using FusedLocationProviderClient
 */

#pragma once

#include "../LocationService.hpp"
#include <jni.h>
#include <mutex>
#include <atomic>
#include <map>

namespace Nova {
namespace Platform {

/**
 * @brief Android location service using Google Play Services FusedLocationProviderClient
 *
 * Features:
 * - FusedLocationProviderClient for best location accuracy with minimal battery
 * - Permission handling (ACCESS_FINE_LOCATION, ACCESS_COARSE_LOCATION)
 * - Background location (ACCESS_BACKGROUND_LOCATION for Android 10+)
 * - Geofencing via GeofencingClient
 * - Activity recognition via ActivityRecognitionClient
 */
class AndroidLocationService : public ILocationService {
public:
    AndroidLocationService();
    ~AndroidLocationService() override;

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

    // === JNI Callbacks (called from Java) ===
    void OnLocationUpdate(JNIEnv* env, jobject location);
    void OnPermissionResult(bool granted, bool fineLocation);
    void OnGeofenceEvent(const std::string& regionId, int transitionType);
    void OnActivityUpdate(int activityType, int confidence);
    void OnLocationError(int errorCode, const std::string& message);

    // === JNI Setup ===
    static void SetJavaVM(JavaVM* vm);
    static void SetActivity(jobject activity);
    static JavaVM* GetJavaVM();
    static jobject GetActivity();

private:
    // JNI helpers
    JNIEnv* GetJNIEnv() const;
    void InitializeJNI();
    void CleanupJNI();
    jclass FindClass(JNIEnv* env, const char* name);

    // Convert between native and Java types
    LocationData ConvertLocation(JNIEnv* env, jobject location);
    int GetPriorityFromAccuracy(LocationAccuracy accuracy) const;
    ActivityType ConvertActivityType(int androidType) const;

    // State
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_updating{false};
    std::atomic<bool> m_significantChanges{false};
    std::atomic<bool> m_rejectMockLocations{false};
    std::atomic<bool> m_backgroundEnabled{false};
    LocationAccuracy m_desiredAccuracy = LocationAccuracy::Best;
    double m_distanceFilter = 0.0;
    int64_t m_updateInterval = 1000; // 1 second default
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

    // Monitored regions
    std::vector<GeofenceRegion> m_monitoredRegions;

    // JNI references
    static JavaVM* s_javaVM;
    static jobject s_activity;
    jclass m_locationServiceClass = nullptr;
    jobject m_locationServiceInstance = nullptr;
    jmethodID m_requestPermissionMethod = nullptr;
    jmethodID m_hasPermissionMethod = nullptr;
    jmethodID m_startUpdatesMethod = nullptr;
    jmethodID m_stopUpdatesMethod = nullptr;
    jmethodID m_requestSingleUpdateMethod = nullptr;
    jmethodID m_getLastKnownMethod = nullptr;
    jmethodID m_setAccuracyMethod = nullptr;
    jmethodID m_setDistanceFilterMethod = nullptr;
    jmethodID m_setIntervalMethod = nullptr;
    jmethodID m_addGeofenceMethod = nullptr;
    jmethodID m_removeGeofenceMethod = nullptr;
    jmethodID m_startActivityRecognitionMethod = nullptr;
    jmethodID m_stopActivityRecognitionMethod = nullptr;
    jmethodID m_isLocationEnabledMethod = nullptr;
    jmethodID m_openSettingsMethod = nullptr;
    jmethodID m_isMockLocationMethod = nullptr;
};

} // namespace Platform
} // namespace Nova
