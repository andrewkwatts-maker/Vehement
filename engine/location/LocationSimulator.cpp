/**
 * @file LocationSimulator.cpp
 * @brief Location simulation implementation
 */

#include "LocationSimulator.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace Nova {
namespace Location {

static constexpr double EARTH_RADIUS_M = 6371000.0;

LocationSimulator& LocationSimulator::Instance() {
    static LocationSimulator instance;
    return instance;
}

void LocationSimulator::Initialize() {
    if (m_initialized) return;

    // Set default location (Melbourne, Australia)
    m_currentLocation.coordinate.latitude = -37.8136;
    m_currentLocation.coordinate.longitude = 144.9631;
    m_currentLocation.horizontalAccuracy = m_simulatedHAccuracy;
    m_currentLocation.verticalAccuracy = m_simulatedVAccuracy;
    m_currentLocation.provider = "Simulator";

    m_initialized = true;
    std::cout << "[LocationSimulator] Initialized" << std::endl;
}

void LocationSimulator::Shutdown() {
    Stop();
    m_initialized = false;
}

void LocationSimulator::SetMode(SimulationMode mode) {
    bool wasRunning = m_running;
    if (wasRunning) {
        Stop();
    }

    m_mode = mode;
    m_currentWaypoint = 0;
    m_segmentProgress = 0.0;
    m_playbackIndex = 0;

    if (wasRunning) {
        Start();
    }
}

void LocationSimulator::Start() {
    if (m_running) return;

    m_stopRequested = false;
    m_running = true;
    m_playbackStartTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    m_simThread = std::thread([this]() {
        SimulationThread();
    });

    std::cout << "[LocationSimulator] Started" << std::endl;
}

void LocationSimulator::Stop() {
    if (!m_running) return;

    m_stopRequested = true;
    if (m_simThread.joinable()) {
        m_simThread.join();
    }

    m_running = false;
    std::cout << "[LocationSimulator] Stopped" << std::endl;
}

void LocationSimulator::SetPaused(bool paused) {
    m_paused = paused;
}

void LocationSimulator::SetLocation(const LocationCoordinate& coord) {
    std::lock_guard<std::mutex> lock(m_locationMutex);
    m_currentLocation.coordinate = coord;
    m_currentLocation.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void LocationSimulator::SetLocation(const LocationData& location) {
    std::lock_guard<std::mutex> lock(m_locationMutex);
    m_currentLocation = location;
    m_currentLocation.isMockLocation = true;
    m_currentLocation.provider = "Simulator";
}

void LocationSimulator::Teleport(const LocationCoordinate& coord) {
    SetLocation(coord);
    NotifyLocation(GetCurrentLocation());
}

void LocationSimulator::SetRoute(const std::vector<RouteWaypoint>& waypoints) {
    m_route = waypoints;
    m_currentWaypoint = 0;
    m_segmentProgress = 0.0;
}

void LocationSimulator::AddWaypoint(const RouteWaypoint& waypoint) {
    m_route.push_back(waypoint);
}

void LocationSimulator::ClearRoute() {
    m_route.clear();
    m_currentWaypoint = 0;
    m_segmentProgress = 0.0;
}

float LocationSimulator::GetRouteProgress() const {
    if (m_route.empty()) return 0.0f;

    double totalSegments = static_cast<double>(m_route.size() - 1);
    if (totalSegments <= 0) return m_currentWaypoint > 0 ? 1.0f : 0.0f;

    double progress = (static_cast<double>(m_currentWaypoint) + m_segmentProgress) / totalSegments;
    return static_cast<float>(std::clamp(progress, 0.0, 1.0));
}

bool LocationSimulator::LoadTrack(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file) return false;

    m_track.clear();
    std::string line;

    // Simple GPX parsing (very basic)
    while (std::getline(file, line)) {
        // Find <trkpt lat="..." lon="...">
        size_t trkptPos = line.find("<trkpt");
        if (trkptPos == std::string::npos) continue;

        size_t latPos = line.find("lat=\"", trkptPos);
        size_t lonPos = line.find("lon=\"", trkptPos);

        if (latPos == std::string::npos || lonPos == std::string::npos) continue;

        latPos += 5;
        lonPos += 5;

        size_t latEnd = line.find("\"", latPos);
        size_t lonEnd = line.find("\"", lonPos);

        if (latEnd == std::string::npos || lonEnd == std::string::npos) continue;

        try {
            TrackPoint point;
            point.location.coordinate.latitude = std::stod(line.substr(latPos, latEnd - latPos));
            point.location.coordinate.longitude = std::stod(line.substr(lonPos, lonEnd - lonPos));
            point.location.horizontalAccuracy = 10.0;
            point.location.provider = "GPX Track";
            point.relativeTimeMs = m_track.size() * 1000; // 1 second between points

            // Look for elevation
            size_t elePos = line.find("<ele>");
            if (elePos != std::string::npos) {
                elePos += 5;
                size_t eleEnd = line.find("</ele>", elePos);
                if (eleEnd != std::string::npos) {
                    point.location.altitude = std::stod(line.substr(elePos, eleEnd - elePos));
                }
            }

            m_track.push_back(point);
        } catch (...) {
            continue;
        }
    }

    std::cout << "[LocationSimulator] Loaded track with " << m_track.size() << " points" << std::endl;
    return !m_track.empty();
}

void LocationSimulator::SetTrack(const std::vector<TrackPoint>& track) {
    m_track = track;
    m_playbackIndex = 0;
}

void LocationSimulator::RecordPoint() {
    TrackPoint point;
    {
        std::lock_guard<std::mutex> lock(m_locationMutex);
        point.location = m_currentLocation;
    }

    if (m_recordedTrack.empty()) {
        point.relativeTimeMs = 0;
    } else {
        point.relativeTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() -
            m_recordedTrack[0].location.timestamp;
    }

    m_recordedTrack.push_back(point);
}

bool LocationSimulator::SaveTrack(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file) return false;

    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<gpx version=\"1.1\" creator=\"NovaEngine\">\n";
    file << "  <trk>\n";
    file << "    <name>Recorded Track</name>\n";
    file << "    <trkseg>\n";

    for (const auto& point : m_recordedTrack) {
        file << "      <trkpt lat=\"" << std::fixed << std::setprecision(7)
             << point.location.coordinate.latitude
             << "\" lon=\"" << point.location.coordinate.longitude << "\">\n";
        if (point.location.altitude != 0) {
            file << "        <ele>" << point.location.altitude << "</ele>\n";
        }
        file << "      </trkpt>\n";
    }

    file << "    </trkseg>\n";
    file << "  </trk>\n";
    file << "</gpx>\n";

    return true;
}

void LocationSimulator::ConfigureRandomWalk(const RandomWalkConfig& config) {
    m_randomWalkConfig = config;
    m_randomHeading = std::uniform_real_distribution<double>(0, 360)(m_rng);
    m_lastDirectionChange = 0;

    // Start at center
    std::lock_guard<std::mutex> lock(m_locationMutex);
    m_currentLocation.coordinate = config.center;
}

void LocationSimulator::SetLocationCallback(LocationCallback callback) {
    m_callback = std::move(callback);
}

LocationData LocationSimulator::GetCurrentLocation() const {
    std::lock_guard<std::mutex> lock(m_locationMutex);
    return m_currentLocation;
}

void LocationSimulator::SetSimulatedAccuracy(double horizontalMeters, double verticalMeters) {
    m_simulatedHAccuracy = horizontalMeters;
    m_simulatedVAccuracy = verticalMeters >= 0 ? verticalMeters : horizontalMeters * 1.5;
}

void LocationSimulator::SetAccuracyJitter(bool enable, double maxJitterMeters) {
    m_accuracyJitterEnabled = enable;
    m_accuracyJitterMax = maxJitterMeters;
}

void LocationSimulator::SetPositionNoise(double noiseMeters) {
    m_positionNoise = noiseMeters;
}

void LocationSimulator::SetUpdateInterval(int64_t milliseconds) {
    m_updateIntervalMs = milliseconds;
}

// === Private Implementation ===

void LocationSimulator::SimulationThread() {
    while (!m_stopRequested) {
        if (!m_paused) {
            switch (m_mode) {
                case SimulationMode::Manual:
                    UpdateManual();
                    break;
                case SimulationMode::Route:
                    UpdateRoute();
                    break;
                case SimulationMode::Playback:
                    UpdatePlayback();
                    break;
                case SimulationMode::RandomWalk:
                    UpdateRandomWalk();
                    break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(m_updateIntervalMs));
    }
}

void LocationSimulator::UpdateManual() {
    // Just notify with current location
    NotifyLocation(GetCurrentLocation());
}

void LocationSimulator::UpdateRoute() {
    if (m_route.size() < 2) {
        NotifyLocation(GetCurrentLocation());
        return;
    }

    // Handle pause at waypoint
    if (m_waypointPauseRemaining > 0) {
        m_waypointPauseRemaining -= m_updateIntervalMs;
        NotifyLocation(GetCurrentLocation());
        return;
    }

    // Get current and next waypoints
    const auto& current = m_route[m_currentWaypoint];
    const auto& next = m_route[(m_currentWaypoint + 1) % m_route.size()];

    // Calculate distance and time for this segment
    double segmentDistance = current.coordinate.DistanceTo(next.coordinate);
    double speedMps = next.speedMps > 0 ? next.speedMps : 1.4;
    double segmentTime = segmentDistance / speedMps;

    // Advance progress
    double updateTimeSeconds = m_updateIntervalMs / 1000.0;
    m_segmentProgress += updateTimeSeconds / segmentTime;

    if (m_segmentProgress >= 1.0) {
        // Move to next waypoint
        m_currentWaypoint++;
        m_segmentProgress = 0.0;

        if (m_currentWaypoint >= m_route.size() - 1) {
            if (m_loopRoute) {
                m_currentWaypoint = 0;
            } else {
                m_currentWaypoint = m_route.size() - 2;
                m_segmentProgress = 1.0;
            }
        }

        // Start pause at new waypoint
        m_waypointPauseRemaining = m_route[(m_currentWaypoint + 1) % m_route.size()].pauseMs;
    }

    // Interpolate position
    LocationData location = InterpolateBetweenPoints(
        m_route[m_currentWaypoint].coordinate,
        m_route[(m_currentWaypoint + 1) % m_route.size()].coordinate,
        m_segmentProgress);

    location.speed = speedMps;
    location.course = current.coordinate.BearingTo(next.coordinate);

    {
        std::lock_guard<std::mutex> lock(m_locationMutex);
        m_currentLocation = location;
    }

    NotifyLocation(location);
}

void LocationSimulator::UpdatePlayback() {
    if (m_track.empty()) {
        NotifyLocation(GetCurrentLocation());
        return;
    }

    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    int64_t playbackTime = static_cast<int64_t>((now - m_playbackStartTime) * m_playbackSpeed);

    // Find appropriate track point
    while (m_playbackIndex < m_track.size() - 1 &&
           m_track[m_playbackIndex + 1].relativeTimeMs <= playbackTime) {
        m_playbackIndex++;
    }

    // Interpolate between points
    LocationData location;
    if (m_playbackIndex >= m_track.size() - 1) {
        location = m_track.back().location;
    } else {
        const auto& p1 = m_track[m_playbackIndex];
        const auto& p2 = m_track[m_playbackIndex + 1];

        int64_t segmentTime = p2.relativeTimeMs - p1.relativeTimeMs;
        double t = segmentTime > 0 ?
            static_cast<double>(playbackTime - p1.relativeTimeMs) / segmentTime : 0.0;
        t = std::clamp(t, 0.0, 1.0);

        location = InterpolateBetweenPoints(p1.location.coordinate, p2.location.coordinate, t);
        location.altitude = p1.location.altitude + (p2.location.altitude - p1.location.altitude) * t;

        // Estimate speed from track points
        double dist = p1.location.coordinate.DistanceTo(p2.location.coordinate);
        double time = segmentTime / 1000.0;
        location.speed = time > 0 ? dist / time : 0;
    }

    {
        std::lock_guard<std::mutex> lock(m_locationMutex);
        m_currentLocation = location;
    }

    NotifyLocation(location);
}

void LocationSimulator::UpdateRandomWalk() {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Change direction periodically
    if (now - m_lastDirectionChange > m_randomWalkConfig.directionChangeIntervalMs) {
        std::uniform_real_distribution<double> headingDist(
            -m_randomWalkConfig.maxHeadingChange,
            m_randomWalkConfig.maxHeadingChange);
        m_randomHeading = std::fmod(m_randomHeading + headingDist(m_rng) + 360.0, 360.0);
        m_lastDirectionChange = now;
    }

    // Random speed
    std::uniform_real_distribution<double> speedDist(
        m_randomWalkConfig.minSpeedMps,
        m_randomWalkConfig.maxSpeedMps);
    double speed = speedDist(m_rng);

    // Calculate distance to move
    double distance = speed * m_updateIntervalMs / 1000.0;

    // Calculate new position
    LocationCoordinate currentCoord;
    {
        std::lock_guard<std::mutex> lock(m_locationMutex);
        currentCoord = m_currentLocation.coordinate;
    }

    // Move in current heading direction
    double headingRad = m_randomHeading * M_PI / 180.0;
    double dLat = (distance * std::cos(headingRad)) / 111320.0;
    double dLon = (distance * std::sin(headingRad)) / (111320.0 * std::cos(currentCoord.latitude * M_PI / 180.0));

    LocationCoordinate newCoord;
    newCoord.latitude = currentCoord.latitude + dLat;
    newCoord.longitude = currentCoord.longitude + dLon;

    // Keep within radius of center
    double distFromCenter = newCoord.DistanceTo(m_randomWalkConfig.center);
    if (distFromCenter > m_randomWalkConfig.radiusMeters) {
        // Turn back toward center
        m_randomHeading = m_randomWalkConfig.center.latitude > newCoord.latitude ? 0 : 180;
        if (m_randomWalkConfig.center.longitude > newCoord.longitude) {
            m_randomHeading += 90;
        } else {
            m_randomHeading -= 90;
        }
        m_randomHeading = std::fmod(m_randomHeading + 360.0, 360.0);

        // Don't move this update
        newCoord = currentCoord;
    }

    LocationData location;
    location.coordinate = AddNoise(newCoord);
    location.speed = speed;
    location.course = m_randomHeading;
    location.horizontalAccuracy = m_simulatedHAccuracy;
    location.verticalAccuracy = m_simulatedVAccuracy;
    location.timestamp = now;
    location.provider = "Simulator";
    location.isMockLocation = true;

    {
        std::lock_guard<std::mutex> lock(m_locationMutex);
        m_currentLocation = location;
    }

    NotifyLocation(location);
}

LocationData LocationSimulator::InterpolateBetweenPoints(const LocationCoordinate& from,
                                                          const LocationCoordinate& to,
                                                          double t) {
    LocationData location;
    location.coordinate.latitude = from.latitude + (to.latitude - from.latitude) * t;
    location.coordinate.longitude = from.longitude + (to.longitude - from.longitude) * t;
    location.coordinate = AddNoise(location.coordinate);

    location.horizontalAccuracy = m_simulatedHAccuracy;
    location.verticalAccuracy = m_simulatedVAccuracy;

    if (m_accuracyJitterEnabled) {
        std::uniform_real_distribution<double> jitter(-m_accuracyJitterMax, m_accuracyJitterMax);
        location.horizontalAccuracy += jitter(m_rng);
        location.horizontalAccuracy = std::max(1.0, location.horizontalAccuracy);
    }

    location.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    location.provider = "Simulator";
    location.isMockLocation = true;

    return location;
}

LocationCoordinate LocationSimulator::AddNoise(const LocationCoordinate& coord) {
    if (m_positionNoise <= 0) {
        return coord;
    }

    std::uniform_real_distribution<double> noiseDist(-m_positionNoise, m_positionNoise);

    double noiseLat = noiseDist(m_rng) / 111320.0;
    double noiseLon = noiseDist(m_rng) / (111320.0 * std::cos(coord.latitude * M_PI / 180.0));

    LocationCoordinate noisy;
    noisy.latitude = coord.latitude + noiseLat;
    noisy.longitude = coord.longitude + noiseLon;

    return noisy;
}

void LocationSimulator::NotifyLocation(const LocationData& location) {
    if (m_callback) {
        m_callback(location);
    }
}

} // namespace Location
} // namespace Nova
