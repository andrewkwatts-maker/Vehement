#include "WorldView.hpp"
#include "Editor.hpp"
#include "TileInspector.hpp"
#include "Inspector.hpp"
#include "../entities/EntityManager.hpp"
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

namespace Vehement {

// Gizmo mode for transform operations
enum class GizmoMode { Translate, Rotate, Scale };
static GizmoMode s_gizmoMode = GizmoMode::Translate;
static uint64_t s_selectedEntity = 0;

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
    // Background - gradient sky
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Sky gradient (dark blue to lighter blue)
    ImU32 skyTop = IM_COL32(20, 30, 60, 255);
    ImU32 skyBottom = IM_COL32(80, 120, 180, 255);
    drawList->AddRectFilledMultiColor(
        ImVec2(m_viewportPos.x, m_viewportPos.y),
        ImVec2(m_viewportPos.x + m_viewportSize.x, m_viewportPos.y + m_viewportSize.y * 0.6f),
        skyTop, skyTop, skyBottom, skyBottom
    );

    // Ground plane
    ImU32 groundColor = IM_COL32(40, 55, 35, 255);
    drawList->AddRectFilled(
        ImVec2(m_viewportPos.x, m_viewportPos.y + m_viewportSize.y * 0.6f),
        ImVec2(m_viewportPos.x + m_viewportSize.x, m_viewportPos.y + m_viewportSize.y),
        groundColor
    );

    // Build view and projection matrices for 3D to 2D conversion
    float aspectRatio = m_viewportSize.x / m_viewportSize.y;
    glm::mat4 projection = glm::perspective(glm::radians(m_cameraFov), aspectRatio, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(m_cameraPosition, m_cameraTarget, glm::vec3(0, 1, 0));
    glm::mat4 viewProj = projection * view;

    // Draw grid on ground plane (in 3D projected to 2D)
    if (m_showGrid) {
        float gridSize = 100.0f;
        float gridSpacing = 5.0f;
        ImU32 gridColor = IM_COL32(60, 70, 50, 180);
        ImU32 gridColorMajor = IM_COL32(80, 90, 70, 200);

        for (float x = -gridSize; x <= gridSize; x += gridSpacing) {
            glm::vec3 start(x, 0, -gridSize);
            glm::vec3 end(x, 0, gridSize);
            glm::vec2 screenStart = WorldToScreen(start);
            glm::vec2 screenEnd = WorldToScreen(end);

            if (screenStart.x >= 0 && screenEnd.x >= 0) {
                ImU32 color = (static_cast<int>(x) % 10 == 0) ? gridColorMajor : gridColor;
                drawList->AddLine(
                    ImVec2(m_viewportPos.x + screenStart.x, m_viewportPos.y + screenStart.y),
                    ImVec2(m_viewportPos.x + screenEnd.x, m_viewportPos.y + screenEnd.y),
                    color
                );
            }
        }

        for (float z = -gridSize; z <= gridSize; z += gridSpacing) {
            glm::vec3 start(-gridSize, 0, z);
            glm::vec3 end(gridSize, 0, z);
            glm::vec2 screenStart = WorldToScreen(start);
            glm::vec2 screenEnd = WorldToScreen(end);

            if (screenStart.x >= 0 && screenEnd.x >= 0) {
                ImU32 color = (static_cast<int>(z) % 10 == 0) ? gridColorMajor : gridColor;
                drawList->AddLine(
                    ImVec2(m_viewportPos.x + screenStart.x, m_viewportPos.y + screenStart.y),
                    ImVec2(m_viewportPos.x + screenEnd.x, m_viewportPos.y + screenEnd.y),
                    color
                );
            }
        }
    }

    // Draw coordinate axes at origin
    glm::vec2 origin = WorldToScreen(glm::vec3(0, 0, 0));
    glm::vec2 xAxis = WorldToScreen(glm::vec3(5, 0, 0));
    glm::vec2 yAxis = WorldToScreen(glm::vec3(0, 5, 0));
    glm::vec2 zAxis = WorldToScreen(glm::vec3(0, 0, 5));

    if (origin.x >= 0) {
        // X axis - red
        drawList->AddLine(
            ImVec2(m_viewportPos.x + origin.x, m_viewportPos.y + origin.y),
            ImVec2(m_viewportPos.x + xAxis.x, m_viewportPos.y + xAxis.y),
            IM_COL32(255, 80, 80, 255), 2.0f
        );
        // Y axis - green
        drawList->AddLine(
            ImVec2(m_viewportPos.x + origin.x, m_viewportPos.y + origin.y),
            ImVec2(m_viewportPos.x + yAxis.x, m_viewportPos.y + yAxis.y),
            IM_COL32(80, 255, 80, 255), 2.0f
        );
        // Z axis - blue
        drawList->AddLine(
            ImVec2(m_viewportPos.x + origin.x, m_viewportPos.y + origin.y),
            ImVec2(m_viewportPos.x + zAxis.x, m_viewportPos.y + zAxis.y),
            IM_COL32(80, 80, 255, 255), 2.0f
        );
    }

    // Draw entities if we have an entity manager
    if (m_editor && m_editor->GetEntityManager()) {
        auto* entityMgr = m_editor->GetEntityManager();
        entityMgr->ForEachEntity([&](Entity& entity) {
            glm::vec3 pos = entity.GetPosition();
            glm::vec2 screenPos = WorldToScreen(pos);

            if (screenPos.x >= 0 && screenPos.x < m_viewportSize.x &&
                screenPos.y >= 0 && screenPos.y < m_viewportSize.y) {

                // Draw entity marker
                ImU32 entityColor = IM_COL32(255, 200, 100, 255);
                if (entity.GetId() == s_selectedEntity) {
                    entityColor = IM_COL32(100, 200, 255, 255);
                }

                float markerSize = 8.0f;
                drawList->AddCircleFilled(
                    ImVec2(m_viewportPos.x + screenPos.x, m_viewportPos.y + screenPos.y),
                    markerSize, entityColor
                );

                // Draw entity name
                std::string name = entity.GetName();
                if (name.empty()) name = "Entity_" + std::to_string(entity.GetId());
                drawList->AddText(
                    ImVec2(m_viewportPos.x + screenPos.x + 10, m_viewportPos.y + screenPos.y - 5),
                    IM_COL32(255, 255, 255, 200),
                    name.c_str()
                );
            }
        });
    }

    // Camera target indicator
    glm::vec2 targetScreen = WorldToScreen(m_cameraTarget);
    if (targetScreen.x >= 0) {
        drawList->AddCircle(
            ImVec2(m_viewportPos.x + targetScreen.x, m_viewportPos.y + targetScreen.y),
            5.0f, IM_COL32(100, 150, 255, 200), 12, 2.0f
        );
    }

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
    // Render transform gizmos for selected entity
    if (s_selectedEntity == 0 || !m_editor || !m_editor->GetEntityManager()) {
        return;
    }

    auto* entity = m_editor->GetEntityManager()->GetEntity(s_selectedEntity);
    if (!entity) return;

    glm::vec3 entityPos = entity->GetPosition();
    glm::vec2 screenPos = WorldToScreen(entityPos);

    if (screenPos.x < 0 || screenPos.x > m_viewportSize.x ||
        screenPos.y < 0 || screenPos.y > m_viewportSize.y) {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float gizmoSize = 60.0f;

    ImVec2 center(m_viewportPos.x + screenPos.x, m_viewportPos.y + screenPos.y);

    // Gizmo mode selector at top of viewport
    ImGui::SetCursorPos(ImVec2(m_viewportSize.x - 180, 50));
    ImGui::BeginChild("GizmoSelector", ImVec2(170, 30), false);
    if (ImGui::RadioButton("Move", s_gizmoMode == GizmoMode::Translate)) s_gizmoMode = GizmoMode::Translate;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rot", s_gizmoMode == GizmoMode::Rotate)) s_gizmoMode = GizmoMode::Rotate;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", s_gizmoMode == GizmoMode::Scale)) s_gizmoMode = GizmoMode::Scale;
    ImGui::EndChild();

    switch (s_gizmoMode) {
        case GizmoMode::Translate: {
            // Translation gizmo - 3 arrows
            // X axis (red)
            drawList->AddLine(center, ImVec2(center.x + gizmoSize, center.y), IM_COL32(255, 80, 80, 255), 3.0f);
            drawList->AddTriangleFilled(
                ImVec2(center.x + gizmoSize, center.y - 6),
                ImVec2(center.x + gizmoSize, center.y + 6),
                ImVec2(center.x + gizmoSize + 12, center.y),
                IM_COL32(255, 80, 80, 255)
            );

            // Y axis (green) - pointing up
            drawList->AddLine(center, ImVec2(center.x, center.y - gizmoSize), IM_COL32(80, 255, 80, 255), 3.0f);
            drawList->AddTriangleFilled(
                ImVec2(center.x - 6, center.y - gizmoSize),
                ImVec2(center.x + 6, center.y - gizmoSize),
                ImVec2(center.x, center.y - gizmoSize - 12),
                IM_COL32(80, 255, 80, 255)
            );

            // Z axis (blue) - pointing diagonally down-right
            drawList->AddLine(center, ImVec2(center.x + gizmoSize * 0.6f, center.y + gizmoSize * 0.6f), IM_COL32(80, 80, 255, 255), 3.0f);
            drawList->AddTriangleFilled(
                ImVec2(center.x + gizmoSize * 0.6f - 4, center.y + gizmoSize * 0.6f + 4),
                ImVec2(center.x + gizmoSize * 0.6f + 4, center.y + gizmoSize * 0.6f - 4),
                ImVec2(center.x + gizmoSize * 0.7f + 4, center.y + gizmoSize * 0.7f + 4),
                IM_COL32(80, 80, 255, 255)
            );
            break;
        }
        case GizmoMode::Rotate: {
            // Rotation gizmo - 3 circles
            float radius = gizmoSize * 0.8f;

            // X rotation (red) - YZ plane
            drawList->AddCircle(center, radius, IM_COL32(255, 80, 80, 200), 32, 2.0f);

            // Y rotation (green) - XZ plane (slightly offset)
            drawList->AddEllipse(center, ImVec2(radius, radius * 0.3f), IM_COL32(80, 255, 80, 200), 0.0f, 32, 2.0f);

            // Z rotation (blue) - XY plane
            drawList->AddEllipse(center, ImVec2(radius * 0.3f, radius), IM_COL32(80, 80, 255, 200), 0.0f, 32, 2.0f);
            break;
        }
        case GizmoMode::Scale: {
            // Scale gizmo - 3 lines with boxes
            float boxSize = 6.0f;

            // X axis (red)
            drawList->AddLine(center, ImVec2(center.x + gizmoSize, center.y), IM_COL32(255, 80, 80, 255), 2.0f);
            drawList->AddRectFilled(
                ImVec2(center.x + gizmoSize - boxSize, center.y - boxSize),
                ImVec2(center.x + gizmoSize + boxSize, center.y + boxSize),
                IM_COL32(255, 80, 80, 255)
            );

            // Y axis (green)
            drawList->AddLine(center, ImVec2(center.x, center.y - gizmoSize), IM_COL32(80, 255, 80, 255), 2.0f);
            drawList->AddRectFilled(
                ImVec2(center.x - boxSize, center.y - gizmoSize - boxSize),
                ImVec2(center.x + boxSize, center.y - gizmoSize + boxSize),
                IM_COL32(80, 255, 80, 255)
            );

            // Z axis (blue)
            drawList->AddLine(center, ImVec2(center.x + gizmoSize * 0.6f, center.y + gizmoSize * 0.6f), IM_COL32(80, 80, 255, 255), 2.0f);
            drawList->AddRectFilled(
                ImVec2(center.x + gizmoSize * 0.6f - boxSize, center.y + gizmoSize * 0.6f - boxSize),
                ImVec2(center.x + gizmoSize * 0.6f + boxSize, center.y + gizmoSize * 0.6f + boxSize),
                IM_COL32(80, 80, 255, 255)
            );

            // Center cube for uniform scale
            drawList->AddRectFilled(
                ImVec2(center.x - boxSize, center.y - boxSize),
                ImVec2(center.x + boxSize, center.y + boxSize),
                IM_COL32(255, 255, 255, 200)
            );
            break;
        }
    }
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
    // Convert geographic coordinates to world coordinates
    // Using reference point: lat 37.7749, lon -122.4194 as origin
    double refLat = 37.7749;
    double refLon = -122.4194;

    // Approximate meters per degree at this latitude
    double metersPerDegLat = 111111.0;
    double metersPerDegLon = 111111.0 * cos(glm::radians(refLat));

    // Calculate offset from reference point in meters
    float x = static_cast<float>((lon - refLon) * metersPerDegLon);
    float z = static_cast<float>((lat - refLat) * metersPerDegLat);

    GoToLocation(x, 0, z);
}

void WorldView::FocusOnSelection() {
    // Focus camera on selected entity
    if (s_selectedEntity == 0 || !m_editor || !m_editor->GetEntityManager()) {
        return;
    }

    auto* entity = m_editor->GetEntityManager()->GetEntity(s_selectedEntity);
    if (!entity) return;

    // Set camera target to entity position
    glm::vec3 entityPos = entity->GetPosition();
    m_cameraTarget = entityPos;

    // Adjust camera distance based on entity size (default to medium distance)
    m_cameraDistance = 30.0f;

    // Position camera to look at entity from a nice angle
    float x = m_cameraDistance * cos(glm::radians(m_cameraPitch)) * sin(glm::radians(m_cameraYaw));
    float y = m_cameraDistance * sin(glm::radians(m_cameraPitch));
    float z = m_cameraDistance * cos(glm::radians(m_cameraPitch)) * cos(glm::radians(m_cameraYaw));
    m_cameraPosition = m_cameraTarget + glm::vec3(x, y, z);
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
    // Raycast to find entity at screen position
    glm::vec3 worldPos = ScreenToWorld(screenX, screenY);

    // Find entity near the clicked world position
    if (m_editor && m_editor->GetEntityManager()) {
        auto* entityMgr = m_editor->GetEntityManager();

        // Find closest entity to click position
        Entity* closestEntity = nullptr;
        float closestDist = 10.0f;  // Max click distance in world units

        entityMgr->ForEachEntity([&](Entity& entity) {
            glm::vec3 entityPos = entity.GetPosition();
            glm::vec2 entityScreen = WorldToScreen(entityPos);

            // Check distance in screen space
            float screenDist = glm::length(glm::vec2(screenX, screenY) - entityScreen);
            if (screenDist < 20.0f) {  // 20 pixel radius
                float worldDist = glm::length(entityPos - worldPos);
                if (worldDist < closestDist) {
                    closestDist = worldDist;
                    closestEntity = &entity;
                }
            }
        });

        if (closestEntity) {
            s_selectedEntity = closestEntity->GetId();

            // Notify inspector
            if (auto* inspector = m_editor->GetInspector()) {
                inspector->SetSelectedEntity(s_selectedEntity);
            }

            // Notify callback
            if (OnEntitySelected) {
                OnEntitySelected(s_selectedEntity);
            }
        } else {
            // Clicked on empty space - select tile instead
            if (auto* tileInspector = m_editor->GetTileInspector()) {
                int tileX = static_cast<int>(std::floor(worldPos.x));
                int tileY = static_cast<int>(std::floor(worldPos.y));
                int tileZ = static_cast<int>(std::floor(worldPos.z));
                tileInspector->SetSelectedTile(tileX, tileY, tileZ);
            }
        }
    }

    if (OnLocationClicked) OnLocationClicked(worldPos);
}

void WorldView::ClearSelection() {
    // Clear entity selection
    s_selectedEntity = 0;

    // Clear inspector selection
    if (m_editor) {
        if (auto* inspector = m_editor->GetInspector()) {
            inspector->ClearSelection();
        }
        if (auto* tileInspector = m_editor->GetTileInspector()) {
            tileInspector->ClearSelection();
        }
    }
}

glm::vec3 WorldView::ScreenToWorld(float screenX, float screenY) {
    // Proper unprojection from screen to world coordinates
    // Normalize screen coordinates to [-1, 1]
    float nx = (screenX / m_viewportSize.x) * 2.0f - 1.0f;
    float ny = 1.0f - (screenY / m_viewportSize.y) * 2.0f;

    // Build inverse view-projection matrix
    float aspectRatio = m_viewportSize.x / m_viewportSize.y;
    glm::mat4 projection = glm::perspective(glm::radians(m_cameraFov), aspectRatio, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(m_cameraPosition, m_cameraTarget, glm::vec3(0, 1, 0));
    glm::mat4 invViewProj = glm::inverse(projection * view);

    // Create ray from near plane to far plane
    glm::vec4 nearPoint = invViewProj * glm::vec4(nx, ny, -1.0f, 1.0f);
    glm::vec4 farPoint = invViewProj * glm::vec4(nx, ny, 1.0f, 1.0f);

    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    // Create ray direction
    glm::vec3 rayOrigin(nearPoint);
    glm::vec3 rayDir = glm::normalize(glm::vec3(farPoint) - rayOrigin);

    // Intersect with ground plane (y = 0)
    if (std::abs(rayDir.y) > 0.0001f) {
        float t = -rayOrigin.y / rayDir.y;
        if (t > 0) {
            return rayOrigin + rayDir * t;
        }
    }

    // Fallback: project to a plane at camera target height
    float t = (m_cameraTarget.y - rayOrigin.y) / rayDir.y;
    return rayOrigin + rayDir * std::max(t, 0.0f);
}

glm::vec2 WorldView::WorldToScreen(const glm::vec3& worldPos) {
    // Proper projection from world to screen coordinates
    float aspectRatio = m_viewportSize.x / m_viewportSize.y;
    glm::mat4 projection = glm::perspective(glm::radians(m_cameraFov), aspectRatio, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(m_cameraPosition, m_cameraTarget, glm::vec3(0, 1, 0));

    // Project world position
    glm::vec4 clipPos = projection * view * glm::vec4(worldPos, 1.0f);

    // Check if behind camera
    if (clipPos.w <= 0.0f) {
        return {-1, -1};  // Invalid - behind camera
    }

    // Perspective divide
    glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;

    // Convert from NDC [-1, 1] to screen coordinates [0, viewport]
    float screenX = (ndcPos.x * 0.5f + 0.5f) * m_viewportSize.x;
    float screenY = (1.0f - (ndcPos.y * 0.5f + 0.5f)) * m_viewportSize.y;

    return {screenX, screenY};
}

} // namespace Vehement
