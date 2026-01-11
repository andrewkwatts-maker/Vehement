#pragma once

#include "../../../../engine/scripting/EventNodes.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace Vehement {

// Forward declarations
class Editor;
class EntityManager;

/**
 * @brief Visual position for a node in the editor
 */
struct EventNodeVisual {
    uint64_t nodeId = 0;
    glm::vec2 position{0.0f};
    glm::vec2 size{200.0f, 100.0f};
    bool collapsed = false;
    bool selected = false;
    glm::vec4 headerColor{0.3f, 0.3f, 0.5f, 1.0f};
};

/**
 * @brief Connection wire visual
 */
struct EventConnectionVisual {
    uint64_t fromNode = 0;
    std::string fromPin;
    uint64_t toNode = 0;
    std::string toPin;
    glm::vec4 color{0.8f, 0.8f, 0.8f, 1.0f};
    float thickness = 2.0f;
};

/**
 * @brief Node category colors
 */
struct EventCategoryStyle {
    Nova::EventNodeCategory category;
    std::string name;
    std::string icon;
    glm::vec4 color;
};

/**
 * @brief Entity event graph with visual data
 */
struct EntityEventGraph {
    std::string name;
    std::string entityType;  // "unit", "building", "hero"
    std::string entityId;    // specific entity ID
    Nova::EventGraph graph;
    std::vector<EventNodeVisual> nodeVisuals;
    std::string description;
    bool modified = false;
};

/**
 * @brief Entity Event Editor - Visual node editor for entity events
 *
 * Features:
 * - Visual node-based event scripting
 * - Drag-and-drop node creation
 * - Pin connection system (flow and data)
 * - Multiple event graphs per entity
 * - Python code generation preview
 * - Entity-specific event templates
 */
class EntityEventEditor {
public:
    struct Config {
        float gridSize = 20.0f;
        float nodeWidth = 200.0f;
        float pinRadius = 6.0f;
        float connectionThickness = 2.0f;
        bool showGrid = true;
        bool snapToGrid = true;
        bool showMinimap = true;
        bool showCodePreview = false;
        glm::vec4 backgroundColor{0.12f, 0.12f, 0.15f, 1.0f};
        glm::vec4 gridColor{0.2f, 0.2f, 0.25f, 1.0f};
        glm::vec4 selectionColor{0.4f, 0.6f, 1.0f, 0.3f};
    };

    EntityEventEditor();
    ~EntityEventEditor();

    /**
     * @brief Initialize the editor
     */
    void Initialize(Editor* editor, const Config& config = {});

    /**
     * @brief Set entity manager reference
     */
    void SetEntityManager(EntityManager* manager) { m_entityManager = manager; }

    // =========================================================================
    // Graph Management
    // =========================================================================

    /**
     * @brief Create new event graph for entity
     */
    EntityEventGraph* CreateGraph(const std::string& name, const std::string& entityType, const std::string& entityId);

    /**
     * @brief Load graph from file
     */
    bool LoadGraph(const std::string& path);

    /**
     * @brief Save current graph
     */
    bool SaveGraph(const std::string& path = "");

    /**
     * @brief Get current graph
     */
    [[nodiscard]] EntityEventGraph* GetCurrentGraph() { return m_currentGraph; }

    /**
     * @brief Set current graph
     */
    void SetCurrentGraph(EntityEventGraph* graph);

    /**
     * @brief Get all graphs
     */
    [[nodiscard]] const std::vector<std::unique_ptr<EntityEventGraph>>& GetGraphs() const { return m_graphs; }

    /**
     * @brief Close graph
     */
    void CloseGraph(EntityEventGraph* graph);

    // =========================================================================
    // Node Operations
    // =========================================================================

    /**
     * @brief Add node at position
     */
    Nova::EventNode::Ptr AddNode(const std::string& typeName, const glm::vec2& position);

    /**
     * @brief Remove selected nodes
     */
    void RemoveSelectedNodes();

    /**
     * @brief Duplicate selected nodes
     */
    void DuplicateSelectedNodes();

    /**
     * @brief Copy selected nodes to clipboard
     */
    void CopySelectedNodes();

    /**
     * @brief Paste nodes from clipboard
     */
    void PasteNodes(const glm::vec2& position);

    /**
     * @brief Cut selected nodes
     */
    void CutSelectedNodes();

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * @brief Select node
     */
    void SelectNode(uint64_t nodeId, bool addToSelection = false);

    /**
     * @brief Deselect node
     */
    void DeselectNode(uint64_t nodeId);

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Select all nodes
     */
    void SelectAllNodes();

    /**
     * @brief Box select nodes
     */
    void BoxSelectNodes(const glm::vec2& start, const glm::vec2& end);

    /**
     * @brief Get selected nodes
     */
    [[nodiscard]] const std::unordered_set<uint64_t>& GetSelectedNodes() const { return m_selectedNodes; }

    // =========================================================================
    // Connections
    // =========================================================================

    /**
     * @brief Start connection from pin
     */
    void StartConnection(uint64_t nodeId, const std::string& pinName, bool isOutput);

    /**
     * @brief Complete connection to pin
     */
    bool CompleteConnection(uint64_t nodeId, const std::string& pinName);

    /**
     * @brief Cancel connection
     */
    void CancelConnection();

    /**
     * @brief Remove connection
     */
    void RemoveConnection(uint64_t fromNode, const std::string& fromPin, uint64_t toNode, const std::string& toPin);

    /**
     * @brief Check if connecting
     */
    [[nodiscard]] bool IsConnecting() const { return m_isConnecting; }

    // =========================================================================
    // View Control
    // =========================================================================

    /**
     * @brief Pan view
     */
    void Pan(const glm::vec2& delta);

    /**
     * @brief Zoom view
     */
    void Zoom(float delta, const glm::vec2& center);

    /**
     * @brief Reset view
     */
    void ResetView();

    /**
     * @brief Frame all nodes
     */
    void FrameAll();

    /**
     * @brief Frame selected nodes
     */
    void FrameSelected();

    /**
     * @brief Get view offset
     */
    [[nodiscard]] glm::vec2 GetViewOffset() const { return m_viewOffset; }

    /**
     * @brief Get view scale
     */
    [[nodiscard]] float GetViewScale() const { return m_viewScale; }

    // =========================================================================
    // Compilation
    // =========================================================================

    /**
     * @brief Compile graph to Python
     */
    std::string CompileToPython();

    /**
     * @brief Validate graph
     */
    bool ValidateGraph(std::vector<std::string>& errors);

    /**
     * @brief Export to Python file
     */
    bool ExportToPython(const std::string& path);

    // =========================================================================
    // Templates
    // =========================================================================

    /**
     * @brief Load template graph
     */
    void LoadTemplate(const std::string& templateName);

    /**
     * @brief Save as template
     */
    bool SaveAsTemplate(const std::string& templateName);

    /**
     * @brief Get available templates
     */
    [[nodiscard]] std::vector<std::string> GetAvailableTemplates() const;

    // =========================================================================
    // UI Rendering
    // =========================================================================

    /**
     * @brief Render the editor UI
     */
    void Render();

    /**
     * @brief Process input
     */
    void ProcessInput();

    /**
     * @brief Update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(Nova::EventNode::Ptr)> OnNodeAdded;
    std::function<void(uint64_t)> OnNodeRemoved;
    std::function<void()> OnSelectionChanged;
    std::function<void()> OnGraphModified;
    std::function<void(const std::string&)> OnCompiled;

private:
    // UI Rendering
    void RenderCanvas();
    void RenderGrid();
    void RenderNodes();
    void RenderNode(EventNodeVisual& visual, Nova::EventNode* node);
    void RenderConnections();
    void RenderConnectionWire(const glm::vec2& start, const glm::vec2& end, const glm::vec4& color, float thickness);
    void RenderPendingConnection();
    void RenderSelectionBox();
    void RenderMinimap();
    void RenderNodePalette();
    void RenderPropertyPanel();
    void RenderCodePreview();
    void RenderToolbar();
    void RenderContextMenu();

    // Node rendering helpers
    void RenderNodeHeader(EventNodeVisual& visual, Nova::EventNode* node);
    void RenderNodePins(EventNodeVisual& visual, Nova::EventNode* node);
    void RenderPin(const glm::vec2& pos, const Nova::EventPin& pin, bool isOutput, bool isHovered);
    glm::vec2 GetPinPosition(const EventNodeVisual& visual, Nova::EventNode* node, const std::string& pinName, bool isOutput);

    // Coordinate conversion
    glm::vec2 ScreenToCanvas(const glm::vec2& screen) const;
    glm::vec2 CanvasToScreen(const glm::vec2& canvas) const;

    // Visual management
    EventNodeVisual* GetNodeVisual(uint64_t nodeId);
    EventNodeVisual& CreateNodeVisual(uint64_t nodeId, const glm::vec2& position);
    void UpdateNodeSize(EventNodeVisual& visual, Nova::EventNode* node);
    glm::vec4 GetCategoryColor(Nova::EventNodeCategory category) const;

    // Snap to grid
    glm::vec2 SnapToGrid(const glm::vec2& position) const;

    // Category styles
    void InitializeCategoryStyles();

    Config m_config;
    Editor* m_editor = nullptr;
    EntityManager* m_entityManager = nullptr;

    // Graphs
    std::vector<std::unique_ptr<EntityEventGraph>> m_graphs;
    EntityEventGraph* m_currentGraph = nullptr;

    // Selection
    std::unordered_set<uint64_t> m_selectedNodes;

    // View
    glm::vec2 m_viewOffset{0.0f};
    float m_viewScale = 1.0f;
    glm::vec2 m_canvasSize{800.0f, 600.0f};
    glm::vec2 m_canvasPos{0.0f};

    // Connection state
    bool m_isConnecting = false;
    uint64_t m_connectionStartNode = 0;
    std::string m_connectionStartPin;
    bool m_connectionStartIsOutput = false;
    glm::vec2 m_connectionEndPos{0.0f};

    // Selection box
    bool m_isBoxSelecting = false;
    glm::vec2 m_boxSelectStart{0.0f};
    glm::vec2 m_boxSelectEnd{0.0f};

    // Dragging
    bool m_isDraggingNodes = false;
    glm::vec2 m_dragStartPos{0.0f};
    std::unordered_map<uint64_t, glm::vec2> m_dragStartPositions;

    // Panning
    bool m_isPanning = false;
    glm::vec2 m_panStartPos{0.0f};

    // Context menu
    bool m_showContextMenu = false;
    glm::vec2 m_contextMenuPos{0.0f};
    std::string m_contextMenuFilter;

    // UI state
    bool m_showNodePalette = true;
    bool m_showPropertyPanel = true;
    bool m_showCodePreview = false;
    uint64_t m_hoveredNode = 0;
    std::string m_hoveredPin;
    bool m_hoveredPinIsOutput = false;

    // Clipboard
    std::vector<std::pair<Nova::EventNode::Ptr, glm::vec2>> m_clipboard;

    // Category styles
    std::vector<EventCategoryStyle> m_categoryStyles;

    // Search
    std::string m_nodeSearchFilter;

    bool m_initialized = false;
};

} // namespace Vehement
