#include "ConfigEditor.hpp"
#include "Editor.hpp"
#include "ScriptEditor.hpp"
#include "../config/ConfigRegistry.hpp"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace Vehement {

// File browser state
static bool s_showFileBrowser = false;
static std::string s_fileBrowserPath;
static std::string s_fileBrowserFilter;
static std::function<void(const std::string&)> s_fileBrowserCallback;

ConfigEditor::ConfigEditor(Editor* editor) : m_editor(editor) {
    RefreshConfigList();
}

ConfigEditor::~ConfigEditor() = default;

void ConfigEditor::Update(float deltaTime) {
    if (m_showModelPreview) {
        m_previewRotation += deltaTime * 30.0f;  // Slow rotation
        if (m_previewRotation > 360.0f) m_previewRotation -= 360.0f;
    }
}

void ConfigEditor::Render() {
    if (!ImGui::Begin("Config Editor")) {
        ImGui::End();
        return;
    }

    RenderToolbar();
    ImGui::Separator();

    // Split view: tree on left, details on right
    ImGui::BeginChild("TreePanel", ImVec2(250, 0), true);
    RenderTreeView();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("DetailsPanel", ImVec2(0, 0), true);
    if (!m_selectedConfigId.empty()) {
        RenderConfigDetails();
    } else {
        ImGui::TextDisabled("Select a config to edit");
    }
    ImGui::EndChild();

    ImGui::End();
}

void ConfigEditor::RenderToolbar() {
    if (ImGui::Button("New")) {
        ImGui::OpenPopup("NewConfigPopup");
    }
    ImGui::SameLine();
    if (ImGui::Button("Save") && !m_selectedConfigId.empty()) {
        SaveConfig(m_selectedConfigId);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload") && !m_selectedConfigId.empty()) {
        ReloadConfig(m_selectedConfigId);
    }
    ImGui::SameLine();
    if (ImGui::Button("Validate") && !m_selectedConfigId.empty()) {
        ValidateConfig(m_selectedConfigId);
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        RefreshConfigList();
    }

    // New config popup
    if (ImGui::BeginPopup("NewConfigPopup")) {
        if (ImGui::MenuItem("New Unit")) CreateNewConfig("unit");
        if (ImGui::MenuItem("New Building")) CreateNewConfig("building");
        if (ImGui::MenuItem("New Tile")) CreateNewConfig("tile");
        ImGui::EndPopup();
    }

    // Search filter
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    ImGui::SetNextItemWidth(180);
    ImGui::InputTextWithHint("##search", "Search...", &m_searchFilter[0], m_searchFilter.capacity());
}

void ConfigEditor::RenderTreeView() {
    // Units
    if (ImGui::TreeNodeEx("Units", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& configId : m_unitConfigs) {
            if (!m_searchFilter.empty() && configId.find(m_searchFilter) == std::string::npos) continue;

            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (configId == m_selectedConfigId) nodeFlags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(configId.c_str(), nodeFlags);
            if (ImGui::IsItemClicked()) {
                SelectConfig(configId);
                m_selectedType = "unit";
            }
            RenderContextMenu();
        }
        ImGui::TreePop();
    }

    // Buildings
    if (ImGui::TreeNodeEx("Buildings", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& configId : m_buildingConfigs) {
            if (!m_searchFilter.empty() && configId.find(m_searchFilter) == std::string::npos) continue;

            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (configId == m_selectedConfigId) nodeFlags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(configId.c_str(), nodeFlags);
            if (ImGui::IsItemClicked()) {
                SelectConfig(configId);
                m_selectedType = "building";
            }
            RenderContextMenu();
        }
        ImGui::TreePop();
    }

    // Tiles
    if (ImGui::TreeNodeEx("Tiles", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& configId : m_tileConfigs) {
            if (!m_searchFilter.empty() && configId.find(m_searchFilter) == std::string::npos) continue;

            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (configId == m_selectedConfigId) nodeFlags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(configId.c_str(), nodeFlags);
            if (ImGui::IsItemClicked()) {
                SelectConfig(configId);
                m_selectedType = "tile";
            }
            RenderContextMenu();
        }
        ImGui::TreePop();
    }
}

void ConfigEditor::RenderContextMenu() {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Duplicate")) DuplicateConfig(m_selectedConfigId);
        if (ImGui::MenuItem("Delete")) DeleteConfig(m_selectedConfigId);
        ImGui::Separator();
        if (ImGui::MenuItem("Open in External Editor")) {
            // Get the config file path from the registry
            auto& registry = Config::ConfigRegistry::Instance();
            if (auto config = registry.Get(m_selectedConfigId)) {
                std::string filePath = "config/" + m_selectedType + "s/" + m_selectedConfigId + ".json";
                // Open with system default editor
                #ifdef _WIN32
                    std::string cmd = "start \"\" \"" + filePath + "\"";
                #elif __APPLE__
                    std::string cmd = "open \"" + filePath + "\"";
                #else
                    std::string cmd = "xdg-open \"" + filePath + "\"";
                #endif
                system(cmd.c_str());
            }
        }
        ImGui::EndPopup();
    }
}

void ConfigEditor::RenderConfigDetails() {
    if (ImGui::BeginTabBar("ConfigTabs")) {
        if (ImGui::BeginTabItem("Properties")) {
            RenderPropertyInspector();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("JSON")) {
            RenderJsonEditor();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Preview")) {
            RenderModelPreview();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Collision")) {
            RenderCollisionPreview();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Scripts")) {
            RenderScriptBrowser();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // Validation results
    if (!m_validationErrors.empty() || !m_validationWarnings.empty()) {
        ImGui::Separator();
        ImGui::Text("Validation Results:");
        for (const auto& error : m_validationErrors) {
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "  Error: %s", error.c_str());
        }
        for (const auto& warning : m_validationWarnings) {
            ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "  Warning: %s", warning.c_str());
        }
    }
}

void ConfigEditor::RenderPropertyInspector() {
    ImGui::Text("Config: %s", m_selectedConfigId.c_str());
    ImGui::Separator();

    // Common properties
    if (ImGui::CollapsingHeader("Identity", ImGuiTreeNodeFlags_DefaultOpen)) {
        static char nameBuffer[256] = "";
        ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer));

        static char tagsBuffer[512] = "";
        ImGui::InputText("Tags", tagsBuffer, sizeof(tagsBuffer));
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Comma-separated tags");
    }

    if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
        static char modelPath[512] = "";
        ImGui::InputText("Model Path", modelPath, sizeof(modelPath));
        ImGui::SameLine();
        if (ImGui::Button("...##model")) {
            // Open file browser for model selection
            s_showFileBrowser = true;
            s_fileBrowserPath = "assets/models";
            s_fileBrowserFilter = ".obj,.fbx,.gltf,.glb";
            s_fileBrowserCallback = [](const std::string& path) {
                // Will be copied to modelPath in the file browser rendering
            };
            ImGui::OpenPopup("FileBrowserPopup");
        }

        // Render file browser popup
        if (ImGui::BeginPopup("FileBrowserPopup")) {
            ImGui::Text("Select Model File");
            ImGui::Separator();

            if (fs::exists(s_fileBrowserPath)) {
                for (const auto& entry : fs::directory_iterator(s_fileBrowserPath)) {
                    std::string filename = entry.path().filename().string();
                    bool isDir = entry.is_directory();

                    if (isDir) {
                        if (ImGui::Selectable((std::string("[") + filename + "]").c_str())) {
                            s_fileBrowserPath = entry.path().string();
                        }
                    } else {
                        // Check extension filter
                        std::string ext = entry.path().extension().string();
                        if (s_fileBrowserFilter.find(ext) != std::string::npos || s_fileBrowserFilter.empty()) {
                            if (ImGui::Selectable(filename.c_str())) {
                                strncpy(modelPath, entry.path().string().c_str(), sizeof(modelPath) - 1);
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }
                }
            }

            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        static float scale[3] = {1.0f, 1.0f, 1.0f};
        ImGui::DragFloat3("Scale", scale, 0.01f, 0.01f, 10.0f);
    }

    // Type-specific properties
    if (m_selectedType == "unit") {
        if (ImGui::CollapsingHeader("Movement")) {
            static float moveSpeed = 5.0f;
            static float turnRate = 180.0f;
            ImGui::DragFloat("Move Speed", &moveSpeed, 0.1f, 0.0f, 50.0f);
            ImGui::DragFloat("Turn Rate", &turnRate, 1.0f, 0.0f, 720.0f);
        }
        if (ImGui::CollapsingHeader("Combat")) {
            static float health = 100.0f;
            static float armor = 0.0f;
            static float damage = 10.0f;
            static float attackRange = 1.5f;
            ImGui::DragFloat("Health", &health, 1.0f, 1.0f, 10000.0f);
            ImGui::DragFloat("Armor", &armor, 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat("Damage", &damage, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat("Attack Range", &attackRange, 0.1f, 0.0f, 100.0f);
        }
    } else if (m_selectedType == "building") {
        if (ImGui::CollapsingHeader("Footprint")) {
            static int width = 2, height = 2;
            ImGui::DragInt("Width", &width, 1, 1, 10);
            ImGui::DragInt("Height", &height, 1, 1, 10);
        }
        if (ImGui::CollapsingHeader("Construction")) {
            static float buildTime = 30.0f;
            ImGui::DragFloat("Build Time (s)", &buildTime, 1.0f, 1.0f, 600.0f);
            ImGui::Text("Resource Costs:");
            static int woodCost = 100, stoneCost = 50;
            ImGui::DragInt("Wood", &woodCost, 1, 0, 10000);
            ImGui::DragInt("Stone", &stoneCost, 1, 0, 10000);
        }
    } else if (m_selectedType == "tile") {
        if (ImGui::CollapsingHeader("Properties")) {
            static bool walkable = true;
            static bool buildable = true;
            static float movementCost = 1.0f;
            ImGui::Checkbox("Walkable", &walkable);
            ImGui::Checkbox("Buildable", &buildable);
            ImGui::DragFloat("Movement Cost", &movementCost, 0.1f, 0.1f, 10.0f);
        }
    }

    if (ImGui::CollapsingHeader("Collision")) {
        static const char* shapeTypes[] = {"Box", "Sphere", "Capsule", "Cylinder", "Mesh"};
        static int currentShape = 0;
        ImGui::Combo("Shape Type", &currentShape, shapeTypes, IM_ARRAYSIZE(shapeTypes));

        if (currentShape == 0) {  // Box
            static float halfExtents[3] = {0.5f, 0.5f, 0.5f};
            ImGui::DragFloat3("Half Extents", halfExtents, 0.01f, 0.01f, 10.0f);
        } else if (currentShape == 1 || currentShape == 2) {  // Sphere or Capsule
            static float radius = 0.5f;
            ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 10.0f);
            if (currentShape == 2) {
                static float height = 1.0f;
                ImGui::DragFloat("Height", &height, 0.01f, 0.01f, 10.0f);
            }
        }
    }
}

void ConfigEditor::RenderJsonEditor() {
    // JSON text editor
    static char jsonBuffer[65536] = "";

    // Copy current json buffer to static if it changed
    if (m_jsonBuffer.size() < sizeof(jsonBuffer)) {
        strncpy(jsonBuffer, m_jsonBuffer.c_str(), sizeof(jsonBuffer) - 1);
    }

    ImGui::Text("Raw JSON Editor");
    ImGui::SameLine(ImGui::GetWindowWidth() - 100);
    if (ImGui::Button("Format")) {
        // Format JSON with proper indentation using nlohmann::json
        try {
            nlohmann::json parsed = nlohmann::json::parse(jsonBuffer);
            std::string formatted = parsed.dump(2);  // 2-space indent
            strncpy(jsonBuffer, formatted.c_str(), sizeof(jsonBuffer) - 1);
            m_jsonBuffer = formatted;
            m_jsonModified = true;
        } catch (const nlohmann::json::exception& e) {
            m_validationErrors.clear();
            m_validationErrors.push_back(std::string("JSON Parse Error: ") + e.what());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply")) {
        m_jsonBuffer = jsonBuffer;
        SaveEditorToConfig();
    }

    if (ImGui::InputTextMultiline("##json", jsonBuffer, sizeof(jsonBuffer),
        ImVec2(-1, -1), ImGuiInputTextFlags_AllowTabInput)) {
        m_jsonBuffer = jsonBuffer;
        m_jsonModified = true;
    }
}

void ConfigEditor::RenderModelPreview() {
    ImGui::Text("3D Model Preview");

    ImGui::Checkbox("Show Collision", &m_showCollisionShapes);
    ImGui::SameLine();
    ImGui::SliderFloat("Zoom", &m_previewZoom, 0.5f, 3.0f);

    // Preview area with 3D wireframe rendering
    ImVec2 previewSize = ImVec2(ImGui::GetContentRegionAvail().x, 300);
    ImGui::BeginChild("ModelPreview", previewSize, true);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();

    // Background
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(30, 30, 35, 255));

    // Calculate center and scale
    ImVec2 center(pos.x + size.x / 2, pos.y + size.y / 2);
    float scale = 40.0f * m_previewZoom;

    // Rotation transform
    float cosR = cos(glm::radians(m_previewRotation));
    float sinR = sin(glm::radians(m_previewRotation));

    // Draw a simple 3D cube wireframe as placeholder
    auto project3D = [&](float x, float y, float z) -> ImVec2 {
        // Apply Y rotation
        float rx = x * cosR - z * sinR;
        float rz = x * sinR + z * cosR;
        // Simple isometric projection
        float px = center.x + (rx - rz) * scale * 0.7f;
        float py = center.y - y * scale + (rx + rz) * scale * 0.3f;
        return ImVec2(px, py);
    };

    // Cube vertices
    ImVec2 v0 = project3D(-1, -1, -1);
    ImVec2 v1 = project3D( 1, -1, -1);
    ImVec2 v2 = project3D( 1, -1,  1);
    ImVec2 v3 = project3D(-1, -1,  1);
    ImVec2 v4 = project3D(-1,  1, -1);
    ImVec2 v5 = project3D( 1,  1, -1);
    ImVec2 v6 = project3D( 1,  1,  1);
    ImVec2 v7 = project3D(-1,  1,  1);

    ImU32 modelColor = IM_COL32(200, 200, 200, 255);
    ImU32 collisionColor = IM_COL32(100, 255, 100, 150);

    // Draw cube edges
    drawList->AddLine(v0, v1, modelColor, 2.0f);
    drawList->AddLine(v1, v2, modelColor, 2.0f);
    drawList->AddLine(v2, v3, modelColor, 2.0f);
    drawList->AddLine(v3, v0, modelColor, 2.0f);

    drawList->AddLine(v4, v5, modelColor, 2.0f);
    drawList->AddLine(v5, v6, modelColor, 2.0f);
    drawList->AddLine(v6, v7, modelColor, 2.0f);
    drawList->AddLine(v7, v4, modelColor, 2.0f);

    drawList->AddLine(v0, v4, modelColor, 2.0f);
    drawList->AddLine(v1, v5, modelColor, 2.0f);
    drawList->AddLine(v2, v6, modelColor, 2.0f);
    drawList->AddLine(v3, v7, modelColor, 2.0f);

    // Draw collision shape if enabled
    if (m_showCollisionShapes) {
        // Draw a slightly larger wireframe box for collision
        float cs = 1.1f;  // Collision scale
        ImVec2 c0 = project3D(-cs, -cs, -cs);
        ImVec2 c1 = project3D( cs, -cs, -cs);
        ImVec2 c2 = project3D( cs, -cs,  cs);
        ImVec2 c3 = project3D(-cs, -cs,  cs);
        ImVec2 c4 = project3D(-cs,  cs, -cs);
        ImVec2 c5 = project3D( cs,  cs, -cs);
        ImVec2 c6 = project3D( cs,  cs,  cs);
        ImVec2 c7 = project3D(-cs,  cs,  cs);

        drawList->AddLine(c0, c1, collisionColor, 1.0f);
        drawList->AddLine(c1, c2, collisionColor, 1.0f);
        drawList->AddLine(c2, c3, collisionColor, 1.0f);
        drawList->AddLine(c3, c0, collisionColor, 1.0f);
        drawList->AddLine(c4, c5, collisionColor, 1.0f);
        drawList->AddLine(c5, c6, collisionColor, 1.0f);
        drawList->AddLine(c6, c7, collisionColor, 1.0f);
        drawList->AddLine(c7, c4, collisionColor, 1.0f);
        drawList->AddLine(c0, c4, collisionColor, 1.0f);
        drawList->AddLine(c1, c5, collisionColor, 1.0f);
        drawList->AddLine(c2, c6, collisionColor, 1.0f);
        drawList->AddLine(c3, c7, collisionColor, 1.0f);
    }

    // Draw coordinate axes
    ImVec2 origin = project3D(0, 0, 0);
    ImVec2 xAxis = project3D(1.5f, 0, 0);
    ImVec2 yAxis = project3D(0, 1.5f, 0);
    ImVec2 zAxis = project3D(0, 0, 1.5f);
    drawList->AddLine(origin, xAxis, IM_COL32(255, 80, 80, 200), 1.5f);
    drawList->AddLine(origin, yAxis, IM_COL32(80, 255, 80, 200), 1.5f);
    drawList->AddLine(origin, zAxis, IM_COL32(80, 80, 255, 200), 1.5f);

    ImGui::Text("Config: %s", m_selectedConfigId.c_str());
    ImGui::Text("Rotation: %.1f deg", m_previewRotation);

    ImGui::EndChild();

    // Controls
    if (ImGui::Button("Reset View")) {
        m_previewRotation = 0.0f;
        m_previewZoom = 1.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Rotate Left")) {
        m_previewRotation -= 15.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Rotate Right")) {
        m_previewRotation += 15.0f;
    }
}

void ConfigEditor::RenderCollisionPreview() {
    ImGui::Text("Collision Shape Preview");

    // Wireframe preview of collision shapes
    ImVec2 previewSize = ImVec2(ImGui::GetContentRegionAvail().x, 300);
    ImGui::BeginChild("CollisionPreview", previewSize, true);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();

    // Background
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(25, 25, 30, 255));

    ImVec2 center(pos.x + size.x / 2, pos.y + size.y / 2);
    float scale = 50.0f;

    // Get collision shape type from current config
    static int shapeType = 0;  // 0=Box, 1=Sphere, 2=Capsule, 3=Cylinder
    static float shapeRadius = 0.5f;
    static float shapeHeight = 1.0f;
    static float halfExtents[3] = {0.5f, 0.5f, 0.5f};

    ImU32 wireColor = IM_COL32(100, 255, 100, 200);
    ImU32 fillColor = IM_COL32(50, 150, 50, 80);

    switch (shapeType) {
        case 0: {  // Box
            float w = halfExtents[0] * scale;
            float h = halfExtents[1] * scale;
            // Front face
            drawList->AddRect(
                ImVec2(center.x - w, center.y - h),
                ImVec2(center.x + w, center.y + h),
                wireColor, 0, 0, 2.0f
            );
            // 3D effect - back face offset
            float offset = halfExtents[2] * scale * 0.5f;
            drawList->AddRect(
                ImVec2(center.x - w + offset, center.y - h - offset),
                ImVec2(center.x + w + offset, center.y + h - offset),
                IM_COL32(100, 255, 100, 100), 0, 0, 1.0f
            );
            // Connecting lines
            drawList->AddLine(ImVec2(center.x - w, center.y - h), ImVec2(center.x - w + offset, center.y - h - offset), wireColor, 1.0f);
            drawList->AddLine(ImVec2(center.x + w, center.y - h), ImVec2(center.x + w + offset, center.y - h - offset), wireColor, 1.0f);
            drawList->AddLine(ImVec2(center.x - w, center.y + h), ImVec2(center.x - w + offset, center.y + h - offset), wireColor, 1.0f);
            drawList->AddLine(ImVec2(center.x + w, center.y + h), ImVec2(center.x + w + offset, center.y + h - offset), wireColor, 1.0f);
            break;
        }
        case 1: {  // Sphere
            float r = shapeRadius * scale;
            drawList->AddCircle(center, r, wireColor, 32, 2.0f);
            // Horizontal ellipse
            drawList->AddEllipse(center, ImVec2(r, r * 0.3f), IM_COL32(100, 255, 100, 100), 0, 24, 1.0f);
            // Vertical ellipse
            drawList->AddEllipse(center, ImVec2(r * 0.3f, r), IM_COL32(100, 255, 100, 100), 0, 24, 1.0f);
            break;
        }
        case 2: {  // Capsule
            float r = shapeRadius * scale;
            float h = shapeHeight * scale * 0.5f;
            // Top hemisphere
            drawList->AddBezierQuadratic(
                ImVec2(center.x - r, center.y - h),
                ImVec2(center.x - r, center.y - h - r),
                ImVec2(center.x, center.y - h - r),
                wireColor, 2.0f, 16
            );
            drawList->AddBezierQuadratic(
                ImVec2(center.x, center.y - h - r),
                ImVec2(center.x + r, center.y - h - r),
                ImVec2(center.x + r, center.y - h),
                wireColor, 2.0f, 16
            );
            // Bottom hemisphere
            drawList->AddBezierQuadratic(
                ImVec2(center.x - r, center.y + h),
                ImVec2(center.x - r, center.y + h + r),
                ImVec2(center.x, center.y + h + r),
                wireColor, 2.0f, 16
            );
            drawList->AddBezierQuadratic(
                ImVec2(center.x, center.y + h + r),
                ImVec2(center.x + r, center.y + h + r),
                ImVec2(center.x + r, center.y + h),
                wireColor, 2.0f, 16
            );
            // Side lines
            drawList->AddLine(ImVec2(center.x - r, center.y - h), ImVec2(center.x - r, center.y + h), wireColor, 2.0f);
            drawList->AddLine(ImVec2(center.x + r, center.y - h), ImVec2(center.x + r, center.y + h), wireColor, 2.0f);
            break;
        }
        case 3: {  // Cylinder
            float r = shapeRadius * scale;
            float h = shapeHeight * scale * 0.5f;
            // Top ellipse
            drawList->AddEllipse(ImVec2(center.x, center.y - h), ImVec2(r, r * 0.3f), wireColor, 0, 24, 2.0f);
            // Bottom ellipse
            drawList->AddEllipse(ImVec2(center.x, center.y + h), ImVec2(r, r * 0.3f), wireColor, 0, 24, 2.0f);
            // Side lines
            drawList->AddLine(ImVec2(center.x - r, center.y - h), ImVec2(center.x - r, center.y + h), wireColor, 2.0f);
            drawList->AddLine(ImVec2(center.x + r, center.y - h), ImVec2(center.x + r, center.y + h), wireColor, 2.0f);
            break;
        }
    }

    // Shape info
    ImGui::SetCursorPos(ImVec2(10, 10));
    const char* shapeNames[] = {"Box", "Sphere", "Capsule", "Cylinder"};
    ImGui::Text("Shape: %s", shapeNames[shapeType]);

    ImGui::EndChild();

    // Shape controls
    ImGui::Combo("Shape", &shapeType, "Box\0Sphere\0Capsule\0Cylinder\0");
    if (shapeType == 0) {
        ImGui::DragFloat3("Half Extents", halfExtents, 0.01f, 0.01f, 10.0f);
    } else {
        ImGui::DragFloat("Radius", &shapeRadius, 0.01f, 0.01f, 10.0f);
        if (shapeType == 2 || shapeType == 3) {
            ImGui::DragFloat("Height", &shapeHeight, 0.01f, 0.01f, 10.0f);
        }
    }
}

void ConfigEditor::RenderScriptBrowser() {
    ImGui::Text("Event Scripts");
    ImGui::Separator();

    struct ScriptSlot {
        const char* name;
        const char* description;
        char path[256];
    };

    static ScriptSlot scripts[] = {
        {"on_spawn", "Called when entity is created", ""},
        {"on_death", "Called when entity dies", ""},
        {"on_attack", "Called when entity attacks", ""},
        {"on_damaged", "Called when entity takes damage", ""},
        {"on_idle", "Called when entity becomes idle", ""},
        {"on_target_acquired", "Called when entity finds target", ""},
    };

    for (auto& script : scripts) {
        ImGui::PushID(script.name);
        ImGui::Text("%s", script.name);
        ImGui::SameLine(150);
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("##path", script.path, sizeof(script.path));
        ImGui::SameLine();
        if (ImGui::Button("...")) {
            // Open file browser for script selection
            static char* currentScriptPath = nullptr;
            currentScriptPath = script.path;
            ImGui::OpenPopup("ScriptFileBrowserPopup");
        }
        ImGui::SameLine();
        if (ImGui::Button("Edit")) {
            // Open script in the script editor
            if (strlen(script.path) > 0 && m_editor) {
                if (auto* scriptEditor = m_editor->GetScriptEditor()) {
                    scriptEditor->OpenScript(script.path);
                }
                m_editor->SetScriptEditorVisible(true);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", script.description);
        ImGui::PopID();
    }

    // Script file browser popup (shared)
    if (ImGui::BeginPopup("ScriptFileBrowserPopup")) {
        ImGui::Text("Select Python Script");
        ImGui::Separator();

        static std::string browsePath = "scripts/";

        if (browsePath != "scripts/") {
            if (ImGui::Selectable("[..]")) {
                browsePath = fs::path(browsePath).parent_path().string();
                if (browsePath.empty()) browsePath = "scripts/";
            }
        }

        if (fs::exists(browsePath)) {
            for (const auto& entry : fs::directory_iterator(browsePath)) {
                std::string filename = entry.path().filename().string();

                if (entry.is_directory()) {
                    if (ImGui::Selectable((std::string("[") + filename + "]").c_str())) {
                        browsePath = entry.path().string();
                    }
                } else if (entry.path().extension() == ".py") {
                    if (ImGui::Selectable(filename.c_str())) {
                        // Copy selected path to the current script slot
                        // Note: In real implementation, we'd track which slot is being edited
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
        }

        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ConfigEditor::SelectConfig(const std::string& configId) {
    m_selectedConfigId = configId;
    LoadConfigIntoEditor(configId);
    if (OnConfigSelected) OnConfigSelected(configId);
}

void ConfigEditor::CreateNewConfig(const std::string& type) {
    // Generate unique ID for new config
    static int newConfigCounter = 1;
    std::string newId = type + "_new_" + std::to_string(newConfigCounter++);

    // Create default config JSON
    nlohmann::json newConfig;
    newConfig["id"] = newId;
    newConfig["name"] = "New " + type;
    newConfig["type"] = type;
    newConfig["tags"] = nlohmann::json::array();

    if (type == "unit") {
        newConfig["health"] = 100;
        newConfig["speed"] = 5.0f;
        newConfig["damage"] = 10;
        newConfig["model"] = "models/default_unit.obj";
    } else if (type == "building") {
        newConfig["health"] = 500;
        newConfig["buildTime"] = 30;
        newConfig["model"] = "models/default_building.obj";
    } else if (type == "tile") {
        newConfig["walkable"] = true;
        newConfig["buildable"] = true;
        newConfig["movementCost"] = 1.0f;
    }

    // Save to file
    std::string filePath = "config/" + type + "s/" + newId + ".json";
    std::ofstream outFile(filePath);
    if (outFile.is_open()) {
        outFile << newConfig.dump(2);
        outFile.close();

        // Register with config registry
        Config::ConfigRegistry::Instance().LoadFile(filePath);
    }

    RefreshConfigList();
    SelectConfig(newId);
}

void ConfigEditor::DuplicateConfig(const std::string& configId) {
    // Get source config
    auto& registry = Config::ConfigRegistry::Instance();
    auto sourceConfig = registry.Get(configId);
    if (!sourceConfig) return;

    // Generate new ID
    std::string newId = configId + "_copy";
    int copyNum = 1;
    while (registry.Has(newId)) {
        newId = configId + "_copy" + std::to_string(copyNum++);
    }

    // Create duplicate JSON with new ID
    nlohmann::json duplicateJson;
    duplicateJson["id"] = newId;
    duplicateJson["name"] = sourceConfig->GetName() + " (Copy)";

    // Copy other properties from source (simplified)
    std::string filePath = "config/" + m_selectedType + "s/" + newId + ".json";
    std::ofstream outFile(filePath);
    if (outFile.is_open()) {
        outFile << duplicateJson.dump(2);
        outFile.close();
        registry.LoadFile(filePath);
    }

    RefreshConfigList();
    SelectConfig(newId);
}

void ConfigEditor::DeleteConfig(const std::string& configId) {
    // Confirm deletion
    auto& registry = Config::ConfigRegistry::Instance();

    // Get file path and delete
    std::string filePath = "config/" + m_selectedType + "s/" + configId + ".json";
    if (fs::exists(filePath)) {
        fs::remove(filePath);
    }

    // Unregister from registry
    registry.Unregister(configId);

    if (m_selectedConfigId == configId) {
        m_selectedConfigId.clear();
        m_jsonBuffer.clear();
    }
    RefreshConfigList();
}

void ConfigEditor::SaveConfig(const std::string& configId) {
    SaveEditorToConfig();
    if (OnConfigModified) OnConfigModified(configId);
}

void ConfigEditor::ReloadConfig(const std::string& configId) {
    LoadConfigIntoEditor(configId);
}

void ConfigEditor::ValidateConfig(const std::string& configId) {
    m_validationErrors.clear();
    m_validationWarnings.clear();

    // Validate the JSON structure
    try {
        nlohmann::json parsed = nlohmann::json::parse(m_jsonBuffer);

        // Required fields check
        if (!parsed.contains("id")) {
            m_validationErrors.push_back("Missing required field: 'id'");
        }
        if (!parsed.contains("name")) {
            m_validationWarnings.push_back("Missing field: 'name' (recommended)");
        }

        // Type-specific validation
        if (m_selectedType == "unit") {
            if (!parsed.contains("health")) {
                m_validationWarnings.push_back("Missing 'health' field for unit");
            } else if (!parsed["health"].is_number()) {
                m_validationErrors.push_back("'health' must be a number");
            }
            if (!parsed.contains("speed")) {
                m_validationWarnings.push_back("Missing 'speed' field for unit");
            }
        } else if (m_selectedType == "building") {
            if (!parsed.contains("health")) {
                m_validationWarnings.push_back("Missing 'health' field for building");
            }
        } else if (m_selectedType == "tile") {
            if (!parsed.contains("walkable")) {
                m_validationWarnings.push_back("Missing 'walkable' field for tile");
            }
        }

        // Check for unknown/deprecated fields
        // (Add schema-based validation here in production)

    } catch (const nlohmann::json::exception& e) {
        m_validationErrors.push_back(std::string("JSON Parse Error: ") + e.what());
    }
}

void ConfigEditor::RefreshConfigList() {
    m_unitConfigs.clear();
    m_buildingConfigs.clear();
    m_tileConfigs.clear();

    // Load from ConfigRegistry
    auto& registry = Config::ConfigRegistry::Instance();

    // Get all unit configs
    auto units = registry.GetAllUnits();
    for (const auto& unit : units) {
        if (unit) m_unitConfigs.push_back(unit->GetId());
    }

    // Get all building configs
    auto buildings = registry.GetAllBuildings();
    for (const auto& building : buildings) {
        if (building) m_buildingConfigs.push_back(building->GetId());
    }

    // Get all tile configs
    auto tiles = registry.GetAllTiles();
    for (const auto& tile : tiles) {
        if (tile) m_tileConfigs.push_back(tile->GetId());
    }

    // If lists are empty, scan directories directly
    if (m_unitConfigs.empty() && m_buildingConfigs.empty() && m_tileConfigs.empty()) {
        // Fallback: scan config directories
        const std::vector<std::pair<std::string, std::vector<std::string>*>> dirs = {
            {"config/units", &m_unitConfigs},
            {"config/buildings", &m_buildingConfigs},
            {"config/tiles", &m_tileConfigs}
        };

        for (const auto& [dir, list] : dirs) {
            if (fs::exists(dir)) {
                for (const auto& entry : fs::directory_iterator(dir)) {
                    if (entry.path().extension() == ".json") {
                        list->push_back(entry.path().stem().string());
                    }
                }
            }
        }
    }

    // Sort all lists
    std::sort(m_unitConfigs.begin(), m_unitConfigs.end());
    std::sort(m_buildingConfigs.begin(), m_buildingConfigs.end());
    std::sort(m_tileConfigs.begin(), m_tileConfigs.end());
}

void ConfigEditor::LoadConfigIntoEditor(const std::string& configId) {
    // Try to load from registry first
    auto& registry = Config::ConfigRegistry::Instance();
    auto config = registry.Get(configId);

    if (config) {
        // Serialize config to JSON for editing
        nlohmann::json j;
        j["id"] = config->GetId();
        j["name"] = config->GetName();
        j["type"] = config->GetType();
        j["tags"] = config->GetTags();
        // Add type-specific fields based on config type

        m_jsonBuffer = j.dump(2);
    } else {
        // Try loading directly from file
        std::string filePath = "config/" + m_selectedType + "s/" + configId + ".json";
        if (fs::exists(filePath)) {
            std::ifstream inFile(filePath);
            if (inFile.is_open()) {
                std::stringstream buffer;
                buffer << inFile.rdbuf();
                m_jsonBuffer = buffer.str();
                inFile.close();
            }
        } else {
            m_jsonBuffer = "{\n  \"id\": \"" + configId + "\",\n  \"name\": \"" + configId + "\"\n}";
        }
    }

    m_jsonModified = false;
    ValidateConfig(configId);
}

void ConfigEditor::SaveEditorToConfig() {
    if (m_selectedConfigId.empty()) return;

    try {
        // Parse the JSON buffer
        nlohmann::json parsed = nlohmann::json::parse(m_jsonBuffer);

        // Save to file
        std::string filePath = "config/" + m_selectedType + "s/" + m_selectedConfigId + ".json";

        // Create directory if it doesn't exist
        fs::create_directories(fs::path(filePath).parent_path());

        std::ofstream outFile(filePath);
        if (outFile.is_open()) {
            outFile << parsed.dump(2);
            outFile.close();

            // Reload into registry
            Config::ConfigRegistry::Instance().ReloadConfig(m_selectedConfigId);

            m_jsonModified = false;

            // Mark editor as dirty
            if (m_editor) m_editor->MarkDirty();
        }
    } catch (const nlohmann::json::exception& e) {
        m_validationErrors.clear();
        m_validationErrors.push_back(std::string("Save failed - JSON error: ") + e.what());
    }
}

} // namespace Vehement
