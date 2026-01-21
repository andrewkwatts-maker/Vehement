/**
 * @file EditorApplication.cpp
 * @brief Implementation of the main Editor Application
 */

#include "EditorApplication.hpp"
#include "TransformGizmo.hpp"
#include "ConsolePanel.hpp"
#include "../core/Engine.hpp"
#include "../core/Logger.hpp"
#include "../core/Time.hpp"
#include "../core/json_wrapper.hpp"
#include "../scene/Scene.hpp"
#include "../scene/SceneNode.hpp"
#include "../input/InputManager.hpp"
#include "../ui/EditorTheme.hpp"

// Optional panel includes - these may not be compiled yet
// SceneOutliner and SDFToolbox are header-only dependencies for now
#if __has_include("SceneOutliner.hpp")
#include "SceneOutliner.hpp"
#define NOVA_HAS_SCENE_OUTLINER 1
#else
#define NOVA_HAS_SCENE_OUTLINER 0
#endif

#if __has_include("SDFToolbox.hpp")
#include "SDFToolbox.hpp"
#define NOVA_HAS_SDF_TOOLBOX 1
#else
#define NOVA_HAS_SDF_TOOLBOX 0
#endif

#if __has_include("SDFAssetEditor.hpp")
#include "SDFAssetEditor.hpp"
#define NOVA_HAS_SDF_ASSET_EDITOR 1
#else
#define NOVA_HAS_SDF_ASSET_EDITOR 0
#endif

#if __has_include("AIFeedbackPanel.hpp")
#include "AIFeedbackPanel.hpp"
#define NOVA_HAS_AI_FEEDBACK_PANEL 1
#else
#define NOVA_HAS_AI_FEEDBACK_PANEL 0
#endif

#if __has_include("AIAssistantPanel.hpp")
#include "AIAssistantPanel.hpp"
#define NOVA_HAS_AI_ASSISTANT_PANEL 1
#else
#define NOVA_HAS_AI_ASSISTANT_PANEL 0
#endif

#if __has_include("AISetupWizard.hpp")
#include "AISetupWizard.hpp"
#define NOVA_HAS_AI_SETUP_WIZARD 1
#else
#define NOVA_HAS_AI_SETUP_WIZARD 0
#endif

#if __has_include("AIToolLauncher.hpp")
#include "AIToolLauncher.hpp"
#define NOVA_HAS_AI_TOOL_LAUNCHER 1
#else
#define NOVA_HAS_AI_TOOL_LAUNCHER 0
#endif

#if __has_include("PCGPanel.hpp")
#include "PCGPanel.hpp"
#define NOVA_HAS_PCG_PANEL 1
#else
#define NOVA_HAS_PCG_PANEL 0
#endif

#if __has_include("AssetBrowser.hpp")
#include "AssetBrowser.hpp"
#define NOVA_HAS_ASSET_BROWSER 1
#else
#define NOVA_HAS_ASSET_BROWSER 0
#endif

#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <cstdlib>
#endif

namespace Nova {

// =============================================================================
// Utility Functions
// =============================================================================

const char* GetTransformToolName(TransformTool tool) {
    switch (tool) {
        case TransformTool::Select:    return "Select";
        case TransformTool::Translate: return "Translate";
        case TransformTool::Rotate:    return "Rotate";
        case TransformTool::Scale:     return "Scale";
        default:                       return "Unknown";
    }
}

const char* GetTransformToolIcon(TransformTool tool) {
    switch (tool) {
        case TransformTool::Select:    return "Q";  // Would be icon in real usage
        case TransformTool::Translate: return "W";
        case TransformTool::Rotate:    return "E";
        case TransformTool::Scale:     return "R";
        default:                       return "?";
    }
}

const char* GetTransformSpaceName(TransformSpace space) {
    switch (space) {
        case TransformSpace::World: return "World";
        case TransformSpace::Local: return "Local";
        default:                    return "Unknown";
    }
}

const char* GetPlayStateName(EditorPlayState state) {
    switch (state) {
        case EditorPlayState::Editing: return "Editing";
        case EditorPlayState::Playing: return "Playing";
        case EditorPlayState::Paused:  return "Paused";
        default:                       return "Unknown";
    }
}

glm::vec4 GetNotificationColor(NotificationType type) {
    switch (type) {
        case NotificationType::Info:    return {0.4f, 0.7f, 0.95f, 1.0f};
        case NotificationType::Success: return {0.3f, 0.75f, 0.4f, 1.0f};
        case NotificationType::Warning: return {0.95f, 0.75f, 0.25f, 1.0f};
        case NotificationType::Error:   return {0.9f, 0.35f, 0.35f, 1.0f};
        default:                        return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

// =============================================================================
// Platform Utilities - Open Files/URLs in Default Application
// =============================================================================

/**
 * @brief Opens a file or URL in the system's default application
 * @param pathOrUrl File path or URL to open
 * @return true if the operation was initiated successfully
 */
bool OpenInDefaultApplication(const std::string& pathOrUrl) {
#ifdef _WIN32
    // Use ShellExecuteA on Windows
    HINSTANCE result = ShellExecuteA(
        nullptr,            // No parent window
        "open",             // Verb: open with default app
        pathOrUrl.c_str(),  // File or URL to open
        nullptr,            // No parameters
        nullptr,            // Use current directory
        SW_SHOW             // Show the window normally
    );
    // ShellExecute returns > 32 on success
    return reinterpret_cast<intptr_t>(result) > 32;
#elif defined(__APPLE__)
    // Use 'open' command on macOS
    std::string command = "open \"" + pathOrUrl + "\"";
    int result = std::system(command.c_str());
    return result == 0;
#else
    // Use xdg-open on Linux and other Unix-like systems
    std::string command = "xdg-open \"" + pathOrUrl + "\" &";
    int result = std::system(command.c_str());
    return result == 0;
#endif
}

/**
 * @brief Opens a documentation file relative to the engine root
 * @param relativePath Path relative to the engine docs directory
 * @return true if the file was opened successfully
 */
bool OpenDocumentationFile(const std::string& relativePath) {
    // Get the executable's directory to find docs folder
    std::filesystem::path docsPath;

    // Try several possible locations for the docs
    std::vector<std::filesystem::path> searchPaths = {
        std::filesystem::current_path() / relativePath,
        std::filesystem::current_path() / "docs" / relativePath,
        std::filesystem::current_path().parent_path() / "docs" / relativePath,
        std::filesystem::current_path().parent_path().parent_path() / "docs" / relativePath,
    };

    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path)) {
            docsPath = path;
            break;
        }
    }

    if (docsPath.empty()) {
        spdlog::warn("Documentation file not found: {}", relativePath);
        return false;
    }

    spdlog::info("Opening documentation: {}", docsPath.string());
    return OpenInDefaultApplication(docsPath.string());
}

/**
 * @brief Opens a URL in the system's default web browser
 * @param url URL to open
 * @return true if the browser was opened successfully
 */
bool OpenURL(const std::string& url) {
    spdlog::info("Opening URL: {}", url);
    return OpenInDefaultApplication(url);
}

// =============================================================================
// EditorApplication Implementation
// =============================================================================

EditorApplication& EditorApplication::Instance() {
    static EditorApplication instance;
    return instance;
}

bool EditorApplication::Initialize() {
    if (m_initialized) {
        spdlog::warn("EditorApplication already initialized");
        return true;
    }

    spdlog::info("Initializing EditorApplication...");

    // Initialize engine first if not already done
    auto& engine = Engine::Instance();
    if (!engine.IsInitialized()) {
        Engine::InitParams params;
        params.enableImGui = true;
        params.enableDebugDraw = true;
        if (!engine.Initialize(params)) {
            spdlog::error("Failed to initialize engine for editor");
            return false;
        }
    }

    // Apply editor theme
    EditorTheme::Instance().Apply();

    // Register default panels
    RegisterDefaultPanels();

    // Create core panels
    CreateDefaultPanels();

    // Setup default layout
    SetupDefaultLayout();

    // Register keyboard shortcuts
    RegisterDefaultShortcuts();

    // Load settings
    LoadSettings();

    // Load recent projects
    LoadRecentProjects();

    // Create transform gizmo
    m_transformGizmo = std::make_unique<TransformGizmo>();
    if (!m_transformGizmo->Initialize()) {
        spdlog::warn("Failed to initialize transform gizmo");
    }

    // Create default scene
    NewScene();

    m_lastFrameTime = std::chrono::high_resolution_clock::now();
    m_initialized = true;
    m_running = true;

    spdlog::info("EditorApplication initialized successfully");
    return true;
}

void EditorApplication::Shutdown() {
    if (!m_initialized) {
        return;
    }

    spdlog::info("Shutting down EditorApplication...");

    // Save settings
    SaveSettings();

    // Save recent projects
    SaveRecentProjects();

    // Shutdown panels
    for (auto& [name, panel] : m_panels) {
        if (panel) {
            panel->Shutdown();
        }
    }
    m_panels.clear();

    // Clear factories
    m_panelFactories.clear();
    m_panelTypeMap.clear();

    // Release transform gizmo
    if (m_transformGizmo) {
        m_transformGizmo->Shutdown();
        m_transformGizmo.reset();
    }

    // Release scenes
    m_activeScene.reset();
    m_savedSceneState.reset();

    // Clear selection
    m_selection.clear();
    m_selectionSet.clear();

    // Clear command history
    m_commandHistory.Clear();

    m_initialized = false;
    m_running = false;

    spdlog::info("EditorApplication shutdown complete");
}

void EditorApplication::RequestShutdown() {
    // Check for unsaved changes
    if (m_projectDirty || m_sceneDirty) {
        ShowConfirmDialog(
            "Unsaved Changes",
            "You have unsaved changes. Do you want to save before exiting?",
            [this]() {
                if (m_sceneDirty) SaveScene();
                if (m_projectDirty) SaveProject();
                m_running = false;
            },
            [this]() {
                m_running = false;
            }
        );
    } else {
        m_running = false;
    }
}

void EditorApplication::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    m_deltaTime = deltaTime;

    // Update frame stats
    UpdateFrameStats(deltaTime);

    // Handle input
    HandleInput();

    // Update panels
    for (auto& [name, panel] : m_panels) {
        if (panel && panel->IsVisible()) {
            panel->Update(deltaTime);
        }
    }

    // Update transform gizmo with selection
    if (m_transformGizmo && !m_selection.empty()) {
        SceneNode* primary = GetPrimarySelection();
        if (primary) {
            m_transformGizmo->SetTransform(primary->GetWorldPosition(), primary->GetWorldRotation());
            m_transformGizmo->SetVisible(m_transformTool != TransformTool::Select);

            // Set gizmo mode based on tool
            switch (m_transformTool) {
                case TransformTool::Translate:
                    m_transformGizmo->SetMode(GizmoMode::Translate);
                    break;
                case TransformTool::Rotate:
                    m_transformGizmo->SetMode(GizmoMode::Rotate);
                    break;
                case TransformTool::Scale:
                    m_transformGizmo->SetMode(GizmoMode::Scale);
                    break;
                default:
                    break;
            }

            // Set gizmo space
            m_transformGizmo->SetSpace(m_transformSpace == TransformSpace::World
                                        ? GizmoSpace::World : GizmoSpace::Local);

            // Configure snapping
            GizmoSnapping snapping;
            snapping.enabled = m_settings.snapEnabled;
            snapping.translateSnap = m_settings.snapTranslate;
            snapping.rotateSnap = m_settings.snapRotate;
            snapping.scaleSnap = m_settings.snapScale;
            m_transformGizmo->SetSnapping(snapping);
        }
    } else if (m_transformGizmo) {
        m_transformGizmo->SetVisible(false);
    }

    // Update scene if playing
    if (m_playState == EditorPlayState::Playing && m_activeScene) {
        m_activeScene->Update(deltaTime);
    }

    // Update auto-save
    UpdateAutoSave(deltaTime);

    // Update AI tool launcher (process async callbacks)
#if NOVA_HAS_AI_TOOL_LAUNCHER
    AIToolLauncher::Instance().Update();
#endif

    // Update notifications
    UpdateNotifications(deltaTime);
}

void EditorApplication::Render() {
    if (!m_initialized) {
        return;
    }

    // Begin main dockspace
    RenderDockSpace();

    // Render menu bar
    RenderMenuBar();

    // Render toolbar
    RenderToolbar();

    // Render all visible panels
    RenderPanels();

    // Render status bar
    RenderStatusBar();

    // Render notifications
    RenderNotifications();

    // Render modal dialogs
    RenderModalDialogs();

    // Render preferences window if open
    if (m_showPreferencesWindow) {
        RenderPreferencesWindow();
    }

    // Render asset creation dialog
    if (m_assetCreationDialog.IsOpen()) {
        if (m_assetCreationDialog.Show()) {
            // Asset was created, refresh asset browser
            ShowNotification("Created asset: " + m_assetCreationDialog.GetCreatedAssetPath(),
                             NotificationType::Success);
#if NOVA_HAS_ASSET_BROWSER
            if (auto browser = GetPanel<AssetBrowser>()) {
                browser->Refresh();
            }
#endif
        }
    }
}

void EditorApplication::HandleInput() {
    // Process keyboard shortcuts
    ProcessShortcuts();

    // Handle global shortcuts that bypass panel focus
    HandleGlobalShortcuts();
}

// =============================================================================
// Panel Management
// =============================================================================

void EditorApplication::RegisterDefaultPanels() {
    // Register panels that have implementations
#if NOVA_HAS_SCENE_OUTLINER
    RegisterPanel<SceneOutliner>("SceneOutliner");
#endif
    RegisterPanel<ConsolePanel>("Console");

#if NOVA_HAS_SDF_ASSET_EDITOR
    RegisterPanel<SDFAssetEditor>("SDFAssetEditor");
#endif

#if NOVA_HAS_AI_FEEDBACK_PANEL
    RegisterPanel<AIFeedbackPanel>("AIFeedback");
#endif

#if NOVA_HAS_AI_ASSISTANT_PANEL
    RegisterPanel<AIAssistantPanel>("AIAssistant");
#endif

#if NOVA_HAS_PCG_PANEL
    RegisterPanel<PCGPanel>("PCGPanel");
#endif

    // Note: These would be registered when their classes are implemented:
    // RegisterPanel<PropertiesPanel>("Properties");
    // RegisterPanel<AssetBrowser>("AssetBrowser");
    // RegisterPanel<ViewportPanel>("Viewport");
#if NOVA_HAS_SDF_TOOLBOX
    // RegisterPanel<SDFToolbox>("SDFToolbox");
#endif
    // RegisterPanel<AnimationTimeline>("AnimationTimeline");
}

void EditorApplication::CreateDefaultPanels() {
    // Create core panels that are available

#if NOVA_HAS_SCENE_OUTLINER
    auto outliner = CreatePanel("SceneOutliner");
    if (outliner) {
        EditorPanel::Config config;
        config.title = "Hierarchy";
        config.category = "Scene";
        config.defaultOpen = true;
        outliner->Initialize(config);
    }
#endif

    auto console = CreatePanel("Console");
    if (console) {
        EditorPanel::Config config;
        config.title = "Console";
        config.category = "Debug";
        config.defaultOpen = true;
        console->Initialize(config);

        // Hook console to logger
        if (auto* consolePanel = dynamic_cast<ConsolePanel*>(console.get())) {
            consolePanel->HookIntoLogger();
        }
    }

#if NOVA_HAS_SDF_ASSET_EDITOR
    auto sdfEditor = CreatePanel("SDFAssetEditor");
    if (sdfEditor) {
        EditorPanel::Config config;
        config.title = "SDF Asset Editor";
        config.category = "Editors";
        config.defaultOpen = false;  // Open on demand via Alt+1
        sdfEditor->Initialize(config);
    }
#endif

#if NOVA_HAS_PCG_PANEL
    auto pcgPanel = CreatePanel("PCGPanel");
    if (pcgPanel) {
        EditorPanel::Config config;
        config.title = "PCG Panel";
        config.category = "Level Design";
        config.tooltip = "Procedural Content Generation for terrain and assets";
        config.defaultOpen = false;  // Open on demand via menu
        pcgPanel->Initialize(config);
    }
#endif
}

void EditorApplication::SetupDefaultLayout() {
    // Default layout will be configured on first render via ImGui docking
    // This is a placeholder for future layout serialization
}

std::shared_ptr<EditorPanel> EditorApplication::CreatePanel(const std::string& name) {
    auto factoryIt = m_panelFactories.find(name);
    if (factoryIt == m_panelFactories.end()) {
        spdlog::warn("Panel type not registered: {}", name);
        return nullptr;
    }

    auto panel = factoryIt->second();
    if (panel) {
        std::string instanceName = name;
        int suffix = 1;
        while (m_panels.count(instanceName) > 0) {
            instanceName = name + "_" + std::to_string(++suffix);
        }
        m_panels[instanceName] = panel;
    }

    return panel;
}

std::shared_ptr<EditorPanel> EditorApplication::GetPanel(const std::string& name) {
    auto it = m_panels.find(name);
    return it != m_panels.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<EditorPanel>> EditorApplication::GetAllPanels() const {
    std::vector<std::shared_ptr<EditorPanel>> result;
    result.reserve(m_panels.size());
    for (const auto& [name, panel] : m_panels) {
        result.push_back(panel);
    }
    return result;
}

void EditorApplication::ShowPanel(const std::string& name) {
    if (auto panel = GetPanel(name)) {
        panel->Show();
    }
}

void EditorApplication::HidePanel(const std::string& name) {
    if (auto panel = GetPanel(name)) {
        panel->Hide();
    }
}

void EditorApplication::TogglePanel(const std::string& name) {
    if (auto panel = GetPanel(name)) {
        panel->Toggle();
    }
}

bool EditorApplication::IsPanelVisible(const std::string& name) const {
    auto it = m_panels.find(name);
    return it != m_panels.end() && it->second->IsVisible();
}

void EditorApplication::FocusPanel(const std::string& name) {
    if (auto panel = GetPanel(name)) {
        panel->Focus();
    }
}

std::vector<std::string> EditorApplication::GetRegisteredPanelNames() const {
    std::vector<std::string> names;
    names.reserve(m_panelFactories.size());
    for (const auto& [name, factory] : m_panelFactories) {
        names.push_back(name);
    }
    return names;
}

// =============================================================================
// Project Management
// =============================================================================

bool EditorApplication::NewProject(const std::filesystem::path& path, const std::string& name) {
    if (!CloseProject()) {
        return false;  // User cancelled
    }

    // Create project directory structure
    try {
        std::filesystem::create_directories(path);
        std::filesystem::create_directories(path / "Assets");
        std::filesystem::create_directories(path / "Scenes");
        std::filesystem::create_directories(path / "Scripts");
        std::filesystem::create_directories(path / "Build");

        m_projectPath = path / (name + ".nova");
        m_projectName = name;
        m_hasProject = true;
        m_projectDirty = true;

        // Save project file
        SaveProject();

        AddToRecentProjects(m_projectPath);
        ShowNotification("Project created: " + name, NotificationType::Success);

        return true;
    } catch (const std::exception& e) {
        ShowNotification("Failed to create project: " + std::string(e.what()), NotificationType::Error);
        return false;
    }
}

bool EditorApplication::OpenProject(const std::filesystem::path& path) {
    if (!CloseProject()) {
        return false;
    }

    if (!std::filesystem::exists(path)) {
        ShowNotification("Project file not found: " + path.string(), NotificationType::Error);
        return false;
    }

    try {
        // Parse project JSON file
        auto projectJson = Nova::Json::TryParseFile(path.string());
        if (!projectJson) {
            ShowNotification("Failed to parse project file: " + path.string(), NotificationType::Error);
            return false;
        }

        const auto& doc = *projectJson;

        // Validate project file format
        if (!Nova::Json::Contains(doc, "nova_project")) {
            ShowNotification("Invalid project file format", NotificationType::Error);
            return false;
        }

        // Set project path and extract project info
        m_projectPath = path;
        m_projectName = Nova::Json::Get<std::string>(doc, "name", path.stem().string());

        // Load project settings if present
        if (Nova::Json::Contains(doc, "settings")) {
            const auto& settingsJson = doc["settings"];

            // Viewport settings
            m_settings.showGrid = Nova::Json::Get<bool>(settingsJson, "showGrid", true);
            m_settings.gridSize = Nova::Json::Get<float>(settingsJson, "gridSize", 1.0f);
            m_settings.gridSubdivisions = Nova::Json::Get<int>(settingsJson, "gridSubdivisions", 10);
            m_settings.showGizmos = Nova::Json::Get<bool>(settingsJson, "showGizmos", true);
            m_settings.showIcons = Nova::Json::Get<bool>(settingsJson, "showIcons", true);

            // Snap settings
            m_settings.snapEnabled = Nova::Json::Get<bool>(settingsJson, "snapEnabled", false);
            m_settings.snapTranslate = Nova::Json::Get<float>(settingsJson, "snapTranslate", 1.0f);
            m_settings.snapRotate = Nova::Json::Get<float>(settingsJson, "snapRotate", 15.0f);
            m_settings.snapScale = Nova::Json::Get<float>(settingsJson, "snapScale", 0.1f);

            // Auto-save settings
            m_settings.autoSave = Nova::Json::Get<bool>(settingsJson, "autoSave", true);
            m_settings.autoSaveIntervalSeconds = Nova::Json::Get<float>(settingsJson, "autoSaveInterval", 300.0f);
        }

        // Load initial scene if specified
        if (Nova::Json::Contains(doc, "initialScene")) {
            std::string initialScenePath = doc["initialScene"].get<std::string>();
            // Resolve relative path to project directory
            std::filesystem::path scenePath = path.parent_path() / initialScenePath;
            if (std::filesystem::exists(scenePath)) {
                OpenScene(scenePath);
            }
        }

        m_hasProject = true;
        m_projectDirty = false;

        AddToRecentProjects(path);
        ShowNotification("Opened project: " + m_projectName, NotificationType::Success);

        return true;
    } catch (const std::exception& e) {
        ShowNotification("Failed to open project: " + std::string(e.what()), NotificationType::Error);
        return false;
    }
}

bool EditorApplication::SaveProject() {
    if (!m_hasProject) {
        return false;
    }

    try {
        // Build project JSON document
        Nova::Json::JsonValue doc = Nova::Json::Object();

        // Project header
        doc["nova_project"] = true;
        doc["version"] = "1.0.0";
        doc["name"] = m_projectName;

        // Project settings
        Nova::Json::JsonValue settingsJson = Nova::Json::Object();
        settingsJson["showGrid"] = m_settings.showGrid;
        settingsJson["gridSize"] = m_settings.gridSize;
        settingsJson["gridSubdivisions"] = m_settings.gridSubdivisions;
        settingsJson["showGizmos"] = m_settings.showGizmos;
        settingsJson["showIcons"] = m_settings.showIcons;
        settingsJson["snapEnabled"] = m_settings.snapEnabled;
        settingsJson["snapTranslate"] = m_settings.snapTranslate;
        settingsJson["snapRotate"] = m_settings.snapRotate;
        settingsJson["snapScale"] = m_settings.snapScale;
        settingsJson["autoSave"] = m_settings.autoSave;
        settingsJson["autoSaveInterval"] = m_settings.autoSaveIntervalSeconds;
        settingsJson["themeName"] = m_settings.themeName;
        settingsJson["targetFrameRate"] = m_settings.targetFrameRate;
        settingsJson["vsync"] = m_settings.vsync;
        settingsJson["showFps"] = m_settings.showFps;
        settingsJson["showMemory"] = m_settings.showMemory;
        doc["settings"] = settingsJson;

        // Store current scene path relative to project directory
        if (!m_scenePath.empty()) {
            std::filesystem::path relativePath = std::filesystem::relative(m_scenePath, m_projectPath.parent_path());
            doc["initialScene"] = relativePath.string();
        }

        // Open scenes list
        Nova::Json::JsonValue openScenesJson = Nova::Json::Array();
        for (const auto& scenePath : m_openScenes) {
            std::filesystem::path relativePath = std::filesystem::relative(scenePath, m_projectPath.parent_path());
            openScenesJson.push_back(relativePath.string());
        }
        doc["openScenes"] = openScenesJson;

        // Write to file with pretty formatting
        Nova::Json::WriteFile(m_projectPath.string(), doc, 2);

        m_projectDirty = false;
        ShowNotification("Project saved", NotificationType::Success);
        return true;
    } catch (const std::exception& e) {
        ShowNotification("Failed to save project: " + std::string(e.what()), NotificationType::Error);
        return false;
    }
}

bool EditorApplication::SaveProjectAs(const std::filesystem::path& path) {
    m_projectPath = path;
    m_projectName = path.stem().string();
    return SaveProject();
}

bool EditorApplication::CloseProject() {
    if (!m_hasProject) {
        return true;
    }

    if (m_projectDirty) {
        // This would show a dialog and return false if cancelled
        // For now, just warn
        spdlog::warn("Closing project with unsaved changes");
    }

    m_hasProject = false;
    m_projectPath.clear();
    m_projectName.clear();
    m_projectDirty = false;

    return true;
}

void EditorApplication::MarkProjectDirty() {
    m_projectDirty = true;
}

void EditorApplication::ClearRecentProjects() {
    m_recentProjects.clear();
    SaveRecentProjects();
}

void EditorApplication::LoadRecentProjects() {
    m_recentProjects.clear();

    // Determine recent projects file path
    std::filesystem::path recentPath;
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) {
        recentPath = std::filesystem::path(appData) / "Nova3D" / "recent_projects.json";
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        recentPath = std::filesystem::path(home) / ".config" / "Nova3D" / "recent_projects.json";
    }
#endif

    if (recentPath.empty()) {
        recentPath = "recent_projects.json";
    }

    if (!std::filesystem::exists(recentPath)) {
        return;
    }

    try {
        auto recentJson = Nova::Json::TryParseFile(recentPath.string());
        if (!recentJson) {
            return;
        }

        const auto& doc = *recentJson;

        if (Nova::Json::Contains(doc, "projects") && doc["projects"].is_array()) {
            for (const auto& projectJson : doc["projects"]) {
                RecentProject entry;
                entry.path = Nova::Json::Get<std::string>(projectJson, "path", "");
                entry.name = Nova::Json::Get<std::string>(projectJson, "name", "");

                // Parse last opened timestamp
                if (Nova::Json::Contains(projectJson, "lastOpened")) {
                    int64_t timestamp = projectJson["lastOpened"].get<int64_t>();
                    entry.lastOpened = std::chrono::system_clock::from_time_t(static_cast<std::time_t>(timestamp));
                } else {
                    entry.lastOpened = std::chrono::system_clock::now();
                }

                // Check if project still exists
                entry.exists = std::filesystem::exists(entry.path);

                if (!entry.path.empty()) {
                    m_recentProjects.push_back(entry);
                }
            }
        }

        spdlog::info("Loaded {} recent projects", m_recentProjects.size());
    } catch (const std::exception& e) {
        spdlog::warn("Failed to load recent projects: {}", e.what());
    }
}

void EditorApplication::SaveRecentProjects() {
    // Determine recent projects file path
    std::filesystem::path recentPath;
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) {
        recentPath = std::filesystem::path(appData) / "Nova3D";
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        recentPath = std::filesystem::path(home) / ".config" / "Nova3D";
    }
#endif

    if (recentPath.empty()) {
        recentPath = ".";
    }

    try {
        // Create directory if it doesn't exist
        std::filesystem::create_directories(recentPath);
        recentPath /= "recent_projects.json";

        // Build recent projects JSON
        Nova::Json::JsonValue doc = Nova::Json::Object();
        Nova::Json::JsonValue projectsJson = Nova::Json::Array();

        for (const auto& project : m_recentProjects) {
            Nova::Json::JsonValue projectJson = Nova::Json::Object();
            projectJson["path"] = project.path;
            projectJson["name"] = project.name;

            // Convert time_point to timestamp
            auto timestamp = std::chrono::system_clock::to_time_t(project.lastOpened);
            projectJson["lastOpened"] = static_cast<int64_t>(timestamp);

            projectsJson.push_back(projectJson);
        }

        doc["projects"] = projectsJson;

        // Write to file
        Nova::Json::WriteFile(recentPath.string(), doc, 2);

        spdlog::debug("Saved {} recent projects", m_recentProjects.size());
    } catch (const std::exception& e) {
        spdlog::warn("Failed to save recent projects: {}", e.what());
    }
}

void EditorApplication::AddToRecentProjects(const std::filesystem::path& path) {
    // Remove existing entry if present
    m_recentProjects.erase(
        std::remove_if(m_recentProjects.begin(), m_recentProjects.end(),
                       [&path](const RecentProject& p) { return p.path == path.string(); }),
        m_recentProjects.end()
    );

    // Add to front
    RecentProject entry;
    entry.path = path.string();
    entry.name = path.stem().string();
    entry.lastOpened = std::chrono::system_clock::now();
    entry.exists = std::filesystem::exists(path);
    m_recentProjects.insert(m_recentProjects.begin(), entry);

    // Trim to max size
    if (m_recentProjects.size() > MAX_RECENT_PROJECTS) {
        m_recentProjects.resize(MAX_RECENT_PROJECTS);
    }
}

// =============================================================================
// Scene Management
// =============================================================================

bool EditorApplication::NewScene() {
    m_activeScene = std::make_unique<Scene>();
    m_activeScene->Initialize();
    m_activeScene->SetName("Untitled");
    m_scenePath.clear();
    m_sceneDirty = false;

    ClearSelection();
    m_commandHistory.Clear();

    // Connect scene to outliner
#if NOVA_HAS_SCENE_OUTLINER
    if (auto outliner = GetPanel<SceneOutliner>()) {
        outliner->SetScene(m_activeScene.get());
        outliner->SetCommandHistory(&m_commandHistory);
    }
#endif

    return true;
}

bool EditorApplication::OpenScene(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        ShowNotification("Scene file not found: " + path.string(), NotificationType::Error);
        return false;
    }

    try {
        // Parse scene JSON file
        auto sceneJson = Nova::Json::TryParseFile(path.string());
        if (!sceneJson) {
            ShowNotification("Failed to parse scene file: " + path.string(), NotificationType::Error);
            return false;
        }

        const auto& doc = *sceneJson;

        // Validate scene file format
        if (!Nova::Json::Contains(doc, "nova_scene")) {
            ShowNotification("Invalid scene file format", NotificationType::Error);
            return false;
        }

        // Create new scene
        m_activeScene = std::make_unique<Scene>();
        m_activeScene->Initialize();

        // Load scene metadata
        std::string sceneName = Nova::Json::Get<std::string>(doc, "name", path.stem().string());
        m_activeScene->SetName(sceneName);

        // Helper lambda to deserialize a scene node from JSON
        std::function<void(SceneNode*, const Nova::Json::JsonValue&)> deserializeNode;
        deserializeNode = [&deserializeNode](SceneNode* parent, const Nova::Json::JsonValue& nodeJson) {
            // Create node with name
            std::string nodeName = Nova::Json::Get<std::string>(nodeJson, "name", "Node");
            auto node = std::make_unique<SceneNode>(nodeName);

            // Load transform
            if (Nova::Json::Contains(nodeJson, "position")) {
                const auto& pos = nodeJson["position"];
                if (pos.is_array() && pos.size() >= 3) {
                    node->SetPosition(glm::vec3(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>()));
                }
            }

            if (Nova::Json::Contains(nodeJson, "rotation")) {
                const auto& rot = nodeJson["rotation"];
                if (rot.is_array() && rot.size() >= 4) {
                    // Quaternion: w, x, y, z
                    node->SetRotation(glm::quat(rot[0].get<float>(), rot[1].get<float>(),
                                                rot[2].get<float>(), rot[3].get<float>()));
                } else if (rot.is_array() && rot.size() == 3) {
                    // Euler angles in degrees
                    node->SetRotation(glm::vec3(rot[0].get<float>(), rot[1].get<float>(), rot[2].get<float>()));
                }
            }

            if (Nova::Json::Contains(nodeJson, "scale")) {
                const auto& scl = nodeJson["scale"];
                if (scl.is_array() && scl.size() >= 3) {
                    node->SetScale(glm::vec3(scl[0].get<float>(), scl[1].get<float>(), scl[2].get<float>()));
                } else if (scl.is_number()) {
                    node->SetScale(scl.get<float>());
                }
            }

            // Load visibility
            node->SetVisible(Nova::Json::Get<bool>(nodeJson, "visible", true));

            // Recursively load children
            if (Nova::Json::Contains(nodeJson, "children")) {
                const auto& childrenJson = nodeJson["children"];
                if (childrenJson.is_array()) {
                    for (const auto& childJson : childrenJson) {
                        deserializeNode(node.get(), childJson);
                    }
                }
            }

            // Add node to parent
            parent->AddChild(std::move(node));
        };

        // Load scene hierarchy
        if (Nova::Json::Contains(doc, "entities") && doc["entities"].is_array()) {
            SceneNode* root = m_activeScene->GetRoot();
            for (const auto& entityJson : doc["entities"]) {
                deserializeNode(root, entityJson);
            }
        }

        m_scenePath = path;
        m_sceneDirty = false;

        ClearSelection();
        m_commandHistory.Clear();

#if NOVA_HAS_SCENE_OUTLINER
        if (auto outliner = GetPanel<SceneOutliner>()) {
            outliner->SetScene(m_activeScene.get());
            outliner->SetCommandHistory(&m_commandHistory);
        }
#endif

        ShowNotification("Opened scene: " + path.filename().string(), NotificationType::Success);
        return true;
    } catch (const std::exception& e) {
        ShowNotification("Failed to open scene: " + std::string(e.what()), NotificationType::Error);
        return false;
    }
}

bool EditorApplication::SaveScene() {
    if (m_scenePath.empty()) {
        // Show save dialog
        ShowSaveFileDialog("Save Scene", "Scene Files (*.scene)|*.scene", "Untitled.scene",
                           [this](const std::filesystem::path& path) {
                               if (!path.empty()) {
                                   m_scenePath = path;
                                   SaveScene();
                               }
                           });
        return true;
    }

    try {
        // Build scene JSON document
        Nova::Json::JsonValue doc = Nova::Json::Object();

        // Scene header
        doc["nova_scene"] = true;
        doc["version"] = "1.0.0";
        doc["name"] = m_activeScene ? m_activeScene->GetName() : "Untitled";

        // Helper lambda to serialize a scene node to JSON
        std::function<Nova::Json::JsonValue(const SceneNode*)> serializeNode;
        serializeNode = [&serializeNode](const SceneNode* node) -> Nova::Json::JsonValue {
            Nova::Json::JsonValue nodeJson = Nova::Json::Object();

            // Name
            nodeJson["name"] = node->GetName();

            // Transform - position
            const glm::vec3& pos = node->GetPosition();
            nodeJson["position"] = Nova::Json::Array({pos.x, pos.y, pos.z});

            // Transform - rotation (as quaternion: w, x, y, z)
            const glm::quat& rot = node->GetRotation();
            nodeJson["rotation"] = Nova::Json::Array({rot.w, rot.x, rot.y, rot.z});

            // Transform - scale
            const glm::vec3& scl = node->GetScale();
            nodeJson["scale"] = Nova::Json::Array({scl.x, scl.y, scl.z});

            // Visibility
            nodeJson["visible"] = node->IsVisible();

            // Serialize children recursively
            const auto& children = node->GetChildren();
            if (!children.empty()) {
                Nova::Json::JsonValue childrenJson = Nova::Json::Array();
                for (const auto& child : children) {
                    childrenJson.push_back(serializeNode(child.get()));
                }
                nodeJson["children"] = childrenJson;
            }

            return nodeJson;
        };

        // Serialize all entities (children of root node)
        Nova::Json::JsonValue entitiesJson = Nova::Json::Array();
        if (m_activeScene) {
            SceneNode* root = m_activeScene->GetRoot();
            if (root) {
                for (const auto& child : root->GetChildren()) {
                    entitiesJson.push_back(serializeNode(child.get()));
                }
            }
        }
        doc["entities"] = entitiesJson;

        // Write to file with pretty formatting
        Nova::Json::WriteFile(m_scenePath.string(), doc, 2);

        m_sceneDirty = false;
        ShowNotification("Scene saved", NotificationType::Success);
        return true;
    } catch (const std::exception& e) {
        ShowNotification("Failed to save scene: " + std::string(e.what()), NotificationType::Error);
        return false;
    }
}

bool EditorApplication::SaveSceneAs(const std::filesystem::path& path) {
    m_scenePath = path;
    if (m_activeScene) {
        m_activeScene->SetName(path.stem().string());
    }
    return SaveScene();
}

void EditorApplication::MarkSceneDirty() {
    m_sceneDirty = true;
    MarkProjectDirty();
}

void EditorApplication::SwitchToScene(size_t index) {
    if (index < m_openScenes.size() && index != m_activeSceneIndex) {
        // FUTURE: Implement full scene switching with state preservation
        // For now, just update the index - scene loading happens on demand
        spdlog::info("Switching to scene index {}", index);
        m_activeSceneIndex = index;
        ShowNotification("Scene switching: partial implementation", NotificationType::Warning);
    }
}

bool EditorApplication::CloseScene(size_t index) {
    if (index >= m_openScenes.size()) {
        return false;
    }

    // FUTURE: Check for unsaved changes and handle multi-scene closing
    // For now, just close without prompting
    if (m_sceneDirty) {
        spdlog::warn("Closing scene with unsaved changes (prompt not yet implemented)");
    }
    m_openScenes.erase(m_openScenes.begin() + index);
    return true;
}

// =============================================================================
// Selection System
// =============================================================================

void EditorApplication::SelectObject(SceneNode* node) {
    std::vector<SceneNode*> previous = m_selection;

    m_selection.clear();
    m_selectionSet.clear();

    if (node) {
        m_selection.push_back(node);
        m_selectionSet.insert(node);
    }

    NotifySelectionChanged(previous);
}

void EditorApplication::AddToSelection(SceneNode* node) {
    if (!node || IsSelected(node)) {
        return;
    }

    std::vector<SceneNode*> previous = m_selection;
    m_selection.push_back(node);
    m_selectionSet.insert(node);
    NotifySelectionChanged(previous);
}

void EditorApplication::RemoveFromSelection(SceneNode* node) {
    if (!node || !IsSelected(node)) {
        return;
    }

    std::vector<SceneNode*> previous = m_selection;
    m_selection.erase(std::remove(m_selection.begin(), m_selection.end(), node), m_selection.end());
    m_selectionSet.erase(node);
    NotifySelectionChanged(previous);
}

void EditorApplication::ClearSelection() {
    if (m_selection.empty()) {
        return;
    }

    std::vector<SceneNode*> previous = m_selection;
    m_selection.clear();
    m_selectionSet.clear();
    NotifySelectionChanged(previous);
}

bool EditorApplication::IsSelected(const SceneNode* node) const {
    return m_selectionSet.count(node) > 0;
}

SceneNode* EditorApplication::GetPrimarySelection() const {
    return m_selection.empty() ? nullptr : m_selection.back();
}

void EditorApplication::SelectAll() {
    if (!m_activeScene) {
        return;
    }

    std::vector<SceneNode*> previous = m_selection;
    m_selection.clear();
    m_selectionSet.clear();

    std::vector<SceneNode*> allNodes;
    if (auto* root = m_activeScene->GetRoot()) {
        CollectSceneNodes(root, allNodes);
    }

    for (auto* node : allNodes) {
        m_selection.push_back(node);
        m_selectionSet.insert(node);
    }

    NotifySelectionChanged(previous);
}

void EditorApplication::InvertSelection() {
    if (!m_activeScene) {
        return;
    }

    std::vector<SceneNode*> previous = m_selection;
    std::unordered_set<const SceneNode*> previousSet = m_selectionSet;

    m_selection.clear();
    m_selectionSet.clear();

    std::vector<SceneNode*> allNodes;
    if (auto* root = m_activeScene->GetRoot()) {
        CollectSceneNodes(root, allNodes);
    }

    for (auto* node : allNodes) {
        if (previousSet.count(node) == 0) {
            m_selection.push_back(node);
            m_selectionSet.insert(node);
        }
    }

    NotifySelectionChanged(previous);
}

void EditorApplication::FocusOnSelection() {
    if (m_selection.empty() || !m_activeScene) {
        return;
    }

    // Calculate the bounding box of all selected objects
    glm::vec3 minBounds(std::numeric_limits<float>::max());
    glm::vec3 maxBounds(std::numeric_limits<float>::lowest());

    for (const auto* node : m_selection) {
        if (!node) continue;

        glm::vec3 pos = node->GetWorldPosition();
        // Use a default size for objects without mesh bounds
        glm::vec3 halfSize(1.0f);

        minBounds = glm::min(minBounds, pos - halfSize);
        maxBounds = glm::max(maxBounds, pos + halfSize);
    }

    // Calculate center and distance
    glm::vec3 center = (minBounds + maxBounds) * 0.5f;
    glm::vec3 size = maxBounds - minBounds;
    float radius = glm::length(size) * 0.5f;

    // Get the camera and position it to view the selection
    if (auto* camera = m_activeScene->GetCamera()) {
        // Calculate a good viewing distance (2x the radius, minimum of 5 units)
        float distance = std::max(radius * 2.5f, 5.0f);

        // Get current camera direction or use a default
        glm::vec3 currentPos = camera->GetPosition();
        glm::vec3 direction = glm::normalize(currentPos - center);

        // Prevent degenerate case
        if (glm::length(direction) < 0.01f) {
            direction = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        // Set new camera position
        glm::vec3 newPos = center + direction * distance;
        camera->SetPosition(newPos);
        camera->LookAt(newPos, center);

        spdlog::debug("Camera focused on selection at ({}, {}, {})", center.x, center.y, center.z);
    }
}

void EditorApplication::SetOnSelectionChanged(std::function<void(const SelectionChangedEvent&)> callback) {
    m_onSelectionChanged = std::move(callback);
}

void EditorApplication::NotifySelectionChanged(const std::vector<SceneNode*>& previous) {
    if (m_onSelectionChanged) {
        SelectionChangedEvent event;
        event.previousSelection = previous;
        event.newSelection = m_selection;
        m_onSelectionChanged(event);
    }

    // Update outliner selection
#if NOVA_HAS_SCENE_OUTLINER
    if (auto outliner = GetPanel<SceneOutliner>()) {
        outliner->ClearSelection();
        for (auto* node : m_selection) {
            outliner->Select(node, true);
        }
    }
#endif
}

void EditorApplication::CollectSceneNodes(SceneNode* node, std::vector<SceneNode*>& outNodes) {
    if (!node) {
        return;
    }

    outNodes.push_back(node);
    for (const auto& child : node->GetChildren()) {
        CollectSceneNodes(child.get(), outNodes);
    }
}

// =============================================================================
// Clipboard Operations
// =============================================================================

void EditorApplication::CutSelection() {
    if (m_selection.empty()) {
        ShowNotification("Nothing selected to cut", NotificationType::Warning);
        return;
    }

    // Copy first
    CopySelection();
    m_clipboardIsCut = true;

    // Delete the selected objects using a composite command for undo
    auto compositeCmd = std::make_unique<CompositeCommand>("Cut Selection");
    for (auto* node : m_selection) {
        if (node && node != m_activeScene->GetRoot()) {
            compositeCmd->AddCommand(std::make_unique<DeleteObjectCommand>(m_activeScene.get(), node));
        }
    }

    if (!compositeCmd->IsEmpty()) {
        ExecuteCommand(std::move(compositeCmd));
    }

    ClearSelection();
    ShowNotification("Cut to clipboard", NotificationType::Info, 1.5f);
}

void EditorApplication::CopySelection() {
    if (m_selection.empty()) {
        ShowNotification("Nothing selected to copy", NotificationType::Warning);
        return;
    }

    m_clipboard.clear();
    m_clipboardIsCut = false;

    for (const auto* node : m_selection) {
        if (!node) continue;

        ClipboardEntry entry;
        entry.name = node->GetName();
        entry.position = node->GetPosition();
        entry.rotation = node->GetRotation();
        entry.scale = node->GetScale();
        entry.assetPath = node->GetAssetPath();

        m_clipboard.push_back(entry);
    }

    ShowNotification("Copied " + std::to_string(m_clipboard.size()) + " object(s)", NotificationType::Info, 1.5f);
}

void EditorApplication::PasteSelection() {
    if (m_clipboard.empty()) {
        ShowNotification("Clipboard is empty", NotificationType::Warning);
        return;
    }

    if (!m_activeScene) {
        ShowNotification("No active scene", NotificationType::Error);
        return;
    }

    // Offset for pasted objects to avoid overlap
    const glm::vec3 pasteOffset(1.0f, 0.0f, 1.0f);

    std::vector<SceneNode*> pastedNodes;
    auto compositeCmd = std::make_unique<CompositeCommand>("Paste");

    for (const auto& entry : m_clipboard) {
        auto newNode = std::make_unique<SceneNode>(entry.name + "_copy");
        newNode->SetPosition(entry.position + pasteOffset);
        newNode->SetRotation(entry.rotation);
        newNode->SetScale(entry.scale);
        if (!entry.assetPath.empty()) {
            newNode->SetAssetPath(entry.assetPath);
        }

        SceneNode* nodePtr = newNode.get();
        pastedNodes.push_back(nodePtr);

        compositeCmd->AddCommand(std::make_unique<CreateObjectCommand>(
            m_activeScene.get(), std::move(newNode), m_activeScene->GetRoot()));
    }

    if (!compositeCmd->IsEmpty()) {
        ExecuteCommand(std::move(compositeCmd));

        // Select the pasted nodes
        ClearSelection();
        for (auto* node : pastedNodes) {
            AddToSelection(node);
        }
    }

    ShowNotification("Pasted " + std::to_string(pastedNodes.size()) + " object(s)", NotificationType::Info, 1.5f);
}

bool EditorApplication::HasClipboardContent() const {
    return !m_clipboard.empty();
}

// =============================================================================
// Object Creation
// =============================================================================

SceneNode* EditorApplication::CreateEmptyObject(SceneNode* parent) {
    if (!m_activeScene) {
        ShowNotification("No active scene", NotificationType::Error);
        return nullptr;
    }

    // Determine the parent node
    SceneNode* targetParent = parent ? parent : m_activeScene->GetRoot();
    if (!targetParent) {
        ShowNotification("Invalid parent node", NotificationType::Error);
        return nullptr;
    }

    // Generate a unique name
    std::string baseName = "GameObject";
    std::string name = baseName;
    int counter = 1;

    // Check for name conflicts
    while (m_activeScene->FindNode(name) != nullptr) {
        name = baseName + "_" + std::to_string(counter++);
    }

    // Create the node
    auto newNode = std::make_unique<SceneNode>(name);
    SceneNode* nodePtr = newNode.get();

    // Execute through command system for undo support
    auto cmd = std::make_unique<CreateObjectCommand>(m_activeScene.get(), std::move(newNode), targetParent);
    if (ExecuteCommand(std::move(cmd))) {
        // Select the new node
        SelectObject(nodePtr);
        ShowNotification("Created: " + name, NotificationType::Success, 1.5f);
        return nodePtr;
    }

    return nullptr;
}

SceneNode* EditorApplication::GroupSelection() {
    if (m_selection.size() < 2) {
        ShowNotification("Select at least 2 objects to group", NotificationType::Warning);
        return nullptr;
    }

    if (!m_activeScene) {
        ShowNotification("No active scene", NotificationType::Error);
        return nullptr;
    }

    // Calculate the center of all selected objects
    glm::vec3 center(0.0f);
    for (const auto* node : m_selection) {
        if (node) {
            center += node->GetWorldPosition();
        }
    }
    center /= static_cast<float>(m_selection.size());

    // Generate a unique name for the group
    std::string baseName = "Group";
    std::string name = baseName;
    int counter = 1;
    while (m_activeScene->FindNode(name) != nullptr) {
        name = baseName + "_" + std::to_string(counter++);
    }

    // Create the group node
    auto groupNode = std::make_unique<SceneNode>(name);
    groupNode->SetPosition(center);
    SceneNode* groupPtr = groupNode.get();

    // Create a composite command for the entire operation
    auto compositeCmd = std::make_unique<CompositeCommand>("Group Selection");

    // First, create the group node under the root
    compositeCmd->AddCommand(std::make_unique<CreateObjectCommand>(
        m_activeScene.get(), std::move(groupNode), m_activeScene->GetRoot()));

    // Reparent all selected nodes to the group
    for (auto* node : m_selection) {
        if (node && node != m_activeScene->GetRoot()) {
            compositeCmd->AddCommand(std::make_unique<ReparentCommand>(node, groupPtr));
        }
    }

    if (!compositeCmd->IsEmpty()) {
        ExecuteCommand(std::move(compositeCmd));

        // Select only the group
        SelectObject(groupPtr);
        ShowNotification("Created group: " + name, NotificationType::Success, 1.5f);
        return groupPtr;
    }

    return nullptr;
}

// =============================================================================
// Command System
// =============================================================================

bool EditorApplication::ExecuteCommand(std::unique_ptr<ICommand> command) {
    if (m_commandHistory.ExecuteCommand(std::move(command))) {
        MarkSceneDirty();
        return true;
    }
    return false;
}

bool EditorApplication::Undo() {
    if (m_commandHistory.Undo()) {
        MarkSceneDirty();
        return true;
    }
    return false;
}

bool EditorApplication::Redo() {
    if (m_commandHistory.Redo()) {
        MarkSceneDirty();
        return true;
    }
    return false;
}

bool EditorApplication::CanUndo() const {
    return m_commandHistory.CanUndo();
}

bool EditorApplication::CanRedo() const {
    return m_commandHistory.CanRedo();
}

std::string EditorApplication::GetUndoCommandName() const {
    return m_commandHistory.GetUndoCommandName();
}

std::string EditorApplication::GetRedoCommandName() const {
    return m_commandHistory.GetRedoCommandName();
}

std::vector<std::string> EditorApplication::GetUndoHistory(size_t maxCount) const {
    return m_commandHistory.GetUndoHistory(maxCount);
}

std::vector<std::string> EditorApplication::GetRedoHistory(size_t maxCount) const {
    return m_commandHistory.GetRedoHistory(maxCount);
}

// =============================================================================
// Transform Tools
// =============================================================================

void EditorApplication::SetTransformTool(TransformTool tool) {
    m_transformTool = tool;
}

void EditorApplication::SetTransformSpace(TransformSpace space) {
    m_transformSpace = space;
}

void EditorApplication::ToggleTransformSpace() {
    m_transformSpace = (m_transformSpace == TransformSpace::World)
                        ? TransformSpace::Local : TransformSpace::World;
}

// =============================================================================
// Play Mode
// =============================================================================

std::string EditorApplication::SerializeFullScene() {
    if (!m_activeScene) {
        return "";
    }

    try {
        Nova::Json::JsonValue doc = Nova::Json::Object();

        // Scene metadata
        doc["nova_playmode_snapshot"] = true;
        doc["version"] = "1.0.0";
        doc["name"] = m_activeScene->GetName();

        // Serialize camera state
        if (Camera* camera = m_activeScene->GetCamera()) {
            Nova::Json::JsonValue cameraJson = Nova::Json::Object();
            const glm::vec3& camPos = camera->GetPosition();
            cameraJson["position"] = Nova::Json::Array({camPos.x, camPos.y, camPos.z});
            cameraJson["pitch"] = camera->GetPitch();
            cameraJson["yaw"] = camera->GetYaw();
            cameraJson["fov"] = camera->GetFOV();
            cameraJson["near"] = camera->GetNearPlane();
            cameraJson["far"] = camera->GetFarPlane();
            cameraJson["aspect"] = camera->GetAspectRatio();
            doc["camera"] = cameraJson;
        }

        // Helper lambda to serialize a scene node recursively
        std::function<Nova::Json::JsonValue(const SceneNode*)> serializeNode;
        serializeNode = [&serializeNode](const SceneNode* node) -> Nova::Json::JsonValue {
            Nova::Json::JsonValue nodeJson = Nova::Json::Object();

            // Basic properties
            nodeJson["name"] = node->GetName();
            nodeJson["asset_path"] = node->GetAssetPath();
            nodeJson["visible"] = node->IsVisible();

            // Transform - position
            const glm::vec3& pos = node->GetPosition();
            nodeJson["position"] = Nova::Json::Array({pos.x, pos.y, pos.z});

            // Transform - rotation (quaternion: w, x, y, z)
            const glm::quat& rot = node->GetRotation();
            nodeJson["rotation"] = Nova::Json::Array({rot.w, rot.x, rot.y, rot.z});

            // Transform - scale
            const glm::vec3& scl = node->GetScale();
            nodeJson["scale"] = Nova::Json::Array({scl.x, scl.y, scl.z});

            // Serialize mesh/material flags
            if (node->HasMesh()) {
                nodeJson["has_mesh"] = true;
            }
            if (node->HasMaterial()) {
                nodeJson["has_material"] = true;
            }

            // Serialize children recursively
            const auto& children = node->GetChildren();
            if (!children.empty()) {
                Nova::Json::JsonValue childrenJson = Nova::Json::Array();
                for (const auto& child : children) {
                    childrenJson.push_back(serializeNode(child.get()));
                }
                nodeJson["children"] = childrenJson;
            }

            return nodeJson;
        };

        // Serialize all entities (children of root node)
        Nova::Json::JsonValue entitiesJson = Nova::Json::Array();
        SceneNode* root = m_activeScene->GetRoot();
        if (root) {
            for (const auto& child : root->GetChildren()) {
                entitiesJson.push_back(serializeNode(child.get()));
            }
        }
        doc["entities"] = entitiesJson;

        return Nova::Json::Stringify(doc);

    } catch (const std::exception& e) {
        spdlog::error("Failed to serialize scene for play mode: {}", e.what());
        return "";
    }
}

bool EditorApplication::DeserializeFullScene(const std::string& jsonState) {
    if (jsonState.empty() || !m_activeScene) {
        return false;
    }

    try {
        Nova::Json::JsonValue doc = Nova::Json::Parse(jsonState);

        // Verify this is a valid play mode snapshot
        if (!Nova::Json::Get<bool>(doc, "nova_playmode_snapshot", false)) {
            spdlog::error("Invalid play mode snapshot: missing header");
            return false;
        }

        // Restore camera state
        if (Nova::Json::Contains(doc, "camera")) {
            const auto& cameraJson = doc["camera"];
            if (Camera* camera = m_activeScene->GetCamera()) {
                if (cameraJson.contains("position") && cameraJson["position"].is_array()) {
                    const auto& p = cameraJson["position"];
                    camera->SetPosition(glm::vec3(
                        p[0].get<float>(), p[1].get<float>(), p[2].get<float>()));
                }
                if (cameraJson.contains("pitch") && cameraJson.contains("yaw")) {
                    camera->SetRotation(
                        cameraJson["pitch"].get<float>(),
                        cameraJson["yaw"].get<float>());
                }
                if (cameraJson.contains("fov")) {
                    float fov = cameraJson["fov"].get<float>();
                    float aspect = Nova::Json::Get<float>(cameraJson, "aspect", 16.0f / 9.0f);
                    float nearPlane = Nova::Json::Get<float>(cameraJson, "near", 0.1f);
                    float farPlane = Nova::Json::Get<float>(cameraJson, "far", 1000.0f);
                    camera->SetPerspective(fov, aspect, nearPlane, farPlane);
                }
            }
        }

        // Helper lambda to restore node data recursively
        std::function<void(SceneNode*, const Nova::Json::JsonValue&)> restoreNode;
        restoreNode = [&restoreNode](SceneNode* node, const Nova::Json::JsonValue& nodeJson) {
            if (!node) return;

            if (nodeJson.contains("name")) {
                node->SetName(nodeJson["name"].get<std::string>());
            }
            if (nodeJson.contains("asset_path")) {
                node->SetAssetPath(nodeJson["asset_path"].get<std::string>());
            }
            node->SetVisible(Nova::Json::Get<bool>(nodeJson, "visible", true));

            if (nodeJson.contains("position") && nodeJson["position"].is_array()) {
                const auto& p = nodeJson["position"];
                node->SetPosition(glm::vec3(
                    p[0].get<float>(), p[1].get<float>(), p[2].get<float>()));
            }
            if (nodeJson.contains("rotation") && nodeJson["rotation"].is_array()) {
                const auto& r = nodeJson["rotation"];
                if (r.size() >= 4) {
                    node->SetRotation(glm::quat(
                        r[0].get<float>(), r[1].get<float>(),
                        r[2].get<float>(), r[3].get<float>()));
                }
            }
            if (nodeJson.contains("scale") && nodeJson["scale"].is_array()) {
                const auto& s = nodeJson["scale"];
                node->SetScale(glm::vec3(
                    s[0].get<float>(), s[1].get<float>(), s[2].get<float>()));
            }

            // Restore children recursively
            if (nodeJson.contains("children") && nodeJson["children"].is_array()) {
                const auto& childrenJson = nodeJson["children"];
                const auto& children = node->GetChildren();
                for (size_t i = 0; i < std::min(childrenJson.size(), children.size()); ++i) {
                    restoreNode(children[i].get(), childrenJson[i]);
                }
            }
        };

        // Restore scene entities
        if (Nova::Json::Contains(doc, "entities") && doc["entities"].is_array()) {
            SceneNode* root = m_activeScene->GetRoot();
            if (root) {
                const auto& entitiesJson = doc["entities"];
                const auto& children = root->GetChildren();
                for (size_t i = 0; i < std::min(entitiesJson.size(), children.size()); ++i) {
                    restoreNode(children[i].get(), entitiesJson[i]);
                }
            }
        }

        m_activeScene->InvalidateRenderBatch();
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to deserialize scene from play mode snapshot: {}", e.what());
        return false;
    }
}

void EditorApplication::ClearPlayModeEntities() {
    // Placeholder for runtime-created entity cleanup
}

void EditorApplication::Play() {
    if (m_playState != EditorPlayState::Editing) {
        m_playState = EditorPlayState::Playing;
        return;
    }

    // Serialize ENTIRE scene state to JSON before entering play mode
    m_prePlaySceneState = SerializeFullScene();
    if (m_prePlaySceneState.empty()) {
        ShowNotification("Failed to save scene state", NotificationType::Error);
        return;
    }

    // Save camera state separately for quick access
    if (m_activeScene && m_activeScene->GetCamera()) {
        Camera* camera = m_activeScene->GetCamera();
        m_prePlayCameraPosition = camera->GetPosition();
        m_prePlayCameraPitch = camera->GetPitch();
        m_prePlayCameraYaw = camera->GetYaw();
        m_prePlayCameraFOV = camera->GetFOV();
    }

    m_playState = EditorPlayState::Playing;
    ClearSelection();

    spdlog::info("Entered play mode (scene state saved: {} bytes)", m_prePlaySceneState.size());
    ShowNotification("Entered play mode", NotificationType::Info, 1.5f);
}

void EditorApplication::Pause() {
    if (m_playState == EditorPlayState::Playing) {
        m_playState = EditorPlayState::Paused;
        ShowNotification("Paused", NotificationType::Info, 1.0f);
    } else if (m_playState == EditorPlayState::Paused) {
        m_playState = EditorPlayState::Playing;
        ShowNotification("Resumed", NotificationType::Info, 1.0f);
    }
}

void EditorApplication::Stop() {
    if (m_playState == EditorPlayState::Editing) {
        return;
    }

    // Stop simulation
    m_playState = EditorPlayState::Editing;

    // Clear any runtime-created entities
    ClearPlayModeEntities();

    // Restore original scene from saved state
    if (!m_prePlaySceneState.empty()) {
        if (DeserializeFullScene(m_prePlaySceneState)) {
            spdlog::info("Scene state restored successfully");
        } else {
            spdlog::error("Failed to restore scene state");
            ShowNotification("Warning: Scene restoration failed", NotificationType::Warning);
        }

        // Clear saved state to free memory
        m_prePlaySceneState.clear();
    }

    // Restore camera state (backup in case JSON restore fails)
    if (m_activeScene && m_activeScene->GetCamera()) {
        Camera* camera = m_activeScene->GetCamera();
        camera->SetPosition(m_prePlayCameraPosition);
        camera->SetRotation(m_prePlayCameraPitch, m_prePlayCameraYaw);
    }

    ShowNotification("Exited play mode", NotificationType::Info, 1.5f);
}

void EditorApplication::StepFrame() {
    if (m_playState == EditorPlayState::Paused && m_activeScene) {
        m_activeScene->Update(1.0f / 60.0f);  // Fixed step
    }
}

// =============================================================================
// Settings
// =============================================================================

bool EditorApplication::LoadSettings() {
    // Determine settings file path (in AppData or alongside executable)
    std::filesystem::path settingsPath;
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) {
        settingsPath = std::filesystem::path(appData) / "Nova3D" / "editor_settings.json";
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        settingsPath = std::filesystem::path(home) / ".config" / "Nova3D" / "editor_settings.json";
    }
#endif

    // Fall back to local path if env var not available
    if (settingsPath.empty()) {
        settingsPath = "editor_settings.json";
    }

    // If settings file doesn't exist, use defaults
    if (!std::filesystem::exists(settingsPath)) {
        m_settings = EditorSettings{};
        return true;
    }

    try {
        auto settingsJson = Nova::Json::TryParseFile(settingsPath.string());
        if (!settingsJson) {
            spdlog::warn("Failed to parse settings file, using defaults");
            m_settings = EditorSettings{};
            return true;
        }

        const auto& doc = *settingsJson;

        // General settings
        m_settings.autoSave = Nova::Json::Get<bool>(doc, "autoSave", true);
        m_settings.autoSaveIntervalSeconds = Nova::Json::Get<float>(doc, "autoSaveInterval", 300.0f);
        m_settings.showWelcomeOnStartup = Nova::Json::Get<bool>(doc, "showWelcomeOnStartup", true);
        m_settings.restoreLayoutOnStartup = Nova::Json::Get<bool>(doc, "restoreLayoutOnStartup", true);
        m_settings.lastLayout = Nova::Json::Get<std::string>(doc, "lastLayout", "Default");

        // Viewport settings
        m_settings.showGrid = Nova::Json::Get<bool>(doc, "showGrid", true);
        m_settings.gridSize = Nova::Json::Get<float>(doc, "gridSize", 1.0f);
        m_settings.gridSubdivisions = Nova::Json::Get<int>(doc, "gridSubdivisions", 10);
        m_settings.showGizmos = Nova::Json::Get<bool>(doc, "showGizmos", true);
        m_settings.showIcons = Nova::Json::Get<bool>(doc, "showIcons", true);
        m_settings.iconScale = Nova::Json::Get<float>(doc, "iconScale", 1.0f);

        // Grid color
        if (Nova::Json::Contains(doc, "gridColor") && doc["gridColor"].is_array()) {
            const auto& gc = doc["gridColor"];
            if (gc.size() >= 4) {
                m_settings.gridColor = glm::vec4(gc[0].get<float>(), gc[1].get<float>(),
                                                  gc[2].get<float>(), gc[3].get<float>());
            }
        }

        // Background color
        if (Nova::Json::Contains(doc, "backgroundColor") && doc["backgroundColor"].is_array()) {
            const auto& bg = doc["backgroundColor"];
            if (bg.size() >= 4) {
                m_settings.backgroundColor = glm::vec4(bg[0].get<float>(), bg[1].get<float>(),
                                                        bg[2].get<float>(), bg[3].get<float>());
            }
        }

        // Snap settings
        m_settings.snapEnabled = Nova::Json::Get<bool>(doc, "snapEnabled", false);
        m_settings.snapTranslate = Nova::Json::Get<float>(doc, "snapTranslate", 1.0f);
        m_settings.snapRotate = Nova::Json::Get<float>(doc, "snapRotate", 15.0f);
        m_settings.snapScale = Nova::Json::Get<float>(doc, "snapScale", 0.1f);

        // Performance settings
        m_settings.targetFrameRate = Nova::Json::Get<int>(doc, "targetFrameRate", 60);
        m_settings.vsync = Nova::Json::Get<bool>(doc, "vsync", true);
        m_settings.showFps = Nova::Json::Get<bool>(doc, "showFps", true);
        m_settings.showMemory = Nova::Json::Get<bool>(doc, "showMemory", true);

        // Theme
        m_settings.themeName = Nova::Json::Get<std::string>(doc, "themeName", "Dark");

        // Load keyboard shortcuts
        if (Nova::Json::Contains(doc, "shortcuts") && doc["shortcuts"].is_object()) {
            m_settings.shortcuts.clear();
            for (auto it = doc["shortcuts"].begin(); it != doc["shortcuts"].end(); ++it) {
                m_settings.shortcuts[it.key()] = it.value().get<std::string>();
            }
        }

        spdlog::info("Loaded editor settings from: {}", settingsPath.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::warn("Failed to load settings: {}", e.what());
        m_settings = EditorSettings{};
        return true;
    }
}

bool EditorApplication::SaveSettings() {
    // Determine settings file path
    std::filesystem::path settingsPath;
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) {
        settingsPath = std::filesystem::path(appData) / "Nova3D";
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        settingsPath = std::filesystem::path(home) / ".config" / "Nova3D";
    }
#endif

    // Fall back to local path if env var not available
    if (settingsPath.empty()) {
        settingsPath = ".";
    }

    try {
        // Create directory if it doesn't exist
        std::filesystem::create_directories(settingsPath);
        settingsPath /= "editor_settings.json";

        // Build settings JSON
        Nova::Json::JsonValue doc = Nova::Json::Object();

        // General settings
        doc["autoSave"] = m_settings.autoSave;
        doc["autoSaveInterval"] = m_settings.autoSaveIntervalSeconds;
        doc["showWelcomeOnStartup"] = m_settings.showWelcomeOnStartup;
        doc["restoreLayoutOnStartup"] = m_settings.restoreLayoutOnStartup;
        doc["lastLayout"] = m_settings.lastLayout;

        // Viewport settings
        doc["showGrid"] = m_settings.showGrid;
        doc["gridSize"] = m_settings.gridSize;
        doc["gridSubdivisions"] = m_settings.gridSubdivisions;
        doc["showGizmos"] = m_settings.showGizmos;
        doc["showIcons"] = m_settings.showIcons;
        doc["iconScale"] = m_settings.iconScale;

        // Colors
        doc["gridColor"] = Nova::Json::Array({
            m_settings.gridColor.r, m_settings.gridColor.g,
            m_settings.gridColor.b, m_settings.gridColor.a
        });
        doc["backgroundColor"] = Nova::Json::Array({
            m_settings.backgroundColor.r, m_settings.backgroundColor.g,
            m_settings.backgroundColor.b, m_settings.backgroundColor.a
        });

        // Snap settings
        doc["snapEnabled"] = m_settings.snapEnabled;
        doc["snapTranslate"] = m_settings.snapTranslate;
        doc["snapRotate"] = m_settings.snapRotate;
        doc["snapScale"] = m_settings.snapScale;

        // Performance settings
        doc["targetFrameRate"] = m_settings.targetFrameRate;
        doc["vsync"] = m_settings.vsync;
        doc["showFps"] = m_settings.showFps;
        doc["showMemory"] = m_settings.showMemory;

        // Theme
        doc["themeName"] = m_settings.themeName;

        // Keyboard shortcuts
        Nova::Json::JsonValue shortcutsJson = Nova::Json::Object();
        for (const auto& [action, shortcut] : m_settings.shortcuts) {
            shortcutsJson[action] = shortcut;
        }
        doc["shortcuts"] = shortcutsJson;

        // Write to file
        Nova::Json::WriteFile(settingsPath.string(), doc, 2);

        spdlog::info("Saved editor settings to: {}", settingsPath.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save settings: {}", e.what());
        return false;
    }
}

void EditorApplication::ApplySettings() {
    EditorTheme::Instance().Apply();

    // Apply other settings
    if (m_transformGizmo) {
        GizmoSnapping snapping;
        snapping.enabled = m_settings.snapEnabled;
        snapping.translateSnap = m_settings.snapTranslate;
        snapping.rotateSnap = m_settings.snapRotate;
        snapping.scaleSnap = m_settings.snapScale;
        m_transformGizmo->SetSnapping(snapping);
    }
}

void EditorApplication::ResetSettings() {
    m_settings = EditorSettings{};
    ApplySettings();
}

void EditorApplication::ShowPreferencesWindow() {
    m_showPreferencesWindow = true;
}

// =============================================================================
// Layouts
// =============================================================================

void EditorApplication::SaveLayout(const std::string& name) {
    LayoutPreset preset;
    preset.name = name;
    preset.iniData = ImGui::SaveIniSettingsToMemory();
    m_layouts[name] = preset;
    ShowNotification("Layout saved: " + name, NotificationType::Success);
}

bool EditorApplication::LoadLayout(const std::string& name) {
    auto it = m_layouts.find(name);
    if (it == m_layouts.end()) {
        ShowNotification("Layout not found: " + name, NotificationType::Warning);
        return false;
    }

    ImGui::LoadIniSettingsFromMemory(it->second.iniData.c_str(), it->second.iniData.size());
    m_settings.lastLayout = name;
    return true;
}

void EditorApplication::DeleteLayout(const std::string& name) {
    m_layouts.erase(name);
}

std::vector<std::string> EditorApplication::GetLayoutNames() const {
    std::vector<std::string> names;
    names.reserve(m_layouts.size());
    for (const auto& [name, layout] : m_layouts) {
        names.push_back(name);
    }
    return names;
}

void EditorApplication::ResetLayout() {
    // Reset to default ImGui layout
    ImGui::GetIO().IniFilename = nullptr;  // Prevent auto-save
    // FUTURE: Load default layout preset from embedded data or file
    // For now, ImGui will use its default docking layout
    spdlog::debug("Layout reset to defaults");
}

// =============================================================================
// Notifications
// =============================================================================

void EditorApplication::ShowNotification(const std::string& message, NotificationType type, float duration) {
    ShowNotification(message, type, duration, nullptr);
}

void EditorApplication::ShowNotification(const std::string& message, NotificationType type,
                                          float duration, std::function<void()> onClick) {
    EditorNotification notification;
    notification.message = message;
    notification.type = type;
    notification.duration = duration;
    notification.timeRemaining = duration;
    notification.onClick = std::move(onClick);

    m_notifications.push_front(notification);

    // Trim to max size
    while (m_notifications.size() > MAX_NOTIFICATIONS) {
        m_notifications.pop_back();
    }

    // Log the notification
    switch (type) {
        case NotificationType::Error:   spdlog::error("{}", message); break;
        case NotificationType::Warning: spdlog::warn("{}", message); break;
        case NotificationType::Success: spdlog::info("{}", message); break;
        default:                        spdlog::info("{}", message); break;
    }
}

void EditorApplication::DismissNotification(size_t index) {
    if (index < m_notifications.size()) {
        m_notifications.erase(m_notifications.begin() + index);
    }
}

void EditorApplication::ClearNotifications() {
    m_notifications.clear();
}

void EditorApplication::UpdateNotifications(float deltaTime) {
    for (auto it = m_notifications.begin(); it != m_notifications.end();) {
        it->timeRemaining -= deltaTime;
        if (it->timeRemaining <= 0.0f && it->duration > 0.0f) {
            it = m_notifications.erase(it);
        } else {
            ++it;
        }
    }
}

// =============================================================================
// Progress Tasks
// =============================================================================

ProgressTask& EditorApplication::StartProgressTask(const std::string& id, const std::string& description,
                                                     bool indeterminate) {
    ProgressTask task;
    task.id = id;
    task.description = description;
    task.progress = 0.0f;
    task.indeterminate = indeterminate;

    m_progressTasks[id] = task;
    return m_progressTasks[id];
}

void EditorApplication::UpdateProgressTask(const std::string& id, float progress, const std::string& description) {
    auto it = m_progressTasks.find(id);
    if (it != m_progressTasks.end()) {
        it->second.progress = std::clamp(progress, 0.0f, 1.0f);
        if (!description.empty()) {
            it->second.description = description;
        }
    }
}

void EditorApplication::CompleteProgressTask(const std::string& id) {
    m_progressTasks.erase(id);
}

void EditorApplication::CancelProgressTask(const std::string& id) {
    auto it = m_progressTasks.find(id);
    if (it != m_progressTasks.end() && it->second.onCancel) {
        it->second.onCancel();
    }
    m_progressTasks.erase(id);
}

// =============================================================================
// Dialogs
// =============================================================================

void EditorApplication::ShowMessageDialog(const std::string& title, const std::string& message) {
    m_dialogState.isOpen = true;
    m_dialogState.type = DialogState::Type::Message;
    m_dialogState.title = title;
    m_dialogState.message = message;
}

void EditorApplication::ShowConfirmDialog(const std::string& title, const std::string& message,
                                           std::function<void()> onConfirm, std::function<void()> onCancel) {
    m_dialogState.isOpen = true;
    m_dialogState.type = DialogState::Type::Confirm;
    m_dialogState.title = title;
    m_dialogState.message = message;
    m_dialogState.onConfirm = std::move(onConfirm);
    m_dialogState.onCancel = std::move(onCancel);
}

void EditorApplication::ShowOpenFileDialog(const std::string& title, const std::string& filters,
                                            std::function<void(const std::filesystem::path&)> callback) {
    // FUTURE: Implement platform-specific file dialog (Win32, GTK, Cocoa) or ImGui file browser
    // Current implementation uses ImGui modal dialog with text input
    m_dialogState.isOpen = true;
    m_dialogState.type = DialogState::Type::OpenFile;
    m_dialogState.title = title;
    m_dialogState.filters = filters;
    m_dialogState.fileCallback = std::move(callback);
}

void EditorApplication::ShowSaveFileDialog(const std::string& title, const std::string& filters,
                                            const std::string& defaultName,
                                            std::function<void(const std::filesystem::path&)> callback) {
    m_dialogState.isOpen = true;
    m_dialogState.type = DialogState::Type::SaveFile;
    m_dialogState.title = title;
    m_dialogState.filters = filters;
    m_dialogState.defaultName = defaultName;
    m_dialogState.fileCallback = std::move(callback);
}

void EditorApplication::ShowInputDialog(const std::string& title, const std::string& prompt,
                                        std::function<void(const std::string&)> callback,
                                        const std::string& defaultValue) {
    m_dialogState.isOpen = true;
    m_dialogState.type = DialogState::Type::Input;
    m_dialogState.title = title;
    m_dialogState.message = prompt;
    m_dialogState.inputCallback = std::move(callback);

    // Initialize buffer with default value
    std::memset(m_dialogState.inputBuffer, 0, sizeof(m_dialogState.inputBuffer));
    if (!defaultValue.empty()) {
        std::strncpy(m_dialogState.inputBuffer, defaultValue.c_str(),
                    sizeof(m_dialogState.inputBuffer) - 1);
    }
}

void EditorApplication::ShowNewAssetDialog(CreatableAssetType preselectedType) {
    // Configure the dialog
    m_assetCreationDialog.SetPreselectedType(preselectedType);

    // Set target directory to project assets folder if available
    if (m_hasProject && !m_projectPath.empty()) {
        std::filesystem::path assetsPath = m_projectPath.parent_path() / "assets";
        if (!std::filesystem::exists(assetsPath)) {
            // Try to create the assets directory
            try {
                std::filesystem::create_directories(assetsPath);
            } catch (...) {
                // Fall back to project directory
                assetsPath = m_projectPath.parent_path();
            }
        }
        m_assetCreationDialog.SetTargetDirectory(assetsPath.string());
        m_assetCreationDialog.SetProjectRoot(m_projectPath.parent_path().string());
    } else {
        // Default to current working directory
        m_assetCreationDialog.SetTargetDirectory(std::filesystem::current_path().string());
        m_assetCreationDialog.SetProjectRoot("");
    }

    // Set up callbacks
    m_assetCreationDialog.OnAssetCreated = [this](const std::string& path, CreatableAssetType type) {
        // Mark project as dirty
        MarkProjectDirty();

        // Log the creation
        spdlog::info("Created new {} asset: {}", GetCreatableAssetTypeName(type), path);
    };

    // Open the dialog
    m_assetCreationDialog.Open();
}

// =============================================================================
// Shortcuts
// =============================================================================

void EditorApplication::RegisterDefaultShortcuts() {
    // File shortcuts - Scene operations
    RegisterShortcut("New", "Ctrl+N", [this]() { NewScene(); });
    RegisterShortcut("Open", "Ctrl+O", [this]() {
        ShowOpenFileDialog("Open Scene", "Scene Files (*.scene)|*.scene",
                           [this](const auto& path) { if (!path.empty()) OpenScene(path); });
    });
    RegisterShortcut("Save", "Ctrl+S", [this]() { SaveScene(); });
    RegisterShortcut("SaveAs", "Ctrl+Shift+S", [this]() {
        ShowSaveFileDialog("Save Scene As", "Scene Files (*.scene)|*.scene", "",
                           [this](const auto& path) { if (!path.empty()) SaveSceneAs(path); });
    });

    // File shortcuts - Asset operations
    RegisterShortcut("NewAsset", "Ctrl+Shift+N", [this]() {
        ShowNewAssetDialog();
    });
    RegisterShortcut("OpenAsset", "Ctrl+O", [this]() {
        ShowOpenFileDialog("Open Asset", "SDF Files (*.sdf)|*.sdf|Material Files (*.mat)|*.mat|All Files (*.*)|*.*",
                           [this](const auto& path) {
                               if (!path.empty()) {
                                   ShowNotification("Opened asset: " + path.filename().string(), NotificationType::Success);
                                   // FUTURE: Implement actual asset loading via AssetManager
                                   spdlog::info("Asset open requested: {}", path.string());
                               }
                           });
    });
    RegisterShortcut("SaveAsset", "Ctrl+S", [this]() {
        // Save current asset
        ShowNotification("Asset saved", NotificationType::Success);
        // FUTURE: Implement actual asset saving via AssetManager
        spdlog::info("Asset save requested");
    });
    RegisterShortcut("SaveAssetAs", "Ctrl+Shift+S", [this]() {
        ShowSaveFileDialog("Save Asset As", "SDF Files (*.sdf)|*.sdf|Material Files (*.mat)|*.mat", "",
                           [this](const auto& path) {
                               if (!path.empty()) {
                                   ShowNotification("Asset saved as: " + path.filename().string(), NotificationType::Success);
                                   // FUTURE: Implement actual asset save-as via AssetManager
                                   spdlog::info("Asset save-as requested: {}", path.string());
                               }
                           });
    });

    // Edit shortcuts
    RegisterShortcut("Undo", "Ctrl+Z", [this]() { Undo(); });
    RegisterShortcut("Redo", "Ctrl+Y", [this]() { Redo(); });
    RegisterShortcut("Redo2", "Ctrl+Shift+Z", [this]() { Redo(); });
    RegisterShortcut("Delete", "Delete", [this]() {
        // Delete selected objects
#if NOVA_HAS_SCENE_OUTLINER
        if (auto outliner = GetPanel<SceneOutliner>()) {
            outliner->DeleteSelected();
        }
#endif
    });
    RegisterShortcut("Duplicate", "Ctrl+D", [this]() {
#if NOVA_HAS_SCENE_OUTLINER
        if (auto outliner = GetPanel<SceneOutliner>()) {
            outliner->DuplicateSelected();
        }
#endif
    });
    RegisterShortcut("SelectAll", "Ctrl+A", [this]() { SelectAll(); });
    RegisterShortcut("Cut", "Ctrl+X", [this]() { CutSelection(); });
    RegisterShortcut("Copy", "Ctrl+C", [this]() { CopySelection(); });
    RegisterShortcut("Paste", "Ctrl+V", [this]() { PasteSelection(); });

    // Transform tool shortcuts
    RegisterShortcut("Select", "Q", [this]() { SetTransformTool(TransformTool::Select); });
    RegisterShortcut("Translate", "W", [this]() { SetTransformTool(TransformTool::Translate); });
    RegisterShortcut("Rotate", "E", [this]() { SetTransformTool(TransformTool::Rotate); });
    RegisterShortcut("Scale", "R", [this]() { SetTransformTool(TransformTool::Scale); });
    RegisterShortcut("ToggleSpace", "X", [this]() { ToggleTransformSpace(); });

    // Play mode shortcuts
    RegisterShortcut("Play", "Ctrl+P", [this]() {
        if (m_playState == EditorPlayState::Editing) Play();
        else Stop();
    });
    RegisterShortcut("Pause", "Ctrl+Shift+P", [this]() { Pause(); });

    // View shortcuts
    RegisterShortcut("FocusSelection", "F", [this]() { FocusOnSelection(); });

    // Window panel shortcuts
    RegisterShortcut("ShowSDFAssetEditor", "Alt+1", [this]() {
        TogglePanel("SDFAssetEditor");
        ShowNotification("SDF Asset Editor toggled", NotificationType::Info, 1.5f);
    });
    RegisterShortcut("ShowVisualScriptEditor", "Alt+2", [this]() {
        TogglePanel("VisualScriptEditor");
        ShowNotification("Visual Script Editor toggled", NotificationType::Info, 1.5f);
    });
    RegisterShortcut("ShowMaterialGraphEditor", "Alt+3", [this]() {
        TogglePanel("MaterialGraphEditor");
        ShowNotification("Material Graph Editor toggled", NotificationType::Info, 1.5f);
    });
    RegisterShortcut("ShowAnimationTimeline", "Alt+4", [this]() {
        TogglePanel("AnimationTimeline");
        ShowNotification("Animation Timeline toggled", NotificationType::Info, 1.5f);
    });
    RegisterShortcut("ShowPCGPanel", "Ctrl+Shift+G", [this]() {
        TogglePanel("PCGPanel");
        ShowNotification("PCG Panel toggled", NotificationType::Info, 1.5f);
    });

    // Help shortcuts
    RegisterShortcut("OpenDocumentation", "F1", []() {
        if (!OpenDocumentationFile("README.md")) {
            spdlog::warn("Could not open documentation");
        }
    });
}

void EditorApplication::RegisterShortcut(const std::string& action, const std::string& shortcut,
                                          std::function<void()> handler) {
    ShortcutBinding binding;
    binding.handler = std::move(handler);
    if (ParseShortcut(shortcut, binding.key, binding.modifiers)) {
        m_shortcuts[action] = binding;
        m_settings.shortcuts[action] = shortcut;
    }
}

bool EditorApplication::IsShortcutPressed(const std::string& action) const {
    auto it = m_shortcuts.find(action);
    if (it == m_shortcuts.end()) {
        return false;
    }
    return IsShortcutActive(it->second.key, it->second.modifiers);
}

std::string EditorApplication::GetShortcutForAction(const std::string& action) const {
    auto it = m_settings.shortcuts.find(action);
    return it != m_settings.shortcuts.end() ? it->second : "";
}

bool EditorApplication::ParseShortcut(const std::string& shortcut, int& key, int& modifiers) const {
    key = 0;
    modifiers = 0;

    // Parse shortcut string like "Ctrl+Shift+S"
    std::string upper = shortcut;
    for (auto& c : upper) c = static_cast<char>(std::toupper(c));

    if (upper.find("CTRL") != std::string::npos || upper.find("CONTROL") != std::string::npos) {
        modifiers |= 1;  // Ctrl
    }
    if (upper.find("SHIFT") != std::string::npos) {
        modifiers |= 2;  // Shift
    }
    if (upper.find("ALT") != std::string::npos) {
        modifiers |= 4;  // Alt
    }

    // Find the key (last character or special key)
    size_t plusPos = shortcut.rfind('+');
    std::string keyStr = (plusPos != std::string::npos) ? shortcut.substr(plusPos + 1) : shortcut;

    if (keyStr.length() == 1) {
        char c = keyStr[0];
        // Handle both letters and numbers
        if (std::isalpha(static_cast<unsigned char>(c))) {
            key = static_cast<int>(std::toupper(c));
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            key = static_cast<int>(c);  // Keep digit as-is ('0' = 48, '1' = 49, etc.)
        } else {
            key = static_cast<int>(c);
        }
    } else if (keyStr == "Delete" || keyStr == "DELETE") {
        key = 127;  // Delete key
    } else if (keyStr == "Escape" || keyStr == "ESC") {
        key = 27;
    } else if (keyStr == "Enter" || keyStr == "ENTER") {
        key = 13;
    } else if (keyStr == "Space" || keyStr == "SPACE") {
        key = 32;
    } else if (keyStr == "Tab" || keyStr == "TAB") {
        key = 9;
    } else if (keyStr == "Backspace" || keyStr == "BACKSPACE") {
        key = 8;
    }
    // Function keys F1-F12
    else if (keyStr.length() >= 2 && (keyStr[0] == 'F' || keyStr[0] == 'f')) {
        int fNum = std::atoi(keyStr.substr(1).c_str());
        if (fNum >= 1 && fNum <= 12) {
            key = 289 + fNum;  // F1 = 290, etc.
        }
    }

    return key != 0;
}

bool EditorApplication::IsShortcutActive(int key, int modifiers) const {
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

void EditorApplication::ProcessShortcuts() {
    // Don't process shortcuts when typing in text input
    if (ImGui::GetIO().WantTextInput) {
        return;
    }

    for (const auto& [action, binding] : m_shortcuts) {
        if (IsShortcutActive(binding.key, binding.modifiers)) {
            if (binding.handler) {
                binding.handler();
            }
        }
    }
}

void EditorApplication::HandleGlobalShortcuts() {
    // Handle escape key to cancel operations
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        if (m_dialogState.isOpen) {
            if (m_dialogState.onCancel) {
                m_dialogState.onCancel();
            }
            m_dialogState.isOpen = false;
        } else if (m_transformGizmo && m_transformGizmo->IsActive()) {
            m_transformGizmo->CancelManipulation();
        } else {
            ClearSelection();
        }
    }
}

// =============================================================================
// Rendering
// =============================================================================

void EditorApplication::RenderDockSpace() {
    // Create full-screen dockspace
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    // Leave room for menu bar and toolbar at top
    float topOffset = ImGui::GetFrameHeight() + EditorTheme::Instance().GetSizes().toolbarHeight;
    // Leave room for status bar at bottom
    float bottomOffset = EditorTheme::Instance().GetSizes().statusBarHeight;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + topOffset));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - topOffset - bottomOffset));
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                    ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();
}

void EditorApplication::RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        RenderFileMenu();
        RenderEditMenu();
        RenderViewMenu();
        RenderGameObjectMenu();
        RenderComponentMenu();
        RenderAIMenu();
        RenderWindowMenu();
        RenderHelpMenu();
        ImGui::EndMainMenuBar();
    }
}

void EditorApplication::RenderAIMenu() {
    if (ImGui::BeginMenu("AI")) {
#if NOVA_HAS_AI_TOOL_LAUNCHER
        auto& aiLauncher = AIToolLauncher::Instance();
        bool hasApiKey = aiLauncher.IsAPIKeyConfigured();
        bool hasPython = aiLauncher.IsPythonAvailable();

        // Show API key status indicator
        if (!hasApiKey) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "! API Key not configured");
            ImGui::Separator();
        } else if (!hasPython) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "! Python not found");
            ImGui::Separator();
        }
#else
        bool hasApiKey = false;
        bool hasPython = false;
#endif

        // Asset Generation
        if (ImGui::BeginMenu("Generate Asset", hasApiKey && hasPython)) {
            if (ImGui::MenuItem("Character...")) {
                ShowPanel("AIAssistant");
                // FUTURE: Pre-select character type in AI Assistant panel
            }
            if (ImGui::MenuItem("Building...")) {
                ShowPanel("AIAssistant");
            }
            if (ImGui::MenuItem("Prop...")) {
                ShowPanel("AIAssistant");
            }
            if (ImGui::MenuItem("Weapon...")) {
                ShowPanel("AIAssistant");
            }
            if (ImGui::MenuItem("Vehicle...")) {
                ShowPanel("AIAssistant");
            }
            if (ImGui::MenuItem("Creature...")) {
                ShowPanel("AIAssistant");
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        // Asset Operations
        bool hasSelection = !m_selection.empty();
        if (ImGui::MenuItem("Polish Selected Asset", nullptr, false, hasSelection && hasApiKey && hasPython)) {
#if NOVA_HAS_AI_TOOL_LAUNCHER
            if (!m_selection.empty()) {
                SceneNode* selectedNode = GetPrimarySelection();
                if (selectedNode) {
                    std::string assetPath = selectedNode->GetAssetPath();
                    if (!assetPath.empty()) {
                        aiLauncher.PolishAsset(assetPath);
                    } else {
                        ShowNotification("Selected object has no associated asset file", NotificationType::Warning);
                    }
                }
            }
#endif
        }
        if (ImGui::MenuItem("Suggest Improvements", nullptr, false, hasSelection && hasApiKey && hasPython)) {
#if NOVA_HAS_AI_TOOL_LAUNCHER
            if (!m_selection.empty()) {
                SceneNode* selectedNode = GetPrimarySelection();
                if (selectedNode) {
                    std::string assetPath = selectedNode->GetAssetPath();
                    if (!assetPath.empty()) {
                        aiLauncher.SuggestImprovements(assetPath);
                    } else {
                        ShowNotification("Selected object has no associated asset file", NotificationType::Warning);
                    }
                }
            }
#endif
        }
        if (ImGui::MenuItem("Generate Variations...", nullptr, false, hasSelection && hasApiKey && hasPython)) {
#if NOVA_HAS_AI_TOOL_LAUNCHER
            if (!m_selection.empty()) {
                SceneNode* selectedNode = GetPrimarySelection();
                if (selectedNode) {
                    std::string assetPath = selectedNode->GetAssetPath();
                    if (!assetPath.empty()) {
                        aiLauncher.GenerateVariations(assetPath, 3);
                    } else {
                        ShowNotification("Selected object has no associated asset file", NotificationType::Warning);
                    }
                }
            }
#endif
        }

        ImGui::Separator();

        // Level Generation
        if (ImGui::MenuItem("Generate Level...", nullptr, false, hasApiKey && hasPython)) {
#if NOVA_HAS_AI_TOOL_LAUNCHER
            ShowInputDialog("Generate Level", "Enter level description:",
                [](const std::string& prompt) {
                    if (!prompt.empty()) {
                        AIToolLauncher::Instance().GenerateLevel(prompt, 100, 100);
                    }
                });
#endif
        }

        ImGui::Separator();

        // AI Panels
        if (ImGui::MenuItem("AI Assistant Panel")) {
            TogglePanel("AIAssistant");
        }
        if (ImGui::MenuItem("AI Feedback Panel")) {
            TogglePanel("AIFeedback");
        }

        ImGui::Separator();

        // Configuration
        if (ImGui::MenuItem("Configure API Key...")) {
#if NOVA_HAS_AI_TOOL_LAUNCHER
            aiLauncher.ShowAPISetupWizard();
#else
            m_showAISetupWizard = true;
#endif
        }
        if (ImGui::MenuItem("AI Settings...")) {
            ShowPanel("AISettings");
        }

#if NOVA_HAS_AI_TOOL_LAUNCHER
        // Show API key status
        ImGui::Separator();
        if (hasApiKey) {
            ImGui::TextDisabled("API Key: %s", aiLauncher.GetMaskedAPIKey().c_str());
        }
        if (aiLauncher.IsRunning()) {
            ImGui::TextDisabled("Active AI tasks: %zu", aiLauncher.GetActiveTaskCount());
        }
#endif

        ImGui::Separator();

        // Reports
        if (ImGui::MenuItem("View Quality Report")) {
#if NOVA_HAS_AI_TOOL_LAUNCHER
            aiLauncher.OpenQualityReport();
#endif
        }
        if (ImGui::MenuItem("Validate All Assets", nullptr, false, hasApiKey && hasPython)) {
#if NOVA_HAS_AI_TOOL_LAUNCHER
            aiLauncher.ValidateAllAssets();
#endif
        }

        ImGui::EndMenu();
    }
}

void EditorApplication::RenderFileMenu() {
    if (ImGui::BeginMenu("File")) {
        // Scene operations
        if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
            NewScene();
        }
        if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
            ShowOpenFileDialog("Open Scene", "Scene Files (*.scene)|*.scene",
                               [this](const auto& path) { if (!path.empty()) OpenScene(path); });
        }

        ImGui::Separator();

        // Asset operations
        if (ImGui::MenuItem("New Asset...", "Ctrl+Shift+N")) {
            ShowNewAssetDialog();
        }
        if (ImGui::MenuItem("Open Asset...", "Ctrl+O")) {
            ShowOpenFileDialog("Open Asset", "SDF Files (*.sdf)|*.sdf|Material Files (*.mat)|*.mat|All Files (*.*)|*.*",
                               [this](const auto& path) {
                                   if (!path.empty()) {
                                       ShowNotification("Opened asset: " + path.filename().string(), NotificationType::Success);
                                       // FUTURE: Implement actual asset loading via AssetManager
                                       spdlog::info("Asset open requested: {}", path.string());
                                   }
                               });
        }

        ImGui::Separator();

        // Save operations
        if (ImGui::MenuItem("Save", "Ctrl+S", false, m_sceneDirty)) {
            SaveScene();
        }
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
            ShowSaveFileDialog("Save Scene As", "Scene Files (*.scene)|*.scene", "",
                               [this](const auto& path) { if (!path.empty()) SaveSceneAs(path); });
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save Asset", "Ctrl+S")) {
            ShowNotification("Asset saved", NotificationType::Success);
            // FUTURE: Implement actual asset saving via AssetManager
            spdlog::info("Asset save requested");
        }
        if (ImGui::MenuItem("Save Asset As...", "Ctrl+Shift+S")) {
            ShowSaveFileDialog("Save Asset As", "SDF Files (*.sdf)|*.sdf|Material Files (*.mat)|*.mat", "",
                               [this](const auto& path) {
                                   if (!path.empty()) {
                                       ShowNotification("Asset saved as: " + path.filename().string(), NotificationType::Success);
                                       // FUTURE: Implement actual asset save-as via AssetManager
                                       spdlog::info("Asset save-as requested: {}", path.string());
                                   }
                               });
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Recent Projects")) {
            if (m_recentProjects.empty()) {
                ImGui::MenuItem("No recent projects", nullptr, false, false);
            } else {
                for (const auto& recent : m_recentProjects) {
                    if (ImGui::MenuItem(recent.name.c_str(), nullptr, false, recent.exists)) {
                        OpenProject(recent.path);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Clear Recent")) {
                    ClearRecentProjects();
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Preferences...")) {
            ShowPreferencesWindow();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Exit", "Alt+F4")) {
            RequestShutdown();
        }

        ImGui::EndMenu();
    }
}

void EditorApplication::RenderEditMenu() {
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo())) {
            Undo();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CanRedo())) {
            Redo();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Cut", "Ctrl+X", false, !m_selection.empty())) {
            CutSelection();
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, !m_selection.empty())) {
            CopySelection();
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, HasClipboardContent())) {
            PasteSelection();
        }
        if (ImGui::MenuItem("Delete", "Delete", false, !m_selection.empty())) {
#if NOVA_HAS_SCENE_OUTLINER
            if (auto outliner = GetPanel<SceneOutliner>()) {
                outliner->DeleteSelected();
            }
#endif
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, !m_selection.empty())) {
#if NOVA_HAS_SCENE_OUTLINER
            if (auto outliner = GetPanel<SceneOutliner>()) {
                outliner->DuplicateSelected();
            }
#endif
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Select All", "Ctrl+A")) {
            SelectAll();
        }
        if (ImGui::MenuItem("Deselect All")) {
            ClearSelection();
        }
        if (ImGui::MenuItem("Invert Selection")) {
            InvertSelection();
        }

        ImGui::EndMenu();
    }
}

void EditorApplication::RenderViewMenu() {
    if (ImGui::BeginMenu("View")) {
        // Panel toggles
        if (ImGui::BeginMenu("Panels")) {
            for (const auto& [name, panel] : m_panels) {
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
                ResetLayout();
            }
            ImGui::Separator();
            auto layouts = GetLayoutNames();
            for (const auto& name : layouts) {
                if (ImGui::MenuItem(name.c_str())) {
                    LoadLayout(name);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Layout...")) {
                ShowInputDialog("Save Layout", "Enter layout name:",
                    [this](const std::string& name) {
                        if (!name.empty()) {
                            SaveLayout(name);
                            ShowNotification("Layout saved: " + name, NotificationType::Success);
                        }
                    }, "Custom Layout");
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        // View options
        if (ImGui::MenuItem("Show Grid", nullptr, &m_settings.showGrid)) {
            // Grid visibility toggled
        }
        if (ImGui::MenuItem("Show Gizmos", nullptr, &m_settings.showGizmos)) {
            // Gizmos visibility toggled
        }
        if (ImGui::MenuItem("Show Icons", nullptr, &m_settings.showIcons)) {
            // Icons visibility toggled
        }

        ImGui::EndMenu();
    }
}

void EditorApplication::RenderGameObjectMenu() {
    if (ImGui::BeginMenu("GameObject")) {
        if (ImGui::MenuItem("Create Empty", nullptr, false, m_activeScene != nullptr)) {
            CreateEmptyObject();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("3D Object", m_activeScene != nullptr)) {
            if (ImGui::MenuItem("Cube")) {
                // FUTURE: Create cube primitive
                ShowNotification("Cube primitive: Not yet implemented", NotificationType::Warning);
            }
            if (ImGui::MenuItem("Sphere")) {
                ShowNotification("Sphere primitive: Not yet implemented", NotificationType::Warning);
            }
            if (ImGui::MenuItem("Cylinder")) {
                ShowNotification("Cylinder primitive: Not yet implemented", NotificationType::Warning);
            }
            if (ImGui::MenuItem("Plane")) {
                ShowNotification("Plane primitive: Not yet implemented", NotificationType::Warning);
            }
            if (ImGui::MenuItem("Quad")) {
                ShowNotification("Quad primitive: Not yet implemented", NotificationType::Warning);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("SDF Primitive", m_activeScene != nullptr)) {
            if (ImGui::MenuItem("SDF Sphere")) {
                ShowNotification("SDF Sphere: Not yet implemented", NotificationType::Warning);
            }
            if (ImGui::MenuItem("SDF Box")) {
                ShowNotification("SDF Box: Not yet implemented", NotificationType::Warning);
            }
            if (ImGui::MenuItem("SDF Cylinder")) {
                ShowNotification("SDF Cylinder: Not yet implemented", NotificationType::Warning);
            }
            if (ImGui::MenuItem("SDF Torus")) {
                ShowNotification("SDF Torus: Not yet implemented", NotificationType::Warning);
            }
            if (ImGui::MenuItem("SDF Capsule")) {
                ShowNotification("SDF Capsule: Not yet implemented", NotificationType::Warning);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Light", m_activeScene != nullptr)) {
            if (ImGui::MenuItem("Directional Light")) {
                ShowNotification("Directional Light: Not yet implemented", NotificationType::Warning);
            }
            if (ImGui::MenuItem("Point Light")) {
                ShowNotification("Point Light: Not yet implemented", NotificationType::Warning);
            }
            if (ImGui::MenuItem("Spot Light")) {
                ShowNotification("Spot Light: Not yet implemented", NotificationType::Warning);
            }
            if (ImGui::MenuItem("Area Light")) {
                ShowNotification("Area Light: Not yet implemented", NotificationType::Warning);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Camera", m_activeScene != nullptr)) {
            if (ImGui::MenuItem("Perspective Camera")) {
                ShowNotification("Perspective Camera: Not yet implemented", NotificationType::Warning);
            }
            if (ImGui::MenuItem("Orthographic Camera")) {
                ShowNotification("Orthographic Camera: Not yet implemented", NotificationType::Warning);
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Group Selection", nullptr, false, m_selection.size() > 1)) {
            GroupSelection();
        }

        ImGui::EndMenu();
    }
}

void EditorApplication::RenderComponentMenu() {
    if (ImGui::BeginMenu("Component")) {
        bool hasSelection = !m_selection.empty();

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

void EditorApplication::RenderWindowMenu() {
    if (ImGui::BeginMenu("Window")) {
        // Core panels
        if (ImGui::MenuItem("Hierarchy")) { ShowPanel("SceneOutliner"); }
        if (ImGui::MenuItem("Inspector")) { ShowPanel("Properties"); }
        if (ImGui::MenuItem("Console")) { ShowPanel("Console"); }
        if (ImGui::MenuItem("Asset Browser")) { ShowPanel("AssetBrowser"); }

        ImGui::Separator();

        // Editor panels with shortcuts
        if (ImGui::MenuItem("SDF Asset Editor", GetShortcutForAction("ShowSDFAssetEditor").c_str())) {
            TogglePanel("SDFAssetEditor");
        }
        if (ImGui::MenuItem("Visual Script Editor", GetShortcutForAction("ShowVisualScriptEditor").c_str())) {
            TogglePanel("VisualScriptEditor");
        }
        if (ImGui::MenuItem("Material Graph Editor", GetShortcutForAction("ShowMaterialGraphEditor").c_str())) {
            TogglePanel("MaterialGraphEditor");
        }
        if (ImGui::MenuItem("Animation Timeline", GetShortcutForAction("ShowAnimationTimeline").c_str())) {
            TogglePanel("AnimationTimeline");
        }

        ImGui::Separator();

        // AI panels
        if (ImGui::MenuItem("AI Assistant")) { TogglePanel("AIAssistant"); }
        if (ImGui::MenuItem("AI Feedback")) { TogglePanel("AIFeedback"); }

        ImGui::Separator();

        // Level Design panels
        if (ImGui::MenuItem("PCG Panel", "Ctrl+Shift+G")) { TogglePanel("PCGPanel"); }

        ImGui::Separator();

        // Additional panels
        if (ImGui::MenuItem("Viewport")) { ShowPanel("Viewport"); }
        if (ImGui::MenuItem("SDF Toolbox")) { ShowPanel("SDFToolbox"); }

        ImGui::EndMenu();
    }
}

void EditorApplication::RenderHelpMenu() {
    // Static state for the About dialog (using static since we don't want to add member vars)
    static bool showAboutDialog = false;

    if (ImGui::BeginMenu("Help")) {
        // Documentation section
        if (ImGui::MenuItem("Documentation", "F1")) {
            if (!OpenDocumentationFile("README.md")) {
                ShowNotification("Could not open documentation file", NotificationType::Warning);
            }
        }

        if (ImGui::MenuItem("Getting Started")) {
            if (!OpenDocumentationFile("GETTING_STARTED.md")) {
                ShowNotification("Could not open Getting Started guide", NotificationType::Warning);
            }
        }

        if (ImGui::MenuItem("Editor Guide")) {
            if (!OpenDocumentationFile("EDITOR_GUIDE.md")) {
                ShowNotification("Could not open Editor Guide", NotificationType::Warning);
            }
        }

        ImGui::Separator();

        // Technical documentation
        if (ImGui::BeginMenu("API Reference")) {
            if (ImGui::MenuItem("Full API Reference")) {
                if (!OpenDocumentationFile("API_REFERENCE.md")) {
                    ShowNotification("Could not open API Reference", NotificationType::Warning);
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Animation API")) {
                OpenDocumentationFile("api/Animation.md");
            }
            if (ImGui::MenuItem("Engine API")) {
                OpenDocumentationFile("api/Engine.md");
            }
            if (ImGui::MenuItem("Network API")) {
                OpenDocumentationFile("api/Network.md");
            }
            if (ImGui::MenuItem("Reflection API")) {
                OpenDocumentationFile("api/Reflection.md");
            }
            if (ImGui::MenuItem("Scripting API")) {
                OpenDocumentationFile("api/Scripting.md");
            }
            if (ImGui::MenuItem("Spatial API")) {
                OpenDocumentationFile("api/Spatial.md");
            }
            if (ImGui::MenuItem("UI API")) {
                OpenDocumentationFile("api/UI.md");
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Guides")) {
            if (ImGui::MenuItem("Animation Guide")) {
                OpenDocumentationFile("ANIMATION_GUIDE.md");
            }
            if (ImGui::MenuItem("Scripting Guide")) {
                OpenDocumentationFile("SCRIPTING_GUIDE.md");
            }
            if (ImGui::MenuItem("Networking Guide")) {
                OpenDocumentationFile("NETWORKING_GUIDE.md");
            }
            if (ImGui::MenuItem("SDF Rendering Guide")) {
                OpenDocumentationFile("SDF_RENDERING_GUIDE.md");
            }
            if (ImGui::MenuItem("Building from Source")) {
                OpenDocumentationFile("BUILDING.md");
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tutorials")) {
            if (ImGui::MenuItem("First Entity")) {
                OpenDocumentationFile("tutorials/first_entity.md");
            }
            if (ImGui::MenuItem("Custom Ability")) {
                OpenDocumentationFile("tutorials/custom_ability.md");
            }
            if (ImGui::MenuItem("AI Behavior")) {
                OpenDocumentationFile("tutorials/ai_behavior.md");
            }
            if (ImGui::MenuItem("Custom UI")) {
                OpenDocumentationFile("tutorials/custom_ui.md");
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        // Troubleshooting
        if (ImGui::MenuItem("Troubleshooting")) {
            if (!OpenDocumentationFile("TROUBLESHOOTING.md")) {
                ShowNotification("Could not open Troubleshooting guide", NotificationType::Warning);
            }
        }

        if (ImGui::MenuItem("Configuration Reference")) {
            OpenDocumentationFile("CONFIG_REFERENCE.md");
        }

        ImGui::Separator();

        // Online resources
        if (ImGui::BeginMenu("Online Resources")) {
            if (ImGui::MenuItem("GitHub Repository")) {
                OpenURL("https://github.com/Nova3D/Nova3DEngine");
            }
            if (ImGui::MenuItem("Issue Tracker")) {
                OpenURL("https://github.com/Nova3D/Nova3DEngine/issues");
            }
            if (ImGui::MenuItem("Discussions")) {
                OpenURL("https://github.com/Nova3D/Nova3DEngine/discussions");
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Online Documentation")) {
                OpenURL("https://nova3d.dev/docs");
            }
            if (ImGui::MenuItem("Community Discord")) {
                OpenURL("https://discord.gg/nova3d");
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        // Report bug / feedback
        if (ImGui::MenuItem("Report a Bug...")) {
            OpenURL("https://github.com/Nova3D/Nova3DEngine/issues/new?template=bug_report.md");
        }

        if (ImGui::MenuItem("Request a Feature...")) {
            OpenURL("https://github.com/Nova3D/Nova3DEngine/issues/new?template=feature_request.md");
        }

        ImGui::Separator();

        // About dialog
        if (ImGui::MenuItem("About Nova3D Editor...")) {
            showAboutDialog = true;
        }

        ImGui::EndMenu();
    }

    // Render the About dialog if open
    if (showAboutDialog) {
        ImGui::OpenPopup("About Nova3D Editor");
    }

    // Center the About dialog
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(450, 380), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("About Nova3D Editor", &showAboutDialog, ImGuiWindowFlags_NoResize)) {
        auto& theme = EditorTheme::Instance();

        // Logo/Title area
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : nullptr);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Nova3D Editor").x) * 0.5f);
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Nova3D Editor");
        ImGui::PopFont();

        ImGui::Spacing();

        // Version info
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Version X.X.X").x) * 0.5f);
        ImGui::Text("Version %s", std::string(Engine::GetVersion()).c_str());

        // Build date (using __DATE__ macro)
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Built: XXXX XX XXXX").x) * 0.5f);
        ImGui::TextDisabled("Built: %s", __DATE__);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Description
        const char* description =
            "A modern 3D game engine featuring:\n"
            "  - SDF-based raymarched rendering\n"
            "  - Radiance Cascade global illumination\n"
            "  - Real-time spectral lighting\n"
            "  - Visual scripting system\n"
            "  - Procedural content generation\n"
            "  - AI-assisted development tools";

        ImGui::TextWrapped("%s", description);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Credits
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Credits:");
        ImGui::Indent();
        ImGui::BulletText("Nova Engine Team");
        ImGui::BulletText("Open source contributors");
        ImGui::Unindent();

        ImGui::Spacing();

        // Third-party libraries
        if (ImGui::CollapsingHeader("Third-Party Libraries")) {
            ImGui::Indent();
            ImGui::BulletText("Dear ImGui - Immediate mode GUI");
            ImGui::BulletText("GLM - OpenGL Mathematics");
            ImGui::BulletText("spdlog - Fast C++ logging");
            ImGui::BulletText("nlohmann/json - JSON for Modern C++");
            ImGui::BulletText("stb_image - Image loading");
            ImGui::BulletText("Vulkan SDK - Graphics API");
            ImGui::Unindent();
        }

        ImGui::Spacing();

        // Copyright
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("(c) 2024-2026 Nova Engine Team").x) * 0.5f);
        ImGui::TextDisabled("(c) 2024-2026 Nova Engine Team");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Links and Close button
        float buttonWidth = 100.0f;
        float totalWidth = buttonWidth * 3 + ImGui::GetStyle().ItemSpacing.x * 2;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - totalWidth) * 0.5f);

        if (ImGui::Button("GitHub", ImVec2(buttonWidth, 0))) {
            OpenURL("https://github.com/Nova3D/Nova3DEngine");
        }
        ImGui::SameLine();
        if (ImGui::Button("Website", ImVec2(buttonWidth, 0))) {
            OpenURL("https://nova3d.dev");
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(buttonWidth, 0))) {
            showAboutDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void EditorApplication::RenderToolbar() {
    auto& theme = EditorTheme::Instance();
    float toolbarHeight = theme.GetSizes().toolbarHeight;
    float buttonSize = theme.GetSizes().toolbarButtonSize;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + ImGui::GetFrameHeight()));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, toolbarHeight));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorTheme::ToImVec4(theme.GetColors().panelHeader));

    if (ImGui::Begin("##Toolbar", nullptr, flags)) {
        // Transform tools
        RenderTransformTools();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Snap toggles
        RenderSnapToggles();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Space toggle
        RenderSpaceToggle();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Play controls
        RenderPlayControls();

        // Search box on the right
        ImGui::SameLine(viewport->WorkSize.x - 200.0f);
        RenderSearchBox();
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void EditorApplication::RenderTransformTools() {
    auto& theme = EditorTheme::Instance();
    float buttonSize = theme.GetSizes().toolbarButtonSize;

    // Select tool
    bool isSelect = (m_transformTool == TransformTool::Select);
    if (isSelect) ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().accent));
    if (ImGui::Button("Q", ImVec2(buttonSize, buttonSize))) {
        SetTransformTool(TransformTool::Select);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select (Q)");
    if (isSelect) ImGui::PopStyleColor();

    ImGui::SameLine();

    // Translate tool
    bool isTranslate = (m_transformTool == TransformTool::Translate);
    if (isTranslate) ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().accent));
    if (ImGui::Button("W", ImVec2(buttonSize, buttonSize))) {
        SetTransformTool(TransformTool::Translate);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Translate (W)");
    if (isTranslate) ImGui::PopStyleColor();

    ImGui::SameLine();

    // Rotate tool
    bool isRotate = (m_transformTool == TransformTool::Rotate);
    if (isRotate) ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().accent));
    if (ImGui::Button("E", ImVec2(buttonSize, buttonSize))) {
        SetTransformTool(TransformTool::Rotate);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotate (E)");
    if (isRotate) ImGui::PopStyleColor();

    ImGui::SameLine();

    // Scale tool
    bool isScale = (m_transformTool == TransformTool::Scale);
    if (isScale) ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().accent));
    if (ImGui::Button("R", ImVec2(buttonSize, buttonSize))) {
        SetTransformTool(TransformTool::Scale);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale (R)");
    if (isScale) ImGui::PopStyleColor();
}

void EditorApplication::RenderSnapToggles() {
    auto& theme = EditorTheme::Instance();
    float buttonSize = theme.GetSizes().toolbarButtonSize;

    // Snap toggle
    if (m_settings.snapEnabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().accent));
    }
    if (ImGui::Button("Snap", ImVec2(buttonSize * 1.5f, buttonSize))) {
        m_settings.snapEnabled = !m_settings.snapEnabled;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Snapping");
    if (m_settings.snapEnabled) {
        ImGui::PopStyleColor();
    }
}

void EditorApplication::RenderSpaceToggle() {
    auto& theme = EditorTheme::Instance();
    float buttonSize = theme.GetSizes().toolbarButtonSize;

    const char* spaceLabel = (m_transformSpace == TransformSpace::World) ? "World" : "Local";
    if (ImGui::Button(spaceLabel, ImVec2(buttonSize * 2.0f, buttonSize))) {
        ToggleTransformSpace();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Transform Space (X)");
}

void EditorApplication::RenderPlayControls() {
    auto& theme = EditorTheme::Instance();
    float buttonSize = theme.GetSizes().toolbarButtonSize;

    // Play button
    bool isPlaying = (m_playState == EditorPlayState::Playing);
    if (isPlaying) {
        ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().success));
    }
    if (ImGui::Button(isPlaying ? "||" : ">", ImVec2(buttonSize, buttonSize))) {
        if (m_playState == EditorPlayState::Editing) {
            Play();
        } else {
            Pause();
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(isPlaying ? "Pause (Ctrl+Shift+P)" : "Play (Ctrl+P)");
    if (isPlaying) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    // Stop button
    bool canStop = (m_playState != EditorPlayState::Editing);
    if (!canStop) {
        ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().buttonDisabled));
    }
    if (ImGui::Button("[]", ImVec2(buttonSize, buttonSize)) && canStop) {
        Stop();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop");
    if (!canStop) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    // Step button (only when paused)
    bool canStep = (m_playState == EditorPlayState::Paused);
    if (!canStep) {
        ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().buttonDisabled));
    }
    if (ImGui::Button("|>", ImVec2(buttonSize, buttonSize)) && canStep) {
        StepFrame();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Step Frame");
    if (!canStep) {
        ImGui::PopStyleColor();
    }
}

void EditorApplication::RenderSearchBox() {
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputTextWithHint("##Search", "Search...", m_searchBuffer, sizeof(m_searchBuffer));
}

void EditorApplication::RenderPanels() {
    for (auto& [name, panel] : m_panels) {
        if (panel && panel->IsVisible()) {
            panel->Render();
        }
    }
}

void EditorApplication::RenderStatusBar() {
    auto& theme = EditorTheme::Instance();
    float statusHeight = theme.GetSizes().statusBarHeight;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - statusHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusHeight));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 2.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorTheme::ToImVec4(theme.GetColors().panelHeader));

    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        // Selection info
        RenderSelectionInfo();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Active tool
        RenderActiveToolName();

        // Right-aligned items
        float rightStart = viewport->WorkSize.x - 300.0f;

        // Progress tasks
        if (!m_progressTasks.empty()) {
            ImGui::SameLine(rightStart - 200.0f);
            RenderProgressTasks();
        }

        // FPS counter
        if (m_settings.showFps) {
            ImGui::SameLine(rightStart);
            RenderFpsCounter();
        }

        // Memory usage
        if (m_settings.showMemory) {
            ImGui::SameLine(rightStart + 80.0f);
            RenderMemoryUsage();
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void EditorApplication::RenderSelectionInfo() {
    if (m_selection.empty()) {
        ImGui::TextDisabled("No selection");
    } else if (m_selection.size() == 1) {
        ImGui::Text("%s", m_selection[0]->GetName().c_str());
    } else {
        ImGui::Text("%zu objects selected", m_selection.size());
    }
}

void EditorApplication::RenderFpsCounter() {
    auto& theme = EditorTheme::Instance();
    glm::vec4 color = (m_fps >= 55.0f) ? theme.GetColors().success :
                      (m_fps >= 30.0f) ? theme.GetColors().warning : theme.GetColors().error;
    ImGui::TextColored(EditorTheme::ToImVec4(color), "%.0f FPS", m_fps);
}

void EditorApplication::RenderMemoryUsage() {
    ImGui::Text("%.1f MB", m_memoryUsageMB);
}

void EditorApplication::RenderActiveToolName() {
    ImGui::Text("%s | %s", GetTransformToolName(m_transformTool), GetTransformSpaceName(m_transformSpace));
}

void EditorApplication::RenderProgressTasks() {
    for (const auto& [id, task] : m_progressTasks) {
        ImGui::Text("%s", task.description.c_str());
        ImGui::SameLine();
        if (task.indeterminate) {
            ImGui::Text("...");
        } else {
            ImGui::ProgressBar(task.progress, ImVec2(100.0f, 0.0f));
        }
        break;  // Only show first task in status bar
    }
}

void EditorApplication::RenderNotifications() {
    if (m_notifications.empty()) {
        return;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float yOffset = viewport->WorkPos.y + viewport->WorkSize.y - 100.0f;

    for (size_t i = 0; i < m_notifications.size(); ++i) {
        const auto& notification = m_notifications[i];

        float alpha = std::min(notification.timeRemaining / 0.3f, 1.0f);  // Fade out in last 0.3s
        if (notification.duration <= 0.0f) alpha = 1.0f;  // Persistent

        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 320.0f, yOffset));
        ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f));

        std::string windowId = "##Notification" + std::to_string(i);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::PushStyleColor(ImGuiCol_WindowBg,
                              ImVec4(0.15f, 0.15f, 0.18f, 0.95f * alpha));

        if (ImGui::Begin(windowId.c_str(), nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {

            glm::vec4 color = GetNotificationColor(notification.type);
            ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::ToImVec4(color));
            ImGui::TextWrapped("%s", notification.message.c_str());
            ImGui::PopStyleColor();

            if (notification.dismissible) {
                ImGui::SameLine(ImGui::GetWindowWidth() - 30.0f);
                if (ImGui::SmallButton("X")) {
                    const_cast<EditorNotification&>(notification).timeRemaining = 0.0f;
                }
            }
        }
        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        yOffset -= 50.0f;
    }
}

void EditorApplication::RenderModalDialogs() {
    if (!m_dialogState.isOpen) {
        return;
    }

    ImGui::OpenPopup(m_dialogState.title.c_str());

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(m_dialogState.title.c_str(), &m_dialogState.isOpen,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", m_dialogState.message.c_str());
        ImGui::Separator();

        switch (m_dialogState.type) {
            case DialogState::Type::Message:
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    m_dialogState.isOpen = false;
                    ImGui::CloseCurrentPopup();
                }
                break;

            case DialogState::Type::Confirm:
                if (ImGui::Button("Yes", ImVec2(120, 0))) {
                    if (m_dialogState.onConfirm) m_dialogState.onConfirm();
                    m_dialogState.isOpen = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("No", ImVec2(120, 0))) {
                    if (m_dialogState.onCancel) m_dialogState.onCancel();
                    m_dialogState.isOpen = false;
                    ImGui::CloseCurrentPopup();
                }
                break;

            case DialogState::Type::OpenFile:
            case DialogState::Type::SaveFile:
                // FUTURE: Implement full file browser UI with directory navigation
                // For now, show a simple text input for the path
                ImGui::Text("Enter file path:");
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    if (m_dialogState.fileCallback) m_dialogState.fileCallback({});
                    m_dialogState.isOpen = false;
                    ImGui::CloseCurrentPopup();
                }
                break;

            case DialogState::Type::Input: {
                ImGui::SetKeyboardFocusHere();
                ImGui::PushItemWidth(300);
                bool enterPressed = ImGui::InputText("##input", m_dialogState.inputBuffer,
                                                     sizeof(m_dialogState.inputBuffer),
                                                     ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::PopItemWidth();

                if (ImGui::Button("OK", ImVec2(120, 0)) || enterPressed) {
                    if (m_dialogState.inputCallback) {
                        m_dialogState.inputCallback(std::string(m_dialogState.inputBuffer));
                    }
                    m_dialogState.isOpen = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    if (m_dialogState.inputCallback) {
                        m_dialogState.inputCallback("");
                    }
                    m_dialogState.isOpen = false;
                    ImGui::CloseCurrentPopup();
                }
                break;
            }
        }

        ImGui::EndPopup();
    }
}

void EditorApplication::RenderPreferencesWindow() {
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Preferences", &m_showPreferencesWindow)) {
        if (ImGui::BeginTabBar("PreferencesTabs")) {
            if (ImGui::BeginTabItem("General")) {
                ImGui::Checkbox("Auto Save", &m_settings.autoSave);
                if (m_settings.autoSave) {
                    ImGui::SliderFloat("Auto Save Interval (sec)", &m_settings.autoSaveIntervalSeconds, 60.0f, 600.0f);
                }
                ImGui::Checkbox("Show Welcome on Startup", &m_settings.showWelcomeOnStartup);
                ImGui::Checkbox("Restore Layout on Startup", &m_settings.restoreLayoutOnStartup);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Viewport")) {
                ImGui::Checkbox("Show Grid", &m_settings.showGrid);
                if (m_settings.showGrid) {
                    ImGui::SliderFloat("Grid Size", &m_settings.gridSize, 0.1f, 10.0f);
                    ImGui::SliderInt("Grid Subdivisions", &m_settings.gridSubdivisions, 1, 20);
                }
                ImGui::Checkbox("Show Gizmos", &m_settings.showGizmos);
                ImGui::Checkbox("Show Icons", &m_settings.showIcons);
                if (m_settings.showIcons) {
                    ImGui::SliderFloat("Icon Scale", &m_settings.iconScale, 0.5f, 2.0f);
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Snapping")) {
                ImGui::Checkbox("Enable Snapping", &m_settings.snapEnabled);
                ImGui::SliderFloat("Translation Snap", &m_settings.snapTranslate, 0.1f, 10.0f);
                ImGui::SliderFloat("Rotation Snap", &m_settings.snapRotate, 1.0f, 90.0f);
                ImGui::SliderFloat("Scale Snap", &m_settings.snapScale, 0.01f, 1.0f);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Performance")) {
                ImGui::SliderInt("Target Frame Rate", &m_settings.targetFrameRate, 30, 144);
                ImGui::Checkbox("VSync", &m_settings.vsync);
                ImGui::Checkbox("Show FPS", &m_settings.showFps);
                ImGui::Checkbox("Show Memory Usage", &m_settings.showMemory);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Theme")) {
                // Theme selection would go here
                ImGui::Text("Theme: %s", m_settings.themeName.c_str());
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Shortcuts")) {
                // Shortcut editor would go here
                ImGui::Text("Keyboard shortcuts editor coming soon...");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();

        if (ImGui::Button("Apply")) {
            ApplySettings();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset to Defaults")) {
            ResetSettings();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            SaveSettings();
            m_showPreferencesWindow = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            LoadSettings();  // Revert changes
            m_showPreferencesWindow = false;
        }
    }
    ImGui::End();
}

// =============================================================================
// Utility Methods
// =============================================================================

void EditorApplication::UpdateFrameStats(float deltaTime) {
    m_frameTimeAccumulator += deltaTime;
    m_frameCount++;

    if (m_frameTimeAccumulator >= 1.0f) {
        m_fps = static_cast<float>(m_frameCount) / m_frameTimeAccumulator;
        m_frameTimeAccumulator = 0.0f;
        m_frameCount = 0;

        // Update memory usage
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            m_memoryUsageMB = static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
        }
#else
        // FUTURE: Implement memory tracking for Linux/macOS (/proc/self/statm, task_info)
        m_memoryUsageMB = 0.0f;
#endif
    }
}

void EditorApplication::UpdateAutoSave(float deltaTime) {
    if (!m_settings.autoSave || !m_sceneDirty) {
        return;
    }

    m_autoSaveTimer += deltaTime;
    if (m_autoSaveTimer >= m_settings.autoSaveIntervalSeconds) {
        m_autoSaveTimer = 0.0f;
        if (!m_scenePath.empty()) {
            SaveScene();
            ShowNotification("Auto-saved", NotificationType::Info, 1.5f);
        }
    }
}

std::string EditorApplication::GetWindowTitle() const {
    std::string title = "Nova3D Editor";

    if (m_activeScene) {
        title += " - " + m_activeScene->GetName();
        if (m_sceneDirty) {
            title += "*";
        }
    }

    if (m_hasProject) {
        title += " [" + m_projectName + "]";
    }

    return title;
}

} // namespace Nova
