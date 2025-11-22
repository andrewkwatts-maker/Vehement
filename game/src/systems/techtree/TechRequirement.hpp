#pragma once

/**
 * @file TechRequirement.hpp
 * @brief Technology requirement checking system
 *
 * Provides comprehensive requirement checking for researching technologies:
 * - Resource requirements
 * - Building requirements
 * - Tech prerequisites
 * - Age requirements
 * - Exclusive tech handling (can only pick one)
 * - Custom script-based requirements
 */

#include "TechNode.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace TechTree {

// Forward declarations
class TechTree;
class TechManager;

// ============================================================================
// Requirement Types
// ============================================================================

/**
 * @brief Types of requirements that can gate technology research
 */
enum class RequirementType : uint8_t {
    Resource,           ///< Requires specific resources
    Building,           ///< Requires a building to exist
    BuildingLevel,      ///< Requires a building at a specific level
    Tech,               ///< Requires another tech to be researched
    TechCount,          ///< Requires N techs from a category/list
    Age,                ///< Requires a minimum age
    Population,         ///< Requires minimum population
    Unit,               ///< Requires a unit type to exist
    UnitCount,          ///< Requires N units of a type
    Territory,          ///< Requires territory size
    Score,              ///< Requires minimum score
    Time,               ///< Requires minimum game time
    NotTech,            ///< Must NOT have a tech (exclusive)
    NotBuilding,        ///< Must NOT have a building
    Custom,             ///< Custom script requirement

    COUNT
};

[[nodiscard]] inline const char* RequirementTypeToString(RequirementType type) {
    switch (type) {
        case RequirementType::Resource:      return "resource";
        case RequirementType::Building:      return "building";
        case RequirementType::BuildingLevel: return "building_level";
        case RequirementType::Tech:          return "tech";
        case RequirementType::TechCount:     return "tech_count";
        case RequirementType::Age:           return "age";
        case RequirementType::Population:    return "population";
        case RequirementType::Unit:          return "unit";
        case RequirementType::UnitCount:     return "unit_count";
        case RequirementType::Territory:     return "territory";
        case RequirementType::Score:         return "score";
        case RequirementType::Time:          return "time";
        case RequirementType::NotTech:       return "not_tech";
        case RequirementType::NotBuilding:   return "not_building";
        case RequirementType::Custom:        return "custom";
        default:                             return "unknown";
    }
}

[[nodiscard]] inline RequirementType StringToRequirementType(const std::string& str) {
    if (str == "resource")       return RequirementType::Resource;
    if (str == "building")       return RequirementType::Building;
    if (str == "building_level") return RequirementType::BuildingLevel;
    if (str == "tech")           return RequirementType::Tech;
    if (str == "tech_count")     return RequirementType::TechCount;
    if (str == "age")            return RequirementType::Age;
    if (str == "population")     return RequirementType::Population;
    if (str == "unit")           return RequirementType::Unit;
    if (str == "unit_count")     return RequirementType::UnitCount;
    if (str == "territory")      return RequirementType::Territory;
    if (str == "score")          return RequirementType::Score;
    if (str == "time")           return RequirementType::Time;
    if (str == "not_tech")       return RequirementType::NotTech;
    if (str == "not_building")   return RequirementType::NotBuilding;
    if (str == "custom")         return RequirementType::Custom;
    return RequirementType::Tech;
}

// ============================================================================
// Requirement Check Result
// ============================================================================

/**
 * @brief Result of checking a single requirement
 */
struct RequirementCheckResult {
    bool met = false;                   ///< Whether the requirement is met
    RequirementType type;               ///< Type of requirement
    std::string target;                 ///< What was required (resource, tech, etc.)
    int required = 0;                   ///< Required amount
    int current = 0;                    ///< Current amount
    std::string message;                ///< Human-readable failure message

    [[nodiscard]] static RequirementCheckResult Success(RequirementType type, const std::string& target) {
        RequirementCheckResult r;
        r.met = true;
        r.type = type;
        r.target = target;
        return r;
    }

    [[nodiscard]] static RequirementCheckResult Failure(RequirementType type, const std::string& target,
                                                         int required, int current, const std::string& message) {
        RequirementCheckResult r;
        r.met = false;
        r.type = type;
        r.target = target;
        r.required = required;
        r.current = current;
        r.message = message;
        return r;
    }
};

/**
 * @brief Complete result of checking all requirements
 */
struct RequirementCheckResults {
    bool allMet = false;                            ///< Whether all requirements are met
    std::vector<RequirementCheckResult> results;    ///< Individual check results
    std::vector<std::string> errors;                ///< Error messages

    [[nodiscard]] std::vector<RequirementCheckResult> GetFailedRequirements() const {
        std::vector<RequirementCheckResult> failed;
        for (const auto& r : results) {
            if (!r.met) {
                failed.push_back(r);
            }
        }
        return failed;
    }

    [[nodiscard]] std::string GetSummaryMessage() const {
        if (allMet) {
            return "All requirements met";
        }

        std::string msg = "Missing requirements: ";
        bool first = true;
        for (const auto& r : results) {
            if (!r.met) {
                if (!first) msg += ", ";
                msg += r.message;
                first = false;
            }
        }
        return msg;
    }
};

// ============================================================================
// Single Requirement
// ============================================================================

/**
 * @brief A single requirement for researching a technology
 */
struct TechRequirement {
    RequirementType type = RequirementType::Tech;
    std::string target;                 ///< Target ID (tech, building, resource, etc.)
    int amount = 1;                     ///< Required amount
    int level = 0;                      ///< Required level (for building_level)
    std::string category;               ///< Category for tech_count
    std::string scriptPath;             ///< Script for custom requirements
    std::string description;            ///< Human-readable description
    bool optional = false;              ///< If true, this is an optional requirement

    [[nodiscard]] nlohmann::json ToJson() const;
    static TechRequirement FromJson(const nlohmann::json& j);

    // Factory methods
    [[nodiscard]] static TechRequirement Resource(const std::string& resourceType, int amount);
    [[nodiscard]] static TechRequirement Building(const std::string& buildingId);
    [[nodiscard]] static TechRequirement BuildingAtLevel(const std::string& buildingId, int level);
    [[nodiscard]] static TechRequirement Tech(const std::string& techId);
    [[nodiscard]] static TechRequirement TechsFromCategory(const std::string& category, int count);
    [[nodiscard]] static TechRequirement MinAge(TechAge age);
    [[nodiscard]] static TechRequirement MinPopulation(int population);
    [[nodiscard]] static TechRequirement NotTech(const std::string& techId);
    [[nodiscard]] static TechRequirement Custom(const std::string& scriptPath, const std::string& description);
};

// ============================================================================
// Requirement Set
// ============================================================================

/**
 * @brief A set of requirements with AND/OR logic
 */
class RequirementSet {
public:
    enum class LogicType {
        All,    ///< All requirements must be met (AND)
        Any,    ///< Any requirement must be met (OR)
        Count   ///< N requirements must be met
    };

    RequirementSet() = default;
    explicit RequirementSet(LogicType logic);

    void AddRequirement(const TechRequirement& req);
    void SetLogicType(LogicType logic) { m_logicType = logic; }
    void SetRequiredCount(int count) { m_requiredCount = count; }

    [[nodiscard]] LogicType GetLogicType() const { return m_logicType; }
    [[nodiscard]] int GetRequiredCount() const { return m_requiredCount; }
    [[nodiscard]] const std::vector<TechRequirement>& GetRequirements() const { return m_requirements; }

    [[nodiscard]] bool IsEmpty() const { return m_requirements.empty(); }

    [[nodiscard]] nlohmann::json ToJson() const;
    static RequirementSet FromJson(const nlohmann::json& j);

private:
    LogicType m_logicType = LogicType::All;
    int m_requiredCount = 1;
    std::vector<TechRequirement> m_requirements;
};

// ============================================================================
// Requirement Context
// ============================================================================

/**
 * @brief Context data needed to check requirements
 *
 * This interface allows the requirement checker to query game state
 * without directly depending on game systems.
 */
class IRequirementContext {
public:
    virtual ~IRequirementContext() = default;

    // Resource checks
    [[nodiscard]] virtual int GetResourceAmount(const std::string& resourceType) const = 0;
    [[nodiscard]] virtual bool HasResources(const std::map<std::string, int>& resources) const = 0;

    // Building checks
    [[nodiscard]] virtual bool HasBuilding(const std::string& buildingId) const = 0;
    [[nodiscard]] virtual int GetBuildingCount(const std::string& buildingId) const = 0;
    [[nodiscard]] virtual int GetBuildingLevel(const std::string& buildingId) const = 0;

    // Tech checks
    [[nodiscard]] virtual bool HasTech(const std::string& techId) const = 0;
    [[nodiscard]] virtual int GetResearchedTechCount() const = 0;
    [[nodiscard]] virtual int GetResearchedTechCountInCategory(const std::string& category) const = 0;
    [[nodiscard]] virtual std::vector<std::string> GetResearchedTechs() const = 0;

    // Age checks
    [[nodiscard]] virtual TechAge GetCurrentAge() const = 0;

    // Population checks
    [[nodiscard]] virtual int GetCurrentPopulation() const = 0;
    [[nodiscard]] virtual int GetMaxPopulation() const = 0;

    // Unit checks
    [[nodiscard]] virtual bool HasUnit(const std::string& unitType) const = 0;
    [[nodiscard]] virtual int GetUnitCount(const std::string& unitType) const = 0;

    // Territory checks
    [[nodiscard]] virtual int GetTerritorySize() const = 0;

    // Score/time checks
    [[nodiscard]] virtual int GetScore() const = 0;
    [[nodiscard]] virtual float GetGameTime() const = 0;

    // Culture checks
    [[nodiscard]] virtual std::string GetCulture() const = 0;

    // Custom script evaluation
    [[nodiscard]] virtual bool EvaluateCustomRequirement(const std::string& scriptPath) const = 0;
};

/**
 * @brief Default/test implementation of requirement context
 */
class DefaultRequirementContext : public IRequirementContext {
public:
    // Set methods for testing
    void SetResource(const std::string& type, int amount) { m_resources[type] = amount; }
    void SetBuilding(const std::string& id, int count, int level = 1) {
        m_buildings[id] = {count, level};
    }
    void SetTech(const std::string& techId, bool researched = true) {
        if (researched) m_techs.insert(techId);
        else m_techs.erase(techId);
    }
    void SetAge(TechAge age) { m_age = age; }
    void SetPopulation(int pop, int maxPop) { m_population = pop; m_maxPopulation = maxPop; }
    void SetUnit(const std::string& type, int count) { m_units[type] = count; }
    void SetTerritorySize(int size) { m_territorySize = size; }
    void SetScore(int score) { m_score = score; }
    void SetGameTime(float time) { m_gameTime = time; }
    void SetCulture(const std::string& culture) { m_culture = culture; }

    // IRequirementContext implementation
    [[nodiscard]] int GetResourceAmount(const std::string& resourceType) const override {
        auto it = m_resources.find(resourceType);
        return it != m_resources.end() ? it->second : 0;
    }

    [[nodiscard]] bool HasResources(const std::map<std::string, int>& resources) const override {
        for (const auto& [type, amount] : resources) {
            if (GetResourceAmount(type) < amount) return false;
        }
        return true;
    }

    [[nodiscard]] bool HasBuilding(const std::string& buildingId) const override {
        auto it = m_buildings.find(buildingId);
        return it != m_buildings.end() && it->second.first > 0;
    }

    [[nodiscard]] int GetBuildingCount(const std::string& buildingId) const override {
        auto it = m_buildings.find(buildingId);
        return it != m_buildings.end() ? it->second.first : 0;
    }

    [[nodiscard]] int GetBuildingLevel(const std::string& buildingId) const override {
        auto it = m_buildings.find(buildingId);
        return it != m_buildings.end() ? it->second.second : 0;
    }

    [[nodiscard]] bool HasTech(const std::string& techId) const override {
        return m_techs.count(techId) > 0;
    }

    [[nodiscard]] int GetResearchedTechCount() const override {
        return static_cast<int>(m_techs.size());
    }

    [[nodiscard]] int GetResearchedTechCountInCategory(const std::string& category) const override {
        (void)category;
        return 0; // Would need tech registry to implement properly
    }

    [[nodiscard]] std::vector<std::string> GetResearchedTechs() const override {
        return std::vector<std::string>(m_techs.begin(), m_techs.end());
    }

    [[nodiscard]] TechAge GetCurrentAge() const override { return m_age; }
    [[nodiscard]] int GetCurrentPopulation() const override { return m_population; }
    [[nodiscard]] int GetMaxPopulation() const override { return m_maxPopulation; }

    [[nodiscard]] bool HasUnit(const std::string& unitType) const override {
        auto it = m_units.find(unitType);
        return it != m_units.end() && it->second > 0;
    }

    [[nodiscard]] int GetUnitCount(const std::string& unitType) const override {
        auto it = m_units.find(unitType);
        return it != m_units.end() ? it->second : 0;
    }

    [[nodiscard]] int GetTerritorySize() const override { return m_territorySize; }
    [[nodiscard]] int GetScore() const override { return m_score; }
    [[nodiscard]] float GetGameTime() const override { return m_gameTime; }
    [[nodiscard]] std::string GetCulture() const override { return m_culture; }

    [[nodiscard]] bool EvaluateCustomRequirement(const std::string& scriptPath) const override {
        (void)scriptPath;
        return true; // Default to true for testing
    }

private:
    std::unordered_map<std::string, int> m_resources;
    std::unordered_map<std::string, std::pair<int, int>> m_buildings; // count, level
    std::unordered_set<std::string> m_techs;
    TechAge m_age = TechAge::Stone;
    int m_population = 0;
    int m_maxPopulation = 100;
    std::unordered_map<std::string, int> m_units;
    int m_territorySize = 0;
    int m_score = 0;
    float m_gameTime = 0.0f;
    std::string m_culture;
};

// ============================================================================
// Requirement Checker
// ============================================================================

/**
 * @brief Checks technology requirements against game state
 */
class RequirementChecker {
public:
    /**
     * @brief Check if a single requirement is met
     */
    [[nodiscard]] static RequirementCheckResult CheckRequirement(
        const TechRequirement& req,
        const IRequirementContext& context);

    /**
     * @brief Check if all requirements for a tech are met
     */
    [[nodiscard]] static RequirementCheckResults CheckTechRequirements(
        const TechNode& tech,
        const IRequirementContext& context);

    /**
     * @brief Check if a requirement set is satisfied
     */
    [[nodiscard]] static RequirementCheckResults CheckRequirementSet(
        const RequirementSet& reqSet,
        const IRequirementContext& context);

    /**
     * @brief Check if player can afford the tech
     */
    [[nodiscard]] static RequirementCheckResults CheckCanAfford(
        const TechNode& tech,
        const IRequirementContext& context);

    /**
     * @brief Check if all prerequisites are researched
     */
    [[nodiscard]] static RequirementCheckResults CheckPrerequisites(
        const TechNode& tech,
        const IRequirementContext& context);

    /**
     * @brief Check if tech is blocked by exclusive techs
     */
    [[nodiscard]] static RequirementCheckResults CheckExclusivity(
        const TechNode& tech,
        const IRequirementContext& context);

    /**
     * @brief Check if player meets the age requirement
     */
    [[nodiscard]] static RequirementCheckResults CheckAgeRequirement(
        const TechNode& tech,
        const IRequirementContext& context);

    /**
     * @brief Check if tech is available to player's culture
     */
    [[nodiscard]] static RequirementCheckResults CheckCultureAvailability(
        const TechNode& tech,
        const IRequirementContext& context);

    /**
     * @brief Get all missing requirements with detailed info
     */
    [[nodiscard]] static std::vector<std::string> GetMissingRequirements(
        const TechNode& tech,
        const IRequirementContext& context);
};

// ============================================================================
// Exclusive Tech Groups
// ============================================================================

/**
 * @brief Manages groups of mutually exclusive technologies
 *
 * When a player researches a tech from an exclusive group,
 * they cannot research other techs in that group.
 */
class ExclusiveTechGroup {
public:
    ExclusiveTechGroup() = default;
    explicit ExclusiveTechGroup(const std::string& id);

    [[nodiscard]] const std::string& GetId() const { return m_id; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }
    [[nodiscard]] const std::string& GetDescription() const { return m_description; }

    void SetName(const std::string& name) { m_name = name; }
    void SetDescription(const std::string& desc) { m_description = desc; }

    void AddTech(const std::string& techId) { m_techs.insert(techId); }
    void RemoveTech(const std::string& techId) { m_techs.erase(techId); }

    [[nodiscard]] bool ContainsTech(const std::string& techId) const {
        return m_techs.count(techId) > 0;
    }

    [[nodiscard]] const std::unordered_set<std::string>& GetTechs() const { return m_techs; }

    /**
     * @brief Check if any tech in this group is already researched
     * @param context Game context for checking research state
     * @return ID of researched tech, or empty if none
     */
    [[nodiscard]] std::optional<std::string> GetResearchedTech(
        const IRequirementContext& context) const;

    /**
     * @brief Get all blocked techs if a tech from this group is researched
     */
    [[nodiscard]] std::vector<std::string> GetBlockedTechs(
        const std::string& researchedTechId) const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static ExclusiveTechGroup FromJson(const nlohmann::json& j);

private:
    std::string m_id;
    std::string m_name;
    std::string m_description;
    std::unordered_set<std::string> m_techs;
};

// ============================================================================
// Requirement Registry
// ============================================================================

/**
 * @brief Manages all requirement-related data
 */
class RequirementRegistry {
public:
    static RequirementRegistry& Instance();

    /**
     * @brief Register an exclusive tech group
     */
    void RegisterExclusiveGroup(const ExclusiveTechGroup& group);

    /**
     * @brief Get an exclusive group by ID
     */
    [[nodiscard]] const ExclusiveTechGroup* GetExclusiveGroup(const std::string& groupId) const;

    /**
     * @brief Get all exclusive groups containing a tech
     */
    [[nodiscard]] std::vector<const ExclusiveTechGroup*> GetGroupsForTech(
        const std::string& techId) const;

    /**
     * @brief Check if researching a tech would block others
     */
    [[nodiscard]] std::vector<std::string> GetBlockedByResearching(
        const std::string& techId) const;

    /**
     * @brief Load exclusive groups from JSON
     */
    void LoadFromJson(const nlohmann::json& j);

    /**
     * @brief Clear all registered groups
     */
    void Clear();

private:
    RequirementRegistry() = default;

    std::unordered_map<std::string, ExclusiveTechGroup> m_exclusiveGroups;
    std::unordered_map<std::string, std::vector<std::string>> m_techToGroups;
};

} // namespace TechTree
} // namespace Vehement
