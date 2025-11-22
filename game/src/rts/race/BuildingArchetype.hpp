#pragma once

/**
 * @file BuildingArchetype.hpp
 * @brief Building template definitions for RTS races
 *
 * Defines all building archetypes including main halls, resource buildings,
 * military structures, defenses, research facilities, and special buildings.
 */

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {
namespace Race {

// ============================================================================
// Building Categories
// ============================================================================

enum class BuildingCategory : uint8_t {
    MainHall = 0,       ///< Town center variants
    Resource,           ///< Resource processing
    Military,           ///< Unit production
    Defense,            ///< Defensive structures
    Research,           ///< Technology research
    Economic,           ///< Economic buildings
    Special,            ///< Unique/special buildings

    COUNT
};

[[nodiscard]] inline const char* BuildingCategoryToString(BuildingCategory cat) {
    switch (cat) {
        case BuildingCategory::MainHall: return "MainHall";
        case BuildingCategory::Resource: return "Resource";
        case BuildingCategory::Military: return "Military";
        case BuildingCategory::Defense: return "Defense";
        case BuildingCategory::Research: return "Research";
        case BuildingCategory::Economic: return "Economic";
        case BuildingCategory::Special: return "Special";
        default: return "Unknown";
    }
}

// ============================================================================
// Building Subtypes
// ============================================================================

enum class BuildingSubtype : uint8_t {
    // Main Hall
    TownCenter = 0,
    Castle,
    Fortress,

    // Resource
    Mine,
    LumberMill,
    Refinery,
    Farm,

    // Military
    Barracks,
    ArcheryRange,
    Stable,
    Factory,
    SiegeWorkshop,
    Dock,
    Airfield,

    // Defense
    Tower,
    Wall,
    Gate,
    Trap,
    Bunker,

    // Research
    Library,
    Workshop,
    Temple,
    Laboratory,

    // Economic
    Market,
    Bank,
    Warehouse,
    TradePost,

    // Special
    Altar,
    Portal,
    Wonder,
    Monument,

    COUNT
};

[[nodiscard]] const char* BuildingSubtypeToString(BuildingSubtype subtype);
[[nodiscard]] BuildingSubtype StringToBuildingSubtype(const std::string& str);

// ============================================================================
// Building Stats
// ============================================================================

struct BuildingBaseStats {
    int health = 500;
    int maxHealth = 500;
    int armor = 5;
    float buildTime = 30.0f;

    // Size
    int sizeX = 2;              ///< Width in tiles
    int sizeY = 2;              ///< Height in tiles

    // Vision
    float visionRange = 10.0f;

    // Garrison
    int garrisonCapacity = 0;
    float garrisonHealRate = 0.0f;

    // Attack (for towers, etc.)
    int damage = 0;
    float attackSpeed = 0.0f;
    float attackRange = 0.0f;

    [[nodiscard]] nlohmann::json ToJson() const;
    static BuildingBaseStats FromJson(const nlohmann::json& j);
};

// ============================================================================
// Building Cost
// ============================================================================

struct BuildingCost {
    int gold = 0;
    int wood = 0;
    int stone = 0;
    int food = 0;
    int metal = 0;

    [[nodiscard]] int GetTotalCost() const {
        return gold + wood + stone + food + metal;
    }

    [[nodiscard]] nlohmann::json ToJson() const;
    static BuildingCost FromJson(const nlohmann::json& j);
};

// ============================================================================
// Production Entry
// ============================================================================

struct ProductionEntry {
    std::string unitId;
    int queueLimit = 5;
    bool requiresTech;
    std::string requiredTech;

    [[nodiscard]] nlohmann::json ToJson() const;
    static ProductionEntry FromJson(const nlohmann::json& j);
};

// ============================================================================
// Resource Generation
// ============================================================================

struct ResourceGeneration {
    std::string resourceType;       ///< "gold", "food", etc.
    float ratePerSecond = 0.0f;
    float maxStorage = 0.0f;
    bool requiresWorker = false;
    float workerEfficiency = 1.0f;

    [[nodiscard]] nlohmann::json ToJson() const;
    static ResourceGeneration FromJson(const nlohmann::json& j);
};

// ============================================================================
// Building Archetype
// ============================================================================

struct BuildingArchetype {
    // Identity
    std::string id;
    std::string name;
    std::string description;
    std::string iconPath;

    // Classification
    BuildingCategory category = BuildingCategory::Military;
    BuildingSubtype subtype = BuildingSubtype::Barracks;

    // Stats
    BuildingBaseStats baseStats;
    BuildingCost cost;

    // Requirements
    std::string requiredBuilding;
    std::string requiredTech;
    int requiredAge = 0;

    // Production
    std::vector<ProductionEntry> productions;
    float productionSpeedModifier = 1.0f;

    // Research
    std::vector<std::string> availableResearch;
    float researchSpeedModifier = 1.0f;

    // Resources
    std::vector<ResourceGeneration> resourceGeneration;
    int populationProvided = 0;
    int populationRequired = 0;

    // Defense
    bool isDefensive = false;
    std::string projectileId;
    bool canAttackAir = false;
    bool canAttackGround = true;

    // Special properties
    bool isMainBase = false;
    bool canBeBuiltOnResource = false;
    bool isPackable = false;        ///< Can be packed/unpacked (nomad)
    bool providesDropOff = false;   ///< Workers can drop resources
    bool providesHealing = false;

    // Upgrade paths
    std::vector<std::string> upgradesTo;
    std::string upgradesFrom;

    // Visual
    std::string modelPath;
    std::string constructionModel;
    std::string destroyedModel;
    float modelScale = 1.0f;

    // Audio
    std::string constructSound;
    std::string completeSound;
    std::string selectSound;
    std::string destroySound;

    // Balance
    int pointCost = 5;
    float powerRating = 1.0f;

    // Tags
    std::vector<std::string> tags;

    // =========================================================================
    // Methods
    // =========================================================================

    [[nodiscard]] BuildingBaseStats CalculateStats(
        const std::map<std::string, float>& modifiers) const;

    [[nodiscard]] bool CanProduce(const std::string& unitId) const;
    [[nodiscard]] bool CanResearch(const std::string& techId) const;

    [[nodiscard]] bool Validate() const;
    [[nodiscard]] std::vector<std::string> GetValidationErrors() const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static BuildingArchetype FromJson(const nlohmann::json& j);

    bool SaveToFile(const std::string& filepath) const;
    bool LoadFromFile(const std::string& filepath);
};

// ============================================================================
// Building Archetype Registry
// ============================================================================

class BuildingArchetypeRegistry {
public:
    [[nodiscard]] static BuildingArchetypeRegistry& Instance();

    bool Initialize();
    void Shutdown();

    bool RegisterArchetype(const BuildingArchetype& archetype);
    bool UnregisterArchetype(const std::string& id);

    [[nodiscard]] const BuildingArchetype* GetArchetype(const std::string& id) const;
    [[nodiscard]] std::vector<const BuildingArchetype*> GetAllArchetypes() const;
    [[nodiscard]] std::vector<const BuildingArchetype*> GetByCategory(BuildingCategory cat) const;

    int LoadFromDirectory(const std::string& directory);

private:
    BuildingArchetypeRegistry() = default;

    bool m_initialized = false;
    std::map<std::string, BuildingArchetype> m_archetypes;

    void InitializeBuiltInArchetypes();
};

// ============================================================================
// Built-in Building Archetypes
// ============================================================================

// Main Hall
[[nodiscard]] BuildingArchetype CreateMainHallArchetype();
[[nodiscard]] BuildingArchetype CreateCastleArchetype();

// Resource
[[nodiscard]] BuildingArchetype CreateMineArchetype();
[[nodiscard]] BuildingArchetype CreateLumberMillArchetype();
[[nodiscard]] BuildingArchetype CreateFarmArchetype();

// Military
[[nodiscard]] BuildingArchetype CreateBarracksArchetype();
[[nodiscard]] BuildingArchetype CreateArcheryRangeArchetype();
[[nodiscard]] BuildingArchetype CreateStableArchetype();
[[nodiscard]] BuildingArchetype CreateSiegeWorkshopArchetype();
[[nodiscard]] BuildingArchetype CreateDockArchetype();

// Defense
[[nodiscard]] BuildingArchetype CreateTowerArchetype();
[[nodiscard]] BuildingArchetype CreateWallArchetype();
[[nodiscard]] BuildingArchetype CreateGateArchetype();

// Research
[[nodiscard]] BuildingArchetype CreateLibraryArchetype();
[[nodiscard]] BuildingArchetype CreateBlacksmithArchetype();
[[nodiscard]] BuildingArchetype CreateTempleArchetype();

// Economic
[[nodiscard]] BuildingArchetype CreateMarketArchetype();
[[nodiscard]] BuildingArchetype CreateWarehouseArchetype();

// Special
[[nodiscard]] BuildingArchetype CreateWonderArchetype();
[[nodiscard]] BuildingArchetype CreateAltarArchetype();

} // namespace Race
} // namespace RTS
} // namespace Vehement
