#pragma once

#include "VisualScriptingCore.hpp"
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>

namespace Nova {
namespace VisualScript {

/**
 * @brief Visual node editor for creating and editing visual scripts
 *
 * Features:
 * - Node palette with search and categories
 * - Canvas for arranging and connecting nodes
 * - Property inspector for selected node
 * - Binding browser for discovering bindable properties
 * - Warning panel for loose/broken bindings
 */
class VisualScriptEditor {
public:
    VisualScriptEditor();
    ~VisualScriptEditor() = default;

    // Main render function - call each frame
    void Render();

    // Graph management
    void NewGraph(const std::string& name = "New Graph");
    void LoadGraph(const std::string& filepath);
    void SaveGraph(const std::string& filepath);
    void SetGraph(GraphPtr graph);
    GraphPtr GetGraph() const { return m_graph; }

    // Selection
    void SelectNode(NodePtr node, bool addToSelection = false);
    void ClearSelection();
    void SelectAll();
    NodePtr GetSelectedNode() const { return m_selectedNode; }
    const std::vector<NodePtr>& GetSelectedNodes() const { return m_selectedNodes; }
    bool IsNodeSelected(NodePtr node) const;

    // Clipboard operations
    void CopySelected();
    void PasteAtPosition(const glm::vec2& position);
    void DuplicateSelected();
    void DeleteSelected();

    // Frame selected nodes in view
    void FrameSelected();
    void FrameAll();

    // Callbacks
    using GraphChangedCallback = std::function<void(GraphPtr)>;
    void SetOnGraphChanged(GraphChangedCallback callback) { m_onGraphChanged = callback; }

    // Panel visibility
    void SetShowNodePalette(bool show) { m_showNodePalette = show; }
    void SetShowPropertyInspector(bool show) { m_showPropertyInspector = show; }
    void SetShowBindingBrowser(bool show) { m_showBindingBrowser = show; }
    void SetShowWarnings(bool show) { m_showWarnings = show; }

private:
    // Panel rendering
    void RenderMenuBar();
    void RenderNodePalette();
    void RenderCanvas();
    void RenderPropertyInspector();
    void RenderBindingBrowser();
    void RenderWarningsPanel();
    void RenderContextMenu();
    void RenderVariablesPanel();
    void RenderBoxSelection();

    // Keyboard shortcuts
    void HandleKeyboardShortcuts();

    // Canvas helpers
    void RenderNode(NodePtr node);
    void RenderPort(PortPtr port, const ImVec2& nodePos, float& yOffset);
    void RenderConnections();
    void RenderPendingConnection();

    // Node creation
    void CreateNodeAtPosition(const std::string& typeId, const glm::vec2& position);

    // Connection handling
    void BeginConnection(PortPtr port);
    void EndConnection(PortPtr port);
    void CancelConnection();

    // Binding helpers
    void RenderBindingState(const BindingReference& ref);
    void RenderBindableProperty(const BindableProperty& prop);
    ImU32 GetBindingStateColor(BindingState state);
    ImU32 GetPortColor(PortType type);

    // Undo/Redo
    void PushUndoState();
    void Undo();
    void Redo();

    // Graph state
    GraphPtr m_graph;
    NodePtr m_selectedNode;
    std::vector<NodePtr> m_selectedNodes;  // Multi-select support
    PortPtr m_selectedPort;

    // Connection dragging
    bool m_isDraggingConnection = false;
    PortPtr m_connectionStartPort;
    ImVec2 m_connectionEndPos;

    // Canvas state
    ImVec2 m_canvasOffset{0.0f, 0.0f};
    float m_canvasZoom = 1.0f;
    bool m_isPanning = false;
    ImVec2 m_panStartPos;

    // Box selection state
    bool m_isBoxSelecting = false;
    ImVec2 m_boxSelectStart;
    ImVec2 m_boxSelectEnd;

    // Node dragging (multi-node support)
    bool m_isDraggingNodes = false;
    std::unordered_map<std::string, glm::vec2> m_dragStartPositions;

    // Clipboard for copy/paste
    nlohmann::json m_clipboard;

    // Search/filter
    char m_nodeSearchBuffer[256] = "";
    char m_bindingSearchBuffer[256] = "";
    std::string m_selectedCategory;

    // Panel visibility
    bool m_showNodePalette = true;
    bool m_showPropertyInspector = true;
    bool m_showBindingBrowser = true;
    bool m_showWarnings = true;

    // Context menu
    bool m_showContextMenu = false;
    ImVec2 m_contextMenuPos;

    // Undo/Redo
    std::vector<nlohmann::json> m_undoStack;
    std::vector<nlohmann::json> m_redoStack;
    size_t m_maxUndoSteps = 50;

    // Dirty flag
    bool m_isDirty = false;
    std::string m_currentFilepath;

    // Callbacks
    GraphChangedCallback m_onGraphChanged;

    // Style
    struct Style {
        ImVec4 nodeBackground{0.15f, 0.15f, 0.15f, 1.0f};
        ImVec4 nodeSelected{0.25f, 0.25f, 0.35f, 1.0f};
        ImVec4 nodeHeader{0.3f, 0.3f, 0.4f, 1.0f};
        float nodeRounding = 8.0f;
        float portRadius = 6.0f;
        float connectionThickness = 2.5f;
        float gridSize = 32.0f;

        // Port colors by type
        ImVec4 flowPortColor{1.0f, 1.0f, 1.0f, 1.0f};
        ImVec4 dataPortColor{0.3f, 0.7f, 1.0f, 1.0f};
        ImVec4 eventPortColor{1.0f, 0.5f, 0.2f, 1.0f};
        ImVec4 bindingPortColor{0.2f, 0.9f, 0.3f, 1.0f};

        // Binding state colors
        ImVec4 hardBindingColor{0.2f, 0.9f, 0.3f, 1.0f};
        ImVec4 looseBindingColor{1.0f, 0.9f, 0.2f, 1.0f};
        ImVec4 brokenBindingColor{1.0f, 0.2f, 0.2f, 1.0f};
    } m_style;
};

/**
 * @brief Standalone window wrapper for the visual script editor
 */
class VisualScriptEditorWindow {
public:
    VisualScriptEditorWindow(const std::string& title = "Visual Script Editor");

    void Render();
    void Open() { m_isOpen = true; }
    void Close() { m_isOpen = false; }
    bool IsOpen() const { return m_isOpen; }

    VisualScriptEditor& GetEditor() { return m_editor; }

private:
    std::string m_title;
    bool m_isOpen = true;
    VisualScriptEditor m_editor;
};

} // namespace VisualScript
} // namespace Nova
