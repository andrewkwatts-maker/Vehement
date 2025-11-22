/**
 * @file LocationSimulator.hpp
 * @brief Location simulation for testing
 */

#pragma once

#include "../platform/LocationService.hpp"
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <random>

namespace Nova {
namespace Location {

using namespace Platform;

/**
 * @brief Waypoint in a simulated route
 */
struct RouteWaypoint {
    LocationCoordinate coordinate;
    double speedMps = 1.4;          ///< Speed to next waypoint (m/s), ~5 km/h walking
    int64_t pauseMs = 0;            ///< Pause at this waypoint (ms)
};

/**
 * @brief Recorded GPS track point
 */
struct TrackPoint {
    LocationData location;
    int64_t relativeTimeMs;         ///< Time since track start
};

/**
 * @brief Simulation mode
 */
enum class SimulationMode {
    Manual,         ///< Manual position updates
    Route,          ///< Follow a predefined route
    Playback,       ///< Playback recorded track
    RandomWalk      ///< Random movement around a point
};

/**
 * @brief Random walk configuration
 */
struct RandomWalkConfig {
    LocationCoordinate center;
    double radiusMeters = 1000.0;
    double minSpeedMps = 0.5;
    double maxSpeedMps = 2.0;
    int64_t directionChangeIntervalMs = 5000;
    double maxHeadingChange = 45.0;     ///< Max heading change per interval
};

/**
 * @brief Location simulator for testing
 *
 * Simulates GPS location updates for testing without actual GPS hardware.
 * Supports:
 * - Manual location setting
 * - Route following
 * - Recorded track playback
 * - Random walk simulation
 */
class LocationSimulator {
public:
    /**
     * @brief Get singleton instance
     */
    static LocationSimulator& Instance();

    // Delete copy/move
    LocationSimulator(const LocationSimulator&) = delete;
    LocationSimulator& operator=(const LocationSimulator&) = delete;

    /**
     * @brief Initialize the simulator
     */
    void Initialize();

    /**
     * @brief Shutdown the simulator
     */
    void Shutdown();

    // === Mode Control ===

    /**
     * @brief Set simulation mode
     */
    void SetMode(SimulationMode mode);

    /**
     * @brief Get current simulation mode
     */
    [[nodiscard]] SimulationMode GetMode() const { return m_mode; }

    /**
     * @brief Start simulation
     */
    void Start();

    /**
     * @brief Stop simulation
     */
    void Stop();

    /**
     * @brief Pause/resume simulation
     */
    void SetPaused(bool paused);

    /**
     * @brief Check if simulation is running
     */
    [[nodiscard]] bool IsRunning() const { return m_running; }

    /**
     * @brief Check if simulation is paused
     */
    [[nodiscard]] bool IsPaused() const { return m_paused; }

    // === Manual Mode ===

    /**
     * @brief Set current location manually
     */
    void SetLocation(const LocationCoordinate& coord);

    /**
     * @brief Set full location data
     */
    void SetLocation(const LocationData& location);

    /**
     * @brief Teleport to a location (instant move)
     */
    void Teleport(const LocationCoordinate& coord);

    // === Route Mode ===

    /**
     * @brief Set route to follow
     */
    void SetRoute(const std::vector<RouteWaypoint>& waypoints);

    /**
     * @brief Add waypoint to current route
     */
    void AddWaypoint(const RouteWaypoint& waypoint);

    /**
     * @brief Clear current route
     */
    void ClearRoute();

    /**
     * @brief Set whether route loops
     */
    void SetRouteLooping(bool loop) { m_loopRoute = loop; }

    /**
     * @brief Get current route progress (0-1)
     */
    [[nodiscard]] float GetRouteProgress() const;

    /**
     * @brief Get current waypoint index
     */
    [[nodiscard]] size_t GetCurrentWaypointIndex() const { return m_currentWaypoint; }

    // === Playback Mode ===

    /**
     * @brief Load recorded track from file (GPX format)
     */
    bool LoadTrack(const std::string& filepath);

    /**
     * @brief Set track directly
     */
    void SetTrack(const std::vector<TrackPoint>& track);

    /**
     * @brief Record current location to track
     */
    void RecordPoint();

    /**
     * @brief Save recorded track to file
     */
    bool SaveTrack(const std::string& filepath) const;

    /**
     * @brief Set playback speed multiplier
     */
    void SetPlaybackSpeed(float multiplier) { m_playbackSpeed = multiplier; }

    /**
     * @brief Get playback speed
     */
    [[nodiscard]] float GetPlaybackSpeed() const { return m_playbackSpeed; }

    // === Random Walk Mode ===

    /**
     * @brief Configure random walk
     */
    void ConfigureRandomWalk(const RandomWalkConfig& config);

    // === Location Callback ===

    /**
     * @brief Set callback for simulated location updates
     */
    void SetLocationCallback(LocationCallback callback);

    /**
     * @brief Get current simulated location
     */
    [[nodiscard]] LocationData GetCurrentLocation() const;

    // === Accuracy Simulation ===

    /**
     * @brief Set simulated accuracy
     */
    void SetSimulatedAccuracy(double horizontalMeters, double verticalMeters = -1);

    /**
     * @brief Enable/disable accuracy jitter
     */
    void SetAccuracyJitter(bool enable, double maxJitterMeters = 5.0);

    /**
     * @brief Set GPS noise level (position jitter)
     */
    void SetPositionNoise(double noiseMeters);

    // === Update Rate ===

    /**
     * @brief Set update interval
     */
    void SetUpdateInterval(int64_t milliseconds);

    /**
     * @brief Get update interval
     */
    [[nodiscard]] int64_t GetUpdateInterval() const { return m_updateIntervalMs; }

private:
    LocationSimulator() = default;
    ~LocationSimulator() = default;

    void SimulationThread();
    void UpdateManual();
    void UpdateRoute();
    void UpdatePlayback();
    void UpdateRandomWalk();

    LocationData InterpolateBetweenPoints(const LocationCoordinate& from,
                                           const LocationCoordinate& to,
                                           double t);
    LocationCoordinate AddNoise(const LocationCoordinate& coord);
    void NotifyLocation(const LocationData& location);

    // State
    bool m_initialized = false;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_stopRequested{false};
    SimulationMode m_mode = SimulationMode::Manual;

    // Current location
    LocationData m_currentLocation;
    mutable std::mutex m_locationMutex;

    // Simulation thread
    std::thread m_simThread;
    int64_t m_updateIntervalMs = 1000;

    // Route mode
    std::vector<RouteWaypoint> m_route;
    size_t m_currentWaypoint = 0;
    double m_segmentProgress = 0.0;
    int64_t m_waypointPauseRemaining = 0;
    bool m_loopRoute = false;

    // Playback mode
    std::vector<TrackPoint> m_track;
    std::vector<TrackPoint> m_recordedTrack;
    size_t m_playbackIndex = 0;
    int64_t m_playbackStartTime = 0;
    float m_playbackSpeed = 1.0f;

    // Random walk mode
    RandomWalkConfig m_randomWalkConfig;
    double m_randomHeading = 0.0;
    int64_t m_lastDirectionChange = 0;

    // Accuracy simulation
    double m_simulatedHAccuracy = 10.0;
    double m_simulatedVAccuracy = 15.0;
    bool m_accuracyJitterEnabled = false;
    double m_accuracyJitterMax = 5.0;
    double m_positionNoise = 0.0;

    // Random number generation
    std::mt19937 m_rng{std::random_device{}()};

    // Callback
    LocationCallback m_callback;
};

} // namespace Location
} // namespace Nova
