/**
 * @file EditorMenuSystem.hpp
 * @brief Menu bar rendering and keyboard shortcut management
 *
 * This class handles:
 * - Menu bar rendering
 * - Menu item registration and callbacks
 * - Keyboard shortcut handling for menus
 * - Recent files list management
 *
 * @author Nova Engine Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <chrono>

namespace Nova {

// Forward declarations
class EditorApplication;
class Scene;
class SceneNode;

// =============================================================================
// Recent Project Entry
// =============================================================================

/**
 * @brief Recent project entry for menu display
 */
struct RecentProject {
    std::string path;
    std::string name;
    std::chrono::system_clock::time_point lastOpened;
    bool exists = true;
};

// =============================================================================
// Shortcut Binding
// =============================================================================

/**
 * @brief Keyboard shortcut binding
 */
struct ShortcutBinding {
    std::string action;                 ///< Action name (e.g., "Undo", "Save")
    std::string shortcutString;         ///< Display string (e.g., "Ctrl+Z")
    int key = 0;                        ///< Key code
    int modifiers = 0;                  ///< Modifier flags (Ctrl=1, Shift=2, Alt=4)
    std::function<void()> handler;      ///< Action handler
    bool global = false;                ///< Execute even when text input is focused
};

// =============================================================================
// Menu Item Types
// =============================================================================

/**
 * @brief Menu item type
 */
enum class MenuItemType : uint8_t {
    Action,         ///< Simple click action
    Checkbox,       ///< Toggle checkbox
    Submenu,        ///< Opens a submenu
    Separator       ///< Visual separator
};

/**
 * @brief Menu item configuration
 */
struct MenuItem {
    std::string id;                     ///< Unique identifier
    std::string label;                  ///< Display label
    std::string shortcut;               ///< Shortcut hint (display only)
    MenuItemType type = MenuItemType::Action;
    bool enabled = true;
    bool checked = false;               ///< For checkbox type
    std::function<void()> action;       ///< Action callback
    std::function<bool()> enabledCallback; ///< Dynamic enabled state
    std::function<bool()> checkedCallback; ///< Dynamic checked state (for checkbox)
    std::vector<MenuItem> submenuItems; ///< For submenu type
};

// =============================================================================
// Editor Menu System
// =============================================================================

/**
 * @brief Manages editor menu bar and keyboard shortcuts
 *
 * Responsibilities:
 * - Menu bar rendering
 * - Menu item registration and dynamic state
 * - Keyboard shortcut handling
 * - Recent files list management
 *
 * Usage:
 *   EditorMenuSystem menuSystem;
 *   menuSystem.Initialize(editorApp);
 *   menuSystem.RegisterShortcut("Save", "Ctrl+S", []() { ... });
 *
 *   // In update loop
 *   menuSystem.ProcessShortcuts();
 *
 *   // In render loop
 *   menuSystem.RenderMenuBar();
 */
class EditorMenuSystem {
public:
    EditorMenuSystem() = default;
    ~EditorMenuSystem() = default;

    // Non-copyable, non-movable
    EditorMenuSystem(const EditorMenuSystem&) = delete;
    EditorMenuSystem& operator=(const EditorMenuSystem&) = delete;
    EditorMenuSystem(EditorMenuSystem&&) = delete;
    EditorMenuSystem& operator=(EditorMenuSystem&&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the menu system
     * @param editor Reference to parent editor application
     */
    void Initialize(EditorApplication* editor);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    // =========================================================================
    // Menu Registration
    // =========================================================================

    /**
     * @brief Register a custom menu
     * @param menuName Name of the menu (e.g., "File", "Edit")
     * @param items Menu items to add
     */
    void RegisterMenu(const std::string& menuName, const std::vector<MenuItem>& items);

    /**
     * @brief Add item to existing menu
     * @param menuName Target menu name
     * @param item Item to add
     * @param insertBefore Optional: insert before this item ID
     */
    void AddMenuItem(const std::string& menuName, const MenuItem& item,
                     const std::string& insertBefore = "");

    /**
     * @brief Remove item from menu
     * @param menuName Target menu name
     * @param itemId Item ID to remove
     */
    void RemoveMenuItem(const std::string& menuName, const std::string& itemId);

    /**
     * @brief Enable/disable menu item
     * @param menuName Menu name
     * @param itemId Item ID
     * @param enabled Enable state
     */
    void SetMenuItemEnabled(const std::string& menuName, const std::string& itemId, bool enabled);

    // =========================================================================
    // Shortcut Management
    // =========================================================================

    /**
     * @brief Register a keyboard shortcut
     * @param action Unique action name
     * @param shortcut Shortcut string (e.g., "Ctrl+S", "Alt+Shift+N")
     * @param handler Callback when shortcut is triggered
     * @param global If true, works even during text input
     */
    void RegisterShortcut(const std::string& action, const std::string& shortcut,
                          std::function<void()> handler, bool global = false);

    /**
     * @brief Unregister a shortcut
     * @param action Action name to remove
     */
    void UnregisterShortcut(const std::string& action);

    /**
     * @brief Rebind a shortcut to a new key combination
     * @param action Action name
     * @param newShortcut New shortcut string
     * @return true if rebind succeeded
     */
    bool RebindShortcut(const std::string& action, const std::string& newShortcut);

    /**
     * @brief Get shortcut string for action
     * @param action Action name
     * @return Shortcut string or empty if not found
     */
    [[nodiscard]] std::string GetShortcutForAction(const std::string& action) const;

    /**
     * @brief Check if shortcut is currently pressed
     * @param action Action name
     * @return true if shortcut is active this frame
     */
    [[nodiscard]] bool IsShortcutPressed(const std::string& action) const;

    /**
     * @brief Get all registered shortcuts
     */
    [[nodiscard]] const std::unordered_map<std::string, ShortcutBinding>& GetShortcuts() const {
        return m_shortcuts;
    }

    /**
     * @brief Process all keyboard shortcuts (call once per frame)
     */
    void ProcessShortcuts();

    // =========================================================================
    // Recent Files
    // =========================================================================

    /**
     * @brief Add file to recent list
     * @param path File path
     */
    void AddRecentFile(const std::filesystem::path& path);

    /**
     * @brief Get recent files list
     */
    [[nodiscard]] const std::vector<RecentProject>& GetRecentFiles() const { return m_recentFiles; }

    /**
     * @brief Clear recent files list
     */
    void ClearRecentFiles();

    /**
     * @brief Load recent files from disk
     * @return true if load succeeded
     */
    bool LoadRecentFiles();

    /**
     * @brief Save recent files to disk
     * @return true if save succeeded
     */
    bool SaveRecentFiles();

    /**
     * @brief Set maximum recent files count
     */
    void SetMaxRecentFiles(size_t max) { m_maxRecentFiles = max; }

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render the main menu bar
     */
    void RenderMenuBar();

    /**
     * @brief Set callback for file menu operations
     */
    void SetFileMenuCallbacks(
        std::function<void()> onNew,
        std::function<void()> onOpen,
        std::function<void()> onSave,
        std::function<void()> onSaveAs,
        std::function<void(const std::filesystem::path&)> onOpenRecent,
        std::function<void()> onPreferences,
        std::function<void()> onExit
    );

    /**
     * @brief Set callback for edit menu operations
     */
    void SetEditMenuCallbacks(
        std::function<void()> onUndo,
        std::function<void()> onRedo,
        std::function<bool()> canUndo,
        std::function<bool()> canRedo,
        std::function<void()> onDelete,
        std::function<void()> onDuplicate,
        std::function<void()> onSelectAll,
        std::function<void()> onDeselectAll,
        std::function<void()> onInvertSelection,
        std::function<bool()> hasSelection
    );

private:
    // =========================================================================
    // Internal Rendering
    // =========================================================================

    void RenderFileMenu();
    void RenderEditMenu();
    void RenderViewMenu();
    void RenderGameObjectMenu();
    void RenderComponentMenu();
    void RenderWindowMenu();
    void RenderHelpMenu();
    void RenderCustomMenus();
    void RenderMenuItem(const MenuItem& item);

    // =========================================================================
    // Shortcut Helpers
    // =========================================================================

    bool ParseShortcut(const std::string& shortcut, int& key, int& modifiers) const;
    bool IsShortcutActive(int key, int modifiers) const;
    void HandleGlobalShortcuts();

    // =========================================================================
    // Member Variables
    // =========================================================================

    EditorApplication* m_editor = nullptr;

    // Shortcuts
    std::unordered_map<std::string, ShortcutBinding> m_shortcuts;

    // Custom menus
    std::unordered_map<std::string, std::vector<MenuItem>> m_customMenus;

    // Recent files
    std::vector<RecentProject> m_recentFiles;
    size_t m_maxRecentFiles = 10;

    // Callbacks for file menu
    std::function<void()> m_onNew;
    std::function<void()> m_onOpen;
    std::function<void()> m_onSave;
    std::function<void()> m_onSaveAs;
    std::function<void(const std::filesystem::path&)> m_onOpenRecent;
    std::function<void()> m_onPreferences;
    std::function<void()> m_onExit;

    // Callbacks for edit menu
    std::function<void()> m_onUndo;
    std::function<void()> m_onRedo;
    std::function<bool()> m_canUndo;
    std::function<bool()> m_canRedo;
    std::function<void()> m_onDelete;
    std::function<void()> m_onDuplicate;
    std::function<void()> m_onSelectAll;
    std::function<void()> m_onDeselectAll;
    std::function<void()> m_onInvertSelection;
    std::function<bool()> m_hasSelection;

    // State flags
    bool m_sceneDirty = false;
};

} // namespace Nova
