#include "BuildingArchetype.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace Vehement {
namespace RTS {
namespace Race {

// ============================================================================
// String Conversions
// ============================================================================

const char* BuildingSubtypeToString(BuildingSubtype subtype) {
    switch (subtype) {
        case BuildingSubtype::TownCenter: return "TownCenter";
        case BuildingSubtype::Castle: return "Castle";
        case BuildingSubtype::Fortress: return "Fortress";
        case BuildingSubtype::Mine: return "Mine";
        case BuildingSubtype::LumberMill: return "LumberMill";
        case BuildingSubtype::Refinery: return "Refinery";
        case BuildingSubtype::Farm: return "Farm";
        case BuildingSubtype::Barracks: return "Barracks";
        case BuildingSubtype::ArcheryRange: return "ArcheryRange";
        case BuildingSubtype::Stable: return "Stable";
        case BuildingSubtype::Factory: return "Factory";
        case BuildingSubtype::SiegeWorkshop: return "SiegeWorkshop";
        case BuildingSubtype::Dock: return "Dock";
        case BuildingSubtype::Airfield: return "Airfield";
        case BuildingSubtype::Tower: return "Tower";
        case BuildingSubtype::Wall: return "Wall";
        case BuildingSubtype::Gate: return "Gate";
        case BuildingSubtype::Trap: return "Trap";
        case BuildingSubtype::Bunker: return "Bunker";
        case BuildingSubtype::Library: return "Library";
        case BuildingSubtype::Workshop: return "Workshop";
        case BuildingSubtype::Temple: return "Temple";
        case BuildingSubtype::Laboratory: return "Laboratory";
        case BuildingSubtype::Market: return "Market";
        case BuildingSubtype::Bank: return "Bank";
        case BuildingSubtype::Warehouse: return "Warehouse";
        case BuildingSubtype::TradePost: return "TradePost";
        case BuildingSubtype::Altar: return "Altar";
        case BuildingSubtype::Portal: return "Portal";
        case BuildingSubtype::Wonder: return "Wonder";
        case BuildingSubtype::Monument: return "Monument";
        default: return "Unknown";
    }
}

BuildingSubtype StringToBuildingSubtype(const std::string& str) {
    if (str == "TownCenter") return BuildingSubtype::TownCenter;
    if (str == "Castle") return BuildingSubtype::Castle;
    if (str == "Fortress") return BuildingSubtype::Fortress;
    if (str == "Mine") return BuildingSubtype::Mine;
    if (str == "LumberMill") return BuildingSubtype::LumberMill;
    if (str == "Refinery") return BuildingSubtype::Refinery;
    if (str == "Farm") return BuildingSubtype::Farm;
    if (str == "Barracks") return BuildingSubtype::Barracks;
    if (str == "ArcheryRange") return BuildingSubtype::ArcheryRange;
    if (str == "Stable") return BuildingSubtype::Stable;
    if (str == "Factory") return BuildingSubtype::Factory;
    if (str == "SiegeWorkshop") return BuildingSubtype::SiegeWorkshop;
    if (str == "Dock") return BuildingSubtype::Dock;
    if (str == "Airfield") return BuildingSubtype::Airfield;
    if (str == "Tower") return BuildingSubtype::Tower;
    if (str == "Wall") return BuildingSubtype::Wall;
    if (str == "Gate") return BuildingSubtype::Gate;
    if (str == "Trap") return BuildingSubtype::Trap;
    if (str == "Bunker") return BuildingSubtype::Bunker;
    if (str == "Library") return BuildingSubtype::Library;
    if (str == "Workshop") return BuildingSubtype::Workshop;
    if (str == "Temple") return BuildingSubtype::Temple;
    if (str == "Laboratory") return BuildingSubtype::Laboratory;
    if (str == "Market") return BuildingSubtype::Market;
    if (str == "Bank") return BuildingSubtype::Bank;
    if (str == "Warehouse") return BuildingSubtype::Warehouse;
    if (str == "TradePost") return BuildingSubtype::TradePost;
    if (str == "Altar") return BuildingSubtype::Altar;
    if (str == "Portal") return BuildingSubtype::Portal;
    if (str == "Wonder") return BuildingSubtype::Wonder;
    if (str == "Monument") return BuildingSubtype::Monument;
    return BuildingSubtype::Barracks;
}

// ============================================================================
// BuildingBaseStats Implementation
// ============================================================================

nlohmann::json BuildingBaseStats::ToJson() const {
    return {
        {"health", health}, {"maxHealth", maxHealth}, {"armor", armor},
        {"buildTime", buildTime}, {"sizeX", sizeX}, {"sizeY", sizeY},
        {"visionRange", visionRange}, {"garrisonCapacity", garrisonCapacity},
        {"garrisonHealRate", garrisonHealRate}, {"damage", damage},
        {"attackSpeed", attackSpeed}, {"attackRange", attackRange}
    };
}

BuildingBaseStats BuildingBaseStats::FromJson(const nlohmann::json& j) {
    BuildingBaseStats s;
    if (j.contains("health")) s.health = j["health"].get<int>();
    if (j.contains("maxHealth")) s.maxHealth = j["maxHealth"].get<int>();
    if (j.contains("armor")) s.armor = j["armor"].get<int>();
    if (j.contains("buildTime")) s.buildTime = j["buildTime"].get<float>();
    if (j.contains("sizeX")) s.sizeX = j["sizeX"].get<int>();
    if (j.contains("sizeY")) s.sizeY = j["sizeY"].get<int>();
    if (j.contains("visionRange")) s.visionRange = j["visionRange"].get<float>();
    if (j.contains("garrisonCapacity")) s.garrisonCapacity = j["garrisonCapacity"].get<int>();
    if (j.contains("garrisonHealRate")) s.garrisonHealRate = j["garrisonHealRate"].get<float>();
    if (j.contains("damage")) s.damage = j["damage"].get<int>();
    if (j.contains("attackSpeed")) s.attackSpeed = j["attackSpeed"].get<float>();
    if (j.contains("attackRange")) s.attackRange = j["attackRange"].get<float>();
    return s;
}

// ============================================================================
// BuildingCost Implementation
// ============================================================================

nlohmann::json BuildingCost::ToJson() const {
    return {{"gold", gold}, {"wood", wood}, {"stone", stone}, {"food", food}, {"metal", metal}};
}

BuildingCost BuildingCost::FromJson(const nlohmann::json& j) {
    BuildingCost c;
    if (j.contains("gold")) c.gold = j["gold"].get<int>();
    if (j.contains("wood")) c.wood = j["wood"].get<int>();
    if (j.contains("stone")) c.stone = j["stone"].get<int>();
    if (j.contains("food")) c.food = j["food"].get<int>();
    if (j.contains("metal")) c.metal = j["metal"].get<int>();
    return c;
}

// ============================================================================
// ProductionEntry Implementation
// ============================================================================

nlohmann::json ProductionEntry::ToJson() const {
    return {{"unitId", unitId}, {"queueLimit", queueLimit},
            {"requiresTech", requiresTech}, {"requiredTech", requiredTech}};
}

ProductionEntry ProductionEntry::FromJson(const nlohmann::json& j) {
    ProductionEntry e;
    if (j.contains("unitId")) e.unitId = j["unitId"].get<std::string>();
    if (j.contains("queueLimit")) e.queueLimit = j["queueLimit"].get<int>();
    if (j.contains("requiresTech")) e.requiresTech = j["requiresTech"].get<bool>();
    if (j.contains("requiredTech")) e.requiredTech = j["requiredTech"].get<std::string>();
    return e;
}

// ============================================================================
// ResourceGeneration Implementation
// ============================================================================

nlohmann::json ResourceGeneration::ToJson() const {
    return {{"resourceType", resourceType}, {"ratePerSecond", ratePerSecond},
            {"maxStorage", maxStorage}, {"requiresWorker", requiresWorker},
            {"workerEfficiency", workerEfficiency}};
}

ResourceGeneration ResourceGeneration::FromJson(const nlohmann::json& j) {
    ResourceGeneration r;
    if (j.contains("resourceType")) r.resourceType = j["resourceType"].get<std::string>();
    if (j.contains("ratePerSecond")) r.ratePerSecond = j["ratePerSecond"].get<float>();
    if (j.contains("maxStorage")) r.maxStorage = j["maxStorage"].get<float>();
    if (j.contains("requiresWorker")) r.requiresWorker = j["requiresWorker"].get<bool>();
    if (j.contains("workerEfficiency")) r.workerEfficiency = j["workerEfficiency"].get<float>();
    return r;
}

// ============================================================================
// BuildingArchetype Implementation
// ============================================================================

BuildingBaseStats BuildingArchetype::CalculateStats(
    const std::map<std::string, float>& modifiers) const {
    BuildingBaseStats stats = baseStats;

    auto apply = [&modifiers](const std::string& key, int& val) {
        auto it = modifiers.find(key);
        if (it != modifiers.end()) val = static_cast<int>(val * it->second);
    };
    auto applyF = [&modifiers](const std::string& key, float& val) {
        auto it = modifiers.find(key);
        if (it != modifiers.end()) val *= it->second;
    };

    apply("buildingHealth", stats.health);
    apply("buildingArmor", stats.armor);
    applyF("buildSpeed", stats.buildTime);
    if (isDefensive) {
        apply("towerDamage", stats.damage);
        applyF("towerRange", stats.attackRange);
    }

    return stats;
}

bool BuildingArchetype::CanProduce(const std::string& unitId) const {
    return std::any_of(productions.begin(), productions.end(),
        [&unitId](const ProductionEntry& e) { return e.unitId == unitId; });
}

bool BuildingArchetype::CanResearch(const std::string& techId) const {
    return std::find(availableResearch.begin(), availableResearch.end(), techId)
        != availableResearch.end();
}

bool BuildingArchetype::Validate() const { return GetValidationErrors().empty(); }

std::vector<std::string> BuildingArchetype::GetValidationErrors() const {
    std::vector<std::string> errors;
    if (id.empty()) errors.push_back("Building archetype ID required");
    if (name.empty()) errors.push_back("Building archetype name required");
    if (baseStats.health <= 0) errors.push_back("Health must be positive");
    return errors;
}

nlohmann::json BuildingArchetype::ToJson() const {
    nlohmann::json prodJson = nlohmann::json::array();
    for (const auto& p : productions) prodJson.push_back(p.ToJson());

    nlohmann::json resGenJson = nlohmann::json::array();
    for (const auto& r : resourceGeneration) resGenJson.push_back(r.ToJson());

    return {
        {"id", id}, {"name", name}, {"description", description}, {"iconPath", iconPath},
        {"category", BuildingCategoryToString(category)},
        {"subtype", BuildingSubtypeToString(subtype)},
        {"baseStats", baseStats.ToJson()}, {"cost", cost.ToJson()},
        {"requiredBuilding", requiredBuilding}, {"requiredTech", requiredTech},
        {"requiredAge", requiredAge}, {"productions", prodJson},
        {"productionSpeedModifier", productionSpeedModifier},
        {"availableResearch", availableResearch}, {"researchSpeedModifier", researchSpeedModifier},
        {"resourceGeneration", resGenJson}, {"populationProvided", populationProvided},
        {"populationRequired", populationRequired}, {"isDefensive", isDefensive},
        {"projectileId", projectileId}, {"canAttackAir", canAttackAir},
        {"canAttackGround", canAttackGround}, {"isMainBase", isMainBase},
        {"canBeBuiltOnResource", canBeBuiltOnResource}, {"isPackable", isPackable},
        {"providesDropOff", providesDropOff}, {"providesHealing", providesHealing},
        {"upgradesTo", upgradesTo}, {"upgradesFrom", upgradesFrom},
        {"modelPath", modelPath}, {"constructionModel", constructionModel},
        {"destroyedModel", destroyedModel}, {"modelScale", modelScale},
        {"constructSound", constructSound}, {"completeSound", completeSound},
        {"selectSound", selectSound}, {"destroySound", destroySound},
        {"pointCost", pointCost}, {"powerRating", powerRating}, {"tags", tags}
    };
}

BuildingArchetype BuildingArchetype::FromJson(const nlohmann::json& j) {
    BuildingArchetype a;
    if (j.contains("id")) a.id = j["id"].get<std::string>();
    if (j.contains("name")) a.name = j["name"].get<std::string>();
    if (j.contains("description")) a.description = j["description"].get<std::string>();
    if (j.contains("iconPath")) a.iconPath = j["iconPath"].get<std::string>();

    if (j.contains("category")) {
        std::string s = j["category"].get<std::string>();
        if (s == "MainHall") a.category = BuildingCategory::MainHall;
        else if (s == "Resource") a.category = BuildingCategory::Resource;
        else if (s == "Military") a.category = BuildingCategory::Military;
        else if (s == "Defense") a.category = BuildingCategory::Defense;
        else if (s == "Research") a.category = BuildingCategory::Research;
        else if (s == "Economic") a.category = BuildingCategory::Economic;
        else if (s == "Special") a.category = BuildingCategory::Special;
    }

    if (j.contains("subtype")) a.subtype = StringToBuildingSubtype(j["subtype"].get<std::string>());
    if (j.contains("baseStats")) a.baseStats = BuildingBaseStats::FromJson(j["baseStats"]);
    if (j.contains("cost")) a.cost = BuildingCost::FromJson(j["cost"]);
    if (j.contains("requiredBuilding")) a.requiredBuilding = j["requiredBuilding"].get<std::string>();
    if (j.contains("requiredTech")) a.requiredTech = j["requiredTech"].get<std::string>();
    if (j.contains("requiredAge")) a.requiredAge = j["requiredAge"].get<int>();

    if (j.contains("productions") && j["productions"].is_array()) {
        for (const auto& p : j["productions"]) a.productions.push_back(ProductionEntry::FromJson(p));
    }

    if (j.contains("productionSpeedModifier")) a.productionSpeedModifier = j["productionSpeedModifier"].get<float>();
    if (j.contains("availableResearch")) a.availableResearch = j["availableResearch"].get<std::vector<std::string>>();
    if (j.contains("researchSpeedModifier")) a.researchSpeedModifier = j["researchSpeedModifier"].get<float>();

    if (j.contains("resourceGeneration") && j["resourceGeneration"].is_array()) {
        for (const auto& r : j["resourceGeneration"]) a.resourceGeneration.push_back(ResourceGeneration::FromJson(r));
    }

    if (j.contains("populationProvided")) a.populationProvided = j["populationProvided"].get<int>();
    if (j.contains("populationRequired")) a.populationRequired = j["populationRequired"].get<int>();
    if (j.contains("isDefensive")) a.isDefensive = j["isDefensive"].get<bool>();
    if (j.contains("projectileId")) a.projectileId = j["projectileId"].get<std::string>();
    if (j.contains("canAttackAir")) a.canAttackAir = j["canAttackAir"].get<bool>();
    if (j.contains("canAttackGround")) a.canAttackGround = j["canAttackGround"].get<bool>();
    if (j.contains("isMainBase")) a.isMainBase = j["isMainBase"].get<bool>();
    if (j.contains("canBeBuiltOnResource")) a.canBeBuiltOnResource = j["canBeBuiltOnResource"].get<bool>();
    if (j.contains("isPackable")) a.isPackable = j["isPackable"].get<bool>();
    if (j.contains("providesDropOff")) a.providesDropOff = j["providesDropOff"].get<bool>();
    if (j.contains("providesHealing")) a.providesHealing = j["providesHealing"].get<bool>();
    if (j.contains("upgradesTo")) a.upgradesTo = j["upgradesTo"].get<std::vector<std::string>>();
    if (j.contains("upgradesFrom")) a.upgradesFrom = j["upgradesFrom"].get<std::string>();
    if (j.contains("modelPath")) a.modelPath = j["modelPath"].get<std::string>();
    if (j.contains("constructionModel")) a.constructionModel = j["constructionModel"].get<std::string>();
    if (j.contains("destroyedModel")) a.destroyedModel = j["destroyedModel"].get<std::string>();
    if (j.contains("modelScale")) a.modelScale = j["modelScale"].get<float>();
    if (j.contains("constructSound")) a.constructSound = j["constructSound"].get<std::string>();
    if (j.contains("completeSound")) a.completeSound = j["completeSound"].get<std::string>();
    if (j.contains("selectSound")) a.selectSound = j["selectSound"].get<std::string>();
    if (j.contains("destroySound")) a.destroySound = j["destroySound"].get<std::string>();
    if (j.contains("pointCost")) a.pointCost = j["pointCost"].get<int>();
    if (j.contains("powerRating")) a.powerRating = j["powerRating"].get<float>();
    if (j.contains("tags")) a.tags = j["tags"].get<std::vector<std::string>>();

    return a;
}

bool BuildingArchetype::SaveToFile(const std::string& filepath) const {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;
    f << ToJson().dump(2);
    return true;
}

bool BuildingArchetype::LoadFromFile(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) return false;
    try { nlohmann::json j; f >> j; *this = FromJson(j); return true; }
    catch (...) { return false; }
}

// ============================================================================
// Registry Implementation
// ============================================================================

BuildingArchetypeRegistry& BuildingArchetypeRegistry::Instance() {
    static BuildingArchetypeRegistry instance;
    return instance;
}

bool BuildingArchetypeRegistry::Initialize() {
    if (m_initialized) return true;
    InitializeBuiltInArchetypes();
    m_initialized = true;
    return true;
}

void BuildingArchetypeRegistry::Shutdown() {
    m_archetypes.clear();
    m_initialized = false;
}

bool BuildingArchetypeRegistry::RegisterArchetype(const BuildingArchetype& a) {
    if (a.id.empty()) return false;
    m_archetypes[a.id] = a;
    return true;
}

bool BuildingArchetypeRegistry::UnregisterArchetype(const std::string& id) {
    return m_archetypes.erase(id) > 0;
}

const BuildingArchetype* BuildingArchetypeRegistry::GetArchetype(const std::string& id) const {
    auto it = m_archetypes.find(id);
    return (it != m_archetypes.end()) ? &it->second : nullptr;
}

std::vector<const BuildingArchetype*> BuildingArchetypeRegistry::GetAllArchetypes() const {
    std::vector<const BuildingArchetype*> r;
    for (const auto& [id, a] : m_archetypes) r.push_back(&a);
    return r;
}

std::vector<const BuildingArchetype*> BuildingArchetypeRegistry::GetByCategory(BuildingCategory cat) const {
    std::vector<const BuildingArchetype*> r;
    for (const auto& [id, a] : m_archetypes) if (a.category == cat) r.push_back(&a);
    return r;
}

int BuildingArchetypeRegistry::LoadFromDirectory(const std::string& dir) {
    int count = 0;
    try {
        for (const auto& e : std::filesystem::directory_iterator(dir)) {
            if (e.path().extension() == ".json") {
                BuildingArchetype a;
                if (a.LoadFromFile(e.path().string()) && RegisterArchetype(a)) ++count;
            }
        }
    } catch (...) {}
    return count;
}

void BuildingArchetypeRegistry::InitializeBuiltInArchetypes() {
    RegisterArchetype(CreateMainHallArchetype());
    RegisterArchetype(CreateBarracksArchetype());
    RegisterArchetype(CreateArcheryRangeArchetype());
    RegisterArchetype(CreateStableArchetype());
    RegisterArchetype(CreateTowerArchetype());
    RegisterArchetype(CreateWallArchetype());
    RegisterArchetype(CreateMarketArchetype());
    RegisterArchetype(CreateFarmArchetype());
}

// ============================================================================
// Built-in Archetypes
// ============================================================================

BuildingArchetype CreateMainHallArchetype() {
    BuildingArchetype a;
    a.id = "main_hall";
    a.name = "Town Center";
    a.description = "Main base building. Produces workers and advances ages.";
    a.category = BuildingCategory::MainHall;
    a.subtype = BuildingSubtype::TownCenter;

    a.baseStats.health = 2000;
    a.baseStats.maxHealth = 2000;
    a.baseStats.armor = 5;
    a.baseStats.buildTime = 120.0f;
    a.baseStats.sizeX = 4;
    a.baseStats.sizeY = 4;
    a.baseStats.visionRange = 12.0f;
    a.baseStats.garrisonCapacity = 10;

    a.cost.wood = 200;
    a.cost.stone = 100;

    a.productions = {{"worker", 10, false, ""}};
    a.isMainBase = true;
    a.providesDropOff = true;
    a.populationProvided = 5;
    a.pointCost = 0; // Required
    a.powerRating = 1.0f;

    return a;
}

BuildingArchetype CreateCastleArchetype() {
    BuildingArchetype a = CreateMainHallArchetype();
    a.id = "castle";
    a.name = "Castle";
    a.description = "Fortified main base with defensive capabilities.";
    a.subtype = BuildingSubtype::Castle;

    a.baseStats.health = 4000;
    a.baseStats.armor = 10;
    a.baseStats.damage = 20;
    a.baseStats.attackSpeed = 1.0f;
    a.baseStats.attackRange = 8.0f;
    a.baseStats.buildTime = 180.0f;

    a.cost.gold = 500;
    a.cost.wood = 300;
    a.cost.stone = 400;

    a.isDefensive = true;
    a.requiredAge = 3;
    a.pointCost = 15;

    return a;
}

BuildingArchetype CreateBarracksArchetype() {
    BuildingArchetype a;
    a.id = "barracks";
    a.name = "Barracks";
    a.description = "Trains infantry units.";
    a.category = BuildingCategory::Military;
    a.subtype = BuildingSubtype::Barracks;

    a.baseStats.health = 800;
    a.baseStats.armor = 3;
    a.baseStats.buildTime = 45.0f;
    a.baseStats.sizeX = 3;
    a.baseStats.sizeY = 3;

    a.cost.gold = 100;
    a.cost.wood = 150;

    a.productions = {
        {"infantry_melee", 5, false, ""},
        {"infantry_pike", 5, false, ""},
        {"infantry_shield", 5, true, "tech_iron_armor"}
    };

    a.requiredBuilding = "main_hall";
    a.pointCost = 5;
    a.powerRating = 1.2f;

    return a;
}

BuildingArchetype CreateArcheryRangeArchetype() {
    BuildingArchetype a;
    a.id = "archery_range";
    a.name = "Archery Range";
    a.description = "Trains ranged units.";
    a.category = BuildingCategory::Military;
    a.subtype = BuildingSubtype::ArcheryRange;

    a.baseStats.health = 700;
    a.baseStats.armor = 2;
    a.baseStats.buildTime = 40.0f;
    a.baseStats.sizeX = 3;
    a.baseStats.sizeY = 3;

    a.cost.gold = 120;
    a.cost.wood = 175;

    a.productions = {{"ranged_archer", 5, false, ""}};

    a.requiredBuilding = "main_hall";
    a.pointCost = 5;

    return a;
}

BuildingArchetype CreateStableArchetype() {
    BuildingArchetype a;
    a.id = "stable";
    a.name = "Stable";
    a.description = "Trains cavalry units.";
    a.category = BuildingCategory::Military;
    a.subtype = BuildingSubtype::Stable;

    a.baseStats.health = 750;
    a.baseStats.armor = 3;
    a.baseStats.buildTime = 50.0f;
    a.baseStats.sizeX = 3;
    a.baseStats.sizeY = 3;

    a.cost.gold = 150;
    a.cost.wood = 200;

    a.productions = {
        {"cavalry_light", 5, false, ""},
        {"cavalry_heavy", 3, true, "tech_heavy_cavalry"}
    };

    a.requiredBuilding = "barracks";
    a.requiredAge = 1;
    a.pointCost = 6;

    return a;
}

BuildingArchetype CreateSiegeWorkshopArchetype() {
    BuildingArchetype a;
    a.id = "siege_workshop";
    a.name = "Siege Workshop";
    a.description = "Produces siege weapons.";
    a.category = BuildingCategory::Military;
    a.subtype = BuildingSubtype::SiegeWorkshop;

    a.baseStats.health = 900;
    a.baseStats.armor = 4;
    a.baseStats.buildTime = 60.0f;
    a.baseStats.sizeX = 4;
    a.baseStats.sizeY = 3;

    a.cost.gold = 200;
    a.cost.wood = 250;

    a.productions = {
        {"siege_ram", 2, false, ""},
        {"siege_catapult", 2, true, "tech_siege_weapons"}
    };

    a.requiredBuilding = "barracks";
    a.requiredAge = 2;
    a.pointCost = 8;

    return a;
}

BuildingArchetype CreateDockArchetype() {
    BuildingArchetype a;
    a.id = "dock";
    a.name = "Dock";
    a.description = "Produces naval units.";
    a.category = BuildingCategory::Military;
    a.subtype = BuildingSubtype::Dock;

    a.baseStats.health = 1000;
    a.baseStats.armor = 2;
    a.baseStats.buildTime = 55.0f;
    a.baseStats.sizeX = 3;
    a.baseStats.sizeY = 4;

    a.cost.gold = 150;
    a.cost.wood = 300;

    a.productions = {
        {"naval_transport", 3, false, ""},
        {"naval_warship", 2, true, "tech_shipbuilding"}
    };

    a.canBeBuiltOnResource = true;  // Built on water
    a.pointCost = 7;

    return a;
}

BuildingArchetype CreateTowerArchetype() {
    BuildingArchetype a;
    a.id = "tower";
    a.name = "Guard Tower";
    a.description = "Defensive structure that attacks enemies.";
    a.category = BuildingCategory::Defense;
    a.subtype = BuildingSubtype::Tower;

    a.baseStats.health = 600;
    a.baseStats.armor = 5;
    a.baseStats.buildTime = 35.0f;
    a.baseStats.sizeX = 2;
    a.baseStats.sizeY = 2;
    a.baseStats.damage = 15;
    a.baseStats.attackSpeed = 1.0f;
    a.baseStats.attackRange = 8.0f;
    a.baseStats.garrisonCapacity = 5;

    a.cost.gold = 75;
    a.cost.stone = 100;

    a.isDefensive = true;
    a.canAttackAir = true;
    a.projectileId = "arrow_basic";
    a.pointCost = 4;

    return a;
}

BuildingArchetype CreateWallArchetype() {
    BuildingArchetype a;
    a.id = "wall";
    a.name = "Stone Wall";
    a.description = "Defensive wall segment.";
    a.category = BuildingCategory::Defense;
    a.subtype = BuildingSubtype::Wall;

    a.baseStats.health = 400;
    a.baseStats.armor = 8;
    a.baseStats.buildTime = 10.0f;
    a.baseStats.sizeX = 1;
    a.baseStats.sizeY = 1;

    a.cost.stone = 25;

    a.isDefensive = true;
    a.pointCost = 1;

    return a;
}

BuildingArchetype CreateGateArchetype() {
    BuildingArchetype a = CreateWallArchetype();
    a.id = "gate";
    a.name = "Gate";
    a.description = "Defensive gate in wall.";
    a.subtype = BuildingSubtype::Gate;

    a.baseStats.health = 600;
    a.baseStats.sizeX = 2;
    a.baseStats.sizeY = 1;
    a.baseStats.buildTime = 20.0f;

    a.cost.stone = 50;
    a.cost.wood = 25;

    a.pointCost = 2;

    return a;
}

BuildingArchetype CreateMineArchetype() {
    BuildingArchetype a;
    a.id = "mine";
    a.name = "Mine";
    a.description = "Extracts stone and metal from deposits.";
    a.category = BuildingCategory::Resource;
    a.subtype = BuildingSubtype::Mine;

    a.baseStats.health = 500;
    a.baseStats.armor = 2;
    a.baseStats.buildTime = 40.0f;
    a.baseStats.sizeX = 3;
    a.baseStats.sizeY = 3;

    a.cost.gold = 75;
    a.cost.wood = 100;

    a.resourceGeneration = {
        {"stone", 0.5f, 500.0f, true, 1.0f},
        {"metal", 0.2f, 200.0f, true, 1.0f}
    };

    a.canBeBuiltOnResource = true;
    a.providesDropOff = true;
    a.pointCost = 4;

    return a;
}

BuildingArchetype CreateLumberMillArchetype() {
    BuildingArchetype a;
    a.id = "lumber_mill";
    a.name = "Lumber Mill";
    a.description = "Processes wood faster.";
    a.category = BuildingCategory::Resource;
    a.subtype = BuildingSubtype::LumberMill;

    a.baseStats.health = 500;
    a.baseStats.armor = 2;
    a.baseStats.buildTime = 35.0f;
    a.baseStats.sizeX = 3;
    a.baseStats.sizeY = 3;

    a.cost.gold = 75;
    a.cost.wood = 150;

    a.resourceGeneration = {{"wood", 0.0f, 500.0f, true, 1.2f}};
    a.providesDropOff = true;
    a.pointCost = 4;

    return a;
}

BuildingArchetype CreateFarmArchetype() {
    BuildingArchetype a;
    a.id = "farm";
    a.name = "Farm";
    a.description = "Produces food over time.";
    a.category = BuildingCategory::Resource;
    a.subtype = BuildingSubtype::Farm;

    a.baseStats.health = 300;
    a.baseStats.armor = 0;
    a.baseStats.buildTime = 25.0f;
    a.baseStats.sizeX = 2;
    a.baseStats.sizeY = 2;

    a.cost.gold = 50;
    a.cost.wood = 75;

    a.resourceGeneration = {{"food", 0.3f, 300.0f, false, 1.0f}};
    a.pointCost = 3;

    return a;
}

BuildingArchetype CreateLibraryArchetype() {
    BuildingArchetype a;
    a.id = "library";
    a.name = "Library";
    a.description = "Researches technologies.";
    a.category = BuildingCategory::Research;
    a.subtype = BuildingSubtype::Library;

    a.baseStats.health = 600;
    a.baseStats.armor = 2;
    a.baseStats.buildTime = 50.0f;
    a.baseStats.sizeX = 3;
    a.baseStats.sizeY = 3;

    a.cost.gold = 150;
    a.cost.wood = 100;
    a.cost.stone = 50;

    a.availableResearch = {
        "tech_bronze_working", "tech_iron_working", "tech_advanced_farming"
    };
    a.researchSpeedModifier = 1.0f;
    a.pointCost = 6;

    return a;
}

BuildingArchetype CreateBlacksmithArchetype() {
    BuildingArchetype a;
    a.id = "blacksmith";
    a.name = "Blacksmith";
    a.description = "Researches military upgrades.";
    a.category = BuildingCategory::Research;
    a.subtype = BuildingSubtype::Workshop;

    a.baseStats.health = 600;
    a.baseStats.armor = 3;
    a.baseStats.buildTime = 45.0f;
    a.baseStats.sizeX = 3;
    a.baseStats.sizeY = 3;

    a.cost.gold = 125;
    a.cost.wood = 75;
    a.cost.metal = 50;

    a.availableResearch = {
        "tech_bronze_weapons", "tech_iron_weapons", "tech_iron_armor"
    };
    a.pointCost = 5;

    return a;
}

BuildingArchetype CreateTempleArchetype() {
    BuildingArchetype a;
    a.id = "temple";
    a.name = "Temple";
    a.description = "Trains support units and provides healing.";
    a.category = BuildingCategory::Research;
    a.subtype = BuildingSubtype::Temple;

    a.baseStats.health = 700;
    a.baseStats.armor = 2;
    a.baseStats.buildTime = 55.0f;
    a.baseStats.sizeX = 3;
    a.baseStats.sizeY = 3;
    a.baseStats.garrisonHealRate = 5.0f;

    a.cost.gold = 200;
    a.cost.stone = 150;

    a.productions = {{"special_healer", 3, false, ""}};
    a.providesHealing = true;
    a.requiredAge = 2;
    a.pointCost = 7;

    return a;
}

BuildingArchetype CreateMarketArchetype() {
    BuildingArchetype a;
    a.id = "market";
    a.name = "Market";
    a.description = "Enables trading and generates gold.";
    a.category = BuildingCategory::Economic;
    a.subtype = BuildingSubtype::Market;

    a.baseStats.health = 600;
    a.baseStats.armor = 2;
    a.baseStats.buildTime = 45.0f;
    a.baseStats.sizeX = 3;
    a.baseStats.sizeY = 3;

    a.cost.gold = 100;
    a.cost.wood = 150;

    a.resourceGeneration = {{"gold", 0.2f, 500.0f, false, 1.0f}};
    a.pointCost = 5;

    return a;
}

BuildingArchetype CreateWarehouseArchetype() {
    BuildingArchetype a;
    a.id = "warehouse";
    a.name = "Warehouse";
    a.description = "Increases resource storage capacity.";
    a.category = BuildingCategory::Economic;
    a.subtype = BuildingSubtype::Warehouse;

    a.baseStats.health = 800;
    a.baseStats.armor = 3;
    a.baseStats.buildTime = 40.0f;
    a.baseStats.sizeX = 3;
    a.baseStats.sizeY = 3;

    a.cost.gold = 75;
    a.cost.wood = 200;

    a.resourceGeneration = {
        {"gold", 0.0f, 1000.0f, false, 1.0f},
        {"wood", 0.0f, 1000.0f, false, 1.0f},
        {"stone", 0.0f, 1000.0f, false, 1.0f},
        {"food", 0.0f, 1000.0f, false, 1.0f}
    };

    a.providesDropOff = true;
    a.pointCost = 4;

    return a;
}

BuildingArchetype CreateWonderArchetype() {
    BuildingArchetype a;
    a.id = "wonder";
    a.name = "Wonder";
    a.description = "Victory condition building. Grants powerful bonuses.";
    a.category = BuildingCategory::Special;
    a.subtype = BuildingSubtype::Wonder;

    a.baseStats.health = 5000;
    a.baseStats.armor = 10;
    a.baseStats.buildTime = 600.0f;
    a.baseStats.sizeX = 5;
    a.baseStats.sizeY = 5;
    a.baseStats.visionRange = 20.0f;

    a.cost.gold = 1000;
    a.cost.wood = 500;
    a.cost.stone = 500;
    a.cost.metal = 250;

    a.requiredAge = 6;  // Future Age
    a.pointCost = 20;
    a.powerRating = 5.0f;

    return a;
}

BuildingArchetype CreateAltarArchetype() {
    BuildingArchetype a;
    a.id = "altar";
    a.name = "Altar";
    a.description = "Special building for hero revival and buffs.";
    a.category = BuildingCategory::Special;
    a.subtype = BuildingSubtype::Altar;

    a.baseStats.health = 800;
    a.baseStats.armor = 5;
    a.baseStats.buildTime = 80.0f;
    a.baseStats.sizeX = 3;
    a.baseStats.sizeY = 3;

    a.cost.gold = 250;
    a.cost.stone = 200;

    a.providesHealing = true;
    a.requiredAge = 3;
    a.pointCost = 8;

    return a;
}

} // namespace Race
} // namespace RTS
} // namespace Vehement
