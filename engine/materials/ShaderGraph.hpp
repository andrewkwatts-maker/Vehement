#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <variant>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class ShaderNode;
class ShaderGraph;
class MaterialCompiler;

// ============================================================================
// Data Types for Shader System
// ============================================================================

/**
 * @brief Shader data types
 */
enum class ShaderDataType {
    Float,
    Vec2,
    Vec3,
    Vec4,
    Int,
    IVec2,
    IVec3,
    IVec4,
    Bool,
    Mat3,
    Mat4,
    Sampler2D,
    SamplerCube,
    Sampler3D,
    Void
};

/**
 * @brief Get GLSL type string
 */
const char* ShaderDataTypeToGLSL(ShaderDataType type);

/**
 * @brief Get default value for type
 */
std::string ShaderDataTypeDefaultValue(ShaderDataType type);

/**
 * @brief Check if types are compatible for connection
 */
bool AreTypesCompatible(ShaderDataType from, ShaderDataType to);

/**
 * @brief Get component count for type
 */
int GetComponentCount(ShaderDataType type);

// ============================================================================
// Node Pin (Input/Output connection point)
// ============================================================================

/**
 * @brief Pin direction
 */
enum class PinDirection {
    Input,
    Output
};

/**
 * @brief A connection point on a node
 */
struct ShaderPin {
    std::string name;
    std::string displayName;
    ShaderDataType type = ShaderDataType::Float;
    PinDirection direction = PinDirection::Input;

    // Default value for inputs (when not connected)
    std::variant<float, glm::vec2, glm::vec3, glm::vec4, int, bool> defaultValue;

    // For connected pins
    std::weak_ptr<ShaderNode> connectedNode;
    std::string connectedPinName;

    // Visual properties
    glm::vec4 color{1.0f};
    bool hidden = false;

    // Unique ID for this pin instance
    uint64_t id = 0;

    [[nodiscard]] bool IsConnected() const { return !connectedNode.expired(); }
};

// ============================================================================
// Node Categories
// ============================================================================

enum class NodeCategory {
    // Inputs
    Input,              // Material inputs (UV, position, normal, etc.)
    Parameter,          // User-defined parameters
    Texture,            // Texture sampling

    // Math
    MathBasic,          // Add, Subtract, Multiply, Divide
    MathAdvanced,       // Power, Sqrt, Log, Exp
    MathTrig,           // Sin, Cos, Tan, etc.
    MathVector,         // Dot, Cross, Normalize, etc.
    MathInterpolation,  // Lerp, SmoothStep, etc.

    // Utility
    Utility,            // Swizzle, Combine, Split, etc.
    Logic,              // If, Compare, etc.

    // Procedural
    Noise,              // Perlin, Simplex, Voronoi, etc.
    Pattern,            // Checkerboard, Gradient, etc.

    // Color
    Color,              // Color operations

    // Output
    Output,             // Material outputs

    // Custom
    Custom,             // User-defined nodes
    SubGraph            // Embedded sub-graphs
};

const char* NodeCategoryToString(NodeCategory category);

// ============================================================================
// Base Shader Node
// ============================================================================

/**
 * @brief Unique node identifier
 */
using NodeId = uint64_t;

/**
 * @brief Base class for all shader nodes
 */
class ShaderNode : public std::enable_shared_from_this<ShaderNode> {
public:
    using Ptr = std::shared_ptr<ShaderNode>;

    ShaderNode(const std::string& name);
    virtual ~ShaderNode() = default;

    // Identity
    [[nodiscard]] NodeId GetId() const { return m_id; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }
    [[nodiscard]] const std::string& GetDisplayName() const { return m_displayName; }
    void SetDisplayName(const std::string& name) { m_displayName = name; }

    // Category
    [[nodiscard]] virtual NodeCategory GetCategory() const = 0;
    [[nodiscard]] virtual const char* GetTypeName() const = 0;
    [[nodiscard]] virtual const char* GetDescription() const { return ""; }

    // Pins
    [[nodiscard]] const std::vector<ShaderPin>& GetInputs() const { return m_inputs; }
    [[nodiscard]] const std::vector<ShaderPin>& GetOutputs() const { return m_outputs; }
    [[nodiscard]] ShaderPin* GetInput(const std::string& name);
    [[nodiscard]] ShaderPin* GetOutput(const std::string& name);
    [[nodiscard]] const ShaderPin* GetInput(const std::string& name) const;
    [[nodiscard]] const ShaderPin* GetOutput(const std::string& name) const;

    // Connections
    bool Connect(const std::string& inputPin, Ptr sourceNode, const std::string& outputPin);
    void Disconnect(const std::string& inputPin);
    void DisconnectAll();

    // Code generation
    [[nodiscard]] virtual std::string GenerateCode(MaterialCompiler& compiler) const = 0;

    // Preview (optional)
    [[nodiscard]] virtual bool SupportsPreview() const { return true; }
    [[nodiscard]] virtual std::string GeneratePreviewCode(MaterialCompiler& compiler) const;

    // Visual position in editor
    void SetPosition(const glm::vec2& pos) { m_position = pos; }
    [[nodiscard]] const glm::vec2& GetPosition() const { return m_position; }

    // Serialization
    [[nodiscard]] virtual std::string ToJson() const;
    virtual void FromJson(const std::string& json);

    // Node state
    void SetExpanded(bool expanded) { m_expanded = expanded; }
    [[nodiscard]] bool IsExpanded() const { return m_expanded; }

    void SetComment(const std::string& comment) { m_comment = comment; }
    [[nodiscard]] const std::string& GetComment() const { return m_comment; }

protected:
    void AddInput(const std::string& name, ShaderDataType type,
                  const std::string& displayName = "");
    void AddOutput(const std::string& name, ShaderDataType type,
                   const std::string& displayName = "");

    template<typename T>
    void SetInputDefault(const std::string& name, const T& value) {
        if (auto* pin = GetInput(name)) {
            pin->defaultValue = value;
        }
    }

    // Helper to get input variable name in generated code
    [[nodiscard]] std::string GetInputValue(const std::string& name, MaterialCompiler& compiler) const;

    NodeId m_id;
    std::string m_name;
    std::string m_displayName;
    std::string m_comment;

    std::vector<ShaderPin> m_inputs;
    std::vector<ShaderPin> m_outputs;

    glm::vec2 m_position{0, 0};
    bool m_expanded = true;

    static NodeId s_nextId;
};

// ============================================================================
// Material Output Types
// ============================================================================

/**
 * @brief Standard material output channels
 */
enum class MaterialOutput {
    // PBR outputs
    BaseColor,
    Metallic,
    Roughness,
    Normal,
    Emissive,
    EmissiveStrength,
    AmbientOcclusion,
    Opacity,
    OpacityMask,

    // Advanced
    Subsurface,
    SubsurfaceColor,
    Specular,
    Anisotropy,
    AnisotropyRotation,

    // Special
    WorldPositionOffset,
    WorldDisplacement,
    TessellationMultiplier,

    // SDF specific
    SDFDistance,
    SDFGradient
};

const char* MaterialOutputToString(MaterialOutput output);
const char* MaterialOutputToGLSL(MaterialOutput output);

// ============================================================================
// Shader Graph
// ============================================================================

/**
 * @brief Material domain (what surface type this material is for)
 */
enum class MaterialDomain {
    Surface,        // Standard 3D surface
    PostProcess,    // Post-processing effect
    Decal,          // Decal projection
    UI,             // UI elements
    Volume,         // Volumetric effects
    SDF             // Signed distance field
};

/**
 * @brief Blend mode for the material
 */
enum class BlendMode {
    Opaque,
    Masked,
    Translucent,
    Additive,
    Modulate
};

/**
 * @brief Shading model
 */
enum class ShadingModel {
    Unlit,
    DefaultLit,
    Subsurface,
    ClearCoat,
    Hair,
    Eye,
    TwoSidedFoliage,
    Cloth
};

/**
 * @brief Complete shader graph representing a material
 */
class ShaderGraph {
public:
    ShaderGraph(const std::string& name = "Material");
    ~ShaderGraph() = default;

    // Name
    void SetName(const std::string& name) { m_name = name; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }

    // Material properties
    void SetDomain(MaterialDomain domain) { m_domain = domain; }
    [[nodiscard]] MaterialDomain GetDomain() const { return m_domain; }

    void SetBlendMode(BlendMode mode) { m_blendMode = mode; }
    [[nodiscard]] BlendMode GetBlendMode() const { return m_blendMode; }

    void SetShadingModel(ShadingModel model) { m_shadingModel = model; }
    [[nodiscard]] ShadingModel GetShadingModel() const { return m_shadingModel; }

    void SetTwoSided(bool twoSided) { m_twoSided = twoSided; }
    [[nodiscard]] bool IsTwoSided() const { return m_twoSided; }

    // Node management
    void AddNode(ShaderNode::Ptr node);
    void RemoveNode(NodeId id);
    [[nodiscard]] ShaderNode::Ptr GetNode(NodeId id) const;
    [[nodiscard]] const std::vector<ShaderNode::Ptr>& GetNodes() const { return m_nodes; }

    // Find nodes by type
    template<typename T>
    std::vector<std::shared_ptr<T>> GetNodesOfType() const;

    // Output node
    [[nodiscard]] ShaderNode::Ptr GetOutputNode() const { return m_outputNode; }

    // Connections
    bool Connect(NodeId fromNode, const std::string& fromPin,
                 NodeId toNode, const std::string& toPin);
    void Disconnect(NodeId toNode, const std::string& toPin);

    // Validation
    [[nodiscard]] bool Validate(std::vector<std::string>& errors) const;
    [[nodiscard]] bool HasCycle() const;

    // Compilation
    [[nodiscard]] std::string Compile() const;
    [[nodiscard]] bool CompileToFiles(const std::string& vertexPath,
                                       const std::string& fragmentPath) const;

    // Serialization
    [[nodiscard]] std::string ToJson() const;
    static std::shared_ptr<ShaderGraph> FromJson(const std::string& json);
    bool SaveToFile(const std::string& path) const;
    static std::shared_ptr<ShaderGraph> LoadFromFile(const std::string& path);

    // Parameters (exposed to material instances)
    struct Parameter {
        std::string name;
        std::string displayName;
        std::string group;
        ShaderDataType type;
        std::variant<float, glm::vec2, glm::vec3, glm::vec4, int, bool, std::string> defaultValue;
        float minValue = 0.0f;
        float maxValue = 1.0f;
        bool isTexture = false;
    };

    void AddParameter(const Parameter& param);
    void RemoveParameter(const std::string& name);
    [[nodiscard]] const std::vector<Parameter>& GetParameters() const { return m_parameters; }
    [[nodiscard]] Parameter* GetParameter(const std::string& name);

    // Sub-graphs
    void AddSubGraph(const std::string& name, std::shared_ptr<ShaderGraph> subGraph);
    [[nodiscard]] std::shared_ptr<ShaderGraph> GetSubGraph(const std::string& name) const;

    // Comments/Groups
    struct NodeGroup {
        std::string name;
        glm::vec4 color;
        glm::vec2 position;
        glm::vec2 size;
        std::vector<NodeId> nodes;
    };

    void AddGroup(const NodeGroup& group);
    [[nodiscard]] const std::vector<NodeGroup>& GetGroups() const { return m_groups; }

private:
    std::string m_name;
    MaterialDomain m_domain = MaterialDomain::Surface;
    BlendMode m_blendMode = BlendMode::Opaque;
    ShadingModel m_shadingModel = ShadingModel::DefaultLit;
    bool m_twoSided = false;

    std::vector<ShaderNode::Ptr> m_nodes;
    ShaderNode::Ptr m_outputNode;

    std::vector<Parameter> m_parameters;
    std::unordered_map<std::string, std::shared_ptr<ShaderGraph>> m_subGraphs;
    std::vector<NodeGroup> m_groups;
};

// ============================================================================
// Material Compiler
// ============================================================================

/**
 * @brief Compiles a shader graph to GLSL code
 */
class MaterialCompiler {
public:
    MaterialCompiler(const ShaderGraph& graph);

    // Compile the full shader
    [[nodiscard]] std::string CompileVertexShader() const;
    [[nodiscard]] std::string CompileFragmentShader() const;
    [[nodiscard]] std::string CompileGeometryShader() const;  // Optional

    // Compile specific sections
    [[nodiscard]] std::string GenerateUniforms() const;
    [[nodiscard]] std::string GenerateVaryings() const;
    [[nodiscard]] std::string GenerateFunctions() const;
    [[nodiscard]] std::string GenerateMainBody() const;

    // Variable management during compilation
    std::string AllocateVariable(ShaderDataType type, const std::string& prefix = "v");
    std::string GetNodeOutputVariable(NodeId nodeId, const std::string& outputName) const;
    void SetNodeOutputVariable(NodeId nodeId, const std::string& outputName, const std::string& varName);

    // Add code to output
    void AddLine(const std::string& code);
    void AddFunction(const std::string& signature, const std::string& body);
    void AddUniform(const std::string& type, const std::string& name);
    void AddVarying(const std::string& type, const std::string& name);

    // Track what's been compiled
    bool IsNodeCompiled(NodeId nodeId) const;
    void MarkNodeCompiled(NodeId nodeId);

    // Get compilation order (topologically sorted)
    [[nodiscard]] std::vector<ShaderNode::Ptr> GetCompilationOrder() const;

    // Error reporting
    void AddError(const std::string& error);
    void AddWarning(const std::string& warning);
    [[nodiscard]] const std::vector<std::string>& GetErrors() const { return m_errors; }
    [[nodiscard]] const std::vector<std::string>& GetWarnings() const { return m_warnings; }

private:
    const ShaderGraph& m_graph;

    mutable int m_variableCounter = 0;
    mutable std::unordered_map<std::string, std::string> m_nodeOutputVars;
    mutable std::unordered_set<NodeId> m_compiledNodes;

    mutable std::vector<std::string> m_uniforms;
    mutable std::vector<std::string> m_varyings;
    mutable std::vector<std::string> m_functions;
    mutable std::vector<std::string> m_mainBody;

    mutable std::vector<std::string> m_errors;
    mutable std::vector<std::string> m_warnings;
};

// ============================================================================
// Node Factory
// ============================================================================

/**
 * @brief Factory for creating shader nodes
 */
class ShaderNodeFactory {
public:
    using CreatorFunc = std::function<ShaderNode::Ptr()>;

    static ShaderNodeFactory& Instance();

    // Registration
    void RegisterNode(const std::string& typeName, NodeCategory category,
                      const std::string& displayName, CreatorFunc creator);

    // Creation
    [[nodiscard]] ShaderNode::Ptr Create(const std::string& typeName) const;

    // Discovery
    [[nodiscard]] std::vector<std::string> GetNodeTypes() const;
    [[nodiscard]] std::vector<std::string> GetNodeTypesInCategory(NodeCategory category) const;
    [[nodiscard]] NodeCategory GetNodeCategory(const std::string& typeName) const;
    [[nodiscard]] std::string GetNodeDisplayName(const std::string& typeName) const;

    // Built-in registration
    void RegisterBuiltinNodes();

private:
    ShaderNodeFactory() = default;

    struct NodeInfo {
        std::string displayName;
        NodeCategory category;
        CreatorFunc creator;
    };

    std::unordered_map<std::string, NodeInfo> m_nodeTypes;
};

// Macro for easy node registration
#define REGISTER_SHADER_NODE(TypeName, Category, DisplayName) \
    ShaderNodeFactory::Instance().RegisterNode(#TypeName, Category, DisplayName, \
        []() { return std::make_shared<TypeName>(); })

} // namespace Nova
