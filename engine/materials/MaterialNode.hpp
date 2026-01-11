#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>

namespace Engine {

/**
 * @brief Material node types (50+ built-in nodes)
 */
enum class MaterialNodeType {
    // Input nodes
    UV,
    WorldPos,
    Normal,
    ViewDir,
    Tangent,
    Bitangent,
    Time,
    CustomInput,
    VertexColor,
    CameraPos,

    // Constant values
    FloatConstant,
    Vec2Constant,
    Vec3Constant,
    Vec4Constant,
    ColorConstant,

    // Math operations (scalar)
    Add,
    Subtract,
    Multiply,
    Divide,
    Power,
    Sqrt,
    Abs,
    Negate,
    Reciprocal,
    Frac,
    Floor,
    Ceil,
    Round,
    Sign,

    // Math operations (trigonometry)
    Sin,
    Cos,
    Tan,
    Asin,
    Acos,
    Atan,
    Atan2,

    // Math operations (exponential)
    Exp,
    Exp2,
    Log,
    Log2,

    // Math operations (vector)
    DotProduct,
    CrossProduct,
    Normalize,
    Length,
    Distance,
    Reflect,
    Refract,

    // Math operations (interpolation)
    Lerp,
    Mix,
    Smoothstep,
    Step,
    Clamp,
    Min,
    Max,
    Saturate,
    Remap,

    // Texture sampling
    TextureSample,
    TextureSampleLOD,
    TriplanarMapping,
    CubemapSample,

    // Procedural noise
    NoisePerlin,
    NoiseVoronoi,
    NoiseSimplex,
    NoiseWorley,
    NoiseFBM,
    NoiseTurbulence,

    // Color operations
    RGB_to_HSV,
    HSV_to_RGB,
    RGB_to_Luminance,
    Desaturate,
    Contrast,
    Brightness,
    ColorRamp,
    ColorMix,

    // Lighting functions
    Fresnel,
    FresnelSchlick,
    Lambert,
    BlinnPhong,
    GGX_BRDF,
    GGX_Distribution,
    SchlickGGX,
    SmithG,

    // Physics-based
    IOR_to_Reflectance,
    Temperature_to_RGB,
    Temperature_to_Emission,
    Blackbody,
    Luminosity_to_RGB,
    Scattering_Phase,
    HenyeyGreenstein,
    RayleighPhase,
    MiePhase,
    Dispersion,

    // UV operations
    UVTile,
    UVScale,
    UVOffset,
    UVRotate,
    UVPolar,
    UVRadial,

    // Utility nodes
    SplitVector,
    CombineVector,
    Swizzle,
    OneMinus,
    Append,

    // Output nodes
    OutputAlbedo,
    OutputNormal,
    OutputRoughness,
    OutputMetallic,
    OutputEmission,
    OutputIOR,
    OutputScattering,
    OutputOpacity,
    OutputAO,
};

/**
 * @brief Value types that can flow through node connections
 */
using MaterialNodeValue = std::variant<
    float,
    glm::vec2,
    glm::vec3,
    glm::vec4
>;

/**
 * @brief Pin connection point on a node
 */
struct MaterialNodePin {
    int id = 0;
    std::string name;
    enum class Type {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Color,
        Any
    } type = Type::Float;

    MaterialNodeValue defaultValue;
    bool isConnected = false;
    int connectedPinId = -1;
};

/**
 * @brief Connection between two pins
 */
struct MaterialConnection {
    int id = 0;
    int startPinId = 0;
    int endPinId = 0;
    int startNodeId = 0;
    int endNodeId = 0;
};

/**
 * @brief Material node in the graph
 */
class MaterialNode {
public:
    MaterialNode(MaterialNodeType type, const std::string& name);
    virtual ~MaterialNode() = default;

    // Node identity
    int id = 0;
    MaterialNodeType type;
    std::string name;
    glm::vec2 position{0.0f};  // Position in graph editor

    // Pins
    std::map<std::string, MaterialNodePin> inputs;
    std::map<std::string, MaterialNodePin> outputs;

    // Parameters (node-specific settings)
    std::map<std::string, float> floatParams;
    std::map<std::string, glm::vec2> vec2Params;
    std::map<std::string, glm::vec3> vec3Params;
    std::map<std::string, glm::vec4> vec4Params;
    std::map<std::string, std::string> stringParams;
    std::map<std::string, bool> boolParams;

    // Node evaluation
    virtual MaterialNodeValue Evaluate(
        const std::map<std::string, MaterialNodeValue>& inputValues
    ) const = 0;

    // GLSL code generation
    virtual std::string GenerateGLSL(
        const std::map<std::string, std::string>& inputVarNames,
        const std::string& outputVarName
    ) const = 0;

    // Pin management
    void AddInputPin(const std::string& name, MaterialNodePin::Type type, MaterialNodeValue defaultValue = 0.0f);
    void AddOutputPin(const std::string& name, MaterialNodePin::Type type);

    // Parameter getters
    float GetFloatParam(const std::string& name, float defaultValue = 0.0f) const;
    glm::vec2 GetVec2Param(const std::string& name, const glm::vec2& defaultValue = glm::vec2(0.0f)) const;
    glm::vec3 GetVec3Param(const std::string& name, const glm::vec3& defaultValue = glm::vec3(0.0f)) const;
    std::string GetStringParam(const std::string& name, const std::string& defaultValue = "") const;
    bool GetBoolParam(const std::string& name, bool defaultValue = false) const;

    // Serialization
    virtual void Serialize(nlohmann::json& json) const;
    virtual void Deserialize(const nlohmann::json& json);

protected:
    static int s_nextPinId;
};

// Node factory
class MaterialNodeFactory {
public:
    static std::unique_ptr<MaterialNode> CreateNode(MaterialNodeType type);
    static std::vector<MaterialNodeType> GetAllNodeTypes();
    static std::string GetNodeTypeName(MaterialNodeType type);
    static std::string GetNodeCategory(MaterialNodeType type);
};

// Concrete node implementations (selection of key nodes)

class UVNode : public MaterialNode {
public:
    UVNode();
    MaterialNodeValue Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const override;
    std::string GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const override;
};

class TextureSampleNode : public MaterialNode {
public:
    TextureSampleNode();
    MaterialNodeValue Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const override;
    std::string GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const override;
};

class AddNode : public MaterialNode {
public:
    AddNode();
    MaterialNodeValue Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const override;
    std::string GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const override;
};

class MultiplyNode : public MaterialNode {
public:
    MultiplyNode();
    MaterialNodeValue Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const override;
    std::string GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const override;
};

class LerpNode : public MaterialNode {
public:
    LerpNode();
    MaterialNodeValue Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const override;
    std::string GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const override;
};

class FresnelNode : public MaterialNode {
public:
    FresnelNode();
    MaterialNodeValue Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const override;
    std::string GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const override;
};

class TemperatureToRGBNode : public MaterialNode {
public:
    TemperatureToRGBNode();
    MaterialNodeValue Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const override;
    std::string GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const override;
};

class BlackbodyNode : public MaterialNode {
public:
    BlackbodyNode();
    MaterialNodeValue Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const override;
    std::string GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const override;
};

class NoisePerlinNode : public MaterialNode {
public:
    NoisePerlinNode();
    MaterialNodeValue Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const override;
    std::string GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const override;
};

class GGX_BRDFNode : public MaterialNode {
public:
    GGX_BRDFNode();
    MaterialNodeValue Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const override;
    std::string GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const override;
};

class DispersionNode : public MaterialNode {
public:
    DispersionNode();
    MaterialNodeValue Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const override;
    std::string GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const override;
};

class RGB_to_HSVNode : public MaterialNode {
public:
    RGB_to_HSVNode();
    MaterialNodeValue Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const override;
    std::string GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const override;
};

} // namespace Engine
