#pragma once

#include "input/InputManager.hpp"
#include "input/InputRebinding.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Nova {
    class Window;
    class Config;
}

/**
 * @brief Settings menu tabs
 */
enum class SettingsTab {
    Input,
    Graphics,
    Audio,
    Game
};

/**
 * @brief Resolution option
 */
struct Resolution {
    int width;
    int height;

    std::string ToString() const {
        return std::to_string(width) + "x" + std::to_string(height);
    }

    bool operator==(const Resolution& other) const {
        return width == other.width && height == other.height;
    }
};

/**
 * @brief Quality preset
 */
enum class QualityPreset {
    Low,
    Medium,
    High,
    Ultra,
    Custom
};

/**
 * @brief Settings menu system with tabbed interface
 *
 * Provides comprehensive settings UI for:
 * - Input rebinding (keyboard/mouse/gamepad)
 * - Graphics settings (resolution, fullscreen, vsync, quality)
 * - Audio settings (volume controls)
 * - Game settings (camera, UI preferences)
 */
class SettingsMenu : public Nova::IRebindingListener {
public:
    SettingsMenu();
    ~SettingsMenu();

    /**
     * @brief Initialize settings menu
     * @param inputManager Reference to input manager
     * @param window Reference to window
     */
    void Initialize(Nova::InputManager* inputManager, Nova::Window* window);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Render the settings menu using ImGui
     */
    void Render(bool* isOpen);

    /**
     * @brief Load settings from config file
     */
    bool LoadSettings(const std::string& filepath);

    /**
     * @brief Save settings to config file
     */
    bool SaveSettings(const std::string& filepath);

    /**
     * @brief Apply all current settings to engine systems
     */
    void ApplySettings();

    /**
     * @brief Reset all settings to default values
     */
    void ResetToDefaults();

    /**
     * @brief Check if settings have been modified
     */
    bool HasUnsavedChanges() const { return m_hasUnsavedChanges; }

    // IRebindingListener implementation
    void OnRebindStarted(const std::string& actionName, Nova::InputDevice device) override;
    void OnRebindCompleted(const std::string& actionName, const Nova::ExtendedBinding& binding) override;
    void OnRebindCancelled(const std::string& actionName) override;
    void OnBindingConflict(const Nova::BindingConflict& conflict) override;

private:
    void RenderTabBar();
    void RenderInputSettings();
    void RenderGraphicsSettings();
    void RenderAudioSettings();
    void RenderGameSettings();
    void RenderControlButtons();

    // Input settings helpers
    void RenderInputCategory(const std::string& category);
    void RenderActionBinding(const Nova::ActionDefinition& action);
    void RenderBindingButton(const std::string& actionName, Nova::InputDevice device,
                            const std::vector<Nova::ExtendedBinding>& bindings);

    // Graphics settings helpers
    void UpdateAvailableResolutions();
    void ApplyGraphicsSettings();
    std::string QualityPresetToString(QualityPreset preset) const;

    // Audio settings helpers
    void ApplyAudioSettings();

    // Game settings helpers
    void ApplyGameSettings();

    // State management
    void MarkAsModified() { m_hasUnsavedChanges = true; }
    void ClearModifiedFlag() { m_hasUnsavedChanges = false; }

    // Systems
    Nova::InputManager* m_inputManager = nullptr;
    Nova::Window* m_window = nullptr;

    // UI State
    SettingsTab m_currentTab = SettingsTab::Input;
    bool m_hasUnsavedChanges = false;

    // Input Settings State
    Nova::InputDevice m_selectedInputDevice = Nova::InputDevice::Keyboard;
    std::string m_rebindingActionName;
    Nova::InputDevice m_rebindingDevice = Nova::InputDevice::Keyboard;
    bool m_showConflictDialog = false;
    Nova::BindingConflict m_currentConflict;

    // Graphics Settings
    struct GraphicsSettings {
        Resolution currentResolution{1920, 1080};
        bool fullscreen = false;
        bool vsync = true;
        QualityPreset qualityPreset = QualityPreset::High;

        // Advanced graphics options
        bool enableShadows = true;
        int shadowQuality = 2; // 0=Low, 1=Medium, 2=High
        bool enableHDR = false;
        bool enableBloom = true;
        bool enableSSAO = true;
        int antiAliasing = 4; // MSAA samples: 0, 2, 4, 8, 16
        float renderScale = 1.0f; // Internal resolution scale
    } m_graphics;

    std::vector<Resolution> m_availableResolutions;
    int m_selectedResolutionIndex = 0;

    // Audio Settings
    struct AudioSettings {
        float masterVolume = 1.0f;
        float musicVolume = 0.7f;
        float sfxVolume = 1.0f;
        float ambientVolume = 0.5f;
        float voiceVolume = 1.0f;
    } m_audio;

    // Game Settings
    struct GameSettings {
        float cameraSpeed = 10.0f;
        float cameraRotationSpeed = 2.0f;
        bool edgeScrolling = true;
        float edgeScrollingSpeed = 5.0f;
        bool showTooltips = true;
        float tooltipDelay = 0.5f;
        bool showFPS = true;
        bool showMinimap = true;
        bool pauseOnLostFocus = true;

        // Camera settings
        float mouseSensitivity = 1.0f;
        bool invertMouseY = false;
        float fov = 45.0f;

        // UI settings
        float uiScale = 1.0f;
        int maxFPS = 0; // 0 = unlimited
    } m_game;

    // Cached original settings for reset/cancel
    GraphicsSettings m_originalGraphics;
    AudioSettings m_originalAudio;
    GameSettings m_originalGame;
};
