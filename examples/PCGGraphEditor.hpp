#pragma once

#include "PCGNodeGraph.hpp"
#include <memory>
#include <vector>

/**
 * @brief Visual editor for PCG node graphs
 *
 * Provides ImGui-based node editor for creating and editing
 * procedural content generation graphs.
 */
class PCGGraphEditor {
public:
    PCGGraphEditor();
    ~PCGGraphEditor();

    /**
     * @brief Initialize the editor
     */
    bool Initialize();

    /**
     * @brief Shutdown the editor
     */
    void Shutdown();

    /**
     * @brief Render the editor UI
     */
    void Render(bool* isOpen = nullptr);

    /**
     * @brief Get the current graph
     */
    PCG::PCGGraph* GetGraph() { return m_graph.get(); }

    /**
     * @brief Set the current graph
     */
    void SetGraph(std::unique_ptr<PCG::PCGGraph> graph) {
        m_graph = std::move(graph);
    }

    /**
     * @brief Create a new empty graph
     */
    void NewGraph();

    /**
     * @brief Load graph from file
     */
    bool LoadGraph(const std::string& path);

    /**
     * @brief Save graph to file
     */
    bool SaveGraph(const std::string& path);

private:
    // UI rendering
    void RenderMenuBar();
    void RenderToolbar();
    void RenderNodePalette();
    void RenderCanvas();
    void RenderNodeContextMenu();
    void RenderPinContextMenu();
    void RenderPropertiesPanel();

    // Node creation
    void CreateNode(PCG::NodeCategory category, const std::string& type);
    void DeleteSelectedNode();

    // Connection handling
    void BeginConnection(int nodeId, int pinIndex, bool isOutput);
    void EndConnection(int nodeId, int pinIndex, bool isInput);
    void DeleteConnection(int nodeId, int pinIndex);

    // Rendering helpers
    void DrawNode(PCG::PCGNode* node);
    void DrawConnections();
    void DrawConnection(const glm::vec2& start, const glm::vec2& end, bool isActive = false);

    // Input handling
    void HandleInput();
    glm::vec2 ScreenToCanvas(const glm::vec2& screen) const;
    glm::vec2 CanvasToScreen(const glm::vec2& canvas) const;

    // State
    bool m_initialized = false;
    std::unique_ptr<PCG::PCGGraph> m_graph;

    // Canvas state
    glm::vec2 m_canvasOffset{0.0f};
    float m_canvasZoom = 1.0f;
    bool m_isPanning = false;
    glm::vec2 m_panStart{0.0f};

    // Selection
    int m_selectedNodeId = -1;
    int m_hoveredNodeId = -1;
    int m_hoveredPinNode = -1;
    int m_hoveredPinIndex = -1;
    bool m_hoveredPinIsOutput = false;

    // Connection state
    bool m_isConnecting = false;
    int m_connectionStartNode = -1;
    int m_connectionStartPin = -1;
    bool m_connectionStartIsOutput = false;
    glm::vec2 m_connectionEndPos{0.0f};

    // UI state
    bool m_showNodePalette = true;
    bool m_showProperties = true;
    bool m_showGrid = true;

    // Node creation state
    bool m_showCreateNodeMenu = false;
    glm::vec2 m_createNodePos{0.0f};

    int m_nextNodeId = 1;
};
