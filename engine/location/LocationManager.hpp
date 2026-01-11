/**
 * @file LocationManager.hpp
 * @brief High-level location manager with caching and movement detection
 */

#pragma once

#include "../platform/LocationService.hpp"
#include <deque>
#include <chrono>
#include <optional>

namespace Nova {
namespace Location {

using namespace Platform;

/**
 * @brief Movement state based on location history
 */
enum class MovementState {
    Unknown,
    Stationary,     ///< User is not moving
    Walking,        ///< Slow movement (~1-2 m/s)
    Running,        ///< Fast walking (~3-5 m/s)
    Driving,        ///< Vehicle movement (~10+ m/s)
    HighSpeed       ///< Very fast movement (train, plane)
};

/**
 * @brief Location history entry
 */
struct LocationHistoryEntry {
    LocationData location;
    int64_t timestamp;
    double distanceFromPrevious;
    double speedEstimate;
};

/**
 * @brief Configuration for LocationManager
 */
struct LocationManagerConfig {
    int historyMaxSize = 100;           ///< Maximum locations to keep in history
    int64_t cacheTTLMs = 30000;         ///< Cache TTL in milliseconds
    double stationaryThresholdM = 10.0; ///< Distance threshold for stationary detection
    int64_t stationaryTimeMs = 60000;   ///< Time threshold for stationary detection
    bool enableSpeedEstimation = true;  ///< Calculate speed from location changes
    bool enableMovementDetection = true;///< Detect movement state changes
    double minDistanceUpdateM = 1.0;    ///< Minimum distance between updates
};

/**
 * @brief High-level location manager
 *
 * Provides:
 * - Platform abstraction
 * - Location caching with TTL
 * - Movement detection
 * - Distance and bearing calculations
 * - Speed estimation from location history
 * - Location history tracking
 */
class LocationManager {
public:
    using MovementCallback = std::function<void(MovementState newState, MovementState oldState)>;

    /**
     * @brief Get singleton instance
     */
    static LocationManager& Instance();

    // Delete copy/move
    LocationManager(const LocationManager&) = delete;
    LocationManager& operator=(const LocationManager&) = delete;

    /**
     * @brief Initialize the location manager
     * @param config Configuration options
     */
    void Initialize(const LocationManagerConfig& config = {});

    /**
     * @brief Shutdown the location manager
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // === Permission Management ===

    /**
     * @brief Request location permission
     */
    bool RequestPermission(bool alwaysAccess = false);

    /**
     * @brief Check if permission granted
     */
    [[nodiscard]] bool HasPermission() const;

    // === Location Access ===

    /**
     * @brief Get current location (may use cache)
     * @param maxAgeMs Maximum age of cached location (0 = always fresh)
     */
    void GetLocation(LocationCallback callback, int64_t maxAgeMs = 0);

    /**
     * @brief Get cached location if available and fresh
     * @param maxAgeMs Maximum age in milliseconds
     */
    [[nodiscard]] std::optional<LocationData> GetCachedLocation(int64_t maxAgeMs = 30000) const;

    /**
     * @brief Start continuous location updates
     */
    void StartUpdates(LocationCallback callback);

    /**
     * @brief Stop continuous location updates
     */
    void StopUpdates();

    /**
     * @brief Check if updates are active
     */
    [[nodiscard]] bool IsUpdating() const;

    // === Location Calculations ===

    /**
     * @brief Calculate distance between two coordinates
     * @return Distance in meters
     */
    [[nodiscard]] static double CalculateDistance(const LocationCoordinate& from,
                                                   const LocationCoordinate& to);

    /**
     * @brief Calculate bearing between two coordinates
     * @return Bearing in degrees (0-360, 0=North)
     */
    [[nodiscard]] static double CalculateBearing(const LocationCoordinate& from,
                                                  const LocationCoordinate& to);

    /**
     * @brief Calculate destination point given start, bearing, and distance
     */
    [[nodiscard]] static LocationCoordinate CalculateDestination(const LocationCoordinate& from,
                                                                  double bearingDegrees,
                                                                  double distanceMeters);

    /**
     * @brief Calculate midpoint between two coordinates
     */
    [[nodiscard]] static LocationCoordinate CalculateMidpoint(const LocationCoordinate& a,
                                                               const LocationCoordinate& b);

    /**
     * @brief Check if a point is within a bounding box
     */
    [[nodiscard]] static bool IsPointInBounds(const LocationCoordinate& point,
                                               const LocationCoordinate& sw,
                                               const LocationCoordinate& ne);

    // === Movement Detection ===

    /**
     * @brief Get current movement state
     */
    [[nodiscard]] MovementState GetMovementState() const;

    /**
     * @brief Get estimated speed in m/s
     */
    [[nodiscard]] double GetEstimatedSpeed() const;

    /**
     * @brief Get estimated heading in degrees
     */
    [[nodiscard]] double GetEstimatedHeading() const;

    /**
     * @brief Set movement state change callback
     */
    void SetMovementCallback(MovementCallback callback);

    // === History Access ===

    /**
     * @brief Get location history
     */
    [[nodiscard]] std::vector<LocationHistoryEntry> GetHistory() const;

    /**
     * @brief Get total distance traveled
     */
    [[nodiscard]] double GetTotalDistance() const;

    /**
     * @brief Clear location history
     */
    void ClearHistory();

    // === Platform Service Access ===

    /**
     * @brief Get the underlying platform service
     */
    [[nodiscard]] ILocationService* GetPlatformService() { return m_service.get(); }

    /**
     * @brief Set accuracy level
     */
    void SetAccuracy(LocationAccuracy accuracy);

    /**
     * @brief Set distance filter for updates
     */
    void SetDistanceFilter(double meters);

private:
    LocationManager() = default;
    ~LocationManager() = default;

    void OnLocationUpdate(const LocationData& location);
    void UpdateMovementState();
    MovementState EstimateMovementFromSpeed(double speed) const;

    // Platform service
    std::unique_ptr<ILocationService> m_service;

    // Configuration
    LocationManagerConfig m_config;
    bool m_initialized = false;

    // Cached location
    LocationData m_cachedLocation;
    std::chrono::steady_clock::time_point m_cacheTime;

    // Location history
    std::deque<LocationHistoryEntry> m_history;
    double m_totalDistance = 0.0;
    mutable std::mutex m_historyMutex;

    // Movement state
    MovementState m_movementState = MovementState::Unknown;
    double m_estimatedSpeed = 0.0;
    double m_estimatedHeading = 0.0;
    MovementCallback m_movementCallback;

    // User callbacks
    LocationCallback m_userCallback;
};

} // namespace Location
} // namespace Nova
