#include "EntityEventEditor.hpp"
#include "../Editor.hpp"
#include <imgui.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>
#include <filesystem>

namespace Vehement {

// =========================================================================
// Helper Functions for EventNode pin filtering
// =========================================================================

namespace {

// Filter pins by kind and direction
std::vector<Nova::EventPin> GetFlowInputs(Nova::EventNode* node) {
    std::vector<Nova::EventPin> result;
    for (const auto& pin : node->GetInputs()) {
        if (pin.kind == Nova::EventPinKind::Flow) {
            result.push_back(pin);
        }
    }
    return result;
}

std::vector<Nova::EventPin> GetFlowOutputs(Nova::EventNode* node) {
    std::vector<Nova::EventPin> result;
    for (const auto& pin : node->GetOutputs()) {
        if (pin.kind == Nova::EventPinKind::Flow) {
            result.push_back(pin);
        }
    }
    return result;
}

std::vector<Nova::EventPin> GetDataInputs(Nova::EventNode* node) {
    std::vector<Nova::EventPin> result;
    for (const auto& pin : node->GetInputs()) {
        if (pin.kind == Nova::EventPinKind::Data) {
            result.push_back(pin);
        }
    }
    return result;
}

std::vector<Nova::EventPin> GetDataOutputs(Nova::EventNode* node) {
    std::vector<Nova::EventPin> result;
    for (const auto& pin : node->GetOutputs()) {
        if (pin.kind == Nova::EventPinKind::Data) {
            result.push_back(pin);
        }
    }
    return result;
}

// Connection structure for visual graph
struct EventConnection {
    uint64_t fromNode;
    std::string fromPin;
    uint64_t toNode;
    std::string toPin;
};

// Build connections list from graph nodes
std::vector<EventConnection> GetConnections(Nova::EventGraph& graph) {
    std::vector<EventConnection> connections;
    for (const auto& node : graph.GetNodes()) {
        for (const auto& input : node->GetInputs()) {
            if (input.connectedNodeId != 0) {
                EventConnection conn;
                conn.fromNode = input.connectedNodeId;
                conn.fromPin = input.connectedPinName;
                conn.toNode = node->GetId();
                conn.toPin = input.name;
                connections.push_back(conn);
            }
        }
    }
    return connections;
}

// Validate graph and collect errors
bool ValidateEventGraph(Nova::EventGraph& graph, std::vector<std::string>& errors) {
    bool valid = true;

    // Check for empty graph
    if (graph.GetNodes().empty()) {
        errors.push_back("Graph has no nodes");
        return false;
    }

    // Check for entry points
    auto entries = graph.GetEntryPoints();
    if (entries.empty()) {
        errors.push_back("Graph has no entry points (event trigger nodes)");
        valid = false;
    }

    // Validate each node's connections
    for (const auto& node : graph.GetNodes()) {
        // Check for required input connections
        for (const auto& input : node->GetInputs()) {
            if (input.kind == Nova::EventPinKind::Flow && !input.hidden) {
                // Flow inputs should generally be connected (except for entry points)
                if (input.connectedNodeId == 0 &&
                    node->GetCategory() != Nova::EventNodeCategory::EventTrigger &&
                    node->GetCategory() != Nova::EventNodeCategory::EventCustom) {
                    // This is a warning, not an error - node may be unreachable
                }
            }
        }

        // Check for valid connection targets
        for (const auto& input : node->GetInputs()) {
            if (input.connectedNodeId != 0) {
                auto sourceNode = graph.GetNode(input.connectedNodeId);
                if (!sourceNode) {
                    errors.push_back("Node '" + node->GetDisplayName() + "' has connection to deleted node");
                    valid = false;
                } else {
                    auto* sourcePin = sourceNode->GetOutput(input.connectedPinName);
                    if (!sourcePin) {
                        errors.push_back("Node '" + node->GetDisplayName() + "' has connection to deleted pin");
                        valid = false;
                    }
                }
            }
        }
    }

    return valid;
}

// Get nodes by category from factory
std::vector<std::string> GetNodesInCategory(Nova::EventNodeCategory category) {
    return Nova::EventNodeFactory::Instance().GetNodeTypesInCategory(category);
}

// Template directory path
std::string GetTemplatesDirectory() {
    return "data/editor/event_templates/";
}

} // anonymous namespace

EntityEventEditor::EntityEventEditor() = default;
EntityEventEditor::~EntityEventEditor() = default;

void EntityEventEditor::Initialize(Editor* editor, const Config& config) {
    m_editor = editor;
    m_config = config;

    InitializeCategoryStyles();

    // Register builtin nodes
    Nova::EventNodeFactory::Instance().RegisterBuiltinNodes();

    m_initialized = true;
}

void EntityEventEditor::InitializeCategoryStyles() {
    m_categoryStyles = {
        {Nova::EventNodeCategory::EventTrigger, "Event Triggers", "E", {0.8f, 0.2f, 0.2f, 1.0f}},
        {Nova::EventNodeCategory::EventCustom, "Custom Events", "C", {0.8f, 0.4f, 0.2f, 1.0f}},
        {Nova::EventNodeCategory::FlowControl, "Flow Control", "F", {0.4f, 0.4f, 0.8f, 1.0f}},
        {Nova::EventNodeCategory::EntityState, "Entity State", "S", {0.2f, 0.6f, 0.8f, 1.0f}},
        {Nova::EventNodeCategory::EntityMesh, "Mesh", "M", {0.6f, 0.4f, 0.8f, 1.0f}},
        {Nova::EventNodeCategory::EntityAnimation, "Animation", "A", {0.8f, 0.6f, 0.2f, 1.0f}},
        {Nova::EventNodeCategory::EntityComponent, "Components", "K", {0.4f, 0.8f, 0.4f, 1.0f}},
        {Nova::EventNodeCategory::EntityMovement, "Movement", "V", {0.2f, 0.8f, 0.6f, 1.0f}},
        {Nova::EventNodeCategory::Combat, "Combat", "X", {0.9f, 0.3f, 0.3f, 1.0f}},
        {Nova::EventNodeCategory::World, "World", "W", {0.3f, 0.7f, 0.3f, 1.0f}},
        {Nova::EventNodeCategory::Terrain, "Terrain", "T", {0.5f, 0.4f, 0.3f, 1.0f}},
        {Nova::EventNodeCategory::Math, "Math", "+", {0.7f, 0.7f, 0.2f, 1.0f}},
        {Nova::EventNodeCategory::Logic, "Logic", "&", {0.5f, 0.5f, 0.7f, 1.0f}},
        {Nova::EventNodeCategory::Comparison, "Comparison", "=", {0.6f, 0.6f, 0.6f, 1.0f}},
        {Nova::EventNodeCategory::Variables, "Variables", "$", {0.3f, 0.6f, 0.9f, 1.0f}},
        {Nova::EventNodeCategory::Arrays, "Arrays", "[]", {0.4f, 0.5f, 0.7f, 1.0f}},
        {Nova::EventNodeCategory::Python, "Python", "Py", {0.3f, 0.5f, 0.8f, 1.0f}},
        {Nova::EventNodeCategory::Debug, "Debug", "D", {0.9f, 0.9f, 0.2f, 1.0f}},
        {Nova::EventNodeCategory::UI, "UI", "U", {0.6f, 0.3f, 0.6f, 1.0f}},
    };
}

// =========================================================================
// Graph Management
// =========================================================================

EntityEventGraph* EntityEventEditor::CreateGraph(const std::string& name, const std::string& entityType, const std::string& entityId) {
    auto graph = std::make_unique<EntityEventGraph>();
    graph->name = name;
    graph->entityType = entityType;
    graph->entityId = entityId;

    EntityEventGraph* ptr = graph.get();
    m_graphs.push_back(std::move(graph));
    m_currentGraph = ptr;

    return ptr;
}

bool EntityEventEditor::LoadGraph(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string jsonContent = buffer.str();

    // Create a new graph
    auto graph = std::make_unique<EntityEventGraph>();

    // Parse the JSON content
    // Expected format:
    // {
    //   "name": "graph name",
    //   "entityType": "unit",
    //   "entityId": "footman",
    //   "description": "...",
    //   "nodes": [...],
    //   "visuals": [{ "nodeId": 1, "position": [x, y], "size": [w, h] }, ...]
    // }

    // Simple JSON parsing - find key values
    auto findString = [&jsonContent](const std::string& key) -> std::string {
        std::string searchKey = "\"" + key + "\":\"";
        size_t pos = jsonContent.find(searchKey);
        if (pos == std::string::npos) return "";
        pos += searchKey.length();
        size_t end = jsonContent.find("\"", pos);
        if (end == std::string::npos) return "";
        return jsonContent.substr(pos, end - pos);
    };

    graph->name = findString("name");
    if (graph->name.empty()) {
        // Try to extract name from filename
        std::filesystem::path filePath(path);
        graph->name = filePath.stem().string();
    }
    graph->entityType = findString("entityType");
    graph->entityId = findString("entityId");
    graph->description = findString("description");

    // Load the underlying graph from JSON
    graph->graph.FromJson(jsonContent);

    // Reconstruct visuals from nodes if not present in JSON
    // (The graph's internal nodes have position data)
    for (const auto& node : graph->graph.GetNodes()) {
        EventNodeVisual visual;
        visual.nodeId = node->GetId();
        visual.position = node->GetPosition();
        visual.size = glm::vec2(m_config.nodeWidth, 100.0f);
        visual.selected = false;
        visual.collapsed = false;
        graph->nodeVisuals.push_back(visual);
    }

    // Parse visual positions from JSON if available
    // Look for "visuals" array in JSON
    size_t visualsPos = jsonContent.find("\"visuals\"");
    if (visualsPos != std::string::npos) {
        // Find array start
        size_t arrayStart = jsonContent.find("[", visualsPos);
        if (arrayStart != std::string::npos) {
            size_t arrayEnd = jsonContent.find("]", arrayStart);
            if (arrayEnd != std::string::npos) {
                std::string visualsArray = jsonContent.substr(arrayStart, arrayEnd - arrayStart + 1);

                // Parse each visual entry
                size_t objStart = 0;
                while ((objStart = visualsArray.find("{", objStart)) != std::string::npos) {
                    size_t objEnd = visualsArray.find("}", objStart);
                    if (objEnd == std::string::npos) break;

                    std::string obj = visualsArray.substr(objStart, objEnd - objStart + 1);

                    // Parse nodeId
                    size_t idPos = obj.find("\"nodeId\":");
                    if (idPos != std::string::npos) {
                        uint64_t nodeId = std::stoull(obj.substr(idPos + 9));

                        // Find the visual for this node
                        for (auto& v : graph->nodeVisuals) {
                            if (v.nodeId == nodeId) {
                                // Parse position
                                size_t posPos = obj.find("\"position\":[");
                                if (posPos != std::string::npos) {
                                    posPos += 12;
                                    size_t comma = obj.find(",", posPos);
                                    size_t bracket = obj.find("]", posPos);
                                    if (comma != std::string::npos && bracket != std::string::npos) {
                                        v.position.x = std::stof(obj.substr(posPos, comma - posPos));
                                        v.position.y = std::stof(obj.substr(comma + 1, bracket - comma - 1));
                                    }
                                }

                                // Parse size
                                size_t sizePos = obj.find("\"size\":[");
                                if (sizePos != std::string::npos) {
                                    sizePos += 8;
                                    size_t comma = obj.find(",", sizePos);
                                    size_t bracket = obj.find("]", sizePos);
                                    if (comma != std::string::npos && bracket != std::string::npos) {
                                        v.size.x = std::stof(obj.substr(sizePos, comma - sizePos));
                                        v.size.y = std::stof(obj.substr(comma + 1, bracket - comma - 1));
                                    }
                                }
                                break;
                            }
                        }
                    }
                    objStart = objEnd + 1;
                }
            }
        }
    }

    EntityEventGraph* ptr = graph.get();
    m_graphs.push_back(std::move(graph));
    m_currentGraph = ptr;
    m_currentGraph->modified = false;

    ClearSelection();
    FrameAll();

    return true;
}

bool EntityEventEditor::SaveGraph(const std::string& path) {
    if (!m_currentGraph) return false;

    std::string savePath = path;
    if (savePath.empty()) {
        // Use default path based on entity type and id
        savePath = "data/events/" + m_currentGraph->entityType + "/" +
                   m_currentGraph->entityId + "/" + m_currentGraph->name + ".json";
    }

    // Ensure directory exists
    std::filesystem::path filePath(savePath);
    std::filesystem::create_directories(filePath.parent_path());

    std::ofstream file(savePath);
    if (!file.is_open()) {
        return false;
    }

    // Build JSON manually for full control over format
    std::stringstream json;
    json << "{\n";
    json << "  \"name\": \"" << m_currentGraph->name << "\",\n";
    json << "  \"entityType\": \"" << m_currentGraph->entityType << "\",\n";
    json << "  \"entityId\": \"" << m_currentGraph->entityId << "\",\n";
    json << "  \"description\": \"" << m_currentGraph->description << "\",\n";

    // Save the graph's internal JSON representation
    std::string graphJson = m_currentGraph->graph.ToJson();
    // Extract nodes array from graph JSON if present
    size_t nodesPos = graphJson.find("\"nodes\":");
    if (nodesPos != std::string::npos) {
        size_t start = graphJson.find("[", nodesPos);
        size_t end = graphJson.rfind("]");
        if (start != std::string::npos && end != std::string::npos && end > start) {
            json << "  \"nodes\": " << graphJson.substr(start, end - start + 1) << ",\n";
        }
    } else {
        json << "  \"nodes\": [],\n";
    }

    // Save visual information
    json << "  \"visuals\": [\n";
    for (size_t i = 0; i < m_currentGraph->nodeVisuals.size(); ++i) {
        const auto& visual = m_currentGraph->nodeVisuals[i];
        json << "    {\n";
        json << "      \"nodeId\": " << visual.nodeId << ",\n";
        json << "      \"position\": [" << visual.position.x << ", " << visual.position.y << "],\n";
        json << "      \"size\": [" << visual.size.x << ", " << visual.size.y << "],\n";
        json << "      \"collapsed\": " << (visual.collapsed ? "true" : "false") << "\n";
        json << "    }";
        if (i < m_currentGraph->nodeVisuals.size() - 1) {
            json << ",";
        }
        json << "\n";
    }
    json << "  ]\n";
    json << "}\n";

    file << json.str();
    file.close();

    m_currentGraph->modified = false;
    return true;
}

void EntityEventEditor::SetCurrentGraph(EntityEventGraph* graph) {
    m_currentGraph = graph;
    ClearSelection();
    ResetView();
}

void EntityEventEditor::CloseGraph(EntityEventGraph* graph) {
    auto it = std::find_if(m_graphs.begin(), m_graphs.end(),
        [graph](const auto& g) { return g.get() == graph; });

    if (it != m_graphs.end()) {
        if (m_currentGraph == graph) {
            m_currentGraph = nullptr;
        }
        m_graphs.erase(it);
    }
}

// =========================================================================
// Node Operations
// =========================================================================

Nova::EventNode::Ptr EntityEventEditor::AddNode(const std::string& typeName, const glm::vec2& position) {
    if (!m_currentGraph) return nullptr;

    auto node = Nova::EventNodeFactory::Instance().Create(typeName);
    if (!node) return nullptr;

    m_currentGraph->graph.AddNode(node);

    glm::vec2 snappedPos = m_config.snapToGrid ? SnapToGrid(position) : position;
    CreateNodeVisual(node->GetId(), snappedPos);

    m_currentGraph->modified = true;

    if (OnNodeAdded) OnNodeAdded(node);
    if (OnGraphModified) OnGraphModified();

    return node;
}

void EntityEventEditor::RemoveSelectedNodes() {
    if (!m_currentGraph) return;

    for (uint64_t nodeId : m_selectedNodes) {
        m_currentGraph->graph.RemoveNode(nodeId);

        // Remove visual
        auto& visuals = m_currentGraph->nodeVisuals;
        visuals.erase(std::remove_if(visuals.begin(), visuals.end(),
            [nodeId](const EventNodeVisual& v) { return v.nodeId == nodeId; }),
            visuals.end());

        if (OnNodeRemoved) OnNodeRemoved(nodeId);
    }

    m_selectedNodes.clear();
    m_currentGraph->modified = true;

    if (OnSelectionChanged) OnSelectionChanged();
    if (OnGraphModified) OnGraphModified();
}

void EntityEventEditor::DuplicateSelectedNodes() {
    if (!m_currentGraph || m_selectedNodes.empty()) return;

    std::vector<Nova::EventNode::Ptr> newNodes;
    glm::vec2 offset{50.0f, 50.0f};

    for (uint64_t nodeId : m_selectedNodes) {
        auto node = m_currentGraph->graph.GetNode(nodeId);
        auto* visual = GetNodeVisual(nodeId);
        if (node && visual) {
            auto newNode = Nova::EventNodeFactory::Instance().Create(node->GetTypeName());
            if (newNode) {
                m_currentGraph->graph.AddNode(newNode);
                CreateNodeVisual(newNode->GetId(), visual->position + offset);
                newNodes.push_back(newNode);
            }
        }
    }

    // Select new nodes
    ClearSelection();
    for (auto& node : newNodes) {
        SelectNode(node->GetId(), true);
    }

    m_currentGraph->modified = true;
    if (OnGraphModified) OnGraphModified();
}

void EntityEventEditor::CopySelectedNodes() {
    m_clipboard.clear();

    if (!m_currentGraph) return;

    for (uint64_t nodeId : m_selectedNodes) {
        auto node = m_currentGraph->graph.GetNode(nodeId);
        auto* visual = GetNodeVisual(nodeId);
        if (node && visual) {
            auto copy = Nova::EventNodeFactory::Instance().Create(node->GetTypeName());
            if (copy) {
                m_clipboard.emplace_back(copy, visual->position);
            }
        }
    }
}

void EntityEventEditor::PasteNodes(const glm::vec2& position) {
    if (!m_currentGraph || m_clipboard.empty()) return;

    // Calculate center of clipboard nodes
    glm::vec2 center{0.0f};
    for (const auto& [node, pos] : m_clipboard) {
        center += pos;
    }
    center /= static_cast<float>(m_clipboard.size());

    // Paste with offset from center to target position
    ClearSelection();
    for (const auto& [node, pos] : m_clipboard) {
        auto newNode = Nova::EventNodeFactory::Instance().Create(node->GetTypeName());
        if (newNode) {
            m_currentGraph->graph.AddNode(newNode);
            glm::vec2 newPos = position + (pos - center);
            CreateNodeVisual(newNode->GetId(), newPos);
            SelectNode(newNode->GetId(), true);
        }
    }

    m_currentGraph->modified = true;
    if (OnGraphModified) OnGraphModified();
}

void EntityEventEditor::CutSelectedNodes() {
    CopySelectedNodes();
    RemoveSelectedNodes();
}

// =========================================================================
// Selection
// =========================================================================

void EntityEventEditor::SelectNode(uint64_t nodeId, bool addToSelection) {
    if (!addToSelection) {
        for (uint64_t id : m_selectedNodes) {
            if (auto* visual = GetNodeVisual(id)) {
                visual->selected = false;
            }
        }
        m_selectedNodes.clear();
    }

    m_selectedNodes.insert(nodeId);
    if (auto* visual = GetNodeVisual(nodeId)) {
        visual->selected = true;
    }

    if (OnSelectionChanged) OnSelectionChanged();
}

void EntityEventEditor::DeselectNode(uint64_t nodeId) {
    m_selectedNodes.erase(nodeId);
    if (auto* visual = GetNodeVisual(nodeId)) {
        visual->selected = false;
    }

    if (OnSelectionChanged) OnSelectionChanged();
}

void EntityEventEditor::ClearSelection() {
    for (uint64_t id : m_selectedNodes) {
        if (auto* visual = GetNodeVisual(id)) {
            visual->selected = false;
        }
    }
    m_selectedNodes.clear();

    if (OnSelectionChanged) OnSelectionChanged();
}

void EntityEventEditor::SelectAllNodes() {
    if (!m_currentGraph) return;

    for (auto& visual : m_currentGraph->nodeVisuals) {
        visual.selected = true;
        m_selectedNodes.insert(visual.nodeId);
    }

    if (OnSelectionChanged) OnSelectionChanged();
}

void EntityEventEditor::BoxSelectNodes(const glm::vec2& start, const glm::vec2& end) {
    if (!m_currentGraph) return;

    glm::vec2 minPos = glm::min(start, end);
    glm::vec2 maxPos = glm::max(start, end);

    for (auto& visual : m_currentGraph->nodeVisuals) {
        bool inBox = visual.position.x + visual.size.x >= minPos.x &&
                     visual.position.x <= maxPos.x &&
                     visual.position.y + visual.size.y >= minPos.y &&
                     visual.position.y <= maxPos.y;

        if (inBox) {
            visual.selected = true;
            m_selectedNodes.insert(visual.nodeId);
        }
    }

    if (OnSelectionChanged) OnSelectionChanged();
}

// =========================================================================
// Connections
// =========================================================================

void EntityEventEditor::StartConnection(uint64_t nodeId, const std::string& pinName, bool isOutput) {
    m_isConnecting = true;
    m_connectionStartNode = nodeId;
    m_connectionStartPin = pinName;
    m_connectionStartIsOutput = isOutput;
}

bool EntityEventEditor::CompleteConnection(uint64_t nodeId, const std::string& pinName) {
    if (!m_isConnecting || !m_currentGraph) {
        CancelConnection();
        return false;
    }

    // Validate connection direction
    if (m_connectionStartIsOutput) {
        // Output to input
        if (m_currentGraph->graph.Connect(m_connectionStartNode, m_connectionStartPin, nodeId, pinName)) {
            m_currentGraph->modified = true;
            if (OnGraphModified) OnGraphModified();
            CancelConnection();
            return true;
        }
    } else {
        // Input to output (reversed)
        if (m_currentGraph->graph.Connect(nodeId, pinName, m_connectionStartNode, m_connectionStartPin)) {
            m_currentGraph->modified = true;
            if (OnGraphModified) OnGraphModified();
            CancelConnection();
            return true;
        }
    }

    CancelConnection();
    return false;
}

void EntityEventEditor::CancelConnection() {
    m_isConnecting = false;
    m_connectionStartNode = 0;
    m_connectionStartPin.clear();
}

void EntityEventEditor::RemoveConnection(uint64_t fromNode, const std::string& fromPin, uint64_t toNode, const std::string& toPin) {
    if (!m_currentGraph) return;

    // EventGraph::Disconnect takes the target node and pin name
    // (The connection info is stored on the input pin)
    m_currentGraph->graph.Disconnect(toNode, toPin);
    m_currentGraph->modified = true;

    if (OnGraphModified) OnGraphModified();
}

// =========================================================================
// View Control
// =========================================================================

void EntityEventEditor::Pan(const glm::vec2& delta) {
    m_viewOffset += delta;
}

void EntityEventEditor::Zoom(float delta, const glm::vec2& center) {
    float oldScale = m_viewScale;
    m_viewScale = std::clamp(m_viewScale + delta * 0.1f, 0.1f, 3.0f);

    // Zoom toward center
    float scaleFactor = m_viewScale / oldScale;
    m_viewOffset = center - (center - m_viewOffset) * scaleFactor;
}

void EntityEventEditor::ResetView() {
    m_viewOffset = glm::vec2{0.0f};
    m_viewScale = 1.0f;
}

void EntityEventEditor::FrameAll() {
    if (!m_currentGraph || m_currentGraph->nodeVisuals.empty()) return;

    glm::vec2 minPos{std::numeric_limits<float>::max()};
    glm::vec2 maxPos{std::numeric_limits<float>::lowest()};

    for (const auto& visual : m_currentGraph->nodeVisuals) {
        minPos = glm::min(minPos, visual.position);
        maxPos = glm::max(maxPos, visual.position + visual.size);
    }

    glm::vec2 center = (minPos + maxPos) * 0.5f;
    glm::vec2 size = maxPos - minPos;

    m_viewScale = std::min(m_canvasSize.x / (size.x + 100.0f), m_canvasSize.y / (size.y + 100.0f));
    m_viewScale = std::clamp(m_viewScale, 0.1f, 1.0f);
    m_viewOffset = m_canvasSize * 0.5f - center * m_viewScale;
}

void EntityEventEditor::FrameSelected() {
    if (!m_currentGraph || m_selectedNodes.empty()) return;

    glm::vec2 minPos{std::numeric_limits<float>::max()};
    glm::vec2 maxPos{std::numeric_limits<float>::lowest()};

    for (uint64_t nodeId : m_selectedNodes) {
        if (auto* visual = GetNodeVisual(nodeId)) {
            minPos = glm::min(minPos, visual->position);
            maxPos = glm::max(maxPos, visual->position + visual->size);
        }
    }

    glm::vec2 center = (minPos + maxPos) * 0.5f;
    m_viewOffset = m_canvasSize * 0.5f - center * m_viewScale;
}

// =========================================================================
// Compilation
// =========================================================================

std::string EntityEventEditor::CompileToPython() {
    if (!m_currentGraph) return "";

    std::string code = m_currentGraph->graph.CompileToPython();

    if (OnCompiled) OnCompiled(code);

    return code;
}

bool EntityEventEditor::ValidateGraph(std::vector<std::string>& errors) {
    if (!m_currentGraph) {
        errors.push_back("No graph loaded");
        return false;
    }

    // Use the local helper function for validation
    return ValidateEventGraph(m_currentGraph->graph, errors);
}

bool EntityEventEditor::ExportToPython(const std::string& path) {
    std::string code = CompileToPython();
    if (code.empty()) return false;

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "# Auto-generated from visual event graph\n";
    file << "# Entity: " << m_currentGraph->entityId << "\n";
    file << "# Graph: " << m_currentGraph->name << "\n\n";
    file << code;

    return true;
}

// =========================================================================
// Templates
// =========================================================================

void EntityEventEditor::LoadTemplate(const std::string& templateName) {
    // Check if it's the Empty template
    if (templateName == "Empty") {
        CreateGraph("New Graph", "unit", "");
        return;
    }

    // Try to load from file
    std::string templatePath = GetTemplatesDirectory() + templateName + ".json";
    if (std::filesystem::exists(templatePath)) {
        LoadGraph(templatePath);
        if (m_currentGraph) {
            m_currentGraph->name = "New " + templateName;
            m_currentGraph->entityId = "";
            m_currentGraph->modified = true;
        }
        return;
    }

    // Create predefined templates programmatically
    auto graph = CreateGraph("New " + templateName, "unit", "");
    if (!graph) return;

    if (templateName == "Basic Unit Events") {
        // OnSpawn node
        auto spawnNode = AddNode("OnSpawn", glm::vec2(100, 100));
        // Print node connected to spawn
        auto printNode = AddNode("Print", glm::vec2(400, 100));
        if (spawnNode && printNode) {
            m_currentGraph->graph.Connect(spawnNode->GetId(), "Exec", printNode->GetId(), "Exec");
        }

        // OnDeath node
        AddNode("OnDeath", glm::vec2(100, 300));

        // OnDamage node
        AddNode("OnDamage", glm::vec2(100, 500));

    } else if (templateName == "Combat Unit") {
        auto spawnNode = AddNode("OnSpawn", glm::vec2(100, 100));
        auto damageNode = AddNode("OnDamage", glm::vec2(100, 300));
        auto deathNode = AddNode("OnDeath", glm::vec2(100, 500));

        // Add attack response
        auto branchNode = AddNode("Branch", glm::vec2(400, 300));
        auto dealDamageNode = AddNode("DealDamage", glm::vec2(700, 200));

        if (damageNode && branchNode) {
            m_currentGraph->graph.Connect(damageNode->GetId(), "Exec", branchNode->GetId(), "Exec");
        }
        if (branchNode && dealDamageNode) {
            m_currentGraph->graph.Connect(branchNode->GetId(), "True", dealDamageNode->GetId(), "Exec");
        }

    } else if (templateName == "Resource Gatherer") {
        auto spawnNode = AddNode("OnSpawn", glm::vec2(100, 100));
        auto timerNode = AddNode("OnTimer", glm::vec2(100, 300));
        auto commandNode = AddNode("OnCommand", glm::vec2(100, 500));

        // Movement to resource
        auto moveNode = AddNode("MoveTo", glm::vec2(400, 500));
        if (commandNode && moveNode) {
            m_currentGraph->graph.Connect(commandNode->GetId(), "Exec", moveNode->GetId(), "Exec");
        }

    } else if (templateName == "Building Construction") {
        auto spawnNode = AddNode("OnSpawn", glm::vec2(100, 100));
        auto timerNode = AddNode("OnTimer", glm::vec2(100, 300));

        // Progress animation/visual
        auto setScaleNode = AddNode("SetScale", glm::vec2(400, 300));
        if (timerNode && setScaleNode) {
            m_currentGraph->graph.Connect(timerNode->GetId(), "Exec", setScaleNode->GetId(), "Exec");
        }

    } else if (templateName == "Hero Abilities") {
        auto spawnNode = AddNode("OnSpawn", glm::vec2(100, 100));
        auto commandNode = AddNode("OnCommand", glm::vec2(100, 300));
        auto damageNode = AddNode("OnDamage", glm::vec2(100, 500));

        // Ability usage
        auto useAbilityNode = AddNode("UseAbility", glm::vec2(400, 300));
        auto branchNode = AddNode("Branch", glm::vec2(700, 300));

        if (commandNode && useAbilityNode) {
            m_currentGraph->graph.Connect(commandNode->GetId(), "Exec", useAbilityNode->GetId(), "Exec");
        }
        if (useAbilityNode && branchNode) {
            m_currentGraph->graph.Connect(useAbilityNode->GetId(), "Exec", branchNode->GetId(), "Exec");
        }

    } else if (templateName == "Spawner") {
        auto spawnNode = AddNode("OnSpawn", glm::vec2(100, 100));
        auto timerNode = AddNode("OnTimer", glm::vec2(100, 300));

        // Spawn entities periodically
        auto spawnEntityNode = AddNode("SpawnEntity", glm::vec2(400, 300));
        if (timerNode && spawnEntityNode) {
            m_currentGraph->graph.Connect(timerNode->GetId(), "Exec", spawnEntityNode->GetId(), "Exec");
        }

    } else if (templateName == "Patrol Unit") {
        auto spawnNode = AddNode("OnSpawn", glm::vec2(100, 100));
        auto timerNode = AddNode("OnTimer", glm::vec2(100, 300));
        auto collisionNode = AddNode("OnCollision", glm::vec2(100, 500));

        // Patrol movement
        auto moveNode = AddNode("MoveTo", glm::vec2(400, 300));
        auto branchNode = AddNode("Branch", glm::vec2(400, 500));
        auto dealDamageNode = AddNode("DealDamage", glm::vec2(700, 500));

        if (timerNode && moveNode) {
            m_currentGraph->graph.Connect(timerNode->GetId(), "Exec", moveNode->GetId(), "Exec");
        }
        if (collisionNode && branchNode) {
            m_currentGraph->graph.Connect(collisionNode->GetId(), "Exec", branchNode->GetId(), "Exec");
        }
        if (branchNode && dealDamageNode) {
            m_currentGraph->graph.Connect(branchNode->GetId(), "True", dealDamageNode->GetId(), "Exec");
        }
    }

    m_currentGraph->modified = true;
    FrameAll();
}

bool EntityEventEditor::SaveAsTemplate(const std::string& templateName) {
    if (!m_currentGraph) return false;

    // Ensure templates directory exists
    std::string templatesDir = GetTemplatesDirectory();
    std::filesystem::create_directories(templatesDir);

    // Save to template path
    std::string templatePath = templatesDir + templateName + ".json";

    // Store original values
    std::string originalName = m_currentGraph->name;
    std::string originalEntityId = m_currentGraph->entityId;

    // Set template metadata
    m_currentGraph->name = templateName;
    m_currentGraph->entityId = "";  // Templates don't have specific entity IDs

    // Save the graph
    bool success = SaveGraph(templatePath);

    // Restore original values
    m_currentGraph->name = originalName;
    m_currentGraph->entityId = originalEntityId;

    return success;
}

std::vector<std::string> EntityEventEditor::GetAvailableTemplates() const {
    return {
        "Empty",
        "Basic Unit Events",
        "Combat Unit",
        "Resource Gatherer",
        "Building Construction",
        "Hero Abilities",
        "Spawner",
        "Patrol Unit"
    };
}

// =========================================================================
// UI Rendering
// =========================================================================

void EntityEventEditor::Render() {
    if (!m_initialized) return;

    ImGui::Begin("Entity Event Editor", nullptr, ImGuiWindowFlags_MenuBar);

    RenderToolbar();

    // Split into panels
    float panelWidth = 250.0f;

    // Left panel - Node palette
    if (m_showNodePalette) {
        ImGui::BeginChild("NodePalette", ImVec2(panelWidth, 0), true);
        RenderNodePalette();
        ImGui::EndChild();
        ImGui::SameLine();
    }

    // Center - Canvas
    ImGui::BeginChild("Canvas", ImVec2(-(m_showPropertyPanel ? panelWidth : 0) - (m_showCodePreview ? 300 : 0), 0), true, ImGuiWindowFlags_NoScrollbar);
    m_canvasPos = glm::vec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    m_canvasSize = glm::vec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);

    RenderCanvas();
    ImGui::EndChild();

    // Right panel - Property panel
    if (m_showPropertyPanel) {
        ImGui::SameLine();
        ImGui::BeginChild("Properties", ImVec2(panelWidth, 0), true);
        RenderPropertyPanel();
        ImGui::EndChild();
    }

    // Code preview panel
    if (m_showCodePreview) {
        ImGui::SameLine();
        ImGui::BeginChild("CodePreview", ImVec2(300, 0), true);
        RenderCodePreview();
        ImGui::EndChild();
    }

    // Context menu
    if (m_showContextMenu) {
        RenderContextMenu();
    }

    ImGui::End();
}

void EntityEventEditor::RenderToolbar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Graph")) {
                CreateGraph("New Graph", "unit", "");
            }
            if (ImGui::MenuItem("Open...")) {}
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                SaveGraph();
            }
            if (ImGui::MenuItem("Export Python...")) {
                // Generate default export path based on graph name
                if (m_currentGraph) {
                    std::string defaultPath = "scripts/generated/" +
                        m_currentGraph->entityType + "/" +
                        (m_currentGraph->entityId.empty() ? m_currentGraph->name : m_currentGraph->entityId) +
                        "_events.py";

                    // Ensure directory exists
                    std::filesystem::path filePath(defaultPath);
                    std::filesystem::create_directories(filePath.parent_path());

                    if (ExportToPython(defaultPath)) {
                        // Success - could show notification
                    }
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) { CutSelectedNodes(); }
            if (ImGui::MenuItem("Copy", "Ctrl+C")) { CopySelectedNodes(); }
            if (ImGui::MenuItem("Paste", "Ctrl+V")) { PasteNodes(ScreenToCanvas(m_canvasSize * 0.5f)); }
            if (ImGui::MenuItem("Delete", "Del")) { RemoveSelectedNodes(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) { SelectAllNodes(); }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Node Palette", nullptr, &m_showNodePalette);
            ImGui::MenuItem("Properties", nullptr, &m_showPropertyPanel);
            ImGui::MenuItem("Code Preview", nullptr, &m_showCodePreview);
            ImGui::Separator();
            if (ImGui::MenuItem("Frame All", "F")) { FrameAll(); }
            if (ImGui::MenuItem("Frame Selected")) { FrameSelected(); }
            if (ImGui::MenuItem("Reset View")) { ResetView(); }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Templates")) {
            for (const auto& tmpl : GetAvailableTemplates()) {
                if (ImGui::MenuItem(tmpl.c_str())) {
                    LoadTemplate(tmpl);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // Toolbar buttons
    if (ImGui::Button("Compile")) {
        CompileToPython();
    }
    ImGui::SameLine();

    std::vector<std::string> errors;
    if (ImGui::Button("Validate")) {
        if (ValidateGraph(errors)) {
            // Show success
        }
    }

    ImGui::SameLine();
    ImGui::Text("Scale: %.1f%%", m_viewScale * 100.0f);

    if (m_currentGraph) {
        ImGui::SameLine();
        ImGui::Text("| %s%s", m_currentGraph->name.c_str(), m_currentGraph->modified ? "*" : "");
    }
}

void EntityEventEditor::RenderCanvas() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Background
    drawList->AddRectFilled(
        ImVec2(m_canvasPos.x, m_canvasPos.y),
        ImVec2(m_canvasPos.x + m_canvasSize.x, m_canvasPos.y + m_canvasSize.y),
        ImColor(m_config.backgroundColor.r, m_config.backgroundColor.g, m_config.backgroundColor.b, m_config.backgroundColor.a)
    );

    // Grid
    if (m_config.showGrid) {
        RenderGrid();
    }

    // Connections
    RenderConnections();

    // Pending connection
    if (m_isConnecting) {
        RenderPendingConnection();
    }

    // Nodes
    RenderNodes();

    // Selection box
    if (m_isBoxSelecting) {
        RenderSelectionBox();
    }

    // Minimap
    if (m_config.showMinimap) {
        RenderMinimap();
    }

    // Handle input
    ProcessInput();
}

void EntityEventEditor::RenderGrid() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float gridSize = m_config.gridSize * m_viewScale;
    ImU32 gridColor = ImColor(m_config.gridColor.r, m_config.gridColor.g, m_config.gridColor.b, m_config.gridColor.a);

    glm::vec2 offset = glm::mod(m_viewOffset, glm::vec2(gridSize));

    for (float x = offset.x; x < m_canvasSize.x; x += gridSize) {
        drawList->AddLine(
            ImVec2(m_canvasPos.x + x, m_canvasPos.y),
            ImVec2(m_canvasPos.x + x, m_canvasPos.y + m_canvasSize.y),
            gridColor
        );
    }

    for (float y = offset.y; y < m_canvasSize.y; y += gridSize) {
        drawList->AddLine(
            ImVec2(m_canvasPos.x, m_canvasPos.y + y),
            ImVec2(m_canvasPos.x + m_canvasSize.x, m_canvasPos.y + y),
            gridColor
        );
    }
}

void EntityEventEditor::RenderNodes() {
    if (!m_currentGraph) return;

    for (auto& visual : m_currentGraph->nodeVisuals) {
        auto node = m_currentGraph->graph.GetNode(visual.nodeId);
        if (node) {
            RenderNode(visual, node.get());
        }
    }
}

void EntityEventEditor::RenderNode(EventNodeVisual& visual, Nova::EventNode* node) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    glm::vec2 screenPos = CanvasToScreen(visual.position);
    glm::vec2 screenSize = visual.size * m_viewScale;

    // Skip if not visible
    if (screenPos.x + screenSize.x < m_canvasPos.x || screenPos.x > m_canvasPos.x + m_canvasSize.x ||
        screenPos.y + screenSize.y < m_canvasPos.y || screenPos.y > m_canvasPos.y + m_canvasSize.y) {
        return;
    }

    // Node body
    ImU32 bodyColor = visual.selected ?
        ImColor(0.25f, 0.25f, 0.35f, 1.0f) :
        ImColor(0.18f, 0.18f, 0.22f, 1.0f);

    float rounding = 4.0f * m_viewScale;

    drawList->AddRectFilled(
        ImVec2(screenPos.x, screenPos.y),
        ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
        bodyColor, rounding
    );

    // Header
    RenderNodeHeader(visual, node);

    // Selection outline
    if (visual.selected) {
        drawList->AddRect(
            ImVec2(screenPos.x, screenPos.y),
            ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
            ImColor(0.4f, 0.6f, 1.0f, 1.0f), rounding, 0, 2.0f * m_viewScale
        );
    }

    // Hover outline
    if (visual.nodeId == m_hoveredNode) {
        drawList->AddRect(
            ImVec2(screenPos.x, screenPos.y),
            ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
            ImColor(0.6f, 0.6f, 0.6f, 0.5f), rounding
        );
    }

    // Pins
    RenderNodePins(visual, node);
}

void EntityEventEditor::RenderNodeHeader(EventNodeVisual& visual, Nova::EventNode* node) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    glm::vec2 screenPos = CanvasToScreen(visual.position);
    glm::vec2 screenSize = visual.size * m_viewScale;
    float headerHeight = 24.0f * m_viewScale;
    float rounding = 4.0f * m_viewScale;

    glm::vec4 headerColor = GetCategoryColor(node->GetCategory());

    drawList->AddRectFilled(
        ImVec2(screenPos.x, screenPos.y),
        ImVec2(screenPos.x + screenSize.x, screenPos.y + headerHeight),
        ImColor(headerColor.r, headerColor.g, headerColor.b, headerColor.a),
        rounding, ImDrawFlags_RoundCornersTop
    );

    // Title
    float fontSize = 14.0f * m_viewScale;
    drawList->AddText(
        ImVec2(screenPos.x + 8.0f * m_viewScale, screenPos.y + 4.0f * m_viewScale),
        ImColor(1.0f, 1.0f, 1.0f, 1.0f),
        node->GetDisplayName().c_str()
    );
}

void EntityEventEditor::RenderNodePins(EventNodeVisual& visual, Nova::EventNode* node) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    glm::vec2 screenPos = CanvasToScreen(visual.position);
    float pinRadius = m_config.pinRadius * m_viewScale;
    float yOffset = 32.0f * m_viewScale;
    float pinSpacing = 20.0f * m_viewScale;

    // Flow inputs
    for (const auto& pin : GetFlowInputs(node)) {
        glm::vec2 pinPos = screenPos + glm::vec2(0, yOffset);
        bool isHovered = (m_hoveredNode == visual.nodeId && m_hoveredPin == pin.name && !m_hoveredPinIsOutput);
        RenderPin(pinPos, pin, false, isHovered);

        // Pin name
        drawList->AddText(
            ImVec2(pinPos.x + pinRadius + 4.0f * m_viewScale, pinPos.y - pinRadius),
            ImColor(0.8f, 0.8f, 0.8f, 1.0f),
            pin.name.c_str()
        );

        yOffset += pinSpacing;
    }

    // Data inputs
    for (const auto& pin : GetDataInputs(node)) {
        glm::vec2 pinPos = screenPos + glm::vec2(0, yOffset);
        bool isHovered = (m_hoveredNode == visual.nodeId && m_hoveredPin == pin.name && !m_hoveredPinIsOutput);
        RenderPin(pinPos, pin, false, isHovered);

        drawList->AddText(
            ImVec2(pinPos.x + pinRadius + 4.0f * m_viewScale, pinPos.y - pinRadius),
            ImColor(0.8f, 0.8f, 0.8f, 1.0f),
            pin.name.c_str()
        );

        yOffset += pinSpacing;
    }

    // Flow outputs
    yOffset = 32.0f * m_viewScale;
    float rightX = screenPos.x + visual.size.x * m_viewScale;

    for (const auto& pin : GetFlowOutputs(node)) {
        glm::vec2 pinPos = glm::vec2(rightX, screenPos.y + yOffset);
        bool isHovered = (m_hoveredNode == visual.nodeId && m_hoveredPin == pin.name && m_hoveredPinIsOutput);
        RenderPin(pinPos, pin, true, isHovered);

        yOffset += pinSpacing;
    }

    // Data outputs
    for (const auto& pin : GetDataOutputs(node)) {
        glm::vec2 pinPos = glm::vec2(rightX, screenPos.y + yOffset);
        bool isHovered = (m_hoveredNode == visual.nodeId && m_hoveredPin == pin.name && m_hoveredPinIsOutput);
        RenderPin(pinPos, pin, true, isHovered);

        yOffset += pinSpacing;
    }

    // Update node size based on pins
    float totalPins = static_cast<float>(std::max(
        GetFlowInputs(node).size() + GetDataInputs(node).size(),
        GetFlowOutputs(node).size() + GetDataOutputs(node).size()
    ));
    visual.size.y = std::max(60.0f, 32.0f + totalPins * 20.0f + 10.0f);
}

void EntityEventEditor::RenderPin(const glm::vec2& pos, const Nova::EventPin& pin, bool isOutput, bool isHovered) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float radius = m_config.pinRadius * m_viewScale;

    // Color based on pin type
    glm::vec4 color;
    if (pin.kind == Nova::EventPinKind::Flow) {
        color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    } else {
        // Color based on data type
        switch (pin.dataType) {
            case Nova::EventDataType::Bool: color = glm::vec4(0.8f, 0.2f, 0.2f, 1.0f); break;
            case Nova::EventDataType::Int: color = glm::vec4(0.2f, 0.8f, 0.8f, 1.0f); break;
            case Nova::EventDataType::Float: color = glm::vec4(0.2f, 0.8f, 0.2f, 1.0f); break;
            case Nova::EventDataType::String: color = glm::vec4(0.8f, 0.2f, 0.8f, 1.0f); break;
            case Nova::EventDataType::Vec3: color = glm::vec4(0.8f, 0.8f, 0.2f, 1.0f); break;
            case Nova::EventDataType::Entity: color = glm::vec4(0.2f, 0.4f, 0.8f, 1.0f); break;
            default: color = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f); break;
        }
    }

    if (isHovered) {
        color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Draw pin
    if (pin.kind == Nova::EventPinKind::Flow) {
        // Triangle for flow pins
        ImVec2 points[3];
        if (isOutput) {
            points[0] = ImVec2(pos.x - radius, pos.y - radius);
            points[1] = ImVec2(pos.x + radius, pos.y);
            points[2] = ImVec2(pos.x - radius, pos.y + radius);
        } else {
            points[0] = ImVec2(pos.x + radius, pos.y - radius);
            points[1] = ImVec2(pos.x - radius, pos.y);
            points[2] = ImVec2(pos.x + radius, pos.y + radius);
        }
        drawList->AddTriangleFilled(points[0], points[1], points[2], ImColor(color.r, color.g, color.b, color.a));
    } else {
        // Circle for data pins
        bool connected = pin.connectedNodeId != 0;
        if (connected) {
            drawList->AddCircleFilled(ImVec2(pos.x, pos.y), radius, ImColor(color.r, color.g, color.b, color.a));
        } else {
            drawList->AddCircle(ImVec2(pos.x, pos.y), radius, ImColor(color.r, color.g, color.b, color.a), 12, 2.0f);
        }
    }
}

void EntityEventEditor::RenderConnections() {
    if (!m_currentGraph) return;

    for (const auto& conn : GetConnections(m_currentGraph->graph)) {
        auto* fromVisual = GetNodeVisual(conn.fromNode);
        auto* toVisual = GetNodeVisual(conn.toNode);
        auto fromNode = m_currentGraph->graph.GetNode(conn.fromNode);
        auto toNode = m_currentGraph->graph.GetNode(conn.toNode);

        if (fromVisual && toVisual && fromNode && toNode) {
            glm::vec2 start = GetPinPosition(*fromVisual, fromNode.get(), conn.fromPin, true);
            glm::vec2 end = GetPinPosition(*toVisual, toNode.get(), conn.toPin, false);

            // Determine color based on connection type
            glm::vec4 color{0.8f, 0.8f, 0.8f, 1.0f};

            // Check if it's a flow connection
            for (const auto& pin : GetFlowOutputs(fromNode.get())) {
                if (pin.name == conn.fromPin) {
                    color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
                    break;
                }
            }

            RenderConnectionWire(start, end, color, m_config.connectionThickness);
        }
    }
}

void EntityEventEditor::RenderConnectionWire(const glm::vec2& start, const glm::vec2& end, const glm::vec4& color, float thickness) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Bezier curve
    float dx = std::abs(end.x - start.x);
    float tangentLength = std::max(50.0f * m_viewScale, dx * 0.5f);

    glm::vec2 cp1 = start + glm::vec2(tangentLength, 0);
    glm::vec2 cp2 = end - glm::vec2(tangentLength, 0);

    drawList->AddBezierCubic(
        ImVec2(start.x, start.y),
        ImVec2(cp1.x, cp1.y),
        ImVec2(cp2.x, cp2.y),
        ImVec2(end.x, end.y),
        ImColor(color.r, color.g, color.b, color.a),
        thickness * m_viewScale
    );
}

void EntityEventEditor::RenderPendingConnection() {
    if (!m_currentGraph) return;

    auto* visual = GetNodeVisual(m_connectionStartNode);
    auto node = m_currentGraph->graph.GetNode(m_connectionStartNode);

    if (visual && node) {
        glm::vec2 start = GetPinPosition(*visual, node.get(), m_connectionStartPin, m_connectionStartIsOutput);
        glm::vec2 end = m_connectionEndPos;

        if (!m_connectionStartIsOutput) {
            std::swap(start, end);
        }

        RenderConnectionWire(start, end, glm::vec4(1.0f, 1.0f, 1.0f, 0.5f), m_config.connectionThickness);
    }
}

void EntityEventEditor::RenderSelectionBox() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    glm::vec2 start = CanvasToScreen(m_boxSelectStart);
    glm::vec2 end = CanvasToScreen(m_boxSelectEnd);

    drawList->AddRectFilled(
        ImVec2(std::min(start.x, end.x), std::min(start.y, end.y)),
        ImVec2(std::max(start.x, end.x), std::max(start.y, end.y)),
        ImColor(m_config.selectionColor.r, m_config.selectionColor.g, m_config.selectionColor.b, m_config.selectionColor.a)
    );

    drawList->AddRect(
        ImVec2(std::min(start.x, end.x), std::min(start.y, end.y)),
        ImVec2(std::max(start.x, end.x), std::max(start.y, end.y)),
        ImColor(0.4f, 0.6f, 1.0f, 1.0f)
    );
}

void EntityEventEditor::RenderMinimap() {
    if (!m_currentGraph || m_currentGraph->nodeVisuals.empty()) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Minimap size and position (bottom-right corner)
    const float minimapWidth = 150.0f;
    const float minimapHeight = 100.0f;
    const float padding = 10.0f;

    glm::vec2 minimapPos = m_canvasPos + m_canvasSize - glm::vec2(minimapWidth + padding, minimapHeight + padding);

    // Calculate bounds of all nodes
    glm::vec2 minBounds{std::numeric_limits<float>::max()};
    glm::vec2 maxBounds{std::numeric_limits<float>::lowest()};

    for (const auto& visual : m_currentGraph->nodeVisuals) {
        minBounds = glm::min(minBounds, visual.position);
        maxBounds = glm::max(maxBounds, visual.position + visual.size);
    }

    glm::vec2 graphSize = maxBounds - minBounds;
    if (graphSize.x < 1.0f) graphSize.x = 1.0f;
    if (graphSize.y < 1.0f) graphSize.y = 1.0f;

    // Add padding to bounds
    float boundsPadding = 50.0f;
    minBounds -= glm::vec2(boundsPadding);
    maxBounds += glm::vec2(boundsPadding);
    graphSize = maxBounds - minBounds;

    // Calculate scale to fit in minimap
    float scaleX = minimapWidth / graphSize.x;
    float scaleY = minimapHeight / graphSize.y;
    float scale = std::min(scaleX, scaleY);

    // Background
    drawList->AddRectFilled(
        ImVec2(minimapPos.x, minimapPos.y),
        ImVec2(minimapPos.x + minimapWidth, minimapPos.y + minimapHeight),
        ImColor(0.1f, 0.1f, 0.12f, 0.9f),
        4.0f
    );

    // Border
    drawList->AddRect(
        ImVec2(minimapPos.x, minimapPos.y),
        ImVec2(minimapPos.x + minimapWidth, minimapPos.y + minimapHeight),
        ImColor(0.3f, 0.3f, 0.35f, 1.0f),
        4.0f
    );

    // Draw nodes as small rectangles
    for (const auto& visual : m_currentGraph->nodeVisuals) {
        glm::vec2 nodeMinimapPos = (visual.position - minBounds) * scale + minimapPos;
        glm::vec2 nodeMinimapSize = visual.size * scale;

        // Clamp to minimap bounds
        nodeMinimapSize = glm::clamp(nodeMinimapSize, glm::vec2(2.0f), glm::vec2(20.0f, 10.0f));

        // Get node category color
        auto node = m_currentGraph->graph.GetNode(visual.nodeId);
        glm::vec4 color = node ? GetCategoryColor(node->GetCategory()) : glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);

        // Highlight selected nodes
        if (visual.selected) {
            color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        drawList->AddRectFilled(
            ImVec2(nodeMinimapPos.x, nodeMinimapPos.y),
            ImVec2(nodeMinimapPos.x + nodeMinimapSize.x, nodeMinimapPos.y + nodeMinimapSize.y),
            ImColor(color.r, color.g, color.b, color.a * 0.8f),
            1.0f
        );
    }

    // Draw viewport rectangle (showing current view area)
    glm::vec2 viewTopLeft = ScreenToCanvas(m_canvasPos);
    glm::vec2 viewBottomRight = ScreenToCanvas(m_canvasPos + m_canvasSize);

    glm::vec2 viewMinimapTopLeft = (viewTopLeft - minBounds) * scale + minimapPos;
    glm::vec2 viewMinimapBottomRight = (viewBottomRight - minBounds) * scale + minimapPos;

    // Clamp to minimap bounds
    viewMinimapTopLeft = glm::clamp(viewMinimapTopLeft, minimapPos, minimapPos + glm::vec2(minimapWidth, minimapHeight));
    viewMinimapBottomRight = glm::clamp(viewMinimapBottomRight, minimapPos, minimapPos + glm::vec2(minimapWidth, minimapHeight));

    drawList->AddRect(
        ImVec2(viewMinimapTopLeft.x, viewMinimapTopLeft.y),
        ImVec2(viewMinimapBottomRight.x, viewMinimapBottomRight.y),
        ImColor(1.0f, 1.0f, 1.0f, 0.5f),
        0.0f, 0, 1.0f
    );
}

void EntityEventEditor::RenderNodePalette() {
    ImGui::Text("Node Palette");
    ImGui::Separator();

    ImGui::InputText("##Search", &m_nodeSearchFilter[0], 256);
    ImGui::Separator();

    for (const auto& style : m_categoryStyles) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(style.color.r, style.color.g, style.color.b, 0.5f));

        if (ImGui::CollapsingHeader(style.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            auto nodes = GetNodesInCategory(style.category);

            for (const auto& nodeName : nodes) {
                // Filter by search
                if (!m_nodeSearchFilter.empty()) {
                    std::string lowerName = nodeName;
                    std::string lowerFilter = m_nodeSearchFilter;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
                    if (lowerName.find(lowerFilter) == std::string::npos) continue;
                }

                if (ImGui::Selectable(nodeName.c_str())) {
                    // Add node at center of canvas
                    glm::vec2 center = ScreenToCanvas(m_canvasSize * 0.5f + m_canvasPos);
                    AddNode(nodeName, center);
                }

                // Drag source for drag-and-drop
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("EVENT_NODE", nodeName.c_str(), nodeName.size() + 1);
                    ImGui::Text("Create %s", nodeName.c_str());
                    ImGui::EndDragDropSource();
                }
            }
        }

        ImGui::PopStyleColor();
    }
}

void EntityEventEditor::RenderPropertyPanel() {
    ImGui::Text("Properties");
    ImGui::Separator();

    if (m_selectedNodes.empty()) {
        ImGui::TextDisabled("No node selected");
        return;
    }

    if (m_selectedNodes.size() > 1) {
        ImGui::Text("%zu nodes selected", m_selectedNodes.size());
        return;
    }

    uint64_t nodeId = *m_selectedNodes.begin();
    if (!m_currentGraph) return;

    auto node = m_currentGraph->graph.GetNode(nodeId);
    if (!node) return;

    ImGui::Text("Type: %s", node->GetTypeName());
    ImGui::Text("Display: %s", node->GetDisplayName().c_str());
    ImGui::Separator();

    // Data input default values
    ImGui::Text("Inputs:");
    for (const auto& pin : GetDataInputs(node.get())) {
        if (pin.connectedNodeId != 0) {
            ImGui::TextDisabled("%s: Connected", pin.name.c_str());
        } else {
            // Show editable default value based on type
            switch (pin.dataType) {
                case Nova::EventDataType::Bool: {
                    bool val = std::get<bool>(pin.defaultValue);
                    if (ImGui::Checkbox(pin.name.c_str(), &val)) {
                        // Update pin default value
                        if (auto* inputPin = node->GetInput(pin.name)) {
                            inputPin->defaultValue = val;
                            m_currentGraph->modified = true;
                            if (OnGraphModified) OnGraphModified();
                        }
                    }
                    break;
                }
                case Nova::EventDataType::Int: {
                    int val = std::get<int>(pin.defaultValue);
                    if (ImGui::DragInt(pin.name.c_str(), &val)) {
                        // Update pin default value
                        if (auto* inputPin = node->GetInput(pin.name)) {
                            inputPin->defaultValue = val;
                            m_currentGraph->modified = true;
                            if (OnGraphModified) OnGraphModified();
                        }
                    }
                    break;
                }
                case Nova::EventDataType::Float: {
                    float val = std::get<float>(pin.defaultValue);
                    if (ImGui::DragFloat(pin.name.c_str(), &val, 0.1f)) {
                        // Update pin default value
                        if (auto* inputPin = node->GetInput(pin.name)) {
                            inputPin->defaultValue = val;
                            m_currentGraph->modified = true;
                            if (OnGraphModified) OnGraphModified();
                        }
                    }
                    break;
                }
                case Nova::EventDataType::String: {
                    std::string val = std::get<std::string>(pin.defaultValue);
                    char buffer[256];
                    strncpy(buffer, val.c_str(), sizeof(buffer));
                    buffer[sizeof(buffer) - 1] = '\0';  // Ensure null termination
                    if (ImGui::InputText(pin.name.c_str(), buffer, sizeof(buffer))) {
                        // Update pin default value
                        if (auto* inputPin = node->GetInput(pin.name)) {
                            inputPin->defaultValue = std::string(buffer);
                            m_currentGraph->modified = true;
                            if (OnGraphModified) OnGraphModified();
                        }
                    }
                    break;
                }
                default:
                    ImGui::Text("%s: %s", pin.name.c_str(), "N/A");
                    break;
            }
        }
    }
}

void EntityEventEditor::RenderCodePreview() {
    ImGui::Text("Python Code Preview");
    ImGui::Separator();

    std::string code = CompileToPython();

    ImGui::BeginChild("CodeScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(code.c_str());
    ImGui::EndChild();
}

void EntityEventEditor::RenderContextMenu() {
    if (ImGui::BeginPopup("NodeContextMenu")) {
        ImGui::InputText("##Filter", &m_contextMenuFilter[0], 256);
        ImGui::Separator();

        for (const auto& style : m_categoryStyles) {
            if (ImGui::BeginMenu(style.name.c_str())) {
                auto nodes = GetNodesInCategory(style.category);

                for (const auto& nodeName : nodes) {
                    if (ImGui::MenuItem(nodeName.c_str())) {
                        AddNode(nodeName, m_contextMenuPos);
                    }
                }

                ImGui::EndMenu();
            }
        }

        ImGui::EndPopup();
    } else {
        m_showContextMenu = false;
    }
}

void EntityEventEditor::ProcessInput() {
    ImGuiIO& io = ImGui::GetIO();

    bool canvasHovered = ImGui::IsWindowHovered();
    glm::vec2 mousePos = glm::vec2(io.MousePos.x, io.MousePos.y);
    glm::vec2 canvasMousePos = ScreenToCanvas(mousePos);

    // Update connection end position
    if (m_isConnecting) {
        m_connectionEndPos = mousePos;
    }

    // Find hovered node and pin
    m_hoveredNode = 0;
    m_hoveredPin.clear();

    if (m_currentGraph && canvasHovered) {
        for (auto& visual : m_currentGraph->nodeVisuals) {
            glm::vec2 screenPos = CanvasToScreen(visual.position);
            glm::vec2 screenSize = visual.size * m_viewScale;

            if (mousePos.x >= screenPos.x && mousePos.x <= screenPos.x + screenSize.x &&
                mousePos.y >= screenPos.y && mousePos.y <= screenPos.y + screenSize.y) {
                m_hoveredNode = visual.nodeId;

                // Check pins
                auto node = m_currentGraph->graph.GetNode(visual.nodeId);
                if (node) {
                    // Check input pins
                    for (const auto& pin : GetFlowInputs(node.get())) {
                        glm::vec2 pinPos = GetPinPosition(visual, node.get(), pin.name, false);
                        if (glm::distance(mousePos, pinPos) < m_config.pinRadius * m_viewScale * 2.0f) {
                            m_hoveredPin = pin.name;
                            m_hoveredPinIsOutput = false;
                        }
                    }
                    for (const auto& pin : GetDataInputs(node.get())) {
                        glm::vec2 pinPos = GetPinPosition(visual, node.get(), pin.name, false);
                        if (glm::distance(mousePos, pinPos) < m_config.pinRadius * m_viewScale * 2.0f) {
                            m_hoveredPin = pin.name;
                            m_hoveredPinIsOutput = false;
                        }
                    }
                    // Check output pins
                    for (const auto& pin : GetFlowOutputs(node.get())) {
                        glm::vec2 pinPos = GetPinPosition(visual, node.get(), pin.name, true);
                        if (glm::distance(mousePos, pinPos) < m_config.pinRadius * m_viewScale * 2.0f) {
                            m_hoveredPin = pin.name;
                            m_hoveredPinIsOutput = true;
                        }
                    }
                    for (const auto& pin : GetDataOutputs(node.get())) {
                        glm::vec2 pinPos = GetPinPosition(visual, node.get(), pin.name, true);
                        if (glm::distance(mousePos, pinPos) < m_config.pinRadius * m_viewScale * 2.0f) {
                            m_hoveredPin = pin.name;
                            m_hoveredPinIsOutput = true;
                        }
                    }
                }
                break;
            }
        }
    }

    // Mouse button handling
    if (canvasHovered) {
        // Left click
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (!m_hoveredPin.empty()) {
                // Start or complete connection
                if (m_isConnecting) {
                    CompleteConnection(m_hoveredNode, m_hoveredPin);
                } else {
                    StartConnection(m_hoveredNode, m_hoveredPin, m_hoveredPinIsOutput);
                }
            } else if (m_hoveredNode != 0) {
                // Select node
                if (!io.KeyCtrl && m_selectedNodes.find(m_hoveredNode) == m_selectedNodes.end()) {
                    ClearSelection();
                }
                SelectNode(m_hoveredNode, io.KeyCtrl);

                // Start dragging
                m_isDraggingNodes = true;
                m_dragStartPos = canvasMousePos;
                m_dragStartPositions.clear();
                for (uint64_t id : m_selectedNodes) {
                    if (auto* v = GetNodeVisual(id)) {
                        m_dragStartPositions[id] = v->position;
                    }
                }
            } else {
                // Cancel connection or start box select
                if (m_isConnecting) {
                    CancelConnection();
                } else {
                    ClearSelection();
                    m_isBoxSelecting = true;
                    m_boxSelectStart = canvasMousePos;
                    m_boxSelectEnd = canvasMousePos;
                }
            }
        }

        // Left drag
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            if (m_isDraggingNodes) {
                glm::vec2 delta = canvasMousePos - m_dragStartPos;
                for (uint64_t id : m_selectedNodes) {
                    if (auto* v = GetNodeVisual(id)) {
                        v->position = m_dragStartPositions[id] + delta;
                        if (m_config.snapToGrid) {
                            v->position = SnapToGrid(v->position);
                        }
                    }
                }
                if (m_currentGraph) m_currentGraph->modified = true;
            } else if (m_isBoxSelecting) {
                m_boxSelectEnd = canvasMousePos;
            }
        }

        // Left release
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            if (m_isBoxSelecting) {
                BoxSelectNodes(m_boxSelectStart, m_boxSelectEnd);
                m_isBoxSelecting = false;
            }
            m_isDraggingNodes = false;
        }

        // Middle click for panning
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
            m_isPanning = true;
            m_panStartPos = mousePos;
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            if (m_isPanning) {
                glm::vec2 delta = mousePos - m_panStartPos;
                Pan(delta);
                m_panStartPos = mousePos;
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
            m_isPanning = false;
        }

        // Right click context menu
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            if (m_hoveredNode != 0) {
                // Node context menu
            } else {
                m_showContextMenu = true;
                m_contextMenuPos = canvasMousePos;
                ImGui::OpenPopup("NodeContextMenu");
            }
        }

        // Scroll zoom
        if (io.MouseWheel != 0.0f) {
            Zoom(io.MouseWheel, mousePos);
        }

        // Drag drop target
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EVENT_NODE")) {
                std::string nodeName(static_cast<const char*>(payload->Data));
                AddNode(nodeName, canvasMousePos);
            }
            ImGui::EndDragDropTarget();
        }
    }

    // Keyboard shortcuts
    if (canvasHovered && !io.WantTextInput) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) CopySelectedNodes();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) PasteNodes(canvasMousePos);
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X)) CutSelectedNodes();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) SelectAllNodes();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) DuplicateSelectedNodes();
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) RemoveSelectedNodes();
        if (ImGui::IsKeyPressed(ImGuiKey_F)) FrameSelected();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            if (m_isConnecting) CancelConnection();
            else ClearSelection();
        }
    }
}

void EntityEventEditor::Update(float deltaTime) {
    // Animation updates, etc.
}

// =========================================================================
// Helper Functions
// =========================================================================

glm::vec2 EntityEventEditor::ScreenToCanvas(const glm::vec2& screen) const {
    return (screen - m_canvasPos - m_viewOffset) / m_viewScale;
}

glm::vec2 EntityEventEditor::CanvasToScreen(const glm::vec2& canvas) const {
    return canvas * m_viewScale + m_viewOffset + m_canvasPos;
}

EventNodeVisual* EntityEventEditor::GetNodeVisual(uint64_t nodeId) {
    if (!m_currentGraph) return nullptr;

    for (auto& visual : m_currentGraph->nodeVisuals) {
        if (visual.nodeId == nodeId) {
            return &visual;
        }
    }
    return nullptr;
}

EventNodeVisual& EntityEventEditor::CreateNodeVisual(uint64_t nodeId, const glm::vec2& position) {
    EventNodeVisual visual;
    visual.nodeId = nodeId;
    visual.position = position;
    visual.size = glm::vec2(m_config.nodeWidth, 100.0f);

    m_currentGraph->nodeVisuals.push_back(visual);
    return m_currentGraph->nodeVisuals.back();
}

glm::vec2 EntityEventEditor::GetPinPosition(const EventNodeVisual& visual, Nova::EventNode* node, const std::string& pinName, bool isOutput) {
    glm::vec2 screenPos = CanvasToScreen(visual.position);
    float yOffset = 32.0f * m_viewScale;
    float pinSpacing = 20.0f * m_viewScale;

    if (isOutput) {
        float rightX = screenPos.x + visual.size.x * m_viewScale;

        for (const auto& pin : GetFlowOutputs(node)) {
            if (pin.name == pinName) {
                return glm::vec2(rightX, screenPos.y + yOffset);
            }
            yOffset += pinSpacing;
        }

        for (const auto& pin : GetDataOutputs(node)) {
            if (pin.name == pinName) {
                return glm::vec2(rightX, screenPos.y + yOffset);
            }
            yOffset += pinSpacing;
        }
    } else {
        for (const auto& pin : GetFlowInputs(node)) {
            if (pin.name == pinName) {
                return glm::vec2(screenPos.x, screenPos.y + yOffset);
            }
            yOffset += pinSpacing;
        }

        for (const auto& pin : GetDataInputs(node)) {
            if (pin.name == pinName) {
                return glm::vec2(screenPos.x, screenPos.y + yOffset);
            }
            yOffset += pinSpacing;
        }
    }

    return screenPos;
}

glm::vec4 EntityEventEditor::GetCategoryColor(Nova::EventNodeCategory category) const {
    for (const auto& style : m_categoryStyles) {
        if (style.category == category) {
            return style.color;
        }
    }
    return glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
}

glm::vec2 EntityEventEditor::SnapToGrid(const glm::vec2& position) const {
    return glm::vec2(
        std::round(position.x / m_config.gridSize) * m_config.gridSize,
        std::round(position.y / m_config.gridSize) * m_config.gridSize
    );
}

} // namespace Vehement
