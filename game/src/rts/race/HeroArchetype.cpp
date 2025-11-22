#include "HeroArchetype.hpp"
#include <fstream>
#include <filesystem>
#include <cmath>

namespace Vehement {
namespace RTS {
namespace Race {

const char* HeroSubclassToString(HeroSubclass s) {
    switch (s) {
        case HeroSubclass::Tank: return "Tank";
        case HeroSubclass::Berserker: return "Berserker";
        case HeroSubclass::Paladin: return "Paladin";
        case HeroSubclass::Archmage: return "Archmage";
        case HeroSubclass::Warlock: return "Warlock";
        case HeroSubclass::Summoner: return "Summoner";
        case HeroSubclass::Scout: return "Scout";
        case HeroSubclass::Sniper: return "Sniper";
        case HeroSubclass::Beastmaster: return "Beastmaster";
        case HeroSubclass::Healer: return "Healer";
        case HeroSubclass::Buffer: return "Buffer";
        case HeroSubclass::Aura: return "Aura";
        case HeroSubclass::SiegeMaster: return "SiegeMaster";
        case HeroSubclass::Assassin: return "Assassin";
        case HeroSubclass::Necromancer: return "Necromancer";
        default: return "Unknown";
    }
}

HeroSubclass StringToHeroSubclass(const std::string& str) {
    if (str == "Tank") return HeroSubclass::Tank;
    if (str == "Berserker") return HeroSubclass::Berserker;
    if (str == "Paladin") return HeroSubclass::Paladin;
    if (str == "Archmage") return HeroSubclass::Archmage;
    if (str == "Warlock") return HeroSubclass::Warlock;
    if (str == "Summoner") return HeroSubclass::Summoner;
    if (str == "Scout") return HeroSubclass::Scout;
    if (str == "Sniper") return HeroSubclass::Sniper;
    if (str == "Beastmaster") return HeroSubclass::Beastmaster;
    if (str == "Healer") return HeroSubclass::Healer;
    if (str == "Buffer") return HeroSubclass::Buffer;
    if (str == "Aura") return HeroSubclass::Aura;
    if (str == "SiegeMaster") return HeroSubclass::SiegeMaster;
    if (str == "Assassin") return HeroSubclass::Assassin;
    if (str == "Necromancer") return HeroSubclass::Necromancer;
    return HeroSubclass::Tank;
}

// HeroAbility
nlohmann::json HeroAbility::ToJson() const {
    return {{"abilityId", abilityId}, {"name", name}, {"description", description},
            {"unlockLevel", unlockLevel}, {"maxLevel", maxLevel}, {"cooldown", cooldown},
            {"manaCost", manaCost}, {"isPassive", isPassive}, {"isUltimate", isUltimate}};
}

HeroAbility HeroAbility::FromJson(const nlohmann::json& j) {
    HeroAbility a;
    if (j.contains("abilityId")) a.abilityId = j["abilityId"].get<std::string>();
    if (j.contains("name")) a.name = j["name"].get<std::string>();
    if (j.contains("description")) a.description = j["description"].get<std::string>();
    if (j.contains("unlockLevel")) a.unlockLevel = j["unlockLevel"].get<int>();
    if (j.contains("maxLevel")) a.maxLevel = j["maxLevel"].get<int>();
    if (j.contains("cooldown")) a.cooldown = j["cooldown"].get<float>();
    if (j.contains("manaCost")) a.manaCost = j["manaCost"].get<float>();
    if (j.contains("isPassive")) a.isPassive = j["isPassive"].get<bool>();
    if (j.contains("isUltimate")) a.isUltimate = j["isUltimate"].get<bool>();
    return a;
}

// HeroBaseStats
nlohmann::json HeroBaseStats::ToJson() const {
    auto base = UnitBaseStats::ToJson();
    base["strength"] = strength;
    base["agility"] = agility;
    base["intelligence"] = intelligence;
    base["mana"] = mana;
    base["maxMana"] = maxMana;
    base["manaRegen"] = manaRegen;
    base["experienceGain"] = experienceGain;
    base["startingLevel"] = startingLevel;
    base["maxLevel"] = maxLevel;
    base["healthPerLevel"] = healthPerLevel;
    base["manaPerLevel"] = manaPerLevel;
    base["damagePerLevel"] = damagePerLevel;
    base["armorPerLevel"] = armorPerLevel;
    base["strengthPerLevel"] = strengthPerLevel;
    base["agilityPerLevel"] = agilityPerLevel;
    base["intelligencePerLevel"] = intelligencePerLevel;
    return base;
}

HeroBaseStats HeroBaseStats::FromJson(const nlohmann::json& j) {
    HeroBaseStats s;
    static_cast<UnitBaseStats&>(s) = UnitBaseStats::FromJson(j);
    if (j.contains("strength")) s.strength = j["strength"].get<int>();
    if (j.contains("agility")) s.agility = j["agility"].get<int>();
    if (j.contains("intelligence")) s.intelligence = j["intelligence"].get<int>();
    if (j.contains("mana")) s.mana = j["mana"].get<float>();
    if (j.contains("maxMana")) s.maxMana = j["maxMana"].get<float>();
    if (j.contains("manaRegen")) s.manaRegen = j["manaRegen"].get<float>();
    if (j.contains("experienceGain")) s.experienceGain = j["experienceGain"].get<float>();
    if (j.contains("startingLevel")) s.startingLevel = j["startingLevel"].get<int>();
    if (j.contains("maxLevel")) s.maxLevel = j["maxLevel"].get<int>();
    if (j.contains("healthPerLevel")) s.healthPerLevel = j["healthPerLevel"].get<float>();
    if (j.contains("manaPerLevel")) s.manaPerLevel = j["manaPerLevel"].get<float>();
    if (j.contains("damagePerLevel")) s.damagePerLevel = j["damagePerLevel"].get<float>();
    if (j.contains("armorPerLevel")) s.armorPerLevel = j["armorPerLevel"].get<float>();
    if (j.contains("strengthPerLevel")) s.strengthPerLevel = j["strengthPerLevel"].get<int>();
    if (j.contains("agilityPerLevel")) s.agilityPerLevel = j["agilityPerLevel"].get<int>();
    if (j.contains("intelligencePerLevel")) s.intelligencePerLevel = j["intelligencePerLevel"].get<int>();
    return s;
}

// HeroArchetype
HeroBaseStats HeroArchetype::CalculateStatsAtLevel(int level,
    const std::map<std::string, float>& modifiers) const {
    HeroBaseStats stats = baseStats;
    int levelsGained = level - baseStats.startingLevel;
    if (levelsGained > 0) {
        stats.health += static_cast<int>(stats.healthPerLevel * levelsGained);
        stats.maxHealth += static_cast<int>(stats.healthPerLevel * levelsGained);
        stats.mana += stats.manaPerLevel * levelsGained;
        stats.maxMana += stats.manaPerLevel * levelsGained;
        stats.damage += static_cast<int>(stats.damagePerLevel * levelsGained);
        stats.armor += static_cast<int>(stats.armorPerLevel * levelsGained);
        stats.strength += stats.strengthPerLevel * levelsGained;
        stats.agility += stats.agilityPerLevel * levelsGained;
        stats.intelligence += stats.intelligencePerLevel * levelsGained;
    }

    // Apply modifiers
    for (const auto& [key, mult] : modifiers) {
        if (key == "health") stats.health = static_cast<int>(stats.health * mult);
        else if (key == "damage") stats.damage = static_cast<int>(stats.damage * mult);
        else if (key == "mana") stats.maxMana *= mult;
    }

    return stats;
}

bool HeroArchetype::Validate() const { return GetValidationErrors().empty(); }

std::vector<std::string> HeroArchetype::GetValidationErrors() const {
    std::vector<std::string> errors;
    if (id.empty()) errors.push_back("Hero archetype ID required");
    if (name.empty()) errors.push_back("Hero archetype name required");
    if (abilities.empty()) errors.push_back("Hero needs at least one ability");
    return errors;
}

nlohmann::json HeroArchetype::ToJson() const {
    nlohmann::json abilitiesJson = nlohmann::json::array();
    for (const auto& a : abilities) abilitiesJson.push_back(a.ToJson());

    return {
        {"id", id}, {"name", name}, {"title", title}, {"description", description},
        {"lore", lore}, {"iconPath", iconPath}, {"portraitPath", portraitPath},
        {"heroClass", HeroClassToString(heroClass)},
        {"subclass", HeroSubclassToString(subclass)},
        {"baseStats", baseStats.ToJson()}, {"goldCost", goldCost},
        {"reviveTime", reviveTime}, {"reviveCost", reviveCost},
        {"requiredBuilding", requiredBuilding}, {"requiredTech", requiredTech},
        {"requiredAge", requiredAge}, {"abilities", abilitiesJson},
        {"ultimateAbilityId", ultimateAbilityId}, {"passiveAuraId", passiveAuraId},
        {"auraRadius", auraRadius}, {"attackType", attackType}, {"damageType", damageType},
        {"projectileId", projectileId}, {"inventorySlots", inventorySlots},
        {"canUseItems", canUseItems}, {"preferredItems", preferredItems},
        {"canRevive", canRevive}, {"isUnique", isUnique}, {"isSummoned", isSummoned},
        {"modelPath", modelPath}, {"animationSet", animationSet}, {"modelScale", modelScale},
        {"pointCost", pointCost}, {"powerRating", powerRating}, {"tags", tags}
    };
}

HeroArchetype HeroArchetype::FromJson(const nlohmann::json& j) {
    HeroArchetype h;
    if (j.contains("id")) h.id = j["id"].get<std::string>();
    if (j.contains("name")) h.name = j["name"].get<std::string>();
    if (j.contains("title")) h.title = j["title"].get<std::string>();
    if (j.contains("description")) h.description = j["description"].get<std::string>();
    if (j.contains("lore")) h.lore = j["lore"].get<std::string>();
    if (j.contains("iconPath")) h.iconPath = j["iconPath"].get<std::string>();
    if (j.contains("portraitPath")) h.portraitPath = j["portraitPath"].get<std::string>();

    if (j.contains("heroClass")) {
        std::string s = j["heroClass"].get<std::string>();
        if (s == "Warrior") h.heroClass = HeroClass::Warrior;
        else if (s == "Mage") h.heroClass = HeroClass::Mage;
        else if (s == "Ranger") h.heroClass = HeroClass::Ranger;
        else if (s == "Support") h.heroClass = HeroClass::Support;
        else if (s == "Specialist") h.heroClass = HeroClass::Specialist;
    }
    if (j.contains("subclass")) h.subclass = StringToHeroSubclass(j["subclass"].get<std::string>());
    if (j.contains("baseStats")) h.baseStats = HeroBaseStats::FromJson(j["baseStats"]);
    if (j.contains("goldCost")) h.goldCost = j["goldCost"].get<int>();
    if (j.contains("reviveTime")) h.reviveTime = j["reviveTime"].get<float>();
    if (j.contains("reviveCost")) h.reviveCost = j["reviveCost"].get<int>();
    if (j.contains("requiredBuilding")) h.requiredBuilding = j["requiredBuilding"].get<std::string>();
    if (j.contains("requiredTech")) h.requiredTech = j["requiredTech"].get<std::string>();
    if (j.contains("requiredAge")) h.requiredAge = j["requiredAge"].get<int>();

    if (j.contains("abilities") && j["abilities"].is_array()) {
        for (const auto& a : j["abilities"]) h.abilities.push_back(HeroAbility::FromJson(a));
    }

    if (j.contains("ultimateAbilityId")) h.ultimateAbilityId = j["ultimateAbilityId"].get<std::string>();
    if (j.contains("passiveAuraId")) h.passiveAuraId = j["passiveAuraId"].get<std::string>();
    if (j.contains("auraRadius")) h.auraRadius = j["auraRadius"].get<float>();
    if (j.contains("attackType")) h.attackType = j["attackType"].get<std::string>();
    if (j.contains("damageType")) h.damageType = j["damageType"].get<std::string>();
    if (j.contains("projectileId")) h.projectileId = j["projectileId"].get<std::string>();
    if (j.contains("inventorySlots")) h.inventorySlots = j["inventorySlots"].get<int>();
    if (j.contains("canUseItems")) h.canUseItems = j["canUseItems"].get<bool>();
    if (j.contains("preferredItems")) h.preferredItems = j["preferredItems"].get<std::vector<std::string>>();
    if (j.contains("canRevive")) h.canRevive = j["canRevive"].get<bool>();
    if (j.contains("isUnique")) h.isUnique = j["isUnique"].get<bool>();
    if (j.contains("isSummoned")) h.isSummoned = j["isSummoned"].get<bool>();
    if (j.contains("modelPath")) h.modelPath = j["modelPath"].get<std::string>();
    if (j.contains("animationSet")) h.animationSet = j["animationSet"].get<std::string>();
    if (j.contains("modelScale")) h.modelScale = j["modelScale"].get<float>();
    if (j.contains("pointCost")) h.pointCost = j["pointCost"].get<int>();
    if (j.contains("powerRating")) h.powerRating = j["powerRating"].get<float>();
    if (j.contains("tags")) h.tags = j["tags"].get<std::vector<std::string>>();

    return h;
}

bool HeroArchetype::SaveToFile(const std::string& filepath) const {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;
    f << ToJson().dump(2);
    return true;
}

bool HeroArchetype::LoadFromFile(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) return false;
    try { nlohmann::json j; f >> j; *this = FromJson(j); return true; }
    catch (...) { return false; }
}

// Registry
HeroArchetypeRegistry& HeroArchetypeRegistry::Instance() {
    static HeroArchetypeRegistry instance;
    return instance;
}

bool HeroArchetypeRegistry::Initialize() {
    if (m_initialized) return true;
    InitializeBuiltInArchetypes();
    m_initialized = true;
    return true;
}

void HeroArchetypeRegistry::Shutdown() { m_archetypes.clear(); m_initialized = false; }

bool HeroArchetypeRegistry::RegisterArchetype(const HeroArchetype& a) {
    if (a.id.empty()) return false;
    m_archetypes[a.id] = a;
    return true;
}

const HeroArchetype* HeroArchetypeRegistry::GetArchetype(const std::string& id) const {
    auto it = m_archetypes.find(id);
    return (it != m_archetypes.end()) ? &it->second : nullptr;
}

std::vector<const HeroArchetype*> HeroArchetypeRegistry::GetAllArchetypes() const {
    std::vector<const HeroArchetype*> r;
    for (const auto& [id, a] : m_archetypes) r.push_back(&a);
    return r;
}

std::vector<const HeroArchetype*> HeroArchetypeRegistry::GetByClass(HeroClass c) const {
    std::vector<const HeroArchetype*> r;
    for (const auto& [id, a] : m_archetypes) if (a.heroClass == c) r.push_back(&a);
    return r;
}

int HeroArchetypeRegistry::LoadFromDirectory(const std::string& dir) {
    int count = 0;
    try {
        for (const auto& e : std::filesystem::directory_iterator(dir)) {
            if (e.path().extension() == ".json") {
                HeroArchetype a;
                if (a.LoadFromFile(e.path().string()) && RegisterArchetype(a)) ++count;
            }
        }
    } catch (...) {}
    return count;
}

void HeroArchetypeRegistry::InitializeBuiltInArchetypes() {
    RegisterArchetype(CreateWarriorTankArchetype());
    RegisterArchetype(CreateMageArchmageArchetype());
    RegisterArchetype(CreateRangerScoutArchetype());
    RegisterArchetype(CreateSupportHealerArchetype());
}

// Built-in Archetypes
HeroArchetype CreateWarriorTankArchetype() {
    HeroArchetype h;
    h.id = "hero_warrior_tank";
    h.name = "Guardian";
    h.title = "The Unbreakable";
    h.description = "A heavily armored warrior who protects allies.";
    h.heroClass = HeroClass::Warrior;
    h.subclass = HeroSubclass::Tank;

    h.baseStats.health = 800;
    h.baseStats.maxHealth = 800;
    h.baseStats.mana = 50;
    h.baseStats.maxMana = 50;
    h.baseStats.armor = 10;
    h.baseStats.damage = 30;
    h.baseStats.attackSpeed = 0.8f;
    h.baseStats.moveSpeed = 3.5f;
    h.baseStats.strength = 30;
    h.baseStats.agility = 15;
    h.baseStats.intelligence = 10;
    h.baseStats.healthPerLevel = 100.0f;
    h.baseStats.strengthPerLevel = 4;

    h.goldCost = 500;
    h.requiredBuilding = "altar";

    h.abilities = {
        {"ability_taunt", "Taunt", "Forces enemies to attack this hero", 1, 3, 15.0f, 30.0f, false, false},
        {"ability_shield_wall", "Shield Wall", "Reduces damage taken", 1, 3, 20.0f, 40.0f, false, false},
        {"ability_fortify", "Fortify", "Increases armor of nearby allies", 3, 3, 30.0f, 50.0f, false, false}
    };
    h.ultimateAbilityId = "ability_avatar";

    h.attackType = "melee";
    h.damageType = "physical";
    h.pointCost = 15;
    h.powerRating = 3.0f;
    h.tags = {"tank", "frontline", "protector"};

    return h;
}

HeroArchetype CreateWarriorBerserkerArchetype() {
    HeroArchetype h = CreateWarriorTankArchetype();
    h.id = "hero_warrior_berserker";
    h.name = "Berserker";
    h.title = "The Furious";
    h.description = "A rage-fueled warrior dealing massive damage.";
    h.subclass = HeroSubclass::Berserker;

    h.baseStats.health = 600;
    h.baseStats.armor = 3;
    h.baseStats.damage = 50;
    h.baseStats.attackSpeed = 1.2f;
    h.baseStats.strength = 25;
    h.baseStats.agility = 25;
    h.baseStats.damagePerLevel = 5.0f;

    h.abilities = {
        {"ability_frenzy", "Frenzy", "Increases attack speed", 1, 3, 20.0f, 40.0f, false, false},
        {"ability_cleave", "Cleave", "Damages multiple enemies", 1, 3, 8.0f, 20.0f, false, false},
        {"ability_bloodlust", "Bloodlust", "Lifesteal attacks", 3, 3, 25.0f, 60.0f, false, false}
    };
    h.ultimateAbilityId = "ability_rampage";
    h.tags = {"damage", "aggressive", "melee"};

    return h;
}

HeroArchetype CreateWarriorPaladinArchetype() {
    HeroArchetype h = CreateWarriorTankArchetype();
    h.id = "hero_warrior_paladin";
    h.name = "Paladin";
    h.title = "The Righteous";
    h.description = "A holy warrior combining defense and healing.";
    h.subclass = HeroSubclass::Paladin;

    h.baseStats.health = 700;
    h.baseStats.mana = 100;
    h.baseStats.maxMana = 100;
    h.baseStats.armor = 8;
    h.baseStats.damage = 35;
    h.baseStats.intelligence = 20;

    h.abilities = {
        {"ability_holy_light", "Holy Light", "Heals an ally", 1, 3, 10.0f, 35.0f, false, false},
        {"ability_divine_shield", "Divine Shield", "Invulnerability", 3, 3, 60.0f, 100.0f, false, false},
        {"ability_devotion_aura", "Devotion Aura", "Armor aura", 1, 3, 0.0f, 0.0f, true, false}
    };
    h.ultimateAbilityId = "ability_resurrection";
    h.passiveAuraId = "aura_devotion";
    h.auraRadius = 10.0f;
    h.tags = {"tank", "healer", "support"};

    return h;
}

HeroArchetype CreateMageArchmageArchetype() {
    HeroArchetype h;
    h.id = "hero_mage_archmage";
    h.name = "Archmage";
    h.title = "Master of the Arcane";
    h.description = "A powerful spellcaster with devastating AoE damage.";
    h.heroClass = HeroClass::Mage;
    h.subclass = HeroSubclass::Archmage;

    h.baseStats.health = 400;
    h.baseStats.maxHealth = 400;
    h.baseStats.mana = 300;
    h.baseStats.maxMana = 300;
    h.baseStats.manaRegen = 2.0f;
    h.baseStats.armor = 2;
    h.baseStats.damage = 20;
    h.baseStats.attackSpeed = 0.7f;
    h.baseStats.attackRange = 8.0f;
    h.baseStats.moveSpeed = 3.5f;
    h.baseStats.strength = 10;
    h.baseStats.agility = 15;
    h.baseStats.intelligence = 35;
    h.baseStats.manaPerLevel = 40.0f;
    h.baseStats.intelligencePerLevel = 4;

    h.goldCost = 550;
    h.requiredBuilding = "altar";

    h.abilities = {
        {"ability_fireball", "Fireball", "Launches a fireball", 1, 3, 8.0f, 40.0f, false, false},
        {"ability_blizzard", "Blizzard", "Area ice damage", 1, 3, 15.0f, 80.0f, false, false},
        {"ability_brilliance_aura", "Brilliance Aura", "Mana regen aura", 3, 3, 0.0f, 0.0f, true, false}
    };
    h.ultimateAbilityId = "ability_meteor";

    h.attackType = "ranged";
    h.damageType = "magic";
    h.projectileId = "magic_bolt";
    h.passiveAuraId = "aura_brilliance";
    h.auraRadius = 10.0f;
    h.pointCost = 16;
    h.powerRating = 3.5f;
    h.tags = {"caster", "aoe_damage", "mage"};

    return h;
}

HeroArchetype CreateMageWarlockArchetype() {
    HeroArchetype h = CreateMageArchmageArchetype();
    h.id = "hero_mage_warlock";
    h.name = "Warlock";
    h.title = "Master of Shadows";
    h.description = "A dark caster dealing damage over time.";
    h.subclass = HeroSubclass::Warlock;

    h.abilities = {
        {"ability_shadow_bolt", "Shadow Bolt", "Dark damage", 1, 3, 6.0f, 30.0f, false, false},
        {"ability_curse", "Curse", "Weakens enemies", 1, 3, 12.0f, 50.0f, false, false},
        {"ability_drain_life", "Drain Life", "Lifesteal spell", 3, 3, 10.0f, 60.0f, false, false}
    };
    h.ultimateAbilityId = "ability_doom";
    h.tags = {"caster", "dot_damage", "dark"};

    return h;
}

HeroArchetype CreateMageSummonerArchetype() {
    HeroArchetype h = CreateMageArchmageArchetype();
    h.id = "hero_mage_summoner";
    h.name = "Summoner";
    h.title = "Lord of the Elements";
    h.description = "A mage who summons creatures to fight.";
    h.subclass = HeroSubclass::Summoner;

    h.abilities = {
        {"ability_summon_elemental", "Summon Elemental", "Summons a fire elemental", 1, 3, 30.0f, 100.0f, false, false},
        {"ability_summon_water", "Water Elemental", "Summons a water elemental", 3, 3, 30.0f, 100.0f, false, false},
        {"ability_empower_summon", "Empower", "Buffs summoned units", 1, 3, 15.0f, 40.0f, false, false}
    };
    h.ultimateAbilityId = "ability_summon_titan";
    h.tags = {"summoner", "caster", "army"};

    return h;
}

HeroArchetype CreateRangerScoutArchetype() {
    HeroArchetype h;
    h.id = "hero_ranger_scout";
    h.name = "Shadow Scout";
    h.title = "Eyes of the Army";
    h.description = "A fast, stealthy hero excelling at reconnaissance.";
    h.heroClass = HeroClass::Ranger;
    h.subclass = HeroSubclass::Scout;

    h.baseStats.health = 450;
    h.baseStats.maxHealth = 450;
    h.baseStats.mana = 100;
    h.baseStats.maxMana = 100;
    h.baseStats.armor = 3;
    h.baseStats.damage = 25;
    h.baseStats.attackSpeed = 1.3f;
    h.baseStats.attackRange = 7.0f;
    h.baseStats.moveSpeed = 5.5f;
    h.baseStats.visionRange = 14.0f;
    h.baseStats.strength = 15;
    h.baseStats.agility = 30;
    h.baseStats.intelligence = 15;
    h.baseStats.agilityPerLevel = 4;

    h.goldCost = 450;
    h.requiredBuilding = "altar";

    h.abilities = {
        {"ability_shadow_meld", "Shadow Meld", "Become invisible", 1, 3, 20.0f, 30.0f, false, false},
        {"ability_reveal", "Reveal", "Reveals area", 1, 3, 15.0f, 25.0f, false, false},
        {"ability_evasion", "Evasion", "Dodge chance", 3, 3, 0.0f, 0.0f, true, false}
    };
    h.ultimateAbilityId = "ability_assassinate";

    h.attackType = "ranged";
    h.damageType = "physical";
    h.projectileId = "arrow_basic";
    h.pointCost = 14;
    h.powerRating = 2.5f;
    h.tags = {"scout", "stealth", "fast"};

    return h;
}

HeroArchetype CreateRangerSniperArchetype() {
    HeroArchetype h = CreateRangerScoutArchetype();
    h.id = "hero_ranger_sniper";
    h.name = "Sharpshooter";
    h.title = "The Deadeye";
    h.description = "Long-range specialist with critical hits.";
    h.subclass = HeroSubclass::Sniper;

    h.baseStats.attackRange = 12.0f;
    h.baseStats.damage = 40;
    h.baseStats.attackSpeed = 0.8f;

    h.abilities = {
        {"ability_aimed_shot", "Aimed Shot", "High damage shot", 1, 3, 10.0f, 35.0f, false, false},
        {"ability_critical_strike", "Critical Strike", "Passive crit chance", 1, 3, 0.0f, 0.0f, true, false},
        {"ability_trueshot_aura", "Trueshot Aura", "Ranged damage aura", 3, 3, 0.0f, 0.0f, true, false}
    };
    h.ultimateAbilityId = "ability_headshot";
    h.tags = {"ranged", "sniper", "critical"};

    return h;
}

HeroArchetype CreateRangerBeastmasterArchetype() {
    HeroArchetype h = CreateRangerScoutArchetype();
    h.id = "hero_ranger_beastmaster";
    h.name = "Beastmaster";
    h.title = "Lord of the Wild";
    h.description = "Commands beasts to fight alongside.";
    h.subclass = HeroSubclass::Beastmaster;

    h.abilities = {
        {"ability_summon_bear", "Summon Bear", "Summons a bear", 1, 3, 30.0f, 75.0f, false, false},
        {"ability_summon_hawk", "Summon Hawk", "Summons a hawk scout", 1, 3, 20.0f, 50.0f, false, false},
        {"ability_roar", "Roar", "Buffs allied beasts", 3, 3, 15.0f, 40.0f, false, false}
    };
    h.ultimateAbilityId = "ability_stampede";
    h.tags = {"summoner", "beasts", "nature"};

    return h;
}

HeroArchetype CreateSupportHealerArchetype() {
    HeroArchetype h;
    h.id = "hero_support_healer";
    h.name = "High Priest";
    h.title = "The Lightbringer";
    h.description = "Dedicated healer keeping the army alive.";
    h.heroClass = HeroClass::Support;
    h.subclass = HeroSubclass::Healer;

    h.baseStats.health = 350;
    h.baseStats.maxHealth = 350;
    h.baseStats.mana = 250;
    h.baseStats.maxMana = 250;
    h.baseStats.manaRegen = 2.5f;
    h.baseStats.armor = 2;
    h.baseStats.damage = 15;
    h.baseStats.attackSpeed = 0.8f;
    h.baseStats.attackRange = 6.0f;
    h.baseStats.moveSpeed = 4.0f;
    h.baseStats.strength = 12;
    h.baseStats.agility = 12;
    h.baseStats.intelligence = 30;
    h.baseStats.intelligencePerLevel = 4;

    h.goldCost = 500;
    h.requiredBuilding = "altar";

    h.abilities = {
        {"ability_heal", "Heal", "Heals single target", 1, 3, 6.0f, 25.0f, false, false},
        {"ability_mass_heal", "Mass Heal", "Heals area", 3, 3, 15.0f, 75.0f, false, false},
        {"ability_inner_fire", "Inner Fire", "Buffs ally", 1, 3, 10.0f, 30.0f, false, false}
    };
    h.ultimateAbilityId = "ability_resurrection";

    h.attackType = "ranged";
    h.damageType = "magic";
    h.pointCost = 14;
    h.powerRating = 3.0f;
    h.tags = {"healer", "support", "backline"};

    return h;
}

HeroArchetype CreateSupportBufferArchetype() {
    HeroArchetype h = CreateSupportHealerArchetype();
    h.id = "hero_support_buffer";
    h.name = "Battle Standard";
    h.title = "The Inspirer";
    h.description = "Buffs allies with powerful enhancements.";
    h.subclass = HeroSubclass::Buffer;

    h.abilities = {
        {"ability_battle_cry", "Battle Cry", "Attack speed buff", 1, 3, 15.0f, 50.0f, false, false},
        {"ability_bloodlust", "Bloodlust", "Damage buff", 1, 3, 12.0f, 40.0f, false, false},
        {"ability_endurance_aura", "Endurance Aura", "Move speed aura", 3, 3, 0.0f, 0.0f, true, false}
    };
    h.ultimateAbilityId = "ability_heroism";
    h.tags = {"buffer", "support", "aura"};

    return h;
}

HeroArchetype CreateSupportAuraArchetype() {
    HeroArchetype h = CreateSupportHealerArchetype();
    h.id = "hero_support_aura";
    h.name = "Aura Master";
    h.title = "The Radiating One";
    h.description = "Provides powerful passive auras.";
    h.subclass = HeroSubclass::Aura;

    h.abilities = {
        {"ability_command_aura", "Command Aura", "Damage aura", 1, 3, 0.0f, 0.0f, true, false},
        {"ability_devotion_aura", "Devotion Aura", "Armor aura", 1, 3, 0.0f, 0.0f, true, false},
        {"ability_vampiric_aura", "Vampiric Aura", "Lifesteal aura", 3, 3, 0.0f, 0.0f, true, false}
    };
    h.ultimateAbilityId = "ability_ultimate_aura";
    h.auraRadius = 15.0f;
    h.tags = {"aura", "support", "passive"};

    return h;
}

HeroArchetype CreateSpecialistSiegeArchetype() {
    HeroArchetype h;
    h.id = "hero_specialist_siege";
    h.name = "Siege Master";
    h.title = "Breaker of Walls";
    h.description = "Expert at destroying buildings.";
    h.heroClass = HeroClass::Specialist;
    h.subclass = HeroSubclass::SiegeMaster;

    h.baseStats.health = 600;
    h.baseStats.maxHealth = 600;
    h.baseStats.mana = 150;
    h.baseStats.maxMana = 150;
    h.baseStats.armor = 5;
    h.baseStats.damage = 60;
    h.baseStats.attackSpeed = 0.5f;
    h.baseStats.attackRange = 10.0f;
    h.baseStats.moveSpeed = 3.0f;
    h.baseStats.strength = 25;
    h.baseStats.agility = 10;
    h.baseStats.intelligence = 20;

    h.goldCost = 550;
    h.requiredBuilding = "altar";
    h.requiredAge = 2;

    h.abilities = {
        {"ability_demolish", "Demolish", "Bonus building damage", 1, 3, 0.0f, 0.0f, true, false},
        {"ability_artillery", "Artillery Strike", "Ranged AoE", 1, 3, 20.0f, 60.0f, false, false},
        {"ability_fortify_siege", "Fortify", "Buffs siege units", 3, 3, 25.0f, 50.0f, false, false}
    };
    h.ultimateAbilityId = "ability_earthquake";

    h.attackType = "ranged";
    h.damageType = "siege";
    h.pointCost = 15;
    h.powerRating = 2.5f;
    h.tags = {"siege", "building_destroyer", "specialist"};

    return h;
}

HeroArchetype CreateSpecialistAssassinArchetype() {
    HeroArchetype h;
    h.id = "hero_specialist_assassin";
    h.name = "Shadow Blade";
    h.title = "The Silent Death";
    h.description = "Stealthy killer targeting key enemies.";
    h.heroClass = HeroClass::Specialist;
    h.subclass = HeroSubclass::Assassin;

    h.baseStats.health = 400;
    h.baseStats.maxHealth = 400;
    h.baseStats.mana = 150;
    h.baseStats.maxMana = 150;
    h.baseStats.armor = 2;
    h.baseStats.damage = 45;
    h.baseStats.attackSpeed = 1.5f;
    h.baseStats.moveSpeed = 5.0f;
    h.baseStats.strength = 15;
    h.baseStats.agility = 35;
    h.baseStats.intelligence = 10;
    h.baseStats.agilityPerLevel = 5;

    h.goldCost = 500;
    h.requiredBuilding = "altar";

    h.abilities = {
        {"ability_backstab", "Backstab", "Bonus from behind", 1, 3, 0.0f, 0.0f, true, false},
        {"ability_shadow_strike", "Shadow Strike", "Teleport attack", 1, 3, 12.0f, 50.0f, false, false},
        {"ability_smoke_bomb", "Smoke Bomb", "AoE stealth", 3, 3, 30.0f, 75.0f, false, false}
    };
    h.ultimateAbilityId = "ability_death_mark";

    h.attackType = "melee";
    h.damageType = "physical";
    h.pointCost = 16;
    h.powerRating = 3.0f;
    h.tags = {"assassin", "stealth", "burst_damage"};

    return h;
}

HeroArchetype CreateSpecialistNecromancerArchetype() {
    HeroArchetype h;
    h.id = "hero_specialist_necromancer";
    h.name = "Lich King";
    h.title = "Master of the Dead";
    h.description = "Raises undead armies from fallen enemies.";
    h.heroClass = HeroClass::Specialist;
    h.subclass = HeroSubclass::Necromancer;

    h.baseStats.health = 450;
    h.baseStats.maxHealth = 450;
    h.baseStats.mana = 350;
    h.baseStats.maxMana = 350;
    h.baseStats.manaRegen = 2.5f;
    h.baseStats.armor = 3;
    h.baseStats.damage = 25;
    h.baseStats.attackSpeed = 0.7f;
    h.baseStats.attackRange = 7.0f;
    h.baseStats.moveSpeed = 3.5f;
    h.baseStats.strength = 12;
    h.baseStats.agility = 12;
    h.baseStats.intelligence = 35;

    h.goldCost = 600;
    h.requiredBuilding = "altar";
    h.requiredAge = 3;

    h.abilities = {
        {"ability_raise_dead", "Raise Dead", "Summons skeletons from corpses", 1, 3, 15.0f, 75.0f, false, false},
        {"ability_death_coil", "Death Coil", "Damage or heal undead", 1, 3, 8.0f, 40.0f, false, false},
        {"ability_unholy_aura", "Unholy Aura", "Buffs undead", 3, 3, 0.0f, 0.0f, true, false}
    };
    h.ultimateAbilityId = "ability_army_of_dead";

    h.attackType = "ranged";
    h.damageType = "magic";
    h.passiveAuraId = "aura_unholy";
    h.auraRadius = 12.0f;
    h.pointCost = 18;
    h.powerRating = 4.0f;
    h.tags = {"necromancer", "summoner", "dark"};

    return h;
}

} // namespace Race
} // namespace RTS
} // namespace Vehement
