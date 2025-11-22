#include "TechTree.hpp"
#include <fstream>
#include <algorithm>
#include <queue>
#include <chrono>

namespace Vehement {
namespace TechTree {

// ============================================================================
// ResearchProgress
// ============================================================================

nlohmann::json ResearchProgress::ToJson() const {
    nlohmann::json j;
    j["tech_id"] = techId;
    j["status"] = ResearchStatusToString(status);
    j["progress"] = progress;
    j["total_time"] = totalTime;
    j["elapsed_time"] = elapsedTime;
    j["start_timestamp"] = startTimestamp;
    j["completed_timestamp"] = completedTimestamp;
    j["times_researched"] = timesResearched;
    j["times_lost"] = timesLost;
    return j;
}

ResearchProgress ResearchProgress::FromJson(const nlohmann::json& j) {
    ResearchProgress p;
    if (j.contains("tech_id")) p.techId = j["tech_id"].get<std::string>();
    if (j.contains("status")) {
        std::string status = j["status"].get<std::string>();
        if (status == "locked") p.status = ResearchStatus::Locked;
        else if (status == "available") p.status = ResearchStatus::Available;
        else if (status == "queued") p.status = ResearchStatus::Queued;
        else if (status == "in_progress") p.status = ResearchStatus::InProgress;
        else if (status == "completed") p.status = ResearchStatus::Completed;
        else if (status == "lost") p.status = ResearchStatus::Lost;
    }
    if (j.contains("progress")) p.progress = j["progress"].get<float>();
    if (j.contains("total_time")) p.totalTime = j["total_time"].get<float>();
    if (j.contains("elapsed_time")) p.elapsedTime = j["elapsed_time"].get<float>();
    if (j.contains("start_timestamp")) p.startTimestamp = j["start_timestamp"].get<int64_t>();
    if (j.contains("completed_timestamp")) p.completedTimestamp = j["completed_timestamp"].get<int64_t>();
    if (j.contains("times_researched")) p.timesResearched = j["times_researched"].get<int>();
    if (j.contains("times_lost")) p.timesLost = j["times_lost"].get<int>();
    return p;
}

// ============================================================================
// TreeConnection
// ============================================================================

nlohmann::json TreeConnection::ToJson() const {
    nlohmann::json j;
    j["from"] = fromTech;
    j["to"] = toTech;
    if (!isRequired) j["required"] = false;
    if (!label.empty()) j["label"] = label;
    return j;
}

TreeConnection TreeConnection::FromJson(const nlohmann::json& j) {
    TreeConnection c;
    if (j.contains("from")) c.fromTech = j["from"].get<std::string>();
    if (j.contains("to")) c.toTech = j["to"].get<std::string>();
    if (j.contains("required")) c.isRequired = j["required"].get<bool>();
    if (j.contains("label")) c.label = j["label"].get<std::string>();
    return c;
}

// ============================================================================
// TreeBranch
// ============================================================================

nlohmann::json TreeBranch::ToJson() const {
    nlohmann::json j;
    j["id"] = id;
    j["name"] = name;
    if (!description.empty()) j["description"] = description;
    if (!icon.empty()) j["icon"] = icon;
    j["category"] = TechCategoryToString(category);
    j["techs"] = techIds;
    j["display_order"] = displayOrder;
    return j;
}

TreeBranch TreeBranch::FromJson(const nlohmann::json& j) {
    TreeBranch b;
    if (j.contains("id")) b.id = j["id"].get<std::string>();
    if (j.contains("name")) b.name = j["name"].get<std::string>();
    if (j.contains("description")) b.description = j["description"].get<std::string>();
    if (j.contains("icon")) b.icon = j["icon"].get<std::string>();
    if (j.contains("category")) b.category = StringToTechCategory(j["category"].get<std::string>());
    if (j.contains("techs")) b.techIds = j["techs"].get<std::vector<std::string>>();
    if (j.contains("display_order")) b.displayOrder = j["display_order"].get<int>();
    return b;
}

// ============================================================================
// TechTreeDef
// ============================================================================

TechTreeDef::TechTreeDef(const std::string& id) : m_id(id) {}

void TechTreeDef::AddNode(const TechNode& node) {
    m_nodes[node.GetId()] = node;
    m_dependenciesDirty = true;
}

void TechTreeDef::RemoveNode(const std::string& techId) {
    m_nodes.erase(techId);
    m_dependenciesDirty = true;
}

const TechNode* TechTreeDef::GetNode(const std::string& techId) const {
    auto it = m_nodes.find(techId);
    return it != m_nodes.end() ? &it->second : nullptr;
}

TechNode* TechTreeDef::GetNodeMutable(const std::string& techId) {
    auto it = m_nodes.find(techId);
    return it != m_nodes.end() ? &it->second : nullptr;
}

std::vector<const TechNode*> TechTreeDef::GetNodesInTier(int tier) const {
    std::vector<const TechNode*> result;
    for (const auto& [id, node] : m_nodes) {
        if (node.GetTier() == tier) {
            result.push_back(&node);
        }
    }
    return result;
}

std::vector<const TechNode*> TechTreeDef::GetNodesInCategory(TechCategory category) const {
    std::vector<const TechNode*> result;
    for (const auto& [id, node] : m_nodes) {
        if (node.GetCategory() == category) {
            result.push_back(&node);
        }
    }
    return result;
}

std::vector<const TechNode*> TechTreeDef::GetNodesForAge(TechAge age) const {
    std::vector<const TechNode*> result;
    for (const auto& [id, node] : m_nodes) {
        if (node.GetAgeRequirement() == age) {
            result.push_back(&node);
        }
    }
    return result;
}

std::vector<const TechNode*> TechTreeDef::GetRootNodes() const {
    std::vector<const TechNode*> result;
    for (const auto& [id, node] : m_nodes) {
        if (node.GetPrerequisites().empty()) {
            result.push_back(&node);
        }
    }
    return result;
}

void TechTreeDef::AddConnection(const TreeConnection& connection) {
    m_connections.push_back(connection);
    m_dependenciesDirty = true;
}

void TechTreeDef::RemoveConnection(const std::string& fromTech, const std::string& toTech) {
    m_connections.erase(
        std::remove_if(m_connections.begin(), m_connections.end(),
            [&](const TreeConnection& c) {
                return c.fromTech == fromTech && c.toTech == toTech;
            }),
        m_connections.end());
    m_dependenciesDirty = true;
}

std::vector<std::string> TechTreeDef::GetDependents(const std::string& techId) const {
    RebuildDependencyCache();
    auto it = m_dependents.find(techId);
    return it != m_dependents.end() ? it->second : std::vector<std::string>{};
}

std::vector<std::string> TechTreeDef::GetDependencies(const std::string& techId) const {
    RebuildDependencyCache();
    auto it = m_dependencies.find(techId);
    return it != m_dependencies.end() ? it->second : std::vector<std::string>{};
}

void TechTreeDef::RebuildDependencyCache() const {
    if (!m_dependenciesDirty) return;

    m_dependents.clear();
    m_dependencies.clear();

    // Build from node prerequisites
    for (const auto& [id, node] : m_nodes) {
        for (const auto& prereq : node.GetPrerequisites()) {
            m_dependents[prereq].push_back(id);
            m_dependencies[id].push_back(prereq);
        }
    }

    // Build from explicit connections
    for (const auto& conn : m_connections) {
        if (conn.isRequired) {
            m_dependents[conn.fromTech].push_back(conn.toTech);
            m_dependencies[conn.toTech].push_back(conn.fromTech);
        }
    }

    m_dependenciesDirty = false;
}

void TechTreeDef::AddBranch(const TreeBranch& branch) {
    m_branches.push_back(branch);
}

const TreeBranch* TechTreeDef::GetBranch(const std::string& branchId) const {
    for (const auto& branch : m_branches) {
        if (branch.id == branchId) {
            return &branch;
        }
    }
    return nullptr;
}

std::vector<std::string> TechTreeDef::Validate() const {
    std::vector<std::string> errors;

    // Check for empty tree
    if (m_nodes.empty()) {
        errors.push_back("Tech tree '" + m_id + "' has no nodes");
    }

    // Validate each node
    for (const auto& [id, node] : m_nodes) {
        auto nodeErrors = node.Validate();
        errors.insert(errors.end(), nodeErrors.begin(), nodeErrors.end());

        // Check prerequisites exist
        for (const auto& prereq : node.GetPrerequisites()) {
            if (m_nodes.find(prereq) == m_nodes.end()) {
                errors.push_back("Node '" + id + "' references unknown prerequisite '" + prereq + "'");
            }
        }
    }

    // Check connections
    for (const auto& conn : m_connections) {
        if (m_nodes.find(conn.fromTech) == m_nodes.end()) {
            errors.push_back("Connection references unknown tech '" + conn.fromTech + "'");
        }
        if (m_nodes.find(conn.toTech) == m_nodes.end()) {
            errors.push_back("Connection references unknown tech '" + conn.toTech + "'");
        }
    }

    // Check for circular dependencies
    if (HasCircularDependencies()) {
        errors.push_back("Tech tree '" + m_id + "' has circular dependencies");
    }

    return errors;
}

bool TechTreeDef::HasCircularDependencies() const {
    // Use DFS to detect cycles
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> inStack;

    std::function<bool(const std::string&)> hasCycle = [&](const std::string& techId) -> bool {
        if (inStack.count(techId)) return true;
        if (visited.count(techId)) return false;

        visited.insert(techId);
        inStack.insert(techId);

        auto node = GetNode(techId);
        if (node) {
            for (const auto& prereq : node->GetPrerequisites()) {
                if (hasCycle(prereq)) return true;
            }
        }

        inStack.erase(techId);
        return false;
    };

    for (const auto& [id, node] : m_nodes) {
        if (hasCycle(id)) return true;
    }

    return false;
}

std::vector<std::string> TechTreeDef::GetUnreachableNodes() const {
    // BFS from root nodes
    std::unordered_set<std::string> reachable;
    std::queue<std::string> queue;

    // Start from root nodes
    for (const auto* root : GetRootNodes()) {
        queue.push(root->GetId());
        reachable.insert(root->GetId());
    }

    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();

        for (const auto& dependent : GetDependents(current)) {
            if (reachable.find(dependent) == reachable.end()) {
                reachable.insert(dependent);
                queue.push(dependent);
            }
        }
    }

    // Find unreachable
    std::vector<std::string> unreachable;
    for (const auto& [id, node] : m_nodes) {
        if (reachable.find(id) == reachable.end()) {
            unreachable.push_back(id);
        }
    }

    return unreachable;
}

nlohmann::json TechTreeDef::ToJson() const {
    nlohmann::json j;

    j["id"] = m_id;
    j["name"] = m_name;
    if (!m_description.empty()) j["description"] = m_description;
    if (!m_icon.empty()) j["icon"] = m_icon;
    if (!m_culture.empty()) j["culture"] = m_culture;
    if (m_isUniversal) j["universal"] = true;

    // Nodes
    nlohmann::json nodesArray = nlohmann::json::array();
    for (const auto& [id, node] : m_nodes) {
        nodesArray.push_back(node.ToJson());
    }
    j["nodes"] = nodesArray;

    // Connections
    if (!m_connections.empty()) {
        nlohmann::json connsArray = nlohmann::json::array();
        for (const auto& conn : m_connections) {
            connsArray.push_back(conn.ToJson());
        }
        j["connections"] = connsArray;
    }

    // Branches
    if (!m_branches.empty()) {
        nlohmann::json branchArray = nlohmann::json::array();
        for (const auto& branch : m_branches) {
            branchArray.push_back(branch.ToJson());
        }
        j["branches"] = branchArray;
    }

    return j;
}

TechTreeDef TechTreeDef::FromJson(const nlohmann::json& j) {
    TechTreeDef tree;

    if (j.contains("id")) tree.m_id = j["id"].get<std::string>();
    if (j.contains("name")) tree.m_name = j["name"].get<std::string>();
    if (j.contains("description")) tree.m_description = j["description"].get<std::string>();
    if (j.contains("icon")) tree.m_icon = j["icon"].get<std::string>();
    if (j.contains("culture")) tree.m_culture = j["culture"].get<std::string>();
    if (j.contains("universal")) tree.m_isUniversal = j["universal"].get<bool>();

    // Nodes
    if (j.contains("nodes")) {
        for (const auto& nodeJson : j["nodes"]) {
            TechNode node = TechNode::FromJson(nodeJson);
            tree.m_nodes[node.GetId()] = node;
        }
    }

    // Connections
    if (j.contains("connections")) {
        for (const auto& connJson : j["connections"]) {
            tree.m_connections.push_back(TreeConnection::FromJson(connJson));
        }
    }

    // Branches
    if (j.contains("branches")) {
        for (const auto& branchJson : j["branches"]) {
            tree.m_branches.push_back(TreeBranch::FromJson(branchJson));
        }
    }

    return tree;
}

bool TechTreeDef::LoadFromFile(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) return false;

        nlohmann::json j;
        file >> j;
        *this = FromJson(j);
        return true;
    } catch (...) {
        return false;
    }
}

bool TechTreeDef::SaveToFile(const std::string& filePath) const {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) return false;

        file << ToJson().dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// PlayerTechTree
// ============================================================================

PlayerTechTree::PlayerTechTree(const TechTreeDef* treeDef, const std::string& playerId) {
    Initialize(treeDef, playerId);
}

void PlayerTechTree::Initialize(const TechTreeDef* treeDef, const std::string& playerId) {
    m_treeDef = treeDef;
    m_playerId = playerId;
    Reset();
}

void PlayerTechTree::Reset() {
    m_completedTechs.clear();
    m_techProgress.clear();
    m_currentResearch.clear();
    m_researchQueue.clear();
    m_totalTechsResearched = 0;
    m_totalTechsLost = 0;
    m_totalResearchTime = 0.0f;
}

ResearchStatus PlayerTechTree::GetTechStatus(const std::string& techId) const {
    auto it = m_techProgress.find(techId);
    if (it != m_techProgress.end()) {
        return it->second.status;
    }

    // Not in progress map, check if prerequisites are met
    if (!m_treeDef) return ResearchStatus::Locked;

    const auto* node = m_treeDef->GetNode(techId);
    if (!node) return ResearchStatus::Locked;

    // Check all prerequisites
    for (const auto& prereq : node->GetPrerequisites()) {
        if (m_completedTechs.find(prereq) == m_completedTechs.end()) {
            return ResearchStatus::Locked;
        }
    }

    return ResearchStatus::Available;
}

bool PlayerTechTree::HasTech(const std::string& techId) const {
    return m_completedTechs.find(techId) != m_completedTechs.end();
}

bool PlayerTechTree::CanResearch(const std::string& techId, const IRequirementContext& context) const {
    if (!m_treeDef) return false;

    const auto* node = m_treeDef->GetNode(techId);
    if (!node) return false;

    // Already completed?
    if (HasTech(techId) && !node->IsRepeatable()) {
        return false;
    }

    // Check requirements
    auto results = RequirementChecker::CheckTechRequirements(*node, context);
    return results.allMet;
}

const ResearchProgress* PlayerTechTree::GetProgress(const std::string& techId) const {
    auto it = m_techProgress.find(techId);
    return it != m_techProgress.end() ? &it->second : nullptr;
}

float PlayerTechTree::GetCurrentProgress() const {
    if (m_currentResearch.empty()) return 0.0f;
    auto it = m_techProgress.find(m_currentResearch);
    return it != m_techProgress.end() ? it->second.GetProgressPercent() : 0.0f;
}

float PlayerTechTree::GetCurrentRemainingTime() const {
    if (m_currentResearch.empty()) return 0.0f;
    auto it = m_techProgress.find(m_currentResearch);
    return it != m_techProgress.end() ? it->second.GetRemainingTime() : 0.0f;
}

bool PlayerTechTree::StartResearch(const std::string& techId, const IRequirementContext& context) {
    if (!CanResearch(techId, context)) return false;

    const auto* node = m_treeDef->GetNode(techId);
    if (!node) return false;

    // Create progress entry
    ResearchProgress progress;
    progress.techId = techId;
    progress.status = ResearchStatus::InProgress;
    progress.totalTime = node->GetResearchTime();
    progress.elapsedTime = 0.0f;
    progress.startTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    m_techProgress[techId] = progress;
    m_currentResearch = techId;

    EmitEvent(ResearchEventType::ResearchStarted, techId);
    return true;
}

void PlayerTechTree::UpdateResearch(float deltaTime, float speedMultiplier) {
    if (m_currentResearch.empty()) {
        ProcessQueue();
        return;
    }

    auto it = m_techProgress.find(m_currentResearch);
    if (it == m_techProgress.end()) {
        m_currentResearch.clear();
        ProcessQueue();
        return;
    }

    ResearchProgress& progress = it->second;
    progress.elapsedTime += deltaTime * speedMultiplier;
    progress.progress = progress.GetProgressPercent();

    m_totalResearchTime += deltaTime * speedMultiplier;

    EmitEvent(ResearchEventType::ResearchProgress, m_currentResearch, progress.progress);

    if (progress.elapsedTime >= progress.totalTime) {
        CompleteCurrentResearch();
    }
}

void PlayerTechTree::CompleteCurrentResearch() {
    if (m_currentResearch.empty()) return;

    OnResearchComplete(m_currentResearch);
    ProcessQueue();
}

std::map<std::string, int> PlayerTechTree::CancelResearch(float refundPercent) {
    std::map<std::string, int> refund;

    if (m_currentResearch.empty()) return refund;

    const auto* node = m_treeDef ? m_treeDef->GetNode(m_currentResearch) : nullptr;
    if (node) {
        float progressRatio = 1.0f - GetCurrentProgress();
        for (const auto& [resource, cost] : node->GetCost().resources) {
            int refundAmount = static_cast<int>(cost * refundPercent * progressRatio);
            if (refundAmount > 0) {
                refund[resource] = refundAmount;
            }
        }
    }

    EmitEvent(ResearchEventType::ResearchCancelled, m_currentResearch);

    m_techProgress.erase(m_currentResearch);
    m_currentResearch.clear();

    return refund;
}

void PlayerTechTree::GrantTech(const std::string& techId) {
    if (HasTech(techId)) return;

    ResearchProgress progress;
    progress.techId = techId;
    progress.status = ResearchStatus::Completed;
    progress.progress = 1.0f;
    progress.timesResearched = 1;
    progress.completedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    m_techProgress[techId] = progress;
    m_completedTechs.insert(techId);
    m_totalTechsResearched++;

    EmitEvent(ResearchEventType::TechUnlocked, techId);
}

bool PlayerTechTree::LoseTech(const std::string& techId) {
    if (!HasTech(techId)) return false;

    const auto* node = m_treeDef ? m_treeDef->GetNode(techId) : nullptr;
    if (node && !node->CanBeLost()) return false;

    m_completedTechs.erase(techId);

    auto it = m_techProgress.find(techId);
    if (it != m_techProgress.end()) {
        it->second.status = ResearchStatus::Lost;
        it->second.timesLost++;
    }

    m_totalTechsLost++;

    EmitEvent(ResearchEventType::TechLost, techId);
    return true;
}

bool PlayerTechTree::QueueResearch(const std::string& techId) {
    if (IsQueued(techId)) return false;

    m_researchQueue.push_back(techId);

    auto it = m_techProgress.find(techId);
    if (it != m_techProgress.end()) {
        it->second.status = ResearchStatus::Queued;
    } else {
        ResearchProgress progress;
        progress.techId = techId;
        progress.status = ResearchStatus::Queued;
        if (m_treeDef) {
            if (const auto* node = m_treeDef->GetNode(techId)) {
                progress.totalTime = node->GetResearchTime();
            }
        }
        m_techProgress[techId] = progress;
    }

    EmitEvent(ResearchEventType::ResearchQueued, techId);
    return true;
}

bool PlayerTechTree::DequeueResearch(const std::string& techId) {
    auto it = std::find(m_researchQueue.begin(), m_researchQueue.end(), techId);
    if (it == m_researchQueue.end()) return false;

    m_researchQueue.erase(it);

    auto progIt = m_techProgress.find(techId);
    if (progIt != m_techProgress.end() && progIt->second.status == ResearchStatus::Queued) {
        m_techProgress.erase(progIt);
    }

    EmitEvent(ResearchEventType::ResearchDequeued, techId);
    return true;
}

void PlayerTechTree::ClearQueue() {
    for (const auto& techId : m_researchQueue) {
        EmitEvent(ResearchEventType::ResearchDequeued, techId);
    }
    m_researchQueue.clear();
}

bool PlayerTechTree::IsQueued(const std::string& techId) const {
    return std::find(m_researchQueue.begin(), m_researchQueue.end(), techId) != m_researchQueue.end();
}

nlohmann::json PlayerTechTree::ToJson() const {
    nlohmann::json j;

    j["player_id"] = m_playerId;
    j["tree_id"] = m_treeDef ? m_treeDef->GetId() : "";
    j["current_research"] = m_currentResearch;

    j["completed_techs"] = std::vector<std::string>(
        m_completedTechs.begin(), m_completedTechs.end());

    nlohmann::json progressArray = nlohmann::json::array();
    for (const auto& [techId, progress] : m_techProgress) {
        progressArray.push_back(progress.ToJson());
    }
    j["tech_progress"] = progressArray;

    j["research_queue"] = m_researchQueue;

    j["stats"] = {
        {"total_researched", m_totalTechsResearched},
        {"total_lost", m_totalTechsLost},
        {"total_time", m_totalResearchTime}
    };

    return j;
}

void PlayerTechTree::FromJson(const nlohmann::json& j) {
    if (j.contains("player_id")) m_playerId = j["player_id"].get<std::string>();
    if (j.contains("current_research")) m_currentResearch = j["current_research"].get<std::string>();

    if (j.contains("completed_techs")) {
        auto techs = j["completed_techs"].get<std::vector<std::string>>();
        m_completedTechs = std::unordered_set<std::string>(techs.begin(), techs.end());
    }

    if (j.contains("tech_progress")) {
        for (const auto& progJson : j["tech_progress"]) {
            auto progress = ResearchProgress::FromJson(progJson);
            m_techProgress[progress.techId] = progress;
        }
    }

    if (j.contains("research_queue")) {
        m_researchQueue = j["research_queue"].get<std::vector<std::string>>();
    }

    if (j.contains("stats")) {
        const auto& stats = j["stats"];
        if (stats.contains("total_researched")) m_totalTechsResearched = stats["total_researched"].get<int>();
        if (stats.contains("total_lost")) m_totalTechsLost = stats["total_lost"].get<int>();
        if (stats.contains("total_time")) m_totalResearchTime = stats["total_time"].get<float>();
    }
}

void PlayerTechTree::OnResearchComplete(const std::string& techId) {
    auto it = m_techProgress.find(techId);
    if (it != m_techProgress.end()) {
        it->second.status = ResearchStatus::Completed;
        it->second.progress = 1.0f;
        it->second.timesResearched++;
        it->second.completedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    m_completedTechs.insert(techId);
    m_totalTechsResearched++;
    m_currentResearch.clear();

    EmitEvent(ResearchEventType::ResearchCompleted, techId);
    EmitEvent(ResearchEventType::TechUnlocked, techId);
}

void PlayerTechTree::ProcessQueue() {
    // Remove completed techs from queue
    m_researchQueue.erase(
        std::remove_if(m_researchQueue.begin(), m_researchQueue.end(),
            [this](const std::string& techId) { return HasTech(techId); }),
        m_researchQueue.end());

    // Start next research if not currently researching
    if (m_currentResearch.empty() && !m_researchQueue.empty()) {
        // For now, just mark as in progress - actual start needs context
        std::string nextTech = m_researchQueue.front();
        m_researchQueue.erase(m_researchQueue.begin());

        auto it = m_techProgress.find(nextTech);
        if (it != m_techProgress.end()) {
            it->second.status = ResearchStatus::InProgress;
            it->second.startTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }
        m_currentResearch = nextTech;

        EmitEvent(ResearchEventType::ResearchStarted, nextTech);
    }
}

void PlayerTechTree::EmitEvent(ResearchEventType type, const std::string& techId,
                                float progress, const std::string& message) {
    if (!m_onResearchEvent) return;

    ResearchEvent event;
    event.type = type;
    event.techId = techId;
    event.treeId = m_treeDef ? m_treeDef->GetId() : "";
    event.progress = progress;
    event.message = message;

    m_onResearchEvent(event);
}

// ============================================================================
// ResearchQueueManager
// ============================================================================

std::vector<std::string> ResearchQueueManager::AutoQueuePrerequisites(
    const std::string& targetTech,
    PlayerTechTree& tree,
    const IRequirementContext& context) {

    std::vector<std::string> added;
    auto path = GetResearchPath(targetTech, tree);

    for (const auto& techId : path) {
        if (!tree.HasTech(techId) && !tree.IsQueued(techId)) {
            if (tree.QueueResearch(techId)) {
                added.push_back(techId);
            }
        }
    }

    return added;
}

std::vector<std::string> ResearchQueueManager::GetResearchPath(
    const std::string& targetTech,
    const PlayerTechTree& tree) {

    std::vector<std::string> path;
    const auto* treeDef = tree.GetTreeDef();
    if (!treeDef) return path;

    // Build dependency order using DFS
    std::unordered_set<std::string> visited;
    std::function<void(const std::string&)> buildPath = [&](const std::string& techId) {
        if (visited.count(techId) || tree.HasTech(techId)) return;
        visited.insert(techId);

        const auto* node = treeDef->GetNode(techId);
        if (!node) return;

        // Visit prerequisites first
        for (const auto& prereq : node->GetPrerequisites()) {
            buildPath(prereq);
        }

        path.push_back(techId);
    };

    buildPath(targetTech);
    return path;
}

float ResearchQueueManager::EstimateTotalTime(
    const std::string& targetTech,
    const PlayerTechTree& tree,
    float speedMultiplier) {

    auto path = GetResearchPath(targetTech, tree);
    const auto* treeDef = tree.GetTreeDef();

    float totalTime = 0.0f;

    for (const auto& techId : path) {
        const auto* node = treeDef ? treeDef->GetNode(techId) : nullptr;
        if (node) {
            totalTime += node->GetResearchTime() / speedMultiplier;
        }
    }

    return totalTime;
}

} // namespace TechTree
} // namespace Vehement
