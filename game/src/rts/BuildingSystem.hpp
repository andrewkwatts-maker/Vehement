#pragma once

/**
 * @file BuildingSystem.hpp
 * @brief Unified header for the RTS Building and Construction System
 *
 * This header includes all components of the building system:
 * - Building: Base building class with types and properties
 * - Construction: Building placement, construction, and demolition
 * - BuildingProduction: Resource generation from production buildings
 * - Defense: Defensive structures, walls, gates, and targeting
 * - BuildingUpgrades: Upgrade paths and tech tree
 *
 * Usage:
 * @code
 * #include "rts/BuildingSystem.hpp"
 *
 * // Create construction system
 * Vehement::Construction construction;
 * construction.Initialize(&world, &tileMap);
 *
 * // Create resource stockpile
 * Vehement::ResourceStockpile resources;
 * resources.Reset(100, 50, 20, 50, 100);
 *
 * // Place a building
 * construction.StartPlacement(Vehement::BuildingType::Farm);
 * construction.UpdateGhostPosition({10, 10});
 * auto* building = construction.ConfirmPlacement(resources);
 *
 * // Production system
 * Vehement::ProductionManager production;
 * production.Initialize(&construction, &resources);
 * production.Update(deltaTime);
 *
 * // Defense system
 * Vehement::DefenseManager defense;
 * defense.Initialize(&world, &construction, &tileMap);
 * defense.Update(deltaTime);
 *
 * // Upgrade system
 * Vehement::UpgradeManager upgrades;
 * upgrades.Initialize(&construction, &resources);
 * if (upgrades.CheckRequirements(building).canUpgrade) {
 *     upgrades.StartUpgrade(building);
 * }
 * @endcode
 *
 * Building Types:
 * ---------------
 *
 * HOUSING (provide living space):
 * - Shelter: Basic housing for 2 workers
 * - House: Standard housing for 4 workers
 * - Barracks: Military housing for 8 workers
 *
 * PRODUCTION (generate resources):
 * - Farm: Produces food (20/min base)
 * - LumberMill: Processes wood (15/min base)
 * - Quarry: Processes stone (12/min base)
 * - Workshop: Crafts items and equipment
 *
 * DEFENSE (protect your base):
 * - WatchTower: Ranged attack, reveals fog of war
 * - Wall: Blocks movement and zombie pathfinding
 * - Gate: Controlled passage (can open/close)
 * - Fortress: Heavy defense, hero respawn point
 *
 * SPECIAL (utility buildings):
 * - TradingPost: Buy/sell resources
 * - Hospital: Heal injured workers
 * - Warehouse: Increase storage capacity
 * - CommandCenter: Main base building (required first)
 *
 * Textures:
 * ---------
 * Buildings use textures from Vehement2/images/:
 * - Wood: Wood1.png, WoodFlooring1-2.png, WoodCrate1-2.png
 * - Stone: StoneRaw.png, StoneMarble1-2.png, StoneBlack.png
 * - Bricks: BricksRock.png, BricksStacked.png, BricksGrey.png
 * - Metal: Metal1-4.png, MetalTile1-4.png
 */

#include "Building.hpp"
#include "Construction.hpp"
#include "BuildingProduction.hpp"
#include "Defense.hpp"
#include "BuildingUpgrades.hpp"

namespace Vehement {

// ============================================================================
// Building System Integration
// ============================================================================

/**
 * @brief Complete RTS building system manager
 *
 * Integrates all building subsystems into a single interface
 */
class BuildingSystem {
public:
    BuildingSystem() = default;
    ~BuildingSystem() = default;

    /**
     * @brief Initialize all subsystems
     */
    void Initialize(World* world, TileMap* tileMap) {
        m_world = world;
        m_tileMap = tileMap;

        m_construction.Initialize(world, tileMap);
        m_production.Initialize(&m_construction, &m_resources);
        m_defense.Initialize(world, &m_construction, tileMap);
        m_upgrades.Initialize(&m_construction, &m_resources);
    }

    /**
     * @brief Update all subsystems
     */
    void Update(float deltaTime) {
        m_construction.Update(deltaTime);
        m_production.Update(deltaTime);
        m_defense.Update(deltaTime);
    }

    /**
     * @brief Render building effects
     */
    void Render(Nova::Renderer& renderer, const Nova::Camera& camera) {
        m_construction.Render(renderer, camera);
        m_defense.Render(renderer);
    }

    // Accessors
    Construction& GetConstruction() { return m_construction; }
    const Construction& GetConstruction() const { return m_construction; }

    ProductionManager& GetProduction() { return m_production; }
    const ProductionManager& GetProduction() const { return m_production; }

    DefenseManager& GetDefense() { return m_defense; }
    const DefenseManager& GetDefense() const { return m_defense; }

    UpgradeManager& GetUpgrades() { return m_upgrades; }
    const UpgradeManager& GetUpgrades() const { return m_upgrades; }

    ResourceStockpile& GetResources() { return m_resources; }
    const ResourceStockpile& GetResources() const { return m_resources; }

    // =========================================================================
    // Quick Actions
    // =========================================================================

    /**
     * @brief Place and start building construction
     */
    Building* PlaceBuilding(BuildingType type, const glm::ivec2& position) {
        m_construction.StartPlacement(type);
        m_construction.UpdateGhostPosition(position);
        return m_construction.ConfirmPlacement(m_resources);
    }

    /**
     * @brief Upgrade a building if possible
     */
    bool UpgradeBuilding(Building* building) {
        return m_upgrades.StartUpgrade(building);
    }

    /**
     * @brief Demolish a building
     */
    bool DemolishBuilding(Building* building) {
        return m_construction.DemolishBuilding(building, m_resources);
    }

    /**
     * @brief Start crafting an item at a workshop
     */
    bool StartCrafting(Building* workshop, CraftedItemType item) {
        return m_production.StartCrafting(workshop, item);
    }

    /**
     * @brief Assign a worker to a building
     */
    bool AssignWorker(Worker* worker, Building* building) {
        if (!building || !worker) return false;

        // If defensive building, use defense system
        if (IsDefensiveBuilding(building->GetBuildingType())) {
            return m_defense.AssignGuard(worker, building);
        }

        // Otherwise direct assignment
        return building->AssignWorker(worker);
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total population capacity
     */
    int GetPopulationCapacity() const {
        return m_construction.GetTotalHousingCapacity();
    }

    /**
     * @brief Get total worker capacity
     */
    int GetWorkerCapacity() const {
        return m_construction.GetTotalWorkerCapacity();
    }

    /**
     * @brief Get defense score
     */
    float GetDefenseScore() const {
        return m_defense.GetDefenseScore();
    }

    /**
     * @brief Get vision coverage percentage
     */
    float GetVisionCoverage() const {
        return m_defense.GetVisionCoverage();
    }

    /**
     * @brief Get net food production (production - consumption)
     */
    float GetNetFoodRate() const {
        return m_production.GetNetFoodRate();
    }

    /**
     * @brief Get resource production summary
     */
    struct ResourceSummary {
        float woodPerMin = 0.0f;
        float stonePerMin = 0.0f;
        float metalPerMin = 0.0f;
        float foodPerMin = 0.0f;
        float foodConsumption = 0.0f;
    };

    ResourceSummary GetResourceSummary() const {
        ResourceSummary summary;
        summary.woodPerMin = m_production.GetTotalProductionRate(ResourceType::Wood);
        summary.stonePerMin = m_production.GetTotalProductionRate(ResourceType::Stone);
        summary.metalPerMin = m_production.GetTotalProductionRate(ResourceType::Metal);
        summary.foodPerMin = m_production.GetTotalProductionRate(ResourceType::Food);
        summary.foodConsumption = m_production.GetFoodConsumptionRate();
        return summary;
    }

private:
    World* m_world = nullptr;
    TileMap* m_tileMap = nullptr;

    Construction m_construction;
    ProductionManager m_production;
    DefenseManager m_defense;
    UpgradeManager m_upgrades;
    ResourceStockpile m_resources;
};

} // namespace Vehement
