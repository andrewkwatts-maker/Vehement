#pragma once

#include <glm/glm.hpp>
#include <string>
#include <functional>

namespace Vehement {

class Editor;

/**
 * @brief 3D World viewport panel
 *
 * Renders the game world with editor controls:
 * - Camera orbit, pan, zoom, fly modes
 * - Coordinate display (game coords + lat/lon)
 * - Go to location functionality
 * - Layer toggles
 * - Entity selection and gizmos
 */
class WorldView {
public:
    enum class CameraMode {
        Orbit,
        Pan,
        Fly
    };

    explicit WorldView(Editor* editor);
    ~WorldView();

    void Update(float deltaTime);
    void Render();

    // Camera
    void SetCameraPosition(const glm::vec3& position);
    void SetCameraTarget(const glm::vec3& target);
    void SetCameraMode(CameraMode mode) { m_cameraMode = mode; }
    glm::vec3 GetCameraPosition() const { return m_cameraPosition; }
    glm::vec3 GetCameraTarget() const { return m_cameraTarget; }

    // Navigation
    void GoToLocation(float x, float y, float z);
    void GoToGeoLocation(double lat, double lon);
    void FocusOnSelection();

    // Layers
    void SetLayerVisible(const std::string& layer, bool visible);
    bool IsLayerVisible(const std::string& layer) const;

    // Selection
    void SelectEntityAt(float screenX, float screenY);
    void ClearSelection();

    // Callbacks
    std::function<void(uint64_t)> OnEntitySelected;
    std::function<void(const glm::vec3&)> OnLocationClicked;

private:
    void HandleInput();
    void RenderToolbar();
    void RenderViewport();
    void RenderOverlay();
    void RenderGizmos();

    glm::vec3 ScreenToWorld(float screenX, float screenY);
    glm::vec2 WorldToScreen(const glm::vec3& worldPos);

    Editor* m_editor = nullptr;

    // Camera state
    CameraMode m_cameraMode = CameraMode::Orbit;
    glm::vec3 m_cameraPosition{0, 50, 50};
    glm::vec3 m_cameraTarget{0, 0, 0};
    float m_cameraDistance = 50.0f;
    float m_cameraYaw = -45.0f;
    float m_cameraPitch = 45.0f;
    float m_cameraFov = 60.0f;
    float m_cameraMoveSpeed = 20.0f;
    float m_cameraRotateSpeed = 0.3f;
    float m_cameraZoomSpeed = 5.0f;

    // Viewport state
    glm::vec2 m_viewportPos{0, 0};
    glm::vec2 m_viewportSize{800, 600};
    bool m_viewportHovered = false;
    bool m_viewportFocused = false;

    // Layer visibility
    bool m_showTerrain = true;
    bool m_showBuildings = true;
    bool m_showUnits = true;
    bool m_showFogOfWar = false;
    bool m_showGrid = true;
    bool m_showZones = true;
    bool m_showPaths = false;
    bool m_showColliders = false;

    // Input state
    bool m_isDragging = false;
    glm::vec2 m_lastMousePos{0, 0};

    // Go to location
    char m_gotoXBuffer[32] = "0";
    char m_gotoYBuffer[32] = "0";
    char m_gotoZBuffer[32] = "0";
    char m_gotoLatBuffer[32] = "0";
    char m_gotoLonBuffer[32] = "0";
};

} // namespace Vehement
