#include "TalentTree.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace Vehement {
namespace RTS {
namespace Talent {

// TalentBranch
nlohmann::json TalentBranch::ToJson() const {
    return {{"id", id}, {"name", name}, {"description", description},
            {"category", TalentCategoryToString(category)}, {"iconPath", iconPath},
            {"colorHex", colorHex}, {"nodeIds", nodeIds}, {"keystoneId", keystoneId}};
}

TalentBranch TalentBranch::FromJson(const nlohmann::json& j) {
    TalentBranch b;
    if (j.contains("id")) b.id = j["id"].get<std::string>();
    if (j.contains("name")) b.name = j["name"].get<std::string>();
    if (j.contains("description")) b.description = j["description"].get<std::string>();
    if (j.contains("category")) {
        std::string c = j["category"].get<std::string>();
        if (c == "Military") b.category = TalentCategory::Military;
        else if (c == "Economy") b.category = TalentCategory::Economy;
        else if (c == "Magic") b.category = TalentCategory::Magic;
        else if (c == "Technology") b.category = TalentCategory::Technology;
        else if (c == "Special") b.category = TalentCategory::Special;
    }
    if (j.contains("iconPath")) b.iconPath = j["iconPath"].get<std::string>();
    if (j.contains("colorHex")) b.colorHex = j["colorHex"].get<std::string>();
    if (j.contains("nodeIds")) b.nodeIds = j["nodeIds"].get<std::vector<std::string>>();
    if (j.contains("keystoneId")) b.keystoneId = j["keystoneId"].get<std::string>();
    return b;
}

// AgeGate
nlohmann::json AgeGate::ToJson() const {
    return {{"age", age}, {"unlockedNodes", unlockedNodes}, {"bonusTalentPoints", bonusTalentPoints}};
}

AgeGate AgeGate::FromJson(const nlohmann::json& j) {
    AgeGate g;
    if (j.contains("age")) g.age = j["age"].get<int>();
    if (j.contains("unlockedNodes")) g.unlockedNodes = j["unlockedNodes"].get<std::vector<std::string>>();
    if (j.contains("bonusTalentPoints")) g.bonusTalentPoints = j["bonusTalentPoints"].get<int>();
    return g;
}

// TalentProgress
nlohmann::json TalentProgress::ToJson() const {
    return {
        {"unlockedTalents", std::vector<std::string>(unlockedTalents.begin(), unlockedTalents.end())},
        {"talentRanks", talentRanks},
        {"totalPointsSpent", totalPointsSpent},
        {"availablePoints", availablePoints}
    };
}

TalentProgress TalentProgress::FromJson(const nlohmann::json& j) {
    TalentProgress p;
    if (j.contains("unlockedTalents")) {
        auto talents = j["unlockedTalents"].get<std::vector<std::string>>();
        p.unlockedTalents = std::set<std::string>(talents.begin(), talents.end());
    }
    if (j.contains("talentRanks")) p.talentRanks = j["talentRanks"].get<std::map<std::string, int>>();
    if (j.contains("totalPointsSpent")) p.totalPointsSpent = j["totalPointsSpent"].get<int>();
    if (j.contains("availablePoints")) p.availablePoints = j["availablePoints"].get<int>();
    return p;
}

// TalentTreeDefinition
nlohmann::json TalentTreeDefinition::ToJson() const {
    nlohmann::json branchesJson = nlohmann::json::array();
    for (const auto& b : branches) branchesJson.push_back(b.ToJson());

    nlohmann::json nodesJson = nlohmann::json::object();
    for (const auto& [id, n] : nodes) nodesJson[id] = n.ToJson();

    nlohmann::json gatesJson = nlohmann::json::array();
    for (const auto& g : ageGates) gatesJson.push_back(g.ToJson());

    return {
        {"id", id}, {"name", name}, {"description", description}, {"raceId", raceId},
        {"branches", branchesJson}, {"nodes", nodesJson}, {"ageGates", gatesJson},
        {"totalTalentPoints", totalTalentPoints}, {"pointsPerAge", pointsPerAge},
        {"startingPoints", startingPoints}, {"treeWidth", treeWidth}, {"treeHeight", treeHeight}
    };
}

TalentTreeDefinition TalentTreeDefinition::FromJson(const nlohmann::json& j) {
    TalentTreeDefinition t;
    if (j.contains("id")) t.id = j["id"].get<std::string>();
    if (j.contains("name")) t.name = j["name"].get<std::string>();
    if (j.contains("description")) t.description = j["description"].get<std::string>();
    if (j.contains("raceId")) t.raceId = j["raceId"].get<std::string>();

    if (j.contains("branches") && j["branches"].is_array()) {
        for (const auto& b : j["branches"]) t.branches.push_back(TalentBranch::FromJson(b));
    }
    if (j.contains("nodes") && j["nodes"].is_object()) {
        for (auto& [id, n] : j["nodes"].items()) t.nodes[id] = TalentNode::FromJson(n);
    }
    if (j.contains("ageGates") && j["ageGates"].is_array()) {
        for (const auto& g : j["ageGates"]) t.ageGates.push_back(AgeGate::FromJson(g));
    }

    if (j.contains("totalTalentPoints")) t.totalTalentPoints = j["totalTalentPoints"].get<int>();
    if (j.contains("pointsPerAge")) t.pointsPerAge = j["pointsPerAge"].get<int>();
    if (j.contains("startingPoints")) t.startingPoints = j["startingPoints"].get<int>();
    if (j.contains("treeWidth")) t.treeWidth = j["treeWidth"].get<int>();
    if (j.contains("treeHeight")) t.treeHeight = j["treeHeight"].get<int>();

    return t;
}

bool TalentTreeDefinition::SaveToFile(const std::string& filepath) const {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;
    f << ToJson().dump(2);
    return true;
}

bool TalentTreeDefinition::LoadFromFile(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) return false;
    try { nlohmann::json j; f >> j; *this = FromJson(j); return true; }
    catch (...) { return false; }
}

// TalentTree
TalentTree::TalentTree() = default;
TalentTree::~TalentTree() = default;

bool TalentTree::Initialize(const TalentTreeDefinition& definition) {
    m_definition = definition;
    m_progress.availablePoints = definition.startingPoints;
    m_initialized = true;
    return true;
}

bool TalentTree::InitializeForRace(const std::string& raceId) {
    auto* tree = TalentTreeRegistry::Instance().GetTreeForRace(raceId);
    if (!tree) tree = TalentTreeRegistry::Instance().GetTree("universal");
    if (!tree) return false;
    return Initialize(*tree);
}

void TalentTree::Shutdown() {
    m_definition = TalentTreeDefinition();
    m_progress = TalentProgress();
    m_initialized = false;
}

void TalentTree::SetTotalPoints(int points) {
    int diff = points - m_definition.totalTalentPoints;
    m_definition.totalTalentPoints = points;
    m_progress.availablePoints += diff;
}

void TalentTree::AddPoints(int points) {
    m_progress.availablePoints += points;
}

bool TalentTree::CanUnlockTalent(const std::string& talentId) const {
    if (!m_initialized) return false;
    if (HasTalent(talentId)) return false;

    auto it = m_definition.nodes.find(talentId);
    if (it == m_definition.nodes.end()) return false;

    const auto& node = it->second;
    std::vector<std::string> owned(m_progress.unlockedTalents.begin(), m_progress.unlockedTalents.end());
    return node.CanUnlock(owned, m_currentAge, m_progress.availablePoints);
}

bool TalentTree::UnlockTalent(const std::string& talentId) {
    if (!CanUnlockTalent(talentId)) return false;

    auto it = m_definition.nodes.find(talentId);
    const auto& node = it->second;

    m_progress.unlockedTalents.insert(talentId);
    m_progress.talentRanks[talentId] = 1;
    m_progress.availablePoints -= node.pointCost;
    m_progress.totalPointsSpent += node.pointCost;

    RecalculateModifiers();

    if (m_onTalentUnlock) {
        m_onTalentUnlock(talentId, node);
    }

    return true;
}

bool TalentTree::RefundTalent(const std::string& talentId) {
    if (!HasTalent(talentId)) return false;

    // Check if other talents depend on this one
    for (const auto& [id, node] : m_definition.nodes) {
        if (id != talentId && HasTalent(id)) {
            if (std::find(node.prerequisites.begin(), node.prerequisites.end(), talentId) != node.prerequisites.end()) {
                return false;  // Cannot refund - other talent depends on this
            }
        }
    }

    auto it = m_definition.nodes.find(talentId);
    const auto& node = it->second;

    m_progress.unlockedTalents.erase(talentId);
    m_progress.talentRanks.erase(talentId);
    m_progress.availablePoints += node.pointCost;
    m_progress.totalPointsSpent -= node.pointCost;

    RecalculateModifiers();
    return true;
}

void TalentTree::ResetAllTalents() {
    m_progress.availablePoints += m_progress.totalPointsSpent;
    m_progress.totalPointsSpent = 0;
    m_progress.unlockedTalents.clear();
    m_progress.talentRanks.clear();
    m_cachedModifiers.clear();

    if (m_onTalentReset) {
        m_onTalentReset();
    }
}

bool TalentTree::HasTalent(const std::string& talentId) const {
    return m_progress.HasTalent(talentId);
}

int TalentTree::GetTalentRank(const std::string& talentId) const {
    return m_progress.GetTalentRank(talentId);
}

std::vector<std::string> TalentTree::GetUnlockedTalents() const {
    return std::vector<std::string>(m_progress.unlockedTalents.begin(), m_progress.unlockedTalents.end());
}

const TalentNode* TalentTree::GetNode(const std::string& nodeId) const {
    auto it = m_definition.nodes.find(nodeId);
    return (it != m_definition.nodes.end()) ? &it->second : nullptr;
}

std::vector<const TalentNode*> TalentTree::GetAllNodes() const {
    std::vector<const TalentNode*> r;
    for (const auto& [id, n] : m_definition.nodes) r.push_back(&n);
    return r;
}

std::vector<const TalentNode*> TalentTree::GetAvailableNodes() const {
    std::vector<const TalentNode*> r;
    for (const auto& [id, n] : m_definition.nodes) {
        if (CanUnlockTalent(id)) r.push_back(&n);
    }
    return r;
}

std::vector<const TalentNode*> TalentTree::GetNodesByCategory(TalentCategory category) const {
    std::vector<const TalentNode*> r;
    for (const auto& [id, n] : m_definition.nodes) {
        if (n.category == category) r.push_back(&n);
    }
    return r;
}

std::vector<const TalentNode*> TalentTree::GetNodesByTier(int tier) const {
    std::vector<const TalentNode*> r;
    for (const auto& [id, n] : m_definition.nodes) {
        if (n.tier == tier) r.push_back(&n);
    }
    return r;
}

const TalentBranch* TalentTree::GetBranch(const std::string& branchId) const {
    for (const auto& b : m_definition.branches) {
        if (b.id == branchId) return &b;
    }
    return nullptr;
}

std::vector<const TalentBranch*> TalentTree::GetAllBranches() const {
    std::vector<const TalentBranch*> r;
    for (const auto& b : m_definition.branches) r.push_back(&b);
    return r;
}

float TalentTree::GetBranchProgress(const std::string& branchId) const {
    const auto* branch = GetBranch(branchId);
    if (!branch || branch->nodeIds.empty()) return 0.0f;

    int unlocked = 0;
    for (const auto& nodeId : branch->nodeIds) {
        if (HasTalent(nodeId)) ++unlocked;
    }
    return static_cast<float>(unlocked) / branch->nodeIds.size();
}

float TalentTree::GetStatModifier(const std::string& stat, const std::string& targetType) const {
    float modifier = 0.0f;
    for (const auto& talentId : m_progress.unlockedTalents) {
        auto it = m_definition.nodes.find(talentId);
        if (it == m_definition.nodes.end()) continue;

        for (const auto& m : it->second.modifiers) {
            if (m.stat == stat && (m.targetType == targetType || m.targetType == "all" || targetType == "all")) {
                modifier += m.value;
            }
        }
    }
    return modifier;
}

std::map<std::string, float> TalentTree::GetAllModifiers() const {
    return m_cachedModifiers;
}

std::vector<std::string> TalentTree::GetUnlockedContent(const std::string& type) const {
    std::vector<std::string> r;
    for (const auto& talentId : m_progress.unlockedTalents) {
        auto it = m_definition.nodes.find(talentId);
        if (it == m_definition.nodes.end()) continue;

        for (const auto& u : it->second.unlocks) {
            if (u.type == type) r.push_back(u.targetId);
        }
    }
    return r;
}

void TalentTree::OnAgeAdvance(int newAge) {
    m_currentAge = newAge;
    m_progress.availablePoints += m_definition.pointsPerAge;

    // Check for bonus points from age gates
    for (const auto& gate : m_definition.ageGates) {
        if (gate.age == newAge && gate.bonusTalentPoints > 0) {
            m_progress.availablePoints += gate.bonusTalentPoints;
        }
    }
}

std::vector<std::string> TalentTree::GetNodesAvailableAtAge(int age) const {
    std::vector<std::string> r;
    for (const auto& gate : m_definition.ageGates) {
        if (gate.age <= age) {
            r.insert(r.end(), gate.unlockedNodes.begin(), gate.unlockedNodes.end());
        }
    }
    return r;
}

nlohmann::json TalentTree::ToJson() const {
    return {{"definition", m_definition.ToJson()}, {"progress", m_progress.ToJson()}, {"currentAge", m_currentAge}};
}

void TalentTree::FromJson(const nlohmann::json& j) {
    if (j.contains("definition")) m_definition = TalentTreeDefinition::FromJson(j["definition"]);
    if (j.contains("progress")) m_progress = TalentProgress::FromJson(j["progress"]);
    if (j.contains("currentAge")) m_currentAge = j["currentAge"].get<int>();
    m_initialized = true;
    RecalculateModifiers();
}

bool TalentTree::SaveProgress(const std::string& filepath) const {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;
    f << ToJson().dump(2);
    return true;
}

bool TalentTree::LoadProgress(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) return false;
    try { nlohmann::json j; f >> j; FromJson(j); return true; }
    catch (...) { return false; }
}

void TalentTree::RecalculateModifiers() {
    m_cachedModifiers.clear();
    for (const auto& talentId : m_progress.unlockedTalents) {
        auto it = m_definition.nodes.find(talentId);
        if (it == m_definition.nodes.end()) continue;

        for (const auto& m : it->second.modifiers) {
            std::string key = m.stat + "_" + m.targetType;
            m_cachedModifiers[key] += m.value;
        }
    }
}

// TalentTreeRegistry
TalentTreeRegistry& TalentTreeRegistry::Instance() {
    static TalentTreeRegistry instance;
    return instance;
}

bool TalentTreeRegistry::Initialize() {
    if (m_initialized) return true;
    InitializeBuiltInTrees();
    m_initialized = true;
    return true;
}

void TalentTreeRegistry::Shutdown() { m_trees.clear(); m_initialized = false; }

bool TalentTreeRegistry::RegisterTree(const TalentTreeDefinition& t) {
    if (t.id.empty()) return false;
    m_trees[t.id] = t;
    return true;
}

const TalentTreeDefinition* TalentTreeRegistry::GetTree(const std::string& id) const {
    auto it = m_trees.find(id);
    return (it != m_trees.end()) ? &it->second : nullptr;
}

const TalentTreeDefinition* TalentTreeRegistry::GetTreeForRace(const std::string& raceId) const {
    for (const auto& [id, t] : m_trees) {
        if (t.raceId == raceId) return &t;
    }
    return nullptr;
}

std::vector<const TalentTreeDefinition*> TalentTreeRegistry::GetAllTrees() const {
    std::vector<const TalentTreeDefinition*> r;
    for (const auto& [id, t] : m_trees) r.push_back(&t);
    return r;
}

int TalentTreeRegistry::LoadFromDirectory(const std::string& dir) {
    int count = 0;
    try {
        for (const auto& e : std::filesystem::directory_iterator(dir)) {
            if (e.path().extension() == ".json") {
                TalentTreeDefinition t;
                if (t.LoadFromFile(e.path().string()) && RegisterTree(t)) ++count;
            }
        }
    } catch (...) {}
    return count;
}

void TalentTreeRegistry::InitializeBuiltInTrees() {
    TalentTreeDefinition universal;
    universal.id = "universal";
    universal.name = "Universal Talent Tree";
    universal.description = "Default talent tree available to all races.";
    universal.totalTalentPoints = 30;
    universal.pointsPerAge = 5;
    universal.treeWidth = 5;
    universal.treeHeight = 7;

    // Add branches
    universal.branches = {
        {"branch_military", "Warfare", "Combat enhancements", TalentCategory::Military, "", "#FF0000", {}, ""},
        {"branch_economy", "Prosperity", "Economic bonuses", TalentCategory::Economy, "", "#FFD700", {}, ""},
        {"branch_magic", "Arcana", "Magical power", TalentCategory::Magic, "", "#9370DB", {}, ""},
        {"branch_tech", "Innovation", "Research bonuses", TalentCategory::Technology, "", "#00BFFF", {}, ""}
    };

    // Add nodes from registry
    auto& nodeRegistry = TalentNodeRegistry::Instance();
    nodeRegistry.Initialize();
    for (const auto* node : nodeRegistry.GetAllNodes()) {
        universal.nodes[node->id] = *node;
    }

    // Set up age gates
    for (int age = 0; age < 7; ++age) {
        AgeGate gate;
        gate.age = age;
        gate.bonusTalentPoints = (age > 0) ? 5 : 0;
        universal.ageGates.push_back(gate);
    }

    RegisterTree(universal);
}

} // namespace Talent
} // namespace RTS
} // namespace Vehement
