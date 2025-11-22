#include "TechVisualizer.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <queue>

namespace Vehement {
namespace TechTree {

// ============================================================================
// VisualNode
// ============================================================================

nlohmann::json VisualNode::ToJson() const {
    nlohmann::json j;
    j["tech_id"] = techId;
    j["name"] = name;
    j["position"] = {position.x, position.y};
    j["size"] = {size.x, size.y};
    j["status"] = ResearchStatusToString(status);
    j["progress"] = progress;
    j["category"] = TechCategoryToString(category);
    j["tier"] = tier;
    return j;
}

VisualNode VisualNode::FromJson(const nlohmann::json& j) {
    VisualNode node;
    if (j.contains("tech_id")) node.techId = j["tech_id"].get<std::string>();
    if (j.contains("name")) node.name = j["name"].get<std::string>();
    if (j.contains("position") && j["position"].size() >= 2) {
        node.position = {j["position"][0].get<float>(), j["position"][1].get<float>()};
    }
    if (j.contains("size") && j["size"].size() >= 2) {
        node.size = {j["size"][0].get<float>(), j["size"][1].get<float>()};
    }
    if (j.contains("progress")) node.progress = j["progress"].get<float>();
    if (j.contains("tier")) node.tier = j["tier"].get<int>();
    return node;
}

// ============================================================================
// VisualConnection
// ============================================================================

nlohmann::json VisualConnection::ToJson() const {
    nlohmann::json j;
    j["from"] = fromTech;
    j["to"] = toTech;
    j["start"] = {startPoint.x, startPoint.y};
    j["end"] = {endPoint.x, endPoint.y};
    if (!controlPoints.empty()) {
        nlohmann::json cps = nlohmann::json::array();
        for (const auto& cp : controlPoints) {
            cps.push_back({cp.x, cp.y});
        }
        j["control_points"] = cps;
    }
    j["highlighted"] = isHighlighted;
    j["required"] = isRequired;
    return j;
}

VisualConnection VisualConnection::FromJson(const nlohmann::json& j) {
    VisualConnection conn;
    if (j.contains("from")) conn.fromTech = j["from"].get<std::string>();
    if (j.contains("to")) conn.toTech = j["to"].get<std::string>();
    if (j.contains("start") && j["start"].size() >= 2) {
        conn.startPoint = {j["start"][0].get<float>(), j["start"][1].get<float>()};
    }
    if (j.contains("end") && j["end"].size() >= 2) {
        conn.endPoint = {j["end"][0].get<float>(), j["end"][1].get<float>()};
    }
    if (j.contains("control_points")) {
        for (const auto& cp : j["control_points"]) {
            if (cp.size() >= 2) {
                conn.controlPoints.push_back({cp[0].get<float>(), cp[1].get<float>()});
            }
        }
    }
    if (j.contains("highlighted")) conn.isHighlighted = j["highlighted"].get<bool>();
    if (j.contains("required")) conn.isRequired = j["required"].get<bool>();
    return conn;
}

// ============================================================================
// LayoutSettings
// ============================================================================

nlohmann::json LayoutSettings::ToJson() const {
    nlohmann::json j;
    j["type"] = LayoutTypeToString(type);
    j["node_width"] = nodeWidth;
    j["node_height"] = nodeHeight;
    j["horizontal_spacing"] = horizontalSpacing;
    j["vertical_spacing"] = verticalSpacing;
    j["tier_spacing"] = tierSpacing;
    j["margin_left"] = marginLeft;
    j["margin_top"] = marginTop;
    j["tree_top_to_bottom"] = treeTopToBottom;
    j["grid_columns"] = gridColumns;
    j["group_by_category"] = groupByCategory;
    j["curved_connections"] = curvedConnections;
    return j;
}

LayoutSettings LayoutSettings::FromJson(const nlohmann::json& j) {
    LayoutSettings s;
    if (j.contains("type")) {
        std::string t = j["type"].get<std::string>();
        if (t == "tree") s.type = LayoutType::Tree;
        else if (t == "grid") s.type = LayoutType::Grid;
        else if (t == "radial") s.type = LayoutType::Radial;
        else if (t == "force") s.type = LayoutType::Force;
        else if (t == "custom") s.type = LayoutType::Custom;
    }
    if (j.contains("node_width")) s.nodeWidth = j["node_width"].get<float>();
    if (j.contains("node_height")) s.nodeHeight = j["node_height"].get<float>();
    if (j.contains("horizontal_spacing")) s.horizontalSpacing = j["horizontal_spacing"].get<float>();
    if (j.contains("vertical_spacing")) s.verticalSpacing = j["vertical_spacing"].get<float>();
    if (j.contains("tier_spacing")) s.tierSpacing = j["tier_spacing"].get<float>();
    if (j.contains("margin_left")) s.marginLeft = j["margin_left"].get<float>();
    if (j.contains("margin_top")) s.marginTop = j["margin_top"].get<float>();
    if (j.contains("tree_top_to_bottom")) s.treeTopToBottom = j["tree_top_to_bottom"].get<bool>();
    if (j.contains("grid_columns")) s.gridColumns = j["grid_columns"].get<int>();
    if (j.contains("group_by_category")) s.groupByCategory = j["group_by_category"].get<bool>();
    if (j.contains("curved_connections")) s.curvedConnections = j["curved_connections"].get<bool>();
    return s;
}

// ============================================================================
// HighlightPath
// ============================================================================

bool HighlightPath::Contains(const std::string& techId) const {
    return std::find(techs.begin(), techs.end(), techId) != techs.end();
}

bool HighlightPath::ContainsConnection(const std::string& from, const std::string& to) const {
    for (const auto& conn : connections) {
        if (conn.first == from && conn.second == to) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// TechTreeVisualizer
// ============================================================================

void TechTreeVisualizer::Initialize(const TechTreeDef* treeDef, const PlayerTechTree* playerTree) {
    m_treeDef = treeDef;
    m_playerTree = playerTree;

    // Set default colors
    m_statusColors[ResearchStatus::Locked] = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);
    m_statusColors[ResearchStatus::Available] = glm::vec4(0.4f, 0.6f, 0.4f, 1.0f);
    m_statusColors[ResearchStatus::Queued] = glm::vec4(0.5f, 0.5f, 0.7f, 1.0f);
    m_statusColors[ResearchStatus::InProgress] = glm::vec4(0.7f, 0.7f, 0.2f, 1.0f);
    m_statusColors[ResearchStatus::Completed] = glm::vec4(0.2f, 0.7f, 0.2f, 1.0f);
    m_statusColors[ResearchStatus::Lost] = glm::vec4(0.6f, 0.3f, 0.3f, 1.0f);

    m_categoryColors[TechCategory::Military] = glm::vec4(0.8f, 0.3f, 0.3f, 1.0f);
    m_categoryColors[TechCategory::Economy] = glm::vec4(0.8f, 0.7f, 0.2f, 1.0f);
    m_categoryColors[TechCategory::Defense] = glm::vec4(0.3f, 0.5f, 0.8f, 1.0f);
    m_categoryColors[TechCategory::Infrastructure] = glm::vec4(0.6f, 0.4f, 0.2f, 1.0f);
    m_categoryColors[TechCategory::Science] = glm::vec4(0.5f, 0.3f, 0.7f, 1.0f);
    m_categoryColors[TechCategory::Magic] = glm::vec4(0.7f, 0.3f, 0.8f, 1.0f);
    m_categoryColors[TechCategory::Culture] = glm::vec4(0.3f, 0.7f, 0.7f, 1.0f);
    m_categoryColors[TechCategory::Special] = glm::vec4(0.9f, 0.6f, 0.1f, 1.0f);

    if (treeDef) {
        GenerateLayout();
    }
}

void TechTreeVisualizer::SetPlayerTree(const PlayerTechTree* playerTree) {
    m_playerTree = playerTree;
    UpdateFromPlayerState();
}

void TechTreeVisualizer::Clear() {
    m_visualNodes.clear();
    m_visualConnections.clear();
    m_nodeIndex.clear();
    m_selectedNodeId.clear();
    m_hoveredNodeId.clear();
    m_highlightedPath = HighlightPath{};
}

void TechTreeVisualizer::SetLayoutSettings(const LayoutSettings& settings) {
    m_layoutSettings = settings;
}

void TechTreeVisualizer::GenerateLayout() {
    if (!m_treeDef) return;

    Clear();

    // Create visual nodes from tech nodes
    const auto& allNodes = m_treeDef->GetAllNodes();
    m_visualNodes.reserve(allNodes.size());

    for (const auto& [techId, techNode] : allNodes) {
        VisualNode vn;
        vn.techId = techId;
        vn.name = techNode.GetName();
        vn.icon = techNode.GetIcon();
        vn.description = techNode.GetDescription();
        vn.category = techNode.GetCategory();
        vn.age = techNode.GetAgeRequirement();
        vn.tier = techNode.GetTier();
        vn.size = glm::vec2(m_layoutSettings.nodeWidth, m_layoutSettings.nodeHeight);

        // Use custom position if available
        const auto& pos = techNode.GetPosition();
        vn.position = glm::vec2(pos.x, pos.y);

        m_nodeIndex[techId] = m_visualNodes.size();
        m_visualNodes.push_back(vn);
    }

    // Run layout algorithm
    switch (m_layoutSettings.type) {
        case LayoutType::Tree:   LayoutTree(); break;
        case LayoutType::Grid:   LayoutGrid(); break;
        case LayoutType::Radial: LayoutRadial(); break;
        case LayoutType::Force:  LayoutForce(); break;
        case LayoutType::Custom: LayoutCustom(); break;
        default: LayoutTree(); break;
    }

    GenerateConnections();
    CalculateBounds();
    UpdateFromPlayerState();
}

void TechTreeVisualizer::LayoutTree() {
    if (m_visualNodes.empty()) return;

    // Group nodes by tier
    std::map<int, std::vector<size_t>> tierNodes;
    for (size_t i = 0; i < m_visualNodes.size(); ++i) {
        tierNodes[m_visualNodes[i].tier].push_back(i);
    }

    float x = m_layoutSettings.marginLeft;
    float y = m_layoutSettings.marginTop;

    for (auto& [tier, indices] : tierNodes) {
        float maxHeight = 0.0f;

        for (size_t i = 0; i < indices.size(); ++i) {
            auto& node = m_visualNodes[indices[i]];

            if (m_layoutSettings.treeTopToBottom) {
                node.position.x = x + i * (m_layoutSettings.nodeWidth + m_layoutSettings.horizontalSpacing);
                node.position.y = y;
            } else {
                node.position.x = x;
                node.position.y = y + i * (m_layoutSettings.nodeHeight + m_layoutSettings.verticalSpacing);
            }

            maxHeight = std::max(maxHeight, node.size.y);
        }

        if (m_layoutSettings.treeTopToBottom) {
            y += maxHeight + m_layoutSettings.tierSpacing;
        } else {
            x += m_layoutSettings.nodeWidth + m_layoutSettings.tierSpacing;
        }
    }
}

void TechTreeVisualizer::LayoutGrid() {
    if (m_visualNodes.empty()) return;

    int col = 0;
    int row = 0;

    for (auto& node : m_visualNodes) {
        node.position.x = m_layoutSettings.marginLeft + col * (m_layoutSettings.nodeWidth + m_layoutSettings.horizontalSpacing);
        node.position.y = m_layoutSettings.marginTop + row * (m_layoutSettings.nodeHeight + m_layoutSettings.verticalSpacing);

        col++;
        if (col >= m_layoutSettings.gridColumns) {
            col = 0;
            row++;
        }
    }
}

void TechTreeVisualizer::LayoutRadial() {
    if (m_visualNodes.empty()) return;

    // Group by tier for concentric rings
    std::map<int, std::vector<size_t>> tierNodes;
    for (size_t i = 0; i < m_visualNodes.size(); ++i) {
        tierNodes[m_visualNodes[i].tier].push_back(i);
    }

    glm::vec2 center(500.0f, 500.0f);
    float radius = m_layoutSettings.radialStartRadius;

    for (auto& [tier, indices] : tierNodes) {
        float angleStep = 2.0f * 3.14159f / static_cast<float>(indices.size());
        float angle = 0.0f;

        for (size_t idx : indices) {
            auto& node = m_visualNodes[idx];
            node.position.x = center.x + radius * std::cos(angle) - node.size.x * 0.5f;
            node.position.y = center.y + radius * std::sin(angle) - node.size.y * 0.5f;
            angle += angleStep;
        }

        radius += m_layoutSettings.radialRadiusIncrement;
    }
}

void TechTreeVisualizer::LayoutForce() {
    // Simplified force-directed layout
    // In a real implementation, this would use iterative simulation
    LayoutTree(); // Fall back to tree layout for now
}

void TechTreeVisualizer::LayoutCustom() {
    // Use positions from tech nodes (already loaded in GenerateLayout)
}

void TechTreeVisualizer::GenerateConnections() {
    m_visualConnections.clear();

    if (!m_treeDef) return;

    const auto& connections = m_treeDef->GetConnections();

    for (const auto& conn : connections) {
        auto fromIt = m_nodeIndex.find(conn.fromTech);
        auto toIt = m_nodeIndex.find(conn.toTech);

        if (fromIt == m_nodeIndex.end() || toIt == m_nodeIndex.end()) continue;

        const auto& fromNode = m_visualNodes[fromIt->second];
        const auto& toNode = m_visualNodes[toIt->second];

        VisualConnection vc;
        vc.fromTech = conn.fromTech;
        vc.toTech = conn.toTech;
        vc.isRequired = conn.isRequired;

        // Calculate connection points (center bottom of from, center top of to)
        vc.startPoint = fromNode.position + glm::vec2(fromNode.size.x * 0.5f, fromNode.size.y);
        vc.endPoint = toNode.position + glm::vec2(toNode.size.x * 0.5f, 0.0f);

        if (m_layoutSettings.curvedConnections) {
            vc.controlPoints = GenerateCurvePoints(vc.startPoint, vc.endPoint, m_layoutSettings.connectionCurveStrength);
        }

        m_visualConnections.push_back(vc);
    }

    // Also generate connections from prerequisites
    for (const auto& [techId, techNode] : m_treeDef->GetAllNodes()) {
        auto toIt = m_nodeIndex.find(techId);
        if (toIt == m_nodeIndex.end()) continue;

        for (const auto& prereq : techNode.GetPrerequisites()) {
            auto fromIt = m_nodeIndex.find(prereq);
            if (fromIt == m_nodeIndex.end()) continue;

            // Check if connection already exists
            bool exists = false;
            for (const auto& vc : m_visualConnections) {
                if (vc.fromTech == prereq && vc.toTech == techId) {
                    exists = true;
                    break;
                }
            }
            if (exists) continue;

            const auto& fromNode = m_visualNodes[fromIt->second];
            const auto& toNode = m_visualNodes[toIt->second];

            VisualConnection vc;
            vc.fromTech = prereq;
            vc.toTech = techId;
            vc.isRequired = true;

            vc.startPoint = fromNode.position + glm::vec2(fromNode.size.x * 0.5f, fromNode.size.y);
            vc.endPoint = toNode.position + glm::vec2(toNode.size.x * 0.5f, 0.0f);

            if (m_layoutSettings.curvedConnections) {
                vc.controlPoints = GenerateCurvePoints(vc.startPoint, vc.endPoint, m_layoutSettings.connectionCurveStrength);
            }

            m_visualConnections.push_back(vc);
        }
    }
}

void TechTreeVisualizer::CalculateBounds() {
    if (m_visualNodes.empty()) {
        m_bounds = glm::vec4(0.0f);
        return;
    }

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();

    for (const auto& node : m_visualNodes) {
        minX = std::min(minX, node.position.x);
        minY = std::min(minY, node.position.y);
        maxX = std::max(maxX, node.position.x + node.size.x);
        maxY = std::max(maxY, node.position.y + node.size.y);
    }

    m_bounds = glm::vec4(minX, minY, maxX - minX, maxY - minY);
}

void TechTreeVisualizer::UpdateFromPlayerState() {
    if (!m_playerTree) return;

    for (auto& node : m_visualNodes) {
        node.status = m_playerTree->GetTechStatus(node.techId);

        const auto* progress = m_playerTree->GetProgress(node.techId);
        node.progress = progress ? progress->GetProgressPercent() : 0.0f;
    }

    UpdateNodeColors();
}

void TechTreeVisualizer::UpdateNodeColors() {
    for (auto& node : m_visualNodes) {
        node.backgroundColor = GetStatusColor(node.status);

        // Modify for highlight/selection
        if (node.isSelected) {
            node.borderColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
        } else if (node.isHovered) {
            node.borderColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
        } else if (node.isHighlighted) {
            node.borderColor = m_highlightedPath.highlightColor;
        } else {
            node.borderColor = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
        }
    }

    for (auto& conn : m_visualConnections) {
        if (conn.isHighlighted) {
            conn.color = m_highlightedPath.highlightColor;
            conn.thickness = 3.0f;
        } else {
            conn.color = glm::vec4(0.5f, 0.5f, 0.5f, 0.8f);
            conn.thickness = 2.0f;
        }
    }
}

void TechTreeVisualizer::Update(float deltaTime) {
    m_animationTime += deltaTime;
    // Could animate progress bars, highlights, etc.
}

void TechTreeVisualizer::SelectNode(const std::string& techId) {
    // Clear old selection
    if (!m_selectedNodeId.empty()) {
        auto it = m_nodeIndex.find(m_selectedNodeId);
        if (it != m_nodeIndex.end()) {
            m_visualNodes[it->second].isSelected = false;
        }
    }

    m_selectedNodeId = techId;

    if (!techId.empty()) {
        auto it = m_nodeIndex.find(techId);
        if (it != m_nodeIndex.end()) {
            m_visualNodes[it->second].isSelected = true;
        }
    }

    UpdateNodeColors();
}

void TechTreeVisualizer::ClearSelection() {
    SelectNode("");
}

void TechTreeVisualizer::SetHoveredNode(const std::string& techId) {
    if (!m_hoveredNodeId.empty()) {
        auto it = m_nodeIndex.find(m_hoveredNodeId);
        if (it != m_nodeIndex.end()) {
            m_visualNodes[it->second].isHovered = false;
        }
    }

    m_hoveredNodeId = techId;

    if (!techId.empty()) {
        auto it = m_nodeIndex.find(techId);
        if (it != m_nodeIndex.end()) {
            m_visualNodes[it->second].isHovered = true;
        }
    }

    UpdateNodeColors();
}

void TechTreeVisualizer::HighlightPathTo(const std::string& techId) {
    HighlightPath path;
    path.targetTech = techId;

    if (m_playerTree) {
        path.techs = ResearchQueueManager::GetResearchPath(techId, *m_playerTree);

        // Build connections
        for (size_t i = 0; i + 1 < path.techs.size(); ++i) {
            path.connections.emplace_back(path.techs[i], path.techs[i + 1]);
        }
    }

    HighlightPath(path);
}

void TechTreeVisualizer::HighlightPath(const HighlightPath& path) {
    ClearHighlight();
    m_highlightedPath = path;

    for (auto& node : m_visualNodes) {
        node.isHighlighted = path.Contains(node.techId);
    }

    for (auto& conn : m_visualConnections) {
        conn.isHighlighted = path.ContainsConnection(conn.fromTech, conn.toTech);
    }

    UpdateNodeColors();
}

void TechTreeVisualizer::ClearHighlight() {
    m_highlightedPath = HighlightPath{};

    for (auto& node : m_visualNodes) {
        node.isHighlighted = false;
    }

    for (auto& conn : m_visualConnections) {
        conn.isHighlighted = false;
    }

    UpdateNodeColors();
}

const VisualNode* TechTreeVisualizer::GetVisualNode(const std::string& techId) const {
    auto it = m_nodeIndex.find(techId);
    return it != m_nodeIndex.end() ? &m_visualNodes[it->second] : nullptr;
}

VisualNode* TechTreeVisualizer::GetVisualNodeMutable(const std::string& techId) {
    auto it = m_nodeIndex.find(techId);
    return it != m_nodeIndex.end() ? &m_visualNodes[it->second] : nullptr;
}

std::vector<const VisualNode*> TechTreeVisualizer::GetNodesInCategory(TechCategory category) const {
    std::vector<const VisualNode*> result;
    for (const auto& node : m_visualNodes) {
        if (node.category == category) {
            result.push_back(&node);
        }
    }
    return result;
}

std::vector<const VisualNode*> TechTreeVisualizer::GetNodesInTier(int tier) const {
    std::vector<const VisualNode*> result;
    for (const auto& node : m_visualNodes) {
        if (node.tier == tier) {
            result.push_back(&node);
        }
    }
    return result;
}

std::vector<const VisualNode*> TechTreeVisualizer::GetNodesByStatus(ResearchStatus status) const {
    std::vector<const VisualNode*> result;
    for (const auto& node : m_visualNodes) {
        if (node.status == status) {
            result.push_back(&node);
        }
    }
    return result;
}

std::string TechTreeVisualizer::HitTest(const glm::vec2& position) const {
    for (const auto& node : m_visualNodes) {
        if (position.x >= node.position.x && position.x <= node.position.x + node.size.x &&
            position.y >= node.position.y && position.y <= node.position.y + node.size.y) {
            return node.techId;
        }
    }
    return "";
}

std::pair<std::string, std::string> TechTreeVisualizer::HitTestConnection(const glm::vec2& position, float tolerance) const {
    // Simple line distance check
    for (const auto& conn : m_visualConnections) {
        // Check distance to line segment
        glm::vec2 lineVec = conn.endPoint - conn.startPoint;
        glm::vec2 pointVec = position - conn.startPoint;
        float lineLen = glm::length(lineVec);
        if (lineLen < 0.001f) continue;

        float t = glm::clamp(glm::dot(pointVec, lineVec) / (lineLen * lineLen), 0.0f, 1.0f);
        glm::vec2 closest = conn.startPoint + t * lineVec;
        float dist = glm::length(position - closest);

        if (dist <= tolerance) {
            return {conn.fromTech, conn.toTech};
        }
    }
    return {"", ""};
}

glm::vec4 TechTreeVisualizer::GetStatusColor(ResearchStatus status) const {
    auto it = m_statusColors.find(status);
    return it != m_statusColors.end() ? it->second : glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
}

glm::vec4 TechTreeVisualizer::GetCategoryColor(TechCategory category) const {
    auto it = m_categoryColors.find(category);
    return it != m_categoryColors.end() ? it->second : glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
}

void TechTreeVisualizer::SetStatusColor(ResearchStatus status, const glm::vec4& color) {
    m_statusColors[status] = color;
}

void TechTreeVisualizer::SetCategoryColor(TechCategory category, const glm::vec4& color) {
    m_categoryColors[category] = color;
}

glm::vec2 TechTreeVisualizer::CalculateBezierPoint(const glm::vec2& start, const glm::vec2& end,
                                                    const glm::vec2& control, float t) const {
    float u = 1.0f - t;
    return u * u * start + 2.0f * u * t * control + t * t * end;
}

std::vector<glm::vec2> TechTreeVisualizer::GenerateCurvePoints(const glm::vec2& start, const glm::vec2& end,
                                                                float curveStrength) const {
    std::vector<glm::vec2> points;

    glm::vec2 mid = (start + end) * 0.5f;
    glm::vec2 control = mid + glm::vec2(0.0f, curveStrength * glm::length(end - start) * 0.3f);

    const int numPoints = 10;
    for (int i = 0; i <= numPoints; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(numPoints);
        points.push_back(CalculateBezierPoint(start, end, control, t));
    }

    return points;
}

nlohmann::json TechTreeVisualizer::ToJson() const {
    nlohmann::json j;

    j["layout_settings"] = m_layoutSettings.ToJson();
    j["bounds"] = {m_bounds.x, m_bounds.y, m_bounds.z, m_bounds.w};

    nlohmann::json nodesArray = nlohmann::json::array();
    for (const auto& node : m_visualNodes) {
        nodesArray.push_back(node.ToJson());
    }
    j["nodes"] = nodesArray;

    nlohmann::json connsArray = nlohmann::json::array();
    for (const auto& conn : m_visualConnections) {
        connsArray.push_back(conn.ToJson());
    }
    j["connections"] = connsArray;

    return j;
}

void TechTreeVisualizer::FromJson(const nlohmann::json& j) {
    if (j.contains("layout_settings")) {
        m_layoutSettings = LayoutSettings::FromJson(j["layout_settings"]);
    }

    if (j.contains("bounds") && j["bounds"].size() >= 4) {
        m_bounds = glm::vec4(
            j["bounds"][0].get<float>(),
            j["bounds"][1].get<float>(),
            j["bounds"][2].get<float>(),
            j["bounds"][3].get<float>()
        );
    }

    if (j.contains("nodes")) {
        m_visualNodes.clear();
        m_nodeIndex.clear();
        for (const auto& nodeJson : j["nodes"]) {
            auto node = VisualNode::FromJson(nodeJson);
            m_nodeIndex[node.techId] = m_visualNodes.size();
            m_visualNodes.push_back(node);
        }
    }

    if (j.contains("connections")) {
        m_visualConnections.clear();
        for (const auto& connJson : j["connections"]) {
            m_visualConnections.push_back(VisualConnection::FromJson(connJson));
        }
    }
}

// ============================================================================
// TechTreeMinimap
// ============================================================================

void TechTreeMinimap::Generate(const TechTreeVisualizer& visualizer, const glm::vec2& targetSize) {
    m_nodes.clear();
    m_targetSize = targetSize;

    const auto& bounds = visualizer.GetBounds();
    if (bounds.z <= 0 || bounds.w <= 0) return;

    m_scale = targetSize / glm::vec2(bounds.z, bounds.w);
    m_offset = glm::vec2(bounds.x, bounds.y);

    float minScale = std::min(m_scale.x, m_scale.y);
    m_scale = glm::vec2(minScale);

    for (const auto& vn : visualizer.GetVisualNodes()) {
        MinimapNode mn;
        mn.position = (vn.position - m_offset) * m_scale;
        mn.size = vn.size * m_scale * 0.5f;
        mn.size = glm::max(mn.size, glm::vec2(4.0f));
        mn.color = visualizer.GetStatusColor(vn.status);
        mn.status = vn.status;
        m_nodes.push_back(mn);
    }
}

glm::vec2 TechTreeMinimap::MinimapToTree(const glm::vec2& minimapPos) const {
    return minimapPos / m_scale + m_offset;
}

glm::vec2 TechTreeMinimap::TreeToMinimap(const glm::vec2& treePos) const {
    return (treePos - m_offset) * m_scale;
}

// ============================================================================
// ProgressIndicatorData
// ============================================================================

std::string ProgressIndicatorData::GetProgressText() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(0) << (progress * 100.0f) << "%";
    return oss.str();
}

std::string ProgressIndicatorData::GetTimeRemainingText() const {
    int totalSeconds = static_cast<int>(remainingTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    std::ostringstream oss;
    if (minutes > 0) {
        oss << minutes << "m " << seconds << "s";
    } else {
        oss << seconds << "s";
    }
    return oss.str();
}

std::vector<ProgressIndicatorData> GetProgressIndicators(const PlayerTechTree& tree, const TechTreeDef& treeDef) {
    std::vector<ProgressIndicatorData> indicators;

    // Current research
    const std::string& currentTechId = tree.GetCurrentResearch();
    if (!currentTechId.empty()) {
        const auto* node = treeDef.GetNode(currentTechId);
        const auto* progress = tree.GetProgress(currentTechId);

        if (node && progress) {
            ProgressIndicatorData data;
            data.techId = currentTechId;
            data.techName = node->GetName();
            data.techIcon = node->GetIcon();
            data.progress = progress->GetProgressPercent();
            data.remainingTime = progress->GetRemainingTime();
            data.totalTime = progress->totalTime;
            data.isCurrentResearch = true;
            data.isQueued = false;
            data.queuePosition = 0;
            indicators.push_back(data);
        }
    }

    // Queued research
    const auto& queue = tree.GetQueue();
    for (size_t i = 0; i < queue.size(); ++i) {
        const auto* node = treeDef.GetNode(queue[i]);
        if (!node) continue;

        ProgressIndicatorData data;
        data.techId = queue[i];
        data.techName = node->GetName();
        data.techIcon = node->GetIcon();
        data.progress = 0.0f;
        data.remainingTime = node->GetResearchTime();
        data.totalTime = node->GetResearchTime();
        data.isCurrentResearch = false;
        data.isQueued = true;
        data.queuePosition = static_cast<int>(i + 1);
        indicators.push_back(data);
    }

    return indicators;
}

} // namespace TechTree
} // namespace Vehement
