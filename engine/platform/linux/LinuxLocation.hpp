/**
 * @file LinuxLocation.hpp
 * @brief Linux desktop location service using GeoClue2 and GPSD
 */

#pragma once

#include "../LocationService.hpp"
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace Nova {
namespace Platform {

/**
 * @brief Linux location service using GeoClue2 D-Bus or GPSD
 *
 * Features:
 * - GeoClue2 D-Bus integration (primary)
 * - GPSD daemon support (for GPS hardware)
 * - IP-based fallback via web API
 * - Manual location override
 */
class LinuxLocationService : public ILocationService {
public:
    /**
     * @brief Location provider priority
     */
    enum class ProviderType {
        GeoClue2,   ///< GeoClue2 D-Bus service
        GPSD,       ///< GPSD daemon (local GPS hardware)
        IPBased,    ///< IP geolocation API
        Manual      ///< Manually set location
    };

    LinuxLocationService();
    ~LinuxLocationService() override;

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

    // === Linux-specific features ===

    /**
     * @brief Set the preferred provider type
     */
    void SetPreferredProvider(ProviderType type);

    /**
     * @brief Get currently active provider
     */
    [[nodiscard]] ProviderType GetActiveProvider() const;

    /**
     * @brief Set manual location (for testing or when no other provider works)
     */
    void SetManualLocation(const LocationData& location);

    /**
     * @brief Configure GPSD connection
     * @param host GPSD host (default: localhost)
     * @param port GPSD port (default: 2947)
     */
    void ConfigureGPSD(const std::string& host = "localhost", int port = 2947);

    /**
     * @brief Configure IP geolocation API
     * @param apiUrl API endpoint URL
     * @param apiKey Optional API key
     */
    void ConfigureIPGeolocation(const std::string& apiUrl, const std::string& apiKey = "");

    /**
     * @brief Check if GeoClue2 is available
     */
    [[nodiscard]] bool IsGeoClueAvailable() const;

    /**
     * @brief Check if GPSD is available
     */
    [[nodiscard]] bool IsGPSDAvailable() const;

private:
    // Provider initialization
    bool InitializeGeoClue();
    void ShutdownGeoClue();
    bool InitializeGPSD();
    void ShutdownGPSD();

    // Update thread functions
    void GeoClueUpdateThread();
    void GPSDUpdateThread();
    void IPGeolocationUpdate();

    // Geofence checking
    void CheckGeofences(const LocationData& location);

    // D-Bus helpers
    void* m_dbusConnection = nullptr;
    void* m_geoClueClient = nullptr;

    // GPSD state
    void* m_gpsdData = nullptr;
    std::string m_gpsdHost = "localhost";
    int m_gpsdPort = 2947;
    std::atomic<bool> m_gpsdConnected{false};

    // IP geolocation config
    std::string m_ipApiUrl = "http://ip-api.com/json";
    std::string m_ipApiKey;

    // State
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_updating{false};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_rejectMockLocations{false};
    ProviderType m_preferredProvider = ProviderType::GeoClue2;
    ProviderType m_activeProvider = ProviderType::GeoClue2;
    LocationAccuracy m_desiredAccuracy = LocationAccuracy::Best;
    double m_distanceFilter = 0.0;
    int64_t m_updateInterval = 1000;
    LocationData m_lastLocation;
    LocationData m_manualLocation;
    bool m_useManualLocation = false;
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
    std::map<std::string, bool> m_regionState; // true = inside region
};

} // namespace Platform
} // namespace Nova
