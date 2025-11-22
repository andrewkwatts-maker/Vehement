#include "ConfigEditor.hpp"
#include "Editor.hpp"
#include "../config/ConfigRegistry.hpp"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

namespace Vehement {

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
            // TODO: Open file externally
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
            // TODO: File browser
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

    ImGui::Text("Raw JSON Editor");
    ImGui::SameLine(ImGui::GetWindowWidth() - 100);
    if (ImGui::Button("Format")) {
        // TODO: Format JSON
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply")) {
        SaveEditorToConfig();
    }

    ImGui::InputTextMultiline("##json", jsonBuffer, sizeof(jsonBuffer),
        ImVec2(-1, -1), ImGuiInputTextFlags_AllowTabInput);
}

void ConfigEditor::RenderModelPreview() {
    ImGui::Text("3D Model Preview");

    ImGui::Checkbox("Show Collision", &m_showCollisionShapes);
    ImGui::SameLine();
    ImGui::SliderFloat("Zoom", &m_previewZoom, 0.5f, 3.0f);

    // Preview area
    ImVec2 previewSize = ImVec2(ImGui::GetContentRegionAvail().x, 300);
    ImGui::BeginChild("ModelPreview", previewSize, true);

    // TODO: Render 3D preview using offscreen framebuffer
    ImGui::TextDisabled("3D Preview Placeholder");
    ImGui::Text("Rotation: %.1f degrees", m_previewRotation);

    ImGui::EndChild();

    // Controls
    if (ImGui::Button("Reset View")) {
        m_previewRotation = 0.0f;
        m_previewZoom = 1.0f;
    }
}

void ConfigEditor::RenderCollisionPreview() {
    ImGui::Text("Collision Shape Preview");

    // Wireframe preview of collision shapes
    ImVec2 previewSize = ImVec2(ImGui::GetContentRegionAvail().x, 300);
    ImGui::BeginChild("CollisionPreview", previewSize, true);

    // TODO: Render collision shapes
    ImGui::TextDisabled("Collision Preview Placeholder");

    ImGui::EndChild();
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
            // TODO: File browser
        }
        ImGui::SameLine();
        if (ImGui::Button("Edit")) {
            // TODO: Open script editor
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", script.description);
        ImGui::PopID();
    }
}

void ConfigEditor::SelectConfig(const std::string& configId) {
    m_selectedConfigId = configId;
    LoadConfigIntoEditor(configId);
    if (OnConfigSelected) OnConfigSelected(configId);
}

void ConfigEditor::CreateNewConfig(const std::string& type) {
    // TODO: Create new config file
    RefreshConfigList();
}

void ConfigEditor::DuplicateConfig(const std::string& configId) {
    // TODO: Duplicate config
    RefreshConfigList();
}

void ConfigEditor::DeleteConfig(const std::string& configId) {
    // TODO: Delete config
    if (m_selectedConfigId == configId) {
        m_selectedConfigId.clear();
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

    // TODO: Validate config against schema
    // For now, just add placeholder validation
    m_validationWarnings.push_back("Validation not fully implemented");
}

void ConfigEditor::RefreshConfigList() {
    m_unitConfigs.clear();
    m_buildingConfigs.clear();
    m_tileConfigs.clear();

    // TODO: Load from ConfigRegistry
    // For now, add placeholders
    m_unitConfigs = {"unit_soldier", "unit_worker", "unit_zombie"};
    m_buildingConfigs = {"building_house", "building_barracks", "building_wall"};
    m_tileConfigs = {"tile_grass", "tile_road", "tile_water", "tile_forest"};
}

void ConfigEditor::LoadConfigIntoEditor(const std::string& configId) {
    // TODO: Load actual config
    m_jsonBuffer = "{\n  \"id\": \"" + configId + "\"\n}";
    m_jsonModified = false;
}

void ConfigEditor::SaveEditorToConfig() {
    // TODO: Parse and save
    m_jsonModified = false;
}

} // namespace Vehement
