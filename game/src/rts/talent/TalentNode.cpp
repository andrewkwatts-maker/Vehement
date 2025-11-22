#include "TalentNode.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace Vehement {
namespace RTS {
namespace Talent {

// TalentUnlock
nlohmann::json TalentUnlock::ToJson() const {
    return {{"type", type}, {"targetId", targetId}, {"description", description}};
}

TalentUnlock TalentUnlock::FromJson(const nlohmann::json& j) {
    TalentUnlock u;
    if (j.contains("type")) u.type = j["type"].get<std::string>();
    if (j.contains("targetId")) u.targetId = j["targetId"].get<std::string>();
    if (j.contains("description")) u.description = j["description"].get<std::string>();
    return u;
}

// TalentModifier
nlohmann::json TalentModifier::ToJson() const {
    return {{"stat", stat}, {"value", value}, {"isPercentage", isPercentage}, {"targetType", targetType}};
}

TalentModifier TalentModifier::FromJson(const nlohmann::json& j) {
    TalentModifier m;
    if (j.contains("stat")) m.stat = j["stat"].get<std::string>();
    if (j.contains("value")) m.value = j["value"].get<float>();
    if (j.contains("isPercentage")) m.isPercentage = j["isPercentage"].get<bool>();
    if (j.contains("targetType")) m.targetType = j["targetType"].get<std::string>();
    return m;
}

// TalentNode
bool TalentNode::CanUnlock(const std::vector<std::string>& ownedTalents, int currentAge, int availablePoints) const {
    // Check points
    if (availablePoints < pointCost) return false;

    // Check age requirement
    if (currentAge < requiredAge) return false;

    // Check prerequisites
    if (!prerequisites.empty()) {
        int metCount = 0;
        for (const auto& prereq : prerequisites) {
            if (std::find(ownedTalents.begin(), ownedTalents.end(), prereq) != ownedTalents.end()) {
                ++metCount;
            }
        }

        int required = (prerequisiteCount > 0) ? prerequisiteCount : static_cast<int>(prerequisites.size());
        if (metCount < required) return false;
    }

    return true;
}

float TalentNode::CalculateSynergyBonus(const std::vector<std::string>& ownedTalents) const {
    if (synergyWith.empty()) return 0.0f;

    int synergyCount = 0;
    for (const auto& synergy : synergyWith) {
        if (std::find(ownedTalents.begin(), ownedTalents.end(), synergy) != ownedTalents.end()) {
            ++synergyCount;
        }
    }

    return synergyCount * synergyBonus;
}

bool TalentNode::Validate() const {
    return !id.empty() && !name.empty() && pointCost > 0;
}

nlohmann::json TalentNode::ToJson() const {
    nlohmann::json unlocksJson = nlohmann::json::array();
    for (const auto& u : unlocks) unlocksJson.push_back(u.ToJson());

    nlohmann::json modifiersJson = nlohmann::json::array();
    for (const auto& m : modifiers) modifiersJson.push_back(m.ToJson());

    return {
        {"id", id}, {"name", name}, {"description", description}, {"iconPath", iconPath},
        {"category", TalentCategoryToString(category)}, {"tier", tier},
        {"pointCost", pointCost}, {"requiredAge", requiredAge},
        {"prerequisites", prerequisites}, {"prerequisiteCount", prerequisiteCount},
        {"unlocks", unlocksJson}, {"modifiers", modifiersJson},
        {"positionX", positionX}, {"positionY", positionY}, {"connectedFrom", connectedFrom},
        {"synergyWith", synergyWith}, {"synergyBonus", synergyBonus},
        {"isKeystone", isKeystone}, {"isPassive", isPassive}, {"maxRank", maxRank},
        {"powerRating", powerRating}, {"tags", tags}
    };
}

TalentNode TalentNode::FromJson(const nlohmann::json& j) {
    TalentNode n;
    if (j.contains("id")) n.id = j["id"].get<std::string>();
    if (j.contains("name")) n.name = j["name"].get<std::string>();
    if (j.contains("description")) n.description = j["description"].get<std::string>();
    if (j.contains("iconPath")) n.iconPath = j["iconPath"].get<std::string>();

    if (j.contains("category")) {
        std::string c = j["category"].get<std::string>();
        if (c == "Military") n.category = TalentCategory::Military;
        else if (c == "Economy") n.category = TalentCategory::Economy;
        else if (c == "Magic") n.category = TalentCategory::Magic;
        else if (c == "Technology") n.category = TalentCategory::Technology;
        else if (c == "Special") n.category = TalentCategory::Special;
    }

    if (j.contains("tier")) n.tier = j["tier"].get<int>();
    if (j.contains("pointCost")) n.pointCost = j["pointCost"].get<int>();
    if (j.contains("requiredAge")) n.requiredAge = j["requiredAge"].get<int>();
    if (j.contains("prerequisites")) n.prerequisites = j["prerequisites"].get<std::vector<std::string>>();
    if (j.contains("prerequisiteCount")) n.prerequisiteCount = j["prerequisiteCount"].get<int>();

    if (j.contains("unlocks") && j["unlocks"].is_array()) {
        for (const auto& u : j["unlocks"]) n.unlocks.push_back(TalentUnlock::FromJson(u));
    }
    if (j.contains("modifiers") && j["modifiers"].is_array()) {
        for (const auto& m : j["modifiers"]) n.modifiers.push_back(TalentModifier::FromJson(m));
    }

    if (j.contains("positionX")) n.positionX = j["positionX"].get<int>();
    if (j.contains("positionY")) n.positionY = j["positionY"].get<int>();
    if (j.contains("connectedFrom")) n.connectedFrom = j["connectedFrom"].get<std::string>();
    if (j.contains("synergyWith")) n.synergyWith = j["synergyWith"].get<std::vector<std::string>>();
    if (j.contains("synergyBonus")) n.synergyBonus = j["synergyBonus"].get<float>();
    if (j.contains("isKeystone")) n.isKeystone = j["isKeystone"].get<bool>();
    if (j.contains("isPassive")) n.isPassive = j["isPassive"].get<bool>();
    if (j.contains("maxRank")) n.maxRank = j["maxRank"].get<int>();
    if (j.contains("powerRating")) n.powerRating = j["powerRating"].get<float>();
    if (j.contains("tags")) n.tags = j["tags"].get<std::vector<std::string>>();

    return n;
}

bool TalentNode::SaveToFile(const std::string& filepath) const {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;
    f << ToJson().dump(2);
    return true;
}

bool TalentNode::LoadFromFile(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) return false;
    try { nlohmann::json j; f >> j; *this = FromJson(j); return true; }
    catch (...) { return false; }
}

// Registry
TalentNodeRegistry& TalentNodeRegistry::Instance() {
    static TalentNodeRegistry instance;
    return instance;
}

bool TalentNodeRegistry::Initialize() {
    if (m_initialized) return true;
    InitializeBuiltInNodes();
    m_initialized = true;
    return true;
}

void TalentNodeRegistry::Shutdown() { m_nodes.clear(); m_initialized = false; }

bool TalentNodeRegistry::RegisterNode(const TalentNode& n) {
    if (n.id.empty()) return false;
    m_nodes[n.id] = n;
    return true;
}

const TalentNode* TalentNodeRegistry::GetNode(const std::string& id) const {
    auto it = m_nodes.find(id);
    return (it != m_nodes.end()) ? &it->second : nullptr;
}

std::vector<const TalentNode*> TalentNodeRegistry::GetAllNodes() const {
    std::vector<const TalentNode*> r;
    for (const auto& [id, n] : m_nodes) r.push_back(&n);
    return r;
}

std::vector<const TalentNode*> TalentNodeRegistry::GetByCategory(TalentCategory cat) const {
    std::vector<const TalentNode*> r;
    for (const auto& [id, n] : m_nodes) if (n.category == cat) r.push_back(&n);
    return r;
}

std::vector<const TalentNode*> TalentNodeRegistry::GetByTier(int tier) const {
    std::vector<const TalentNode*> r;
    for (const auto& [id, n] : m_nodes) if (n.tier == tier) r.push_back(&n);
    return r;
}

int TalentNodeRegistry::LoadFromDirectory(const std::string& dir) {
    int count = 0;
    try {
        for (const auto& e : std::filesystem::directory_iterator(dir)) {
            if (e.path().extension() == ".json") {
                TalentNode n;
                if (n.LoadFromFile(e.path().string()) && RegisterNode(n)) ++count;
            }
        }
    } catch (...) {}
    return count;
}

void TalentNodeRegistry::InitializeBuiltInNodes() {
    // Military tree - Tier 1
    TalentNode n1;
    n1.id = "talent_military_damage";
    n1.name = "Weapon Training";
    n1.description = "+5% unit damage";
    n1.category = TalentCategory::Military;
    n1.tier = 1;
    n1.pointCost = 1;
    n1.modifiers = {{"damage", 0.05f, true, "all"}};
    n1.positionX = 0;
    n1.positionY = 0;
    RegisterNode(n1);

    TalentNode n2;
    n2.id = "talent_military_armor";
    n2.name = "Heavy Armor";
    n2.description = "+2 armor for all units";
    n2.category = TalentCategory::Military;
    n2.tier = 1;
    n2.pointCost = 1;
    n2.modifiers = {{"armor", 2.0f, false, "all"}};
    n2.positionX = 1;
    n2.positionY = 0;
    RegisterNode(n2);

    // Economy tree - Tier 1
    TalentNode e1;
    e1.id = "talent_economy_gather";
    e1.name = "Efficient Gathering";
    e1.description = "+10% gathering speed";
    e1.category = TalentCategory::Economy;
    e1.tier = 1;
    e1.pointCost = 1;
    e1.modifiers = {{"gatherSpeed", 0.10f, true, "worker"}};
    e1.positionX = 0;
    e1.positionY = 0;
    RegisterNode(e1);

    TalentNode e2;
    e2.id = "talent_economy_build";
    e2.name = "Quick Construction";
    e2.description = "+15% build speed";
    e2.category = TalentCategory::Economy;
    e2.tier = 1;
    e2.pointCost = 1;
    e2.modifiers = {{"buildSpeed", 0.15f, true, "all"}};
    e2.positionX = 1;
    e2.positionY = 0;
    RegisterNode(e2);

    // Magic tree - Tier 1
    TalentNode m1;
    m1.id = "talent_magic_power";
    m1.name = "Arcane Power";
    m1.description = "+10% spell damage";
    m1.category = TalentCategory::Magic;
    m1.tier = 1;
    m1.pointCost = 1;
    m1.modifiers = {{"spellDamage", 0.10f, true, "all"}};
    m1.positionX = 0;
    m1.positionY = 0;
    RegisterNode(m1);

    // Technology tree - Tier 1
    TalentNode t1;
    t1.id = "talent_tech_research";
    t1.name = "Quick Study";
    t1.description = "+15% research speed";
    t1.category = TalentCategory::Technology;
    t1.tier = 1;
    t1.pointCost = 1;
    t1.modifiers = {{"researchSpeed", 0.15f, true, "all"}};
    t1.positionX = 0;
    t1.positionY = 0;
    RegisterNode(t1);
}

} // namespace Talent
} // namespace RTS
} // namespace Vehement
