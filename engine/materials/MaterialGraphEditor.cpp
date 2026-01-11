#include "MaterialGraphEditor.hpp"
#include "AdvancedMaterial.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <queue>

using json = nlohmann::json;

namespace Engine {

// MaterialGraph implementation
MaterialGraph::MaterialGraph() {
}

MaterialGraph::~MaterialGraph() {
}

int MaterialGraph::AddNode(std::unique_ptr<MaterialNode> node) {
    int id = m_nextNodeId++;
    node->id = id;
    m_nodes[id] = std::move(node);
    return id;
}

void MaterialGraph::RemoveNode(int nodeId) {
    // Remove all connections to/from this node
    std::vector<int> connectionsToRemove;
    for (const auto& [connId, conn] : m_connections) {
        if (conn.startNodeId == nodeId || conn.endNodeId == nodeId) {
            connectionsToRemove.push_back(connId);
        }
    }

    for (int connId : connectionsToRemove) {
        RemoveConnection(connId);
    }

    m_nodes.erase(nodeId);
}

MaterialNode* MaterialGraph::GetNode(int nodeId) {
    auto it = m_nodes.find(nodeId);
    return (it != m_nodes.end()) ? it->second.get() : nullptr;
}

const MaterialNode* MaterialGraph::GetNode(int nodeId) const {
    auto it = m_nodes.find(nodeId);
    return (it != m_nodes.end()) ? it->second.get() : nullptr;
}

std::vector<MaterialNode*> MaterialGraph::GetAllNodes() {
    std::vector<MaterialNode*> nodes;
    for (auto& [id, node] : m_nodes) {
        nodes.push_back(node.get());
    }
    return nodes;
}

bool MaterialGraph::AddConnection(int startPinId, int endPinId) {
    // Find nodes that own these pins
    MaterialNode* startNode = FindNodeByPin(startPinId);
    MaterialNode* endNode = FindNodeByPin(endPinId);

    if (!startNode || !endNode) {
        return false;
    }

    // Create connection
    MaterialConnection conn;
    conn.id = m_nextConnectionId++;
    conn.startPinId = startPinId;
    conn.endPinId = endPinId;
    conn.startNodeId = startNode->id;
    conn.endNodeId = endNode->id;

    // Check for cycles
    m_connections[conn.id] = conn;
    if (HasCycle()) {
        m_connections.erase(conn.id);
        return false;
    }

    return true;
}

void MaterialGraph::RemoveConnection(int connectionId) {
    m_connections.erase(connectionId);
}

void MaterialGraph::RemoveConnectionsFromPin(int pinId) {
    std::vector<int> connectionsToRemove;
    for (const auto& [connId, conn] : m_connections) {
        if (conn.startPinId == pinId || conn.endPinId == pinId) {
            connectionsToRemove.push_back(connId);
        }
    }

    for (int connId : connectionsToRemove) {
        RemoveConnection(connId);
    }
}

MaterialConnection* MaterialGraph::GetConnection(int connectionId) {
    auto it = m_connections.find(connectionId);
    return (it != m_connections.end()) ? &it->second : nullptr;
}

std::vector<MaterialConnection> MaterialGraph::GetAllConnections() const {
    std::vector<MaterialConnection> connections;
    for (const auto& [id, conn] : m_connections) {
        connections.push_back(conn);
    }
    return connections;
}

bool MaterialGraph::Validate() const {
    m_validationErrors.clear();

    // Check for cycles
    if (HasCycle()) {
        m_validationErrors.push_back("Graph contains cycles");
    }

    // Check for orphaned nodes
    for (const auto& [id, node] : m_nodes) {
        bool hasConnections = false;
        for (const auto& [connId, conn] : m_connections) {
            if (conn.startNodeId == id || conn.endNodeId == id) {
                hasConnections = true;
                break;
            }
        }

        if (!hasConnections && node->type != MaterialNodeType::OutputAlbedo) {
            m_validationErrors.push_back("Node '" + node->name + "' has no connections");
        }
    }

    // Check for output nodes
    bool hasOutput = false;
    for (const auto& [id, node] : m_nodes) {
        if (node->type >= MaterialNodeType::OutputAlbedo &&
            node->type <= MaterialNodeType::OutputAO) {
            hasOutput = true;
            break;
        }
    }

    if (!hasOutput) {
        m_validationErrors.push_back("Graph has no output nodes");
    }

    return m_validationErrors.empty();
}

std::vector<std::string> MaterialGraph::GetValidationErrors() const {
    return m_validationErrors;
}

std::vector<int> MaterialGraph::GetTopologicalOrder() const {
    std::vector<int> order;
    std::set<int> visited;
    std::map<int, int> inDegree;

    // Calculate in-degree for each node
    for (const auto& [id, node] : m_nodes) {
        inDegree[id] = 0;
    }

    for (const auto& [connId, conn] : m_connections) {
        inDegree[conn.endNodeId]++;
    }

    // Queue nodes with in-degree 0
    std::queue<int> queue;
    for (const auto& [id, degree] : inDegree) {
        if (degree == 0) {
            queue.push(id);
        }
    }

    // Process nodes
    while (!queue.empty()) {
        int nodeId = queue.front();
        queue.pop();
        order.push_back(nodeId);
        visited.insert(nodeId);

        // Reduce in-degree of connected nodes
        for (const auto& [connId, conn] : m_connections) {
            if (conn.startNodeId == nodeId) {
                inDegree[conn.endNodeId]--;
                if (inDegree[conn.endNodeId] == 0 && visited.find(conn.endNodeId) == visited.end()) {
                    queue.push(conn.endNodeId);
                }
            }
        }
    }

    return order;
}

std::string MaterialGraph::CompileToGLSL() const {
    MaterialGraphCompiler compiler(this);
    return compiler.Compile();
}

std::string MaterialGraph::GenerateFragmentShader() const {
    MaterialGraphCompiler compiler(this);
    return compiler.GenerateFragmentShader();
}

void MaterialGraph::Save(const std::string& filepath) const {
    json j = Serialize();

    std::ofstream file(filepath);
    file << j.dump(4);
    file.close();
}

void MaterialGraph::Load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return;
    }

    json j;
    file >> j;
    Deserialize(j);

    file.close();
}

json MaterialGraph::Serialize() const {
    json j;
    j["name"] = name;
    j["viewportOffset"] = {viewportOffset.x, viewportOffset.y};
    j["viewportZoom"] = viewportZoom;

    // Serialize nodes
    j["nodes"] = json::array();
    for (const auto& [id, node] : m_nodes) {
        json nodeJson;
        node->Serialize(nodeJson);
        j["nodes"].push_back(nodeJson);
    }

    // Serialize connections
    j["connections"] = json::array();
    for (const auto& [id, conn] : m_connections) {
        j["connections"].push_back({
            {"id", conn.id},
            {"startPinId", conn.startPinId},
            {"endPinId", conn.endPinId},
            {"startNodeId", conn.startNodeId},
            {"endNodeId", conn.endNodeId}
        });
    }

    return j;
}

void MaterialGraph::Deserialize(const json& j) {
    name = j["name"];
    viewportOffset = glm::vec2(j["viewportOffset"][0], j["viewportOffset"][1]);
    viewportZoom = j["viewportZoom"];

    // Deserialize nodes
    m_nodes.clear();
    for (const auto& nodeJson : j["nodes"]) {
        MaterialNodeType type = static_cast<MaterialNodeType>(nodeJson["type"]);
        auto node = MaterialNodeFactory::CreateNode(type);
        if (node) {
            node->Deserialize(nodeJson);
            m_nodes[node->id] = std::move(node);
            m_nextNodeId = std::max(m_nextNodeId, node->id + 1);
        }
    }

    // Deserialize connections
    m_connections.clear();
    for (const auto& connJson : j["connections"]) {
        MaterialConnection conn;
        conn.id = connJson["id"];
        conn.startPinId = connJson["startPinId"];
        conn.endPinId = connJson["endPinId"];
        conn.startNodeId = connJson["startNodeId"];
        conn.endNodeId = connJson["endNodeId"];
        m_connections[conn.id] = conn;
        m_nextConnectionId = std::max(m_nextConnectionId, conn.id + 1);
    }
}

bool MaterialGraph::HasCycle() const {
    std::set<int> visited;
    std::set<int> recStack;

    for (const auto& [id, node] : m_nodes) {
        if (visited.find(id) == visited.end()) {
            if (HasCycleUtil(id, visited, recStack)) {
                return true;
            }
        }
    }

    return false;
}

bool MaterialGraph::HasCycleUtil(int nodeId, std::set<int>& visited, std::set<int>& recStack) const {
    visited.insert(nodeId);
    recStack.insert(nodeId);

    // Check all nodes connected from this node
    for (const auto& [connId, conn] : m_connections) {
        if (conn.startNodeId == nodeId) {
            int nextNode = conn.endNodeId;

            if (visited.find(nextNode) == visited.end()) {
                if (HasCycleUtil(nextNode, visited, recStack)) {
                    return true;
                }
            } else if (recStack.find(nextNode) != recStack.end()) {
                return true;
            }
        }
    }

    recStack.erase(nodeId);
    return false;
}

MaterialNode* MaterialGraph::FindNodeByPin(int pinId) {
    for (auto& [id, node] : m_nodes) {
        for (auto& [name, pin] : node->inputs) {
            if (pin.id == pinId) {
                return node.get();
            }
        }
        for (auto& [name, pin] : node->outputs) {
            if (pin.id == pinId) {
                return node.get();
            }
        }
    }
    return nullptr;
}

const MaterialNode* MaterialGraph::FindNodeByPin(int pinId) const {
    for (const auto& [id, node] : m_nodes) {
        for (const auto& [name, pin] : node->inputs) {
            if (pin.id == pinId) {
                return node.get();
            }
        }
        for (const auto& [name, pin] : node->outputs) {
            if (pin.id == pinId) {
                return node.get();
            }
        }
    }
    return nullptr;
}

// MaterialGraphCompiler implementation
MaterialGraphCompiler::MaterialGraphCompiler(const MaterialGraph* graph)
    : m_graph(graph) {
}

std::string MaterialGraphCompiler::Compile() {
    return GenerateFragmentShader();
}

std::string MaterialGraphCompiler::GenerateFragmentShader() {
    std::ostringstream code;

    code << GenerateHeader();
    code << GenerateUniforms();
    code << GenerateInputs();
    code << GenerateOutputs();
    code << GenerateHelperFunctions();
    code << GenerateMainFunction();

    return code.str();
}

std::string MaterialGraphCompiler::GenerateHeader() {
    std::ostringstream code;
    code << "#version " << options.shaderVersion << "\n\n";

    if (options.includeComments) {
        code << "// Generated by Material Graph Editor\n";
        code << "// Graph: " << m_graph->name << "\n\n";
    }

    return code.str();
}

std::string MaterialGraphCompiler::GenerateUniforms() {
    std::ostringstream code;
    code << "// Uniforms\n";
    code << "uniform float u_Time;\n";
    code << "uniform vec3 u_CameraPos;\n";
    code << "uniform sampler2D u_Texture;\n\n";
    return code.str();
}

std::string MaterialGraphCompiler::GenerateInputs() {
    std::ostringstream code;
    code << "// Inputs\n";
    code << "in vec2 v_TexCoord;\n";
    code << "in vec3 v_WorldPos;\n";
    code << "in vec3 v_Normal;\n";
    code << "in vec3 v_Tangent;\n";
    code << "in vec3 v_Bitangent;\n";
    code << "in vec4 v_VertexColor;\n\n";
    return code.str();
}

std::string MaterialGraphCompiler::GenerateOutputs() {
    std::ostringstream code;
    code << "// Outputs\n";
    code << "out vec4 FragColor;\n\n";
    return code.str();
}

std::string MaterialGraphCompiler::GenerateHelperFunctions() {
    std::ostringstream code;
    code << "// Helper Functions\n";

    // Temperature to RGB
    code << "vec3 temperatureToRGB(float temp) {\n";
    code << "    temp = temp / 100.0;\n";
    code << "    float r, g, b;\n";
    code << "    if (temp <= 66.0) {\n";
    code << "        r = 1.0;\n";
    code << "        g = clamp(0.39 * log(temp) - 0.63, 0.0, 1.0);\n";
    code << "    } else {\n";
    code << "        r = clamp(1.29 * pow(temp - 60.0, -0.13), 0.0, 1.0);\n";
    code << "        g = clamp(1.13 * pow(temp - 60.0, -0.08), 0.0, 1.0);\n";
    code << "    }\n";
    code << "    if (temp >= 66.0) b = 1.0;\n";
    code << "    else if (temp <= 19.0) b = 0.0;\n";
    code << "    else b = clamp(0.54 * log(temp - 10.0) - 1.19, 0.0, 1.0);\n";
    code << "    return vec3(r, g, b);\n";
    code << "}\n\n";

    // RGB to HSV
    code << "vec3 rgbToHsv(vec3 rgb) {\n";
    code << "    float maxC = max(max(rgb.r, rgb.g), rgb.b);\n";
    code << "    float minC = min(min(rgb.r, rgb.g), rgb.b);\n";
    code << "    float delta = maxC - minC;\n";
    code << "    vec3 hsv = vec3(0.0);\n";
    code << "    if (delta > 0.0) {\n";
    code << "        if (maxC == rgb.r) hsv.x = mod((rgb.g - rgb.b) / delta, 6.0);\n";
    code << "        else if (maxC == rgb.g) hsv.x = ((rgb.b - rgb.r) / delta) + 2.0;\n";
    code << "        else hsv.x = ((rgb.r - rgb.g) / delta) + 4.0;\n";
    code << "        hsv.x = hsv.x / 6.0;\n";
    code << "    }\n";
    code << "    hsv.y = (maxC > 0.0) ? (delta / maxC) : 0.0;\n";
    code << "    hsv.z = maxC;\n";
    code << "    return hsv;\n";
    code << "}\n\n";

    // Perlin noise
    code << "float perlinNoise(vec3 p, int octaves) {\n";
    code << "    // Simplified Perlin noise\n";
    code << "    return fract(sin(dot(p, vec3(12.9898, 78.233, 45.164))) * 43758.5453);\n";
    code << "}\n\n";

    // GGX BRDF
    code << "vec3 GGX_BRDF(vec3 N, vec3 V, vec3 L, float roughness, vec3 F0) {\n";
    code << "    vec3 H = normalize(V + L);\n";
    code << "    float NdotH = max(dot(N, H), 0.0);\n";
    code << "    float NdotV = max(dot(N, V), 0.0);\n";
    code << "    float NdotL = max(dot(N, L), 0.0);\n";
    code << "    float alpha = roughness * roughness;\n";
    code << "    float alpha2 = alpha * alpha;\n";
    code << "    float denom = (NdotH * NdotH * (alpha2 - 1.0) + 1.0);\n";
    code << "    float D = alpha2 / (3.14159 * denom * denom);\n";
    code << "    float k = alpha / 2.0;\n";
    code << "    float G1V = NdotV / (NdotV * (1.0 - k) + k);\n";
    code << "    float G1L = NdotL / (NdotL * (1.0 - k) + k);\n";
    code << "    float G = G1V * G1L;\n";
    code << "    float VdotH = max(dot(V, H), 0.0);\n";
    code << "    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);\n";
    code << "    return D * G * F / max(4.0 * NdotV * NdotL, 0.001);\n";
    code << "}\n\n";

    return code.str();
}

std::string MaterialGraphCompiler::GenerateMainFunction() {
    std::ostringstream code;
    code << "void main() {\n";

    // Get topological order
    std::vector<int> order = m_graph->GetTopologicalOrder();

    std::map<int, std::string> varNames;

    // Generate code for each node in order
    for (int nodeId : order) {
        const MaterialNode* node = m_graph->GetNode(nodeId);
        if (node) {
            code << CompileNode(node, varNames);
        }
    }

    // Default output if no output nodes found
    code << "    FragColor = vec4(1.0, 0.0, 1.0, 1.0); // Error: magenta\n";

    code << "}\n";

    return code.str();
}

std::string MaterialGraphCompiler::CompileNode(const MaterialNode* node, std::map<int, std::string>& varNames) {
    // Map input pins to variable names
    std::map<std::string, std::string> inputVarNames;

    for (const auto& [pinName, pin] : node->inputs) {
        // Find connection to this input
        bool foundConnection = false;
        for (const auto& conn : m_graph->GetAllConnections()) {
            if (conn.endPinId == pin.id) {
                // Find output pin name
                const MaterialNode* sourceNode = m_graph->GetNode(conn.startNodeId);
                if (sourceNode) {
                    for (const auto& [outPinName, outPin] : sourceNode->outputs) {
                        if (outPin.id == conn.startPinId) {
                            inputVarNames[pinName] = varNames[sourceNode->id];
                            foundConnection = true;
                            break;
                        }
                    }
                }
                break;
            }
        }

        if (!foundConnection) {
            // Use default value
            inputVarNames[pinName] = "0.0";  // Simplified
        }
    }

    // Generate variable name for this node's output
    std::string outputVarName = "node_" + std::to_string(node->id);
    varNames[node->id] = outputVarName;

    // Generate GLSL code
    return "    " + node->GenerateGLSL(inputVarNames, outputVarName);
}

// MaterialGraphEditor implementation
MaterialGraphEditor::MaterialGraphEditor() {
    InitializeNodePalette();
}

MaterialGraphEditor::~MaterialGraphEditor() {
}

void MaterialGraphEditor::SetGraph(std::shared_ptr<MaterialGraph> graph) {
    m_graph = graph;
}

std::shared_ptr<MaterialGraph> MaterialGraphEditor::GetGraph() const {
    return m_graph;
}

void MaterialGraphEditor::NewGraph() {
    m_graph = std::make_shared<MaterialGraph>();
    ClearSelection();
}

void MaterialGraphEditor::LoadGraph(const std::string& filepath) {
    m_graph = std::make_shared<MaterialGraph>();
    m_graph->Load(filepath);
    ClearSelection();
}

void MaterialGraphEditor::SaveGraph(const std::string& filepath) {
    if (m_graph) {
        m_graph->Save(filepath);
    }
}

void MaterialGraphEditor::InitializeNodePalette() {
    m_nodePalette["Input"] = {
        MaterialNodeType::UV,
        MaterialNodeType::WorldPos,
        MaterialNodeType::Normal,
        MaterialNodeType::ViewDir,
        MaterialNodeType::Time
    };

    m_nodePalette["Math"] = {
        MaterialNodeType::Add,
        MaterialNodeType::Multiply,
        MaterialNodeType::Lerp,
        MaterialNodeType::Clamp
    };

    m_nodePalette["Texture"] = {
        MaterialNodeType::TextureSample,
        MaterialNodeType::NoisePerlin
    };

    m_nodePalette["Color"] = {
        MaterialNodeType::RGB_to_HSV,
        MaterialNodeType::HSV_to_RGB
    };

    m_nodePalette["Lighting"] = {
        MaterialNodeType::Fresnel,
        MaterialNodeType::GGX_BRDF
    };

    m_nodePalette["Physics"] = {
        MaterialNodeType::Temperature_to_RGB,
        MaterialNodeType::Blackbody,
        MaterialNodeType::Dispersion
    };
}

void MaterialGraphEditor::CompileGraph() {
    if (m_graph) {
        m_compiledShaderCode = m_graph->CompileToGLSL();
    }
}

std::string MaterialGraphEditor::GetCompiledShaderCode() const {
    return m_compiledShaderCode;
}

// Template implementations
std::shared_ptr<MaterialGraph> MaterialGraphTemplates::CreateBasicPBR() {
    auto graph = std::make_shared<MaterialGraph>();
    graph->name = "Basic PBR";

    // Create nodes: UV -> Texture -> Albedo output
    auto uvNode = MaterialNodeFactory::CreateNode(MaterialNodeType::UV);
    uvNode->position = glm::vec2(100.0f, 200.0f);
    int uvId = graph->AddNode(std::move(uvNode));

    auto texNode = MaterialNodeFactory::CreateNode(MaterialNodeType::TextureSample);
    texNode->position = glm::vec2(300.0f, 200.0f);
    int texId = graph->AddNode(std::move(texNode));

    return graph;
}

} // namespace Engine
