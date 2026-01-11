#include "BuildingUI.hpp"
#include <algorithm>
#include <random>

namespace Nova {
namespace Buildings {

// =============================================================================
// TechTreeNode Implementation
// =============================================================================

TechTreeNode::TechTreeNode(const std::string& id, const std::string& name, UpgradeType type)
    : m_id(id), m_name(name), m_type(type) {}

bool TechTreeNode::CanResearch(int buildingLevel, const std::vector<std::string>& completedResearch) const {
    // Check building level requirement
    if (buildingLevel < m_requiredBuildingLevel) {
        return false;
    }

    // Check all prerequisites are researched
    for (const auto& prereq : m_prerequisites) {
        if (std::find(completedResearch.begin(), completedResearch.end(), prereq) == completedResearch.end()) {
            return false;
        }
    }

    return true;
}

nlohmann::json TechTreeNode::Serialize() const {
    nlohmann::json json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["description"] = m_description;
    json["type"] = static_cast<int>(m_type);
    json["iconPath"] = m_iconPath;
    json["treePosition"] = {m_treePosition.x, m_treePosition.y};
    json["requiredBuildingLevel"] = m_requiredBuildingLevel;
    json["prerequisites"] = m_prerequisites;
    json["cost"] = m_cost;
    json["researchTime"] = m_researchTime;

    nlohmann::json effectsJson = nlohmann::json::array();
    for (const auto& effect : m_effects) {
        nlohmann::json effectJson;
        effectJson["target"] = effect.target;
        effectJson["effectType"] = effect.effectType;
        effectJson["value"] = effect.value;
        effectJson["data"] = effect.data;
        effectsJson.push_back(effectJson);
    }
    json["effects"] = effectsJson;

    return json;
}

TechTreeNodePtr TechTreeNode::Deserialize(const nlohmann::json& json) {
    auto node = std::make_shared<TechTreeNode>();
    node->m_id = json.value("id", "");
    node->m_name = json.value("name", "");
    node->m_description = json.value("description", "");
    node->m_type = static_cast<UpgradeType>(json.value("type", 0));
    node->m_iconPath = json.value("iconPath", "");

    if (json.contains("treePosition")) {
        auto pos = json["treePosition"];
        node->m_treePosition = glm::vec2(pos[0], pos[1]);
    }

    node->m_requiredBuildingLevel = json.value("requiredBuildingLevel", 1);
    node->m_prerequisites = json.value("prerequisites", std::vector<std::string>{});
    node->m_cost = json.value("cost", std::unordered_map<std::string, float>{});
    node->m_researchTime = json.value("researchTime", 10.0f);

    if (json.contains("effects")) {
        for (const auto& effectJson : json["effects"]) {
            Effect effect;
            effect.target = effectJson.value("target", "");
            effect.effectType = effectJson.value("effectType", "");
            effect.value = effectJson.value("value", 1.0f);
            effect.data = effectJson.value("data", nlohmann::json{});
            node->m_effects.push_back(effect);
        }
    }

    return node;
}

// =============================================================================
// TechTree Implementation
// =============================================================================

void TechTree::AddNode(TechTreeNodePtr node) {
    if (node) {
        m_nodes[node->GetId()] = node;
    }
}

TechTreeNodePtr TechTree::GetNode(const std::string& id) const {
    auto it = m_nodes.find(id);
    return it != m_nodes.end() ? it->second : nullptr;
}

std::vector<TechTreeNodePtr> TechTree::GetAvailableNodes(int buildingLevel,
                                                          const std::vector<std::string>& completedResearch) const {
    std::vector<TechTreeNodePtr> available;
    for (const auto& [id, node] : m_nodes) {
        // Skip already researched
        if (std::find(completedResearch.begin(), completedResearch.end(), id) != completedResearch.end()) {
            continue;
        }

        // Check if can research
        if (node->CanResearch(buildingLevel, completedResearch)) {
            available.push_back(node);
        }
    }
    return available;
}

std::vector<TechTreeNodePtr> TechTree::GetAllNodes() const {
    std::vector<TechTreeNodePtr> nodes;
    for (const auto& [id, node] : m_nodes) {
        nodes.push_back(node);
    }
    return nodes;
}

bool TechTree::IsResearched(const std::string& techId, const std::vector<std::string>& completedResearch) const {
    return std::find(completedResearch.begin(), completedResearch.end(), techId) != completedResearch.end();
}

nlohmann::json TechTree::Serialize() const {
    nlohmann::json json;
    nlohmann::json nodesJson = nlohmann::json::array();
    for (const auto& [id, node] : m_nodes) {
        nodesJson.push_back(node->Serialize());
    }
    json["nodes"] = nodesJson;
    return json;
}

std::shared_ptr<TechTree> TechTree::Deserialize(const nlohmann::json& json) {
    auto tree = std::make_shared<TechTree>();
    if (json.contains("nodes")) {
        for (const auto& nodeJson : json["nodes"]) {
            auto node = TechTreeNode::Deserialize(nodeJson);
            tree->AddNode(node);
        }
    }
    return tree;
}

// =============================================================================
// ProductionItem Implementation
// =============================================================================

nlohmann::json ProductionItem::Serialize() const {
    nlohmann::json json;
    json["unitId"] = unitId;
    json["unitName"] = unitName;
    json["iconPath"] = iconPath;
    json["productionTime"] = productionTime;
    json["elapsedTime"] = elapsedTime;
    json["cost"] = cost;
    json["priority"] = priority;
    json["paused"] = paused;
    return json;
}

ProductionItem ProductionItem::Deserialize(const nlohmann::json& json) {
    ProductionItem item;
    item.unitId = json.value("unitId", "");
    item.unitName = json.value("unitName", "");
    item.iconPath = json.value("iconPath", "");
    item.productionTime = json.value("productionTime", 10.0f);
    item.elapsedTime = json.value("elapsedTime", 0.0f);
    item.cost = json.value("cost", std::unordered_map<std::string, float>{});
    item.priority = json.value("priority", 0);
    item.paused = json.value("paused", false);
    return item;
}

// =============================================================================
// ProductionQueue Implementation
// =============================================================================

void ProductionQueue::AddToQueue(const ProductionItem& item) {
    m_queue.push_back(item);
    SortByPriority();
}

void ProductionQueue::RemoveFromQueue(size_t index) {
    if (index < m_queue.size()) {
        m_queue.erase(m_queue.begin() + index);
    }
}

void ProductionQueue::ClearQueue() {
    m_queue.clear();
}

void ProductionQueue::PauseProduction(bool pause) {
    m_paused = pause;
}

void ProductionQueue::SetPriority(size_t index, int priority) {
    if (index < m_queue.size()) {
        m_queue[index].priority = priority;
        SortByPriority();
    }
}

void ProductionQueue::Update(float deltaTime) {
    if (m_paused || m_queue.empty()) {
        return;
    }

    auto& current = m_queue[0];
    if (current.paused) {
        return;
    }

    current.elapsedTime += deltaTime * m_speedMultiplier;

    if (current.IsComplete()) {
        // Notify completion
        if (m_onComplete) {
            m_onComplete(current);
        }

        // Remove from queue
        m_queue.erase(m_queue.begin());
    }
}

ProductionItem* ProductionQueue::GetCurrentItem() {
    return m_queue.empty() ? nullptr : &m_queue[0];
}

void ProductionQueue::SortByPriority() {
    std::stable_sort(m_queue.begin(), m_queue.end(),
                    [](const ProductionItem& a, const ProductionItem& b) {
                        return a.priority > b.priority;
                    });
}

nlohmann::json ProductionQueue::Serialize() const {
    nlohmann::json json;
    nlohmann::json queueJson = nlohmann::json::array();
    for (const auto& item : m_queue) {
        queueJson.push_back(item.Serialize());
    }
    json["queue"] = queueJson;
    json["speedMultiplier"] = m_speedMultiplier;
    json["paused"] = m_paused;
    return json;
}

std::shared_ptr<ProductionQueue> ProductionQueue::Deserialize(const nlohmann::json& json) {
    auto queue = std::make_shared<ProductionQueue>();
    if (json.contains("queue")) {
        for (const auto& itemJson : json["queue"]) {
            queue->m_queue.push_back(ProductionItem::Deserialize(itemJson));
        }
    }
    queue->m_speedMultiplier = json.value("speedMultiplier", 1.0f);
    queue->m_paused = json.value("paused", false);
    return queue;
}

// =============================================================================
// UnitDefinition Implementation
// =============================================================================

bool UnitDefinition::CanProduce(int buildingLevel, const std::vector<std::string>& completedResearch) const {
    // Check building level
    if (buildingLevel < m_requiredLevel) {
        return false;
    }

    // Check required techs
    for (const auto& tech : m_requiredTechs) {
        if (std::find(completedResearch.begin(), completedResearch.end(), tech) == completedResearch.end()) {
            return false;
        }
    }

    return true;
}

ProductionItem UnitDefinition::CreateProductionItem() const {
    ProductionItem item;
    item.unitId = m_id;
    item.unitName = m_name;
    item.iconPath = m_iconPath;
    item.productionTime = m_productionTime;
    item.cost = m_cost;
    return item;
}

nlohmann::json UnitDefinition::Serialize() const {
    nlohmann::json json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["description"] = m_description;
    json["category"] = m_category;
    json["iconPath"] = m_iconPath;
    json["modelPath"] = m_modelPath;
    json["requiredLevel"] = m_requiredLevel;
    json["requiredTechs"] = m_requiredTechs;
    json["productionTime"] = m_productionTime;
    json["cost"] = m_cost;
    json["populationCost"] = m_populationCost;
    json["stats"] = m_stats;
    return json;
}

std::shared_ptr<UnitDefinition> UnitDefinition::Deserialize(const nlohmann::json& json) {
    auto unit = std::make_shared<UnitDefinition>();
    unit->m_id = json.value("id", "");
    unit->m_name = json.value("name", "");
    unit->m_description = json.value("description", "");
    unit->m_category = json.value("category", "");
    unit->m_iconPath = json.value("iconPath", "");
    unit->m_modelPath = json.value("modelPath", "");
    unit->m_requiredLevel = json.value("requiredLevel", 1);
    unit->m_requiredTechs = json.value("requiredTechs", std::vector<std::string>{});
    unit->m_productionTime = json.value("productionTime", 10.0f);
    unit->m_cost = json.value("cost", std::unordered_map<std::string, float>{});
    unit->m_populationCost = json.value("populationCost", 1);
    unit->m_stats = json.value("stats", nlohmann::json{});
    return unit;
}

// =============================================================================
// BuildingUIState Implementation
// =============================================================================

void BuildingUIState::SetActiveResearch(const std::string& techId, float duration, float currentTime) {
    ActiveResearch research;
    research.techId = techId;
    research.startTime = currentTime;
    research.duration = duration;
    m_activeResearch = research;
}

void BuildingUIState::AddNotification(const std::string& message, float timestamp, const glm::vec4& color) {
    Notification notif;
    notif.message = message;
    notif.timestamp = timestamp;
    notif.color = color;
    m_notifications.push_back(notif);
}

std::vector<BuildingUIState::Notification> BuildingUIState::GetActiveNotifications(float currentTime) const {
    std::vector<Notification> active;
    for (const auto& notif : m_notifications) {
        if (currentTime - notif.timestamp < notif.duration) {
            active.push_back(notif);
        }
    }
    return active;
}

nlohmann::json BuildingUIState::Serialize() const {
    nlohmann::json json;
    json["selected"] = m_selected;
    json["activeTab"] = static_cast<int>(m_activeTab);
    json["hoveredElement"] = m_hoveredElement;

    if (m_activeResearch) {
        nlohmann::json researchJson;
        researchJson["techId"] = m_activeResearch->techId;
        researchJson["startTime"] = m_activeResearch->startTime;
        researchJson["duration"] = m_activeResearch->duration;
        json["activeResearch"] = researchJson;
    }

    nlohmann::json notifsJson = nlohmann::json::array();
    for (const auto& notif : m_notifications) {
        nlohmann::json notifJson;
        notifJson["message"] = notif.message;
        notifJson["timestamp"] = notif.timestamp;
        notifJson["duration"] = notif.duration;
        notifJson["color"] = {notif.color.r, notif.color.g, notif.color.b, notif.color.a};
        notifsJson.push_back(notifJson);
    }
    json["notifications"] = notifsJson;

    return json;
}

std::shared_ptr<BuildingUIState> BuildingUIState::Deserialize(const nlohmann::json& json) {
    auto state = std::make_shared<BuildingUIState>();
    state->m_selected = json.value("selected", false);
    state->m_activeTab = static_cast<UITab>(json.value("activeTab", 0));
    state->m_hoveredElement = json.value("hoveredElement", "");

    if (json.contains("activeResearch")) {
        ActiveResearch research;
        research.techId = json["activeResearch"].value("techId", "");
        research.startTime = json["activeResearch"].value("startTime", 0.0f);
        research.duration = json["activeResearch"].value("duration", 0.0f);
        state->m_activeResearch = research;
    }

    if (json.contains("notifications")) {
        for (const auto& notifJson : json["notifications"]) {
            Notification notif;
            notif.message = notifJson.value("message", "");
            notif.timestamp = notifJson.value("timestamp", 0.0f);
            notif.duration = notifJson.value("duration", 5.0f);
            if (notifJson.contains("color")) {
                auto c = notifJson["color"];
                notif.color = glm::vec4(c[0], c[1], c[2], c[3]);
            }
            state->m_notifications.push_back(notif);
        }
    }

    return state;
}

// =============================================================================
// BuildingUIRenderer Implementation
// =============================================================================

void BuildingUIRenderer::RenderBuildingUI(BuildingInstancePtr building, BuildingUIState& uiState,
                                         float currentTime, float deltaTime) {
    // This would use ImGui to render the UI
    // For now, just stub implementation showing the structure
}

void BuildingUIRenderer::RenderOverviewPanel(BuildingInstancePtr building) {
    // Render building name, level, stats, etc.
}

void BuildingUIRenderer::RenderProductionPanel(BuildingInstancePtr building, float currentTime) {
    // Render production queue and unit buttons
}

void BuildingUIRenderer::RenderTechTreePanel(BuildingInstancePtr building, BuildingUIState& uiState,
                                            float currentTime) {
    // Render tech tree nodes and connections
}

void BuildingUIRenderer::RenderComponentsPanel(BuildingInstancePtr building) {
    // Render component placement interface
}

void BuildingUIRenderer::RenderStatsPanel(BuildingInstancePtr building) {
    // Render detailed statistics
}

void BuildingUIRenderer::RenderProductionQueue(ProductionQueue& queue, float currentTime) {
    // Render queue items with progress bars
}

void BuildingUIRenderer::RenderTechNode(TechTreeNodePtr node, bool available, bool researched,
                                       const glm::vec2& position) {
    // Render individual tech tree node
}

void BuildingUIRenderer::RenderUnitButton(const UnitDefinition& unit, bool available) {
    // Render clickable unit production button
}

void BuildingUIRenderer::RenderProgressBar(float progress, const glm::vec2& size,
                                          const std::string& label) {
    // Render progress bar
}

} // namespace Buildings
} // namespace Nova
