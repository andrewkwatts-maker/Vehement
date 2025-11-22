#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace Vehement {

class Editor;

namespace WebEditor {

class WebView;
class JSBridge;
class JSValue;

/**
 * @brief Node port (input/output connection point)
 */
struct NodePort {
    std::string id;
    std::string name;
    std::string type;        // Data type (e.g., "bool", "number", "any")
    bool isInput = true;     // Input or output
    bool allowMultiple = false;  // Allow multiple connections

    // Visual properties
    float x = 0.0f;
    float y = 0.0f;
    std::string color = "#4a9eff";
};

/**
 * @brief Node in the graph
 */
struct GraphNode {
    std::string id;
    std::string type;        // Node type (e.g., "tech", "condition", "action")
    std::string title;
    std::string subtitle;

    // Position
    float x = 0.0f;
    float y = 0.0f;

    // Size (auto-calculated or manual)
    float width = 200.0f;
    float height = 100.0f;

    // Ports
    std::vector<NodePort> inputs;
    std::vector<NodePort> outputs;

    // Custom data
    std::string dataJson;

    // Visual properties
    std::string headerColor = "#333344";
    std::string backgroundColor = "#252530";
    std::string icon;
    bool collapsed = false;
    bool selected = false;
    bool locked = false;

    // State
    bool isValid = true;
    std::string validationError;
};

/**
 * @brief Connection between two nodes
 */
struct NodeConnection {
    std::string id;
    std::string sourceNodeId;
    std::string sourcePortId;
    std::string targetNodeId;
    std::string targetPortId;

    // Visual properties
    std::string color = "#4a9eff";
    float thickness = 2.0f;
    bool animated = false;

    // State
    bool selected = false;
    bool isValid = true;
};

/**
 * @brief Node type definition for creating new nodes
 */
struct NodeTypeDefinition {
    std::string type;
    std::string category;
    std::string title;
    std::string description;
    std::string icon;
    std::string headerColor;

    std::vector<NodePort> defaultInputs;
    std::vector<NodePort> defaultOutputs;

    // Default data template
    std::string defaultDataJson;

    // Validation
    std::function<bool(const GraphNode&)> validator;
};

/**
 * @brief Graph viewport state
 */
struct GraphViewport {
    float panX = 0.0f;
    float panY = 0.0f;
    float zoom = 1.0f;

    // Zoom limits
    float minZoom = 0.1f;
    float maxZoom = 4.0f;
};

/**
 * @brief Selection state
 */
struct GraphSelection {
    std::unordered_set<std::string> nodeIds;
    std::unordered_set<std::string> connectionIds;

    bool empty() const { return nodeIds.empty() && connectionIds.empty(); }
    void clear() { nodeIds.clear(); connectionIds.clear(); }
};

/**
 * @brief Node-based graph editor panel
 *
 * Provides a visual editor for:
 * - Tech trees (technology dependencies)
 * - Behavior trees (AI logic)
 * - Dialogue trees
 * - Quest chains
 * - Any node-based data structure
 *
 * Features:
 * - Drag to create connections
 * - Zoom and pan with mouse/trackpad
 * - Mini-map for navigation
 * - Multiple selection
 * - Copy/paste nodes
 * - Undo/redo
 * - Layout algorithms (auto-arrange)
 */
class GraphEditor {
public:
    explicit GraphEditor(Editor* editor);
    ~GraphEditor();

    // Non-copyable
    GraphEditor(const GraphEditor&) = delete;
    GraphEditor& operator=(const GraphEditor&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialize the graph editor
     * @return true if successful
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render the panel
     */
    void Render();

    // =========================================================================
    // Node Type Registration
    // =========================================================================

    /**
     * @brief Register a node type
     * @param definition Node type definition
     */
    void RegisterNodeType(const NodeTypeDefinition& definition);

    /**
     * @brief Unregister a node type
     */
    void UnregisterNodeType(const std::string& type);

    /**
     * @brief Get node type definition
     */
    [[nodiscard]] const NodeTypeDefinition* GetNodeType(const std::string& type) const;

    /**
     * @brief Get all registered node types
     */
    [[nodiscard]] std::vector<std::string> GetNodeTypes() const;

    /**
     * @brief Get node types by category
     */
    [[nodiscard]] std::vector<NodeTypeDefinition> GetNodeTypesByCategory(const std::string& category) const;

    // =========================================================================
    // Graph Management
    // =========================================================================

    /**
     * @brief Clear the graph
     */
    void Clear();

    /**
     * @brief Load graph from JSON
     * @param json JSON string
     * @return true if successful
     */
    bool LoadFromJson(const std::string& json);

    /**
     * @brief Save graph to JSON
     * @return JSON string
     */
    [[nodiscard]] std::string SaveToJson() const;

    /**
     * @brief Load graph from file
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief Save graph to file
     */
    bool SaveToFile(const std::string& path);

    /**
     * @brief Check if graph has unsaved changes
     */
    [[nodiscard]] bool IsDirty() const { return m_isDirty; }

    /**
     * @brief Mark as saved
     */
    void MarkClean() { m_isDirty = false; }

    // =========================================================================
    // Node Operations
    // =========================================================================

    /**
     * @brief Create a new node
     * @param type Node type
     * @param x X position
     * @param y Y position
     * @return Node ID, or empty string on failure
     */
    std::string CreateNode(const std::string& type, float x, float y);

    /**
     * @brief Delete a node
     * @param nodeId Node ID
     * @return true if successful
     */
    bool DeleteNode(const std::string& nodeId);

    /**
     * @brief Duplicate a node
     * @param nodeId Node ID to duplicate
     * @return ID of new node, or empty string on failure
     */
    std::string DuplicateNode(const std::string& nodeId);

    /**
     * @brief Get node by ID
     */
    [[nodiscard]] GraphNode* GetNode(const std::string& nodeId);
    [[nodiscard]] const GraphNode* GetNode(const std::string& nodeId) const;

    /**
     * @brief Get all nodes
     */
    [[nodiscard]] const std::vector<GraphNode>& GetNodes() const { return m_nodes; }

    /**
     * @brief Update node position
     */
    void SetNodePosition(const std::string& nodeId, float x, float y);

    /**
     * @brief Update node data
     */
    void SetNodeData(const std::string& nodeId, const std::string& dataJson);

    /**
     * @brief Collapse/expand a node
     */
    void SetNodeCollapsed(const std::string& nodeId, bool collapsed);

    // =========================================================================
    // Connection Operations
    // =========================================================================

    /**
     * @brief Create a connection between two ports
     * @return Connection ID, or empty string on failure
     */
    std::string CreateConnection(const std::string& sourceNodeId,
                                  const std::string& sourcePortId,
                                  const std::string& targetNodeId,
                                  const std::string& targetPortId);

    /**
     * @brief Delete a connection
     */
    bool DeleteConnection(const std::string& connectionId);

    /**
     * @brief Get connection by ID
     */
    [[nodiscard]] NodeConnection* GetConnection(const std::string& connectionId);

    /**
     * @brief Get all connections
     */
    [[nodiscard]] const std::vector<NodeConnection>& GetConnections() const { return m_connections; }

    /**
     * @brief Get connections for a node
     */
    [[nodiscard]] std::vector<NodeConnection> GetNodeConnections(const std::string& nodeId) const;

    /**
     * @brief Check if connection would be valid
     */
    [[nodiscard]] bool CanConnect(const std::string& sourceNodeId,
                                   const std::string& sourcePortId,
                                   const std::string& targetNodeId,
                                   const std::string& targetPortId) const;

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * @brief Select a node
     */
    void SelectNode(const std::string& nodeId, bool addToSelection = false);

    /**
     * @brief Select a connection
     */
    void SelectConnection(const std::string& connectionId, bool addToSelection = false);

    /**
     * @brief Select all
     */
    void SelectAll();

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Get selection
     */
    [[nodiscard]] const GraphSelection& GetSelection() const { return m_selection; }

    /**
     * @brief Delete selected items
     */
    void DeleteSelection();

    /**
     * @brief Copy selected to clipboard
     */
    void CopySelection();

    /**
     * @brief Paste from clipboard
     */
    void Paste();

    // =========================================================================
    // Viewport
    // =========================================================================

    /**
     * @brief Get viewport
     */
    [[nodiscard]] const GraphViewport& GetViewport() const { return m_viewport; }

    /**
     * @brief Set pan position
     */
    void SetPan(float x, float y);

    /**
     * @brief Set zoom level
     */
    void SetZoom(float zoom);

    /**
     * @brief Zoom to fit all nodes
     */
    void ZoomToFit();

    /**
     * @brief Zoom to fit selection
     */
    void ZoomToSelection();

    /**
     * @brief Center on node
     */
    void CenterOnNode(const std::string& nodeId);

    /**
     * @brief Convert screen coordinates to graph coordinates
     */
    void ScreenToGraph(float screenX, float screenY, float& graphX, float& graphY) const;

    /**
     * @brief Convert graph coordinates to screen coordinates
     */
    void GraphToScreen(float graphX, float graphY, float& screenX, float& screenY) const;

    // =========================================================================
    // Mini-map
    // =========================================================================

    /**
     * @brief Enable/disable mini-map
     */
    void SetMinimapEnabled(bool enabled) { m_showMinimap = enabled; }

    /**
     * @brief Check if mini-map is enabled
     */
    [[nodiscard]] bool IsMinimapEnabled() const { return m_showMinimap; }

    /**
     * @brief Set mini-map position (corner)
     */
    enum class MinimapPosition { TopLeft, TopRight, BottomLeft, BottomRight };
    void SetMinimapPosition(MinimapPosition pos) { m_minimapPosition = pos; }

    // =========================================================================
    // Layout
    // =========================================================================

    /**
     * @brief Auto-arrange nodes
     */
    enum class LayoutAlgorithm {
        Hierarchical,   // Top-down tree layout
        ForceDirected,  // Physics-based layout
        Grid,           // Simple grid
        Horizontal,     // Left-to-right
        Vertical        // Top-to-bottom
    };
    void AutoArrange(LayoutAlgorithm algorithm = LayoutAlgorithm::Hierarchical);

    /**
     * @brief Align selected nodes
     */
    enum class Alignment { Left, Right, Top, Bottom, CenterH, CenterV };
    void AlignSelection(Alignment alignment);

    /**
     * @brief Distribute selected nodes evenly
     */
    void DistributeSelection(bool horizontal);

    // =========================================================================
    // Undo/Redo
    // =========================================================================

    /**
     * @brief Undo last action
     */
    void Undo();

    /**
     * @brief Redo last undone action
     */
    void Redo();

    /**
     * @brief Check if undo is available
     */
    [[nodiscard]] bool CanUndo() const { return m_undoIndex > 0; }

    /**
     * @brief Check if redo is available
     */
    [[nodiscard]] bool CanRedo() const { return m_undoIndex < m_undoHistory.size(); }

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate entire graph
     * @return List of validation errors
     */
    [[nodiscard]] std::vector<std::string> Validate() const;

    /**
     * @brief Check if graph is valid
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief Find cycles in graph (invalid for trees)
     */
    [[nodiscard]] std::vector<std::vector<std::string>> FindCycles() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnNodeCreated;
    std::function<void(const std::string&)> OnNodeDeleted;
    std::function<void(const std::string&)> OnNodeSelected;
    std::function<void(const std::string&)> OnNodeDoubleClicked;
    std::function<void(const std::string&, const std::string&)> OnNodeDataChanged;
    std::function<void(const std::string&)> OnConnectionCreated;
    std::function<void(const std::string&)> OnConnectionDeleted;
    std::function<void()> OnSelectionChanged;
    std::function<void()> OnGraphChanged;

private:
    // JavaScript bridge setup
    void SetupJSBridge();
    void RegisterBridgeFunctions();

    // State management
    void SaveUndoState();
    void RestoreState(const std::string& stateJson);

    // ID generation
    std::string GenerateNodeId();
    std::string GenerateConnectionId();

    // Validation helpers
    bool ValidateConnection(const NodeConnection& conn) const;
    bool WouldCreateCycle(const std::string& sourceNodeId, const std::string& targetNodeId) const;

    // Layout helpers
    void LayoutHierarchical();
    void LayoutForceDirected();
    void LayoutGrid();

    Editor* m_editor = nullptr;

    // Web view
    std::unique_ptr<WebView> m_webView;
    std::unique_ptr<JSBridge> m_bridge;

    // Node types
    std::unordered_map<std::string, NodeTypeDefinition> m_nodeTypes;

    // Graph data
    std::vector<GraphNode> m_nodes;
    std::vector<NodeConnection> m_connections;
    std::unordered_map<std::string, size_t> m_nodeIndex;
    std::unordered_map<std::string, size_t> m_connectionIndex;

    // State
    GraphSelection m_selection;
    GraphViewport m_viewport;
    bool m_isDirty = false;

    // Mini-map
    bool m_showMinimap = true;
    MinimapPosition m_minimapPosition = MinimapPosition::BottomRight;

    // Undo/redo
    std::vector<std::string> m_undoHistory;  // JSON snapshots
    size_t m_undoIndex = 0;
    static constexpr size_t MAX_UNDO_HISTORY = 50;

    // Clipboard
    std::string m_clipboardJson;

    // ID counters
    uint64_t m_nextNodeId = 1;
    uint64_t m_nextConnectionId = 1;

    // Drag state
    bool m_isDraggingConnection = false;
    std::string m_dragSourceNodeId;
    std::string m_dragSourcePortId;
    float m_dragEndX = 0.0f;
    float m_dragEndY = 0.0f;
};

} // namespace WebEditor
} // namespace Vehement
