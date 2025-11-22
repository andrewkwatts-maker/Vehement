#include "BlendTreeEditor.hpp"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <queue>

namespace Vehement {

// ============================================================================
// BlendTreeEditor Implementation
// ============================================================================

BlendTreeEditor::BlendTreeEditor() = default;
BlendTreeEditor::~BlendTreeEditor() = default;

void BlendTreeEditor::Initialize(const Config& config) {
    m_config = config;
    m_initialized = true;
    m_nodeIdCounter = 0;
    m_nodes.clear();
    m_connections.clear();
    m_parameters.clear();
    m_undoStack.clear();
    m_redoStack.clear();
}

bool BlendTreeEditor::LoadBlendTree(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    try {
        json data;
        file >> data;
        file.close();

        if (!ImportFromJson(data)) {
            return false;
        }

        m_filePath = filepath;
        m_dirty = false;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool BlendTreeEditor::LoadBlendTree(BlendTree* blendTree) {
    if (!blendTree) {
        return false;
    }

    m_blendTree = blendTree;
    m_nodes.clear();
    m_connections.clear();
    m_parameters.clear();

    // Build visual representation from blend tree
    // This would parse the blend tree structure and create nodes
    m_dirty = false;
    return true;
}

bool BlendTreeEditor::SaveBlendTree(const std::string& filepath) {
    json data = ExportToJson();

    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    file << data.dump(2);
    file.close();

    m_filePath = filepath;
    m_dirty = false;
    return true;
}

bool BlendTreeEditor::SaveBlendTree() {
    if (m_filePath.empty()) {
        return false;
    }
    return SaveBlendTree(m_filePath);
}

void BlendTreeEditor::NewBlendTree(const std::string& type) {
    m_nodes.clear();
    m_connections.clear();
    m_parameters.clear();
    m_rootNodeId.clear();
    m_filePath.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_nodeIdCounter = 0;

    // Create root node based on type
    BlendTreeNode* root = AddNode(type, glm::vec2(400.0f, 100.0f));
    if (root) {
        m_rootNodeId = root->id;
        root->name = "Root";

        // Add default parameters based on type
        if (type == "simple_1d") {
            AddParameter("blend", 0.0f, 0.0f, 1.0f);
            root->parameter = "blend";
        } else if (type == "simple_2d" || type == "freeform_2d") {
            AddParameter("blendX", 0.0f, -1.0f, 1.0f);
            AddParameter("blendY", 0.0f, -1.0f, 1.0f);
            root->parameterX = "blendX";
            root->parameterY = "blendY";
        }
    }

    m_dirty = false;
}

json BlendTreeEditor::ExportToJson() const {
    json data;

    // Determine blend tree type from root node
    std::string treeType = "simple_1d";
    if (!m_rootNodeId.empty()) {
        for (const auto& node : m_nodes) {
            if (node.id == m_rootNodeId) {
                if (node.type == "1d") treeType = "simple_1d";
                else if (node.type == "2d") treeType = "simple_2d";
                else if (node.type == "freeform_2d") treeType = "freeform_2d";
                else if (node.type == "additive") treeType = "additive";
                else if (node.type == "direct") treeType = "direct";
                break;
            }
        }
    }

    data["type"] = treeType;

    // Export parameters
    if (treeType == "simple_1d") {
        for (const auto& node : m_nodes) {
            if (node.id == m_rootNodeId) {
                data["parameter"] = node.parameter;
                break;
            }
        }
    } else if (treeType == "simple_2d" || treeType == "freeform_2d") {
        for (const auto& node : m_nodes) {
            if (node.id == m_rootNodeId) {
                data["parameterX"] = node.parameterX;
                data["parameterY"] = node.parameterY;
                break;
            }
        }
    }

    // Export children
    json children = json::array();
    for (const auto& conn : m_connections) {
        if (conn.parentId == m_rootNodeId) {
            for (const auto& node : m_nodes) {
                if (node.id == conn.childId) {
                    json child;
                    child["clip"] = node.clipPath;

                    if (treeType == "simple_1d") {
                        child["threshold"] = node.threshold;
                    } else {
                        child["position"]["x"] = node.position2D.x;
                        child["position"]["y"] = node.position2D.y;
                    }

                    children.push_back(child);
                    break;
                }
            }
        }
    }
    data["children"] = children;

    // Export editor metadata
    json editorData;
    json nodePositions = json::object();
    for (const auto& node : m_nodes) {
        nodePositions[node.id]["x"] = node.position.x;
        nodePositions[node.id]["y"] = node.position.y;
    }
    editorData["nodePositions"] = nodePositions;
    editorData["viewOffset"]["x"] = m_viewOffset.x;
    editorData["viewOffset"]["y"] = m_viewOffset.y;
    editorData["zoom"] = m_zoom;
    data["_editor"] = editorData;

    return data;
}

bool BlendTreeEditor::ImportFromJson(const json& data) {
    try {
        m_nodes.clear();
        m_connections.clear();
        m_parameters.clear();
        m_nodeIdCounter = 0;

        std::string type = data.value("type", "simple_1d");

        // Create root node
        BlendTreeNode* root = AddNode(type == "simple_1d" ? "1d" : "2d",
                                       glm::vec2(400.0f, 100.0f));
        if (!root) return false;

        m_rootNodeId = root->id;
        root->name = "Root";

        // Set parameters
        if (type == "simple_1d") {
            root->parameter = data.value("parameter", "blend");
            AddParameter(root->parameter, 0.0f, 0.0f, 1.0f);
        } else {
            root->parameterX = data.value("parameterX", "blendX");
            root->parameterY = data.value("parameterY", "blendY");
            AddParameter(root->parameterX, 0.0f, -1.0f, 1.0f);
            AddParameter(root->parameterY, 0.0f, -1.0f, 1.0f);
        }

        // Create child nodes
        if (data.contains("children")) {
            float yOffset = 250.0f;
            float xSpacing = 200.0f;
            float startX = 400.0f - (data["children"].size() - 1) * xSpacing / 2.0f;

            for (size_t i = 0; i < data["children"].size(); ++i) {
                const auto& childData = data["children"][i];

                BlendTreeNode* child = AddClipNode(
                    childData.value("clip", ""),
                    glm::vec2(startX + i * xSpacing, yOffset)
                );

                if (child) {
                    if (type == "simple_1d") {
                        child->threshold = childData.value("threshold", 0.0f);
                    } else if (childData.contains("position")) {
                        child->position2D.x = childData["position"].value("x", 0.0f);
                        child->position2D.y = childData["position"].value("y", 0.0f);
                    }

                    ConnectNodes(m_rootNodeId, child->id);
                }
            }
        }

        // Load editor metadata if present
        if (data.contains("_editor")) {
            const auto& editorData = data["_editor"];

            if (editorData.contains("nodePositions")) {
                for (auto& node : m_nodes) {
                    if (editorData["nodePositions"].contains(node.id)) {
                        node.position.x = editorData["nodePositions"][node.id].value("x", node.position.x);
                        node.position.y = editorData["nodePositions"][node.id].value("y", node.position.y);
                    }
                }
            }

            if (editorData.contains("viewOffset")) {
                m_viewOffset.x = editorData["viewOffset"].value("x", 0.0f);
                m_viewOffset.y = editorData["viewOffset"].value("y", 0.0f);
            }

            if (editorData.contains("zoom")) {
                m_zoom = editorData.value("zoom", 1.0f);
            }
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// -------------------------------------------------------------------------
// Node Operations
// -------------------------------------------------------------------------

BlendTreeNode* BlendTreeEditor::AddNode(const std::string& type, const glm::vec2& position) {
    json before = ExportToJson();

    BlendTreeNode node;
    node.id = GenerateNodeId();
    node.type = type;
    node.position = m_config.snapToGrid ? SnapToGrid(position) : position;

    // Set default properties based on type
    if (type == "1d" || type == "simple_1d") {
        node.type = "1d";
        node.name = "1D Blend";
        node.color = 0x4488FFFF;
        node.size = glm::vec2(200.0f, 80.0f);
    } else if (type == "2d" || type == "simple_2d") {
        node.type = "2d";
        node.name = "2D Blend";
        node.color = 0x44FF88FF;
        node.size = glm::vec2(200.0f, 100.0f);
    } else if (type == "freeform_2d") {
        node.type = "freeform_2d";
        node.name = "Freeform 2D";
        node.color = 0x88FF44FF;
        node.size = glm::vec2(200.0f, 100.0f);
    } else if (type == "additive") {
        node.type = "additive";
        node.name = "Additive";
        node.color = 0xFF8844FF;
        node.size = glm::vec2(180.0f, 80.0f);
    } else if (type == "clip") {
        node.type = "clip";
        node.name = "Clip";
        node.color = 0xAAAAAAFF;
        node.size = glm::vec2(160.0f, 70.0f);
    }

    m_nodes.push_back(node);
    m_dirty = true;

    json after = ExportToJson();
    RecordAction(BlendTreeEditorAction::Type::AddNode, node.id, before, after);

    if (OnModified) {
        OnModified();
    }

    return &m_nodes.back();
}

BlendTreeNode* BlendTreeEditor::AddClipNode(const std::string& clipPath, const glm::vec2& position) {
    BlendTreeNode* node = AddNode("clip", position);
    if (node) {
        node->clipPath = clipPath;

        // Extract name from path
        size_t lastSlash = clipPath.find_last_of('/');
        if (lastSlash != std::string::npos) {
            node->name = clipPath.substr(lastSlash + 1);
        } else {
            node->name = clipPath;
        }
    }
    return node;
}

bool BlendTreeEditor::RemoveNode(const std::string& id) {
    // Cannot remove root node
    if (id == m_rootNodeId) {
        return false;
    }

    auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
                           [&id](const BlendTreeNode& n) { return n.id == id; });

    if (it == m_nodes.end()) {
        return false;
    }

    json before = ExportToJson();

    // Remove all connections involving this node
    m_connections.erase(
        std::remove_if(m_connections.begin(), m_connections.end(),
                       [&id](const BlendTreeConnection& c) {
                           return c.parentId == id || c.childId == id;
                       }),
        m_connections.end()
    );

    m_nodes.erase(it);
    m_dirty = true;

    json after = ExportToJson();
    RecordAction(BlendTreeEditorAction::Type::RemoveNode, id, before, after);

    if (m_selectedNodeId == id) {
        ClearSelection();
    }

    if (OnModified) {
        OnModified();
    }

    return true;
}

BlendTreeNode* BlendTreeEditor::GetNode(const std::string& id) {
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
                           [&id](const BlendTreeNode& n) { return n.id == id; });
    return it != m_nodes.end() ? &(*it) : nullptr;
}

bool BlendTreeEditor::ConnectNodes(const std::string& parentId, const std::string& childId) {
    // Verify both nodes exist
    if (!GetNode(parentId) || !GetNode(childId)) {
        return false;
    }

    // Check if connection already exists
    auto it = std::find_if(m_connections.begin(), m_connections.end(),
                           [&](const BlendTreeConnection& c) {
                               return c.parentId == parentId && c.childId == childId;
                           });

    if (it != m_connections.end()) {
        return false;  // Already connected
    }

    json before = ExportToJson();

    BlendTreeConnection conn;
    conn.parentId = parentId;
    conn.childId = childId;
    m_connections.push_back(conn);

    // Update parent's child list
    BlendTreeNode* parent = GetNode(parentId);
    if (parent) {
        parent->childIds.push_back(childId);
    }

    m_dirty = true;

    json after = ExportToJson();
    RecordAction(BlendTreeEditorAction::Type::AddConnection, parentId + "->" + childId, before, after);

    if (OnModified) {
        OnModified();
    }

    return true;
}

bool BlendTreeEditor::DisconnectNodes(const std::string& parentId, const std::string& childId) {
    auto it = std::find_if(m_connections.begin(), m_connections.end(),
                           [&](const BlendTreeConnection& c) {
                               return c.parentId == parentId && c.childId == childId;
                           });

    if (it == m_connections.end()) {
        return false;
    }

    json before = ExportToJson();

    m_connections.erase(it);

    // Update parent's child list
    BlendTreeNode* parent = GetNode(parentId);
    if (parent) {
        auto childIt = std::find(parent->childIds.begin(), parent->childIds.end(), childId);
        if (childIt != parent->childIds.end()) {
            parent->childIds.erase(childIt);
        }
    }

    m_dirty = true;

    json after = ExportToJson();
    RecordAction(BlendTreeEditorAction::Type::RemoveConnection, parentId + "->" + childId, before, after);

    if (OnModified) {
        OnModified();
    }

    return true;
}

void BlendTreeEditor::SetRootNode(const std::string& id) {
    if (GetNode(id)) {
        m_rootNodeId = id;
        m_dirty = true;

        if (OnModified) {
            OnModified();
        }
    }
}

// -------------------------------------------------------------------------
// Parameter Operations
// -------------------------------------------------------------------------

void BlendTreeEditor::AddParameter(const std::string& name, float defaultValue,
                                    float minValue, float maxValue) {
    // Check if parameter already exists
    auto it = std::find_if(m_parameters.begin(), m_parameters.end(),
                           [&name](const ParameterSlider& p) { return p.name == name; });

    if (it != m_parameters.end()) {
        return;  // Already exists
    }

    ParameterSlider param;
    param.name = name;
    param.value = defaultValue;
    param.minValue = minValue;
    param.maxValue = maxValue;
    m_parameters.push_back(param);
}

bool BlendTreeEditor::RemoveParameter(const std::string& name) {
    auto it = std::find_if(m_parameters.begin(), m_parameters.end(),
                           [&name](const ParameterSlider& p) { return p.name == name; });

    if (it == m_parameters.end()) {
        return false;
    }

    m_parameters.erase(it);
    return true;
}

void BlendTreeEditor::SetParameterValue(const std::string& name, float value) {
    auto it = std::find_if(m_parameters.begin(), m_parameters.end(),
                           [&name](const ParameterSlider& p) { return p.name == name; });

    if (it != m_parameters.end()) {
        it->value = std::clamp(value, it->minValue, it->maxValue);

        CalculateBlendWeights();

        if (OnParameterChanged) {
            OnParameterChanged(name, it->value);
        }
    }
}

float BlendTreeEditor::GetParameterValue(const std::string& name) const {
    auto it = std::find_if(m_parameters.begin(), m_parameters.end(),
                           [&name](const ParameterSlider& p) { return p.name == name; });

    return it != m_parameters.end() ? it->value : 0.0f;
}

// -------------------------------------------------------------------------
// Blend Space
// -------------------------------------------------------------------------

std::vector<BlendSpacePoint> BlendTreeEditor::GetBlendSpacePoints() const {
    std::vector<BlendSpacePoint> points;

    for (const auto& conn : m_connections) {
        if (conn.parentId == m_rootNodeId) {
            for (const auto& node : m_nodes) {
                if (node.id == conn.childId && node.type == "clip") {
                    BlendSpacePoint point;
                    point.clipName = node.name;
                    point.position = node.position2D;

                    // Find weight from current weights
                    for (const auto& [clip, weight] : m_currentWeights) {
                        if (clip == node.clipPath) {
                            point.weight = weight;
                            break;
                        }
                    }

                    point.selected = (node.id == m_selectedNodeId);
                    points.push_back(point);
                }
            }
        }
    }

    return points;
}

glm::vec2 BlendTreeEditor::GetCurrentBlendPosition() const {
    float x = 0.0f, y = 0.0f;

    for (const auto& param : m_parameters) {
        if (param.name.find("X") != std::string::npos ||
            param.name.find("x") != std::string::npos) {
            x = param.value;
        } else if (param.name.find("Y") != std::string::npos ||
                   param.name.find("y") != std::string::npos) {
            y = param.value;
        }
    }

    return glm::vec2(x, y);
}

std::vector<std::pair<std::string, float>> BlendTreeEditor::GetBlendWeights() const {
    return m_currentWeights;
}

void BlendTreeEditor::SetBlendPosition(const glm::vec2& position) {
    for (auto& param : m_parameters) {
        if (param.name.find("X") != std::string::npos ||
            param.name.find("x") != std::string::npos) {
            param.value = std::clamp(position.x, param.minValue, param.maxValue);
        } else if (param.name.find("Y") != std::string::npos ||
                   param.name.find("y") != std::string::npos) {
            param.value = std::clamp(position.y, param.minValue, param.maxValue);
        }
    }

    CalculateBlendWeights();
}

// -------------------------------------------------------------------------
// Selection
// -------------------------------------------------------------------------

void BlendTreeEditor::SelectNode(const std::string& id) {
    // Deselect all
    for (auto& node : m_nodes) {
        node.selected = (node.id == id);
    }
    for (auto& conn : m_connections) {
        conn.selected = false;
    }

    m_selectedNodeId = id;
    m_selectedConnectionParent.clear();
    m_selectedConnectionChild.clear();

    if (OnSelectionChanged) {
        OnSelectionChanged(id);
    }

    if (OnNodeSelected) {
        OnNodeSelected(GetNode(id));
    }
}

void BlendTreeEditor::SelectConnection(const std::string& parentId, const std::string& childId) {
    // Deselect all
    for (auto& node : m_nodes) {
        node.selected = false;
    }
    for (auto& conn : m_connections) {
        conn.selected = (conn.parentId == parentId && conn.childId == childId);
    }

    m_selectedNodeId.clear();
    m_selectedConnectionParent = parentId;
    m_selectedConnectionChild = childId;

    if (OnSelectionChanged) {
        OnSelectionChanged(parentId + "->" + childId);
    }
}

void BlendTreeEditor::ClearSelection() {
    for (auto& node : m_nodes) {
        node.selected = false;
    }
    for (auto& conn : m_connections) {
        conn.selected = false;
    }

    m_selectedNodeId.clear();
    m_selectedConnectionParent.clear();
    m_selectedConnectionChild.clear();

    if (OnSelectionChanged) {
        OnSelectionChanged("");
    }
}

// -------------------------------------------------------------------------
// View Control
// -------------------------------------------------------------------------

void BlendTreeEditor::SetViewOffset(const glm::vec2& offset) {
    m_viewOffset = offset;
}

void BlendTreeEditor::SetZoom(float zoom) {
    m_zoom = std::clamp(zoom, m_config.zoomMin, m_config.zoomMax);
}

void BlendTreeEditor::ZoomToFit() {
    if (m_nodes.empty()) {
        return;
    }

    glm::vec2 minPos(std::numeric_limits<float>::max());
    glm::vec2 maxPos(std::numeric_limits<float>::lowest());

    for (const auto& node : m_nodes) {
        minPos.x = std::min(minPos.x, node.position.x);
        minPos.y = std::min(minPos.y, node.position.y);
        maxPos.x = std::max(maxPos.x, node.position.x + node.size.x);
        maxPos.y = std::max(maxPos.y, node.position.y + node.size.y);
    }

    glm::vec2 center = (minPos + maxPos) * 0.5f;
    glm::vec2 size = maxPos - minPos;

    // Calculate zoom to fit
    float zoomX = m_config.canvasSize.x / (size.x + 100.0f);
    float zoomY = m_config.canvasSize.y / (size.y + 100.0f);
    m_zoom = std::clamp(std::min(zoomX, zoomY), m_config.zoomMin, m_config.zoomMax);

    m_viewOffset = center - m_config.canvasSize * 0.5f / m_zoom;
}

void BlendTreeEditor::CenterOnNode(const std::string& id) {
    BlendTreeNode* node = GetNode(id);
    if (node) {
        glm::vec2 center = node->position + node->size * 0.5f;
        m_viewOffset = center - m_config.canvasSize * 0.5f / m_zoom;
    }
}

// -------------------------------------------------------------------------
// Input Handling
// -------------------------------------------------------------------------

void BlendTreeEditor::OnMouseDown(const glm::vec2& position, int button) {
    glm::vec2 worldPos = ScreenToWorld(position);

    if (button == 0) {  // Left click
        BlendTreeNode* node = FindNodeAt(worldPos);
        if (node) {
            SelectNode(node->id);
            m_dragging = true;
            m_dragStart = worldPos;
            m_dragOffset = node->position - worldPos;
        } else {
            BlendTreeConnection* conn = FindConnectionAt(worldPos);
            if (conn) {
                SelectConnection(conn->parentId, conn->childId);
            } else {
                ClearSelection();
            }
        }
    } else if (button == 1) {  // Right click - start panning
        m_panning = true;
        m_dragStart = position;
    } else if (button == 2) {  // Middle click - start connection creation
        BlendTreeNode* node = FindNodeAt(worldPos);
        if (node && node->type != "clip") {
            m_creatingConnection = true;
            m_connectionStartNode = node->id;
        }
    }
}

void BlendTreeEditor::OnMouseUp(const glm::vec2& position, int button) {
    glm::vec2 worldPos = ScreenToWorld(position);

    if (button == 0 && m_dragging) {
        m_dragging = false;
    } else if (button == 1 && m_panning) {
        m_panning = false;
    } else if (button == 2 && m_creatingConnection) {
        BlendTreeNode* targetNode = FindNodeAt(worldPos);
        if (targetNode && targetNode->id != m_connectionStartNode) {
            ConnectNodes(m_connectionStartNode, targetNode->id);
        }
        m_creatingConnection = false;
        m_connectionStartNode.clear();
    }
}

void BlendTreeEditor::OnMouseMove(const glm::vec2& position) {
    glm::vec2 worldPos = ScreenToWorld(position);

    if (m_dragging && !m_selectedNodeId.empty()) {
        json before = ExportToJson();

        BlendTreeNode* node = GetNode(m_selectedNodeId);
        if (node) {
            glm::vec2 newPos = worldPos + m_dragOffset;
            node->position = m_config.snapToGrid ? SnapToGrid(newPos) : newPos;
            m_dirty = true;
        }

        // Record action will be done on mouse up
    } else if (m_panning) {
        glm::vec2 delta = (position - m_dragStart) / m_zoom;
        m_viewOffset -= delta;
        m_dragStart = position;
    }
}

void BlendTreeEditor::OnKeyDown(int key) {
    // Delete selected node
    if (key == 127 || key == 8) {  // Delete or Backspace
        if (!m_selectedNodeId.empty()) {
            RemoveNode(m_selectedNodeId);
        } else if (!m_selectedConnectionParent.empty()) {
            DisconnectNodes(m_selectedConnectionParent, m_selectedConnectionChild);
        }
    }
    // Undo
    else if (key == 26) {  // Ctrl+Z
        Undo();
    }
    // Redo
    else if (key == 25) {  // Ctrl+Y
        Redo();
    }
}

void BlendTreeEditor::OnScroll(float delta) {
    float newZoom = m_zoom * (1.0f + delta * 0.1f);
    SetZoom(newZoom);
}

// -------------------------------------------------------------------------
// Undo/Redo
// -------------------------------------------------------------------------

void BlendTreeEditor::Undo() {
    if (m_undoStack.empty()) {
        return;
    }

    BlendTreeEditorAction action = m_undoStack.back();
    m_undoStack.pop_back();

    ImportFromJson(action.beforeData);

    m_redoStack.push_back(action);

    if (OnModified) {
        OnModified();
    }
}

void BlendTreeEditor::Redo() {
    if (m_redoStack.empty()) {
        return;
    }

    BlendTreeEditorAction action = m_redoStack.back();
    m_redoStack.pop_back();

    ImportFromJson(action.afterData);

    m_undoStack.push_back(action);

    if (OnModified) {
        OnModified();
    }
}

// -------------------------------------------------------------------------
// Layout
// -------------------------------------------------------------------------

void BlendTreeEditor::AutoLayout() {
    if (m_rootNodeId.empty()) {
        return;
    }

    float startX = 400.0f;
    float startY = 100.0f;
    float levelHeight = 150.0f;

    LayoutSubtree(m_rootNodeId, startX, startY, levelHeight);

    m_dirty = true;

    if (OnModified) {
        OnModified();
    }
}

void BlendTreeEditor::CollapseNode(const std::string& id) {
    BlendTreeNode* node = GetNode(id);
    if (node) {
        node->expanded = false;
    }
}

void BlendTreeEditor::ExpandNode(const std::string& id) {
    BlendTreeNode* node = GetNode(id);
    if (node) {
        node->expanded = true;
    }
}

// -------------------------------------------------------------------------
// Preview
// -------------------------------------------------------------------------

void BlendTreeEditor::StartPreview() {
    m_previewActive = true;
    m_previewTime = 0.0f;
    CalculateBlendWeights();
}

void BlendTreeEditor::StopPreview() {
    m_previewActive = false;
}

void BlendTreeEditor::UpdatePreview(float deltaTime) {
    if (!m_previewActive) {
        return;
    }

    m_previewTime += deltaTime;
    CalculateBlendWeights();

    // In a full implementation, this would update the skeleton pose
    // based on the blend weights
}

// -------------------------------------------------------------------------
// Animation Mask
// -------------------------------------------------------------------------

void BlendTreeEditor::SetNodeMask(const std::string& nodeId, const AnimationMask& mask) {
    // Store mask in node or separate map
    // Implementation would depend on how masks are stored
}

AnimationMask BlendTreeEditor::GetNodeMask(const std::string& nodeId) const {
    AnimationMask mask;
    // Retrieve mask for node
    return mask;
}

void BlendTreeEditor::ShowMaskEditor(const std::string& nodeId) {
    // Would open a dialog or panel to edit the mask
}

// -------------------------------------------------------------------------
// Private Methods
// -------------------------------------------------------------------------

void BlendTreeEditor::RecordAction(BlendTreeEditorAction::Type type, const std::string& target,
                                    const json& before, const json& after) {
    BlendTreeEditorAction action;
    action.type = type;
    action.targetId = target;
    action.beforeData = before;
    action.afterData = after;

    m_undoStack.push_back(action);
    m_redoStack.clear();

    // Limit undo stack size
    while (m_undoStack.size() > MAX_UNDO_SIZE) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

BlendTreeNode* BlendTreeEditor::FindNodeAt(const glm::vec2& position) {
    // Search in reverse order (top-most first)
    for (auto it = m_nodes.rbegin(); it != m_nodes.rend(); ++it) {
        if (position.x >= it->position.x &&
            position.x <= it->position.x + it->size.x &&
            position.y >= it->position.y &&
            position.y <= it->position.y + it->size.y) {
            return &(*it);
        }
    }
    return nullptr;
}

BlendTreeConnection* BlendTreeEditor::FindConnectionAt(const glm::vec2& position) {
    const float threshold = 10.0f;

    for (auto& conn : m_connections) {
        BlendTreeNode* parent = GetNode(conn.parentId);
        BlendTreeNode* child = GetNode(conn.childId);

        if (parent && child) {
            glm::vec2 start = parent->position + glm::vec2(parent->size.x / 2, parent->size.y);
            glm::vec2 end = child->position + glm::vec2(child->size.x / 2, 0);

            // Simple line distance check
            glm::vec2 lineVec = end - start;
            glm::vec2 pointVec = position - start;
            float lineLenSq = glm::dot(lineVec, lineVec);
            float t = std::clamp(glm::dot(pointVec, lineVec) / lineLenSq, 0.0f, 1.0f);
            glm::vec2 closest = start + t * lineVec;
            float dist = glm::length(position - closest);

            if (dist < threshold) {
                return &conn;
            }
        }
    }
    return nullptr;
}

glm::vec2 BlendTreeEditor::ScreenToWorld(const glm::vec2& screenPos) const {
    return screenPos / m_zoom + m_viewOffset;
}

glm::vec2 BlendTreeEditor::WorldToScreen(const glm::vec2& worldPos) const {
    return (worldPos - m_viewOffset) * m_zoom;
}

glm::vec2 BlendTreeEditor::SnapToGrid(const glm::vec2& position) const {
    return glm::vec2(
        std::round(position.x / m_config.gridSize.x) * m_config.gridSize.x,
        std::round(position.y / m_config.gridSize.y) * m_config.gridSize.y
    );
}

std::string BlendTreeEditor::GenerateNodeId() {
    return "node_" + std::to_string(m_nodeIdCounter++);
}

void BlendTreeEditor::CalculateBlendWeights() {
    m_currentWeights.clear();

    BlendTreeNode* root = GetNode(m_rootNodeId);
    if (!root) return;

    if (root->type == "1d") {
        float blendValue = GetParameterValue(root->parameter);

        // Collect children with thresholds
        std::vector<std::pair<float, std::string>> children;
        for (const auto& conn : m_connections) {
            if (conn.parentId == m_rootNodeId) {
                BlendTreeNode* child = GetNode(conn.childId);
                if (child && child->type == "clip") {
                    children.push_back({child->threshold, child->clipPath});
                }
            }
        }

        // Sort by threshold
        std::sort(children.begin(), children.end());

        // Calculate weights
        for (size_t i = 0; i < children.size(); ++i) {
            float weight = 0.0f;

            if (children.size() == 1) {
                weight = 1.0f;
            } else if (i == 0) {
                if (blendValue <= children[0].first) {
                    weight = 1.0f;
                } else if (blendValue < children[1].first) {
                    weight = 1.0f - (blendValue - children[0].first) /
                             (children[1].first - children[0].first);
                }
            } else if (i == children.size() - 1) {
                if (blendValue >= children[i].first) {
                    weight = 1.0f;
                } else if (blendValue > children[i - 1].first) {
                    weight = (blendValue - children[i - 1].first) /
                             (children[i].first - children[i - 1].first);
                }
            } else {
                if (blendValue >= children[i - 1].first && blendValue <= children[i].first) {
                    weight = (blendValue - children[i - 1].first) /
                             (children[i].first - children[i - 1].first);
                } else if (blendValue >= children[i].first && blendValue <= children[i + 1].first) {
                    weight = 1.0f - (blendValue - children[i].first) /
                             (children[i + 1].first - children[i].first);
                }
            }

            m_currentWeights.push_back({children[i].second, weight});
        }
    } else if (root->type == "2d" || root->type == "freeform_2d") {
        glm::vec2 blendPos = GetCurrentBlendPosition();

        // Calculate weights using inverse distance weighting
        float totalWeight = 0.0f;
        std::vector<std::pair<std::string, float>> weights;

        for (const auto& conn : m_connections) {
            if (conn.parentId == m_rootNodeId) {
                BlendTreeNode* child = GetNode(conn.childId);
                if (child && child->type == "clip") {
                    float dist = glm::length(child->position2D - blendPos);
                    float weight = 1.0f / (dist + 0.001f);  // Avoid division by zero
                    weights.push_back({child->clipPath, weight});
                    totalWeight += weight;
                }
            }
        }

        // Normalize weights
        for (auto& [clip, weight] : weights) {
            weight /= totalWeight;
            m_currentWeights.push_back({clip, weight});
        }
    }
}

void BlendTreeEditor::LayoutSubtree(const std::string& nodeId, float x, float& y, float levelHeight) {
    BlendTreeNode* node = GetNode(nodeId);
    if (!node) return;

    // Collect children
    std::vector<std::string> children;
    for (const auto& conn : m_connections) {
        if (conn.parentId == nodeId) {
            children.push_back(conn.childId);
        }
    }

    if (children.empty()) {
        node->position = glm::vec2(x - node->size.x / 2, y);
        y += node->size.y + 20.0f;
    } else {
        float childStartY = y + levelHeight;
        float childX = x - (children.size() - 1) * 100.0f;

        for (const auto& childId : children) {
            LayoutSubtree(childId, childX, childStartY, levelHeight);
            childX += 200.0f;
        }

        node->position = glm::vec2(x - node->size.x / 2, y);
    }
}

// ============================================================================
// BlendTreeNodePropertiesPanel Implementation
// ============================================================================

void BlendTreeNodePropertiesPanel::SetNode(BlendTreeNode* node) {
    m_node = node;
    if (node) {
        m_editNode = *node;
    }
}

bool BlendTreeNodePropertiesPanel::Render() {
    if (!m_node) {
        return false;
    }

    bool modified = false;

    // Would render UI controls for editing node properties
    // This would use ImGui or similar in the actual implementation

    if (modified && OnNodeModified) {
        OnNodeModified(m_editNode);
    }

    return modified;
}

BlendTreeNode BlendTreeNodePropertiesPanel::GetModifiedNode() const {
    return m_editNode;
}

// ============================================================================
// BlendTreeParameterPanel Implementation
// ============================================================================

void BlendTreeParameterPanel::SetParameters(std::vector<ParameterSlider>* parameters) {
    m_parameters = parameters;
}

void BlendTreeParameterPanel::Render() {
    if (!m_parameters) {
        return;
    }

    // Would render slider controls for each parameter
    // This would use ImGui or similar in the actual implementation
}

void BlendTreeParameterPanel::OnSliderDrag(const std::string& name, float value) {
    if (OnParameterChanged) {
        OnParameterChanged(name, value);
    }
}

// ============================================================================
// BlendSpacePanel Implementation
// ============================================================================

void BlendSpacePanel::Initialize(const glm::vec2& size) {
    m_size = size;
}

void BlendSpacePanel::SetPoints(const std::vector<BlendSpacePoint>& points) {
    m_points = points;
}

void BlendSpacePanel::SetCurrentPosition(const glm::vec2& position) {
    m_currentPosition = position;
}

void BlendSpacePanel::Render() {
    // Would render the 2D blend space visualization
    // - Grid background
    // - Points at their positions (sized by weight)
    // - Current position indicator
}

void BlendSpacePanel::OnMouseDown(const glm::vec2& position, int button) {
    if (button == 0) {
        m_dragging = true;

        // Convert screen position to blend space coordinates
        glm::vec2 blendPos;
        blendPos.x = (position.x / m_size.x) * 2.0f - 1.0f;
        blendPos.y = (position.y / m_size.y) * 2.0f - 1.0f;

        if (OnPositionChanged) {
            OnPositionChanged(blendPos);
        }
    }
}

void BlendSpacePanel::OnMouseMove(const glm::vec2& position) {
    if (m_dragging) {
        glm::vec2 blendPos;
        blendPos.x = (position.x / m_size.x) * 2.0f - 1.0f;
        blendPos.y = (position.y / m_size.y) * 2.0f - 1.0f;

        blendPos.x = std::clamp(blendPos.x, -1.0f, 1.0f);
        blendPos.y = std::clamp(blendPos.y, -1.0f, 1.0f);

        if (OnPositionChanged) {
            OnPositionChanged(blendPos);
        }
    }
}

void BlendSpacePanel::OnMouseUp(const glm::vec2& position, int button) {
    if (button == 0) {
        m_dragging = false;
    }
}

} // namespace Vehement
