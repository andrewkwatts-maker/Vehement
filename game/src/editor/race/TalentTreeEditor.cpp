#include "TalentTreeEditor.hpp"
#include <algorithm>
#include <fstream>
#include <set>
#include <queue>

namespace Vehement {
namespace Editor {

// EditorConnection
nlohmann::json EditorConnection::ToJson() const {
    return {{"fromNodeId", fromNodeId}, {"toNodeId", toNodeId}, {"isHighlighted", isHighlighted}};
}

EditorConnection EditorConnection::FromJson(const nlohmann::json& j) {
    EditorConnection c;
    if (j.contains("fromNodeId")) c.fromNodeId = j["fromNodeId"].get<std::string>();
    if (j.contains("toNodeId")) c.toNodeId = j["toNodeId"].get<std::string>();
    if (j.contains("isHighlighted")) c.isHighlighted = j["isHighlighted"].get<bool>();
    return c;
}

// TalentTreeEditor
TalentTreeEditor::TalentTreeEditor() = default;
TalentTreeEditor::~TalentTreeEditor() = default;

bool TalentTreeEditor::Initialize() {
    if (m_initialized) return true;
    TalentNodeRegistry::Instance().Initialize();
    TalentTreeRegistry::Instance().Initialize();
    NewTree();
    m_initialized = true;
    return true;
}

void TalentTreeEditor::Shutdown() {
    m_initialized = false;
}

void TalentTreeEditor::NewTree() {
    m_tree = TalentTreeDefinition();
    m_tree.id = "new_tree";
    m_tree.name = "New Talent Tree";
    m_tree.totalTalentPoints = 30;
    m_tree.pointsPerAge = 5;
    m_tree.treeWidth = 5;
    m_tree.treeHeight = 7;
    m_state = TalentEditorState();
    m_connections.clear();
}

void TalentTreeEditor::NewTreeForRace(const std::string& raceId) {
    NewTree();
    m_tree.raceId = raceId;
    m_tree.id = raceId + "_tree";
}

bool TalentTreeEditor::LoadTree(const std::string& filepath) {
    if (!m_tree.LoadFromFile(filepath)) return false;
    RebuildConnections();
    m_state = TalentEditorState();
    return true;
}

bool TalentTreeEditor::SaveTree(const std::string& filepath) {
    return m_tree.SaveToFile(filepath);
}

void TalentTreeEditor::AddNode(const TalentNode& node) {
    m_tree.nodes[node.id] = node;
    RebuildConnections();
    MarkModified();
}

void TalentTreeEditor::RemoveNode(const std::string& nodeId) {
    m_tree.nodes.erase(nodeId);

    // Remove from branches
    for (auto& branch : m_tree.branches) {
        auto it = std::find(branch.nodeIds.begin(), branch.nodeIds.end(), nodeId);
        if (it != branch.nodeIds.end()) branch.nodeIds.erase(it);
    }

    // Remove connections
    m_connections.erase(
        std::remove_if(m_connections.begin(), m_connections.end(),
            [&nodeId](const EditorConnection& c) {
                return c.fromNodeId == nodeId || c.toNodeId == nodeId;
            }),
        m_connections.end());

    if (m_state.selectedNodeId == nodeId) {
        DeselectNode();
    }

    MarkModified();
}

void TalentTreeEditor::UpdateNode(const TalentNode& node) {
    m_tree.nodes[node.id] = node;
    RebuildConnections();
    MarkModified();
}

void TalentTreeEditor::MoveNode(const std::string& nodeId, int x, int y) {
    auto it = m_tree.nodes.find(nodeId);
    if (it != m_tree.nodes.end()) {
        it->second.positionX = x;
        it->second.positionY = y;
        MarkModified();
    }
}

void TalentTreeEditor::DuplicateNode(const std::string& nodeId) {
    auto it = m_tree.nodes.find(nodeId);
    if (it != m_tree.nodes.end()) {
        TalentNode newNode = it->second;
        newNode.id = nodeId + "_copy";
        newNode.positionX += 1;
        AddNode(newNode);
    }
}

const TalentNode* TalentTreeEditor::GetNode(const std::string& nodeId) const {
    auto it = m_tree.nodes.find(nodeId);
    return (it != m_tree.nodes.end()) ? &it->second : nullptr;
}

std::vector<const TalentNode*> TalentTreeEditor::GetAllNodes() const {
    std::vector<const TalentNode*> result;
    for (const auto& [id, node] : m_tree.nodes) {
        result.push_back(&node);
    }
    return result;
}

std::vector<const TalentNode*> TalentTreeEditor::GetNodesAtPosition(int x, int y) const {
    std::vector<const TalentNode*> result;
    for (const auto& [id, node] : m_tree.nodes) {
        if (node.positionX == x && node.positionY == y) {
            result.push_back(&node);
        }
    }
    return result;
}

void TalentTreeEditor::AddConnection(const std::string& fromId, const std::string& toId) {
    if (HasConnection(fromId, toId)) return;

    // Add to node prerequisites
    auto it = m_tree.nodes.find(toId);
    if (it != m_tree.nodes.end()) {
        if (std::find(it->second.prerequisites.begin(), it->second.prerequisites.end(), fromId)
            == it->second.prerequisites.end()) {
            it->second.prerequisites.push_back(fromId);
            it->second.connectedFrom = fromId;
        }
    }

    EditorConnection conn;
    conn.fromNodeId = fromId;
    conn.toNodeId = toId;
    m_connections.push_back(conn);

    MarkModified();
}

void TalentTreeEditor::RemoveConnection(const std::string& fromId, const std::string& toId) {
    // Remove from node prerequisites
    auto it = m_tree.nodes.find(toId);
    if (it != m_tree.nodes.end()) {
        auto& prereqs = it->second.prerequisites;
        prereqs.erase(std::remove(prereqs.begin(), prereqs.end(), fromId), prereqs.end());
        if (it->second.connectedFrom == fromId) {
            it->second.connectedFrom = prereqs.empty() ? "" : prereqs[0];
        }
    }

    m_connections.erase(
        std::remove_if(m_connections.begin(), m_connections.end(),
            [&fromId, &toId](const EditorConnection& c) {
                return c.fromNodeId == fromId && c.toNodeId == toId;
            }),
        m_connections.end());

    MarkModified();
}

std::vector<EditorConnection> TalentTreeEditor::GetConnections() const {
    return m_connections;
}

bool TalentTreeEditor::HasConnection(const std::string& fromId, const std::string& toId) const {
    return std::any_of(m_connections.begin(), m_connections.end(),
        [&fromId, &toId](const EditorConnection& c) {
            return c.fromNodeId == fromId && c.toNodeId == toId;
        });
}

void TalentTreeEditor::AddBranch(const TalentBranch& branch) {
    m_tree.branches.push_back(branch);
    MarkModified();
}

void TalentTreeEditor::RemoveBranch(const std::string& branchId) {
    m_tree.branches.erase(
        std::remove_if(m_tree.branches.begin(), m_tree.branches.end(),
            [&branchId](const TalentBranch& b) { return b.id == branchId; }),
        m_tree.branches.end());
    MarkModified();
}

void TalentTreeEditor::UpdateBranch(const TalentBranch& branch) {
    for (auto& b : m_tree.branches) {
        if (b.id == branch.id) {
            b = branch;
            break;
        }
    }
    MarkModified();
}

void TalentTreeEditor::AssignNodeToBranch(const std::string& nodeId, const std::string& branchId) {
    // Remove from other branches
    for (auto& branch : m_tree.branches) {
        auto it = std::find(branch.nodeIds.begin(), branch.nodeIds.end(), nodeId);
        if (it != branch.nodeIds.end()) branch.nodeIds.erase(it);
    }

    // Add to target branch
    for (auto& branch : m_tree.branches) {
        if (branch.id == branchId) {
            branch.nodeIds.push_back(nodeId);
            break;
        }
    }
    MarkModified();
}

const TalentBranch* TalentTreeEditor::GetBranch(const std::string& branchId) const {
    for (const auto& branch : m_tree.branches) {
        if (branch.id == branchId) return &branch;
    }
    return nullptr;
}

std::vector<const TalentBranch*> TalentTreeEditor::GetAllBranches() const {
    std::vector<const TalentBranch*> result;
    for (const auto& branch : m_tree.branches) {
        result.push_back(&branch);
    }
    return result;
}

void TalentTreeEditor::SetAgeGate(int age, const std::vector<std::string>& nodeIds, int bonusPoints) {
    for (auto& gate : m_tree.ageGates) {
        if (gate.age == age) {
            gate.unlockedNodes = nodeIds;
            gate.bonusTalentPoints = bonusPoints;
            return;
        }
    }

    AgeGate newGate;
    newGate.age = age;
    newGate.unlockedNodes = nodeIds;
    newGate.bonusTalentPoints = bonusPoints;
    m_tree.ageGates.push_back(newGate);
    MarkModified();
}

std::vector<std::string> TalentTreeEditor::GetNodesForAge(int age) const {
    for (const auto& gate : m_tree.ageGates) {
        if (gate.age == age) return gate.unlockedNodes;
    }
    return {};
}

void TalentTreeEditor::SelectNode(const std::string& nodeId) {
    m_state.selectedNodeId = nodeId;
    if (m_onNodeSelect) {
        m_onNodeSelect(nodeId);
    }
}

void TalentTreeEditor::DeselectNode() {
    m_state.selectedNodeId = "";
    m_state.hoveredNodeId = "";
}

const TalentNode* TalentTreeEditor::GetSelectedNode() const {
    return GetNode(m_state.selectedNodeId);
}

void TalentTreeEditor::SetViewOffset(float x, float y) {
    m_state.viewOffsetX = x;
    m_state.viewOffsetY = y;
}

void TalentTreeEditor::SetZoomLevel(float zoom) {
    m_state.zoomLevel = std::max(0.25f, std::min(4.0f, zoom));
}

void TalentTreeEditor::CenterView() {
    m_state.viewOffsetX = 0.0f;
    m_state.viewOffsetY = 0.0f;
}

void TalentTreeEditor::ZoomToFit() {
    // Calculate bounds and fit
    m_state.zoomLevel = 1.0f;
    CenterView();
}

void TalentTreeEditor::SetPreviewAge(int age) {
    m_state.currentAge = std::max(0, std::min(6, age));
}

std::vector<const TalentNode*> TalentTreeEditor::GetAvailableNodesAtAge(int age) const {
    std::vector<const TalentNode*> result;
    for (const auto& [id, node] : m_tree.nodes) {
        if (node.requiredAge <= age) {
            result.push_back(&node);
        }
    }
    return result;
}

bool TalentTreeEditor::Validate() const {
    return GetValidationErrors().empty();
}

std::vector<std::string> TalentTreeEditor::GetValidationErrors() const {
    std::vector<std::string> errors;

    if (m_tree.nodes.empty()) {
        errors.push_back("Tree has no nodes");
    }

    for (const auto& [id, node] : m_tree.nodes) {
        if (!node.Validate()) {
            errors.push_back("Invalid node: " + id);
        }

        // Check prerequisites exist
        for (const auto& prereq : node.prerequisites) {
            if (m_tree.nodes.find(prereq) == m_tree.nodes.end()) {
                errors.push_back("Node " + id + " has invalid prerequisite: " + prereq);
            }
        }
    }

    if (HasCircularDependency()) {
        errors.push_back("Tree has circular dependencies");
    }

    return errors;
}

bool TalentTreeEditor::HasCircularDependency() const {
    // DFS to detect cycles
    std::set<std::string> visited;
    std::set<std::string> inStack;

    std::function<bool(const std::string&)> hasCycle = [&](const std::string& nodeId) -> bool {
        visited.insert(nodeId);
        inStack.insert(nodeId);

        auto it = m_tree.nodes.find(nodeId);
        if (it != m_tree.nodes.end()) {
            for (const auto& prereq : it->second.prerequisites) {
                if (inStack.find(prereq) != inStack.end()) return true;
                if (visited.find(prereq) == visited.end() && hasCycle(prereq)) return true;
            }
        }

        inStack.erase(nodeId);
        return false;
    };

    for (const auto& [id, node] : m_tree.nodes) {
        if (visited.find(id) == visited.end()) {
            if (hasCycle(id)) return true;
        }
    }

    return false;
}

void TalentTreeEditor::Render() {}
void TalentTreeEditor::RenderNode(const TalentNode& node) {}
void TalentTreeEditor::RenderConnection(const EditorConnection& connection) {}
void TalentTreeEditor::RenderBranch(const TalentBranch& branch) {}
void TalentTreeEditor::RenderAgeMarker(int age) {}
void TalentTreeEditor::RenderGrid() {}
void TalentTreeEditor::RenderNodeProperties() {}

void TalentTreeEditor::OnMouseDown(float x, float y, int button) {
    auto [worldX, worldY] = ScreenToWorld(x, y);

    // Check if clicked on a node
    for (const auto& [id, node] : m_tree.nodes) {
        float nodeX = node.positionX * 100.0f;
        float nodeY = node.positionY * 100.0f;
        if (worldX >= nodeX && worldX <= nodeX + 80 &&
            worldY >= nodeY && worldY <= nodeY + 80) {
            if (button == 0) {
                SelectNode(id);
                m_state.isDragging = true;
                m_state.dragStartX = worldX - nodeX;
                m_state.dragStartY = worldY - nodeY;
            }
            return;
        }
    }

    DeselectNode();
}

void TalentTreeEditor::OnMouseUp(float x, float y, int button) {
    m_state.isDragging = false;
}

void TalentTreeEditor::OnMouseMove(float x, float y) {
    if (m_state.isDragging && !m_state.selectedNodeId.empty()) {
        auto [worldX, worldY] = ScreenToWorld(x, y);
        int newX = static_cast<int>((worldX - m_state.dragStartX) / 100.0f + 0.5f);
        int newY = static_cast<int>((worldY - m_state.dragStartY) / 100.0f + 0.5f);
        MoveNode(m_state.selectedNodeId, newX, newY);
    }
}

void TalentTreeEditor::OnMouseWheel(float delta) {
    SetZoomLevel(m_state.zoomLevel + delta * 0.1f);
}

void TalentTreeEditor::OnKeyDown(int key) {
    // Delete key
    if (key == 127 || key == 8) {
        if (!m_state.selectedNodeId.empty()) {
            RemoveNode(m_state.selectedNodeId);
        }
    }
}

std::string TalentTreeEditor::ExportToJson() const {
    return m_tree.ToJson().dump(2);
}

bool TalentTreeEditor::ImportFromJson(const std::string& json) {
    try {
        auto j = nlohmann::json::parse(json);
        m_tree = TalentTreeDefinition::FromJson(j);
        RebuildConnections();
        return true;
    } catch (...) {
        return false;
    }
}

void TalentTreeEditor::MarkModified() {
    if (m_onTreeModified) {
        m_onTreeModified();
    }
}

void TalentTreeEditor::RebuildConnections() {
    m_connections.clear();
    for (const auto& [id, node] : m_tree.nodes) {
        for (const auto& prereq : node.prerequisites) {
            EditorConnection conn;
            conn.fromNodeId = prereq;
            conn.toNodeId = id;
            m_connections.push_back(conn);
        }
    }
}

std::pair<float, float> TalentTreeEditor::ScreenToWorld(float screenX, float screenY) const {
    float worldX = (screenX - m_state.viewOffsetX) / m_state.zoomLevel;
    float worldY = (screenY - m_state.viewOffsetY) / m_state.zoomLevel;
    return {worldX, worldY};
}

std::pair<float, float> TalentTreeEditor::WorldToScreen(float worldX, float worldY) const {
    float screenX = worldX * m_state.zoomLevel + m_state.viewOffsetX;
    float screenY = worldY * m_state.zoomLevel + m_state.viewOffsetY;
    return {screenX, screenY};
}

// HTML Bridge
TalentTreeEditorHTMLBridge& TalentTreeEditorHTMLBridge::Instance() {
    static TalentTreeEditorHTMLBridge instance;
    return instance;
}

void TalentTreeEditorHTMLBridge::Initialize(TalentTreeEditor* editor) {
    m_editor = editor;
}

std::string TalentTreeEditorHTMLBridge::GetTreeJson() const {
    if (!m_editor) return "{}";
    return m_editor->ExportToJson();
}

void TalentTreeEditorHTMLBridge::SetTreeJson(const std::string& json) {
    if (m_editor) m_editor->ImportFromJson(json);
}

std::string TalentTreeEditorHTMLBridge::GetNodesJson() const {
    if (!m_editor) return "[]";
    nlohmann::json arr = nlohmann::json::array();
    for (const auto* node : m_editor->GetAllNodes()) {
        arr.push_back(node->ToJson());
    }
    return arr.dump();
}

std::string TalentTreeEditorHTMLBridge::GetConnectionsJson() const {
    if (!m_editor) return "[]";
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& conn : m_editor->GetConnections()) {
        arr.push_back(conn.ToJson());
    }
    return arr.dump();
}

std::string TalentTreeEditorHTMLBridge::GetSelectedNodeJson() const {
    if (!m_editor) return "null";
    const auto* node = m_editor->GetSelectedNode();
    return node ? node->ToJson().dump() : "null";
}

void TalentTreeEditorHTMLBridge::SelectNode(const std::string& nodeId) {
    if (m_editor) m_editor->SelectNode(nodeId);
}

void TalentTreeEditorHTMLBridge::AddNode(const std::string& nodeJson) {
    if (!m_editor) return;
    try {
        auto j = nlohmann::json::parse(nodeJson);
        m_editor->AddNode(TalentNode::FromJson(j));
    } catch (...) {}
}

void TalentTreeEditorHTMLBridge::UpdateNode(const std::string& nodeJson) {
    if (!m_editor) return;
    try {
        auto j = nlohmann::json::parse(nodeJson);
        m_editor->UpdateNode(TalentNode::FromJson(j));
    } catch (...) {}
}

void TalentTreeEditorHTMLBridge::RemoveNode(const std::string& nodeId) {
    if (m_editor) m_editor->RemoveNode(nodeId);
}

void TalentTreeEditorHTMLBridge::MoveNode(const std::string& nodeId, int x, int y) {
    if (m_editor) m_editor->MoveNode(nodeId, x, y);
}

void TalentTreeEditorHTMLBridge::AddConnection(const std::string& fromId, const std::string& toId) {
    if (m_editor) m_editor->AddConnection(fromId, toId);
}

void TalentTreeEditorHTMLBridge::RemoveConnection(const std::string& fromId, const std::string& toId) {
    if (m_editor) m_editor->RemoveConnection(fromId, toId);
}

} // namespace Editor
} // namespace Vehement
