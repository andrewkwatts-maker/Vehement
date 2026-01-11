#include "ShaderGraph.hpp"
#include "ShaderNodes.hpp"
#include "NoiseNodes.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <queue>

namespace Nova {

using json = nlohmann::json;

// ============================================================================
// Data Type Utilities
// ============================================================================

const char* ShaderDataTypeToGLSL(ShaderDataType type) {
    switch (type) {
        case ShaderDataType::Float: return "float";
        case ShaderDataType::Vec2: return "vec2";
        case ShaderDataType::Vec3: return "vec3";
        case ShaderDataType::Vec4: return "vec4";
        case ShaderDataType::Int: return "int";
        case ShaderDataType::IVec2: return "ivec2";
        case ShaderDataType::IVec3: return "ivec3";
        case ShaderDataType::IVec4: return "ivec4";
        case ShaderDataType::Bool: return "bool";
        case ShaderDataType::Mat3: return "mat3";
        case ShaderDataType::Mat4: return "mat4";
        case ShaderDataType::Sampler2D: return "sampler2D";
        case ShaderDataType::SamplerCube: return "samplerCube";
        case ShaderDataType::Sampler3D: return "sampler3D";
        case ShaderDataType::Void: return "void";
    }
    return "float";
}

std::string ShaderDataTypeDefaultValue(ShaderDataType type) {
    switch (type) {
        case ShaderDataType::Float: return "0.0";
        case ShaderDataType::Vec2: return "vec2(0.0)";
        case ShaderDataType::Vec3: return "vec3(0.0)";
        case ShaderDataType::Vec4: return "vec4(0.0)";
        case ShaderDataType::Int: return "0";
        case ShaderDataType::IVec2: return "ivec2(0)";
        case ShaderDataType::IVec3: return "ivec3(0)";
        case ShaderDataType::IVec4: return "ivec4(0)";
        case ShaderDataType::Bool: return "false";
        case ShaderDataType::Mat3: return "mat3(1.0)";
        case ShaderDataType::Mat4: return "mat4(1.0)";
        default: return "0.0";
    }
}

bool AreTypesCompatible(ShaderDataType from, ShaderDataType to) {
    if (from == to) return true;

    // Allow implicit conversions
    int fromComponents = GetComponentCount(from);
    int toComponents = GetComponentCount(to);

    // Float can convert to any float-based type
    if (from == ShaderDataType::Float &&
        (to == ShaderDataType::Vec2 || to == ShaderDataType::Vec3 || to == ShaderDataType::Vec4)) {
        return true;
    }

    // Higher dimension vectors can be swizzled down
    if (fromComponents >= toComponents &&
        from != ShaderDataType::Mat3 && from != ShaderDataType::Mat4 &&
        to != ShaderDataType::Mat3 && to != ShaderDataType::Mat4) {
        return true;
    }

    return false;
}

int GetComponentCount(ShaderDataType type) {
    switch (type) {
        case ShaderDataType::Float:
        case ShaderDataType::Int:
        case ShaderDataType::Bool:
            return 1;
        case ShaderDataType::Vec2:
        case ShaderDataType::IVec2:
            return 2;
        case ShaderDataType::Vec3:
        case ShaderDataType::IVec3:
            return 3;
        case ShaderDataType::Vec4:
        case ShaderDataType::IVec4:
            return 4;
        case ShaderDataType::Mat3:
            return 9;
        case ShaderDataType::Mat4:
            return 16;
        default:
            return 1;
    }
}

const char* NodeCategoryToString(NodeCategory category) {
    switch (category) {
        case NodeCategory::Input: return "Input";
        case NodeCategory::Parameter: return "Parameter";
        case NodeCategory::Texture: return "Texture";
        case NodeCategory::MathBasic: return "Math/Basic";
        case NodeCategory::MathAdvanced: return "Math/Advanced";
        case NodeCategory::MathTrig: return "Math/Trigonometry";
        case NodeCategory::MathVector: return "Math/Vector";
        case NodeCategory::MathInterpolation: return "Math/Interpolation";
        case NodeCategory::Utility: return "Utility";
        case NodeCategory::Logic: return "Logic";
        case NodeCategory::Noise: return "Procedural/Noise";
        case NodeCategory::Pattern: return "Procedural/Pattern";
        case NodeCategory::Color: return "Color";
        case NodeCategory::Output: return "Output";
        case NodeCategory::Custom: return "Custom";
        case NodeCategory::SubGraph: return "SubGraph";
    }
    return "Unknown";
}

const char* MaterialOutputToString(MaterialOutput output) {
    switch (output) {
        case MaterialOutput::BaseColor: return "Base Color";
        case MaterialOutput::Metallic: return "Metallic";
        case MaterialOutput::Roughness: return "Roughness";
        case MaterialOutput::Normal: return "Normal";
        case MaterialOutput::Emissive: return "Emissive";
        case MaterialOutput::EmissiveStrength: return "Emissive Strength";
        case MaterialOutput::AmbientOcclusion: return "Ambient Occlusion";
        case MaterialOutput::Opacity: return "Opacity";
        case MaterialOutput::OpacityMask: return "Opacity Mask";
        case MaterialOutput::Subsurface: return "Subsurface";
        case MaterialOutput::SubsurfaceColor: return "Subsurface Color";
        case MaterialOutput::Specular: return "Specular";
        case MaterialOutput::Anisotropy: return "Anisotropy";
        case MaterialOutput::AnisotropyRotation: return "Anisotropy Rotation";
        case MaterialOutput::WorldPositionOffset: return "World Position Offset";
        case MaterialOutput::WorldDisplacement: return "World Displacement";
        case MaterialOutput::TessellationMultiplier: return "Tessellation Multiplier";
        case MaterialOutput::SDFDistance: return "SDF Distance";
        case MaterialOutput::SDFGradient: return "SDF Gradient";
    }
    return "Unknown";
}

const char* MaterialOutputToGLSL(MaterialOutput output) {
    switch (output) {
        case MaterialOutput::BaseColor: return "mat_baseColor";
        case MaterialOutput::Metallic: return "mat_metallic";
        case MaterialOutput::Roughness: return "mat_roughness";
        case MaterialOutput::Normal: return "mat_normal";
        case MaterialOutput::Emissive: return "mat_emissive";
        case MaterialOutput::EmissiveStrength: return "mat_emissiveStrength";
        case MaterialOutput::AmbientOcclusion: return "mat_ao";
        case MaterialOutput::Opacity: return "mat_opacity";
        case MaterialOutput::OpacityMask: return "mat_opacityMask";
        case MaterialOutput::Subsurface: return "mat_subsurface";
        case MaterialOutput::SubsurfaceColor: return "mat_subsurfaceColor";
        case MaterialOutput::Specular: return "mat_specular";
        case MaterialOutput::Anisotropy: return "mat_anisotropy";
        case MaterialOutput::AnisotropyRotation: return "mat_anisotropyRotation";
        case MaterialOutput::WorldPositionOffset: return "mat_worldPosOffset";
        case MaterialOutput::WorldDisplacement: return "mat_displacement";
        case MaterialOutput::TessellationMultiplier: return "mat_tessellation";
        case MaterialOutput::SDFDistance: return "mat_sdfDistance";
        case MaterialOutput::SDFGradient: return "mat_sdfGradient";
    }
    return "mat_unknown";
}

// ============================================================================
// ShaderNode Implementation
// ============================================================================

NodeId ShaderNode::s_nextId = 1;

ShaderNode::ShaderNode(const std::string& name)
    : m_id(s_nextId++), m_name(name), m_displayName(name) {}

ShaderPin* ShaderNode::GetInput(const std::string& name) {
    for (auto& pin : m_inputs) {
        if (pin.name == name) return &pin;
    }
    return nullptr;
}

ShaderPin* ShaderNode::GetOutput(const std::string& name) {
    for (auto& pin : m_outputs) {
        if (pin.name == name) return &pin;
    }
    return nullptr;
}

const ShaderPin* ShaderNode::GetInput(const std::string& name) const {
    for (const auto& pin : m_inputs) {
        if (pin.name == name) return &pin;
    }
    return nullptr;
}

const ShaderPin* ShaderNode::GetOutput(const std::string& name) const {
    for (const auto& pin : m_outputs) {
        if (pin.name == name) return &pin;
    }
    return nullptr;
}

bool ShaderNode::Connect(const std::string& inputPin, Ptr sourceNode, const std::string& outputPin) {
    auto* input = GetInput(inputPin);
    if (!input) return false;

    auto* output = sourceNode->GetOutput(outputPin);
    if (!output) return false;

    if (!AreTypesCompatible(output->type, input->type)) return false;

    input->connectedNode = sourceNode;
    input->connectedPinName = outputPin;
    return true;
}

void ShaderNode::Disconnect(const std::string& inputPin) {
    auto* input = GetInput(inputPin);
    if (input) {
        input->connectedNode.reset();
        input->connectedPinName.clear();
    }
}

void ShaderNode::DisconnectAll() {
    for (auto& input : m_inputs) {
        input.connectedNode.reset();
        input.connectedPinName.clear();
    }
}

void ShaderNode::AddInput(const std::string& name, ShaderDataType type, const std::string& displayName) {
    ShaderPin pin;
    pin.name = name;
    pin.displayName = displayName.empty() ? name : displayName;
    pin.type = type;
    pin.direction = PinDirection::Input;
    pin.id = s_nextId++;

    // Set default color based on type
    switch (type) {
        case ShaderDataType::Float: pin.color = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f); break;
        case ShaderDataType::Vec2: pin.color = glm::vec4(0.4f, 0.8f, 0.4f, 1.0f); break;
        case ShaderDataType::Vec3: pin.color = glm::vec4(0.8f, 0.8f, 0.2f, 1.0f); break;
        case ShaderDataType::Vec4: pin.color = glm::vec4(0.8f, 0.2f, 0.8f, 1.0f); break;
        case ShaderDataType::Sampler2D: pin.color = glm::vec4(0.9f, 0.4f, 0.1f, 1.0f); break;
        default: pin.color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f); break;
    }

    m_inputs.push_back(pin);
}

void ShaderNode::AddOutput(const std::string& name, ShaderDataType type, const std::string& displayName) {
    ShaderPin pin;
    pin.name = name;
    pin.displayName = displayName.empty() ? name : displayName;
    pin.type = type;
    pin.direction = PinDirection::Output;
    pin.id = s_nextId++;

    // Set color based on type
    switch (type) {
        case ShaderDataType::Float: pin.color = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f); break;
        case ShaderDataType::Vec2: pin.color = glm::vec4(0.4f, 0.8f, 0.4f, 1.0f); break;
        case ShaderDataType::Vec3: pin.color = glm::vec4(0.8f, 0.8f, 0.2f, 1.0f); break;
        case ShaderDataType::Vec4: pin.color = glm::vec4(0.8f, 0.2f, 0.8f, 1.0f); break;
        case ShaderDataType::Sampler2D: pin.color = glm::vec4(0.9f, 0.4f, 0.1f, 1.0f); break;
        default: pin.color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f); break;
    }

    m_outputs.push_back(pin);
}

std::string ShaderNode::GetInputValue(const std::string& name, MaterialCompiler& compiler) const {
    const auto* input = GetInput(name);
    if (!input) return ShaderDataTypeDefaultValue(ShaderDataType::Float);

    if (input->IsConnected()) {
        auto sourceNode = input->connectedNode.lock();
        if (sourceNode) {
            // Ensure source node is compiled first
            if (!compiler.IsNodeCompiled(sourceNode->GetId())) {
                sourceNode->GenerateCode(compiler);
                compiler.MarkNodeCompiled(sourceNode->GetId());
            }
            return compiler.GetNodeOutputVariable(sourceNode->GetId(), input->connectedPinName);
        }
    }

    // Return default value
    return std::visit([input](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, float>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, glm::vec2>) {
            return "vec2(" + std::to_string(arg.x) + ", " + std::to_string(arg.y) + ")";
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            return "vec3(" + std::to_string(arg.x) + ", " + std::to_string(arg.y) + ", " + std::to_string(arg.z) + ")";
        } else if constexpr (std::is_same_v<T, glm::vec4>) {
            return "vec4(" + std::to_string(arg.r) + ", " + std::to_string(arg.g) + ", " +
                   std::to_string(arg.b) + ", " + std::to_string(arg.a) + ")";
        } else if constexpr (std::is_same_v<T, int>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        }
        return ShaderDataTypeDefaultValue(input->type);
    }, input->defaultValue);
}

std::string ShaderNode::GeneratePreviewCode(MaterialCompiler& compiler) const {
    return GenerateCode(compiler);
}

std::string ShaderNode::ToJson() const {
    json j;
    j["id"] = m_id;
    j["type"] = GetTypeName();
    j["name"] = m_name;
    j["displayName"] = m_displayName;
    j["comment"] = m_comment;
    j["position"] = {m_position.x, m_position.y};
    j["expanded"] = m_expanded;

    // Save connections
    json connections = json::array();
    for (const auto& input : m_inputs) {
        if (input.IsConnected()) {
            auto sourceNode = input.connectedNode.lock();
            if (sourceNode) {
                connections.push_back({
                    {"inputPin", input.name},
                    {"sourceNode", sourceNode->GetId()},
                    {"sourcePin", input.connectedPinName}
                });
            }
        }
    }
    j["connections"] = connections;

    return j.dump(2);
}

void ShaderNode::FromJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        if (j.contains("displayName")) m_displayName = j["displayName"];
        if (j.contains("comment")) m_comment = j["comment"];
        if (j.contains("position") && j["position"].is_array()) {
            m_position = glm::vec2(j["position"][0], j["position"][1]);
        }
        if (j.contains("expanded")) m_expanded = j["expanded"];
    } catch (...) {}
}

// ============================================================================
// ShaderGraph Implementation
// ============================================================================

ShaderGraph::ShaderGraph(const std::string& name)
    : m_name(name) {
    // Create output node
    m_outputNode = std::make_shared<MaterialOutputNode>();
    m_nodes.push_back(m_outputNode);
}

void ShaderGraph::AddNode(ShaderNode::Ptr node) {
    m_nodes.push_back(node);
}

void ShaderGraph::RemoveNode(NodeId id) {
    // Don't remove output node
    if (m_outputNode && m_outputNode->GetId() == id) return;

    // Disconnect any nodes connected to this one
    for (auto& node : m_nodes) {
        for (auto& input : const_cast<std::vector<ShaderPin>&>(node->GetInputs())) {
            if (input.IsConnected()) {
                auto sourceNode = input.connectedNode.lock();
                if (sourceNode && sourceNode->GetId() == id) {
                    input.connectedNode.reset();
                    input.connectedPinName.clear();
                }
            }
        }
    }

    m_nodes.erase(
        std::remove_if(m_nodes.begin(), m_nodes.end(),
            [id](const ShaderNode::Ptr& n) { return n->GetId() == id; }),
        m_nodes.end());
}

ShaderNode::Ptr ShaderGraph::GetNode(NodeId id) const {
    for (const auto& node : m_nodes) {
        if (node->GetId() == id) return node;
    }
    return nullptr;
}

bool ShaderGraph::Connect(NodeId fromNode, const std::string& fromPin,
                          NodeId toNode, const std::string& toPin) {
    auto source = GetNode(fromNode);
    auto dest = GetNode(toNode);
    if (!source || !dest) return false;

    return dest->Connect(toPin, source, fromPin);
}

void ShaderGraph::Disconnect(NodeId toNode, const std::string& toPin) {
    auto node = GetNode(toNode);
    if (node) {
        node->Disconnect(toPin);
    }
}

bool ShaderGraph::Validate(std::vector<std::string>& errors) const {
    errors.clear();

    if (!m_outputNode) {
        errors.push_back("No output node in graph");
        return false;
    }

    if (HasCycle()) {
        errors.push_back("Graph contains a cycle");
        return false;
    }

    // Check that at least base color is connected
    const auto* baseColorInput = m_outputNode->GetInput("BaseColor");
    if (baseColorInput && !baseColorInput->IsConnected()) {
        errors.push_back("Base Color is not connected");
    }

    return errors.empty();
}

bool ShaderGraph::HasCycle() const {
    std::unordered_set<NodeId> visited;
    std::unordered_set<NodeId> inStack;

    std::function<bool(NodeId)> hasCycleDFS = [&](NodeId nodeId) -> bool {
        if (inStack.count(nodeId)) return true;
        if (visited.count(nodeId)) return false;

        visited.insert(nodeId);
        inStack.insert(nodeId);

        auto node = GetNode(nodeId);
        if (node) {
            for (const auto& input : node->GetInputs()) {
                if (input.IsConnected()) {
                    auto sourceNode = input.connectedNode.lock();
                    if (sourceNode && hasCycleDFS(sourceNode->GetId())) {
                        return true;
                    }
                }
            }
        }

        inStack.erase(nodeId);
        return false;
    };

    for (const auto& node : m_nodes) {
        if (hasCycleDFS(node->GetId())) return true;
    }

    return false;
}

std::string ShaderGraph::Compile() const {
    MaterialCompiler compiler(*this);
    return compiler.CompileFragmentShader();
}

bool ShaderGraph::CompileToFiles(const std::string& vertexPath, const std::string& fragmentPath) const {
    MaterialCompiler compiler(*this);

    std::ofstream vertFile(vertexPath);
    if (!vertFile.is_open()) return false;
    vertFile << compiler.CompileVertexShader();
    vertFile.close();

    std::ofstream fragFile(fragmentPath);
    if (!fragFile.is_open()) return false;
    fragFile << compiler.CompileFragmentShader();
    fragFile.close();

    return true;
}

std::string ShaderGraph::ToJson() const {
    json j;
    j["name"] = m_name;
    j["domain"] = static_cast<int>(m_domain);
    j["blendMode"] = static_cast<int>(m_blendMode);
    j["shadingModel"] = static_cast<int>(m_shadingModel);
    j["twoSided"] = m_twoSided;

    // Nodes
    json nodes = json::array();
    for (const auto& node : m_nodes) {
        nodes.push_back(json::parse(node->ToJson()));
    }
    j["nodes"] = nodes;

    // Parameters
    json params = json::array();
    for (const auto& param : m_parameters) {
        json p;
        p["name"] = param.name;
        p["displayName"] = param.displayName;
        p["group"] = param.group;
        p["type"] = static_cast<int>(param.type);
        p["isTexture"] = param.isTexture;
        p["minValue"] = param.minValue;
        p["maxValue"] = param.maxValue;
        params.push_back(p);
    }
    j["parameters"] = params;

    // Groups
    json groups = json::array();
    for (const auto& group : m_groups) {
        json g;
        g["name"] = group.name;
        g["color"] = {group.color.r, group.color.g, group.color.b, group.color.a};
        g["position"] = {group.position.x, group.position.y};
        g["size"] = {group.size.x, group.size.y};
        g["nodes"] = group.nodes;
        groups.push_back(g);
    }
    j["groups"] = groups;

    return j.dump(2);
}

std::shared_ptr<ShaderGraph> ShaderGraph::FromJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        auto graph = std::make_shared<ShaderGraph>();

        if (j.contains("name")) graph->SetName(j["name"]);
        if (j.contains("domain")) graph->SetDomain(static_cast<MaterialDomain>(j["domain"].get<int>()));
        if (j.contains("blendMode")) graph->SetBlendMode(static_cast<BlendMode>(j["blendMode"].get<int>()));
        if (j.contains("shadingModel")) graph->SetShadingModel(static_cast<ShadingModel>(j["shadingModel"].get<int>()));
        if (j.contains("twoSided")) graph->SetTwoSided(j["twoSided"]);

        // Load nodes (simplified - would need full factory)
        // The actual implementation would use ShaderNodeFactory

        return graph;
    } catch (...) {
        return nullptr;
    }
}

bool ShaderGraph::SaveToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << ToJson();
    return true;
}

std::shared_ptr<ShaderGraph> ShaderGraph::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return nullptr;

    std::stringstream buffer;
    buffer << file.rdbuf();
    return FromJson(buffer.str());
}

void ShaderGraph::AddParameter(const Parameter& param) {
    m_parameters.push_back(param);
}

void ShaderGraph::RemoveParameter(const std::string& name) {
    m_parameters.erase(
        std::remove_if(m_parameters.begin(), m_parameters.end(),
            [&name](const Parameter& p) { return p.name == name; }),
        m_parameters.end());
}

ShaderGraph::Parameter* ShaderGraph::GetParameter(const std::string& name) {
    for (auto& param : m_parameters) {
        if (param.name == name) return &param;
    }
    return nullptr;
}

void ShaderGraph::AddSubGraph(const std::string& name, std::shared_ptr<ShaderGraph> subGraph) {
    m_subGraphs[name] = subGraph;
}

std::shared_ptr<ShaderGraph> ShaderGraph::GetSubGraph(const std::string& name) const {
    auto it = m_subGraphs.find(name);
    return it != m_subGraphs.end() ? it->second : nullptr;
}

void ShaderGraph::AddGroup(const NodeGroup& group) {
    m_groups.push_back(group);
}

// ============================================================================
// MaterialCompiler Implementation
// ============================================================================

MaterialCompiler::MaterialCompiler(const ShaderGraph& graph)
    : m_graph(graph) {}

std::string MaterialCompiler::AllocateVariable(ShaderDataType type, const std::string& prefix) {
    return prefix + std::to_string(m_variableCounter++);
}

std::string MaterialCompiler::GetNodeOutputVariable(NodeId nodeId, const std::string& outputName) const {
    std::string key = std::to_string(nodeId) + "_" + outputName;
    auto it = m_nodeOutputVars.find(key);
    return it != m_nodeOutputVars.end() ? it->second : "";
}

void MaterialCompiler::SetNodeOutputVariable(NodeId nodeId, const std::string& outputName, const std::string& varName) {
    std::string key = std::to_string(nodeId) + "_" + outputName;
    m_nodeOutputVars[key] = varName;
}

void MaterialCompiler::AddLine(const std::string& code) {
    m_mainBody.push_back(code);
}

void MaterialCompiler::AddFunction(const std::string& signature, const std::string& body) {
    m_functions.push_back(signature + " {\n" + body + "\n}");
}

void MaterialCompiler::AddUniform(const std::string& type, const std::string& name) {
    m_uniforms.push_back("uniform " + type + " " + name + ";");
}

void MaterialCompiler::AddVarying(const std::string& type, const std::string& name) {
    m_varyings.push_back("in " + type + " " + name + ";");
}

bool MaterialCompiler::IsNodeCompiled(NodeId nodeId) const {
    return m_compiledNodes.count(nodeId) > 0;
}

void MaterialCompiler::MarkNodeCompiled(NodeId nodeId) {
    m_compiledNodes.insert(nodeId);
}

std::vector<ShaderNode::Ptr> MaterialCompiler::GetCompilationOrder() const {
    std::vector<ShaderNode::Ptr> order;
    std::unordered_set<NodeId> visited;

    std::function<void(ShaderNode::Ptr)> visit = [&](ShaderNode::Ptr node) {
        if (!node || visited.count(node->GetId())) return;
        visited.insert(node->GetId());

        // Visit dependencies first
        for (const auto& input : node->GetInputs()) {
            if (input.IsConnected()) {
                auto sourceNode = input.connectedNode.lock();
                if (sourceNode) {
                    visit(sourceNode);
                }
            }
        }

        order.push_back(node);
    };

    // Start from output node
    visit(m_graph.GetOutputNode());

    return order;
}

void MaterialCompiler::AddError(const std::string& error) {
    m_errors.push_back(error);
}

void MaterialCompiler::AddWarning(const std::string& warning) {
    m_warnings.push_back(warning);
}

std::string MaterialCompiler::CompileVertexShader() const {
    std::stringstream ss;

    ss << "#version 460 core\n\n";

    // Attributes
    ss << "layout(location = 0) in vec3 a_Position;\n";
    ss << "layout(location = 1) in vec3 a_Normal;\n";
    ss << "layout(location = 2) in vec2 a_TexCoord;\n";
    ss << "layout(location = 3) in vec4 a_Color;\n";
    ss << "layout(location = 4) in vec3 a_Tangent;\n";
    ss << "layout(location = 5) in vec3 a_Bitangent;\n\n";

    // Uniforms
    ss << "uniform mat4 u_Model;\n";
    ss << "uniform mat4 u_View;\n";
    ss << "uniform mat4 u_Projection;\n";
    ss << "uniform mat3 u_NormalMatrix;\n";
    ss << "uniform float u_Time;\n\n";

    // Outputs
    ss << "out vec3 v_WorldPos;\n";
    ss << "out vec3 v_Normal;\n";
    ss << "out vec2 v_TexCoord;\n";
    ss << "out vec4 v_Color;\n";
    ss << "out mat3 v_TBN;\n\n";

    // Main
    ss << "void main() {\n";
    ss << "    v_TexCoord = a_TexCoord;\n";
    ss << "    v_Color = a_Color;\n";
    ss << "    v_Normal = u_NormalMatrix * a_Normal;\n";
    ss << "    \n";
    ss << "    vec3 T = normalize(u_NormalMatrix * a_Tangent);\n";
    ss << "    vec3 B = normalize(u_NormalMatrix * a_Bitangent);\n";
    ss << "    vec3 N = normalize(v_Normal);\n";
    ss << "    v_TBN = mat3(T, B, N);\n";
    ss << "    \n";
    ss << "    vec4 worldPos = u_Model * vec4(a_Position, 1.0);\n";
    ss << "    v_WorldPos = worldPos.xyz;\n";
    ss << "    gl_Position = u_Projection * u_View * worldPos;\n";
    ss << "}\n";

    return ss.str();
}

std::string MaterialCompiler::CompileFragmentShader() const {
    std::stringstream ss;

    ss << "#version 460 core\n\n";

    // Outputs
    ss << "layout(location = 0) out vec4 FragColor;\n\n";

    // Inputs from vertex shader
    ss << "in vec3 v_WorldPos;\n";
    ss << "in vec3 v_Normal;\n";
    ss << "in vec2 v_TexCoord;\n";
    ss << "in vec4 v_Color;\n";
    ss << "in mat3 v_TBN;\n\n";

    // Standard uniforms
    ss << "uniform vec3 u_CameraPos;\n";
    ss << "uniform float u_Time;\n";
    ss << "uniform vec2 u_Resolution;\n\n";

    // Material parameter uniforms
    for (const auto& param : m_graph.GetParameters()) {
        ss << "uniform " << ShaderDataTypeToGLSL(param.type) << " u_" << param.name << ";\n";
    }
    ss << "\n";

    // Include noise library
    ss << GetNoiseLibraryGLSL() << "\n";
    ss << GetColorLibraryGLSL() << "\n";

    // Custom functions
    for (const auto& func : m_functions) {
        ss << func << "\n";
    }
    ss << "\n";

    // Main function
    ss << "void main() {\n";

    // Initialize material outputs with defaults
    ss << "    vec3 mat_baseColor = vec3(0.5);\n";
    ss << "    float mat_metallic = 0.0;\n";
    ss << "    float mat_roughness = 0.5;\n";
    ss << "    vec3 mat_normal = v_Normal;\n";
    ss << "    vec3 mat_emissive = vec3(0.0);\n";
    ss << "    float mat_emissiveStrength = 1.0;\n";
    ss << "    float mat_ao = 1.0;\n";
    ss << "    float mat_opacity = 1.0;\n";
    ss << "    \n";

    // Generate code for all nodes in order
    auto order = GetCompilationOrder();
    for (const auto& node : order) {
        if (!IsNodeCompiled(node->GetId())) {
            std::string code = node->GenerateCode(const_cast<MaterialCompiler&>(*this));
            if (!code.empty()) {
                ss << "    " << code << "\n";
            }
            const_cast<MaterialCompiler*>(this)->MarkNodeCompiled(node->GetId());
        }
    }

    // Final output
    ss << "    \n";
    ss << "    // Simple output for now - would use full PBR lighting\n";
    ss << "    vec3 finalColor = mat_baseColor * mat_ao + mat_emissive * mat_emissiveStrength;\n";
    ss << "    FragColor = vec4(finalColor, mat_opacity);\n";
    ss << "}\n";

    return ss.str();
}

std::string MaterialCompiler::CompileGeometryShader() const {
    return "";  // Optional, not always needed
}

// ============================================================================
// ShaderNodeFactory Implementation
// ============================================================================

ShaderNodeFactory& ShaderNodeFactory::Instance() {
    static ShaderNodeFactory instance;
    return instance;
}

void ShaderNodeFactory::RegisterNode(const std::string& typeName, NodeCategory category,
                                      const std::string& displayName, CreatorFunc creator) {
    m_nodeTypes[typeName] = {displayName, category, creator};
}

ShaderNode::Ptr ShaderNodeFactory::Create(const std::string& typeName) const {
    auto it = m_nodeTypes.find(typeName);
    if (it != m_nodeTypes.end()) {
        return it->second.creator();
    }
    return nullptr;
}

std::vector<std::string> ShaderNodeFactory::GetNodeTypes() const {
    std::vector<std::string> types;
    for (const auto& [name, _] : m_nodeTypes) {
        types.push_back(name);
    }
    return types;
}

std::vector<std::string> ShaderNodeFactory::GetNodeTypesInCategory(NodeCategory category) const {
    std::vector<std::string> types;
    for (const auto& [name, info] : m_nodeTypes) {
        if (info.category == category) {
            types.push_back(name);
        }
    }
    return types;
}

NodeCategory ShaderNodeFactory::GetNodeCategory(const std::string& typeName) const {
    auto it = m_nodeTypes.find(typeName);
    return it != m_nodeTypes.end() ? it->second.category : NodeCategory::Custom;
}

std::string ShaderNodeFactory::GetNodeDisplayName(const std::string& typeName) const {
    auto it = m_nodeTypes.find(typeName);
    return it != m_nodeTypes.end() ? it->second.displayName : typeName;
}

void ShaderNodeFactory::RegisterBuiltinNodes() {
    // Input nodes
    RegisterNode("MaterialOutput", NodeCategory::Output, "Material Output", []() { return std::make_shared<MaterialOutputNode>(); });
    RegisterNode("TexCoord", NodeCategory::Input, "Texture Coordinates", []() { return std::make_shared<TexCoordNode>(); });
    RegisterNode("WorldPosition", NodeCategory::Input, "World Position", []() { return std::make_shared<WorldPositionNode>(); });
    RegisterNode("WorldNormal", NodeCategory::Input, "World Normal", []() { return std::make_shared<WorldNormalNode>(); });
    RegisterNode("VertexColor", NodeCategory::Input, "Vertex Color", []() { return std::make_shared<VertexColorNode>(); });
    RegisterNode("ViewDirection", NodeCategory::Input, "View Direction", []() { return std::make_shared<ViewDirectionNode>(); });
    RegisterNode("Time", NodeCategory::Input, "Time", []() { return std::make_shared<TimeNode>(); });
    RegisterNode("ScreenPosition", NodeCategory::Input, "Screen Position", []() { return std::make_shared<ScreenPositionNode>(); });

    // Parameters
    RegisterNode("FloatConstant", NodeCategory::Parameter, "Float", []() { return std::make_shared<FloatConstantNode>(); });
    RegisterNode("VectorConstant", NodeCategory::Parameter, "Vector", []() { return std::make_shared<VectorConstantNode>(); });
    RegisterNode("ColorConstant", NodeCategory::Parameter, "Color", []() { return std::make_shared<ColorConstantNode>(); });
    RegisterNode("Parameter", NodeCategory::Parameter, "Parameter", []() { return std::make_shared<ParameterNode>(); });

    // Textures
    RegisterNode("Texture2D", NodeCategory::Texture, "Texture 2D", []() { return std::make_shared<Texture2DNode>(); });
    RegisterNode("NormalMap", NodeCategory::Texture, "Normal Map", []() { return std::make_shared<NormalMapNode>(); });
    RegisterNode("TextureCube", NodeCategory::Texture, "Texture Cube", []() { return std::make_shared<TextureCubeNode>(); });

    // Math Basic
    RegisterNode("Add", NodeCategory::MathBasic, "Add", []() { return std::make_shared<AddNode>(); });
    RegisterNode("Subtract", NodeCategory::MathBasic, "Subtract", []() { return std::make_shared<SubtractNode>(); });
    RegisterNode("Multiply", NodeCategory::MathBasic, "Multiply", []() { return std::make_shared<MultiplyNode>(); });
    RegisterNode("Divide", NodeCategory::MathBasic, "Divide", []() { return std::make_shared<DivideNode>(); });
    RegisterNode("OneMinus", NodeCategory::MathBasic, "One Minus", []() { return std::make_shared<OneMinusNode>(); });
    RegisterNode("Abs", NodeCategory::MathBasic, "Absolute", []() { return std::make_shared<AbsNode>(); });
    RegisterNode("Negate", NodeCategory::MathBasic, "Negate", []() { return std::make_shared<NegateNode>(); });
    RegisterNode("Min", NodeCategory::MathBasic, "Min", []() { return std::make_shared<MinNode>(); });
    RegisterNode("Max", NodeCategory::MathBasic, "Max", []() { return std::make_shared<MaxNode>(); });
    RegisterNode("Clamp", NodeCategory::MathBasic, "Clamp", []() { return std::make_shared<ClampNode>(); });
    RegisterNode("Saturate", NodeCategory::MathBasic, "Saturate", []() { return std::make_shared<SaturateNode>(); });
    RegisterNode("Floor", NodeCategory::MathBasic, "Floor", []() { return std::make_shared<FloorNode>(); });
    RegisterNode("Ceil", NodeCategory::MathBasic, "Ceil", []() { return std::make_shared<CeilNode>(); });
    RegisterNode("Round", NodeCategory::MathBasic, "Round", []() { return std::make_shared<RoundNode>(); });
    RegisterNode("Frac", NodeCategory::MathBasic, "Frac", []() { return std::make_shared<FracNode>(); });
    RegisterNode("Mod", NodeCategory::MathBasic, "Modulo", []() { return std::make_shared<ModNode>(); });

    // Math Advanced
    RegisterNode("Power", NodeCategory::MathAdvanced, "Power", []() { return std::make_shared<PowerNode>(); });
    RegisterNode("Sqrt", NodeCategory::MathAdvanced, "Square Root", []() { return std::make_shared<SqrtNode>(); });
    RegisterNode("Log", NodeCategory::MathAdvanced, "Log", []() { return std::make_shared<LogNode>(); });
    RegisterNode("Exp", NodeCategory::MathAdvanced, "Exp", []() { return std::make_shared<ExpNode>(); });

    // Trig
    RegisterNode("Sin", NodeCategory::MathTrig, "Sin", []() { return std::make_shared<SinNode>(); });
    RegisterNode("Cos", NodeCategory::MathTrig, "Cos", []() { return std::make_shared<CosNode>(); });
    RegisterNode("Tan", NodeCategory::MathTrig, "Tan", []() { return std::make_shared<TanNode>(); });
    RegisterNode("Atan2", NodeCategory::MathTrig, "Atan2", []() { return std::make_shared<Atan2Node>(); });

    // Vector
    RegisterNode("Dot", NodeCategory::MathVector, "Dot Product", []() { return std::make_shared<DotNode>(); });
    RegisterNode("Cross", NodeCategory::MathVector, "Cross Product", []() { return std::make_shared<CrossNode>(); });
    RegisterNode("Normalize", NodeCategory::MathVector, "Normalize", []() { return std::make_shared<NormalizeNode>(); });
    RegisterNode("Length", NodeCategory::MathVector, "Length", []() { return std::make_shared<LengthNode>(); });
    RegisterNode("Distance", NodeCategory::MathVector, "Distance", []() { return std::make_shared<DistanceNode>(); });
    RegisterNode("Reflect", NodeCategory::MathVector, "Reflect", []() { return std::make_shared<ReflectNode>(); });

    // Interpolation
    RegisterNode("Lerp", NodeCategory::MathInterpolation, "Lerp", []() { return std::make_shared<LerpNode>(); });
    RegisterNode("SmoothStep", NodeCategory::MathInterpolation, "Smooth Step", []() { return std::make_shared<SmoothStepNode>(); });
    RegisterNode("Step", NodeCategory::MathInterpolation, "Step", []() { return std::make_shared<StepNode>(); });
    RegisterNode("Remap", NodeCategory::MathInterpolation, "Remap", []() { return std::make_shared<RemapNode>(); });

    // Utility
    RegisterNode("Swizzle", NodeCategory::Utility, "Swizzle", []() { return std::make_shared<SwizzleNode>(); });
    RegisterNode("Split", NodeCategory::Utility, "Split", []() { return std::make_shared<SplitNode>(); });
    RegisterNode("Combine", NodeCategory::Utility, "Combine", []() { return std::make_shared<CombineNode>(); });
    RegisterNode("Append", NodeCategory::Utility, "Append", []() { return std::make_shared<AppendNode>(); });
    RegisterNode("DDX", NodeCategory::Utility, "DDX", []() { return std::make_shared<DDXNode>(); });
    RegisterNode("DDY", NodeCategory::Utility, "DDY", []() { return std::make_shared<DDYNode>(); });

    // Logic
    RegisterNode("If", NodeCategory::Logic, "If", []() { return std::make_shared<IfNode>(); });
    RegisterNode("Compare", NodeCategory::Logic, "Compare", []() { return std::make_shared<CompareNode>(); });

    // Color
    RegisterNode("Blend", NodeCategory::Color, "Blend", []() { return std::make_shared<BlendNode>(); });
    RegisterNode("HSV", NodeCategory::Color, "HSV Adjust", []() { return std::make_shared<HSVNode>(); });
    RegisterNode("Contrast", NodeCategory::Color, "Contrast", []() { return std::make_shared<ContrastNode>(); });
    RegisterNode("Grayscale", NodeCategory::Color, "Grayscale", []() { return std::make_shared<GrayscaleNode>(); });

    // Noise
    RegisterNode("PerlinNoise", NodeCategory::Noise, "Perlin Noise", []() { return std::make_shared<PerlinNoiseNode>(); });
    RegisterNode("SimplexNoise", NodeCategory::Noise, "Simplex Noise", []() { return std::make_shared<SimplexNoiseNode>(); });
    RegisterNode("WorleyNoise", NodeCategory::Noise, "Worley Noise", []() { return std::make_shared<WorleyNoiseNode>(); });
    RegisterNode("Voronoi", NodeCategory::Noise, "Voronoi", []() { return std::make_shared<VoronoiNode>(); });
    RegisterNode("FBM", NodeCategory::Noise, "FBM", []() { return std::make_shared<FBMNode>(); });

    // Pattern
    RegisterNode("Checkerboard", NodeCategory::Pattern, "Checkerboard", []() { return std::make_shared<CheckerboardNode>(); });
    RegisterNode("Brick", NodeCategory::Pattern, "Brick", []() { return std::make_shared<BrickNode>(); });
    RegisterNode("Gradient", NodeCategory::Pattern, "Gradient", []() { return std::make_shared<GradientPatternNode>(); });
    RegisterNode("PolarCoordinates", NodeCategory::Pattern, "Polar Coordinates", []() { return std::make_shared<PolarCoordinatesNode>(); });
    RegisterNode("Triplanar", NodeCategory::Pattern, "Triplanar", []() { return std::make_shared<TriplanarNode>(); });
    RegisterNode("TilingOffset", NodeCategory::Pattern, "Tiling & Offset", []() { return std::make_shared<TilingOffsetNode>(); });
    RegisterNode("RotateUV", NodeCategory::Pattern, "Rotate UV", []() { return std::make_shared<RotateUVNode>(); });
}

} // namespace Nova
