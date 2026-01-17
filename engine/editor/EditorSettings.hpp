/**
 * @file EditorSettings.hpp
 * @brief Comprehensive Editor Settings/Preferences System for Nova3D/Vehement
 *
 * Provides a centralized configuration system for all editor preferences:
 * - General settings (auto-save, undo history, language)
 * - Appearance settings (theme, colors, fonts, icons)
 * - Viewport settings (camera, grid, gizmos, rendering)
 * - Input settings (mouse, keyboard shortcuts)
 * - Performance settings (memory, threading, quality)
 * - Path settings (projects, temp, plugins)
 *
 * Features:
 * - Type-safe Get<T>/Set operations
 * - JSON persistence with versioning
 * - Settings migration for backward compatibility
 * - Validation and conflict detection
 * - Change notifications
 * - Import/Export functionality
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <variant>
#include <optional>
#include <memory>
#include <optional>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace Nova {

// Forward declarations
class PreferencesPanel;

// =============================================================================
// Setting Types and Enums
// =============================================================================

/**
 * @brief Theme preset options
 */
enum class EditorThemePreset {
    Dark,
    Light,
    Custom
};

/**
 * @brief Icon size presets
 */
enum class IconSize {
    Small,    // 16x16
    Medium,   // 24x24
    Large     // 32x32
};

/**
 * @brief Default camera mode for viewport
 */
enum class DefaultCameraMode {
    Perspective,
    Orthographic,
    Top,
    Front,
    Side
};

/**
 * @brief Anti-aliasing modes
 */
enum class AntiAliasingMode {
    None,
    FXAA,
    MSAA_2x,
    MSAA_4x,
    MSAA_8x,
    TAA
};

/**
 * @brief Shadow quality presets
 */
enum class ShadowQualityPreset {
    Off,
    Low,
    Medium,
    High,
    Ultra
};

/**
 * @brief Keyboard shortcut context
 */
enum class ShortcutContext {
    Global,      // Works everywhere
    Viewport,    // Only when viewport is focused
    Panel,       // Only within panels
    TextEdit     // Only in text editing contexts
};

/**
 * @brief Key modifiers for shortcuts
 */
enum class KeyModifiers : uint32_t {
    None = 0,
    Ctrl = 1 << 0,
    Shift = 1 << 1,
    Alt = 1 << 2,
    Super = 1 << 3  // Windows/Command key
};

inline KeyModifiers operator|(KeyModifiers a, KeyModifiers b) {
    return static_cast<KeyModifiers>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(KeyModifiers a, KeyModifiers b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

// =============================================================================
// KeyboardShortcut Structure
// =============================================================================

/**
 * @brief Represents a keyboard shortcut binding
 */
struct KeyboardShortcut {
    std::string action;           // Action identifier (e.g., "editor.save", "viewport.focus")
    int key = 0;                  // Key code (GLFW key codes)
    KeyModifiers modifiers = KeyModifiers::None;
    ShortcutContext context = ShortcutContext::Global;
    std::string displayName;      // Human-readable name
    std::string category;         // Category for organization

    /**
     * @brief Get human-readable shortcut string (e.g., "Ctrl+S")
     */
    [[nodiscard]] std::string ToString() const;

    /**
     * @brief Parse shortcut from string (e.g., "Ctrl+Shift+S")
     */
    static std::optional<KeyboardShortcut> FromString(const std::string& str);

    /**
     * @brief Check if shortcut matches key event
     */
    [[nodiscard]] bool Matches(int keyCode, KeyModifiers mods) const;

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static KeyboardShortcut FromJson(const nlohmann::json& json);

    bool operator==(const KeyboardShortcut& other) const;
    bool operator!=(const KeyboardShortcut& other) const { return !(*this == other); }
};

// =============================================================================
// Settings Structures
// =============================================================================

/**
 * @brief General editor settings
 */
struct GeneralSettings {
    bool autoSaveEnabled = true;
    int autoSaveIntervalMinutes = 5;
    int undoHistorySize = 100;
    bool showWelcomeOnStartup = true;
    std::string language = "en-US";
    std::string dateFormat = "yyyy-MM-dd";
    bool confirmOnExit = true;
    bool reopenLastProject = true;
    int recentProjectsMax = 10;
    bool checkForUpdates = true;

    [[nodiscard]] nlohmann::json ToJson() const;
    static GeneralSettings FromJson(const nlohmann::json& json);
};

/**
 * @brief Appearance/UI settings
 */
struct AppearanceSettings {
    EditorThemePreset theme = EditorThemePreset::Dark;
    glm::vec4 accentColor{0.40f, 0.60f, 1.0f, 1.0f};
    float fontSize = 14.0f;
    IconSize iconSize = IconSize::Medium;
    bool showToolbarText = true;
    float panelBorderWidth = 1.0f;
    float windowOpacity = 1.0f;
    bool useNativeWindowFrame = false;
    bool animateTransitions = true;
    float animationSpeed = 1.0f;
    std::string customThemePath;

    [[nodiscard]] nlohmann::json ToJson() const;
    static AppearanceSettings FromJson(const nlohmann::json& json);
};

/**
 * @brief Viewport/3D view settings
 */
struct ViewportSettings {
    DefaultCameraMode defaultCameraMode = DefaultCameraMode::Perspective;
    float gridSize = 1.0f;
    int gridSubdivisions = 10;
    glm::vec4 gridColor{0.3f, 0.3f, 0.3f, 0.5f};
    glm::vec4 backgroundColor{0.15f, 0.15f, 0.18f, 1.0f};
    float gizmoSize = 1.0f;
    glm::vec4 selectionColor{1.0f, 0.6f, 0.0f, 1.0f};
    glm::vec4 selectionHighlightColor{1.0f, 0.8f, 0.2f, 0.3f};
    AntiAliasingMode antiAliasingMode = AntiAliasingMode::TAA;
    int maxFPS = 0;  // 0 = unlimited
    bool showFPS = true;
    bool showStats = false;
    bool showGrid = true;
    bool showAxisGizmo = true;
    bool showWorldOrigin = true;
    float nearClipPlane = 0.1f;
    float farClipPlane = 10000.0f;
    float fieldOfView = 60.0f;

    [[nodiscard]] nlohmann::json ToJson() const;
    static ViewportSettings FromJson(const nlohmann::json& json);
};

/**
 * @brief Input/Controls settings
 */
struct InputSettings {
    float mouseSensitivity = 1.0f;
    float scrollSpeed = 1.0f;
    bool invertMouseY = false;
    bool invertMouseX = false;
    float panSpeed = 1.0f;
    float orbitSpeed = 1.0f;
    float zoomSpeed = 1.0f;
    bool smoothCamera = true;
    float cameraSmoothness = 0.15f;
    bool enableGamepad = true;
    float gamepadDeadzone = 0.15f;
    float doubleClickTime = 0.3f;
    float dragThreshold = 4.0f;

    // Keyboard shortcuts stored separately for extensibility
    std::vector<KeyboardShortcut> shortcuts;

    [[nodiscard]] nlohmann::json ToJson() const;
    static InputSettings FromJson(const nlohmann::json& json);
};

/**
 * @brief Performance settings
 */
struct PerformanceEditorSettings {
    int maxTextureSize = 4096;
    float lodBias = 0.0f;
    ShadowQualityPreset shadowQuality = ShadowQualityPreset::High;
    bool enableVSync = true;
    int gpuMemoryLimitMB = 0;  // 0 = auto
    int workerThreadCount = 0;  // 0 = auto-detect
    bool enableAsyncLoading = true;
    bool enableTextureStreaming = true;
    int thumbnailCacheSizeMB = 256;
    bool enableEditorProfiling = false;
    bool lowPowerMode = false;
    int targetEditorFPS = 60;

    [[nodiscard]] nlohmann::json ToJson() const;
    static PerformanceEditorSettings FromJson(const nlohmann::json& json);
};

/**
 * @brief Path/Directory settings
 */
struct PathSettings {
    std::string defaultProjectPath;
    std::string tempDirectory;
    std::vector<std::string> pluginDirectories;
    std::vector<std::string> scriptDirectories;
    std::vector<std::string> assetSearchPaths;
    std::string screenshotDirectory;
    std::string logDirectory;
    std::string autosaveDirectory;
    bool useRelativePaths = true;

    [[nodiscard]] nlohmann::json ToJson() const;
    static PathSettings FromJson(const nlohmann::json& json);
};

/**
 * @brief Plugin settings
 */
struct PluginSettings {
    bool autoLoadPlugins = true;
    bool sandboxPlugins = true;
    std::vector<std::string> enabledPlugins;
    std::vector<std::string> disabledPlugins;
    std::unordered_map<std::string, nlohmann::json> pluginConfigs;

    [[nodiscard]] nlohmann::json ToJson() const;
    static PluginSettings FromJson(const nlohmann::json& json);
};

// =============================================================================
// Settings Category
// =============================================================================

/**
 * @brief Settings category enumeration
 */
enum class SettingsCategory {
    General,
    Appearance,
    Viewport,
    Input,
    Performance,
    Paths,
    Plugins
};

/**
 * @brief Get display name for settings category
 */
const char* GetSettingsCategoryName(SettingsCategory category);

/**
 * @brief Get icon for settings category
 */
const char* GetSettingsCategoryIcon(SettingsCategory category);

// =============================================================================
// Settings Validation
// =============================================================================

/**
 * @brief Settings validation result
 */
struct SettingsValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<std::pair<std::string, std::string>> shortcutConflicts;  // action1, action2

    void AddError(const std::string& error) {
        errors.push_back(error);
        valid = false;
    }

    void AddWarning(const std::string& warning) {
        warnings.push_back(warning);
    }

    void AddConflict(const std::string& action1, const std::string& action2) {
        shortcutConflicts.emplace_back(action1, action2);
        valid = false;
    }

    [[nodiscard]] bool HasConflicts() const { return !shortcutConflicts.empty(); }
};

// =============================================================================
// Complete Editor Settings
// =============================================================================

/**
 * @brief Complete editor settings structure
 */
struct CompleteEditorSettings {
    static constexpr int CURRENT_VERSION = 1;

    int version = CURRENT_VERSION;
    GeneralSettings general;
    AppearanceSettings appearance;
    ViewportSettings viewport;
    InputSettings input;
    PerformanceEditorSettings performance;
    PathSettings paths;
    PluginSettings plugins;

    [[nodiscard]] nlohmann::json ToJson() const;
    static CompleteEditorSettings FromJson(const nlohmann::json& json);
};

// =============================================================================
// EditorSettings Singleton
// =============================================================================

/**
 * @brief Singleton manager for all editor settings
 *
 * Provides centralized access to editor preferences with:
 * - Type-safe Get<T>/Set operations
 * - JSON persistence
 * - Settings migration
 * - Validation
 * - Change notifications
 */
class EditorSettings {
public:
    /**
     * @brief Get singleton instance
     */
    static EditorSettings& Instance();

    // Delete copy/move for singleton
    EditorSettings(const EditorSettings&) = delete;
    EditorSettings& operator=(const EditorSettings&) = delete;
    EditorSettings(EditorSettings&&) = delete;
    EditorSettings& operator=(EditorSettings&&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize with default settings
     */
    void Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    // =========================================================================
    // Load/Save
    // =========================================================================

    /**
     * @brief Load settings from file
     * @param filepath Path to settings JSON file
     * @return Success or error message
     */
    [[nodiscard]] std::optional<void> Load(const std::string& filepath = "");

    /**
     * @brief Save settings to file
     * @param filepath Path to save to (empty = default location)
     * @return Success or error message
     */
    [[nodiscard]] std::optional<void> Save(const std::string& filepath = "");

    /**
     * @brief Get default settings file path
     */
    [[nodiscard]] std::string GetDefaultSettingsPath() const;

    // =========================================================================
    // Generic Get/Set
    // =========================================================================

    /**
     * @brief Get a setting value by key with default
     * @tparam T Value type
     * @param key Setting key (e.g., "general.autoSaveEnabled")
     * @param defaultValue Default if not found
     * @return Setting value
     */
    template<typename T>
    [[nodiscard]] T Get(const std::string& key, const T& defaultValue) const;

    /**
     * @brief Set a setting value by key
     * @tparam T Value type
     * @param key Setting key
     * @param value New value
     */
    template<typename T>
    void Set(const std::string& key, const T& value);

    /**
     * @brief Reset a single setting to default
     * @param key Setting key
     */
    void Reset(const std::string& key);

    /**
     * @brief Reset all settings to defaults
     */
    void ResetAll();

    /**
     * @brief Reset settings in a category to defaults
     */
    void ResetCategory(SettingsCategory category);

    // =========================================================================
    // Direct Access
    // =========================================================================

    [[nodiscard]] const CompleteEditorSettings& GetSettings() const { return m_settings; }
    [[nodiscard]] CompleteEditorSettings& GetSettings() { return m_settings; }

    [[nodiscard]] const GeneralSettings& GetGeneral() const { return m_settings.general; }
    [[nodiscard]] GeneralSettings& GetGeneral() { return m_settings.general; }
    void SetGeneral(const GeneralSettings& settings);

    [[nodiscard]] const AppearanceSettings& GetAppearance() const { return m_settings.appearance; }
    [[nodiscard]] AppearanceSettings& GetAppearance() { return m_settings.appearance; }
    void SetAppearance(const AppearanceSettings& settings);

    [[nodiscard]] const ViewportSettings& GetViewport() const { return m_settings.viewport; }
    [[nodiscard]] ViewportSettings& GetViewport() { return m_settings.viewport; }
    void SetViewport(const ViewportSettings& settings);

    [[nodiscard]] const InputSettings& GetInput() const { return m_settings.input; }
    [[nodiscard]] InputSettings& GetInput() { return m_settings.input; }
    void SetInput(const InputSettings& settings);

    [[nodiscard]] const PerformanceEditorSettings& GetPerformance() const { return m_settings.performance; }
    [[nodiscard]] PerformanceEditorSettings& GetPerformance() { return m_settings.performance; }
    void SetPerformance(const PerformanceEditorSettings& settings);

    [[nodiscard]] const PathSettings& GetPaths() const { return m_settings.paths; }
    [[nodiscard]] PathSettings& GetPaths() { return m_settings.paths; }
    void SetPaths(const PathSettings& settings);

    [[nodiscard]] const PluginSettings& GetPlugins() const { return m_settings.plugins; }
    [[nodiscard]] PluginSettings& GetPlugins() { return m_settings.plugins; }
    void SetPlugins(const PluginSettings& settings);

    // =========================================================================
    // Keyboard Shortcuts
    // =========================================================================

    /**
     * @brief Get shortcut for action
     * @param action Action identifier
     * @return Shortcut or nullopt if not found
     */
    [[nodiscard]] std::optional<KeyboardShortcut> GetShortcut(const std::string& action) const;

    /**
     * @brief Set shortcut for action
     * @param shortcut The shortcut to set
     */
    void SetShortcut(const KeyboardShortcut& shortcut);

    /**
     * @brief Remove shortcut for action
     * @param action Action identifier
     */
    void RemoveShortcut(const std::string& action);

    /**
     * @brief Get all shortcuts in a category
     */
    [[nodiscard]] std::vector<KeyboardShortcut> GetShortcutsByCategory(const std::string& category) const;

    /**
     * @brief Get all shortcut categories
     */
    [[nodiscard]] std::vector<std::string> GetShortcutCategories() const;

    /**
     * @brief Check for shortcut conflicts
     */
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> FindShortcutConflicts() const;

    /**
     * @brief Reset all shortcuts to defaults
     */
    void ResetShortcutsToDefaults();

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate current settings
     * @return Validation result with errors and warnings
     */
    [[nodiscard]] SettingsValidationResult Validate() const;

    /**
     * @brief Validate a specific path setting
     */
    [[nodiscard]] bool ValidatePath(const std::string& path, bool mustExist = false) const;

    // =========================================================================
    // Import/Export
    // =========================================================================

    /**
     * @brief Export settings to file
     * @param filepath Destination path
     * @return Success or error
     */
    [[nodiscard]] std::optional<void> Export(const std::string& filepath) const;

    /**
     * @brief Import settings from file
     * @param filepath Source path
     * @param merge If true, merge with existing; if false, replace all
     * @return Success or error
     */
    [[nodiscard]] std::optional<void> Import(const std::string& filepath, bool merge = false);

    // =========================================================================
    // Migration
    // =========================================================================

    /**
     * @brief Migrate settings from older version
     * @param oldVersion The old version number
     * @param json The settings JSON to migrate
     * @return Migrated JSON
     */
    [[nodiscard]] nlohmann::json MigrateSettings(int oldVersion, const nlohmann::json& json) const;

    // =========================================================================
    // Change Notifications
    // =========================================================================

    using ChangeCallback = std::function<void(const std::string& key)>;
    using CategoryChangeCallback = std::function<void(SettingsCategory category)>;

    /**
     * @brief Register callback for any setting change
     * @param callback Function to call on change
     * @return Callback ID for unregistering
     */
    uint64_t RegisterChangeCallback(ChangeCallback callback);

    /**
     * @brief Register callback for category change
     * @param category Category to watch
     * @param callback Function to call on change
     * @return Callback ID for unregistering
     */
    uint64_t RegisterCategoryCallback(SettingsCategory category, CategoryChangeCallback callback);

    /**
     * @brief Unregister a callback
     * @param callbackId ID returned from Register*
     */
    void UnregisterCallback(uint64_t callbackId);

    /**
     * @brief Check if settings have been modified since last save
     */
    [[nodiscard]] bool IsDirty() const { return m_dirty; }

    /**
     * @brief Mark settings as clean (after save)
     */
    void ClearDirty() { m_dirty = false; }

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Get list of all setting keys
     */
    [[nodiscard]] std::vector<std::string> GetAllKeys() const;

    /**
     * @brief Check if a key exists
     */
    [[nodiscard]] bool HasKey(const std::string& key) const;

    /**
     * @brief Get setting type as string
     */
    [[nodiscard]] std::string GetSettingType(const std::string& key) const;

private:
    EditorSettings() = default;
    ~EditorSettings() = default;

    void InitializeDefaultShortcuts();
    void NotifyChange(const std::string& key);
    void NotifyCategoryChange(SettingsCategory category);

    CompleteEditorSettings m_settings;
    CompleteEditorSettings m_defaultSettings;
    std::string m_settingsPath;
    bool m_dirty = false;
    bool m_initialized = false;

    // Callbacks
    struct CallbackEntry {
        uint64_t id;
        std::optional<SettingsCategory> category;
        std::variant<ChangeCallback, CategoryChangeCallback> callback;
    };
    std::vector<CallbackEntry> m_callbacks;
    uint64_t m_nextCallbackId = 1;
};

// =============================================================================
// Template Implementations
// =============================================================================

// Specializations declared - implementations in cpp file
template<> bool EditorSettings::Get<bool>(const std::string& key, const bool& defaultValue) const;
template<> int EditorSettings::Get<int>(const std::string& key, const int& defaultValue) const;
template<> float EditorSettings::Get<float>(const std::string& key, const float& defaultValue) const;
template<> std::string EditorSettings::Get<std::string>(const std::string& key, const std::string& defaultValue) const;
template<> glm::vec4 EditorSettings::Get<glm::vec4>(const std::string& key, const glm::vec4& defaultValue) const;

template<> void EditorSettings::Set<bool>(const std::string& key, const bool& value);
template<> void EditorSettings::Set<int>(const std::string& key, const int& value);
template<> void EditorSettings::Set<float>(const std::string& key, const float& value);
template<> void EditorSettings::Set<std::string>(const std::string& key, const std::string& value);
template<> void EditorSettings::Set<glm::vec4>(const std::string& key, const glm::vec4& value);

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Convert enum to string
 */
const char* EditorThemePresetToString(EditorThemePreset preset);
const char* IconSizeToString(IconSize size);
const char* DefaultCameraModeToString(DefaultCameraMode mode);
const char* AntiAliasingModeToString(AntiAliasingMode mode);
const char* ShadowQualityPresetToString(ShadowQualityPreset quality);
const char* ShortcutContextToString(ShortcutContext context);
const char* KeyModifiersToString(KeyModifiers mods);

/**
 * @brief Convert string to enum
 */
EditorThemePreset StringToEditorThemePreset(const std::string& str);
IconSize StringToIconSize(const std::string& str);
DefaultCameraMode StringToDefaultCameraMode(const std::string& str);
AntiAliasingMode StringToAntiAliasingMode(const std::string& str);
ShadowQualityPreset StringToShadowQualityPreset(const std::string& str);
ShortcutContext StringToShortcutContext(const std::string& str);

/**
 * @brief Get key name from key code
 */
const char* GetKeyName(int keyCode);

/**
 * @brief Get key code from key name
 */
int GetKeyCode(const std::string& keyName);

} // namespace Nova
