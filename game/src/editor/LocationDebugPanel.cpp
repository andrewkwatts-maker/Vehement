/**
 * @file LocationDebugPanel.cpp
 * @brief Location debug panel implementation
 */

#include "LocationDebugPanel.hpp"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <iostream>

// ImGui would normally be included here
// #include <imgui.h>

namespace Vehement {
namespace Editor {

LocationDebugPanel& LocationDebugPanel::Instance() {
    static LocationDebugPanel instance;
    return instance;
}

void LocationDebugPanel::Initialize() {
    if (m_initialized) return;

    // Start receiving location updates
    Nova::Location::LocationManager::Instance().StartUpdates(
        [this](const LocationData& location) {
            OnLocationUpdate(location);
        });

    m_initialized = true;
    std::cout << "[LocationDebugPanel] Initialized" << std::endl;
}

void LocationDebugPanel::Shutdown() {
    StopRecording();
    DisableSimulator();

    Nova::Location::LocationManager::Instance().StopUpdates();

    m_initialized = false;
}

void LocationDebugPanel::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update world position
    if (m_currentLocation.IsValid()) {
        m_worldPosition = Location::WorldLocation::Instance().GPSToWorld(m_currentLocation.coordinate);

        // Auto-center map
        if (m_config.autoCenter) {
            m_mapCenter = m_currentLocation.coordinate;
        }
    }
}

void LocationDebugPanel::OnLocationUpdate(const LocationData& location) {
    m_currentLocation = location;

    // Add to history
    if (location.IsValid()) {
        std::lock_guard<std::mutex> lock(m_historyMutex);

        LocationHistoryPoint point;
        point.coordinate = location.coordinate;
        point.timestamp = location.timestamp;
        point.accuracy = static_cast<float>(location.horizontalAccuracy);

        m_history.push_back(point);

        while (m_history.size() > static_cast<size_t>(m_config.maxHistoryPoints)) {
            m_history.pop_front();
        }

        // Record if enabled
        if (m_recording) {
            m_recordedPath.push_back(point);
        }
    }
}

void LocationDebugPanel::Render() {
    if (!m_visible) return;

    // In a real implementation, this would use ImGui
    // For now, just log the render call
    // ImGui::Begin("Location Debug", &m_visible);

    RenderLocationInfo();
    RenderServiceStatus();
    RenderControls();
    RenderRecordingControls();
    RenderSimulatorControls();
    RenderMapView();

    // ImGui::End();
}

void LocationDebugPanel::RenderLocationInfo() {
    // Location info section
    // ImGui::Text("Current Location:");

    if (m_currentLocation.IsValid()) {
        std::cout << "[LocationDebug] Current: "
                  << std::fixed << std::setprecision(6)
                  << m_currentLocation.coordinate.latitude << ", "
                  << m_currentLocation.coordinate.longitude << std::endl;
        std::cout << "  Accuracy: " << m_currentLocation.horizontalAccuracy << "m" << std::endl;
        std::cout << "  Speed: " << m_currentLocation.speed << " m/s" << std::endl;
        std::cout << "  World: (" << m_worldPosition.x << ", "
                  << m_worldPosition.y << ", " << m_worldPosition.z << ")" << std::endl;
    } else {
        std::cout << "[LocationDebug] No location available" << std::endl;
    }
}

void LocationDebugPanel::RenderMapView() {
    // Map visualization (would use OpenGL/ImGui drawing)
    // For HTML version, this is handled by JavaScript
}

void LocationDebugPanel::RenderControls() {
    // Accuracy selector
    const char* accuracyOptions[] = { "Best", "Good (100m)", "Low (1km)" };
    // ImGui::Combo("Accuracy", &m_selectedAccuracy, accuracyOptions, 3);

    LocationAccuracy accuracy = LocationAccuracy::Best;
    switch (m_selectedAccuracy) {
        case 0: accuracy = LocationAccuracy::Best; break;
        case 1: accuracy = LocationAccuracy::HundredMeters; break;
        case 2: accuracy = LocationAccuracy::Kilometer; break;
    }

    auto* service = Nova::Location::LocationManager::Instance().GetPlatformService();
    if (service) {
        service->SetDesiredAccuracy(accuracy);
    }

    // Background updates toggle
    // ImGui::Checkbox("Background Updates", &m_backgroundUpdates);
    if (service) {
        service->SetBackgroundUpdatesEnabled(m_backgroundUpdates);
    }
}

void LocationDebugPanel::RenderServiceStatus() {
    auto* service = Nova::Location::LocationManager::Instance().GetPlatformService();
    if (!service) {
        std::cout << "[LocationDebug] No location service" << std::endl;
        return;
    }

    std::cout << "[LocationDebug] Service: " << service->GetServiceName() << std::endl;
    std::cout << "  Enabled: " << (service->AreLocationServicesEnabled() ? "Yes" : "No") << std::endl;
    std::cout << "  Permission: " << (service->HasPermission() ? "Granted" : "Not granted") << std::endl;
    std::cout << "  Updating: " << (service->IsUpdating() ? "Yes" : "No") << std::endl;
}

void LocationDebugPanel::RenderRecordingControls() {
    if (m_recording) {
        std::cout << "[LocationDebug] Recording... (" << m_recordedPath.size() << " points)" << std::endl;
        // ImGui::Text("Recording... (%zu points)", m_recordedPath.size());
        // if (ImGui::Button("Stop Recording")) StopRecording();
    } else {
        // if (ImGui::Button("Start Recording")) StartRecording();
    }

    if (!m_recordedPath.empty()) {
        // if (ImGui::Button("Save Recording")) SaveRecording("recorded_path.gpx");
        // if (ImGui::Button("Clear")) ClearRecording();
    }
}

void LocationDebugPanel::RenderSimulatorControls() {
    // Mock location input
    // ImGui::InputFloat("Latitude", &m_mockLatitude, 0.0001f, 0.001f, "%.6f");
    // ImGui::InputFloat("Longitude", &m_mockLongitude, 0.0001f, 0.001f, "%.6f");

    // if (ImGui::Button("Set Mock Location")) {
    //     SetMockLocation({m_mockLatitude, m_mockLongitude});
    // }

    // Route speed
    // ImGui::SliderFloat("Route Speed (m/s)", &m_routeSpeed, 0.5f, 30.0f);
}

// === Location Controls ===

void LocationDebugPanel::EnableSimulator() {
    m_simulatorEnabled = true;
    Nova::Location::LocationSimulator::Instance().Initialize();
    Nova::Location::LocationSimulator::Instance().Start();

    Nova::Location::LocationSimulator::Instance().SetLocationCallback(
        [this](const LocationData& location) {
            OnLocationUpdate(location);
        });

    std::cout << "[LocationDebugPanel] Simulator enabled" << std::endl;
}

void LocationDebugPanel::DisableSimulator() {
    if (!m_simulatorEnabled) return;

    Nova::Location::LocationSimulator::Instance().Stop();
    m_simulatorEnabled = false;

    std::cout << "[LocationDebugPanel] Simulator disabled" << std::endl;
}

void LocationDebugPanel::SetMockLocation(const LocationCoordinate& coord) {
    if (!m_simulatorEnabled) {
        EnableSimulator();
    }

    Nova::Location::LocationSimulator::Instance().SetMode(SimulationMode::Manual);
    Nova::Location::LocationSimulator::Instance().Teleport(coord);

    std::cout << "[LocationDebugPanel] Mock location set: " << coord.latitude
              << ", " << coord.longitude << std::endl;
}

void LocationDebugPanel::StartRecording() {
    m_recording = true;
    m_recordedPath.clear();
    std::cout << "[LocationDebugPanel] Recording started" << std::endl;
}

void LocationDebugPanel::StopRecording() {
    m_recording = false;
    std::cout << "[LocationDebugPanel] Recording stopped (" << m_recordedPath.size()
              << " points)" << std::endl;
}

void LocationDebugPanel::ClearRecording() {
    m_recordedPath.clear();
}

bool LocationDebugPanel::SaveRecording(const std::string& filepath) {
    return Nova::Location::LocationSimulator::Instance().SaveTrack(filepath);
}

bool LocationDebugPanel::LoadAndPlayback(const std::string& filepath) {
    if (!m_simulatorEnabled) {
        EnableSimulator();
    }

    if (Nova::Location::LocationSimulator::Instance().LoadTrack(filepath)) {
        Nova::Location::LocationSimulator::Instance().SetMode(SimulationMode::Playback);
        Nova::Location::LocationSimulator::Instance().Start();
        return true;
    }
    return false;
}

// === Data Access ===

LocationData LocationDebugPanel::GetCurrentLocation() const {
    return m_currentLocation;
}

std::vector<LocationHistoryPoint> LocationDebugPanel::GetHistory() const {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    return std::vector<LocationHistoryPoint>(m_history.begin(), m_history.end());
}

glm::vec3 LocationDebugPanel::GetWorldPosition() const {
    return m_worldPosition;
}

std::string LocationDebugPanel::GetServiceStatus() const {
    auto* service = Nova::Location::LocationManager::Instance().GetPlatformService();
    if (!service) {
        return "No service";
    }

    std::ostringstream ss;
    ss << service->GetServiceName();
    ss << " - " << (service->HasPermission() ? "Authorized" : "Not authorized");
    ss << " - " << (service->IsUpdating() ? "Active" : "Inactive");

    return ss.str();
}

void LocationDebugPanel::SetMapCenter(const LocationCoordinate& center) {
    m_mapCenter = center;
    m_config.autoCenter = false;
}

void LocationDebugPanel::SetMapZoom(float zoom) {
    m_mapZoom = zoom;
    m_config.mapZoom = zoom;
}

// === JSON Export for HTML ===

std::string LocationDebugPanel::GetLocationDataJSON() const {
    std::ostringstream json;
    json << std::fixed << std::setprecision(7);

    json << "{\n";

    // Current location
    json << "  \"current\": {\n";
    json << "    \"latitude\": " << m_currentLocation.coordinate.latitude << ",\n";
    json << "    \"longitude\": " << m_currentLocation.coordinate.longitude << ",\n";
    json << "    \"altitude\": " << m_currentLocation.altitude << ",\n";
    json << "    \"accuracy\": " << m_currentLocation.horizontalAccuracy << ",\n";
    json << "    \"speed\": " << m_currentLocation.speed << ",\n";
    json << "    \"heading\": " << m_currentLocation.course << ",\n";
    json << "    \"timestamp\": " << m_currentLocation.timestamp << ",\n";
    json << "    \"valid\": " << (m_currentLocation.IsValid() ? "true" : "false") << ",\n";
    json << "    \"mock\": " << (m_currentLocation.isMockLocation ? "true" : "false") << "\n";
    json << "  },\n";

    // World position
    json << "  \"world\": {\n";
    json << "    \"x\": " << m_worldPosition.x << ",\n";
    json << "    \"y\": " << m_worldPosition.y << ",\n";
    json << "    \"z\": " << m_worldPosition.z << "\n";
    json << "  },\n";

    // History
    json << "  \"history\": [\n";
    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        for (size_t i = 0; i < m_history.size(); ++i) {
            const auto& point = m_history[i];
            json << "    [" << point.coordinate.latitude << ", "
                 << point.coordinate.longitude << "]";
            if (i < m_history.size() - 1) json << ",";
            json << "\n";
        }
    }
    json << "  ],\n";

    // Status
    json << "  \"status\": {\n";
    json << "    \"service\": \"" << GetServiceStatus() << "\",\n";
    json << "    \"recording\": " << (m_recording ? "true" : "false") << ",\n";
    json << "    \"simulator\": " << (m_simulatorEnabled ? "true" : "false") << ",\n";
    json << "    \"recordedPoints\": " << m_recordedPath.size() << "\n";
    json << "  },\n";

    // Map settings
    json << "  \"map\": {\n";
    json << "    \"centerLat\": " << m_mapCenter.latitude << ",\n";
    json << "    \"centerLon\": " << m_mapCenter.longitude << ",\n";
    json << "    \"zoom\": " << m_mapZoom << ",\n";
    json << "    \"autoCenter\": " << (m_config.autoCenter ? "true" : "false") << "\n";
    json << "  }\n";

    json << "}\n";

    return json.str();
}

void LocationDebugPanel::ProcessHTMLCommand(const std::string& command, const std::string& data) {
    if (command == "setMockLocation") {
        // Parse data as "lat,lon"
        size_t comma = data.find(',');
        if (comma != std::string::npos) {
            double lat = std::stod(data.substr(0, comma));
            double lon = std::stod(data.substr(comma + 1));
            SetMockLocation({lat, lon});
        }
    } else if (command == "startRecording") {
        StartRecording();
    } else if (command == "stopRecording") {
        StopRecording();
    } else if (command == "clearRecording") {
        ClearRecording();
    } else if (command == "enableSimulator") {
        EnableSimulator();
    } else if (command == "disableSimulator") {
        DisableSimulator();
    } else if (command == "setZoom") {
        SetMapZoom(std::stof(data));
    } else if (command == "setAutoCenter") {
        m_config.autoCenter = (data == "true");
    }
}

} // namespace Editor
} // namespace Vehement
