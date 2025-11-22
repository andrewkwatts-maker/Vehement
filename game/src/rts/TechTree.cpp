#include "TechTree.hpp"
#include "../network/FirebaseManager.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>

namespace Vehement {
namespace RTS {

// ============================================================================
// AgeRequirements Implementation
// ============================================================================

nlohmann::json AgeRequirements::ToJson() const {
    nlohmann::json j;
    j["age"] = static_cast<int>(age);
    j["researchTime"] = researchTime;
    j["description"] = description;
    j["requiredBuildings"] = requiredBuildings;
    j["requiredPopulation"] = requiredPopulation;

    nlohmann::json costs;
    for (const auto& [type, amount] : resourceCost) {
        costs[std::to_string(static_cast<int>(type))] = amount;
    }
    j["resourceCost"] = costs;

    j["requiredTechs"] = requiredTechs;
    return j;
}

AgeRequirements AgeRequirements::FromJson(const nlohmann::json& j) {
    AgeRequirements req;
    req.age = static_cast<Age>(j.value("age", 0));
    req.researchTime = j.value("researchTime", 60.0f);
    req.description = j.value("description", "");
    req.requiredBuildings = j.value("requiredBuildings", 0);
    req.requiredPopulation = j.value("requiredPopulation", 0);

    if (j.contains("resourceCost")) {
        for (auto& [key, value] : j["resourceCost"].items()) {
            ResourceType type = static_cast<ResourceType>(std::stoi(key));
            req.resourceCost[type] = value.get<int>();
        }
    }

    if (j.contains("requiredTechs")) {
        req.requiredTechs = j["requiredTechs"].get<std::vector<std::string>>();
    }

    return req;
}

// ============================================================================
// TechEffect Implementation
// ============================================================================

TechEffect TechEffect::Multiplier(const std::string& target, float mult, const std::string& desc) {
    TechEffect e;
    e.type = TechEffectType::StatMultiplier;
    e.target = target;
    e.value = mult;
    e.description = desc;
    return e;
}

TechEffect TechEffect::FlatBonus(const std::string& target, float amount, const std::string& desc) {
    TechEffect e;
    e.type = TechEffectType::StatFlat;
    e.target = target;
    e.value = amount;
    e.description = desc;
    return e;
}

TechEffect TechEffect::UnlockBuilding(const std::string& buildingId, const std::string& desc) {
    TechEffect e;
    e.type = TechEffectType::UnlockBuilding;
    e.target = buildingId;
    e.stringValue = buildingId;
    e.description = desc.empty() ? ("Unlocks " + buildingId) : desc;
    return e;
}

TechEffect TechEffect::UnlockUnit(const std::string& unitId, const std::string& desc) {
    TechEffect e;
    e.type = TechEffectType::UnlockUnit;
    e.target = unitId;
    e.stringValue = unitId;
    e.description = desc.empty() ? ("Unlocks " + unitId) : desc;
    return e;
}

TechEffect TechEffect::UnlockAbility(const std::string& abilityId, const std::string& desc) {
    TechEffect e;
    e.type = TechEffectType::UnlockAbility;
    e.target = abilityId;
    e.description = desc;
    return e;
}

TechEffect TechEffect::EnableFeature(const std::string& feature, const std::string& desc) {
    TechEffect e;
    e.type = TechEffectType::EnableFeature;
    e.target = feature;
    e.stringValue = feature;
    e.description = desc;
    return e;
}

TechEffect TechEffect::ReduceCost(const std::string& target, float percent, const std::string& desc) {
    TechEffect e;
    e.type = TechEffectType::ReduceCost;
    e.target = target;
    e.value = percent;
    e.description = desc;
    return e;
}

nlohmann::json TechEffect::ToJson() const {
    return {
        {"type", static_cast<int>(type)},
        {"target", target},
        {"value", value},
        {"stringValue", stringValue},
        {"description", description}
    };
}

TechEffect TechEffect::FromJson(const nlohmann::json& j) {
    TechEffect e;
    e.type = static_cast<TechEffectType>(j.value("type", 0));
    e.target = j.value("target", "");
    e.value = j.value("value", 0.0f);
    e.stringValue = j.value("stringValue", "");
    e.description = j.value("description", "");
    return e;
}

// ============================================================================
// TechNode Implementation
// ============================================================================

bool TechNode::IsAvailableTo(CultureType culture) const {
    if (isUniversal) return true;
    if (availableToCultures.empty()) return true;  // Empty means all cultures

    for (CultureType c : availableToCultures) {
        if (c == culture) return true;
    }
    return false;
}

int TechNode::GetTotalCostValue() const {
    int total = 0;
    for (const auto& [type, amount] : cost) {
        // Simple weighting by resource type
        int weight = 1;
        if (type == ResourceType::Metal) weight = 3;
        else if (type == ResourceType::Stone) weight = 2;
        else if (type == ResourceType::Coins) weight = 5;
        total += amount * weight;
    }
    return total;
}

nlohmann::json TechNode::ToJson() const {
    nlohmann::json j;
    j["id"] = id;
    j["name"] = name;
    j["description"] = description;
    j["iconPath"] = iconPath;
    j["category"] = static_cast<int>(category);
    j["requiredAge"] = static_cast<int>(requiredAge);
    j["prerequisites"] = prerequisites;
    j["researchTime"] = researchTime;
    j["lossChanceOnDeath"] = lossChanceOnDeath;
    j["canBeLost"] = canBeLost;
    j["minimumAgeLoss"] = static_cast<int>(minimumAgeLoss);
    j["isUniversal"] = isUniversal;
    j["treeRow"] = treeRow;
    j["treeColumn"] = treeColumn;
    j["tier"] = tier;
    j["isKeyTech"] = isKeyTech;
    j["protectionBonus"] = protectionBonus;
    j["unlocksBuildings"] = unlocksBuildings;
    j["unlocksUnits"] = unlocksUnits;
    j["unlocksAbilities"] = unlocksAbilities;

    nlohmann::json costs;
    for (const auto& [type, amount] : cost) {
        costs[std::to_string(static_cast<int>(type))] = amount;
    }
    j["cost"] = costs;

    nlohmann::json effectsJson = nlohmann::json::array();
    for (const auto& effect : effects) {
        effectsJson.push_back(effect.ToJson());
    }
    j["effects"] = effectsJson;

    nlohmann::json culturesJson = nlohmann::json::array();
    for (CultureType c : availableToCultures) {
        culturesJson.push_back(static_cast<int>(c));
    }
    j["availableToCultures"] = culturesJson;

    return j;
}

TechNode TechNode::FromJson(const nlohmann::json& j) {
    TechNode node;
    node.id = j.value("id", "");
    node.name = j.value("name", "");
    node.description = j.value("description", "");
    node.iconPath = j.value("iconPath", "");
    node.category = static_cast<TechCategory>(j.value("category", 0));
    node.requiredAge = static_cast<Age>(j.value("requiredAge", 0));
    node.researchTime = j.value("researchTime", 30.0f);
    node.lossChanceOnDeath = j.value("lossChanceOnDeath", 0.3f);
    node.canBeLost = j.value("canBeLost", true);
    node.minimumAgeLoss = static_cast<Age>(j.value("minimumAgeLoss", 0));
    node.isUniversal = j.value("isUniversal", false);
    node.treeRow = j.value("treeRow", 0);
    node.treeColumn = j.value("treeColumn", 0);
    node.tier = j.value("tier", 1);
    node.isKeyTech = j.value("isKeyTech", false);
    node.protectionBonus = j.value("protectionBonus", 0.0f);

    if (j.contains("prerequisites")) {
        node.prerequisites = j["prerequisites"].get<std::vector<std::string>>();
    }
    if (j.contains("unlocksBuildings")) {
        node.unlocksBuildings = j["unlocksBuildings"].get<std::vector<std::string>>();
    }
    if (j.contains("unlocksUnits")) {
        node.unlocksUnits = j["unlocksUnits"].get<std::vector<std::string>>();
    }
    if (j.contains("unlocksAbilities")) {
        node.unlocksAbilities = j["unlocksAbilities"].get<std::vector<std::string>>();
    }

    if (j.contains("cost")) {
        for (auto& [key, value] : j["cost"].items()) {
            ResourceType type = static_cast<ResourceType>(std::stoi(key));
            node.cost[type] = value.get<int>();
        }
    }

    if (j.contains("effects")) {
        for (const auto& effectJson : j["effects"]) {
            node.effects.push_back(TechEffect::FromJson(effectJson));
        }
    }

    if (j.contains("availableToCultures")) {
        for (const auto& cultureJson : j["availableToCultures"]) {
            node.availableToCultures.push_back(static_cast<CultureType>(cultureJson.get<int>()));
        }
    }

    return node;
}

// ============================================================================
// ResearchProgress Implementation
// ============================================================================

nlohmann::json ResearchProgress::ToJson() const {
    return {
        {"techId", techId},
        {"status", static_cast<int>(status)},
        {"progressTime", progressTime},
        {"totalTime", totalTime},
        {"startedTimestamp", startedTimestamp},
        {"completedTimestamp", completedTimestamp},
        {"timesResearched", timesResearched},
        {"timesLost", timesLost}
    };
}

ResearchProgress ResearchProgress::FromJson(const nlohmann::json& j) {
    ResearchProgress p;
    p.techId = j.value("techId", "");
    p.status = static_cast<TechStatus>(j.value("status", 0));
    p.progressTime = j.value("progressTime", 0.0f);
    p.totalTime = j.value("totalTime", 0.0f);
    p.startedTimestamp = j.value("startedTimestamp", 0LL);
    p.completedTimestamp = j.value("completedTimestamp", 0LL);
    p.timesResearched = j.value("timesResearched", 0);
    p.timesLost = j.value("timesLost", 0);
    return p;
}

// ============================================================================
// TechTree Implementation
// ============================================================================

TechTree::TechTree() {
    m_ageRequirements.resize(static_cast<size_t>(Age::COUNT));
}

TechTree::~TechTree() {
    Shutdown();
}

TechTree::TechTree(TechTree&&) noexcept = default;
TechTree& TechTree::operator=(TechTree&&) noexcept = default;

bool TechTree::Initialize(CultureType culture, const std::string& playerId) {
    if (m_initialized) {
        Shutdown();
    }

    m_culture = culture;
    m_playerId = playerId;

    // Initialize age requirements
    InitializeAgeRequirements();

    // Initialize universal techs (available to all)
    InitializeUniversalTechs();

    // Initialize culture-specific techs
    InitializeCultureTechs(culture);

    // Build dependency graph
    BuildTechDependencies();

    m_initialized = true;
    return true;
}

void TechTree::Shutdown() {
    if (m_firebaseSyncEnabled) {
        DisableFirebaseSync();
    }

    m_allTechs.clear();
    m_researchedTechs.clear();
    m_techProgress.clear();
    m_currentResearch.clear();
    m_researchQueue.clear();
    m_researchProgress = 0.0f;
    m_currentAge = Age::Stone;
    m_isAdvancingAge = false;
    m_ageAdvancementProgress = 0.0f;

    m_statMultipliers.clear();
    m_statFlatBonuses.clear();
    m_unlockedBuildings.clear();
    m_unlockedUnits.clear();
    m_unlockedAbilities.clear();

    m_initialized = false;
}

void TechTree::SetCulture(CultureType culture) {
    if (culture != m_culture) {
        // Re-initialize with new culture
        std::string playerId = m_playerId;
        auto researchedTechs = m_researchedTechs;

        Initialize(culture, playerId);

        // Keep universal techs that were researched
        for (const auto& techId : researchedTechs) {
            auto it = m_allTechs.find(techId);
            if (it != m_allTechs.end() && it->second.isUniversal) {
                m_researchedTechs.insert(techId);
            }
        }

        RecalculateBonuses();
    }
}

void TechTree::InitializeAgeRequirements() {
    // Stone Age - starting age, no requirements
    m_ageRequirements[static_cast<size_t>(Age::Stone)] = {
        Age::Stone,
        {},
        {},
        0.0f,
        "The dawn of civilization. Humanity struggles to survive with primitive tools.",
        0, 0
    };

    // Bronze Age
    m_ageRequirements[static_cast<size_t>(Age::Bronze)] = {
        Age::Bronze,
        {{ResourceType::Food, 200}, {ResourceType::Wood, 150}},
        {UniversalTechs::PRIMITIVE_TOOLS, UniversalTechs::BASIC_SHELTER},
        45.0f,
        "The discovery of bronze transforms warfare and agriculture.",
        2, 5
    };

    // Iron Age
    m_ageRequirements[static_cast<size_t>(Age::Iron)] = {
        Age::Iron,
        {{ResourceType::Food, 400}, {ResourceType::Wood, 300}, {ResourceType::Stone, 200}},
        {UniversalTechs::BRONZE_WORKING, UniversalTechs::AGRICULTURE},
        60.0f,
        "Iron revolutionizes construction and military might.",
        4, 10
    };

    // Medieval Age
    m_ageRequirements[static_cast<size_t>(Age::Medieval)] = {
        Age::Medieval,
        {{ResourceType::Food, 800}, {ResourceType::Stone, 600}, {ResourceType::Metal, 300}},
        {UniversalTechs::IRON_WORKING, UniversalTechs::STONE_FORTIFICATIONS},
        90.0f,
        "The age of castles, knights, and feudal empires.",
        6, 20
    };

    // Industrial Age
    m_ageRequirements[static_cast<size_t>(Age::Industrial)] = {
        Age::Industrial,
        {{ResourceType::Food, 1200}, {ResourceType::Metal, 800}, {ResourceType::Coins, 500}},
        {UniversalTechs::CASTLE_BUILDING, UniversalTechs::GUILDS},
        120.0f,
        "Steam and steel transform the world forever.",
        10, 35
    };

    // Modern Age
    m_ageRequirements[static_cast<size_t>(Age::Modern)] = {
        Age::Modern,
        {{ResourceType::Metal, 1500}, {ResourceType::Fuel, 800}, {ResourceType::Coins, 1000}},
        {UniversalTechs::STEAM_POWER, UniversalTechs::FACTORIES},
        150.0f,
        "Electricity and engines power a new era of progress.",
        15, 50
    };

    // Future Age
    m_ageRequirements[static_cast<size_t>(Age::Future)] = {
        Age::Future,
        {{ResourceType::Metal, 3000}, {ResourceType::Fuel, 2000}, {ResourceType::Coins, 2500}},
        {UniversalTechs::ELECTRICITY, UniversalTechs::AUTOMATIC_WEAPONS},
        180.0f,
        "Technology transcends the limits of the present.",
        20, 75
    };
}

void TechTree::InitializeUniversalTechs() {
    // ========== STONE AGE TECHS ==========
    TechNode primitiveTools;
    primitiveTools.id = UniversalTechs::PRIMITIVE_TOOLS;
    primitiveTools.name = "Primitive Tools";
    primitiveTools.description = "Basic stone tools for gathering and building.";
    primitiveTools.category = TechCategory::Economy;
    primitiveTools.requiredAge = Age::Stone;
    primitiveTools.cost = {{ResourceType::Wood, 25}};
    primitiveTools.researchTime = 15.0f;
    primitiveTools.isUniversal = true;
    primitiveTools.canBeLost = false;  // Permanent
    primitiveTools.treeRow = 0;
    primitiveTools.treeColumn = 0;
    primitiveTools.effects.push_back(TechEffect::Multiplier("gather_speed", 1.15f, "+15% gathering speed"));
    AddTech(primitiveTools);

    TechNode basicShelter;
    basicShelter.id = UniversalTechs::BASIC_SHELTER;
    basicShelter.name = "Basic Shelter";
    basicShelter.description = "Simple shelters to protect from the elements.";
    basicShelter.category = TechCategory::Infrastructure;
    basicShelter.requiredAge = Age::Stone;
    basicShelter.cost = {{ResourceType::Wood, 30}};
    basicShelter.researchTime = 15.0f;
    basicShelter.isUniversal = true;
    basicShelter.canBeLost = false;
    basicShelter.treeRow = 0;
    basicShelter.treeColumn = 1;
    basicShelter.effects.push_back(TechEffect::UnlockBuilding("Shelter", "Unlocks Shelter building"));
    basicShelter.unlocksBuildings.push_back("Shelter");
    AddTech(basicShelter);

    TechNode foraging;
    foraging.id = UniversalTechs::FORAGING;
    foraging.name = "Foraging";
    foraging.description = "Knowledge of edible plants and hunting grounds.";
    foraging.category = TechCategory::Economy;
    foraging.requiredAge = Age::Stone;
    foraging.prerequisites = {UniversalTechs::PRIMITIVE_TOOLS};
    foraging.cost = {{ResourceType::Food, 30}};
    foraging.researchTime = 20.0f;
    foraging.isUniversal = true;
    foraging.treeRow = 1;
    foraging.treeColumn = 0;
    foraging.effects.push_back(TechEffect::Multiplier("food_gather", 1.2f, "+20% food gathering"));
    AddTech(foraging);

    TechNode stoneWeapons;
    stoneWeapons.id = UniversalTechs::STONE_WEAPONS;
    stoneWeapons.name = "Stone Weapons";
    stoneWeapons.description = "Sharpened stone spears and clubs for defense.";
    stoneWeapons.category = TechCategory::Military;
    stoneWeapons.requiredAge = Age::Stone;
    stoneWeapons.prerequisites = {UniversalTechs::PRIMITIVE_TOOLS};
    stoneWeapons.cost = {{ResourceType::Wood, 30}, {ResourceType::Stone, 20}};
    stoneWeapons.researchTime = 20.0f;
    stoneWeapons.isUniversal = true;
    stoneWeapons.treeRow = 1;
    stoneWeapons.treeColumn = 2;
    stoneWeapons.effects.push_back(TechEffect::Multiplier("attack_damage", 1.1f, "+10% attack damage"));
    stoneWeapons.effects.push_back(TechEffect::UnlockUnit("Warrior", "Unlocks basic warriors"));
    stoneWeapons.unlocksUnits.push_back("Warrior");
    AddTech(stoneWeapons);

    // ========== BRONZE AGE TECHS ==========
    TechNode bronzeWorking;
    bronzeWorking.id = UniversalTechs::BRONZE_WORKING;
    bronzeWorking.name = "Bronze Working";
    bronzeWorking.description = "Smelting copper and tin to create bronze.";
    bronzeWorking.category = TechCategory::Science;
    bronzeWorking.requiredAge = Age::Bronze;
    bronzeWorking.prerequisites = {UniversalTechs::STONE_WEAPONS};
    bronzeWorking.cost = {{ResourceType::Stone, 60}, {ResourceType::Metal, 30}};
    bronzeWorking.researchTime = 30.0f;
    bronzeWorking.isUniversal = true;
    bronzeWorking.isKeyTech = true;
    bronzeWorking.lossChanceOnDeath = 0.15f;  // Key tech, harder to lose
    bronzeWorking.treeRow = 2;
    bronzeWorking.treeColumn = 1;
    bronzeWorking.effects.push_back(TechEffect::EnableFeature("bronze_crafting", "Enables bronze crafting"));
    AddTech(bronzeWorking);

    TechNode bronzeWeapons;
    bronzeWeapons.id = UniversalTechs::BRONZE_WEAPONS;
    bronzeWeapons.name = "Bronze Weapons";
    bronzeWeapons.description = "Stronger, sharper weapons from bronze.";
    bronzeWeapons.category = TechCategory::Military;
    bronzeWeapons.requiredAge = Age::Bronze;
    bronzeWeapons.prerequisites = {UniversalTechs::BRONZE_WORKING};
    bronzeWeapons.cost = {{ResourceType::Metal, 50}, {ResourceType::Coins, 20}};
    bronzeWeapons.researchTime = 25.0f;
    bronzeWeapons.isUniversal = true;
    bronzeWeapons.treeRow = 3;
    bronzeWeapons.treeColumn = 2;
    bronzeWeapons.effects.push_back(TechEffect::Multiplier("attack_damage", 1.2f, "+20% attack damage"));
    bronzeWeapons.effects.push_back(TechEffect::UnlockUnit("BronzeWarrior", "Unlocks bronze warriors"));
    bronzeWeapons.unlocksUnits.push_back("BronzeWarrior");
    AddTech(bronzeWeapons);

    TechNode agriculture;
    agriculture.id = UniversalTechs::AGRICULTURE;
    agriculture.name = "Agriculture";
    agriculture.description = "Systematic farming and crop cultivation.";
    agriculture.category = TechCategory::Economy;
    agriculture.requiredAge = Age::Bronze;
    agriculture.prerequisites = {UniversalTechs::FORAGING};
    agriculture.cost = {{ResourceType::Food, 80}, {ResourceType::Wood, 40}};
    agriculture.researchTime = 35.0f;
    agriculture.isUniversal = true;
    agriculture.isKeyTech = true;
    agriculture.canBeLost = false;  // Permanent tech
    agriculture.treeRow = 2;
    agriculture.treeColumn = 0;
    agriculture.effects.push_back(TechEffect::Multiplier("food_production", 1.5f, "+50% food production"));
    agriculture.effects.push_back(TechEffect::UnlockBuilding("Farm", "Unlocks Farm building"));
    agriculture.unlocksBuildings.push_back("Farm");
    AddTech(agriculture);

    TechNode pottery;
    pottery.id = UniversalTechs::POTTERY;
    pottery.name = "Pottery";
    pottery.description = "Clay vessels for storage and trade.";
    pottery.category = TechCategory::Economy;
    pottery.requiredAge = Age::Bronze;
    pottery.prerequisites = {UniversalTechs::FORAGING};
    pottery.cost = {{ResourceType::Stone, 40}};
    pottery.researchTime = 20.0f;
    pottery.isUniversal = true;
    pottery.treeRow = 3;
    pottery.treeColumn = 0;
    pottery.effects.push_back(TechEffect::Multiplier("storage_capacity", 1.25f, "+25% storage capacity"));
    AddTech(pottery);

    TechNode basicWalls;
    basicWalls.id = UniversalTechs::BASIC_WALLS;
    basicWalls.name = "Basic Walls";
    basicWalls.description = "Simple wooden palisades for defense.";
    basicWalls.category = TechCategory::Defense;
    basicWalls.requiredAge = Age::Bronze;
    basicWalls.prerequisites = {UniversalTechs::BASIC_SHELTER};
    basicWalls.cost = {{ResourceType::Wood, 80}};
    basicWalls.researchTime = 25.0f;
    basicWalls.isUniversal = true;
    basicWalls.treeRow = 2;
    basicWalls.treeColumn = 3;
    basicWalls.effects.push_back(TechEffect::UnlockBuilding("Wall", "Unlocks wooden walls"));
    basicWalls.unlocksBuildings.push_back("Wall");
    AddTech(basicWalls);

    // ========== IRON AGE TECHS ==========
    TechNode ironWorking;
    ironWorking.id = UniversalTechs::IRON_WORKING;
    ironWorking.name = "Iron Working";
    ironWorking.description = "Mastery of iron smelting and forging.";
    ironWorking.category = TechCategory::Science;
    ironWorking.requiredAge = Age::Iron;
    ironWorking.prerequisites = {UniversalTechs::BRONZE_WORKING};
    ironWorking.cost = {{ResourceType::Metal, 100}, {ResourceType::Stone, 80}};
    ironWorking.researchTime = 40.0f;
    ironWorking.isUniversal = true;
    ironWorking.isKeyTech = true;
    ironWorking.lossChanceOnDeath = 0.1f;
    ironWorking.treeRow = 4;
    ironWorking.treeColumn = 1;
    ironWorking.effects.push_back(TechEffect::EnableFeature("iron_crafting", "Enables iron crafting"));
    AddTech(ironWorking);

    TechNode ironWeapons;
    ironWeapons.id = UniversalTechs::IRON_WEAPONS;
    ironWeapons.name = "Iron Weapons";
    ironWeapons.description = "Superior iron swords and spears.";
    ironWeapons.category = TechCategory::Military;
    ironWeapons.requiredAge = Age::Iron;
    ironWeapons.prerequisites = {UniversalTechs::IRON_WORKING};
    ironWeapons.cost = {{ResourceType::Metal, 120}, {ResourceType::Coins, 50}};
    ironWeapons.researchTime = 35.0f;
    ironWeapons.isUniversal = true;
    ironWeapons.treeRow = 5;
    ironWeapons.treeColumn = 2;
    ironWeapons.effects.push_back(TechEffect::Multiplier("attack_damage", 1.3f, "+30% attack damage"));
    AddTech(ironWeapons);

    TechNode stoneFortifications;
    stoneFortifications.id = UniversalTechs::STONE_FORTIFICATIONS;
    stoneFortifications.name = "Stone Fortifications";
    stoneFortifications.description = "Massive stone walls and towers.";
    stoneFortifications.category = TechCategory::Defense;
    stoneFortifications.requiredAge = Age::Iron;
    stoneFortifications.prerequisites = {UniversalTechs::BASIC_WALLS, UniversalTechs::IRON_WORKING};
    stoneFortifications.cost = {{ResourceType::Stone, 200}, {ResourceType::Wood, 100}};
    stoneFortifications.researchTime = 45.0f;
    stoneFortifications.isUniversal = true;
    stoneFortifications.isKeyTech = true;
    stoneFortifications.treeRow = 5;
    stoneFortifications.treeColumn = 3;
    stoneFortifications.effects.push_back(TechEffect::Multiplier("wall_hp", 2.0f, "+100% wall health"));
    stoneFortifications.effects.push_back(TechEffect::UnlockBuilding("Tower", "Unlocks defensive towers"));
    stoneFortifications.unlocksBuildings.push_back("Tower");
    stoneFortifications.unlocksBuildings.push_back("StoneWall");
    AddTech(stoneFortifications);

    TechNode ironArmor;
    ironArmor.id = UniversalTechs::IRON_ARMOR;
    ironArmor.name = "Iron Armor";
    ironArmor.description = "Protective iron plates and chainmail.";
    ironArmor.category = TechCategory::Military;
    ironArmor.requiredAge = Age::Iron;
    ironArmor.prerequisites = {UniversalTechs::IRON_WORKING};
    ironArmor.cost = {{ResourceType::Metal, 150}};
    ironArmor.researchTime = 30.0f;
    ironArmor.isUniversal = true;
    ironArmor.treeRow = 5;
    ironArmor.treeColumn = 1;
    ironArmor.effects.push_back(TechEffect::Multiplier("armor", 1.4f, "+40% armor"));
    ironArmor.effects.push_back(TechEffect::UnlockUnit("HeavyInfantry", "Unlocks heavy infantry"));
    ironArmor.unlocksUnits.push_back("HeavyInfantry");
    AddTech(ironArmor);

    TechNode advancedFarming;
    advancedFarming.id = UniversalTechs::ADVANCED_FARMING;
    advancedFarming.name = "Advanced Farming";
    advancedFarming.description = "Irrigation and crop rotation techniques.";
    advancedFarming.category = TechCategory::Economy;
    advancedFarming.requiredAge = Age::Iron;
    advancedFarming.prerequisites = {UniversalTechs::AGRICULTURE, UniversalTechs::IRON_WORKING};
    advancedFarming.cost = {{ResourceType::Food, 150}, {ResourceType::Metal, 50}};
    advancedFarming.researchTime = 35.0f;
    advancedFarming.isUniversal = true;
    advancedFarming.treeRow = 4;
    advancedFarming.treeColumn = 0;
    advancedFarming.effects.push_back(TechEffect::Multiplier("food_production", 1.4f, "+40% food production"));
    AddTech(advancedFarming);

    // ========== MEDIEVAL AGE TECHS ==========
    TechNode castleBuilding;
    castleBuilding.id = UniversalTechs::CASTLE_BUILDING;
    castleBuilding.name = "Castle Building";
    castleBuilding.description = "Construction of massive fortified castles.";
    castleBuilding.category = TechCategory::Defense;
    castleBuilding.requiredAge = Age::Medieval;
    castleBuilding.prerequisites = {UniversalTechs::STONE_FORTIFICATIONS};
    castleBuilding.cost = {{ResourceType::Stone, 400}, {ResourceType::Metal, 150}, {ResourceType::Coins, 100}};
    castleBuilding.researchTime = 60.0f;
    castleBuilding.isUniversal = true;
    castleBuilding.isKeyTech = true;
    castleBuilding.lossChanceOnDeath = 0.1f;
    castleBuilding.treeRow = 6;
    castleBuilding.treeColumn = 3;
    castleBuilding.effects.push_back(TechEffect::UnlockBuilding("Castle", "Unlocks Castle"));
    castleBuilding.unlocksBuildings.push_back("Castle");
    AddTech(castleBuilding);

    TechNode siegeWeapons;
    siegeWeapons.id = UniversalTechs::SIEGE_WEAPONS;
    siegeWeapons.name = "Siege Weapons";
    siegeWeapons.description = "Trebuchets, battering rams, and catapults.";
    siegeWeapons.category = TechCategory::Military;
    siegeWeapons.requiredAge = Age::Medieval;
    siegeWeapons.prerequisites = {UniversalTechs::IRON_WEAPONS};
    siegeWeapons.cost = {{ResourceType::Wood, 300}, {ResourceType::Metal, 200}};
    siegeWeapons.researchTime = 50.0f;
    siegeWeapons.isUniversal = true;
    siegeWeapons.treeRow = 6;
    siegeWeapons.treeColumn = 2;
    siegeWeapons.effects.push_back(TechEffect::UnlockUnit("Trebuchet", "Unlocks siege weapons"));
    siegeWeapons.effects.push_back(TechEffect::Multiplier("siege_damage", 2.0f, "+100% siege damage"));
    siegeWeapons.unlocksUnits.push_back("Trebuchet");
    siegeWeapons.unlocksUnits.push_back("BatteringRam");
    AddTech(siegeWeapons);

    TechNode heavyCavalry;
    heavyCavalry.id = UniversalTechs::HEAVY_CAVALRY;
    heavyCavalry.name = "Heavy Cavalry";
    heavyCavalry.description = "Armored knights on horseback.";
    heavyCavalry.category = TechCategory::Military;
    heavyCavalry.requiredAge = Age::Medieval;
    heavyCavalry.prerequisites = {UniversalTechs::IRON_ARMOR};
    heavyCavalry.cost = {{ResourceType::Metal, 250}, {ResourceType::Food, 150}};
    heavyCavalry.researchTime = 45.0f;
    heavyCavalry.isUniversal = true;
    heavyCavalry.treeRow = 6;
    heavyCavalry.treeColumn = 1;
    heavyCavalry.effects.push_back(TechEffect::UnlockUnit("Knight", "Unlocks knights"));
    heavyCavalry.unlocksUnits.push_back("Knight");
    AddTech(heavyCavalry);

    TechNode crossbows;
    crossbows.id = UniversalTechs::CROSSBOWS;
    crossbows.name = "Crossbows";
    crossbows.description = "Powerful ranged weapons that pierce armor.";
    crossbows.category = TechCategory::Military;
    crossbows.requiredAge = Age::Medieval;
    crossbows.prerequisites = {UniversalTechs::IRON_WEAPONS};
    crossbows.cost = {{ResourceType::Wood, 100}, {ResourceType::Metal, 80}};
    crossbows.researchTime = 35.0f;
    crossbows.isUniversal = true;
    crossbows.treeRow = 7;
    crossbows.treeColumn = 2;
    crossbows.effects.push_back(TechEffect::UnlockUnit("Crossbowman", "Unlocks crossbowmen"));
    crossbows.effects.push_back(TechEffect::Multiplier("ranged_damage", 1.3f, "+30% ranged damage"));
    crossbows.unlocksUnits.push_back("Crossbowman");
    AddTech(crossbows);

    TechNode guilds;
    guilds.id = UniversalTechs::GUILDS;
    guilds.name = "Guilds";
    guilds.description = "Organized trade and craft guilds.";
    guilds.category = TechCategory::Economy;
    guilds.requiredAge = Age::Medieval;
    guilds.prerequisites = {UniversalTechs::ADVANCED_FARMING};
    guilds.cost = {{ResourceType::Coins, 150}, {ResourceType::Food, 100}};
    guilds.researchTime = 40.0f;
    guilds.isUniversal = true;
    guilds.isKeyTech = true;
    guilds.treeRow = 6;
    guilds.treeColumn = 0;
    guilds.effects.push_back(TechEffect::Multiplier("production_speed", 1.25f, "+25% production speed"));
    guilds.effects.push_back(TechEffect::Multiplier("trade_profit", 1.3f, "+30% trade profit"));
    AddTech(guilds);

    // ========== INDUSTRIAL AGE TECHS ==========
    TechNode steamPower;
    steamPower.id = UniversalTechs::STEAM_POWER;
    steamPower.name = "Steam Power";
    steamPower.description = "Harness the power of steam engines.";
    steamPower.category = TechCategory::Science;
    steamPower.requiredAge = Age::Industrial;
    steamPower.prerequisites = {UniversalTechs::GUILDS};
    steamPower.cost = {{ResourceType::Metal, 300}, {ResourceType::Fuel, 100}};
    steamPower.researchTime = 60.0f;
    steamPower.isUniversal = true;
    steamPower.isKeyTech = true;
    steamPower.lossChanceOnDeath = 0.1f;
    steamPower.treeRow = 8;
    steamPower.treeColumn = 1;
    steamPower.effects.push_back(TechEffect::EnableFeature("steam_power", "Enables steam-powered machinery"));
    steamPower.effects.push_back(TechEffect::Multiplier("production_speed", 1.5f, "+50% production speed"));
    AddTech(steamPower);

    TechNode firearms;
    firearms.id = UniversalTechs::FIREARMS;
    firearms.name = "Firearms";
    firearms.description = "Muskets and early rifles change warfare.";
    firearms.category = TechCategory::Military;
    firearms.requiredAge = Age::Industrial;
    firearms.prerequisites = {UniversalTechs::CROSSBOWS};
    firearms.cost = {{ResourceType::Metal, 200}, {ResourceType::Ammunition, 100}};
    firearms.researchTime = 50.0f;
    firearms.isUniversal = true;
    firearms.treeRow = 8;
    firearms.treeColumn = 2;
    firearms.effects.push_back(TechEffect::UnlockUnit("Musketeer", "Unlocks musketeers"));
    firearms.effects.push_back(TechEffect::Multiplier("ranged_damage", 1.5f, "+50% ranged damage"));
    firearms.unlocksUnits.push_back("Musketeer");
    AddTech(firearms);

    TechNode factories;
    factories.id = UniversalTechs::FACTORIES;
    factories.name = "Factories";
    factories.description = "Large-scale manufacturing facilities.";
    factories.category = TechCategory::Infrastructure;
    factories.requiredAge = Age::Industrial;
    factories.prerequisites = {UniversalTechs::STEAM_POWER};
    factories.cost = {{ResourceType::Metal, 400}, {ResourceType::Stone, 200}};
    factories.researchTime = 55.0f;
    factories.isUniversal = true;
    factories.isKeyTech = true;
    factories.treeRow = 9;
    factories.treeColumn = 0;
    factories.effects.push_back(TechEffect::UnlockBuilding("Factory", "Unlocks Factory"));
    factories.effects.push_back(TechEffect::Multiplier("production_speed", 2.0f, "+100% production speed"));
    factories.unlocksBuildings.push_back("Factory");
    AddTech(factories);

    TechNode railroads;
    railroads.id = UniversalTechs::RAILROADS;
    railroads.name = "Railroads";
    railroads.description = "Rail networks for rapid transport.";
    railroads.category = TechCategory::Infrastructure;
    railroads.requiredAge = Age::Industrial;
    railroads.prerequisites = {UniversalTechs::STEAM_POWER};
    railroads.cost = {{ResourceType::Metal, 350}, {ResourceType::Wood, 200}};
    railroads.researchTime = 50.0f;
    railroads.isUniversal = true;
    railroads.treeRow = 9;
    railroads.treeColumn = 1;
    railroads.effects.push_back(TechEffect::Multiplier("movement_speed", 1.5f, "+50% movement speed"));
    railroads.effects.push_back(TechEffect::Multiplier("trade_profit", 1.4f, "+40% trade profit"));
    AddTech(railroads);

    TechNode artillery;
    artillery.id = UniversalTechs::ARTILLERY;
    artillery.name = "Artillery";
    artillery.description = "Powerful cannons and field guns.";
    artillery.category = TechCategory::Military;
    artillery.requiredAge = Age::Industrial;
    artillery.prerequisites = {UniversalTechs::FIREARMS, UniversalTechs::SIEGE_WEAPONS};
    artillery.cost = {{ResourceType::Metal, 400}, {ResourceType::Ammunition, 200}};
    artillery.researchTime = 60.0f;
    artillery.isUniversal = true;
    artillery.treeRow = 9;
    artillery.treeColumn = 2;
    artillery.effects.push_back(TechEffect::UnlockUnit("Cannon", "Unlocks artillery cannons"));
    artillery.effects.push_back(TechEffect::Multiplier("siege_damage", 2.5f, "+150% siege damage"));
    artillery.unlocksUnits.push_back("Cannon");
    AddTech(artillery);

    // ========== MODERN AGE TECHS ==========
    TechNode electricity;
    electricity.id = UniversalTechs::ELECTRICITY;
    electricity.name = "Electricity";
    electricity.description = "Harness electrical power for everything.";
    electricity.category = TechCategory::Science;
    electricity.requiredAge = Age::Modern;
    electricity.prerequisites = {UniversalTechs::FACTORIES};
    electricity.cost = {{ResourceType::Metal, 500}, {ResourceType::Fuel, 300}};
    electricity.researchTime = 75.0f;
    electricity.isUniversal = true;
    electricity.isKeyTech = true;
    electricity.lossChanceOnDeath = 0.05f;
    electricity.treeRow = 10;
    electricity.treeColumn = 1;
    electricity.effects.push_back(TechEffect::EnableFeature("electricity", "Enables electric power"));
    electricity.effects.push_back(TechEffect::Multiplier("production_speed", 1.75f, "+75% production speed"));
    electricity.effects.push_back(TechEffect::UnlockBuilding("PowerPlant", "Unlocks Power Plant"));
    electricity.unlocksBuildings.push_back("PowerPlant");
    AddTech(electricity);

    TechNode combustionEngine;
    combustionEngine.id = UniversalTechs::COMBUSTION_ENGINE;
    combustionEngine.name = "Combustion Engine";
    combustionEngine.description = "Internal combustion engines for vehicles.";
    combustionEngine.category = TechCategory::Science;
    combustionEngine.requiredAge = Age::Modern;
    combustionEngine.prerequisites = {UniversalTechs::RAILROADS};
    combustionEngine.cost = {{ResourceType::Metal, 400}, {ResourceType::Fuel, 250}};
    combustionEngine.researchTime = 60.0f;
    combustionEngine.isUniversal = true;
    combustionEngine.treeRow = 10;
    combustionEngine.treeColumn = 0;
    combustionEngine.effects.push_back(TechEffect::EnableFeature("vehicles", "Enables motorized vehicles"));
    combustionEngine.effects.push_back(TechEffect::Multiplier("movement_speed", 2.0f, "+100% movement speed"));
    AddTech(combustionEngine);

    TechNode radioComm;
    radioComm.id = UniversalTechs::RADIO_COMM;
    radioComm.name = "Radio Communication";
    radioComm.description = "Wireless communication across distances.";
    radioComm.category = TechCategory::Science;
    radioComm.requiredAge = Age::Modern;
    radioComm.prerequisites = {UniversalTechs::ELECTRICITY};
    radioComm.cost = {{ResourceType::Metal, 300}, {ResourceType::Coins, 200}};
    radioComm.researchTime = 50.0f;
    radioComm.isUniversal = true;
    radioComm.treeRow = 11;
    radioComm.treeColumn = 1;
    radioComm.effects.push_back(TechEffect::Multiplier("vision_range", 1.5f, "+50% vision range"));
    radioComm.effects.push_back(TechEffect::EnableFeature("long_range_comm", "Enables long-range communication"));
    AddTech(radioComm);

    TechNode automaticWeapons;
    automaticWeapons.id = UniversalTechs::AUTOMATIC_WEAPONS;
    automaticWeapons.name = "Automatic Weapons";
    automaticWeapons.description = "Machine guns and automatic rifles.";
    automaticWeapons.category = TechCategory::Military;
    automaticWeapons.requiredAge = Age::Modern;
    automaticWeapons.prerequisites = {UniversalTechs::FIREARMS, UniversalTechs::ELECTRICITY};
    automaticWeapons.cost = {{ResourceType::Metal, 400}, {ResourceType::Ammunition, 300}};
    automaticWeapons.researchTime = 55.0f;
    automaticWeapons.isUniversal = true;
    automaticWeapons.treeRow = 11;
    automaticWeapons.treeColumn = 2;
    automaticWeapons.effects.push_back(TechEffect::Multiplier("attack_speed", 2.0f, "+100% attack speed"));
    automaticWeapons.effects.push_back(TechEffect::UnlockUnit("MachineGunner", "Unlocks machine gunners"));
    automaticWeapons.unlocksUnits.push_back("MachineGunner");
    AddTech(automaticWeapons);

    TechNode tanks;
    tanks.id = UniversalTechs::TANKS;
    tanks.name = "Tanks";
    tanks.description = "Armored fighting vehicles.";
    tanks.category = TechCategory::Military;
    tanks.requiredAge = Age::Modern;
    tanks.prerequisites = {UniversalTechs::COMBUSTION_ENGINE, UniversalTechs::ARTILLERY};
    tanks.cost = {{ResourceType::Metal, 600}, {ResourceType::Fuel, 400}};
    tanks.researchTime = 70.0f;
    tanks.isUniversal = true;
    tanks.treeRow = 11;
    tanks.treeColumn = 0;
    tanks.effects.push_back(TechEffect::UnlockUnit("Tank", "Unlocks tanks"));
    tanks.unlocksUnits.push_back("Tank");
    AddTech(tanks);

    // ========== FUTURE AGE TECHS ==========
    TechNode fusionPower;
    fusionPower.id = UniversalTechs::FUSION_POWER;
    fusionPower.name = "Fusion Power";
    fusionPower.description = "Clean, unlimited energy from nuclear fusion.";
    fusionPower.category = TechCategory::Science;
    fusionPower.requiredAge = Age::Future;
    fusionPower.prerequisites = {UniversalTechs::ELECTRICITY};
    fusionPower.cost = {{ResourceType::Metal, 1000}, {ResourceType::Fuel, 500}};
    fusionPower.researchTime = 90.0f;
    fusionPower.isUniversal = true;
    fusionPower.isKeyTech = true;
    fusionPower.lossChanceOnDeath = 0.05f;
    fusionPower.treeRow = 12;
    fusionPower.treeColumn = 1;
    fusionPower.effects.push_back(TechEffect::EnableFeature("fusion_power", "Enables fusion power"));
    fusionPower.effects.push_back(TechEffect::Multiplier("all_production", 2.0f, "+100% all production"));
    AddTech(fusionPower);

    TechNode energyShields;
    energyShields.id = UniversalTechs::ENERGY_SHIELDS;
    energyShields.name = "Energy Shields";
    energyShields.description = "Force fields that absorb damage.";
    energyShields.category = TechCategory::Defense;
    energyShields.requiredAge = Age::Future;
    energyShields.prerequisites = {UniversalTechs::FUSION_POWER};
    energyShields.cost = {{ResourceType::Metal, 800}, {ResourceType::Fuel, 600}};
    energyShields.researchTime = 80.0f;
    energyShields.isUniversal = true;
    energyShields.treeRow = 13;
    energyShields.treeColumn = 3;
    energyShields.effects.push_back(TechEffect::FlatBonus("shield_hp", 500.0f, "+500 shield points"));
    energyShields.effects.push_back(TechEffect::Multiplier("damage_reduction", 1.5f, "+50% damage reduction"));
    AddTech(energyShields);

    TechNode plasmaWeapons;
    plasmaWeapons.id = UniversalTechs::PLASMA_WEAPONS;
    plasmaWeapons.name = "Plasma Weapons";
    plasmaWeapons.description = "Devastating weapons using superheated plasma.";
    plasmaWeapons.category = TechCategory::Military;
    plasmaWeapons.requiredAge = Age::Future;
    plasmaWeapons.prerequisites = {UniversalTechs::FUSION_POWER, UniversalTechs::AUTOMATIC_WEAPONS};
    plasmaWeapons.cost = {{ResourceType::Metal, 900}, {ResourceType::Fuel, 700}};
    plasmaWeapons.researchTime = 85.0f;
    plasmaWeapons.isUniversal = true;
    plasmaWeapons.treeRow = 13;
    plasmaWeapons.treeColumn = 2;
    plasmaWeapons.effects.push_back(TechEffect::Multiplier("attack_damage", 2.5f, "+150% attack damage"));
    plasmaWeapons.effects.push_back(TechEffect::UnlockUnit("PlasmaRifleman", "Unlocks plasma units"));
    plasmaWeapons.unlocksUnits.push_back("PlasmaRifleman");
    AddTech(plasmaWeapons);

    TechNode aiSystems;
    aiSystems.id = UniversalTechs::AI_SYSTEMS;
    aiSystems.name = "AI Systems";
    aiSystems.description = "Advanced artificial intelligence for automation.";
    aiSystems.category = TechCategory::Science;
    aiSystems.requiredAge = Age::Future;
    aiSystems.prerequisites = {UniversalTechs::RADIO_COMM, UniversalTechs::FUSION_POWER};
    aiSystems.cost = {{ResourceType::Metal, 700}, {ResourceType::Coins, 500}};
    aiSystems.researchTime = 75.0f;
    aiSystems.isUniversal = true;
    aiSystems.treeRow = 13;
    aiSystems.treeColumn = 0;
    aiSystems.effects.push_back(TechEffect::Multiplier("production_speed", 2.5f, "+150% production speed"));
    aiSystems.effects.push_back(TechEffect::EnableFeature("auto_management", "Enables automatic base management"));
    AddTech(aiSystems);

    TechNode nanotech;
    nanotech.id = UniversalTechs::NANOTECH;
    nanotech.name = "Nanotechnology";
    nanotech.description = "Molecular-scale construction and repair.";
    nanotech.category = TechCategory::Science;
    nanotech.requiredAge = Age::Future;
    nanotech.prerequisites = {UniversalTechs::AI_SYSTEMS, UniversalTechs::ENERGY_SHIELDS};
    nanotech.cost = {{ResourceType::Metal, 1200}, {ResourceType::Coins, 800}};
    nanotech.researchTime = 100.0f;
    nanotech.isUniversal = true;
    nanotech.isKeyTech = true;
    nanotech.canBeLost = false;  // Ultimate tech, permanent
    nanotech.treeRow = 14;
    nanotech.treeColumn = 1;
    nanotech.effects.push_back(TechEffect::Multiplier("repair_speed", 5.0f, "+400% repair speed"));
    nanotech.effects.push_back(TechEffect::FlatBonus("health_regen", 10.0f, "+10 health regeneration"));
    nanotech.effects.push_back(TechEffect::EnableFeature("nano_repair", "Enables automatic repairs"));
    AddTech(nanotech);
}

void TechTree::InitializeCultureTechs(CultureType culture) {
    // Add culture-specific techs based on selected culture
    switch (culture) {
        case CultureType::Fortress:
            // Fortress culture - defensive specialists
            {
                TechNode stoneMasonry;
                stoneMasonry.id = FortressTechs::STONE_MASONRY;
                stoneMasonry.name = "Stone Masonry";
                stoneMasonry.description = "Advanced stone cutting and construction techniques.";
                stoneMasonry.category = TechCategory::Defense;
                stoneMasonry.requiredAge = Age::Bronze;
                stoneMasonry.prerequisites = {UniversalTechs::BASIC_WALLS};
                stoneMasonry.cost = {{ResourceType::Stone, 100}};
                stoneMasonry.researchTime = 30.0f;
                stoneMasonry.availableToCultures = {CultureType::Fortress};
                stoneMasonry.treeRow = 3;
                stoneMasonry.treeColumn = 4;
                stoneMasonry.effects.push_back(TechEffect::Multiplier("wall_hp", 1.5f, "+50% wall health"));
                stoneMasonry.effects.push_back(TechEffect::Multiplier("build_speed_walls", 1.3f, "+30% wall build speed"));
                AddTech(stoneMasonry);

                TechNode thickWalls;
                thickWalls.id = FortressTechs::THICK_WALLS;
                thickWalls.name = "Thick Walls";
                thickWalls.description = "Doubled wall thickness for maximum protection.";
                thickWalls.category = TechCategory::Defense;
                thickWalls.requiredAge = Age::Iron;
                thickWalls.prerequisites = {FortressTechs::STONE_MASONRY, UniversalTechs::STONE_FORTIFICATIONS};
                thickWalls.cost = {{ResourceType::Stone, 250}, {ResourceType::Metal, 50}};
                thickWalls.researchTime = 45.0f;
                thickWalls.availableToCultures = {CultureType::Fortress};
                thickWalls.treeRow = 5;
                thickWalls.treeColumn = 4;
                thickWalls.effects.push_back(TechEffect::Multiplier("wall_hp", 2.0f, "+100% wall health"));
                thickWalls.effects.push_back(TechEffect::Multiplier("wall_armor", 1.5f, "+50% wall armor"));
                AddTech(thickWalls);

                TechNode castleKeep;
                castleKeep.id = FortressTechs::CASTLE_KEEP;
                castleKeep.name = "Castle Keep";
                castleKeep.description = "A massive central fortress that provides bonuses to all defenders.";
                castleKeep.category = TechCategory::Defense;
                castleKeep.requiredAge = Age::Medieval;
                castleKeep.prerequisites = {FortressTechs::THICK_WALLS, UniversalTechs::CASTLE_BUILDING};
                castleKeep.cost = {{ResourceType::Stone, 500}, {ResourceType::Metal, 200}, {ResourceType::Coins, 150}};
                castleKeep.researchTime = 75.0f;
                castleKeep.availableToCultures = {CultureType::Fortress};
                castleKeep.isKeyTech = true;
                castleKeep.lossChanceOnDeath = 0.1f;
                castleKeep.treeRow = 7;
                castleKeep.treeColumn = 4;
                castleKeep.effects.push_back(TechEffect::UnlockBuilding("Keep", "Unlocks the Castle Keep"));
                castleKeep.effects.push_back(TechEffect::Multiplier("defender_damage", 1.3f, "+30% defender damage"));
                castleKeep.unlocksBuildings.push_back("Keep");
                AddTech(castleKeep);

                TechNode impenetrable;
                impenetrable.id = FortressTechs::IMPENETRABLE;
                impenetrable.name = "Impenetrable Defense";
                impenetrable.description = "Ultimate fortress technology - nearly invulnerable walls.";
                impenetrable.category = TechCategory::Special;
                impenetrable.requiredAge = Age::Industrial;
                impenetrable.prerequisites = {FortressTechs::CASTLE_KEEP};
                impenetrable.cost = {{ResourceType::Stone, 800}, {ResourceType::Metal, 400}};
                impenetrable.researchTime = 90.0f;
                impenetrable.availableToCultures = {CultureType::Fortress};
                impenetrable.isKeyTech = true;
                impenetrable.canBeLost = false;
                impenetrable.treeRow = 9;
                impenetrable.treeColumn = 4;
                impenetrable.effects.push_back(TechEffect::Multiplier("wall_hp", 3.0f, "+200% wall health"));
                impenetrable.effects.push_back(TechEffect::UnlockAbility("fortress_mode", "Unlocks Fortress Mode ability"));
                impenetrable.unlocksAbilities.push_back("fortress_mode");
                AddTech(impenetrable);
            }
            break;

        case CultureType::Nomad:
            // Nomad culture - mobility specialists
            {
                TechNode mobileCamps;
                mobileCamps.id = NomadTechs::MOBILE_CAMPS;
                mobileCamps.name = "Mobile Camps";
                mobileCamps.description = "Quickly packable buildings that can relocate.";
                mobileCamps.category = TechCategory::Infrastructure;
                mobileCamps.requiredAge = Age::Bronze;
                mobileCamps.prerequisites = {UniversalTechs::BASIC_SHELTER};
                mobileCamps.cost = {{ResourceType::Wood, 80}, {ResourceType::Food, 40}};
                mobileCamps.researchTime = 25.0f;
                mobileCamps.availableToCultures = {CultureType::Nomad};
                mobileCamps.treeRow = 2;
                mobileCamps.treeColumn = 4;
                mobileCamps.effects.push_back(TechEffect::EnableFeature("mobile_buildings", "Buildings can be packed and moved"));
                mobileCamps.effects.push_back(TechEffect::Multiplier("pack_speed", 2.0f, "+100% packing speed"));
                AddTech(mobileCamps);

                TechNode hitAndRun;
                hitAndRun.id = NomadTechs::HIT_AND_RUN;
                hitAndRun.name = "Hit and Run Tactics";
                hitAndRun.description = "Strike fast and retreat before the enemy can react.";
                hitAndRun.category = TechCategory::Military;
                hitAndRun.requiredAge = Age::Iron;
                hitAndRun.prerequisites = {NomadTechs::MOBILE_CAMPS, UniversalTechs::IRON_WEAPONS};
                hitAndRun.cost = {{ResourceType::Food, 100}, {ResourceType::Coins, 50}};
                hitAndRun.researchTime = 35.0f;
                hitAndRun.availableToCultures = {CultureType::Nomad};
                hitAndRun.treeRow = 5;
                hitAndRun.treeColumn = 4;
                hitAndRun.effects.push_back(TechEffect::Multiplier("movement_speed", 1.4f, "+40% movement speed"));
                hitAndRun.effects.push_back(TechEffect::UnlockAbility("tactical_retreat", "Unlocks Tactical Retreat"));
                hitAndRun.unlocksAbilities.push_back("tactical_retreat");
                AddTech(hitAndRun);

                TechNode windRiders;
                windRiders.id = NomadTechs::WIND_RIDERS;
                windRiders.name = "Wind Riders";
                windRiders.description = "Elite mounted units with unmatched speed.";
                windRiders.category = TechCategory::Special;
                windRiders.requiredAge = Age::Medieval;
                windRiders.prerequisites = {NomadTechs::HIT_AND_RUN, UniversalTechs::HEAVY_CAVALRY};
                windRiders.cost = {{ResourceType::Food, 300}, {ResourceType::Metal, 150}};
                windRiders.researchTime = 60.0f;
                windRiders.availableToCultures = {CultureType::Nomad};
                windRiders.isKeyTech = true;
                windRiders.treeRow = 7;
                windRiders.treeColumn = 4;
                windRiders.effects.push_back(TechEffect::UnlockUnit("WindRider", "Unlocks Wind Rider cavalry"));
                windRiders.effects.push_back(TechEffect::Multiplier("cavalry_speed", 2.0f, "+100% cavalry speed"));
                windRiders.unlocksUnits.push_back("WindRider");
                AddTech(windRiders);
            }
            break;

        case CultureType::Merchant:
            // Merchant culture - economic specialists
            {
                TechNode tradeRoutes;
                tradeRoutes.id = MerchantTechs::TRADE_ROUTES;
                tradeRoutes.name = "Trade Routes";
                tradeRoutes.description = "Established paths for safe and profitable trade.";
                tradeRoutes.category = TechCategory::Economy;
                tradeRoutes.requiredAge = Age::Bronze;
                tradeRoutes.prerequisites = {UniversalTechs::AGRICULTURE};
                tradeRoutes.cost = {{ResourceType::Coins, 50}, {ResourceType::Food, 50}};
                tradeRoutes.researchTime = 30.0f;
                tradeRoutes.availableToCultures = {CultureType::Merchant};
                tradeRoutes.treeRow = 3;
                tradeRoutes.treeColumn = 4;
                tradeRoutes.effects.push_back(TechEffect::Multiplier("trade_profit", 1.5f, "+50% trade profit"));
                tradeRoutes.effects.push_back(TechEffect::UnlockBuilding("TradingPost", "Unlocks Trading Post"));
                tradeRoutes.unlocksBuildings.push_back("TradingPost");
                AddTech(tradeRoutes);

                TechNode mercenaries;
                mercenaries.id = MerchantTechs::MERCENARIES;
                mercenaries.name = "Mercenary Contracts";
                mercenaries.description = "Hire powerful fighters with gold.";
                mercenaries.category = TechCategory::Military;
                mercenaries.requiredAge = Age::Iron;
                mercenaries.prerequisites = {MerchantTechs::TRADE_ROUTES};
                mercenaries.cost = {{ResourceType::Coins, 200}};
                mercenaries.researchTime = 40.0f;
                mercenaries.availableToCultures = {CultureType::Merchant};
                mercenaries.treeRow = 5;
                mercenaries.treeColumn = 4;
                mercenaries.effects.push_back(TechEffect::UnlockUnit("Mercenary", "Unlocks mercenary units"));
                mercenaries.effects.push_back(TechEffect::EnableFeature("hire_mercs", "Can hire mercenaries for gold"));
                mercenaries.unlocksUnits.push_back("Mercenary");
                AddTech(mercenaries);

                TechNode tradeEmpire;
                tradeEmpire.id = MerchantTechs::TRADE_EMPIRE;
                tradeEmpire.name = "Trade Empire";
                tradeEmpire.description = "Your commercial network spans the world.";
                tradeEmpire.category = TechCategory::Special;
                tradeEmpire.requiredAge = Age::Industrial;
                tradeEmpire.prerequisites = {MerchantTechs::MERCENARIES, UniversalTechs::GUILDS};
                tradeEmpire.cost = {{ResourceType::Coins, 1000}};
                tradeEmpire.researchTime = 80.0f;
                tradeEmpire.availableToCultures = {CultureType::Merchant};
                tradeEmpire.isKeyTech = true;
                tradeEmpire.canBeLost = false;
                tradeEmpire.treeRow = 9;
                tradeEmpire.treeColumn = 4;
                tradeEmpire.effects.push_back(TechEffect::Multiplier("trade_profit", 3.0f, "+200% trade profit"));
                tradeEmpire.effects.push_back(TechEffect::Multiplier("gold_income", 2.0f, "+100% gold income"));
                AddTech(tradeEmpire);
            }
            break;

        case CultureType::Industrial:
            // Industrial culture - production specialists
            {
                TechNode assemblyLine;
                assemblyLine.id = IndustrialTechs::ASSEMBLY_LINE;
                assemblyLine.name = "Assembly Line";
                assemblyLine.description = "Streamlined production for maximum efficiency.";
                assemblyLine.category = TechCategory::Economy;
                assemblyLine.requiredAge = Age::Iron;
                assemblyLine.prerequisites = {UniversalTechs::ADVANCED_FARMING};
                assemblyLine.cost = {{ResourceType::Metal, 100}, {ResourceType::Wood, 80}};
                assemblyLine.researchTime = 35.0f;
                assemblyLine.availableToCultures = {CultureType::Industrial};
                assemblyLine.treeRow = 4;
                assemblyLine.treeColumn = 4;
                assemblyLine.effects.push_back(TechEffect::Multiplier("production_speed", 1.5f, "+50% production speed"));
                AddTech(assemblyLine);

                TechNode automation;
                automation.id = IndustrialTechs::AUTOMATION;
                automation.name = "Automation";
                automation.description = "Machines that work without constant supervision.";
                automation.category = TechCategory::Science;
                automation.requiredAge = Age::Industrial;
                automation.prerequisites = {IndustrialTechs::ASSEMBLY_LINE, UniversalTechs::STEAM_POWER};
                automation.cost = {{ResourceType::Metal, 300}, {ResourceType::Fuel, 150}};
                automation.researchTime = 55.0f;
                automation.availableToCultures = {CultureType::Industrial};
                automation.treeRow = 8;
                automation.treeColumn = 4;
                automation.effects.push_back(TechEffect::Multiplier("worker_efficiency", 2.0f, "+100% worker efficiency"));
                automation.effects.push_back(TechEffect::FlatBonus("auto_production", 10.0f, "+10 automatic production"));
                AddTech(automation);

                TechNode revolution;
                revolution.id = IndustrialTechs::REVOLUTION;
                revolution.name = "Industrial Revolution";
                revolution.description = "Complete transformation of your economy.";
                revolution.category = TechCategory::Special;
                revolution.requiredAge = Age::Modern;
                revolution.prerequisites = {IndustrialTechs::AUTOMATION, UniversalTechs::FACTORIES};
                revolution.cost = {{ResourceType::Metal, 800}, {ResourceType::Fuel, 500}, {ResourceType::Coins, 400}};
                revolution.researchTime = 100.0f;
                revolution.availableToCultures = {CultureType::Industrial};
                revolution.isKeyTech = true;
                revolution.canBeLost = false;
                revolution.treeRow = 11;
                revolution.treeColumn = 4;
                revolution.effects.push_back(TechEffect::Multiplier("all_production", 3.0f, "+200% all production"));
                revolution.effects.push_back(TechEffect::Multiplier("build_speed", 2.0f, "+100% build speed"));
                AddTech(revolution);
            }
            break;

        default:
            // Other cultures get basic additional techs
            break;
    }
}

void TechTree::AddTech(TechNode tech) {
    m_allTechs[tech.id] = std::move(tech);
}

void TechTree::BuildTechDependencies() {
    // Verify all prerequisites exist
    for (auto& [id, tech] : m_allTechs) {
        for (const auto& prereq : tech.prerequisites) {
            if (m_allTechs.find(prereq) == m_allTechs.end()) {
                // Prerequisite doesn't exist - remove it
                // In production, this would log a warning
            }
        }
    }
}

const TechNode* TechTree::GetTech(const std::string& techId) const {
    auto it = m_allTechs.find(techId);
    return (it != m_allTechs.end()) ? &it->second : nullptr;
}

std::vector<const TechNode*> TechTree::GetAvailableTechs() const {
    std::vector<const TechNode*> result;

    for (const auto& [id, tech] : m_allTechs) {
        if (CanResearch(id)) {
            result.push_back(&tech);
        }
    }

    return result;
}

std::vector<const TechNode*> TechTree::GetTechsForAge(Age age) const {
    std::vector<const TechNode*> result;

    for (const auto& [id, tech] : m_allTechs) {
        if (tech.requiredAge == age && tech.IsAvailableTo(m_culture)) {
            result.push_back(&tech);
        }
    }

    return result;
}

std::vector<const TechNode*> TechTree::GetTechsByCategory(TechCategory category) const {
    std::vector<const TechNode*> result;

    for (const auto& [id, tech] : m_allTechs) {
        if (tech.category == category && tech.IsAvailableTo(m_culture)) {
            result.push_back(&tech);
        }
    }

    return result;
}

std::vector<const TechNode*> TechTree::GetTechsUnlockingBuilding(const std::string& buildingId) const {
    std::vector<const TechNode*> result;

    for (const auto& [id, tech] : m_allTechs) {
        for (const auto& unlock : tech.unlocksBuildings) {
            if (unlock == buildingId) {
                result.push_back(&tech);
                break;
            }
        }
    }

    return result;
}

bool TechTree::HasTech(const std::string& techId) const {
    return m_researchedTechs.count(techId) > 0;
}

bool TechTree::CanResearch(const std::string& techId) const {
    // Already researched?
    if (HasTech(techId)) {
        return false;
    }

    // Tech exists?
    const TechNode* tech = GetTech(techId);
    if (!tech) {
        return false;
    }

    // Available to our culture?
    if (!tech->IsAvailableTo(m_culture)) {
        return false;
    }

    // Age requirement met?
    if (static_cast<int>(tech->requiredAge) > static_cast<int>(m_currentAge)) {
        return false;
    }

    // Prerequisites met?
    for (const auto& prereq : tech->prerequisites) {
        if (!HasTech(prereq)) {
            return false;
        }
    }

    return true;
}

TechStatus TechTree::GetTechStatus(const std::string& techId) const {
    // Currently researching?
    if (m_currentResearch == techId) {
        return TechStatus::InProgress;
    }

    // Already completed?
    if (HasTech(techId)) {
        return TechStatus::Completed;
    }

    // Check for lost status
    auto it = m_techProgress.find(techId);
    if (it != m_techProgress.end() && it->second.status == TechStatus::Lost) {
        return TechStatus::Lost;
    }

    // Can research?
    if (CanResearch(techId)) {
        return TechStatus::Available;
    }

    return TechStatus::Locked;
}

std::optional<ResearchProgress> TechTree::GetResearchProgress(const std::string& techId) const {
    auto it = m_techProgress.find(techId);
    if (it != m_techProgress.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> TechTree::GetMissingPrerequisites(const std::string& techId) const {
    std::vector<std::string> missing;

    const TechNode* tech = GetTech(techId);
    if (!tech) return missing;

    for (const auto& prereq : tech->prerequisites) {
        if (!HasTech(prereq)) {
            missing.push_back(prereq);
        }
    }

    return missing;
}

bool TechTree::StartResearch(const std::string& techId) {
    if (!CanResearch(techId)) {
        return false;
    }

    // Cancel any current research
    if (!m_currentResearch.empty()) {
        CancelResearch(0.0f);  // No refund when starting new research
    }

    const TechNode* tech = GetTech(techId);
    if (!tech) return false;

    m_currentResearch = techId;
    m_researchProgress = 0.0f;

    // Create/update progress entry
    ResearchProgress& progress = m_techProgress[techId];
    progress.techId = techId;
    progress.status = TechStatus::InProgress;
    progress.progressTime = 0.0f;
    progress.totalTime = tech->researchTime;
    progress.startedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    return true;
}

void TechTree::UpdateResearch(float deltaTime) {
    // Update age advancement
    if (m_isAdvancingAge) {
        m_ageAdvancementProgress += deltaTime / m_ageAdvancementTime;
        if (m_ageAdvancementProgress >= 1.0f) {
            AdvanceAge();
        }
    }

    // Update tech protection duration
    if (m_protectionDuration > 0.0f) {
        m_protectionDuration -= deltaTime;
        if (m_protectionDuration <= 0.0f) {
            m_temporaryProtection = 0.0f;
            m_protectionDuration = 0.0f;
        }
    }

    // Update current research
    if (m_currentResearch.empty()) {
        ProcessResearchQueue();
        return;
    }

    const TechNode* tech = GetTech(m_currentResearch);
    if (!tech) {
        m_currentResearch.clear();
        return;
    }

    // Update progress
    auto& progress = m_techProgress[m_currentResearch];
    progress.progressTime += deltaTime;
    m_researchProgress = progress.GetProgressPercent();
    m_totalResearchTime += deltaTime;

    // Check completion
    if (progress.progressTime >= progress.totalTime) {
        ProcessResearchCompletion();
    }
}

void TechTree::ProcessResearchCompletion() {
    if (m_currentResearch.empty()) return;

    const TechNode* tech = GetTech(m_currentResearch);
    if (!tech) {
        m_currentResearch.clear();
        return;
    }

    // Mark as completed
    m_researchedTechs.insert(m_currentResearch);

    auto& progress = m_techProgress[m_currentResearch];
    progress.status = TechStatus::Completed;
    progress.completedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    progress.timesResearched++;

    m_totalTechsResearched++;

    // Recalculate bonuses
    RecalculateBonuses();

    // Trigger callback
    if (m_onResearchComplete) {
        m_onResearchComplete(m_currentResearch, *tech);
    }

    // Clear current research
    std::string completedTech = m_currentResearch;
    m_currentResearch.clear();
    m_researchProgress = 0.0f;

    // Process queue
    ProcessResearchQueue();
}

void TechTree::ProcessResearchQueue() {
    while (!m_researchQueue.empty() && m_currentResearch.empty()) {
        std::string nextTech = m_researchQueue.front();
        m_researchQueue.erase(m_researchQueue.begin());

        if (CanResearch(nextTech)) {
            StartResearch(nextTech);
            break;
        }
    }
}

void TechTree::CompleteResearch() {
    if (m_currentResearch.empty()) return;

    auto& progress = m_techProgress[m_currentResearch];
    progress.progressTime = progress.totalTime;
    ProcessResearchCompletion();
}

std::map<ResourceType, int> TechTree::CancelResearch(float refundPercent) {
    std::map<ResourceType, int> refund;

    if (m_currentResearch.empty()) return refund;

    const TechNode* tech = GetTech(m_currentResearch);
    if (tech && refundPercent > 0.0f) {
        // Calculate refund based on progress
        float progressMult = 1.0f - m_researchProgress;
        float totalRefund = refundPercent * progressMult;

        for (const auto& [type, amount] : tech->cost) {
            refund[type] = static_cast<int>(amount * totalRefund);
        }
    }

    // Reset progress
    auto& progress = m_techProgress[m_currentResearch];
    progress.status = TechStatus::Available;
    progress.progressTime = 0.0f;

    m_currentResearch.clear();
    m_researchProgress = 0.0f;

    return refund;
}

void TechTree::GrantTech(const std::string& techId) {
    const TechNode* tech = GetTech(techId);
    if (!tech) return;

    // Add to researched
    m_researchedTechs.insert(techId);

    // Update progress
    ResearchProgress& progress = m_techProgress[techId];
    progress.techId = techId;
    progress.status = TechStatus::Completed;
    progress.progressTime = tech->researchTime;
    progress.totalTime = tech->researchTime;
    progress.timesResearched++;

    m_totalTechsResearched++;

    RecalculateBonuses();
}

bool TechTree::LoseTech(const std::string& techId) {
    if (!HasTech(techId)) return false;

    const TechNode* tech = GetTech(techId);
    if (!tech || !tech->canBeLost) return false;

    // Can't lose tech below minimum age
    if (static_cast<int>(tech->requiredAge) < static_cast<int>(tech->minimumAgeLoss)) {
        return false;
    }

    // Remove from researched
    m_researchedTechs.erase(techId);

    // Update progress
    auto& progress = m_techProgress[techId];
    progress.status = TechStatus::Lost;
    progress.timesLost++;

    m_totalTechsLost++;

    RecalculateBonuses();

    if (m_onTechLost) {
        m_onTechLost(techId, *tech);
    }

    return true;
}

bool TechTree::QueueResearch(const std::string& techId) {
    // Check if already in queue
    for (const auto& queued : m_researchQueue) {
        if (queued == techId) return false;
    }

    // Check if already researched
    if (HasTech(techId)) return false;

    // Add to queue
    m_researchQueue.push_back(techId);

    // Start if nothing in progress
    if (m_currentResearch.empty()) {
        ProcessResearchQueue();
    }

    return true;
}

bool TechTree::DequeueResearch(const std::string& techId) {
    auto it = std::find(m_researchQueue.begin(), m_researchQueue.end(), techId);
    if (it != m_researchQueue.end()) {
        m_researchQueue.erase(it);
        return true;
    }
    return false;
}

void TechTree::ClearResearchQueue() {
    m_researchQueue.clear();
}

// Age system implementation
bool TechTree::CanAdvanceAge() const {
    if (m_currentAge == Age::Future) return false;  // Max age
    if (m_isAdvancingAge) return false;  // Already advancing

    const AgeRequirements& req = GetAgeRequirements(static_cast<Age>(static_cast<int>(m_currentAge) + 1));

    // Check required techs
    for (const auto& techId : req.requiredTechs) {
        if (!HasTech(techId)) return false;
    }

    // Note: Resource check would be done by caller
    return true;
}

std::optional<AgeRequirements> TechTree::GetNextAgeRequirements() const {
    if (m_currentAge == Age::Future) return std::nullopt;

    Age nextAge = static_cast<Age>(static_cast<int>(m_currentAge) + 1);
    return m_ageRequirements[static_cast<size_t>(nextAge)];
}

bool TechTree::StartAgeAdvancement() {
    if (!CanAdvanceAge()) return false;

    Age nextAge = static_cast<Age>(static_cast<int>(m_currentAge) + 1);
    const AgeRequirements& req = GetAgeRequirements(nextAge);

    m_isAdvancingAge = true;
    m_ageAdvancementProgress = 0.0f;
    m_ageAdvancementTime = req.researchTime;

    return true;
}

void TechTree::AdvanceAge() {
    if (m_currentAge == Age::Future) return;

    Age previousAge = m_currentAge;
    m_currentAge = static_cast<Age>(static_cast<int>(m_currentAge) + 1);
    m_isAdvancingAge = false;
    m_ageAdvancementProgress = 0.0f;

    // Track highest age
    if (static_cast<int>(m_currentAge) > static_cast<int>(m_highestAgeAchieved)) {
        m_highestAgeAchieved = m_currentAge;
    }

    if (m_onAgeAdvance) {
        m_onAgeAdvance(m_currentAge, previousAge);
    }
}

void TechTree::RegressToAge(Age age) {
    if (static_cast<int>(age) >= static_cast<int>(m_currentAge)) return;

    Age previousAge = m_currentAge;
    m_currentAge = age;

    // Remove techs from higher ages
    std::vector<std::string> techsToRemove;
    for (const auto& techId : m_researchedTechs) {
        const TechNode* tech = GetTech(techId);
        if (tech && static_cast<int>(tech->requiredAge) > static_cast<int>(age)) {
            if (tech->canBeLost) {
                techsToRemove.push_back(techId);
            }
        }
    }

    for (const auto& techId : techsToRemove) {
        LoseTech(techId);
    }

    if (m_onAgeAdvance) {
        m_onAgeAdvance(m_currentAge, previousAge);
    }
}

const AgeRequirements& TechTree::GetAgeRequirements(Age age) const {
    return m_ageRequirements[static_cast<size_t>(age)];
}

void TechTree::RecalculateBonuses() {
    m_statMultipliers.clear();
    m_statFlatBonuses.clear();
    m_unlockedBuildings.clear();
    m_unlockedUnits.clear();
    m_unlockedAbilities.clear();

    for (const auto& techId : m_researchedTechs) {
        const TechNode* tech = GetTech(techId);
        if (!tech) continue;

        // Process effects
        for (const auto& effect : tech->effects) {
            switch (effect.type) {
                case TechEffectType::StatMultiplier:
                    m_statMultipliers[effect.target] *= effect.value;
                    break;
                case TechEffectType::StatFlat:
                    m_statFlatBonuses[effect.target] += effect.value;
                    break;
                case TechEffectType::UnlockBuilding:
                    m_unlockedBuildings.insert(effect.target);
                    break;
                case TechEffectType::UnlockUnit:
                    m_unlockedUnits.insert(effect.target);
                    break;
                case TechEffectType::UnlockAbility:
                    m_unlockedAbilities.insert(effect.target);
                    break;
                default:
                    break;
            }
        }

        // Process explicit unlocks
        for (const auto& building : tech->unlocksBuildings) {
            m_unlockedBuildings.insert(building);
        }
        for (const auto& unit : tech->unlocksUnits) {
            m_unlockedUnits.insert(unit);
        }
        for (const auto& ability : tech->unlocksAbilities) {
            m_unlockedAbilities.insert(ability);
        }

        // Add protection bonus
        m_baseTechProtection += tech->protectionBonus;
    }
}

float TechTree::GetStatMultiplier(const std::string& statName) const {
    auto it = m_statMultipliers.find(statName);
    return (it != m_statMultipliers.end()) ? it->second : 1.0f;
}

float TechTree::GetStatFlatBonus(const std::string& statName) const {
    auto it = m_statFlatBonuses.find(statName);
    return (it != m_statFlatBonuses.end()) ? it->second : 0.0f;
}

bool TechTree::IsBuildingUnlocked(const std::string& buildingId) const {
    return m_unlockedBuildings.count(buildingId) > 0;
}

bool TechTree::IsUnitUnlocked(const std::string& unitId) const {
    return m_unlockedUnits.count(unitId) > 0;
}

bool TechTree::IsAbilityUnlocked(const std::string& abilityId) const {
    return m_unlockedAbilities.count(abilityId) > 0;
}

std::vector<std::string> TechTree::GetUnlockedBuildings() const {
    return {m_unlockedBuildings.begin(), m_unlockedBuildings.end()};
}

std::vector<std::string> TechTree::GetUnlockedUnits() const {
    return {m_unlockedUnits.begin(), m_unlockedUnits.end()};
}

std::vector<std::string> TechTree::GetUnlockedAbilities() const {
    return {m_unlockedAbilities.begin(), m_unlockedAbilities.end()};
}

float TechTree::GetTechProtectionLevel() const {
    return std::min(1.0f, m_baseTechProtection + m_temporaryProtection);
}

bool TechTree::IsTechProtected(const std::string& techId) const {
    const TechNode* tech = GetTech(techId);
    if (!tech) return false;

    // Permanent techs are always protected
    if (!tech->canBeLost) return true;

    // Key techs get bonus protection
    float effectiveProtection = GetTechProtectionLevel();
    if (tech->isKeyTech) {
        effectiveProtection += 0.3f;
    }

    // Random chance based on protection
    // For deterministic check, use protection > loss chance
    return effectiveProtection >= tech->lossChanceOnDeath;
}

void TechTree::AddTechProtection(float bonus, float duration) {
    m_temporaryProtection = std::min(1.0f, m_temporaryProtection + bonus);
    m_protectionDuration = std::max(m_protectionDuration, duration);
}

nlohmann::json TechTree::ToJson() const {
    nlohmann::json j;
    j["culture"] = static_cast<int>(m_culture);
    j["playerId"] = m_playerId;
    j["currentAge"] = static_cast<int>(m_currentAge);
    j["highestAgeAchieved"] = static_cast<int>(m_highestAgeAchieved);
    j["totalTechsResearched"] = m_totalTechsResearched;
    j["totalTechsLost"] = m_totalTechsLost;
    j["totalResearchTime"] = m_totalResearchTime;

    j["researchedTechs"] = nlohmann::json::array();
    for (const auto& techId : m_researchedTechs) {
        j["researchedTechs"].push_back(techId);
    }

    j["techProgress"] = nlohmann::json::object();
    for (const auto& [id, progress] : m_techProgress) {
        j["techProgress"][id] = progress.ToJson();
    }

    j["currentResearch"] = m_currentResearch;
    j["researchProgress"] = m_researchProgress;
    j["researchQueue"] = m_researchQueue;

    j["isAdvancingAge"] = m_isAdvancingAge;
    j["ageAdvancementProgress"] = m_ageAdvancementProgress;

    return j;
}

void TechTree::FromJson(const nlohmann::json& j) {
    m_culture = static_cast<CultureType>(j.value("culture", 0));
    m_playerId = j.value("playerId", "");
    m_currentAge = static_cast<Age>(j.value("currentAge", 0));
    m_highestAgeAchieved = static_cast<Age>(j.value("highestAgeAchieved", 0));
    m_totalTechsResearched = j.value("totalTechsResearched", 0);
    m_totalTechsLost = j.value("totalTechsLost", 0);
    m_totalResearchTime = j.value("totalResearchTime", 0.0f);

    m_researchedTechs.clear();
    if (j.contains("researchedTechs")) {
        for (const auto& techId : j["researchedTechs"]) {
            m_researchedTechs.insert(techId.get<std::string>());
        }
    }

    m_techProgress.clear();
    if (j.contains("techProgress")) {
        for (const auto& [id, progressJson] : j["techProgress"].items()) {
            m_techProgress[id] = ResearchProgress::FromJson(progressJson);
        }
    }

    m_currentResearch = j.value("currentResearch", "");
    m_researchProgress = j.value("researchProgress", 0.0f);

    m_researchQueue.clear();
    if (j.contains("researchQueue")) {
        m_researchQueue = j["researchQueue"].get<std::vector<std::string>>();
    }

    m_isAdvancingAge = j.value("isAdvancingAge", false);
    m_ageAdvancementProgress = j.value("ageAdvancementProgress", 0.0f);

    RecalculateBonuses();
}

std::string TechTree::GetFirebasePath() const {
    return "players/" + m_playerId + "/techTree";
}

void TechTree::SaveToFirebase() {
    if (m_playerId.empty()) return;

    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) return;

    firebase.SetValue(GetFirebasePath(), ToJson());
}

void TechTree::LoadFromFirebase(std::function<void(bool success)> callback) {
    if (m_playerId.empty()) {
        if (callback) callback(false);
        return;
    }

    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) {
        if (callback) callback(false);
        return;
    }

    firebase.GetValue(GetFirebasePath(), [this, callback](const nlohmann::json& data) {
        if (!data.is_null()) {
            FromJson(data);
            if (callback) callback(true);
        } else {
            if (callback) callback(false);
        }
    });
}

void TechTree::EnableFirebaseSync() {
    if (m_playerId.empty() || m_firebaseSyncEnabled) return;

    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) return;

    m_firebaseListenerId = firebase.ListenToPath(GetFirebasePath(),
        [this](const nlohmann::json& data) {
            if (!data.is_null()) {
                FromJson(data);
            }
        });

    m_firebaseSyncEnabled = true;
}

void TechTree::DisableFirebaseSync() {
    if (!m_firebaseSyncEnabled) return;

    auto& firebase = FirebaseManager::Instance();
    if (firebase.IsInitialized() && !m_firebaseListenerId.empty()) {
        firebase.StopListeningById(m_firebaseListenerId);
    }

    m_firebaseListenerId.clear();
    m_firebaseSyncEnabled = false;
}

} // namespace RTS
} // namespace Vehement
