#pragma once

#include "EditorPanel.hpp"
#include "EditorTheme.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <optional>

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================

class DockSpace;
class DockNode;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Position for docking a panel
 */
enum class DockPosition {
    Left,       // Dock to the left side
    Right,      // Dock to the right side
    Top,        // Dock to the top
    Bottom,     // Dock to the bottom
    Center,     // Dock in center (tabbed)
    Floating    // Floating window
};

/**
 * @brief Split direction for dock nodes
 */
enum class SplitDirection {
    None,       // Leaf node (contains panel)
    Horizontal, // Split horizontally (left/right children)
    Vertical    // Split vertically (top/bottom children)
};

/**
 * @brief Drag state for dock operations
 */
enum class DockDragState {
    None,           // No drag in progress
    Dragging,       // Currently dragging
    PreviewLeft,    // Hovering over left drop zone
    PreviewRight,   // Hovering over right drop zone
    PreviewTop,     // Hovering over top drop zone
    PreviewBottom,  // Hovering over bottom drop zone
    PreviewCenter,  // Hovering over center (tab) zone
    PreviewFloating // Will become floating
};

// ============================================================================
// Rectangle Helper
// ============================================================================

/**
 * @brief Simple rectangle structure for dock bounds
 */
struct DockRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    DockRect() = default;
    DockRect(float x_, float y_, float w, float h) : x(x_), y(y_), width(w), height(h) {}
    DockRect(const glm::vec2& pos, const glm::vec2& size) : x(pos.x), y(pos.y), width(size.x), height(size.y) {}

    glm::vec2 GetPos() const { return glm::vec2(x, y); }
    glm::vec2 GetSize() const { return glm::vec2(width, height); }
    glm::vec2 GetCenter() const { return glm::vec2(x + width * 0.5f, y + height * 0.5f); }
    glm::vec2 GetMin() const { return glm::vec2(x, y); }
    glm::vec2 GetMax() const { return glm::vec2(x + width, y + height); }

    bool Contains(const glm::vec2& point) const {
        return point.x >= x && point.x < x + width &&
               point.y >= y && point.y < y + height;
    }

    bool Contains(float px, float py) const {
        return Contains(glm::vec2(px, py));
    }

    bool Intersects(const DockRect& other) const {
        return x < other.x + other.width && x + width > other.x &&
               y < other.y + other.height && y + height > other.y;
    }

    DockRect Shrink(float amount) const {
        return DockRect(x + amount, y + amount,
                       std::max(0.0f, width - amount * 2),
                       std::max(0.0f, height - amount * 2));
    }

    DockRect Expand(float amount) const {
        return DockRect(x - amount, y - amount, width + amount * 2, height + amount * 2);
    }

    // Split operations
    DockRect GetLeftHalf(float ratio = 0.5f) const {
        return DockRect(x, y, width * ratio, height);
    }

    DockRect GetRightHalf(float ratio = 0.5f) const {
        float leftWidth = width * ratio;
        return DockRect(x + leftWidth, y, width - leftWidth, height);
    }

    DockRect GetTopHalf(float ratio = 0.5f) const {
        return DockRect(x, y, width, height * ratio);
    }

    DockRect GetBottomHalf(float ratio = 0.5f) const {
        float topHeight = height * ratio;
        return DockRect(x, y + topHeight, width, height - topHeight);
    }
};

// ============================================================================
// DockNode
// ============================================================================

/**
 * @brief Node in the docking tree hierarchy
 *
 * DockNodes form a binary tree structure where:
 * - Split nodes have two children (first/second)
 * - Leaf nodes contain panels (possibly multiple in tabs)
 */
class DockNode {
public:
    using Ptr = std::shared_ptr<DockNode>;
    using WeakPtr = std::weak_ptr<DockNode>;

    /**
     * @brief Unique identifier for this node
     */
    uint64_t id = 0;

    /**
     * @brief Bounds of this node in screen space
     */
    DockRect bounds;

    /**
     * @brief Split direction (None for leaf nodes)
     */
    SplitDirection splitDirection = SplitDirection::None;

    /**
     * @brief Split ratio (0.0 - 1.0) for split nodes
     */
    float splitRatio = 0.5f;

    /**
     * @brief First child (left or top depending on split direction)
     */
    Ptr firstChild;

    /**
     * @brief Second child (right or bottom depending on split direction)
     */
    Ptr secondChild;

    /**
     * @brief Parent node (null for root)
     */
    WeakPtr parent;

    /**
     * @brief Panels contained in this node (for leaf nodes)
     * Multiple panels are displayed as tabs
     */
    std::vector<EditorPanel*> panels;

    /**
     * @brief Index of the currently active tab (for tabbed panels)
     */
    int activeTabIndex = 0;

    /**
     * @brief Whether this node is floating
     */
    bool isFloating = false;

    /**
     * @brief Floating window position (only valid if isFloating)
     */
    glm::vec2 floatingPos{100, 100};

    /**
     * @brief Floating window size (only valid if isFloating)
     */
    glm::vec2 floatingSize{400, 300};

    /**
     * @brief User-defined name for this node
     */
    std::string name;

    // Constructor
    DockNode();
    explicit DockNode(uint64_t nodeId);

    // Node queries
    bool IsLeaf() const { return splitDirection == SplitDirection::None; }
    bool IsSplit() const { return splitDirection != SplitDirection::None; }
    bool IsEmpty() const { return IsLeaf() && panels.empty(); }
    bool HasPanels() const { return !panels.empty(); }
    int GetPanelCount() const { return static_cast<int>(panels.size()); }

    /**
     * @brief Get the active panel (for leaf nodes with tabs)
     */
    EditorPanel* GetActivePanel() const;

    /**
     * @brief Set the active panel by index
     */
    void SetActivePanel(int index);

    /**
     * @brief Set the active panel by pointer
     */
    void SetActivePanel(EditorPanel* panel);

    /**
     * @brief Find panel index
     */
    int FindPanelIndex(EditorPanel* panel) const;

    /**
     * @brief Add a panel to this node
     */
    void AddPanel(EditorPanel* panel);

    /**
     * @brief Remove a panel from this node
     */
    bool RemovePanel(EditorPanel* panel);

    /**
     * @brief Get parent as shared_ptr
     */
    Ptr GetParent() const { return parent.lock(); }

    /**
     * @brief Calculate bounds for children based on split
     */
    void CalculateChildBounds();

    /**
     * @brief Get the sibling of a child node
     */
    Ptr GetSibling(const Ptr& child) const;

    /**
     * @brief Check if this node is an ancestor of another
     */
    bool IsAncestorOf(const Ptr& node) const;

    /**
     * @brief Get depth in tree (0 for root)
     */
    int GetDepth() const;

    /**
     * @brief Collect all leaf nodes under this node
     */
    void CollectLeafNodes(std::vector<Ptr>& outLeaves);

    /**
     * @brief Find a node by ID in subtree
     */
    Ptr FindNodeById(uint64_t nodeId);

    /**
     * @brief Find node containing a specific panel
     */
    Ptr FindNodeByPanel(EditorPanel* panel);
};

// ============================================================================
// DockDropZone
// ============================================================================

/**
 * @brief Represents a drop zone for drag-drop docking
 */
struct DockDropZone {
    DockRect bounds;
    DockPosition position;
    DockNode::Ptr targetNode;
    bool isValid = true;
};

// ============================================================================
// DockDragInfo
// ============================================================================

/**
 * @brief Information about an ongoing drag operation
 */
struct DockDragInfo {
    EditorPanel* panel = nullptr;           // Panel being dragged
    DockNode::Ptr sourceNode;               // Source node (may become empty)
    glm::vec2 dragOffset{0, 0};             // Offset from panel title to mouse
    glm::vec2 currentPos{0, 0};             // Current mouse position
    DockDragState state = DockDragState::None;
    DockDropZone hoveredZone;               // Currently hovered drop zone
    bool detached = false;                  // Has panel been detached from source
};

// ============================================================================
// DockLayout
// ============================================================================

/**
 * @brief Serializable layout description
 */
struct DockLayout {
    struct NodeLayout {
        uint64_t id;
        uint64_t parentId;
        SplitDirection splitDirection;
        float splitRatio;
        std::vector<std::string> panelIds;
        int activeTabIndex;
        bool isFloating;
        glm::vec2 floatingPos;
        glm::vec2 floatingSize;
        DockRect bounds;
    };

    std::string name;
    std::vector<NodeLayout> nodes;
    uint64_t rootNodeId;
    glm::vec2 workAreaPos;
    glm::vec2 workAreaSize;
};

// ============================================================================
// DockSpace
// ============================================================================

/**
 * @brief Main docking system manager
 *
 * Manages a tree of DockNodes and handles:
 * - Panel docking/undocking
 * - Drag-drop operations
 * - Layout persistence
 * - Resize handles
 */
class DockSpace {
public:
    // Configuration
    struct Config {
        float splitterSize = 4.0f;              // Size of resize splitters
        float minNodeSize = 50.0f;              // Minimum size for any node
        float dropZoneSize = 40.0f;             // Size of drop zone indicators
        float tabHeight = 26.0f;                // Height of tab bar
        float tabCloseButtonSize = 14.0f;       // Size of tab close button
        bool allowFloating = true;              // Allow floating windows
        bool allowCloseTabs = true;             // Allow closing tabs
        bool showDropPreview = true;            // Show drop preview overlay
        glm::vec4 dropPreviewColor{0.3f, 0.5f, 0.8f, 0.3f};
        glm::vec4 splitterColor{0.2f, 0.2f, 0.25f, 1.0f};
        glm::vec4 splitterHoverColor{0.3f, 0.5f, 0.8f, 1.0f};
    };

    DockSpace();
    ~DockSpace();

    // Non-copyable
    DockSpace(const DockSpace&) = delete;
    DockSpace& operator=(const DockSpace&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the dock space
     */
    bool Initialize(const DockRect& workArea, const Config& config = Config());

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Set the work area bounds
     */
    void SetWorkArea(const DockRect& workArea);

    /**
     * @brief Get the work area bounds
     */
    const DockRect& GetWorkArea() const { return m_workArea; }

    /**
     * @brief Get configuration
     */
    Config& GetConfig() { return m_config; }
    const Config& GetConfig() const { return m_config; }

    // =========================================================================
    // Panel Management
    // =========================================================================

    /**
     * @brief Add a panel to the dock space
     * @param panel The panel to add
     * @param position Where to dock (Left, Right, Top, Bottom, Center, Floating)
     * @param relativeTo Node to dock relative to (null = root)
     * @return The node containing the panel
     */
    DockNode::Ptr AddPanel(EditorPanel* panel, DockPosition position,
                           DockNode* relativeTo = nullptr);

    /**
     * @brief Remove a panel from the dock space
     */
    bool RemovePanel(EditorPanel* panel);

    /**
     * @brief Move a panel to a new location
     */
    bool MovePanel(EditorPanel* panel, DockPosition position,
                   DockNode* relativeTo = nullptr);

    /**
     * @brief Check if a panel is docked
     */
    bool IsPanelDocked(EditorPanel* panel) const;

    /**
     * @brief Find the node containing a panel
     */
    DockNode::Ptr FindNodeByPanel(EditorPanel* panel);

    /**
     * @brief Get all docked panels
     */
    std::vector<EditorPanel*> GetAllPanels() const;

    // =========================================================================
    // Node Operations
    // =========================================================================

    /**
     * @brief Get the root node
     */
    DockNode::Ptr GetRootNode() const { return m_rootNode; }

    /**
     * @brief Split a node in the specified direction
     */
    DockNode::Ptr SplitNode(DockNode* node, DockPosition direction,
                            float ratio = 0.5f);

    /**
     * @brief Find a node by ID
     */
    DockNode::Ptr FindNodeById(uint64_t id);

    /**
     * @brief Get all leaf nodes
     */
    std::vector<DockNode::Ptr> GetLeafNodes() const;

    /**
     * @brief Get all floating nodes
     */
    std::vector<DockNode::Ptr> GetFloatingNodes() const;

    // =========================================================================
    // Drag & Drop
    // =========================================================================

    /**
     * @brief Begin dragging a panel
     */
    void BeginDrag(EditorPanel* panel, const glm::vec2& mousePos);

    /**
     * @brief Update drag state
     */
    void UpdateDrag(const glm::vec2& mousePos);

    /**
     * @brief End drag operation
     * @return true if panel was docked somewhere
     */
    bool EndDrag();

    /**
     * @brief Cancel drag operation
     */
    void CancelDrag();

    /**
     * @brief Check if drag is in progress
     */
    bool IsDragging() const { return m_dragInfo.state != DockDragState::None; }

    /**
     * @brief Get current drag info
     */
    const DockDragInfo& GetDragInfo() const { return m_dragInfo; }

    // =========================================================================
    // Resize Handles
    // =========================================================================

    /**
     * @brief Check if mouse is over a splitter
     */
    bool IsOverSplitter(const glm::vec2& mousePos, DockNode::Ptr& outNode) const;

    /**
     * @brief Begin resizing at splitter
     */
    void BeginResize(DockNode* node, const glm::vec2& mousePos);

    /**
     * @brief Update resize operation
     */
    void UpdateResize(const glm::vec2& mousePos);

    /**
     * @brief End resize operation
     */
    void EndResize();

    /**
     * @brief Reset splitter to default ratio
     */
    void ResetSplitter(DockNode* node);

    /**
     * @brief Check if resize is in progress
     */
    bool IsResizing() const { return m_isResizing; }

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render all docked panels
     */
    void Render();

    /**
     * @brief Render drop preview overlay
     */
    void RenderDropPreview();

    /**
     * @brief Render splitter handles
     */
    void RenderSplitters();

    /**
     * @brief Render floating windows
     */
    void RenderFloatingWindows();

    // =========================================================================
    // Layout Persistence
    // =========================================================================

    /**
     * @brief Save current layout to file
     */
    bool SaveLayout(const std::string& path) const;

    /**
     * @brief Load layout from file
     */
    bool LoadLayout(const std::string& path);

    /**
     * @brief Get current layout as structure
     */
    DockLayout GetCurrentLayout() const;

    /**
     * @brief Apply a layout structure
     */
    bool ApplyLayout(const DockLayout& layout);

    /**
     * @brief Clear all panels and reset to empty
     */
    void ClearLayout();

    // =========================================================================
    // Preset Layouts
    // =========================================================================

    /**
     * @brief Create the default editor layout
     * - Left: Hierarchy/Scene
     * - Right: Inspector/Properties
     * - Bottom: Console/Assets
     * - Center: Viewport
     */
    void CreateDefaultLayout();

    /**
     * @brief Create a compact layout
     * - Single panel view with tabs
     */
    void CreateCompactLayout();

    /**
     * @brief Create a wide monitor layout
     * - Optimized for ultrawide monitors
     * - More horizontal space for viewport
     */
    void CreateWideLayout();

    // =========================================================================
    // Events / Callbacks
    // =========================================================================

    /** Called when layout changes */
    std::function<void()> OnLayoutChanged;

    /** Called when a panel is docked */
    std::function<void(EditorPanel*, DockNode*)> OnPanelDocked;

    /** Called when a panel is undocked */
    std::function<void(EditorPanel*)> OnPanelUndocked;

    /** Called when a panel tab is closed */
    std::function<void(EditorPanel*)> OnPanelClosed;

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    uint64_t GenerateNodeId();
    DockNode::Ptr CreateNode();

    void RecalculateBounds();
    void RecalculateNodeBounds(DockNode::Ptr node, const DockRect& bounds);

    void CollectDropZones(DockNode::Ptr node, const glm::vec2& mousePos,
                          std::vector<DockDropZone>& zones);
    DockDropZone FindBestDropZone(const glm::vec2& mousePos);

    void RenderNode(DockNode::Ptr node);
    void RenderTabBar(DockNode::Ptr node);
    void RenderPanelContent(DockNode::Ptr node, EditorPanel* panel);
    void RenderSplitterForNode(DockNode::Ptr node);

    void RemoveEmptyNodes();
    void CollapseEmptyNode(DockNode::Ptr node);

    DockNode::Ptr DockPanelToNode(EditorPanel* panel, DockNode::Ptr targetNode,
                                   DockPosition position);

    void SerializeNode(const DockNode::Ptr& node, DockLayout& layout) const;
    DockNode::Ptr DeserializeNode(const DockLayout::NodeLayout& nodeLayout,
                                   const std::unordered_map<std::string, EditorPanel*>& panelMap);

    // =========================================================================
    // Member Variables
    // =========================================================================

    Config m_config;
    DockRect m_workArea;
    DockNode::Ptr m_rootNode;
    std::vector<DockNode::Ptr> m_floatingNodes;

    // ID generation
    uint64_t m_nextNodeId = 1;

    // Drag state
    DockDragInfo m_dragInfo;

    // Resize state
    bool m_isResizing = false;
    DockNode::Ptr m_resizeNode;
    glm::vec2 m_resizeStartPos;
    float m_resizeStartRatio = 0.5f;

    // Splitter hover
    DockNode::Ptr m_hoveredSplitterNode;

    // Initialization flag
    bool m_initialized = false;
};

// ============================================================================
// Global Dock Space Access
// ============================================================================

/**
 * @brief Get the global dock space instance
 */
DockSpace& GetDockSpace();

/**
 * @brief Set a custom dock space instance
 */
void SetDockSpace(std::unique_ptr<DockSpace> dockSpace);

} // namespace Nova
