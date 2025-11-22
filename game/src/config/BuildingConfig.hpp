#pragma once

#include "EntityConfig.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace Vehement {
namespace Config {

// ============================================================================
// Resource Cost
// ============================================================================

/**
 * @brief Resource cost for construction or production
 */
struct ResourceCost {
    std::unordered_map<ResourceType, int> resources;

    int GetCost(ResourceType type) const {
        auto it = resources.find(type);
        return (it != resources.end()) ? it->second : 0;
    }

    void SetCost(ResourceType type, int amount) {
        if (amount > 0) {
            resources[type] = amount;
        } else {
            resources.erase(type);
        }
    }

    bool IsEmpty() const { return resources.empty(); }
};

// ============================================================================
// Construction Stage
// ============================================================================

/**
 * @brief A single construction stage with its own model
 */
struct ConstructionStage {
    std::string name;                // e.g., "foundation", "framing", "complete"
    std::string modelPath;           // Model for this stage
    float progressStart = 0.0f;      // 0-100 when this stage begins
    float progressEnd = 100.0f;      // 0-100 when this stage ends
    std::string effectPath;          // Particle effect during this stage
};

// ============================================================================
// Production Configuration
// ============================================================================

/**
 * @brief Production capability for a building
 */
struct ProductionCapability {
    enum class ProductionType {
        Unit,           // Produces units
        Resource,       // Generates resources
        Research,       // Unlocks technologies
        Item            // Crafts items
    };

    ProductionType type = ProductionType::Resource;
    std::string outputId;            // Unit ID, Resource type, Tech ID, or Item ID
    int outputAmount = 1;            // Amount produced per cycle
    float productionTime = 10.0f;    // Seconds per production cycle
    ResourceCost cost;               // Cost per production
    int maxQueue = 5;                // Max items in queue (for units)

    // Requirements
    std::vector<std::string> requiredTechs;
    int requiredBuildingLevel = 1;
};

// ============================================================================
// Upgrade Path
// ============================================================================

/**
 * @brief Building upgrade definition
 */
struct BuildingUpgrade {
    std::string upgradeId;
    std::string name;
    std::string description;

    int targetLevel = 0;             // 0 = transforms to different building
    std::string transformsTo;        // Building ID if transforms

    ResourceCost cost;
    float upgradeTime = 30.0f;       // Seconds
    std::vector<std::string> requiredTechs;

    // Stat modifiers
    float healthMultiplier = 1.0f;
    float productionMultiplier = 1.0f;
    float capacityMultiplier = 1.0f;
};

// ============================================================================
// Building Footprint
// ============================================================================

/**
 * @brief Building footprint on the grid
 */
struct BuildingFootprint {
    GridType gridType = GridType::Rect;
    glm::ivec2 size{1, 1};           // Size in grid cells

    // For hex grids, which cells are occupied (relative to center)
    std::vector<glm::ivec2> occupiedCells;

    // Entry/exit points
    std::vector<glm::ivec2> entrancePositions;

    // Visual bounds (may differ from collision)
    glm::vec3 visualSize{1.0f, 1.0f, 1.0f};
};

// ============================================================================
// Building Configuration
// ============================================================================

/**
 * @brief Complete configuration for a building
 *
 * Supports:
 * - Footprint size (hex or rect grid)
 * - Construction stages with models
 * - Resource costs and build time
 * - Production capabilities
 * - Garrison capacity
 * - Power/resource consumption
 * - Upgrade paths
 * - Python hooks: on_construct_start, on_construct_complete, on_destroyed,
 *                 on_capture, on_produce
 */
class BuildingConfig : public EntityConfig {
public:
    BuildingConfig() = default;
    ~BuildingConfig() override = default;

    [[nodiscard]] std::string GetConfigType() const override { return "building"; }

    ValidationResult Validate() const override;
    void ApplyBaseConfig(const EntityConfig& baseConfig) override;

    // =========================================================================
    // Footprint
    // =========================================================================

    [[nodiscard]] const BuildingFootprint& GetFootprint() const { return m_footprint; }
    void SetFootprint(const BuildingFootprint& footprint) { m_footprint = footprint; }

    [[nodiscard]] glm::ivec2 GetSize() const { return m_footprint.size; }
    [[nodiscard]] GridType GetGridType() const { return m_footprint.gridType; }

    // =========================================================================
    // Construction
    // =========================================================================

    [[nodiscard]] const ResourceCost& GetConstructionCost() const { return m_constructionCost; }
    void SetConstructionCost(const ResourceCost& cost) { m_constructionCost = cost; }

    [[nodiscard]] float GetBuildTime() const { return m_buildTime; }
    void SetBuildTime(float time) { m_buildTime = time; }

    [[nodiscard]] const std::vector<ConstructionStage>& GetConstructionStages() const {
        return m_constructionStages;
    }
    void SetConstructionStages(const std::vector<ConstructionStage>& stages) {
        m_constructionStages = stages;
    }

    [[nodiscard]] const ConstructionStage* GetStageForProgress(float progress) const;

    // =========================================================================
    // Stats
    // =========================================================================

    [[nodiscard]] float GetMaxHealth() const { return m_maxHealth; }
    void SetMaxHealth(float health) { m_maxHealth = health; }

    [[nodiscard]] float GetArmor() const { return m_armor; }
    void SetArmor(float armor) { m_armor = armor; }

    [[nodiscard]] int GetMaxLevel() const { return m_maxLevel; }
    void SetMaxLevel(int level) { m_maxLevel = level; }

    // =========================================================================
    // Production
    // =========================================================================

    [[nodiscard]] const std::vector<ProductionCapability>& GetProductionCapabilities() const {
        return m_productionCapabilities;
    }
    void SetProductionCapabilities(const std::vector<ProductionCapability>& caps) {
        m_productionCapabilities = caps;
    }
    void AddProductionCapability(const ProductionCapability& cap) {
        m_productionCapabilities.push_back(cap);
    }

    [[nodiscard]] bool CanProduceUnit(const std::string& unitId) const;
    [[nodiscard]] bool CanProduceResource(ResourceType type) const;

    // =========================================================================
    // Garrison
    // =========================================================================

    [[nodiscard]] int GetGarrisonCapacity() const { return m_garrisonCapacity; }
    void SetGarrisonCapacity(int capacity) { m_garrisonCapacity = capacity; }

    [[nodiscard]] const std::vector<std::string>& GetAllowedGarrisonTypes() const {
        return m_allowedGarrisonTypes;
    }
    void SetAllowedGarrisonTypes(const std::vector<std::string>& types) {
        m_allowedGarrisonTypes = types;
    }

    // =========================================================================
    // Resource Consumption
    // =========================================================================

    [[nodiscard]] const ResourceCost& GetUpkeepCost() const { return m_upkeepCost; }
    void SetUpkeepCost(const ResourceCost& cost) { m_upkeepCost = cost; }

    [[nodiscard]] float GetPowerConsumption() const { return m_powerConsumption; }
    void SetPowerConsumption(float power) { m_powerConsumption = power; }

    [[nodiscard]] float GetPowerProduction() const { return m_powerProduction; }
    void SetPowerProduction(float power) { m_powerProduction = power; }

    // =========================================================================
    // Upgrades
    // =========================================================================

    [[nodiscard]] const std::vector<BuildingUpgrade>& GetUpgrades() const { return m_upgrades; }
    void SetUpgrades(const std::vector<BuildingUpgrade>& upgrades) { m_upgrades = upgrades; }
    void AddUpgrade(const BuildingUpgrade& upgrade) { m_upgrades.push_back(upgrade); }

    [[nodiscard]] const BuildingUpgrade* GetUpgrade(const std::string& upgradeId) const;

    // =========================================================================
    // Defense
    // =========================================================================

    [[nodiscard]] bool HasDefense() const { return m_attackDamage > 0; }
    [[nodiscard]] float GetAttackDamage() const { return m_attackDamage; }
    [[nodiscard]] float GetAttackRange() const { return m_attackRange; }
    [[nodiscard]] float GetAttackSpeed() const { return m_attackSpeed; }

    void SetAttackDamage(float damage) { m_attackDamage = damage; }
    void SetAttackRange(float range) { m_attackRange = range; }
    void SetAttackSpeed(float speed) { m_attackSpeed = speed; }

    // =========================================================================
    // Vision
    // =========================================================================

    [[nodiscard]] float GetVisionRange() const { return m_visionRange; }
    void SetVisionRange(float range) { m_visionRange = range; }

    // =========================================================================
    // Requirements
    // =========================================================================

    [[nodiscard]] const std::vector<std::string>& GetRequiredTechs() const {
        return m_requiredTechs;
    }
    void SetRequiredTechs(const std::vector<std::string>& techs) { m_requiredTechs = techs; }

    [[nodiscard]] const std::vector<std::string>& GetRequiredBuildings() const {
        return m_requiredBuildings;
    }
    void SetRequiredBuildings(const std::vector<std::string>& buildings) {
        m_requiredBuildings = buildings;
    }

    // =========================================================================
    // Python Script Hooks
    // =========================================================================

    [[nodiscard]] std::string GetOnConstructStartScript() const {
        return GetScriptHook("on_construct_start");
    }
    [[nodiscard]] std::string GetOnConstructCompleteScript() const {
        return GetScriptHook("on_construct_complete");
    }
    [[nodiscard]] std::string GetOnDestroyedScript() const {
        return GetScriptHook("on_destroyed");
    }
    [[nodiscard]] std::string GetOnCaptureScript() const {
        return GetScriptHook("on_capture");
    }
    [[nodiscard]] std::string GetOnProduceScript() const {
        return GetScriptHook("on_produce");
    }

    void SetOnConstructStartScript(const std::string& path) {
        SetScriptHook("on_construct_start", path);
    }
    void SetOnConstructCompleteScript(const std::string& path) {
        SetScriptHook("on_construct_complete", path);
    }
    void SetOnDestroyedScript(const std::string& path) {
        SetScriptHook("on_destroyed", path);
    }
    void SetOnCaptureScript(const std::string& path) {
        SetScriptHook("on_capture", path);
    }
    void SetOnProduceScript(const std::string& path) {
        SetScriptHook("on_produce", path);
    }

    // =========================================================================
    // Classification
    // =========================================================================

    [[nodiscard]] const std::string& GetBuildingCategory() const { return m_category; }
    void SetBuildingCategory(const std::string& category) { m_category = category; }

    [[nodiscard]] const std::string& GetFaction() const { return m_faction; }
    void SetFaction(const std::string& faction) { m_faction = faction; }

    [[nodiscard]] bool IsUnique() const { return m_isUnique; }
    void SetIsUnique(bool unique) { m_isUnique = unique; }

    [[nodiscard]] int GetMaxCount() const { return m_maxCount; }
    void SetMaxCount(int count) { m_maxCount = count; }

protected:
    void ParseTypeSpecificFields(const std::string& jsonContent) override;
    std::string SerializeTypeSpecificFields() const override;

private:
    std::string GetScriptHook(const std::string& hookName) const;
    void SetScriptHook(const std::string& hookName, const std::string& path);

    // Footprint
    BuildingFootprint m_footprint;

    // Construction
    ResourceCost m_constructionCost;
    float m_buildTime = 30.0f;
    std::vector<ConstructionStage> m_constructionStages;

    // Stats
    float m_maxHealth = 500.0f;
    float m_armor = 5.0f;
    int m_maxLevel = 3;

    // Production
    std::vector<ProductionCapability> m_productionCapabilities;

    // Garrison
    int m_garrisonCapacity = 0;
    std::vector<std::string> m_allowedGarrisonTypes;

    // Resources
    ResourceCost m_upkeepCost;
    float m_powerConsumption = 0.0f;
    float m_powerProduction = 0.0f;

    // Upgrades
    std::vector<BuildingUpgrade> m_upgrades;

    // Defense
    float m_attackDamage = 0.0f;
    float m_attackRange = 0.0f;
    float m_attackSpeed = 1.0f;

    // Vision
    float m_visionRange = 10.0f;

    // Requirements
    std::vector<std::string> m_requiredTechs;
    std::vector<std::string> m_requiredBuildings;

    // Script hooks
    std::unordered_map<std::string, std::string> m_scriptHooks;

    // Classification
    std::string m_category;          // e.g., "housing", "production", "defense"
    std::string m_faction;
    bool m_isUnique = false;         // Only one can be built
    int m_maxCount = -1;             // -1 = unlimited
};

// Register the building config type
REGISTER_CONFIG_TYPE("building", BuildingConfig)

} // namespace Config
} // namespace Vehement
