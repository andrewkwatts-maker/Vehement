#include "SettingsMenu.hpp"
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
    m_graphics.antiAliasing = config.Get("window.samples", 4);

    // Audio
    m_audio.masterVolume = config.Get("audio.master_volume", 1.0f);
    m_audio.musicVolume = config.Get("audio.music_volume", 0.7f);
    m_audio.sfxVolume = config.Get("audio.sfx_volume", 1.0f);
    m_audio.ambientVolume = config.Get("audio.ambient_volume", 0.5f);

    // Game
    m_game.cameraSpeed = config.Get("camera.move_speed", 10.0f);
    m_game.mouseSensitivity = config.Get("input.mouse_sensitivity", 1.0f);
    m_game.invertMouseY = config.Get("input.invert_y", false);
    m_game.fov = config.Get("camera.fov", 45.0f);
    m_game.showFPS = config.Get("debug.show_fps", true);

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

    ClearModifiedFlag();
    spdlog::info("Settings menu initialized");
}

void SettingsMenu::Shutdown() {
    m_inputManager = nullptr;
    m_window = nullptr;
}

void SettingsMenu::Render(bool* isOpen) {
    if (!isOpen || !*isOpen) return;

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Settings", isOpen, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    // Update rebinding system
    Nova::InputRebinding::Instance().Update();

    // Render tab bar
    RenderTabBar();

    ImGui::Separator();

    // Render content based on selected tab
    ImGui::BeginChild("SettingsContent", ImVec2(0, -40), false);
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

    ImGui::Separator();

    // Render control buttons
    RenderControlButtons();

    // Render conflict dialog if needed
    if (m_showConflictDialog) {
        ImGui::OpenPopup("Binding Conflict");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Binding Conflict", &m_showConflictDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", m_currentConflict.GetMessage().c_str());
            ImGui::Spacing();
            ImGui::Text("Do you want to replace the existing binding?");
            ImGui::Spacing();

            if (ImGui::Button("Replace", ImVec2(120, 0))) {
                // Remove the conflict and apply the new binding
                Nova::InputRebinding::Instance().SetBinding(
                    m_currentConflict.newAction,
                    m_currentConflict.binding,
                    true // removeConflicts
                );
                m_showConflictDialog = false;
                MarkAsModified();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_showConflictDialog = false;
            }

            ImGui::EndPopup();
        }
    }

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
    ImGui::Text("Input Controls");
    ImGui::Spacing();

    // Device selection
    ImGui::Text("Configure inputs for:");
    ImGui::SameLine();

    const char* deviceNames[] = { "Keyboard", "Mouse", "Gamepad" };
    int currentDevice = static_cast<int>(m_selectedInputDevice);
    if (ImGui::Combo("##InputDevice", &currentDevice, deviceNames, 3)) {
        m_selectedInputDevice = static_cast<Nova::InputDevice>(currentDevice);
    }

    ImGui::Spacing();
    ImGui::Separator();
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
    ImGui::Separator();
    ImGui::Text("Sensitivity Settings");
    ImGui::Spacing();

    auto& rebinding = Nova::InputRebinding::Instance();

    if (m_selectedInputDevice == Nova::InputDevice::Mouse) {
        float mouseSens = rebinding.GetMouseSensitivity();
        if (ImGui::SliderFloat("Mouse Sensitivity", &mouseSens, 0.1f, 3.0f)) {
            rebinding.SetMouseSensitivity(mouseSens);
            MarkAsModified();
        }

        bool invertY = rebinding.GetInvertMouseY();
        if (ImGui::Checkbox("Invert Y Axis", &invertY)) {
            rebinding.SetInvertMouseY(invertY);
            MarkAsModified();
        }
    } else if (m_selectedInputDevice == Nova::InputDevice::Gamepad) {
        float sensX = rebinding.GetGamepadSensitivityX();
        if (ImGui::SliderFloat("Gamepad Sensitivity X", &sensX, 0.1f, 3.0f)) {
            rebinding.SetGamepadSensitivityX(sensX);
            MarkAsModified();
        }

        float sensY = rebinding.GetGamepadSensitivityY();
        if (ImGui::SliderFloat("Gamepad Sensitivity Y", &sensY, 0.1f, 3.0f)) {
            rebinding.SetGamepadSensitivityY(sensY);
            MarkAsModified();
        }

        float deadzone = rebinding.GetGamepadDeadzone();
        if (ImGui::SliderFloat("Deadzone", &deadzone, 0.0f, 0.5f)) {
            rebinding.SetGamepadDeadzone(deadzone);
            MarkAsModified();
        }

        bool invertGamepadY = rebinding.GetInvertGamepadY();
        if (ImGui::Checkbox("Invert Y Axis", &invertGamepadY)) {
            rebinding.SetInvertGamepadY(invertGamepadY);
            MarkAsModified();
        }
    }
}

void SettingsMenu::RenderInputCategory(const std::string& category) {
    if (ImGui::CollapsingHeader(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
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
    ImGui::SameLine(250);

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

    if (ImGui::Button(bindingText.c_str(), ImVec2(200, 0))) {
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
    ImGui::Text("Graphics Settings");
    ImGui::Spacing();

    // Resolution
    ImGui::Text("Resolution:");
    ImGui::SameLine(200);
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

    // Display mode
    ImGui::Text("Display Mode:");
    ImGui::SameLine(200);
    if (ImGui::Checkbox("Fullscreen", &m_graphics.fullscreen)) {
        MarkAsModified();
    }

    // VSync
    ImGui::Text("Vertical Sync:");
    ImGui::SameLine(200);
    if (ImGui::Checkbox("VSync", &m_graphics.vsync)) {
        MarkAsModified();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Quality Settings");
    ImGui::Spacing();

    // Quality preset
    const char* presetNames[] = { "Low", "Medium", "High", "Ultra", "Custom" };
    int currentPreset = static_cast<int>(m_graphics.qualityPreset);
    ImGui::Text("Preset:");
    ImGui::SameLine(200);
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

    ImGui::Spacing();
    ImGui::Text("Advanced Settings");
    ImGui::Spacing();

    // Individual quality settings
    bool settingChanged = false;

    ImGui::Text("Shadows:");
    ImGui::SameLine(200);
    settingChanged |= ImGui::Checkbox("##Shadows", &m_graphics.enableShadows);

    if (m_graphics.enableShadows) {
        ImGui::Text("Shadow Quality:");
        ImGui::SameLine(200);
        const char* shadowQualityNames[] = { "Low", "Medium", "High", "Ultra" };
        settingChanged |= ImGui::Combo("##ShadowQuality", &m_graphics.shadowQuality, shadowQualityNames, 4);
    }

    ImGui::Text("HDR:");
    ImGui::SameLine(200);
    settingChanged |= ImGui::Checkbox("##HDR", &m_graphics.enableHDR);

    ImGui::Text("Bloom:");
    ImGui::SameLine(200);
    settingChanged |= ImGui::Checkbox("##Bloom", &m_graphics.enableBloom);

    ImGui::Text("SSAO:");
    ImGui::SameLine(200);
    settingChanged |= ImGui::Checkbox("##SSAO", &m_graphics.enableSSAO);

    ImGui::Text("Anti-Aliasing:");
    ImGui::SameLine(200);
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

    ImGui::Text("Render Scale:");
    ImGui::SameLine(200);
    settingChanged |= ImGui::SliderFloat("##RenderScale", &m_graphics.renderScale, 0.5f, 2.0f, "%.2f");

    if (settingChanged) {
        m_graphics.qualityPreset = QualityPreset::Custom;
        MarkAsModified();
    }
}

void SettingsMenu::RenderAudioSettings() {
    ImGui::Text("Audio Settings");
    ImGui::Spacing();

    // Master volume
    ImGui::Text("Master Volume:");
    ImGui::SameLine(200);
    if (ImGui::SliderFloat("##MasterVolume", &m_audio.masterVolume, 0.0f, 1.0f, "%.2f")) {
        MarkAsModified();
    }

    // Music volume
    ImGui::Text("Music Volume:");
    ImGui::SameLine(200);
    if (ImGui::SliderFloat("##MusicVolume", &m_audio.musicVolume, 0.0f, 1.0f, "%.2f")) {
        MarkAsModified();
    }

    // SFX volume
    ImGui::Text("Sound Effects:");
    ImGui::SameLine(200);
    if (ImGui::SliderFloat("##SFXVolume", &m_audio.sfxVolume, 0.0f, 1.0f, "%.2f")) {
        MarkAsModified();
    }

    // Ambient volume
    ImGui::Text("Ambient Volume:");
    ImGui::SameLine(200);
    if (ImGui::SliderFloat("##AmbientVolume", &m_audio.ambientVolume, 0.0f, 1.0f, "%.2f")) {
        MarkAsModified();
    }

    // Voice volume
    ImGui::Text("Voice Volume:");
    ImGui::SameLine(200);
    if (ImGui::SliderFloat("##VoiceVolume", &m_audio.voiceVolume, 0.0f, 1.0f, "%.2f")) {
        MarkAsModified();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Test audio button
    if (ImGui::Button("Test Audio")) {
        spdlog::info("Audio test - Master: {:.2f}, Music: {:.2f}, SFX: {:.2f}",
                    m_audio.masterVolume, m_audio.musicVolume, m_audio.sfxVolume);
    }
}

void SettingsMenu::RenderGameSettings() {
    ImGui::Text("Game Settings");
    ImGui::Spacing();

    // Camera settings
    ImGui::Text("Camera Settings");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Camera Speed:");
    ImGui::SameLine(200);
    if (ImGui::SliderFloat("##CameraSpeed", &m_game.cameraSpeed, 1.0f, 50.0f, "%.1f")) {
        MarkAsModified();
    }

    ImGui::Text("Rotation Speed:");
    ImGui::SameLine(200);
    if (ImGui::SliderFloat("##RotationSpeed", &m_game.cameraRotationSpeed, 0.5f, 10.0f, "%.1f")) {
        MarkAsModified();
    }

    ImGui::Text("Field of View:");
    ImGui::SameLine(200);
    if (ImGui::SliderFloat("##FOV", &m_game.fov, 30.0f, 120.0f, "%.0f")) {
        MarkAsModified();
    }

    ImGui::Spacing();
    ImGui::Text("RTS Controls");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Edge Scrolling:");
    ImGui::SameLine(200);
    if (ImGui::Checkbox("##EdgeScrolling", &m_game.edgeScrolling)) {
        MarkAsModified();
    }

    if (m_game.edgeScrolling) {
        ImGui::Text("Edge Scroll Speed:");
        ImGui::SameLine(200);
        if (ImGui::SliderFloat("##EdgeScrollSpeed", &m_game.edgeScrollingSpeed, 1.0f, 20.0f, "%.1f")) {
            MarkAsModified();
        }
    }

    ImGui::Spacing();
    ImGui::Text("UI Settings");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Show Tooltips:");
    ImGui::SameLine(200);
    if (ImGui::Checkbox("##ShowTooltips", &m_game.showTooltips)) {
        MarkAsModified();
    }

    if (m_game.showTooltips) {
        ImGui::Text("Tooltip Delay:");
        ImGui::SameLine(200);
        if (ImGui::SliderFloat("##TooltipDelay", &m_game.tooltipDelay, 0.0f, 2.0f, "%.1f s")) {
            MarkAsModified();
        }
    }

    ImGui::Text("Show FPS:");
    ImGui::SameLine(200);
    if (ImGui::Checkbox("##ShowFPS", &m_game.showFPS)) {
        MarkAsModified();
    }

    ImGui::Text("Show Minimap:");
    ImGui::SameLine(200);
    if (ImGui::Checkbox("##ShowMinimap", &m_game.showMinimap)) {
        MarkAsModified();
    }

    ImGui::Text("UI Scale:");
    ImGui::SameLine(200);
    if (ImGui::SliderFloat("##UIScale", &m_game.uiScale, 0.5f, 2.0f, "%.2f")) {
        MarkAsModified();
    }

    ImGui::Spacing();
    ImGui::Text("Performance");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Max FPS:");
    ImGui::SameLine(200);
    if (ImGui::SliderInt("##MaxFPS", &m_game.maxFPS, 0, 300)) {
        MarkAsModified();
    }
    if (m_game.maxFPS == 0) {
        ImGui::SameLine();
        ImGui::TextDisabled("(Unlimited)");
    }

    ImGui::Text("Pause on Focus Loss:");
    ImGui::SameLine(200);
    if (ImGui::Checkbox("##PauseOnFocusLoss", &m_game.pauseOnLostFocus)) {
        MarkAsModified();
    }
}

void SettingsMenu::RenderControlButtons() {
    float buttonWidth = 120.0f;
    float spacing = 10.0f;
    float totalWidth = buttonWidth * 3 + spacing * 2;
    float offsetX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

    // Apply button
    if (ImGui::Button("Apply", ImVec2(buttonWidth, 0))) {
        ApplySettings();
    }

    ImGui::SameLine();

    // Save button
    if (ImGui::Button("Save", ImVec2(buttonWidth, 0))) {
        SaveSettings("config/settings.json");
        ApplySettings();
        ClearModifiedFlag();
        spdlog::info("Settings saved");
    }

    ImGui::SameLine();

    // Reset to defaults button
    if (ImGui::Button("Reset to Default", ImVec2(buttonWidth, 0))) {
        ImGui::OpenPopup("ResetConfirm");
    }

    // Confirmation popup
    if (ImGui::BeginPopupModal("ResetConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Reset all settings to default values?");
        ImGui::Spacing();

        if (ImGui::Button("Yes", ImVec2(120, 0))) {
            ResetToDefaults();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Show unsaved changes indicator
    if (m_hasUnsavedChanges) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Unsaved changes");
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

void SettingsMenu::ApplySettings() {
    ApplyGraphicsSettings();
    ApplyAudioSettings();
    ApplyGameSettings();

    // Cache as original settings
    m_originalGraphics = m_graphics;
    m_originalAudio = m_audio;
    m_originalGame = m_game;

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
        // Note: Resolution change requires window recreation in many cases
        spdlog::info("Graphics settings changed - resolution: {}x{}, fullscreen: {}",
                    m_graphics.currentResolution.width,
                    m_graphics.currentResolution.height,
                    m_graphics.fullscreen);
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
    config.Set("window.samples", m_graphics.antiAliasing);
}

void SettingsMenu::ApplyAudioSettings() {
    auto& config = Nova::Config::Instance();

    config.Set("audio.master_volume", m_audio.masterVolume);
    config.Set("audio.music_volume", m_audio.musicVolume);
    config.Set("audio.sfx_volume", m_audio.sfxVolume);
    config.Set("audio.ambient_volume", m_audio.ambientVolume);

    // Apply to audio system (would be implemented in audio engine)
    spdlog::info("Audio settings applied");
}

void SettingsMenu::ApplyGameSettings() {
    auto& config = Nova::Config::Instance();

    config.Set("camera.move_speed", m_game.cameraSpeed);
    config.Set("input.mouse_sensitivity", m_game.mouseSensitivity);
    config.Set("input.invert_y", m_game.invertMouseY);
    config.Set("camera.fov", m_game.fov);
    config.Set("debug.show_fps", m_game.showFPS);

    spdlog::info("Game settings applied");
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

    spdlog::info("Settings loaded from {}", filepath);
    return true;
}

bool SettingsMenu::SaveSettings(const std::string& filepath) {
    auto& config = Nova::Config::Instance();

    // Apply current settings to config
    ApplyGraphicsSettings();
    ApplyAudioSettings();
    ApplyGameSettings();

    // Save config
    if (auto result = config.Save(filepath); !result) {
        spdlog::error("Failed to save settings to {}", filepath);
        return false;
    }

    // Save input bindings
    Nova::InputRebinding::Instance().SaveBindings(filepath);

    spdlog::info("Settings saved to {}", filepath);
    return true;
}

void SettingsMenu::ResetToDefaults() {
    // Reset graphics
    m_graphics.currentResolution = {1920, 1080};
    m_graphics.fullscreen = false;
    m_graphics.vsync = true;
    m_graphics.qualityPreset = QualityPreset::High;
    m_graphics.enableShadows = true;
    m_graphics.shadowQuality = 2;
    m_graphics.enableHDR = false;
    m_graphics.enableBloom = true;
    m_graphics.enableSSAO = true;
    m_graphics.antiAliasing = 4;
    m_graphics.renderScale = 1.0f;

    // Reset audio
    m_audio.masterVolume = 1.0f;
    m_audio.musicVolume = 0.7f;
    m_audio.sfxVolume = 1.0f;
    m_audio.ambientVolume = 0.5f;
    m_audio.voiceVolume = 1.0f;

    // Reset game settings
    m_game.cameraSpeed = 10.0f;
    m_game.cameraRotationSpeed = 2.0f;
    m_game.edgeScrolling = true;
    m_game.edgeScrollingSpeed = 5.0f;
    m_game.showTooltips = true;
    m_game.tooltipDelay = 0.5f;
    m_game.showFPS = true;
    m_game.showMinimap = true;
    m_game.pauseOnLostFocus = true;
    m_game.mouseSensitivity = 1.0f;
    m_game.invertMouseY = false;
    m_game.fov = 45.0f;
    m_game.uiScale = 1.0f;
    m_game.maxFPS = 0;

    // Reset input bindings
    Nova::InputRebinding::Instance().ResetToDefaults();

    MarkAsModified();
    spdlog::info("Settings reset to defaults");
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
