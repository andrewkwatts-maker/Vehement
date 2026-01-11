#include "ShaderNodes.hpp"
#include <sstream>

namespace Nova {

// ============================================================================
// OUTPUT NODE
// ============================================================================

MaterialOutputNode::MaterialOutputNode() : ShaderNode("MaterialOutput") {
    m_displayName = "Material Output";

    // PBR inputs
    AddInput("BaseColor", ShaderDataType::Vec3, "Base Color");
    AddInput("Metallic", ShaderDataType::Float, "Metallic");
    AddInput("Roughness", ShaderDataType::Float, "Roughness");
    AddInput("Normal", ShaderDataType::Vec3, "Normal");
    AddInput("Emissive", ShaderDataType::Vec3, "Emissive");
    AddInput("EmissiveStrength", ShaderDataType::Float, "Emissive Strength");
    AddInput("AmbientOcclusion", ShaderDataType::Float, "Ambient Occlusion");
    AddInput("Opacity", ShaderDataType::Float, "Opacity");

    SetInputDefault("BaseColor", glm::vec3(0.5f));
    SetInputDefault("Metallic", 0.0f);
    SetInputDefault("Roughness", 0.5f);
    SetInputDefault("AmbientOcclusion", 1.0f);
    SetInputDefault("Opacity", 1.0f);
    SetInputDefault("EmissiveStrength", 1.0f);
}

std::string MaterialOutputNode::GenerateCode(MaterialCompiler& compiler) const {
    std::stringstream ss;

    // Get connected values or defaults
    const auto* baseColor = GetInput("BaseColor");
    if (baseColor && baseColor->IsConnected()) {
        ss << "mat_baseColor = " << GetInputValue("BaseColor", compiler) << ";\n";
    }

    const auto* metallic = GetInput("Metallic");
    if (metallic && metallic->IsConnected()) {
        ss << "    mat_metallic = " << GetInputValue("Metallic", compiler) << ";\n";
    }

    const auto* roughness = GetInput("Roughness");
    if (roughness && roughness->IsConnected()) {
        ss << "    mat_roughness = " << GetInputValue("Roughness", compiler) << ";\n";
    }

    const auto* normal = GetInput("Normal");
    if (normal && normal->IsConnected()) {
        ss << "    mat_normal = " << GetInputValue("Normal", compiler) << ";\n";
    }

    const auto* emissive = GetInput("Emissive");
    if (emissive && emissive->IsConnected()) {
        ss << "    mat_emissive = " << GetInputValue("Emissive", compiler) << ";\n";
    }

    const auto* ao = GetInput("AmbientOcclusion");
    if (ao && ao->IsConnected()) {
        ss << "    mat_ao = " << GetInputValue("AmbientOcclusion", compiler) << ";\n";
    }

    const auto* opacity = GetInput("Opacity");
    if (opacity && opacity->IsConnected()) {
        ss << "    mat_opacity = " << GetInputValue("Opacity", compiler) << ";\n";
    }

    return ss.str();
}

// ============================================================================
// INPUT NODES
// ============================================================================

TexCoordNode::TexCoordNode(int uvChannel) : ShaderNode("TexCoord"), m_uvChannel(uvChannel) {
    m_displayName = "Texture Coordinates";
    AddOutput("UV", ShaderDataType::Vec2);
    AddOutput("U", ShaderDataType::Float);
    AddOutput("V", ShaderDataType::Float);
}

std::string TexCoordNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvVar = compiler.AllocateVariable(ShaderDataType::Vec2, "uv");
    compiler.SetNodeOutputVariable(m_id, "UV", uvVar);
    compiler.SetNodeOutputVariable(m_id, "U", uvVar + ".x");
    compiler.SetNodeOutputVariable(m_id, "V", uvVar + ".y");
    return "vec2 " + uvVar + " = v_TexCoord;";
}

WorldPositionNode::WorldPositionNode() : ShaderNode("WorldPosition") {
    m_displayName = "World Position";
    AddOutput("Position", ShaderDataType::Vec3);
    AddOutput("X", ShaderDataType::Float);
    AddOutput("Y", ShaderDataType::Float);
    AddOutput("Z", ShaderDataType::Float);
}

std::string WorldPositionNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec3, "worldPos");
    compiler.SetNodeOutputVariable(m_id, "Position", var);
    compiler.SetNodeOutputVariable(m_id, "X", var + ".x");
    compiler.SetNodeOutputVariable(m_id, "Y", var + ".y");
    compiler.SetNodeOutputVariable(m_id, "Z", var + ".z");
    return "vec3 " + var + " = v_WorldPos;";
}

WorldNormalNode::WorldNormalNode() : ShaderNode("WorldNormal") {
    m_displayName = "World Normal";
    AddOutput("Normal", ShaderDataType::Vec3);
}

std::string WorldNormalNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec3, "normal");
    compiler.SetNodeOutputVariable(m_id, "Normal", var);
    return "vec3 " + var + " = normalize(v_Normal);";
}

VertexColorNode::VertexColorNode() : ShaderNode("VertexColor") {
    m_displayName = "Vertex Color";
    AddOutput("Color", ShaderDataType::Vec4);
    AddOutput("RGB", ShaderDataType::Vec3);
    AddOutput("R", ShaderDataType::Float);
    AddOutput("G", ShaderDataType::Float);
    AddOutput("B", ShaderDataType::Float);
    AddOutput("A", ShaderDataType::Float);
}

std::string VertexColorNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "vcolor");
    compiler.SetNodeOutputVariable(m_id, "Color", var);
    compiler.SetNodeOutputVariable(m_id, "RGB", var + ".rgb");
    compiler.SetNodeOutputVariable(m_id, "R", var + ".r");
    compiler.SetNodeOutputVariable(m_id, "G", var + ".g");
    compiler.SetNodeOutputVariable(m_id, "B", var + ".b");
    compiler.SetNodeOutputVariable(m_id, "A", var + ".a");
    return "vec4 " + var + " = v_Color;";
}

ViewDirectionNode::ViewDirectionNode() : ShaderNode("ViewDirection") {
    m_displayName = "View Direction";
    AddOutput("Direction", ShaderDataType::Vec3);
}

std::string ViewDirectionNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec3, "viewDir");
    compiler.SetNodeOutputVariable(m_id, "Direction", var);
    return "vec3 " + var + " = normalize(u_CameraPos - v_WorldPos);";
}

TimeNode::TimeNode() : ShaderNode("Time") {
    m_displayName = "Time";
    AddOutput("Time", ShaderDataType::Float);
    AddOutput("SinTime", ShaderDataType::Float);
    AddOutput("CosTime", ShaderDataType::Float);
}

std::string TimeNode::GenerateCode(MaterialCompiler& compiler) const {
    compiler.SetNodeOutputVariable(m_id, "Time", "u_Time");
    compiler.SetNodeOutputVariable(m_id, "SinTime", "sin(u_Time)");
    compiler.SetNodeOutputVariable(m_id, "CosTime", "cos(u_Time)");
    return "";  // No new variable needed
}

ScreenPositionNode::ScreenPositionNode() : ShaderNode("ScreenPosition") {
    m_displayName = "Screen Position";
    AddOutput("Position", ShaderDataType::Vec2);
}

std::string ScreenPositionNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec2, "screenPos");
    compiler.SetNodeOutputVariable(m_id, "Position", var);
    return "vec2 " + var + " = gl_FragCoord.xy / u_Resolution;";
}

// ============================================================================
// PARAMETER NODES
// ============================================================================

FloatConstantNode::FloatConstantNode(float value) : ShaderNode("FloatConstant"), m_value(value) {
    m_displayName = "Float";
    AddOutput("Value", ShaderDataType::Float);
}

std::string FloatConstantNode::GenerateCode(MaterialCompiler& compiler) const {
    compiler.SetNodeOutputVariable(m_id, "Value", std::to_string(m_value));
    return "";
}

VectorConstantNode::VectorConstantNode(const glm::vec4& value) : ShaderNode("VectorConstant"), m_value(value) {
    m_displayName = "Vector";
    AddOutput("RGBA", ShaderDataType::Vec4);
    AddOutput("RGB", ShaderDataType::Vec3);
    AddOutput("RG", ShaderDataType::Vec2);
    AddOutput("R", ShaderDataType::Float);
}

std::string VectorConstantNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string vec4Str = "vec4(" + std::to_string(m_value.r) + ", " + std::to_string(m_value.g) + ", " +
                          std::to_string(m_value.b) + ", " + std::to_string(m_value.a) + ")";
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "vec");
    compiler.SetNodeOutputVariable(m_id, "RGBA", var);
    compiler.SetNodeOutputVariable(m_id, "RGB", var + ".rgb");
    compiler.SetNodeOutputVariable(m_id, "RG", var + ".rg");
    compiler.SetNodeOutputVariable(m_id, "R", var + ".r");
    return "vec4 " + var + " = " + vec4Str + ";";
}

ColorConstantNode::ColorConstantNode(const glm::vec4& color) : ShaderNode("ColorConstant"), m_color(color) {
    m_displayName = "Color";
    AddOutput("Color", ShaderDataType::Vec4);
    AddOutput("RGB", ShaderDataType::Vec3);
    AddOutput("A", ShaderDataType::Float);
}

std::string ColorConstantNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string vec4Str = "vec4(" + std::to_string(m_color.r) + ", " + std::to_string(m_color.g) + ", " +
                          std::to_string(m_color.b) + ", " + std::to_string(m_color.a) + ")";
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "color");
    compiler.SetNodeOutputVariable(m_id, "Color", var);
    compiler.SetNodeOutputVariable(m_id, "RGB", var + ".rgb");
    compiler.SetNodeOutputVariable(m_id, "A", var + ".a");
    return "vec4 " + var + " = " + vec4Str + ";";
}

ParameterNode::ParameterNode(const std::string& paramName, ShaderDataType type)
    : ShaderNode("Parameter"), m_parameterName(paramName), m_parameterType(type) {
    m_displayName = paramName;
    AddOutput("Value", type);
}

void ParameterNode::SetParameterType(ShaderDataType type) {
    m_parameterType = type;
    if (!m_outputs.empty()) {
        m_outputs[0].type = type;
    }
}

std::string ParameterNode::GenerateCode(MaterialCompiler& compiler) const {
    compiler.SetNodeOutputVariable(m_id, "Value", "u_" + m_parameterName);
    return "";
}

// ============================================================================
// TEXTURE NODES
// ============================================================================

Texture2DNode::Texture2DNode(const std::string& textureName)
    : ShaderNode("Texture2D"), m_textureName(textureName) {
    m_displayName = "Texture 2D";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddOutput("RGBA", ShaderDataType::Vec4);
    AddOutput("RGB", ShaderDataType::Vec3);
    AddOutput("R", ShaderDataType::Float);
    AddOutput("G", ShaderDataType::Float);
    AddOutput("B", ShaderDataType::Float);
    AddOutput("A", ShaderDataType::Float);
}

std::string Texture2DNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvInput = GetInputValue("UV", compiler);
    if (uvInput.empty()) uvInput = "v_TexCoord";

    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "tex");
    compiler.SetNodeOutputVariable(m_id, "RGBA", var);
    compiler.SetNodeOutputVariable(m_id, "RGB", var + ".rgb");
    compiler.SetNodeOutputVariable(m_id, "R", var + ".r");
    compiler.SetNodeOutputVariable(m_id, "G", var + ".g");
    compiler.SetNodeOutputVariable(m_id, "B", var + ".b");
    compiler.SetNodeOutputVariable(m_id, "A", var + ".a");

    compiler.AddUniform("sampler2D", "u_" + m_textureName);
    return "vec4 " + var + " = texture(u_" + m_textureName + ", " + uvInput + ");";
}

NormalMapNode::NormalMapNode(const std::string& textureName)
    : ShaderNode("NormalMap"), m_textureName(textureName) {
    m_displayName = "Normal Map";
    AddInput("UV", ShaderDataType::Vec2, "UV");
    AddInput("Strength", ShaderDataType::Float, "Strength");
    AddOutput("Normal", ShaderDataType::Vec3);
    SetInputDefault("Strength", 1.0f);
}

std::string NormalMapNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string uvInput = GetInputValue("UV", compiler);
    if (uvInput.empty()) uvInput = "v_TexCoord";
    std::string strengthInput = GetInputValue("Strength", compiler);

    std::string var = compiler.AllocateVariable(ShaderDataType::Vec3, "normal");
    compiler.SetNodeOutputVariable(m_id, "Normal", var);
    compiler.AddUniform("sampler2D", "u_" + m_textureName);

    return "vec3 " + var + " = normalize(v_TBN * (texture(u_" + m_textureName + ", " + uvInput + ").xyz * 2.0 - 1.0) * vec3(" + strengthInput + ", " + strengthInput + ", 1.0));";
}

TextureCubeNode::TextureCubeNode(const std::string& textureName)
    : ShaderNode("TextureCube"), m_textureName(textureName) {
    m_displayName = "Texture Cube";
    AddInput("Direction", ShaderDataType::Vec3, "Direction");
    AddOutput("RGBA", ShaderDataType::Vec4);
    AddOutput("RGB", ShaderDataType::Vec3);
}

std::string TextureCubeNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string dirInput = GetInputValue("Direction", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "cubeTex");
    compiler.SetNodeOutputVariable(m_id, "RGBA", var);
    compiler.SetNodeOutputVariable(m_id, "RGB", var + ".rgb");
    compiler.AddUniform("samplerCube", "u_" + m_textureName);
    return "vec4 " + var + " = texture(u_" + m_textureName + ", " + dirInput + ");";
}

// ============================================================================
// MATH BASIC NODES
// ============================================================================

#define DEFINE_BINARY_OP_NODE(NodeName, Op) \
NodeName::NodeName() : ShaderNode(#NodeName) { \
    m_displayName = #NodeName; \
    AddInput("A", ShaderDataType::Vec4, "A"); \
    AddInput("B", ShaderDataType::Vec4, "B"); \
    AddOutput("Result", ShaderDataType::Vec4); \
    SetInputDefault("A", glm::vec4(0.0f)); \
    SetInputDefault("B", glm::vec4(0.0f)); \
} \
std::string NodeName::GenerateCode(MaterialCompiler& compiler) const { \
    std::string a = GetInputValue("A", compiler); \
    std::string b = GetInputValue("B", compiler); \
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v"); \
    compiler.SetNodeOutputVariable(m_id, "Result", var); \
    return "vec4 " + var + " = " + a + " " #Op " " + b + ";"; \
}

DEFINE_BINARY_OP_NODE(AddNode, +)
DEFINE_BINARY_OP_NODE(SubtractNode, -)
DEFINE_BINARY_OP_NODE(MultiplyNode, *)
DEFINE_BINARY_OP_NODE(DivideNode, /)

#define DEFINE_UNARY_FUNC_NODE(NodeName, Func) \
NodeName::NodeName() : ShaderNode(#NodeName) { \
    m_displayName = #NodeName; \
    AddInput("Input", ShaderDataType::Vec4, "Input"); \
    AddOutput("Result", ShaderDataType::Vec4); \
} \
std::string NodeName::GenerateCode(MaterialCompiler& compiler) const { \
    std::string input = GetInputValue("Input", compiler); \
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v"); \
    compiler.SetNodeOutputVariable(m_id, "Result", var); \
    return "vec4 " + var + " = " #Func "(" + input + ");"; \
}

OneMinusNode::OneMinusNode() : ShaderNode("OneMinus") {
    m_displayName = "One Minus";
    AddInput("Input", ShaderDataType::Vec4, "Input");
    AddOutput("Result", ShaderDataType::Vec4);
}

std::string OneMinusNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("Input", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = vec4(1.0) - " + input + ";";
}

DEFINE_UNARY_FUNC_NODE(AbsNode, abs)
DEFINE_UNARY_FUNC_NODE(FloorNode, floor)
DEFINE_UNARY_FUNC_NODE(CeilNode, ceil)
DEFINE_UNARY_FUNC_NODE(RoundNode, round)
DEFINE_UNARY_FUNC_NODE(FracNode, fract)

NegateNode::NegateNode() : ShaderNode("Negate") {
    m_displayName = "Negate";
    AddInput("Input", ShaderDataType::Vec4, "Input");
    AddOutput("Result", ShaderDataType::Vec4);
}

std::string NegateNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("Input", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = -" + input + ";";
}

MinNode::MinNode() : ShaderNode("Min") {
    m_displayName = "Min";
    AddInput("A", ShaderDataType::Vec4, "A");
    AddInput("B", ShaderDataType::Vec4, "B");
    AddOutput("Result", ShaderDataType::Vec4);
}

std::string MinNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string a = GetInputValue("A", compiler);
    std::string b = GetInputValue("B", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = min(" + a + ", " + b + ");";
}

MaxNode::MaxNode() : ShaderNode("Max") {
    m_displayName = "Max";
    AddInput("A", ShaderDataType::Vec4, "A");
    AddInput("B", ShaderDataType::Vec4, "B");
    AddOutput("Result", ShaderDataType::Vec4);
}

std::string MaxNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string a = GetInputValue("A", compiler);
    std::string b = GetInputValue("B", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = max(" + a + ", " + b + ");";
}

ClampNode::ClampNode() : ShaderNode("Clamp") {
    m_displayName = "Clamp";
    AddInput("Value", ShaderDataType::Vec4, "Value");
    AddInput("Min", ShaderDataType::Vec4, "Min");
    AddInput("Max", ShaderDataType::Vec4, "Max");
    AddOutput("Result", ShaderDataType::Vec4);
    SetInputDefault("Min", glm::vec4(0.0f));
    SetInputDefault("Max", glm::vec4(1.0f));
}

std::string ClampNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string val = GetInputValue("Value", compiler);
    std::string minVal = GetInputValue("Min", compiler);
    std::string maxVal = GetInputValue("Max", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = clamp(" + val + ", " + minVal + ", " + maxVal + ");";
}

SaturateNode::SaturateNode() : ShaderNode("Saturate") {
    m_displayName = "Saturate";
    AddInput("Input", ShaderDataType::Vec4, "Input");
    AddOutput("Result", ShaderDataType::Vec4);
}

std::string SaturateNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("Input", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = clamp(" + input + ", vec4(0.0), vec4(1.0));";
}

ModNode::ModNode() : ShaderNode("Mod") {
    m_displayName = "Modulo";
    AddInput("A", ShaderDataType::Vec4, "A");
    AddInput("B", ShaderDataType::Vec4, "B");
    AddOutput("Result", ShaderDataType::Vec4);
    SetInputDefault("B", glm::vec4(1.0f));
}

std::string ModNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string a = GetInputValue("A", compiler);
    std::string b = GetInputValue("B", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = mod(" + a + ", " + b + ");";
}

// ============================================================================
// MATH ADVANCED NODES
// ============================================================================

PowerNode::PowerNode() : ShaderNode("Power") {
    m_displayName = "Power";
    AddInput("Base", ShaderDataType::Vec4, "Base");
    AddInput("Exponent", ShaderDataType::Vec4, "Exponent");
    AddOutput("Result", ShaderDataType::Vec4);
    SetInputDefault("Exponent", glm::vec4(2.0f));
}

std::string PowerNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string base = GetInputValue("Base", compiler);
    std::string exp = GetInputValue("Exponent", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = pow(" + base + ", " + exp + ");";
}

DEFINE_UNARY_FUNC_NODE(SqrtNode, sqrt)
DEFINE_UNARY_FUNC_NODE(InverseSqrtNode, inversesqrt)
DEFINE_UNARY_FUNC_NODE(LogNode, log)
DEFINE_UNARY_FUNC_NODE(Log2Node, log2)
DEFINE_UNARY_FUNC_NODE(ExpNode, exp)
DEFINE_UNARY_FUNC_NODE(Exp2Node, exp2)

// ============================================================================
// MATH TRIG NODES
// ============================================================================

DEFINE_UNARY_FUNC_NODE(SinNode, sin)
DEFINE_UNARY_FUNC_NODE(CosNode, cos)
DEFINE_UNARY_FUNC_NODE(TanNode, tan)
DEFINE_UNARY_FUNC_NODE(AsinNode, asin)
DEFINE_UNARY_FUNC_NODE(AcosNode, acos)
DEFINE_UNARY_FUNC_NODE(AtanNode, atan)

Atan2Node::Atan2Node() : ShaderNode("Atan2") {
    m_displayName = "Atan2";
    AddInput("Y", ShaderDataType::Float, "Y");
    AddInput("X", ShaderDataType::Float, "X");
    AddOutput("Result", ShaderDataType::Float);
}

std::string Atan2Node::GenerateCode(MaterialCompiler& compiler) const {
    std::string y = GetInputValue("Y", compiler);
    std::string x = GetInputValue("X", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Float, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "float " + var + " = atan(" + y + ", " + x + ");";
}

// ============================================================================
// MATH VECTOR NODES
// ============================================================================

DotNode::DotNode() : ShaderNode("Dot") {
    m_displayName = "Dot Product";
    AddInput("A", ShaderDataType::Vec3, "A");
    AddInput("B", ShaderDataType::Vec3, "B");
    AddOutput("Result", ShaderDataType::Float);
}

std::string DotNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string a = GetInputValue("A", compiler);
    std::string b = GetInputValue("B", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Float, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "float " + var + " = dot(" + a + ", " + b + ");";
}

CrossNode::CrossNode() : ShaderNode("Cross") {
    m_displayName = "Cross Product";
    AddInput("A", ShaderDataType::Vec3, "A");
    AddInput("B", ShaderDataType::Vec3, "B");
    AddOutput("Result", ShaderDataType::Vec3);
}

std::string CrossNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string a = GetInputValue("A", compiler);
    std::string b = GetInputValue("B", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec3, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec3 " + var + " = cross(" + a + ", " + b + ");";
}

NormalizeNode::NormalizeNode() : ShaderNode("Normalize") {
    m_displayName = "Normalize";
    AddInput("Input", ShaderDataType::Vec3, "Input");
    AddOutput("Result", ShaderDataType::Vec3);
}

std::string NormalizeNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("Input", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec3, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec3 " + var + " = normalize(" + input + ");";
}

LengthNode::LengthNode() : ShaderNode("Length") {
    m_displayName = "Length";
    AddInput("Input", ShaderDataType::Vec3, "Input");
    AddOutput("Result", ShaderDataType::Float);
}

std::string LengthNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("Input", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Float, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "float " + var + " = length(" + input + ");";
}

DistanceNode::DistanceNode() : ShaderNode("Distance") {
    m_displayName = "Distance";
    AddInput("A", ShaderDataType::Vec3, "A");
    AddInput("B", ShaderDataType::Vec3, "B");
    AddOutput("Result", ShaderDataType::Float);
}

std::string DistanceNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string a = GetInputValue("A", compiler);
    std::string b = GetInputValue("B", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Float, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "float " + var + " = distance(" + a + ", " + b + ");";
}

ReflectNode::ReflectNode() : ShaderNode("Reflect") {
    m_displayName = "Reflect";
    AddInput("Incident", ShaderDataType::Vec3, "Incident");
    AddInput("Normal", ShaderDataType::Vec3, "Normal");
    AddOutput("Result", ShaderDataType::Vec3);
}

std::string ReflectNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string i = GetInputValue("Incident", compiler);
    std::string n = GetInputValue("Normal", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec3, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec3 " + var + " = reflect(" + i + ", " + n + ");";
}

RefractNode::RefractNode() : ShaderNode("Refract") {
    m_displayName = "Refract";
    AddInput("Incident", ShaderDataType::Vec3, "Incident");
    AddInput("Normal", ShaderDataType::Vec3, "Normal");
    AddInput("IOR", ShaderDataType::Float, "IOR");
    AddOutput("Result", ShaderDataType::Vec3);
    SetInputDefault("IOR", 1.0f);
}

std::string RefractNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string i = GetInputValue("Incident", compiler);
    std::string n = GetInputValue("Normal", compiler);
    std::string ior = GetInputValue("IOR", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec3, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec3 " + var + " = refract(" + i + ", " + n + ", " + ior + ");";
}

TransformNode::TransformNode() : ShaderNode("Transform") {
    m_displayName = "Transform";
    AddInput("Vector", ShaderDataType::Vec4, "Vector");
    AddInput("Matrix", ShaderDataType::Mat4, "Matrix");
    AddOutput("Result", ShaderDataType::Vec4);
}

std::string TransformNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string vec = GetInputValue("Vector", compiler);
    std::string mat = GetInputValue("Matrix", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = " + mat + " * " + vec + ";";
}

// ============================================================================
// INTERPOLATION NODES
// ============================================================================

LerpNode::LerpNode() : ShaderNode("Lerp") {
    m_displayName = "Lerp";
    AddInput("A", ShaderDataType::Vec4, "A");
    AddInput("B", ShaderDataType::Vec4, "B");
    AddInput("T", ShaderDataType::Float, "T");
    AddOutput("Result", ShaderDataType::Vec4);
    SetInputDefault("T", 0.5f);
}

std::string LerpNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string a = GetInputValue("A", compiler);
    std::string b = GetInputValue("B", compiler);
    std::string t = GetInputValue("T", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = mix(" + a + ", " + b + ", " + t + ");";
}

SmoothStepNode::SmoothStepNode() : ShaderNode("SmoothStep") {
    m_displayName = "Smooth Step";
    AddInput("Edge0", ShaderDataType::Float, "Edge0");
    AddInput("Edge1", ShaderDataType::Float, "Edge1");
    AddInput("X", ShaderDataType::Float, "X");
    AddOutput("Result", ShaderDataType::Float);
    SetInputDefault("Edge0", 0.0f);
    SetInputDefault("Edge1", 1.0f);
}

std::string SmoothStepNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string e0 = GetInputValue("Edge0", compiler);
    std::string e1 = GetInputValue("Edge1", compiler);
    std::string x = GetInputValue("X", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Float, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "float " + var + " = smoothstep(" + e0 + ", " + e1 + ", " + x + ");";
}

StepNode::StepNode() : ShaderNode("Step") {
    m_displayName = "Step";
    AddInput("Edge", ShaderDataType::Float, "Edge");
    AddInput("X", ShaderDataType::Float, "X");
    AddOutput("Result", ShaderDataType::Float);
    SetInputDefault("Edge", 0.5f);
}

std::string StepNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string edge = GetInputValue("Edge", compiler);
    std::string x = GetInputValue("X", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Float, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "float " + var + " = step(" + edge + ", " + x + ");";
}

InverseLerpNode::InverseLerpNode() : ShaderNode("InverseLerp") {
    m_displayName = "Inverse Lerp";
    AddInput("A", ShaderDataType::Float, "A");
    AddInput("B", ShaderDataType::Float, "B");
    AddInput("Value", ShaderDataType::Float, "Value");
    AddOutput("Result", ShaderDataType::Float);
}

std::string InverseLerpNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string a = GetInputValue("A", compiler);
    std::string b = GetInputValue("B", compiler);
    std::string val = GetInputValue("Value", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Float, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "float " + var + " = (" + val + " - " + a + ") / (" + b + " - " + a + ");";
}

RemapNode::RemapNode() : ShaderNode("Remap") {
    m_displayName = "Remap";
    AddInput("Value", ShaderDataType::Float, "Value");
    AddInput("InMin", ShaderDataType::Float, "In Min");
    AddInput("InMax", ShaderDataType::Float, "In Max");
    AddInput("OutMin", ShaderDataType::Float, "Out Min");
    AddInput("OutMax", ShaderDataType::Float, "Out Max");
    AddOutput("Result", ShaderDataType::Float);
    SetInputDefault("InMin", 0.0f);
    SetInputDefault("InMax", 1.0f);
    SetInputDefault("OutMin", 0.0f);
    SetInputDefault("OutMax", 1.0f);
}

std::string RemapNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string val = GetInputValue("Value", compiler);
    std::string inMin = GetInputValue("InMin", compiler);
    std::string inMax = GetInputValue("InMax", compiler);
    std::string outMin = GetInputValue("OutMin", compiler);
    std::string outMax = GetInputValue("OutMax", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Float, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "float " + var + " = " + outMin + " + (" + val + " - " + inMin + ") * (" + outMax + " - " + outMin + ") / (" + inMax + " - " + inMin + ");";
}

// ============================================================================
// UTILITY NODES
// ============================================================================

SwizzleNode::SwizzleNode(const std::string& mask) : ShaderNode("Swizzle"), m_mask(mask) {
    m_displayName = "Swizzle";
    AddInput("Input", ShaderDataType::Vec4, "Input");
    AddOutput("Result", ShaderDataType::Vec4);
}

std::string SwizzleNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("Input", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = " + input + "." + m_mask + ";";
}

SplitNode::SplitNode() : ShaderNode("Split") {
    m_displayName = "Split";
    AddInput("Input", ShaderDataType::Vec4, "Input");
    AddOutput("R", ShaderDataType::Float);
    AddOutput("G", ShaderDataType::Float);
    AddOutput("B", ShaderDataType::Float);
    AddOutput("A", ShaderDataType::Float);
}

std::string SplitNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("Input", compiler);
    compiler.SetNodeOutputVariable(m_id, "R", input + ".r");
    compiler.SetNodeOutputVariable(m_id, "G", input + ".g");
    compiler.SetNodeOutputVariable(m_id, "B", input + ".b");
    compiler.SetNodeOutputVariable(m_id, "A", input + ".a");
    return "";
}

CombineNode::CombineNode() : ShaderNode("Combine") {
    m_displayName = "Combine";
    AddInput("R", ShaderDataType::Float, "R");
    AddInput("G", ShaderDataType::Float, "G");
    AddInput("B", ShaderDataType::Float, "B");
    AddInput("A", ShaderDataType::Float, "A");
    AddOutput("RGBA", ShaderDataType::Vec4);
    AddOutput("RGB", ShaderDataType::Vec3);
    SetInputDefault("R", 0.0f);
    SetInputDefault("G", 0.0f);
    SetInputDefault("B", 0.0f);
    SetInputDefault("A", 1.0f);
}

std::string CombineNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string r = GetInputValue("R", compiler);
    std::string g = GetInputValue("G", compiler);
    std::string b = GetInputValue("B", compiler);
    std::string a = GetInputValue("A", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "RGBA", var);
    compiler.SetNodeOutputVariable(m_id, "RGB", var + ".rgb");
    return "vec4 " + var + " = vec4(" + r + ", " + g + ", " + b + ", " + a + ");";
}

AppendNode::AppendNode() : ShaderNode("Append") {
    m_displayName = "Append";
    AddInput("A", ShaderDataType::Vec3, "A");
    AddInput("B", ShaderDataType::Float, "B");
    AddOutput("Result", ShaderDataType::Vec4);
}

std::string AppendNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string a = GetInputValue("A", compiler);
    std::string b = GetInputValue("B", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = vec4(" + a + ", " + b + ");";
}

DDXNode::DDXNode() : ShaderNode("DDX") {
    m_displayName = "DDX";
    AddInput("Input", ShaderDataType::Vec4, "Input");
    AddOutput("Result", ShaderDataType::Vec4);
}

std::string DDXNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("Input", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = dFdx(" + input + ");";
}

DDYNode::DDYNode() : ShaderNode("DDY") {
    m_displayName = "DDY";
    AddInput("Input", ShaderDataType::Vec4, "Input");
    AddOutput("Result", ShaderDataType::Vec4);
}

std::string DDYNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("Input", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = dFdy(" + input + ");";
}

// ============================================================================
// LOGIC NODES
// ============================================================================

IfNode::IfNode() : ShaderNode("If") {
    m_displayName = "If";
    AddInput("Condition", ShaderDataType::Float, "Condition");
    AddInput("True", ShaderDataType::Vec4, "True");
    AddInput("False", ShaderDataType::Vec4, "False");
    AddOutput("Result", ShaderDataType::Vec4);
}

std::string IfNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string cond = GetInputValue("Condition", compiler);
    std::string trueVal = GetInputValue("True", compiler);
    std::string falseVal = GetInputValue("False", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec4 " + var + " = " + cond + " > 0.5 ? " + trueVal + " : " + falseVal + ";";
}

CompareNode::CompareNode(Operation op) : ShaderNode("Compare"), m_operation(op) {
    m_displayName = "Compare";
    AddInput("A", ShaderDataType::Float, "A");
    AddInput("B", ShaderDataType::Float, "B");
    AddOutput("Result", ShaderDataType::Float);
}

std::string CompareNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string a = GetInputValue("A", compiler);
    std::string b = GetInputValue("B", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Float, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);

    std::string op;
    switch (m_operation) {
        case Operation::Equal: op = "=="; break;
        case Operation::NotEqual: op = "!="; break;
        case Operation::Greater: op = ">"; break;
        case Operation::GreaterEqual: op = ">="; break;
        case Operation::Less: op = "<"; break;
        case Operation::LessEqual: op = "<="; break;
    }

    return "float " + var + " = " + a + " " + op + " " + b + " ? 1.0 : 0.0;";
}

// ============================================================================
// COLOR NODES
// ============================================================================

BlendNode::BlendNode(Mode mode) : ShaderNode("Blend"), m_mode(mode) {
    m_displayName = "Blend";
    AddInput("Base", ShaderDataType::Vec4, "Base");
    AddInput("Blend", ShaderDataType::Vec4, "Blend");
    AddInput("Opacity", ShaderDataType::Float, "Opacity");
    AddOutput("Result", ShaderDataType::Vec4);
    SetInputDefault("Opacity", 1.0f);
}

std::string BlendNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string base = GetInputValue("Base", compiler);
    std::string blend = GetInputValue("Blend", compiler);
    std::string opacity = GetInputValue("Opacity", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec4, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);

    std::string blendOp;
    switch (m_mode) {
        case Mode::Normal: blendOp = blend; break;
        case Mode::Multiply: blendOp = base + " * " + blend; break;
        case Mode::Screen: blendOp = "vec4(1.0) - (vec4(1.0) - " + base + ") * (vec4(1.0) - " + blend + ")"; break;
        case Mode::Add: blendOp = base + " + " + blend; break;
        case Mode::Subtract: blendOp = base + " - " + blend; break;
        case Mode::Difference: blendOp = "abs(" + base + " - " + blend + ")"; break;
        default: blendOp = blend; break;
    }

    return "vec4 " + var + " = mix(" + base + ", " + blendOp + ", " + opacity + ");";
}

HSVNode::HSVNode() : ShaderNode("HSV") {
    m_displayName = "HSV Adjust";
    AddInput("Input", ShaderDataType::Vec3, "Input");
    AddInput("Hue", ShaderDataType::Float, "Hue");
    AddInput("Saturation", ShaderDataType::Float, "Saturation");
    AddInput("Value", ShaderDataType::Float, "Value");
    AddOutput("Result", ShaderDataType::Vec3);
    SetInputDefault("Hue", 0.0f);
    SetInputDefault("Saturation", 1.0f);
    SetInputDefault("Value", 1.0f);
}

std::string HSVNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("Input", compiler);
    std::string h = GetInputValue("Hue", compiler);
    std::string s = GetInputValue("Saturation", compiler);
    std::string v = GetInputValue("Value", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec3, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec3 " + var + " = adjustHSV(" + input + ", " + h + ", " + s + ", " + v + ");";
}

RGBToHSVNode::RGBToHSVNode() : ShaderNode("RGBToHSV") {
    m_displayName = "RGB to HSV";
    AddInput("RGB", ShaderDataType::Vec3, "RGB");
    AddOutput("HSV", ShaderDataType::Vec3);
}

std::string RGBToHSVNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("RGB", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec3, "v");
    compiler.SetNodeOutputVariable(m_id, "HSV", var);
    return "vec3 " + var + " = rgbToHsv(" + input + ");";
}

HSVToRGBNode::HSVToRGBNode() : ShaderNode("HSVToRGB") {
    m_displayName = "HSV to RGB";
    AddInput("HSV", ShaderDataType::Vec3, "HSV");
    AddOutput("RGB", ShaderDataType::Vec3);
}

std::string HSVToRGBNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("HSV", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec3, "v");
    compiler.SetNodeOutputVariable(m_id, "RGB", var);
    return "vec3 " + var + " = hsvToRgb(" + input + ");";
}

ContrastNode::ContrastNode() : ShaderNode("Contrast") {
    m_displayName = "Contrast";
    AddInput("Input", ShaderDataType::Vec3, "Input");
    AddInput("Contrast", ShaderDataType::Float, "Contrast");
    AddOutput("Result", ShaderDataType::Vec3);
    SetInputDefault("Contrast", 1.0f);
}

std::string ContrastNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("Input", compiler);
    std::string contrast = GetInputValue("Contrast", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec3, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec3 " + var + " = (" + input + " - 0.5) * " + contrast + " + 0.5;";
}

PosterizeNode::PosterizeNode() : ShaderNode("Posterize") {
    m_displayName = "Posterize";
    AddInput("Input", ShaderDataType::Vec3, "Input");
    AddInput("Levels", ShaderDataType::Float, "Levels");
    AddOutput("Result", ShaderDataType::Vec3);
    SetInputDefault("Levels", 4.0f);
}

std::string PosterizeNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("Input", compiler);
    std::string levels = GetInputValue("Levels", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Vec3, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "vec3 " + var + " = floor(" + input + " * " + levels + ") / " + levels + ";";
}

GrayscaleNode::GrayscaleNode() : ShaderNode("Grayscale") {
    m_displayName = "Grayscale";
    AddInput("Input", ShaderDataType::Vec3, "Input");
    AddOutput("Result", ShaderDataType::Float);
}

std::string GrayscaleNode::GenerateCode(MaterialCompiler& compiler) const {
    std::string input = GetInputValue("Input", compiler);
    std::string var = compiler.AllocateVariable(ShaderDataType::Float, "v");
    compiler.SetNodeOutputVariable(m_id, "Result", var);
    return "float " + var + " = dot(" + input + ", vec3(0.299, 0.587, 0.114));";
}

} // namespace Nova
