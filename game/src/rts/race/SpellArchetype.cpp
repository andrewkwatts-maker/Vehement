#include "SpellArchetype.hpp"
#include <fstream>
#include <filesystem>
#include <cmath>

namespace Vehement {
namespace RTS {
namespace Race {

// SpellEffect
nlohmann::json SpellEffect::ToJson() const {
    return {{"effectType", effectType}, {"statAffected", statAffected},
            {"baseValue", baseValue}, {"scalingFactor", scalingFactor},
            {"duration", duration}, {"tickRate", tickRate}, {"appliedEffect", appliedEffect}};
}

SpellEffect SpellEffect::FromJson(const nlohmann::json& j) {
    SpellEffect e;
    if (j.contains("effectType")) e.effectType = j["effectType"].get<std::string>();
    if (j.contains("statAffected")) e.statAffected = j["statAffected"].get<std::string>();
    if (j.contains("baseValue")) e.baseValue = j["baseValue"].get<float>();
    if (j.contains("scalingFactor")) e.scalingFactor = j["scalingFactor"].get<float>();
    if (j.contains("duration")) e.duration = j["duration"].get<float>();
    if (j.contains("tickRate")) e.tickRate = j["tickRate"].get<float>();
    if (j.contains("appliedEffect")) e.appliedEffect = j["appliedEffect"].get<std::string>();
    return e;
}

// SpellArchetype
float SpellArchetype::CalculateEffectValue(int level, float casterStat) const {
    if (effects.empty()) return 0.0f;
    float base = effects[0].baseValue;
    float scaling = effects[0].scalingFactor * casterStat;
    float levelBonus = base * effectPerLevel * (level - 1);
    return base + scaling + levelBonus;
}

float SpellArchetype::CalculateManaCost(int level) const {
    return manaCost + manaCostPerLevel * (level - 1);
}

float SpellArchetype::CalculateCooldown(int level, float cdrBonus) const {
    float cd = cooldown * (1.0f - cdrBonus);
    return std::max(1.0f, cd);
}

bool SpellArchetype::Validate() const { return GetValidationErrors().empty(); }

std::vector<std::string> SpellArchetype::GetValidationErrors() const {
    std::vector<std::string> errors;
    if (id.empty()) errors.push_back("Spell ID required");
    if (name.empty()) errors.push_back("Spell name required");
    if (manaCost < 0) errors.push_back("Mana cost cannot be negative");
    if (cooldown < 0) errors.push_back("Cooldown cannot be negative");
    return errors;
}

nlohmann::json SpellArchetype::ToJson() const {
    nlohmann::json effectsJson = nlohmann::json::array();
    for (const auto& e : effects) effectsJson.push_back(e.ToJson());

    return {
        {"id", id}, {"name", name}, {"description", description}, {"iconPath", iconPath},
        {"category", SpellCategoryToString(category)},
        {"targetType", SpellTargetTypeToString(targetType)},
        {"manaCost", manaCost}, {"cooldown", cooldown}, {"castTime", castTime},
        {"range", range}, {"radius", radius}, {"effects", effectsJson},
        {"summonUnitId", summonUnitId}, {"summonCount", summonCount},
        {"summonDuration", summonDuration}, {"requiredBuilding", requiredBuilding},
        {"requiredTech", requiredTech}, {"requiredAge", requiredAge},
        {"canUpgrade", canUpgrade}, {"maxLevel", maxLevel},
        {"manaCostPerLevel", manaCostPerLevel}, {"effectPerLevel", effectPerLevel},
        {"castEffect", castEffect}, {"impactEffect", impactEffect},
        {"projectileId", projectileId}, {"castSound", castSound}, {"impactSound", impactSound},
        {"pointCost", pointCost}, {"powerRating", powerRating}, {"tags", tags}
    };
}

SpellArchetype SpellArchetype::FromJson(const nlohmann::json& j) {
    SpellArchetype s;
    if (j.contains("id")) s.id = j["id"].get<std::string>();
    if (j.contains("name")) s.name = j["name"].get<std::string>();
    if (j.contains("description")) s.description = j["description"].get<std::string>();
    if (j.contains("iconPath")) s.iconPath = j["iconPath"].get<std::string>();

    if (j.contains("category")) {
        std::string cat = j["category"].get<std::string>();
        if (cat == "Damage") s.category = SpellCategory::Damage;
        else if (cat == "Healing") s.category = SpellCategory::Healing;
        else if (cat == "Buff") s.category = SpellCategory::Buff;
        else if (cat == "Debuff") s.category = SpellCategory::Debuff;
        else if (cat == "Summon") s.category = SpellCategory::Summon;
        else if (cat == "Utility") s.category = SpellCategory::Utility;
        else if (cat == "Ultimate") s.category = SpellCategory::Ultimate;
    }

    if (j.contains("targetType")) {
        std::string tt = j["targetType"].get<std::string>();
        if (tt == "Self") s.targetType = SpellTargetType::Self;
        else if (tt == "SingleAlly") s.targetType = SpellTargetType::SingleAlly;
        else if (tt == "SingleEnemy") s.targetType = SpellTargetType::SingleEnemy;
        else if (tt == "SingleUnit") s.targetType = SpellTargetType::SingleUnit;
        else if (tt == "AlliedArea") s.targetType = SpellTargetType::AlliedArea;
        else if (tt == "EnemyArea") s.targetType = SpellTargetType::EnemyArea;
        else if (tt == "AllArea") s.targetType = SpellTargetType::AllArea;
        else if (tt == "Ground") s.targetType = SpellTargetType::Ground;
        else if (tt == "None") s.targetType = SpellTargetType::None;
    }

    if (j.contains("manaCost")) s.manaCost = j["manaCost"].get<float>();
    if (j.contains("cooldown")) s.cooldown = j["cooldown"].get<float>();
    if (j.contains("castTime")) s.castTime = j["castTime"].get<float>();
    if (j.contains("range")) s.range = j["range"].get<float>();
    if (j.contains("radius")) s.radius = j["radius"].get<float>();

    if (j.contains("effects") && j["effects"].is_array()) {
        for (const auto& e : j["effects"]) s.effects.push_back(SpellEffect::FromJson(e));
    }

    if (j.contains("summonUnitId")) s.summonUnitId = j["summonUnitId"].get<std::string>();
    if (j.contains("summonCount")) s.summonCount = j["summonCount"].get<int>();
    if (j.contains("summonDuration")) s.summonDuration = j["summonDuration"].get<float>();
    if (j.contains("requiredBuilding")) s.requiredBuilding = j["requiredBuilding"].get<std::string>();
    if (j.contains("requiredTech")) s.requiredTech = j["requiredTech"].get<std::string>();
    if (j.contains("requiredAge")) s.requiredAge = j["requiredAge"].get<int>();
    if (j.contains("canUpgrade")) s.canUpgrade = j["canUpgrade"].get<bool>();
    if (j.contains("maxLevel")) s.maxLevel = j["maxLevel"].get<int>();
    if (j.contains("manaCostPerLevel")) s.manaCostPerLevel = j["manaCostPerLevel"].get<float>();
    if (j.contains("effectPerLevel")) s.effectPerLevel = j["effectPerLevel"].get<float>();
    if (j.contains("castEffect")) s.castEffect = j["castEffect"].get<std::string>();
    if (j.contains("impactEffect")) s.impactEffect = j["impactEffect"].get<std::string>();
    if (j.contains("projectileId")) s.projectileId = j["projectileId"].get<std::string>();
    if (j.contains("castSound")) s.castSound = j["castSound"].get<std::string>();
    if (j.contains("impactSound")) s.impactSound = j["impactSound"].get<std::string>();
    if (j.contains("pointCost")) s.pointCost = j["pointCost"].get<int>();
    if (j.contains("powerRating")) s.powerRating = j["powerRating"].get<float>();
    if (j.contains("tags")) s.tags = j["tags"].get<std::vector<std::string>>();

    return s;
}

bool SpellArchetype::SaveToFile(const std::string& filepath) const {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;
    f << ToJson().dump(2);
    return true;
}

bool SpellArchetype::LoadFromFile(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) return false;
    try { nlohmann::json j; f >> j; *this = FromJson(j); return true; }
    catch (...) { return false; }
}

// Registry
SpellArchetypeRegistry& SpellArchetypeRegistry::Instance() {
    static SpellArchetypeRegistry instance;
    return instance;
}

bool SpellArchetypeRegistry::Initialize() {
    if (m_initialized) return true;
    InitializeBuiltInArchetypes();
    m_initialized = true;
    return true;
}

void SpellArchetypeRegistry::Shutdown() { m_archetypes.clear(); m_initialized = false; }

bool SpellArchetypeRegistry::RegisterArchetype(const SpellArchetype& a) {
    if (a.id.empty()) return false;
    m_archetypes[a.id] = a;
    return true;
}

const SpellArchetype* SpellArchetypeRegistry::GetArchetype(const std::string& id) const {
    auto it = m_archetypes.find(id);
    return (it != m_archetypes.end()) ? &it->second : nullptr;
}

std::vector<const SpellArchetype*> SpellArchetypeRegistry::GetAllArchetypes() const {
    std::vector<const SpellArchetype*> r;
    for (const auto& [id, a] : m_archetypes) r.push_back(&a);
    return r;
}

std::vector<const SpellArchetype*> SpellArchetypeRegistry::GetByCategory(SpellCategory cat) const {
    std::vector<const SpellArchetype*> r;
    for (const auto& [id, a] : m_archetypes) if (a.category == cat) r.push_back(&a);
    return r;
}

int SpellArchetypeRegistry::LoadFromDirectory(const std::string& dir) {
    int count = 0;
    try {
        for (const auto& e : std::filesystem::directory_iterator(dir)) {
            if (e.path().extension() == ".json") {
                SpellArchetype a;
                if (a.LoadFromFile(e.path().string()) && RegisterArchetype(a)) ++count;
            }
        }
    } catch (...) {}
    return count;
}

void SpellArchetypeRegistry::InitializeBuiltInArchetypes() {
    RegisterArchetype(CreateDamageSingleArchetype());
    RegisterArchetype(CreateDamageAOEArchetype());
    RegisterArchetype(CreateHealingSingleArchetype());
    RegisterArchetype(CreateHealingAOEArchetype());
    RegisterArchetype(CreateBuffAttackArchetype());
    RegisterArchetype(CreateDebuffSlowArchetype());
    RegisterArchetype(CreateSummonUnitsArchetype());
    RegisterArchetype(CreateUtilityTeleportArchetype());
    RegisterArchetype(CreateUltimateMeteorArchetype());
}

// Built-in Archetypes
SpellArchetype CreateDamageSingleArchetype() {
    SpellArchetype s;
    s.id = "spell_damage_single";
    s.name = "Fireball";
    s.description = "Launches a fireball at a single target.";
    s.category = SpellCategory::Damage;
    s.targetType = SpellTargetType::SingleEnemy;
    s.manaCost = 40.0f;
    s.cooldown = 8.0f;
    s.castTime = 0.5f;
    s.range = 10.0f;
    s.effects = {{"damage", "health", 80.0f, 0.5f, 0.0f, 0.0f, "burning"}};
    s.projectileId = "projectile_fireball";
    s.pointCost = 4;
    s.powerRating = 1.0f;
    s.tags = {"fire", "single_target"};
    return s;
}

SpellArchetype CreateDamageAOEArchetype() {
    SpellArchetype s;
    s.id = "spell_damage_aoe";
    s.name = "Blizzard";
    s.description = "Calls down ice shards over an area.";
    s.category = SpellCategory::Damage;
    s.targetType = SpellTargetType::EnemyArea;
    s.manaCost = 80.0f;
    s.cooldown = 15.0f;
    s.castTime = 1.0f;
    s.range = 12.0f;
    s.radius = 5.0f;
    s.effects = {{"damage", "health", 30.0f, 0.3f, 6.0f, 1.0f, "frozen"}};
    s.pointCost = 6;
    s.powerRating = 1.5f;
    s.tags = {"ice", "aoe", "dot"};
    return s;
}

SpellArchetype CreateDamageDOTArchetype() {
    SpellArchetype s;
    s.id = "spell_damage_dot";
    s.name = "Poison Cloud";
    s.description = "Creates a cloud of poison dealing damage over time.";
    s.category = SpellCategory::Damage;
    s.targetType = SpellTargetType::Ground;
    s.manaCost = 60.0f;
    s.cooldown = 12.0f;
    s.range = 10.0f;
    s.radius = 4.0f;
    s.effects = {{"damage", "health", 15.0f, 0.2f, 10.0f, 1.0f, "poisoned"}};
    s.pointCost = 5;
    s.tags = {"poison", "aoe", "dot"};
    return s;
}

SpellArchetype CreateHealingSingleArchetype() {
    SpellArchetype s;
    s.id = "spell_heal_single";
    s.name = "Heal";
    s.description = "Heals a single allied unit.";
    s.category = SpellCategory::Healing;
    s.targetType = SpellTargetType::SingleAlly;
    s.manaCost = 35.0f;
    s.cooldown = 6.0f;
    s.range = 8.0f;
    s.effects = {{"heal", "health", 100.0f, 0.6f, 0.0f, 0.0f, ""}};
    s.pointCost = 4;
    s.tags = {"healing", "single_target"};
    return s;
}

SpellArchetype CreateHealingAOEArchetype() {
    SpellArchetype s;
    s.id = "spell_heal_aoe";
    s.name = "Mass Heal";
    s.description = "Heals all allies in an area.";
    s.category = SpellCategory::Healing;
    s.targetType = SpellTargetType::AlliedArea;
    s.manaCost = 100.0f;
    s.cooldown = 20.0f;
    s.range = 0.0f;
    s.radius = 8.0f;
    s.effects = {{"heal", "health", 60.0f, 0.4f, 0.0f, 0.0f, ""}};
    s.pointCost = 7;
    s.tags = {"healing", "aoe"};
    return s;
}

SpellArchetype CreateHealingHOTArchetype() {
    SpellArchetype s;
    s.id = "spell_heal_hot";
    s.name = "Regeneration";
    s.description = "Heals target over time.";
    s.category = SpellCategory::Healing;
    s.targetType = SpellTargetType::SingleAlly;
    s.manaCost = 45.0f;
    s.cooldown = 10.0f;
    s.range = 8.0f;
    s.effects = {{"heal", "health", 20.0f, 0.3f, 12.0f, 2.0f, "regeneration"}};
    s.pointCost = 5;
    s.tags = {"healing", "hot"};
    return s;
}

SpellArchetype CreateBuffAttackArchetype() {
    SpellArchetype s;
    s.id = "spell_buff_attack";
    s.name = "Bloodlust";
    s.description = "Increases attack speed and damage.";
    s.category = SpellCategory::Buff;
    s.targetType = SpellTargetType::SingleAlly;
    s.manaCost = 50.0f;
    s.cooldown = 15.0f;
    s.range = 8.0f;
    s.effects = {
        {"buff", "attackSpeed", 0.3f, 0.0f, 20.0f, 0.0f, "bloodlust"},
        {"buff", "damage", 0.2f, 0.0f, 20.0f, 0.0f, ""}
    };
    s.pointCost = 5;
    s.tags = {"buff", "attack"};
    return s;
}

SpellArchetype CreateBuffDefenseArchetype() {
    SpellArchetype s;
    s.id = "spell_buff_defense";
    s.name = "Stone Skin";
    s.description = "Increases armor significantly.";
    s.category = SpellCategory::Buff;
    s.targetType = SpellTargetType::SingleAlly;
    s.manaCost = 45.0f;
    s.cooldown = 12.0f;
    s.range = 8.0f;
    s.effects = {{"buff", "armor", 10.0f, 0.0f, 15.0f, 0.0f, "fortified"}};
    s.pointCost = 4;
    s.tags = {"buff", "defense"};
    return s;
}

SpellArchetype CreateBuffSpeedArchetype() {
    SpellArchetype s;
    s.id = "spell_buff_speed";
    s.name = "Haste";
    s.description = "Greatly increases movement speed.";
    s.category = SpellCategory::Buff;
    s.targetType = SpellTargetType::SingleAlly;
    s.manaCost = 40.0f;
    s.cooldown = 10.0f;
    s.range = 8.0f;
    s.effects = {{"buff", "moveSpeed", 0.5f, 0.0f, 10.0f, 0.0f, "haste"}};
    s.pointCost = 4;
    s.tags = {"buff", "speed"};
    return s;
}

SpellArchetype CreateDebuffSlowArchetype() {
    SpellArchetype s;
    s.id = "spell_debuff_slow";
    s.name = "Frost Nova";
    s.description = "Slows enemies in an area.";
    s.category = SpellCategory::Debuff;
    s.targetType = SpellTargetType::EnemyArea;
    s.manaCost = 60.0f;
    s.cooldown = 12.0f;
    s.range = 0.0f;
    s.radius = 6.0f;
    s.effects = {{"debuff", "moveSpeed", -0.4f, 0.0f, 8.0f, 0.0f, "slowed"}};
    s.pointCost = 5;
    s.tags = {"debuff", "slow", "aoe"};
    return s;
}

SpellArchetype CreateDebuffWeakenArchetype() {
    SpellArchetype s;
    s.id = "spell_debuff_weaken";
    s.name = "Curse";
    s.description = "Reduces enemy damage.";
    s.category = SpellCategory::Debuff;
    s.targetType = SpellTargetType::SingleEnemy;
    s.manaCost = 45.0f;
    s.cooldown = 10.0f;
    s.range = 10.0f;
    s.effects = {{"debuff", "damage", -0.3f, 0.0f, 15.0f, 0.0f, "weakened"}};
    s.pointCost = 4;
    s.tags = {"debuff", "weaken"};
    return s;
}

SpellArchetype CreateDebuffSilenceArchetype() {
    SpellArchetype s;
    s.id = "spell_debuff_silence";
    s.name = "Silence";
    s.description = "Prevents spellcasting.";
    s.category = SpellCategory::Debuff;
    s.targetType = SpellTargetType::EnemyArea;
    s.manaCost = 75.0f;
    s.cooldown = 20.0f;
    s.range = 10.0f;
    s.radius = 4.0f;
    s.effects = {{"debuff", "canCast", 0.0f, 0.0f, 6.0f, 0.0f, "silenced"}};
    s.pointCost = 6;
    s.tags = {"debuff", "silence"};
    return s;
}

SpellArchetype CreateSummonUnitsArchetype() {
    SpellArchetype s;
    s.id = "spell_summon_units";
    s.name = "Raise Skeleton";
    s.description = "Summons skeleton warriors.";
    s.category = SpellCategory::Summon;
    s.targetType = SpellTargetType::Ground;
    s.manaCost = 75.0f;
    s.cooldown = 25.0f;
    s.range = 8.0f;
    s.summonUnitId = "unit_skeleton";
    s.summonCount = 3;
    s.summonDuration = 60.0f;
    s.pointCost = 7;
    s.tags = {"summon", "undead"};
    return s;
}

SpellArchetype CreateSummonStructureArchetype() {
    SpellArchetype s;
    s.id = "spell_summon_structure";
    s.name = "Deploy Turret";
    s.description = "Creates a temporary turret.";
    s.category = SpellCategory::Summon;
    s.targetType = SpellTargetType::Ground;
    s.manaCost = 100.0f;
    s.cooldown = 45.0f;
    s.range = 6.0f;
    s.summonUnitId = "building_turret_temp";
    s.summonCount = 1;
    s.summonDuration = 30.0f;
    s.pointCost = 8;
    s.tags = {"summon", "structure"};
    return s;
}

SpellArchetype CreateSummonElementalArchetype() {
    SpellArchetype s;
    s.id = "spell_summon_elemental";
    s.name = "Summon Fire Elemental";
    s.description = "Summons a powerful fire elemental.";
    s.category = SpellCategory::Summon;
    s.targetType = SpellTargetType::Ground;
    s.manaCost = 150.0f;
    s.cooldown = 60.0f;
    s.range = 8.0f;
    s.summonUnitId = "unit_fire_elemental";
    s.summonCount = 1;
    s.summonDuration = 45.0f;
    s.pointCost = 10;
    s.tags = {"summon", "elemental", "fire"};
    return s;
}

SpellArchetype CreateUtilityTeleportArchetype() {
    SpellArchetype s;
    s.id = "spell_utility_teleport";
    s.name = "Blink";
    s.description = "Teleports caster to target location.";
    s.category = SpellCategory::Utility;
    s.targetType = SpellTargetType::Ground;
    s.manaCost = 50.0f;
    s.cooldown = 12.0f;
    s.castTime = 0.0f;
    s.range = 15.0f;
    s.pointCost = 6;
    s.tags = {"utility", "movement", "teleport"};
    return s;
}

SpellArchetype CreateUtilityRevealArchetype() {
    SpellArchetype s;
    s.id = "spell_utility_reveal";
    s.name = "Far Sight";
    s.description = "Reveals area of the map.";
    s.category = SpellCategory::Utility;
    s.targetType = SpellTargetType::Ground;
    s.manaCost = 40.0f;
    s.cooldown = 30.0f;
    s.range = 50.0f;
    s.radius = 15.0f;
    s.effects = {{"reveal", "", 0.0f, 0.0f, 10.0f, 0.0f, ""}};
    s.pointCost = 4;
    s.tags = {"utility", "vision"};
    return s;
}

SpellArchetype CreateUtilityDispelArchetype() {
    SpellArchetype s;
    s.id = "spell_utility_dispel";
    s.name = "Dispel Magic";
    s.description = "Removes buffs from enemies and debuffs from allies.";
    s.category = SpellCategory::Utility;
    s.targetType = SpellTargetType::AllArea;
    s.manaCost = 60.0f;
    s.cooldown = 15.0f;
    s.range = 10.0f;
    s.radius = 6.0f;
    s.effects = {{"dispel", "", 0.0f, 0.0f, 0.0f, 0.0f, ""}};
    s.pointCost = 5;
    s.tags = {"utility", "dispel"};
    return s;
}

SpellArchetype CreateUltimateMeteorArchetype() {
    SpellArchetype s;
    s.id = "spell_ultimate_meteor";
    s.name = "Meteor Strike";
    s.description = "Calls down a massive meteor dealing devastating damage.";
    s.category = SpellCategory::Ultimate;
    s.targetType = SpellTargetType::Ground;
    s.manaCost = 250.0f;
    s.cooldown = 120.0f;
    s.castTime = 2.0f;
    s.range = 20.0f;
    s.radius = 8.0f;
    s.effects = {{"damage", "health", 400.0f, 1.0f, 0.0f, 0.0f, "burning"}};
    s.pointCost = 15;
    s.powerRating = 3.0f;
    s.tags = {"ultimate", "aoe", "fire", "devastation"};
    return s;
}

SpellArchetype CreateUltimateResurrectionArchetype() {
    SpellArchetype s;
    s.id = "spell_ultimate_resurrection";
    s.name = "Mass Resurrection";
    s.description = "Brings dead allied units back to life.";
    s.category = SpellCategory::Ultimate;
    s.targetType = SpellTargetType::AlliedArea;
    s.manaCost = 300.0f;
    s.cooldown = 180.0f;
    s.castTime = 3.0f;
    s.range = 0.0f;
    s.radius = 15.0f;
    s.effects = {{"resurrect", "", 0.5f, 0.0f, 0.0f, 0.0f, ""}};  // 50% health
    s.pointCost = 18;
    s.tags = {"ultimate", "resurrection", "holy"};
    return s;
}

SpellArchetype CreateUltimateMindControlArchetype() {
    SpellArchetype s;
    s.id = "spell_ultimate_mind_control";
    s.name = "Dominate";
    s.description = "Takes control of an enemy unit permanently.";
    s.category = SpellCategory::Ultimate;
    s.targetType = SpellTargetType::SingleEnemy;
    s.manaCost = 200.0f;
    s.cooldown = 150.0f;
    s.castTime = 2.0f;
    s.range = 8.0f;
    s.effects = {{"control", "", 0.0f, 0.0f, 0.0f, 0.0f, "dominated"}};
    s.pointCost = 16;
    s.tags = {"ultimate", "mind_control", "dark"};
    return s;
}

} // namespace Race
} // namespace RTS
} // namespace Vehement
