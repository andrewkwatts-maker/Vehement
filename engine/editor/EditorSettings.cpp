/**
 * @file EditorSettings.cpp
 * @brief Implementation of the Editor Settings/Preferences System
 */

#include "EditorSettings.hpp"
#include "../core/Logger.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <cstdlib>

namespace Nova {

// =============================================================================
// KeyboardShortcut Implementation
// =============================================================================

std::string KeyboardShortcut::ToString() const {
    std::string result;

    if (modifiers & KeyModifiers::Ctrl) {
        result += "Ctrl+";
    }
    if (modifiers & KeyModifiers::Shift) {
        result += "Shift+";
    }
    if (modifiers & KeyModifiers::Alt) {
        result += "Alt+";
    }
    if (modifiers & KeyModifiers::Super) {
#ifdef _WIN32
        result += "Win+";
#else
        result += "Cmd+";
#endif
    }

    result += GetKeyName(key);
    return result;
}

std::optional<KeyboardShortcut> KeyboardShortcut::FromString(const std::string& str) {
    KeyboardShortcut shortcut;
    std::string remaining = str;

    // Parse modifiers
    auto parseModifier = [&](const std::string& mod, KeyModifiers flag) {
        size_t pos = remaining.find(mod + "+");
        if (pos == 0) {
            shortcut.modifiers = shortcut.modifiers | flag;
            remaining = remaining.substr(mod.length() + 1);
            return true;
        }
        return false;
    };

    while (true) {
        bool found = false;
        found |= parseModifier("Ctrl", KeyModifiers::Ctrl);
        found |= parseModifier("Shift", KeyModifiers::Shift);
        found |= parseModifier("Alt", KeyModifiers::Alt);
        found |= parseModifier("Win", KeyModifiers::Super);
        found |= parseModifier("Cmd", KeyModifiers::Super);
        found |= parseModifier("Super", KeyModifiers::Super);
        if (!found) break;
    }

    // Parse key
    shortcut.key = GetKeyCode(remaining);
    if (shortcut.key == 0 && remaining != "Unknown") {
        return std::nullopt;
    }

    return shortcut;
}

bool KeyboardShortcut::Matches(int keyCode, KeyModifiers mods) const {
    return key == keyCode && modifiers == mods;
}

nlohmann::json KeyboardShortcut::ToJson() const {
    return nlohmann::json{
        {"action", action},
        {"key", key},
        {"modifiers", static_cast<uint32_t>(modifiers)},
        {"context", ShortcutContextToString(context)},
        {"displayName", displayName},
        {"category", category}
    };
}

KeyboardShortcut KeyboardShortcut::FromJson(const nlohmann::json& json) {
    KeyboardShortcut shortcut;
    shortcut.action = json.value("action", "");
    shortcut.key = json.value("key", 0);
    shortcut.modifiers = static_cast<KeyModifiers>(json.value("modifiers", 0u));
    shortcut.context = StringToShortcutContext(json.value("context", "Global"));
    shortcut.displayName = json.value("displayName", "");
    shortcut.category = json.value("category", "");
    return shortcut;
}

bool KeyboardShortcut::operator==(const KeyboardShortcut& other) const {
    return action == other.action &&
           key == other.key &&
           modifiers == other.modifiers &&
           context == other.context;
}

// =============================================================================
// GeneralSettings Implementation
// =============================================================================

nlohmann::json GeneralSettings::ToJson() const {
    return nlohmann::json{
        {"autoSaveEnabled", autoSaveEnabled},
        {"autoSaveIntervalMinutes", autoSaveIntervalMinutes},
        {"undoHistorySize", undoHistorySize},
        {"showWelcomeOnStartup", showWelcomeOnStartup},
        {"language", language},
        {"dateFormat", dateFormat},
        {"confirmOnExit", confirmOnExit},
        {"reopenLastProject", reopenLastProject},
        {"recentProjectsMax", recentProjectsMax},
        {"checkForUpdates", checkForUpdates}
    };
}

GeneralSettings GeneralSettings::FromJson(const nlohmann::json& json) {
    GeneralSettings settings;
    settings.autoSaveEnabled = json.value("autoSaveEnabled", true);
    settings.autoSaveIntervalMinutes = json.value("autoSaveIntervalMinutes", 5);
    settings.undoHistorySize = json.value("undoHistorySize", 100);
    settings.showWelcomeOnStartup = json.value("showWelcomeOnStartup", true);
    settings.language = json.value("language", "en-US");
    settings.dateFormat = json.value("dateFormat", "yyyy-MM-dd");
    settings.confirmOnExit = json.value("confirmOnExit", true);
    settings.reopenLastProject = json.value("reopenLastProject", true);
    settings.recentProjectsMax = json.value("recentProjectsMax", 10);
    settings.checkForUpdates = json.value("checkForUpdates", true);
    return settings;
}

// =============================================================================
// AppearanceSettings Implementation
// =============================================================================

nlohmann::json AppearanceSettings::ToJson() const {
    return nlohmann::json{
        {"theme", EditorThemePresetToString(theme)},
        {"accentColor", {accentColor.r, accentColor.g, accentColor.b, accentColor.a}},
        {"fontSize", fontSize},
        {"iconSize", IconSizeToString(iconSize)},
        {"showToolbarText", showToolbarText},
        {"panelBorderWidth", panelBorderWidth},
        {"windowOpacity", windowOpacity},
        {"useNativeWindowFrame", useNativeWindowFrame},
        {"animateTransitions", animateTransitions},
        {"animationSpeed", animationSpeed},
        {"customThemePath", customThemePath}
    };
}

AppearanceSettings AppearanceSettings::FromJson(const nlohmann::json& json) {
    AppearanceSettings settings;
    settings.theme = StringToEditorThemePreset(json.value("theme", "Dark"));

    if (json.contains("accentColor") && json["accentColor"].is_array()) {
        auto& arr = json["accentColor"];
        settings.accentColor = glm::vec4(
            arr[0].get<float>(),
            arr[1].get<float>(),
            arr[2].get<float>(),
            arr.size() > 3 ? arr[3].get<float>() : 1.0f
        );
    }

    settings.fontSize = json.value("fontSize", 14.0f);
    settings.iconSize = StringToIconSize(json.value("iconSize", "Medium"));
    settings.showToolbarText = json.value("showToolbarText", true);
    settings.panelBorderWidth = json.value("panelBorderWidth", 1.0f);
    settings.windowOpacity = json.value("windowOpacity", 1.0f);
    settings.useNativeWindowFrame = json.value("useNativeWindowFrame", false);
    settings.animateTransitions = json.value("animateTransitions", true);
    settings.animationSpeed = json.value("animationSpeed", 1.0f);
    settings.customThemePath = json.value("customThemePath", "");
    return settings;
}

// =============================================================================
// ViewportSettings Implementation
// =============================================================================

nlohmann::json ViewportSettings::ToJson() const {
    return nlohmann::json{
        {"defaultCameraMode", DefaultCameraModeToString(defaultCameraMode)},
        {"gridSize", gridSize},
        {"gridSubdivisions", gridSubdivisions},
        {"gridColor", {gridColor.r, gridColor.g, gridColor.b, gridColor.a}},
        {"backgroundColor", {backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a}},
        {"gizmoSize", gizmoSize},
        {"selectionColor", {selectionColor.r, selectionColor.g, selectionColor.b, selectionColor.a}},
        {"selectionHighlightColor", {selectionHighlightColor.r, selectionHighlightColor.g, selectionHighlightColor.b, selectionHighlightColor.a}},
        {"antiAliasingMode", AntiAliasingModeToString(antiAliasingMode)},
        {"maxFPS", maxFPS},
        {"showFPS", showFPS},
        {"showStats", showStats},
        {"showGrid", showGrid},
        {"showAxisGizmo", showAxisGizmo},
        {"showWorldOrigin", showWorldOrigin},
        {"nearClipPlane", nearClipPlane},
        {"farClipPlane", farClipPlane},
        {"fieldOfView", fieldOfView}
    };
}

ViewportSettings ViewportSettings::FromJson(const nlohmann::json& json) {
    ViewportSettings settings;
    settings.defaultCameraMode = StringToDefaultCameraMode(json.value("defaultCameraMode", "Perspective"));
    settings.gridSize = json.value("gridSize", 1.0f);
    settings.gridSubdivisions = json.value("gridSubdivisions", 10);

    auto parseColor = [&json](const std::string& key, const glm::vec4& def) -> glm::vec4 {
        if (json.contains(key) && json[key].is_array()) {
            auto& arr = json[key];
            return glm::vec4(
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>(),
                arr.size() > 3 ? arr[3].get<float>() : 1.0f
            );
        }
        return def;
    };

    settings.gridColor = parseColor("gridColor", {0.3f, 0.3f, 0.3f, 0.5f});
    settings.backgroundColor = parseColor("backgroundColor", {0.15f, 0.15f, 0.18f, 1.0f});
    settings.selectionColor = parseColor("selectionColor", {1.0f, 0.6f, 0.0f, 1.0f});
    settings.selectionHighlightColor = parseColor("selectionHighlightColor", {1.0f, 0.8f, 0.2f, 0.3f});

    settings.gizmoSize = json.value("gizmoSize", 1.0f);
    settings.antiAliasingMode = StringToAntiAliasingMode(json.value("antiAliasingMode", "TAA"));
    settings.maxFPS = json.value("maxFPS", 0);
    settings.showFPS = json.value("showFPS", true);
    settings.showStats = json.value("showStats", false);
    settings.showGrid = json.value("showGrid", true);
    settings.showAxisGizmo = json.value("showAxisGizmo", true);
    settings.showWorldOrigin = json.value("showWorldOrigin", true);
    settings.nearClipPlane = json.value("nearClipPlane", 0.1f);
    settings.farClipPlane = json.value("farClipPlane", 10000.0f);
    settings.fieldOfView = json.value("fieldOfView", 60.0f);
    return settings;
}

// =============================================================================
// InputSettings Implementation
// =============================================================================

nlohmann::json InputSettings::ToJson() const {
    nlohmann::json shortcutsArray = nlohmann::json::array();
    for (const auto& shortcut : shortcuts) {
        shortcutsArray.push_back(shortcut.ToJson());
    }

    return nlohmann::json{
        {"mouseSensitivity", mouseSensitivity},
        {"scrollSpeed", scrollSpeed},
        {"invertMouseY", invertMouseY},
        {"invertMouseX", invertMouseX},
        {"panSpeed", panSpeed},
        {"orbitSpeed", orbitSpeed},
        {"zoomSpeed", zoomSpeed},
        {"smoothCamera", smoothCamera},
        {"cameraSmoothness", cameraSmoothness},
        {"enableGamepad", enableGamepad},
        {"gamepadDeadzone", gamepadDeadzone},
        {"doubleClickTime", doubleClickTime},
        {"dragThreshold", dragThreshold},
        {"shortcuts", shortcutsArray}
    };
}

InputSettings InputSettings::FromJson(const nlohmann::json& json) {
    InputSettings settings;
    settings.mouseSensitivity = json.value("mouseSensitivity", 1.0f);
    settings.scrollSpeed = json.value("scrollSpeed", 1.0f);
    settings.invertMouseY = json.value("invertMouseY", false);
    settings.invertMouseX = json.value("invertMouseX", false);
    settings.panSpeed = json.value("panSpeed", 1.0f);
    settings.orbitSpeed = json.value("orbitSpeed", 1.0f);
    settings.zoomSpeed = json.value("zoomSpeed", 1.0f);
    settings.smoothCamera = json.value("smoothCamera", true);
    settings.cameraSmoothness = json.value("cameraSmoothness", 0.15f);
    settings.enableGamepad = json.value("enableGamepad", true);
    settings.gamepadDeadzone = json.value("gamepadDeadzone", 0.15f);
    settings.doubleClickTime = json.value("doubleClickTime", 0.3f);
    settings.dragThreshold = json.value("dragThreshold", 4.0f);

    if (json.contains("shortcuts") && json["shortcuts"].is_array()) {
        for (const auto& shortcutJson : json["shortcuts"]) {
            settings.shortcuts.push_back(KeyboardShortcut::FromJson(shortcutJson));
        }
    }

    return settings;
}

// =============================================================================
// PerformanceEditorSettings Implementation
// =============================================================================

nlohmann::json PerformanceEditorSettings::ToJson() const {
    return nlohmann::json{
        {"maxTextureSize", maxTextureSize},
        {"lodBias", lodBias},
        {"shadowQuality", ShadowQualityPresetToString(shadowQuality)},
        {"enableVSync", enableVSync},
        {"gpuMemoryLimitMB", gpuMemoryLimitMB},
        {"workerThreadCount", workerThreadCount},
        {"enableAsyncLoading", enableAsyncLoading},
        {"enableTextureStreaming", enableTextureStreaming},
        {"thumbnailCacheSizeMB", thumbnailCacheSizeMB},
        {"enableEditorProfiling", enableEditorProfiling},
        {"lowPowerMode", lowPowerMode},
        {"targetEditorFPS", targetEditorFPS}
    };
}

PerformanceEditorSettings PerformanceEditorSettings::FromJson(const nlohmann::json& json) {
    PerformanceEditorSettings settings;
    settings.maxTextureSize = json.value("maxTextureSize", 4096);
    settings.lodBias = json.value("lodBias", 0.0f);
    settings.shadowQuality = StringToShadowQualityPreset(json.value("shadowQuality", "High"));
    settings.enableVSync = json.value("enableVSync", true);
    settings.gpuMemoryLimitMB = json.value("gpuMemoryLimitMB", 0);
    settings.workerThreadCount = json.value("workerThreadCount", 0);
    settings.enableAsyncLoading = json.value("enableAsyncLoading", true);
    settings.enableTextureStreaming = json.value("enableTextureStreaming", true);
    settings.thumbnailCacheSizeMB = json.value("thumbnailCacheSizeMB", 256);
    settings.enableEditorProfiling = json.value("enableEditorProfiling", false);
    settings.lowPowerMode = json.value("lowPowerMode", false);
    settings.targetEditorFPS = json.value("targetEditorFPS", 60);
    return settings;
}

// =============================================================================
// PathSettings Implementation
// =============================================================================

nlohmann::json PathSettings::ToJson() const {
    return nlohmann::json{
        {"defaultProjectPath", defaultProjectPath},
        {"tempDirectory", tempDirectory},
        {"pluginDirectories", pluginDirectories},
        {"scriptDirectories", scriptDirectories},
        {"assetSearchPaths", assetSearchPaths},
        {"screenshotDirectory", screenshotDirectory},
        {"logDirectory", logDirectory},
        {"autosaveDirectory", autosaveDirectory},
        {"useRelativePaths", useRelativePaths}
    };
}

PathSettings PathSettings::FromJson(const nlohmann::json& json) {
    PathSettings settings;
    settings.defaultProjectPath = json.value("defaultProjectPath", "");
    settings.tempDirectory = json.value("tempDirectory", "");
    settings.pluginDirectories = json.value("pluginDirectories", std::vector<std::string>{});
    settings.scriptDirectories = json.value("scriptDirectories", std::vector<std::string>{});
    settings.assetSearchPaths = json.value("assetSearchPaths", std::vector<std::string>{});
    settings.screenshotDirectory = json.value("screenshotDirectory", "");
    settings.logDirectory = json.value("logDirectory", "");
    settings.autosaveDirectory = json.value("autosaveDirectory", "");
    settings.useRelativePaths = json.value("useRelativePaths", true);
    return settings;
}

// =============================================================================
// PluginSettings Implementation
// =============================================================================

nlohmann::json PluginSettings::ToJson() const {
    return nlohmann::json{
        {"autoLoadPlugins", autoLoadPlugins},
        {"sandboxPlugins", sandboxPlugins},
        {"enabledPlugins", enabledPlugins},
        {"disabledPlugins", disabledPlugins},
        {"pluginConfigs", pluginConfigs}
    };
}

PluginSettings PluginSettings::FromJson(const nlohmann::json& json) {
    PluginSettings settings;
    settings.autoLoadPlugins = json.value("autoLoadPlugins", true);
    settings.sandboxPlugins = json.value("sandboxPlugins", true);
    settings.enabledPlugins = json.value("enabledPlugins", std::vector<std::string>{});
    settings.disabledPlugins = json.value("disabledPlugins", std::vector<std::string>{});

    if (json.contains("pluginConfigs") && json["pluginConfigs"].is_object()) {
        for (auto& [key, value] : json["pluginConfigs"].items()) {
            settings.pluginConfigs[key] = value;
        }
    }

    return settings;
}

// =============================================================================
// CompleteEditorSettings Implementation
// =============================================================================

nlohmann::json CompleteEditorSettings::ToJson() const {
    return nlohmann::json{
        {"version", version},
        {"general", general.ToJson()},
        {"appearance", appearance.ToJson()},
        {"viewport", viewport.ToJson()},
        {"input", input.ToJson()},
        {"performance", performance.ToJson()},
        {"paths", paths.ToJson()},
        {"plugins", plugins.ToJson()}
    };
}

CompleteEditorSettings CompleteEditorSettings::FromJson(const nlohmann::json& json) {
    CompleteEditorSettings settings;
    settings.version = json.value("version", CURRENT_VERSION);

    if (json.contains("general")) {
        settings.general = GeneralSettings::FromJson(json["general"]);
    }
    if (json.contains("appearance")) {
        settings.appearance = AppearanceSettings::FromJson(json["appearance"]);
    }
    if (json.contains("viewport")) {
        settings.viewport = ViewportSettings::FromJson(json["viewport"]);
    }
    if (json.contains("input")) {
        settings.input = InputSettings::FromJson(json["input"]);
    }
    if (json.contains("performance")) {
        settings.performance = PerformanceEditorSettings::FromJson(json["performance"]);
    }
    if (json.contains("paths")) {
        settings.paths = PathSettings::FromJson(json["paths"]);
    }
    if (json.contains("plugins")) {
        settings.plugins = PluginSettings::FromJson(json["plugins"]);
    }

    return settings;
}

// =============================================================================
// Category Helper Functions
// =============================================================================

const char* GetSettingsCategoryName(SettingsCategory category) {
    switch (category) {
        case SettingsCategory::General:     return "General";
        case SettingsCategory::Appearance:  return "Appearance";
        case SettingsCategory::Viewport:    return "Viewport";
        case SettingsCategory::Input:       return "Input";
        case SettingsCategory::Performance: return "Performance";
        case SettingsCategory::Paths:       return "Paths";
        case SettingsCategory::Plugins:     return "Plugins";
        default:                            return "Unknown";
    }
}

const char* GetSettingsCategoryIcon(SettingsCategory category) {
    switch (category) {
        case SettingsCategory::General:     return "\xef\x80\x93";  // fa-cog
        case SettingsCategory::Appearance:  return "\xef\x94\xbf";  // fa-palette
        case SettingsCategory::Viewport:    return "\xef\x84\xb0";  // fa-eye
        case SettingsCategory::Input:       return "\xef\x84\x9c";  // fa-keyboard
        case SettingsCategory::Performance: return "\xef\x87\xa2";  // fa-tachometer
        case SettingsCategory::Paths:       return "\xef\x81\xbc";  // fa-folder
        case SettingsCategory::Plugins:     return "\xef\x84\xa6";  // fa-puzzle-piece
        default:                            return "\xef\x81\x99";  // fa-question
    }
}

// =============================================================================
// EditorSettings Singleton Implementation
// =============================================================================

EditorSettings& EditorSettings::Instance() {
    static EditorSettings instance;
    return instance;
}

void EditorSettings::Initialize() {
    if (m_initialized) return;

    // Set default settings
    m_settings = CompleteEditorSettings{};
    m_defaultSettings = m_settings;

    // Initialize default shortcuts
    InitializeDefaultShortcuts();

    // Set default paths based on platform
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) {
        m_settings.paths.defaultProjectPath = std::string(appData) + "\\Nova3D\\Projects";
        m_settings.paths.tempDirectory = std::string(appData) + "\\Nova3D\\Temp";
        m_settings.paths.logDirectory = std::string(appData) + "\\Nova3D\\Logs";
        m_settings.paths.autosaveDirectory = std::string(appData) + "\\Nova3D\\Autosave";
        m_settingsPath = std::string(appData) + "\\Nova3D\\editor_settings.json";
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        m_settings.paths.defaultProjectPath = std::string(home) + "/.nova3d/projects";
        m_settings.paths.tempDirectory = std::string(home) + "/.nova3d/temp";
        m_settings.paths.logDirectory = std::string(home) + "/.nova3d/logs";
        m_settings.paths.autosaveDirectory = std::string(home) + "/.nova3d/autosave";
        m_settingsPath = std::string(home) + "/.nova3d/editor_settings.json";
    }
#endif

    m_defaultSettings = m_settings;
    m_initialized = true;
    m_dirty = false;

    LOG_INFO("EditorSettings initialized");
}

void EditorSettings::Shutdown() {
    if (m_dirty) {
        auto result = Save();
        if (!result) {
            LOG_WARNING("Failed to save editor settings on shutdown: {}", result.error());
        }
    }

    m_callbacks.clear();
    m_initialized = false;
}

void EditorSettings::InitializeDefaultShortcuts() {
    auto& shortcuts = m_settings.input.shortcuts;
    shortcuts.clear();

    // File operations
    shortcuts.push_back({"file.new", 'N', KeyModifiers::Ctrl, ShortcutContext::Global, "New", "File"});
    shortcuts.push_back({"file.open", 'O', KeyModifiers::Ctrl, ShortcutContext::Global, "Open", "File"});
    shortcuts.push_back({"file.save", 'S', KeyModifiers::Ctrl, ShortcutContext::Global, "Save", "File"});
    shortcuts.push_back({"file.saveAs", 'S', KeyModifiers::Ctrl | KeyModifiers::Shift, ShortcutContext::Global, "Save As", "File"});
    shortcuts.push_back({"file.saveAll", 'S', KeyModifiers::Ctrl | KeyModifiers::Alt, ShortcutContext::Global, "Save All", "File"});

    // Edit operations
    shortcuts.push_back({"edit.undo", 'Z', KeyModifiers::Ctrl, ShortcutContext::Global, "Undo", "Edit"});
    shortcuts.push_back({"edit.redo", 'Y', KeyModifiers::Ctrl, ShortcutContext::Global, "Redo", "Edit"});
    shortcuts.push_back({"edit.redoAlt", 'Z', KeyModifiers::Ctrl | KeyModifiers::Shift, ShortcutContext::Global, "Redo (Alt)", "Edit"});
    shortcuts.push_back({"edit.cut", 'X', KeyModifiers::Ctrl, ShortcutContext::Global, "Cut", "Edit"});
    shortcuts.push_back({"edit.copy", 'C', KeyModifiers::Ctrl, ShortcutContext::Global, "Copy", "Edit"});
    shortcuts.push_back({"edit.paste", 'V', KeyModifiers::Ctrl, ShortcutContext::Global, "Paste", "Edit"});
    shortcuts.push_back({"edit.duplicate", 'D', KeyModifiers::Ctrl, ShortcutContext::Global, "Duplicate", "Edit"});
    shortcuts.push_back({"edit.delete", 127, KeyModifiers::None, ShortcutContext::Global, "Delete", "Edit"});  // Delete key
    shortcuts.push_back({"edit.selectAll", 'A', KeyModifiers::Ctrl, ShortcutContext::Global, "Select All", "Edit"});

    // Viewport operations
    shortcuts.push_back({"viewport.focus", 'F', KeyModifiers::None, ShortcutContext::Viewport, "Focus Selection", "Viewport"});
    shortcuts.push_back({"viewport.frameAll", 'A', KeyModifiers::None, ShortcutContext::Viewport, "Frame All", "Viewport"});
    shortcuts.push_back({"viewport.toggleGrid", 'G', KeyModifiers::None, ShortcutContext::Viewport, "Toggle Grid", "Viewport"});
    shortcuts.push_back({"viewport.toggleWireframe", 'Z', KeyModifiers::None, ShortcutContext::Viewport, "Toggle Wireframe", "Viewport"});
    shortcuts.push_back({"viewport.toggleOrtho", '5', KeyModifiers::None, ShortcutContext::Viewport, "Toggle Orthographic", "Viewport"});

    // Transform modes
    shortcuts.push_back({"transform.translate", 'W', KeyModifiers::None, ShortcutContext::Viewport, "Translate Mode", "Transform"});
    shortcuts.push_back({"transform.rotate", 'E', KeyModifiers::None, ShortcutContext::Viewport, "Rotate Mode", "Transform"});
    shortcuts.push_back({"transform.scale", 'R', KeyModifiers::None, ShortcutContext::Viewport, "Scale Mode", "Transform"});
    shortcuts.push_back({"transform.toggleLocal", 'L', KeyModifiers::None, ShortcutContext::Viewport, "Toggle Local/World", "Transform"});
    shortcuts.push_back({"transform.toggleSnap", 'X', KeyModifiers::None, ShortcutContext::Viewport, "Toggle Snap", "Transform"});

    // View operations
    shortcuts.push_back({"view.top", '7', KeyModifiers::None, ShortcutContext::Viewport, "Top View", "View"});
    shortcuts.push_back({"view.front", '1', KeyModifiers::None, ShortcutContext::Viewport, "Front View", "View"});
    shortcuts.push_back({"view.side", '3', KeyModifiers::None, ShortcutContext::Viewport, "Side View", "View"});
    shortcuts.push_back({"view.bottom", '7', KeyModifiers::Ctrl, ShortcutContext::Viewport, "Bottom View", "View"});
    shortcuts.push_back({"view.back", '1', KeyModifiers::Ctrl, ShortcutContext::Viewport, "Back View", "View"});

    // Window/Panel operations
    shortcuts.push_back({"window.preferences", ',', KeyModifiers::Ctrl, ShortcutContext::Global, "Preferences", "Window"});
    shortcuts.push_back({"window.console", '`', KeyModifiers::None, ShortcutContext::Global, "Console", "Window"});
    shortcuts.push_back({"window.hierarchy", 'H', KeyModifiers::Ctrl | KeyModifiers::Shift, ShortcutContext::Global, "Hierarchy", "Window"});
    shortcuts.push_back({"window.inspector", 'I', KeyModifiers::Ctrl | KeyModifiers::Shift, ShortcutContext::Global, "Inspector", "Window"});
    shortcuts.push_back({"window.project", 'P', KeyModifiers::Ctrl | KeyModifiers::Shift, ShortcutContext::Global, "Project", "Window"});

    // Play mode
    shortcuts.push_back({"play.play", 'P', KeyModifiers::Ctrl, ShortcutContext::Global, "Play", "Play"});
    shortcuts.push_back({"play.pause", 'P', KeyModifiers::Ctrl | KeyModifiers::Shift, ShortcutContext::Global, "Pause", "Play"});
    shortcuts.push_back({"play.stop", 'P', KeyModifiers::Ctrl | KeyModifiers::Alt, ShortcutContext::Global, "Stop", "Play"});
    shortcuts.push_back({"play.step", 'P', KeyModifiers::Alt, ShortcutContext::Global, "Step Frame", "Play"});

    // Store defaults
    m_defaultSettings.input.shortcuts = shortcuts;
}

std::optional<void> EditorSettings::Load(const std::string& filepath) {
    std::string path = filepath.empty() ? m_settingsPath : filepath;

    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return std::nullopt;
        }

        nlohmann::json json;
        file >> json;
        file.close();

        // Check version and migrate if needed
        int fileVersion = json.value("version", 1);
        if (fileVersion < CompleteEditorSettings::CURRENT_VERSION) {
            json = MigrateSettings(fileVersion, json);
            LOG_INFO("Migrated settings from version {} to {}", fileVersion, CompleteEditorSettings::CURRENT_VERSION);
        }

        m_settings = CompleteEditorSettings::FromJson(json);
        m_settingsPath = path;
        m_dirty = false;

        LOG_INFO("Loaded editor settings from: {}", path);
        return {};
    }
    catch (const std::exception& e) {
        return std::unexpected(std::string("Failed to parse settings: ") + e.what());
    }
}

std::optional<void> EditorSettings::Save(const std::string& filepath) {
    std::string path = filepath.empty() ? m_settingsPath : filepath;

    try {
        // Ensure directory exists
        std::filesystem::path dirPath = std::filesystem::path(path).parent_path();
        if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
            std::filesystem::create_directories(dirPath);
        }

        nlohmann::json json = m_settings.ToJson();

        std::ofstream file(path);
        if (!file.is_open()) {
            return std::nullopt;
        }

        file << json.dump(4);
        file.close();

        m_settingsPath = path;
        m_dirty = false;

        LOG_INFO("Saved editor settings to: {}", path);
        return {};
    }
    catch (const std::exception& e) {
        return std::unexpected(std::string("Failed to save settings: ") + e.what());
    }
}

std::string EditorSettings::GetDefaultSettingsPath() const {
    return m_settingsPath;
}

void EditorSettings::Reset(const std::string& key) {
    // Parse key to find the setting
    size_t dotPos = key.find('.');
    if (dotPos == std::string::npos) return;

    std::string category = key.substr(0, dotPos);
    std::string setting = key.substr(dotPos + 1);

    // Reset specific settings based on category
    if (category == "general") {
        m_settings.general = m_defaultSettings.general;
        NotifyCategoryChange(SettingsCategory::General);
    }
    else if (category == "appearance") {
        m_settings.appearance = m_defaultSettings.appearance;
        NotifyCategoryChange(SettingsCategory::Appearance);
    }
    else if (category == "viewport") {
        m_settings.viewport = m_defaultSettings.viewport;
        NotifyCategoryChange(SettingsCategory::Viewport);
    }
    else if (category == "input") {
        m_settings.input = m_defaultSettings.input;
        NotifyCategoryChange(SettingsCategory::Input);
    }
    else if (category == "performance") {
        m_settings.performance = m_defaultSettings.performance;
        NotifyCategoryChange(SettingsCategory::Performance);
    }
    else if (category == "paths") {
        m_settings.paths = m_defaultSettings.paths;
        NotifyCategoryChange(SettingsCategory::Paths);
    }
    else if (category == "plugins") {
        m_settings.plugins = m_defaultSettings.plugins;
        NotifyCategoryChange(SettingsCategory::Plugins);
    }

    m_dirty = true;
    NotifyChange(key);
}

void EditorSettings::ResetAll() {
    m_settings = m_defaultSettings;
    m_dirty = true;

    for (int i = 0; i <= static_cast<int>(SettingsCategory::Plugins); ++i) {
        NotifyCategoryChange(static_cast<SettingsCategory>(i));
    }
}

void EditorSettings::ResetCategory(SettingsCategory category) {
    switch (category) {
        case SettingsCategory::General:
            m_settings.general = m_defaultSettings.general;
            break;
        case SettingsCategory::Appearance:
            m_settings.appearance = m_defaultSettings.appearance;
            break;
        case SettingsCategory::Viewport:
            m_settings.viewport = m_defaultSettings.viewport;
            break;
        case SettingsCategory::Input:
            m_settings.input = m_defaultSettings.input;
            break;
        case SettingsCategory::Performance:
            m_settings.performance = m_defaultSettings.performance;
            break;
        case SettingsCategory::Paths:
            m_settings.paths = m_defaultSettings.paths;
            break;
        case SettingsCategory::Plugins:
            m_settings.plugins = m_defaultSettings.plugins;
            break;
    }

    m_dirty = true;
    NotifyCategoryChange(category);
}

// Direct setters with notifications
void EditorSettings::SetGeneral(const GeneralSettings& settings) {
    m_settings.general = settings;
    m_dirty = true;
    NotifyCategoryChange(SettingsCategory::General);
}

void EditorSettings::SetAppearance(const AppearanceSettings& settings) {
    m_settings.appearance = settings;
    m_dirty = true;
    NotifyCategoryChange(SettingsCategory::Appearance);
}

void EditorSettings::SetViewport(const ViewportSettings& settings) {
    m_settings.viewport = settings;
    m_dirty = true;
    NotifyCategoryChange(SettingsCategory::Viewport);
}

void EditorSettings::SetInput(const InputSettings& settings) {
    m_settings.input = settings;
    m_dirty = true;
    NotifyCategoryChange(SettingsCategory::Input);
}

void EditorSettings::SetPerformance(const PerformanceEditorSettings& settings) {
    m_settings.performance = settings;
    m_dirty = true;
    NotifyCategoryChange(SettingsCategory::Performance);
}

void EditorSettings::SetPaths(const PathSettings& settings) {
    m_settings.paths = settings;
    m_dirty = true;
    NotifyCategoryChange(SettingsCategory::Paths);
}

void EditorSettings::SetPlugins(const PluginSettings& settings) {
    m_settings.plugins = settings;
    m_dirty = true;
    NotifyCategoryChange(SettingsCategory::Plugins);
}

// =============================================================================
// Keyboard Shortcuts
// =============================================================================

std::optional<KeyboardShortcut> EditorSettings::GetShortcut(const std::string& action) const {
    for (const auto& shortcut : m_settings.input.shortcuts) {
        if (shortcut.action == action) {
            return shortcut;
        }
    }
    return std::nullopt;
}

void EditorSettings::SetShortcut(const KeyboardShortcut& shortcut) {
    // Remove existing shortcut for this action
    auto& shortcuts = m_settings.input.shortcuts;
    shortcuts.erase(
        std::remove_if(shortcuts.begin(), shortcuts.end(),
            [&](const KeyboardShortcut& s) { return s.action == shortcut.action; }),
        shortcuts.end()
    );

    // Add new shortcut
    shortcuts.push_back(shortcut);
    m_dirty = true;
    NotifyChange("input.shortcut." + shortcut.action);
}

void EditorSettings::RemoveShortcut(const std::string& action) {
    auto& shortcuts = m_settings.input.shortcuts;
    shortcuts.erase(
        std::remove_if(shortcuts.begin(), shortcuts.end(),
            [&](const KeyboardShortcut& s) { return s.action == action; }),
        shortcuts.end()
    );
    m_dirty = true;
    NotifyChange("input.shortcut." + action);
}

std::vector<KeyboardShortcut> EditorSettings::GetShortcutsByCategory(const std::string& category) const {
    std::vector<KeyboardShortcut> result;
    for (const auto& shortcut : m_settings.input.shortcuts) {
        if (shortcut.category == category) {
            result.push_back(shortcut);
        }
    }
    return result;
}

std::vector<std::string> EditorSettings::GetShortcutCategories() const {
    std::vector<std::string> categories;
    for (const auto& shortcut : m_settings.input.shortcuts) {
        if (std::find(categories.begin(), categories.end(), shortcut.category) == categories.end()) {
            categories.push_back(shortcut.category);
        }
    }
    std::sort(categories.begin(), categories.end());
    return categories;
}

std::vector<std::pair<std::string, std::string>> EditorSettings::FindShortcutConflicts() const {
    std::vector<std::pair<std::string, std::string>> conflicts;
    const auto& shortcuts = m_settings.input.shortcuts;

    for (size_t i = 0; i < shortcuts.size(); ++i) {
        for (size_t j = i + 1; j < shortcuts.size(); ++j) {
            const auto& a = shortcuts[i];
            const auto& b = shortcuts[j];

            // Check if shortcuts conflict (same key, modifiers, and overlapping context)
            if (a.key == b.key && a.modifiers == b.modifiers) {
                // Check context overlap
                bool contextOverlap = (a.context == b.context) ||
                                     (a.context == ShortcutContext::Global) ||
                                     (b.context == ShortcutContext::Global);
                if (contextOverlap) {
                    conflicts.emplace_back(a.action, b.action);
                }
            }
        }
    }

    return conflicts;
}

void EditorSettings::ResetShortcutsToDefaults() {
    m_settings.input.shortcuts = m_defaultSettings.input.shortcuts;
    m_dirty = true;
    NotifyCategoryChange(SettingsCategory::Input);
}

// =============================================================================
// Validation
// =============================================================================

SettingsValidationResult EditorSettings::Validate() const {
    SettingsValidationResult result;

    // Validate general settings
    if (m_settings.general.autoSaveIntervalMinutes < 1) {
        result.AddWarning("Auto-save interval is less than 1 minute");
    }
    if (m_settings.general.undoHistorySize < 10) {
        result.AddWarning("Undo history size is very small");
    }
    if (m_settings.general.undoHistorySize > 1000) {
        result.AddWarning("Large undo history may consume significant memory");
    }

    // Validate appearance settings
    if (m_settings.appearance.fontSize < 8.0f || m_settings.appearance.fontSize > 32.0f) {
        result.AddError("Font size must be between 8 and 32");
    }
    if (m_settings.appearance.windowOpacity < 0.5f) {
        result.AddWarning("Window opacity is very low");
    }

    // Validate viewport settings
    if (m_settings.viewport.gridSize <= 0.0f) {
        result.AddError("Grid size must be positive");
    }
    if (m_settings.viewport.nearClipPlane >= m_settings.viewport.farClipPlane) {
        result.AddError("Near clip plane must be less than far clip plane");
    }
    if (m_settings.viewport.fieldOfView < 10.0f || m_settings.viewport.fieldOfView > 170.0f) {
        result.AddError("Field of view must be between 10 and 170 degrees");
    }

    // Validate input settings
    if (m_settings.input.mouseSensitivity <= 0.0f) {
        result.AddError("Mouse sensitivity must be positive");
    }
    if (m_settings.input.doubleClickTime <= 0.0f) {
        result.AddError("Double-click time must be positive");
    }

    // Validate performance settings
    if (m_settings.performance.maxTextureSize < 256) {
        result.AddWarning("Max texture size is very low");
    }
    if (m_settings.performance.thumbnailCacheSizeMB < 16) {
        result.AddWarning("Thumbnail cache size is very small");
    }

    // Validate paths
    if (!m_settings.paths.defaultProjectPath.empty() && !ValidatePath(m_settings.paths.defaultProjectPath, false)) {
        result.AddWarning("Default project path may not be valid");
    }

    // Check for shortcut conflicts
    auto conflicts = FindShortcutConflicts();
    for (const auto& [action1, action2] : conflicts) {
        result.AddConflict(action1, action2);
    }

    return result;
}

bool EditorSettings::ValidatePath(const std::string& path, bool mustExist) const {
    if (path.empty()) return true;

    try {
        std::filesystem::path fsPath(path);

        if (mustExist) {
            return std::filesystem::exists(fsPath);
        }

        // Check if parent directory exists or can be created
        std::filesystem::path parent = fsPath.parent_path();
        return parent.empty() || std::filesystem::exists(parent) ||
               std::filesystem::exists(parent.parent_path());
    }
    catch (...) {
        return false;
    }
}

// =============================================================================
// Import/Export
// =============================================================================

std::optional<void> EditorSettings::Export(const std::string& filepath) const {
    try {
        nlohmann::json json = m_settings.ToJson();
        json["exportedAt"] = std::time(nullptr);

        std::ofstream file(filepath);
        if (!file.is_open()) {
            return std::nullopt;
        }

        file << json.dump(4);
        file.close();

        LOG_INFO("Exported editor settings to: {}", filepath);
        return {};
    }
    catch (const std::exception& e) {
        return std::unexpected(std::string("Failed to export settings: ") + e.what());
    }
}

std::optional<void> EditorSettings::Import(const std::string& filepath, bool merge) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return std::nullopt;
        }

        nlohmann::json json;
        file >> json;
        file.close();

        // Check version compatibility
        int fileVersion = json.value("version", 1);
        if (fileVersion > CompleteEditorSettings::CURRENT_VERSION) {
            return std::nullopt;
        }

        if (fileVersion < CompleteEditorSettings::CURRENT_VERSION) {
            json = MigrateSettings(fileVersion, json);
        }

        if (merge) {
            // Merge with existing settings
            CompleteEditorSettings imported = CompleteEditorSettings::FromJson(json);

            // Only override non-default values (simplified merge)
            // In a full implementation, you'd compare each field
            m_settings = imported;
        }
        else {
            m_settings = CompleteEditorSettings::FromJson(json);
        }

        m_dirty = true;

        // Notify all categories
        for (int i = 0; i <= static_cast<int>(SettingsCategory::Plugins); ++i) {
            NotifyCategoryChange(static_cast<SettingsCategory>(i));
        }

        LOG_INFO("Imported editor settings from: {}", filepath);
        return {};
    }
    catch (const std::exception& e) {
        return std::unexpected(std::string("Failed to import settings: ") + e.what());
    }
}

// =============================================================================
// Migration
// =============================================================================

nlohmann::json EditorSettings::MigrateSettings(int oldVersion, const nlohmann::json& json) const {
    nlohmann::json migrated = json;

    // Version 0 -> 1 migration
    if (oldVersion < 1) {
        // Add any missing fields with defaults
        if (!migrated.contains("general")) {
            migrated["general"] = GeneralSettings{}.ToJson();
        }
        if (!migrated.contains("appearance")) {
            migrated["appearance"] = AppearanceSettings{}.ToJson();
        }
        // ... etc for other categories
    }

    // Future migrations would go here:
    // if (oldVersion < 2) { ... }

    migrated["version"] = CompleteEditorSettings::CURRENT_VERSION;
    return migrated;
}

// =============================================================================
// Change Notifications
// =============================================================================

uint64_t EditorSettings::RegisterChangeCallback(ChangeCallback callback) {
    uint64_t id = m_nextCallbackId++;
    m_callbacks.push_back({id, std::nullopt, std::move(callback)});
    return id;
}

uint64_t EditorSettings::RegisterCategoryCallback(SettingsCategory category, CategoryChangeCallback callback) {
    uint64_t id = m_nextCallbackId++;
    m_callbacks.push_back({id, category, std::move(callback)});
    return id;
}

void EditorSettings::UnregisterCallback(uint64_t callbackId) {
    m_callbacks.erase(
        std::remove_if(m_callbacks.begin(), m_callbacks.end(),
            [callbackId](const CallbackEntry& entry) { return entry.id == callbackId; }),
        m_callbacks.end()
    );
}

void EditorSettings::NotifyChange(const std::string& key) {
    for (const auto& entry : m_callbacks) {
        if (!entry.category.has_value()) {
            auto* callback = std::get_if<ChangeCallback>(&entry.callback);
            if (callback && *callback) {
                (*callback)(key);
            }
        }
    }
}

void EditorSettings::NotifyCategoryChange(SettingsCategory category) {
    for (const auto& entry : m_callbacks) {
        if (entry.category.has_value() && entry.category.value() == category) {
            auto* callback = std::get_if<CategoryChangeCallback>(&entry.callback);
            if (callback && *callback) {
                (*callback)(category);
            }
        }
    }
}

// =============================================================================
// Template Specializations
// =============================================================================

template<>
bool EditorSettings::Get<bool>(const std::string& key, const bool& defaultValue) const {
    // Parse key and return appropriate value
    if (key == "general.autoSaveEnabled") return m_settings.general.autoSaveEnabled;
    if (key == "general.showWelcomeOnStartup") return m_settings.general.showWelcomeOnStartup;
    if (key == "general.confirmOnExit") return m_settings.general.confirmOnExit;
    if (key == "general.reopenLastProject") return m_settings.general.reopenLastProject;
    if (key == "general.checkForUpdates") return m_settings.general.checkForUpdates;
    if (key == "appearance.showToolbarText") return m_settings.appearance.showToolbarText;
    if (key == "appearance.useNativeWindowFrame") return m_settings.appearance.useNativeWindowFrame;
    if (key == "appearance.animateTransitions") return m_settings.appearance.animateTransitions;
    if (key == "viewport.showFPS") return m_settings.viewport.showFPS;
    if (key == "viewport.showStats") return m_settings.viewport.showStats;
    if (key == "viewport.showGrid") return m_settings.viewport.showGrid;
    if (key == "viewport.showAxisGizmo") return m_settings.viewport.showAxisGizmo;
    if (key == "viewport.showWorldOrigin") return m_settings.viewport.showWorldOrigin;
    if (key == "input.invertMouseY") return m_settings.input.invertMouseY;
    if (key == "input.invertMouseX") return m_settings.input.invertMouseX;
    if (key == "input.smoothCamera") return m_settings.input.smoothCamera;
    if (key == "input.enableGamepad") return m_settings.input.enableGamepad;
    if (key == "performance.enableVSync") return m_settings.performance.enableVSync;
    if (key == "performance.enableAsyncLoading") return m_settings.performance.enableAsyncLoading;
    if (key == "performance.enableTextureStreaming") return m_settings.performance.enableTextureStreaming;
    if (key == "performance.enableEditorProfiling") return m_settings.performance.enableEditorProfiling;
    if (key == "performance.lowPowerMode") return m_settings.performance.lowPowerMode;
    if (key == "paths.useRelativePaths") return m_settings.paths.useRelativePaths;
    if (key == "plugins.autoLoadPlugins") return m_settings.plugins.autoLoadPlugins;
    if (key == "plugins.sandboxPlugins") return m_settings.plugins.sandboxPlugins;
    return defaultValue;
}

template<>
int EditorSettings::Get<int>(const std::string& key, const int& defaultValue) const {
    if (key == "general.autoSaveIntervalMinutes") return m_settings.general.autoSaveIntervalMinutes;
    if (key == "general.undoHistorySize") return m_settings.general.undoHistorySize;
    if (key == "general.recentProjectsMax") return m_settings.general.recentProjectsMax;
    if (key == "viewport.gridSubdivisions") return m_settings.viewport.gridSubdivisions;
    if (key == "viewport.maxFPS") return m_settings.viewport.maxFPS;
    if (key == "performance.maxTextureSize") return m_settings.performance.maxTextureSize;
    if (key == "performance.gpuMemoryLimitMB") return m_settings.performance.gpuMemoryLimitMB;
    if (key == "performance.workerThreadCount") return m_settings.performance.workerThreadCount;
    if (key == "performance.thumbnailCacheSizeMB") return m_settings.performance.thumbnailCacheSizeMB;
    if (key == "performance.targetEditorFPS") return m_settings.performance.targetEditorFPS;
    return defaultValue;
}

template<>
float EditorSettings::Get<float>(const std::string& key, const float& defaultValue) const {
    if (key == "appearance.fontSize") return m_settings.appearance.fontSize;
    if (key == "appearance.panelBorderWidth") return m_settings.appearance.panelBorderWidth;
    if (key == "appearance.windowOpacity") return m_settings.appearance.windowOpacity;
    if (key == "appearance.animationSpeed") return m_settings.appearance.animationSpeed;
    if (key == "viewport.gridSize") return m_settings.viewport.gridSize;
    if (key == "viewport.gizmoSize") return m_settings.viewport.gizmoSize;
    if (key == "viewport.nearClipPlane") return m_settings.viewport.nearClipPlane;
    if (key == "viewport.farClipPlane") return m_settings.viewport.farClipPlane;
    if (key == "viewport.fieldOfView") return m_settings.viewport.fieldOfView;
    if (key == "input.mouseSensitivity") return m_settings.input.mouseSensitivity;
    if (key == "input.scrollSpeed") return m_settings.input.scrollSpeed;
    if (key == "input.panSpeed") return m_settings.input.panSpeed;
    if (key == "input.orbitSpeed") return m_settings.input.orbitSpeed;
    if (key == "input.zoomSpeed") return m_settings.input.zoomSpeed;
    if (key == "input.cameraSmoothness") return m_settings.input.cameraSmoothness;
    if (key == "input.gamepadDeadzone") return m_settings.input.gamepadDeadzone;
    if (key == "input.doubleClickTime") return m_settings.input.doubleClickTime;
    if (key == "input.dragThreshold") return m_settings.input.dragThreshold;
    if (key == "performance.lodBias") return m_settings.performance.lodBias;
    return defaultValue;
}

template<>
std::string EditorSettings::Get<std::string>(const std::string& key, const std::string& defaultValue) const {
    if (key == "general.language") return m_settings.general.language;
    if (key == "general.dateFormat") return m_settings.general.dateFormat;
    if (key == "appearance.customThemePath") return m_settings.appearance.customThemePath;
    if (key == "paths.defaultProjectPath") return m_settings.paths.defaultProjectPath;
    if (key == "paths.tempDirectory") return m_settings.paths.tempDirectory;
    if (key == "paths.screenshotDirectory") return m_settings.paths.screenshotDirectory;
    if (key == "paths.logDirectory") return m_settings.paths.logDirectory;
    if (key == "paths.autosaveDirectory") return m_settings.paths.autosaveDirectory;
    return defaultValue;
}

template<>
glm::vec4 EditorSettings::Get<glm::vec4>(const std::string& key, const glm::vec4& defaultValue) const {
    if (key == "appearance.accentColor") return m_settings.appearance.accentColor;
    if (key == "viewport.gridColor") return m_settings.viewport.gridColor;
    if (key == "viewport.backgroundColor") return m_settings.viewport.backgroundColor;
    if (key == "viewport.selectionColor") return m_settings.viewport.selectionColor;
    if (key == "viewport.selectionHighlightColor") return m_settings.viewport.selectionHighlightColor;
    return defaultValue;
}

template<>
void EditorSettings::Set<bool>(const std::string& key, const bool& value) {
    if (key == "general.autoSaveEnabled") { m_settings.general.autoSaveEnabled = value; }
    else if (key == "general.showWelcomeOnStartup") { m_settings.general.showWelcomeOnStartup = value; }
    else if (key == "general.confirmOnExit") { m_settings.general.confirmOnExit = value; }
    else if (key == "general.reopenLastProject") { m_settings.general.reopenLastProject = value; }
    else if (key == "general.checkForUpdates") { m_settings.general.checkForUpdates = value; }
    else if (key == "appearance.showToolbarText") { m_settings.appearance.showToolbarText = value; }
    else if (key == "appearance.useNativeWindowFrame") { m_settings.appearance.useNativeWindowFrame = value; }
    else if (key == "appearance.animateTransitions") { m_settings.appearance.animateTransitions = value; }
    else if (key == "viewport.showFPS") { m_settings.viewport.showFPS = value; }
    else if (key == "viewport.showStats") { m_settings.viewport.showStats = value; }
    else if (key == "viewport.showGrid") { m_settings.viewport.showGrid = value; }
    else if (key == "viewport.showAxisGizmo") { m_settings.viewport.showAxisGizmo = value; }
    else if (key == "viewport.showWorldOrigin") { m_settings.viewport.showWorldOrigin = value; }
    else if (key == "input.invertMouseY") { m_settings.input.invertMouseY = value; }
    else if (key == "input.invertMouseX") { m_settings.input.invertMouseX = value; }
    else if (key == "input.smoothCamera") { m_settings.input.smoothCamera = value; }
    else if (key == "input.enableGamepad") { m_settings.input.enableGamepad = value; }
    else if (key == "performance.enableVSync") { m_settings.performance.enableVSync = value; }
    else if (key == "performance.enableAsyncLoading") { m_settings.performance.enableAsyncLoading = value; }
    else if (key == "performance.enableTextureStreaming") { m_settings.performance.enableTextureStreaming = value; }
    else if (key == "performance.enableEditorProfiling") { m_settings.performance.enableEditorProfiling = value; }
    else if (key == "performance.lowPowerMode") { m_settings.performance.lowPowerMode = value; }
    else if (key == "paths.useRelativePaths") { m_settings.paths.useRelativePaths = value; }
    else if (key == "plugins.autoLoadPlugins") { m_settings.plugins.autoLoadPlugins = value; }
    else if (key == "plugins.sandboxPlugins") { m_settings.plugins.sandboxPlugins = value; }
    else { return; }

    m_dirty = true;
    NotifyChange(key);
}

template<>
void EditorSettings::Set<int>(const std::string& key, const int& value) {
    if (key == "general.autoSaveIntervalMinutes") { m_settings.general.autoSaveIntervalMinutes = value; }
    else if (key == "general.undoHistorySize") { m_settings.general.undoHistorySize = value; }
    else if (key == "general.recentProjectsMax") { m_settings.general.recentProjectsMax = value; }
    else if (key == "viewport.gridSubdivisions") { m_settings.viewport.gridSubdivisions = value; }
    else if (key == "viewport.maxFPS") { m_settings.viewport.maxFPS = value; }
    else if (key == "performance.maxTextureSize") { m_settings.performance.maxTextureSize = value; }
    else if (key == "performance.gpuMemoryLimitMB") { m_settings.performance.gpuMemoryLimitMB = value; }
    else if (key == "performance.workerThreadCount") { m_settings.performance.workerThreadCount = value; }
    else if (key == "performance.thumbnailCacheSizeMB") { m_settings.performance.thumbnailCacheSizeMB = value; }
    else if (key == "performance.targetEditorFPS") { m_settings.performance.targetEditorFPS = value; }
    else { return; }

    m_dirty = true;
    NotifyChange(key);
}

template<>
void EditorSettings::Set<float>(const std::string& key, const float& value) {
    if (key == "appearance.fontSize") { m_settings.appearance.fontSize = value; }
    else if (key == "appearance.panelBorderWidth") { m_settings.appearance.panelBorderWidth = value; }
    else if (key == "appearance.windowOpacity") { m_settings.appearance.windowOpacity = value; }
    else if (key == "appearance.animationSpeed") { m_settings.appearance.animationSpeed = value; }
    else if (key == "viewport.gridSize") { m_settings.viewport.gridSize = value; }
    else if (key == "viewport.gizmoSize") { m_settings.viewport.gizmoSize = value; }
    else if (key == "viewport.nearClipPlane") { m_settings.viewport.nearClipPlane = value; }
    else if (key == "viewport.farClipPlane") { m_settings.viewport.farClipPlane = value; }
    else if (key == "viewport.fieldOfView") { m_settings.viewport.fieldOfView = value; }
    else if (key == "input.mouseSensitivity") { m_settings.input.mouseSensitivity = value; }
    else if (key == "input.scrollSpeed") { m_settings.input.scrollSpeed = value; }
    else if (key == "input.panSpeed") { m_settings.input.panSpeed = value; }
    else if (key == "input.orbitSpeed") { m_settings.input.orbitSpeed = value; }
    else if (key == "input.zoomSpeed") { m_settings.input.zoomSpeed = value; }
    else if (key == "input.cameraSmoothness") { m_settings.input.cameraSmoothness = value; }
    else if (key == "input.gamepadDeadzone") { m_settings.input.gamepadDeadzone = value; }
    else if (key == "input.doubleClickTime") { m_settings.input.doubleClickTime = value; }
    else if (key == "input.dragThreshold") { m_settings.input.dragThreshold = value; }
    else if (key == "performance.lodBias") { m_settings.performance.lodBias = value; }
    else { return; }

    m_dirty = true;
    NotifyChange(key);
}

template<>
void EditorSettings::Set<std::string>(const std::string& key, const std::string& value) {
    if (key == "general.language") { m_settings.general.language = value; }
    else if (key == "general.dateFormat") { m_settings.general.dateFormat = value; }
    else if (key == "appearance.customThemePath") { m_settings.appearance.customThemePath = value; }
    else if (key == "paths.defaultProjectPath") { m_settings.paths.defaultProjectPath = value; }
    else if (key == "paths.tempDirectory") { m_settings.paths.tempDirectory = value; }
    else if (key == "paths.screenshotDirectory") { m_settings.paths.screenshotDirectory = value; }
    else if (key == "paths.logDirectory") { m_settings.paths.logDirectory = value; }
    else if (key == "paths.autosaveDirectory") { m_settings.paths.autosaveDirectory = value; }
    else { return; }

    m_dirty = true;
    NotifyChange(key);
}

template<>
void EditorSettings::Set<glm::vec4>(const std::string& key, const glm::vec4& value) {
    if (key == "appearance.accentColor") { m_settings.appearance.accentColor = value; }
    else if (key == "viewport.gridColor") { m_settings.viewport.gridColor = value; }
    else if (key == "viewport.backgroundColor") { m_settings.viewport.backgroundColor = value; }
    else if (key == "viewport.selectionColor") { m_settings.viewport.selectionColor = value; }
    else if (key == "viewport.selectionHighlightColor") { m_settings.viewport.selectionHighlightColor = value; }
    else { return; }

    m_dirty = true;
    NotifyChange(key);
}

// =============================================================================
// Utility
// =============================================================================

std::vector<std::string> EditorSettings::GetAllKeys() const {
    return {
        // General
        "general.autoSaveEnabled",
        "general.autoSaveIntervalMinutes",
        "general.undoHistorySize",
        "general.showWelcomeOnStartup",
        "general.language",
        "general.dateFormat",
        "general.confirmOnExit",
        "general.reopenLastProject",
        "general.recentProjectsMax",
        "general.checkForUpdates",
        // Appearance
        "appearance.theme",
        "appearance.accentColor",
        "appearance.fontSize",
        "appearance.iconSize",
        "appearance.showToolbarText",
        "appearance.panelBorderWidth",
        "appearance.windowOpacity",
        "appearance.useNativeWindowFrame",
        "appearance.animateTransitions",
        "appearance.animationSpeed",
        "appearance.customThemePath",
        // Viewport
        "viewport.defaultCameraMode",
        "viewport.gridSize",
        "viewport.gridSubdivisions",
        "viewport.gridColor",
        "viewport.backgroundColor",
        "viewport.gizmoSize",
        "viewport.selectionColor",
        "viewport.selectionHighlightColor",
        "viewport.antiAliasingMode",
        "viewport.maxFPS",
        "viewport.showFPS",
        "viewport.showStats",
        "viewport.showGrid",
        "viewport.showAxisGizmo",
        "viewport.showWorldOrigin",
        "viewport.nearClipPlane",
        "viewport.farClipPlane",
        "viewport.fieldOfView",
        // Input
        "input.mouseSensitivity",
        "input.scrollSpeed",
        "input.invertMouseY",
        "input.invertMouseX",
        "input.panSpeed",
        "input.orbitSpeed",
        "input.zoomSpeed",
        "input.smoothCamera",
        "input.cameraSmoothness",
        "input.enableGamepad",
        "input.gamepadDeadzone",
        "input.doubleClickTime",
        "input.dragThreshold",
        // Performance
        "performance.maxTextureSize",
        "performance.lodBias",
        "performance.shadowQuality",
        "performance.enableVSync",
        "performance.gpuMemoryLimitMB",
        "performance.workerThreadCount",
        "performance.enableAsyncLoading",
        "performance.enableTextureStreaming",
        "performance.thumbnailCacheSizeMB",
        "performance.enableEditorProfiling",
        "performance.lowPowerMode",
        "performance.targetEditorFPS",
        // Paths
        "paths.defaultProjectPath",
        "paths.tempDirectory",
        "paths.screenshotDirectory",
        "paths.logDirectory",
        "paths.autosaveDirectory",
        "paths.useRelativePaths",
        // Plugins
        "plugins.autoLoadPlugins",
        "plugins.sandboxPlugins"
    };
}

bool EditorSettings::HasKey(const std::string& key) const {
    auto keys = GetAllKeys();
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

std::string EditorSettings::GetSettingType(const std::string& key) const {
    // Determine type from key prefix and suffix
    if (key.find("enabled") != std::string::npos ||
        key.find("Enabled") != std::string::npos ||
        key.find("show") != std::string::npos ||
        key.find("Show") != std::string::npos ||
        key.find("invert") != std::string::npos ||
        key.find("enable") != std::string::npos ||
        key.find("use") != std::string::npos ||
        key.find("smooth") != std::string::npos ||
        key.find("sandbox") != std::string::npos ||
        key.find("autoLoad") != std::string::npos ||
        key.find("confirm") != std::string::npos ||
        key.find("reopen") != std::string::npos ||
        key.find("check") != std::string::npos) {
        return "bool";
    }

    if (key.find("Size") != std::string::npos ||
        key.find("Count") != std::string::npos ||
        key.find("Limit") != std::string::npos ||
        key.find("Max") != std::string::npos ||
        key.find("Subdivisions") != std::string::npos ||
        key.find("FPS") != std::string::npos ||
        key.find("Interval") != std::string::npos) {
        return "int";
    }

    if (key.find("Sensitivity") != std::string::npos ||
        key.find("Speed") != std::string::npos ||
        key.find("Bias") != std::string::npos ||
        key.find("Opacity") != std::string::npos ||
        key.find("Width") != std::string::npos ||
        key.find("Plane") != std::string::npos ||
        key.find("View") != std::string::npos ||
        key.find("Smoothness") != std::string::npos ||
        key.find("Deadzone") != std::string::npos ||
        key.find("Time") != std::string::npos ||
        key.find("Threshold") != std::string::npos ||
        key.find("gridSize") != std::string::npos ||
        key.find("gizmoSize") != std::string::npos) {
        return "float";
    }

    if (key.find("Color") != std::string::npos) {
        return "vec4";
    }

    if (key.find("Path") != std::string::npos ||
        key.find("Directory") != std::string::npos ||
        key.find("language") != std::string::npos ||
        key.find("Format") != std::string::npos) {
        return "string";
    }

    return "unknown";
}

// =============================================================================
// Enum String Conversions
// =============================================================================

const char* EditorThemePresetToString(EditorThemePreset preset) {
    switch (preset) {
        case EditorThemePreset::Dark:   return "Dark";
        case EditorThemePreset::Light:  return "Light";
        case EditorThemePreset::Custom: return "Custom";
        default:                        return "Dark";
    }
}

const char* IconSizeToString(IconSize size) {
    switch (size) {
        case IconSize::Small:  return "Small";
        case IconSize::Medium: return "Medium";
        case IconSize::Large:  return "Large";
        default:               return "Medium";
    }
}

const char* DefaultCameraModeToString(DefaultCameraMode mode) {
    switch (mode) {
        case DefaultCameraMode::Perspective:  return "Perspective";
        case DefaultCameraMode::Orthographic: return "Orthographic";
        case DefaultCameraMode::Top:          return "Top";
        case DefaultCameraMode::Front:        return "Front";
        case DefaultCameraMode::Side:         return "Side";
        default:                              return "Perspective";
    }
}

const char* AntiAliasingModeToString(AntiAliasingMode mode) {
    switch (mode) {
        case AntiAliasingMode::None:    return "None";
        case AntiAliasingMode::FXAA:    return "FXAA";
        case AntiAliasingMode::MSAA_2x: return "MSAA 2x";
        case AntiAliasingMode::MSAA_4x: return "MSAA 4x";
        case AntiAliasingMode::MSAA_8x: return "MSAA 8x";
        case AntiAliasingMode::TAA:     return "TAA";
        default:                        return "None";
    }
}

const char* ShadowQualityPresetToString(ShadowQualityPreset quality) {
    switch (quality) {
        case ShadowQualityPreset::Off:    return "Off";
        case ShadowQualityPreset::Low:    return "Low";
        case ShadowQualityPreset::Medium: return "Medium";
        case ShadowQualityPreset::High:   return "High";
        case ShadowQualityPreset::Ultra:  return "Ultra";
        default:                          return "Medium";
    }
}

const char* ShortcutContextToString(ShortcutContext context) {
    switch (context) {
        case ShortcutContext::Global:   return "Global";
        case ShortcutContext::Viewport: return "Viewport";
        case ShortcutContext::Panel:    return "Panel";
        case ShortcutContext::TextEdit: return "TextEdit";
        default:                        return "Global";
    }
}

const char* KeyModifiersToString(KeyModifiers mods) {
    static thread_local std::string result;
    result.clear();

    if (mods & KeyModifiers::Ctrl)  result += "Ctrl+";
    if (mods & KeyModifiers::Shift) result += "Shift+";
    if (mods & KeyModifiers::Alt)   result += "Alt+";
    if (mods & KeyModifiers::Super) {
#ifdef _WIN32
        result += "Win+";
#else
        result += "Cmd+";
#endif
    }

    if (!result.empty() && result.back() == '+') {
        result.pop_back();
    }

    return result.empty() ? "None" : result.c_str();
}

EditorThemePreset StringToEditorThemePreset(const std::string& str) {
    if (str == "Light")  return EditorThemePreset::Light;
    if (str == "Custom") return EditorThemePreset::Custom;
    return EditorThemePreset::Dark;
}

IconSize StringToIconSize(const std::string& str) {
    if (str == "Small") return IconSize::Small;
    if (str == "Large") return IconSize::Large;
    return IconSize::Medium;
}

DefaultCameraMode StringToDefaultCameraMode(const std::string& str) {
    if (str == "Orthographic") return DefaultCameraMode::Orthographic;
    if (str == "Top")          return DefaultCameraMode::Top;
    if (str == "Front")        return DefaultCameraMode::Front;
    if (str == "Side")         return DefaultCameraMode::Side;
    return DefaultCameraMode::Perspective;
}

AntiAliasingMode StringToAntiAliasingMode(const std::string& str) {
    if (str == "FXAA")    return AntiAliasingMode::FXAA;
    if (str == "MSAA 2x") return AntiAliasingMode::MSAA_2x;
    if (str == "MSAA 4x") return AntiAliasingMode::MSAA_4x;
    if (str == "MSAA 8x") return AntiAliasingMode::MSAA_8x;
    if (str == "TAA")     return AntiAliasingMode::TAA;
    return AntiAliasingMode::None;
}

ShadowQualityPreset StringToShadowQualityPreset(const std::string& str) {
    if (str == "Off")    return ShadowQualityPreset::Off;
    if (str == "Low")    return ShadowQualityPreset::Low;
    if (str == "Medium") return ShadowQualityPreset::Medium;
    if (str == "High")   return ShadowQualityPreset::High;
    if (str == "Ultra")  return ShadowQualityPreset::Ultra;
    return ShadowQualityPreset::Medium;
}

ShortcutContext StringToShortcutContext(const std::string& str) {
    if (str == "Viewport") return ShortcutContext::Viewport;
    if (str == "Panel")    return ShortcutContext::Panel;
    if (str == "TextEdit") return ShortcutContext::TextEdit;
    return ShortcutContext::Global;
}

// =============================================================================
// Key Name Mappings
// =============================================================================

const char* GetKeyName(int keyCode) {
    // Common key mappings (GLFW-style key codes)
    switch (keyCode) {
        case 32:  return "Space";
        case 39:  return "'";
        case 44:  return ",";
        case 45:  return "-";
        case 46:  return ".";
        case 47:  return "/";
        case 48:  return "0";
        case 49:  return "1";
        case 50:  return "2";
        case 51:  return "3";
        case 52:  return "4";
        case 53:  return "5";
        case 54:  return "6";
        case 55:  return "7";
        case 56:  return "8";
        case 57:  return "9";
        case 59:  return ";";
        case 61:  return "=";
        case 65:  return "A";
        case 66:  return "B";
        case 67:  return "C";
        case 68:  return "D";
        case 69:  return "E";
        case 70:  return "F";
        case 71:  return "G";
        case 72:  return "H";
        case 73:  return "I";
        case 74:  return "J";
        case 75:  return "K";
        case 76:  return "L";
        case 77:  return "M";
        case 78:  return "N";
        case 79:  return "O";
        case 80:  return "P";
        case 81:  return "Q";
        case 82:  return "R";
        case 83:  return "S";
        case 84:  return "T";
        case 85:  return "U";
        case 86:  return "V";
        case 87:  return "W";
        case 88:  return "X";
        case 89:  return "Y";
        case 90:  return "Z";
        case 91:  return "[";
        case 92:  return "\\";
        case 93:  return "]";
        case 96:  return "`";
        case 256: return "Escape";
        case 257: return "Enter";
        case 258: return "Tab";
        case 259: return "Backspace";
        case 260: return "Insert";
        case 261: return "Delete";
        case 262: return "Right";
        case 263: return "Left";
        case 264: return "Down";
        case 265: return "Up";
        case 266: return "PageUp";
        case 267: return "PageDown";
        case 268: return "Home";
        case 269: return "End";
        case 280: return "CapsLock";
        case 281: return "ScrollLock";
        case 282: return "NumLock";
        case 283: return "PrintScreen";
        case 284: return "Pause";
        case 290: return "F1";
        case 291: return "F2";
        case 292: return "F3";
        case 293: return "F4";
        case 294: return "F5";
        case 295: return "F6";
        case 296: return "F7";
        case 297: return "F8";
        case 298: return "F9";
        case 299: return "F10";
        case 300: return "F11";
        case 301: return "F12";
        default:
            if (keyCode >= 'A' && keyCode <= 'Z') {
                static char buf[2] = {0};
                buf[0] = static_cast<char>(keyCode);
                return buf;
            }
            return "Unknown";
    }
}

int GetKeyCode(const std::string& keyName) {
    if (keyName == "Space")       return 32;
    if (keyName == "'")           return 39;
    if (keyName == ",")           return 44;
    if (keyName == "-")           return 45;
    if (keyName == ".")           return 46;
    if (keyName == "/")           return 47;
    if (keyName == "0")           return 48;
    if (keyName == "1")           return 49;
    if (keyName == "2")           return 50;
    if (keyName == "3")           return 51;
    if (keyName == "4")           return 52;
    if (keyName == "5")           return 53;
    if (keyName == "6")           return 54;
    if (keyName == "7")           return 55;
    if (keyName == "8")           return 56;
    if (keyName == "9")           return 57;
    if (keyName == ";")           return 59;
    if (keyName == "=")           return 61;
    if (keyName == "[")           return 91;
    if (keyName == "\\")          return 92;
    if (keyName == "]")           return 93;
    if (keyName == "`")           return 96;
    if (keyName == "Escape")      return 256;
    if (keyName == "Enter")       return 257;
    if (keyName == "Tab")         return 258;
    if (keyName == "Backspace")   return 259;
    if (keyName == "Insert")      return 260;
    if (keyName == "Delete")      return 261;
    if (keyName == "Right")       return 262;
    if (keyName == "Left")        return 263;
    if (keyName == "Down")        return 264;
    if (keyName == "Up")          return 265;
    if (keyName == "PageUp")      return 266;
    if (keyName == "PageDown")    return 267;
    if (keyName == "Home")        return 268;
    if (keyName == "End")         return 269;
    if (keyName == "CapsLock")    return 280;
    if (keyName == "ScrollLock")  return 281;
    if (keyName == "NumLock")     return 282;
    if (keyName == "PrintScreen") return 283;
    if (keyName == "Pause")       return 284;
    if (keyName == "F1")          return 290;
    if (keyName == "F2")          return 291;
    if (keyName == "F3")          return 292;
    if (keyName == "F4")          return 293;
    if (keyName == "F5")          return 294;
    if (keyName == "F6")          return 295;
    if (keyName == "F7")          return 296;
    if (keyName == "F8")          return 297;
    if (keyName == "F9")          return 298;
    if (keyName == "F10")         return 299;
    if (keyName == "F11")         return 300;
    if (keyName == "F12")         return 301;

    // Single character keys
    if (keyName.length() == 1) {
        char c = keyName[0];
        if (c >= 'A' && c <= 'Z') return c;
        if (c >= 'a' && c <= 'z') return c - 32;  // Convert to uppercase
    }

    return 0;  // Unknown key
}

} // namespace Nova
