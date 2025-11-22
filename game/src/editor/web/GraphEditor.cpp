#include "GraphEditor.hpp"
#include "WebViewManager.hpp"
#include "JSBridge.hpp"
#include "../Editor.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <queue>
#include <imgui.h>

namespace Vehement {
namespace WebEditor {

GraphEditor::GraphEditor(Editor* editor)
    : m_editor(editor)
{
}

GraphEditor::~GraphEditor() {
    Shutdown();
}

bool GraphEditor::Initialize() {
    // Create web view
    WebViewConfig config;
    config.id = "graph_editor";
    config.title = "Graph Editor";
    config.width = 1200;
    config.height = 800;
    config.debug = true;

    m_webView.reset(new WebView(config));

    // Create bridge
    m_bridge.reset(new JSBridge());
    SetupJSBridge();

    // Load HTML
    std::string htmlPath = WebViewManager::Instance().ResolvePath("editor/html/graph_editor.html");
    if (!m_webView->LoadFile(htmlPath)) {
        // Use inline HTML fallback
        m_webView->LoadHtml(R"(
<!DOCTYPE html>
<html>
<head>
    <link rel="stylesheet" href="editor.css">
    <script src="editor_core.js"></script>
    <script src="graph_editor.js"></script>
</head>
<body class="graph-editor">
    <div id="graph-container">
        <canvas id="graph-canvas"></canvas>
        <div id="minimap"></div>
    </div>
    <div id="node-palette"></div>
    <script>
        var graphEditor = new GraphEditor('graph-canvas', 'minimap', 'node-palette');
    </script>
</body>
</html>
)");
    }

    // Register default node types
    RegisterDefaultNodeTypes();

    return true;
}

void GraphEditor::Shutdown() {
    m_webView.reset();
    m_bridge.reset();
    Clear();
}

void GraphEditor::Update(float deltaTime) {
    if (m_webView) {
        m_webView->Update(deltaTime);
    }

    if (m_bridge) {
        m_bridge->ProcessPending();
    }
}

void GraphEditor::Render() {
    if (!m_webView) return;

    WebViewManager::Instance().RenderImGuiInline(m_webView->GetId());
}

void GraphEditor::SetupJSBridge() {
    if (!m_bridge) return;

    m_bridge->SetScriptExecutor([this](const std::string& script, JSCallback callback) {
        if (m_webView) {
            m_webView->ExecuteJS(script, [callback](const std::string& result) {
                if (callback) {
                    callback(JSResult::Success(JSValue::FromJson(result)));
                }
            });
        }
    });

    m_webView->SetMessageHandler([this](const std::string& type, const std::string& payload) {
        m_bridge->HandleIncomingMessage("{\"type\":\"" + type + "\",\"payload\":" + payload + "}");
    });

    RegisterBridgeFunctions();
}

void GraphEditor::RegisterBridgeFunctions() {
    if (!m_bridge) return;

    // Get all nodes
    m_bridge->RegisterFunction("graphEditor.getNodes", [this](const auto& args) {
        JSValue::Array nodes;
        for (const auto& node : m_nodes) {
            JSValue::Object obj;
            obj["id"] = node.id;
            obj["type"] = node.type;
            obj["title"] = node.title;
            obj["subtitle"] = node.subtitle;
            obj["x"] = node.x;
            obj["y"] = node.y;
            obj["width"] = node.width;
            obj["height"] = node.height;
            obj["collapsed"] = node.collapsed;
            obj["selected"] = m_selection.nodeIds.count(node.id) > 0;
            obj["data"] = JSValue::FromJson(node.dataJson);

            JSValue::Array inputs;
            for (const auto& port : node.inputs) {
                JSValue::Object portObj;
                portObj["id"] = port.id;
                portObj["name"] = port.name;
                portObj["type"] = port.type;
                inputs.push_back(portObj);
            }
            obj["inputs"] = inputs;

            JSValue::Array outputs;
            for (const auto& port : node.outputs) {
                JSValue::Object portObj;
                portObj["id"] = port.id;
                portObj["name"] = port.name;
                portObj["type"] = port.type;
                outputs.push_back(portObj);
            }
            obj["outputs"] = outputs;

            nodes.push_back(obj);
        }
        return JSResult::Success(nodes);
    });

    // Get all connections
    m_bridge->RegisterFunction("graphEditor.getConnections", [this](const auto& args) {
        JSValue::Array conns;
        for (const auto& conn : m_connections) {
            JSValue::Object obj;
            obj["id"] = conn.id;
            obj["sourceNodeId"] = conn.sourceNodeId;
            obj["sourcePortId"] = conn.sourcePortId;
            obj["targetNodeId"] = conn.targetNodeId;
            obj["targetPortId"] = conn.targetPortId;
            obj["selected"] = m_selection.connectionIds.count(conn.id) > 0;
            conns.push_back(obj);
        }
        return JSResult::Success(conns);
    });

    // Get node types
    m_bridge->RegisterFunction("graphEditor.getNodeTypes", [this](const auto& args) {
        JSValue::Array types;
        for (const auto& [id, def] : m_nodeTypes) {
            JSValue::Object obj;
            obj["type"] = def.type;
            obj["category"] = def.category;
            obj["title"] = def.title;
            obj["description"] = def.description;
            obj["icon"] = def.icon;
            types.push_back(obj);
        }
        return JSResult::Success(types);
    });

    // Create node
    m_bridge->RegisterFunction("graphEditor.createNode", [this](const auto& args) {
        if (args.size() < 3) {
            return JSResult::Error("Missing type, x, y");
        }

        std::string type = args[0].GetString();
        float x = static_cast<float>(args[1].GetNumber());
        float y = static_cast<float>(args[2].GetNumber());

        std::string id = CreateNode(type, x, y);
        if (id.empty()) {
            return JSResult::Error("Failed to create node");
        }

        return JSResult::Success(id);
    });

    // Delete node
    m_bridge->RegisterFunction("graphEditor.deleteNode", [this](const auto& args) {
        if (args.empty()) {
            return JSResult::Error("Missing node ID");
        }

        if (DeleteNode(args[0].GetString())) {
            return JSResult::Success();
        }
        return JSResult::Error("Failed to delete node");
    });

    // Update node position
    m_bridge->RegisterFunction("graphEditor.setNodePosition", [this](const auto& args) {
        if (args.size() < 3) {
            return JSResult::Error("Missing nodeId, x, y");
        }

        SetNodePosition(args[0].GetString(),
                        static_cast<float>(args[1].GetNumber()),
                        static_cast<float>(args[2].GetNumber()));
        return JSResult::Success();
    });

    // Create connection
    m_bridge->RegisterFunction("graphEditor.createConnection", [this](const auto& args) {
        if (args.size() < 4) {
            return JSResult::Error("Missing connection parameters");
        }

        std::string id = CreateConnection(
            args[0].GetString(), args[1].GetString(),
            args[2].GetString(), args[3].GetString()
        );

        if (id.empty()) {
            return JSResult::Error("Failed to create connection");
        }

        return JSResult::Success(id);
    });

    // Delete connection
    m_bridge->RegisterFunction("graphEditor.deleteConnection", [this](const auto& args) {
        if (args.empty()) {
            return JSResult::Error("Missing connection ID");
        }

        if (DeleteConnection(args[0].GetString())) {
            return JSResult::Success();
        }
        return JSResult::Error("Failed to delete connection");
    });

    // Check if can connect
    m_bridge->RegisterFunction("graphEditor.canConnect", [this](const auto& args) {
        if (args.size() < 4) {
            return JSResult::Error("Missing parameters");
        }

        bool canConnect = CanConnect(
            args[0].GetString(), args[1].GetString(),
            args[2].GetString(), args[3].GetString()
        );

        return JSResult::Success(canConnect);
    });

    // Select node
    m_bridge->RegisterFunction("graphEditor.selectNode", [this](const auto& args) {
        if (args.empty()) {
            return JSResult::Error("Missing node ID");
        }

        bool addToSelection = args.size() > 1 && args[1].GetBool();
        SelectNode(args[0].GetString(), addToSelection);
        return JSResult::Success();
    });

    // Clear selection
    m_bridge->RegisterFunction("graphEditor.clearSelection", [this](const auto& args) {
        ClearSelection();
        return JSResult::Success();
    });

    // Set viewport
    m_bridge->RegisterFunction("graphEditor.setViewport", [this](const auto& args) {
        if (args.size() < 3) {
            return JSResult::Error("Missing panX, panY, zoom");
        }

        m_viewport.panX = static_cast<float>(args[0].GetNumber());
        m_viewport.panY = static_cast<float>(args[1].GetNumber());
        m_viewport.zoom = static_cast<float>(args[2].GetNumber());
        return JSResult::Success();
    });

    // Get viewport
    m_bridge->RegisterFunction("graphEditor.getViewport", [this](const auto& args) {
        JSValue::Object obj;
        obj["panX"] = m_viewport.panX;
        obj["panY"] = m_viewport.panY;
        obj["zoom"] = m_viewport.zoom;
        return JSResult::Success(obj);
    });

    // Undo
    m_bridge->RegisterFunction("graphEditor.undo", [this](const auto& args) {
        if (CanUndo()) {
            Undo();
            return JSResult::Success();
        }
        return JSResult::Error("Nothing to undo");
    });

    // Redo
    m_bridge->RegisterFunction("graphEditor.redo", [this](const auto& args) {
        if (CanRedo()) {
            Redo();
            return JSResult::Success();
        }
        return JSResult::Error("Nothing to redo");
    });

    // Auto arrange
    m_bridge->RegisterFunction("graphEditor.autoArrange", [this](const auto& args) {
        LayoutAlgorithm algo = LayoutAlgorithm::Hierarchical;
        if (!args.empty()) {
            int algoInt = args[0].GetInt();
            algo = static_cast<LayoutAlgorithm>(algoInt);
        }
        AutoArrange(algo);
        return JSResult::Success();
    });

    // Set node data
    m_bridge->RegisterFunction("graphEditor.setNodeData", [this](const auto& args) {
        if (args.size() < 2) {
            return JSResult::Error("Missing nodeId and data");
        }

        SetNodeData(args[0].GetString(), JSON::Stringify(args[1]));
        return JSResult::Success();
    });
}

void GraphEditor::RegisterDefaultNodeTypes() {
    // Tech Tree node types
    NodeTypeDefinition techNode;
    techNode.type = "tech";
    techNode.category = "Tech Tree";
    techNode.title = "Technology";
    techNode.description = "A researchable technology";
    techNode.icon = "flask";
    techNode.headerColor = "#2e7d32";
    techNode.defaultInputs = {{.id = "prereq", .name = "Prerequisites", .type = "tech", .isInput = true, .allowMultiple = true}};
    techNode.defaultOutputs = {{.id = "unlocks", .name = "Unlocks", .type = "tech", .isInput = false, .allowMultiple = true}};
    techNode.defaultDataJson = R"({"name": "New Tech", "cost": 100, "time": 60})";
    RegisterNodeType(techNode);

    // Behavior Tree node types
    NodeTypeDefinition selectorNode;
    selectorNode.type = "bt_selector";
    selectorNode.category = "Behavior Tree";
    selectorNode.title = "Selector";
    selectorNode.description = "Tries children until one succeeds";
    selectorNode.headerColor = "#1565c0";
    selectorNode.defaultInputs = {{.id = "in", .name = "In", .type = "flow", .isInput = true}};
    selectorNode.defaultOutputs = {{.id = "out", .name = "Children", .type = "flow", .isInput = false, .allowMultiple = true}};
    RegisterNodeType(selectorNode);

    NodeTypeDefinition sequenceNode;
    sequenceNode.type = "bt_sequence";
    sequenceNode.category = "Behavior Tree";
    sequenceNode.title = "Sequence";
    sequenceNode.description = "Runs children in order until one fails";
    sequenceNode.headerColor = "#6a1b9a";
    sequenceNode.defaultInputs = {{.id = "in", .name = "In", .type = "flow", .isInput = true}};
    sequenceNode.defaultOutputs = {{.id = "out", .name = "Children", .type = "flow", .isInput = false, .allowMultiple = true}};
    RegisterNodeType(sequenceNode);

    NodeTypeDefinition actionNode;
    actionNode.type = "bt_action";
    actionNode.category = "Behavior Tree";
    actionNode.title = "Action";
    actionNode.description = "Executes an action";
    actionNode.headerColor = "#c62828";
    actionNode.defaultInputs = {{.id = "in", .name = "In", .type = "flow", .isInput = true}};
    actionNode.defaultDataJson = R"({"action": ""})";
    RegisterNodeType(actionNode);

    NodeTypeDefinition conditionNode;
    conditionNode.type = "bt_condition";
    conditionNode.category = "Behavior Tree";
    conditionNode.title = "Condition";
    conditionNode.description = "Checks a condition";
    conditionNode.headerColor = "#f57c00";
    conditionNode.defaultInputs = {{.id = "in", .name = "In", .type = "flow", .isInput = true}};
    conditionNode.defaultDataJson = R"({"condition": ""})";
    RegisterNodeType(conditionNode);
}

void GraphEditor::RegisterNodeType(const NodeTypeDefinition& definition) {
    m_nodeTypes[definition.type] = definition;
}

void GraphEditor::UnregisterNodeType(const std::string& type) {
    m_nodeTypes.erase(type);
}

const NodeTypeDefinition* GraphEditor::GetNodeType(const std::string& type) const {
    auto it = m_nodeTypes.find(type);
    return it != m_nodeTypes.end() ? &it->second : nullptr;
}

std::vector<std::string> GraphEditor::GetNodeTypes() const {
    std::vector<std::string> types;
    types.reserve(m_nodeTypes.size());
    for (const auto& [type, def] : m_nodeTypes) {
        types.push_back(type);
    }
    return types;
}

std::vector<NodeTypeDefinition> GraphEditor::GetNodeTypesByCategory(const std::string& category) const {
    std::vector<NodeTypeDefinition> result;
    for (const auto& [type, def] : m_nodeTypes) {
        if (def.category == category) {
            result.push_back(def);
        }
    }
    return result;
}

void GraphEditor::Clear() {
    m_nodes.clear();
    m_connections.clear();
    m_nodeIndex.clear();
    m_connectionIndex.clear();
    m_selection.clear();
    m_undoHistory.clear();
    m_undoIndex = 0;
    m_isDirty = false;
}

bool GraphEditor::LoadFromJson(const std::string& json) {
    Clear();

    JSValue data = JSValue::FromJson(json);
    if (!data.IsObject()) {
        return false;
    }

    // Load nodes
    if (data["nodes"].IsArray()) {
        for (const auto& nodeVal : data["nodes"].AsArray()) {
            GraphNode node;
            node.id = nodeVal["id"].GetString();
            node.type = nodeVal["type"].GetString();
            node.title = nodeVal["title"].GetString(node.type);
            node.subtitle = nodeVal["subtitle"].GetString();
            node.x = static_cast<float>(nodeVal["x"].GetNumber());
            node.y = static_cast<float>(nodeVal["y"].GetNumber());
            node.width = static_cast<float>(nodeVal["width"].GetNumber(200.0));
            node.height = static_cast<float>(nodeVal["height"].GetNumber(100.0));
            node.collapsed = nodeVal["collapsed"].GetBool();

            if (nodeVal["data"].IsObject()) {
                node.dataJson = JSON::Stringify(nodeVal["data"]);
            }

            // Get ports from type definition or from data
            auto* typeDef = GetNodeType(node.type);
            if (typeDef) {
                node.inputs = typeDef->defaultInputs;
                node.outputs = typeDef->defaultOutputs;
                node.headerColor = typeDef->headerColor;
            }

            m_nodeIndex[node.id] = m_nodes.size();
            m_nodes.push_back(std::move(node));

            // Update ID counter
            if (node.id.substr(0, 5) == "node_") {
                uint64_t num = std::stoull(node.id.substr(5));
                if (num >= m_nextNodeId) m_nextNodeId = num + 1;
            }
        }
    }

    // Load connections
    if (data["connections"].IsArray()) {
        for (const auto& connVal : data["connections"].AsArray()) {
            NodeConnection conn;
            conn.id = connVal["id"].GetString();
            conn.sourceNodeId = connVal["sourceNodeId"].GetString();
            conn.sourcePortId = connVal["sourcePortId"].GetString();
            conn.targetNodeId = connVal["targetNodeId"].GetString();
            conn.targetPortId = connVal["targetPortId"].GetString();

            m_connectionIndex[conn.id] = m_connections.size();
            m_connections.push_back(std::move(conn));

            // Update ID counter
            if (conn.id.substr(0, 5) == "conn_") {
                uint64_t num = std::stoull(conn.id.substr(5));
                if (num >= m_nextConnectionId) m_nextConnectionId = num + 1;
            }
        }
    }

    // Load viewport
    if (data["viewport"].IsObject()) {
        m_viewport.panX = static_cast<float>(data["viewport"]["panX"].GetNumber());
        m_viewport.panY = static_cast<float>(data["viewport"]["panY"].GetNumber());
        m_viewport.zoom = static_cast<float>(data["viewport"]["zoom"].GetNumber(1.0));
    }

    SaveUndoState();
    return true;
}

std::string GraphEditor::SaveToJson() const {
    JSValue::Object data;

    // Save nodes
    JSValue::Array nodes;
    for (const auto& node : m_nodes) {
        JSValue::Object obj;
        obj["id"] = node.id;
        obj["type"] = node.type;
        obj["title"] = node.title;
        obj["subtitle"] = node.subtitle;
        obj["x"] = node.x;
        obj["y"] = node.y;
        obj["width"] = node.width;
        obj["height"] = node.height;
        obj["collapsed"] = node.collapsed;

        if (!node.dataJson.empty()) {
            obj["data"] = JSValue::FromJson(node.dataJson);
        }

        nodes.push_back(obj);
    }
    data["nodes"] = nodes;

    // Save connections
    JSValue::Array conns;
    for (const auto& conn : m_connections) {
        JSValue::Object obj;
        obj["id"] = conn.id;
        obj["sourceNodeId"] = conn.sourceNodeId;
        obj["sourcePortId"] = conn.sourcePortId;
        obj["targetNodeId"] = conn.targetNodeId;
        obj["targetPortId"] = conn.targetPortId;
        conns.push_back(obj);
    }
    data["connections"] = conns;

    // Save viewport
    JSValue::Object viewport;
    viewport["panX"] = m_viewport.panX;
    viewport["panY"] = m_viewport.panY;
    viewport["zoom"] = m_viewport.zoom;
    data["viewport"] = viewport;

    return JSON::Stringify(JSValue(data), true);
}

bool GraphEditor::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return LoadFromJson(buffer.str());
}

bool GraphEditor::SaveToFile(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << SaveToJson();
    m_isDirty = false;
    return true;
}

std::string GraphEditor::CreateNode(const std::string& type, float x, float y) {
    auto* typeDef = GetNodeType(type);
    if (!typeDef) {
        return "";
    }

    SaveUndoState();

    GraphNode node;
    node.id = GenerateNodeId();
    node.type = type;
    node.title = typeDef->title;
    node.x = x;
    node.y = y;
    node.inputs = typeDef->defaultInputs;
    node.outputs = typeDef->defaultOutputs;
    node.headerColor = typeDef->headerColor;
    node.dataJson = typeDef->defaultDataJson;

    m_nodeIndex[node.id] = m_nodes.size();
    m_nodes.push_back(std::move(node));
    m_isDirty = true;

    if (OnNodeCreated) {
        OnNodeCreated(m_nodes.back().id);
    }

    if (OnGraphChanged) {
        OnGraphChanged();
    }

    // Notify web view
    if (m_bridge) {
        m_bridge->EmitEvent("nodeCreated", JSValue(m_nodes.back().id));
    }

    return m_nodes.back().id;
}

bool GraphEditor::DeleteNode(const std::string& nodeId) {
    auto it = m_nodeIndex.find(nodeId);
    if (it == m_nodeIndex.end()) {
        return false;
    }

    SaveUndoState();

    // Remove connections to/from this node
    m_connections.erase(
        std::remove_if(m_connections.begin(), m_connections.end(),
                       [&nodeId](const NodeConnection& conn) {
                           return conn.sourceNodeId == nodeId || conn.targetNodeId == nodeId;
                       }),
        m_connections.end());

    // Rebuild connection index
    m_connectionIndex.clear();
    for (size_t i = 0; i < m_connections.size(); ++i) {
        m_connectionIndex[m_connections[i].id] = i;
    }

    // Remove node
    size_t index = it->second;
    m_nodes.erase(m_nodes.begin() + index);
    m_nodeIndex.erase(it);

    // Rebuild node index
    m_nodeIndex.clear();
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        m_nodeIndex[m_nodes[i].id] = i;
    }

    // Remove from selection
    m_selection.nodeIds.erase(nodeId);

    m_isDirty = true;

    if (OnNodeDeleted) {
        OnNodeDeleted(nodeId);
    }

    if (OnGraphChanged) {
        OnGraphChanged();
    }

    return true;
}

std::string GraphEditor::DuplicateNode(const std::string& nodeId) {
    auto* node = GetNode(nodeId);
    if (!node) {
        return "";
    }

    SaveUndoState();

    GraphNode newNode = *node;
    newNode.id = GenerateNodeId();
    newNode.x += 50;
    newNode.y += 50;
    newNode.selected = false;

    m_nodeIndex[newNode.id] = m_nodes.size();
    m_nodes.push_back(std::move(newNode));
    m_isDirty = true;

    if (OnNodeCreated) {
        OnNodeCreated(m_nodes.back().id);
    }

    return m_nodes.back().id;
}

GraphNode* GraphEditor::GetNode(const std::string& nodeId) {
    auto it = m_nodeIndex.find(nodeId);
    if (it != m_nodeIndex.end() && it->second < m_nodes.size()) {
        return &m_nodes[it->second];
    }
    return nullptr;
}

const GraphNode* GraphEditor::GetNode(const std::string& nodeId) const {
    auto it = m_nodeIndex.find(nodeId);
    if (it != m_nodeIndex.end() && it->second < m_nodes.size()) {
        return &m_nodes[it->second];
    }
    return nullptr;
}

void GraphEditor::SetNodePosition(const std::string& nodeId, float x, float y) {
    auto* node = GetNode(nodeId);
    if (node) {
        node->x = x;
        node->y = y;
        m_isDirty = true;
    }
}

void GraphEditor::SetNodeData(const std::string& nodeId, const std::string& dataJson) {
    auto* node = GetNode(nodeId);
    if (node) {
        SaveUndoState();
        std::string oldData = node->dataJson;
        node->dataJson = dataJson;
        m_isDirty = true;

        if (OnNodeDataChanged) {
            OnNodeDataChanged(nodeId, dataJson);
        }
    }
}

void GraphEditor::SetNodeCollapsed(const std::string& nodeId, bool collapsed) {
    auto* node = GetNode(nodeId);
    if (node) {
        node->collapsed = collapsed;
    }
}

std::string GraphEditor::CreateConnection(const std::string& sourceNodeId,
                                           const std::string& sourcePortId,
                                           const std::string& targetNodeId,
                                           const std::string& targetPortId) {
    if (!CanConnect(sourceNodeId, sourcePortId, targetNodeId, targetPortId)) {
        return "";
    }

    SaveUndoState();

    NodeConnection conn;
    conn.id = GenerateConnectionId();
    conn.sourceNodeId = sourceNodeId;
    conn.sourcePortId = sourcePortId;
    conn.targetNodeId = targetNodeId;
    conn.targetPortId = targetPortId;

    m_connectionIndex[conn.id] = m_connections.size();
    m_connections.push_back(std::move(conn));
    m_isDirty = true;

    if (OnConnectionCreated) {
        OnConnectionCreated(m_connections.back().id);
    }

    if (OnGraphChanged) {
        OnGraphChanged();
    }

    return m_connections.back().id;
}

bool GraphEditor::DeleteConnection(const std::string& connectionId) {
    auto it = m_connectionIndex.find(connectionId);
    if (it == m_connectionIndex.end()) {
        return false;
    }

    SaveUndoState();

    size_t index = it->second;
    m_connections.erase(m_connections.begin() + index);
    m_connectionIndex.erase(it);

    // Rebuild index
    m_connectionIndex.clear();
    for (size_t i = 0; i < m_connections.size(); ++i) {
        m_connectionIndex[m_connections[i].id] = i;
    }

    m_selection.connectionIds.erase(connectionId);
    m_isDirty = true;

    if (OnConnectionDeleted) {
        OnConnectionDeleted(connectionId);
    }

    if (OnGraphChanged) {
        OnGraphChanged();
    }

    return true;
}

NodeConnection* GraphEditor::GetConnection(const std::string& connectionId) {
    auto it = m_connectionIndex.find(connectionId);
    if (it != m_connectionIndex.end() && it->second < m_connections.size()) {
        return &m_connections[it->second];
    }
    return nullptr;
}

std::vector<NodeConnection> GraphEditor::GetNodeConnections(const std::string& nodeId) const {
    std::vector<NodeConnection> result;
    for (const auto& conn : m_connections) {
        if (conn.sourceNodeId == nodeId || conn.targetNodeId == nodeId) {
            result.push_back(conn);
        }
    }
    return result;
}

bool GraphEditor::CanConnect(const std::string& sourceNodeId,
                              const std::string& sourcePortId,
                              const std::string& targetNodeId,
                              const std::string& targetPortId) const {
    // Can't connect to self
    if (sourceNodeId == targetNodeId) {
        return false;
    }

    // Check nodes exist
    auto* sourceNode = GetNode(sourceNodeId);
    auto* targetNode = GetNode(targetNodeId);
    if (!sourceNode || !targetNode) {
        return false;
    }

    // Check ports exist
    bool foundSourcePort = false;
    for (const auto& port : sourceNode->outputs) {
        if (port.id == sourcePortId) {
            foundSourcePort = true;
            break;
        }
    }

    bool foundTargetPort = false;
    const NodePort* targetPort = nullptr;
    for (const auto& port : targetNode->inputs) {
        if (port.id == targetPortId) {
            foundTargetPort = true;
            targetPort = &port;
            break;
        }
    }

    if (!foundSourcePort || !foundTargetPort) {
        return false;
    }

    // Check if target already has a connection (unless it allows multiple)
    if (targetPort && !targetPort->allowMultiple) {
        for (const auto& conn : m_connections) {
            if (conn.targetNodeId == targetNodeId && conn.targetPortId == targetPortId) {
                return false;
            }
        }
    }

    // Check for cycles (if this is a tree/DAG)
    if (WouldCreateCycle(sourceNodeId, targetNodeId)) {
        return false;
    }

    return true;
}

void GraphEditor::SelectNode(const std::string& nodeId, bool addToSelection) {
    if (!addToSelection) {
        m_selection.clear();
    }

    m_selection.nodeIds.insert(nodeId);

    if (OnNodeSelected) {
        OnNodeSelected(nodeId);
    }

    if (OnSelectionChanged) {
        OnSelectionChanged();
    }
}

void GraphEditor::SelectConnection(const std::string& connectionId, bool addToSelection) {
    if (!addToSelection) {
        m_selection.clear();
    }

    m_selection.connectionIds.insert(connectionId);

    if (OnSelectionChanged) {
        OnSelectionChanged();
    }
}

void GraphEditor::SelectAll() {
    m_selection.clear();
    for (const auto& node : m_nodes) {
        m_selection.nodeIds.insert(node.id);
    }
    for (const auto& conn : m_connections) {
        m_selection.connectionIds.insert(conn.id);
    }

    if (OnSelectionChanged) {
        OnSelectionChanged();
    }
}

void GraphEditor::ClearSelection() {
    m_selection.clear();

    if (OnSelectionChanged) {
        OnSelectionChanged();
    }
}

void GraphEditor::DeleteSelection() {
    if (m_selection.empty()) return;

    SaveUndoState();

    // Delete connections first
    for (const auto& connId : m_selection.connectionIds) {
        auto it = m_connectionIndex.find(connId);
        if (it != m_connectionIndex.end()) {
            m_connections.erase(m_connections.begin() + it->second);
        }
    }

    // Delete nodes
    for (const auto& nodeId : m_selection.nodeIds) {
        auto it = m_nodeIndex.find(nodeId);
        if (it != m_nodeIndex.end()) {
            // Remove connected connections
            m_connections.erase(
                std::remove_if(m_connections.begin(), m_connections.end(),
                               [&nodeId](const NodeConnection& conn) {
                                   return conn.sourceNodeId == nodeId || conn.targetNodeId == nodeId;
                               }),
                m_connections.end());

            m_nodes.erase(m_nodes.begin() + it->second);
        }
    }

    // Rebuild indices
    m_nodeIndex.clear();
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        m_nodeIndex[m_nodes[i].id] = i;
    }

    m_connectionIndex.clear();
    for (size_t i = 0; i < m_connections.size(); ++i) {
        m_connectionIndex[m_connections[i].id] = i;
    }

    m_selection.clear();
    m_isDirty = true;

    if (OnGraphChanged) {
        OnGraphChanged();
    }
}

void GraphEditor::CopySelection() {
    if (m_selection.nodeIds.empty()) return;

    JSValue::Object clipboard;
    JSValue::Array nodes;
    JSValue::Array connections;

    for (const auto& nodeId : m_selection.nodeIds) {
        auto* node = GetNode(nodeId);
        if (node) {
            JSValue::Object obj;
            obj["id"] = node->id;
            obj["type"] = node->type;
            obj["title"] = node->title;
            obj["x"] = node->x;
            obj["y"] = node->y;
            obj["data"] = JSValue::FromJson(node->dataJson);
            nodes.push_back(obj);
        }
    }

    // Copy internal connections
    for (const auto& conn : m_connections) {
        if (m_selection.nodeIds.count(conn.sourceNodeId) &&
            m_selection.nodeIds.count(conn.targetNodeId)) {
            JSValue::Object obj;
            obj["sourceNodeId"] = conn.sourceNodeId;
            obj["sourcePortId"] = conn.sourcePortId;
            obj["targetNodeId"] = conn.targetNodeId;
            obj["targetPortId"] = conn.targetPortId;
            connections.push_back(obj);
        }
    }

    clipboard["nodes"] = nodes;
    clipboard["connections"] = connections;

    m_clipboardJson = JSON::Stringify(JSValue(clipboard));
}

void GraphEditor::Paste() {
    if (m_clipboardJson.empty()) return;

    SaveUndoState();

    JSValue clipboard = JSValue::FromJson(m_clipboardJson);
    if (!clipboard.IsObject()) return;

    std::unordered_map<std::string, std::string> idMap;  // Old ID -> New ID

    // Create nodes
    if (clipboard["nodes"].IsArray()) {
        for (const auto& nodeVal : clipboard["nodes"].AsArray()) {
            std::string oldId = nodeVal["id"].GetString();
            std::string type = nodeVal["type"].GetString();
            float x = static_cast<float>(nodeVal["x"].GetNumber()) + 50;
            float y = static_cast<float>(nodeVal["y"].GetNumber()) + 50;

            std::string newId = CreateNode(type, x, y);
            if (!newId.empty()) {
                idMap[oldId] = newId;

                auto* node = GetNode(newId);
                if (node) {
                    node->title = nodeVal["title"].GetString(node->title);
                    if (nodeVal["data"].IsObject()) {
                        node->dataJson = JSON::Stringify(nodeVal["data"]);
                    }
                }
            }
        }
    }

    // Create connections
    if (clipboard["connections"].IsArray()) {
        for (const auto& connVal : clipboard["connections"].AsArray()) {
            std::string oldSourceId = connVal["sourceNodeId"].GetString();
            std::string oldTargetId = connVal["targetNodeId"].GetString();

            auto sourceIt = idMap.find(oldSourceId);
            auto targetIt = idMap.find(oldTargetId);

            if (sourceIt != idMap.end() && targetIt != idMap.end()) {
                CreateConnection(
                    sourceIt->second, connVal["sourcePortId"].GetString(),
                    targetIt->second, connVal["targetPortId"].GetString()
                );
            }
        }
    }

    // Select pasted nodes
    m_selection.clear();
    for (const auto& [oldId, newId] : idMap) {
        m_selection.nodeIds.insert(newId);
    }
}

void GraphEditor::SetPan(float x, float y) {
    m_viewport.panX = x;
    m_viewport.panY = y;
}

void GraphEditor::SetZoom(float zoom) {
    m_viewport.zoom = std::clamp(zoom, m_viewport.minZoom, m_viewport.maxZoom);
}

void GraphEditor::ZoomToFit() {
    if (m_nodes.empty()) return;

    float minX = FLT_MAX, minY = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX;

    for (const auto& node : m_nodes) {
        minX = std::min(minX, node.x);
        minY = std::min(minY, node.y);
        maxX = std::max(maxX, node.x + node.width);
        maxY = std::max(maxY, node.y + node.height);
    }

    float graphWidth = maxX - minX;
    float graphHeight = maxY - minY;

    // Calculate zoom to fit
    float viewWidth = static_cast<float>(m_webView->GetWidth());
    float viewHeight = static_cast<float>(m_webView->GetHeight());

    float zoomX = viewWidth / (graphWidth + 100);
    float zoomY = viewHeight / (graphHeight + 100);

    m_viewport.zoom = std::min(zoomX, zoomY);
    m_viewport.zoom = std::clamp(m_viewport.zoom, m_viewport.minZoom, m_viewport.maxZoom);

    m_viewport.panX = -(minX + graphWidth / 2) * m_viewport.zoom + viewWidth / 2;
    m_viewport.panY = -(minY + graphHeight / 2) * m_viewport.zoom + viewHeight / 2;
}

void GraphEditor::ZoomToSelection() {
    if (m_selection.nodeIds.empty()) return;

    float minX = FLT_MAX, minY = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX;

    for (const auto& nodeId : m_selection.nodeIds) {
        auto* node = GetNode(nodeId);
        if (node) {
            minX = std::min(minX, node->x);
            minY = std::min(minY, node->y);
            maxX = std::max(maxX, node->x + node->width);
            maxY = std::max(maxY, node->y + node->height);
        }
    }

    CenterOnNode(*m_selection.nodeIds.begin());
}

void GraphEditor::CenterOnNode(const std::string& nodeId) {
    auto* node = GetNode(nodeId);
    if (!node) return;

    float viewWidth = static_cast<float>(m_webView->GetWidth());
    float viewHeight = static_cast<float>(m_webView->GetHeight());

    float nodeCenterX = node->x + node->width / 2;
    float nodeCenterY = node->y + node->height / 2;

    m_viewport.panX = -nodeCenterX * m_viewport.zoom + viewWidth / 2;
    m_viewport.panY = -nodeCenterY * m_viewport.zoom + viewHeight / 2;
}

void GraphEditor::ScreenToGraph(float screenX, float screenY, float& graphX, float& graphY) const {
    graphX = (screenX - m_viewport.panX) / m_viewport.zoom;
    graphY = (screenY - m_viewport.panY) / m_viewport.zoom;
}

void GraphEditor::GraphToScreen(float graphX, float graphY, float& screenX, float& screenY) const {
    screenX = graphX * m_viewport.zoom + m_viewport.panX;
    screenY = graphY * m_viewport.zoom + m_viewport.panY;
}

void GraphEditor::AutoArrange(LayoutAlgorithm algorithm) {
    SaveUndoState();

    switch (algorithm) {
        case LayoutAlgorithm::Hierarchical:
            LayoutHierarchical();
            break;
        case LayoutAlgorithm::ForceDirected:
            LayoutForceDirected();
            break;
        case LayoutAlgorithm::Grid:
            LayoutGrid();
            break;
        case LayoutAlgorithm::Horizontal:
        case LayoutAlgorithm::Vertical:
            LayoutHierarchical();  // Use hierarchical for directional layouts
            break;
    }

    m_isDirty = true;

    if (m_bridge) {
        m_bridge->EmitEvent("layoutChanged", {});
    }
}

void GraphEditor::AlignSelection(Alignment alignment) {
    if (m_selection.nodeIds.size() < 2) return;

    SaveUndoState();

    // Find bounds
    float minX = FLT_MAX, minY = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX;
    float totalX = 0, totalY = 0;

    for (const auto& nodeId : m_selection.nodeIds) {
        auto* node = GetNode(nodeId);
        if (node) {
            minX = std::min(minX, node->x);
            minY = std::min(minY, node->y);
            maxX = std::max(maxX, node->x + node->width);
            maxY = std::max(maxY, node->y + node->height);
            totalX += node->x + node->width / 2;
            totalY += node->y + node->height / 2;
        }
    }

    float avgX = totalX / m_selection.nodeIds.size();
    float avgY = totalY / m_selection.nodeIds.size();

    // Apply alignment
    for (const auto& nodeId : m_selection.nodeIds) {
        auto* node = GetNode(nodeId);
        if (!node) continue;

        switch (alignment) {
            case Alignment::Left:
                node->x = minX;
                break;
            case Alignment::Right:
                node->x = maxX - node->width;
                break;
            case Alignment::Top:
                node->y = minY;
                break;
            case Alignment::Bottom:
                node->y = maxY - node->height;
                break;
            case Alignment::CenterH:
                node->x = avgX - node->width / 2;
                break;
            case Alignment::CenterV:
                node->y = avgY - node->height / 2;
                break;
        }
    }

    m_isDirty = true;
}

void GraphEditor::DistributeSelection(bool horizontal) {
    if (m_selection.nodeIds.size() < 3) return;

    SaveUndoState();

    std::vector<GraphNode*> nodes;
    for (const auto& nodeId : m_selection.nodeIds) {
        auto* node = GetNode(nodeId);
        if (node) nodes.push_back(node);
    }

    // Sort by position
    if (horizontal) {
        std::sort(nodes.begin(), nodes.end(), [](const GraphNode* a, const GraphNode* b) {
            return a->x < b->x;
        });
    } else {
        std::sort(nodes.begin(), nodes.end(), [](const GraphNode* a, const GraphNode* b) {
            return a->y < b->y;
        });
    }

    // Calculate spacing
    float start, end;
    if (horizontal) {
        start = nodes.front()->x;
        end = nodes.back()->x;
    } else {
        start = nodes.front()->y;
        end = nodes.back()->y;
    }

    float spacing = (end - start) / (nodes.size() - 1);

    // Apply distribution
    for (size_t i = 1; i < nodes.size() - 1; ++i) {
        if (horizontal) {
            nodes[i]->x = start + spacing * i;
        } else {
            nodes[i]->y = start + spacing * i;
        }
    }

    m_isDirty = true;
}

void GraphEditor::Undo() {
    if (m_undoIndex == 0) return;

    --m_undoIndex;
    RestoreState(m_undoHistory[m_undoIndex]);
}

void GraphEditor::Redo() {
    if (m_undoIndex >= m_undoHistory.size()) return;

    ++m_undoIndex;
    if (m_undoIndex < m_undoHistory.size()) {
        RestoreState(m_undoHistory[m_undoIndex]);
    }
}

std::vector<std::string> GraphEditor::Validate() const {
    std::vector<std::string> errors;

    // Check for orphan nodes (no connections)
    for (const auto& node : m_nodes) {
        bool hasConnection = false;
        for (const auto& conn : m_connections) {
            if (conn.sourceNodeId == node.id || conn.targetNodeId == node.id) {
                hasConnection = true;
                break;
            }
        }

        if (!hasConnection && m_nodes.size() > 1) {
            errors.push_back("Node '" + node.title + "' has no connections");
        }
    }

    // Check for cycles
    auto cycles = FindCycles();
    for (const auto& cycle : cycles) {
        std::string cycleStr = "Cycle detected: ";
        for (size_t i = 0; i < cycle.size(); ++i) {
            if (i > 0) cycleStr += " -> ";
            auto* node = GetNode(cycle[i]);
            cycleStr += node ? node->title : cycle[i];
        }
        errors.push_back(cycleStr);
    }

    return errors;
}

bool GraphEditor::IsValid() const {
    return Validate().empty();
}

std::vector<std::vector<std::string>> GraphEditor::FindCycles() const {
    std::vector<std::vector<std::string>> cycles;

    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recursionStack;
    std::vector<std::string> path;

    std::function<void(const std::string&)> dfs = [&](const std::string& nodeId) {
        visited.insert(nodeId);
        recursionStack.insert(nodeId);
        path.push_back(nodeId);

        for (const auto& conn : m_connections) {
            if (conn.sourceNodeId == nodeId) {
                if (recursionStack.count(conn.targetNodeId)) {
                    // Found cycle
                    std::vector<std::string> cycle;
                    auto it = std::find(path.begin(), path.end(), conn.targetNodeId);
                    if (it != path.end()) {
                        cycle.assign(it, path.end());
                        cycle.push_back(conn.targetNodeId);
                        cycles.push_back(cycle);
                    }
                } else if (!visited.count(conn.targetNodeId)) {
                    dfs(conn.targetNodeId);
                }
            }
        }

        path.pop_back();
        recursionStack.erase(nodeId);
    };

    for (const auto& node : m_nodes) {
        if (!visited.count(node.id)) {
            dfs(node.id);
        }
    }

    return cycles;
}

void GraphEditor::SaveUndoState() {
    // Remove any redo states
    if (m_undoIndex < m_undoHistory.size()) {
        m_undoHistory.resize(m_undoIndex);
    }

    // Save current state
    m_undoHistory.push_back(SaveToJson());
    ++m_undoIndex;

    // Limit history size
    if (m_undoHistory.size() > MAX_UNDO_HISTORY) {
        m_undoHistory.erase(m_undoHistory.begin());
        --m_undoIndex;
    }
}

void GraphEditor::RestoreState(const std::string& stateJson) {
    LoadFromJson(stateJson);

    if (m_bridge) {
        m_bridge->EmitEvent("graphChanged", {});
    }
}

std::string GraphEditor::GenerateNodeId() {
    return "node_" + std::to_string(m_nextNodeId++);
}

std::string GraphEditor::GenerateConnectionId() {
    return "conn_" + std::to_string(m_nextConnectionId++);
}

bool GraphEditor::ValidateConnection(const NodeConnection& conn) const {
    return GetNode(conn.sourceNodeId) != nullptr &&
           GetNode(conn.targetNodeId) != nullptr;
}

bool GraphEditor::WouldCreateCycle(const std::string& sourceNodeId,
                                    const std::string& targetNodeId) const {
    // BFS from target to see if we can reach source
    std::unordered_set<std::string> visited;
    std::queue<std::string> queue;
    queue.push(targetNodeId);

    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();

        if (current == sourceNodeId) {
            return true;  // Would create cycle
        }

        if (visited.count(current)) continue;
        visited.insert(current);

        for (const auto& conn : m_connections) {
            if (conn.sourceNodeId == current && !visited.count(conn.targetNodeId)) {
                queue.push(conn.targetNodeId);
            }
        }
    }

    return false;
}

void GraphEditor::LayoutHierarchical() {
    // Find root nodes (no incoming connections)
    std::vector<std::string> roots;
    std::unordered_set<std::string> hasIncoming;

    for (const auto& conn : m_connections) {
        hasIncoming.insert(conn.targetNodeId);
    }

    for (const auto& node : m_nodes) {
        if (!hasIncoming.count(node.id)) {
            roots.push_back(node.id);
        }
    }

    if (roots.empty() && !m_nodes.empty()) {
        roots.push_back(m_nodes[0].id);
    }

    // Assign levels using BFS
    std::unordered_map<std::string, int> levels;
    std::queue<std::string> queue;

    for (const auto& rootId : roots) {
        queue.push(rootId);
        levels[rootId] = 0;
    }

    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();

        for (const auto& conn : m_connections) {
            if (conn.sourceNodeId == current) {
                if (levels.find(conn.targetNodeId) == levels.end()) {
                    levels[conn.targetNodeId] = levels[current] + 1;
                    queue.push(conn.targetNodeId);
                }
            }
        }
    }

    // Group by level
    std::map<int, std::vector<std::string>> levelGroups;
    for (const auto& [nodeId, level] : levels) {
        levelGroups[level].push_back(nodeId);
    }

    // Position nodes
    float levelSpacing = 150.0f;
    float nodeSpacing = 200.0f;

    for (const auto& [level, nodeIds] : levelGroups) {
        float startX = -(nodeIds.size() - 1) * nodeSpacing / 2;

        for (size_t i = 0; i < nodeIds.size(); ++i) {
            auto* node = GetNode(nodeIds[i]);
            if (node) {
                node->x = startX + i * nodeSpacing;
                node->y = level * levelSpacing;
            }
        }
    }
}

void GraphEditor::LayoutForceDirected() {
    const int iterations = 50;
    const float repulsion = 5000.0f;
    const float attraction = 0.1f;
    const float damping = 0.9f;

    std::unordered_map<std::string, float> velocityX, velocityY;

    for (int iter = 0; iter < iterations; ++iter) {
        // Repulsion between all nodes
        for (size_t i = 0; i < m_nodes.size(); ++i) {
            for (size_t j = i + 1; j < m_nodes.size(); ++j) {
                float dx = m_nodes[j].x - m_nodes[i].x;
                float dy = m_nodes[j].y - m_nodes[i].y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < 1.0f) dist = 1.0f;

                float force = repulsion / (dist * dist);
                float fx = force * dx / dist;
                float fy = force * dy / dist;

                velocityX[m_nodes[i].id] -= fx;
                velocityY[m_nodes[i].id] -= fy;
                velocityX[m_nodes[j].id] += fx;
                velocityY[m_nodes[j].id] += fy;
            }
        }

        // Attraction along connections
        for (const auto& conn : m_connections) {
            auto* source = GetNode(conn.sourceNodeId);
            auto* target = GetNode(conn.targetNodeId);
            if (!source || !target) continue;

            float dx = target->x - source->x;
            float dy = target->y - source->y;

            velocityX[source->id] += dx * attraction;
            velocityY[source->id] += dy * attraction;
            velocityX[target->id] -= dx * attraction;
            velocityY[target->id] -= dy * attraction;
        }

        // Apply velocities with damping
        for (auto& node : m_nodes) {
            node.x += velocityX[node.id];
            node.y += velocityY[node.id];
            velocityX[node.id] *= damping;
            velocityY[node.id] *= damping;
        }
    }
}

void GraphEditor::LayoutGrid() {
    int cols = static_cast<int>(std::ceil(std::sqrt(m_nodes.size())));
    float spacing = 250.0f;

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        int row = static_cast<int>(i) / cols;
        int col = static_cast<int>(i) % cols;

        m_nodes[i].x = col * spacing;
        m_nodes[i].y = row * spacing;
    }
}

} // namespace WebEditor
} // namespace Vehement
