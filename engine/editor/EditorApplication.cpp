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

#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
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
        // TODO: Load project file (JSON or binary format)
        m_projectPath = path;
        m_projectName = path.stem().string();
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
        // TODO: Save project file
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
    // TODO: Load from settings file
    m_recentProjects.clear();
}

void EditorApplication::SaveRecentProjects() {
    // TODO: Save to settings file
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
        // TODO: Deserialize scene from file
        m_activeScene = std::make_unique<Scene>();
        m_activeScene->Initialize();
        m_activeScene->SetName(path.stem().string());
        m_scenePath = path;
        m_sceneDirty = false;

        ClearSelection();
        m_commandHistory.Clear();

#if NOVA_HAS_SCENE_OUTLINER
        if (auto outliner = GetPanel<SceneOutliner>()) {
            outliner->SetScene(m_activeScene.get());
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
        // TODO: Serialize scene to file
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
        // TODO: Implement scene switching
        m_activeSceneIndex = index;
    }
}

bool EditorApplication::CloseScene(size_t index) {
    if (index >= m_openScenes.size()) {
        return false;
    }

    // TODO: Check for unsaved changes and handle multi-scene closing
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
    // TODO: Implement viewport camera focus on selection
    if (!m_selection.empty()) {
        spdlog::debug("Focus on selection requested");
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

void EditorApplication::Play() {
    if (m_playState != EditorPlayState::Editing) {
        m_playState = EditorPlayState::Playing;
        return;
    }

    // Save scene state for restoration
    // TODO: Implement proper scene serialization for play mode
    m_playState = EditorPlayState::Playing;
    ShowNotification("Entered play mode", NotificationType::Info, 1.5f);
}

void EditorApplication::Pause() {
    if (m_playState == EditorPlayState::Playing) {
        m_playState = EditorPlayState::Paused;
    } else if (m_playState == EditorPlayState::Paused) {
        m_playState = EditorPlayState::Playing;
    }
}

void EditorApplication::Stop() {
    if (m_playState == EditorPlayState::Editing) {
        return;
    }

    // Restore scene state
    // TODO: Implement proper scene restoration
    m_playState = EditorPlayState::Editing;
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
    // TODO: Load from JSON file
    m_settings = EditorSettings{};  // Use defaults for now
    return true;
}

bool EditorApplication::SaveSettings() {
    // TODO: Save to JSON file
    return true;
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
    // TODO: Load default layout preset
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
    // TODO: Implement platform-specific file dialog or use ImGui file browser
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

// =============================================================================
// Shortcuts
// =============================================================================

void EditorApplication::RegisterDefaultShortcuts() {
    // File shortcuts
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
        key = static_cast<int>(std::toupper(keyStr[0]));
    } else if (keyStr == "Delete" || keyStr == "DELETE") {
        key = 127;  // Delete key
    } else if (keyStr == "Escape" || keyStr == "ESC") {
        key = 27;
    } else if (keyStr == "Enter" || keyStr == "ENTER") {
        key = 13;
    } else if (keyStr == "Space" || keyStr == "SPACE") {
        key = 32;
    }
    // Add more special keys as needed

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

    // Check key
    if (key >= 0 && key < 512) {
        return ImGui::IsKeyPressed(static_cast<ImGuiKey>(key));
    }

    return false;
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
        RenderWindowMenu();
        RenderHelpMenu();
        ImGui::EndMainMenuBar();
    }
}

void EditorApplication::RenderFileMenu() {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
            NewScene();
        }
        if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
            ShowOpenFileDialog("Open Scene", "Scene Files (*.scene)|*.scene",
                               [this](const auto& path) { if (!path.empty()) OpenScene(path); });
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save", "Ctrl+S", false, m_sceneDirty)) {
            SaveScene();
        }
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
            ShowSaveFileDialog("Save Scene As", "Scene Files (*.scene)|*.scene", "",
                               [this](const auto& path) { if (!path.empty()) SaveSceneAs(path); });
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
            // TODO: Implement cut
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, !m_selection.empty())) {
            // TODO: Implement copy
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V")) {
            // TODO: Implement paste
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
                // TODO: Show save layout dialog
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

        if (ImGui::MenuItem("Group Selection", nullptr, false, m_selection.size() > 1)) {
#if NOVA_HAS_SCENE_OUTLINER
            if (auto outliner = GetPanel<SceneOutliner>()) {
                outliner->GroupSelected();
            }
#endif
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
        if (ImGui::MenuItem("Inspector")) { /* ShowPanel("Properties"); */ }
        if (ImGui::MenuItem("Console")) { ShowPanel("Console"); }
        if (ImGui::MenuItem("Asset Browser")) { /* ShowPanel("AssetBrowser"); */ }

        ImGui::Separator();

        // Editor panels
        if (ImGui::MenuItem("Viewport")) { /* ShowPanel("Viewport"); */ }
        if (ImGui::MenuItem("SDF Toolbox")) { /* ShowPanel("SDFToolbox"); */ }
        if (ImGui::MenuItem("Animation Timeline")) { /* ShowPanel("AnimationTimeline"); */ }
        if (ImGui::MenuItem("Material Editor")) { /* ShowPanel("MaterialEditor"); */ }

        ImGui::EndMenu();
    }
}

void EditorApplication::RenderHelpMenu() {
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Documentation")) {
            // TODO: Open documentation
        }
        if (ImGui::MenuItem("API Reference")) {
            // TODO: Open API docs
        }

        ImGui::Separator();

        if (ImGui::MenuItem("About Nova3D Editor")) {
            ShowMessageDialog("About Nova3D Editor",
                              "Nova3D Engine v" + std::string(Engine::GetVersion()) + "\n\n"
                              "A modern 3D game engine with SDF rendering,\n"
                              "global illumination, and advanced tooling.\n\n"
                              "(c) 2024 Nova Engine Team");
        }

        ImGui::EndMenu();
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
                // TODO: Implement file browser UI
                ImGui::Text("File browser not yet implemented");
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    if (m_dialogState.fileCallback) m_dialogState.fileCallback({});
                    m_dialogState.isOpen = false;
                    ImGui::CloseCurrentPopup();
                }
                break;
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
        m_memoryUsageMB = 0.0f;  // TODO: Implement for other platforms
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
