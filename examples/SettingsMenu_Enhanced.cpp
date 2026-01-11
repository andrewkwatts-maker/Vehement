#include "SettingsMenu_Enhanced.hpp"
#include "ModernUI.hpp"
#include "core/Window.hpp"
#include "config/Config.hpp"

#include <imgui.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <algorithm>

SettingsMenu::SettingsMenu() = default;

SettingsMenu::~SettingsMenu() {
    Shutdown();
}

void SettingsMenu::Initialize(Nova::InputManager* inputManager, Nova::Window* window) {
    m_inputManager = inputManager;
    m_window = window;

    // Initialize input rebinding system
    Nova::InputRebinding::Instance().Initialize(inputManager);

    // Load common resolutions
    UpdateAvailableResolutions();

    // Load current settings from engine
    auto& config = Nova::Config::Instance();

    // Graphics
    m_graphics.currentResolution.width = config.Get("window.width", 1920);
    m_graphics.currentResolution.height = config.Get("window.height", 1080);
    m_graphics.fullscreen = config.Get("window.fullscreen", false);
    m_graphics.vsync = config.Get("window.vsync", true);
    m_graphics.enableShadows = config.Get("render.enable_shadows", true);
    m_graphics.shadowQuality = config.Get("render.shadow_map_size", 2048) / 1024 - 1;
    m_graphics.enableHDR = config.Get("render.enable_hdr", false);
    m_graphics.enableBloom = config.Get("render.enable_bloom", true);
    m_graphics.enableSSAO = config.Get("render.enable_ssao", true);
    m_graphics.antiAliasing = config.Get("window.samples", 4);
    m_graphics.renderScale = config.Get("render.scale", 1.0f);

    // Audio
    m_audio.masterVolume = config.Get("audio.master_volume", 1.0f);
    m_audio.musicVolume = config.Get("audio.music_volume", 0.7f);
    m_audio.sfxVolume = config.Get("audio.sfx_volume", 1.0f);
    m_audio.ambientVolume = config.Get("audio.ambient_volume", 0.5f);
    m_audio.voiceVolume = config.Get("audio.voice_volume", 1.0f);
    m_audio.masterMute = config.Get("audio.master_mute", false);
    m_audio.musicMute = config.Get("audio.music_mute", false);
    m_audio.sfxMute = config.Get("audio.sfx_mute", false);

    // Game
    m_game.cameraSpeed = config.Get("camera.move_speed", 10.0f);
    m_game.cameraRotationSpeed = config.Get("camera.rotation_speed", 2.0f);
    m_game.mouseSensitivity = config.Get("input.mouse_sensitivity", 1.0f);
    m_game.invertMouseY = config.Get("input.invert_y", false);
    m_game.fov = config.Get("camera.fov", 45.0f);
    m_game.edgeScrolling = config.Get("camera.edge_scrolling", true);
    m_game.edgeScrollingSpeed = config.Get("camera.edge_scroll_speed", 5.0f);
    m_game.showFPS = config.Get("debug.show_fps", true);
    m_game.showMinimap = config.Get("ui.show_minimap", true);
    m_game.showTooltips = config.Get("ui.show_tooltips", true);
    m_game.tooltipDelay = config.Get("ui.tooltip_delay", 0.5f);
    m_game.uiScale = config.Get("ui.scale", 1.0f);
    m_game.maxFPS = config.Get("render.max_fps", 0);
    m_game.pauseOnLostFocus = config.Get("game.pause_on_focus_loss", true);

    // Camera Settings (NEW)
    m_cameraSettings.sensitivity = config.Get("camera.sensitivity", 1.0f);
    m_cameraSettings.invertY = config.Get("camera.invert_y", false);
    m_cameraSettings.edgeScrolling = config.Get("camera.edge_scrolling_enabled", true);
    m_cameraSettings.edgeScrollSpeed = config.Get("camera.edge_scroll_speed_multiplier", 1.0f);
    m_cameraSettings.zoomSpeed = config.Get("camera.zoom_speed", 1.0f);
    m_cameraSettings.zoomMin = config.Get("camera.zoom_min", 10.0f);
    m_cameraSettings.zoomMax = config.Get("camera.zoom_max", 100.0f);

    // Editor Settings (NEW)
    m_editorSettings.autoSaveEnabled = config.Get("editor.auto_save_enabled", true);
    m_editorSettings.autoSaveInterval = config.Get("editor.auto_save_interval", 5);

    // Find current resolution index
    for (size_t i = 0; i < m_availableResolutions.size(); ++i) {
        if (m_availableResolutions[i] == m_graphics.currentResolution) {
            m_selectedResolutionIndex = static_cast<int>(i);
            break;
        }
    }

    // Cache original settings
    m_originalGraphics = m_graphics;
    m_originalAudio = m_audio;
    m_originalGame = m_game;
    m_originalCameraSettings = m_cameraSettings;
    m_originalEditorSettings = m_editorSettings;

    ClearModifiedFlag();
    spdlog::info("Settings menu initialized (version {})", CONFIG_VERSION);
}

void SettingsMenu::Shutdown() {
    m_inputManager = nullptr;
    m_window = nullptr;
}

void SettingsMenu::Render(bool* isOpen) {
    if (!isOpen || !*isOpen) return;

    // Check for pending close with unsaved changes
    if (m_pendingClose) {
        m_pendingClose = false;
        if (m_hasUnsavedChanges) {
            m_showUnsavedDialog = true;
        } else {
            *isOpen = false;
            return;
        }
    }

    ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Settings", isOpen, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    // Check if window is being closed
    if (!*isOpen && m_hasUnsavedChanges) {
        *isOpen = true; // Keep open
        m_pendingClose = true;
    }

    // Update rebinding system
    Nova::InputRebinding::Instance().Update();

    // Render tab bar
    RenderTabBar();

    ModernUI::GradientSeparator();

    // Render content based on selected tab
    ImGui::BeginChild("SettingsContent", ImVec2(0, -50), false);
    {
        switch (m_currentTab) {
            case SettingsTab::Input:
                RenderInputSettings();
                break;
            case SettingsTab::Graphics:
                RenderGraphicsSettings();
                break;
            case SettingsTab::Audio:
                RenderAudioSettings();
                break;
            case SettingsTab::Game:
                RenderGameSettings();
                break;
        }
    }
    ImGui::EndChild();

    ModernUI::GradientSeparator();

    // Render control buttons
    RenderControlButtons();

    // Render dialogs
    if (m_showConflictDialog) {
        ImGui::OpenPopup("Binding Conflict");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Binding Conflict", &m_showConflictDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", m_currentConflict.GetMessage().c_str());
            ImGui::Spacing();
            ImGui::Text("Do you want to replace the existing binding?");
            ImGui::Spacing();

            float buttonWidth = 120.0f;
            float totalWidth = buttonWidth * 2 + 10.0f;
            float offsetX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

            if (ModernUI::GlowButton("Replace", ImVec2(buttonWidth, 0))) {
                Nova::InputRebinding::Instance().SetBinding(
                    m_currentConflict.newAction,
                    m_currentConflict.binding,
                    true
                );
                m_showConflictDialog = false;
                MarkAsModified();
            }
            ImGui::SameLine();
            if (ModernUI::GlowButton("Cancel", ImVec2(buttonWidth, 0))) {
                m_showConflictDialog = false;
            }

            ImGui::EndPopup();
        }
    }

    RenderUnsavedChangesDialog();
    RenderValidationWarningDialog();

    ImGui::End();
}

void SettingsMenu::RenderTabBar() {
    if (ImGui::BeginTabBar("SettingsTabs")) {
        if (ImGui::TabItemButton("Input")) {
            m_currentTab = SettingsTab::Input;
        }
        if (ImGui::TabItemButton("Graphics")) {
            m_currentTab = SettingsTab::Graphics;
        }
        if (ImGui::TabItemButton("Audio")) {
            m_currentTab = SettingsTab::Audio;
        }
        if (ImGui::TabItemButton("Game")) {
            m_currentTab = SettingsTab::Game;
        }
        ImGui::EndTabBar();
    }
}

void SettingsMenu::RenderInputSettings() {
    ModernUI::GradientText("Input Controls");
    ImGui::Spacing();

    // Device selection
    ImGui::Text("Configure inputs for:");
    ImGui::SameLine();

    const char* deviceNames[] = { "Keyboard", "Mouse", "Gamepad" };
    int currentDevice = static_cast<int>(m_selectedInputDevice);
    if (ImGui::Combo("##InputDevice", &currentDevice, deviceNames, 3)) {
        m_selectedInputDevice = static_cast<Nova::InputDevice>(currentDevice);
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Select which input device to configure");
    }

    ImGui::Spacing();
    ModernUI::GradientSeparator();
    ImGui::Spacing();

    // Get all categories
    auto categories = Nova::InputRebinding::Instance().GetCategories();

    // Render each category
    for (const auto& category : categories) {
        RenderInputCategory(category);
        ImGui::Spacing();
    }

    // Sensitivity settings
    ImGui::Spacing();
    ModernUI::GradientSeparator();
    ModernUI::GradientText("Sensitivity Settings");
    ImGui::Spacing();

    auto& rebinding = Nova::InputRebinding::Instance();

    if (m_selectedInputDevice == Nova::InputDevice::Mouse) {
        float mouseSens = rebinding.GetMouseSensitivity();
        if (ImGui::SliderFloat("Mouse Sensitivity", &mouseSens, 0.1f, 3.0f)) {
            rebinding.SetMouseSensitivity(mouseSens);
            MarkAsModified();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Adjust mouse movement sensitivity (0.1 - 3.0)");
        }

        bool invertY = rebinding.GetInvertMouseY();
        if (ImGui::Checkbox("Invert Y Axis", &invertY)) {
            rebinding.SetInvertMouseY(invertY);
            MarkAsModified();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Invert vertical mouse movement");
        }
    } else if (m_selectedInputDevice == Nova::InputDevice::Gamepad) {
        float sensX = rebinding.GetGamepadSensitivityX();
        if (ImGui::SliderFloat("Gamepad Sensitivity X", &sensX, 0.1f, 3.0f)) {
            rebinding.SetGamepadSensitivityX(sensX);
            MarkAsModified();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Horizontal gamepad stick sensitivity");
        }

        float sensY = rebinding.GetGamepadSensitivityY();
        if (ImGui::SliderFloat("Gamepad Sensitivity Y", &sensY, 0.1f, 3.0f)) {
            rebinding.SetGamepadSensitivityY(sensY);
            MarkAsModified();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Vertical gamepad stick sensitivity");
        }

        float deadzone = rebinding.GetGamepadDeadzone();
        if (ImGui::SliderFloat("Deadzone", &deadzone, 0.0f, 0.5f)) {
            rebinding.SetGamepadDeadzone(deadzone);
            MarkAsModified();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Minimum stick movement before registering input");
        }

        bool invertGamepadY = rebinding.GetInvertGamepadY();
        if (ImGui::Checkbox("Invert Y Axis##Gamepad", &invertGamepadY)) {
            rebinding.SetInvertGamepadY(invertGamepadY);
            MarkAsModified();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Invert vertical gamepad stick movement");
        }
    }

    ImGui::Spacing();
    ModernUI::GradientSeparator();
    ImGui::Spacing();

    // Reset button for this tab
    if (ModernUI::GlowButton("Reset Input to Defaults", ImVec2(200, 0))) {
        Nova::InputRebinding::Instance().ResetToDefaults();
        MarkAsModified();
        spdlog::info("Input settings reset to defaults");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reset all input bindings and sensitivity to default values");
    }
}

void SettingsMenu::RenderInputCategory(const std::string& category) {
    if (ModernUI::GradientHeader(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        auto actions = Nova::InputRebinding::Instance().GetActionsByCategory(category);

        ImGui::Indent();
        for (const auto* action : actions) {
            RenderActionBinding(*action);
        }
        ImGui::Unindent();
    }
}

void SettingsMenu::RenderActionBinding(const Nova::ActionDefinition& action) {
    ImGui::PushID(action.name.c_str());

    // Action name
    ImGui::Text("%s", action.displayName.c_str());
    ImGui::SameLine(300);

    // Get bindings for current device
    const std::vector<Nova::ExtendedBinding>* bindings = nullptr;
    switch (m_selectedInputDevice) {
        case Nova::InputDevice::Keyboard:
            bindings = &action.keyboardBindings;
            break;
        case Nova::InputDevice::Mouse:
            bindings = &action.mouseBindings;
            break;
        case Nova::InputDevice::Gamepad:
            bindings = &action.gamepadBindings;
            break;
    }

    if (bindings) {
        RenderBindingButton(action.name, m_selectedInputDevice, *bindings);
    }

    ImGui::PopID();
}

void SettingsMenu::RenderBindingButton(const std::string& actionName, Nova::InputDevice device,
                                       const std::vector<Nova::ExtendedBinding>& bindings) {
    // Display current binding(s)
    std::string bindingText = "None";
    if (!bindings.empty()) {
        bindingText = bindings[0].GetDisplayString();
        if (bindings.size() > 1) {
            bindingText += " (+" + std::to_string(bindings.size() - 1) + " more)";
        }
    }

    // Check if this action is currently being rebound
    bool isRebinding = Nova::InputRebinding::Instance().IsRebinding() &&
                      m_rebindingActionName == actionName &&
                      m_rebindingDevice == device;

    if (isRebinding) {
        bindingText = "Press any key...";
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.0f, 1.0f));
    }

    if (ModernUI::GlowButton(bindingText.c_str(), ImVec2(250, 0))) {
        // Start rebinding
        m_rebindingActionName = actionName;
        m_rebindingDevice = device;
        Nova::InputRebinding::Instance().StartRebinding(actionName, device, this);
    }

    if (isRebinding) {
        ImGui::PopStyleColor();
    }

    // Context menu for additional options
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Clear Binding")) {
            Nova::InputRebinding::Instance().ClearBindings(actionName, device);
            MarkAsModified();
        }
        if (ImGui::MenuItem("Reset to Default")) {
            Nova::InputRebinding::Instance().ResetActionToDefault(actionName);
            MarkAsModified();
        }
        ImGui::EndPopup();
    }
}

void SettingsMenu::RenderGraphicsSettings() {
    ModernUI::GradientText("Graphics Settings");
    ImGui::Spacing();

    auto defaults = GraphicsSettings::Default();

    // Resolution
    ImGui::Text("Resolution:");
    ImGui::SameLine(220);
    std::vector<const char*> resolutionStrings;
    std::vector<std::string> resStrings;
    for (const auto& res : m_availableResolutions) {
        resStrings.push_back(res.ToString());
    }
    for (const auto& str : resStrings) {
        resolutionStrings.push_back(str.c_str());
    }

    if (ImGui::Combo("##Resolution", &m_selectedResolutionIndex,
                     resolutionStrings.data(), static_cast<int>(resolutionStrings.size()))) {
        m_graphics.currentResolution = m_availableResolutions[m_selectedResolutionIndex];
        MarkAsModified();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Screen resolution (requires restart for some systems)");
    }

    // Show indicator if differs from default
    if (m_graphics.currentResolution.width != defaults.currentResolution.width ||
        m_graphics.currentResolution.height != defaults.currentResolution.height) {
        ImGui::SameLine();
        ImGui::TextColored(ModernUI::Gold, "*");
    }

    // Display mode
    ImGui::Text("Display Mode:");
    ImGui::SameLine(220);
    bool fullscreenChanged = ImGui::Checkbox("Fullscreen", &m_graphics.fullscreen);
    if (fullscreenChanged) {
        MarkAsModified();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle fullscreen mode");
    }
    if (m_graphics.fullscreen != defaults.fullscreen) {
        ImGui::SameLine();
        ImGui::TextColored(ModernUI::Gold, "*");
    }

    // VSync
    ImGui::Text("Vertical Sync:");
    ImGui::SameLine(220);
    if (ImGui::Checkbox("VSync", &m_graphics.vsync)) {
        MarkAsModified();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Synchronize frame rate with monitor refresh rate");
    }
    if (m_graphics.vsync != defaults.vsync) {
        ImGui::SameLine();
        ImGui::TextColored(ModernUI::Gold, "*");
    }

    ImGui::Spacing();
    ModernUI::GradientSeparator();
    ModernUI::GradientText("Quality Settings");
    ImGui::Spacing();

    // Quality preset
    const char* presetNames[] = { "Low", "Medium", "High", "Ultra", "Custom" };
    int currentPreset = static_cast<int>(m_graphics.qualityPreset);
    ImGui::Text("Preset:");
    ImGui::SameLine(220);
    if (ImGui::Combo("##QualityPreset", &currentPreset, presetNames, 5)) {
        m_graphics.qualityPreset = static_cast<QualityPreset>(currentPreset);
        MarkAsModified();

        // Apply preset values
        switch (m_graphics.qualityPreset) {
            case QualityPreset::Low:
                m_graphics.enableShadows = false;
                m_graphics.shadowQuality = 0;
                m_graphics.enableHDR = false;
                m_graphics.enableBloom = false;
                m_graphics.enableSSAO = false;
                m_graphics.antiAliasing = 0;
                m_graphics.renderScale = 0.75f;
                break;
            case QualityPreset::Medium:
                m_graphics.enableShadows = true;
                m_graphics.shadowQuality = 1;
                m_graphics.enableHDR = false;
                m_graphics.enableBloom = true;
                m_graphics.enableSSAO = false;
                m_graphics.antiAliasing = 2;
                m_graphics.renderScale = 1.0f;
                break;
            case QualityPreset::High:
                m_graphics.enableShadows = true;
                m_graphics.shadowQuality = 2;
                m_graphics.enableHDR = true;
                m_graphics.enableBloom = true;
                m_graphics.enableSSAO = true;
                m_graphics.antiAliasing = 4;
                m_graphics.renderScale = 1.0f;
                break;
            case QualityPreset::Ultra:
                m_graphics.enableShadows = true;
                m_graphics.shadowQuality = 3;
                m_graphics.enableHDR = true;
                m_graphics.enableBloom = true;
                m_graphics.enableSSAO = true;
                m_graphics.antiAliasing = 8;
                m_graphics.renderScale = 1.0f;
                break;
            case QualityPreset::Custom:
                // Don't change anything, user customizes
                break;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Apply quality preset or customize individual settings");
    }

    ImGui::Spacing();
    ModernUI::GradientSeparator();
    ModernUI::GradientText("Advanced Settings");
    ImGui::Spacing();

    // Individual quality settings
    bool settingChanged = false;

    ImGui::Text("Shadows:");
    ImGui::SameLine(220);
    settingChanged |= ImGui::Checkbox("##Shadows", &m_graphics.enableShadows);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enable dynamic shadows");
    }

    if (m_graphics.enableShadows) {
        ImGui::Text("Shadow Quality:");
        ImGui::SameLine(220);
        const char* shadowQualityNames[] = { "Low", "Medium", "High", "Ultra" };
        settingChanged |= ImGui::Combo("##ShadowQuality", &m_graphics.shadowQuality, shadowQualityNames, 4);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Shadow map resolution quality");
        }
    }

    ImGui::Text("HDR:");
    ImGui::SameLine(220);
    settingChanged |= ImGui::Checkbox("##HDR", &m_graphics.enableHDR);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("High Dynamic Range rendering");
    }

    ImGui::Text("Bloom:");
    ImGui::SameLine(220);
    settingChanged |= ImGui::Checkbox("##Bloom", &m_graphics.enableBloom);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Glow effect for bright areas");
    }

    ImGui::Text("SSAO:");
    ImGui::SameLine(220);
    settingChanged |= ImGui::Checkbox("##SSAO", &m_graphics.enableSSAO);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Screen Space Ambient Occlusion (subtle shadows in crevices)");
    }

    ImGui::Text("Anti-Aliasing:");
    ImGui::SameLine(220);
    const char* aaNames[] = { "Off", "2x MSAA", "4x MSAA", "8x MSAA", "16x MSAA" };
    int aaIndex = m_graphics.antiAliasing == 0 ? 0 :
                 m_graphics.antiAliasing == 2 ? 1 :
                 m_graphics.antiAliasing == 4 ? 2 :
                 m_graphics.antiAliasing == 8 ? 3 : 4;
    if (ImGui::Combo("##AA", &aaIndex, aaNames, 5)) {
        m_graphics.antiAliasing = aaIndex == 0 ? 0 :
                                 aaIndex == 1 ? 2 :
                                 aaIndex == 2 ? 4 :
                                 aaIndex == 3 ? 8 : 16;
        settingChanged = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Multi-Sample Anti-Aliasing (smooths jagged edges)");
    }

    ImGui::Text("Render Scale:");
    ImGui::SameLine(220);
    settingChanged |= ImGui::SliderFloat("##RenderScale", &m_graphics.renderScale, 0.5f, 2.0f, "%.2f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Internal resolution multiplier (< 1.0 improves performance, > 1.0 improves quality)");
    }

    if (settingChanged) {
        m_graphics.qualityPreset = QualityPreset::Custom;
        MarkAsModified();
    }

    ImGui::Spacing();
    ModernUI::GradientSeparator();
    ImGui::Spacing();

    // Reset button for this tab
    if (ModernUI::GlowButton("Reset Graphics to Defaults", ImVec2(220, 0))) {
        m_graphics = GraphicsSettings::Default();
        MarkAsModified();
        spdlog::info("Graphics settings reset to defaults");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reset all graphics settings to default values");
    }
}

void SettingsMenu::RenderAudioSettings() {
    ModernUI::GradientText("Audio Settings");
    ImGui::Spacing();

    auto defaults = AudioSettings::Default();

    // Master volume
    ImGui::Text("Master Volume:");
    ImGui::SameLine(220);
    ImGui::PushItemWidth(200);
    if (ImGui::SliderFloat("##MasterVolume", &m_audio.masterVolume, 0.0f, 1.0f, "%.2f")) {
        MarkAsModified();
    }
    ImGui::PopItemWidth();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Overall volume level (0.0 - 1.0)");
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Mute##Master", &m_audio.masterMute)) {
        MarkAsModified();
    }
    if (m_audio.masterVolume != defaults.masterVolume || m_audio.masterMute != defaults.masterMute) {
        ImGui::SameLine();
        ImGui::TextColored(ModernUI::Gold, "*");
    }

    // Music volume
    ImGui::Text("Music Volume:");
    ImGui::SameLine(220);
    ImGui::PushItemWidth(200);
    if (ImGui::SliderFloat("##MusicVolume", &m_audio.musicVolume, 0.0f, 1.0f, "%.2f")) {
        MarkAsModified();
    }
    ImGui::PopItemWidth();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Background music volume");
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Mute##Music", &m_audio.musicMute)) {
        MarkAsModified();
    }
    if (m_audio.musicVolume != defaults.musicVolume || m_audio.musicMute != defaults.musicMute) {
        ImGui::SameLine();
        ImGui::TextColored(ModernUI::Gold, "*");
    }

    // SFX volume
    ImGui::Text("Sound Effects:");
    ImGui::SameLine(220);
    ImGui::PushItemWidth(200);
    if (ImGui::SliderFloat("##SFXVolume", &m_audio.sfxVolume, 0.0f, 1.0f, "%.2f")) {
        MarkAsModified();
    }
    ImGui::PopItemWidth();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Sound effects volume");
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Mute##SFX", &m_audio.sfxMute)) {
        MarkAsModified();
    }
    if (m_audio.sfxVolume != defaults.sfxVolume || m_audio.sfxMute != defaults.sfxMute) {
        ImGui::SameLine();
        ImGui::TextColored(ModernUI::Gold, "*");
    }

    // Ambient volume
    ImGui::Text("Ambient Volume:");
    ImGui::SameLine(220);
    ImGui::PushItemWidth(200);
    if (ImGui::SliderFloat("##AmbientVolume", &m_audio.ambientVolume, 0.0f, 1.0f, "%.2f")) {
        MarkAsModified();
    }
    ImGui::PopItemWidth();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Environmental ambient sounds");
    }
    if (m_audio.ambientVolume != defaults.ambientVolume) {
        ImGui::SameLine();
        ImGui::TextColored(ModernUI::Gold, "*");
    }

    // Voice volume
    ImGui::Text("Voice Volume:");
    ImGui::SameLine(220);
    ImGui::PushItemWidth(200);
    if (ImGui::SliderFloat("##VoiceVolume", &m_audio.voiceVolume, 0.0f, 1.0f, "%.2f")) {
        MarkAsModified();
    }
    ImGui::PopItemWidth();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Voice-over and dialogue volume");
    }
    if (m_audio.voiceVolume != defaults.voiceVolume) {
        ImGui::SameLine();
        ImGui::TextColored(ModernUI::Gold, "*");
    }

    ImGui::Spacing();
    ModernUI::GradientSeparator();
    ImGui::Spacing();

    // Test audio button
    if (ModernUI::GlowButton("Test Audio", ImVec2(150, 0))) {
        spdlog::info("Audio test - Master: {:.2f} (mute: {}), Music: {:.2f} (mute: {}), SFX: {:.2f} (mute: {})",
                    m_audio.masterVolume, m_audio.masterMute,
                    m_audio.musicVolume, m_audio.musicMute,
                    m_audio.sfxVolume, m_audio.sfxMute);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Play test sound to verify audio settings");
    }

    ImGui::SameLine();

    // Reset button for this tab
    if (ModernUI::GlowButton("Reset Audio to Defaults", ImVec2(200, 0))) {
        m_audio = AudioSettings::Default();
        MarkAsModified();
        spdlog::info("Audio settings reset to defaults");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reset all audio settings to default values");
    }
}

void SettingsMenu::RenderGameSettings() {
    ModernUI::GradientText("Game Settings");
    ImGui::Spacing();

    auto gameDefaults = GameSettings::Default();
    auto cameraDefaults = CameraSettings::Default();
    auto editorDefaults = EditorSettings::Default();

    // ========== Camera Settings Section ==========
    if (ModernUI::GradientHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        ImGui::Text("Camera Speed:");
        ImGui::SameLine(220);
        ImGui::PushItemWidth(300);
        if (ImGui::SliderFloat("##CameraSpeed", &m_game.cameraSpeed, 1.0f, 50.0f, "%.1f")) {
            MarkAsModified();
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Camera movement speed (1.0 - 50.0)");
        }
        if (m_game.cameraSpeed != gameDefaults.cameraSpeed) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        ImGui::Text("Rotation Speed:");
        ImGui::SameLine(220);
        ImGui::PushItemWidth(300);
        if (ImGui::SliderFloat("##RotationSpeed", &m_game.cameraRotationSpeed, 0.5f, 10.0f, "%.1f")) {
            MarkAsModified();
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Camera rotation speed (0.5 - 10.0)");
        }
        if (m_game.cameraRotationSpeed != gameDefaults.cameraRotationSpeed) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        ImGui::Text("Camera Sensitivity:");
        ImGui::SameLine(220);
        ImGui::PushItemWidth(300);
        if (ImGui::SliderFloat("##CameraSensitivity", &m_cameraSettings.sensitivity, 0.1f, 5.0f, "%.1f")) {
            MarkAsModified();
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Mouse camera sensitivity (0.1 - 5.0)");
        }
        if (m_cameraSettings.sensitivity != cameraDefaults.sensitivity) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        ImGui::Text("Invert Y Axis:");
        ImGui::SameLine(220);
        if (ImGui::Checkbox("##InvertY", &m_cameraSettings.invertY)) {
            MarkAsModified();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Invert vertical camera movement");
        }
        if (m_cameraSettings.invertY != cameraDefaults.invertY) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        ImGui::Text("Field of View:");
        ImGui::SameLine(220);
        ImGui::PushItemWidth(300);
        if (ImGui::SliderFloat("##FOV", &m_game.fov, 30.0f, 120.0f, "%.0f")) {
            MarkAsModified();
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Camera field of view in degrees (30 - 120)");
        }
        if (m_game.fov != gameDefaults.fov) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        ImGui::Unindent();
        ImGui::Spacing();
    }

    // ========== RTS Camera Controls Section ==========
    if (ModernUI::GradientHeader("RTS Camera Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        ImGui::Text("Edge Scrolling:");
        ImGui::SameLine(220);
        if (ImGui::Checkbox("##EdgeScrolling", &m_cameraSettings.edgeScrolling)) {
            MarkAsModified();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Move camera when mouse reaches screen edge");
        }
        if (m_cameraSettings.edgeScrolling != cameraDefaults.edgeScrolling) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        if (m_cameraSettings.edgeScrolling) {
            ImGui::Text("Edge Scroll Speed:");
            ImGui::SameLine(220);
            ImGui::PushItemWidth(300);
            if (ImGui::SliderFloat("##EdgeScrollSpeed", &m_cameraSettings.edgeScrollSpeed, 0.5f, 3.0f, "%.1f")) {
                MarkAsModified();
            }
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Speed multiplier for edge scrolling (0.5 - 3.0)");
            }
            if (m_cameraSettings.edgeScrollSpeed != cameraDefaults.edgeScrollSpeed) {
                ImGui::SameLine();
                ImGui::TextColored(ModernUI::Gold, "*");
            }
        }

        ImGui::Text("Zoom Speed:");
        ImGui::SameLine(220);
        ImGui::PushItemWidth(300);
        if (ImGui::SliderFloat("##ZoomSpeed", &m_cameraSettings.zoomSpeed, 0.5f, 3.0f, "%.1f")) {
            MarkAsModified();
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Camera zoom speed multiplier (0.5 - 3.0)");
        }
        if (m_cameraSettings.zoomSpeed != cameraDefaults.zoomSpeed) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        ImGui::Text("Zoom Min Distance:");
        ImGui::SameLine(220);
        ImGui::PushItemWidth(300);
        if (ImGui::SliderFloat("##ZoomMin", &m_cameraSettings.zoomMin, 5.0f, 50.0f, "%.0f")) {
            // Ensure min doesn't exceed max
            if (m_cameraSettings.zoomMin > m_cameraSettings.zoomMax) {
                m_cameraSettings.zoomMin = m_cameraSettings.zoomMax;
            }
            MarkAsModified();
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Closest zoom distance (5 - 50)");
        }
        if (m_cameraSettings.zoomMin != cameraDefaults.zoomMin) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        ImGui::Text("Zoom Max Distance:");
        ImGui::SameLine(220);
        ImGui::PushItemWidth(300);
        if (ImGui::SliderFloat("##ZoomMax", &m_cameraSettings.zoomMax, 50.0f, 200.0f, "%.0f")) {
            // Ensure max doesn't go below min
            if (m_cameraSettings.zoomMax < m_cameraSettings.zoomMin) {
                m_cameraSettings.zoomMax = m_cameraSettings.zoomMin;
            }
            MarkAsModified();
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Farthest zoom distance (50 - 200)");
        }
        if (m_cameraSettings.zoomMax != cameraDefaults.zoomMax) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        ImGui::Unindent();
        ImGui::Spacing();
    }

    // ========== Editor Settings Section ==========
    if (ModernUI::GradientHeader("Editor Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        ImGui::Text("Auto-Save:");
        ImGui::SameLine(220);
        if (ImGui::Checkbox("##AutoSave", &m_editorSettings.autoSaveEnabled)) {
            MarkAsModified();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Automatically save project at regular intervals");
        }
        if (m_editorSettings.autoSaveEnabled != editorDefaults.autoSaveEnabled) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        if (m_editorSettings.autoSaveEnabled) {
            ImGui::Text("Auto-Save Interval:");
            ImGui::SameLine(220);
            ImGui::PushItemWidth(300);
            if (ImGui::SliderInt("##AutoSaveInterval", &m_editorSettings.autoSaveInterval, 1, 30, "%d minutes")) {
                MarkAsModified();
            }
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Time between auto-saves in minutes (1 - 30)");
            }
            if (m_editorSettings.autoSaveInterval != editorDefaults.autoSaveInterval) {
                ImGui::SameLine();
                ImGui::TextColored(ModernUI::Gold, "*");
            }
        }

        ImGui::Unindent();
        ImGui::Spacing();
    }

    // ========== UI Settings Section ==========
    if (ModernUI::GradientHeader("UI Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        ImGui::Text("Show Tooltips:");
        ImGui::SameLine(220);
        if (ImGui::Checkbox("##ShowTooltips", &m_game.showTooltips)) {
            MarkAsModified();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Display helpful tooltips when hovering over UI elements");
        }
        if (m_game.showTooltips != gameDefaults.showTooltips) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        if (m_game.showTooltips) {
            ImGui::Text("Tooltip Delay:");
            ImGui::SameLine(220);
            ImGui::PushItemWidth(300);
            if (ImGui::SliderFloat("##TooltipDelay", &m_game.tooltipDelay, 0.0f, 2.0f, "%.1f s")) {
                MarkAsModified();
            }
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Delay before showing tooltips (0.0 - 2.0 seconds)");
            }
            if (m_game.tooltipDelay != gameDefaults.tooltipDelay) {
                ImGui::SameLine();
                ImGui::TextColored(ModernUI::Gold, "*");
            }
        }

        ImGui::Text("Show FPS:");
        ImGui::SameLine(220);
        if (ImGui::Checkbox("##ShowFPS", &m_game.showFPS)) {
            MarkAsModified();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Display frames per second counter");
        }
        if (m_game.showFPS != gameDefaults.showFPS) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        ImGui::Text("Show Minimap:");
        ImGui::SameLine(220);
        if (ImGui::Checkbox("##ShowMinimap", &m_game.showMinimap)) {
            MarkAsModified();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Display minimap on screen");
        }
        if (m_game.showMinimap != gameDefaults.showMinimap) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        ImGui::Text("UI Scale:");
        ImGui::SameLine(220);
        ImGui::PushItemWidth(300);
        if (ImGui::SliderFloat("##UIScale", &m_game.uiScale, 0.5f, 2.0f, "%.2f")) {
            MarkAsModified();
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("User interface scale (0.5 - 2.0)");
        }
        if (m_game.uiScale != gameDefaults.uiScale) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        ImGui::Unindent();
        ImGui::Spacing();
    }

    // ========== Performance Settings Section ==========
    if (ModernUI::GradientHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        ImGui::Text("Max FPS:");
        ImGui::SameLine(220);
        ImGui::PushItemWidth(300);
        if (ImGui::SliderInt("##MaxFPS", &m_game.maxFPS, 0, 300)) {
            MarkAsModified();
        }
        ImGui::PopItemWidth();
        if (m_game.maxFPS == 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("(Unlimited)");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Maximum frame rate limit (0 = unlimited)");
        }
        if (m_game.maxFPS != gameDefaults.maxFPS) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        ImGui::Text("Pause on Focus Loss:");
        ImGui::SameLine(220);
        if (ImGui::Checkbox("##PauseOnFocusLoss", &m_game.pauseOnLostFocus)) {
            MarkAsModified();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Pause game when window loses focus");
        }
        if (m_game.pauseOnLostFocus != gameDefaults.pauseOnLostFocus) {
            ImGui::SameLine();
            ImGui::TextColored(ModernUI::Gold, "*");
        }

        ImGui::Unindent();
        ImGui::Spacing();
    }

    ModernUI::GradientSeparator();
    ImGui::Spacing();

    // Reset button for this tab
    if (ModernUI::GlowButton("Reset Game Settings to Defaults", ImVec2(250, 0))) {
        ResetCurrentTabToDefaults();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reset all game settings to default values");
    }
}

void SettingsMenu::RenderControlButtons() {
    float buttonWidth = 120.0f;
    float spacing = 10.0f;
    float totalWidth = buttonWidth * 3 + spacing * 2;
    float offsetX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

    // Apply button
    if (ModernUI::GlowButton("Apply", ImVec2(buttonWidth, 0))) {
        if (ValidateSettings()) {
            ApplySettings();
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Apply settings without saving to file");
    }

    ImGui::SameLine();

    // Save button
    if (ModernUI::GlowButton("Save", ImVec2(buttonWidth, 0))) {
        if (ValidateSettings()) {
            SaveSettings("config/settings.json");
            ApplySettings();
            ClearModifiedFlag();
            spdlog::info("Settings saved");
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Save settings to file and apply them");
    }

    ImGui::SameLine();

    // Reset to defaults button
    if (ModernUI::GlowButton("Reset All", ImVec2(buttonWidth, 0))) {
        ImGui::OpenPopup("ResetConfirm");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reset ALL settings to default values");
    }

    // Confirmation popup
    if (ImGui::BeginPopupModal("ResetConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Reset all settings to default values?");
        ImGui::Text("This will affect Input, Graphics, Audio, and Game settings.");
        ImGui::Spacing();

        float confirmButtonWidth = 120.0f;
        float confirmTotalWidth = confirmButtonWidth * 2 + 10.0f;
        float confirmOffsetX = (ImGui::GetContentRegionAvail().x - confirmTotalWidth) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + confirmOffsetX);

        if (ModernUI::GlowButton("Yes", ImVec2(confirmButtonWidth, 0))) {
            ResetToDefaults();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ModernUI::GlowButton("No", ImVec2(confirmButtonWidth, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Show unsaved changes indicator
    if (m_hasUnsavedChanges) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "* Unsaved changes");
    }
}

void SettingsMenu::RenderUnsavedChangesDialog() {
    if (m_showUnsavedDialog) {
        ImGui::OpenPopup("Unsaved Changes");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Unsaved Changes", &m_showUnsavedDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
            ModernUI::GradientText("You have unsaved changes.");
            ImGui::Spacing();
            ImGui::Text("Do you want to apply them before closing?");
            ImGui::Spacing();

            float buttonWidth = 150.0f;
            float totalWidth = buttonWidth * 3 + 20.0f;
            float offsetX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

            if (ModernUI::GlowButton("Apply and Close", ImVec2(buttonWidth, 0))) {
                if (ValidateSettings()) {
                    ApplySettings();
                    SaveSettings("config/settings.json");
                    m_showUnsavedDialog = false;
                    ClearModifiedFlag();
                    // Signal to close (caller should handle)
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Save and apply all changes before closing");
            }

            ImGui::SameLine();
            if (ModernUI::GlowButton("Discard and Close", ImVec2(buttonWidth, 0))) {
                m_showUnsavedDialog = false;
                ClearModifiedFlag();
                // Restore original settings
                m_graphics = m_originalGraphics;
                m_audio = m_originalAudio;
                m_game = m_originalGame;
                m_cameraSettings = m_originalCameraSettings;
                m_editorSettings = m_originalEditorSettings;
                spdlog::info("Settings discarded");
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Close without saving changes");
            }

            ImGui::SameLine();
            if (ModernUI::GlowButton("Cancel", ImVec2(buttonWidth, 0))) {
                m_showUnsavedDialog = false;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Return to settings menu");
            }

            ImGui::EndPopup();
        }
    }
}

void SettingsMenu::RenderValidationWarningDialog() {
    if (m_showValidationWarning) {
        ImGui::OpenPopup("Invalid Settings");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Invalid Settings", &m_showValidationWarning, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Warning: Invalid Settings");
            ImGui::Spacing();
            ImGui::TextWrapped("%s", m_validationMessage.c_str());
            ImGui::Spacing();

            float buttonWidth = 120.0f;
            float offsetX = (ImGui::GetContentRegionAvail().x - buttonWidth) * 0.5f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

            if (ModernUI::GlowButton("OK", ImVec2(buttonWidth, 0))) {
                m_showValidationWarning = false;
            }

            ImGui::EndPopup();
        }
    }
}

void SettingsMenu::UpdateAvailableResolutions() {
    m_availableResolutions.clear();

    // Get monitor video modes from GLFW
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor) {
        int count;
        const GLFWvidmode* modes = glfwGetVideoModes(monitor, &count);

        // Add unique resolutions
        for (int i = 0; i < count; ++i) {
            Resolution res{modes[i].width, modes[i].height};

            // Only add common aspect ratios and skip duplicates
            bool isDuplicate = false;
            for (const auto& existing : m_availableResolutions) {
                if (existing == res) {
                    isDuplicate = true;
                    break;
                }
            }

            if (!isDuplicate && res.width >= 800 && res.height >= 600) {
                m_availableResolutions.push_back(res);
            }
        }
    }

    // Add common resolutions if not already present
    std::vector<Resolution> commonResolutions = {
        {1920, 1080}, {2560, 1440}, {3840, 2160},
        {1680, 1050}, {1600, 900}, {1366, 768},
        {1280, 720}, {1024, 768}, {800, 600}
    };

    for (const auto& res : commonResolutions) {
        bool exists = false;
        for (const auto& existing : m_availableResolutions) {
            if (existing == res) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            m_availableResolutions.push_back(res);
        }
    }

    // Sort by width, then height
    std::sort(m_availableResolutions.begin(), m_availableResolutions.end(),
        [](const Resolution& a, const Resolution& b) {
            if (a.width != b.width) return a.width > b.width;
            return a.height > b.height;
        });
}

bool SettingsMenu::ValidateSettings() {
    if (!ValidateGraphicsSettings()) return false;
    if (!ValidateAudioSettings()) return false;
    if (!ValidateGameSettings()) return false;
    return true;
}

bool SettingsMenu::ValidateGraphicsSettings() {
    // Validate resolution
    if (m_graphics.currentResolution.width < 800 || m_graphics.currentResolution.height < 600) {
        ShowValidationWarning("Resolution must be at least 800x600 pixels.");
        return false;
    }

    // Validate render scale
    if (m_graphics.renderScale < 0.5f || m_graphics.renderScale > 2.0f) {
        ShowValidationWarning("Render scale must be between 0.5 and 2.0.");
        return false;
    }

    return true;
}

bool SettingsMenu::ValidateAudioSettings() {
    // Validate volume ranges
    if (m_audio.masterVolume < 0.0f || m_audio.masterVolume > 1.0f) {
        ShowValidationWarning("Master volume must be between 0.0 and 1.0.");
        return false;
    }
    if (m_audio.musicVolume < 0.0f || m_audio.musicVolume > 1.0f) {
        ShowValidationWarning("Music volume must be between 0.0 and 1.0.");
        return false;
    }
    if (m_audio.sfxVolume < 0.0f || m_audio.sfxVolume > 1.0f) {
        ShowValidationWarning("SFX volume must be between 0.0 and 1.0.");
        return false;
    }

    return true;
}

bool SettingsMenu::ValidateGameSettings() {
    // Validate camera zoom ranges
    if (m_cameraSettings.zoomMin < 5.0f || m_cameraSettings.zoomMin > 50.0f) {
        ShowValidationWarning("Zoom minimum must be between 5 and 50.");
        return false;
    }
    if (m_cameraSettings.zoomMax < 50.0f || m_cameraSettings.zoomMax > 200.0f) {
        ShowValidationWarning("Zoom maximum must be between 50 and 200.");
        return false;
    }
    if (m_cameraSettings.zoomMin > m_cameraSettings.zoomMax) {
        ShowValidationWarning("Zoom minimum cannot be greater than zoom maximum.");
        return false;
    }

    // Validate auto-save interval
    if (m_editorSettings.autoSaveInterval < 1 || m_editorSettings.autoSaveInterval > 30) {
        ShowValidationWarning("Auto-save interval must be between 1 and 30 minutes.");
        return false;
    }

    // Validate FOV
    if (m_game.fov < 30.0f || m_game.fov > 120.0f) {
        ShowValidationWarning("Field of view must be between 30 and 120 degrees.");
        return false;
    }

    return true;
}

void SettingsMenu::ShowValidationWarning(const std::string& message) {
    m_validationMessage = message;
    m_showValidationWarning = true;
    spdlog::warn("Settings validation failed: {}", message);
}

void SettingsMenu::ApplySettings() {
    ApplyGraphicsSettings();
    ApplyAudioSettings();
    ApplyGameSettings();
    ApplyCameraSettings();
    ApplyEditorSettings();

    // Cache as original settings
    m_originalGraphics = m_graphics;
    m_originalAudio = m_audio;
    m_originalGame = m_game;
    m_originalCameraSettings = m_cameraSettings;
    m_originalEditorSettings = m_editorSettings;

    spdlog::info("Settings applied");
}

void SettingsMenu::ApplyGraphicsSettings() {
    if (!m_window) return;

    auto& config = Nova::Config::Instance();

    // Apply resolution and fullscreen
    if (m_window->GetWidth() != m_graphics.currentResolution.width ||
        m_window->GetHeight() != m_graphics.currentResolution.height ||
        m_window->IsFullscreen() != m_graphics.fullscreen) {

        m_window->SetFullscreen(m_graphics.fullscreen);
        // NOTE: Full resolution change requires window recreation in many cases
        // This should be handled by the main application
        spdlog::info("Graphics settings changed - resolution: {}x{}, fullscreen: {}",
                    m_graphics.currentResolution.width,
                    m_graphics.currentResolution.height,
                    m_graphics.fullscreen);
        spdlog::warn("Resolution changes may require application restart");
    }

    // Apply VSync
    m_window->SetVSync(m_graphics.vsync);

    // Save to config
    config.Set("window.width", m_graphics.currentResolution.width);
    config.Set("window.height", m_graphics.currentResolution.height);
    config.Set("window.fullscreen", m_graphics.fullscreen);
    config.Set("window.vsync", m_graphics.vsync);
    config.Set("render.enable_shadows", m_graphics.enableShadows);
    config.Set("render.shadow_map_size", (m_graphics.shadowQuality + 1) * 1024);
    config.Set("render.enable_hdr", m_graphics.enableHDR);
    config.Set("render.enable_bloom", m_graphics.enableBloom);
    config.Set("render.enable_ssao", m_graphics.enableSSAO);
    config.Set("window.samples", m_graphics.antiAliasing);
    config.Set("render.scale", m_graphics.renderScale);
}

void SettingsMenu::ApplyAudioSettings() {
    auto& config = Nova::Config::Instance();

    config.Set("audio.master_volume", m_audio.masterVolume);
    config.Set("audio.music_volume", m_audio.musicVolume);
    config.Set("audio.sfx_volume", m_audio.sfxVolume);
    config.Set("audio.ambient_volume", m_audio.ambientVolume);
    config.Set("audio.voice_volume", m_audio.voiceVolume);
    config.Set("audio.master_mute", m_audio.masterMute);
    config.Set("audio.music_mute", m_audio.musicMute);
    config.Set("audio.sfx_mute", m_audio.sfxMute);

    // TODO: Apply to actual audio system when available
    spdlog::info("Audio settings applied - Master: {:.2f} (mute: {}), Music: {:.2f} (mute: {}), SFX: {:.2f} (mute: {})",
                m_audio.masterVolume, m_audio.masterMute,
                m_audio.musicVolume, m_audio.musicMute,
                m_audio.sfxVolume, m_audio.sfxMute);
}

void SettingsMenu::ApplyGameSettings() {
    auto& config = Nova::Config::Instance();

    config.Set("camera.move_speed", m_game.cameraSpeed);
    config.Set("camera.rotation_speed", m_game.cameraRotationSpeed);
    config.Set("input.mouse_sensitivity", m_game.mouseSensitivity);
    config.Set("input.invert_y", m_game.invertMouseY);
    config.Set("camera.fov", m_game.fov);
    config.Set("camera.edge_scrolling", m_game.edgeScrolling);
    config.Set("camera.edge_scroll_speed", m_game.edgeScrollingSpeed);
    config.Set("debug.show_fps", m_game.showFPS);
    config.Set("ui.show_minimap", m_game.showMinimap);
    config.Set("ui.show_tooltips", m_game.showTooltips);
    config.Set("ui.tooltip_delay", m_game.tooltipDelay);
    config.Set("ui.scale", m_game.uiScale);
    config.Set("render.max_fps", m_game.maxFPS);
    config.Set("game.pause_on_focus_loss", m_game.pauseOnLostFocus);

    spdlog::info("Game settings applied");
}

void SettingsMenu::ApplyCameraSettings() {
    auto& config = Nova::Config::Instance();

    config.Set("camera.sensitivity", m_cameraSettings.sensitivity);
    config.Set("camera.invert_y", m_cameraSettings.invertY);
    config.Set("camera.edge_scrolling_enabled", m_cameraSettings.edgeScrolling);
    config.Set("camera.edge_scroll_speed_multiplier", m_cameraSettings.edgeScrollSpeed);
    config.Set("camera.zoom_speed", m_cameraSettings.zoomSpeed);
    config.Set("camera.zoom_min", m_cameraSettings.zoomMin);
    config.Set("camera.zoom_max", m_cameraSettings.zoomMax);

    spdlog::info("Camera settings applied - Sensitivity: {:.1f}, Zoom: {:.0f}-{:.0f}",
                m_cameraSettings.sensitivity, m_cameraSettings.zoomMin, m_cameraSettings.zoomMax);
}

void SettingsMenu::ApplyEditorSettings() {
    auto& config = Nova::Config::Instance();

    config.Set("editor.auto_save_enabled", m_editorSettings.autoSaveEnabled);
    config.Set("editor.auto_save_interval", m_editorSettings.autoSaveInterval);

    spdlog::info("Editor settings applied - Auto-save: {} (interval: {} minutes)",
                m_editorSettings.autoSaveEnabled, m_editorSettings.autoSaveInterval);
}

bool SettingsMenu::LoadSettings(const std::string& filepath) {
    auto& config = Nova::Config::Instance();

    if (auto result = config.Load(filepath); !result) {
        spdlog::error("Failed to load settings from {}", filepath);
        return false;
    }

    // Reload settings from config
    Initialize(m_inputManager, m_window);

    // Load input bindings
    Nova::InputRebinding::Instance().LoadBindings(filepath);

    spdlog::info("Settings loaded from {} (version {})", filepath, CONFIG_VERSION);
    return true;
}

bool SettingsMenu::SaveSettings(const std::string& filepath) {
    auto& config = Nova::Config::Instance();

    // Add version to config
    config.Set("settings_version", CONFIG_VERSION);

    // Apply current settings to config
    ApplyGraphicsSettings();
    ApplyAudioSettings();
    ApplyGameSettings();
    ApplyCameraSettings();
    ApplyEditorSettings();

    // Save config
    if (auto result = config.Save(filepath); !result) {
        spdlog::error("Failed to save settings to {}", filepath);
        return false;
    }

    // Save input bindings
    Nova::InputRebinding::Instance().SaveBindings(filepath);

    spdlog::info("Settings saved to {} (version {})", filepath, CONFIG_VERSION);
    return true;
}

void SettingsMenu::ResetToDefaults() {
    // Reset graphics
    m_graphics = GraphicsSettings::Default();

    // Reset audio
    m_audio = AudioSettings::Default();

    // Reset game settings
    m_game = GameSettings::Default();

    // Reset camera settings
    m_cameraSettings = CameraSettings::Default();

    // Reset editor settings
    m_editorSettings = EditorSettings::Default();

    // Reset input bindings
    Nova::InputRebinding::Instance().ResetToDefaults();

    MarkAsModified();
    spdlog::info("All settings reset to defaults");
}

void SettingsMenu::ResetCurrentTabToDefaults() {
    switch (m_currentTab) {
        case SettingsTab::Input:
            Nova::InputRebinding::Instance().ResetToDefaults();
            spdlog::info("Input settings reset to defaults");
            break;
        case SettingsTab::Graphics:
            m_graphics = GraphicsSettings::Default();
            spdlog::info("Graphics settings reset to defaults");
            break;
        case SettingsTab::Audio:
            m_audio = AudioSettings::Default();
            spdlog::info("Audio settings reset to defaults");
            break;
        case SettingsTab::Game:
            m_game = GameSettings::Default();
            m_cameraSettings = CameraSettings::Default();
            m_editorSettings = EditorSettings::Default();
            spdlog::info("Game settings reset to defaults");
            break;
    }
    MarkAsModified();
}

// IRebindingListener implementation
void SettingsMenu::OnRebindStarted(const std::string& actionName, Nova::InputDevice device) {
    spdlog::info("Rebinding started: {} ({})", actionName, Nova::InputDeviceToString(device));
}

void SettingsMenu::OnRebindCompleted(const std::string& actionName, const Nova::ExtendedBinding& binding) {
    spdlog::info("Rebinding completed: {} -> {}", actionName, binding.GetDisplayString());
    MarkAsModified();
}

void SettingsMenu::OnRebindCancelled(const std::string& actionName) {
    spdlog::info("Rebinding cancelled: {}", actionName);
}

void SettingsMenu::OnBindingConflict(const Nova::BindingConflict& conflict) {
    m_currentConflict = conflict;
    m_showConflictDialog = true;
    spdlog::warn("Binding conflict: {}", conflict.GetMessage());
}

std::string SettingsMenu::QualityPresetToString(QualityPreset preset) const {
    switch (preset) {
        case QualityPreset::Low: return "Low";
        case QualityPreset::Medium: return "Medium";
        case QualityPreset::High: return "High";
        case QualityPreset::Ultra: return "Ultra";
        case QualityPreset::Custom: return "Custom";
    }
    return "Unknown";
}
