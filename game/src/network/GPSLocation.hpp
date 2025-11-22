#pragma once

#include <string>
#include <functional>
#include <memory>
#include <optional>
#include <cmath>

namespace Vehement {

/**
 * @brief GPS coordinate pair
 */
struct GPSCoordinates {
    double latitude = 0.0;
    double longitude = 0.0;

    /**
     * @brief Check if coordinates are valid
     */
    [[nodiscard]] bool IsValid() const noexcept {
        return latitude >= -90.0 && latitude <= 90.0 &&
               longitude >= -180.0 && longitude <= 180.0 &&
               (latitude != 0.0 || longitude != 0.0);
    }

    /**
     * @brief Calculate distance to another point in kilometers (Haversine formula)
     * @param other The other GPS coordinates
     * @return Distance in kilometers
     */
    [[nodiscard]] double DistanceTo(const GPSCoordinates& other) const {
        constexpr double EARTH_RADIUS_KM = 6371.0;

        double lat1Rad = latitude * M_PI / 180.0;
        double lat2Rad = other.latitude * M_PI / 180.0;
        double deltaLat = (other.latitude - latitude) * M_PI / 180.0;
        double deltaLon = (other.longitude - longitude) * M_PI / 180.0;

        double a = std::sin(deltaLat / 2) * std::sin(deltaLat / 2) +
                   std::cos(lat1Rad) * std::cos(lat2Rad) *
                   std::sin(deltaLon / 2) * std::sin(deltaLon / 2);

        double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

        return EARTH_RADIUS_KM * c;
    }

    /**
     * @brief Equality comparison
     */
    bool operator==(const GPSCoordinates& other) const {
        constexpr double EPSILON = 0.000001;
        return std::abs(latitude - other.latitude) < EPSILON &&
               std::abs(longitude - other.longitude) < EPSILON;
    }

    bool operator!=(const GPSCoordinates& other) const {
        return !(*this == other);
    }
};

/**
 * @brief Information about a town/city location
 */
struct TownInfo {
    std::string townId;          ///< Unique ID (e.g., "melbourne-au-3000")
    std::string townName;        ///< Display name (e.g., "Melbourne")
    std::string region;          ///< State/province/region
    std::string country;         ///< Country name
    std::string countryCode;     ///< ISO country code (e.g., "AU")
    std::string postalCode;      ///< Postal/ZIP code
    GPSCoordinates center;       ///< Town center coordinates
    float radiusKm = 5.0f;       ///< Town boundary radius in kilometers

    /**
     * @brief Check if town info is valid
     */
    [[nodiscard]] bool IsValid() const noexcept {
        return !townId.empty() && !townName.empty() && center.IsValid();
    }

    /**
     * @brief Check if a point is within town boundaries
     * @param coords Coordinates to check
     * @return true if within radius
     */
    [[nodiscard]] bool ContainsPoint(const GPSCoordinates& coords) const {
        return center.DistanceTo(coords) <= radiusKm;
    }

    /**
     * @brief Generate a unique town ID from location data
     * @return Formatted town ID string
     */
    [[nodiscard]] static std::string GenerateTownId(const std::string& name,
                                                     const std::string& countryCode,
                                                     const std::string& postalCode);
};

/**
 * @brief Location provider interface
 *
 * Allows plugging in different location providers:
 * - Native GPS (mobile)
 * - IP geolocation (PC)
 * - Manual input (testing)
 */
class ILocationProvider {
public:
    virtual ~ILocationProvider() = default;

    /**
     * @brief Request current location asynchronously
     * @param callback Called with coordinates when available
     */
    virtual void RequestLocation(std::function<void(std::optional<GPSCoordinates>)> callback) = 0;

    /**
     * @brief Check if this provider is available on current platform
     */
    [[nodiscard]] virtual bool IsAvailable() const = 0;

    /**
     * @brief Get provider name for debugging
     */
    [[nodiscard]] virtual std::string GetName() const = 0;
};

/**
 * @brief GPS and location handling for Vehement2
 *
 * Handles location acquisition across different platforms:
 * - Mobile: Native GPS hardware
 * - PC/Desktop: IP geolocation API
 * - Fallback: Manual location input
 *
 * Also provides geocoding services to convert coordinates to town names.
 */
class GPSLocation {
public:
    /**
     * @brief Location acquisition status
     */
    enum class Status {
        Idle,
        Requesting,
        Success,
        Failed,
        PermissionDenied,
        Timeout
    };

    /**
     * @brief Geocoding API configuration
     */
    struct GeocodingConfig {
        std::string apiUrl = "https://nominatim.openstreetmap.org/reverse";
        std::string apiKey;  // Optional for some services
        int timeoutMs = 5000;
    };

    // Callback types
    using LocationCallback = std::function<void(GPSCoordinates coords)>;
    using LocationErrorCallback = std::function<void(Status status, const std::string& error)>;
    using TownCallback = std::function<void(TownInfo town)>;

    /**
     * @brief Get the singleton instance
     */
    [[nodiscard]] static GPSLocation& Instance();

    // Delete copy/move
    GPSLocation(const GPSLocation&) = delete;
    GPSLocation& operator=(const GPSLocation&) = delete;

    /**
     * @brief Initialize with geocoding configuration
     * @param config Geocoding API configuration
     */
    void Initialize(const GeocodingConfig& config = {});

    /**
     * @brief Register a custom location provider
     * @param provider Location provider implementation
     */
    void RegisterProvider(std::unique_ptr<ILocationProvider> provider);

    /**
     * @brief Get current GPS coordinates asynchronously
     *
     * Tries providers in order:
     * 1. Native GPS (mobile)
     * 2. IP geolocation (PC)
     * 3. Cached location
     *
     * @param callback Called with coordinates when available
     * @param errorCallback Called if location acquisition fails
     */
    void GetCurrentLocation(LocationCallback callback,
                           LocationErrorCallback errorCallback = nullptr);

    /**
     * @brief Convert GPS coordinates to town information
     *
     * Uses reverse geocoding API to get town/city name from coordinates.
     *
     * @param coords GPS coordinates
     * @param callback Called with town information
     */
    void GetTownFromCoordinates(GPSCoordinates coords, TownCallback callback);

    /**
     * @brief Get location from IP address (PC fallback)
     *
     * Uses IP geolocation API to estimate location on desktop platforms.
     *
     * @param callback Called with estimated coordinates
     */
    void GetLocationFromIP(LocationCallback callback);

    /**
     * @brief Set a manual location (for testing or manual input)
     * @param coords Manual coordinates
     */
    void SetManualLocation(GPSCoordinates coords);

    /**
     * @brief Get the last known location
     * @return Last known coordinates, or invalid coordinates if none
     */
    [[nodiscard]] GPSCoordinates GetLastLocation() const { return m_lastLocation; }

    /**
     * @brief Get the last known town
     * @return Last known town info, or invalid town if none
     */
    [[nodiscard]] const TownInfo& GetLastTown() const { return m_lastTown; }

    /**
     * @brief Get current status
     */
    [[nodiscard]] Status GetStatus() const { return m_status; }

    /**
     * @brief Check if location services are available
     */
    [[nodiscard]] bool IsLocationAvailable() const;

    /**
     * @brief Clear cached location data
     */
    void ClearCache();

    /**
     * @brief Create a default town for a location (when geocoding fails)
     * @param coords Center coordinates
     * @return Default town info
     */
    [[nodiscard]] static TownInfo CreateDefaultTown(GPSCoordinates coords);

private:
    GPSLocation() = default;
    ~GPSLocation() = default;

    // Providers
    std::vector<std::unique_ptr<ILocationProvider>> m_providers;

    // Configuration
    GeocodingConfig m_geocodingConfig;

    // Cached data
    GPSCoordinates m_lastLocation;
    TownInfo m_lastTown;
    Status m_status = Status::Idle;

    // Manual override
    bool m_useManualLocation = false;
    GPSCoordinates m_manualLocation;

    // Helper methods
    void TryNextProvider(size_t index, LocationCallback callback,
                        LocationErrorCallback errorCallback);
    void ParseGeocodingResponse(const std::string& json, TownCallback callback);
    std::string SanitizeTownName(const std::string& name);
};

/**
 * @brief Stub location provider for offline/testing use
 */
class StubLocationProvider : public ILocationProvider {
public:
    explicit StubLocationProvider(GPSCoordinates defaultLocation = {-37.8136, 144.9631}); // Melbourne

    void RequestLocation(std::function<void(std::optional<GPSCoordinates>)> callback) override;
    [[nodiscard]] bool IsAvailable() const override { return true; }
    [[nodiscard]] std::string GetName() const override { return "StubProvider"; }

    void SetLocation(GPSCoordinates coords) { m_location = coords; }

private:
    GPSCoordinates m_location;
};

/**
 * @brief IP-based geolocation provider
 */
class IPGeolocationProvider : public ILocationProvider {
public:
    struct Config {
        std::string apiUrl = "http://ip-api.com/json";
        int timeoutMs = 5000;
    };

    explicit IPGeolocationProvider(const Config& config = {});

    void RequestLocation(std::function<void(std::optional<GPSCoordinates>)> callback) override;
    [[nodiscard]] bool IsAvailable() const override;
    [[nodiscard]] std::string GetName() const override { return "IPGeolocation"; }

private:
    Config m_config;
};

/**
 * @brief Platform-native location provider
 *
 * Wraps the Nova::Platform::ILocationService to work with
 * the Vehement ILocationProvider interface.
 */
class PlatformLocationProvider : public ILocationProvider {
public:
    PlatformLocationProvider();
    ~PlatformLocationProvider() override;

    void RequestLocation(std::function<void(std::optional<GPSCoordinates>)> callback) override;
    [[nodiscard]] bool IsAvailable() const override;
    [[nodiscard]] std::string GetName() const override;

    /**
     * @brief Request location permission
     * @param alwaysAccess Request background location access
     */
    bool RequestPermission(bool alwaysAccess = false);

    /**
     * @brief Check if permission is granted
     */
    [[nodiscard]] bool HasPermission() const;

    /**
     * @brief Start continuous location updates
     */
    void StartContinuousUpdates(std::function<void(GPSCoordinates)> callback);

    /**
     * @brief Stop continuous updates
     */
    void StopContinuousUpdates();

    /**
     * @brief Check if updates are active
     */
    [[nodiscard]] bool IsUpdating() const;

    /**
     * @brief Check if location appears to be mocked/spoofed
     */
    [[nodiscard]] bool IsMockLocationDetected() const;

    /**
     * @brief Set whether to reject mock locations
     */
    void SetRejectMockLocations(bool reject);

    /**
     * @brief Get platform service name
     */
    [[nodiscard]] std::string GetPlatformServiceName() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Vehement
