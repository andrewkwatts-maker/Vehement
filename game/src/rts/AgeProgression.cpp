#include "AgeProgression.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace RTS {

// ============================================================================
// AgeUITheme Implementation
// ============================================================================

nlohmann::json AgeUITheme::ToJson() const {
    return {
        {"primaryColor", primaryColor},
        {"secondaryColor", secondaryColor},
        {"backgroundColor", backgroundColor},
        {"textColor", textColor},
        {"highlightColor", highlightColor},
        {"fontStyle", fontStyle},
        {"iconSet", iconSet},
        {"borderStyle", borderStyle},
        {"uiScale", uiScale},
        {"useAnimations", useAnimations}
    };
}

AgeUITheme AgeUITheme::FromJson(const nlohmann::json& j) {
    AgeUITheme theme;
    theme.primaryColor = j.value("primaryColor", "#8B4513");
    theme.secondaryColor = j.value("secondaryColor", "#A0522D");
    theme.backgroundColor = j.value("backgroundColor", "#2F2F2F");
    theme.textColor = j.value("textColor", "#FFFFFF");
    theme.highlightColor = j.value("highlightColor", "#FFD700");
    theme.fontStyle = j.value("fontStyle", "default");
    theme.iconSet = j.value("iconSet", "default");
    theme.borderStyle = j.value("borderStyle", "simple");
    theme.uiScale = j.value("uiScale", 1.0f);
    theme.useAnimations = j.value("useAnimations", true);
    return theme;
}

// ============================================================================
// AgeContent Implementation
// ============================================================================

bool AgeContent::HasBuilding(const std::string& buildingId) const {
    return std::find(buildings.begin(), buildings.end(), buildingId) != buildings.end();
}

bool AgeContent::HasUnit(UnitType unit) const {
    return std::find(units.begin(), units.end(), unit) != units.end();
}

bool AgeContent::HasResource(ResourceType resource) const {
    return std::find(resources.begin(), resources.end(), resource) != resources.end();
}

nlohmann::json AgeContent::ToJson() const {
    nlohmann::json j;
    j["age"] = static_cast<int>(age);
    j["buildings"] = buildings;

    nlohmann::json unitsJson = nlohmann::json::array();
    for (UnitType u : units) {
        unitsJson.push_back(static_cast<int>(u));
    }
    j["units"] = unitsJson;

    j["abilities"] = abilities;

    nlohmann::json resourcesJson = nlohmann::json::array();
    for (ResourceType r : resources) {
        resourcesJson.push_back(static_cast<int>(r));
    }
    j["resources"] = resourcesJson;

    j["buildingStyle"] = static_cast<int>(buildingStyle);
    j["unitStyle"] = static_cast<int>(unitStyle);
    j["uiTheme"] = uiTheme.ToJson();

    j["buildingTexturePrefix"] = buildingTexturePrefix;
    j["unitTexturePrefix"] = unitTexturePrefix;
    j["effectTexturePrefix"] = effectTexturePrefix;
    j["musicTrack"] = musicTrack;
    j["ambientSounds"] = ambientSounds;
    j["combatSounds"] = combatSounds;

    j["gatherRateMultiplier"] = gatherRateMultiplier;
    j["buildSpeedMultiplier"] = buildSpeedMultiplier;
    j["combatDamageMultiplier"] = combatDamageMultiplier;
    j["defenseMultiplier"] = defenseMultiplier;
    j["movementSpeedMultiplier"] = movementSpeedMultiplier;
    j["productionSpeedMultiplier"] = productionSpeedMultiplier;
    j["healingRateMultiplier"] = healingRateMultiplier;
    j["visionRangeMultiplier"] = visionRangeMultiplier;

    j["basePopulationCap"] = basePopulationCap;
    j["populationPerHouse"] = populationPerHouse;
    j["baseStorageCapacity"] = baseStorageCapacity;

    j["canTrade"] = canTrade;
    j["canResearch"] = canResearch;
    j["canBuildWalls"] = canBuildWalls;
    j["canBuildSiege"] = canBuildSiege;
    j["canBuildNaval"] = canBuildNaval;
    j["canBuildAir"] = canBuildAir;

    return j;
}

AgeContent AgeContent::FromJson(const nlohmann::json& j) {
    AgeContent content;
    content.age = static_cast<Age>(j.value("age", 0));
    content.buildings = j.value("buildings", std::vector<std::string>{});

    if (j.contains("units")) {
        for (const auto& u : j["units"]) {
            content.units.push_back(static_cast<UnitType>(u.get<int>()));
        }
    }

    content.abilities = j.value("abilities", std::vector<std::string>{});

    if (j.contains("resources")) {
        for (const auto& r : j["resources"]) {
            content.resources.push_back(static_cast<ResourceType>(r.get<int>()));
        }
    }

    content.buildingStyle = static_cast<BuildingStyle>(j.value("buildingStyle", 0));
    content.unitStyle = static_cast<UnitStyle>(j.value("unitStyle", 0));

    if (j.contains("uiTheme")) {
        content.uiTheme = AgeUITheme::FromJson(j["uiTheme"]);
    }

    content.buildingTexturePrefix = j.value("buildingTexturePrefix", "");
    content.unitTexturePrefix = j.value("unitTexturePrefix", "");
    content.effectTexturePrefix = j.value("effectTexturePrefix", "");
    content.musicTrack = j.value("musicTrack", "");
    content.ambientSounds = j.value("ambientSounds", "");
    content.combatSounds = j.value("combatSounds", "");

    content.gatherRateMultiplier = j.value("gatherRateMultiplier", 1.0f);
    content.buildSpeedMultiplier = j.value("buildSpeedMultiplier", 1.0f);
    content.combatDamageMultiplier = j.value("combatDamageMultiplier", 1.0f);
    content.defenseMultiplier = j.value("defenseMultiplier", 1.0f);
    content.movementSpeedMultiplier = j.value("movementSpeedMultiplier", 1.0f);
    content.productionSpeedMultiplier = j.value("productionSpeedMultiplier", 1.0f);
    content.healingRateMultiplier = j.value("healingRateMultiplier", 1.0f);
    content.visionRangeMultiplier = j.value("visionRangeMultiplier", 1.0f);

    content.basePopulationCap = j.value("basePopulationCap", 20);
    content.populationPerHouse = j.value("populationPerHouse", 5);
    content.baseStorageCapacity = j.value("baseStorageCapacity", 500);

    content.canTrade = j.value("canTrade", false);
    content.canResearch = j.value("canResearch", false);
    content.canBuildWalls = j.value("canBuildWalls", false);
    content.canBuildSiege = j.value("canBuildSiege", false);
    content.canBuildNaval = j.value("canBuildNaval", false);
    content.canBuildAir = j.value("canBuildAir", false);

    return content;
}

// ============================================================================
// AgeProgressionManager Implementation
// ============================================================================

AgeProgressionManager& AgeProgressionManager::Instance() {
    static AgeProgressionManager instance;
    return instance;
}

bool AgeProgressionManager::Initialize() {
    if (m_initialized) {
        Shutdown();
    }

    m_ageContents.resize(static_cast<size_t>(Age::COUNT));

    // Initialize each age
    InitializeStoneAge();
    InitializeBronzeAge();
    InitializeIronAge();
    InitializeMedievalAge();
    InitializeIndustrialAge();
    InitializeModernAge();
    InitializeFutureAge();

    // Initialize UI themes
    InitializeUIThemes();

    m_initialized = true;
    return true;
}

void AgeProgressionManager::Shutdown() {
    m_ageContents.clear();
    m_cultureAdditions.clear();
    m_initialized = false;
}

void AgeProgressionManager::InitializeStoneAge() {
    AgeContent& content = m_ageContents[static_cast<size_t>(Age::Stone)];
    content.age = Age::Stone;

    // Buildings
    content.buildings = {"Shelter", "Campfire", "StoragePit", "HuntingGround"};

    // Units
    content.units = {UnitType::Worker, UnitType::Scout, UnitType::Clubman, UnitType::Hunter};

    // Abilities
    content.abilities = {"gather", "hunt", "build_basic"};

    // Resources (basic only)
    content.resources = {ResourceType::Food, ResourceType::Wood, ResourceType::Stone};

    // Visual style
    content.buildingStyle = BuildingStyle::Primitive;
    content.unitStyle = UnitStyle::Tribal;
    content.buildingTexturePrefix = "Vehement2/images/primitive/";
    content.unitTexturePrefix = "Vehement2/images/units/tribal/";

    // Audio
    content.musicTrack = "audio/music/stone_age.ogg";
    content.ambientSounds = "audio/ambient/wilderness.ogg";
    content.combatSounds = "audio/combat/primitive.ogg";

    // Gameplay modifiers (baseline)
    content.gatherRateMultiplier = 1.0f;
    content.buildSpeedMultiplier = 0.8f;  // Slow building
    content.combatDamageMultiplier = 0.8f;
    content.defenseMultiplier = 0.7f;
    content.movementSpeedMultiplier = 1.0f;
    content.productionSpeedMultiplier = 0.7f;
    content.healingRateMultiplier = 0.5f;  // Primitive healing
    content.visionRangeMultiplier = 0.8f;

    // Population
    content.basePopulationCap = 15;
    content.populationPerHouse = 3;
    content.baseStorageCapacity = 300;

    // Mechanics
    content.canTrade = false;
    content.canResearch = false;
    content.canBuildWalls = false;
    content.canBuildSiege = false;
    content.canBuildNaval = false;
    content.canBuildAir = false;
}

void AgeProgressionManager::InitializeBronzeAge() {
    AgeContent& content = m_ageContents[static_cast<size_t>(Age::Bronze)];
    content.age = Age::Bronze;

    // Buildings
    content.buildings = {"House", "Farm", "LumberMill", "Quarry", "Barracks", "Wall"};

    // Units
    content.units = {UnitType::Spearman, UnitType::Slinger, UnitType::BronzeWarrior};

    // Abilities
    content.abilities = {"farm", "fortify", "trade_basic"};

    // Resources
    content.resources = {ResourceType::Food, ResourceType::Wood, ResourceType::Stone, ResourceType::Metal};

    // Visual style
    content.buildingStyle = BuildingStyle::Wooden;
    content.unitStyle = UnitStyle::Bronze;
    content.buildingTexturePrefix = "Vehement2/images/wooden/";
    content.unitTexturePrefix = "Vehement2/images/units/bronze/";

    // Audio
    content.musicTrack = "audio/music/bronze_age.ogg";
    content.ambientSounds = "audio/ambient/village.ogg";
    content.combatSounds = "audio/combat/bronze.ogg";

    // Gameplay modifiers
    content.gatherRateMultiplier = 1.2f;
    content.buildSpeedMultiplier = 1.0f;
    content.combatDamageMultiplier = 1.0f;
    content.defenseMultiplier = 0.9f;
    content.movementSpeedMultiplier = 1.0f;
    content.productionSpeedMultiplier = 0.9f;
    content.healingRateMultiplier = 0.7f;
    content.visionRangeMultiplier = 1.0f;

    // Population
    content.basePopulationCap = 25;
    content.populationPerHouse = 4;
    content.baseStorageCapacity = 500;

    // Mechanics
    content.canTrade = true;
    content.canResearch = false;
    content.canBuildWalls = true;
    content.canBuildSiege = false;
    content.canBuildNaval = false;
    content.canBuildAir = false;
}

void AgeProgressionManager::InitializeIronAge() {
    AgeContent& content = m_ageContents[static_cast<size_t>(Age::Iron)];
    content.age = Age::Iron;

    // Buildings
    content.buildings = {"Forge", "Tower", "StoneWall", "Armory", "Stable", "Workshop"};

    // Units
    content.units = {UnitType::Swordsman, UnitType::Archer, UnitType::HeavyInfantry, UnitType::Cavalry};

    // Abilities
    content.abilities = {"forge_weapons", "cavalry_charge", "defensive_formation"};

    // Resources (all basic)
    content.resources = {ResourceType::Food, ResourceType::Wood, ResourceType::Stone,
                         ResourceType::Metal, ResourceType::Coins};

    // Visual style
    content.buildingStyle = BuildingStyle::Stone;
    content.unitStyle = UnitStyle::Iron;
    content.buildingTexturePrefix = "Vehement2/images/stone/";
    content.unitTexturePrefix = "Vehement2/images/units/iron/";

    // Audio
    content.musicTrack = "audio/music/iron_age.ogg";
    content.ambientSounds = "audio/ambient/town.ogg";
    content.combatSounds = "audio/combat/iron.ogg";

    // Gameplay modifiers
    content.gatherRateMultiplier = 1.4f;
    content.buildSpeedMultiplier = 1.2f;
    content.combatDamageMultiplier = 1.3f;
    content.defenseMultiplier = 1.2f;
    content.movementSpeedMultiplier = 1.1f;
    content.productionSpeedMultiplier = 1.1f;
    content.healingRateMultiplier = 0.9f;
    content.visionRangeMultiplier = 1.1f;

    // Population
    content.basePopulationCap = 40;
    content.populationPerHouse = 5;
    content.baseStorageCapacity = 800;

    // Mechanics
    content.canTrade = true;
    content.canResearch = true;
    content.canBuildWalls = true;
    content.canBuildSiege = false;
    content.canBuildNaval = false;
    content.canBuildAir = false;
}

void AgeProgressionManager::InitializeMedievalAge() {
    AgeContent& content = m_ageContents[static_cast<size_t>(Age::Medieval)];
    content.age = Age::Medieval;

    // Buildings
    content.buildings = {"Castle", "Keep", "Cathedral", "Market", "University", "SiegeWorkshop"};

    // Units
    content.units = {UnitType::Knight, UnitType::Crossbowman, UnitType::Pikeman,
                     UnitType::Trebuchet, UnitType::BatteringRam};

    // Abilities
    content.abilities = {"siege", "cavalry_charge_heavy", "inspire", "heal_units"};

    // Resources
    content.resources = {ResourceType::Food, ResourceType::Wood, ResourceType::Stone,
                         ResourceType::Metal, ResourceType::Coins};

    // Visual style
    content.buildingStyle = BuildingStyle::Medieval;
    content.unitStyle = UnitStyle::Feudal;
    content.buildingTexturePrefix = "Vehement2/images/medieval/";
    content.unitTexturePrefix = "Vehement2/images/units/feudal/";

    // Audio
    content.musicTrack = "audio/music/medieval_age.ogg";
    content.ambientSounds = "audio/ambient/castle.ogg";
    content.combatSounds = "audio/combat/medieval.ogg";

    // Gameplay modifiers
    content.gatherRateMultiplier = 1.6f;
    content.buildSpeedMultiplier = 1.3f;
    content.combatDamageMultiplier = 1.5f;
    content.defenseMultiplier = 1.5f;
    content.movementSpeedMultiplier = 1.2f;
    content.productionSpeedMultiplier = 1.3f;
    content.healingRateMultiplier = 1.1f;
    content.visionRangeMultiplier = 1.2f;

    // Population
    content.basePopulationCap = 60;
    content.populationPerHouse = 6;
    content.baseStorageCapacity = 1200;

    // Mechanics
    content.canTrade = true;
    content.canResearch = true;
    content.canBuildWalls = true;
    content.canBuildSiege = true;
    content.canBuildNaval = true;
    content.canBuildAir = false;
}

void AgeProgressionManager::InitializeIndustrialAge() {
    AgeContent& content = m_ageContents[static_cast<size_t>(Age::Industrial)];
    content.age = Age::Industrial;

    // Buildings
    content.buildings = {"Factory", "PowerPlant", "TrainStation", "Arsenal", "Hospital", "Bank"};

    // Units
    content.units = {UnitType::Musketeer, UnitType::Cannon, UnitType::Dragoon};

    // Abilities
    content.abilities = {"artillery_barrage", "mass_production", "railroad_supply"};

    // Resources (add fuel)
    content.resources = {ResourceType::Food, ResourceType::Wood, ResourceType::Stone,
                         ResourceType::Metal, ResourceType::Coins, ResourceType::Fuel};

    // Visual style
    content.buildingStyle = BuildingStyle::Brick;
    content.unitStyle = UnitStyle::Colonial;
    content.buildingTexturePrefix = "Vehement2/images/industrial/";
    content.unitTexturePrefix = "Vehement2/images/units/colonial/";

    // Audio
    content.musicTrack = "audio/music/industrial_age.ogg";
    content.ambientSounds = "audio/ambient/factory.ogg";
    content.combatSounds = "audio/combat/industrial.ogg";

    // Gameplay modifiers
    content.gatherRateMultiplier = 2.0f;
    content.buildSpeedMultiplier = 1.6f;
    content.combatDamageMultiplier = 2.0f;
    content.defenseMultiplier = 1.8f;
    content.movementSpeedMultiplier = 1.4f;
    content.productionSpeedMultiplier = 2.0f;
    content.healingRateMultiplier = 1.3f;
    content.visionRangeMultiplier = 1.3f;

    // Population
    content.basePopulationCap = 80;
    content.populationPerHouse = 8;
    content.baseStorageCapacity = 2000;

    // Mechanics
    content.canTrade = true;
    content.canResearch = true;
    content.canBuildWalls = true;
    content.canBuildSiege = true;
    content.canBuildNaval = true;
    content.canBuildAir = false;
}

void AgeProgressionManager::InitializeModernAge() {
    AgeContent& content = m_ageContents[static_cast<size_t>(Age::Modern)];
    content.age = Age::Modern;

    // Buildings
    content.buildings = {"PowerGrid", "Airport", "ResearchCenter", "Bunker", "Radar", "MilitaryBase"};

    // Units
    content.units = {UnitType::Rifleman, UnitType::MachineGunner, UnitType::Tank, UnitType::APC};

    // Abilities
    content.abilities = {"airstrike", "mechanized_assault", "radar_scan", "communications"};

    // Resources (all)
    content.resources = {ResourceType::Food, ResourceType::Wood, ResourceType::Stone,
                         ResourceType::Metal, ResourceType::Coins, ResourceType::Fuel,
                         ResourceType::Medicine, ResourceType::Ammunition};

    // Visual style
    content.buildingStyle = BuildingStyle::Modern;
    content.unitStyle = UnitStyle::Military;
    content.buildingTexturePrefix = "Vehement2/images/modern/";
    content.unitTexturePrefix = "Vehement2/images/units/military/";

    // Audio
    content.musicTrack = "audio/music/modern_age.ogg";
    content.ambientSounds = "audio/ambient/city.ogg";
    content.combatSounds = "audio/combat/modern.ogg";

    // Gameplay modifiers
    content.gatherRateMultiplier = 2.5f;
    content.buildSpeedMultiplier = 2.0f;
    content.combatDamageMultiplier = 3.0f;
    content.defenseMultiplier = 2.5f;
    content.movementSpeedMultiplier = 2.0f;
    content.productionSpeedMultiplier = 2.5f;
    content.healingRateMultiplier = 2.0f;
    content.visionRangeMultiplier = 2.0f;

    // Population
    content.basePopulationCap = 100;
    content.populationPerHouse = 10;
    content.baseStorageCapacity = 4000;

    // Mechanics
    content.canTrade = true;
    content.canResearch = true;
    content.canBuildWalls = true;
    content.canBuildSiege = true;
    content.canBuildNaval = true;
    content.canBuildAir = true;
}

void AgeProgressionManager::InitializeFutureAge() {
    AgeContent& content = m_ageContents[static_cast<size_t>(Age::Future)];
    content.age = Age::Future;

    // Buildings
    content.buildings = {"FusionReactor", "ShieldGenerator", "DroneFactory", "QuantumLab",
                         "OrbitalUplink", "NanoAssembler"};

    // Units
    content.units = {UnitType::PlasmaRifleman, UnitType::HoverTank,
                     UnitType::BattleDrone, UnitType::MechWarrior};

    // Abilities
    content.abilities = {"energy_shield", "plasma_bombardment", "nanorepair",
                         "ai_override", "quantum_teleport"};

    // Resources (all)
    content.resources = {ResourceType::Food, ResourceType::Wood, ResourceType::Stone,
                         ResourceType::Metal, ResourceType::Coins, ResourceType::Fuel,
                         ResourceType::Medicine, ResourceType::Ammunition};

    // Visual style
    content.buildingStyle = BuildingStyle::Futuristic;
    content.unitStyle = UnitStyle::SciFi;
    content.buildingTexturePrefix = "Vehement2/images/future/";
    content.unitTexturePrefix = "Vehement2/images/units/scifi/";

    // Audio
    content.musicTrack = "audio/music/future_age.ogg";
    content.ambientSounds = "audio/ambient/scifi.ogg";
    content.combatSounds = "audio/combat/energy.ogg";

    // Gameplay modifiers (highest)
    content.gatherRateMultiplier = 3.5f;
    content.buildSpeedMultiplier = 3.0f;
    content.combatDamageMultiplier = 4.0f;
    content.defenseMultiplier = 3.5f;
    content.movementSpeedMultiplier = 2.5f;
    content.productionSpeedMultiplier = 4.0f;
    content.healingRateMultiplier = 3.0f;
    content.visionRangeMultiplier = 3.0f;

    // Population
    content.basePopulationCap = 150;
    content.populationPerHouse = 15;
    content.baseStorageCapacity = 10000;

    // Mechanics
    content.canTrade = true;
    content.canResearch = true;
    content.canBuildWalls = true;
    content.canBuildSiege = true;
    content.canBuildNaval = true;
    content.canBuildAir = true;
}

void AgeProgressionManager::InitializeUIThemes() {
    // Stone Age - earthy browns
    m_ageContents[static_cast<size_t>(Age::Stone)].uiTheme = {
        "#8B4513", "#A0522D", "#3D2817", "#E8D5B7", "#CD853F",
        "primitive", "icons/stone/", "rough", 1.0f, false
    };

    // Bronze Age - bronze/gold tones
    m_ageContents[static_cast<size_t>(Age::Bronze)].uiTheme = {
        "#CD7F32", "#B8860B", "#2F2F1F", "#FFE4B5", "#DAA520",
        "ancient", "icons/bronze/", "ornate", 1.0f, true
    };

    // Iron Age - steel grey
    m_ageContents[static_cast<size_t>(Age::Iron)].uiTheme = {
        "#708090", "#4682B4", "#1C1C1C", "#DCDCDC", "#B0C4DE",
        "classical", "icons/iron/", "beveled", 1.0f, true
    };

    // Medieval Age - royal purple/gold
    m_ageContents[static_cast<size_t>(Age::Medieval)].uiTheme = {
        "#4B0082", "#800000", "#1A1A2E", "#F5F5DC", "#FFD700",
        "medieval", "icons/medieval/", "gothic", 1.0f, true
    };

    // Industrial Age - copper/rust
    m_ageContents[static_cast<size_t>(Age::Industrial)].uiTheme = {
        "#B87333", "#8B0000", "#2F2F2F", "#C0C0C0", "#FF6B35",
        "industrial", "icons/industrial/", "riveted", 1.0f, true
    };

    // Modern Age - military green/grey
    m_ageContents[static_cast<size_t>(Age::Modern)].uiTheme = {
        "#556B2F", "#36454F", "#1A1A1A", "#E0E0E0", "#00FF00",
        "modern", "icons/modern/", "military", 1.0f, true
    };

    // Future Age - cyan/blue glow
    m_ageContents[static_cast<size_t>(Age::Future)].uiTheme = {
        "#00CED1", "#4169E1", "#0D0D1A", "#E0FFFF", "#00FFFF",
        "futuristic", "icons/future/", "holographic", 1.1f, true
    };
}

const AgeContent& AgeProgressionManager::GetAgeContent(Age age) const {
    return m_ageContents[static_cast<size_t>(age)];
}

AgeContent AgeProgressionManager::GetAgeContentForCulture(Age age, CultureType culture) const {
    AgeContent content = GetAgeContent(age);

    // Apply culture-specific additions
    auto key = std::make_pair(age, culture);
    auto it = m_cultureAdditions.find(key);
    if (it != m_cultureAdditions.end()) {
        const auto& additions = it->second;

        // Add culture buildings
        for (const auto& building : additions.buildings) {
            if (!content.HasBuilding(building)) {
                content.buildings.push_back(building);
            }
        }

        // Add culture units
        for (UnitType unit : additions.units) {
            if (!content.HasUnit(unit)) {
                content.units.push_back(unit);
            }
        }

        // Add culture abilities
        for (const auto& ability : additions.abilities) {
            content.abilities.push_back(ability);
        }

        // Apply modifiers
        for (const auto& [stat, mult] : additions.modifiers) {
            if (stat == "gather") content.gatherRateMultiplier *= mult;
            else if (stat == "build") content.buildSpeedMultiplier *= mult;
            else if (stat == "damage") content.combatDamageMultiplier *= mult;
            else if (stat == "defense") content.defenseMultiplier *= mult;
            else if (stat == "movement") content.movementSpeedMultiplier *= mult;
            else if (stat == "production") content.productionSpeedMultiplier *= mult;
        }
    }

    return content;
}

bool AgeProgressionManager::IsBuildingAvailable(const std::string& buildingId, Age age) const {
    for (int i = 0; i <= static_cast<int>(age); i++) {
        if (m_ageContents[i].HasBuilding(buildingId)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> AgeProgressionManager::GetAvailableBuildings(Age age) const {
    std::vector<std::string> result;

    for (int i = 0; i <= static_cast<int>(age); i++) {
        for (const auto& building : m_ageContents[i].buildings) {
            if (std::find(result.begin(), result.end(), building) == result.end()) {
                result.push_back(building);
            }
        }
    }

    return result;
}

std::vector<std::string> AgeProgressionManager::GetBuildingsUnlockedAtAge(Age age) const {
    return m_ageContents[static_cast<size_t>(age)].buildings;
}

Age AgeProgressionManager::GetBuildingMinAge(const std::string& buildingId) const {
    for (int i = 0; i < static_cast<int>(Age::COUNT); i++) {
        if (m_ageContents[i].HasBuilding(buildingId)) {
            return static_cast<Age>(i);
        }
    }
    return Age::Stone;
}

bool AgeProgressionManager::IsUnitAvailable(UnitType unit, Age age) const {
    for (int i = 0; i <= static_cast<int>(age); i++) {
        if (m_ageContents[i].HasUnit(unit)) {
            return true;
        }
    }
    return false;
}

std::vector<UnitType> AgeProgressionManager::GetAvailableUnits(Age age) const {
    std::vector<UnitType> result;

    for (int i = 0; i <= static_cast<int>(age); i++) {
        for (UnitType unit : m_ageContents[i].units) {
            if (std::find(result.begin(), result.end(), unit) == result.end()) {
                result.push_back(unit);
            }
        }
    }

    return result;
}

std::vector<UnitType> AgeProgressionManager::GetUnitsUnlockedAtAge(Age age) const {
    return m_ageContents[static_cast<size_t>(age)].units;
}

Age AgeProgressionManager::GetUnitMinAge(UnitType unit) const {
    for (int i = 0; i < static_cast<int>(Age::COUNT); i++) {
        if (m_ageContents[i].HasUnit(unit)) {
            return static_cast<Age>(i);
        }
    }
    return Age::Stone;
}

float AgeProgressionManager::GetGatherRateModifier(Age age) const {
    return m_ageContents[static_cast<size_t>(age)].gatherRateMultiplier;
}

float AgeProgressionManager::GetBuildSpeedModifier(Age age) const {
    return m_ageContents[static_cast<size_t>(age)].buildSpeedMultiplier;
}

float AgeProgressionManager::GetCombatDamageModifier(Age age) const {
    return m_ageContents[static_cast<size_t>(age)].combatDamageMultiplier;
}

float AgeProgressionManager::GetDefenseModifier(Age age) const {
    return m_ageContents[static_cast<size_t>(age)].defenseMultiplier;
}

int AgeProgressionManager::GetPopulationCap(Age age) const {
    return m_ageContents[static_cast<size_t>(age)].basePopulationCap;
}

int AgeProgressionManager::GetStorageCapacity(Age age) const {
    return m_ageContents[static_cast<size_t>(age)].baseStorageCapacity;
}

BuildingStyle AgeProgressionManager::GetBuildingStyle(Age age) const {
    return m_ageContents[static_cast<size_t>(age)].buildingStyle;
}

UnitStyle AgeProgressionManager::GetUnitStyle(Age age) const {
    return m_ageContents[static_cast<size_t>(age)].unitStyle;
}

const AgeUITheme& AgeProgressionManager::GetUITheme(Age age) const {
    return m_ageContents[static_cast<size_t>(age)].uiTheme;
}

std::string AgeProgressionManager::GetBuildingTexturePrefix(Age age) const {
    return m_ageContents[static_cast<size_t>(age)].buildingTexturePrefix;
}

std::string AgeProgressionManager::GetUnitTexturePrefix(Age age) const {
    return m_ageContents[static_cast<size_t>(age)].unitTexturePrefix;
}

void AgeProgressionManager::OnAgeTransition(Age from, Age to, CultureType culture) {
    if (m_onAgeTransition) {
        m_onAgeTransition(from, to, culture);
    }
}

void AgeProgressionManager::AddCultureBuilding(Age age, CultureType culture, const std::string& buildingId) {
    auto key = std::make_pair(age, culture);
    m_cultureAdditions[key].buildings.push_back(buildingId);
}

void AgeProgressionManager::AddCultureUnit(Age age, CultureType culture, UnitType unit) {
    auto key = std::make_pair(age, culture);
    m_cultureAdditions[key].units.push_back(unit);
}

float AgeProgressionManager::GetCultureModifier(CultureType culture, const std::string& stat) const {
    // Return culture-specific modifiers
    // This would be expanded based on CultureBonusModifiers
    return 1.0f;
}

nlohmann::json AgeProgressionManager::ToJson() const {
    nlohmann::json j;

    nlohmann::json contentsJson = nlohmann::json::array();
    for (const auto& content : m_ageContents) {
        contentsJson.push_back(content.ToJson());
    }
    j["ageContents"] = contentsJson;

    return j;
}

void AgeProgressionManager::FromJson(const nlohmann::json& j) {
    if (j.contains("ageContents")) {
        m_ageContents.clear();
        for (const auto& contentJson : j["ageContents"]) {
            m_ageContents.push_back(AgeContent::FromJson(contentJson));
        }
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

float GetProgressionValue(Age age, float ageProgress) {
    float baseValue = static_cast<float>(static_cast<int>(age)) / static_cast<float>(static_cast<int>(Age::COUNT) - 1);
    float progressPortion = 1.0f / static_cast<float>(static_cast<int>(Age::COUNT));
    return baseValue + (ageProgress * progressPortion);
}

Age GetAgeFromProgressionValue(float progress) {
    int ageIndex = static_cast<int>(progress * static_cast<float>(static_cast<int>(Age::COUNT)));
    ageIndex = std::max(0, std::min(ageIndex, static_cast<int>(Age::COUNT) - 1));
    return static_cast<Age>(ageIndex);
}

std::string InterpolateAgeColor(Age fromAge, Age toAge, float t,
                                 const AgeProgressionManager& manager) {
    // Simple linear interpolation between age colors
    const auto& fromTheme = manager.GetUITheme(fromAge);
    const auto& toTheme = manager.GetUITheme(toAge);

    // For simplicity, just return one or the other based on t
    return (t < 0.5f) ? fromTheme.primaryColor : toTheme.primaryColor;
}

int CalculateAgePowerLevel(Age age, int techCount) {
    int basePower = (static_cast<int>(age) + 1) * 100;
    int techPower = techCount * 10;
    return basePower + techPower;
}

} // namespace RTS
} // namespace Vehement
