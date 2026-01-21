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
     * @brief Reset current tab's settings to defaults
     */
    void ResetCurrentTabToDefaults();

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
    void RenderUnsavedChangesDialog();
    void RenderValidationWarningDialog();

    // Input settings helpers
    void RenderInputCategory(const std::string& category);
    void RenderActionBinding(const Nova::ActionDefinition& action);
    void RenderBindingButton(const std::string& actionName, Nova::InputDevice device,
                            const std::vector<Nova::ExtendedBinding>& bindings);

    // Graphics settings helpers
    void UpdateAvailableResolutions();
    void ApplyGraphicsSettings();
    std::string QualityPresetToString(QualityPreset preset) const;
    bool ValidateGraphicsSettings();

    // Audio settings helpers
    void ApplyAudioSettings();
    bool ValidateAudioSettings();

    // Game settings helpers
    void ApplyGameSettings();
    void ApplyCameraSettings();
    void ApplyEditorSettings();
    bool ValidateGameSettings();

    // Validation
    bool ValidateSettings();
    void ShowValidationWarning(const std::string& message);

    // State management
    void MarkAsModified() { m_hasUnsavedChanges = true; }
    void ClearModifiedFlag() { m_hasUnsavedChanges = false; }

    // Check if value differs from default
    template<typename T>
    bool DiffersFromDefault(const T& current, const T& defaultVal) const {
        return current != defaultVal;
    }

    // Audio system interface
    class IAudioSystem {
    public:
        virtual ~IAudioSystem() = default;
        virtual void SetMasterVolume(float volume) = 0;
        virtual void SetMusicVolume(float volume) = 0;
        virtual void SetSFXVolume(float volume) = 0;
        virtual void SetAmbientVolume(float volume) = 0;
        virtual void SetVoiceVolume(float volume) = 0;
    };

    // Systems
    Nova::InputManager* m_inputManager = nullptr;
    Nova::Window* m_window = nullptr;
    IAudioSystem* m_audioSystem = nullptr;

public:
    void SetAudioSystem(IAudioSystem* audioSystem) { m_audioSystem = audioSystem; }

private:

    // UI State
    SettingsTab m_currentTab = SettingsTab::Input;
    bool m_hasUnsavedChanges = false;
    bool m_showUnsavedDialog = false;
    bool m_showValidationWarning = false;
    std::string m_validationMessage;
    bool m_pendingClose = false;

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
        int shadowQuality = 2; // 0=Low, 1=Medium, 2=High, 3=Ultra
        bool enableHDR = false;
        bool enableBloom = true;
        bool enableSSAO = true;
        int antiAliasing = 4; // MSAA samples: 0, 2, 4, 8, 16
        float renderScale = 1.0f; // Internal resolution scale

        // Default values for comparison
        static GraphicsSettings Default() {
            GraphicsSettings defaults;
            defaults.currentResolution = {1920, 1080};
            defaults.fullscreen = false;
            defaults.vsync = true;
            defaults.qualityPreset = QualityPreset::High;
            defaults.enableShadows = true;
            defaults.shadowQuality = 2;
            defaults.enableHDR = false;
            defaults.enableBloom = true;
            defaults.enableSSAO = true;
            defaults.antiAliasing = 4;
            defaults.renderScale = 1.0f;
            return defaults;
        }
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
        bool masterMute = false;
        bool musicMute = false;
        bool sfxMute = false;

        static AudioSettings Default() {
            AudioSettings defaults;
            defaults.masterVolume = 1.0f;
            defaults.musicVolume = 0.7f;
            defaults.sfxVolume = 1.0f;
            defaults.ambientVolume = 0.5f;
            defaults.voiceVolume = 1.0f;
            defaults.masterMute = false;
            defaults.musicMute = false;
            defaults.sfxMute = false;
            return defaults;
        }
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

        // Camera settings (ENHANCED)
        float mouseSensitivity = 1.0f;
        bool invertMouseY = false;
        float fov = 45.0f;

        // UI settings
        float uiScale = 1.0f;
        int maxFPS = 0; // 0 = unlimited

        static GameSettings Default() {
            GameSettings defaults;
            defaults.cameraSpeed = 10.0f;
            defaults.cameraRotationSpeed = 2.0f;
            defaults.edgeScrolling = true;
            defaults.edgeScrollingSpeed = 5.0f;
            defaults.showTooltips = true;
            defaults.tooltipDelay = 0.5f;
            defaults.showFPS = true;
            defaults.showMinimap = true;
            defaults.pauseOnLostFocus = true;
            defaults.mouseSensitivity = 1.0f;
            defaults.invertMouseY = false;
            defaults.fov = 45.0f;
            defaults.uiScale = 1.0f;
            defaults.maxFPS = 0;
            return defaults;
        }
    } m_game;

    // NEW: Camera Settings (Additional controls)
    struct CameraSettings {
        float sensitivity = 1.0f;           // Camera rotation sensitivity
        bool invertY = false;               // Invert Y-axis
        bool edgeScrolling = true;          // Enable edge scrolling
        float edgeScrollSpeed = 1.0f;       // Edge scroll speed multiplier
        float zoomSpeed = 1.0f;             // Zoom speed multiplier
        float zoomMin = 10.0f;              // Minimum zoom distance
        float zoomMax = 100.0f;             // Maximum zoom distance

        static CameraSettings Default() {
            CameraSettings defaults;
            defaults.sensitivity = 1.0f;
            defaults.invertY = false;
            defaults.edgeScrolling = true;
            defaults.edgeScrollSpeed = 1.0f;
            defaults.zoomSpeed = 1.0f;
            defaults.zoomMin = 10.0f;
            defaults.zoomMax = 100.0f;
            return defaults;
        }
    } m_cameraSettings;

    // NEW: Editor Settings (Auto-save)
    struct EditorSettings {
        bool autoSaveEnabled = true;
        int autoSaveInterval = 5;  // minutes (1-30)

        static EditorSettings Default() {
            EditorSettings defaults;
            defaults.autoSaveEnabled = true;
            defaults.autoSaveInterval = 5;
            return defaults;
        }
    } m_editorSettings;

    // Cached original settings for reset/cancel
    GraphicsSettings m_originalGraphics;
    AudioSettings m_originalAudio;
    GameSettings m_originalGame;
    CameraSettings m_originalCameraSettings;
    EditorSettings m_originalEditorSettings;

    // Config version for future compatibility
    static constexpr int CONFIG_VERSION = 1;
};
