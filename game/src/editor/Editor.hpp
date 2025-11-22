#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <stack>
#include <unordered_map>
#include <glm/glm.hpp>

// Forward declarations for ImGui
struct ImGuiContext;
struct GLFWwindow;

namespace Nova {
    class Engine;
    class Window;
    class Renderer;
}

namespace Vehement {

// Forward declarations
class Game;
class World;
class EntityManager;
class ConfigEditor;
class WorldView;
class TileInspector;
class PCGPanel;
class LocationCrafter;
class ScriptEditor;
class AssetBrowser;
class Hierarchy;
class Inspector;
class Console;
class Toolbar;

/**
 * @brief Command for undo/redo system
 */
class EditorCommand {
public:
    virtual ~EditorCommand() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    [[nodiscard]] virtual std::string GetDescription() const = 0;
};

/**
 * @brief Editor theme configuration
 */
struct EditorTheme {
    glm::vec4 windowBg{0.1f, 0.1f, 0.12f, 1.0f};
    glm::vec4 titleBg{0.15f, 0.15f, 0.18f, 1.0f};
    glm::vec4 titleBgActive{0.2f, 0.2f, 0.25f, 1.0f};
    glm::vec4 frameBg{0.18f, 0.18f, 0.22f, 1.0f};
    glm::vec4 frameBgHovered{0.25f, 0.25f, 0.3f, 1.0f};
    glm::vec4 frameBgActive{0.3f, 0.3f, 0.35f, 1.0f};
    glm::vec4 button{0.25f, 0.25f, 0.3f, 1.0f};
    glm::vec4 buttonHovered{0.35f, 0.35f, 0.4f, 1.0f};
    glm::vec4 buttonActive{0.4f, 0.4f, 0.45f, 1.0f};
    glm::vec4 header{0.2f, 0.2f, 0.25f, 1.0f};
    glm::vec4 headerHovered{0.3f, 0.3f, 0.35f, 1.0f};
    glm::vec4 headerActive{0.35f, 0.35f, 0.4f, 1.0f};
    glm::vec4 tab{0.15f, 0.15f, 0.18f, 1.0f};
    glm::vec4 tabHovered{0.3f, 0.3f, 0.35f, 1.0f};
    glm::vec4 tabActive{0.25f, 0.25f, 0.3f, 1.0f};
    glm::vec4 text{0.95f, 0.95f, 0.95f, 1.0f};
    glm::vec4 textDisabled{0.5f, 0.5f, 0.5f, 1.0f};
    glm::vec4 accent{0.4f, 0.6f, 1.0f, 1.0f};
    glm::vec4 accentHovered{0.5f, 0.7f, 1.0f, 1.0f};
    glm::vec4 success{0.2f, 0.8f, 0.3f, 1.0f};
    glm::vec4 warning{1.0f, 0.8f, 0.2f, 1.0f};
    glm::vec4 error{1.0f, 0.3f, 0.3f, 1.0f};
    float windowRounding = 4.0f;
    float frameRounding = 2.0f;
    float grabRounding = 2.0f;
};

/**
 * @brief Keyboard shortcut definition
 */
struct KeyboardShortcut {
    int key = 0;
    int modifiers = 0;  // GLFW modifier bits
    std::string action;
    std::string description;
    std::function<void()> callback;
};

/**
 * @brief Editor configuration
 */
struct EditorConfig {
    std::string projectPath;
    std::string layoutPath = "config/editor_layout.json";
    bool showDemoWindow = false;
    bool enableVsync = true;
    int targetFPS = 60;
    float autosaveInterval = 300.0f;  // 5 minutes
    EditorTheme theme;
};

/**
 * @brief Main Editor Application
 *
 * Provides a comprehensive ImGui-based editor for the Nova3D/Vehement2 engine:
 * - Dockspace layout with saveable window positions
 * - Menu bar with File, Edit, View, Tools, Help menus
 * - Keyboard shortcuts system
 * - Undo/redo command system
 * - Project save/load functionality
 *
 * Can run standalone or integrated with the game.
 */
class Editor {
public:
    /**
     * @brief Editor mode
     */
    enum class Mode {
        Standalone,     // Editor runs independently
        Integrated      // Editor runs alongside game
    };

    Editor();
    ~Editor();

    // Non-copyable, non-movable
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;
    Editor(Editor&&) = delete;
    Editor& operator=(Editor&&) = delete;

    // =========================================================================
    // Initialization and Lifecycle
    // =========================================================================

    /**
     * @brief Initialize the editor
     * @param engine Reference to Nova engine
     * @param config Editor configuration
     * @return true if initialization succeeded
     */
    bool Initialize(Nova::Engine& engine, const EditorConfig& config = {});

    /**
     * @brief Initialize for standalone mode (creates own window)
     * @param config Editor configuration
     * @return true if initialization succeeded
     */
    bool InitializeStandalone(const EditorConfig& config = {});

    /**
     * @brief Shutdown the editor
     */
    void Shutdown();

    /**
     * @brief Check if editor is initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Run the editor main loop (standalone mode only)
     * @return Exit code
     */
    int Run();

    /**
     * @brief Update editor state (call each frame in integrated mode)
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render the editor UI
     */
    void Render();

    /**
     * @brief Process input events
     */
    void ProcessInput();

    // =========================================================================
    // Panel Access
    // =========================================================================

    [[nodiscard]] ConfigEditor* GetConfigEditor() { return m_configEditor.get(); }
    [[nodiscard]] WorldView* GetWorldView() { return m_worldView.get(); }
    [[nodiscard]] TileInspector* GetTileInspector() { return m_tileInspector.get(); }
    [[nodiscard]] PCGPanel* GetPCGPanel() { return m_pcgPanel.get(); }
    [[nodiscard]] LocationCrafter* GetLocationCrafter() { return m_locationCrafter.get(); }
    [[nodiscard]] ScriptEditor* GetScriptEditor() { return m_scriptEditor.get(); }
    [[nodiscard]] AssetBrowser* GetAssetBrowser() { return m_assetBrowser.get(); }
    [[nodiscard]] Hierarchy* GetHierarchy() { return m_hierarchy.get(); }
    [[nodiscard]] Inspector* GetInspector() { return m_inspector.get(); }
    [[nodiscard]] Console* GetConsole() { return m_console.get(); }
    [[nodiscard]] Toolbar* GetToolbar() { return m_toolbar.get(); }

    // =========================================================================
    // Undo/Redo System
    // =========================================================================

    /**
     * @brief Execute a command and add to undo stack
     */
    void ExecuteCommand(std::unique_ptr<EditorCommand> command);

    /**
     * @brief Undo the last command
     */
    void Undo();

    /**
     * @brief Redo the last undone command
     */
    void Redo();

    /**
     * @brief Check if undo is available
     */
    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }

    /**
     * @brief Check if redo is available
     */
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }

    /**
     * @brief Clear undo/redo history
     */
    void ClearHistory();

    // =========================================================================
    // Project Management
    // =========================================================================

    /**
     * @brief Create a new project
     * @param path Project directory path
     * @return true if successful
     */
    bool NewProject(const std::string& path);

    /**
     * @brief Open an existing project
     * @param path Project file path
     * @return true if successful
     */
    bool OpenProject(const std::string& path);

    /**
     * @brief Save the current project
     * @return true if successful
     */
    bool SaveProject();

    /**
     * @brief Save project to a new location
     * @param path New project file path
     * @return true if successful
     */
    bool SaveProjectAs(const std::string& path);

    /**
     * @brief Close the current project
     */
    void CloseProject();

    /**
     * @brief Check if a project is currently open
     */
    [[nodiscard]] bool HasOpenProject() const { return !m_projectPath.empty(); }

    /**
     * @brief Check if project has unsaved changes
     */
    [[nodiscard]] bool HasUnsavedChanges() const { return m_hasUnsavedChanges; }

    /**
     * @brief Mark project as having unsaved changes
     */
    void MarkDirty() { m_hasUnsavedChanges = true; }

    // =========================================================================
    // Layout Management
    // =========================================================================

    /**
     * @brief Save current window layout
     * @param path Layout file path
     */
    bool SaveLayout(const std::string& path);

    /**
     * @brief Load window layout
     * @param path Layout file path
     */
    bool LoadLayout(const std::string& path);

    /**
     * @brief Reset layout to default
     */
    void ResetLayout();

    // =========================================================================
    // Keyboard Shortcuts
    // =========================================================================

    /**
     * @brief Register a keyboard shortcut
     */
    void RegisterShortcut(const KeyboardShortcut& shortcut);

    /**
     * @brief Unregister a keyboard shortcut
     */
    void UnregisterShortcut(const std::string& action);

    /**
     * @brief Get all registered shortcuts
     */
    [[nodiscard]] const std::vector<KeyboardShortcut>& GetShortcuts() const { return m_shortcuts; }

    // =========================================================================
    // Theme
    // =========================================================================

    /**
     * @brief Apply editor theme
     */
    void ApplyTheme(const EditorTheme& theme);

    /**
     * @brief Get current theme
     */
    [[nodiscard]] const EditorTheme& GetTheme() const { return m_config.theme; }

    // =========================================================================
    // Game Integration
    // =========================================================================

    /**
     * @brief Set game reference for integrated mode
     */
    void SetGame(Game* game) { m_game = game; }

    /**
     * @brief Get game reference
     */
    [[nodiscard]] Game* GetGame() { return m_game; }

    /**
     * @brief Set world reference
     */
    void SetWorld(World* world) { m_world = world; }

    /**
     * @brief Get world reference
     */
    [[nodiscard]] World* GetWorld() { return m_world; }

    /**
     * @brief Set entity manager reference
     */
    void SetEntityManager(EntityManager* entityManager) { m_entityManager = entityManager; }

    /**
     * @brief Get entity manager reference
     */
    [[nodiscard]] EntityManager* GetEntityManager() { return m_entityManager; }

    // =========================================================================
    // Window Visibility
    // =========================================================================

    void SetConfigEditorVisible(bool visible) { m_showConfigEditor = visible; }
    void SetWorldViewVisible(bool visible) { m_showWorldView = visible; }
    void SetTileInspectorVisible(bool visible) { m_showTileInspector = visible; }
    void SetPCGPanelVisible(bool visible) { m_showPCGPanel = visible; }
    void SetLocationCrafterVisible(bool visible) { m_showLocationCrafter = visible; }
    void SetScriptEditorVisible(bool visible) { m_showScriptEditor = visible; }
    void SetAssetBrowserVisible(bool visible) { m_showAssetBrowser = visible; }
    void SetHierarchyVisible(bool visible) { m_showHierarchy = visible; }
    void SetInspectorVisible(bool visible) { m_showInspector = visible; }
    void SetConsoleVisible(bool visible) { m_showConsole = visible; }

    [[nodiscard]] bool IsConfigEditorVisible() const { return m_showConfigEditor; }
    [[nodiscard]] bool IsWorldViewVisible() const { return m_showWorldView; }
    [[nodiscard]] bool IsTileInspectorVisible() const { return m_showTileInspector; }
    [[nodiscard]] bool IsPCGPanelVisible() const { return m_showPCGPanel; }
    [[nodiscard]] bool IsLocationCrafterVisible() const { return m_showLocationCrafter; }
    [[nodiscard]] bool IsScriptEditorVisible() const { return m_showScriptEditor; }
    [[nodiscard]] bool IsAssetBrowserVisible() const { return m_showAssetBrowser; }
    [[nodiscard]] bool IsHierarchyVisible() const { return m_showHierarchy; }
    [[nodiscard]] bool IsInspectorVisible() const { return m_showInspector; }
    [[nodiscard]] bool IsConsoleVisible() const { return m_showConsole; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void()> OnProjectNew;
    std::function<void(const std::string&)> OnProjectOpen;
    std::function<void()> OnProjectSave;
    std::function<void()> OnProjectClose;
    std::function<void()> OnPlay;
    std::function<void()> OnPause;
    std::function<void()> OnStop;

private:
    // ImGui initialization
    bool InitImGui(GLFWwindow* window);
    void ShutdownImGui();

    // UI rendering
    void BeginDockspace();
    void EndDockspace();
    void RenderMenuBar();
    void RenderFileMenu();
    void RenderEditMenu();
    void RenderViewMenu();
    void RenderToolsMenu();
    void RenderHelpMenu();
    void RenderStatusBar();
    void RenderPanels();

    // Dialogs
    void ShowNewProjectDialog();
    void ShowOpenProjectDialog();
    void ShowSaveAsDialog();
    void ShowAboutDialog();
    void ShowShortcutsDialog();
    void ShowSettingsDialog();

    // Shortcut handling
    void RegisterDefaultShortcuts();
    void ProcessShortcuts();

    // Autosave
    void CheckAutosave(float deltaTime);

    // Mode
    Mode m_mode = Mode::Integrated;
    bool m_initialized = false;
    bool m_running = false;

    // Configuration
    EditorConfig m_config;

    // ImGui context
    ImGuiContext* m_imguiContext = nullptr;
    GLFWwindow* m_window = nullptr;
    bool m_ownsWindow = false;  // True if we created the window

    // Engine reference
    Nova::Engine* m_engine = nullptr;

    // Game references (for integrated mode)
    Game* m_game = nullptr;
    World* m_world = nullptr;
    EntityManager* m_entityManager = nullptr;

    // Panels
    std::unique_ptr<ConfigEditor> m_configEditor;
    std::unique_ptr<WorldView> m_worldView;
    std::unique_ptr<TileInspector> m_tileInspector;
    std::unique_ptr<PCGPanel> m_pcgPanel;
    std::unique_ptr<LocationCrafter> m_locationCrafter;
    std::unique_ptr<ScriptEditor> m_scriptEditor;
    std::unique_ptr<AssetBrowser> m_assetBrowser;
    std::unique_ptr<Hierarchy> m_hierarchy;
    std::unique_ptr<Inspector> m_inspector;
    std::unique_ptr<Console> m_console;
    std::unique_ptr<Toolbar> m_toolbar;

    // Panel visibility
    bool m_showConfigEditor = true;
    bool m_showWorldView = true;
    bool m_showTileInspector = true;
    bool m_showPCGPanel = false;
    bool m_showLocationCrafter = false;
    bool m_showScriptEditor = false;
    bool m_showAssetBrowser = true;
    bool m_showHierarchy = true;
    bool m_showInspector = true;
    bool m_showConsole = true;
    bool m_showDemoWindow = false;
    bool m_showAboutDialog = false;
    bool m_showShortcutsDialog = false;
    bool m_showSettingsDialog = false;
    bool m_showNewProjectDialog = false;
    bool m_showOpenProjectDialog = false;
    bool m_showSaveAsDialog = false;

    // Project state
    std::string m_projectPath;
    bool m_hasUnsavedChanges = false;
    float m_autosaveTimer = 0.0f;

    // Undo/Redo
    std::stack<std::unique_ptr<EditorCommand>> m_undoStack;
    std::stack<std::unique_ptr<EditorCommand>> m_redoStack;
    static constexpr size_t MAX_UNDO_HISTORY = 100;

    // Shortcuts
    std::vector<KeyboardShortcut> m_shortcuts;

    // Dockspace ID
    unsigned int m_dockspaceId = 0;
    bool m_firstFrame = true;
};

} // namespace Vehement
