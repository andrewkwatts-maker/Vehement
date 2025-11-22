#include "RacialBonus.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace Vehement {
namespace RTS {
namespace Race {

// BonusEffect
nlohmann::json BonusEffect::ToJson() const {
    return {{"target", target}, {"value", value}, {"isMultiplier", isMultiplier}, {"condition", condition}};
}

BonusEffect BonusEffect::FromJson(const nlohmann::json& j) {
    BonusEffect e;
    if (j.contains("target")) e.target = j["target"].get<std::string>();
    if (j.contains("value")) e.value = j["value"].get<float>();
    if (j.contains("isMultiplier")) e.isMultiplier = j["isMultiplier"].get<bool>();
    if (j.contains("condition")) e.condition = j["condition"].get<std::string>();
    return e;
}

// RacialBonus
float RacialBonus::ApplyBonus(float baseValue, const std::string& targetStat) const {
    for (const auto& effect : effects) {
        if (effect.target == targetStat || effect.target == "*") {
            if (effect.isMultiplier) {
                return baseValue * (1.0f + effect.value);
            } else {
                return baseValue + effect.value;
            }
        }
    }
    return baseValue;
}

bool RacialBonus::IsApplicable(const std::string& targetStat) const {
    return std::any_of(effects.begin(), effects.end(),
        [&targetStat](const BonusEffect& e) { return e.target == targetStat || e.target == "*"; });
}

bool RacialBonus::Validate() const {
    return !id.empty() && !name.empty() && !effects.empty();
}

nlohmann::json RacialBonus::ToJson() const {
    nlohmann::json effectsJson = nlohmann::json::array();
    for (const auto& e : effects) effectsJson.push_back(e.ToJson());

    return {
        {"id", id}, {"name", name}, {"description", description}, {"iconPath", iconPath},
        {"type", BonusTypeToString(type)}, {"isPassive", isPassive}, {"isUnique", isUnique},
        {"effects", effectsJson}, {"activationCondition", activationCondition},
        {"deactivationCondition", deactivationCondition}, {"cooldown", cooldown},
        {"duration", duration}, {"requiredAge", requiredAge}, {"requiredTech", requiredTech},
        {"pointCost", pointCost}, {"powerRating", powerRating}, {"tags", tags}
    };
}

RacialBonus RacialBonus::FromJson(const nlohmann::json& j) {
    RacialBonus b;
    if (j.contains("id")) b.id = j["id"].get<std::string>();
    if (j.contains("name")) b.name = j["name"].get<std::string>();
    if (j.contains("description")) b.description = j["description"].get<std::string>();
    if (j.contains("iconPath")) b.iconPath = j["iconPath"].get<std::string>();

    if (j.contains("type")) {
        std::string t = j["type"].get<std::string>();
        if (t == "GatherRate") b.type = BonusType::GatherRate;
        else if (t == "BuildSpeed") b.type = BonusType::BuildSpeed;
        else if (t == "UnitStat") b.type = BonusType::UnitStat;
        else if (t == "BuildingStat") b.type = BonusType::BuildingStat;
        else if (t == "SpellEnhancement") b.type = BonusType::SpellEnhancement;
        else if (t == "UniqueAbility") b.type = BonusType::UniqueAbility;
        else if (t == "EconomyBoost") b.type = BonusType::EconomyBoost;
        else if (t == "MilitaryBoost") b.type = BonusType::MilitaryBoost;
        else if (t == "ResearchBoost") b.type = BonusType::ResearchBoost;
        else if (t == "StartingBonus") b.type = BonusType::StartingBonus;
    }

    if (j.contains("isPassive")) b.isPassive = j["isPassive"].get<bool>();
    if (j.contains("isUnique")) b.isUnique = j["isUnique"].get<bool>();

    if (j.contains("effects") && j["effects"].is_array()) {
        for (const auto& e : j["effects"]) b.effects.push_back(BonusEffect::FromJson(e));
    }

    if (j.contains("activationCondition")) b.activationCondition = j["activationCondition"].get<std::string>();
    if (j.contains("deactivationCondition")) b.deactivationCondition = j["deactivationCondition"].get<std::string>();
    if (j.contains("cooldown")) b.cooldown = j["cooldown"].get<float>();
    if (j.contains("duration")) b.duration = j["duration"].get<float>();
    if (j.contains("requiredAge")) b.requiredAge = j["requiredAge"].get<int>();
    if (j.contains("requiredTech")) b.requiredTech = j["requiredTech"].get<std::string>();
    if (j.contains("pointCost")) b.pointCost = j["pointCost"].get<int>();
    if (j.contains("powerRating")) b.powerRating = j["powerRating"].get<float>();
    if (j.contains("tags")) b.tags = j["tags"].get<std::vector<std::string>>();

    return b;
}

bool RacialBonus::SaveToFile(const std::string& filepath) const {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;
    f << ToJson().dump(2);
    return true;
}

bool RacialBonus::LoadFromFile(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) return false;
    try { nlohmann::json j; f >> j; *this = FromJson(j); return true; }
    catch (...) { return false; }
}

// Registry
RacialBonusRegistry& RacialBonusRegistry::Instance() {
    static RacialBonusRegistry instance;
    return instance;
}

bool RacialBonusRegistry::Initialize() {
    if (m_initialized) return true;
    InitializeBuiltInBonuses();
    m_initialized = true;
    return true;
}

void RacialBonusRegistry::Shutdown() { m_bonuses.clear(); m_initialized = false; }

bool RacialBonusRegistry::RegisterBonus(const RacialBonus& b) {
    if (b.id.empty()) return false;
    m_bonuses[b.id] = b;
    return true;
}

const RacialBonus* RacialBonusRegistry::GetBonus(const std::string& id) const {
    auto it = m_bonuses.find(id);
    return (it != m_bonuses.end()) ? &it->second : nullptr;
}

std::vector<const RacialBonus*> RacialBonusRegistry::GetAllBonuses() const {
    std::vector<const RacialBonus*> r;
    for (const auto& [id, b] : m_bonuses) r.push_back(&b);
    return r;
}

std::vector<const RacialBonus*> RacialBonusRegistry::GetByType(BonusType type) const {
    std::vector<const RacialBonus*> r;
    for (const auto& [id, b] : m_bonuses) if (b.type == type) r.push_back(&b);
    return r;
}

int RacialBonusRegistry::LoadFromDirectory(const std::string& dir) {
    int count = 0;
    try {
        for (const auto& e : std::filesystem::directory_iterator(dir)) {
            if (e.path().extension() == ".json") {
                RacialBonus b;
                if (b.LoadFromFile(e.path().string()) && RegisterBonus(b)) ++count;
            }
        }
    } catch (...) {}
    return count;
}

void RacialBonusRegistry::InitializeBuiltInBonuses() {
    RegisterBonus(CreateGatherSpeedBonus());
    RegisterBonus(CreateBuildSpeedBonus());
    RegisterBonus(CreateInfantryDamageBonus());
    RegisterBonus(CreateCavalrySpeedBonus());
    RegisterBonus(CreateMagicPowerBonus());
    RegisterBonus(CreateDefenseBonus());
    RegisterBonus(CreateResearchSpeedBonus());
    RegisterBonus(CreateStartingResourcesBonus());
}

// Built-in Bonuses
RacialBonus CreateGatherSpeedBonus() {
    RacialBonus b;
    b.id = "bonus_gather_speed";
    b.name = "Efficient Gatherers";
    b.description = "+15% resource gathering speed.";
    b.type = BonusType::GatherRate;
    b.effects = {{"gatherSpeed", 0.15f, true, ""}};
    b.pointCost = 5;
    b.tags = {"economy", "gathering"};
    return b;
}

RacialBonus CreateBuildSpeedBonus() {
    RacialBonus b;
    b.id = "bonus_build_speed";
    b.name = "Master Builders";
    b.description = "+20% construction speed.";
    b.type = BonusType::BuildSpeed;
    b.effects = {{"buildSpeed", 0.20f, true, ""}};
    b.pointCost = 5;
    b.tags = {"economy", "building"};
    return b;
}

RacialBonus CreateInfantryDamageBonus() {
    RacialBonus b;
    b.id = "bonus_infantry_damage";
    b.name = "Battle Hardened";
    b.description = "+10% infantry damage.";
    b.type = BonusType::MilitaryBoost;
    b.effects = {{"infantryDamage", 0.10f, true, ""}};
    b.pointCost = 6;
    b.tags = {"military", "infantry"};
    return b;
}

RacialBonus CreateCavalrySpeedBonus() {
    RacialBonus b;
    b.id = "bonus_cavalry_speed";
    b.name = "Swift Riders";
    b.description = "+15% cavalry movement speed.";
    b.type = BonusType::MilitaryBoost;
    b.effects = {{"cavalrySpeed", 0.15f, true, ""}};
    b.pointCost = 5;
    b.tags = {"military", "cavalry"};
    return b;
}

RacialBonus CreateMagicPowerBonus() {
    RacialBonus b;
    b.id = "bonus_magic_power";
    b.name = "Arcane Affinity";
    b.description = "+15% spell damage.";
    b.type = BonusType::SpellEnhancement;
    b.effects = {{"spellDamage", 0.15f, true, ""}};
    b.pointCost = 6;
    b.tags = {"magic", "damage"};
    return b;
}

RacialBonus CreateDefenseBonus() {
    RacialBonus b;
    b.id = "bonus_defense";
    b.name = "Thick Skinned";
    b.description = "+2 armor for all units.";
    b.type = BonusType::UnitStat;
    b.effects = {{"armor", 2.0f, false, ""}};
    b.pointCost = 6;
    b.tags = {"military", "defense"};
    return b;
}

RacialBonus CreateResearchSpeedBonus() {
    RacialBonus b;
    b.id = "bonus_research_speed";
    b.name = "Quick Learners";
    b.description = "+20% research speed.";
    b.type = BonusType::ResearchBoost;
    b.effects = {{"researchSpeed", 0.20f, true, ""}};
    b.pointCost = 5;
    b.tags = {"technology", "research"};
    return b;
}

RacialBonus CreateStartingResourcesBonus() {
    RacialBonus b;
    b.id = "bonus_starting_resources";
    b.name = "Wealthy Heritage";
    b.description = "+100 starting gold and wood.";
    b.type = BonusType::StartingBonus;
    b.effects = {{"startingGold", 100.0f, false, ""}, {"startingWood", 100.0f, false, ""}};
    b.pointCost = 4;
    b.tags = {"economy", "starting"};
    return b;
}

} // namespace Race
} // namespace RTS
} // namespace Vehement
