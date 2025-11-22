#include "WorldView.hpp"
#include "Editor.hpp"
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace Vehement {

WorldView::WorldView(Editor* editor) : m_editor(editor) {
}

WorldView::~WorldView() = default;

void WorldView::Update(float deltaTime) {
    if (!m_viewportFocused) return;

    // Update camera position based on mode
    if (m_cameraMode == CameraMode::Orbit) {
        float x = m_cameraDistance * cos(glm::radians(m_cameraPitch)) * sin(glm::radians(m_cameraYaw));
        float y = m_cameraDistance * sin(glm::radians(m_cameraPitch));
        float z = m_cameraDistance * cos(glm::radians(m_cameraPitch)) * cos(glm::radians(m_cameraYaw));
        m_cameraPosition = m_cameraTarget + glm::vec3(x, y, z);
    }
}

void WorldView::Render() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (!ImGui::Begin("World View")) {
        ImGui::PopStyleVar();
        ImGui::End();
        return;
    }
    ImGui::PopStyleVar();

    RenderToolbar();

    // Get viewport dimensions
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    m_viewportSize = {viewportPanelSize.x, viewportPanelSize.y};

    ImVec2 viewportPos = ImGui::GetCursorScreenPos();
    m_viewportPos = {viewportPos.x, viewportPos.y};

    // Render viewport area
    RenderViewport();

    // Handle input
    m_viewportHovered = ImGui::IsWindowHovered();
    m_viewportFocused = ImGui::IsWindowFocused();
    if (m_viewportHovered) {
        HandleInput();
    }

    // Render overlay info
    RenderOverlay();

    ImGui::End();
}

void WorldView::RenderToolbar() {
    // Camera mode buttons
    if (ImGui::RadioButton("Orbit", m_cameraMode == CameraMode::Orbit)) m_cameraMode = CameraMode::Orbit;
    ImGui::SameLine();
    if (ImGui::RadioButton("Pan", m_cameraMode == CameraMode::Pan)) m_cameraMode = CameraMode::Pan;
    ImGui::SameLine();
    if (ImGui::RadioButton("Fly", m_cameraMode == CameraMode::Fly)) m_cameraMode = CameraMode::Fly;

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Layer toggles
    ImGui::Checkbox("Grid", &m_showGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Colliders", &m_showColliders);
    ImGui::SameLine();
    ImGui::Checkbox("Zones", &m_showZones);
    ImGui::SameLine();
    ImGui::Checkbox("Fog", &m_showFogOfWar);

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Go to location
    if (ImGui::Button("Go To...")) {
        ImGui::OpenPopup("GoToLocation");
    }

    // Go To popup
    if (ImGui::BeginPopup("GoToLocation")) {
        ImGui::Text("Game Coordinates:");
        ImGui::SetNextItemWidth(80);
        ImGui::InputText("X", m_gotoXBuffer, sizeof(m_gotoXBuffer));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputText("Y", m_gotoYBuffer, sizeof(m_gotoYBuffer));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputText("Z", m_gotoZBuffer, sizeof(m_gotoZBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Go##xyz")) {
            float x = std::stof(m_gotoXBuffer);
            float y = std::stof(m_gotoYBuffer);
            float z = std::stof(m_gotoZBuffer);
            GoToLocation(x, y, z);
            ImGui::CloseCurrentPopup();
        }

        ImGui::Separator();
        ImGui::Text("Geographic Coordinates:");
        ImGui::SetNextItemWidth(120);
        ImGui::InputText("Lat", m_gotoLatBuffer, sizeof(m_gotoLatBuffer));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        ImGui::InputText("Lon", m_gotoLonBuffer, sizeof(m_gotoLonBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Go##geo")) {
            double lat = std::stod(m_gotoLatBuffer);
            double lon = std::stod(m_gotoLonBuffer);
            GoToGeoLocation(lat, lon);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void WorldView::RenderViewport() {
    // Background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        ImVec2(m_viewportPos.x, m_viewportPos.y),
        ImVec2(m_viewportPos.x + m_viewportSize.x, m_viewportPos.y + m_viewportSize.y),
        IM_COL32(30, 30, 35, 255)
    );

    // TODO: Render actual 3D scene to framebuffer and display
    // For now, draw placeholder grid
    if (m_showGrid) {
        float gridSpacing = 50.0f;
        ImU32 gridColor = IM_COL32(50, 50, 55, 255);

        for (float x = 0; x < m_viewportSize.x; x += gridSpacing) {
            drawList->AddLine(
                ImVec2(m_viewportPos.x + x, m_viewportPos.y),
                ImVec2(m_viewportPos.x + x, m_viewportPos.y + m_viewportSize.y),
                gridColor
            );
        }
        for (float y = 0; y < m_viewportSize.y; y += gridSpacing) {
            drawList->AddLine(
                ImVec2(m_viewportPos.x, m_viewportPos.y + y),
                ImVec2(m_viewportPos.x + m_viewportSize.x, m_viewportPos.y + y),
                gridColor
            );
        }
    }

    // Center indicator
    float cx = m_viewportPos.x + m_viewportSize.x * 0.5f;
    float cy = m_viewportPos.y + m_viewportSize.y * 0.5f;
    drawList->AddCircle(ImVec2(cx, cy), 5.0f, IM_COL32(100, 150, 255, 200), 12, 2.0f);

    RenderGizmos();
}

void WorldView::RenderOverlay() {
    // Position overlay in top-left of viewport
    ImGui::SetCursorPos(ImVec2(10, 50));

    ImGui::BeginChild("ViewportOverlay", ImVec2(200, 100), false,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);

    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Camera");
    ImGui::Text("Pos: %.1f, %.1f, %.1f", m_cameraPosition.x, m_cameraPosition.y, m_cameraPosition.z);
    ImGui::Text("Target: %.1f, %.1f, %.1f", m_cameraTarget.x, m_cameraTarget.y, m_cameraTarget.z);
    ImGui::Text("Distance: %.1f", m_cameraDistance);

    ImGui::EndChild();

    // FPS counter in bottom-right
    ImGui::SetCursorPos(ImVec2(m_viewportSize.x - 80, m_viewportSize.y - 20));
    ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
}

void WorldView::RenderGizmos() {
    // TODO: Render transform gizmos for selected entities
}

void WorldView::HandleInput() {
    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 mousePos(io.MousePos.x, io.MousePos.y);
    glm::vec2 mouseDelta = mousePos - m_lastMousePos;
    m_lastMousePos = mousePos;

    // Mouse wheel zoom
    if (io.MouseWheel != 0) {
        m_cameraDistance -= io.MouseWheel * m_cameraZoomSpeed;
        m_cameraDistance = glm::clamp(m_cameraDistance, 5.0f, 500.0f);
    }

    // Right mouse button - rotate
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        m_cameraYaw += mouseDelta.x * m_cameraRotateSpeed;
        m_cameraPitch -= mouseDelta.y * m_cameraRotateSpeed;
        m_cameraPitch = glm::clamp(m_cameraPitch, -89.0f, 89.0f);
    }

    // Middle mouse button - pan
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        glm::vec3 right = glm::normalize(glm::cross(
            glm::vec3(0, 1, 0),
            m_cameraTarget - m_cameraPosition
        ));
        glm::vec3 up(0, 1, 0);

        m_cameraTarget -= right * mouseDelta.x * 0.1f;
        m_cameraTarget += up * mouseDelta.y * 0.1f;
    }

    // Left click - select
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        glm::vec2 relativePos = mousePos - m_viewportPos;
        SelectEntityAt(relativePos.x, relativePos.y);
    }

    // Keyboard controls in fly mode
    if (m_cameraMode == CameraMode::Fly && m_viewportFocused) {
        float speed = m_cameraMoveSpeed * ImGui::GetIO().DeltaTime;
        glm::vec3 forward = glm::normalize(m_cameraTarget - m_cameraPosition);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

        if (ImGui::IsKeyDown(ImGuiKey_W)) {
            m_cameraPosition += forward * speed;
            m_cameraTarget += forward * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S)) {
            m_cameraPosition -= forward * speed;
            m_cameraTarget -= forward * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_A)) {
            m_cameraPosition -= right * speed;
            m_cameraTarget -= right * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_D)) {
            m_cameraPosition += right * speed;
            m_cameraTarget += right * speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q)) {
            m_cameraPosition.y -= speed;
            m_cameraTarget.y -= speed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_E)) {
            m_cameraPosition.y += speed;
            m_cameraTarget.y += speed;
        }
    }
}

void WorldView::SetCameraPosition(const glm::vec3& position) {
    m_cameraPosition = position;
}

void WorldView::SetCameraTarget(const glm::vec3& target) {
    m_cameraTarget = target;
    m_cameraDistance = glm::distance(m_cameraPosition, m_cameraTarget);
}

void WorldView::GoToLocation(float x, float y, float z) {
    m_cameraTarget = glm::vec3(x, y, z);
}

void WorldView::GoToGeoLocation(double lat, double lon) {
    // TODO: Convert geo coordinates to world coordinates
    // For now, use simple conversion
    float x = static_cast<float>(lon * 1000.0);
    float z = static_cast<float>(lat * 1000.0);
    GoToLocation(x, 0, z);
}

void WorldView::FocusOnSelection() {
    // TODO: Focus camera on selected entities
}

void WorldView::SetLayerVisible(const std::string& layer, bool visible) {
    if (layer == "terrain") m_showTerrain = visible;
    else if (layer == "buildings") m_showBuildings = visible;
    else if (layer == "units") m_showUnits = visible;
    else if (layer == "fog") m_showFogOfWar = visible;
    else if (layer == "grid") m_showGrid = visible;
    else if (layer == "zones") m_showZones = visible;
    else if (layer == "paths") m_showPaths = visible;
    else if (layer == "colliders") m_showColliders = visible;
}

bool WorldView::IsLayerVisible(const std::string& layer) const {
    if (layer == "terrain") return m_showTerrain;
    if (layer == "buildings") return m_showBuildings;
    if (layer == "units") return m_showUnits;
    if (layer == "fog") return m_showFogOfWar;
    if (layer == "grid") return m_showGrid;
    if (layer == "zones") return m_showZones;
    if (layer == "paths") return m_showPaths;
    if (layer == "colliders") return m_showColliders;
    return false;
}

void WorldView::SelectEntityAt(float screenX, float screenY) {
    // TODO: Raycast to find entity
    glm::vec3 worldPos = ScreenToWorld(screenX, screenY);
    if (OnLocationClicked) OnLocationClicked(worldPos);
}

void WorldView::ClearSelection() {
    // TODO: Clear selection
}

glm::vec3 WorldView::ScreenToWorld(float screenX, float screenY) {
    // Simplified screen to world conversion
    // TODO: Proper unprojection
    float nx = (screenX / m_viewportSize.x) * 2.0f - 1.0f;
    float ny = 1.0f - (screenY / m_viewportSize.y) * 2.0f;

    return m_cameraTarget + glm::vec3(nx * 50.0f, 0, ny * 50.0f);
}

glm::vec2 WorldView::WorldToScreen(const glm::vec3& worldPos) {
    // TODO: Proper projection
    return {0, 0};
}

} // namespace Vehement
