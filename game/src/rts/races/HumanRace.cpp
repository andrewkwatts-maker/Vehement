/**
 * @file HumanRace.cpp
 * @brief Implementation of Human race data loader
 */

#include "HumanRace.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace Vehement {
namespace RTS {

namespace fs = std::filesystem;

// ============================================================================
// HumanUnitTemplate
// ============================================================================

nlohmann::json HumanUnitTemplate::ToJson() const {
    return nlohmann::json{
        {"id", id},
        {"name", name},
        {"description", description},
        {"tier", tier},
        {"age_requirement", ageRequirement},
        {"stats", {
            {"health", health},
            {"healthRegen", healthRegen},
            {"mana", mana},
            {"manaRegen", manaRegen},
            {"armor", armor},
            {"magicResist", magicResist},
            {"moveSpeed", moveSpeed}
        }},
        {"combat", {
            {"attackDamage", attackDamage},
            {"attackSpeed", attackSpeed},
            {"attackRange", attackRange},
            {"damageType", damageType},
            {"armorType", armorType}
        }},
        {"production", {
            {"cost", cost},
            {"buildTime", buildTime},
            {"populationCost", populationCost},
            {"building", productionBuilding},
            {"prerequisites", prerequisites}
        }},
        {"abilities", abilityIds},
        {"visuals", {
            {"model", modelPath},
            {"icon", iconPath},
            {"portrait", portraitPath}
        }}
    };
}

HumanUnitTemplate HumanUnitTemplate::FromJson(const nlohmann::json& j) {
    HumanUnitTemplate t;

    t.id = j.value("id", "");
    t.name = j.value("name", "");
    t.description = j.value("description", "");
    t.tier = j.value("tier", 1);
    t.ageRequirement = j.value("age_requirement", 1);

    // Parse stats
    if (j.contains("stats")) {
        const auto& stats = j["stats"];
        t.health = stats.value("health", 100.0f);
        t.healthRegen = stats.value("healthRegen", 0.5f);
        t.mana = stats.value("mana", 0.0f);
        t.manaRegen = stats.value("manaRegen", 0.0f);
        t.armor = stats.value("armor", 0);
        t.magicResist = stats.value("magicResist", 0);
        t.moveSpeed = stats.value("moveSpeed", 5.0f);
    }

    // Parse combat
    if (j.contains("combat")) {
        const auto& combat = j["combat"];
        t.attackDamage = combat.value("attackDamage", 10.0f);
        t.attackSpeed = combat.value("attackSpeed", 1.0f);
        t.attackRange = combat.value("attackRange", 1.0f);
        t.damageType = combat.value("damageType", "normal");
        t.armorType = combat.value("armorType", "light");
    }

    // Parse production
    if (j.contains("production")) {
        const auto& prod = j["production"];
        if (prod.contains("cost")) {
            for (auto& [key, value] : prod["cost"].items()) {
                t.cost[key] = value.get<int>();
            }
        }
        t.buildTime = prod.value("build_time", 20.0f);
        t.populationCost = prod.value("population_cost", 1);
        t.productionBuilding = prod.value("building", "");
        if (prod.contains("prerequisites")) {
            for (const auto& prereq : prod["prerequisites"]) {
                t.prerequisites.push_back(prereq.get<std::string>());
            }
        }
    }

    // Parse abilities
    if (j.contains("abilities")) {
        for (const auto& ability : j["abilities"]) {
            if (ability.is_string()) {
                t.abilityIds.push_back(ability.get<std::string>());
            } else if (ability.is_object() && ability.contains("id")) {
                t.abilityIds.push_back(ability["id"].get<std::string>());
            }
        }
    }

    // Parse visuals
    if (j.contains("model")) {
        if (j["model"].is_string()) {
            t.modelPath = j["model"].get<std::string>();
        } else if (j["model"].is_object()) {
            t.modelPath = j["model"].value("path", "");
        }
    }

    if (j.contains("ui")) {
        t.iconPath = j["ui"].value("icon", "");
        t.portraitPath = j["ui"].value("portrait", "");
    }

    return t;
}

// ============================================================================
// HumanBuildingTemplate
// ============================================================================

nlohmann::json HumanBuildingTemplate::ToJson() const {
    return nlohmann::json{
        {"id", id},
        {"name", name},
        {"description", description},
        {"tier", tier},
        {"age_requirement", ageRequirement},
        {"stats", {
            {"maxHealth", maxHealth},
            {"armor", armor},
            {"magicResist", magicResist}
        }},
        {"construction", {
            {"cost", cost},
            {"time", buildTime},
            {"footprint", footprint}
        }},
        {"prerequisites", prerequisites},
        {"production", producibleUnits},
        {"upgrades_to", upgradesTo},
        {"upgrades_from", upgradesFrom},
        {"unique", unique},
        {"maxCount", maxCount}
    };
}

HumanBuildingTemplate HumanBuildingTemplate::FromJson(const nlohmann::json& j) {
    HumanBuildingTemplate t;

    t.id = j.value("id", "");
    t.name = j.value("name", "");
    t.description = j.value("description", "");
    t.tier = j.value("tier", 1);
    t.ageRequirement = j.value("age_requirement", 1);

    // Parse stats
    if (j.contains("stats")) {
        const auto& stats = j["stats"];
        t.maxHealth = stats.value("maxHealth", 1000.0f);
        t.armor = stats.value("armor", 5);
        t.magicResist = stats.value("magicResist", 5);
    }

    // Parse construction
    if (j.contains("construction")) {
        const auto& constr = j["construction"];
        if (constr.contains("cost")) {
            for (auto& [key, value] : constr["cost"].items()) {
                t.cost[key] = value.get<int>();
            }
        }
        t.buildTime = constr.value("time", 60.0f);
    }

    // Parse footprint
    if (j.contains("footprint") && j["footprint"].contains("size")) {
        t.footprint.clear();
        for (const auto& dim : j["footprint"]["size"]) {
            t.footprint.push_back(dim.get<int>());
        }
    }

    // Parse prerequisites
    if (j.contains("requirements") && j["requirements"].contains("buildings")) {
        for (const auto& prereq : j["requirements"]["buildings"]) {
            t.prerequisites.push_back(prereq.get<std::string>());
        }
    }

    // Parse production
    if (j.contains("production")) {
        for (const auto& prod : j["production"]) {
            if (prod.contains("output")) {
                t.producibleUnits.push_back(prod["output"].get<std::string>());
            }
        }
    }

    // Parse researches
    if (j.contains("researches")) {
        for (const auto& research : j["researches"]) {
            if (research.contains("id")) {
                t.researchableUpgrades.push_back(research["id"].get<std::string>());
            }
        }
    }

    t.upgradesTo = j.value("upgrades_to", "");

    if (j.contains("upgrade_from")) {
        t.upgradesFrom = j["upgrade_from"].value("building", "");
    }

    t.unique = j.value("unique", false);
    t.maxCount = j.value("maxCount", 10);

    // Visuals
    if (j.contains("model")) {
        if (j["model"].is_string()) {
            t.modelPath = j["model"].get<std::string>();
        } else if (j["model"].is_object()) {
            t.modelPath = j["model"].value("path", "");
        }
    }

    if (j.contains("ui")) {
        t.iconPath = j["ui"].value("icon", "");
    }

    return t;
}

// ============================================================================
// HumanHeroTemplate
// ============================================================================

nlohmann::json HumanHeroTemplate::ToJson() const {
    return nlohmann::json{
        {"id", id},
        {"name", name},
        {"title", title},
        {"class", heroClass},
        {"primary_attribute", primaryAttribute},
        {"description", description},
        {"lore", lore},
        {"base_stats", {
            {"health", health},
            {"mana", mana},
            {"damage", damage},
            {"armor", armor},
            {"magic_resist", magicResist},
            {"move_speed", moveSpeed}
        }},
        {"abilities", abilityIds}
    };
}

HumanHeroTemplate HumanHeroTemplate::FromJson(const nlohmann::json& j) {
    HumanHeroTemplate t;

    t.id = j.value("id", "");
    t.name = j.value("name", "");
    t.title = j.value("title", "");
    t.heroClass = j.value("class", "warrior");
    t.primaryAttribute = j.value("primary_attribute", "strength");
    t.description = j.value("description", "");
    t.lore = j.value("lore", "");

    // Parse base stats
    if (j.contains("base_stats")) {
        const auto& stats = j["base_stats"];
        t.health = stats.value("health", 500.0f);
        t.mana = stats.value("mana", 200.0f);
        t.damage = stats.value("damage", 25.0f);
        t.armor = stats.value("armor", 3);
        t.magicResist = stats.value("magic_resist", 15);
        t.moveSpeed = stats.value("move_speed", 5.5f);
        t.attackRange = stats.value("attack_range", 1.5f);
        t.attackSpeed = stats.value("attack_speed", 1.5f);
        t.strength = stats.value("strength", 20);
        t.agility = stats.value("agility", 15);
        t.intelligence = stats.value("intelligence", 15);
    }

    // Parse stat growth
    if (j.contains("stat_growth")) {
        const auto& growth = j["stat_growth"];
        t.healthPerLevel = growth.value("health_per_level", 50.0f);
        t.manaPerLevel = growth.value("mana_per_level", 25.0f);
        t.damagePerLevel = growth.value("damage_per_level", 2.5f);
    }

    // Parse abilities
    if (j.contains("abilities")) {
        for (const auto& ability : j["abilities"]) {
            if (ability.contains("id")) {
                t.abilityIds.push_back(ability["id"].get<std::string>());
            }
        }
    }

    // Parse talents
    if (j.contains("talents")) {
        for (const auto& talent : j["talents"]) {
            int level = talent.value("unlock_level", 10);
            std::vector<std::string> choices;
            if (talent.contains("choices")) {
                for (const auto& choice : talent["choices"]) {
                    choices.push_back(choice.get<std::string>());
                }
            }
            t.talentChoices.emplace_back(level, choices);
        }
    }

    // Parse production cost
    if (j.contains("properties") && j["properties"].contains("summon_cost")) {
        for (auto& [key, value] : j["properties"]["summon_cost"].items()) {
            t.cost[key] = value.get<int>();
        }
    }

    // Visuals
    if (j.contains("visuals")) {
        t.modelPath = j["visuals"].value("model", "");
        t.portraitPath = j["visuals"].value("portrait", "");
        t.iconPath = j["visuals"].value("icon", "");
        t.modelScale = j["visuals"].value("scale", 1.0f);
    }

    return t;
}

// ============================================================================
// HumanRace Implementation
// ============================================================================

HumanRace::HumanRace() = default;
HumanRace::~HumanRace() = default;
HumanRace::HumanRace(HumanRace&&) noexcept = default;
HumanRace& HumanRace::operator=(HumanRace&&) noexcept = default;

bool HumanRace::Initialize(const std::string& configBasePath) {
    if (m_initialized) {
        return true;
    }

    m_configBasePath = configBasePath.empty() ?
        std::string(HumanRaceConstants::CONFIG_PATH) : configBasePath;

    // Ensure path ends with separator
    if (!m_configBasePath.empty() && m_configBasePath.back() != '/') {
        m_configBasePath += '/';
    }

    // Load all configurations
    if (!LoadRaceDefinition(m_configBasePath + "race_humans.json")) {
        std::cerr << "Failed to load human race definition" << std::endl;
        return false;
    }

    if (!LoadUnitConfigs(m_configBasePath + "units/")) {
        std::cerr << "Failed to load human unit configs" << std::endl;
        return false;
    }

    if (!LoadBuildingConfigs(m_configBasePath + "buildings/")) {
        std::cerr << "Failed to load human building configs" << std::endl;
        return false;
    }

    if (!LoadHeroConfigs(m_configBasePath + "heroes/")) {
        std::cerr << "Failed to load human hero configs" << std::endl;
        return false;
    }

    if (!LoadTechTree(m_configBasePath + "tech_tree/")) {
        std::cerr << "Failed to load human tech tree" << std::endl;
        return false;
    }

    if (!LoadAbilities(m_configBasePath + "abilities/")) {
        std::cerr << "Failed to load human abilities" << std::endl;
        return false;
    }

    if (!LoadTalentTree(m_configBasePath + "talent_tree.json")) {
        std::cerr << "Failed to load human talent tree" << std::endl;
        return false;
    }

    if (!LoadVisuals(m_configBasePath + "humans_visuals.json")) {
        std::cerr << "Failed to load human visuals config" << std::endl;
        return false;
    }

    if (!LoadAI(m_configBasePath + "humans_ai.json")) {
        std::cerr << "Failed to load human AI config" << std::endl;
        return false;
    }

    // Initialize tech tree
    m_techTree.Initialize(CultureType::Fortress, "");

    m_initialized = true;
    return true;
}

void HumanRace::Shutdown() {
    m_unitTemplates.clear();
    m_buildingTemplates.clear();
    m_heroTemplates.clear();
    m_abilityLookup.clear();
    m_statBonuses.clear();
    m_techTree.Shutdown();
    m_initialized = false;
}

bool HumanRace::ReloadConfigs() {
    Shutdown();
    return Initialize(m_configBasePath);
}

nlohmann::json HumanRace::LoadJsonFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return nlohmann::json{};
    }

    try {
        // Read file and remove comments (JSON doesn't support comments, but our configs use them)
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        // Simple comment removal (single-line only)
        std::string cleanedContent;
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line)) {
            size_t commentPos = line.find("//");
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }
            cleanedContent += line + "\n";
        }

        return nlohmann::json::parse(cleanedContent);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON file " << path << ": " << e.what() << std::endl;
        return nlohmann::json{};
    }
}

bool HumanRace::LoadRaceDefinition(const std::string& path) {
    m_raceConfig = LoadJsonFile(path);
    if (m_raceConfig.empty()) {
        return false;
    }

    m_raceId = m_raceConfig.value("id", "humans");
    m_raceName = m_raceConfig.value("name", "Kingdom of Valorheim");
    m_description = m_raceConfig.value("description", "");

    ParseStrengths(m_raceConfig);
    ParseWeaknesses(m_raceConfig);

    return true;
}

void HumanRace::ParseStrengths(const nlohmann::json& j) {
    m_strengths.clear();
    if (j.contains("strengths")) {
        for (const auto& strength : j["strengths"]) {
            if (strength.contains("name")) {
                m_strengths.push_back(strength["name"].get<std::string>());
            }

            // Parse stat bonuses
            if (strength.contains("modifier")) {
                std::string stat = strength["modifier"].value("stat", "");
                float value = strength["modifier"].value("value", 0.0f);
                if (!stat.empty()) {
                    m_statBonuses[stat] = value;
                }
            }
            if (strength.contains("modifiers")) {
                for (const auto& mod : strength["modifiers"]) {
                    std::string stat = mod.value("stat", "");
                    float value = mod.value("value", 0.0f);
                    if (!stat.empty()) {
                        m_statBonuses[stat] = value;
                    }
                }
            }
        }
    }
}

void HumanRace::ParseWeaknesses(const nlohmann::json& j) {
    m_weaknesses.clear();
    if (j.contains("weaknesses")) {
        for (const auto& weakness : j["weaknesses"]) {
            if (weakness.contains("name")) {
                m_weaknesses.push_back(weakness["name"].get<std::string>());
            }

            // Parse stat penalties
            if (weakness.contains("modifier")) {
                std::string stat = weakness["modifier"].value("stat", "");
                float value = weakness["modifier"].value("value", 0.0f);
                if (!stat.empty()) {
                    m_statBonuses[stat] = value;
                }
            }
        }
    }
}

bool HumanRace::LoadUnitConfigs(const std::string& path) {
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.path().extension() == ".json") {
                auto j = LoadJsonFile(entry.path().string());
                if (!j.empty() && j.contains("id")) {
                    auto tmpl = HumanUnitTemplate::FromJson(j);
                    m_unitTemplates[tmpl.id] = std::move(tmpl);
                }
            }
        }
        return !m_unitTemplates.empty();
    } catch (const std::exception& e) {
        std::cerr << "Error loading unit configs: " << e.what() << std::endl;
        return false;
    }
}

bool HumanRace::LoadBuildingConfigs(const std::string& path) {
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.path().extension() == ".json") {
                auto j = LoadJsonFile(entry.path().string());
                if (!j.empty() && j.contains("id")) {
                    auto tmpl = HumanBuildingTemplate::FromJson(j);
                    m_buildingTemplates[tmpl.id] = std::move(tmpl);
                }
            }
        }
        return !m_buildingTemplates.empty();
    } catch (const std::exception& e) {
        std::cerr << "Error loading building configs: " << e.what() << std::endl;
        return false;
    }
}

bool HumanRace::LoadHeroConfigs(const std::string& path) {
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.path().extension() == ".json") {
                auto j = LoadJsonFile(entry.path().string());
                if (!j.empty() && j.contains("id")) {
                    auto tmpl = HumanHeroTemplate::FromJson(j);
                    m_heroTemplates[tmpl.id] = std::move(tmpl);
                }
            }
        }
        return !m_heroTemplates.empty();
    } catch (const std::exception& e) {
        std::cerr << "Error loading hero configs: " << e.what() << std::endl;
        return false;
    }
}

bool HumanRace::LoadTechTree(const std::string& path) {
    m_ageConfig = LoadJsonFile(path + "ages.json");
    m_upgradesConfig = LoadJsonFile(path + "upgrades.json");
    m_technologiesConfig = LoadJsonFile(path + "technologies.json");

    return !m_ageConfig.empty() || !m_upgradesConfig.empty() || !m_technologiesConfig.empty();
}

bool HumanRace::LoadAbilities(const std::string& path) {
    m_heroAbilities = LoadJsonFile(path + "hero_abilities.json");
    m_unitAbilities = LoadJsonFile(path + "unit_abilities.json");

    // Build ability lookup
    auto buildLookup = [this](const nlohmann::json& j) {
        if (j.contains("abilities")) {
            for (const auto& ability : j["abilities"]) {
                if (ability.contains("id")) {
                    m_abilityLookup[ability["id"].get<std::string>()] = ability;
                }
            }
        }
    };

    buildLookup(m_heroAbilities);
    buildLookup(m_unitAbilities);

    return !m_heroAbilities.empty() || !m_unitAbilities.empty();
}

bool HumanRace::LoadTalentTree(const std::string& path) {
    m_talentTree = LoadJsonFile(path);
    return !m_talentTree.empty();
}

bool HumanRace::LoadVisuals(const std::string& path) {
    m_visualsConfig = LoadJsonFile(path);
    return !m_visualsConfig.empty();
}

bool HumanRace::LoadAI(const std::string& path) {
    m_aiConfig = LoadJsonFile(path);
    return !m_aiConfig.empty();
}

std::vector<const HumanUnitTemplate*> HumanRace::GetUnitsByTier(int tier) const {
    std::vector<const HumanUnitTemplate*> result;
    for (const auto& [id, tmpl] : m_unitTemplates) {
        if (tmpl.tier == tier) {
            result.push_back(&tmpl);
        }
    }
    return result;
}

std::vector<const HumanUnitTemplate*> HumanRace::GetUnitsForAge(int age) const {
    std::vector<const HumanUnitTemplate*> result;
    for (const auto& [id, tmpl] : m_unitTemplates) {
        if (tmpl.ageRequirement <= age) {
            result.push_back(&tmpl);
        }
    }
    return result;
}

bool HumanRace::IsUnitAvailable(const std::string& unitId,
                                 int currentAge,
                                 const std::vector<std::string>& completedTechs,
                                 const std::vector<std::string>& builtBuildings) const {
    const auto* tmpl = GetUnitTemplate(unitId);
    if (!tmpl) {
        return false;
    }

    // Check age requirement
    if (currentAge < tmpl->ageRequirement) {
        return false;
    }

    // Check prerequisites
    for (const auto& prereq : tmpl->prerequisites) {
        bool found = std::find(completedTechs.begin(), completedTechs.end(), prereq) != completedTechs.end() ||
                     std::find(builtBuildings.begin(), builtBuildings.end(), prereq) != builtBuildings.end();
        if (!found) {
            return false;
        }
    }

    return true;
}

std::vector<const HumanBuildingTemplate*> HumanRace::GetBuildingsForAge(int age) const {
    std::vector<const HumanBuildingTemplate*> result;
    for (const auto& [id, tmpl] : m_buildingTemplates) {
        if (tmpl.ageRequirement <= age) {
            result.push_back(&tmpl);
        }
    }
    return result;
}

bool HumanRace::CanBuildBuilding(const std::string& buildingId,
                                  int currentAge,
                                  const std::vector<std::string>& existingBuildings,
                                  const std::vector<std::string>& completedTechs) const {
    const auto* tmpl = GetBuildingTemplate(buildingId);
    if (!tmpl) {
        return false;
    }

    // Check age requirement
    if (currentAge < tmpl->ageRequirement) {
        return false;
    }

    // Check max count
    if (tmpl->maxCount > 0) {
        int count = std::count(existingBuildings.begin(), existingBuildings.end(), buildingId);
        if (count >= tmpl->maxCount) {
            return false;
        }
    }

    // Check prerequisites
    for (const auto& prereq : tmpl->prerequisites) {
        bool found = std::find(existingBuildings.begin(), existingBuildings.end(), prereq) != existingBuildings.end() ||
                     std::find(completedTechs.begin(), completedTechs.end(), prereq) != completedTechs.end();
        if (!found) {
            return false;
        }
    }

    return true;
}

std::vector<const HumanHeroTemplate*> HumanRace::GetAvailableHeroes() const {
    std::vector<const HumanHeroTemplate*> result;
    for (const auto& [id, tmpl] : m_heroTemplates) {
        result.push_back(&tmpl);
    }
    return result;
}

const nlohmann::json* HumanRace::GetAbilityData(const std::string& abilityId) const {
    auto it = m_abilityLookup.find(abilityId);
    return (it != m_abilityLookup.end()) ? &it->second : nullptr;
}

float HumanRace::GetStatBonus(const std::string& statName) const {
    auto it = m_statBonuses.find(statName);
    return (it != m_statBonuses.end()) ? it->second : 0.0f;
}

std::map<std::string, int> HumanRace::GetStartingResources() const {
    std::map<std::string, int> resources;
    if (m_raceConfig.contains("starting_resources")) {
        for (auto& [key, value] : m_raceConfig["starting_resources"].items()) {
            resources[key] = value.get<int>();
        }
    }
    return resources;
}

std::vector<std::pair<std::string, int>> HumanRace::GetStartingUnits() const {
    std::vector<std::pair<std::string, int>> units;
    if (m_raceConfig.contains("starting_units")) {
        for (const auto& unit : m_raceConfig["starting_units"]) {
            std::string id = unit.value("unit_id", "");
            int count = unit.value("count", 1);
            if (!id.empty()) {
                units.emplace_back(id, count);
            }
        }
    }
    return units;
}

std::vector<std::pair<std::string, int>> HumanRace::GetStartingBuildings() const {
    std::vector<std::pair<std::string, int>> buildings;
    if (m_raceConfig.contains("starting_buildings")) {
        for (const auto& building : m_raceConfig["starting_buildings"]) {
            std::string id = building.value("building_id", "");
            int count = building.value("count", 1);
            if (!id.empty()) {
                buildings.emplace_back(id, count);
            }
        }
    }
    return buildings;
}

} // namespace RTS
} // namespace Vehement
