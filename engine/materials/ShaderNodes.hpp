#pragma once

#include "ShaderGraph.hpp"
#include <cmath>

namespace Nova {

// ============================================================================
// INPUT NODES
// ============================================================================

/**
 * @brief Material output node - the final destination for all material properties
 */
class MaterialOutputNode : public ShaderNode {
public:
    MaterialOutputNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Output; }
    [[nodiscard]] const char* GetTypeName() const override { return "MaterialOutput"; }
    [[nodiscard]] const char* GetDescription() const override {
        return "Final material output connecting to the rendering pipeline";
    }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
    [[nodiscard]] bool SupportsPreview() const override { return false; }
};

/**
 * @brief Texture coordinate input
 */
class TexCoordNode : public ShaderNode {
public:
    TexCoordNode(int uvChannel = 0);
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Input; }
    [[nodiscard]] const char* GetTypeName() const override { return "TexCoord"; }
    [[nodiscard]] const char* GetDescription() const override { return "Texture coordinates"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;

    void SetUVChannel(int channel) { m_uvChannel = channel; }
    [[nodiscard]] int GetUVChannel() const { return m_uvChannel; }

private:
    int m_uvChannel = 0;
};

/**
 * @brief World position input
 */
class WorldPositionNode : public ShaderNode {
public:
    WorldPositionNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Input; }
    [[nodiscard]] const char* GetTypeName() const override { return "WorldPosition"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief World normal input
 */
class WorldNormalNode : public ShaderNode {
public:
    WorldNormalNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Input; }
    [[nodiscard]] const char* GetTypeName() const override { return "WorldNormal"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Vertex color input
 */
class VertexColorNode : public ShaderNode {
public:
    VertexColorNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Input; }
    [[nodiscard]] const char* GetTypeName() const override { return "VertexColor"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Camera/View direction
 */
class ViewDirectionNode : public ShaderNode {
public:
    ViewDirectionNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Input; }
    [[nodiscard]] const char* GetTypeName() const override { return "ViewDirection"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Time input for animations
 */
class TimeNode : public ShaderNode {
public:
    TimeNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Input; }
    [[nodiscard]] const char* GetTypeName() const override { return "Time"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Screen position
 */
class ScreenPositionNode : public ShaderNode {
public:
    ScreenPositionNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Input; }
    [[nodiscard]] const char* GetTypeName() const override { return "ScreenPosition"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

// ============================================================================
// PARAMETER NODES
// ============================================================================

/**
 * @brief Constant float value
 */
class FloatConstantNode : public ShaderNode {
public:
    FloatConstantNode(float value = 0.0f);
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Parameter; }
    [[nodiscard]] const char* GetTypeName() const override { return "FloatConstant"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;

    void SetValue(float value) { m_value = value; }
    [[nodiscard]] float GetValue() const { return m_value; }

private:
    float m_value;
};

/**
 * @brief Constant vector value
 */
class VectorConstantNode : public ShaderNode {
public:
    VectorConstantNode(const glm::vec4& value = glm::vec4(0.0f));
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Parameter; }
    [[nodiscard]] const char* GetTypeName() const override { return "VectorConstant"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;

    void SetValue(const glm::vec4& value) { m_value = value; }
    [[nodiscard]] const glm::vec4& GetValue() const { return m_value; }

private:
    glm::vec4 m_value;
};

/**
 * @brief Color constant with color picker
 */
class ColorConstantNode : public ShaderNode {
public:
    ColorConstantNode(const glm::vec4& color = glm::vec4(1.0f));
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Parameter; }
    [[nodiscard]] const char* GetTypeName() const override { return "ColorConstant"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;

    void SetColor(const glm::vec4& color) { m_color = color; }
    [[nodiscard]] const glm::vec4& GetColor() const { return m_color; }

private:
    glm::vec4 m_color;
};

/**
 * @brief Parameter exposed to material instances
 */
class ParameterNode : public ShaderNode {
public:
    ParameterNode(const std::string& paramName = "Parameter", ShaderDataType type = ShaderDataType::Float);
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Parameter; }
    [[nodiscard]] const char* GetTypeName() const override { return "Parameter"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;

    void SetParameterName(const std::string& name) { m_parameterName = name; }
    [[nodiscard]] const std::string& GetParameterName() const { return m_parameterName; }

    void SetParameterType(ShaderDataType type);
    [[nodiscard]] ShaderDataType GetParameterType() const { return m_parameterType; }

private:
    std::string m_parameterName;
    ShaderDataType m_parameterType;
};

// ============================================================================
// TEXTURE NODES
// ============================================================================

/**
 * @brief 2D texture sampler
 */
class Texture2DNode : public ShaderNode {
public:
    Texture2DNode(const std::string& textureName = "texture");
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Texture; }
    [[nodiscard]] const char* GetTypeName() const override { return "Texture2D"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;

    void SetTextureName(const std::string& name) { m_textureName = name; }
    [[nodiscard]] const std::string& GetTextureName() const { return m_textureName; }

    void SetDefaultTexturePath(const std::string& path) { m_defaultPath = path; }

private:
    std::string m_textureName;
    std::string m_defaultPath;
};

/**
 * @brief Normal map sampler with unpacking
 */
class NormalMapNode : public ShaderNode {
public:
    NormalMapNode(const std::string& textureName = "normalMap");
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Texture; }
    [[nodiscard]] const char* GetTypeName() const override { return "NormalMap"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;

    void SetStrength(float strength) { m_strength = strength; }

private:
    std::string m_textureName;
    float m_strength = 1.0f;
};

/**
 * @brief Cubemap sampler
 */
class TextureCubeNode : public ShaderNode {
public:
    TextureCubeNode(const std::string& textureName = "cubemap");
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Texture; }
    [[nodiscard]] const char* GetTypeName() const override { return "TextureCube"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;

private:
    std::string m_textureName;
};

// ============================================================================
// MATH BASIC NODES
// ============================================================================

/**
 * @brief Add two values
 */
class AddNode : public ShaderNode {
public:
    AddNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Add"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Subtract two values
 */
class SubtractNode : public ShaderNode {
public:
    SubtractNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Subtract"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Multiply two values
 */
class MultiplyNode : public ShaderNode {
public:
    MultiplyNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Multiply"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Divide two values
 */
class DivideNode : public ShaderNode {
public:
    DivideNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Divide"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief One minus value
 */
class OneMinusNode : public ShaderNode {
public:
    OneMinusNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "OneMinus"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Absolute value
 */
class AbsNode : public ShaderNode {
public:
    AbsNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Abs"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Negate value
 */
class NegateNode : public ShaderNode {
public:
    NegateNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Negate"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Min of two values
 */
class MinNode : public ShaderNode {
public:
    MinNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Min"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Max of two values
 */
class MaxNode : public ShaderNode {
public:
    MaxNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Max"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Clamp value between min and max
 */
class ClampNode : public ShaderNode {
public:
    ClampNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Clamp"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Saturate (clamp 0-1)
 */
class SaturateNode : public ShaderNode {
public:
    SaturateNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Saturate"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Floor
 */
class FloorNode : public ShaderNode {
public:
    FloorNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Floor"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Ceil
 */
class CeilNode : public ShaderNode {
public:
    CeilNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Ceil"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Round
 */
class RoundNode : public ShaderNode {
public:
    RoundNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Round"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Frac (fractional part)
 */
class FracNode : public ShaderNode {
public:
    FracNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Frac"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Modulo
 */
class ModNode : public ShaderNode {
public:
    ModNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathBasic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Mod"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

// ============================================================================
// MATH ADVANCED NODES
// ============================================================================

/**
 * @brief Power (x^y)
 */
class PowerNode : public ShaderNode {
public:
    PowerNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathAdvanced; }
    [[nodiscard]] const char* GetTypeName() const override { return "Power"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Square root
 */
class SqrtNode : public ShaderNode {
public:
    SqrtNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathAdvanced; }
    [[nodiscard]] const char* GetTypeName() const override { return "Sqrt"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Inverse square root
 */
class InverseSqrtNode : public ShaderNode {
public:
    InverseSqrtNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathAdvanced; }
    [[nodiscard]] const char* GetTypeName() const override { return "InverseSqrt"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Natural logarithm
 */
class LogNode : public ShaderNode {
public:
    LogNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathAdvanced; }
    [[nodiscard]] const char* GetTypeName() const override { return "Log"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Log base 2
 */
class Log2Node : public ShaderNode {
public:
    Log2Node();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathAdvanced; }
    [[nodiscard]] const char* GetTypeName() const override { return "Log2"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Exponential (e^x)
 */
class ExpNode : public ShaderNode {
public:
    ExpNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathAdvanced; }
    [[nodiscard]] const char* GetTypeName() const override { return "Exp"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Exp2 (2^x)
 */
class Exp2Node : public ShaderNode {
public:
    Exp2Node();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathAdvanced; }
    [[nodiscard]] const char* GetTypeName() const override { return "Exp2"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

// ============================================================================
// MATH TRIG NODES
// ============================================================================

/**
 * @brief Sine
 */
class SinNode : public ShaderNode {
public:
    SinNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathTrig; }
    [[nodiscard]] const char* GetTypeName() const override { return "Sin"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Cosine
 */
class CosNode : public ShaderNode {
public:
    CosNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathTrig; }
    [[nodiscard]] const char* GetTypeName() const override { return "Cos"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Tangent
 */
class TanNode : public ShaderNode {
public:
    TanNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathTrig; }
    [[nodiscard]] const char* GetTypeName() const override { return "Tan"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Arc sine
 */
class AsinNode : public ShaderNode {
public:
    AsinNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathTrig; }
    [[nodiscard]] const char* GetTypeName() const override { return "Asin"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Arc cosine
 */
class AcosNode : public ShaderNode {
public:
    AcosNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathTrig; }
    [[nodiscard]] const char* GetTypeName() const override { return "Acos"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Arc tangent
 */
class AtanNode : public ShaderNode {
public:
    AtanNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathTrig; }
    [[nodiscard]] const char* GetTypeName() const override { return "Atan"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Atan2
 */
class Atan2Node : public ShaderNode {
public:
    Atan2Node();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathTrig; }
    [[nodiscard]] const char* GetTypeName() const override { return "Atan2"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

// ============================================================================
// MATH VECTOR NODES
// ============================================================================

/**
 * @brief Dot product
 */
class DotNode : public ShaderNode {
public:
    DotNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathVector; }
    [[nodiscard]] const char* GetTypeName() const override { return "Dot"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Cross product
 */
class CrossNode : public ShaderNode {
public:
    CrossNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathVector; }
    [[nodiscard]] const char* GetTypeName() const override { return "Cross"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Normalize vector
 */
class NormalizeNode : public ShaderNode {
public:
    NormalizeNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathVector; }
    [[nodiscard]] const char* GetTypeName() const override { return "Normalize"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Vector length
 */
class LengthNode : public ShaderNode {
public:
    LengthNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathVector; }
    [[nodiscard]] const char* GetTypeName() const override { return "Length"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Distance between two points
 */
class DistanceNode : public ShaderNode {
public:
    DistanceNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathVector; }
    [[nodiscard]] const char* GetTypeName() const override { return "Distance"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Reflect vector
 */
class ReflectNode : public ShaderNode {
public:
    ReflectNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathVector; }
    [[nodiscard]] const char* GetTypeName() const override { return "Reflect"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Refract vector
 */
class RefractNode : public ShaderNode {
public:
    RefractNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathVector; }
    [[nodiscard]] const char* GetTypeName() const override { return "Refract"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Transform vector by matrix
 */
class TransformNode : public ShaderNode {
public:
    TransformNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathVector; }
    [[nodiscard]] const char* GetTypeName() const override { return "Transform"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

// ============================================================================
// INTERPOLATION NODES
// ============================================================================

/**
 * @brief Linear interpolation
 */
class LerpNode : public ShaderNode {
public:
    LerpNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathInterpolation; }
    [[nodiscard]] const char* GetTypeName() const override { return "Lerp"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Smooth step
 */
class SmoothStepNode : public ShaderNode {
public:
    SmoothStepNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathInterpolation; }
    [[nodiscard]] const char* GetTypeName() const override { return "SmoothStep"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Step function
 */
class StepNode : public ShaderNode {
public:
    StepNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathInterpolation; }
    [[nodiscard]] const char* GetTypeName() const override { return "Step"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Inverse lerp (get T from value)
 */
class InverseLerpNode : public ShaderNode {
public:
    InverseLerpNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathInterpolation; }
    [[nodiscard]] const char* GetTypeName() const override { return "InverseLerp"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Remap value from one range to another
 */
class RemapNode : public ShaderNode {
public:
    RemapNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::MathInterpolation; }
    [[nodiscard]] const char* GetTypeName() const override { return "Remap"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

// ============================================================================
// UTILITY NODES
// ============================================================================

/**
 * @brief Swizzle components
 */
class SwizzleNode : public ShaderNode {
public:
    SwizzleNode(const std::string& mask = "xyzw");
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Utility; }
    [[nodiscard]] const char* GetTypeName() const override { return "Swizzle"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;

    void SetMask(const std::string& mask) { m_mask = mask; }
    [[nodiscard]] const std::string& GetMask() const { return m_mask; }

private:
    std::string m_mask;
};

/**
 * @brief Split vector into components
 */
class SplitNode : public ShaderNode {
public:
    SplitNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Utility; }
    [[nodiscard]] const char* GetTypeName() const override { return "Split"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Combine components into vector
 */
class CombineNode : public ShaderNode {
public:
    CombineNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Utility; }
    [[nodiscard]] const char* GetTypeName() const override { return "Combine"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Append value to vector
 */
class AppendNode : public ShaderNode {
public:
    AppendNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Utility; }
    [[nodiscard]] const char* GetTypeName() const override { return "Append"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief DDX (screen-space derivative)
 */
class DDXNode : public ShaderNode {
public:
    DDXNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Utility; }
    [[nodiscard]] const char* GetTypeName() const override { return "DDX"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief DDY (screen-space derivative)
 */
class DDYNode : public ShaderNode {
public:
    DDYNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Utility; }
    [[nodiscard]] const char* GetTypeName() const override { return "DDY"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

// ============================================================================
// LOGIC NODES
// ============================================================================

/**
 * @brief If/Branch node
 */
class IfNode : public ShaderNode {
public:
    IfNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Logic; }
    [[nodiscard]] const char* GetTypeName() const override { return "If"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Comparison node
 */
class CompareNode : public ShaderNode {
public:
    enum class Operation { Equal, NotEqual, Greater, GreaterEqual, Less, LessEqual };

    CompareNode(Operation op = Operation::Greater);
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Logic; }
    [[nodiscard]] const char* GetTypeName() const override { return "Compare"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;

    void SetOperation(Operation op) { m_operation = op; }

private:
    Operation m_operation;
};

// ============================================================================
// COLOR NODES
// ============================================================================

/**
 * @brief Blend two colors
 */
class BlendNode : public ShaderNode {
public:
    enum class Mode { Normal, Multiply, Screen, Overlay, Add, Subtract, Difference };

    BlendNode(Mode mode = Mode::Normal);
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Color; }
    [[nodiscard]] const char* GetTypeName() const override { return "Blend"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;

    void SetMode(Mode mode) { m_mode = mode; }

private:
    Mode m_mode;
};

/**
 * @brief Hue/Saturation/Value adjustment
 */
class HSVNode : public ShaderNode {
public:
    HSVNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Color; }
    [[nodiscard]] const char* GetTypeName() const override { return "HSV"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Convert RGB to HSV
 */
class RGBToHSVNode : public ShaderNode {
public:
    RGBToHSVNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Color; }
    [[nodiscard]] const char* GetTypeName() const override { return "RGBToHSV"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Convert HSV to RGB
 */
class HSVToRGBNode : public ShaderNode {
public:
    HSVToRGBNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Color; }
    [[nodiscard]] const char* GetTypeName() const override { return "HSVToRGB"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Contrast adjustment
 */
class ContrastNode : public ShaderNode {
public:
    ContrastNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Color; }
    [[nodiscard]] const char* GetTypeName() const override { return "Contrast"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Posterize (reduce color levels)
 */
class PosterizeNode : public ShaderNode {
public:
    PosterizeNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Color; }
    [[nodiscard]] const char* GetTypeName() const override { return "Posterize"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

/**
 * @brief Grayscale/Desaturate
 */
class GrayscaleNode : public ShaderNode {
public:
    GrayscaleNode();
    [[nodiscard]] NodeCategory GetCategory() const override { return NodeCategory::Color; }
    [[nodiscard]] const char* GetTypeName() const override { return "Grayscale"; }
    [[nodiscard]] std::string GenerateCode(MaterialCompiler& compiler) const override;
};

} // namespace Nova
