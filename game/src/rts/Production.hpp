#pragma once

#include "Resource.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace Vehement {
namespace RTS {

// ============================================================================
// Production Building Types
// ============================================================================

/**
 * @brief Types of production buildings
 */
enum class ProductionBuildingType : uint8_t {
    Farm,           ///< Produces Food (no inputs)
    LumberMill,     ///< Processes Wood into refined lumber
    Quarry,         ///< Processes Stone into cut blocks
    Foundry,        ///< Smelts Metal into refined metal
    Workshop,       ///< Creates equipment from various materials
    Refinery,       ///< Processes Fuel
    Hospital,       ///< Heals workers, produces Medicine
    Armory,         ///< Produces Ammunition
    Mint,           ///< Converts resources to Coins
    Warehouse,      ///< Increases storage capacity (no production)

    COUNT
};

/**
 * @brief Get display name for a building type
 */
[[nodiscard]] const char* GetBuildingTypeName(ProductionBuildingType type);

/**
 * @brief Get description for a building type
 */
[[nodiscard]] const char* GetBuildingTypeDescription(ProductionBuildingType type);

/**
 * @brief Get build cost for a building type
 */
[[nodiscard]] ResourceCost GetBuildingCost(ProductionBuildingType type);

// ============================================================================
// Production Recipe
// ============================================================================

/**
 * @brief Defines a production recipe that transforms inputs into outputs
 */
struct ProductionRecipe {
    /// Unique identifier for this recipe
    uint32_t id = 0;

    /// Display name
    std::string name;

    /// Description
    std::string description;

    /// Input resources required
    std::vector<std::pair<ResourceType, int>> inputs;

    /// Output resources produced
    std::vector<std::pair<ResourceType, int>> outputs;

    /// Time to complete one production cycle (seconds)
    float productionTime = 10.0f;

    /// Number of workers required
    int workersRequired = 1;

    /// Building type that can use this recipe
    ProductionBuildingType buildingType = ProductionBuildingType::Farm;

    /// Whether this recipe is unlocked
    bool unlocked = true;

    /**
     * @brief Check if stock has required inputs
     */
    [[nodiscard]] bool CanProduce(const ResourceStock& stock) const;

    /**
     * @brief Consume inputs from stock
     * @return true if successful
     */
    bool ConsumeInputs(ResourceStock& stock) const;

    /**
     * @brief Add outputs to stock
     * @return Total outputs added
     */
    int AddOutputs(ResourceStock& stock) const;

    /**
     * @brief Get the resource cost representation of inputs
     */
    [[nodiscard]] ResourceCost GetInputCost() const;

    /**
     * @brief Get efficiency (output value / input value / time)
     */
    [[nodiscard]] float GetEfficiency() const;
};

// ============================================================================
// Production Queue Item
// ============================================================================

/**
 * @brief An item in a production building's queue
 */
struct ProductionQueueItem {
    /// Recipe being produced
    const ProductionRecipe* recipe = nullptr;

    /// Progress through production (0.0 - 1.0)
    float progress = 0.0f;

    /// Whether production is paused
    bool paused = false;

    /// Number of times to repeat (0 = once, -1 = infinite)
    int repeatCount = 0;

    /**
     * @brief Get remaining time in seconds
     */
    [[nodiscard]] float GetRemainingTime() const {
        if (!recipe) return 0.0f;
        return recipe->productionTime * (1.0f - progress);
    }

    /**
     * @brief Check if this item should repeat after completion
     */
    [[nodiscard]] bool ShouldRepeat() const {
        return repeatCount != 0;
    }
};

// ============================================================================
// Production Building
// ============================================================================

/**
 * @brief A building that produces resources
 */
struct ProductionBuilding {
    /// Unique identifier
    uint32_t id = 0;

    /// Building type
    ProductionBuildingType type = ProductionBuildingType::Farm;

    /// World position
    glm::vec2 position{0.0f};

    /// Building name (player-assignable)
    std::string name;

    /// Whether the building is operational
    bool operational = true;

    /// Whether production is paused
    bool paused = false;

    /// Upgrade level (affects production speed and efficiency)
    int level = 1;

    /// Maximum level
    static constexpr int MAX_LEVEL = 5;

    /// Number of workers assigned
    int assignedWorkers = 0;

    /// Maximum workers
    int maxWorkers = 3;

    /// Production speed multiplier (from upgrades, workers, etc.)
    float speedMultiplier = 1.0f;

    /// Production queue
    std::vector<ProductionQueueItem> productionQueue;

    /// Maximum queue size
    static constexpr int MAX_QUEUE_SIZE = 5;

    /// Storage capacity bonus (for warehouses)
    int storageBonus = 0;

    /// Health (for damage from attacks)
    float health = 100.0f;
    float maxHealth = 100.0f;

    /**
     * @brief Check if building can accept more workers
     */
    [[nodiscard]] bool CanAssignWorker() const {
        return assignedWorkers < maxWorkers;
    }

    /**
     * @brief Check if queue has space
     */
    [[nodiscard]] bool CanQueueProduction() const {
        return static_cast<int>(productionQueue.size()) < MAX_QUEUE_SIZE;
    }

    /**
     * @brief Check if currently producing
     */
    [[nodiscard]] bool IsProducing() const {
        return operational && !paused && !productionQueue.empty() && !productionQueue[0].paused;
    }

    /**
     * @brief Get effective production speed (accounting for workers, level, etc.)
     */
    [[nodiscard]] float GetEffectiveSpeed() const {
        float workerBonus = 0.5f + 0.5f * (static_cast<float>(assignedWorkers) / maxWorkers);
        float levelBonus = 1.0f + 0.2f * (level - 1);
        return speedMultiplier * workerBonus * levelBonus;
    }

    /**
     * @brief Get cost to upgrade to next level
     */
    [[nodiscard]] ResourceCost GetUpgradeCost() const;

    /**
     * @brief Check if building can be upgraded
     */
    [[nodiscard]] bool CanUpgrade() const { return level < MAX_LEVEL; }
};

// ============================================================================
// Production System
// ============================================================================

/**
 * @brief Configuration for the production system
 */
struct ProductionConfig {
    float baseProductionSpeed = 1.0f;       ///< Global production speed multiplier
    bool autoRepeatProduction = false;      ///< Default auto-repeat for new queues
    int maxBuildingsPerType = 10;           ///< Maximum buildings of each type
};

/**
 * @brief Manages all production buildings and resource transformation
 */
class ProductionSystem {
public:
    ProductionSystem();
    ~ProductionSystem();

    // Delete copy, allow move
    ProductionSystem(const ProductionSystem&) = delete;
    ProductionSystem& operator=(const ProductionSystem&) = delete;
    ProductionSystem(ProductionSystem&&) noexcept = default;
    ProductionSystem& operator=(ProductionSystem&&) noexcept = default;

    /**
     * @brief Initialize the production system
     */
    void Initialize(const ProductionConfig& config = ProductionConfig{});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update all production buildings
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    // -------------------------------------------------------------------------
    // Recipe Management
    // -------------------------------------------------------------------------

    /**
     * @brief Register a production recipe
     * @return ID of the registered recipe
     */
    uint32_t RegisterRecipe(const ProductionRecipe& recipe);

    /**
     * @brief Get all recipes for a building type
     */
    [[nodiscard]] std::vector<const ProductionRecipe*> GetRecipesForBuilding(ProductionBuildingType type) const;

    /**
     * @brief Get a recipe by ID
     */
    [[nodiscard]] const ProductionRecipe* GetRecipe(uint32_t recipeId) const;

    /**
     * @brief Unlock a recipe
     */
    void UnlockRecipe(uint32_t recipeId);

    /**
     * @brief Initialize default recipes
     */
    void InitializeDefaultRecipes();

    // -------------------------------------------------------------------------
    // Building Management
    // -------------------------------------------------------------------------

    /**
     * @brief Create a production building
     * @param type Building type
     * @param position World position
     * @param stock Resource stock to charge
     * @return Pointer to created building, or nullptr if failed
     */
    ProductionBuilding* CreateBuilding(ProductionBuildingType type, const glm::vec2& position, ResourceStock& stock);

    /**
     * @brief Create a building without cost (for loading saves, etc.)
     */
    ProductionBuilding* CreateBuildingFree(ProductionBuildingType type, const glm::vec2& position);

    /**
     * @brief Remove a building
     */
    void RemoveBuilding(uint32_t buildingId);

    /**
     * @brief Get a building by ID
     */
    [[nodiscard]] ProductionBuilding* GetBuilding(uint32_t buildingId);
    [[nodiscard]] const ProductionBuilding* GetBuilding(uint32_t buildingId) const;

    /**
     * @brief Get all buildings
     */
    [[nodiscard]] const std::vector<ProductionBuilding>& GetBuildings() const { return m_buildings; }

    /**
     * @brief Get buildings of a specific type
     */
    [[nodiscard]] std::vector<ProductionBuilding*> GetBuildingsByType(ProductionBuildingType type);

    /**
     * @brief Upgrade a building
     */
    bool UpgradeBuilding(uint32_t buildingId, ResourceStock& stock);

    /**
     * @brief Repair a building
     */
    bool RepairBuilding(uint32_t buildingId, ResourceStock& stock);

    // -------------------------------------------------------------------------
    // Production Queue
    // -------------------------------------------------------------------------

    /**
     * @brief Queue a recipe for production
     * @param buildingId Building to produce in
     * @param recipeId Recipe to produce
     * @param repeat Number of repetitions (0 = once, -1 = infinite)
     * @return true if successfully queued
     */
    bool QueueProduction(uint32_t buildingId, uint32_t recipeId, int repeat = 0);

    /**
     * @brief Cancel a queued production
     * @param buildingId Building ID
     * @param queueIndex Index in queue
     */
    void CancelProduction(uint32_t buildingId, int queueIndex);

    /**
     * @brief Pause/resume production for a building
     */
    void SetBuildingPaused(uint32_t buildingId, bool paused);

    /**
     * @brief Clear entire production queue
     */
    void ClearQueue(uint32_t buildingId);

    // -------------------------------------------------------------------------
    // Worker Management
    // -------------------------------------------------------------------------

    /**
     * @brief Assign a worker to a building
     */
    bool AssignWorker(uint32_t buildingId);

    /**
     * @brief Remove a worker from a building
     */
    bool RemoveWorker(uint32_t buildingId);

    // -------------------------------------------------------------------------
    // Storage Management
    // -------------------------------------------------------------------------

    /**
     * @brief Set the resource stock for production
     */
    void SetResourceStock(ResourceStock* stock) { m_resourceStock = stock; }

    /**
     * @brief Calculate total storage bonus from all warehouses
     */
    [[nodiscard]] int CalculateTotalStorageBonus() const;

    // -------------------------------------------------------------------------
    // Statistics
    // -------------------------------------------------------------------------

    /**
     * @brief Get total production per resource type (per second)
     */
    [[nodiscard]] float GetProductionRate(ResourceType type) const;

    /**
     * @brief Get total consumption per resource type (per second)
     */
    [[nodiscard]] float GetConsumptionRate(ResourceType type) const;

    /**
     * @brief Get building count by type
     */
    [[nodiscard]] int GetBuildingCount(ProductionBuildingType type) const;

    /**
     * @brief Get total production value
     */
    [[nodiscard]] int GetTotalProductionValue() const;

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Apply scarcity settings
     */
    void ApplyScarcitySettings(const ScarcitySettings& settings);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const ProductionConfig& GetConfig() const { return m_config; }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using ProductionCompleteCallback = std::function<void(const ProductionBuilding&, const ProductionRecipe&)>;
    using BuildingCreatedCallback = std::function<void(const ProductionBuilding&)>;
    using BuildingDestroyedCallback = std::function<void(const ProductionBuilding&)>;

    void SetOnProductionComplete(ProductionCompleteCallback callback) { m_onProductionComplete = std::move(callback); }
    void SetOnBuildingCreated(BuildingCreatedCallback callback) { m_onBuildingCreated = std::move(callback); }
    void SetOnBuildingDestroyed(BuildingDestroyedCallback callback) { m_onBuildingDestroyed = std::move(callback); }

private:
    void UpdateBuilding(ProductionBuilding& building, float deltaTime);
    void CompleteProduction(ProductionBuilding& building, ProductionQueueItem& item);

    uint32_t GenerateBuildingId();
    uint32_t GenerateRecipeId();

    ProductionConfig m_config;
    ScarcitySettings m_scarcitySettings;

    std::vector<ProductionBuilding> m_buildings;
    std::vector<ProductionRecipe> m_recipes;

    ResourceStock* m_resourceStock = nullptr;

    // Statistics
    std::unordered_map<ResourceType, float> m_productionRates;
    std::unordered_map<ResourceType, float> m_consumptionRates;

    // ID generators
    uint32_t m_nextBuildingId = 1;
    uint32_t m_nextRecipeId = 1;

    // Callbacks
    ProductionCompleteCallback m_onProductionComplete;
    BuildingCreatedCallback m_onBuildingCreated;
    BuildingDestroyedCallback m_onBuildingDestroyed;

    bool m_initialized = false;
};

// ============================================================================
// Default Recipes
// ============================================================================

namespace DefaultRecipes {

/**
 * @brief Farm produces food without inputs
 */
inline ProductionRecipe FarmFood() {
    ProductionRecipe r;
    r.name = "Grow Food";
    r.description = "Grow crops for food.";
    r.outputs = {{ResourceType::Food, 10}};
    r.productionTime = 15.0f;
    r.workersRequired = 1;
    r.buildingType = ProductionBuildingType::Farm;
    return r;
}

/**
 * @brief Lumber mill processes wood
 */
inline ProductionRecipe ProcessWood() {
    ProductionRecipe r;
    r.name = "Process Lumber";
    r.description = "Process raw wood into refined lumber.";
    r.inputs = {{ResourceType::Wood, 5}};
    r.outputs = {{ResourceType::Wood, 8}}; // Net gain of 3
    r.productionTime = 10.0f;
    r.workersRequired = 1;
    r.buildingType = ProductionBuildingType::LumberMill;
    return r;
}

/**
 * @brief Quarry processes stone
 */
inline ProductionRecipe ProcessStone() {
    ProductionRecipe r;
    r.name = "Cut Stone";
    r.description = "Cut raw stone into usable blocks.";
    r.inputs = {{ResourceType::Stone, 5}};
    r.outputs = {{ResourceType::Stone, 7}}; // Net gain of 2
    r.productionTime = 12.0f;
    r.workersRequired = 1;
    r.buildingType = ProductionBuildingType::Quarry;
    return r;
}

/**
 * @brief Foundry smelts metal
 */
inline ProductionRecipe SmeltMetal() {
    ProductionRecipe r;
    r.name = "Smelt Metal";
    r.description = "Smelt raw metal into refined ingots.";
    r.inputs = {{ResourceType::Metal, 3}, {ResourceType::Fuel, 2}};
    r.outputs = {{ResourceType::Metal, 6}}; // Net gain of 3
    r.productionTime = 20.0f;
    r.workersRequired = 2;
    r.buildingType = ProductionBuildingType::Foundry;
    return r;
}

/**
 * @brief Workshop creates equipment
 */
inline ProductionRecipe CraftEquipment() {
    ProductionRecipe r;
    r.name = "Craft Equipment";
    r.description = "Craft useful equipment from materials.";
    r.inputs = {{ResourceType::Wood, 3}, {ResourceType::Metal, 2}};
    r.outputs = {{ResourceType::Coins, 25}};
    r.productionTime = 25.0f;
    r.workersRequired = 2;
    r.buildingType = ProductionBuildingType::Workshop;
    return r;
}

/**
 * @brief Refinery processes fuel
 */
inline ProductionRecipe RefineFuel() {
    ProductionRecipe r;
    r.name = "Refine Fuel";
    r.description = "Refine crude fuel into usable form.";
    r.inputs = {{ResourceType::Fuel, 5}};
    r.outputs = {{ResourceType::Fuel, 8}}; // Net gain of 3
    r.productionTime = 15.0f;
    r.workersRequired = 1;
    r.buildingType = ProductionBuildingType::Refinery;
    return r;
}

/**
 * @brief Hospital produces medicine
 */
inline ProductionRecipe CreateMedicine() {
    ProductionRecipe r;
    r.name = "Create Medicine";
    r.description = "Synthesize medicine from supplies.";
    r.inputs = {{ResourceType::Food, 2}};
    r.outputs = {{ResourceType::Medicine, 3}};
    r.productionTime = 30.0f;
    r.workersRequired = 2;
    r.buildingType = ProductionBuildingType::Hospital;
    return r;
}

/**
 * @brief Armory manufactures ammunition
 */
inline ProductionRecipe ManufactureAmmo() {
    ProductionRecipe r;
    r.name = "Manufacture Ammo";
    r.description = "Manufacture ammunition from metal.";
    r.inputs = {{ResourceType::Metal, 2}};
    r.outputs = {{ResourceType::Ammunition, 20}};
    r.productionTime = 15.0f;
    r.workersRequired = 1;
    r.buildingType = ProductionBuildingType::Armory;
    return r;
}

/**
 * @brief Mint converts resources to coins
 */
inline ProductionRecipe MintCoins() {
    ProductionRecipe r;
    r.name = "Mint Coins";
    r.description = "Convert precious materials into currency.";
    r.inputs = {{ResourceType::Metal, 5}};
    r.outputs = {{ResourceType::Coins, 30}};
    r.productionTime = 20.0f;
    r.workersRequired = 2;
    r.buildingType = ProductionBuildingType::Mint;
    return r;
}

} // namespace DefaultRecipes

} // namespace RTS
} // namespace Vehement
