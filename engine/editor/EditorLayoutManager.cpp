/**
 * @file EditorLayoutManager.cpp
 * @brief Implementation of panel layout management
 */

#include "EditorLayoutManager.hpp"
#include "../ui/EditorPanel.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>
#include <cstring>

namespace Nova {

// =============================================================================
// Initialization
// =============================================================================

void EditorLayoutManager::Initialize(const std::filesystem::path& configDir) {
    m_configDir = configDir;

    // Create config directory if it doesn't exist
    if (!m_configDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(m_configDir, ec);
        if (ec) {
            spdlog::warn("Failed to create layout config directory: {}", ec.message());
        }
    }

    // Load existing layouts
    LoadLayouts();

    // Create default layouts if none exist
    if (m_layouts.empty()) {
        CreateDefaultLayouts();
    }

    spdlog::debug("EditorLayoutManager initialized with config dir: {}", m_configDir.string());
}

void EditorLayoutManager::Shutdown() {
    // Save current layout state
    if (!m_currentLayout.empty()) {
        // Update current layout with latest state
        auto it = m_layouts.find(m_currentLayout);
        if (it != m_layouts.end()) {
            it->second.iniData = CaptureCurrentLayout();
        }
    }

    // Save all layouts to disk
    SaveLayouts();

    m_layouts.clear();
    m_panelStates.clear();
    m_onLayoutChanged = nullptr;

    spdlog::debug("EditorLayoutManager shutdown");
}

// =============================================================================
// Layout Management
// =============================================================================

bool EditorLayoutManager::SaveLayout(const std::string& name, const std::string& description) {
    if (name.empty()) {
        spdlog::warn("Cannot save layout with empty name");
        return false;
    }

    LayoutPreset preset;
    preset.name = name;
    preset.description = description;
    preset.iniData = CaptureCurrentLayout();
    preset.isBuiltIn = false;

    m_layouts[name] = preset;
    m_currentLayout = name;

    // Save to disk
    SaveLayouts();

    spdlog::info("Saved layout: {}", name);
    return true;
}

bool EditorLayoutManager::LoadLayout(const std::string& name) {
    auto it = m_layouts.find(name);
    if (it == m_layouts.end()) {
        spdlog::warn("Layout not found: {}", name);
        return false;
    }

    std::string previous = m_currentLayout;

    ApplyLayout(it->second.iniData);
    m_currentLayout = name;

    NotifyLayoutChanged(previous);

    spdlog::info("Loaded layout: {}", name);
    return true;
}

bool EditorLayoutManager::DeleteLayout(const std::string& name) {
    auto it = m_layouts.find(name);
    if (it == m_layouts.end()) {
        return false;
    }

    // Cannot delete built-in layouts
    if (it->second.isBuiltIn) {
        spdlog::warn("Cannot delete built-in layout: {}", name);
        return false;
    }

    m_layouts.erase(it);

    // Delete file if exists
    auto path = GetLayoutFilePath(name);
    if (std::filesystem::exists(path)) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    // If we deleted the current layout, reset to default
    if (m_currentLayout == name) {
        LoadLayout(m_defaultLayout);
    }

    spdlog::info("Deleted layout: {}", name);
    return true;
}

bool EditorLayoutManager::RenameLayout(const std::string& oldName, const std::string& newName) {
    if (oldName == newName) {
        return true;
    }

    auto it = m_layouts.find(oldName);
    if (it == m_layouts.end()) {
        return false;
    }

    // Cannot rename built-in layouts
    if (it->second.isBuiltIn) {
        spdlog::warn("Cannot rename built-in layout: {}", oldName);
        return false;
    }

    // Check if new name already exists
    if (m_layouts.count(newName) > 0) {
        spdlog::warn("Layout already exists: {}", newName);
        return false;
    }

    // Move the layout
    LayoutPreset preset = std::move(it->second);
    preset.name = newName;
    m_layouts.erase(it);
    m_layouts[newName] = std::move(preset);

    // Delete old file and save new
    auto oldPath = GetLayoutFilePath(oldName);
    if (std::filesystem::exists(oldPath)) {
        std::error_code ec;
        std::filesystem::remove(oldPath, ec);
    }

    SaveLayouts();

    // Update current layout name if it was renamed
    if (m_currentLayout == oldName) {
        m_currentLayout = newName;
    }

    spdlog::info("Renamed layout: {} -> {}", oldName, newName);
    return true;
}

bool EditorLayoutManager::HasLayout(const std::string& name) const {
    return m_layouts.count(name) > 0;
}

const LayoutPreset* EditorLayoutManager::GetLayout(const std::string& name) const {
    auto it = m_layouts.find(name);
    return it != m_layouts.end() ? &it->second : nullptr;
}

std::vector<std::string> EditorLayoutManager::GetLayoutNames() const {
    std::vector<std::string> names;
    names.reserve(m_layouts.size());
    for (const auto& [name, layout] : m_layouts) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

// =============================================================================
// Default Layout
// =============================================================================

void EditorLayoutManager::ResetLayout() {
    if (m_layouts.count(m_defaultLayout) > 0) {
        LoadLayout(m_defaultLayout);
    } else if (!m_layouts.empty()) {
        LoadLayout(m_layouts.begin()->first);
    }
}

void EditorLayoutManager::SetDefaultLayout(const std::string& name) {
    if (m_layouts.count(name) > 0) {
        m_defaultLayout = name;
        m_layouts[name].isDefault = true;

        // Clear default flag on other layouts
        for (auto& [layoutName, preset] : m_layouts) {
            if (layoutName != name) {
                preset.isDefault = false;
            }
        }
    }
}

void EditorLayoutManager::CreateDefaultLayouts() {
    // Create a basic default layout
    LayoutPreset defaultPreset;
    defaultPreset.name = "Default";
    defaultPreset.description = "Default editor layout";
    defaultPreset.isBuiltIn = true;
    defaultPreset.isDefault = true;
    defaultPreset.iniData = "";  // Empty - will use ImGui's auto-layout

    m_layouts["Default"] = defaultPreset;

    // Create debug layout
    LayoutPreset debugPreset;
    debugPreset.name = "Debug";
    debugPreset.description = "Layout optimized for debugging with expanded console";
    debugPreset.isBuiltIn = true;
    debugPreset.iniData = "";

    m_layouts["Debug"] = debugPreset;

    // Create animation layout
    LayoutPreset animPreset;
    animPreset.name = "Animation";
    animPreset.description = "Layout optimized for animation editing";
    animPreset.isBuiltIn = true;
    animPreset.iniData = "";

    m_layouts["Animation"] = animPreset;

    m_defaultLayout = "Default";
}

// =============================================================================
// Panel State
// =============================================================================

void EditorLayoutManager::SavePanelState(const EditorPanel* panel) {
    if (!panel) {
        return;
    }

    PanelState state;
    state.name = panel->GetTitle();
    state.visible = panel->IsVisible();
    // Note: Other properties would need to be queried from ImGui
    // which requires more complex integration

    m_panelStates[state.name] = state;
}

bool EditorLayoutManager::RestorePanelState(EditorPanel* panel) {
    if (!panel) {
        return false;
    }

    auto it = m_panelStates.find(panel->GetTitle());
    if (it == m_panelStates.end()) {
        return false;
    }

    panel->SetVisible(it->second.visible);
    return true;
}

const PanelState* EditorLayoutManager::GetPanelState(const std::string& panelName) const {
    auto it = m_panelStates.find(panelName);
    return it != m_panelStates.end() ? &it->second : nullptr;
}

void EditorLayoutManager::ClearPanelStates() {
    m_panelStates.clear();
}

// =============================================================================
// Persistence
// =============================================================================

bool EditorLayoutManager::LoadLayouts() {
    if (m_configDir.empty()) {
        return false;
    }

    // Look for layout files in config directory
    std::error_code ec;
    if (!std::filesystem::exists(m_configDir, ec)) {
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(m_configDir, ec)) {
        if (entry.path().extension() == ".layout") {
            std::ifstream file(entry.path());
            if (file.is_open()) {
                LayoutPreset preset;
                preset.name = entry.path().stem().string();

                // Read entire file as INI data
                std::stringstream buffer;
                buffer << file.rdbuf();
                preset.iniData = buffer.str();

                m_layouts[preset.name] = preset;
            }
        }
    }

    return true;
}

bool EditorLayoutManager::SaveLayouts() {
    if (m_configDir.empty()) {
        return false;
    }

    // Create directory if needed
    std::error_code ec;
    std::filesystem::create_directories(m_configDir, ec);

    for (const auto& [name, preset] : m_layouts) {
        // Skip built-in layouts with empty data
        if (preset.isBuiltIn && preset.iniData.empty()) {
            continue;
        }

        auto path = GetLayoutFilePath(name);
        std::ofstream file(path);
        if (file.is_open()) {
            file << preset.iniData;
        }
    }

    return true;
}

bool EditorLayoutManager::ExportLayout(const std::string& name, const std::filesystem::path& path) {
    auto it = m_layouts.find(name);
    if (it == m_layouts.end()) {
        return false;
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        spdlog::error("Failed to open file for layout export: {}", path.string());
        return false;
    }

    // Write layout metadata as comments
    file << "# Nova3D Layout Export\n";
    file << "# Name: " << it->second.name << "\n";
    if (!it->second.description.empty()) {
        file << "# Description: " << it->second.description << "\n";
    }
    file << "\n";
    file << it->second.iniData;

    spdlog::info("Exported layout '{}' to: {}", name, path.string());
    return true;
}

bool EditorLayoutManager::ImportLayout(const std::filesystem::path& path, const std::string& name) {
    if (!std::filesystem::exists(path)) {
        spdlog::error("Layout file not found: {}", path.string());
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::error("Failed to open layout file: {}", path.string());
        return false;
    }

    LayoutPreset preset;
    preset.name = name.empty() ? path.stem().string() : name;
    preset.isBuiltIn = false;

    // Read file content (skip comment lines for description)
    std::stringstream buffer;
    std::string line;
    while (std::getline(file, line)) {
        if (line.starts_with("# Description: ")) {
            preset.description = line.substr(15);
        } else if (!line.starts_with("#")) {
            buffer << line << "\n";
        }
    }
    preset.iniData = buffer.str();

    // Check for name conflicts
    std::string finalName = preset.name;
    int suffix = 1;
    while (m_layouts.count(finalName) > 0) {
        finalName = preset.name + "_" + std::to_string(++suffix);
    }
    preset.name = finalName;

    m_layouts[finalName] = preset;
    SaveLayouts();

    spdlog::info("Imported layout '{}' from: {}", finalName, path.string());
    return true;
}

// =============================================================================
// Docking
// =============================================================================

void EditorLayoutManager::BeginDefaultDocking() {
    m_needsDefaultDocking = true;
}

void EditorLayoutManager::EndDefaultDocking() {
    m_needsDefaultDocking = false;
}

// =============================================================================
// Callbacks
// =============================================================================

void EditorLayoutManager::SetOnLayoutChanged(std::function<void(const LayoutChangedEvent&)> callback) {
    m_onLayoutChanged = std::move(callback);
}

// =============================================================================
// Rendering
// =============================================================================

void EditorLayoutManager::RenderLayoutSelector() {
    if (ImGui::BeginMenu("Layout")) {
        // Default layout option
        if (ImGui::MenuItem("Reset to Default")) {
            ResetLayout();
        }

        ImGui::Separator();

        // List all layouts
        auto names = GetLayoutNames();
        for (const auto& name : names) {
            bool isCurrent = (name == m_currentLayout);
            if (ImGui::MenuItem(name.c_str(), nullptr, isCurrent)) {
                if (!isCurrent) {
                    LoadLayout(name);
                }
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save Current Layout...")) {
            m_showLayoutManager = true;
            std::strncpy(m_newLayoutName, "", sizeof(m_newLayoutName));
        }

        if (ImGui::MenuItem("Manage Layouts...")) {
            m_showLayoutManager = true;
        }

        ImGui::EndMenu();
    }
}

void EditorLayoutManager::RenderLayoutManager() {
    if (!m_showLayoutManager) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Layout Manager", &m_showLayoutManager)) {
        // Save new layout section
        ImGui::Text("Save Current Layout");
        ImGui::Separator();

        ImGui::InputText("Name##NewLayout", m_newLayoutName, sizeof(m_newLayoutName));
        ImGui::InputText("Description##NewLayout", m_layoutDescription, sizeof(m_layoutDescription));

        if (ImGui::Button("Save Layout")) {
            if (std::strlen(m_newLayoutName) > 0) {
                SaveLayout(m_newLayoutName, m_layoutDescription);
                std::strncpy(m_newLayoutName, "", sizeof(m_newLayoutName));
                std::strncpy(m_layoutDescription, "", sizeof(m_layoutDescription));
            }
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // Existing layouts section
        ImGui::Text("Existing Layouts");
        ImGui::Separator();

        auto names = GetLayoutNames();
        for (const auto& name : names) {
            ImGui::PushID(name.c_str());

            const auto* preset = GetLayout(name);
            bool isCurrent = (name == m_currentLayout);
            bool isDefault = (name == m_defaultLayout);

            // Layout name with indicators
            std::string displayName = name;
            if (isCurrent) displayName += " [Current]";
            if (isDefault) displayName += " [Default]";

            ImGui::Text("%s", displayName.c_str());

            if (preset && !preset->description.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", preset->description.c_str());
                }
            }

            ImGui::SameLine(ImGui::GetWindowWidth() - 150);

            // Load button
            if (ImGui::SmallButton("Load") && !isCurrent) {
                LoadLayout(name);
            }

            ImGui::SameLine();

            // Set as default button
            if (ImGui::SmallButton("Default") && !isDefault) {
                SetDefaultLayout(name);
            }

            ImGui::SameLine();

            // Delete button (disabled for built-in)
            bool canDelete = preset && !preset->isBuiltIn;
            if (!canDelete) {
                ImGui::BeginDisabled();
            }
            if (ImGui::SmallButton("Delete")) {
                DeleteLayout(name);
            }
            if (!canDelete) {
                ImGui::EndDisabled();
            }

            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::Separator();

        // Import/Export section
        if (ImGui::Button("Import Layout...")) {
            // TODO: Show file dialog
        }

        ImGui::SameLine();

        if (ImGui::Button("Export Current...")) {
            // TODO: Show save file dialog
        }
    }
    ImGui::End();
}

// =============================================================================
// Internal Helpers
// =============================================================================

std::string EditorLayoutManager::CaptureCurrentLayout() {
    return ImGui::SaveIniSettingsToMemory();
}

void EditorLayoutManager::ApplyLayout(const std::string& iniData) {
    if (!iniData.empty()) {
        ImGui::LoadIniSettingsFromMemory(iniData.c_str(), iniData.size());
    }
}

std::filesystem::path EditorLayoutManager::GetLayoutFilePath(const std::string& name) const {
    return m_configDir / (name + ".layout");
}

void EditorLayoutManager::NotifyLayoutChanged(const std::string& previous) {
    if (m_onLayoutChanged) {
        LayoutChangedEvent event;
        event.previousLayout = previous;
        event.newLayout = m_currentLayout;
        m_onLayoutChanged(event);
    }
}

} // namespace Nova
