/**
 * @file EditorMenuSystem.cpp
 * @brief Implementation of menu bar rendering and shortcut management
 */

#include "EditorMenuSystem.hpp"
#include "EditorApplication.hpp"
#include "../core/Engine.hpp"
#include "../ui/EditorTheme.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cctype>
#include <fstream>

namespace Nova {

// =============================================================================
// Initialization
// =============================================================================

void EditorMenuSystem::Initialize(EditorApplication* editor) {
    m_editor = editor;

    // Load recent files from disk
    LoadRecentFiles();

    spdlog::debug("EditorMenuSystem initialized");
}

void EditorMenuSystem::Shutdown() {
    // Save recent files before shutdown
    SaveRecentFiles();

    m_shortcuts.clear();
    m_customMenus.clear();
    m_recentFiles.clear();
    m_editor = nullptr;

    spdlog::debug("EditorMenuSystem shutdown");
}

// =============================================================================
// Menu Registration
// =============================================================================

void EditorMenuSystem::RegisterMenu(const std::string& menuName, const std::vector<MenuItem>& items) {
    m_customMenus[menuName] = items;
}

void EditorMenuSystem::AddMenuItem(const std::string& menuName, const MenuItem& item,
                                    const std::string& insertBefore) {
    auto& menu = m_customMenus[menuName];

    if (insertBefore.empty()) {
        menu.push_back(item);
    } else {
        auto it = std::find_if(menu.begin(), menu.end(),
            [&insertBefore](const MenuItem& m) { return m.id == insertBefore; });
        if (it != menu.end()) {
            menu.insert(it, item);
        } else {
            menu.push_back(item);
        }
    }
}

void EditorMenuSystem::RemoveMenuItem(const std::string& menuName, const std::string& itemId) {
    auto menuIt = m_customMenus.find(menuName);
    if (menuIt == m_customMenus.end()) {
        return;
    }

    auto& menu = menuIt->second;
    menu.erase(std::remove_if(menu.begin(), menu.end(),
        [&itemId](const MenuItem& m) { return m.id == itemId; }), menu.end());
}

void EditorMenuSystem::SetMenuItemEnabled(const std::string& menuName, const std::string& itemId, bool enabled) {
    auto menuIt = m_customMenus.find(menuName);
    if (menuIt == m_customMenus.end()) {
        return;
    }

    for (auto& item : menuIt->second) {
        if (item.id == itemId) {
            item.enabled = enabled;
            break;
        }
    }
}

// =============================================================================
// Shortcut Management
// =============================================================================

void EditorMenuSystem::RegisterShortcut(const std::string& action, const std::string& shortcut,
                                         std::function<void()> handler, bool global) {
    ShortcutBinding binding;
    binding.action = action;
    binding.shortcutString = shortcut;
    binding.handler = std::move(handler);
    binding.global = global;

    if (ParseShortcut(shortcut, binding.key, binding.modifiers)) {
        m_shortcuts[action] = std::move(binding);
    } else {
        spdlog::warn("Failed to parse shortcut '{}' for action '{}'", shortcut, action);
    }
}

void EditorMenuSystem::UnregisterShortcut(const std::string& action) {
    m_shortcuts.erase(action);
}

bool EditorMenuSystem::RebindShortcut(const std::string& action, const std::string& newShortcut) {
    auto it = m_shortcuts.find(action);
    if (it == m_shortcuts.end()) {
        return false;
    }

    int key, modifiers;
    if (!ParseShortcut(newShortcut, key, modifiers)) {
        return false;
    }

    it->second.shortcutString = newShortcut;
    it->second.key = key;
    it->second.modifiers = modifiers;
    return true;
}

std::string EditorMenuSystem::GetShortcutForAction(const std::string& action) const {
    auto it = m_shortcuts.find(action);
    return it != m_shortcuts.end() ? it->second.shortcutString : "";
}

bool EditorMenuSystem::IsShortcutPressed(const std::string& action) const {
    auto it = m_shortcuts.find(action);
    if (it == m_shortcuts.end()) {
        return false;
    }
    return IsShortcutActive(it->second.key, it->second.modifiers);
}

void EditorMenuSystem::ProcessShortcuts() {
    // Don't process non-global shortcuts when typing in text input
    bool wantTextInput = ImGui::GetIO().WantTextInput;

    for (const auto& [action, binding] : m_shortcuts) {
        // Skip non-global shortcuts during text input
        if (wantTextInput && !binding.global) {
            continue;
        }

        if (IsShortcutActive(binding.key, binding.modifiers)) {
            if (binding.handler) {
                binding.handler();
            }
        }
    }

    // Handle global shortcuts like Escape
    HandleGlobalShortcuts();
}

bool EditorMenuSystem::ParseShortcut(const std::string& shortcut, int& key, int& modifiers) const {
    key = 0;
    modifiers = 0;

    // Parse shortcut string like "Ctrl+Shift+S"
    std::string upper = shortcut;
    for (auto& c : upper) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    // Check for modifiers
    if (upper.find("CTRL") != std::string::npos || upper.find("CONTROL") != std::string::npos) {
        modifiers |= 1;  // Ctrl flag
    }
    if (upper.find("SHIFT") != std::string::npos) {
        modifiers |= 2;  // Shift flag
    }
    if (upper.find("ALT") != std::string::npos) {
        modifiers |= 4;  // Alt flag
    }

    // Find the key (last component after '+')
    size_t plusPos = shortcut.rfind('+');
    std::string keyStr = (plusPos != std::string::npos) ? shortcut.substr(plusPos + 1) : shortcut;

    // Trim whitespace
    while (!keyStr.empty() && std::isspace(static_cast<unsigned char>(keyStr.front()))) {
        keyStr.erase(keyStr.begin());
    }
    while (!keyStr.empty() && std::isspace(static_cast<unsigned char>(keyStr.back()))) {
        keyStr.pop_back();
    }

    // Parse key
    if (keyStr.length() == 1) {
        key = static_cast<int>(std::toupper(static_cast<unsigned char>(keyStr[0])));
    } else {
        // Handle special keys
        std::string upperKey = keyStr;
        for (auto& c : upperKey) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }

        if (upperKey == "DELETE" || upperKey == "DEL") {
            key = 127;
        } else if (upperKey == "ESCAPE" || upperKey == "ESC") {
            key = 27;
        } else if (upperKey == "ENTER" || upperKey == "RETURN") {
            key = 13;
        } else if (upperKey == "SPACE") {
            key = 32;
        } else if (upperKey == "TAB") {
            key = 9;
        } else if (upperKey == "BACKSPACE") {
            key = 8;
        } else if (upperKey == "INSERT") {
            key = 155;
        } else if (upperKey == "HOME") {
            key = 156;
        } else if (upperKey == "END") {
            key = 157;
        } else if (upperKey == "PAGEUP") {
            key = 158;
        } else if (upperKey == "PAGEDOWN") {
            key = 159;
        } else if (upperKey == "UP") {
            key = 160;
        } else if (upperKey == "DOWN") {
            key = 161;
        } else if (upperKey == "LEFT") {
            key = 162;
        } else if (upperKey == "RIGHT") {
            key = 163;
        } else if (upperKey.length() >= 2 && upperKey[0] == 'F') {
            // Function keys F1-F12
            int fNum = std::atoi(upperKey.substr(1).c_str());
            if (fNum >= 1 && fNum <= 12) {
                key = 289 + fNum;  // F1 = 290, etc.
            }
        }
    }

    return key != 0;
}

bool EditorMenuSystem::IsShortcutActive(int key, int modifiers) const {
    ImGuiIO& io = ImGui::GetIO();

    // Check modifiers
    bool ctrlRequired = (modifiers & 1) != 0;
    bool shiftRequired = (modifiers & 2) != 0;
    bool altRequired = (modifiers & 4) != 0;

    if (ctrlRequired != io.KeyCtrl) return false;
    if (shiftRequired != io.KeyShift) return false;
    if (altRequired != io.KeyAlt) return false;

    // Check key press (map our key codes to ImGuiKey)
    ImGuiKey imguiKey = ImGuiKey_None;

    if (key >= 'A' && key <= 'Z') {
        imguiKey = static_cast<ImGuiKey>(ImGuiKey_A + (key - 'A'));
    } else if (key >= '0' && key <= '9') {
        imguiKey = static_cast<ImGuiKey>(ImGuiKey_0 + (key - '0'));
    } else {
        // Special keys
        switch (key) {
            case 127: imguiKey = ImGuiKey_Delete; break;
            case 27:  imguiKey = ImGuiKey_Escape; break;
            case 13:  imguiKey = ImGuiKey_Enter; break;
            case 32:  imguiKey = ImGuiKey_Space; break;
            case 9:   imguiKey = ImGuiKey_Tab; break;
            case 8:   imguiKey = ImGuiKey_Backspace; break;
            case 155: imguiKey = ImGuiKey_Insert; break;
            case 156: imguiKey = ImGuiKey_Home; break;
            case 157: imguiKey = ImGuiKey_End; break;
            case 158: imguiKey = ImGuiKey_PageUp; break;
            case 159: imguiKey = ImGuiKey_PageDown; break;
            case 160: imguiKey = ImGuiKey_UpArrow; break;
            case 161: imguiKey = ImGuiKey_DownArrow; break;
            case 162: imguiKey = ImGuiKey_LeftArrow; break;
            case 163: imguiKey = ImGuiKey_RightArrow; break;
            default:
                // Function keys
                if (key >= 290 && key <= 301) {
                    imguiKey = static_cast<ImGuiKey>(ImGuiKey_F1 + (key - 290));
                }
                break;
        }
    }

    return imguiKey != ImGuiKey_None && ImGui::IsKeyPressed(imguiKey);
}

void EditorMenuSystem::HandleGlobalShortcuts() {
    // Escape key handling is now part of ProcessShortcuts via registered shortcuts
}

// =============================================================================
// Recent Files
// =============================================================================

void EditorMenuSystem::AddRecentFile(const std::filesystem::path& path) {
    // Remove existing entry if present
    m_recentFiles.erase(
        std::remove_if(m_recentFiles.begin(), m_recentFiles.end(),
            [&path](const RecentProject& p) { return p.path == path.string(); }),
        m_recentFiles.end()
    );

    // Add to front
    RecentProject entry;
    entry.path = path.string();
    entry.name = path.stem().string();
    entry.lastOpened = std::chrono::system_clock::now();
    entry.exists = std::filesystem::exists(path);
    m_recentFiles.insert(m_recentFiles.begin(), entry);

    // Trim to max size
    if (m_recentFiles.size() > m_maxRecentFiles) {
        m_recentFiles.resize(m_maxRecentFiles);
    }
}

void EditorMenuSystem::ClearRecentFiles() {
    m_recentFiles.clear();
    SaveRecentFiles();
}

bool EditorMenuSystem::LoadRecentFiles() {
    // TODO: Load from settings/config file
    m_recentFiles.clear();
    return true;
}

bool EditorMenuSystem::SaveRecentFiles() {
    // TODO: Save to settings/config file
    return true;
}

// =============================================================================
// Callbacks
// =============================================================================

void EditorMenuSystem::SetFileMenuCallbacks(
    std::function<void()> onNew,
    std::function<void()> onOpen,
    std::function<void()> onSave,
    std::function<void()> onSaveAs,
    std::function<void(const std::filesystem::path&)> onOpenRecent,
    std::function<void()> onPreferences,
    std::function<void()> onExit)
{
    m_onNew = std::move(onNew);
    m_onOpen = std::move(onOpen);
    m_onSave = std::move(onSave);
    m_onSaveAs = std::move(onSaveAs);
    m_onOpenRecent = std::move(onOpenRecent);
    m_onPreferences = std::move(onPreferences);
    m_onExit = std::move(onExit);
}

void EditorMenuSystem::SetEditMenuCallbacks(
    std::function<void()> onUndo,
    std::function<void()> onRedo,
    std::function<bool()> canUndo,
    std::function<bool()> canRedo,
    std::function<void()> onDelete,
    std::function<void()> onDuplicate,
    std::function<void()> onSelectAll,
    std::function<void()> onDeselectAll,
    std::function<void()> onInvertSelection,
    std::function<bool()> hasSelection)
{
    m_onUndo = std::move(onUndo);
    m_onRedo = std::move(onRedo);
    m_canUndo = std::move(canUndo);
    m_canRedo = std::move(canRedo);
    m_onDelete = std::move(onDelete);
    m_onDuplicate = std::move(onDuplicate);
    m_onSelectAll = std::move(onSelectAll);
    m_onDeselectAll = std::move(onDeselectAll);
    m_onInvertSelection = std::move(onInvertSelection);
    m_hasSelection = std::move(hasSelection);
}

// =============================================================================
// Rendering
// =============================================================================

void EditorMenuSystem::RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        RenderFileMenu();
        RenderEditMenu();
        RenderViewMenu();
        RenderGameObjectMenu();
        RenderComponentMenu();
        RenderWindowMenu();
        RenderCustomMenus();
        RenderHelpMenu();
        ImGui::EndMainMenuBar();
    }
}

void EditorMenuSystem::RenderFileMenu() {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene", GetShortcutForAction("New").c_str())) {
            if (m_onNew) m_onNew();
        }
        if (ImGui::MenuItem("Open Scene...", GetShortcutForAction("Open").c_str())) {
            if (m_onOpen) m_onOpen();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save", GetShortcutForAction("Save").c_str(), false, m_sceneDirty)) {
            if (m_onSave) m_onSave();
        }
        if (ImGui::MenuItem("Save As...", GetShortcutForAction("SaveAs").c_str())) {
            if (m_onSaveAs) m_onSaveAs();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Recent Projects")) {
            if (m_recentFiles.empty()) {
                ImGui::MenuItem("No recent projects", nullptr, false, false);
            } else {
                for (const auto& recent : m_recentFiles) {
                    if (ImGui::MenuItem(recent.name.c_str(), nullptr, false, recent.exists)) {
                        if (m_onOpenRecent) m_onOpenRecent(recent.path);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Clear Recent")) {
                    ClearRecentFiles();
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Preferences...")) {
            if (m_onPreferences) m_onPreferences();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Exit", "Alt+F4")) {
            if (m_onExit) m_onExit();
        }

        ImGui::EndMenu();
    }
}

void EditorMenuSystem::RenderEditMenu() {
    if (ImGui::BeginMenu("Edit")) {
        bool canUndo = m_canUndo ? m_canUndo() : false;
        bool canRedo = m_canRedo ? m_canRedo() : false;
        bool hasSelection = m_hasSelection ? m_hasSelection() : false;

        if (ImGui::MenuItem("Undo", GetShortcutForAction("Undo").c_str(), false, canUndo)) {
            if (m_onUndo) m_onUndo();
        }
        if (ImGui::MenuItem("Redo", GetShortcutForAction("Redo").c_str(), false, canRedo)) {
            if (m_onRedo) m_onRedo();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Cut", "Ctrl+X", false, hasSelection)) {
            // TODO: Implement cut
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, hasSelection)) {
            // TODO: Implement copy
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V")) {
            // TODO: Implement paste
        }
        if (ImGui::MenuItem("Delete", GetShortcutForAction("Delete").c_str(), false, hasSelection)) {
            if (m_onDelete) m_onDelete();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Duplicate", GetShortcutForAction("Duplicate").c_str(), false, hasSelection)) {
            if (m_onDuplicate) m_onDuplicate();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Select All", GetShortcutForAction("SelectAll").c_str())) {
            if (m_onSelectAll) m_onSelectAll();
        }
        if (ImGui::MenuItem("Deselect All")) {
            if (m_onDeselectAll) m_onDeselectAll();
        }
        if (ImGui::MenuItem("Invert Selection")) {
            if (m_onInvertSelection) m_onInvertSelection();
        }

        ImGui::EndMenu();
    }
}

void EditorMenuSystem::RenderViewMenu() {
    if (!m_editor) return;

    if (ImGui::BeginMenu("View")) {
        // Panel toggles
        if (ImGui::BeginMenu("Panels")) {
            auto panels = m_editor->GetAllPanels();
            for (const auto& panel : panels) {
                bool visible = panel->IsVisible();
                if (ImGui::MenuItem(panel->GetTitle().c_str(), nullptr, &visible)) {
                    panel->SetVisible(visible);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        // Layout presets
        if (ImGui::BeginMenu("Layout")) {
            if (ImGui::MenuItem("Default")) {
                m_editor->GetLayoutManager().ResetLayout();
            }
            ImGui::Separator();
            auto layouts = m_editor->GetLayoutManager().GetLayoutNames();
            for (const auto& name : layouts) {
                if (ImGui::MenuItem(name.c_str())) {
                    m_editor->GetLayoutManager().LoadLayout(name);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Layout...")) {
                // TODO: Show save layout dialog
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        // View options
        auto& settings = m_editor->GetSettings();
        if (ImGui::MenuItem("Show Grid", nullptr, &settings.showGrid)) {}
        if (ImGui::MenuItem("Show Gizmos", nullptr, &settings.showGizmos)) {}
        if (ImGui::MenuItem("Show Icons", nullptr, &settings.showIcons)) {}

        ImGui::EndMenu();
    }
}

void EditorMenuSystem::RenderGameObjectMenu() {
    if (ImGui::BeginMenu("GameObject")) {
        if (ImGui::MenuItem("Create Empty")) {
            // TODO: Create empty game object
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("3D Object")) {
            if (ImGui::MenuItem("Cube")) {}
            if (ImGui::MenuItem("Sphere")) {}
            if (ImGui::MenuItem("Cylinder")) {}
            if (ImGui::MenuItem("Plane")) {}
            if (ImGui::MenuItem("Quad")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("SDF Primitive")) {
            if (ImGui::MenuItem("SDF Sphere")) {}
            if (ImGui::MenuItem("SDF Box")) {}
            if (ImGui::MenuItem("SDF Cylinder")) {}
            if (ImGui::MenuItem("SDF Torus")) {}
            if (ImGui::MenuItem("SDF Capsule")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Light")) {
            if (ImGui::MenuItem("Directional Light")) {}
            if (ImGui::MenuItem("Point Light")) {}
            if (ImGui::MenuItem("Spot Light")) {}
            if (ImGui::MenuItem("Area Light")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Camera")) {
            if (ImGui::MenuItem("Perspective Camera")) {}
            if (ImGui::MenuItem("Orthographic Camera")) {}
            ImGui::EndMenu();
        }

        ImGui::Separator();

        bool hasMultiSelection = m_hasSelection && m_hasSelection() &&
                                  m_editor && m_editor->GetSelectionManager().GetSelectionCount() > 1;
        if (ImGui::MenuItem("Group Selection", nullptr, false, hasMultiSelection)) {
            // TODO: Group selected objects
        }

        ImGui::EndMenu();
    }
}

void EditorMenuSystem::RenderComponentMenu() {
    bool hasSelection = m_hasSelection ? m_hasSelection() : false;

    if (ImGui::BeginMenu("Component")) {
        if (ImGui::BeginMenu("Rendering", hasSelection)) {
            if (ImGui::MenuItem("Mesh Renderer")) {}
            if (ImGui::MenuItem("SDF Renderer")) {}
            if (ImGui::MenuItem("Particle System")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Physics", hasSelection)) {
            if (ImGui::MenuItem("Rigidbody")) {}
            if (ImGui::MenuItem("Collider")) {}
            if (ImGui::MenuItem("SDF Collider")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Audio", hasSelection)) {
            if (ImGui::MenuItem("Audio Source")) {}
            if (ImGui::MenuItem("Audio Listener")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Animation", hasSelection)) {
            if (ImGui::MenuItem("Animator")) {}
            if (ImGui::MenuItem("Animation")) {}
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }
}

void EditorMenuSystem::RenderWindowMenu() {
    if (ImGui::BeginMenu("Window")) {
        // Core panels
        if (ImGui::MenuItem("Hierarchy")) {
            if (m_editor) m_editor->ShowPanel("SceneOutliner");
        }
        if (ImGui::MenuItem("Inspector")) {
            // TODO: ShowPanel("Properties")
        }
        if (ImGui::MenuItem("Console")) {
            if (m_editor) m_editor->ShowPanel("Console");
        }
        if (ImGui::MenuItem("Asset Browser")) {
            // TODO: ShowPanel("AssetBrowser")
        }

        ImGui::Separator();

        // Editor panels
        if (ImGui::MenuItem("Viewport")) {}
        if (ImGui::MenuItem("SDF Toolbox")) {}
        if (ImGui::MenuItem("Animation Timeline")) {}
        if (ImGui::MenuItem("Material Editor")) {}

        ImGui::EndMenu();
    }
}

void EditorMenuSystem::RenderHelpMenu() {
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Documentation")) {
            // TODO: Open documentation
        }
        if (ImGui::MenuItem("API Reference")) {
            // TODO: Open API docs
        }

        ImGui::Separator();

        if (ImGui::MenuItem("About Nova3D Editor")) {
            if (m_editor) {
                m_editor->ShowMessageDialog("About Nova3D Editor",
                    "Nova3D Engine v" + std::string(Engine::GetVersion()) + "\n\n"
                    "A modern 3D game engine with SDF rendering,\n"
                    "global illumination, and advanced tooling.\n\n"
                    "(c) 2024 Nova Engine Team");
            }
        }

        ImGui::EndMenu();
    }
}

void EditorMenuSystem::RenderCustomMenus() {
    for (const auto& [menuName, items] : m_customMenus) {
        if (ImGui::BeginMenu(menuName.c_str())) {
            for (const auto& item : items) {
                RenderMenuItem(item);
            }
            ImGui::EndMenu();
        }
    }
}

void EditorMenuSystem::RenderMenuItem(const MenuItem& item) {
    // Determine enabled state
    bool enabled = item.enabled;
    if (item.enabledCallback) {
        enabled = enabled && item.enabledCallback();
    }

    switch (item.type) {
        case MenuItemType::Action:
            if (ImGui::MenuItem(item.label.c_str(), item.shortcut.c_str(), false, enabled)) {
                if (item.action) item.action();
            }
            break;

        case MenuItemType::Checkbox: {
            bool checked = item.checked;
            if (item.checkedCallback) {
                checked = item.checkedCallback();
            }
            if (ImGui::MenuItem(item.label.c_str(), item.shortcut.c_str(), &checked, enabled)) {
                if (item.action) item.action();
            }
            break;
        }

        case MenuItemType::Submenu:
            if (ImGui::BeginMenu(item.label.c_str(), enabled)) {
                for (const auto& subitem : item.submenuItems) {
                    RenderMenuItem(subitem);
                }
                ImGui::EndMenu();
            }
            break;

        case MenuItemType::Separator:
            ImGui::Separator();
            break;
    }
}

} // namespace Nova
