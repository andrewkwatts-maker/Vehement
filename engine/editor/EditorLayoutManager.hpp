/**
 * @file EditorLayoutManager.hpp
 * @brief Panel layout management and persistence
 *
 * This class handles:
 * - Panel layout save/load
 * - Layout presets (Default, Debug, Animation, etc.)
 * - Window arrangement
 * - Docking state persistence
 *
 * @author Nova Engine Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <memory>

namespace Nova {

// Forward declarations
class EditorPanel;

// =============================================================================
// Layout Preset
// =============================================================================

/**
 * @brief Layout preset configuration
 */
struct LayoutPreset {
    std::string name;           ///< Display name
    std::string description;    ///< Optional description
    std::string iniData;        ///< ImGui docking layout data
    bool isBuiltIn = false;     ///< Cannot be deleted
    bool isDefault = false;     ///< Loaded on startup
};

// =============================================================================
// Panel State
// =============================================================================

/**
 * @brief Saved panel state
 */
struct PanelState {
    std::string name;           ///< Panel identifier
    bool visible = true;        ///< Is panel visible
    bool docked = true;         ///< Is panel docked
    float posX = 0.0f;          ///< Window position X (if floating)
    float posY = 0.0f;          ///< Window position Y (if floating)
    float width = 300.0f;       ///< Window width
    float height = 400.0f;      ///< Window height
};

// =============================================================================
// Layout Changed Event
// =============================================================================

/**
 * @brief Layout change event data
 */
struct LayoutChangedEvent {
    std::string previousLayout;
    std::string newLayout;
};

// =============================================================================
// Editor Layout Manager
// =============================================================================

/**
 * @brief Manages editor panel layouts and persistence
 *
 * Responsibilities:
 * - Layout save/load
 * - Layout presets management
 * - Window arrangement
 * - Docking state serialization
 *
 * Usage:
 *   EditorLayoutManager layoutMgr;
 *   layoutMgr.Initialize(configPath);
 *
 *   layoutMgr.SaveLayout("MyLayout");
 *   layoutMgr.LoadLayout("Default");
 */
class EditorLayoutManager {
public:
    EditorLayoutManager() = default;
    ~EditorLayoutManager() = default;

    // Non-copyable, non-movable
    EditorLayoutManager(const EditorLayoutManager&) = delete;
    EditorLayoutManager& operator=(const EditorLayoutManager&) = delete;
    EditorLayoutManager(EditorLayoutManager&&) = delete;
    EditorLayoutManager& operator=(EditorLayoutManager&&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the layout manager
     * @param configDir Directory for layout configuration files
     */
    void Initialize(const std::filesystem::path& configDir);

    /**
     * @brief Shutdown and save state
     */
    void Shutdown();

    /**
     * @brief Set config directory
     */
    void SetConfigDirectory(const std::filesystem::path& path) { m_configDir = path; }

    /**
     * @brief Get config directory
     */
    [[nodiscard]] const std::filesystem::path& GetConfigDirectory() const { return m_configDir; }

    // =========================================================================
    // Layout Management
    // =========================================================================

    /**
     * @brief Save current layout with a name
     * @param name Layout name
     * @param description Optional description
     * @return true if save succeeded
     */
    bool SaveLayout(const std::string& name, const std::string& description = "");

    /**
     * @brief Load a layout by name
     * @param name Layout name
     * @return true if load succeeded
     */
    bool LoadLayout(const std::string& name);

    /**
     * @brief Delete a layout
     * @param name Layout name
     * @return true if deletion succeeded
     */
    bool DeleteLayout(const std::string& name);

    /**
     * @brief Rename a layout
     * @param oldName Current name
     * @param newName New name
     * @return true if rename succeeded
     */
    bool RenameLayout(const std::string& oldName, const std::string& newName);

    /**
     * @brief Check if a layout exists
     * @param name Layout name
     */
    [[nodiscard]] bool HasLayout(const std::string& name) const;

    /**
     * @brief Get a layout preset
     * @param name Layout name
     * @return Pointer to preset or nullptr if not found
     */
    [[nodiscard]] const LayoutPreset* GetLayout(const std::string& name) const;

    /**
     * @brief Get all layout names
     */
    [[nodiscard]] std::vector<std::string> GetLayoutNames() const;

    /**
     * @brief Get all layout presets
     */
    [[nodiscard]] const std::unordered_map<std::string, LayoutPreset>& GetLayouts() const {
        return m_layouts;
    }

    /**
     * @brief Get current layout name
     */
    [[nodiscard]] const std::string& GetCurrentLayout() const { return m_currentLayout; }

    // =========================================================================
    // Default Layout
    // =========================================================================

    /**
     * @brief Reset to default layout
     */
    void ResetLayout();

    /**
     * @brief Set default layout name
     * @param name Layout to use as default
     */
    void SetDefaultLayout(const std::string& name);

    /**
     * @brief Get default layout name
     */
    [[nodiscard]] const std::string& GetDefaultLayout() const { return m_defaultLayout; }

    /**
     * @brief Create default layouts if not existing
     */
    void CreateDefaultLayouts();

    // =========================================================================
    // Panel State
    // =========================================================================

    /**
     * @brief Save panel state
     * @param panel Panel to save state for
     */
    void SavePanelState(const EditorPanel* panel);

    /**
     * @brief Restore panel state
     * @param panel Panel to restore state to
     * @return true if state was restored
     */
    bool RestorePanelState(EditorPanel* panel);

    /**
     * @brief Get saved panel state
     * @param panelName Panel identifier
     * @return Pointer to state or nullptr if not found
     */
    [[nodiscard]] const PanelState* GetPanelState(const std::string& panelName) const;

    /**
     * @brief Clear all saved panel states
     */
    void ClearPanelStates();

    // =========================================================================
    // Persistence
    // =========================================================================

    /**
     * @brief Load all layouts from disk
     * @return true if load succeeded
     */
    bool LoadLayouts();

    /**
     * @brief Save all layouts to disk
     * @return true if save succeeded
     */
    bool SaveLayouts();

    /**
     * @brief Export layout to file
     * @param name Layout name
     * @param path Export file path
     * @return true if export succeeded
     */
    bool ExportLayout(const std::string& name, const std::filesystem::path& path);

    /**
     * @brief Import layout from file
     * @param path Import file path
     * @param name Optional name override
     * @return true if import succeeded
     */
    bool ImportLayout(const std::filesystem::path& path, const std::string& name = "");

    // =========================================================================
    // Docking
    // =========================================================================

    /**
     * @brief Begin setting up default docking layout
     * Call this before creating any panels on first run
     */
    void BeginDefaultDocking();

    /**
     * @brief Finish default docking setup
     */
    void EndDefaultDocking();

    /**
     * @brief Get dockspace ID
     */
    [[nodiscard]] unsigned int GetDockspaceId() const { return m_dockspaceId; }

    /**
     * @brief Set dockspace ID (called by EditorApplication)
     */
    void SetDockspaceId(unsigned int id) { m_dockspaceId = id; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set layout changed callback
     */
    void SetOnLayoutChanged(std::function<void(const LayoutChangedEvent&)> callback);

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render layout selection UI
     */
    void RenderLayoutSelector();

    /**
     * @brief Render layout management window
     */
    void RenderLayoutManager();

    /**
     * @brief Show/hide layout manager window
     */
    void ShowLayoutManager(bool show) { m_showLayoutManager = show; }

    /**
     * @brief Check if layout manager window is visible
     */
    [[nodiscard]] bool IsLayoutManagerVisible() const { return m_showLayoutManager; }

private:
    // =========================================================================
    // Internal Helpers
    // =========================================================================

    std::string CaptureCurrentLayout();
    void ApplyLayout(const std::string& iniData);
    std::filesystem::path GetLayoutFilePath(const std::string& name) const;
    void NotifyLayoutChanged(const std::string& previous);

    // =========================================================================
    // Member Variables
    // =========================================================================

    std::filesystem::path m_configDir;

    // Layouts
    std::unordered_map<std::string, LayoutPreset> m_layouts;
    std::string m_currentLayout;
    std::string m_defaultLayout = "Default";

    // Panel states
    std::unordered_map<std::string, PanelState> m_panelStates;

    // Docking
    unsigned int m_dockspaceId = 0;
    bool m_needsDefaultDocking = false;

    // UI state
    bool m_showLayoutManager = false;
    char m_newLayoutName[128] = "";
    char m_layoutDescription[256] = "";

    // Callbacks
    std::function<void(const LayoutChangedEvent&)> m_onLayoutChanged;
};

} // namespace Nova
