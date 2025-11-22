#include "UnitArchetype.hpp"
#include <fstream>
#include <filesystem>

namespace Vehement {
namespace RTS {
namespace Race {

// ============================================================================
// UnitSubtype String Conversion
// ============================================================================

const char* UnitSubtypeToString(UnitSubtype subtype) {
    switch (subtype) {
        case UnitSubtype::Harvester: return "Harvester";
        case UnitSubtype::Builder: return "Builder";
        case UnitSubtype::Scout: return "Scout";
        case UnitSubtype::Melee: return "Melee";
        case UnitSubtype::Pike: return "Pike";
        case UnitSubtype::Shield: return "Shield";
        case UnitSubtype::Berserker: return "Berserker";
        case UnitSubtype::Archer: return "Archer";
        case UnitSubtype::Gunner: return "Gunner";
        case UnitSubtype::Caster: return "Caster";
        case UnitSubtype::Thrower: return "Thrower";
        case UnitSubtype::Light: return "Light";
        case UnitSubtype::Heavy: return "Heavy";
        case UnitSubtype::Chariot: return "Chariot";
        case UnitSubtype::BeastRider: return "BeastRider";
        case UnitSubtype::Catapult: return "Catapult";
        case UnitSubtype::Ram: return "Ram";
        case UnitSubtype::Tower: return "Tower";
        case UnitSubtype::Cannon: return "Cannon";
        case UnitSubtype::Transport: return "Transport";
        case UnitSubtype::Warship: return "Warship";
        case UnitSubtype::Submarine: return "Submarine";
        case UnitSubtype::Carrier: return "Carrier";
        case UnitSubtype::AirScout: return "AirScout";
        case UnitSubtype::Fighter: return "Fighter";
        case UnitSubtype::Bomber: return "Bomber";
        case UnitSubtype::Transport_Air: return "Transport_Air";
        case UnitSubtype::Assassin: return "Assassin";
        case UnitSubtype::Healer: return "Healer";
        case UnitSubtype::Summoner: return "Summoner";
        case UnitSubtype::Commander: return "Commander";
        default: return "Unknown";
    }
}

UnitSubtype StringToUnitSubtype(const std::string& str) {
    if (str == "Harvester") return UnitSubtype::Harvester;
    if (str == "Builder") return UnitSubtype::Builder;
    if (str == "Scout") return UnitSubtype::Scout;
    if (str == "Melee") return UnitSubtype::Melee;
    if (str == "Pike") return UnitSubtype::Pike;
    if (str == "Shield") return UnitSubtype::Shield;
    if (str == "Berserker") return UnitSubtype::Berserker;
    if (str == "Archer") return UnitSubtype::Archer;
    if (str == "Gunner") return UnitSubtype::Gunner;
    if (str == "Caster") return UnitSubtype::Caster;
    if (str == "Thrower") return UnitSubtype::Thrower;
    if (str == "Light") return UnitSubtype::Light;
    if (str == "Heavy") return UnitSubtype::Heavy;
    if (str == "Chariot") return UnitSubtype::Chariot;
    if (str == "BeastRider") return UnitSubtype::BeastRider;
    if (str == "Catapult") return UnitSubtype::Catapult;
    if (str == "Ram") return UnitSubtype::Ram;
    if (str == "Tower") return UnitSubtype::Tower;
    if (str == "Cannon") return UnitSubtype::Cannon;
    if (str == "Transport") return UnitSubtype::Transport;
    if (str == "Warship") return UnitSubtype::Warship;
    if (str == "Submarine") return UnitSubtype::Submarine;
    if (str == "Carrier") return UnitSubtype::Carrier;
    if (str == "AirScout") return UnitSubtype::AirScout;
    if (str == "Fighter") return UnitSubtype::Fighter;
    if (str == "Bomber") return UnitSubtype::Bomber;
    if (str == "Transport_Air") return UnitSubtype::Transport_Air;
    if (str == "Assassin") return UnitSubtype::Assassin;
    if (str == "Healer") return UnitSubtype::Healer;
    if (str == "Summoner") return UnitSubtype::Summoner;
    if (str == "Commander") return UnitSubtype::Commander;
    return UnitSubtype::Melee;
}

// ============================================================================
// UnitBaseStats Implementation
// ============================================================================

nlohmann::json UnitBaseStats::ToJson() const {
    return {
        {"health", health},
        {"maxHealth", maxHealth},
        {"armor", armor},
        {"magicResist", magicResist},
        {"damage", damage},
        {"attackSpeed", attackSpeed},
        {"attackRange", attackRange},
        {"moveSpeed", moveSpeed},
        {"turnSpeed", turnSpeed},
        {"visionRange", visionRange},
        {"detectionRange", detectionRange},
        {"carryCapacity", carryCapacity},
        {"gatherSpeed", gatherSpeed},
        {"buildSpeed", buildSpeed},
        {"populationCost", populationCost}
    };
}

UnitBaseStats UnitBaseStats::FromJson(const nlohmann::json& j) {
    UnitBaseStats stats;
    if (j.contains("health")) stats.health = j["health"].get<int>();
    if (j.contains("maxHealth")) stats.maxHealth = j["maxHealth"].get<int>();
    if (j.contains("armor")) stats.armor = j["armor"].get<int>();
    if (j.contains("magicResist")) stats.magicResist = j["magicResist"].get<int>();
    if (j.contains("damage")) stats.damage = j["damage"].get<int>();
    if (j.contains("attackSpeed")) stats.attackSpeed = j["attackSpeed"].get<float>();
    if (j.contains("attackRange")) stats.attackRange = j["attackRange"].get<float>();
    if (j.contains("moveSpeed")) stats.moveSpeed = j["moveSpeed"].get<float>();
    if (j.contains("turnSpeed")) stats.turnSpeed = j["turnSpeed"].get<float>();
    if (j.contains("visionRange")) stats.visionRange = j["visionRange"].get<float>();
    if (j.contains("detectionRange")) stats.detectionRange = j["detectionRange"].get<float>();
    if (j.contains("carryCapacity")) stats.carryCapacity = j["carryCapacity"].get<int>();
    if (j.contains("gatherSpeed")) stats.gatherSpeed = j["gatherSpeed"].get<float>();
    if (j.contains("buildSpeed")) stats.buildSpeed = j["buildSpeed"].get<float>();
    if (j.contains("populationCost")) stats.populationCost = j["populationCost"].get<int>();
    return stats;
}

// ============================================================================
// UnitCost Implementation
// ============================================================================

nlohmann::json UnitCost::ToJson() const {
    return {
        {"gold", gold},
        {"wood", wood},
        {"stone", stone},
        {"food", food},
        {"metal", metal},
        {"buildTime", buildTime}
    };
}

UnitCost UnitCost::FromJson(const nlohmann::json& j) {
    UnitCost cost;
    if (j.contains("gold")) cost.gold = j["gold"].get<int>();
    if (j.contains("wood")) cost.wood = j["wood"].get<int>();
    if (j.contains("stone")) cost.stone = j["stone"].get<int>();
    if (j.contains("food")) cost.food = j["food"].get<int>();
    if (j.contains("metal")) cost.metal = j["metal"].get<int>();
    if (j.contains("buildTime")) cost.buildTime = j["buildTime"].get<float>();
    return cost;
}

// ============================================================================
// UnitAbilityRef Implementation
// ============================================================================

nlohmann::json UnitAbilityRef::ToJson() const {
    return {
        {"abilityId", abilityId},
        {"unlockLevel", unlockLevel},
        {"isPassive", isPassive}
    };
}

UnitAbilityRef UnitAbilityRef::FromJson(const nlohmann::json& j) {
    UnitAbilityRef ref;
    if (j.contains("abilityId")) ref.abilityId = j["abilityId"].get<std::string>();
    if (j.contains("unlockLevel")) ref.unlockLevel = j["unlockLevel"].get<int>();
    if (j.contains("isPassive")) ref.isPassive = j["isPassive"].get<bool>();
    return ref;
}

// ============================================================================
// UnitArchetype Implementation
// ============================================================================

UnitBaseStats UnitArchetype::CalculateStats(
    const std::map<std::string, float>& modifiers) const {

    UnitBaseStats stats = baseStats;

    auto applyModifier = [&modifiers](const std::string& key, float& value) {
        auto it = modifiers.find(key);
        if (it != modifiers.end()) {
            value *= it->second;
        }
    };

    auto applyModifierInt = [&modifiers](const std::string& key, int& value) {
        auto it = modifiers.find(key);
        if (it != modifiers.end()) {
            value = static_cast<int>(value * it->second);
        }
    };

    // Apply modifiers based on category
    applyModifierInt("health", stats.health);
    applyModifierInt("maxHealth", stats.maxHealth);
    applyModifierInt("armor", stats.armor);
    applyModifierInt("damage", stats.damage);
    applyModifier("attackSpeed", stats.attackSpeed);
    applyModifier("moveSpeed", stats.moveSpeed);
    applyModifier("visionRange", stats.visionRange);

    // Category-specific modifiers
    switch (category) {
        case UnitCategory::Worker:
            applyModifierInt("carryCapacity", stats.carryCapacity);
            applyModifier("gatherSpeed", stats.gatherSpeed);
            applyModifier("buildSpeed", stats.buildSpeed);
            break;
        case UnitCategory::Infantry:
            applyModifierInt("damage", stats.damage); // Double-apply melee damage
            applyModifierInt("armor", stats.armor);
            break;
        case UnitCategory::Ranged:
            applyModifier("attackRange", stats.attackRange);
            break;
        case UnitCategory::Cavalry:
            applyModifier("moveSpeed", stats.moveSpeed); // Double-apply speed
            break;
        case UnitCategory::Siege:
            applyModifierInt("damage", stats.damage);
            applyModifier("attackRange", stats.attackRange);
            break;
        default:
            break;
    }

    return stats;
}

bool UnitArchetype::Validate() const {
    return GetValidationErrors().empty();
}

std::vector<std::string> UnitArchetype::GetValidationErrors() const {
    std::vector<std::string> errors;

    if (id.empty()) errors.push_back("Unit archetype ID is required");
    if (name.empty()) errors.push_back("Unit archetype name is required");
    if (baseStats.health <= 0) errors.push_back("Health must be positive");
    if (baseStats.damage < 0) errors.push_back("Damage cannot be negative");
    if (cost.buildTime <= 0) errors.push_back("Build time must be positive");

    return errors;
}

nlohmann::json UnitArchetype::ToJson() const {
    nlohmann::json abilitiesJson = nlohmann::json::array();
    for (const auto& ability : abilities) {
        abilitiesJson.push_back(ability.ToJson());
    }

    return {
        {"id", id},
        {"name", name},
        {"description", description},
        {"iconPath", iconPath},
        {"category", UnitCategoryToString(category)},
        {"subtype", UnitSubtypeToString(subtype)},
        {"baseStats", baseStats.ToJson()},
        {"cost", cost.ToJson()},
        {"requiredBuilding", requiredBuilding},
        {"requiredTech", requiredTech},
        {"requiredAge", requiredAge},
        {"abilities", abilitiesJson},
        {"passiveEffects", passiveEffects},
        {"attackType", attackType},
        {"damageType", damageType},
        {"projectileId", projectileId},
        {"canAttackAir", canAttackAir},
        {"canAttackGround", canAttackGround},
        {"movementType", movementType},
        {"canClimb", canClimb},
        {"canBurrow", canBurrow},
        {"isHero", isHero},
        {"isBuilding", isBuilding},
        {"isSummoned", isSummoned},
        {"isDetector", isDetector},
        {"isStealthed", isStealthed},
        {"canGather", canGather},
        {"canBuild", canBuild},
        {"canRepair", canRepair},
        {"canHeal", canHeal},
        {"upgradesTo", upgradesTo},
        {"upgradesFrom", upgradesFrom},
        {"modelPath", modelPath},
        {"animationSet", animationSet},
        {"modelScale", modelScale},
        {"selectSound", selectSound},
        {"moveSound", moveSound},
        {"attackSound", attackSound},
        {"deathSound", deathSound},
        {"pointCost", pointCost},
        {"powerRating", powerRating},
        {"tags", tags}
    };
}

UnitArchetype UnitArchetype::FromJson(const nlohmann::json& j) {
    UnitArchetype arch;

    if (j.contains("id")) arch.id = j["id"].get<std::string>();
    if (j.contains("name")) arch.name = j["name"].get<std::string>();
    if (j.contains("description")) arch.description = j["description"].get<std::string>();
    if (j.contains("iconPath")) arch.iconPath = j["iconPath"].get<std::string>();

    if (j.contains("category")) {
        std::string catStr = j["category"].get<std::string>();
        if (catStr == "Worker") arch.category = UnitCategory::Worker;
        else if (catStr == "Infantry") arch.category = UnitCategory::Infantry;
        else if (catStr == "Ranged") arch.category = UnitCategory::Ranged;
        else if (catStr == "Cavalry") arch.category = UnitCategory::Cavalry;
        else if (catStr == "Siege") arch.category = UnitCategory::Siege;
        else if (catStr == "Naval") arch.category = UnitCategory::Naval;
        else if (catStr == "Air") arch.category = UnitCategory::Air;
        else if (catStr == "Special") arch.category = UnitCategory::Special;
    }

    if (j.contains("subtype")) arch.subtype = StringToUnitSubtype(j["subtype"].get<std::string>());
    if (j.contains("baseStats")) arch.baseStats = UnitBaseStats::FromJson(j["baseStats"]);
    if (j.contains("cost")) arch.cost = UnitCost::FromJson(j["cost"]);
    if (j.contains("requiredBuilding")) arch.requiredBuilding = j["requiredBuilding"].get<std::string>();
    if (j.contains("requiredTech")) arch.requiredTech = j["requiredTech"].get<std::string>();
    if (j.contains("requiredAge")) arch.requiredAge = j["requiredAge"].get<int>();

    if (j.contains("abilities") && j["abilities"].is_array()) {
        for (const auto& a : j["abilities"]) {
            arch.abilities.push_back(UnitAbilityRef::FromJson(a));
        }
    }

    if (j.contains("passiveEffects")) arch.passiveEffects = j["passiveEffects"].get<std::vector<std::string>>();
    if (j.contains("attackType")) arch.attackType = j["attackType"].get<std::string>();
    if (j.contains("damageType")) arch.damageType = j["damageType"].get<std::string>();
    if (j.contains("projectileId")) arch.projectileId = j["projectileId"].get<std::string>();
    if (j.contains("canAttackAir")) arch.canAttackAir = j["canAttackAir"].get<bool>();
    if (j.contains("canAttackGround")) arch.canAttackGround = j["canAttackGround"].get<bool>();
    if (j.contains("movementType")) arch.movementType = j["movementType"].get<std::string>();
    if (j.contains("canClimb")) arch.canClimb = j["canClimb"].get<bool>();
    if (j.contains("canBurrow")) arch.canBurrow = j["canBurrow"].get<bool>();
    if (j.contains("isHero")) arch.isHero = j["isHero"].get<bool>();
    if (j.contains("isBuilding")) arch.isBuilding = j["isBuilding"].get<bool>();
    if (j.contains("isSummoned")) arch.isSummoned = j["isSummoned"].get<bool>();
    if (j.contains("isDetector")) arch.isDetector = j["isDetector"].get<bool>();
    if (j.contains("isStealthed")) arch.isStealthed = j["isStealthed"].get<bool>();
    if (j.contains("canGather")) arch.canGather = j["canGather"].get<bool>();
    if (j.contains("canBuild")) arch.canBuild = j["canBuild"].get<bool>();
    if (j.contains("canRepair")) arch.canRepair = j["canRepair"].get<bool>();
    if (j.contains("canHeal")) arch.canHeal = j["canHeal"].get<bool>();
    if (j.contains("upgradesTo")) arch.upgradesTo = j["upgradesTo"].get<std::vector<std::string>>();
    if (j.contains("upgradesFrom")) arch.upgradesFrom = j["upgradesFrom"].get<std::string>();
    if (j.contains("modelPath")) arch.modelPath = j["modelPath"].get<std::string>();
    if (j.contains("animationSet")) arch.animationSet = j["animationSet"].get<std::string>();
    if (j.contains("modelScale")) arch.modelScale = j["modelScale"].get<float>();
    if (j.contains("selectSound")) arch.selectSound = j["selectSound"].get<std::string>();
    if (j.contains("moveSound")) arch.moveSound = j["moveSound"].get<std::string>();
    if (j.contains("attackSound")) arch.attackSound = j["attackSound"].get<std::string>();
    if (j.contains("deathSound")) arch.deathSound = j["deathSound"].get<std::string>();
    if (j.contains("pointCost")) arch.pointCost = j["pointCost"].get<int>();
    if (j.contains("powerRating")) arch.powerRating = j["powerRating"].get<float>();
    if (j.contains("tags")) arch.tags = j["tags"].get<std::vector<std::string>>();

    return arch;
}

bool UnitArchetype::SaveToFile(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    file << ToJson().dump(2);
    return true;
}

bool UnitArchetype::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;
    try {
        nlohmann::json j;
        file >> j;
        *this = FromJson(j);
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// UnitArchetypeRegistry Implementation
// ============================================================================

UnitArchetypeRegistry& UnitArchetypeRegistry::Instance() {
    static UnitArchetypeRegistry instance;
    return instance;
}

bool UnitArchetypeRegistry::Initialize() {
    if (m_initialized) return true;
    InitializeBuiltInArchetypes();
    m_initialized = true;
    return true;
}

void UnitArchetypeRegistry::Shutdown() {
    m_archetypes.clear();
    m_initialized = false;
}

bool UnitArchetypeRegistry::RegisterArchetype(const UnitArchetype& archetype) {
    if (archetype.id.empty()) return false;
    m_archetypes[archetype.id] = archetype;
    return true;
}

bool UnitArchetypeRegistry::UnregisterArchetype(const std::string& id) {
    return m_archetypes.erase(id) > 0;
}

const UnitArchetype* UnitArchetypeRegistry::GetArchetype(const std::string& id) const {
    auto it = m_archetypes.find(id);
    return (it != m_archetypes.end()) ? &it->second : nullptr;
}

std::vector<const UnitArchetype*> UnitArchetypeRegistry::GetAllArchetypes() const {
    std::vector<const UnitArchetype*> result;
    for (const auto& [id, arch] : m_archetypes) {
        result.push_back(&arch);
    }
    return result;
}

std::vector<const UnitArchetype*> UnitArchetypeRegistry::GetByCategory(UnitCategory category) const {
    std::vector<const UnitArchetype*> result;
    for (const auto& [id, arch] : m_archetypes) {
        if (arch.category == category) {
            result.push_back(&arch);
        }
    }
    return result;
}

std::vector<const UnitArchetype*> UnitArchetypeRegistry::GetBySubtype(UnitSubtype subtype) const {
    std::vector<const UnitArchetype*> result;
    for (const auto& [id, arch] : m_archetypes) {
        if (arch.subtype == subtype) {
            result.push_back(&arch);
        }
    }
    return result;
}

int UnitArchetypeRegistry::LoadFromDirectory(const std::string& directory) {
    int count = 0;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.path().extension() == ".json") {
                UnitArchetype arch;
                if (arch.LoadFromFile(entry.path().string())) {
                    if (RegisterArchetype(arch)) {
                        ++count;
                    }
                }
            }
        }
    } catch (...) {}
    return count;
}

void UnitArchetypeRegistry::InitializeBuiltInArchetypes() {
    RegisterArchetype(CreateWorkerArchetype());
    RegisterArchetype(CreateInfantryMeleeArchetype());
    RegisterArchetype(CreateInfantryPikeArchetype());
    RegisterArchetype(CreateInfantryShieldArchetype());
    RegisterArchetype(CreateRangedArcherArchetype());
    RegisterArchetype(CreateCavalryLightArchetype());
    RegisterArchetype(CreateCavalryHeavyArchetype());
    RegisterArchetype(CreateSiegeCatapultArchetype());
    RegisterArchetype(CreateSiegeRamArchetype());
}

// ============================================================================
// Built-in Archetype Implementations
// ============================================================================

UnitArchetype CreateWorkerArchetype() {
    UnitArchetype arch;
    arch.id = "worker";
    arch.name = "Worker";
    arch.description = "Basic resource gatherer and builder.";
    arch.category = UnitCategory::Worker;
    arch.subtype = UnitSubtype::Harvester;

    arch.baseStats.health = 40;
    arch.baseStats.maxHealth = 40;
    arch.baseStats.damage = 5;
    arch.baseStats.attackSpeed = 0.8f;
    arch.baseStats.moveSpeed = 3.5f;
    arch.baseStats.carryCapacity = 20;
    arch.baseStats.gatherSpeed = 1.0f;
    arch.baseStats.buildSpeed = 1.0f;
    arch.baseStats.visionRange = 6.0f;

    arch.cost.gold = 50;
    arch.cost.buildTime = 15.0f;

    arch.attackType = "melee";
    arch.damageType = "physical";
    arch.movementType = "ground";
    arch.canGather = true;
    arch.canBuild = true;
    arch.canRepair = true;

    arch.requiredBuilding = "main_hall";
    arch.pointCost = 1;
    arch.powerRating = 0.5f;

    return arch;
}

UnitArchetype CreateBuilderArchetype() {
    UnitArchetype arch = CreateWorkerArchetype();
    arch.id = "builder";
    arch.name = "Builder";
    arch.description = "Specialized construction unit.";
    arch.subtype = UnitSubtype::Builder;

    arch.baseStats.buildSpeed = 1.5f;
    arch.baseStats.gatherSpeed = 0.5f;

    return arch;
}

UnitArchetype CreateInfantryMeleeArchetype() {
    UnitArchetype arch;
    arch.id = "infantry_melee";
    arch.name = "Swordsman";
    arch.description = "Standard melee infantry unit.";
    arch.category = UnitCategory::Infantry;
    arch.subtype = UnitSubtype::Melee;

    arch.baseStats.health = 100;
    arch.baseStats.maxHealth = 100;
    arch.baseStats.armor = 2;
    arch.baseStats.damage = 12;
    arch.baseStats.attackSpeed = 1.0f;
    arch.baseStats.attackRange = 1.0f;
    arch.baseStats.moveSpeed = 4.0f;
    arch.baseStats.visionRange = 8.0f;

    arch.cost.gold = 75;
    arch.cost.food = 25;
    arch.cost.buildTime = 20.0f;

    arch.attackType = "melee";
    arch.damageType = "physical";
    arch.movementType = "ground";

    arch.requiredBuilding = "barracks";
    arch.pointCost = 3;
    arch.powerRating = 1.0f;

    return arch;
}

UnitArchetype CreateInfantryPikeArchetype() {
    UnitArchetype arch;
    arch.id = "infantry_pike";
    arch.name = "Pikeman";
    arch.description = "Anti-cavalry melee infantry.";
    arch.category = UnitCategory::Infantry;
    arch.subtype = UnitSubtype::Pike;

    arch.baseStats.health = 85;
    arch.baseStats.maxHealth = 85;
    arch.baseStats.armor = 1;
    arch.baseStats.damage = 15;
    arch.baseStats.attackSpeed = 0.9f;
    arch.baseStats.attackRange = 1.5f;
    arch.baseStats.moveSpeed = 3.5f;

    arch.cost.gold = 80;
    arch.cost.wood = 30;
    arch.cost.buildTime = 22.0f;

    arch.attackType = "melee";
    arch.damageType = "pierce";
    arch.movementType = "ground";

    arch.requiredBuilding = "barracks";
    arch.pointCost = 4;
    arch.powerRating = 1.2f;
    arch.tags = {"anti_cavalry"};

    return arch;
}

UnitArchetype CreateInfantryShieldArchetype() {
    UnitArchetype arch;
    arch.id = "infantry_shield";
    arch.name = "Shieldbearer";
    arch.description = "Heavy defensive infantry.";
    arch.category = UnitCategory::Infantry;
    arch.subtype = UnitSubtype::Shield;

    arch.baseStats.health = 150;
    arch.baseStats.maxHealth = 150;
    arch.baseStats.armor = 5;
    arch.baseStats.damage = 8;
    arch.baseStats.attackSpeed = 0.8f;
    arch.baseStats.moveSpeed = 3.0f;

    arch.cost.gold = 100;
    arch.cost.metal = 30;
    arch.cost.buildTime = 28.0f;

    arch.attackType = "melee";
    arch.damageType = "physical";
    arch.movementType = "ground";

    arch.requiredBuilding = "barracks";
    arch.requiredAge = 1;  // Bronze Age
    arch.pointCost = 5;
    arch.powerRating = 1.3f;
    arch.tags = {"tank", "defensive"};

    return arch;
}

UnitArchetype CreateInfantryBerserkerArchetype() {
    UnitArchetype arch;
    arch.id = "infantry_berserker";
    arch.name = "Berserker";
    arch.description = "Aggressive melee fighter with high damage.";
    arch.category = UnitCategory::Infantry;
    arch.subtype = UnitSubtype::Berserker;

    arch.baseStats.health = 80;
    arch.baseStats.maxHealth = 80;
    arch.baseStats.armor = 0;
    arch.baseStats.damage = 20;
    arch.baseStats.attackSpeed = 1.3f;
    arch.baseStats.moveSpeed = 4.5f;

    arch.cost.gold = 90;
    arch.cost.food = 40;
    arch.cost.buildTime = 25.0f;

    arch.attackType = "melee";
    arch.damageType = "physical";
    arch.movementType = "ground";

    arch.requiredBuilding = "barracks";
    arch.pointCost = 5;
    arch.powerRating = 1.4f;
    arch.tags = {"aggressive", "glass_cannon"};

    return arch;
}

UnitArchetype CreateRangedArcherArchetype() {
    UnitArchetype arch;
    arch.id = "ranged_archer";
    arch.name = "Archer";
    arch.description = "Standard ranged unit with bow.";
    arch.category = UnitCategory::Ranged;
    arch.subtype = UnitSubtype::Archer;

    arch.baseStats.health = 60;
    arch.baseStats.maxHealth = 60;
    arch.baseStats.armor = 0;
    arch.baseStats.damage = 10;
    arch.baseStats.attackSpeed = 1.2f;
    arch.baseStats.attackRange = 6.0f;
    arch.baseStats.moveSpeed = 4.0f;

    arch.cost.gold = 70;
    arch.cost.wood = 25;
    arch.cost.buildTime = 18.0f;

    arch.attackType = "ranged";
    arch.damageType = "pierce";
    arch.projectileId = "arrow_basic";
    arch.movementType = "ground";
    arch.canAttackAir = true;

    arch.requiredBuilding = "archery_range";
    arch.pointCost = 4;
    arch.powerRating = 1.1f;

    return arch;
}

UnitArchetype CreateRangedGunnerArchetype() {
    UnitArchetype arch;
    arch.id = "ranged_gunner";
    arch.name = "Musketeer";
    arch.description = "Ranged unit with firearm.";
    arch.category = UnitCategory::Ranged;
    arch.subtype = UnitSubtype::Gunner;

    arch.baseStats.health = 50;
    arch.baseStats.maxHealth = 50;
    arch.baseStats.damage = 25;
    arch.baseStats.attackSpeed = 0.5f;
    arch.baseStats.attackRange = 8.0f;
    arch.baseStats.moveSpeed = 3.5f;

    arch.cost.gold = 100;
    arch.cost.metal = 40;
    arch.cost.buildTime = 25.0f;

    arch.attackType = "ranged";
    arch.damageType = "pierce";
    arch.projectileId = "bullet_rifle";
    arch.movementType = "ground";

    arch.requiredBuilding = "barracks";
    arch.requiredAge = 4;  // Industrial Age
    arch.pointCost = 6;
    arch.powerRating = 1.5f;

    return arch;
}

UnitArchetype CreateRangedCasterArchetype() {
    UnitArchetype arch;
    arch.id = "ranged_caster";
    arch.name = "Battle Mage";
    arch.description = "Magical ranged unit.";
    arch.category = UnitCategory::Ranged;
    arch.subtype = UnitSubtype::Caster;

    arch.baseStats.health = 45;
    arch.baseStats.maxHealth = 45;
    arch.baseStats.magicResist = 5;
    arch.baseStats.damage = 18;
    arch.baseStats.attackSpeed = 0.8f;
    arch.baseStats.attackRange = 7.0f;
    arch.baseStats.moveSpeed = 3.5f;

    arch.cost.gold = 120;
    arch.cost.buildTime = 30.0f;

    arch.attackType = "ranged";
    arch.damageType = "magic";
    arch.projectileId = "magic_bolt";
    arch.movementType = "ground";

    arch.requiredBuilding = "arcane_sanctuary";
    arch.requiredAge = 2;
    arch.pointCost = 6;
    arch.powerRating = 1.4f;

    return arch;
}

UnitArchetype CreateCavalryLightArchetype() {
    UnitArchetype arch;
    arch.id = "cavalry_light";
    arch.name = "Light Cavalry";
    arch.description = "Fast mounted scout and raider.";
    arch.category = UnitCategory::Cavalry;
    arch.subtype = UnitSubtype::Light;

    arch.baseStats.health = 80;
    arch.baseStats.maxHealth = 80;
    arch.baseStats.armor = 1;
    arch.baseStats.damage = 10;
    arch.baseStats.attackSpeed = 1.1f;
    arch.baseStats.moveSpeed = 7.0f;
    arch.baseStats.visionRange = 10.0f;

    arch.cost.gold = 80;
    arch.cost.food = 40;
    arch.cost.buildTime = 22.0f;

    arch.attackType = "melee";
    arch.damageType = "physical";
    arch.movementType = "ground";

    arch.requiredBuilding = "stable";
    arch.pointCost = 5;
    arch.powerRating = 1.2f;
    arch.tags = {"fast", "scout"};

    return arch;
}

UnitArchetype CreateCavalryHeavyArchetype() {
    UnitArchetype arch;
    arch.id = "cavalry_heavy";
    arch.name = "Knight";
    arch.description = "Heavily armored mounted warrior.";
    arch.category = UnitCategory::Cavalry;
    arch.subtype = UnitSubtype::Heavy;

    arch.baseStats.health = 150;
    arch.baseStats.maxHealth = 150;
    arch.baseStats.armor = 4;
    arch.baseStats.damage = 20;
    arch.baseStats.attackSpeed = 0.9f;
    arch.baseStats.moveSpeed = 5.5f;

    arch.cost.gold = 150;
    arch.cost.food = 50;
    arch.cost.metal = 30;
    arch.cost.buildTime = 35.0f;

    arch.attackType = "melee";
    arch.damageType = "physical";
    arch.movementType = "ground";

    arch.requiredBuilding = "stable";
    arch.requiredAge = 2;
    arch.pointCost = 8;
    arch.powerRating = 2.0f;
    arch.tags = {"heavy", "charge"};

    return arch;
}

UnitArchetype CreateCavalryChariotArchetype() {
    UnitArchetype arch;
    arch.id = "cavalry_chariot";
    arch.name = "War Chariot";
    arch.description = "Mobile ranged platform.";
    arch.category = UnitCategory::Cavalry;
    arch.subtype = UnitSubtype::Chariot;

    arch.baseStats.health = 120;
    arch.baseStats.maxHealth = 120;
    arch.baseStats.armor = 2;
    arch.baseStats.damage = 12;
    arch.baseStats.attackSpeed = 1.0f;
    arch.baseStats.attackRange = 4.0f;
    arch.baseStats.moveSpeed = 6.0f;

    arch.cost.gold = 120;
    arch.cost.wood = 60;
    arch.cost.buildTime = 30.0f;

    arch.attackType = "ranged";
    arch.damageType = "pierce";
    arch.movementType = "ground";

    arch.requiredBuilding = "stable";
    arch.requiredAge = 1;
    arch.pointCost = 7;
    arch.powerRating = 1.6f;

    return arch;
}

UnitArchetype CreateSiegeCatapultArchetype() {
    UnitArchetype arch;
    arch.id = "siege_catapult";
    arch.name = "Catapult";
    arch.description = "Long-range siege weapon.";
    arch.category = UnitCategory::Siege;
    arch.subtype = UnitSubtype::Catapult;

    arch.baseStats.health = 100;
    arch.baseStats.maxHealth = 100;
    arch.baseStats.armor = 0;
    arch.baseStats.damage = 50;
    arch.baseStats.attackSpeed = 0.2f;
    arch.baseStats.attackRange = 12.0f;
    arch.baseStats.moveSpeed = 2.0f;

    arch.cost.gold = 200;
    arch.cost.wood = 100;
    arch.cost.buildTime = 45.0f;

    arch.attackType = "ranged";
    arch.damageType = "siege";
    arch.projectileId = "boulder";
    arch.movementType = "ground";
    arch.canAttackAir = false;

    arch.requiredBuilding = "siege_workshop";
    arch.requiredAge = 2;
    arch.pointCost = 10;
    arch.powerRating = 2.5f;
    arch.tags = {"siege", "building_destroyer"};

    return arch;
}

UnitArchetype CreateSiegeRamArchetype() {
    UnitArchetype arch;
    arch.id = "siege_ram";
    arch.name = "Battering Ram";
    arch.description = "Heavy siege unit for destroying buildings.";
    arch.category = UnitCategory::Siege;
    arch.subtype = UnitSubtype::Ram;

    arch.baseStats.health = 200;
    arch.baseStats.maxHealth = 200;
    arch.baseStats.armor = 3;
    arch.baseStats.damage = 80;
    arch.baseStats.attackSpeed = 0.3f;
    arch.baseStats.attackRange = 1.0f;
    arch.baseStats.moveSpeed = 2.0f;

    arch.cost.gold = 150;
    arch.cost.wood = 150;
    arch.cost.buildTime = 50.0f;

    arch.attackType = "melee";
    arch.damageType = "siege";
    arch.movementType = "ground";

    arch.requiredBuilding = "siege_workshop";
    arch.requiredAge = 2;
    arch.pointCost = 8;
    arch.powerRating = 2.0f;
    arch.tags = {"siege", "anti_building"};

    return arch;
}

UnitArchetype CreateSiegeTowerArchetype() {
    UnitArchetype arch;
    arch.id = "siege_tower";
    arch.name = "Siege Tower";
    arch.description = "Mobile tower for breaching walls.";
    arch.category = UnitCategory::Siege;
    arch.subtype = UnitSubtype::Tower;

    arch.baseStats.health = 300;
    arch.baseStats.maxHealth = 300;
    arch.baseStats.armor = 5;
    arch.baseStats.damage = 0;
    arch.baseStats.moveSpeed = 1.5f;

    arch.cost.gold = 250;
    arch.cost.wood = 200;
    arch.cost.buildTime = 60.0f;

    arch.movementType = "ground";

    arch.requiredBuilding = "siege_workshop";
    arch.requiredAge = 3;
    arch.pointCost = 12;
    arch.powerRating = 2.0f;
    arch.tags = {"siege", "transport", "wall_breaker"};

    return arch;
}

UnitArchetype CreateNavalTransportArchetype() {
    UnitArchetype arch;
    arch.id = "naval_transport";
    arch.name = "Transport Ship";
    arch.description = "Ship for transporting ground units.";
    arch.category = UnitCategory::Naval;
    arch.subtype = UnitSubtype::Transport;

    arch.baseStats.health = 150;
    arch.baseStats.maxHealth = 150;
    arch.baseStats.damage = 0;
    arch.baseStats.moveSpeed = 5.0f;

    arch.cost.gold = 100;
    arch.cost.wood = 150;
    arch.cost.buildTime = 35.0f;

    arch.movementType = "swim";

    arch.requiredBuilding = "dock";
    arch.pointCost = 5;
    arch.powerRating = 1.0f;
    arch.tags = {"naval", "transport"};

    return arch;
}

UnitArchetype CreateNavalWarshipArchetype() {
    UnitArchetype arch;
    arch.id = "naval_warship";
    arch.name = "Warship";
    arch.description = "Combat vessel with cannons.";
    arch.category = UnitCategory::Naval;
    arch.subtype = UnitSubtype::Warship;

    arch.baseStats.health = 250;
    arch.baseStats.maxHealth = 250;
    arch.baseStats.armor = 3;
    arch.baseStats.damage = 40;
    arch.baseStats.attackSpeed = 0.4f;
    arch.baseStats.attackRange = 10.0f;
    arch.baseStats.moveSpeed = 4.0f;

    arch.cost.gold = 250;
    arch.cost.wood = 200;
    arch.cost.metal = 50;
    arch.cost.buildTime = 50.0f;

    arch.attackType = "ranged";
    arch.damageType = "siege";
    arch.movementType = "swim";

    arch.requiredBuilding = "dock";
    arch.requiredAge = 3;
    arch.pointCost = 10;
    arch.powerRating = 2.5f;
    arch.tags = {"naval", "combat"};

    return arch;
}

UnitArchetype CreateNavalSubmarineArchetype() {
    UnitArchetype arch;
    arch.id = "naval_submarine";
    arch.name = "Submarine";
    arch.description = "Stealthy underwater vessel.";
    arch.category = UnitCategory::Naval;
    arch.subtype = UnitSubtype::Submarine;

    arch.baseStats.health = 150;
    arch.baseStats.maxHealth = 150;
    arch.baseStats.damage = 60;
    arch.baseStats.attackSpeed = 0.3f;
    arch.baseStats.attackRange = 5.0f;
    arch.baseStats.moveSpeed = 3.5f;

    arch.cost.gold = 300;
    arch.cost.metal = 150;
    arch.cost.buildTime = 60.0f;

    arch.attackType = "ranged";
    arch.damageType = "pierce";
    arch.movementType = "swim";
    arch.isStealthed = true;

    arch.requiredBuilding = "dock";
    arch.requiredAge = 5;
    arch.pointCost = 12;
    arch.powerRating = 2.5f;
    arch.tags = {"naval", "stealth"};

    return arch;
}

UnitArchetype CreateAirScoutArchetype() {
    UnitArchetype arch;
    arch.id = "air_scout";
    arch.name = "Scout Flyer";
    arch.description = "Fast aerial reconnaissance unit.";
    arch.category = UnitCategory::Air;
    arch.subtype = UnitSubtype::AirScout;

    arch.baseStats.health = 40;
    arch.baseStats.maxHealth = 40;
    arch.baseStats.damage = 5;
    arch.baseStats.attackSpeed = 1.0f;
    arch.baseStats.moveSpeed = 8.0f;
    arch.baseStats.visionRange = 14.0f;

    arch.cost.gold = 100;
    arch.cost.buildTime = 25.0f;

    arch.attackType = "ranged";
    arch.damageType = "physical";
    arch.movementType = "fly";
    arch.canAttackAir = true;
    arch.isDetector = true;

    arch.requiredBuilding = "airfield";
    arch.requiredAge = 4;
    arch.pointCost = 5;
    arch.powerRating = 1.0f;
    arch.tags = {"air", "scout", "detector"};

    return arch;
}

UnitArchetype CreateAirFighterArchetype() {
    UnitArchetype arch;
    arch.id = "air_fighter";
    arch.name = "Fighter";
    arch.description = "Air superiority combat aircraft.";
    arch.category = UnitCategory::Air;
    arch.subtype = UnitSubtype::Fighter;

    arch.baseStats.health = 80;
    arch.baseStats.maxHealth = 80;
    arch.baseStats.damage = 20;
    arch.baseStats.attackSpeed = 1.5f;
    arch.baseStats.attackRange = 4.0f;
    arch.baseStats.moveSpeed = 10.0f;

    arch.cost.gold = 200;
    arch.cost.metal = 100;
    arch.cost.buildTime = 40.0f;

    arch.attackType = "ranged";
    arch.damageType = "physical";
    arch.movementType = "fly";
    arch.canAttackAir = true;
    arch.canAttackGround = false;

    arch.requiredBuilding = "airfield";
    arch.requiredAge = 5;
    arch.pointCost = 10;
    arch.powerRating = 2.0f;
    arch.tags = {"air", "anti_air"};

    return arch;
}

UnitArchetype CreateAirBomberArchetype() {
    UnitArchetype arch;
    arch.id = "air_bomber";
    arch.name = "Bomber";
    arch.description = "Heavy aircraft for ground attacks.";
    arch.category = UnitCategory::Air;
    arch.subtype = UnitSubtype::Bomber;

    arch.baseStats.health = 120;
    arch.baseStats.maxHealth = 120;
    arch.baseStats.damage = 60;
    arch.baseStats.attackSpeed = 0.3f;
    arch.baseStats.attackRange = 3.0f;
    arch.baseStats.moveSpeed = 6.0f;

    arch.cost.gold = 300;
    arch.cost.metal = 150;
    arch.cost.buildTime = 55.0f;

    arch.attackType = "ranged";
    arch.damageType = "siege";
    arch.movementType = "fly";
    arch.canAttackAir = false;
    arch.canAttackGround = true;

    arch.requiredBuilding = "airfield";
    arch.requiredAge = 5;
    arch.pointCost = 12;
    arch.powerRating = 2.5f;
    arch.tags = {"air", "bomber", "siege"};

    return arch;
}

UnitArchetype CreateSpecialAssassinArchetype() {
    UnitArchetype arch;
    arch.id = "special_assassin";
    arch.name = "Assassin";
    arch.description = "Stealthy unit with high single-target damage.";
    arch.category = UnitCategory::Special;
    arch.subtype = UnitSubtype::Assassin;

    arch.baseStats.health = 50;
    arch.baseStats.maxHealth = 50;
    arch.baseStats.damage = 40;
    arch.baseStats.attackSpeed = 0.8f;
    arch.baseStats.moveSpeed = 5.5f;
    arch.baseStats.visionRange = 6.0f;

    arch.cost.gold = 150;
    arch.cost.buildTime = 35.0f;

    arch.attackType = "melee";
    arch.damageType = "physical";
    arch.movementType = "ground";
    arch.isStealthed = true;

    arch.requiredBuilding = "barracks";
    arch.requiredTech = "tech_stealth";
    arch.requiredAge = 3;
    arch.pointCost = 8;
    arch.powerRating = 1.8f;
    arch.tags = {"stealth", "burst_damage"};

    return arch;
}

UnitArchetype CreateSpecialHealerArchetype() {
    UnitArchetype arch;
    arch.id = "special_healer";
    arch.name = "Healer";
    arch.description = "Support unit that heals allies.";
    arch.category = UnitCategory::Special;
    arch.subtype = UnitSubtype::Healer;

    arch.baseStats.health = 45;
    arch.baseStats.maxHealth = 45;
    arch.baseStats.damage = 0;
    arch.baseStats.moveSpeed = 4.0f;

    arch.cost.gold = 100;
    arch.cost.buildTime = 30.0f;

    arch.movementType = "ground";
    arch.canHeal = true;

    arch.requiredBuilding = "temple";
    arch.pointCost = 6;
    arch.powerRating = 1.5f;
    arch.tags = {"support", "healer"};

    return arch;
}

UnitArchetype CreateSpecialSummonerArchetype() {
    UnitArchetype arch;
    arch.id = "special_summoner";
    arch.name = "Summoner";
    arch.description = "Magical unit that summons creatures.";
    arch.category = UnitCategory::Special;
    arch.subtype = UnitSubtype::Summoner;

    arch.baseStats.health = 40;
    arch.baseStats.maxHealth = 40;
    arch.baseStats.damage = 8;
    arch.baseStats.attackSpeed = 0.6f;
    arch.baseStats.attackRange = 5.0f;
    arch.baseStats.moveSpeed = 3.5f;

    arch.cost.gold = 180;
    arch.cost.buildTime = 40.0f;

    arch.attackType = "ranged";
    arch.damageType = "magic";
    arch.movementType = "ground";

    arch.requiredBuilding = "arcane_sanctuary";
    arch.requiredAge = 3;
    arch.pointCost = 10;
    arch.powerRating = 2.0f;
    arch.tags = {"magic", "summoner"};

    return arch;
}

} // namespace Race
} // namespace RTS
} // namespace Vehement
