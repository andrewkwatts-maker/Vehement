/**
 * @file PreferencesPanel.cpp
 * @brief Implementation of the Preferences/Settings modal dialog panel
 */

#include "PreferencesPanel.hpp"
#include "../core/Logger.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace Nova {

// =============================================================================
// PreferencesPanel Implementation
// =============================================================================

PreferencesPanel::PreferencesPanel() {
    // Initialize configuration
    m_config.title = "Preferences";
    m_config.id = "preferences_panel";
    m_config.flags = EditorPanel::Flags::NoTitleBar |
                     EditorPanel::Flags::NoResize |
                     EditorPanel::Flags::NoMove |
                     EditorPanel::Flags::NoDocking;
    m_config.defaultSize = glm::vec2(900, 650);
    m_config.minSize = glm::vec2(700, 500);

    // Initialize expanded groups
    m_expandedGroups["General"] = true;
    m_expandedGroups["Appearance"] = true;
    m_expandedGroups["Viewport"] = true;
    m_expandedGroups["Camera"] = true;
    m_expandedGroups["Grid"] = true;
    m_expandedGroups["Input"] = true;
    m_expandedGroups["Mouse"] = true;
    m_expandedGroups["Shortcuts"] = true;
    m_expandedGroups["Performance"] = true;
    m_expandedGroups["Paths"] = true;
    m_expandedGroups["Plugins"] = true;
}

PreferencesPanel::~PreferencesPanel() {
    // Nothing to clean up
}

void PreferencesPanel::OnInitialize() {
    // Copy current settings to pending
    m_pendingSettings = EditorSettings::Instance().GetSettings();
    m_hasChanges = false;
}

void PreferencesPanel::OnShutdown() {
    // Discard any pending changes
    m_hasChanges = false;
}

void PreferencesPanel::ShowModal() {
    m_showing = true;
    m_shouldClose = false;
    m_animationProgress = 0.0f;

    // Copy current settings
    m_pendingSettings = EditorSettings::Instance().GetSettings();
    m_hasChanges = false;

    // Validate on open
    ValidateSettings();

    Show();
}

void PreferencesPanel::HideModal() {
    m_showing = false;
    m_shouldClose = false;
    Hide();
}

void PreferencesPanel::SelectCategory(SettingsCategory category) {
    m_selectedCategory = category;
}

void PreferencesPanel::FocusSetting(const std::string& settingKey) {
    // Parse category from key
    size_t dotPos = settingKey.find('.');
    if (dotPos != std::string::npos) {
        std::string category = settingKey.substr(0, dotPos);

        if (category == "general")     m_selectedCategory = SettingsCategory::General;
        else if (category == "appearance")  m_selectedCategory = SettingsCategory::Appearance;
        else if (category == "viewport")    m_selectedCategory = SettingsCategory::Viewport;
        else if (category == "input")       m_selectedCategory = SettingsCategory::Input;
        else if (category == "performance") m_selectedCategory = SettingsCategory::Performance;
        else if (category == "paths")       m_selectedCategory = SettingsCategory::Paths;
        else if (category == "plugins")     m_selectedCategory = SettingsCategory::Plugins;
    }

    m_scrollToSetting = true;
    m_settingToScrollTo = settingKey;
}

void PreferencesPanel::Update(float deltaTime) {
    if (!m_showing) return;

    // Animate opening/closing
    if (m_shouldClose) {
        m_animationProgress -= deltaTime * 5.0f;
        if (m_animationProgress <= 0.0f) {
            m_showing = false;
            m_shouldClose = false;
            Hide();
        }
    }
    else {
        m_animationProgress = std::min(1.0f, m_animationProgress + deltaTime * 5.0f);
    }
}

void PreferencesPanel::OnRender() {
    if (!m_showing) return;

    // Center the modal
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize(m_config.defaultSize.x, m_config.defaultSize.y);
    ImVec2 windowPos(
        (io.DisplaySize.x - windowSize.x) * 0.5f,
        (io.DisplaySize.y - windowSize.y) * 0.5f
    );

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    // Modal dimming overlay
    ImGui::SetNextWindowBgAlpha(0.6f * m_animationProgress);
    ImGui::Begin("##PreferencesOverlay", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::SetWindowPos(ImVec2(0, 0));
    ImGui::SetWindowSize(io.DisplaySize);
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
        // Click outside to close (optional)
        // m_shouldClose = true;
    }
    ImGui::End();

    // Main preferences window
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("Preferences##Modal", &m_showing, windowFlags)) {
        ImGui::PopStyleVar();  // WindowPadding

        // Title bar
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
        ImGui::BeginChild("TitleBar", ImVec2(0, 40), false);
        ImGui::SetCursorPos(ImVec2(16, 10));
        ImGui::Text("Preferences");

        // Close button
        float closeX = ImGui::GetWindowWidth() - 36;
        ImGui::SetCursorPos(ImVec2(closeX, 8));
        if (ImGui::Button("X", ImVec2(24, 24))) {
            if (m_hasChanges) {
                // TODO: Show confirmation dialog
                DiscardChanges();
            }
            m_shouldClose = true;
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Search bar
        RenderSearchBar();

        // Main content area
        ImGui::BeginChild("MainContent", ImVec2(0, -50), false);

        // Split into category list and settings content
        ImGui::Columns(2, "PreferencesColumns", true);
        ImGui::SetColumnWidth(0, m_categoryListWidth);

        // Left column: Category list
        ImGui::BeginChild("CategoryList", ImVec2(0, 0), true);
        RenderCategoryList();
        ImGui::EndChild();

        ImGui::NextColumn();

        // Right column: Settings content
        ImGui::BeginChild("SettingsContent", ImVec2(0, 0), true);
        RenderSettingsContent();
        ImGui::EndChild();

        ImGui::Columns(1);
        ImGui::EndChild();

        // Button bar
        RenderButtonBar();

        // Validation messages
        RenderValidationMessages();
    }
    else {
        ImGui::PopStyleVar();  // WindowPadding
    }
    ImGui::End();

    ImGui::PopStyleVar();  // WindowRounding

    // Conflict dialog
    if (m_showConflictDialog) {
        RenderConflictDialog();
    }
}

void PreferencesPanel::RenderCategoryList() {
    const auto& theme = EditorTheme::Instance();

    struct CategoryItem {
        SettingsCategory category;
        const char* name;
        const char* icon;
    };

    CategoryItem categories[] = {
        {SettingsCategory::General,     "General",     GetSettingsCategoryIcon(SettingsCategory::General)},
        {SettingsCategory::Appearance,  "Appearance",  GetSettingsCategoryIcon(SettingsCategory::Appearance)},
        {SettingsCategory::Viewport,    "Viewport",    GetSettingsCategoryIcon(SettingsCategory::Viewport)},
        {SettingsCategory::Input,       "Input",       GetSettingsCategoryIcon(SettingsCategory::Input)},
        {SettingsCategory::Performance, "Performance", GetSettingsCategoryIcon(SettingsCategory::Performance)},
        {SettingsCategory::Paths,       "Paths",       GetSettingsCategoryIcon(SettingsCategory::Paths)},
        {SettingsCategory::Plugins,     "Plugins",     GetSettingsCategoryIcon(SettingsCategory::Plugins)}
    };

    for (const auto& item : categories) {
        bool selected = (m_selectedCategory == item.category);

        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));

        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Header, EditorTheme::ToImVec4(theme.GetColors().selection));
        }

        if (ImGui::Selectable(item.name, selected, ImGuiSelectableFlags_None, ImVec2(0, 28))) {
            m_selectedCategory = item.category;
        }

        if (selected) {
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleVar();
    }

    // Bottom buttons
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Import...", ImVec2(-1, 0))) {
        ImportSettings();
    }

    if (ImGui::Button("Export...", ImVec2(-1, 0))) {
        ExportSettings();
    }

    ImGui::Spacing();

    if (ImGui::Button("Reset All", ImVec2(-1, 0))) {
        ImGui::OpenPopup("ResetAllConfirm");
    }

    // Reset confirmation popup
    if (ImGui::BeginPopupModal("ResetAllConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Reset all settings to defaults?");
        ImGui::Text("This cannot be undone.");
        ImGui::Spacing();

        if (ImGui::Button("Reset", ImVec2(100, 0))) {
            ResetAllSettings();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void PreferencesPanel::RenderSearchBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
    ImGui::SetCursorPos(ImVec2(m_categoryListWidth + 16, 50));
    ImGui::SetNextItemWidth(300);

    if (ImGui::InputTextWithHint("##SearchSettings", "Search settings...",
                                  m_searchBuffer, sizeof(m_searchBuffer))) {
        m_searchFilter = m_searchBuffer;
    }

    ImGui::PopStyleVar();
}

void PreferencesPanel::RenderSettingsContent() {
    // Header
    const char* categoryName = GetSettingsCategoryName(m_selectedCategory);
    ImGui::PushFont(nullptr);  // Would use header font
    ImGui::Text("%s", categoryName);
    ImGui::PopFont();

    // Reset category button
    ImGui::SameLine(ImGui::GetWindowWidth() - 120);
    if (ImGui::SmallButton("Reset to Defaults")) {
        ResetCurrentCategory();
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Render category-specific settings
    RenderCategorySettings(m_selectedCategory);
}

void PreferencesPanel::RenderCategorySettings(SettingsCategory category) {
    switch (category) {
        case SettingsCategory::General:
            RenderGeneralSettings();
            break;
        case SettingsCategory::Appearance:
            RenderAppearanceSettings();
            break;
        case SettingsCategory::Viewport:
            RenderViewportSettings();
            break;
        case SettingsCategory::Input:
            RenderInputSettings();
            break;
        case SettingsCategory::Performance:
            RenderPerformanceSettings();
            break;
        case SettingsCategory::Paths:
            RenderPathSettings();
            break;
        case SettingsCategory::Plugins:
            RenderPluginSettings();
            break;
    }
}

void PreferencesPanel::RenderGeneralSettings() {
    auto& settings = m_pendingSettings.general;

    // Auto-save section
    if (BeginSettingsGroup("Auto-Save")) {
        if (BoolSetting("Enable Auto-Save", settings.autoSaveEnabled,
                       "Automatically save your work at regular intervals")) {
            m_hasChanges = true;
        }

        if (settings.autoSaveEnabled) {
            if (IntSetting("Interval (minutes)", settings.autoSaveIntervalMinutes, 1, 60,
                          "Time between automatic saves")) {
                m_hasChanges = true;
            }
        }
        EndSettingsGroup();
    }

    // History section
    if (BeginSettingsGroup("History")) {
        if (IntSetting("Undo History Size", settings.undoHistorySize, 10, 500,
                      "Maximum number of undo steps to keep")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Startup section
    if (BeginSettingsGroup("Startup")) {
        if (BoolSetting("Show Welcome Screen", settings.showWelcomeOnStartup,
                       "Show the welcome screen when the editor starts")) {
            m_hasChanges = true;
        }

        if (BoolSetting("Reopen Last Project", settings.reopenLastProject,
                       "Automatically reopen the last project on startup")) {
            m_hasChanges = true;
        }

        if (IntSetting("Recent Projects Max", settings.recentProjectsMax, 5, 25,
                      "Maximum number of recent projects to remember")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Behavior section
    if (BeginSettingsGroup("Behavior")) {
        if (BoolSetting("Confirm on Exit", settings.confirmOnExit,
                       "Ask for confirmation when exiting with unsaved changes")) {
            m_hasChanges = true;
        }

        if (BoolSetting("Check for Updates", settings.checkForUpdates,
                       "Automatically check for editor updates on startup")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Localization section
    if (BeginSettingsGroup("Localization")) {
        // Language dropdown
        const char* languages[] = {"en-US", "en-GB", "de-DE", "fr-FR", "es-ES", "ja-JP", "zh-CN"};
        int langIndex = 0;
        for (int i = 0; i < 7; ++i) {
            if (settings.language == languages[i]) {
                langIndex = i;
                break;
            }
        }
        if (ImGui::Combo("Language", &langIndex, languages, 7)) {
            settings.language = languages[langIndex];
            m_hasChanges = true;
        }

        if (StringSetting("Date Format", settings.dateFormat,
                         "Format for displaying dates (e.g., yyyy-MM-dd)")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }
}

void PreferencesPanel::RenderAppearanceSettings() {
    auto& settings = m_pendingSettings.appearance;

    // Theme section
    if (BeginSettingsGroup("Theme")) {
        const char* themes[] = {"Dark", "Light", "Custom"};
        int themeIndex = static_cast<int>(settings.theme);
        if (ImGui::Combo("Theme", &themeIndex, themes, 3)) {
            settings.theme = static_cast<EditorThemePreset>(themeIndex);
            m_hasChanges = true;
        }

        if (settings.theme == EditorThemePreset::Custom) {
            if (PathSetting("Custom Theme File", settings.customThemePath, false,
                           "Path to custom theme JSON file")) {
                m_hasChanges = true;
            }
        }

        if (ColorSetting("Accent Color", settings.accentColor,
                        "Primary accent color used throughout the UI")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Text section
    if (BeginSettingsGroup("Text")) {
        if (FloatSetting("Font Size", settings.fontSize, 8.0f, 24.0f,
                        "Base font size for UI text")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Icons section
    if (BeginSettingsGroup("Icons")) {
        const char* iconSizes[] = {"Small", "Medium", "Large"};
        int sizeIndex = static_cast<int>(settings.iconSize);
        if (ImGui::Combo("Icon Size", &sizeIndex, iconSizes, 3)) {
            settings.iconSize = static_cast<IconSize>(sizeIndex);
            m_hasChanges = true;
        }

        if (BoolSetting("Show Toolbar Text", settings.showToolbarText,
                       "Show text labels on toolbar buttons")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Window section
    if (BeginSettingsGroup("Window")) {
        if (FloatSetting("Window Opacity", settings.windowOpacity, 0.5f, 1.0f,
                        "Opacity of editor windows")) {
            m_hasChanges = true;
        }

        if (FloatSetting("Panel Border Width", settings.panelBorderWidth, 0.0f, 4.0f,
                        "Width of panel borders")) {
            m_hasChanges = true;
        }

        if (BoolSetting("Use Native Window Frame", settings.useNativeWindowFrame,
                       "Use the operating system's native window frame")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Animation section
    if (BeginSettingsGroup("Animation")) {
        if (BoolSetting("Animate Transitions", settings.animateTransitions,
                       "Enable smooth transitions for UI elements")) {
            m_hasChanges = true;
        }

        if (settings.animateTransitions) {
            if (FloatSetting("Animation Speed", settings.animationSpeed, 0.5f, 2.0f,
                            "Speed multiplier for UI animations")) {
                m_hasChanges = true;
            }
        }
        EndSettingsGroup();
    }
}

void PreferencesPanel::RenderViewportSettings() {
    auto& settings = m_pendingSettings.viewport;

    // Camera section
    if (BeginSettingsGroup("Camera")) {
        const char* cameraModes[] = {"Perspective", "Orthographic", "Top", "Front", "Side"};
        int modeIndex = static_cast<int>(settings.defaultCameraMode);
        if (ImGui::Combo("Default Camera Mode", &modeIndex, cameraModes, 5)) {
            settings.defaultCameraMode = static_cast<DefaultCameraMode>(modeIndex);
            m_hasChanges = true;
        }

        if (FloatSetting("Field of View", settings.fieldOfView, 30.0f, 120.0f,
                        "Vertical field of view in degrees")) {
            m_hasChanges = true;
        }

        if (FloatSetting("Near Clip Plane", settings.nearClipPlane, 0.01f, 10.0f,
                        "Near clipping plane distance")) {
            m_hasChanges = true;
        }

        if (FloatSetting("Far Clip Plane", settings.farClipPlane, 100.0f, 100000.0f,
                        "Far clipping plane distance")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Grid section
    if (BeginSettingsGroup("Grid")) {
        if (BoolSetting("Show Grid", settings.showGrid,
                       "Display the reference grid in viewport")) {
            m_hasChanges = true;
        }

        if (settings.showGrid) {
            if (FloatSetting("Grid Size", settings.gridSize, 0.1f, 100.0f,
                            "Size of grid cells")) {
                m_hasChanges = true;
            }

            if (IntSetting("Grid Subdivisions", settings.gridSubdivisions, 1, 20,
                          "Number of subdivisions per grid cell")) {
                m_hasChanges = true;
            }

            if (ColorSetting("Grid Color", settings.gridColor,
                            "Color of the grid lines")) {
                m_hasChanges = true;
            }
        }
        EndSettingsGroup();
    }

    // Display section
    if (BeginSettingsGroup("Display")) {
        if (ColorSetting("Background Color", settings.backgroundColor,
                        "Viewport background color")) {
            m_hasChanges = true;
        }

        if (ColorSetting("Selection Color", settings.selectionColor,
                        "Color of selected objects' outlines")) {
            m_hasChanges = true;
        }

        if (BoolSetting("Show Axis Gizmo", settings.showAxisGizmo,
                       "Display the axis orientation gizmo")) {
            m_hasChanges = true;
        }

        if (BoolSetting("Show World Origin", settings.showWorldOrigin,
                       "Display a marker at the world origin")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Gizmos section
    if (BeginSettingsGroup("Gizmos")) {
        if (FloatSetting("Gizmo Size", settings.gizmoSize, 0.5f, 3.0f,
                        "Size of transform gizmos")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Rendering section
    if (BeginSettingsGroup("Rendering")) {
        const char* aaModes[] = {"None", "FXAA", "MSAA 2x", "MSAA 4x", "MSAA 8x", "TAA"};
        int aaIndex = static_cast<int>(settings.antiAliasingMode);
        if (ImGui::Combo("Anti-Aliasing", &aaIndex, aaModes, 6)) {
            settings.antiAliasingMode = static_cast<AntiAliasingMode>(aaIndex);
            m_hasChanges = true;
        }

        if (IntSetting("Max FPS", settings.maxFPS, 0, 240,
                      "Maximum frame rate (0 = unlimited)")) {
            m_hasChanges = true;
        }

        if (BoolSetting("Show FPS Counter", settings.showFPS,
                       "Display frame rate in viewport")) {
            m_hasChanges = true;
        }

        if (BoolSetting("Show Statistics", settings.showStats,
                       "Display detailed rendering statistics")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }
}

void PreferencesPanel::RenderInputSettings() {
    auto& settings = m_pendingSettings.input;

    // Mouse section
    if (BeginSettingsGroup("Mouse")) {
        if (FloatSetting("Mouse Sensitivity", settings.mouseSensitivity, 0.1f, 5.0f,
                        "Overall mouse sensitivity multiplier")) {
            m_hasChanges = true;
        }

        if (FloatSetting("Scroll Speed", settings.scrollSpeed, 0.1f, 5.0f,
                        "Mouse scroll wheel speed")) {
            m_hasChanges = true;
        }

        if (BoolSetting("Invert Mouse Y", settings.invertMouseY,
                       "Invert vertical mouse movement")) {
            m_hasChanges = true;
        }

        if (BoolSetting("Invert Mouse X", settings.invertMouseX,
                       "Invert horizontal mouse movement")) {
            m_hasChanges = true;
        }

        if (FloatSetting("Double-Click Time", settings.doubleClickTime, 0.1f, 1.0f,
                        "Maximum time between clicks for double-click (seconds)")) {
            m_hasChanges = true;
        }

        if (FloatSetting("Drag Threshold", settings.dragThreshold, 1.0f, 20.0f,
                        "Minimum distance to start a drag operation (pixels)")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Camera Navigation section
    if (BeginSettingsGroup("Camera Navigation")) {
        if (FloatSetting("Pan Speed", settings.panSpeed, 0.1f, 5.0f,
                        "Camera panning speed")) {
            m_hasChanges = true;
        }

        if (FloatSetting("Orbit Speed", settings.orbitSpeed, 0.1f, 5.0f,
                        "Camera orbit speed")) {
            m_hasChanges = true;
        }

        if (FloatSetting("Zoom Speed", settings.zoomSpeed, 0.1f, 5.0f,
                        "Camera zoom speed")) {
            m_hasChanges = true;
        }

        if (BoolSetting("Smooth Camera", settings.smoothCamera,
                       "Enable smooth camera movement")) {
            m_hasChanges = true;
        }

        if (settings.smoothCamera) {
            if (FloatSetting("Camera Smoothness", settings.cameraSmoothness, 0.0f, 0.5f,
                            "Amount of smoothing applied to camera movement")) {
                m_hasChanges = true;
            }
        }
        EndSettingsGroup();
    }

    // Gamepad section
    if (BeginSettingsGroup("Gamepad")) {
        if (BoolSetting("Enable Gamepad", settings.enableGamepad,
                       "Enable gamepad/controller input")) {
            m_hasChanges = true;
        }

        if (settings.enableGamepad) {
            if (FloatSetting("Deadzone", settings.gamepadDeadzone, 0.0f, 0.5f,
                            "Analog stick deadzone")) {
                m_hasChanges = true;
            }
        }
        EndSettingsGroup();
    }

    // Keyboard Shortcuts section
    if (BeginSettingsGroup("Keyboard Shortcuts")) {
        RenderShortcutsEditor();
        EndSettingsGroup();
    }
}

void PreferencesPanel::RenderPerformanceSettings() {
    auto& settings = m_pendingSettings.performance;

    // Quality section
    if (BeginSettingsGroup("Quality")) {
        const char* shadowQualities[] = {"Off", "Low", "Medium", "High", "Ultra"};
        int sqIndex = static_cast<int>(settings.shadowQuality);
        if (ImGui::Combo("Shadow Quality", &sqIndex, shadowQualities, 5)) {
            settings.shadowQuality = static_cast<ShadowQualityPreset>(sqIndex);
            m_hasChanges = true;
        }

        if (IntSetting("Max Texture Size", settings.maxTextureSize, 256, 8192,
                      "Maximum texture resolution")) {
            m_hasChanges = true;
        }

        if (FloatSetting("LOD Bias", settings.lodBias, -2.0f, 2.0f,
                        "Level of detail bias (negative = higher quality)")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Frame Rate section
    if (BeginSettingsGroup("Frame Rate")) {
        if (BoolSetting("Enable VSync", settings.enableVSync,
                       "Synchronize frame rate with display refresh rate")) {
            m_hasChanges = true;
        }

        if (!settings.enableVSync) {
            if (IntSetting("Target Editor FPS", settings.targetEditorFPS, 30, 240,
                          "Target frame rate for the editor")) {
                m_hasChanges = true;
            }
        }

        if (BoolSetting("Low Power Mode", settings.lowPowerMode,
                       "Reduce editor frame rate when not in focus")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Memory section
    if (BeginSettingsGroup("Memory")) {
        if (IntSetting("GPU Memory Limit (MB)", settings.gpuMemoryLimitMB, 0, 16384,
                      "Maximum GPU memory usage (0 = automatic)")) {
            m_hasChanges = true;
        }

        if (IntSetting("Thumbnail Cache (MB)", settings.thumbnailCacheSizeMB, 32, 1024,
                      "Memory allocated for asset thumbnail cache")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Threading section
    if (BeginSettingsGroup("Threading")) {
        if (IntSetting("Worker Threads", settings.workerThreadCount, 0, 32,
                      "Number of background worker threads (0 = automatic)")) {
            m_hasChanges = true;
        }

        if (BoolSetting("Async Loading", settings.enableAsyncLoading,
                       "Load assets asynchronously in the background")) {
            m_hasChanges = true;
        }

        if (BoolSetting("Texture Streaming", settings.enableTextureStreaming,
                       "Stream textures on demand instead of loading all at once")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Debugging section
    if (BeginSettingsGroup("Debugging")) {
        if (BoolSetting("Enable Profiling", settings.enableEditorProfiling,
                       "Enable performance profiling (may impact performance)")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }
}

void PreferencesPanel::RenderPathSettings() {
    auto& settings = m_pendingSettings.paths;

    // Project Paths section
    if (BeginSettingsGroup("Project Paths")) {
        if (PathSetting("Default Project Path", settings.defaultProjectPath, true,
                       "Default location for new projects")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Editor Paths section
    if (BeginSettingsGroup("Editor Paths")) {
        if (PathSetting("Temp Directory", settings.tempDirectory, true,
                       "Temporary files directory")) {
            m_hasChanges = true;
        }

        if (PathSetting("Log Directory", settings.logDirectory, true,
                       "Editor log files directory")) {
            m_hasChanges = true;
        }

        if (PathSetting("Autosave Directory", settings.autosaveDirectory, true,
                       "Autosave backup files directory")) {
            m_hasChanges = true;
        }

        if (PathSetting("Screenshot Directory", settings.screenshotDirectory, true,
                       "Default location for screenshots")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Plugin Paths section
    if (BeginSettingsGroup("Plugin Paths")) {
        ImGui::Text("Plugin Directories:");
        for (size_t i = 0; i < settings.pluginDirectories.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            char buf[512];
            std::strncpy(buf, settings.pluginDirectories[i].c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';

            if (ImGui::InputText("##PluginDir", buf, sizeof(buf))) {
                settings.pluginDirectories[i] = buf;
                m_hasChanges = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("X")) {
                settings.pluginDirectories.erase(settings.pluginDirectories.begin() + i);
                m_hasChanges = true;
            }
            ImGui::PopID();
        }
        if (ImGui::Button("Add Plugin Directory")) {
            settings.pluginDirectories.push_back("");
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Script Paths section
    if (BeginSettingsGroup("Script Paths")) {
        ImGui::Text("Script Directories:");
        for (size_t i = 0; i < settings.scriptDirectories.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            char buf[512];
            std::strncpy(buf, settings.scriptDirectories[i].c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';

            if (ImGui::InputText("##ScriptDir", buf, sizeof(buf))) {
                settings.scriptDirectories[i] = buf;
                m_hasChanges = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("X")) {
                settings.scriptDirectories.erase(settings.scriptDirectories.begin() + i);
                m_hasChanges = true;
            }
            ImGui::PopID();
        }
        if (ImGui::Button("Add Script Directory")) {
            settings.scriptDirectories.push_back("");
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Options section
    if (BeginSettingsGroup("Options")) {
        if (BoolSetting("Use Relative Paths", settings.useRelativePaths,
                       "Store paths relative to project directory when possible")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }
}

void PreferencesPanel::RenderPluginSettings() {
    auto& settings = m_pendingSettings.plugins;

    // General section
    if (BeginSettingsGroup("General")) {
        if (BoolSetting("Auto-Load Plugins", settings.autoLoadPlugins,
                       "Automatically load plugins on editor startup")) {
            m_hasChanges = true;
        }

        if (BoolSetting("Sandbox Plugins", settings.sandboxPlugins,
                       "Run plugins in isolated sandbox (safer but slower)")) {
            m_hasChanges = true;
        }
        EndSettingsGroup();
    }

    // Enabled Plugins section
    if (BeginSettingsGroup("Enabled Plugins")) {
        if (settings.enabledPlugins.empty()) {
            ImGui::TextDisabled("No plugins enabled");
        }
        else {
            for (size_t i = 0; i < settings.enabledPlugins.size(); ++i) {
                ImGui::BulletText("%s", settings.enabledPlugins[i].c_str());
            }
        }
        EndSettingsGroup();
    }

    // Disabled Plugins section
    if (BeginSettingsGroup("Disabled Plugins")) {
        if (settings.disabledPlugins.empty()) {
            ImGui::TextDisabled("No plugins disabled");
        }
        else {
            for (size_t i = 0; i < settings.disabledPlugins.size(); ++i) {
                ImGui::BulletText("%s", settings.disabledPlugins[i].c_str());
            }
        }
        EndSettingsGroup();
    }
}

void PreferencesPanel::RenderShortcutsEditor() {
    auto& shortcuts = m_pendingSettings.input.shortcuts;

    // Get categories
    std::vector<std::string> categories;
    for (const auto& shortcut : shortcuts) {
        if (std::find(categories.begin(), categories.end(), shortcut.category) == categories.end()) {
            categories.push_back(shortcut.category);
        }
    }
    std::sort(categories.begin(), categories.end());

    // Reset shortcuts button
    if (ImGui::SmallButton("Reset All Shortcuts")) {
        m_pendingSettings.input.shortcuts = EditorSettings::Instance().GetSettings().input.shortcuts;
        EditorSettings::Instance().ResetShortcutsToDefaults();
        m_pendingSettings.input.shortcuts = EditorSettings::Instance().GetSettings().input.shortcuts;
        m_hasChanges = true;
    }

    ImGui::Separator();

    // Render shortcuts by category
    for (const auto& category : categories) {
        if (ImGui::TreeNode(category.c_str())) {
            for (auto& shortcut : shortcuts) {
                if (shortcut.category != category) continue;

                if (!MatchesSearch(shortcut.displayName, shortcut.action)) continue;

                ImGui::PushID(shortcut.action.c_str());

                // Action name
                ImGui::Text("%s", shortcut.displayName.c_str());
                ImGui::SameLine(200);

                // Shortcut button
                std::string shortcutStr = shortcut.ToString();
                if (m_capturingShortcut && m_capturingAction == shortcut.action) {
                    shortcutStr = "Press key...";
                }

                if (ImGui::Button(shortcutStr.c_str(), ImVec2(150, 0))) {
                    m_capturingShortcut = true;
                    m_capturingAction = shortcut.action;
                }

                // Clear button
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    shortcut.key = 0;
                    shortcut.modifiers = KeyModifiers::None;
                    m_hasChanges = true;
                }

                // Capture key press
                if (m_capturingShortcut && m_capturingAction == shortcut.action) {
                    ImGuiIO& io = ImGui::GetIO();
                    for (int key = 0; key < IM_ARRAYSIZE(io.KeysDown); ++key) {
                        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key))) {
                            // Skip modifier keys alone
                            if (key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl ||
                                key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift ||
                                key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt ||
                                key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper) {
                                continue;
                            }

                            KeyModifiers mods = KeyModifiers::None;
                            if (io.KeyCtrl)  mods = mods | KeyModifiers::Ctrl;
                            if (io.KeyShift) mods = mods | KeyModifiers::Shift;
                            if (io.KeyAlt)   mods = mods | KeyModifiers::Alt;
                            if (io.KeySuper) mods = mods | KeyModifiers::Super;

                            shortcut.key = key;
                            shortcut.modifiers = mods;
                            m_capturingShortcut = false;
                            m_capturingAction.clear();
                            m_hasChanges = true;

                            // Check for conflicts
                            CheckShortcutConflicts();
                            break;
                        }
                    }

                    // Cancel on Escape
                    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                        m_capturingShortcut = false;
                        m_capturingAction.clear();
                    }
                }

                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
}

void PreferencesPanel::RenderConflictDialog() {
    ImGui::OpenPopup("Shortcut Conflict");

    if (ImGui::BeginPopupModal("Shortcut Conflict", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("The shortcut you entered conflicts with:");
        ImGui::Text("  %s", m_conflictAction2.c_str());
        ImGui::Spacing();
        ImGui::Text("What would you like to do?");
        ImGui::Spacing();

        if (ImGui::Button("Replace", ImVec2(100, 0))) {
            // Remove conflicting shortcut
            auto& shortcuts = m_pendingSettings.input.shortcuts;
            for (auto& s : shortcuts) {
                if (s.action == m_conflictAction2) {
                    s.key = 0;
                    s.modifiers = KeyModifiers::None;
                    break;
                }
            }
            m_showConflictDialog = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Keep Both", ImVec2(100, 0))) {
            // Do nothing, allow conflict
            m_showConflictDialog = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            // Revert the change
            auto& shortcuts = m_pendingSettings.input.shortcuts;
            for (auto& s : shortcuts) {
                if (s.action == m_conflictAction1) {
                    s.key = 0;
                    s.modifiers = KeyModifiers::None;
                    break;
                }
            }
            m_showConflictDialog = false;
        }

        ImGui::EndPopup();
    }
}

void PreferencesPanel::RenderButtonBar() {
    ImGui::Separator();

    float buttonWidth = 80.0f;
    float totalWidth = buttonWidth * 3 + ImGui::GetStyle().ItemSpacing.x * 2;
    float startX = ImGui::GetWindowWidth() - totalWidth - 16;

    ImGui::SetCursorPos(ImVec2(16, ImGui::GetCursorPosY() + 8));

    // Left side: dirty indicator
    if (m_hasChanges) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "Unsaved changes");
    }

    ImGui::SetCursorPos(ImVec2(startX, ImGui::GetCursorPosY() - 8));

    // Apply button
    if (ImGui::Button("Apply", ImVec2(buttonWidth, 0))) {
        ApplyChanges();
    }

    ImGui::SameLine();

    // Cancel button
    if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
        DiscardChanges();
        m_shouldClose = true;
    }

    ImGui::SameLine();

    // OK button
    if (ImGui::Button("OK", ImVec2(buttonWidth, 0))) {
        ApplyChanges();
        m_shouldClose = true;
    }
}

void PreferencesPanel::RenderValidationMessages() {
    if (!m_showValidation || m_validationResult.valid) return;

    ImGui::SetCursorPos(ImVec2(200, ImGui::GetWindowHeight() - 80));

    if (!m_validationResult.errors.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.3f, 0.3f, 1));
        for (const auto& error : m_validationResult.errors) {
            ImGui::BulletText("%s", error.c_str());
        }
        ImGui::PopStyleColor();
    }

    if (!m_validationResult.warnings.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.8f, 0.2f, 1));
        for (const auto& warning : m_validationResult.warnings) {
            ImGui::BulletText("%s", warning.c_str());
        }
        ImGui::PopStyleColor();
    }
}

bool PreferencesPanel::MatchesSearch(const std::string& label, const std::string& description) const {
    if (m_searchFilter.empty()) return true;

    std::string lowerLabel = label;
    std::string lowerDesc = description;
    std::string lowerSearch = m_searchFilter;

    std::transform(lowerLabel.begin(), lowerLabel.end(), lowerLabel.begin(), ::tolower);
    std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);
    std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);

    return lowerLabel.find(lowerSearch) != std::string::npos ||
           lowerDesc.find(lowerSearch) != std::string::npos;
}

void PreferencesPanel::ApplyChanges() {
    // Validate before applying
    ValidateSettings();
    if (!m_validationResult.valid) {
        m_showValidation = true;
        return;
    }

    // Apply to EditorSettings
    EditorSettings::Instance().SetGeneral(m_pendingSettings.general);
    EditorSettings::Instance().SetAppearance(m_pendingSettings.appearance);
    EditorSettings::Instance().SetViewport(m_pendingSettings.viewport);
    EditorSettings::Instance().SetInput(m_pendingSettings.input);
    EditorSettings::Instance().SetPerformance(m_pendingSettings.performance);
    EditorSettings::Instance().SetPaths(m_pendingSettings.paths);
    EditorSettings::Instance().SetPlugins(m_pendingSettings.plugins);

    // Save to file
    auto result = EditorSettings::Instance().Save();
    if (!result) {
        LOG_ERROR("Failed to save settings: {}", result.error());
    }

    m_hasChanges = false;

    // Notify callback
    if (m_onApplied) {
        m_onApplied();
    }
}

void PreferencesPanel::DiscardChanges() {
    // Revert to current settings
    m_pendingSettings = EditorSettings::Instance().GetSettings();
    m_hasChanges = false;
}

void PreferencesPanel::ResetCurrentCategory() {
    CompleteEditorSettings defaultSettings;

    switch (m_selectedCategory) {
        case SettingsCategory::General:
            m_pendingSettings.general = defaultSettings.general;
            break;
        case SettingsCategory::Appearance:
            m_pendingSettings.appearance = defaultSettings.appearance;
            break;
        case SettingsCategory::Viewport:
            m_pendingSettings.viewport = defaultSettings.viewport;
            break;
        case SettingsCategory::Input:
            m_pendingSettings.input = defaultSettings.input;
            break;
        case SettingsCategory::Performance:
            m_pendingSettings.performance = defaultSettings.performance;
            break;
        case SettingsCategory::Paths:
            m_pendingSettings.paths = defaultSettings.paths;
            break;
        case SettingsCategory::Plugins:
            m_pendingSettings.plugins = defaultSettings.plugins;
            break;
    }

    m_hasChanges = true;
}

void PreferencesPanel::ResetAllSettings() {
    m_pendingSettings = CompleteEditorSettings{};
    EditorSettings::Instance().Initialize();  // Re-initialize with defaults
    m_pendingSettings = EditorSettings::Instance().GetSettings();
    m_hasChanges = true;
}

void PreferencesPanel::ImportSettings() {
    std::string filepath;
    if (EditorWidgets::OpenFileDialog("Import Settings", filepath, "*.json")) {
        auto result = EditorSettings::Instance().Import(filepath, false);
        if (result) {
            m_pendingSettings = EditorSettings::Instance().GetSettings();
            m_hasChanges = false;
            EditorWidgets::ShowNotification("Settings Imported", "Settings imported successfully",
                                           EditorWidgets::NotificationType::Success);
        }
        else {
            EditorWidgets::ShowNotification("Import Failed", result.error().c_str(),
                                           EditorWidgets::NotificationType::Error);
        }
    }
}

void PreferencesPanel::ExportSettings() {
    std::string filepath;
    if (EditorWidgets::SaveFileDialog("Export Settings", filepath, "*.json", "editor_settings.json")) {
        auto result = EditorSettings::Instance().Export(filepath);
        if (result) {
            EditorWidgets::ShowNotification("Settings Exported", "Settings exported successfully",
                                           EditorWidgets::NotificationType::Success);
        }
        else {
            EditorWidgets::ShowNotification("Export Failed", result.error().c_str(),
                                           EditorWidgets::NotificationType::Error);
        }
    }
}

void PreferencesPanel::ValidateSettings() {
    // Create a temporary EditorSettings for validation
    // For now, do basic validation
    m_validationResult = SettingsValidationResult{};

    // Check font size
    if (m_pendingSettings.appearance.fontSize < 8.0f ||
        m_pendingSettings.appearance.fontSize > 32.0f) {
        m_validationResult.AddError("Font size must be between 8 and 32");
    }

    // Check grid size
    if (m_pendingSettings.viewport.gridSize <= 0.0f) {
        m_validationResult.AddError("Grid size must be positive");
    }

    // Check clip planes
    if (m_pendingSettings.viewport.nearClipPlane >= m_pendingSettings.viewport.farClipPlane) {
        m_validationResult.AddError("Near clip plane must be less than far clip plane");
    }

    // Check FOV
    if (m_pendingSettings.viewport.fieldOfView < 10.0f ||
        m_pendingSettings.viewport.fieldOfView > 170.0f) {
        m_validationResult.AddError("Field of view must be between 10 and 170 degrees");
    }

    // Check mouse sensitivity
    if (m_pendingSettings.input.mouseSensitivity <= 0.0f) {
        m_validationResult.AddError("Mouse sensitivity must be positive");
    }

    // Add warnings for unusual settings
    if (m_pendingSettings.general.undoHistorySize > 500) {
        m_validationResult.AddWarning("Large undo history may consume significant memory");
    }

    if (m_pendingSettings.performance.maxTextureSize < 512) {
        m_validationResult.AddWarning("Low max texture size may cause visual quality issues");
    }
}

void PreferencesPanel::CheckShortcutConflicts() {
    auto& shortcuts = m_pendingSettings.input.shortcuts;

    for (size_t i = 0; i < shortcuts.size(); ++i) {
        for (size_t j = i + 1; j < shortcuts.size(); ++j) {
            const auto& a = shortcuts[i];
            const auto& b = shortcuts[j];

            if (a.key == b.key && a.modifiers == b.modifiers && a.key != 0) {
                bool contextOverlap = (a.context == b.context) ||
                                     (a.context == ShortcutContext::Global) ||
                                     (b.context == ShortcutContext::Global);

                if (contextOverlap) {
                    m_conflictAction1 = a.action;
                    m_conflictAction2 = b.action;
                    m_showConflictDialog = true;
                    return;
                }
            }
        }
    }
}

// ==========================================================================
// Widget Helpers
// ==========================================================================

void PreferencesPanel::SectionHeader(const char* label) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::Text("%s", label);
    ImGui::PopStyleColor();
    ImGui::Separator();
}

bool PreferencesPanel::BeginSettingsGroup(const char* label, bool defaultOpen) {
    std::string key = label;
    if (m_expandedGroups.find(key) == m_expandedGroups.end()) {
        m_expandedGroups[key] = defaultOpen;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 6));
    bool open = ImGui::CollapsingHeader(label,
        m_expandedGroups[key] ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None);
    ImGui::PopStyleVar();

    m_expandedGroups[key] = open;

    if (open) {
        ImGui::Indent(16.0f);
    }

    return open;
}

void PreferencesPanel::EndSettingsGroup() {
    ImGui::Unindent(16.0f);
    ImGui::Spacing();
}

bool PreferencesPanel::BoolSetting(const char* label, bool& value, const char* tooltip) {
    if (!MatchesSearch(label, tooltip ? tooltip : "")) return false;

    bool changed = ImGui::Checkbox(label, &value);
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return changed;
}

bool PreferencesPanel::IntSetting(const char* label, int& value, int min, int max, const char* tooltip) {
    if (!MatchesSearch(label, tooltip ? tooltip : "")) return false;

    ImGui::PushItemWidth(150);
    bool changed = ImGui::SliderInt(label, &value, min, max);
    ImGui::PopItemWidth();

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return changed;
}

bool PreferencesPanel::FloatSetting(const char* label, float& value, float min, float max, const char* tooltip) {
    if (!MatchesSearch(label, tooltip ? tooltip : "")) return false;

    ImGui::PushItemWidth(150);
    bool changed = ImGui::SliderFloat(label, &value, min, max);
    ImGui::PopItemWidth();

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return changed;
}

bool PreferencesPanel::StringSetting(const char* label, std::string& value, const char* tooltip) {
    if (!MatchesSearch(label, tooltip ? tooltip : "")) return false;

    char buf[512];
    std::strncpy(buf, value.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    ImGui::PushItemWidth(250);
    bool changed = ImGui::InputText(label, buf, sizeof(buf));
    ImGui::PopItemWidth();

    if (changed) {
        value = buf;
    }

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return changed;
}

bool PreferencesPanel::PathSetting(const char* label, std::string& value, bool isFolder, const char* tooltip) {
    if (!MatchesSearch(label, tooltip ? tooltip : "")) return false;

    char buf[512];
    std::strncpy(buf, value.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    ImGui::PushItemWidth(200);
    bool changed = ImGui::InputText(label, buf, sizeof(buf));
    ImGui::PopItemWidth();

    if (changed) {
        value = buf;
    }

    ImGui::SameLine();
    std::string buttonId = std::string("...##") + label;
    if (ImGui::Button(buttonId.c_str())) {
        std::string newPath;
        bool success = false;
        if (isFolder) {
            success = EditorWidgets::FolderDialog("Select Folder", newPath);
        }
        else {
            success = EditorWidgets::OpenFileDialog("Select File", newPath);
        }
        if (success) {
            value = newPath;
            changed = true;
        }
    }

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return changed;
}

bool PreferencesPanel::ColorSetting(const char* label, glm::vec4& color, const char* tooltip) {
    if (!MatchesSearch(label, tooltip ? tooltip : "")) return false;

    float col[4] = {color.r, color.g, color.b, color.a};
    bool changed = ImGui::ColorEdit4(label, col, ImGuiColorEditFlags_AlphaBar);
    if (changed) {
        color = glm::vec4(col[0], col[1], col[2], col[3]);
    }

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return changed;
}

// =============================================================================
// PreferencesManager Implementation
// =============================================================================

PreferencesManager& PreferencesManager::Instance() {
    static PreferencesManager instance;
    return instance;
}

PreferencesManager::PreferencesManager() {
    m_panel = std::make_unique<PreferencesPanel>();

    EditorPanel::Config config;
    config.title = "Preferences";
    config.id = "preferences";
    m_panel->Initialize(config);
}

PreferencesManager::~PreferencesManager() {
    if (m_panel) {
        m_panel->Shutdown();
    }
}

void PreferencesManager::ShowPreferences() {
    if (m_panel) {
        m_panel->ShowModal();
    }
}

void PreferencesManager::HidePreferences() {
    if (m_panel) {
        m_panel->HideModal();
    }
}

void PreferencesManager::TogglePreferences() {
    if (m_panel) {
        if (m_panel->IsShowing()) {
            m_panel->HideModal();
        }
        else {
            m_panel->ShowModal();
        }
    }
}

bool PreferencesManager::IsShowing() const {
    return m_panel && m_panel->IsShowing();
}

void PreferencesManager::Update(float deltaTime) {
    if (m_panel) {
        m_panel->Update(deltaTime);
    }
}

void PreferencesManager::Render() {
    if (m_panel && m_panel->IsShowing()) {
        m_panel->Render();
    }
}

void PreferencesManager::ShowCategory(SettingsCategory category) {
    if (m_panel) {
        m_panel->SelectCategory(category);
        m_panel->ShowModal();
    }
}

void PreferencesManager::ShowSetting(const std::string& settingKey) {
    if (m_panel) {
        m_panel->FocusSetting(settingKey);
        m_panel->ShowModal();
    }
}

} // namespace Nova
