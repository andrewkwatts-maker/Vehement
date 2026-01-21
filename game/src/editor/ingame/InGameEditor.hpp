#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Nova {
    class Engine;
    class Renderer;
}

namespace Vehement {

// Forward declarations
class Game;
class World;
class MapEditor;
class TriggerEditor;
class ObjectEditor;
class CampaignEditor;
class ScenarioSettings;
class AIEditor;
class TerrainPanel;
class ObjectPalette;
class TriggerPanel;
class PropertiesPanel;

/**
 * @brief Permission levels for editor access
 */
enum class EditorPermission : uint8_t {
    Player,     // Basic map editing, can create custom games
    Modder,     // Extended access, can modify game objects
    Developer   // Full access, can modify core systems
};

/**
 * @brief Current state of the in-game editor
 */
enum class EditorState : uint8_t {
    Disabled,       // Editor is not active
    MapEditing,     // Editing terrain and objects
    TriggerEditing, // Creating triggers and events
    ObjectEditing,  // Modifying game object properties
    CampaignEditing,// Building campaign missions
    ScenarioConfig, // Configuring game rules
    AIEditing,      // Editing AI behavior
    Testing         // Testing the current map
};

/**
 * @brief Custom content metadata
 */
struct CustomContentInfo {
    std::string id;
    std::string name;
    std::string description;
    std::string author;
    std::string version;
    uint64_t createdTime = 0;
    uint64_t modifiedTime = 0;
    std::string thumbnailPath;
    std::vector<std::string> tags;
    bool isPublished = false;
    std::string workshopId;
};

/**
 * @brief Workshop publish settings
 */
struct WorkshopPublishSettings {
    std::string title;
    std::string description;
    std::vector<std::string> tags;
    std::string thumbnailPath;
    std::string changeNotes;
    bool isPublic = true;
    bool allowComments = true;
};

/**
 * @brief Main in-game editor integration
 *
 * Merges the development editor with the game client, allowing players
 * to create custom maps, campaigns, and game modes using the same tools
 * used for official content.
 *
 * Features:
 * - Seamless toggle between game and editor modes
 * - Same UI framework for both modes
 * - Permission levels (player, modder, developer)
 * - Save/load custom content
 * - Workshop integration for publishing and downloading
 */
class InGameEditor {
public:
    InGameEditor();
    ~InGameEditor();

    // Non-copyable
    InGameEditor(const InGameEditor&) = delete;
    InGameEditor& operator=(const InGameEditor&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the in-game editor
     * @param engine Reference to Nova engine
     * @param game Reference to the game
     * @return true if initialization succeeded
     */
    bool Initialize(Nova::Engine& engine, Game& game);

    /**
     * @brief Shutdown the editor
     */
    void Shutdown();

    /**
     * @brief Check if editor is initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Mode Switching
    // =========================================================================

    /**
     * @brief Toggle between game and editor mode
     */
    void ToggleEditor();

    /**
     * @brief Enter editor mode
     */
    void EnterEditorMode();

    /**
     * @brief Exit editor mode and return to game
     */
    void ExitEditorMode();

    /**
     * @brief Check if currently in editor mode
     */
    [[nodiscard]] bool IsInEditorMode() const noexcept {
        return m_state != EditorState::Disabled && m_state != EditorState::Testing;
    }

    /**
     * @brief Get current editor state
     */
    [[nodiscard]] EditorState GetState() const noexcept { return m_state; }

    /**
     * @brief Set editor state
     */
    void SetState(EditorState state);

    // =========================================================================
    // Update and Render
    // =========================================================================

    /**
     * @brief Update editor state
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
    // Permission System
    // =========================================================================

    /**
     * @brief Set current permission level
     */
    void SetPermission(EditorPermission permission);

    /**
     * @brief Get current permission level
     */
    [[nodiscard]] EditorPermission GetPermission() const noexcept { return m_permission; }

    /**
     * @brief Check if action is allowed at current permission level
     */
    [[nodiscard]] bool IsActionAllowed(const std::string& action) const;

    /**
     * @brief Unlock developer mode (requires password/key)
     */
    bool UnlockDeveloperMode(const std::string& key);

    // =========================================================================
    // Content Management
    // =========================================================================

    /**
     * @brief Create new custom map
     * @param name Map name
     * @param width Map width in tiles
     * @param height Map height in tiles
     * @return true if successful
     */
    bool NewMap(const std::string& name, int width, int height);

    /**
     * @brief Load custom map
     * @param path Path to map file
     * @return true if successful
     */
    bool LoadMap(const std::string& path);

    /**
     * @brief Save current map
     * @return true if successful
     */
    bool SaveMap();

    /**
     * @brief Save map to specific path
     * @param path Destination path
     * @return true if successful
     */
    bool SaveMapAs(const std::string& path);

    /**
     * @brief Create new campaign
     * @param name Campaign name
     * @return true if successful
     */
    bool NewCampaign(const std::string& name);

    /**
     * @brief Load campaign
     * @param path Path to campaign file
     * @return true if successful
     */
    bool LoadCampaign(const std::string& path);

    /**
     * @brief Save current campaign
     * @return true if successful
     */
    bool SaveCampaign();

    /**
     * @brief Get current content info
     */
    [[nodiscard]] const CustomContentInfo& GetContentInfo() const { return m_contentInfo; }

    /**
     * @brief Set content info
     */
    void SetContentInfo(const CustomContentInfo& info) { m_contentInfo = info; }

    // =========================================================================
    // Workshop Integration
    // =========================================================================

    /**
     * @brief Publish content to workshop
     * @param settings Publish settings
     * @return true if publish started (async operation)
     */
    bool PublishToWorkshop(const WorkshopPublishSettings& settings);

    /**
     * @brief Update published content
     * @param workshopId Workshop item ID
     * @param changeNotes Change notes
     * @return true if update started
     */
    bool UpdateWorkshopItem(const std::string& workshopId, const std::string& changeNotes);

    /**
     * @brief Download content from workshop
     * @param workshopId Workshop item ID
     * @return true if download started
     */
    bool DownloadFromWorkshop(const std::string& workshopId);

    /**
     * @brief Get workshop upload progress (0-100)
     */
    [[nodiscard]] float GetWorkshopProgress() const noexcept { return m_workshopProgress; }

    /**
     * @brief Check if workshop operation is in progress
     */
    [[nodiscard]] bool IsWorkshopBusy() const noexcept { return m_workshopBusy; }

    // =========================================================================
    // Testing
    // =========================================================================

    /**
     * @brief Start testing the current map
     */
    void StartTest();

    /**
     * @brief Stop testing and return to editor
     */
    void StopTest();

    /**
     * @brief Check if currently testing
     */
    [[nodiscard]] bool IsTesting() const noexcept { return m_state == EditorState::Testing; }

    // =========================================================================
    // Sub-Editor Access
    // =========================================================================

    [[nodiscard]] MapEditor* GetMapEditor() { return m_mapEditor.get(); }
    [[nodiscard]] TriggerEditor* GetTriggerEditor() { return m_triggerEditor.get(); }
    [[nodiscard]] ObjectEditor* GetObjectEditor() { return m_objectEditor.get(); }
    [[nodiscard]] CampaignEditor* GetCampaignEditor() { return m_campaignEditor.get(); }
    [[nodiscard]] ScenarioSettings* GetScenarioSettings() { return m_scenarioSettings.get(); }
    [[nodiscard]] AIEditor* GetAIEditor() { return m_aiEditor.get(); }

    // =========================================================================
    // Panel Access
    // =========================================================================

    [[nodiscard]] TerrainPanel* GetTerrainPanel() { return m_terrainPanel.get(); }
    [[nodiscard]] ObjectPalette* GetObjectPalette() { return m_objectPalette.get(); }
    [[nodiscard]] TriggerPanel* GetTriggerPanel() { return m_triggerPanel.get(); }
    [[nodiscard]] PropertiesPanel* GetPropertiesPanel() { return m_propertiesPanel.get(); }

    // =========================================================================
    // Undo/Redo
    // =========================================================================

    void Undo();
    void Redo();
    [[nodiscard]] bool CanUndo() const;
    [[nodiscard]] bool CanRedo() const;
    void ClearHistory();

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void()> OnEditorEnter;
    std::function<void()> OnEditorExit;
    std::function<void(const std::string&)> OnMapLoaded;
    std::function<void(const std::string&)> OnMapSaved;
    std::function<void(const std::string&)> OnWorkshopPublished;
    std::function<void(const std::string&)> OnWorkshopDownloaded;
    std::function<void()> OnTestStart;
    std::function<void()> OnTestStop;

private:
    // Rendering helpers
    void RenderMenuBar();
    void RenderToolbar();
    void RenderPanels();
    void RenderStatusBar();

    // Dialog helpers
    void ShowNewMapDialog();
    void ShowNewCampaignDialog();
    void ShowPublishDialog();
    void ShowSettingsDialog();
    void ShowSavePromptDialog();

    // Permission check helpers
    void InitializePermissions();
    bool CheckPermission(EditorPermission required) const;

    // State
    bool m_initialized = false;
    EditorState m_state = EditorState::Disabled;
    EditorState m_previousState = EditorState::Disabled;
    EditorPermission m_permission = EditorPermission::Player;

    // Engine references
    Nova::Engine* m_engine = nullptr;
    Game* m_game = nullptr;

    // Sub-editors
    std::unique_ptr<MapEditor> m_mapEditor;
    std::unique_ptr<TriggerEditor> m_triggerEditor;
    std::unique_ptr<ObjectEditor> m_objectEditor;
    std::unique_ptr<CampaignEditor> m_campaignEditor;
    std::unique_ptr<ScenarioSettings> m_scenarioSettings;
    std::unique_ptr<AIEditor> m_aiEditor;

    // UI Panels
    std::unique_ptr<TerrainPanel> m_terrainPanel;
    std::unique_ptr<ObjectPalette> m_objectPalette;
    std::unique_ptr<TriggerPanel> m_triggerPanel;
    std::unique_ptr<PropertiesPanel> m_propertiesPanel;

    // Content state
    CustomContentInfo m_contentInfo;
    std::string m_currentPath;
    bool m_hasUnsavedChanges = false;

    // Workshop state
    bool m_workshopBusy = false;
    float m_workshopProgress = 0.0f;
    std::string m_workshopError;

    // Dialog state
    bool m_showNewMapDialog = false;
    bool m_showNewCampaignDialog = false;
    bool m_showPublishDialog = false;
    bool m_showSettingsDialog = false;
    bool m_showAboutDialog = false;
    bool m_showSavePromptDialog = false;
    bool m_pendingExit = false;

    // Panel visibility
    bool m_showTerrainPanel = true;
    bool m_showObjectPalette = true;
    bool m_showTriggerPanel = true;
    bool m_showPropertiesPanel = true;
    bool m_showMinimap = true;
    bool m_showLayerPanel = true;

    // Permission actions
    std::unordered_map<std::string, EditorPermission> m_actionPermissions;

    // Developer mode key (hashed)
    static constexpr uint64_t DEVELOPER_KEY_HASH = 0x5A8F3D2E1B4C9A7F;
};

} // namespace Vehement
