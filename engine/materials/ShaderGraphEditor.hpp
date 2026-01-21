/**
 * @file ShaderGraphEditor.hpp
 * @brief Visual node-based shader editor UI
 *
 * Provides an ImGui-based visual scripting interface for building materials
 * similar to Unreal Material Editor or Unity Shader Graph.
 */

#pragma once

#include "ShaderGraph.hpp"
#include "ShaderNodes.hpp"
#include "NoiseNodes.hpp"
#include "../graphics/PreviewRenderer.hpp"
#include "../graphics/Material.hpp"
#include "../graphics/Shader.hpp"
#include <imgui.h>
#include <unordered_map>
#include <functional>
#include <memory>
#include <string>
#include <optional>
#include <variant>
#include <vector>

namespace Nova {

/**
 * @brief Visual position and size of a node in the editor
 */
struct NodeVisualData {
    ImVec2 position{100, 100};
    ImVec2 size{200, 150};
    bool selected = false;
    bool collapsed = false;
    uint32_t colorTint = 0xFFFFFFFF;
};

/**
 * @brief Connection link between two pins
 */
struct NodeLink {
    uint64_t id;
    uint64_t sourceNodeId;
    std::string sourcePin;
    uint64_t destNodeId;
    std::string destPin;
};

/**
 * @brief Editor state for undo/redo
 */
struct EditorAction {
    enum class Type {
        CreateNode,
        DeleteNode,
        MoveNode,
        CreateLink,
        DeleteLink,
        ChangeProperty
    };

    Type type;
    std::string data; // JSON serialized state
};

/**
 * @brief Mini-map display for navigating large graphs
 */
class MiniMap {
public:
    void Draw(const ImVec2& editorSize, const ImVec2& canvasOffset,
              const std::vector<std::pair<ImVec2, ImVec2>>& nodeBounds);

    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }

private:
    bool m_enabled = true;
    ImVec2 m_size{150, 100};
    float m_padding = 10.0f;
};

/**
 * @brief Visual shader graph editor
 */
class ShaderGraphEditor {
public:
    ShaderGraphEditor();
    ~ShaderGraphEditor();

    /**
     * @brief Initialize preview renderer (must be called after OpenGL context is ready)
     */
    void Initialize();

    /**
     * @brief Set the graph to edit
     */
    void SetGraph(ShaderGraph* graph);

    /**
     * @brief Get the current graph
     */
    ShaderGraph* GetGraph() const { return m_graph; }

    /**
     * @brief Create a new empty graph
     */
    void NewGraph();

    /**
     * @brief Draw the editor UI
     */
    void Draw();

    /**
     * @brief Handle keyboard shortcuts
     */
    void HandleShortcuts();

    /**
     * @brief Save graph to JSON file
     */
    bool SaveToFile(const std::string& path);

    /**
     * @brief Load graph from JSON file
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief Compile and preview shader
     */
    void CompileShader();

    /**
     * @brief Get compiled vertex shader
     */
    const std::string& GetCompiledVertexShader() const { return m_compiledVS; }

    /**
     * @brief Get compiled fragment shader
     */
    const std::string& GetCompiledFragmentShader() const { return m_compiledFS; }

    /**
     * @brief Undo last action
     */
    void Undo();

    /**
     * @brief Redo last undone action
     */
    void Redo();

    /**
     * @brief Delete selected nodes
     */
    void DeleteSelected();

    /**
     * @brief Duplicate selected nodes
     */
    void DuplicateSelected();

    /**
     * @brief Select all nodes
     */
    void SelectAll();

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Frame all nodes in view
     */
    void FrameAll();

    /**
     * @brief Frame selected nodes
     */
    void FrameSelected();

    // Callbacks
    using CompiledCallback = std::function<void(const std::string& vs, const std::string& fs)>;
    void SetCompiledCallback(CompiledCallback callback) { m_compiledCallback = callback; }

    /**
     * @brief Set auto-compile mode (automatically recompiles when graph changes)
     */
    void SetAutoCompile(bool autoCompile) { m_autoCompile = autoCompile; }
    bool GetAutoCompile() const { return m_autoCompile; }

    /**
     * @brief Mark the graph as dirty (needs recompile)
     */
    void MarkGraphDirty() { m_graphDirty = true; }

    /**
     * @brief Get the preview renderer
     */
    PreviewRenderer* GetPreviewRenderer() { return m_previewRenderer.get(); }

private:
    // Drawing methods
    void DrawMenuBar();
    void DrawToolbar();
    void DrawNodeCanvas();
    void DrawNode(ShaderNode* node, NodeVisualData& visual, uint64_t nodeId);
    void DrawNodePins(ShaderNode* node, const ImVec2& nodePos, uint64_t nodeId);
    void DrawLinks();
    void DrawPendingLink();
    void DrawContextMenu();
    void DrawNodePalette();
    void DrawPropertyPanel();
    void DrawPreviewPanel();
    void DrawShaderOutput();
    void DrawMiniMap();

    // Node creation
    ShaderNode::Ptr CreateNodeFromType(const std::string& typeName);
    void AddNodeAtPosition(const std::string& typeName, const ImVec2& pos);

    // Link management
    bool CanCreateLink(uint64_t srcNode, const std::string& srcPin,
                       uint64_t dstNode, const std::string& dstPin);
    void CreateLink(uint64_t srcNode, const std::string& srcPin,
                    uint64_t dstNode, const std::string& dstPin);
    void DeleteLink(uint64_t linkId);
    void DeleteLinksForNode(uint64_t nodeId);

    // Utility
    uint64_t GetNextId();
    ImVec2 ScreenToCanvas(const ImVec2& screenPos);
    ImVec2 CanvasToScreen(const ImVec2& canvasPos);
    ImU32 GetTypeColor(ShaderDataType type);
    const char* GetCategoryIcon(NodeCategory category);
    void RecordAction(EditorAction::Type type, const std::string& data);
    void UpdateNodeConnections();

    // Preview compilation
    bool CompileGraphToShader();
    void UpdatePreviewMaterial();

    // State
    ShaderGraph* m_graph = nullptr;
    std::shared_ptr<ShaderGraph> m_ownedGraph;  // shared_ptr to match ShaderGraph::FromJson return type
    std::unordered_map<uint64_t, NodeVisualData> m_nodeVisuals;
    std::vector<NodeLink> m_links;
    std::vector<uint64_t> m_selectedNodes;

    // Canvas state
    ImVec2 m_canvasOffset{0, 0};
    float m_zoom = 1.0f;
    bool m_isPanning = false;
    ImVec2 m_panStart;

    // Link creation state
    bool m_isLinking = false;
    uint64_t m_linkSourceNode = 0;
    std::string m_linkSourcePin;
    bool m_linkFromOutput = false;
    ImVec2 m_linkEndPos;

    // Node dragging
    bool m_isDragging = false;
    ImVec2 m_dragStartPos;
    std::unordered_map<uint64_t, ImVec2> m_dragStartPositions;

    // Box selection
    bool m_isBoxSelecting = false;
    ImVec2 m_boxSelectStart;
    ImVec2 m_boxSelectEnd;

    // Context menu
    bool m_showContextMenu = false;
    ImVec2 m_contextMenuPos;
    std::string m_contextMenuSearch;

    // Panels
    bool m_showPalette = true;
    bool m_showProperties = true;
    bool m_showPreview = true;
    bool m_showShaderCode = false;

    // Undo/redo
    std::vector<EditorAction> m_undoStack;
    std::vector<EditorAction> m_redoStack;
    static constexpr size_t MAX_UNDO_STACK = 100;

    // Compiled output
    std::string m_compiledVS;
    std::string m_compiledFS;
    bool m_needsRecompile = true;
    std::string m_compileError;

    // Preview
    uint32_t m_previewTexture = 0;
    float m_previewRotation = 0.0f;
    int m_previewMeshType = 0; // 0=sphere, 1=cube, 2=plane

    // Preview renderer integration
    std::unique_ptr<PreviewRenderer> m_previewRenderer;
    std::shared_ptr<Material> m_previewMaterial;
    std::shared_ptr<Shader> m_compiledShader;
    bool m_autoCompile = true;
    bool m_graphDirty = true;
    int m_previewSize = 256;

    // Callbacks
    CompiledCallback m_compiledCallback;

    // Node factory categories
    struct NodeInfo {
        std::string name;
        std::string typeName;
        std::string description;
        NodeCategory category;
    };
    std::vector<NodeInfo> m_nodeInfos;

    // ID counter
    uint64_t m_nextId = 1;

    // File state
    std::string m_currentFilePath;
    bool m_showOpenDialog = false;
    bool m_showSaveDialog = false;
    char m_filePathBuffer[512] = {0};

    // Clipboard
    std::string m_clipboard;  // JSON representation of copied nodes

    // Mini-map
    MiniMap m_miniMap;

    // Node color scheme by category
    static constexpr ImU32 COLOR_INPUT = IM_COL32(60, 120, 180, 255);
    static constexpr ImU32 COLOR_PARAMETER = IM_COL32(120, 60, 180, 255);
    static constexpr ImU32 COLOR_TEXTURE = IM_COL32(180, 120, 60, 255);
    static constexpr ImU32 COLOR_MATH = IM_COL32(80, 160, 80, 255);
    static constexpr ImU32 COLOR_VECTOR = IM_COL32(160, 80, 160, 255);
    static constexpr ImU32 COLOR_UTILITY = IM_COL32(100, 100, 100, 255);
    static constexpr ImU32 COLOR_NOISE = IM_COL32(60, 160, 160, 255);
    static constexpr ImU32 COLOR_PATTERN = IM_COL32(160, 160, 60, 255);
    static constexpr ImU32 COLOR_OUTPUT = IM_COL32(180, 60, 60, 255);
};

/**
 * @brief Material library browser
 */
class MaterialLibrary {
public:
    using MaterialSelectedCallback = std::function<void(const std::string& path)>;

    /**
     * @brief Draw the library browser panel
     */
    void Draw();

    /**
     * @brief Add a material to the library
     */
    void AddMaterial(const std::string& name, const std::string& category,
                     const std::string& jsonPath);

    /**
     * @brief Get material JSON path by name
     */
    std::optional<std::string> GetMaterialPath(const std::string& name) const;

    /**
     * @brief Scan directory for materials
     */
    void ScanDirectory(const std::string& path);

    /**
     * @brief Set callback for when a material is selected
     */
    void SetMaterialSelectedCallback(MaterialSelectedCallback callback) { m_onMaterialSelected = callback; }

    /**
     * @brief Get the last selected material path (alternative to callback)
     */
    const std::string& GetSelectedPath() const { return m_selectedPath; }

    /**
     * @brief Clear the selected path
     */
    void ClearSelectedPath() { m_selectedPath.clear(); }

private:
    struct MaterialEntry {
        std::string name;
        std::string category;
        std::string path;
        uint32_t thumbnail = 0;
    };

    std::vector<MaterialEntry> m_materials;
    std::string m_searchFilter;
    std::string m_categoryFilter;
    std::string m_selectedPath;
    MaterialSelectedCallback m_onMaterialSelected;
};

/**
 * @brief Shader parameter inspector
 */
class ShaderParameterInspector {
public:
    /**
     * @brief Draw inspector for all parameters in a graph
     */
    void Draw(ShaderGraph* graph);

    /**
     * @brief Get modified parameters
     */
    const std::unordered_map<std::string, std::variant<float, glm::vec2, glm::vec3, glm::vec4, int, bool>>&
    GetModifiedValues() const { return m_modifiedValues; }

private:
    std::unordered_map<std::string, std::variant<float, glm::vec2, glm::vec3, glm::vec4, int, bool>> m_modifiedValues;
};

} // namespace Nova
