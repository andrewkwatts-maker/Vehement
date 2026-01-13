#pragma once

#include "MaterialNode.hpp"
#include "../graphics/PreviewRenderer.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <set>

namespace Engine {

// Forward declarations
class AdvancedMaterial;

/**
 * @brief Material graph containing nodes and connections
 */
class MaterialGraph {
public:
    MaterialGraph();
    ~MaterialGraph();

    // Node management
    int AddNode(std::unique_ptr<MaterialNode> node);
    void RemoveNode(int nodeId);
    MaterialNode* GetNode(int nodeId);
    const MaterialNode* GetNode(int nodeId) const;
    std::vector<MaterialNode*> GetAllNodes();

    // Connection management
    bool AddConnection(int startPinId, int endPinId);
    void RemoveConnection(int connectionId);
    void RemoveConnectionsFromPin(int pinId);
    MaterialConnection* GetConnection(int connectionId);
    std::vector<MaterialConnection> GetAllConnections() const;

    // Graph evaluation
    bool Validate() const;
    std::vector<std::string> GetValidationErrors() const;

    // Topological sort (for execution order)
    std::vector<int> GetTopologicalOrder() const;

    // GLSL code generation
    std::string CompileToGLSL() const;
    std::string GenerateFragmentShader() const;

    // Serialization
    void Save(const std::string& filepath) const;
    void Load(const std::string& filepath);

    nlohmann::json Serialize() const;
    void Deserialize(const nlohmann::json& json);

    // Graph properties
    std::string name = "Untitled Graph";
    glm::vec2 viewportOffset{0.0f};
    float viewportZoom = 1.0f;

    // Output nodes
    int GetOutputNode(const std::string& outputType) const;
    void SetOutputNode(const std::string& outputType, int nodeId);

private:
    std::map<int, std::unique_ptr<MaterialNode>> m_nodes;
    std::map<int, MaterialConnection> m_connections;
    std::map<std::string, int> m_outputNodes;  // OutputType -> NodeId

    int m_nextNodeId = 1;
    int m_nextConnectionId = 1;

    mutable std::vector<std::string> m_validationErrors;

    // Helper functions
    bool HasCycle() const;
    bool HasCycleUtil(int nodeId, std::set<int>& visited, std::set<int>& recStack) const;
    MaterialNode* FindNodeByPin(int pinId);
    const MaterialNode* FindNodeByPin(int pinId) const;
    std::vector<int> GetNodeInputs(int nodeId) const;
    std::vector<int> GetNodeOutputs(int nodeId) const;
};

/**
 * @brief Material graph compiler
 *
 * Compiles material graph to GLSL shader code
 */
class MaterialGraphCompiler {
public:
    MaterialGraphCompiler(const MaterialGraph* graph);

    // Compilation
    std::string Compile();
    std::string GenerateVertexShader();
    std::string GenerateFragmentShader();

    // Options
    struct CompilerOptions {
        bool includeComments = true;
        bool optimizeCode = true;
        bool generateDebugInfo = false;
        std::string shaderVersion = "450 core";
    } options;

private:
    const MaterialGraph* m_graph;

    std::string GenerateHeader();
    std::string GenerateUniforms();
    std::string GenerateInputs();
    std::string GenerateOutputs();
    std::string GenerateHelperFunctions();
    std::string GenerateMainFunction();

    std::string CompileNode(const MaterialNode* node, std::map<int, std::string>& varNames);
    std::string GetVariableName(int nodeId, const std::string& pinName);

    std::map<int, std::string> m_nodeOutputVars;
};

/**
 * @brief Material graph editor with visual UI
 */
class MaterialGraphEditor {
public:
    MaterialGraphEditor();
    ~MaterialGraphEditor();

    // Graph management
    void SetGraph(std::shared_ptr<MaterialGraph> graph);
    std::shared_ptr<MaterialGraph> GetGraph() const;

    void NewGraph();
    void LoadGraph(const std::string& filepath);
    void SaveGraph(const std::string& filepath);

    // UI rendering
    void Render();
    void RenderNodeEditor();
    void RenderNodePalette();
    void RenderProperties();
    void RenderToolbar();
    void RenderPreview();

    // Node operations
    void AddNode(MaterialNodeType type, const glm::vec2& position);
    void DeleteSelectedNodes();
    void DuplicateSelectedNodes();

    // Selection
    void SelectNode(int nodeId);
    void DeselectNode(int nodeId);
    void ClearSelection();
    bool IsNodeSelected(int nodeId) const;

    // Clipboard
    void CopySelectedNodes();
    void PasteNodes();

    // Undo/Redo
    void Undo();
    void Redo();
    bool CanUndo() const;
    bool CanRedo() const;

    // Compilation
    void CompileGraph();
    std::string GetCompiledShaderCode() const;

    // Preview
    void UpdatePreview();
    void SetPreviewMaterial(std::shared_ptr<AdvancedMaterial> material);

    // Settings
    struct EditorSettings {
        bool showGrid = true;
        bool showMinimap = false;
        bool autoCompile = true;
        float gridSize = 50.0f;
        glm::vec4 backgroundColor{0.15f, 0.15f, 0.15f, 1.0f};
        glm::vec4 gridColor{0.2f, 0.2f, 0.2f, 1.0f};
    } settings;

private:
    std::shared_ptr<MaterialGraph> m_graph;
    std::set<int> m_selectedNodes;
    std::string m_compiledShaderCode;

    // UI state
    int m_hoveredNode = -1;
    int m_hoveredPin = -1;
    bool m_isDraggingConnection = false;
    int m_dragStartPin = -1;

    // Node palette
    std::map<std::string, std::vector<MaterialNodeType>> m_nodePalette;
    std::string m_nodeSearchFilter;

    // Undo/Redo
    struct EditorCommand {
        virtual void Execute() = 0;
        virtual void Undo() = 0;
        virtual ~EditorCommand() = default;
    };
    std::vector<std::unique_ptr<EditorCommand>> m_undoStack;
    std::vector<std::unique_ptr<EditorCommand>> m_redoStack;

    // Commands
    class AddNodeCommand;
    class DeleteNodeCommand;
    class AddConnectionCommand;
    class DeleteConnectionCommand;

    // Helper functions
    void InitializeNodePalette();
    void RenderNode(MaterialNode* node);
    void RenderConnection(const MaterialConnection& conn);
    void HandleNodeInteraction();
    void HandleConnectionDragging();
    void ValidateGraph();

    // Preview
    struct PreviewData {
        std::shared_ptr<AdvancedMaterial> material;
        bool needsUpdate = false;
    } m_preview;
};

/**
 * @brief Node templates for common material setups
 */
class MaterialGraphTemplates {
public:
    static std::shared_ptr<MaterialGraph> CreateBasicPBR();
    static std::shared_ptr<MaterialGraph> CreateGlass();
    static std::shared_ptr<MaterialGraph> CreateEmissive();
    static std::shared_ptr<MaterialGraph> CreateSubsurface();
    static std::shared_ptr<MaterialGraph> CreateToon();
    static std::shared_ptr<MaterialGraph> CreateMetallic();
    static std::shared_ptr<MaterialGraph> CreateWood();
    static std::shared_ptr<MaterialGraph> CreateMarble();
    static std::shared_ptr<MaterialGraph> CreateCloth();
    static std::shared_ptr<MaterialGraph> CreateWater();

    static std::vector<std::string> GetTemplateNames();
    static std::shared_ptr<MaterialGraph> CreateTemplate(const std::string& name);
};

/**
 * @brief Material graph preview renderer
 *
 * This class is now a thin wrapper around Nova::PreviewRenderer for backward compatibility.
 * New code should use Nova::PreviewRenderer directly.
 *
 * @see Nova::PreviewRenderer
 */
class MaterialGraphPreviewRenderer {
public:
    MaterialGraphPreviewRenderer();
    ~MaterialGraphPreviewRenderer();

    void Initialize();
    void Render(std::shared_ptr<MaterialGraph> graph);
    void Resize(int width, int height);

    // Preview settings
    enum class PreviewShape {
        Sphere,
        Cube,
        Plane,
        Cylinder,
        Torus
    };

    PreviewShape previewShape = PreviewShape::Sphere;
    float lightIntensity = 1.0f;
    glm::vec3 lightColor{1.0f};
    float rotation = 0.0f;
    bool autoRotate = true;

    unsigned int GetPreviewTexture() const;

    /**
     * @brief Get the underlying PreviewRenderer instance
     *
     * Allows access to the full PreviewRenderer API for advanced use cases.
     *
     * @return Pointer to the internal PreviewRenderer
     */
    Nova::PreviewRenderer* GetPreviewRenderer() { return m_renderer.get(); }
    const Nova::PreviewRenderer* GetPreviewRenderer() const { return m_renderer.get(); }

private:
    std::unique_ptr<Nova::PreviewRenderer> m_renderer;
    int m_width = 512;
    int m_height = 512;

    // Helper to sync legacy settings with PreviewRenderer
    void SyncSettings();
};

} // namespace Engine
