#include "BuildingConfig.hpp"
#include <nlohmann/json.hpp>

namespace Vehement {
namespace Config {

using json = nlohmann::json;

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

glm::ivec2 ParseIVec2(const json& j) {
    if (j.is_array() && j.size() >= 2) {
        return glm::ivec2(j[0].get<int>(), j[1].get<int>());
    }
    return glm::ivec2(1);
}

glm::vec3 ParseVec3(const json& j) {
    if (j.is_array() && j.size() >= 3) {
        return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    }
    return glm::vec3(1.0f);
}

ResourceCost ParseResourceCost(const json& j) {
    ResourceCost cost;
    for (auto& [key, value] : j.items()) {
        ResourceType type = StringToResourceType(key);
        if (type != ResourceType::None) {
            cost.SetCost(type, value.get<int>());
        }
    }
    return cost;
}

json ResourceCostToJson(const ResourceCost& cost) {
    json j;
    for (const auto& [type, amount] : cost.resources) {
        j[ResourceTypeToString(type)] = amount;
    }
    return j;
}

BuildingFootprint ParseFootprint(const json& j) {
    BuildingFootprint footprint;

    if (j.contains("gridType")) {
        footprint.gridType = StringToGridType(j["gridType"].get<std::string>());
    }

    if (j.contains("size")) {
        footprint.size = ParseIVec2(j["size"]);
    }

    if (j.contains("occupiedCells") && j["occupiedCells"].is_array()) {
        for (const auto& cell : j["occupiedCells"]) {
            footprint.occupiedCells.push_back(ParseIVec2(cell));
        }
    }

    if (j.contains("entrances") && j["entrances"].is_array()) {
        for (const auto& entrance : j["entrances"]) {
            footprint.entrancePositions.push_back(ParseIVec2(entrance));
        }
    }

    if (j.contains("visualSize")) {
        footprint.visualSize = ParseVec3(j["visualSize"]);
    }

    return footprint;
}

ConstructionStage ParseConstructionStage(const json& j) {
    ConstructionStage stage;

    if (j.contains("name")) stage.name = j["name"].get<std::string>();
    if (j.contains("model")) stage.modelPath = j["model"].get<std::string>();
    if (j.contains("progressStart")) stage.progressStart = j["progressStart"].get<float>();
    if (j.contains("progressEnd")) stage.progressEnd = j["progressEnd"].get<float>();
    if (j.contains("effect")) stage.effectPath = j["effect"].get<std::string>();

    return stage;
}

ProductionCapability ParseProductionCapability(const json& j) {
    ProductionCapability cap;

    if (j.contains("type")) {
        std::string typeStr = j["type"].get<std::string>();
        if (typeStr == "unit") cap.type = ProductionCapability::ProductionType::Unit;
        else if (typeStr == "resource") cap.type = ProductionCapability::ProductionType::Resource;
        else if (typeStr == "research") cap.type = ProductionCapability::ProductionType::Research;
        else if (typeStr == "item") cap.type = ProductionCapability::ProductionType::Item;
    }

    if (j.contains("output")) cap.outputId = j["output"].get<std::string>();
    if (j.contains("outputId")) cap.outputId = j["outputId"].get<std::string>();
    if (j.contains("amount")) cap.outputAmount = j["amount"].get<int>();
    if (j.contains("time")) cap.productionTime = j["time"].get<float>();
    if (j.contains("productionTime")) cap.productionTime = j["productionTime"].get<float>();
    if (j.contains("maxQueue")) cap.maxQueue = j["maxQueue"].get<int>();

    if (j.contains("cost") && j["cost"].is_object()) {
        cap.cost = ParseResourceCost(j["cost"]);
    }

    if (j.contains("requiredTechs") && j["requiredTechs"].is_array()) {
        for (const auto& tech : j["requiredTechs"]) {
            cap.requiredTechs.push_back(tech.get<std::string>());
        }
    }

    if (j.contains("requiredLevel")) {
        cap.requiredBuildingLevel = j["requiredLevel"].get<int>();
    }

    return cap;
}

BuildingUpgrade ParseBuildingUpgrade(const json& j) {
    BuildingUpgrade upgrade;

    if (j.contains("id")) upgrade.upgradeId = j["id"].get<std::string>();
    if (j.contains("name")) upgrade.name = j["name"].get<std::string>();
    if (j.contains("description")) upgrade.description = j["description"].get<std::string>();

    if (j.contains("targetLevel")) upgrade.targetLevel = j["targetLevel"].get<int>();
    if (j.contains("transformsTo")) upgrade.transformsTo = j["transformsTo"].get<std::string>();

    if (j.contains("cost") && j["cost"].is_object()) {
        upgrade.cost = ParseResourceCost(j["cost"]);
    }

    if (j.contains("time")) upgrade.upgradeTime = j["time"].get<float>();

    if (j.contains("requiredTechs") && j["requiredTechs"].is_array()) {
        for (const auto& tech : j["requiredTechs"]) {
            upgrade.requiredTechs.push_back(tech.get<std::string>());
        }
    }

    if (j.contains("healthMultiplier")) upgrade.healthMultiplier = j["healthMultiplier"].get<float>();
    if (j.contains("productionMultiplier")) upgrade.productionMultiplier = j["productionMultiplier"].get<float>();
    if (j.contains("capacityMultiplier")) upgrade.capacityMultiplier = j["capacityMultiplier"].get<float>();

    return upgrade;
}

} // anonymous namespace

// ============================================================================
// BuildingConfig Implementation
// ============================================================================

void BuildingConfig::ParseTypeSpecificFields(const std::string& jsonContent) {
    try {
        json j = json::parse(StripComments(jsonContent));

        // Parse footprint
        if (j.contains("footprint")) {
            m_footprint = ParseFootprint(j["footprint"]);
        } else {
            // Simple size shorthand
            if (j.contains("size")) {
                m_footprint.size = ParseIVec2(j["size"]);
            }
            if (j.contains("gridType")) {
                m_footprint.gridType = StringToGridType(j["gridType"].get<std::string>());
            }
        }

        // Parse construction
        if (j.contains("construction")) {
            const auto& constr = j["construction"];
            if (constr.contains("cost") && constr["cost"].is_object()) {
                m_constructionCost = ParseResourceCost(constr["cost"]);
            }
            if (constr.contains("time")) m_buildTime = constr["time"].get<float>();
            if (constr.contains("buildTime")) m_buildTime = constr["buildTime"].get<float>();

            if (constr.contains("stages") && constr["stages"].is_array()) {
                m_constructionStages.clear();
                for (const auto& stageJson : constr["stages"]) {
                    m_constructionStages.push_back(ParseConstructionStage(stageJson));
                }
            }
        } else {
            // Shorthand
            if (j.contains("cost") && j["cost"].is_object()) {
                m_constructionCost = ParseResourceCost(j["cost"]);
            }
            if (j.contains("buildTime")) m_buildTime = j["buildTime"].get<float>();
        }

        // Parse stats
        if (j.contains("stats")) {
            const auto& stats = j["stats"];
            if (stats.contains("health")) m_maxHealth = stats["health"].get<float>();
            if (stats.contains("maxHealth")) m_maxHealth = stats["maxHealth"].get<float>();
            if (stats.contains("armor")) m_armor = stats["armor"].get<float>();
            if (stats.contains("maxLevel")) m_maxLevel = stats["maxLevel"].get<int>();
        } else {
            if (j.contains("health")) m_maxHealth = j["health"].get<float>();
            if (j.contains("armor")) m_armor = j["armor"].get<float>();
        }

        // Parse production
        if (j.contains("production") && j["production"].is_array()) {
            m_productionCapabilities.clear();
            for (const auto& prodJson : j["production"]) {
                m_productionCapabilities.push_back(ParseProductionCapability(prodJson));
            }
        }

        // Parse garrison
        if (j.contains("garrison")) {
            const auto& garrison = j["garrison"];
            if (garrison.contains("capacity")) m_garrisonCapacity = garrison["capacity"].get<int>();
            if (garrison.contains("allowedTypes") && garrison["allowedTypes"].is_array()) {
                m_allowedGarrisonTypes.clear();
                for (const auto& type : garrison["allowedTypes"]) {
                    m_allowedGarrisonTypes.push_back(type.get<std::string>());
                }
            }
        } else if (j.contains("garrisonCapacity")) {
            m_garrisonCapacity = j["garrisonCapacity"].get<int>();
        }

        // Parse upkeep
        if (j.contains("upkeep") && j["upkeep"].is_object()) {
            m_upkeepCost = ParseResourceCost(j["upkeep"]);
        }

        // Parse power
        if (j.contains("power")) {
            const auto& power = j["power"];
            if (power.contains("consumption")) m_powerConsumption = power["consumption"].get<float>();
            if (power.contains("production")) m_powerProduction = power["production"].get<float>();
        }

        // Parse upgrades
        if (j.contains("upgrades") && j["upgrades"].is_array()) {
            m_upgrades.clear();
            for (const auto& upgradeJson : j["upgrades"]) {
                m_upgrades.push_back(ParseBuildingUpgrade(upgradeJson));
            }
        }

        // Parse defense
        if (j.contains("defense")) {
            const auto& defense = j["defense"];
            if (defense.contains("damage")) m_attackDamage = defense["damage"].get<float>();
            if (defense.contains("range")) m_attackRange = defense["range"].get<float>();
            if (defense.contains("attackSpeed")) m_attackSpeed = defense["attackSpeed"].get<float>();
        }

        // Parse vision
        if (j.contains("visionRange")) m_visionRange = j["visionRange"].get<float>();

        // Parse requirements
        if (j.contains("requirements")) {
            const auto& req = j["requirements"];
            if (req.contains("techs") && req["techs"].is_array()) {
                m_requiredTechs.clear();
                for (const auto& tech : req["techs"]) {
                    m_requiredTechs.push_back(tech.get<std::string>());
                }
            }
            if (req.contains("buildings") && req["buildings"].is_array()) {
                m_requiredBuildings.clear();
                for (const auto& building : req["buildings"]) {
                    m_requiredBuildings.push_back(building.get<std::string>());
                }
            }
        }

        // Parse script hooks
        if (j.contains("scripts") && j["scripts"].is_object()) {
            for (auto& [hook, path] : j["scripts"].items()) {
                m_scriptHooks[hook] = path.get<std::string>();
            }
        }

        // Parse classification
        if (j.contains("category")) m_category = j["category"].get<std::string>();
        if (j.contains("faction")) m_faction = j["faction"].get<std::string>();
        if (j.contains("unique")) m_isUnique = j["unique"].get<bool>();
        if (j.contains("maxCount")) m_maxCount = j["maxCount"].get<int>();

    } catch (const json::exception&) {
        // Error parsing building-specific fields
    }
}

std::string BuildingConfig::SerializeTypeSpecificFields() const {
    json j;

    // Footprint
    json footprint;
    footprint["gridType"] = GridTypeToString(m_footprint.gridType);
    footprint["size"] = json::array({m_footprint.size.x, m_footprint.size.y});
    j["footprint"] = footprint;

    // Construction
    json construction;
    construction["cost"] = ResourceCostToJson(m_constructionCost);
    construction["time"] = m_buildTime;
    j["construction"] = construction;

    // Stats
    json stats;
    stats["maxHealth"] = m_maxHealth;
    stats["armor"] = m_armor;
    stats["maxLevel"] = m_maxLevel;
    j["stats"] = stats;

    // Production
    if (!m_productionCapabilities.empty()) {
        json production = json::array();
        for (const auto& cap : m_productionCapabilities) {
            json p;
            p["output"] = cap.outputId;
            p["time"] = cap.productionTime;
            production.push_back(p);
        }
        j["production"] = production;
    }

    // Garrison
    if (m_garrisonCapacity > 0) {
        j["garrisonCapacity"] = m_garrisonCapacity;
    }

    // Scripts
    if (!m_scriptHooks.empty()) {
        j["scripts"] = m_scriptHooks;
    }

    return j.dump(2);
}

ValidationResult BuildingConfig::Validate() const {
    ValidationResult result = EntityConfig::Validate();

    // Validate footprint
    if (m_footprint.size.x <= 0 || m_footprint.size.y <= 0) {
        result.AddError("footprint.size", "Building size must be positive");
    }

    // Validate construction
    if (m_buildTime < 0) {
        result.AddError("construction.time", "Build time cannot be negative");
    }

    // Validate construction stages
    for (size_t i = 0; i < m_constructionStages.size(); ++i) {
        const auto& stage = m_constructionStages[i];
        if (stage.progressEnd <= stage.progressStart) {
            result.AddError("construction.stages[" + std::to_string(i) + "]",
                          "Stage end must be greater than start");
        }
    }

    // Validate stats
    if (m_maxHealth <= 0) {
        result.AddError("stats.maxHealth", "Max health must be positive");
    }

    // Validate defense
    if (m_attackDamage > 0 && m_attackRange <= 0) {
        result.AddWarning("defense", "Building has damage but no range");
    }

    return result;
}

void BuildingConfig::ApplyBaseConfig(const EntityConfig& baseConfig) {
    EntityConfig::ApplyBaseConfig(baseConfig);

    if (const auto* baseBuilding = dynamic_cast<const BuildingConfig*>(&baseConfig)) {
        // Inherit footprint if default
        if (m_footprint.size == glm::ivec2(1, 1)) {
            m_footprint = baseBuilding->m_footprint;
        }

        // Inherit construction cost if empty
        if (m_constructionCost.IsEmpty()) {
            m_constructionCost = baseBuilding->m_constructionCost;
        }

        // Inherit stats
        if (m_maxHealth == 500.0f) m_maxHealth = baseBuilding->m_maxHealth;
        if (m_armor == 5.0f) m_armor = baseBuilding->m_armor;

        // Merge production capabilities
        for (const auto& cap : baseBuilding->m_productionCapabilities) {
            bool found = false;
            for (const auto& existing : m_productionCapabilities) {
                if (existing.outputId == cap.outputId) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                m_productionCapabilities.push_back(cap);
            }
        }

        // Merge upgrades
        for (const auto& upgrade : baseBuilding->m_upgrades) {
            if (!GetUpgrade(upgrade.upgradeId)) {
                m_upgrades.push_back(upgrade);
            }
        }

        // Merge script hooks
        for (const auto& [hook, path] : baseBuilding->m_scriptHooks) {
            if (m_scriptHooks.find(hook) == m_scriptHooks.end()) {
                m_scriptHooks[hook] = path;
            }
        }

        // Inherit classification
        if (m_category.empty()) m_category = baseBuilding->m_category;
        if (m_faction.empty()) m_faction = baseBuilding->m_faction;
    }
}

const ConstructionStage* BuildingConfig::GetStageForProgress(float progress) const {
    for (const auto& stage : m_constructionStages) {
        if (progress >= stage.progressStart && progress < stage.progressEnd) {
            return &stage;
        }
    }
    return nullptr;
}

bool BuildingConfig::CanProduceUnit(const std::string& unitId) const {
    for (const auto& cap : m_productionCapabilities) {
        if (cap.type == ProductionCapability::ProductionType::Unit &&
            cap.outputId == unitId) {
            return true;
        }
    }
    return false;
}

bool BuildingConfig::CanProduceResource(ResourceType type) const {
    for (const auto& cap : m_productionCapabilities) {
        if (cap.type == ProductionCapability::ProductionType::Resource &&
            cap.outputId == ResourceTypeToString(type)) {
            return true;
        }
    }
    return false;
}

const BuildingUpgrade* BuildingConfig::GetUpgrade(const std::string& upgradeId) const {
    for (const auto& upgrade : m_upgrades) {
        if (upgrade.upgradeId == upgradeId) {
            return &upgrade;
        }
    }
    return nullptr;
}

std::string BuildingConfig::GetScriptHook(const std::string& hookName) const {
    auto it = m_scriptHooks.find(hookName);
    return (it != m_scriptHooks.end()) ? it->second : "";
}

void BuildingConfig::SetScriptHook(const std::string& hookName, const std::string& path) {
    if (path.empty()) {
        m_scriptHooks.erase(hookName);
    } else {
        m_scriptHooks[hookName] = path;
    }
}

} // namespace Config
} // namespace Vehement
