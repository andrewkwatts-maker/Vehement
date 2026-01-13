/**
 * @file EditorSelectionManager.hpp
 * @brief Multi-selection support with callbacks and history
 *
 * This class handles:
 * - Single and multi-selection support
 * - Selection changed callbacks
 * - Selection filters by type
 * - Selection history for undo/redo
 *
 * @author Nova Engine Team
 * @version 1.0.0
 */

#pragma once

#include <vector>
#include <unordered_set>
#include <functional>
#include <deque>
#include <typeindex>
#include <memory>
#include <string>

namespace Nova {

// Forward declarations
class SceneNode;
class Scene;

// =============================================================================
// Selection Changed Event
// =============================================================================

/**
 * @brief Selection change event data
 */
struct SelectionChangedEvent {
    std::vector<SceneNode*> previousSelection;
    std::vector<SceneNode*> newSelection;
    bool fromUndo = false;  ///< True if change came from undo/redo
};

// =============================================================================
// Selection Filter
// =============================================================================

/**
 * @brief Filter type for selection operations
 */
enum class SelectionFilterType : uint8_t {
    All,            ///< Accept all node types
    Meshes,         ///< Only mesh renderers
    Lights,         ///< Only lights
    Cameras,        ///< Only cameras
    SDFPrimitives,  ///< Only SDF primitives
    Empty,          ///< Only empty nodes
    Custom          ///< Custom filter function
};

/**
 * @brief Selection filter configuration
 */
struct SelectionFilter {
    SelectionFilterType type = SelectionFilterType::All;
    std::function<bool(const SceneNode*)> customFilter;  ///< For Custom type
    bool includeChildren = true;                          ///< Include children in operations
    bool includeHidden = false;                           ///< Include hidden nodes
};

// =============================================================================
// Selection History Entry
// =============================================================================

/**
 * @brief Selection history entry for undo support
 */
struct SelectionHistoryEntry {
    std::vector<SceneNode*> selection;
    std::string description;  ///< Optional description for UI
};

// =============================================================================
// Editor Selection Manager
// =============================================================================

/**
 * @brief Manages scene object selection with history support
 *
 * Responsibilities:
 * - Single and multi-selection
 * - Selection callbacks and events
 * - Selection filters
 * - Undo/redo selection history
 *
 * Usage:
 *   EditorSelectionManager selectionMgr;
 *   selectionMgr.Initialize(scene);
 *
 *   selectionMgr.SetOnSelectionChanged([](const SelectionChangedEvent& e) {
 *       // Handle selection change
 *   });
 *
 *   selectionMgr.Select(node);
 *   selectionMgr.AddToSelection(otherNode);
 */
class EditorSelectionManager {
public:
    EditorSelectionManager() = default;
    ~EditorSelectionManager() = default;

    // Non-copyable, non-movable
    EditorSelectionManager(const EditorSelectionManager&) = delete;
    EditorSelectionManager& operator=(const EditorSelectionManager&) = delete;
    EditorSelectionManager(EditorSelectionManager&&) = delete;
    EditorSelectionManager& operator=(EditorSelectionManager&&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the selection manager
     * @param scene Active scene for selection operations
     */
    void Initialize(Scene* scene);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Set the active scene
     */
    void SetScene(Scene* scene) { m_scene = scene; }

    /**
     * @brief Get the active scene
     */
    [[nodiscard]] Scene* GetScene() const { return m_scene; }

    // =========================================================================
    // Selection Operations
    // =========================================================================

    /**
     * @brief Select a single object (clears previous selection)
     * @param node Node to select (nullptr clears selection)
     */
    void Select(SceneNode* node);

    /**
     * @brief Select multiple objects (clears previous selection)
     * @param nodes Nodes to select
     */
    void Select(const std::vector<SceneNode*>& nodes);

    /**
     * @brief Add object to current selection
     * @param node Node to add
     */
    void AddToSelection(SceneNode* node);

    /**
     * @brief Add multiple objects to selection
     * @param nodes Nodes to add
     */
    void AddToSelection(const std::vector<SceneNode*>& nodes);

    /**
     * @brief Remove object from selection
     * @param node Node to remove
     */
    void RemoveFromSelection(SceneNode* node);

    /**
     * @brief Toggle object selection state
     * @param node Node to toggle
     */
    void ToggleSelection(SceneNode* node);

    /**
     * @brief Clear current selection
     */
    void ClearSelection();

    /**
     * @brief Select all objects in scene
     */
    void SelectAll();

    /**
     * @brief Select all objects matching filter
     * @param filter Selection filter
     */
    void SelectAll(const SelectionFilter& filter);

    /**
     * @brief Invert current selection
     */
    void InvertSelection();

    /**
     * @brief Select parent of currently selected objects
     */
    void SelectParent();

    /**
     * @brief Select children of currently selected objects
     * @param recursive Include all descendants
     */
    void SelectChildren(bool recursive = false);

    /**
     * @brief Select siblings of currently selected objects
     */
    void SelectSiblings();

    // =========================================================================
    // Selection Queries
    // =========================================================================

    /**
     * @brief Check if a node is selected
     * @param node Node to check
     * @return true if node is in selection
     */
    [[nodiscard]] bool IsSelected(const SceneNode* node) const;

    /**
     * @brief Get current selection
     */
    [[nodiscard]] const std::vector<SceneNode*>& GetSelection() const { return m_selection; }

    /**
     * @brief Get selection count
     */
    [[nodiscard]] size_t GetSelectionCount() const { return m_selection.size(); }

    /**
     * @brief Check if selection is empty
     */
    [[nodiscard]] bool IsEmpty() const { return m_selection.empty(); }

    /**
     * @brief Check if multiple objects are selected
     */
    [[nodiscard]] bool HasMultipleSelection() const { return m_selection.size() > 1; }

    /**
     * @brief Get primary (last) selected object
     * @return Primary selection or nullptr if empty
     */
    [[nodiscard]] SceneNode* GetPrimarySelection() const;

    /**
     * @brief Get first selected object
     * @return First selection or nullptr if empty
     */
    [[nodiscard]] SceneNode* GetFirstSelection() const;

    /**
     * @brief Get selection filtered by type
     * @param filter Filter to apply
     * @return Filtered selection
     */
    [[nodiscard]] std::vector<SceneNode*> GetFilteredSelection(const SelectionFilter& filter) const;

    // =========================================================================
    // Selection History (Undo/Redo)
    // =========================================================================

    /**
     * @brief Undo to previous selection
     * @return true if undo was performed
     */
    bool UndoSelection();

    /**
     * @brief Redo to next selection
     * @return true if redo was performed
     */
    bool RedoSelection();

    /**
     * @brief Check if selection undo is available
     */
    [[nodiscard]] bool CanUndoSelection() const;

    /**
     * @brief Check if selection redo is available
     */
    [[nodiscard]] bool CanRedoSelection() const;

    /**
     * @brief Clear selection history
     */
    void ClearHistory();

    /**
     * @brief Set maximum history size
     */
    void SetMaxHistorySize(size_t maxSize) { m_maxHistorySize = maxSize; }

    /**
     * @brief Enable/disable history recording
     */
    void SetHistoryEnabled(bool enabled) { m_historyEnabled = enabled; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set selection changed callback
     * @param callback Function called when selection changes
     */
    void SetOnSelectionChanged(std::function<void(const SelectionChangedEvent&)> callback);

    /**
     * @brief Add selection changed listener
     * @param callback Listener callback
     * @return Listener ID for removal
     */
    size_t AddSelectionListener(std::function<void(const SelectionChangedEvent&)> callback);

    /**
     * @brief Remove selection listener
     * @param listenerId ID returned by AddSelectionListener
     */
    void RemoveSelectionListener(size_t listenerId);

    // =========================================================================
    // Filter Management
    // =========================================================================

    /**
     * @brief Set active selection filter
     */
    void SetFilter(const SelectionFilter& filter) { m_activeFilter = filter; }

    /**
     * @brief Get active selection filter
     */
    [[nodiscard]] const SelectionFilter& GetFilter() const { return m_activeFilter; }

    /**
     * @brief Clear active filter (accept all)
     */
    void ClearFilter();

    /**
     * @brief Check if node passes current filter
     */
    [[nodiscard]] bool PassesFilter(const SceneNode* node) const;

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Get bounding box center of selection
     * @param outCenter Output center position
     * @return true if selection has valid bounds
     */
    bool GetSelectionCenter(float* outCenter) const;

    /**
     * @brief Get combined bounding box of selection
     * @param outMin Output minimum corner
     * @param outMax Output maximum corner
     * @return true if selection has valid bounds
     */
    bool GetSelectionBounds(float* outMin, float* outMax) const;

    /**
     * @brief Clean up references to deleted nodes
     * @param deletedNode Node that was deleted
     */
    void OnNodeDeleted(SceneNode* deletedNode);

private:
    // =========================================================================
    // Internal Helpers
    // =========================================================================

    void NotifySelectionChanged(const std::vector<SceneNode*>& previous, bool fromUndo = false);
    void PushToHistory();
    void CollectSceneNodes(SceneNode* node, std::vector<SceneNode*>& outNodes) const;
    bool PassesFilterInternal(const SceneNode* node, const SelectionFilter& filter) const;

    // =========================================================================
    // Member Variables
    // =========================================================================

    Scene* m_scene = nullptr;

    // Current selection
    std::vector<SceneNode*> m_selection;
    std::unordered_set<const SceneNode*> m_selectionSet;

    // Selection history
    std::deque<SelectionHistoryEntry> m_undoHistory;
    std::deque<SelectionHistoryEntry> m_redoHistory;
    size_t m_maxHistorySize = 50;
    bool m_historyEnabled = true;

    // Callbacks
    std::function<void(const SelectionChangedEvent&)> m_onSelectionChanged;
    std::unordered_map<size_t, std::function<void(const SelectionChangedEvent&)>> m_listeners;
    size_t m_nextListenerId = 1;

    // Active filter
    SelectionFilter m_activeFilter;
};

} // namespace Nova
