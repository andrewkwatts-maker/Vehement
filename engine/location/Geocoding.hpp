/**
 * @file Geocoding.hpp
 * @brief Geocoding services for address-coordinate conversion
 */

#pragma once

#include "../platform/LocationService.hpp"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <chrono>
#include <mutex>

namespace Nova {
namespace Location {

using namespace Platform;

/**
 * @brief Address component structure
 */
struct AddressComponents {
    std::string streetNumber;
    std::string street;
    std::string neighborhood;
    std::string city;
    std::string county;
    std::string state;
    std::string country;
    std::string countryCode;
    std::string postalCode;
    std::string formattedAddress;
};

/**
 * @brief Geocoding result
 */
struct GeocodingResult {
    LocationCoordinate coordinate;
    AddressComponents address;
    std::string placeId;
    std::string displayName;
    double confidence = 0.0;    ///< Confidence level 0-1
    std::string category;       ///< Place category (road, building, etc.)
};

/**
 * @brief Geocoding error codes
 */
enum class GeocodingError {
    None,
    NetworkError,
    InvalidRequest,
    NoResults,
    QuotaExceeded,
    ServerError,
    ParseError
};

/**
 * @brief Geocoding configuration
 */
struct GeocodingConfig {
    std::string provider = "nominatim";     ///< Provider: nominatim, google, mapbox
    std::string apiUrl = "https://nominatim.openstreetmap.org";
    std::string apiKey;                     ///< API key (if required)
    std::string userAgent = "NovaEngine/1.0";
    int timeoutMs = 10000;
    int maxResults = 5;
    bool enableCache = true;
    int cacheTTLMinutes = 60;
    std::string language = "en";            ///< Preferred language for results
};

using GeocodingCallback = std::function<void(const std::vector<GeocodingResult>& results,
                                              GeocodingError error,
                                              const std::string& message)>;

/**
 * @brief Geocoding service for address-coordinate conversion
 *
 * Supports:
 * - Forward geocoding (address to coordinates)
 * - Reverse geocoding (coordinates to address)
 * - Multiple providers (Nominatim, Google, Mapbox)
 * - Result caching for offline use
 * - Batch geocoding
 */
class GeocodingService {
public:
    /**
     * @brief Get singleton instance
     */
    static GeocodingService& Instance();

    // Delete copy/move
    GeocodingService(const GeocodingService&) = delete;
    GeocodingService& operator=(const GeocodingService&) = delete;

    /**
     * @brief Initialize the geocoding service
     */
    void Initialize(const GeocodingConfig& config = {});

    /**
     * @brief Shutdown the service
     */
    void Shutdown();

    // === Forward Geocoding ===

    /**
     * @brief Convert address to coordinates
     * @param address Address string to geocode
     * @param callback Called with results
     */
    void ForwardGeocode(const std::string& address, GeocodingCallback callback);

    /**
     * @brief Forward geocode with location bias
     * @param address Address to geocode
     * @param nearLocation Prefer results near this location
     * @param callback Called with results
     */
    void ForwardGeocodeNear(const std::string& address,
                            const LocationCoordinate& nearLocation,
                            GeocodingCallback callback);

    /**
     * @brief Forward geocode within bounds
     * @param address Address to geocode
     * @param sw Southwest corner of bounds
     * @param ne Northeast corner of bounds
     * @param callback Called with results
     */
    void ForwardGeocodeWithinBounds(const std::string& address,
                                     const LocationCoordinate& sw,
                                     const LocationCoordinate& ne,
                                     GeocodingCallback callback);

    // === Reverse Geocoding ===

    /**
     * @brief Convert coordinates to address
     * @param coordinate Coordinates to reverse geocode
     * @param callback Called with results
     */
    void ReverseGeocode(const LocationCoordinate& coordinate, GeocodingCallback callback);

    /**
     * @brief Reverse geocode with zoom level
     * @param coordinate Coordinates to reverse geocode
     * @param zoomLevel Zoom level (0-18, higher = more detailed)
     * @param callback Called with results
     */
    void ReverseGeocodeAtZoom(const LocationCoordinate& coordinate,
                               int zoomLevel,
                               GeocodingCallback callback);

    // === Batch Operations ===

    /**
     * @brief Batch forward geocode
     * @param addresses List of addresses
     * @param callback Called with all results (one vector per address)
     */
    void BatchForwardGeocode(const std::vector<std::string>& addresses,
                              std::function<void(const std::vector<std::vector<GeocodingResult>>&,
                                                GeocodingError)> callback);

    /**
     * @brief Batch reverse geocode
     * @param coordinates List of coordinates
     * @param callback Called with all results
     */
    void BatchReverseGeocode(const std::vector<LocationCoordinate>& coordinates,
                              std::function<void(const std::vector<std::vector<GeocodingResult>>&,
                                                GeocodingError)> callback);

    // === Cache Management ===

    /**
     * @brief Get cached forward geocoding result
     */
    [[nodiscard]] std::vector<GeocodingResult> GetCachedForward(const std::string& address) const;

    /**
     * @brief Get cached reverse geocoding result
     */
    [[nodiscard]] std::vector<GeocodingResult> GetCachedReverse(const LocationCoordinate& coord) const;

    /**
     * @brief Clear geocoding cache
     */
    void ClearCache();

    /**
     * @brief Save cache to file for offline use
     */
    bool SaveCache(const std::string& filepath) const;

    /**
     * @brief Load cache from file
     */
    bool LoadCache(const std::string& filepath);

    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        size_t forwardEntries;
        size_t reverseEntries;
        size_t totalSizeBytes;
    };
    [[nodiscard]] CacheStats GetCacheStats() const;

private:
    GeocodingService() = default;
    ~GeocodingService() = default;

    // Provider-specific implementation
    void NominatimForward(const std::string& address,
                           const std::string& params,
                           GeocodingCallback callback);
    void NominatimReverse(const LocationCoordinate& coord,
                           int zoom,
                           GeocodingCallback callback);

    std::vector<GeocodingResult> ParseNominatimResponse(const std::string& json);
    std::string HttpGet(const std::string& url);

    // Cache key generation
    std::string MakeForwardCacheKey(const std::string& address) const;
    std::string MakeReverseCacheKey(const LocationCoordinate& coord) const;

    // Configuration
    GeocodingConfig m_config;
    bool m_initialized = false;

    // Cache
    struct CacheEntry {
        std::vector<GeocodingResult> results;
        std::chrono::system_clock::time_point timestamp;
    };
    std::map<std::string, CacheEntry> m_forwardCache;
    std::map<std::string, CacheEntry> m_reverseCache;
    mutable std::mutex m_cacheMutex;
};

} // namespace Location
} // namespace Nova
