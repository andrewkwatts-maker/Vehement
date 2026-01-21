/**
 * @file EditorApplication.hpp
 * @brief Main Editor Application that ties all editor components together
 *
 * This is the central coordinator for the Nova3D/Vehement editor, managing:
 * - Panel registration and lifecycle
 * - Menu bar and toolbar rendering
 * - Project and scene management
 * - Selection and command systems
 * - Settings and preferences
 * - Notifications and status display
 *
 * @author Nova Engine Team
 * @version 1.0.0
 */

#pragma once

#include "../core/Engine.hpp"
#include "../ui/EditorPanel.hpp"
#include "../ui/EditorTheme.hpp"
#include "CommandHistory.hpp"
#include "EditorCommand.hpp"
#include "AssetCreationDialog.hpp"
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <deque>
#include <chrono>
#include <filesystem>
#include <typeindex>
#include <any>

namespace Nova {

// Forward declarations
class Scene;
class SceneNode;
class Camera;
class TransformGizmo;
class SceneOutliner;
class ConsolePanel;
class InputManager;

// =============================================================================
// Transform Tool Mode
// =============================================================================

/**
 * @brief Active transform manipulation tool
 */
enum class TransformTool : uint8_t {
    Select,     ///< Selection only
    Translate,  ///< Move objects
    Rotate,     ///< Rotate objects
    Scale       ///< Scale objects
};

/**
 * @brief Transform coordinate space
 */
enum class TransformSpace : uint8_t {
    World,  ///< World coordinates
    Local   ///< Object local coordinates
};

// =============================================================================
// Editor Play State
// =============================================================================

/**
 * @brief Editor play mode state
 */
enum class EditorPlayState : uint8_t {
    Editing,    ///< Normal editing mode
    Playing,    ///< Game is running
    Paused      ///< Game is paused
};

// =============================================================================
// Notification Types
// =============================================================================

/**
 * @brief Notification severity level
 */
enum class NotificationType : uint8_t {
    Info,
    Success,
    Warning,
    Error
};

/**
 * @brief Notification entry for display
 */
struct EditorNotification {
    std::string message;
    NotificationType type = NotificationType::Info;
    float duration = 3.0f;          ///< Display duration in seconds
    float timeRemaining = 3.0f;     ///< Time remaining before fade
    bool dismissible = true;        ///< Can be dismissed by user
    std::function<void()> onClick;  ///< Optional click handler
};

// =============================================================================
// Progress Task
// =============================================================================

/**
 * @brief Progress task for status bar display
 */
struct ProgressTask {
    std::string id;
    std::string description;
    float progress = 0.0f;          ///< 0.0 to 1.0
    bool indeterminate = false;     ///< If true, show spinner instead of progress bar
    bool cancellable = false;
    std::function<void()> onCancel;
};

// =============================================================================
// Recent Project Entry
// =============================================================================

/**
 * @brief Recent project entry
 */
struct RecentProject {
    std::string path;
    std::string name;
    std::chrono::system_clock::time_point lastOpened;
    bool exists = true;
};

// =============================================================================
// Layout Preset
// =============================================================================

/**
 * @brief Layout preset configuration
 */
struct LayoutPreset {
    std::string name;
    std::string description;
    std::string iniData;  ///< ImGui docking layout data
};

// =============================================================================
// Editor Settings
// =============================================================================

/**
 * @brief Editor settings/preferences
 */
struct EditorSettings {
    // General
    bool autoSave = true;
    float autoSaveIntervalSeconds = 300.0f;  ///< 5 minutes
    bool showWelcomeOnStartup = true;
    bool restoreLayoutOnStartup = true;
    std::string lastLayout = "Default";

    // Viewport
    bool showGrid = true;
    float gridSize = 1.0f;
    int gridSubdivisions = 10;
    bool showGizmos = true;
    bool showIcons = true;
    float iconScale = 1.0f;
    glm::vec4 gridColor{0.5f, 0.5f, 0.5f, 0.3f};
    glm::vec4 backgroundColor{0.1f, 0.1f, 0.12f, 1.0f};

    // Snapping
    bool snapEnabled = false;
    float snapTranslate = 1.0f;
    float snapRotate = 15.0f;
    float snapScale = 0.1f;

    // Performance
    int targetFrameRate = 60;
    bool vsync = true;
    bool showFps = true;
    bool showMemory = true;

    // Theme
    std::string themeName = "Dark";

    // Keyboard shortcuts (action -> shortcut string)
    std::unordered_map<std::string, std::string> shortcuts;
};

// =============================================================================
// Selection Changed Event
// =============================================================================

/**
 * @brief Selection change event data
 */
struct SelectionChangedEvent {
    std::vector<SceneNode*> previousSelection;
    std::vector<SceneNode*> newSelection;
};

// =============================================================================
// Panel Factory Function
// =============================================================================

using PanelFactoryFunc = std::function<std::shared_ptr<EditorPanel>()>;

// =============================================================================
// Editor Application
// =============================================================================

/**
 * @brief Main editor application class
 *
 * Coordinates all editor systems including panels, menus, toolbars,
 * project/scene management, selection, commands, and settings.
 *
 * Usage:
 *   EditorApplication& editor = EditorApplication::Instance();
 *   if (!editor.Initialize()) return -1;
 *
 *   // Main loop
 *   while (editor.IsRunning()) {
 *       editor.Update(deltaTime);
 *       editor.Render();
 *   }
 *
 *   editor.Shutdown();
 */
class EditorApplication {
public:
    // =========================================================================
    // Singleton & Lifecycle
    // =========================================================================

    /**
     * @brief Get singleton instance
     */
    static EditorApplication& Instance();

    // Delete copy/move
    EditorApplication(const EditorApplication&) = delete;
    EditorApplication& operator=(const EditorApplication&) = delete;
    EditorApplication(EditorApplication&&) = delete;
    EditorApplication& operator=(EditorApplication&&) = delete;

    /**
     * @brief Initialize the editor application
     * @return true if initialization succeeded
     */
    bool Initialize();

    /**
     * @brief Shutdown the editor application
     */
    void Shutdown();

    /**
     * @brief Check if editor is initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Check if editor should continue running
     */
    [[nodiscard]] bool IsRunning() const { return m_running; }

    /**
     * @brief Request editor shutdown
     */
    void RequestShutdown();

    /**
     * @brief Update editor state
     * @param deltaTime Time since last update in seconds
     */
    void Update(float deltaTime);

    /**
     * @brief Render the entire editor UI
     */
    void Render();

    /**
     * @brief Handle input events
     */
    void HandleInput();

    // =========================================================================
    // Panel Management
    // =========================================================================

    /**
     * @brief Register a panel type with a factory function
     * @tparam T Panel type (must derive from EditorPanel)
     * @param name Unique panel name
     */
    template<typename T>
    void RegisterPanel(const std::string& name) {
        static_assert(std::is_base_of_v<EditorPanel, T>, "T must derive from EditorPanel");
        m_panelFactories[name] = []() -> std::shared_ptr<EditorPanel> {
            return std::make_shared<T>();
        };
        m_panelTypeMap[std::type_index(typeid(T))] = name;
    }

    /**
     * @brief Create a panel instance by name
     * @param name Panel type name
     * @return Created panel or nullptr if type not found
     */
    std::shared_ptr<EditorPanel> CreatePanel(const std::string& name);

    /**
     * @brief Create a panel instance by type
     * @tparam T Panel type
     * @return Created panel or nullptr if type not registered
     */
    template<typename T>
    std::shared_ptr<T> CreatePanel() {
        auto it = m_panelTypeMap.find(std::type_index(typeid(T)));
        if (it != m_panelTypeMap.end()) {
            return std::dynamic_pointer_cast<T>(CreatePanel(it->second));
        }
        return nullptr;
    }

    /**
     * @brief Get an existing panel by type
     * @tparam T Panel type
     * @return Panel pointer or nullptr if not found
     */
    template<typename T>
    std::shared_ptr<T> GetPanel() {
        for (auto& [name, panel] : m_panels) {
            if (auto typed = std::dynamic_pointer_cast<T>(panel)) {
                return typed;
            }
        }
        return nullptr;
    }

    /**
     * @brief Get an existing panel by name
     * @param name Panel name
     * @return Panel pointer or nullptr if not found
     */
    std::shared_ptr<EditorPanel> GetPanel(const std::string& name);

    /**
     * @brief Get all registered panels
     */
    [[nodiscard]] std::vector<std::shared_ptr<EditorPanel>> GetAllPanels() const;

    /**
     * @brief Show a panel by name
     */
    void ShowPanel(const std::string& name);

    /**
     * @brief Hide a panel by name
     */
    void HidePanel(const std::string& name);

    /**
     * @brief Toggle panel visibility
     */
    void TogglePanel(const std::string& name);

    /**
     * @brief Check if panel is visible
     */
    [[nodiscard]] bool IsPanelVisible(const std::string& name) const;

    /**
     * @brief Focus a panel (bring to front)
     */
    void FocusPanel(const std::string& name);

    /**
     * @brief Get registered panel names
     */
    [[nodiscard]] std::vector<std::string> GetRegisteredPanelNames() const;

    // =========================================================================
    // Project Management
    // =========================================================================

    /**
     * @brief Create a new project
     * @param path Project directory path
     * @param name Project name
     * @return true if project was created
     */
    bool NewProject(const std::filesystem::path& path, const std::string& name);

    /**
     * @brief Open an existing project
     * @param path Project file path
     * @return true if project was opened
     */
    bool OpenProject(const std::filesystem::path& path);

    /**
     * @brief Save current project
     * @return true if save succeeded
     */
    bool SaveProject();

    /**
     * @brief Save project to a new location
     * @param path New project file path
     * @return true if save succeeded
     */
    bool SaveProjectAs(const std::filesystem::path& path);

    /**
     * @brief Close current project
     * @return true if project was closed (false if cancelled)
     */
    bool CloseProject();

    /**
     * @brief Check if a project is currently open
     */
    [[nodiscard]] bool HasProject() const { return m_hasProject; }

    /**
     * @brief Get current project path
     */
    [[nodiscard]] const std::filesystem::path& GetProjectPath() const { return m_projectPath; }

    /**
     * @brief Get current project name
     */
    [[nodiscard]] const std::string& GetProjectName() const { return m_projectName; }

    /**
     * @brief Check if project has unsaved changes
     */
    [[nodiscard]] bool IsProjectDirty() const { return m_projectDirty; }

    /**
     * @brief Mark project as dirty (has unsaved changes)
     */
    void MarkProjectDirty();

    /**
     * @brief Get recent projects list
     */
    [[nodiscard]] const std::vector<RecentProject>& GetRecentProjects() const { return m_recentProjects; }

    /**
     * @brief Clear recent projects list
     */
    void ClearRecentProjects();

    // =========================================================================
    // Scene Management
    // =========================================================================

    /**
     * @brief Create a new scene
     * @return true if scene was created
     */
    bool NewScene();

    /**
     * @brief Open a scene file
     * @param path Scene file path
     * @return true if scene was opened
     */
    bool OpenScene(const std::filesystem::path& path);

    /**
     * @brief Save current scene
     * @return true if save succeeded
     */
    bool SaveScene();

    /**
     * @brief Save scene to a new location
     * @param path New scene file path
     * @return true if save succeeded
     */
    bool SaveSceneAs(const std::filesystem::path& path);

    /**
     * @brief Get active scene
     */
    [[nodiscard]] Scene* GetActiveScene() { return m_activeScene.get(); }
    [[nodiscard]] const Scene* GetActiveScene() const { return m_activeScene.get(); }

    /**
     * @brief Check if scene has unsaved changes
     */
    [[nodiscard]] bool IsSceneDirty() const { return m_sceneDirty; }

    /**
     * @brief Mark scene as dirty
     */
    void MarkSceneDirty();

    /**
     * @brief Get current scene path
     */
    [[nodiscard]] const std::filesystem::path& GetScenePath() const { return m_scenePath; }

    /**
     * @brief Get open scene tabs (for multi-scene editing)
     */
    [[nodiscard]] const std::vector<std::filesystem::path>& GetOpenScenes() const { return m_openScenes; }

    /**
     * @brief Switch to a different open scene
     */
    void SwitchToScene(size_t index);

    /**
     * @brief Close a scene tab
     */
    bool CloseScene(size_t index);

    // =========================================================================
    // Selection System
    // =========================================================================

    /**
     * @brief Select a single object (clears previous selection)
     */
    void SelectObject(SceneNode* node);

    /**
     * @brief Add object to selection
     */
    void AddToSelection(SceneNode* node);

    /**
     * @brief Remove object from selection
     */
    void RemoveFromSelection(SceneNode* node);

    /**
     * @brief Clear current selection
     */
    void ClearSelection();

    /**
     * @brief Get currently selected objects
     */
    [[nodiscard]] const std::vector<SceneNode*>& GetSelection() const { return m_selection; }

    /**
     * @brief Check if a node is selected
     */
    [[nodiscard]] bool IsSelected(const SceneNode* node) const;

    /**
     * @brief Get primary (last) selected object
     */
    [[nodiscard]] SceneNode* GetPrimarySelection() const;

    /**
     * @brief Select all objects in scene
     */
    void SelectAll();

    /**
     * @brief Invert selection
     */
    void InvertSelection();

    /**
     * @brief Focus viewport on selection
     */
    void FocusOnSelection();

    /**
     * @brief Set selection changed callback
     */
    void SetOnSelectionChanged(std::function<void(const SelectionChangedEvent&)> callback);

    // =========================================================================
    // Clipboard Operations
    // =========================================================================

    /**
     * @brief Cut selected entities to clipboard
     *
     * Serializes selected entities to JSON, stores in clipboard with isCut=true,
     * and deletes the original entities.
     */
    void CutSelection();

    /**
     * @brief Copy selected entities to clipboard
     *
     * Serializes selected entities to JSON and stores in clipboard with isCut=false.
     */
    void CopySelection();

    /**
     * @brief Paste entities from clipboard
     *
     * Creates new entities from clipboard JSON data, offsets their positions
     * to avoid overlap, and selects the new entities.
     */
    void PasteSelection();

    /**
     * @brief Check if clipboard has content to paste
     */
    [[nodiscard]] bool HasClipboardContent() const;

    // =========================================================================
    // Object Creation
    // =========================================================================

    /**
     * @brief Create an empty game object in the scene
     * @param parent Optional parent node (nullptr for root)
     * @return Pointer to created node
     */
    SceneNode* CreateEmptyObject(SceneNode* parent = nullptr);

    /**
     * @brief Group selected objects under a new parent
     * @return Pointer to group node, or nullptr if failed
     */
    SceneNode* GroupSelection();

    // =========================================================================
    // Command System (Undo/Redo)
    // =========================================================================

    /**
     * @brief Execute a command and add to history
     * @param command Command to execute (ownership transferred)
     * @return true if execution succeeded
     */
    bool ExecuteCommand(std::unique_ptr<ICommand> command);

    /**
     * @brief Undo the last command
     * @return true if undo succeeded
     */
    bool Undo();

    /**
     * @brief Redo the last undone command
     * @return true if redo succeeded
     */
    bool Redo();

    /**
     * @brief Check if undo is available
     */
    [[nodiscard]] bool CanUndo() const;

    /**
     * @brief Check if redo is available
     */
    [[nodiscard]] bool CanRedo() const;

    /**
     * @brief Get undo command name
     */
    [[nodiscard]] std::string GetUndoCommandName() const;

    /**
     * @brief Get redo command name
     */
    [[nodiscard]] std::string GetRedoCommandName() const;

    /**
     * @brief Get command history
     */
    [[nodiscard]] CommandHistory& GetCommandHistory() { return m_commandHistory; }
    [[nodiscard]] const CommandHistory& GetCommandHistory() const { return m_commandHistory; }

    /**
     * @brief Get undo history names
     */
    [[nodiscard]] std::vector<std::string> GetUndoHistory(size_t maxCount = 20) const;

    /**
     * @brief Get redo history names
     */
    [[nodiscard]] std::vector<std::string> GetRedoHistory(size_t maxCount = 20) const;

    // =========================================================================
    // Transform Tools
    // =========================================================================

    /**
     * @brief Set active transform tool
     */
    void SetTransformTool(TransformTool tool);

    /**
     * @brief Get active transform tool
     */
    [[nodiscard]] TransformTool GetTransformTool() const { return m_transformTool; }

    /**
     * @brief Set transform space
     */
    void SetTransformSpace(TransformSpace space);

    /**
     * @brief Get transform space
     */
    [[nodiscard]] TransformSpace GetTransformSpace() const { return m_transformSpace; }

    /**
     * @brief Toggle transform space between World and Local
     */
    void ToggleTransformSpace();

    /**
     * @brief Get transform gizmo
     */
    [[nodiscard]] TransformGizmo* GetTransformGizmo() { return m_transformGizmo.get(); }

    // =========================================================================
    // Play Mode
    // =========================================================================

    /**
     * @brief Enter play mode
     */
    void Play();

    /**
     * @brief Pause play mode
     */
    void Pause();

    /**
     * @brief Stop play mode and return to editing
     */
    void Stop();

    /**
     * @brief Step one frame while paused
     */
    void StepFrame();

    /**
     * @brief Get current play state
     */
    [[nodiscard]] EditorPlayState GetPlayState() const { return m_playState; }

    /**
     * @brief Check if in play mode (Playing or Paused)
     */
    [[nodiscard]] bool IsPlaying() const { return m_playState != EditorPlayState::Editing; }

    // =========================================================================
    // Settings/Preferences
    // =========================================================================

    /**
     * @brief Load settings from file
     * @return true if load succeeded
     */
    bool LoadSettings();

    /**
     * @brief Save settings to file
     * @return true if save succeeded
     */
    bool SaveSettings();

    /**
     * @brief Get current settings
     */
    [[nodiscard]] EditorSettings& GetSettings() { return m_settings; }
    [[nodiscard]] const EditorSettings& GetSettings() const { return m_settings; }

    /**
     * @brief Apply settings changes
     */
    void ApplySettings();

    /**
     * @brief Reset settings to defaults
     */
    void ResetSettings();

    /**
     * @brief Open preferences window
     */
    void ShowPreferencesWindow();

    // =========================================================================
    // Layouts
    // =========================================================================

    /**
     * @brief Save current layout
     * @param name Layout name
     */
    void SaveLayout(const std::string& name);

    /**
     * @brief Load a layout
     * @param name Layout name
     * @return true if load succeeded
     */
    bool LoadLayout(const std::string& name);

    /**
     * @brief Delete a layout
     * @param name Layout name
     */
    void DeleteLayout(const std::string& name);

    /**
     * @brief Get available layouts
     */
    [[nodiscard]] std::vector<std::string> GetLayoutNames() const;

    /**
     * @brief Reset to default layout
     */
    void ResetLayout();

    // =========================================================================
    // Notifications
    // =========================================================================

    /**
     * @brief Show a notification
     * @param message Notification message
     * @param type Notification type
     * @param duration Display duration in seconds (0 = persistent)
     */
    void ShowNotification(const std::string& message, NotificationType type = NotificationType::Info,
                          float duration = 3.0f);

    /**
     * @brief Show notification with click action
     */
    void ShowNotification(const std::string& message, NotificationType type,
                          float duration, std::function<void()> onClick);

    /**
     * @brief Dismiss a notification
     */
    void DismissNotification(size_t index);

    /**
     * @brief Clear all notifications
     */
    void ClearNotifications();

    // =========================================================================
    // Progress Tasks
    // =========================================================================

    /**
     * @brief Start a progress task
     * @param id Unique task identifier
     * @param description Task description
     * @param indeterminate If true, show spinner instead of progress
     * @return Task reference for updates
     */
    ProgressTask& StartProgressTask(const std::string& id, const std::string& description,
                                     bool indeterminate = false);

    /**
     * @brief Update progress task
     * @param id Task identifier
     * @param progress Progress value (0.0 to 1.0)
     * @param description Optional new description
     */
    void UpdateProgressTask(const std::string& id, float progress,
                            const std::string& description = "");

    /**
     * @brief Complete a progress task
     * @param id Task identifier
     */
    void CompleteProgressTask(const std::string& id);

    /**
     * @brief Cancel a progress task
     * @param id Task identifier
     */
    void CancelProgressTask(const std::string& id);

    /**
     * @brief Check if any tasks are running
     */
    [[nodiscard]] bool HasActiveTasks() const { return !m_progressTasks.empty(); }

    // =========================================================================
    // Dialogs
    // =========================================================================

    /**
     * @brief Show a modal dialog with OK button
     * @param title Dialog title
     * @param message Dialog message
     */
    void ShowMessageDialog(const std::string& title, const std::string& message);

    /**
     * @brief Show a confirmation dialog
     * @param title Dialog title
     * @param message Dialog message
     * @param onConfirm Callback when confirmed
     * @param onCancel Callback when cancelled (optional)
     */
    void ShowConfirmDialog(const std::string& title, const std::string& message,
                           std::function<void()> onConfirm,
                           std::function<void()> onCancel = nullptr);

    /**
     * @brief Show file open dialog
     * @param title Dialog title
     * @param filters File type filters (e.g., "Scene Files (*.scene)|*.scene")
     * @param callback Called with selected path (empty if cancelled)
     */
    void ShowOpenFileDialog(const std::string& title, const std::string& filters,
                            std::function<void(const std::filesystem::path&)> callback);

    /**
     * @brief Show file save dialog
     * @param title Dialog title
     * @param filters File type filters
     * @param defaultName Default file name
     * @param callback Called with selected path (empty if cancelled)
     */
    void ShowSaveFileDialog(const std::string& title, const std::string& filters,
                            const std::string& defaultName,
                            std::function<void(const std::filesystem::path&)> callback);

    /**
     * @brief Show a text input dialog
     * @param title Dialog title
     * @param prompt Prompt text shown above the input field
     * @param callback Called with user input (empty if cancelled)
     * @param defaultValue Optional default value for the input
     */
    void ShowInputDialog(const std::string& title, const std::string& prompt,
                        std::function<void(const std::string&)> callback,
                        const std::string& defaultValue = "");

    /**
     * @brief Show the new asset creation dialog
     * @param preselectedType Optional type to preselect (defaults to SDFModel)
     */
    void ShowNewAssetDialog(CreatableAssetType preselectedType = CreatableAssetType::SDFModel);

    /**
     * @brief Get the asset creation dialog
     */
    [[nodiscard]] AssetCreationDialog& GetAssetCreationDialog() { return m_assetCreationDialog; }

    // =========================================================================
    // Keyboard Shortcuts
    // =========================================================================

    /**
     * @brief Register a keyboard shortcut
     * @param action Action name (e.g., "Undo", "Save")
     * @param shortcut Shortcut string (e.g., "Ctrl+Z")
     * @param handler Action handler
     */
    void RegisterShortcut(const std::string& action, const std::string& shortcut,
                          std::function<void()> handler);

    /**
     * @brief Check if shortcut is pressed this frame
     */
    [[nodiscard]] bool IsShortcutPressed(const std::string& action) const;

    /**
     * @brief Get shortcut for action
     */
    [[nodiscard]] std::string GetShortcutForAction(const std::string& action) const;

    // =========================================================================
    // Accessors
    // =========================================================================

    /**
     * @brief Get engine reference
     */
    [[nodiscard]] Engine& GetEngine() { return Engine::Instance(); }
    [[nodiscard]] const Engine& GetEngine() const { return Engine::Instance(); }

    /**
     * @brief Get input manager
     */
    [[nodiscard]] InputManager& GetInput() { return GetEngine().GetInput(); }
    [[nodiscard]] const InputManager& GetInput() const { return GetEngine().GetInput(); }

    /**
     * @brief Get delta time from last frame
     */
    [[nodiscard]] float GetDeltaTime() const { return m_deltaTime; }

    /**
     * @brief Get frames per second
     */
    [[nodiscard]] float GetFPS() const { return m_fps; }

    /**
     * @brief Get memory usage in MB
     */
    [[nodiscard]] float GetMemoryUsageMB() const { return m_memoryUsageMB; }

private:
    EditorApplication() = default;
    ~EditorApplication() = default;

    // =========================================================================
    // Initialization
    // =========================================================================

    void RegisterDefaultPanels();
    void CreateDefaultPanels();
    void SetupDefaultLayout();
    void RegisterDefaultShortcuts();
    void LoadRecentProjects();
    void SaveRecentProjects();
    void AddToRecentProjects(const std::filesystem::path& path);

    // =========================================================================
    // Rendering
    // =========================================================================

    void RenderMenuBar();
    void RenderFileMenu();
    void RenderEditMenu();
    void RenderViewMenu();
    void RenderGameObjectMenu();
    void RenderComponentMenu();
    void RenderWindowMenu();
    void RenderAIMenu();
    void RenderHelpMenu();

    void RenderToolbar();
    void RenderTransformTools();
    void RenderSnapToggles();
    void RenderSpaceToggle();
    void RenderPlayControls();
    void RenderSearchBox();

    void RenderStatusBar();
    void RenderSelectionInfo();
    void RenderFpsCounter();
    void RenderMemoryUsage();
    void RenderActiveToolName();
    void RenderProgressTasks();
    void RenderNotifications();

    void RenderDockSpace();
    void RenderPanels();

    void RenderModalDialogs();
    void RenderPreferencesWindow();

    // =========================================================================
    // Input Handling
    // =========================================================================

    void ProcessShortcuts();
    void HandleGlobalShortcuts();
    bool ParseShortcut(const std::string& shortcut, int& key, int& modifiers) const;
    bool IsShortcutActive(int key, int modifiers) const;

    // =========================================================================
    // Utility
    // =========================================================================

    void UpdateFrameStats(float deltaTime);
    void UpdateAutoSave(float deltaTime);
    void UpdateNotifications(float deltaTime);
    void CollectSceneNodes(SceneNode* node, std::vector<SceneNode*>& outNodes);
    std::string GetWindowTitle() const;
    void NotifySelectionChanged(const std::vector<SceneNode*>& previous);

    // =========================================================================
    // Play Mode Serialization
    // =========================================================================

    /**
     * @brief Serialize the entire scene state to JSON for play mode save/restore
     * @return JSON string containing full scene state, or empty string on failure
     */
    std::string SerializeFullScene();

    /**
     * @brief Deserialize and restore the scene state from JSON
     * @param jsonState The JSON string containing the saved scene state
     * @return true if restoration succeeded
     */
    bool DeserializeFullScene(const std::string& jsonState);

    /**
     * @brief Clear entities created during play mode
     */
    void ClearPlayModeEntities();

    // =========================================================================
    // Member Variables
    // =========================================================================

    // State
    bool m_initialized = false;
    bool m_running = false;
    float m_deltaTime = 0.0f;
    float m_fps = 0.0f;
    float m_memoryUsageMB = 0.0f;
    float m_autoSaveTimer = 0.0f;

    // Panel management
    std::unordered_map<std::string, PanelFactoryFunc> m_panelFactories;
    std::unordered_map<std::type_index, std::string> m_panelTypeMap;
    std::unordered_map<std::string, std::shared_ptr<EditorPanel>> m_panels;

    // Project
    bool m_hasProject = false;
    bool m_projectDirty = false;
    std::filesystem::path m_projectPath;
    std::string m_projectName;
    std::vector<RecentProject> m_recentProjects;
    static constexpr size_t MAX_RECENT_PROJECTS = 10;

    // Scene
    std::unique_ptr<Scene> m_activeScene;
    std::unique_ptr<Scene> m_savedSceneState;  // Saved state for play mode restore
    std::filesystem::path m_scenePath;
    bool m_sceneDirty = false;
    std::vector<std::filesystem::path> m_openScenes;
    size_t m_activeSceneIndex = 0;

    // Scene state for play mode (JSON serialization)
    std::string m_prePlaySceneState;
    glm::vec3 m_prePlayCameraPosition{0.0f};
    float m_prePlayCameraPitch = 0.0f;
    float m_prePlayCameraYaw = -90.0f;
    float m_prePlayCameraFOV = 45.0f;

    // Selection
    std::vector<SceneNode*> m_selection;
    std::unordered_set<const SceneNode*> m_selectionSet;
    std::function<void(const SelectionChangedEvent&)> m_onSelectionChanged;

    // Commands
    CommandHistory m_commandHistory;

    // Transform tools
    TransformTool m_transformTool = TransformTool::Select;
    TransformSpace m_transformSpace = TransformSpace::World;
    std::unique_ptr<TransformGizmo> m_transformGizmo;

    // Play state
    EditorPlayState m_playState = EditorPlayState::Editing;

    // Settings
    EditorSettings m_settings;
    bool m_showPreferencesWindow = false;
    bool m_showAISetupWizard = false;

    // Layouts
    std::unordered_map<std::string, LayoutPreset> m_layouts;

    // Notifications
    std::deque<EditorNotification> m_notifications;
    static constexpr size_t MAX_NOTIFICATIONS = 5;

    // Progress tasks
    std::unordered_map<std::string, ProgressTask> m_progressTasks;

    // Dialogs
    struct DialogState {
        bool isOpen = false;
        std::string title;
        std::string message;
        std::function<void()> onConfirm;
        std::function<void()> onCancel;
        enum class Type { Message, Confirm, OpenFile, SaveFile, Input } type = Type::Message;
        std::string filters;
        std::string defaultName;
        std::function<void(const std::filesystem::path&)> fileCallback;
        std::function<void(const std::string&)> inputCallback;
        char inputBuffer[512] = {};  // Buffer for input dialog
    };
    DialogState m_dialogState;

    // Asset Creation Dialog
    AssetCreationDialog m_assetCreationDialog;

    // Shortcuts
    struct ShortcutBinding {
        int key = 0;
        int modifiers = 0;  // Ctrl, Shift, Alt flags
        std::function<void()> handler;
    };
    std::unordered_map<std::string, ShortcutBinding> m_shortcuts;

    // Clipboard for scene nodes
    struct ClipboardEntry {
        std::string name;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
        std::string assetPath;
        // Store serialized node data for full restoration
        // FUTURE: Add component data serialization
    };
    std::vector<ClipboardEntry> m_clipboard;
    bool m_clipboardIsCut = false;

    // Toolbar search
    char m_searchBuffer[256] = "";

    // Frame timing
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    float m_frameTimeAccumulator = 0.0f;
    int m_frameCount = 0;
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Get display name for transform tool
 */
const char* GetTransformToolName(TransformTool tool);

/**
 * @brief Get icon for transform tool
 */
const char* GetTransformToolIcon(TransformTool tool);

/**
 * @brief Get display name for transform space
 */
const char* GetTransformSpaceName(TransformSpace space);

/**
 * @brief Get display name for play state
 */
const char* GetPlayStateName(EditorPlayState state);

/**
 * @brief Get color for notification type
 */
glm::vec4 GetNotificationColor(NotificationType type);

} // namespace Nova
