#include "MaterialGraphEditor.hpp"
#include "AdvancedMaterial.hpp"
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <queue>
#include <set>

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

// Command classes for undo/redo

class MaterialGraphEditor::AddNodeCommand : public MaterialGraphEditor::EditorCommand {
public:
    AddNodeCommand(MaterialGraph* graph, MaterialNodeType type, const glm::vec2& position)
        : m_graph(graph), m_type(type), m_position(position), m_nodeId(-1) {}

    void Execute() override {
        auto node = MaterialNodeFactory::CreateNode(m_type);
        if (node) {
            node->position = m_position;
            m_nodeId = m_graph->AddNode(std::move(node));
        }
    }

    void Undo() override {
        if (m_nodeId >= 0) {
            m_graph->RemoveNode(m_nodeId);
        }
    }

    int GetNodeId() const { return m_nodeId; }

private:
    MaterialGraph* m_graph;
    MaterialNodeType m_type;
    glm::vec2 m_position;
    int m_nodeId;
};

class MaterialGraphEditor::DeleteNodeCommand : public MaterialGraphEditor::EditorCommand {
public:
    DeleteNodeCommand(MaterialGraph* graph, int nodeId)
        : m_graph(graph), m_nodeId(nodeId) {}

    void Execute() override {
        // Store the node data before deletion for undo
        MaterialNode* node = m_graph->GetNode(m_nodeId);
        if (node) {
            // Serialize the node for later restoration
            node->Serialize(m_nodeData);

            // Store all connections involving this node
            m_connections.clear();
            for (const auto& conn : m_graph->GetAllConnections()) {
                if (conn.startNodeId == m_nodeId || conn.endNodeId == m_nodeId) {
                    m_connections.push_back(conn);
                }
            }

            // Remove the node (this also removes its connections)
            m_graph->RemoveNode(m_nodeId);
        }
    }

    void Undo() override {
        // Recreate the node from stored data
        if (!m_nodeData.empty()) {
            MaterialNodeType type = static_cast<MaterialNodeType>(m_nodeData["type"]);
            auto node = MaterialNodeFactory::CreateNode(type);
            if (node) {
                node->Deserialize(m_nodeData);
                node->id = m_nodeId;  // Restore original ID
                m_graph->AddNode(std::move(node));

                // Restore connections
                for (const auto& conn : m_connections) {
                    m_graph->AddConnection(conn.startPinId, conn.endPinId);
                }
            }
        }
    }

private:
    MaterialGraph* m_graph;
    int m_nodeId;
    nlohmann::json m_nodeData;
    std::vector<MaterialConnection> m_connections;
};

class MaterialGraphEditor::AddConnectionCommand : public MaterialGraphEditor::EditorCommand {
public:
    AddConnectionCommand(MaterialGraph* graph, int startPinId, int endPinId)
        : m_graph(graph), m_startPinId(startPinId), m_endPinId(endPinId), m_connectionId(-1) {}

    void Execute() override {
        if (m_graph->AddConnection(m_startPinId, m_endPinId)) {
            // Find the connection ID we just created
            for (const auto& conn : m_graph->GetAllConnections()) {
                if (conn.startPinId == m_startPinId && conn.endPinId == m_endPinId) {
                    m_connectionId = conn.id;
                    break;
                }
            }
        }
    }

    void Undo() override {
        if (m_connectionId >= 0) {
            m_graph->RemoveConnection(m_connectionId);
        }
    }

    int GetConnectionId() const { return m_connectionId; }

private:
    MaterialGraph* m_graph;
    int m_startPinId;
    int m_endPinId;
    int m_connectionId;
};

class MaterialGraphEditor::DeleteConnectionCommand : public MaterialGraphEditor::EditorCommand {
public:
    DeleteConnectionCommand(MaterialGraph* graph, int connectionId)
        : m_graph(graph), m_connectionId(connectionId) {}

    void Execute() override {
        // Store connection data before deletion
        MaterialConnection* conn = m_graph->GetConnection(m_connectionId);
        if (conn) {
            m_startPinId = conn->startPinId;
            m_endPinId = conn->endPinId;
            m_startNodeId = conn->startNodeId;
            m_endNodeId = conn->endNodeId;
            m_graph->RemoveConnection(m_connectionId);
        }
    }

    void Undo() override {
        // Restore the connection
        if (m_graph->AddConnection(m_startPinId, m_endPinId)) {
            // The connection ID might be different after restoration
            // but the logical connection is restored
        }
    }

private:
    MaterialGraph* m_graph;
    int m_connectionId;
    int m_startPinId = 0;
    int m_endPinId = 0;
    int m_startNodeId = 0;
    int m_endNodeId = 0;
};

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
    m_currentFilePath.clear();
    m_clipboardData.clear();
    m_compiledShaderCode.clear();
    ClearSelection();

    // Clear undo/redo stacks
    m_undoStack.clear();
    m_redoStack.clear();
}

void MaterialGraphEditor::LoadGraph(const std::string& filepath) {
    m_graph = std::make_shared<MaterialGraph>();
    m_graph->Load(filepath);
    m_currentFilePath = filepath;
    ClearSelection();

    // Clear undo/redo stacks when loading a new graph
    m_undoStack.clear();
    m_redoStack.clear();

    // Auto-compile if enabled
    if (settings.autoCompile) {
        CompileGraph();
    }
}

void MaterialGraphEditor::SaveGraph(const std::string& filepath) {
    if (m_graph) {
        m_graph->Save(filepath);
        m_currentFilePath = filepath;
    }
}

bool MaterialGraphEditor::ExportShader(const std::string& filepath) {
    if (!m_graph) return false;

    // Compile the graph if needed
    if (m_compiledShaderCode.empty()) {
        CompileGraph();
    }

    if (m_compiledShaderCode.empty()) {
        return false;
    }

    // Write the compiled GLSL shader to file
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    // Add header comment
    file << "// Generated GLSL Fragment Shader\n";
    file << "// Material Graph: " << m_graph->name << "\n";
    file << "// Generated by MaterialGraphEditor\n";
    file << "//\n\n";

    file << m_compiledShaderCode;
    file.close();

    return true;
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

void MaterialGraphEditor::AddNode(MaterialNodeType type, const glm::vec2& position) {
    if (!m_graph) return;

    auto command = std::make_unique<AddNodeCommand>(m_graph.get(), type, position);
    command->Execute();

    // Clear redo stack when a new action is performed
    m_redoStack.clear();

    // Push to undo stack
    m_undoStack.push_back(std::move(command));

    // Auto-compile if enabled
    if (settings.autoCompile) {
        CompileGraph();
    }
}

void MaterialGraphEditor::DeleteSelectedNodes() {
    if (!m_graph || m_selectedNodes.empty()) return;

    // Delete each selected node via commands
    for (int nodeId : m_selectedNodes) {
        auto command = std::make_unique<DeleteNodeCommand>(m_graph.get(), nodeId);
        command->Execute();
        m_undoStack.push_back(std::move(command));
    }

    // Clear redo stack when a new action is performed
    m_redoStack.clear();

    // Clear selection
    m_selectedNodes.clear();

    // Auto-compile if enabled
    if (settings.autoCompile) {
        CompileGraph();
    }
}

void MaterialGraphEditor::DuplicateSelectedNodes() {
    if (!m_graph || m_selectedNodes.empty()) return;

    std::set<int> newSelection;
    const glm::vec2 offset(50.0f, 50.0f);  // Offset for duplicated nodes

    for (int nodeId : m_selectedNodes) {
        MaterialNode* originalNode = m_graph->GetNode(nodeId);
        if (originalNode) {
            // Create a new node of the same type at an offset position
            auto command = std::make_unique<AddNodeCommand>(
                m_graph.get(),
                originalNode->type,
                originalNode->position + offset
            );
            command->Execute();

            int newNodeId = command->GetNodeId();
            if (newNodeId >= 0) {
                // Copy parameters from original node
                MaterialNode* newNode = m_graph->GetNode(newNodeId);
                if (newNode) {
                    newNode->floatParams = originalNode->floatParams;
                    newNode->vec2Params = originalNode->vec2Params;
                    newNode->vec3Params = originalNode->vec3Params;
                    newNode->vec4Params = originalNode->vec4Params;
                    newNode->stringParams = originalNode->stringParams;
                    newNode->boolParams = originalNode->boolParams;
                }
                newSelection.insert(newNodeId);
            }

            m_undoStack.push_back(std::move(command));
        }
    }

    // Clear redo stack
    m_redoStack.clear();

    // Select the new nodes
    m_selectedNodes = newSelection;

    // Auto-compile if enabled
    if (settings.autoCompile) {
        CompileGraph();
    }
}

void MaterialGraphEditor::SelectNode(int nodeId) {
    m_selectedNodes.insert(nodeId);
}

void MaterialGraphEditor::DeselectNode(int nodeId) {
    m_selectedNodes.erase(nodeId);
}

void MaterialGraphEditor::ClearSelection() {
    m_selectedNodes.clear();
}

bool MaterialGraphEditor::IsNodeSelected(int nodeId) const {
    return m_selectedNodes.find(nodeId) != m_selectedNodes.end();
}

void MaterialGraphEditor::CopySelectedNodes() {
    if (!m_graph || m_selectedNodes.empty()) return;

    // Create JSON object for clipboard data
    json clipboardData;
    clipboardData["type"] = "MaterialGraphNodes";
    clipboardData["nodes"] = json::array();
    clipboardData["connections"] = json::array();

    // Find center of selection for relative positioning
    glm::vec2 center(0.0f);
    for (int nodeId : m_selectedNodes) {
        MaterialNode* node = m_graph->GetNode(nodeId);
        if (node) {
            center += node->position;
        }
    }
    if (!m_selectedNodes.empty()) {
        center /= static_cast<float>(m_selectedNodes.size());
    }

    // Serialize selected nodes with relative positions
    for (int nodeId : m_selectedNodes) {
        MaterialNode* node = m_graph->GetNode(nodeId);
        if (node) {
            json nodeJson;
            node->Serialize(nodeJson);
            // Store relative position from center
            nodeJson["relativePos"] = {node->position.x - center.x, node->position.y - center.y};
            clipboardData["nodes"].push_back(nodeJson);
        }
    }

    // Serialize connections between selected nodes
    for (const auto& conn : m_graph->GetAllConnections()) {
        // Only include connections where both nodes are selected
        if (m_selectedNodes.count(conn.startNodeId) && m_selectedNodes.count(conn.endNodeId)) {
            clipboardData["connections"].push_back({
                {"startNodeId", conn.startNodeId},
                {"endNodeId", conn.endNodeId},
                {"startPinId", conn.startPinId},
                {"endPinId", conn.endPinId}
            });
        }
    }

    // Store to internal clipboard as JSON string
    m_clipboardData = clipboardData.dump();
}

void MaterialGraphEditor::CutSelectedNodes() {
    // Cut = Copy + Delete
    CopySelectedNodes();
    DeleteSelectedNodes();
}

void MaterialGraphEditor::PasteNodes() {
    if (!m_graph || m_clipboardData.empty()) return;

    try {
        json clipboardData = json::parse(m_clipboardData);

        // Verify clipboard type
        if (!clipboardData.contains("type") || clipboardData["type"] != "MaterialGraphNodes") {
            return;
        }

        // Calculate paste position (offset from current viewport center or mouse position)
        glm::vec2 pasteCenter = m_graph->viewportOffset + glm::vec2(100.0f, 100.0f);

        // Map old node IDs to new node IDs
        std::map<int, int> nodeIdMap;
        // Map old pin IDs to new pin IDs
        std::map<int, int> pinIdMap;

        // Clear selection before pasting
        ClearSelection();

        // Create new nodes from clipboard data
        for (const auto& nodeJson : clipboardData["nodes"]) {
            MaterialNodeType type = static_cast<MaterialNodeType>(nodeJson["type"]);
            auto node = MaterialNodeFactory::CreateNode(type);
            if (node) {
                // Deserialize node data
                node->Deserialize(nodeJson);

                // Store old ID for mapping
                int oldNodeId = nodeJson["id"];

                // Store old pin IDs before adding node (which may reassign IDs)
                std::map<std::string, int> oldInputPinIds;
                std::map<std::string, int> oldOutputPinIds;
                for (const auto& [pinName, pin] : node->inputs) {
                    oldInputPinIds[pinName] = pin.id;
                }
                for (const auto& [pinName, pin] : node->outputs) {
                    oldOutputPinIds[pinName] = pin.id;
                }

                // Calculate new position relative to paste center
                glm::vec2 relativePos(0.0f);
                if (nodeJson.contains("relativePos")) {
                    relativePos = glm::vec2(nodeJson["relativePos"][0], nodeJson["relativePos"][1]);
                }
                node->position = pasteCenter + relativePos;

                // Add node to graph (this assigns a new ID)
                int newNodeId = m_graph->AddNode(std::move(node));

                // Map old ID to new ID
                nodeIdMap[oldNodeId] = newNodeId;

                // Map old pin IDs to new pin IDs
                MaterialNode* addedNode = m_graph->GetNode(newNodeId);
                if (addedNode) {
                    for (const auto& [pinName, oldPinId] : oldInputPinIds) {
                        auto it = addedNode->inputs.find(pinName);
                        if (it != addedNode->inputs.end()) {
                            pinIdMap[oldPinId] = it->second.id;
                        }
                    }
                    for (const auto& [pinName, oldPinId] : oldOutputPinIds) {
                        auto it = addedNode->outputs.find(pinName);
                        if (it != addedNode->outputs.end()) {
                            pinIdMap[oldPinId] = it->second.id;
                        }
                    }
                }

                // Select the new node
                m_selectedNodes.insert(newNodeId);
            }
        }

        // Recreate connections between pasted nodes using mapped IDs
        for (const auto& connJson : clipboardData["connections"]) {
            int oldStartPinId = connJson["startPinId"];
            int oldEndPinId = connJson["endPinId"];

            // Only create connection if both pins were mapped
            auto startIt = pinIdMap.find(oldStartPinId);
            auto endIt = pinIdMap.find(oldEndPinId);
            if (startIt != pinIdMap.end() && endIt != pinIdMap.end()) {
                m_graph->AddConnection(startIt->second, endIt->second);
            }
        }

        // Auto-compile if enabled
        if (settings.autoCompile) {
            CompileGraph();
        }

    } catch (const std::exception& e) {
        // Invalid clipboard data, ignore
        (void)e;
    }
}

void MaterialGraphEditor::Undo() {
    if (!CanUndo()) return;

    // Get the last command from the undo stack
    auto command = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    // Execute the undo operation
    command->Undo();

    // Move to redo stack
    m_redoStack.push_back(std::move(command));

    // Auto-compile if enabled
    if (settings.autoCompile) {
        CompileGraph();
    }
}

void MaterialGraphEditor::Redo() {
    if (!CanRedo()) return;

    // Get the last command from the redo stack
    auto command = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    // Execute the command again
    command->Execute();

    // Move back to undo stack
    m_undoStack.push_back(std::move(command));

    // Auto-compile if enabled
    if (settings.autoCompile) {
        CompileGraph();
    }
}

bool MaterialGraphEditor::CanUndo() const {
    return !m_undoStack.empty();
}

bool MaterialGraphEditor::CanRedo() const {
    return !m_redoStack.empty();
}

void MaterialGraphEditor::UpdatePreview() {
    m_preview.needsUpdate = true;
}

void MaterialGraphEditor::SetPreviewMaterial(std::shared_ptr<AdvancedMaterial> material) {
    m_preview.material = material;
    m_preview.needsUpdate = true;
}

// Connection operations with undo support (internal helper methods)

bool MaterialGraphEditor::AddConnectionWithUndo(int startPinId, int endPinId) {
    if (!m_graph) return false;

    auto command = std::make_unique<AddConnectionCommand>(m_graph.get(), startPinId, endPinId);
    command->Execute();

    if (command->GetConnectionId() >= 0) {
        // Clear redo stack when a new action is performed
        m_redoStack.clear();

        // Push to undo stack
        m_undoStack.push_back(std::move(command));

        // Auto-compile if enabled
        if (settings.autoCompile) {
            CompileGraph();
        }
        return true;
    }
    return false;
}

void MaterialGraphEditor::DeleteConnectionWithUndo(int connectionId) {
    if (!m_graph) return;

    auto command = std::make_unique<DeleteConnectionCommand>(m_graph.get(), connectionId);
    command->Execute();

    // Clear redo stack when a new action is performed
    m_redoStack.clear();

    // Push to undo stack
    m_undoStack.push_back(std::move(command));

    // Auto-compile if enabled
    if (settings.autoCompile) {
        CompileGraph();
    }
}

void MaterialGraphEditor::HandleNodeInteraction() {
    // This would typically be called from Render() to handle user input
    // Implementation depends on the UI framework (Dear ImGui, etc.)
}

void MaterialGraphEditor::HandleConnectionDragging() {
    // This would typically be called from Render() to handle connection creation
    // When a connection drag completes successfully:
    // if (dragCompleted && m_dragStartPin >= 0 && targetPin >= 0) {
    //     AddConnectionWithUndo(m_dragStartPin, targetPin);
    //     m_isDraggingConnection = false;
    //     m_dragStartPin = -1;
    // }
}

void MaterialGraphEditor::ValidateGraph() {
    if (m_graph) {
        m_graph->Validate();
    }
}

void MaterialGraphEditor::Render() {
    RenderToolbar();
    RenderNodePalette();
    RenderNodeEditor();
    RenderProperties();
    RenderPreview();
}

void MaterialGraphEditor::RenderToolbar() {
    // Toolbar rendering - implementation depends on UI framework
}

void MaterialGraphEditor::RenderNodePalette() {
    // Node palette rendering - implementation depends on UI framework
}

void MaterialGraphEditor::RenderNodeEditor() {
    // Main node editor canvas rendering - implementation depends on UI framework
    HandleNodeInteraction();
    HandleConnectionDragging();
}

void MaterialGraphEditor::RenderProperties() {
    // Selected node properties panel - implementation depends on UI framework
}

void MaterialGraphEditor::RenderPreview() {
    // Material preview rendering - implementation depends on UI framework
}

void MaterialGraphEditor::RenderNode(MaterialNode* node) {
    // Individual node rendering - implementation depends on UI framework
}

void MaterialGraphEditor::RenderConnection(const MaterialConnection& conn) {
    // Connection line rendering - implementation depends on UI framework
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
