#include "BuildingProgression.hpp"
#include <algorithm>
#include <sstream>

namespace Vehement {
namespace RTS {

// ============================================================================
// AgeRequirement Implementation
// ============================================================================

bool AgeRequirement::IsMet(int buildings, int population,
                            const std::unordered_set<std::string>& completedTechs) const {
    if (buildings < buildingsRequired) return false;
    if (population < populationRequired) return false;

    for (const auto& tech : techsRequired) {
        if (completedTechs.find(tech) == completedTechs.end()) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// BuildingUnlock Implementation
// ============================================================================

bool BuildingUnlock::IsAvailableTo(CultureType culture, Age currentAge,
                                    const std::unordered_set<std::string>& completedTechs,
                                    const std::vector<BuildingType>& existingBuildings) const {

    // Check age requirement
    if (static_cast<int>(currentAge) < static_cast<int>(requiredAge)) {
        return false;
    }

    // Check culture restriction
    if (cultureOnly.has_value() && cultureOnly.value() != culture) {
        return false;
    }

    // Check culture exclusion
    for (CultureType excluded : culturesExcluded) {
        if (excluded == culture) {
            return false;
        }
    }

    // Check required technologies
    for (const auto& tech : requiredTechs) {
        if (completedTechs.find(tech) == completedTechs.end()) {
            return false;
        }
    }

    // Check required buildings
    for (BuildingType reqBuilding : requiredBuildings) {
        bool found = false;
        for (BuildingType existing : existingBuildings) {
            if (existing == reqBuilding) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    return true;
}

std::string BuildingUnlock::GetRequirementsString() const {
    std::ostringstream ss;

    ss << "Requires: " << AgeToString(requiredAge);

    if (!requiredTechs.empty()) {
        ss << ", Technologies: ";
        for (size_t i = 0; i < requiredTechs.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << requiredTechs[i];
        }
    }

    if (!requiredBuildings.empty()) {
        ss << ", Buildings: ";
        for (size_t i = 0; i < requiredBuildings.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << BuildingTypeToString(requiredBuildings[i]);
        }
    }

    if (cultureOnly.has_value()) {
        ss << " (Only: " << CultureTypeToString(cultureOnly.value()) << ")";
    }

    return ss.str();
}

// ============================================================================
// BuildingUpgradePath Implementation
// ============================================================================

const BuildingUpgradePath::LevelData* BuildingUpgradePath::GetLevel(int level) const {
    for (const auto& lvl : levels) {
        if (lvl.level == level) {
            return &lvl;
        }
    }
    return nullptr;
}

bool BuildingUpgradePath::CanUpgradeTo(int targetLevel, Age currentAge,
                                        const std::unordered_set<std::string>& techs) const {
    const LevelData* lvl = GetLevel(targetLevel);
    if (!lvl) return false;

    if (static_cast<int>(currentAge) < static_cast<int>(lvl->requiredAge)) {
        return false;
    }

    for (const auto& tech : lvl->requiredTechs) {
        if (techs.find(tech) == techs.end()) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// BuildingProgression Implementation
// ============================================================================

BuildingProgression::BuildingProgression() = default;

BuildingProgression::~BuildingProgression() = default;

bool BuildingProgression::Initialize() {
    InitializeAgeRequirements();
    InitializeBuildingUnlocks();
    InitializeUpgradePaths();
    return true;
}

void BuildingProgression::Shutdown() {
    m_ageRequirements.clear();
    m_unlocks.clear();
    m_upgradePaths.clear();
}

void BuildingProgression::InitializeAgeRequirements() {
    // Stone Age (starting age)
    m_ageRequirements[Age::Stone] = {
        Age::Stone, 0, 0, {}, ResourceCost()
    };

    // Bronze Age
    m_ageRequirements[Age::Bronze] = {
        Age::Bronze, 5, 10,
        {UniversalTechs::IMPROVED_GATHERING},
        ResourceCost(100, 50, 0, 50, 0)
    };

    // Iron Age
    m_ageRequirements[Age::Iron] = {
        Age::Iron, 15, 30,
        {UniversalTechs::BASIC_WEAPONS, UniversalTechs::ARMOR_PLATING},
        ResourceCost(200, 150, 50, 100, 50)
    };

    // Medieval Age
    m_ageRequirements[Age::Medieval] = {
        Age::Medieval, 30, 60,
        {UniversalTechs::REINFORCED_WALLS, UniversalTechs::TOWER_UPGRADES},
        ResourceCost(400, 300, 150, 200, 100)
    };

    // Renaissance
    m_ageRequirements[Age::Renaissance] = {
        Age::Renaissance, 50, 100,
        {},
        ResourceCost(600, 500, 300, 300, 200)
    };

    // Industrial
    m_ageRequirements[Age::Industrial] = {
        Age::Industrial, 80, 150,
        {},
        ResourceCost(1000, 800, 500, 500, 400)
    };

    // Modern
    m_ageRequirements[Age::Modern] = {
        Age::Modern, 120, 200,
        {},
        ResourceCost(1500, 1200, 800, 800, 600)
    };

    // Future
    m_ageRequirements[Age::Future] = {
        Age::Future, 200, 300,
        {},
        ResourceCost(2500, 2000, 1500, 1500, 1000)
    };
}

void BuildingProgression::InitializeBuildingUnlocks() {
    // -------------------------------------------------------------------------
    // Core Buildings (Always Available)
    // -------------------------------------------------------------------------

    AddUnlock({BuildingType::Headquarters, "headquarters",
        Age::Stone, {}, {}, std::nullopt, {}, 1, -1, true,
        "Your main base of operations"});

    AddUnlock({BuildingType::Storage, "storage",
        Age::Stone, {}, {}, std::nullopt, {}, -1, -1, false,
        "Increases resource storage"});

    // -------------------------------------------------------------------------
    // Housing (Stone Age)
    // -------------------------------------------------------------------------

    AddUnlock({BuildingType::Barracks, "barracks",
        Age::Stone, {}, {BuildingType::Headquarters}, std::nullopt, {}, -1, -1, false,
        "Houses and trains soldiers"});

    // -------------------------------------------------------------------------
    // Production (Stone/Bronze Age)
    // -------------------------------------------------------------------------

    AddUnlock({BuildingType::Farm, "farm",
        Age::Stone, {}, {BuildingType::Headquarters}, std::nullopt, {}, -1, -1, false,
        "Produces food for your population"});

    AddUnlock({BuildingType::Mine, "mine",
        Age::Bronze, {UniversalTechs::IMPROVED_GATHERING}, {}, std::nullopt, {}, -1, -1, false,
        "Extracts resources from the earth"});

    AddUnlock({BuildingType::Workshop, "workshop",
        Age::Bronze, {}, {BuildingType::Storage}, std::nullopt, {}, -1, -1, false,
        "Crafts equipment and items"});

    AddUnlock({BuildingType::Warehouse, "warehouse",
        Age::Bronze, {UniversalTechs::ADVANCED_STORAGE}, {BuildingType::Storage}, std::nullopt, {}, -1, -1, false,
        "Large storage facility"});

    // -------------------------------------------------------------------------
    // Defense (Stone Age+)
    // -------------------------------------------------------------------------

    AddUnlock({BuildingType::Wall, "wall",
        Age::Stone, {}, {}, std::nullopt, {}, -1, -1, false,
        "Basic defensive wall segment"});

    AddUnlock({BuildingType::WallGate, "wallgate",
        Age::Stone, {}, {BuildingType::Wall}, std::nullopt, {}, -1, -1, false,
        "Gate for walls - can open/close"});

    AddUnlock({BuildingType::Tower, "tower",
        Age::Bronze, {UniversalTechs::TOWER_UPGRADES}, {BuildingType::Wall}, std::nullopt, {}, -1, -1, false,
        "Defensive tower with ranged attack"});

    AddUnlock({BuildingType::Bunker, "bunker",
        Age::Iron, {UniversalTechs::REINFORCED_WALLS}, {BuildingType::Tower}, std::nullopt, {}, -1, -1, false,
        "Heavily fortified position"});

    AddUnlock({BuildingType::Turret, "turret",
        Age::Industrial, {}, {BuildingType::Bunker}, std::nullopt, {}, -1, -1, false,
        "Automated defense turret"});

    // -------------------------------------------------------------------------
    // Support (Bronze Age+)
    // -------------------------------------------------------------------------

    AddUnlock({BuildingType::Market, "market",
        Age::Bronze, {}, {BuildingType::Storage}, std::nullopt, {}, -1, 3, false,
        "Trade resources with others"});

    AddUnlock({BuildingType::Hospital, "hospital",
        Age::Iron, {}, {BuildingType::Barracks}, std::nullopt, {}, -1, 2, false,
        "Heals injured units"});

    AddUnlock({BuildingType::ResearchLab, "researchlab",
        Age::Iron, {}, {BuildingType::Workshop}, std::nullopt, {}, -1, 1, false,
        "Research new technologies"});

    AddUnlock({BuildingType::PowerPlant, "powerplant",
        Age::Industrial, {}, {BuildingType::Workshop}, std::nullopt, {}, -1, 3, false,
        "Generates power for buildings"});

    // -------------------------------------------------------------------------
    // Culture-Specific Buildings
    // -------------------------------------------------------------------------

    // Merchant - Bazaar
    AddUnlock({BuildingType::Bazaar, "bazaar",
        Age::Bronze, {MerchantTechs::BAZAAR_CONNECTIONS}, {},
        CultureType::Merchant, {}, -1, 2, false,
        "Large trading hub with better prices"});

    // Underground - Hidden Entrance
    AddUnlock({BuildingType::HiddenEntrance, "hidden_entrance",
        Age::Iron, {UndergroundTechs::HIDDEN_BASES}, {},
        CultureType::Underground, {}, -1, -1, false,
        "Concealed tunnel entrance"});

    // Nomad - Mobile Workshop
    AddUnlock({BuildingType::MobileWorkshop, "mobile_workshop",
        Age::Bronze, {NomadTechs::MOBILE_STRUCTURES}, {},
        CultureType::Nomad, {}, -1, 2, false,
        "Packable crafting station"});

    // Nomad - Yurt
    AddUnlock({BuildingType::Yurt, "yurt",
        Age::Stone, {}, {},
        CultureType::Nomad, {}, -1, -1, false,
        "Mobile housing structure"});

    // Fortress - Castle
    AddUnlock({BuildingType::Castle, "castle",
        Age::Medieval, {FortressTechs::CASTLE_ARCHITECTURE}, {BuildingType::Tower},
        CultureType::Fortress, {}, 1, 1, true,
        "Grand fortified stronghold"});

    // Industrial - Factory
    AddUnlock({BuildingType::Factory, "factory",
        Age::Industrial, {IndustrialTechs::ASSEMBLY_LINE}, {BuildingType::Workshop},
        CultureType::Industrial, {}, -1, 3, false,
        "Mass production facility"});
}

void BuildingProgression::InitializeUpgradePaths() {
    // Headquarters upgrade path
    {
        BuildingUpgradePath path;
        path.baseType = BuildingType::Headquarters;
        path.maxLevel = 3;

        path.levels.push_back({1, "Command Post", ResourceCost(), 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f, Age::Stone, {}});

        path.levels.push_back({2, "Command Center", ResourceCost(200, 200, 100, 0, 100), 60.0f,
            1.5f, 1.2f, 1.5f, 1.2f, Age::Bronze, {}});

        path.levels.push_back({3, "Fortress HQ", ResourceCost(500, 500, 300, 0, 300), 120.0f,
            2.0f, 1.5f, 2.0f, 1.5f, Age::Iron, {UniversalTechs::REINFORCED_WALLS}});

        AddUpgradePath(path);
    }

    // Tower upgrade path
    {
        BuildingUpgradePath path;
        path.baseType = BuildingType::Tower;
        path.maxLevel = 3;

        path.levels.push_back({1, "Watch Tower", ResourceCost(), 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f, Age::Bronze, {}});

        path.levels.push_back({2, "Guard Tower", ResourceCost(100, 150, 50, 0, 50), 45.0f,
            1.5f, 1.3f, 1.0f, 1.3f, Age::Iron, {UniversalTechs::TOWER_UPGRADES}});

        path.levels.push_back({3, "Siege Tower", ResourceCost(200, 300, 150, 0, 150), 90.0f,
            2.0f, 1.6f, 1.0f, 1.6f, Age::Medieval, {}});

        AddUpgradePath(path);
    }

    // Barracks upgrade path
    {
        BuildingUpgradePath path;
        path.baseType = BuildingType::Barracks;
        path.maxLevel = 3;

        path.levels.push_back({1, "Training Camp", ResourceCost(), 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f, Age::Stone, {}});

        path.levels.push_back({2, "Barracks", ResourceCost(150, 100, 50, 0, 50), 40.0f,
            1.3f, 1.2f, 1.5f, 1.0f, Age::Bronze, {UniversalTechs::COMBAT_TRAINING}});

        path.levels.push_back({3, "Military Academy", ResourceCost(300, 250, 150, 0, 200), 80.0f,
            1.6f, 1.5f, 2.0f, 1.0f, Age::Iron, {}});

        AddUpgradePath(path);
    }

    // Farm upgrade path
    {
        BuildingUpgradePath path;
        path.baseType = BuildingType::Farm;
        path.maxLevel = 3;

        path.levels.push_back({1, "Small Farm", ResourceCost(), 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f, Age::Stone, {}});

        path.levels.push_back({2, "Large Farm", ResourceCost(100, 50, 20, 0, 30), 30.0f,
            1.2f, 1.5f, 1.5f, 1.0f, Age::Bronze, {UniversalTechs::EFFICIENT_PRODUCTION}});

        path.levels.push_back({3, "Agricultural Complex", ResourceCost(250, 150, 80, 0, 100), 60.0f,
            1.4f, 2.0f, 2.0f, 1.0f, Age::Iron, {}});

        AddUpgradePath(path);
    }

    // Workshop upgrade path
    {
        BuildingUpgradePath path;
        path.baseType = BuildingType::Workshop;
        path.maxLevel = 3;

        path.levels.push_back({1, "Basic Workshop", ResourceCost(), 0.0f,
            1.0f, 1.0f, 1.0f, 1.0f, Age::Bronze, {}});

        path.levels.push_back({2, "Advanced Workshop", ResourceCost(150, 100, 100, 0, 80), 50.0f,
            1.3f, 1.4f, 1.3f, 1.0f, Age::Iron, {}});

        path.levels.push_back({3, "Master Workshop", ResourceCost(350, 250, 250, 0, 200), 100.0f,
            1.5f, 1.8f, 1.6f, 1.0f, Age::Medieval, {}});

        AddUpgradePath(path);
    }
}

void BuildingProgression::AddUnlock(const BuildingUnlock& unlock) {
    m_unlocks[unlock.type] = unlock;
}

void BuildingProgression::AddUpgradePath(const BuildingUpgradePath& path) {
    m_upgradePaths[path.baseType] = path;
}

// =========================================================================
// Age Management
// =========================================================================

const AgeRequirement* BuildingProgression::GetAgeRequirement(Age age) const {
    auto it = m_ageRequirements.find(age);
    return (it != m_ageRequirements.end()) ? &it->second : nullptr;
}

bool BuildingProgression::CanAdvanceAge(Age currentAge, int buildings, int population,
                                         const std::unordered_set<std::string>& techs) const {
    Age nextAge = GetNextAge(currentAge);
    if (nextAge == currentAge) return false; // Already max age

    const AgeRequirement* req = GetAgeRequirement(nextAge);
    return req && req->IsMet(buildings, population, techs);
}

Age BuildingProgression::GetNextAge(Age current) const {
    int next = static_cast<int>(current) + 1;
    if (next >= static_cast<int>(Age::COUNT)) {
        return current; // Already at max
    }
    return static_cast<Age>(next);
}

// =========================================================================
// Building Availability
// =========================================================================

std::vector<BuildingType> BuildingProgression::GetAvailableBuildings(
    CultureType culture, Age currentAge,
    const std::unordered_set<std::string>& completedTechs,
    const std::vector<BuildingType>& existingBuildings) const {

    std::vector<BuildingType> available;

    for (const auto& [type, unlock] : m_unlocks) {
        if (unlock.IsAvailableTo(culture, currentAge, completedTechs, existingBuildings)) {
            if (!IsAtBuildLimit(type, existingBuildings)) {
                available.push_back(type);
            }
        }
    }

    return available;
}

bool BuildingProgression::CanBuild(BuildingType type, CultureType culture, Age currentAge,
                                    const std::unordered_set<std::string>& completedTechs,
                                    const std::vector<BuildingType>& existingBuildings) const {
    auto it = m_unlocks.find(type);
    if (it == m_unlocks.end()) return false;

    if (!it->second.IsAvailableTo(culture, currentAge, completedTechs, existingBuildings)) {
        return false;
    }

    return !IsAtBuildLimit(type, existingBuildings);
}

bool BuildingProgression::IsAtBuildLimit(BuildingType type,
                                          const std::vector<BuildingType>& existingBuildings) const {
    auto it = m_unlocks.find(type);
    if (it == m_unlocks.end()) return false;

    const BuildingUnlock& unlock = it->second;

    if (unlock.maxCount == -1 && !unlock.isUnique) {
        return false; // No limit
    }

    int count = 0;
    for (BuildingType existing : existingBuildings) {
        if (existing == type) ++count;
    }

    if (unlock.isUnique && count > 0) return true;
    if (unlock.maxCount > 0 && count >= unlock.maxCount) return true;

    return false;
}

std::string BuildingProgression::GetUnlockRequirements(BuildingType type) const {
    auto it = m_unlocks.find(type);
    if (it == m_unlocks.end()) return "Unknown building";
    return it->second.GetRequirementsString();
}

std::vector<BuildingType> BuildingProgression::GetBuildingsUnlockedByTech(
    const std::string& techId) const {

    std::vector<BuildingType> result;

    for (const auto& [type, unlock] : m_unlocks) {
        for (const auto& reqTech : unlock.requiredTechs) {
            if (reqTech == techId) {
                result.push_back(type);
                break;
            }
        }
    }

    return result;
}

// =========================================================================
// Building Upgrades
// =========================================================================

int BuildingProgression::GetMaxBuildingLevel(BuildingType type) const {
    auto it = m_upgradePaths.find(type);
    return (it != m_upgradePaths.end()) ? it->second.maxLevel : 1;
}

int BuildingProgression::GetMaxBuildingLevel(BuildingType type, Age currentAge,
                                              const std::unordered_set<std::string>& techs) const {
    auto it = m_upgradePaths.find(type);
    if (it == m_upgradePaths.end()) return 1;

    int maxLevel = 1;
    for (const auto& lvl : it->second.levels) {
        if (it->second.CanUpgradeTo(lvl.level, currentAge, techs)) {
            maxLevel = std::max(maxLevel, lvl.level);
        }
    }

    return maxLevel;
}

const BuildingUpgradePath* BuildingProgression::GetUpgradePath(BuildingType type) const {
    auto it = m_upgradePaths.find(type);
    return (it != m_upgradePaths.end()) ? &it->second : nullptr;
}

bool BuildingProgression::CanUpgrade(BuildingType type, int currentLevel, Age age,
                                      const std::unordered_set<std::string>& techs) const {
    const BuildingUpgradePath* path = GetUpgradePath(type);
    if (!path) return false;
    if (currentLevel >= path->maxLevel) return false;

    return path->CanUpgradeTo(currentLevel + 1, age, techs);
}

ResourceCost BuildingProgression::GetUpgradeCost(BuildingType type, int targetLevel) const {
    const BuildingUpgradePath* path = GetUpgradePath(type);
    if (!path) return ResourceCost();

    const BuildingUpgradePath::LevelData* lvl = path->GetLevel(targetLevel);
    return lvl ? lvl->upgradeCost : ResourceCost();
}

float BuildingProgression::GetUpgradeTime(BuildingType type, int targetLevel) const {
    const BuildingUpgradePath* path = GetUpgradePath(type);
    if (!path) return 0.0f;

    const BuildingUpgradePath::LevelData* lvl = path->GetLevel(targetLevel);
    return lvl ? lvl->upgradeTime : 0.0f;
}

std::string BuildingProgression::GetLevelName(BuildingType type, int level) const {
    const BuildingUpgradePath* path = GetUpgradePath(type);
    if (!path) return BuildingTypeToString(type);

    const BuildingUpgradePath::LevelData* lvl = path->GetLevel(level);
    return lvl ? lvl->name : BuildingTypeToString(type);
}

// =========================================================================
// Culture-Specific
// =========================================================================

std::vector<BuildingType> BuildingProgression::GetCultureUniqueBuildings(CultureType culture) const {
    std::vector<BuildingType> result;

    for (const auto& [type, unlock] : m_unlocks) {
        if (unlock.cultureOnly.has_value() && unlock.cultureOnly.value() == culture) {
            result.push_back(type);
        }
    }

    return result;
}

std::vector<BuildingType> BuildingProgression::GetCultureExcludedBuildings(CultureType culture) const {
    std::vector<BuildingType> result;

    for (const auto& [type, unlock] : m_unlocks) {
        for (CultureType excluded : unlock.culturesExcluded) {
            if (excluded == culture) {
                result.push_back(type);
                break;
            }
        }
    }

    return result;
}

bool BuildingProgression::IsCultureSpecific(BuildingType type) const {
    auto it = m_unlocks.find(type);
    return it != m_unlocks.end() && it->second.cultureOnly.has_value();
}

std::optional<CultureType> BuildingProgression::GetCultureForBuilding(BuildingType type) const {
    auto it = m_unlocks.find(type);
    if (it == m_unlocks.end()) return std::nullopt;
    return it->second.cultureOnly;
}

// =========================================================================
// Categories
// =========================================================================

std::vector<BuildingType> BuildingProgression::GetBuildingsByCategory(BuildingCategory /* category */) const {
    std::vector<BuildingType> result;
    // Would filter by category
    return result;
}

std::vector<BuildingType> BuildingProgression::GetBuildingsForAge(Age age) const {
    std::vector<BuildingType> result;

    for (const auto& [type, unlock] : m_unlocks) {
        if (unlock.requiredAge == age) {
            result.push_back(type);
        }
    }

    return result;
}

// =========================================================================
// UI Helpers
// =========================================================================

BuildingProgression::BuildingInfo BuildingProgression::GetBuildingInfo(
    BuildingType type, CultureType culture, Age currentAge,
    const std::unordered_set<std::string>& techs,
    const std::vector<BuildingType>& existing) const {

    BuildingInfo info;
    info.type = type;
    info.name = BuildingTypeToString(type);
    info.isAvailable = CanBuild(type, culture, currentAge, techs, existing);

    auto it = m_unlocks.find(type);
    if (it != m_unlocks.end()) {
        info.requiredAge = it->second.requiredAge;
        info.maxCount = it->second.maxCount;
        info.isLocked = !info.isAvailable;

        if (info.isLocked) {
            info.lockReason = it->second.GetRequirementsString();
        }
    }

    // Count existing
    info.currentCount = 0;
    for (BuildingType e : existing) {
        if (e == type) ++info.currentCount;
    }

    return info;
}

std::vector<BuildingProgression::BuildingInfo> BuildingProgression::GetAllBuildingInfo(
    CultureType culture, Age currentAge,
    const std::unordered_set<std::string>& techs,
    const std::vector<BuildingType>& existing) const {

    std::vector<BuildingInfo> result;

    for (const auto& [type, unlock] : m_unlocks) {
        result.push_back(GetBuildingInfo(type, culture, currentAge, techs, existing));
    }

    return result;
}

// ============================================================================
// AgeProgression Implementation
// ============================================================================

AgeProgression::AgeProgression() = default;

void AgeProgression::Initialize(CultureType culture) {
    m_culture = culture;
    m_currentAge = Age::Stone;
    m_timeInAge = 0.0f;
}

bool AgeProgression::CanAdvance(const BuildingProgression& progression,
                                 int buildings, int population,
                                 const std::unordered_set<std::string>& techs) const {
    return progression.CanAdvanceAge(m_currentAge, buildings, population, techs);
}

bool AgeProgression::Advance(const BuildingProgression& progression,
                              int buildings, int population,
                              const std::unordered_set<std::string>& techs) {
    if (!CanAdvance(progression, buildings, population, techs)) {
        return false;
    }

    Age oldAge = m_currentAge;
    m_currentAge = progression.GetNextAge(m_currentAge);
    m_timeInAge = 0.0f;

    if (m_onAdvance) {
        m_onAdvance(oldAge, m_currentAge);
    }

    return true;
}

float AgeProgression::GetProgressToNextAge(int /* buildings */, int /* population */) const {
    // Would calculate based on requirements
    return 0.0f;
}

void AgeProgression::Update(float deltaTime) {
    m_timeInAge += deltaTime;
}

} // namespace RTS
} // namespace Vehement
