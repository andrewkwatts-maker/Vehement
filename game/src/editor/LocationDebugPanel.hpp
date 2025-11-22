/**
 * @file LocationDebugPanel.hpp
 * @brief Debug panel for location services in the editor
 */

#pragma once

#include "../../../engine/platform/LocationService.hpp"
#include "../../../engine/location/LocationManager.hpp"
#include "../../../engine/location/LocationSimulator.hpp"
#include "../location/WorldLocation.hpp"
#include <vector>
#include <deque>
#include <string>

namespace Vehement {
namespace Editor {

using namespace Nova::Platform;
using namespace Nova::Location;

/**
 * @brief Location history entry for visualization
 */
struct LocationHistoryPoint {
    LocationCoordinate coordinate;
    int64_t timestamp;
    float accuracy;
};

/**
 * @brief Debug panel configuration
 */
struct LocationDebugConfig {
    int maxHistoryPoints = 100;
    bool showAccuracyCircle = true;
    bool showPath = true;
    bool showCoordinates = true;
    bool autoCenter = true;
    float mapZoom = 15.0f;
};

/**
 * @brief Location debug panel for editor
 *
 * Provides:
 * - Current location display
 * - Location history/path visualization
 * - Mock location input
 * - Route recording and playback
 * - Accuracy indicator
 * - Platform service status
 */
class LocationDebugPanel {
public:
    /**
     * @brief Get singleton instance
     */
    static LocationDebugPanel& Instance();

    // Delete copy/move
    LocationDebugPanel(const LocationDebugPanel&) = delete;
    LocationDebugPanel& operator=(const LocationDebugPanel&) = delete;

    /**
     * @brief Initialize the debug panel
     */
    void Initialize();

    /**
     * @brief Shutdown the panel
     */
    void Shutdown();

    /**
     * @brief Update (call each frame)
     */
    void Update(float deltaTime);

    /**
     * @brief Render the debug panel (ImGui)
     */
    void Render();

    /**
     * @brief Check if panel is visible
     */
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    /**
     * @brief Set panel visibility
     */
    void SetVisible(bool visible) { m_visible = visible; }

    /**
     * @brief Toggle visibility
     */
    void ToggleVisible() { m_visible = !m_visible; }

    // === Location Controls ===

    /**
     * @brief Enable mock/simulator mode
     */
    void EnableSimulator();

    /**
     * @brief Disable simulator, use real location
     */
    void DisableSimulator();

    /**
     * @brief Set mock location
     */
    void SetMockLocation(const LocationCoordinate& coord);

    /**
     * @brief Start recording location path
     */
    void StartRecording();

    /**
     * @brief Stop recording
     */
    void StopRecording();

    /**
     * @brief Clear recorded path
     */
    void ClearRecording();

    /**
     * @brief Save recorded path to file
     */
    bool SaveRecording(const std::string& filepath);

    /**
     * @brief Load and play back a recorded path
     */
    bool LoadAndPlayback(const std::string& filepath);

    // === Data Access ===

    /**
     * @brief Get current location
     */
    [[nodiscard]] LocationData GetCurrentLocation() const;

    /**
     * @brief Get location history
     */
    [[nodiscard]] std::vector<LocationHistoryPoint> GetHistory() const;

    /**
     * @brief Get world position
     */
    [[nodiscard]] glm::vec3 GetWorldPosition() const;

    /**
     * @brief Get service status string
     */
    [[nodiscard]] std::string GetServiceStatus() const;

    // === Configuration ===

    /**
     * @brief Get configuration
     */
    LocationDebugConfig& GetConfig() { return m_config; }

    /**
     * @brief Set map center coordinate
     */
    void SetMapCenter(const LocationCoordinate& center);

    /**
     * @brief Set map zoom level
     */
    void SetMapZoom(float zoom);

    // === Export for HTML View ===

    /**
     * @brief Get JSON data for HTML view
     */
    [[nodiscard]] std::string GetLocationDataJSON() const;

    /**
     * @brief Process command from HTML view
     */
    void ProcessHTMLCommand(const std::string& command, const std::string& data);

private:
    LocationDebugPanel() = default;
    ~LocationDebugPanel() = default;

    void OnLocationUpdate(const LocationData& location);
    void RenderLocationInfo();
    void RenderMapView();
    void RenderControls();
    void RenderServiceStatus();
    void RenderRecordingControls();
    void RenderSimulatorControls();

    bool m_initialized = false;
    bool m_visible = false;
    bool m_simulatorEnabled = false;
    bool m_recording = false;

    LocationDebugConfig m_config;

    // Current location data
    LocationData m_currentLocation;
    glm::vec3 m_worldPosition;

    // Location history
    std::deque<LocationHistoryPoint> m_history;
    std::vector<LocationHistoryPoint> m_recordedPath;
    mutable std::mutex m_historyMutex;

    // Map view
    LocationCoordinate m_mapCenter;
    float m_mapZoom = 15.0f;

    // Mock location input
    float m_mockLatitude = 0.0f;
    float m_mockLongitude = 0.0f;

    // Route simulation
    std::vector<LocationCoordinate> m_simulatedRoute;
    size_t m_routeIndex = 0;
    float m_routeSpeed = 1.4f; // Walking speed m/s

    // UI state
    int m_selectedAccuracy = 1; // 0=Best, 1=Good, 2=Low
    bool m_backgroundUpdates = false;
};

} // namespace Editor
} // namespace Vehement
