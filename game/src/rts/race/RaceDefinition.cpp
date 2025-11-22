#include "RaceDefinition.hpp"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <chrono>

namespace Vehement {
namespace RTS {
namespace Race {

// ============================================================================
// CampaignInfo Implementation
// ============================================================================

nlohmann::json CampaignInfo::ToJson() const {
    return {
        {"campaignId", campaignId},
        {"storyDescription", storyDescription},
        {"homeworld", homeworld},
        {"allies", allies},
        {"enemies", enemies},
        {"campaignMissionCount", campaignMissionCount},
        {"difficulty", difficulty},
        {"voicePackId", voicePackId},
        {"dialogueSetId", dialogueSetId}
    };
}

CampaignInfo CampaignInfo::FromJson(const nlohmann::json& j) {
    CampaignInfo info;
    if (j.contains("campaignId")) info.campaignId = j["campaignId"].get<std::string>();
    if (j.contains("storyDescription")) info.storyDescription = j["storyDescription"].get<std::string>();
    if (j.contains("homeworld")) info.homeworld = j["homeworld"].get<std::string>();
    if (j.contains("allies")) info.allies = j["allies"].get<std::vector<std::string>>();
    if (j.contains("enemies")) info.enemies = j["enemies"].get<std::vector<std::string>>();
    if (j.contains("campaignMissionCount")) info.campaignMissionCount = j["campaignMissionCount"].get<int>();
    if (j.contains("difficulty")) info.difficulty = j["difficulty"].get<int>();
    if (j.contains("voicePackId")) info.voicePackId = j["voicePackId"].get<std::string>();
    if (j.contains("dialogueSetId")) info.dialogueSetId = j["dialogueSetId"].get<std::string>();
    return info;
}

// ============================================================================
// RaceVisualStyle Implementation
// ============================================================================

nlohmann::json RaceVisualStyle::ToJson() const {
    return {
        {"iconPath", iconPath},
        {"bannerPath", bannerPath},
        {"portraitPath", portraitPath},
        {"backgroundPath", backgroundPath},
        {"primaryColor", primaryColor},
        {"secondaryColor", secondaryColor},
        {"accentColor", accentColor},
        {"uiSkinId", uiSkinId},
        {"minimapColor", minimapColor},
        {"musicTheme", musicTheme},
        {"ambientSound", ambientSound}
    };
}

RaceVisualStyle RaceVisualStyle::FromJson(const nlohmann::json& j) {
    RaceVisualStyle style;
    if (j.contains("iconPath")) style.iconPath = j["iconPath"].get<std::string>();
    if (j.contains("bannerPath")) style.bannerPath = j["bannerPath"].get<std::string>();
    if (j.contains("portraitPath")) style.portraitPath = j["portraitPath"].get<std::string>();
    if (j.contains("backgroundPath")) style.backgroundPath = j["backgroundPath"].get<std::string>();
    if (j.contains("primaryColor")) style.primaryColor = j["primaryColor"].get<std::string>();
    if (j.contains("secondaryColor")) style.secondaryColor = j["secondaryColor"].get<std::string>();
    if (j.contains("accentColor")) style.accentColor = j["accentColor"].get<std::string>();
    if (j.contains("uiSkinId")) style.uiSkinId = j["uiSkinId"].get<std::string>();
    if (j.contains("minimapColor")) style.minimapColor = j["minimapColor"].get<std::string>();
    if (j.contains("musicTheme")) style.musicTheme = j["musicTheme"].get<std::string>();
    if (j.contains("ambientSound")) style.ambientSound = j["ambientSound"].get<std::string>();
    return style;
}

// ============================================================================
// StartingConfig Implementation
// ============================================================================

nlohmann::json StartingConfig::ToJson() const {
    nlohmann::json unitsJson = nlohmann::json::array();
    for (const auto& [unitId, count] : startingUnits) {
        unitsJson.push_back({{"unitId", unitId}, {"count", count}});
    }

    return {
        {"startingGold", startingGold},
        {"startingWood", startingWood},
        {"startingStone", startingStone},
        {"startingFood", startingFood},
        {"startingMetal", startingMetal},
        {"startingUnits", unitsJson},
        {"startingBuildings", startingBuildings},
        {"startingTechs", startingTechs},
        {"startingAge", AgeToShortString(startingAge)},
        {"startingPopCap", startingPopCap},
        {"startingPopulation", startingPopulation}
    };
}

StartingConfig StartingConfig::FromJson(const nlohmann::json& j) {
    StartingConfig config;
    if (j.contains("startingGold")) config.startingGold = j["startingGold"].get<int>();
    if (j.contains("startingWood")) config.startingWood = j["startingWood"].get<int>();
    if (j.contains("startingStone")) config.startingStone = j["startingStone"].get<int>();
    if (j.contains("startingFood")) config.startingFood = j["startingFood"].get<int>();
    if (j.contains("startingMetal")) config.startingMetal = j["startingMetal"].get<int>();

    if (j.contains("startingUnits") && j["startingUnits"].is_array()) {
        for (const auto& unit : j["startingUnits"]) {
            std::string unitId = unit["unitId"].get<std::string>();
            int count = unit["count"].get<int>();
            config.startingUnits.emplace_back(unitId, count);
        }
    }

    if (j.contains("startingBuildings")) {
        config.startingBuildings = j["startingBuildings"].get<std::vector<std::string>>();
    }
    if (j.contains("startingTechs")) {
        config.startingTechs = j["startingTechs"].get<std::vector<std::string>>();
    }
    if (j.contains("startingAge")) {
        config.startingAge = StringToAge(j["startingAge"].get<std::string>());
    }
    if (j.contains("startingPopCap")) config.startingPopCap = j["startingPopCap"].get<int>();
    if (j.contains("startingPopulation")) config.startingPopulation = j["startingPopulation"].get<int>();

    return config;
}

// ============================================================================
// RaceRestrictions Implementation
// ============================================================================

nlohmann::json RaceRestrictions::ToJson() const {
    return {
        {"forbiddenBuildings", forbiddenBuildings},
        {"forbiddenUnits", forbiddenUnits},
        {"forbiddenTechs", forbiddenTechs},
        {"canGatherWood", canGatherWood},
        {"canGatherStone", canGatherStone},
        {"canGatherGold", canGatherGold},
        {"canGatherFood", canGatherFood},
        {"canGatherMetal", canGatherMetal},
        {"canTrade", canTrade},
        {"canAlly", canAlly},
        {"maxHeroes", maxHeroes},
        {"maxAge", maxAge}
    };
}

RaceRestrictions RaceRestrictions::FromJson(const nlohmann::json& j) {
    RaceRestrictions restrictions;
    if (j.contains("forbiddenBuildings")) {
        restrictions.forbiddenBuildings = j["forbiddenBuildings"].get<std::vector<std::string>>();
    }
    if (j.contains("forbiddenUnits")) {
        restrictions.forbiddenUnits = j["forbiddenUnits"].get<std::vector<std::string>>();
    }
    if (j.contains("forbiddenTechs")) {
        restrictions.forbiddenTechs = j["forbiddenTechs"].get<std::vector<std::string>>();
    }
    if (j.contains("canGatherWood")) restrictions.canGatherWood = j["canGatherWood"].get<bool>();
    if (j.contains("canGatherStone")) restrictions.canGatherStone = j["canGatherStone"].get<bool>();
    if (j.contains("canGatherGold")) restrictions.canGatherGold = j["canGatherGold"].get<bool>();
    if (j.contains("canGatherFood")) restrictions.canGatherFood = j["canGatherFood"].get<bool>();
    if (j.contains("canGatherMetal")) restrictions.canGatherMetal = j["canGatherMetal"].get<bool>();
    if (j.contains("canTrade")) restrictions.canTrade = j["canTrade"].get<bool>();
    if (j.contains("canAlly")) restrictions.canAlly = j["canAlly"].get<bool>();
    if (j.contains("maxHeroes")) restrictions.maxHeroes = j["maxHeroes"].get<int>();
    if (j.contains("maxAge")) restrictions.maxAge = j["maxAge"].get<int>();
    return restrictions;
}

// ============================================================================
// RaceDefinition Implementation
// ============================================================================

bool RaceDefinition::Validate() const {
    return GetValidationErrors().empty();
}

std::vector<std::string> RaceDefinition::GetValidationErrors() const {
    std::vector<std::string> errors;

    // Check required fields
    if (id.empty()) {
        errors.push_back("Race ID is required");
    }
    if (name.empty()) {
        errors.push_back("Race name is required");
    }

    // Check point allocation
    if (!allocation.Validate()) {
        errors.push_back("Point allocation is invalid: " + allocation.GetValidationError());
    }

    // Check archetypes
    if (unitArchetypes.empty()) {
        errors.push_back("At least one unit archetype is required");
    }
    if (buildingArchetypes.empty()) {
        errors.push_back("At least one building archetype is required");
    }

    // Check for essential archetypes
    bool hasWorker = std::find(unitArchetypes.begin(), unitArchetypes.end(), "worker") != unitArchetypes.end();
    if (!hasWorker) {
        errors.push_back("Worker unit archetype is required");
    }

    bool hasMainHall = std::find(buildingArchetypes.begin(), buildingArchetypes.end(), "main_hall") != buildingArchetypes.end();
    if (!hasMainHall) {
        errors.push_back("Main hall building archetype is required");
    }

    // Check balance
    BalanceScore score = allocation.CalculateBalanceScore();
    if (score.HasCriticalWarnings()) {
        for (const auto& warning : score.warnings) {
            if (warning.severity == BalanceWarningType::Critical) {
                errors.push_back("Balance warning: " + warning.message);
            }
        }
    }

    return errors;
}

float RaceDefinition::CalculatePowerLevel() const {
    return BalanceCalculator::Instance().CalculatePowerLevel(allocation);
}

BalanceScore RaceDefinition::GetBalanceScore() const {
    return allocation.CalculateBalanceScore();
}

bool RaceDefinition::HasUnitArchetype(const std::string& archetypeId) const {
    return std::find(unitArchetypes.begin(), unitArchetypes.end(), archetypeId) != unitArchetypes.end();
}

bool RaceDefinition::HasBuildingArchetype(const std::string& archetypeId) const {
    return std::find(buildingArchetypes.begin(), buildingArchetypes.end(), archetypeId) != buildingArchetypes.end();
}

bool RaceDefinition::HasHeroArchetype(const std::string& archetypeId) const {
    return std::find(heroArchetypes.begin(), heroArchetypes.end(), archetypeId) != heroArchetypes.end();
}

bool RaceDefinition::HasSpellArchetype(const std::string& archetypeId) const {
    return std::find(spellArchetypes.begin(), spellArchetypes.end(), archetypeId) != spellArchetypes.end();
}

float RaceDefinition::GetStatModifier(const std::string& statName) const {
    auto it = statModifiers.find(statName);
    if (it != statModifiers.end()) {
        return it->second;
    }

    // Check allocation bonuses
    float bonus = allocation.GetBonus(statName);
    return 1.0f + bonus;  // Convert bonus to multiplier
}

void RaceDefinition::ApplyAllocationBonuses() {
    auto bonuses = allocation.GetAllBonuses();
    for (const auto& [name, value] : bonuses) {
        if (statModifiers.find(name) == statModifiers.end()) {
            statModifiers[name] = 1.0f + value;
        }
    }
}

nlohmann::json RaceDefinition::ToJson() const {
    return {
        {"id", id},
        {"name", name},
        {"shortName", shortName},
        {"description", description},
        {"theme", RaceThemeToString(theme)},
        {"totalPoints", totalPoints},
        {"allocation", allocation.ToJson()},
        {"unitArchetypes", unitArchetypes},
        {"buildingArchetypes", buildingArchetypes},
        {"heroArchetypes", heroArchetypes},
        {"spellArchetypes", spellArchetypes},
        {"bonusIds", bonusIds},
        {"statModifiers", statModifiers},
        {"techTreeId", techTreeId},
        {"uniqueTechs", uniqueTechs},
        {"campaign", campaign.ToJson()},
        {"visualStyle", visualStyle.ToJson()},
        {"startingConfig", startingConfig.ToJson()},
        {"restrictions", restrictions.ToJson()},
        {"author", author},
        {"version", version},
        {"createdTimestamp", createdTimestamp},
        {"modifiedTimestamp", modifiedTimestamp},
        {"isBuiltIn", isBuiltIn},
        {"isEnabled", isEnabled}
    };
}

RaceDefinition RaceDefinition::FromJson(const nlohmann::json& j) {
    RaceDefinition race;

    if (j.contains("id")) race.id = j["id"].get<std::string>();
    if (j.contains("name")) race.name = j["name"].get<std::string>();
    if (j.contains("shortName")) race.shortName = j["shortName"].get<std::string>();
    if (j.contains("description")) race.description = j["description"].get<std::string>();
    if (j.contains("theme")) race.theme = StringToRaceTheme(j["theme"].get<std::string>());
    if (j.contains("totalPoints")) race.totalPoints = j["totalPoints"].get<int>();
    if (j.contains("allocation")) race.allocation = PointAllocation::FromJson(j["allocation"]);

    if (j.contains("unitArchetypes")) {
        race.unitArchetypes = j["unitArchetypes"].get<std::vector<std::string>>();
    }
    if (j.contains("buildingArchetypes")) {
        race.buildingArchetypes = j["buildingArchetypes"].get<std::vector<std::string>>();
    }
    if (j.contains("heroArchetypes")) {
        race.heroArchetypes = j["heroArchetypes"].get<std::vector<std::string>>();
    }
    if (j.contains("spellArchetypes")) {
        race.spellArchetypes = j["spellArchetypes"].get<std::vector<std::string>>();
    }
    if (j.contains("bonusIds")) {
        race.bonusIds = j["bonusIds"].get<std::vector<std::string>>();
    }
    if (j.contains("statModifiers")) {
        race.statModifiers = j["statModifiers"].get<std::map<std::string, float>>();
    }
    if (j.contains("techTreeId")) race.techTreeId = j["techTreeId"].get<std::string>();
    if (j.contains("uniqueTechs")) {
        race.uniqueTechs = j["uniqueTechs"].get<std::vector<std::string>>();
    }
    if (j.contains("campaign")) race.campaign = CampaignInfo::FromJson(j["campaign"]);
    if (j.contains("visualStyle")) race.visualStyle = RaceVisualStyle::FromJson(j["visualStyle"]);
    if (j.contains("startingConfig")) race.startingConfig = StartingConfig::FromJson(j["startingConfig"]);
    if (j.contains("restrictions")) race.restrictions = RaceRestrictions::FromJson(j["restrictions"]);

    if (j.contains("author")) race.author = j["author"].get<std::string>();
    if (j.contains("version")) race.version = j["version"].get<std::string>();
    if (j.contains("createdTimestamp")) race.createdTimestamp = j["createdTimestamp"].get<int64_t>();
    if (j.contains("modifiedTimestamp")) race.modifiedTimestamp = j["modifiedTimestamp"].get<int64_t>();
    if (j.contains("isBuiltIn")) race.isBuiltIn = j["isBuiltIn"].get<bool>();
    if (j.contains("isEnabled")) race.isEnabled = j["isEnabled"].get<bool>();

    return race;
}

bool RaceDefinition::SaveToFile(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    file << ToJson().dump(2);
    return true;
}

bool RaceDefinition::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

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
// RaceRegistry Implementation
// ============================================================================

RaceRegistry& RaceRegistry::Instance() {
    static RaceRegistry instance;
    return instance;
}

bool RaceRegistry::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Register built-in templates
    m_templates["human"] = CreateHumanRace();
    m_templates["orc"] = CreateOrcRace();
    m_templates["elf"] = CreateElfRace();
    m_templates["undead"] = CreateUndeadRace();
    m_templates["dwarf"] = CreateDwarfRace();
    m_templates["blank"] = CreateBlankRace();

    m_initialized = true;
    return true;
}

void RaceRegistry::Shutdown() {
    m_races.clear();
    m_templates.clear();
    m_initialized = false;
}

bool RaceRegistry::RegisterRace(const RaceDefinition& race) {
    if (race.id.empty()) {
        return false;
    }

    m_races[race.id] = race;
    return true;
}

bool RaceRegistry::UnregisterRace(const std::string& raceId) {
    return m_races.erase(raceId) > 0;
}

const RaceDefinition* RaceRegistry::GetRace(const std::string& raceId) const {
    auto it = m_races.find(raceId);
    return (it != m_races.end()) ? &it->second : nullptr;
}

std::vector<const RaceDefinition*> RaceRegistry::GetAllRaces() const {
    std::vector<const RaceDefinition*> races;
    for (const auto& [id, race] : m_races) {
        races.push_back(&race);
    }
    return races;
}

std::vector<const RaceDefinition*> RaceRegistry::GetRacesByTheme(RaceTheme theme) const {
    std::vector<const RaceDefinition*> races;
    for (const auto& [id, race] : m_races) {
        if (race.theme == theme) {
            races.push_back(&race);
        }
    }
    return races;
}

std::vector<const RaceDefinition*> RaceRegistry::GetEnabledRaces() const {
    std::vector<const RaceDefinition*> races;
    for (const auto& [id, race] : m_races) {
        if (race.isEnabled) {
            races.push_back(&race);
        }
    }
    return races;
}

int RaceRegistry::LoadRacesFromDirectory(const std::string& directory) {
    int count = 0;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.path().extension() == ".json") {
                RaceDefinition race;
                if (race.LoadFromFile(entry.path().string())) {
                    if (RegisterRace(race)) {
                        ++count;
                    }
                }
            }
        }
    } catch (...) {
        // Directory doesn't exist or other error
    }

    return count;
}

int RaceRegistry::SaveRacesToDirectory(const std::string& directory) const {
    int count = 0;

    try {
        std::filesystem::create_directories(directory);

        for (const auto& [id, race] : m_races) {
            std::string filepath = directory + "/" + id + ".json";
            if (race.SaveToFile(filepath)) {
                ++count;
            }
        }
    } catch (...) {
        // Error creating directory
    }

    return count;
}

RaceDefinition RaceRegistry::CreateFromTemplate(const std::string& templateName) const {
    auto it = m_templates.find(templateName);
    if (it != m_templates.end()) {
        RaceDefinition race = it->second;
        race.isBuiltIn = false;
        race.createdTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return race;
    }
    return CreateBlankRace();
}

std::vector<std::string> RaceRegistry::GetTemplateNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : m_templates) {
        names.push_back(name);
    }
    return names;
}

std::map<std::string, std::vector<std::string>> RaceRegistry::ValidateAllRaces() const {
    std::map<std::string, std::vector<std::string>> errors;
    for (const auto& [id, race] : m_races) {
        auto raceErrors = race.GetValidationErrors();
        if (!raceErrors.empty()) {
            errors[id] = raceErrors;
        }
    }
    return errors;
}

// ============================================================================
// Built-in Race Implementations
// ============================================================================

RaceDefinition CreateHumanRace() {
    RaceDefinition race;
    race.id = "humans";
    race.name = "Human Empire";
    race.shortName = "Humans";
    race.description = "Versatile and adaptable, humans excel at balanced gameplay with strong economy and diverse military options.";
    race.theme = RaceTheme::Fantasy;
    race.isBuiltIn = true;

    // Balanced allocation
    race.allocation = CreateBalancedPreset();

    // Unit archetypes
    race.unitArchetypes = {
        "worker", "infantry_melee", "infantry_pike", "infantry_shield",
        "ranged_archer", "ranged_crossbow", "cavalry_light", "cavalry_heavy",
        "siege_catapult", "siege_ram"
    };

    // Building archetypes
    race.buildingArchetypes = {
        "main_hall", "house", "barracks", "archery_range", "stable",
        "siege_workshop", "blacksmith", "market", "tower", "wall"
    };

    // Hero archetypes
    race.heroArchetypes = {
        "hero_warrior", "hero_mage", "hero_ranger", "hero_paladin"
    };

    // Spell archetypes
    race.spellArchetypes = {
        "spell_heal", "spell_buff_attack", "spell_summon_militia"
    };

    // Visual style
    race.visualStyle.primaryColor = "#1E90FF";
    race.visualStyle.secondaryColor = "#FFD700";
    race.visualStyle.accentColor = "#FFFFFF";

    // Starting config
    race.startingConfig.startingGold = 500;
    race.startingConfig.startingWood = 300;
    race.startingConfig.startingFood = 100;
    race.startingConfig.startingUnits = {{"worker", 5}};
    race.startingConfig.startingBuildings = {"main_hall"};

    // Campaign
    race.campaign.storyDescription = "Rise from a small kingdom to conquer the realm.";
    race.campaign.difficulty = 2;

    return race;
}

RaceDefinition CreateOrcRace() {
    RaceDefinition race;
    race.id = "orcs";
    race.name = "Orcish Horde";
    race.shortName = "Orcs";
    race.description = "Brutal warriors focused on military might. Strong melee units but weaker economy.";
    race.theme = RaceTheme::Fantasy;
    race.isBuiltIn = true;

    // Military-focused allocation
    race.allocation = CreateMilitaryPreset();

    // Unit archetypes
    race.unitArchetypes = {
        "worker", "infantry_melee", "infantry_berserker", "infantry_brute",
        "ranged_thrower", "cavalry_wolf_rider", "cavalry_boar_rider",
        "siege_catapult", "siege_tower"
    };

    // Building archetypes
    race.buildingArchetypes = {
        "main_hall", "burrow", "war_camp", "beast_pit",
        "forge", "pillage_camp", "spike_wall"
    };

    // Hero archetypes
    race.heroArchetypes = {
        "hero_warlord", "hero_shaman", "hero_berserker"
    };

    // Spell archetypes
    race.spellArchetypes = {
        "spell_bloodlust", "spell_war_drums", "spell_summon_wolves"
    };

    // Visual style
    race.visualStyle.primaryColor = "#228B22";
    race.visualStyle.secondaryColor = "#8B0000";
    race.visualStyle.accentColor = "#000000";

    // Starting config
    race.startingConfig.startingGold = 400;
    race.startingConfig.startingWood = 250;
    race.startingConfig.startingFood = 150;
    race.startingConfig.startingUnits = {{"worker", 4}, {"infantry_melee", 2}};
    race.startingConfig.startingBuildings = {"main_hall"};

    // Stat modifiers - stronger melee, weaker economy
    race.statModifiers["meleeDamage"] = 1.15f;
    race.statModifiers["harvestSpeed"] = 0.9f;

    return race;
}

RaceDefinition CreateElfRace() {
    RaceDefinition race;
    race.id = "elves";
    race.name = "Elven Kingdom";
    race.shortName = "Elves";
    race.description = "Ancient and magical, elves excel at ranged combat and spellcasting with weaker melee options.";
    race.theme = RaceTheme::Fantasy;
    race.isBuiltIn = true;

    // Magic-focused allocation
    race.allocation = CreateMagicPreset();

    // Unit archetypes
    race.unitArchetypes = {
        "worker", "infantry_blade_dancer", "infantry_sentinel",
        "ranged_archer", "ranged_marksman", "ranged_caster",
        "cavalry_unicorn", "cavalry_stag"
    };

    // Building archetypes
    race.buildingArchetypes = {
        "main_hall", "dwelling", "training_glade", "archery_pavilion",
        "arcane_sanctuary", "moonwell", "ancient_tree"
    };

    // Hero archetypes
    race.heroArchetypes = {
        "hero_archmage", "hero_ranger", "hero_druid", "hero_blade_master"
    };

    // Spell archetypes
    race.spellArchetypes = {
        "spell_starfall", "spell_entangle", "spell_nature_blessing", "spell_moonbeam"
    };

    // Visual style
    race.visualStyle.primaryColor = "#9370DB";
    race.visualStyle.secondaryColor = "#98FB98";
    race.visualStyle.accentColor = "#FFFAFA";

    // Starting config
    race.startingConfig.startingGold = 450;
    race.startingConfig.startingWood = 400;
    race.startingConfig.startingFood = 100;
    race.startingConfig.startingUnits = {{"worker", 4}};
    race.startingConfig.startingBuildings = {"main_hall"};

    // Stat modifiers - stronger magic and ranged, weaker melee
    race.statModifiers["spellDamage"] = 1.2f;
    race.statModifiers["rangedDamage"] = 1.1f;
    race.statModifiers["meleeDamage"] = 0.85f;

    return race;
}

RaceDefinition CreateUndeadRace() {
    RaceDefinition race;
    race.id = "undead";
    race.name = "Scourge of Undeath";
    race.shortName = "Undead";
    race.description = "Masters of death magic who raise fallen enemies to fight. Strong late game but slow start.";
    race.theme = RaceTheme::Horror;
    race.isBuiltIn = true;

    // Tech-focused allocation (slow start, strong late)
    race.allocation = CreateTechPreset();

    // Unit archetypes
    race.unitArchetypes = {
        "worker", "infantry_skeleton", "infantry_ghoul", "infantry_abomination",
        "ranged_skeleton_archer", "ranged_banshee",
        "cavalry_death_knight", "siege_meat_wagon"
    };

    // Building archetypes
    race.buildingArchetypes = {
        "main_hall", "crypt", "graveyard", "slaughterhouse",
        "temple_of_damned", "bone_tower", "necrotic_wall"
    };

    // Hero archetypes
    race.heroArchetypes = {
        "hero_death_knight", "hero_lich", "hero_necromancer"
    };

    // Spell archetypes
    race.spellArchetypes = {
        "spell_raise_dead", "spell_death_coil", "spell_unholy_aura", "spell_plague"
    };

    // Visual style
    race.visualStyle.primaryColor = "#4B0082";
    race.visualStyle.secondaryColor = "#00FF00";
    race.visualStyle.accentColor = "#000000";

    // Special restrictions - cannot gather food normally
    race.restrictions.canGatherFood = false;

    // Starting config
    race.startingConfig.startingGold = 600;
    race.startingConfig.startingWood = 200;
    race.startingConfig.startingFood = 0;
    race.startingConfig.startingUnits = {{"worker", 3}};
    race.startingConfig.startingBuildings = {"main_hall", "graveyard"};

    return race;
}

RaceDefinition CreateDwarfRace() {
    RaceDefinition race;
    race.id = "dwarves";
    race.name = "Dwarven Clans";
    race.shortName = "Dwarves";
    race.description = "Master craftsmen and miners with strong defenses and siege weapons. Slow but sturdy.";
    race.theme = RaceTheme::Fantasy;
    race.isBuiltIn = true;

    // Economy/Defense focused (turtle preset)
    race.allocation = CreateTurtlePreset();

    // Unit archetypes
    race.unitArchetypes = {
        "worker", "infantry_warrior", "infantry_guardian", "infantry_ironbreaker",
        "ranged_thunderer", "ranged_crossbow",
        "siege_cannon", "siege_gyrocopter"
    };

    // Building archetypes
    race.buildingArchetypes = {
        "main_hall", "mine", "barracks", "engineering_guild",
        "forge", "brewery", "stone_wall", "cannon_tower"
    };

    // Hero archetypes
    race.heroArchetypes = {
        "hero_thane", "hero_engineer", "hero_runesmith"
    };

    // Spell archetypes
    race.spellArchetypes = {
        "spell_rune_of_protection", "spell_forge_fire", "spell_earthquake"
    };

    // Visual style
    race.visualStyle.primaryColor = "#DAA520";
    race.visualStyle.secondaryColor = "#808080";
    race.visualStyle.accentColor = "#8B4513";

    // Starting config - more resources, fewer starting units
    race.startingConfig.startingGold = 400;
    race.startingConfig.startingWood = 200;
    race.startingConfig.startingStone = 400;
    race.startingConfig.startingMetal = 200;
    race.startingConfig.startingFood = 100;
    race.startingConfig.startingUnits = {{"worker", 4}};
    race.startingConfig.startingBuildings = {"main_hall"};

    // Stat modifiers - stronger defenses, slower movement
    race.statModifiers["buildingArmor"] = 1.25f;
    race.statModifiers["unitArmor"] = 1.1f;
    race.statModifiers["moveSpeed"] = 0.9f;
    race.statModifiers["miningSpeed"] = 1.2f;

    return race;
}

RaceDefinition CreateBlankRace() {
    RaceDefinition race;
    race.id = "custom_race";
    race.name = "Custom Race";
    race.shortName = "Custom";
    race.description = "A blank race template for custom creation.";
    race.theme = RaceTheme::Fantasy;
    race.isBuiltIn = false;

    // Balanced allocation
    race.allocation = CreateBalancedPreset();

    // Minimal archetypes
    race.unitArchetypes = {"worker"};
    race.buildingArchetypes = {"main_hall"};

    // Starting config
    race.startingConfig.startingGold = 500;
    race.startingConfig.startingWood = 300;
    race.startingConfig.startingFood = 100;
    race.startingConfig.startingUnits = {{"worker", 5}};
    race.startingConfig.startingBuildings = {"main_hall"};

    return race;
}

} // namespace Race
} // namespace RTS
} // namespace Vehement
