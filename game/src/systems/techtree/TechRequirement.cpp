#include "TechRequirement.hpp"
#include <algorithm>
#include <sstream>

namespace Vehement {
namespace TechTree {

// ============================================================================
// TechRequirement
// ============================================================================

nlohmann::json TechRequirement::ToJson() const {
    nlohmann::json j;

    j["type"] = RequirementTypeToString(type);
    j["target"] = target;

    if (amount != 1) {
        j["amount"] = amount;
    }

    if (level > 0) {
        j["level"] = level;
    }

    if (!category.empty()) {
        j["category"] = category;
    }

    if (!scriptPath.empty()) {
        j["script"] = scriptPath;
    }

    if (!description.empty()) {
        j["description"] = description;
    }

    if (optional) {
        j["optional"] = true;
    }

    return j;
}

TechRequirement TechRequirement::FromJson(const nlohmann::json& j) {
    TechRequirement req;

    if (j.contains("type")) {
        req.type = StringToRequirementType(j["type"].get<std::string>());
    }

    if (j.contains("target")) {
        req.target = j["target"].get<std::string>();
    }

    if (j.contains("amount")) {
        req.amount = j["amount"].get<int>();
    }

    if (j.contains("level")) {
        req.level = j["level"].get<int>();
    }

    if (j.contains("category")) {
        req.category = j["category"].get<std::string>();
    }

    if (j.contains("script")) {
        req.scriptPath = j["script"].get<std::string>();
    }

    if (j.contains("description")) {
        req.description = j["description"].get<std::string>();
    }

    if (j.contains("optional")) {
        req.optional = j["optional"].get<bool>();
    }

    return req;
}

TechRequirement TechRequirement::Resource(const std::string& resourceType, int amount) {
    TechRequirement req;
    req.type = RequirementType::Resource;
    req.target = resourceType;
    req.amount = amount;
    return req;
}

TechRequirement TechRequirement::Building(const std::string& buildingId) {
    TechRequirement req;
    req.type = RequirementType::Building;
    req.target = buildingId;
    req.amount = 1;
    return req;
}

TechRequirement TechRequirement::BuildingAtLevel(const std::string& buildingId, int level) {
    TechRequirement req;
    req.type = RequirementType::BuildingLevel;
    req.target = buildingId;
    req.level = level;
    return req;
}

TechRequirement TechRequirement::Tech(const std::string& techId) {
    TechRequirement req;
    req.type = RequirementType::Tech;
    req.target = techId;
    return req;
}

TechRequirement TechRequirement::TechsFromCategory(const std::string& category, int count) {
    TechRequirement req;
    req.type = RequirementType::TechCount;
    req.category = category;
    req.amount = count;
    return req;
}

TechRequirement TechRequirement::MinAge(TechAge age) {
    TechRequirement req;
    req.type = RequirementType::Age;
    req.target = TechAgeToShortString(age);
    req.amount = static_cast<int>(age);
    return req;
}

TechRequirement TechRequirement::MinPopulation(int population) {
    TechRequirement req;
    req.type = RequirementType::Population;
    req.amount = population;
    return req;
}

TechRequirement TechRequirement::NotTech(const std::string& techId) {
    TechRequirement req;
    req.type = RequirementType::NotTech;
    req.target = techId;
    return req;
}

TechRequirement TechRequirement::Custom(const std::string& scriptPath, const std::string& description) {
    TechRequirement req;
    req.type = RequirementType::Custom;
    req.scriptPath = scriptPath;
    req.description = description;
    return req;
}

// ============================================================================
// RequirementSet
// ============================================================================

RequirementSet::RequirementSet(LogicType logic) : m_logicType(logic) {}

void RequirementSet::AddRequirement(const TechRequirement& req) {
    m_requirements.push_back(req);
}

nlohmann::json RequirementSet::ToJson() const {
    nlohmann::json j;

    if (m_logicType != LogicType::All) {
        switch (m_logicType) {
            case LogicType::Any:
                j["logic"] = "any";
                break;
            case LogicType::Count:
                j["logic"] = "count";
                j["required_count"] = m_requiredCount;
                break;
            default:
                break;
        }
    }

    nlohmann::json reqsArray = nlohmann::json::array();
    for (const auto& req : m_requirements) {
        reqsArray.push_back(req.ToJson());
    }
    j["requirements"] = reqsArray;

    return j;
}

RequirementSet RequirementSet::FromJson(const nlohmann::json& j) {
    RequirementSet set;

    if (j.contains("logic")) {
        std::string logic = j["logic"].get<std::string>();
        if (logic == "any") {
            set.m_logicType = LogicType::Any;
        } else if (logic == "count") {
            set.m_logicType = LogicType::Count;
        } else {
            set.m_logicType = LogicType::All;
        }
    }

    if (j.contains("required_count")) {
        set.m_requiredCount = j["required_count"].get<int>();
    }

    if (j.contains("requirements")) {
        for (const auto& reqJson : j["requirements"]) {
            set.m_requirements.push_back(TechRequirement::FromJson(reqJson));
        }
    }

    return set;
}

// ============================================================================
// RequirementChecker
// ============================================================================

RequirementCheckResult RequirementChecker::CheckRequirement(
    const TechRequirement& req,
    const IRequirementContext& context) {

    switch (req.type) {
        case RequirementType::Resource: {
            int current = context.GetResourceAmount(req.target);
            if (current >= req.amount) {
                return RequirementCheckResult::Success(req.type, req.target);
            }
            return RequirementCheckResult::Failure(
                req.type, req.target, req.amount, current,
                "Need " + std::to_string(req.amount) + " " + req.target +
                " (have " + std::to_string(current) + ")");
        }

        case RequirementType::Building: {
            int count = context.GetBuildingCount(req.target);
            if (count >= req.amount) {
                return RequirementCheckResult::Success(req.type, req.target);
            }
            return RequirementCheckResult::Failure(
                req.type, req.target, req.amount, count,
                "Need " + req.target + " building");
        }

        case RequirementType::BuildingLevel: {
            int level = context.GetBuildingLevel(req.target);
            if (level >= req.level) {
                return RequirementCheckResult::Success(req.type, req.target);
            }
            return RequirementCheckResult::Failure(
                req.type, req.target, req.level, level,
                "Need " + req.target + " at level " + std::to_string(req.level));
        }

        case RequirementType::Tech: {
            if (context.HasTech(req.target)) {
                return RequirementCheckResult::Success(req.type, req.target);
            }
            return RequirementCheckResult::Failure(
                req.type, req.target, 1, 0,
                "Need to research " + req.target);
        }

        case RequirementType::TechCount: {
            int count = context.GetResearchedTechCountInCategory(req.category);
            if (count >= req.amount) {
                return RequirementCheckResult::Success(req.type, req.category);
            }
            return RequirementCheckResult::Failure(
                req.type, req.category, req.amount, count,
                "Need " + std::to_string(req.amount) + " " + req.category + " techs");
        }

        case RequirementType::Age: {
            int currentAge = static_cast<int>(context.GetCurrentAge());
            if (currentAge >= req.amount) {
                return RequirementCheckResult::Success(req.type, req.target);
            }
            return RequirementCheckResult::Failure(
                req.type, req.target, req.amount, currentAge,
                "Need to reach " + req.target);
        }

        case RequirementType::Population: {
            int pop = context.GetCurrentPopulation();
            if (pop >= req.amount) {
                return RequirementCheckResult::Success(req.type, "population");
            }
            return RequirementCheckResult::Failure(
                req.type, "population", req.amount, pop,
                "Need " + std::to_string(req.amount) + " population");
        }

        case RequirementType::Unit: {
            if (context.HasUnit(req.target)) {
                return RequirementCheckResult::Success(req.type, req.target);
            }
            return RequirementCheckResult::Failure(
                req.type, req.target, 1, 0,
                "Need " + req.target + " unit");
        }

        case RequirementType::UnitCount: {
            int count = context.GetUnitCount(req.target);
            if (count >= req.amount) {
                return RequirementCheckResult::Success(req.type, req.target);
            }
            return RequirementCheckResult::Failure(
                req.type, req.target, req.amount, count,
                "Need " + std::to_string(req.amount) + " " + req.target + " units");
        }

        case RequirementType::Territory: {
            int size = context.GetTerritorySize();
            if (size >= req.amount) {
                return RequirementCheckResult::Success(req.type, "territory");
            }
            return RequirementCheckResult::Failure(
                req.type, "territory", req.amount, size,
                "Need territory size " + std::to_string(req.amount));
        }

        case RequirementType::Score: {
            int score = context.GetScore();
            if (score >= req.amount) {
                return RequirementCheckResult::Success(req.type, "score");
            }
            return RequirementCheckResult::Failure(
                req.type, "score", req.amount, score,
                "Need score " + std::to_string(req.amount));
        }

        case RequirementType::Time: {
            float time = context.GetGameTime();
            if (time >= static_cast<float>(req.amount)) {
                return RequirementCheckResult::Success(req.type, "time");
            }
            return RequirementCheckResult::Failure(
                req.type, "time", req.amount, static_cast<int>(time),
                "Need to wait " + std::to_string(req.amount) + " seconds");
        }

        case RequirementType::NotTech: {
            if (!context.HasTech(req.target)) {
                return RequirementCheckResult::Success(req.type, req.target);
            }
            return RequirementCheckResult::Failure(
                req.type, req.target, 0, 1,
                "Cannot have " + req.target + " researched");
        }

        case RequirementType::NotBuilding: {
            if (!context.HasBuilding(req.target)) {
                return RequirementCheckResult::Success(req.type, req.target);
            }
            return RequirementCheckResult::Failure(
                req.type, req.target, 0, 1,
                "Cannot have " + req.target + " built");
        }

        case RequirementType::Custom: {
            if (context.EvaluateCustomRequirement(req.scriptPath)) {
                return RequirementCheckResult::Success(req.type, req.scriptPath);
            }
            return RequirementCheckResult::Failure(
                req.type, req.scriptPath, 1, 0,
                req.description.empty() ? "Custom requirement not met" : req.description);
        }

        default:
            return RequirementCheckResult::Failure(
                req.type, "", 0, 0, "Unknown requirement type");
    }
}

RequirementCheckResults RequirementChecker::CheckTechRequirements(
    const TechNode& tech,
    const IRequirementContext& context) {

    RequirementCheckResults results;
    results.allMet = true;

    // Check age requirement
    auto ageResults = CheckAgeRequirement(tech, context);
    results.results.insert(results.results.end(),
        ageResults.results.begin(), ageResults.results.end());
    if (!ageResults.allMet) {
        results.allMet = false;
    }

    // Check culture availability
    auto cultureResults = CheckCultureAvailability(tech, context);
    results.results.insert(results.results.end(),
        cultureResults.results.begin(), cultureResults.results.end());
    if (!cultureResults.allMet) {
        results.allMet = false;
    }

    // Check prerequisites
    auto prereqResults = CheckPrerequisites(tech, context);
    results.results.insert(results.results.end(),
        prereqResults.results.begin(), prereqResults.results.end());
    if (!prereqResults.allMet) {
        results.allMet = false;
    }

    // Check exclusivity
    auto exclusiveResults = CheckExclusivity(tech, context);
    results.results.insert(results.results.end(),
        exclusiveResults.results.begin(), exclusiveResults.results.end());
    if (!exclusiveResults.allMet) {
        results.allMet = false;
    }

    // Check cost
    auto costResults = CheckCanAfford(tech, context);
    results.results.insert(results.results.end(),
        costResults.results.begin(), costResults.results.end());
    if (!costResults.allMet) {
        results.allMet = false;
    }

    return results;
}

RequirementCheckResults RequirementChecker::CheckRequirementSet(
    const RequirementSet& reqSet,
    const IRequirementContext& context) {

    RequirementCheckResults results;

    if (reqSet.IsEmpty()) {
        results.allMet = true;
        return results;
    }

    int metCount = 0;

    for (const auto& req : reqSet.GetRequirements()) {
        auto result = CheckRequirement(req, context);
        results.results.push_back(result);
        if (result.met) {
            metCount++;
        }
    }

    switch (reqSet.GetLogicType()) {
        case RequirementSet::LogicType::All:
            results.allMet = (metCount == static_cast<int>(reqSet.GetRequirements().size()));
            break;

        case RequirementSet::LogicType::Any:
            results.allMet = (metCount > 0);
            break;

        case RequirementSet::LogicType::Count:
            results.allMet = (metCount >= reqSet.GetRequiredCount());
            break;
    }

    return results;
}

RequirementCheckResults RequirementChecker::CheckCanAfford(
    const TechNode& tech,
    const IRequirementContext& context) {

    RequirementCheckResults results;
    results.allMet = true;

    const auto& cost = tech.GetCost();

    for (const auto& [resource, amount] : cost.resources) {
        auto req = TechRequirement::Resource(resource, amount);
        auto result = CheckRequirement(req, context);
        results.results.push_back(result);
        if (!result.met) {
            results.allMet = false;
        }
    }

    return results;
}

RequirementCheckResults RequirementChecker::CheckPrerequisites(
    const TechNode& tech,
    const IRequirementContext& context) {

    RequirementCheckResults results;
    results.allMet = true;

    // Check required prerequisites (all must be met)
    for (const auto& prereqId : tech.GetPrerequisites()) {
        auto req = TechRequirement::Tech(prereqId);
        auto result = CheckRequirement(req, context);
        results.results.push_back(result);
        if (!result.met) {
            results.allMet = false;
        }
    }

    // Check optional prerequisites (N must be met)
    const auto& optionalPrereqs = tech.GetOptionalPrereqs();
    if (!optionalPrereqs.empty()) {
        int metCount = 0;
        int requiredCount = tech.GetOptionalRequiredCount();

        for (const auto& prereqId : optionalPrereqs) {
            if (context.HasTech(prereqId)) {
                metCount++;
            }
        }

        if (metCount < requiredCount) {
            results.allMet = false;
            RequirementCheckResult r;
            r.met = false;
            r.type = RequirementType::TechCount;
            r.required = requiredCount;
            r.current = metCount;
            r.message = "Need " + std::to_string(requiredCount) +
                       " of optional prerequisites (" + std::to_string(metCount) + " met)";
            results.results.push_back(r);
        }
    }

    return results;
}

RequirementCheckResults RequirementChecker::CheckExclusivity(
    const TechNode& tech,
    const IRequirementContext& context) {

    RequirementCheckResults results;
    results.allMet = true;

    for (const auto& exclusiveTech : tech.GetExclusiveWith()) {
        if (context.HasTech(exclusiveTech)) {
            results.allMet = false;
            RequirementCheckResult r;
            r.met = false;
            r.type = RequirementType::NotTech;
            r.target = exclusiveTech;
            r.message = "Cannot research - already have exclusive tech " + exclusiveTech;
            results.results.push_back(r);
        }
    }

    return results;
}

RequirementCheckResults RequirementChecker::CheckAgeRequirement(
    const TechNode& tech,
    const IRequirementContext& context) {

    RequirementCheckResults results;
    results.allMet = true;

    TechAge currentAge = context.GetCurrentAge();
    TechAge requiredAge = tech.GetAgeRequirement();

    if (static_cast<int>(currentAge) < static_cast<int>(requiredAge)) {
        results.allMet = false;
        RequirementCheckResult r;
        r.met = false;
        r.type = RequirementType::Age;
        r.target = TechAgeToShortString(requiredAge);
        r.required = static_cast<int>(requiredAge);
        r.current = static_cast<int>(currentAge);
        r.message = "Need to reach " + std::string(TechAgeToString(requiredAge));
        results.results.push_back(r);
    }

    return results;
}

RequirementCheckResults RequirementChecker::CheckCultureAvailability(
    const TechNode& tech,
    const IRequirementContext& context) {

    RequirementCheckResults results;
    results.allMet = true;

    if (!tech.IsAvailableToCulture(context.GetCulture())) {
        results.allMet = false;
        RequirementCheckResult r;
        r.met = false;
        r.type = RequirementType::Custom;
        r.message = "Technology not available to your culture";
        results.results.push_back(r);
    }

    return results;
}

std::vector<std::string> RequirementChecker::GetMissingRequirements(
    const TechNode& tech,
    const IRequirementContext& context) {

    auto results = CheckTechRequirements(tech, context);

    std::vector<std::string> missing;
    for (const auto& r : results.results) {
        if (!r.met) {
            missing.push_back(r.message);
        }
    }

    return missing;
}

// ============================================================================
// ExclusiveTechGroup
// ============================================================================

ExclusiveTechGroup::ExclusiveTechGroup(const std::string& id) : m_id(id) {}

std::optional<std::string> ExclusiveTechGroup::GetResearchedTech(
    const IRequirementContext& context) const {

    for (const auto& techId : m_techs) {
        if (context.HasTech(techId)) {
            return techId;
        }
    }
    return std::nullopt;
}

std::vector<std::string> ExclusiveTechGroup::GetBlockedTechs(
    const std::string& researchedTechId) const {

    std::vector<std::string> blocked;
    for (const auto& techId : m_techs) {
        if (techId != researchedTechId) {
            blocked.push_back(techId);
        }
    }
    return blocked;
}

nlohmann::json ExclusiveTechGroup::ToJson() const {
    nlohmann::json j;
    j["id"] = m_id;
    j["name"] = m_name;
    if (!m_description.empty()) {
        j["description"] = m_description;
    }
    j["techs"] = std::vector<std::string>(m_techs.begin(), m_techs.end());
    return j;
}

ExclusiveTechGroup ExclusiveTechGroup::FromJson(const nlohmann::json& j) {
    ExclusiveTechGroup group;

    if (j.contains("id")) {
        group.m_id = j["id"].get<std::string>();
    }
    if (j.contains("name")) {
        group.m_name = j["name"].get<std::string>();
    }
    if (j.contains("description")) {
        group.m_description = j["description"].get<std::string>();
    }
    if (j.contains("techs")) {
        for (const auto& tech : j["techs"]) {
            group.m_techs.insert(tech.get<std::string>());
        }
    }

    return group;
}

// ============================================================================
// RequirementRegistry
// ============================================================================

RequirementRegistry& RequirementRegistry::Instance() {
    static RequirementRegistry instance;
    return instance;
}

void RequirementRegistry::RegisterExclusiveGroup(const ExclusiveTechGroup& group) {
    m_exclusiveGroups[group.GetId()] = group;

    // Update tech to group mapping
    for (const auto& techId : group.GetTechs()) {
        m_techToGroups[techId].push_back(group.GetId());
    }
}

const ExclusiveTechGroup* RequirementRegistry::GetExclusiveGroup(const std::string& groupId) const {
    auto it = m_exclusiveGroups.find(groupId);
    return it != m_exclusiveGroups.end() ? &it->second : nullptr;
}

std::vector<const ExclusiveTechGroup*> RequirementRegistry::GetGroupsForTech(
    const std::string& techId) const {

    std::vector<const ExclusiveTechGroup*> groups;

    auto it = m_techToGroups.find(techId);
    if (it != m_techToGroups.end()) {
        for (const auto& groupId : it->second) {
            if (auto* group = GetExclusiveGroup(groupId)) {
                groups.push_back(group);
            }
        }
    }

    return groups;
}

std::vector<std::string> RequirementRegistry::GetBlockedByResearching(
    const std::string& techId) const {

    std::vector<std::string> blocked;

    for (const auto* group : GetGroupsForTech(techId)) {
        auto groupBlocked = group->GetBlockedTechs(techId);
        blocked.insert(blocked.end(), groupBlocked.begin(), groupBlocked.end());
    }

    // Remove duplicates
    std::sort(blocked.begin(), blocked.end());
    blocked.erase(std::unique(blocked.begin(), blocked.end()), blocked.end());

    return blocked;
}

void RequirementRegistry::LoadFromJson(const nlohmann::json& j) {
    if (j.contains("exclusive_groups")) {
        for (const auto& groupJson : j["exclusive_groups"]) {
            RegisterExclusiveGroup(ExclusiveTechGroup::FromJson(groupJson));
        }
    }
}

void RequirementRegistry::Clear() {
    m_exclusiveGroups.clear();
    m_techToGroups.clear();
}

} // namespace TechTree
} // namespace Vehement
