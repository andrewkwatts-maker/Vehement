/**
 * @file SceneOutliner.hpp
 * @brief Hierarchical scene outliner panel for the Nova3D/Vehement editor
 *
 * Provides a tree view of all scene objects with:
 * - Drag-and-drop reparenting
 * - Multi-selection (Ctrl/Shift)
 * - Search/filter by name or type
 * - Right-click context menu
 * - Object type icons
 * - Visibility/lock toggles
 * - Grouping support
 */

#pragma once

#include "../scene/Scene.hpp"
#include "../scene/SceneNode.hpp"
#include "../ui/EditorPanel.hpp"
#include "../ui/EditorWidgets.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <memory>

namespace Nova {

// Forward declarations
class Scene;
class SceneNode;
class CommandHistory;

// =============================================================================
// Scene Object Types
// =============================================================================

/**
 * @brief Types of objects that can appear in the scene hierarchy
 */
enum class SceneObjectType {
    Unknown,
    Empty,          ///< Empty node (group/transform only)
    Mesh,           ///< Polygon mesh
    SDFPrimitive,   ///< SDF primitive
    SDFModel,       ///< SDF model (composed of primitives)
    Light,          ///< Light source
    Camera,         ///< Camera
    Group,          ///< Group/folder node
    Prefab,         ///< Prefab instance
    Terrain,        ///< Terrain object
    Particle,       ///< Particle system
    Audio,          ///< Audio source
    Trigger,        ///< Trigger volume
    Spline          ///< Spline/path
};

/**
 * @brief Get icon string for object type
 */
const char* GetObjectTypeIcon(SceneObjectType type);

/**
 * @brief Get display name for object type
 */
const char* GetObjectTypeName(SceneObjectType type);

// =============================================================================
// Tree Node for Scene Outliner
// =============================================================================

/**
 * @brief Extended tree node with scene-specific data
 */
struct OutlinerTreeNode {
    SceneNode* sceneNode = nullptr;     ///< Pointer to actual scene node
    std::string name;                   ///< Display name
    std::string id;                     ///< Unique identifier
    SceneObjectType objectType = SceneObjectType::Unknown;

    // Hierarchy
    OutlinerTreeNode* parent = nullptr;
    std::vector<std::unique_ptr<OutlinerTreeNode>> children;
    int depth = 0;
    size_t siblingIndex = 0;

    // UI State
    bool expanded = false;
    bool selected = false;
    bool visible = true;            ///< Node's own visibility flag
    bool locked = false;            ///< Cannot be selected/modified
    bool highlighted = false;       ///< Temporary highlight (e.g., search result)
    bool isRenaming = false;        ///< Currently being renamed
    bool matchesFilter = true;      ///< Matches current search filter

    // Cached display
    std::string displayLabel;       ///< Cached formatted label
    glm::vec4 labelColor{1.0f};     ///< Label color

    /**
     * @brief Check if node is visible in hierarchy (all ancestors visible and expanded)
     */
    [[nodiscard]] bool IsVisibleInTree() const;

    /**
     * @brief Check if any ancestor is collapsed
     */
    [[nodiscard]] bool HasCollapsedAncestor() const;

    /**
     * @brief Get the root node
     */
    [[nodiscard]] OutlinerTreeNode* GetRoot();
    [[nodiscard]] const OutlinerTreeNode* GetRoot() const;

    /**
     * @brief Find child by name
     */
    [[nodiscard]] OutlinerTreeNode* FindChild(const std::string& name, bool recursive = true);

    /**
     * @brief Count total descendants
     */
    [[nodiscard]] size_t CountDescendants() const;
};

// =============================================================================
// Selection State
// =============================================================================

/**
 * @brief Manages multi-selection state for outliner
 */
class OutlinerSelection {
public:
    /**
     * @brief Clear all selections
     */
    void Clear();

    /**
     * @brief Select a single node (clears previous selection)
     */
    void Select(OutlinerTreeNode* node);

    /**
     * @brief Add node to selection
     */
    void Add(OutlinerTreeNode* node);

    /**
     * @brief Remove node from selection
     */
    void Remove(OutlinerTreeNode* node);

    /**
     * @brief Toggle node selection
     */
    void Toggle(OutlinerTreeNode* node);

    /**
     * @brief Check if node is selected
     */
    [[nodiscard]] bool IsSelected(const OutlinerTreeNode* node) const;

    /**
     * @brief Select range of nodes (Shift+click)
     */
    void SelectRange(OutlinerTreeNode* from, OutlinerTreeNode* to, OutlinerTreeNode* root);

    /**
     * @brief Get all selected nodes
     */
    [[nodiscard]] const std::vector<OutlinerTreeNode*>& GetSelection() const { return m_selected; }

    /**
     * @brief Get primary (last) selected node
     */
    [[nodiscard]] OutlinerTreeNode* GetPrimary() const;

    /**
     * @brief Get number of selected nodes
     */
    [[nodiscard]] size_t Count() const { return m_selected.size(); }

    /**
     * @brief Check if selection is empty
     */
    [[nodiscard]] bool IsEmpty() const { return m_selected.empty(); }

    /**
     * @brief Get selected scene nodes
     */
    [[nodiscard]] std::vector<SceneNode*> GetSelectedSceneNodes() const;

private:
    std::vector<OutlinerTreeNode*> m_selected;
    std::unordered_set<const OutlinerTreeNode*> m_selectedSet;  // For fast lookup
    OutlinerTreeNode* m_anchor = nullptr;  // For range selection
};

// =============================================================================
// Drag and Drop
// =============================================================================

/**
 * @brief Drag-drop payload for scene nodes
 */
struct OutlinerDragPayload {
    std::vector<OutlinerTreeNode*> nodes;
    bool isValid = false;
};

/**
 * @brief Drop location indicator
 */
enum class DropLocation {
    None,
    Before,     ///< Insert before target node
    After,      ///< Insert after target node
    Inside      ///< Reparent as child of target
};

/**
 * @brief Drop target info
 */
struct DropTarget {
    OutlinerTreeNode* node = nullptr;
    DropLocation location = DropLocation::None;
    float insertLineY = 0.0f;
};

// =============================================================================
// Callbacks
// =============================================================================

/**
 * @brief Callback signatures for outliner events
 */
struct OutlinerCallbacks {
    /// Called when selection changes
    std::function<void(const std::vector<SceneNode*>&)> onSelectionChanged;

    /// Called when a node is double-clicked
    std::function<void(SceneNode*)> onNodeDoubleClicked;

    /// Called when a node is reparented via drag-drop
    std::function<void(SceneNode* node, SceneNode* newParent, size_t newIndex)> onNodeReparented;

    /// Called when a node is deleted
    std::function<void(const std::vector<SceneNode*>&)> onNodesDeleted;

    /// Called when a node is duplicated
    std::function<void(const std::vector<SceneNode*>&)> onNodesDuplicated;

    /// Called when a node is renamed
    std::function<void(SceneNode*, const std::string& oldName, const std::string& newName)> onNodeRenamed;

    /// Called when visibility is toggled
    std::function<void(SceneNode*, bool visible)> onVisibilityChanged;

    /// Called when lock state is toggled
    std::function<void(SceneNode*, bool locked)> onLockChanged;

    /// Called when a group is created
    std::function<void(SceneNode* group, const std::vector<SceneNode*>& members)> onGroupCreated;

    /// Called when focus on node is requested
    std::function<void(SceneNode*)> onFocusRequested;
};

// =============================================================================
// Scene Outliner Panel
// =============================================================================

/**
 * @brief Hierarchical scene outliner panel
 *
 * Features:
 * - Tree view of all scene objects
 * - Drag-and-drop reparenting
 * - Multi-selection with Ctrl/Shift
 * - Search/filter by name or type
 * - Right-click context menu (delete, duplicate, rename)
 * - Icons per object type (mesh, light, camera, SDF)
 * - Visibility toggles
 * - Lock/unlock objects
 * - Grouping support
 *
 * Usage:
 *   SceneOutliner outliner;
 *   outliner.SetScene(&myScene);
 *   outliner.Callbacks.onSelectionChanged = [](const auto& nodes) {
 *       // Handle selection change
 *   };
 *   // In render loop:
 *   outliner.Render();
 */
class SceneOutliner : public EditorPanel {
public:
    SceneOutliner();
    ~SceneOutliner() override;

    // Non-copyable
    SceneOutliner(const SceneOutliner&) = delete;
    SceneOutliner& operator=(const SceneOutliner&) = delete;

    // =========================================================================
    // Scene Management
    // =========================================================================

    /**
     * @brief Set the scene to display
     */
    void SetScene(Scene* scene);

    /**
     * @brief Get the current scene
     */
    [[nodiscard]] Scene* GetScene() const { return m_scene; }

    /**
     * @brief Force refresh of the tree structure
     */
    void Refresh();

    /**
     * @brief Mark tree as needing rebuild
     */
    void Invalidate() { m_needsRebuild = true; }

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * @brief Get currently selected scene nodes
     */
    [[nodiscard]] std::vector<SceneNode*> GetSelected() const;

    /**
     * @brief Select a specific node
     */
    void Select(SceneNode* node, bool addToSelection = false);

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Select all visible nodes
     */
    void SelectAll();

    /**
     * @brief Invert selection
     */
    void InvertSelection();

    /**
     * @brief Select children of current selection
     */
    void SelectChildren();

    /**
     * @brief Select parent of current selection
     */
    void SelectParent();

    // =========================================================================
    // Navigation
    // =========================================================================

    /**
     * @brief Expand all tree nodes
     */
    void ExpandAll();

    /**
     * @brief Collapse all tree nodes
     */
    void CollapseAll();

    /**
     * @brief Expand to show a specific node
     */
    void RevealNode(SceneNode* node);

    /**
     * @brief Scroll to show a specific node
     */
    void ScrollToNode(SceneNode* node);

    /**
     * @brief Focus on selected node (expand and scroll)
     */
    void FocusSelection();

    // =========================================================================
    // Operations
    // =========================================================================

    /**
     * @brief Delete selected nodes
     */
    void DeleteSelected();

    /**
     * @brief Duplicate selected nodes
     */
    void DuplicateSelected();

    /**
     * @brief Start renaming the selected node
     */
    void RenameSelected();

    /**
     * @brief Group selected nodes under a new parent
     */
    void GroupSelected();

    /**
     * @brief Ungroup selected group nodes
     */
    void UngroupSelected();

    /**
     * @brief Toggle visibility of selected nodes
     */
    void ToggleVisibilitySelected();

    /**
     * @brief Toggle lock state of selected nodes
     */
    void ToggleLockSelected();

    /**
     * @brief Create a new empty node
     */
    SceneNode* CreateEmptyNode(SceneNode* parent = nullptr);

    // =========================================================================
    // Filtering
    // =========================================================================

    /**
     * @brief Set search filter
     */
    void SetFilter(const std::string& filter);

    /**
     * @brief Get current filter
     */
    [[nodiscard]] const std::string& GetFilter() const { return m_filterText; }

    /**
     * @brief Clear search filter
     */
    void ClearFilter();

    /**
     * @brief Set type filter
     */
    void SetTypeFilter(SceneObjectType type, bool enabled);

    /**
     * @brief Clear all type filters
     */
    void ClearTypeFilters();

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Show/hide visibility toggles
     */
    void SetShowVisibilityToggles(bool show) { m_showVisibilityToggles = show; }

    /**
     * @brief Show/hide lock toggles
     */
    void SetShowLockToggles(bool show) { m_showLockToggles = show; }

    /**
     * @brief Show/hide object type icons
     */
    void SetShowTypeIcons(bool show) { m_showTypeIcons = show; }

    /**
     * @brief Enable/disable drag-drop reparenting
     */
    void SetEnableDragDrop(bool enable) { m_enableDragDrop = enable; }

    /**
     * @brief Set whether to show hidden nodes (visibility=false)
     */
    void SetShowHiddenNodes(bool show) { m_showHiddenNodes = show; }

    /**
     * @brief Set command history for undo/redo
     */
    void SetCommandHistory(CommandHistory* history) { m_commandHistory = history; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    OutlinerCallbacks Callbacks;

protected:
    // =========================================================================
    // EditorPanel Overrides
    // =========================================================================

    void OnRender() override;
    void OnRenderToolbar() override;
    void OnRenderMenuBar() override;
    void OnSearchChanged(const std::string& filter) override;
    void OnInitialize() override;
    void OnShutdown() override;

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    // Tree building
    void RebuildTree();
    void BuildTreeRecursive(SceneNode* sceneNode, OutlinerTreeNode* parent, int depth);
    void UpdateTreeNodeFromScene(OutlinerTreeNode* node);
    OutlinerTreeNode* FindTreeNode(SceneNode* sceneNode);
    OutlinerTreeNode* FindTreeNodeRecursive(OutlinerTreeNode* node, SceneNode* target);

    // Rendering
    void RenderTree();
    void RenderTreeNode(OutlinerTreeNode* node);
    void RenderNodeRow(OutlinerTreeNode* node, float rowY);
    void RenderNodeIcon(OutlinerTreeNode* node);
    void RenderNodeLabel(OutlinerTreeNode* node);
    void RenderVisibilityToggle(OutlinerTreeNode* node);
    void RenderLockToggle(OutlinerTreeNode* node);
    void RenderDropIndicator(const DropTarget& target);
    void RenderContextMenu();
    void RenderRenamePopup();

    // Filtering
    void ApplyFilter();
    bool MatchesFilter(const OutlinerTreeNode* node) const;
    void ExpandMatchingNodes(OutlinerTreeNode* node);

    // Interaction
    void HandleNodeClick(OutlinerTreeNode* node, bool ctrlHeld, bool shiftHeld);
    void HandleNodeDoubleClick(OutlinerTreeNode* node);
    void HandleKeyboardInput();
    bool HandleDragSource(OutlinerTreeNode* node);
    bool HandleDropTarget(OutlinerTreeNode* node, DropTarget& outTarget);
    void ExecuteDrop(const OutlinerDragPayload& payload, const DropTarget& target);

    // Utility
    SceneObjectType DetectObjectType(SceneNode* node) const;
    void CollectVisibleNodes(OutlinerTreeNode* node, std::vector<OutlinerTreeNode*>& outNodes);
    size_t GetFlatIndex(OutlinerTreeNode* node) const;
    OutlinerTreeNode* GetNodeAtFlatIndex(size_t index) const;
    void NotifySelectionChanged();

    // Undo/Redo helpers
    void RecordReparentCommand(SceneNode* node, SceneNode* oldParent, SceneNode* newParent,
                                size_t oldIndex, size_t newIndex);
    void RecordDeleteCommand(const std::vector<SceneNode*>& nodes);
    void RecordRenameCommand(SceneNode* node, const std::string& oldName, const std::string& newName);

    // =========================================================================
    // Member Variables
    // =========================================================================

    // Scene reference
    Scene* m_scene = nullptr;

    // Tree structure
    std::unique_ptr<OutlinerTreeNode> m_rootNode;
    std::unordered_map<SceneNode*, OutlinerTreeNode*> m_nodeMap;  // Fast scene->tree lookup
    bool m_needsRebuild = true;

    // Selection
    OutlinerSelection m_selection;
    OutlinerTreeNode* m_lastClickedNode = nullptr;  // For range selection

    // Filtering
    std::string m_filterText;
    char m_filterBuffer[256] = "";
    std::unordered_set<SceneObjectType> m_typeFilters;
    bool m_hasActiveFilter = false;

    // Drag and drop
    bool m_enableDragDrop = true;
    bool m_isDragging = false;
    OutlinerDragPayload m_dragPayload;
    DropTarget m_currentDropTarget;

    // Renaming
    bool m_isRenaming = false;
    OutlinerTreeNode* m_renamingNode = nullptr;
    char m_renameBuffer[256] = "";
    bool m_renameNeedsFocus = false;

    // UI State
    bool m_showVisibilityToggles = true;
    bool m_showLockToggles = true;
    bool m_showTypeIcons = true;
    bool m_showHiddenNodes = false;
    float m_indentWidth = 18.0f;
    float m_rowHeight = 22.0f;
    float m_iconWidth = 18.0f;
    float m_toggleWidth = 18.0f;
    OutlinerTreeNode* m_scrollToNode = nullptr;

    // Context menu
    bool m_showContextMenu = false;
    OutlinerTreeNode* m_contextMenuNode = nullptr;

    // Keyboard navigation
    OutlinerTreeNode* m_focusedNode = nullptr;
    bool m_navigatedThisFrame = false;

    // Command history
    CommandHistory* m_commandHistory = nullptr;

    // Cached flat list for keyboard navigation
    mutable std::vector<OutlinerTreeNode*> m_flatList;
    mutable bool m_flatListDirty = true;
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Sort nodes by hierarchy order
 */
void SortByHierarchyOrder(std::vector<OutlinerTreeNode*>& nodes);

/**
 * @brief Check if node A is ancestor of node B
 */
bool IsAncestorOf(const OutlinerTreeNode* ancestor, const OutlinerTreeNode* descendant);

/**
 * @brief Find common ancestor of multiple nodes
 */
OutlinerTreeNode* FindCommonAncestor(const std::vector<OutlinerTreeNode*>& nodes);

} // namespace Nova
