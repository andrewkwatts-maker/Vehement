#pragma once

#include "Building.hpp"
#include "../world/TileMap.hpp"
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {
    class Renderer;
    class Camera;
}

namespace Vehement {

// Forward declarations
class World;

// ============================================================================
// Resource Types
// ============================================================================

/**
 * @brief Resource types used in construction
 */
enum class ResourceType : uint8_t {
    Wood,
    Stone,
    Metal,
    Food,
    Coins,
    COUNT
};

/**
 * @brief Get display name for resource type
 */
inline const char* GetResourceTypeName(ResourceType type) {
    switch (type) {
        case ResourceType::Wood:    return "Wood";
        case ResourceType::Stone:   return "Stone";
        case ResourceType::Metal:   return "Metal";
        case ResourceType::Food:    return "Food";
        case ResourceType::Coins:   return "Coins";
        default:                    return "Unknown";
    }
}

/**
 * @brief Get icon path for resource type
 */
inline const char* GetResourceIcon(ResourceType type) {
    switch (type) {
        case ResourceType::Wood:    return "Vehement2/images/Wood/WoodCrate1.png";
        case ResourceType::Stone:   return "Vehement2/images/Stone/StoneRaw.png";
        case ResourceType::Metal:   return "Vehement2/images/Metal/Metal1.png";
        case ResourceType::Food:    return "Vehement2/images/Items/Apple.png";
        case ResourceType::Coins:   return "Vehement2/images/Items/Bar.png";
        default:                    return "";
    }
}

// ============================================================================
// Building Cost
// ============================================================================

/**
 * @brief Cost to construct or upgrade a building
 */
struct BuildingCost {
    int wood = 0;
    int stone = 0;
    int metal = 0;
    int coins = 0;
    float buildTime = 10.0f;    // Base time in seconds (before worker modifiers)

    /**
     * @brief Check if player has enough resources
     */
    bool CanAfford(int availableWood, int availableStone,
                   int availableMetal, int availableCoins) const {
        return availableWood >= wood &&
               availableStone >= stone &&
               availableMetal >= metal &&
               availableCoins >= coins;
    }

    /**
     * @brief Get total resource cost
     */
    int GetTotalCost() const {
        return wood + stone + metal + coins;
    }

    /**
     * @brief Multiply all costs (for upgrades)
     */
    BuildingCost operator*(float multiplier) const {
        BuildingCost result;
        result.wood = static_cast<int>(wood * multiplier);
        result.stone = static_cast<int>(stone * multiplier);
        result.metal = static_cast<int>(metal * multiplier);
        result.coins = static_cast<int>(coins * multiplier);
        result.buildTime = buildTime * multiplier;
        return result;
    }

    /**
     * @brief Add two costs together
     */
    BuildingCost operator+(const BuildingCost& other) const {
        BuildingCost result;
        result.wood = wood + other.wood;
        result.stone = stone + other.stone;
        result.metal = metal + other.metal;
        result.coins = coins + other.coins;
        result.buildTime = buildTime + other.buildTime;
        return result;
    }
};

/**
 * @brief Get base construction cost for building type
 */
inline BuildingCost GetBuildingCost(BuildingType type) {
    BuildingCost cost;

    switch (type) {
        // Housing
        case BuildingType::Shelter:
            cost.wood = 30;
            cost.stone = 10;
            cost.buildTime = 15.0f;
            break;

        case BuildingType::House:
            cost.wood = 60;
            cost.stone = 30;
            cost.metal = 5;
            cost.buildTime = 30.0f;
            break;

        case BuildingType::Barracks:
            cost.wood = 80;
            cost.stone = 100;
            cost.metal = 30;
            cost.buildTime = 60.0f;
            break;

        // Production
        case BuildingType::Farm:
            cost.wood = 50;
            cost.stone = 20;
            cost.buildTime = 25.0f;
            break;

        case BuildingType::LumberMill:
            cost.wood = 40;
            cost.stone = 30;
            cost.metal = 10;
            cost.buildTime = 35.0f;
            break;

        case BuildingType::Quarry:
            cost.wood = 30;
            cost.stone = 10;
            cost.metal = 20;
            cost.buildTime = 40.0f;
            break;

        case BuildingType::Workshop:
            cost.wood = 60;
            cost.stone = 40;
            cost.metal = 40;
            cost.buildTime = 45.0f;
            break;

        // Defense
        case BuildingType::WatchTower:
            cost.wood = 40;
            cost.stone = 60;
            cost.metal = 10;
            cost.buildTime = 30.0f;
            break;

        case BuildingType::Wall:
            cost.stone = 20;
            cost.buildTime = 5.0f;  // Fast to build
            break;

        case BuildingType::Gate:
            cost.wood = 20;
            cost.stone = 30;
            cost.metal = 15;
            cost.buildTime = 15.0f;
            break;

        case BuildingType::Fortress:
            cost.wood = 100;
            cost.stone = 200;
            cost.metal = 80;
            cost.coins = 100;
            cost.buildTime = 120.0f;
            break;

        // Special
        case BuildingType::TradingPost:
            cost.wood = 60;
            cost.stone = 40;
            cost.coins = 50;
            cost.buildTime = 40.0f;
            break;

        case BuildingType::Hospital:
            cost.wood = 50;
            cost.stone = 80;
            cost.metal = 30;
            cost.coins = 80;
            cost.buildTime = 50.0f;
            break;

        case BuildingType::Warehouse:
            cost.wood = 100;
            cost.stone = 60;
            cost.metal = 20;
            cost.buildTime = 45.0f;
            break;

        case BuildingType::CommandCenter:
            cost.wood = 150;
            cost.stone = 150;
            cost.metal = 100;
            cost.coins = 200;
            cost.buildTime = 90.0f;
            break;

        default:
            cost.wood = 50;
            cost.stone = 50;
            cost.buildTime = 30.0f;
            break;
    }

    return cost;
}

/**
 * @brief Get upgrade cost for a building at a given level
 */
inline BuildingCost GetUpgradeCost(BuildingType type, int currentLevel) {
    BuildingCost baseCost = GetBuildingCost(type);
    // Each level costs 1.5x more
    return baseCost * (1.0f + currentLevel * 0.5f);
}

/**
 * @brief Get repair cost (percentage of build cost based on damage)
 */
inline BuildingCost GetRepairCost(BuildingType type, float damagePercent) {
    BuildingCost baseCost = GetBuildingCost(type);
    return baseCost * (damagePercent * 0.5f);  // 50% of proportional cost
}

/**
 * @brief Get demolition refund (percentage of build cost)
 */
inline BuildingCost GetDemolitionRefund(BuildingType type, int level) {
    BuildingCost baseCost = GetBuildingCost(type);
    // Return 40% of resources, increased with building level
    float refundPercent = 0.4f + (level - 1) * 0.1f;
    return baseCost * refundPercent;
}

// ============================================================================
// Placement Validation
// ============================================================================

/**
 * @brief Result of placement validation
 */
enum class PlacementResult {
    Valid,
    InvalidTerrain,         // Cannot build on this terrain
    Occupied,               // Another building already here
    TooCloseToEnemy,        // Too close to danger zone
    OutOfBounds,            // Outside map boundaries
    RequiresCommandCenter,  // Need CommandCenter first
    BlocksPath,             // Would block all pathways
    InsufficientResources   // Cannot afford
};

/**
 * @brief Get message for placement result
 */
inline const char* GetPlacementMessage(PlacementResult result) {
    switch (result) {
        case PlacementResult::Valid:
            return "Valid placement";
        case PlacementResult::InvalidTerrain:
            return "Cannot build on this terrain!";
        case PlacementResult::Occupied:
            return "Location already occupied!";
        case PlacementResult::TooCloseToEnemy:
            return "Too close to danger zone!";
        case PlacementResult::OutOfBounds:
            return "Outside map boundaries!";
        case PlacementResult::RequiresCommandCenter:
            return "Build a Command Center first!";
        case PlacementResult::BlocksPath:
            return "Would block all pathways!";
        case PlacementResult::InsufficientResources:
            return "Insufficient resources!";
        default:
            return "Invalid placement";
    }
}

// ============================================================================
// Building Ghost (Preview)
// ============================================================================

/**
 * @brief Ghost/preview of a building before placement
 */
struct BuildingGhost {
    BuildingType type = BuildingType::Shelter;
    glm::ivec2 gridPosition{0, 0};
    glm::ivec2 size{1, 1};
    PlacementResult placementResult = PlacementResult::Valid;
    bool isVisible = false;

    /**
     * @brief Get world position for rendering
     */
    [[nodiscard]] glm::vec3 GetWorldPosition() const {
        return glm::vec3(
            gridPosition.x + size.x * 0.5f,
            0.0f,
            gridPosition.y + size.y * 0.5f
        );
    }

    /**
     * @brief Check if placement is valid
     */
    [[nodiscard]] bool IsValid() const {
        return placementResult == PlacementResult::Valid;
    }

    /**
     * @brief Get color for ghost rendering
     */
    [[nodiscard]] glm::vec4 GetColor() const {
        if (IsValid()) {
            return glm::vec4(0.0f, 1.0f, 0.0f, 0.5f);  // Green, semi-transparent
        } else {
            return glm::vec4(1.0f, 0.0f, 0.0f, 0.5f);  // Red, semi-transparent
        }
    }
};

// ============================================================================
// Player Resources
// ============================================================================

/**
 * @brief Player's resource stockpile
 */
class ResourceStockpile {
public:
    ResourceStockpile() = default;

    // Resource access
    [[nodiscard]] int GetWood() const { return m_wood; }
    [[nodiscard]] int GetStone() const { return m_stone; }
    [[nodiscard]] int GetMetal() const { return m_metal; }
    [[nodiscard]] int GetFood() const { return m_food; }
    [[nodiscard]] int GetCoins() const { return m_coins; }

    // Capacity
    [[nodiscard]] int GetCapacity() const { return m_capacity; }
    void SetCapacity(int capacity) { m_capacity = capacity; }
    void AddCapacity(int amount) { m_capacity += amount; }

    // Resource manipulation
    void AddWood(int amount) { m_wood = std::min(m_wood + amount, m_capacity); }
    void AddStone(int amount) { m_stone = std::min(m_stone + amount, m_capacity); }
    void AddMetal(int amount) { m_metal = std::min(m_metal + amount, m_capacity); }
    void AddFood(int amount) { m_food = std::min(m_food + amount, m_capacity); }
    void AddCoins(int amount) { m_coins += amount; }  // Coins have no cap

    bool SpendWood(int amount) {
        if (m_wood >= amount) { m_wood -= amount; return true; }
        return false;
    }
    bool SpendStone(int amount) {
        if (m_stone >= amount) { m_stone -= amount; return true; }
        return false;
    }
    bool SpendMetal(int amount) {
        if (m_metal >= amount) { m_metal -= amount; return true; }
        return false;
    }
    bool SpendFood(int amount) {
        if (m_food >= amount) { m_food -= amount; return true; }
        return false;
    }
    bool SpendCoins(int amount) {
        if (m_coins >= amount) { m_coins -= amount; return true; }
        return false;
    }

    /**
     * @brief Check if can afford a cost
     */
    [[nodiscard]] bool CanAfford(const BuildingCost& cost) const {
        return m_wood >= cost.wood &&
               m_stone >= cost.stone &&
               m_metal >= cost.metal &&
               m_coins >= cost.coins;
    }

    /**
     * @brief Spend resources for a building cost
     */
    bool Spend(const BuildingCost& cost) {
        if (!CanAfford(cost)) return false;

        m_wood -= cost.wood;
        m_stone -= cost.stone;
        m_metal -= cost.metal;
        m_coins -= cost.coins;
        return true;
    }

    /**
     * @brief Add resources from a refund
     */
    void AddRefund(const BuildingCost& refund) {
        AddWood(refund.wood);
        AddStone(refund.stone);
        AddMetal(refund.metal);
        AddCoins(refund.coins);
    }

    /**
     * @brief Reset to starting resources
     */
    void Reset(int startingWood = 100, int startingStone = 50,
               int startingMetal = 20, int startingFood = 50,
               int startingCoins = 100) {
        m_wood = startingWood;
        m_stone = startingStone;
        m_metal = startingMetal;
        m_food = startingFood;
        m_coins = startingCoins;
        m_capacity = 500;
    }

private:
    int m_wood = 100;
    int m_stone = 50;
    int m_metal = 20;
    int m_food = 50;
    int m_coins = 100;
    int m_capacity = 500;  // Max storage (except coins)
};

// ============================================================================
// Construction System
// ============================================================================

/**
 * @brief Manages building construction, placement, and upgrades
 */
class Construction {
public:
    Construction();
    ~Construction() = default;

    /**
     * @brief Initialize with world reference
     */
    void Initialize(World* world, TileMap* tileMap);

    /**
     * @brief Update construction progress
     */
    void Update(float deltaTime);

    /**
     * @brief Render building ghosts and construction visuals
     */
    void Render(Nova::Renderer& renderer, const Nova::Camera& camera);

    // =========================================================================
    // Building Placement
    // =========================================================================

    /**
     * @brief Start placing a building (show ghost)
     */
    void StartPlacement(BuildingType type);

    /**
     * @brief Update ghost position based on cursor
     */
    void UpdateGhostPosition(const glm::ivec2& gridPos);

    /**
     * @brief Update ghost position from world coordinates
     */
    void UpdateGhostPositionWorld(const glm::vec3& worldPos);

    /**
     * @brief Cancel current placement
     */
    void CancelPlacement();

    /**
     * @brief Rotate ghost 90 degrees (for asymmetric buildings)
     */
    void RotateGhost();

    /**
     * @brief Get current ghost
     */
    [[nodiscard]] const BuildingGhost& GetGhost() const { return m_ghost; }

    /**
     * @brief Check if currently placing a building
     */
    [[nodiscard]] bool IsPlacing() const { return m_ghost.isVisible; }

    // =========================================================================
    // Placement Validation
    // =========================================================================

    /**
     * @brief Validate placement at a position
     */
    PlacementResult ValidatePlacement(BuildingType type, const glm::ivec2& gridPos) const;

    /**
     * @brief Check if terrain allows building
     */
    bool IsTerrainBuildable(int x, int y) const;

    /**
     * @brief Check if tiles are unoccupied
     */
    bool AreTilesAvailable(const glm::ivec2& pos, const glm::ivec2& size) const;

    // =========================================================================
    // Construction Actions
    // =========================================================================

    /**
     * @brief Confirm placement and start construction
     * @param resources Player's resource stockpile
     * @return Pointer to new building, or nullptr if failed
     */
    Building* ConfirmPlacement(ResourceStockpile& resources);

    /**
     * @brief Place blueprint without starting construction
     */
    Building* PlaceBlueprint(BuildingType type, const glm::ivec2& gridPos);

    /**
     * @brief Start construction on a blueprint
     */
    bool StartConstruction(Building* building, ResourceStockpile& resources);

    /**
     * @brief Add construction progress to a building
     * @param building Building to progress
     * @param workerCount Number of workers building
     * @param totalSkill Sum of worker skill levels
     * @param deltaTime Time elapsed
     */
    void AddProgress(Building* building, int workerCount, float totalSkill, float deltaTime);

    // =========================================================================
    // Repair and Upgrade
    // =========================================================================

    /**
     * @brief Repair a damaged building
     */
    bool RepairBuilding(Building* building, ResourceStockpile& resources);

    /**
     * @brief Start upgrading a building
     */
    bool UpgradeBuilding(Building* building, ResourceStockpile& resources);

    /**
     * @brief Demolish a building (returns partial resources)
     */
    bool DemolishBuilding(Building* building, ResourceStockpile& resources);

    // =========================================================================
    // Building Queries
    // =========================================================================

    /**
     * @brief Get all buildings
     */
    [[nodiscard]] const std::vector<std::shared_ptr<Building>>& GetBuildings() const {
        return m_buildings;
    }

    /**
     * @brief Get buildings by type
     */
    std::vector<Building*> GetBuildingsByType(BuildingType type) const;

    /**
     * @brief Get buildings in construction
     */
    std::vector<Building*> GetBuildingsUnderConstruction() const;

    /**
     * @brief Get building at grid position
     */
    Building* GetBuildingAt(int x, int y) const;

    /**
     * @brief Get building at world position
     */
    Building* GetBuildingAtWorld(const glm::vec3& worldPos) const;

    /**
     * @brief Check if command center exists
     */
    bool HasCommandCenter() const;

    /**
     * @brief Get command center (if exists)
     */
    Building* GetCommandCenter() const;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total housing capacity
     */
    int GetTotalHousingCapacity() const;

    /**
     * @brief Get total worker capacity
     */
    int GetTotalWorkerCapacity() const;

    /**
     * @brief Get count of buildings by type
     */
    int GetBuildingCount(BuildingType type) const;

    /**
     * @brief Get total building count
     */
    int GetTotalBuildingCount() const { return static_cast<int>(m_buildings.size()); }

    // =========================================================================
    // Callbacks
    // =========================================================================

    using PlacementCallback = std::function<void(Building*, PlacementResult)>;
    void SetOnPlacement(PlacementCallback callback) { m_onPlacement = std::move(callback); }

    using ConstructionCallback = std::function<void(Building*)>;
    void SetOnConstructionStart(ConstructionCallback callback) {
        m_onConstructionStart = std::move(callback);
    }
    void SetOnConstructionComplete(ConstructionCallback callback) {
        m_onConstructionComplete = std::move(callback);
    }

    using DemolitionCallback = std::function<void(Building*, const BuildingCost& refund)>;
    void SetOnDemolition(DemolitionCallback callback) { m_onDemolition = std::move(callback); }

private:
    void UpdateTileMap(Building* building, bool occupied);
    void RebuildNavigationGraph();

    World* m_world = nullptr;
    TileMap* m_tileMap = nullptr;

    // All buildings
    std::vector<std::shared_ptr<Building>> m_buildings;

    // Building placement ghost
    BuildingGhost m_ghost;

    // Occupied tiles (for quick lookup)
    std::vector<std::vector<Building*>> m_occupancyGrid;
    int m_gridWidth = 0;
    int m_gridHeight = 0;

    // Callbacks
    PlacementCallback m_onPlacement;
    ConstructionCallback m_onConstructionStart;
    ConstructionCallback m_onConstructionComplete;
    DemolitionCallback m_onDemolition;
};

} // namespace Vehement
