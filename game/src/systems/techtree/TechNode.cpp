#include "TechNode.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Vehement {
namespace TechTree {

// ============================================================================
// TechCost
// ============================================================================

nlohmann::json TechCost::ToJson() const {
    nlohmann::json j;

    for (const auto& [resource, amount] : resources) {
        j[resource] = amount;
    }

    if (time > 0.0f) {
        j["time"] = time;
    }

    if (researchPoints > 0) {
        j["research_points"] = researchPoints;
    }

    return j;
}

TechCost TechCost::FromJson(const nlohmann::json& j) {
    TechCost cost;

    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string& key = it.key();

        if (key == "time") {
            cost.time = it.value().get<float>();
        } else if (key == "research_points") {
            cost.researchPoints = it.value().get<int>();
        } else if (it.value().is_number()) {
            cost.resources[key] = it.value().get<int>();
        }
    }

    return cost;
}

// ============================================================================
// TechUnlocks
// ============================================================================

nlohmann::json TechUnlocks::ToJson() const {
    nlohmann::json j;

    if (!buildings.empty()) {
        j["buildings"] = buildings;
    }
    if (!units.empty()) {
        j["units"] = units;
    }
    if (!abilities.empty()) {
        j["abilities"] = abilities;
    }
    if (!upgrades.empty()) {
        j["upgrades"] = upgrades;
    }
    if (!spells.empty()) {
        j["spells"] = spells;
    }
    if (!features.empty()) {
        j["features"] = features;
    }
    if (!resources.empty()) {
        j["resources"] = resources;
    }

    return j;
}

TechUnlocks TechUnlocks::FromJson(const nlohmann::json& j) {
    TechUnlocks unlocks;

    if (j.contains("buildings")) {
        unlocks.buildings = j["buildings"].get<std::vector<std::string>>();
    }
    if (j.contains("units")) {
        unlocks.units = j["units"].get<std::vector<std::string>>();
    }
    if (j.contains("abilities")) {
        unlocks.abilities = j["abilities"].get<std::vector<std::string>>();
    }
    if (j.contains("upgrades")) {
        unlocks.upgrades = j["upgrades"].get<std::vector<std::string>>();
    }
    if (j.contains("spells")) {
        unlocks.spells = j["spells"].get<std::vector<std::string>>();
    }
    if (j.contains("features")) {
        unlocks.features = j["features"].get<std::vector<std::string>>();
    }
    if (j.contains("resources")) {
        unlocks.resources = j["resources"].get<std::vector<std::string>>();
    }

    return unlocks;
}

// ============================================================================
// TechScriptEvents
// ============================================================================

nlohmann::json TechScriptEvents::ToJson() const {
    nlohmann::json j;

    if (!onResearchStart.empty()) {
        j["on_research_start"] = onResearchStart;
    }
    if (!onResearchComplete.empty()) {
        j["on_research_complete"] = onResearchComplete;
    }
    if (!onResearchCancel.empty()) {
        j["on_research_cancel"] = onResearchCancel;
    }
    if (!onCreate.empty()) {
        j["on_create"] = onCreate;
    }
    if (!onTick.empty()) {
        j["on_tick"] = onTick;
    }
    if (!onDestroy.empty()) {
        j["on_destroy"] = onDestroy;
    }
    if (!onApply.empty()) {
        j["on_apply"] = onApply;
    }
    if (!onRemove.empty()) {
        j["on_remove"] = onRemove;
    }

    return j;
}

TechScriptEvents TechScriptEvents::FromJson(const nlohmann::json& j) {
    TechScriptEvents events;

    if (j.contains("on_research_start")) {
        events.onResearchStart = j["on_research_start"].get<std::string>();
    }
    if (j.contains("on_research_complete")) {
        events.onResearchComplete = j["on_research_complete"].get<std::string>();
    }
    if (j.contains("on_research_cancel")) {
        events.onResearchCancel = j["on_research_cancel"].get<std::string>();
    }
    if (j.contains("on_create")) {
        events.onCreate = j["on_create"].get<std::string>();
    }
    if (j.contains("on_tick")) {
        events.onTick = j["on_tick"].get<std::string>();
    }
    if (j.contains("on_destroy")) {
        events.onDestroy = j["on_destroy"].get<std::string>();
    }
    if (j.contains("on_apply")) {
        events.onApply = j["on_apply"].get<std::string>();
    }
    if (j.contains("on_remove")) {
        events.onRemove = j["on_remove"].get<std::string>();
    }

    return events;
}

// ============================================================================
// TechPosition
// ============================================================================

nlohmann::json TechPosition::ToJson() const {
    nlohmann::json j;
    j["x"] = x;
    j["y"] = y;
    if (row != 0 || column != 0) {
        j["row"] = row;
        j["column"] = column;
    }
    return j;
}

TechPosition TechPosition::FromJson(const nlohmann::json& j) {
    TechPosition pos;

    if (j.contains("x")) {
        pos.x = j["x"].get<float>();
    }
    if (j.contains("y")) {
        pos.y = j["y"].get<float>();
    }
    if (j.contains("row")) {
        pos.row = j["row"].get<int>();
    }
    if (j.contains("column")) {
        pos.column = j["column"].get<int>();
    }

    return pos;
}

// ============================================================================
// TechNode
// ============================================================================

TechNode::TechNode(const std::string& id) : m_id(id) {}

bool TechNode::HasTag(const std::string& tag) const {
    return std::find(m_tags.begin(), m_tags.end(), tag) != m_tags.end();
}

bool TechNode::IsExclusiveWith(const std::string& techId) const {
    return std::find(m_exclusiveWith.begin(), m_exclusiveWith.end(), techId) != m_exclusiveWith.end();
}

bool TechNode::UnlocksBuilding(const std::string& buildingId) const {
    return std::find(m_unlocks.buildings.begin(), m_unlocks.buildings.end(), buildingId) != m_unlocks.buildings.end();
}

bool TechNode::UnlocksUnit(const std::string& unitId) const {
    return std::find(m_unlocks.units.begin(), m_unlocks.units.end(), unitId) != m_unlocks.units.end();
}

bool TechNode::UnlocksAbility(const std::string& abilityId) const {
    return std::find(m_unlocks.abilities.begin(), m_unlocks.abilities.end(), abilityId) != m_unlocks.abilities.end();
}

bool TechNode::IsAvailableToCulture(const std::string& culture) const {
    if (m_isUniversal) {
        return true;
    }
    if (m_availableToCultures.empty()) {
        return true; // Empty = available to all
    }
    return std::find(m_availableToCultures.begin(), m_availableToCultures.end(), culture) != m_availableToCultures.end();
}

nlohmann::json TechNode::ToJson() const {
    nlohmann::json j;

    // Identity
    j["id"] = m_id;
    j["name"] = m_name;
    if (!m_description.empty()) {
        j["description"] = m_description;
    }
    if (!m_icon.empty()) {
        j["icon"] = m_icon;
    }
    if (!m_flavorText.empty()) {
        j["flavor_text"] = m_flavorText;
    }

    // Classification
    j["category"] = TechCategoryToString(m_category);
    if (m_ageRequirement != TechAge::Stone) {
        j["age_requirement"] = TechAgeToShortString(m_ageRequirement);
    }
    j["tier"] = m_tier;
    if (!m_tags.empty()) {
        j["tags"] = m_tags;
    }

    // Prerequisites
    if (!m_prerequisites.empty()) {
        j["prerequisites"] = m_prerequisites;
    }
    if (!m_optionalPrereqs.empty()) {
        j["optional_prerequisites"] = m_optionalPrereqs;
        j["optional_required_count"] = m_optionalRequiredCount;
    }
    if (!m_exclusiveWith.empty()) {
        j["exclusive_with"] = m_exclusiveWith;
    }

    // Cost
    if (!m_cost.IsEmpty() || m_cost.time > 0.0f) {
        j["cost"] = m_cost.ToJson();
    }

    // Unlocks
    if (!m_unlocks.IsEmpty()) {
        j["unlocks"] = m_unlocks.ToJson();
    }

    // Modifiers
    if (!m_modifiers.empty()) {
        nlohmann::json modsArray = nlohmann::json::array();
        for (const auto& mod : m_modifiers) {
            modsArray.push_back(mod.ToJson());
        }
        j["modifiers"] = modsArray;
    }

    // Scripts
    if (m_scriptEvents.HasAnyScripts()) {
        j["events"] = m_scriptEvents.ToJson();
    }

    // Position
    j["position"] = m_position.ToJson();

    // Culture restrictions
    if (!m_availableToCultures.empty()) {
        j["available_to_cultures"] = m_availableToCultures;
    }
    if (m_isUniversal) {
        j["universal"] = true;
    }

    // Special properties
    if (m_repeatable) {
        j["repeatable"] = true;
        if (m_maxResearchCount > 0) {
            j["max_research_count"] = m_maxResearchCount;
        }
    }
    if (m_hidden) {
        j["hidden"] = true;
    }
    if (!m_canBeLost) {
        j["can_be_lost"] = false;
    }
    if (m_lossChance != 0.3f) {
        j["loss_chance"] = m_lossChance;
    }
    if (m_isKeyTech) {
        j["key_tech"] = true;
    }

    return j;
}

TechNode TechNode::FromJson(const nlohmann::json& j) {
    TechNode node;

    // Identity
    if (j.contains("id")) {
        node.m_id = j["id"].get<std::string>();
    }
    if (j.contains("name")) {
        node.m_name = j["name"].get<std::string>();
    }
    if (j.contains("description")) {
        node.m_description = j["description"].get<std::string>();
    }
    if (j.contains("icon")) {
        node.m_icon = j["icon"].get<std::string>();
    }
    if (j.contains("flavor_text")) {
        node.m_flavorText = j["flavor_text"].get<std::string>();
    }

    // Classification
    if (j.contains("category")) {
        node.m_category = StringToTechCategory(j["category"].get<std::string>());
    }
    if (j.contains("age_requirement")) {
        node.m_ageRequirement = StringToTechAge(j["age_requirement"].get<std::string>());
    }
    if (j.contains("tier")) {
        node.m_tier = j["tier"].get<int>();
    }
    if (j.contains("tags")) {
        node.m_tags = j["tags"].get<std::vector<std::string>>();
    }

    // Prerequisites
    if (j.contains("prerequisites")) {
        node.m_prerequisites = j["prerequisites"].get<std::vector<std::string>>();
    }
    if (j.contains("optional_prerequisites")) {
        node.m_optionalPrereqs = j["optional_prerequisites"].get<std::vector<std::string>>();
    }
    if (j.contains("optional_required_count")) {
        node.m_optionalRequiredCount = j["optional_required_count"].get<int>();
    }
    if (j.contains("exclusive_with")) {
        node.m_exclusiveWith = j["exclusive_with"].get<std::vector<std::string>>();
    }

    // Cost
    if (j.contains("cost")) {
        node.m_cost = TechCost::FromJson(j["cost"]);
    }

    // Unlocks
    if (j.contains("unlocks")) {
        node.m_unlocks = TechUnlocks::FromJson(j["unlocks"]);
    }

    // Modifiers
    if (j.contains("modifiers")) {
        for (const auto& modJson : j["modifiers"]) {
            node.m_modifiers.push_back(TechModifier::FromJson(modJson));
        }
    }

    // Scripts
    if (j.contains("events")) {
        node.m_scriptEvents = TechScriptEvents::FromJson(j["events"]);
    }

    // Position
    if (j.contains("position")) {
        node.m_position = TechPosition::FromJson(j["position"]);
    }

    // Culture restrictions
    if (j.contains("available_to_cultures")) {
        node.m_availableToCultures = j["available_to_cultures"].get<std::vector<std::string>>();
    }
    if (j.contains("universal")) {
        node.m_isUniversal = j["universal"].get<bool>();
    }

    // Special properties
    if (j.contains("repeatable")) {
        node.m_repeatable = j["repeatable"].get<bool>();
    }
    if (j.contains("max_research_count")) {
        node.m_maxResearchCount = j["max_research_count"].get<int>();
    }
    if (j.contains("hidden")) {
        node.m_hidden = j["hidden"].get<bool>();
    }
    if (j.contains("can_be_lost")) {
        node.m_canBeLost = j["can_be_lost"].get<bool>();
    }
    if (j.contains("loss_chance")) {
        node.m_lossChance = j["loss_chance"].get<float>();
    }
    if (j.contains("key_tech")) {
        node.m_isKeyTech = j["key_tech"].get<bool>();
    }

    return node;
}

bool TechNode::LoadFromFile(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json j;
        file >> j;
        *this = FromJson(j);
        return true;
    } catch (...) {
        return false;
    }
}

bool TechNode::SaveToFile(const std::string& filePath) const {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        file << ToJson().dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> TechNode::Validate() const {
    std::vector<std::string> errors;

    if (m_id.empty()) {
        errors.push_back("Tech node must have an ID");
    }

    if (m_name.empty()) {
        errors.push_back("Tech node '" + m_id + "' must have a name");
    }

    if (m_cost.time < 0.0f) {
        errors.push_back("Tech node '" + m_id + "' has negative research time");
    }

    for (const auto& [resource, amount] : m_cost.resources) {
        if (amount < 0) {
            errors.push_back("Tech node '" + m_id + "' has negative cost for " + resource);
        }
    }

    if (m_lossChance < 0.0f || m_lossChance > 1.0f) {
        errors.push_back("Tech node '" + m_id + "' has invalid loss chance (should be 0.0-1.0)");
    }

    // Check for self-reference in prerequisites
    for (const auto& prereq : m_prerequisites) {
        if (prereq == m_id) {
            errors.push_back("Tech node '" + m_id + "' cannot require itself as a prerequisite");
        }
    }

    return errors;
}

void TechNode::OnCreate() {
    // Execute onCreate script if defined
    // This would be implemented by the TechManager using the script system
}

void TechNode::OnTick(float deltaTime) {
    // Execute onTick script if defined
    (void)deltaTime;
}

void TechNode::OnDestroy() {
    // Execute onDestroy script if defined
}

// ============================================================================
// TechNodeBuilder
// ============================================================================

TechNodeBuilder::TechNodeBuilder(const std::string& id) {
    m_node.SetId(id);
}

TechNodeBuilder& TechNodeBuilder::Name(const std::string& name) {
    m_node.SetName(name);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Description(const std::string& desc) {
    m_node.SetDescription(desc);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Icon(const std::string& icon) {
    m_node.SetIcon(icon);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::FlavorText(const std::string& text) {
    m_node.SetFlavorText(text);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Category(TechCategory cat) {
    m_node.SetCategory(cat);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Age(TechAge age) {
    m_node.SetAgeRequirement(age);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Tier(int tier) {
    m_node.SetTier(tier);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Tag(const std::string& tag) {
    m_node.AddTag(tag);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Prerequisite(const std::string& techId) {
    m_node.AddPrerequisite(techId);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Prerequisites(const std::vector<std::string>& prereqs) {
    m_node.SetPrerequisites(prereqs);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::ExclusiveWith(const std::string& techId) {
    auto exclusive = m_node.GetExclusiveWith();
    exclusive.push_back(techId);
    m_node.SetExclusiveWith(exclusive);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Cost(const std::string& resource, int amount) {
    auto cost = m_node.GetCost();
    cost.SetResourceCost(resource, amount);
    m_node.SetCost(cost);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::ResearchTime(float time) {
    m_node.SetResearchTime(time);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::UnlockBuilding(const std::string& buildingId) {
    auto unlocks = m_node.GetUnlocks();
    unlocks.buildings.push_back(buildingId);
    m_node.SetUnlocks(unlocks);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::UnlockUnit(const std::string& unitId) {
    auto unlocks = m_node.GetUnlocks();
    unlocks.units.push_back(unitId);
    m_node.SetUnlocks(unlocks);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::UnlockAbility(const std::string& abilityId) {
    auto unlocks = m_node.GetUnlocks();
    unlocks.abilities.push_back(abilityId);
    m_node.SetUnlocks(unlocks);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::UnlockUpgrade(const std::string& upgradeId) {
    auto unlocks = m_node.GetUnlocks();
    unlocks.upgrades.push_back(upgradeId);
    m_node.SetUnlocks(unlocks);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::UnlockSpell(const std::string& spellId) {
    auto unlocks = m_node.GetUnlocks();
    unlocks.spells.push_back(spellId);
    m_node.SetUnlocks(unlocks);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Modifier(const TechModifier& mod) {
    m_node.AddModifier(mod);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::FlatBonus(const std::string& stat, float value, TargetScope scope) {
    m_node.AddModifier(TechModifier::FlatBonus(stat, value, std::move(scope)));
    return *this;
}

TechNodeBuilder& TechNodeBuilder::PercentBonus(const std::string& stat, float percent, TargetScope scope) {
    m_node.AddModifier(TechModifier::PercentBonus(stat, percent, std::move(scope)));
    return *this;
}

TechNodeBuilder& TechNodeBuilder::OnResearchComplete(const std::string& script) {
    auto events = m_node.GetScriptEvents();
    events.onResearchComplete = script;
    m_node.SetScriptEvents(events);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::OnCreate(const std::string& script) {
    auto events = m_node.GetScriptEvents();
    events.onCreate = script;
    m_node.SetScriptEvents(events);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::OnTick(const std::string& script) {
    auto events = m_node.GetScriptEvents();
    events.onTick = script;
    m_node.SetScriptEvents(events);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::OnDestroy(const std::string& script) {
    auto events = m_node.GetScriptEvents();
    events.onDestroy = script;
    m_node.SetScriptEvents(events);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Position(float x, float y) {
    TechPosition pos = m_node.GetPosition();
    pos.x = x;
    pos.y = y;
    m_node.SetPosition(pos);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::GridPosition(int row, int column) {
    TechPosition pos = m_node.GetPosition();
    pos.row = row;
    pos.column = column;
    m_node.SetPosition(pos);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Culture(const std::string& culture) {
    auto cultures = m_node.GetAvailableToCultures();
    cultures.push_back(culture);
    m_node.SetAvailableToCultures(cultures);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Universal() {
    m_node.SetUniversal(true);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Repeatable(int maxCount) {
    m_node.SetRepeatable(true);
    m_node.SetMaxResearchCount(maxCount);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::Hidden() {
    m_node.SetHidden(true);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::CannotBeLost() {
    m_node.SetCanBeLost(false);
    return *this;
}

TechNodeBuilder& TechNodeBuilder::KeyTech() {
    m_node.SetKeyTech(true);
    return *this;
}

TechNode TechNodeBuilder::Build() const {
    return m_node;
}

} // namespace TechTree
} // namespace Vehement
