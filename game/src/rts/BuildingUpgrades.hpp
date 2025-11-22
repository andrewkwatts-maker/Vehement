#pragma once

#include "Building.hpp"
#include "Construction.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>

namespace Vehement {

// ============================================================================
// Upgrade Path Definitions
// ============================================================================

/**
 * @brief Defines upgrade progression for buildings
 *
 * Upgrade Paths:
 * Housing:
 *   Shelter (Lvl 1) -> House (Lvl 2) -> Manor (Lvl 3)
 *
 * Defense:
 *   WatchTower (Lvl 1) -> Guard Tower (Lvl 2) -> Fortress Tower (Lvl 3)
 *   Wall (Lvl 1) -> Reinforced Wall (Lvl 2) -> Stone Wall (Lvl 3)
 *   Gate (Lvl 1) -> Iron Gate (Lvl 2) -> Fortress Gate (Lvl 3)
 *
 * Production:
 *   Farm (Lvl 1) -> Large Farm (Lvl 2) -> Plantation (Lvl 3)
 *   LumberMill (Lvl 1) -> Sawmill (Lvl 2) -> Timber Factory (Lvl 3)
 *   Quarry (Lvl 1) -> Deep Quarry (Lvl 2) -> Mining Complex (Lvl 3)
 *   Workshop (Lvl 1) -> Forge (Lvl 2) -> Factory (Lvl 3)
 *
 * Special:
 *   TradingPost (Lvl 1) -> Market (Lvl 2) -> Trade Hub (Lvl 3)
 *   Hospital (Lvl 1) -> Clinic (Lvl 2) -> Medical Center (Lvl 3)
 *   Warehouse (Lvl 1) -> Depot (Lvl 2) -> Distribution Center (Lvl 3)
 *   CommandCenter: No upgrade path (unique building)
 *   Fortress: No upgrade path (already top-tier defense)
 */

/**
 * @brief Information about an upgrade level
 */
struct UpgradeLevelInfo {
    int level = 1;
    std::string name;
    std::string description;

    // Stat multipliers (relative to base level 1)
    float healthMultiplier = 1.0f;
    float capacityMultiplier = 1.0f;
    float productionMultiplier = 1.0f;
    float defenseMultiplier = 1.0f;
    float rangeMultiplier = 1.0f;

    // Visual changes
    std::string textureOverride;    // Different texture for this level
    float heightMultiplier = 1.0f;  // Building gets taller

    // Requirements
    BuildingCost cost;
    int requiredCommandCenterLevel = 1;
    std::vector<std::pair<BuildingType, int>> buildingRequirements;  // Other buildings needed
};

/**
 * @brief Get upgrade info for a building type at a specific level
 */
inline UpgradeLevelInfo GetUpgradeInfo(BuildingType type, int level) {
    UpgradeLevelInfo info;
    info.level = level;

    switch (type) {
        // =====================================================================
        // Housing Upgrades
        // =====================================================================
        case BuildingType::Shelter:
            switch (level) {
                case 1:
                    info.name = "Shelter";
                    info.description = "Basic shelter with room for 2 workers.";
                    info.healthMultiplier = 1.0f;
                    info.capacityMultiplier = 1.0f;
                    break;
                case 2:
                    info.name = "House";
                    info.description = "Comfortable house with room for 3 workers.";
                    info.healthMultiplier = 1.5f;
                    info.capacityMultiplier = 1.5f;
                    info.heightMultiplier = 1.2f;
                    info.cost = GetBuildingCost(BuildingType::Shelter) * 1.5f;
                    break;
                case 3:
                    info.name = "Manor";
                    info.description = "Large manor housing 5 workers in comfort.";
                    info.healthMultiplier = 2.0f;
                    info.capacityMultiplier = 2.5f;
                    info.heightMultiplier = 1.5f;
                    info.cost = GetBuildingCost(BuildingType::Shelter) * 2.5f;
                    info.requiredCommandCenterLevel = 2;
                    break;
            }
            break;

        case BuildingType::House:
            switch (level) {
                case 1:
                    info.name = "House";
                    info.description = "Standard house for 4 workers.";
                    break;
                case 2:
                    info.name = "Large House";
                    info.description = "Spacious house for 6 workers.";
                    info.healthMultiplier = 1.4f;
                    info.capacityMultiplier = 1.5f;
                    info.cost = GetBuildingCost(BuildingType::House) * 1.5f;
                    break;
                case 3:
                    info.name = "Estate";
                    info.description = "Grand estate housing 10 workers.";
                    info.healthMultiplier = 2.0f;
                    info.capacityMultiplier = 2.5f;
                    info.cost = GetBuildingCost(BuildingType::House) * 2.5f;
                    info.requiredCommandCenterLevel = 2;
                    break;
            }
            break;

        case BuildingType::Barracks:
            switch (level) {
                case 1:
                    info.name = "Barracks";
                    info.description = "Military housing for 8 soldiers.";
                    break;
                case 2:
                    info.name = "Garrison";
                    info.description = "Fortified garrison for 12 soldiers.";
                    info.healthMultiplier = 1.5f;
                    info.capacityMultiplier = 1.5f;
                    info.defenseMultiplier = 1.2f;
                    info.cost = GetBuildingCost(BuildingType::Barracks) * 1.8f;
                    break;
                case 3:
                    info.name = "Military Compound";
                    info.description = "Large compound housing 20 soldiers.";
                    info.healthMultiplier = 2.0f;
                    info.capacityMultiplier = 2.5f;
                    info.defenseMultiplier = 1.5f;
                    info.cost = GetBuildingCost(BuildingType::Barracks) * 3.0f;
                    info.requiredCommandCenterLevel = 3;
                    break;
            }
            break;

        // =====================================================================
        // Production Upgrades
        // =====================================================================
        case BuildingType::Farm:
            switch (level) {
                case 1:
                    info.name = "Farm";
                    info.description = "Basic farm producing 20 food/min.";
                    break;
                case 2:
                    info.name = "Large Farm";
                    info.description = "Expanded farm producing 35 food/min.";
                    info.productionMultiplier = 1.75f;
                    info.capacityMultiplier = 1.5f;
                    info.cost = GetBuildingCost(BuildingType::Farm) * 1.5f;
                    break;
                case 3:
                    info.name = "Plantation";
                    info.description = "Industrial plantation producing 60 food/min.";
                    info.productionMultiplier = 3.0f;
                    info.capacityMultiplier = 2.0f;
                    info.cost = GetBuildingCost(BuildingType::Farm) * 2.5f;
                    info.requiredCommandCenterLevel = 2;
                    break;
            }
            break;

        case BuildingType::LumberMill:
            switch (level) {
                case 1:
                    info.name = "Lumber Mill";
                    info.description = "Basic mill producing 15 wood/min.";
                    break;
                case 2:
                    info.name = "Sawmill";
                    info.description = "Efficient sawmill producing 25 wood/min.";
                    info.productionMultiplier = 1.67f;
                    info.cost = GetBuildingCost(BuildingType::LumberMill) * 1.5f;
                    break;
                case 3:
                    info.name = "Timber Factory";
                    info.description = "Industrial factory producing 45 wood/min.";
                    info.productionMultiplier = 3.0f;
                    info.cost = GetBuildingCost(BuildingType::LumberMill) * 2.5f;
                    info.requiredCommandCenterLevel = 2;
                    break;
            }
            break;

        case BuildingType::Quarry:
            switch (level) {
                case 1:
                    info.name = "Quarry";
                    info.description = "Open pit quarry producing 12 stone/min.";
                    break;
                case 2:
                    info.name = "Deep Quarry";
                    info.description = "Deep quarry producing 20 stone/min.";
                    info.productionMultiplier = 1.67f;
                    info.cost = GetBuildingCost(BuildingType::Quarry) * 1.6f;
                    break;
                case 3:
                    info.name = "Mining Complex";
                    info.description = "Mining complex producing 35 stone/min.";
                    info.productionMultiplier = 2.9f;
                    info.cost = GetBuildingCost(BuildingType::Quarry) * 2.8f;
                    info.requiredCommandCenterLevel = 2;
                    break;
            }
            break;

        case BuildingType::Workshop:
            switch (level) {
                case 1:
                    info.name = "Workshop";
                    info.description = "Basic workshop for crafting items.";
                    break;
                case 2:
                    info.name = "Forge";
                    info.description = "Advanced forge with faster crafting.";
                    info.productionMultiplier = 1.5f;
                    info.cost = GetBuildingCost(BuildingType::Workshop) * 1.8f;
                    info.requiredCommandCenterLevel = 2;
                    break;
                case 3:
                    info.name = "Factory";
                    info.description = "Industrial factory with maximum efficiency.";
                    info.productionMultiplier = 2.5f;
                    info.cost = GetBuildingCost(BuildingType::Workshop) * 3.0f;
                    info.requiredCommandCenterLevel = 3;
                    break;
            }
            break;

        // =====================================================================
        // Defense Upgrades
        // =====================================================================
        case BuildingType::WatchTower:
            switch (level) {
                case 1:
                    info.name = "Watch Tower";
                    info.description = "Basic tower with 15 damage, 12 range.";
                    break;
                case 2:
                    info.name = "Guard Tower";
                    info.description = "Fortified tower with 25 damage, 16 range.";
                    info.defenseMultiplier = 1.67f;
                    info.rangeMultiplier = 1.33f;
                    info.healthMultiplier = 1.5f;
                    info.cost = GetBuildingCost(BuildingType::WatchTower) * 2.0f;
                    break;
                case 3:
                    info.name = "Fortress Tower";
                    info.description = "Massive tower with 40 damage, 20 range.";
                    info.defenseMultiplier = 2.67f;
                    info.rangeMultiplier = 1.67f;
                    info.healthMultiplier = 2.5f;
                    info.cost = GetBuildingCost(BuildingType::WatchTower) * 3.5f;
                    info.requiredCommandCenterLevel = 2;
                    break;
            }
            break;

        case BuildingType::Wall:
            switch (level) {
                case 1:
                    info.name = "Wooden Wall";
                    info.description = "Basic wooden wall with 500 HP.";
                    break;
                case 2:
                    info.name = "Reinforced Wall";
                    info.description = "Reinforced wall with 800 HP.";
                    info.healthMultiplier = 1.6f;
                    info.cost = GetBuildingCost(BuildingType::Wall) * 1.5f;
                    break;
                case 3:
                    info.name = "Stone Wall";
                    info.description = "Solid stone wall with 1500 HP.";
                    info.healthMultiplier = 3.0f;
                    info.cost = GetBuildingCost(BuildingType::Wall) * 2.5f;
                    info.requiredCommandCenterLevel = 2;
                    break;
            }
            break;

        case BuildingType::Gate:
            switch (level) {
                case 1:
                    info.name = "Wooden Gate";
                    info.description = "Basic gate with 400 HP.";
                    break;
                case 2:
                    info.name = "Iron Gate";
                    info.description = "Reinforced iron gate with 700 HP.";
                    info.healthMultiplier = 1.75f;
                    info.cost = GetBuildingCost(BuildingType::Gate) * 1.8f;
                    break;
                case 3:
                    info.name = "Fortress Gate";
                    info.description = "Massive fortress gate with 1200 HP.";
                    info.healthMultiplier = 3.0f;
                    info.cost = GetBuildingCost(BuildingType::Gate) * 3.0f;
                    info.requiredCommandCenterLevel = 2;
                    break;
            }
            break;

        // =====================================================================
        // Special Building Upgrades
        // =====================================================================
        case BuildingType::TradingPost:
            switch (level) {
                case 1:
                    info.name = "Trading Post";
                    info.description = "Basic trading with standard rates.";
                    break;
                case 2:
                    info.name = "Market";
                    info.description = "Better rates and more trade options.";
                    info.productionMultiplier = 1.2f;  // Better exchange rates
                    info.cost = GetBuildingCost(BuildingType::TradingPost) * 1.8f;
                    break;
                case 3:
                    info.name = "Trade Hub";
                    info.description = "Regional hub with best rates.";
                    info.productionMultiplier = 1.5f;
                    info.cost = GetBuildingCost(BuildingType::TradingPost) * 3.0f;
                    info.requiredCommandCenterLevel = 2;
                    break;
            }
            break;

        case BuildingType::Hospital:
            switch (level) {
                case 1:
                    info.name = "Hospital";
                    info.description = "Basic medical facility.";
                    break;
                case 2:
                    info.name = "Clinic";
                    info.description = "Advanced clinic with faster healing.";
                    info.productionMultiplier = 1.5f;  // Faster healing
                    info.cost = GetBuildingCost(BuildingType::Hospital) * 1.8f;
                    break;
                case 3:
                    info.name = "Medical Center";
                    info.description = "Full medical center with research.";
                    info.productionMultiplier = 2.5f;
                    info.cost = GetBuildingCost(BuildingType::Hospital) * 3.0f;
                    info.requiredCommandCenterLevel = 2;
                    break;
            }
            break;

        case BuildingType::Warehouse:
            switch (level) {
                case 1:
                    info.name = "Warehouse";
                    info.description = "Basic storage facility (+200 capacity).";
                    break;
                case 2:
                    info.name = "Depot";
                    info.description = "Large depot (+400 capacity).";
                    info.capacityMultiplier = 2.0f;
                    info.cost = GetBuildingCost(BuildingType::Warehouse) * 1.5f;
                    break;
                case 3:
                    info.name = "Distribution Center";
                    info.description = "Massive center (+800 capacity).";
                    info.capacityMultiplier = 4.0f;
                    info.cost = GetBuildingCost(BuildingType::Warehouse) * 2.5f;
                    info.requiredCommandCenterLevel = 2;
                    break;
            }
            break;

        case BuildingType::CommandCenter:
            switch (level) {
                case 1:
                    info.name = "Command Center";
                    info.description = "Basic command post.";
                    break;
                case 2:
                    info.name = "Command Headquarters";
                    info.description = "Upgraded HQ unlocking advanced buildings.";
                    info.healthMultiplier = 1.5f;
                    info.cost = GetBuildingCost(BuildingType::CommandCenter) * 2.0f;
                    break;
                case 3:
                    info.name = "Command Fortress";
                    info.description = "Fortified command center unlocking all.";
                    info.healthMultiplier = 2.0f;
                    info.defenseMultiplier = 1.5f;
                    info.cost = GetBuildingCost(BuildingType::CommandCenter) * 3.5f;
                    break;
            }
            break;

        case BuildingType::Fortress:
            // Fortress has no upgrade path - it's the pinnacle of defense
            info.name = "Fortress";
            info.description = "Ultimate defensive structure.";
            break;

        default:
            info.name = GetBuildingTypeName(type);
            info.description = GetBuildingDescription(type);
            break;
    }

    return info;
}

/**
 * @brief Get the maximum level for a building type
 */
inline int GetMaxLevel(BuildingType type) {
    switch (type) {
        case BuildingType::Fortress:
            return 1;  // Fortress cannot be upgraded
        default:
            return 3;  // Most buildings have 3 levels
    }
}

/**
 * @brief Check if a building can be upgraded
 */
inline bool CanUpgrade(BuildingType type, int currentLevel) {
    return currentLevel < GetMaxLevel(type);
}

// ============================================================================
// Upgrade Requirements Check
// ============================================================================

/**
 * @brief Result of upgrade requirement check
 */
struct UpgradeRequirementResult {
    bool canUpgrade = false;
    std::string reason;
    BuildingCost cost;
    std::vector<std::string> missingRequirements;
};

/**
 * @brief Check if a building can be upgraded given current game state
 */
inline UpgradeRequirementResult CheckUpgradeRequirements(
    Building* building,
    const Construction& construction,
    const ResourceStockpile& resources
) {
    UpgradeRequirementResult result;
    result.canUpgrade = true;

    if (!building) {
        result.canUpgrade = false;
        result.reason = "Invalid building.";
        return result;
    }

    // Check if building is operational
    if (!building->IsOperational()) {
        result.canUpgrade = false;
        result.reason = "Building must be operational to upgrade.";
        return result;
    }

    // Check max level
    int currentLevel = building->GetLevel();
    if (!CanUpgrade(building->GetBuildingType(), currentLevel)) {
        result.canUpgrade = false;
        result.reason = "Building is already at maximum level.";
        return result;
    }

    // Get upgrade info
    UpgradeLevelInfo info = GetUpgradeInfo(building->GetBuildingType(), currentLevel + 1);
    result.cost = info.cost;

    // Check Command Center level requirement
    Building* commandCenter = construction.GetCommandCenter();
    int ccLevel = commandCenter ? commandCenter->GetLevel() : 0;
    if (ccLevel < info.requiredCommandCenterLevel) {
        result.canUpgrade = false;
        result.missingRequirements.push_back(
            "Command Center level " + std::to_string(info.requiredCommandCenterLevel) + " required."
        );
    }

    // Check building requirements
    for (const auto& [reqType, reqCount] : info.buildingRequirements) {
        int count = construction.GetBuildingCount(reqType);
        if (count < reqCount) {
            result.canUpgrade = false;
            result.missingRequirements.push_back(
                std::to_string(reqCount) + "x " + GetBuildingTypeName(reqType) + " required."
            );
        }
    }

    // Check resources
    if (!resources.CanAfford(info.cost)) {
        result.canUpgrade = false;
        result.missingRequirements.push_back("Insufficient resources.");
    }

    if (!result.missingRequirements.empty()) {
        result.reason = result.missingRequirements[0];
    }

    return result;
}

// ============================================================================
// Upgrade Manager
// ============================================================================

/**
 * @brief Manages building upgrades and tech tree
 */
class UpgradeManager {
public:
    UpgradeManager() = default;
    ~UpgradeManager() = default;

    /**
     * @brief Initialize with construction reference
     */
    void Initialize(Construction* construction, ResourceStockpile* resources) {
        m_construction = construction;
        m_resources = resources;
    }

    /**
     * @brief Get upgrade info for a building
     */
    [[nodiscard]] UpgradeLevelInfo GetUpgradeInfo(Building* building) const {
        if (!building) return UpgradeLevelInfo{};
        return Vehement::GetUpgradeInfo(building->GetBuildingType(), building->GetLevel() + 1);
    }

    /**
     * @brief Check if building can be upgraded
     */
    [[nodiscard]] UpgradeRequirementResult CheckRequirements(Building* building) const {
        if (!m_construction || !m_resources) {
            UpgradeRequirementResult result;
            result.canUpgrade = false;
            result.reason = "System not initialized.";
            return result;
        }
        return CheckUpgradeRequirements(building, *m_construction, *m_resources);
    }

    /**
     * @brief Start upgrading a building
     */
    bool StartUpgrade(Building* building) {
        if (!m_construction || !m_resources || !building) return false;

        auto result = CheckRequirements(building);
        if (!result.canUpgrade) return false;

        // Spend resources
        if (!m_resources->Spend(result.cost)) return false;

        // Start upgrade via construction system
        return m_construction->UpgradeBuilding(building, *m_resources);
    }

    /**
     * @brief Get current level name for a building
     */
    [[nodiscard]] std::string GetLevelName(Building* building) const {
        if (!building) return "Unknown";
        auto info = Vehement::GetUpgradeInfo(building->GetBuildingType(), building->GetLevel());
        return info.name;
    }

    /**
     * @brief Get all available upgrades
     */
    [[nodiscard]] std::vector<std::pair<Building*, UpgradeLevelInfo>> GetAvailableUpgrades() const {
        std::vector<std::pair<Building*, UpgradeLevelInfo>> upgrades;

        if (!m_construction) return upgrades;

        for (const auto& building : m_construction->GetBuildings()) {
            if (!building || !building->IsOperational()) continue;
            if (!CanUpgrade(building->GetBuildingType(), building->GetLevel())) continue;

            auto result = CheckRequirements(building.get());
            if (result.canUpgrade) {
                upgrades.emplace_back(
                    building.get(),
                    Vehement::GetUpgradeInfo(building->GetBuildingType(), building->GetLevel() + 1)
                );
            }
        }

        return upgrades;
    }

    /**
     * @brief Get upgrade path display for UI
     */
    [[nodiscard]] std::vector<UpgradeLevelInfo> GetUpgradePath(BuildingType type) const {
        std::vector<UpgradeLevelInfo> path;
        int maxLevel = GetMaxLevel(type);

        for (int level = 1; level <= maxLevel; ++level) {
            path.push_back(Vehement::GetUpgradeInfo(type, level));
        }

        return path;
    }

private:
    Construction* m_construction = nullptr;
    ResourceStockpile* m_resources = nullptr;
};

// ============================================================================
// Storage Capacity from Warehouses
// ============================================================================

/**
 * @brief Get storage capacity bonus from warehouses
 */
inline int GetStorageCapacityBonus(const Construction& construction) {
    int bonus = 0;

    for (const auto& building : construction.GetBuildings()) {
        if (!building || !building->IsOperational()) continue;
        if (building->GetBuildingType() != BuildingType::Warehouse) continue;

        auto info = GetUpgradeInfo(BuildingType::Warehouse, building->GetLevel());

        // Base warehouse adds 200, upgrades multiply this
        bonus += static_cast<int>(200 * info.capacityMultiplier);
    }

    return bonus;
}

} // namespace Vehement
