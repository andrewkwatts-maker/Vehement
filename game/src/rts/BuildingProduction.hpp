#pragma once

#include "Building.hpp"
#include "Construction.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Vehement {

// ============================================================================
// Production Output
// ============================================================================

/**
 * @brief Defines what a building produces
 */
struct ProductionOutput {
    ResourceType resource = ResourceType::Food;
    float baseAmountPerMinute = 10.0f;      // Base production rate
    int workersRequired = 1;                 // Minimum workers for production
    int workersOptimal = 2;                  // Workers for maximum efficiency

    /**
     * @brief Calculate actual production based on assigned workers
     * @param assignedWorkers Number of workers assigned
     * @param totalSkill Sum of worker skill levels
     * @return Production amount per minute
     */
    [[nodiscard]] float CalculateProduction(int assignedWorkers, float totalSkill) const {
        if (assignedWorkers < workersRequired) {
            return 0.0f;  // Not enough workers
        }

        // Base efficiency at optimal workers
        float efficiency = 1.0f;

        if (assignedWorkers < workersOptimal) {
            // Below optimal: linear scaling
            efficiency = static_cast<float>(assignedWorkers) / static_cast<float>(workersOptimal);
        } else if (assignedWorkers > workersOptimal) {
            // Above optimal: diminishing returns
            int extraWorkers = assignedWorkers - workersOptimal;
            efficiency = 1.0f + extraWorkers * 0.2f;  // 20% bonus per extra worker
        }

        // Skill bonus (average skill)
        float avgSkill = totalSkill / std::max(1, assignedWorkers);

        return baseAmountPerMinute * efficiency * avgSkill;
    }
};

/**
 * @brief Get production output for a building type
 */
inline ProductionOutput GetBuildingProduction(BuildingType type) {
    ProductionOutput output;

    switch (type) {
        case BuildingType::Farm:
            output.resource = ResourceType::Food;
            output.baseAmountPerMinute = 20.0f;
            output.workersRequired = 1;
            output.workersOptimal = 4;
            break;

        case BuildingType::LumberMill:
            output.resource = ResourceType::Wood;
            output.baseAmountPerMinute = 15.0f;
            output.workersRequired = 1;
            output.workersOptimal = 3;
            break;

        case BuildingType::Quarry:
            output.resource = ResourceType::Stone;
            output.baseAmountPerMinute = 12.0f;
            output.workersRequired = 2;
            output.workersOptimal = 5;
            break;

        case BuildingType::Workshop:
            output.resource = ResourceType::Metal;
            output.baseAmountPerMinute = 8.0f;
            output.workersRequired = 2;
            output.workersOptimal = 4;
            break;

        default:
            output.baseAmountPerMinute = 0.0f;
            output.workersRequired = 0;
            output.workersOptimal = 0;
            break;
    }

    return output;
}

/**
 * @brief Check if a building type produces resources
 */
inline bool IsProductionBuilding(BuildingType type) {
    return type == BuildingType::Farm ||
           type == BuildingType::LumberMill ||
           type == BuildingType::Quarry ||
           type == BuildingType::Workshop;
}

// ============================================================================
// Production Bonuses
// ============================================================================

/**
 * @brief Production bonus modifiers
 */
struct ProductionBonus {
    float multiplier = 1.0f;
    std::string source;

    ProductionBonus() = default;
    ProductionBonus(float mult, const std::string& src)
        : multiplier(mult), source(src) {}
};

// ============================================================================
// Crafting Recipe (for Workshop)
// ============================================================================

/**
 * @brief Item types that can be crafted
 */
enum class CraftedItemType : uint8_t {
    // Tools
    Pickaxe,            // Speeds up quarry work
    Axe,                // Speeds up lumber mill work
    Hoe,                // Speeds up farm work
    Hammer,             // Speeds up construction

    // Weapons
    Sword,              // Melee weapon
    Bow,                // Ranged weapon
    Shield,             // Defense item

    // Equipment
    Armor,              // Worker protection
    Torch,              // Reveals fog of war

    COUNT
};

/**
 * @brief Get display name for crafted item
 */
inline const char* GetCraftedItemName(CraftedItemType type) {
    switch (type) {
        case CraftedItemType::Pickaxe:  return "Pickaxe";
        case CraftedItemType::Axe:      return "Axe";
        case CraftedItemType::Hoe:      return "Hoe";
        case CraftedItemType::Hammer:   return "Hammer";
        case CraftedItemType::Sword:    return "Sword";
        case CraftedItemType::Bow:      return "Bow";
        case CraftedItemType::Shield:   return "Shield";
        case CraftedItemType::Armor:    return "Armor";
        case CraftedItemType::Torch:    return "Torch";
        default:                        return "Unknown";
    }
}

/**
 * @brief Recipe for crafting an item
 */
struct CraftingRecipe {
    CraftedItemType item = CraftedItemType::Pickaxe;
    int woodCost = 0;
    int stoneCost = 0;
    int metalCost = 0;
    float craftTime = 30.0f;  // Seconds
    int workshopLevel = 1;    // Required workshop level

    /**
     * @brief Check if can afford this recipe
     */
    [[nodiscard]] bool CanAfford(int wood, int stone, int metal) const {
        return wood >= woodCost && stone >= stoneCost && metal >= metalCost;
    }
};

/**
 * @brief Get crafting recipe for an item
 */
inline CraftingRecipe GetCraftingRecipe(CraftedItemType item) {
    CraftingRecipe recipe;
    recipe.item = item;

    switch (item) {
        case CraftedItemType::Pickaxe:
            recipe.woodCost = 10;
            recipe.metalCost = 15;
            recipe.craftTime = 20.0f;
            recipe.workshopLevel = 1;
            break;

        case CraftedItemType::Axe:
            recipe.woodCost = 15;
            recipe.metalCost = 10;
            recipe.craftTime = 20.0f;
            recipe.workshopLevel = 1;
            break;

        case CraftedItemType::Hoe:
            recipe.woodCost = 10;
            recipe.metalCost = 5;
            recipe.craftTime = 15.0f;
            recipe.workshopLevel = 1;
            break;

        case CraftedItemType::Hammer:
            recipe.woodCost = 15;
            recipe.metalCost = 20;
            recipe.craftTime = 25.0f;
            recipe.workshopLevel = 1;
            break;

        case CraftedItemType::Sword:
            recipe.metalCost = 30;
            recipe.craftTime = 40.0f;
            recipe.workshopLevel = 2;
            break;

        case CraftedItemType::Bow:
            recipe.woodCost = 25;
            recipe.metalCost = 5;
            recipe.craftTime = 35.0f;
            recipe.workshopLevel = 2;
            break;

        case CraftedItemType::Shield:
            recipe.woodCost = 20;
            recipe.metalCost = 25;
            recipe.craftTime = 45.0f;
            recipe.workshopLevel = 2;
            break;

        case CraftedItemType::Armor:
            recipe.metalCost = 50;
            recipe.craftTime = 60.0f;
            recipe.workshopLevel = 3;
            break;

        case CraftedItemType::Torch:
            recipe.woodCost = 5;
            recipe.craftTime = 10.0f;
            recipe.workshopLevel = 1;
            break;

        default:
            break;
    }

    return recipe;
}

/**
 * @brief Get all available recipes for a workshop level
 */
inline std::vector<CraftingRecipe> GetAvailableRecipes(int workshopLevel) {
    std::vector<CraftingRecipe> recipes;

    for (int i = 0; i < static_cast<int>(CraftedItemType::COUNT); ++i) {
        CraftedItemType item = static_cast<CraftedItemType>(i);
        CraftingRecipe recipe = GetCraftingRecipe(item);

        if (recipe.workshopLevel <= workshopLevel) {
            recipes.push_back(recipe);
        }
    }

    return recipes;
}

// ============================================================================
// Food Consumption
// ============================================================================

/**
 * @brief Food consumption rates
 */
struct FoodConsumption {
    static constexpr float kFoodPerWorkerPerMinute = 1.0f;
    static constexpr float kFoodPerSoldierPerMinute = 1.5f;
    static constexpr float kStarvationDamagePerSecond = 5.0f;

    /**
     * @brief Calculate food needed per minute for population
     */
    static float CalculateConsumption(int workerCount, int soldierCount) {
        return workerCount * kFoodPerWorkerPerMinute +
               soldierCount * kFoodPerSoldierPerMinute;
    }
};

// ============================================================================
// Production Manager
// ============================================================================

/**
 * @brief Manages production for all buildings
 */
class ProductionManager {
public:
    ProductionManager() = default;
    ~ProductionManager() = default;

    /**
     * @brief Initialize with construction system reference
     */
    void Initialize(Construction* construction, ResourceStockpile* resources);

    /**
     * @brief Update all production
     */
    void Update(float deltaTime);

    // =========================================================================
    // Production Control
    // =========================================================================

    /**
     * @brief Pause production for a building
     */
    void PauseProduction(Building* building);

    /**
     * @brief Resume production for a building
     */
    void ResumeProduction(Building* building);

    /**
     * @brief Check if building production is paused
     */
    [[nodiscard]] bool IsProductionPaused(Building* building) const;

    // =========================================================================
    // Crafting (Workshop)
    // =========================================================================

    /**
     * @brief Start crafting an item at a workshop
     */
    bool StartCrafting(Building* workshop, CraftedItemType item);

    /**
     * @brief Cancel current crafting
     */
    void CancelCrafting(Building* workshop);

    /**
     * @brief Get current crafting progress (0-100%)
     */
    [[nodiscard]] float GetCraftingProgress(Building* workshop) const;

    /**
     * @brief Get item being crafted
     */
    [[nodiscard]] std::optional<CraftedItemType> GetCraftingItem(Building* workshop) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total production rate for a resource
     */
    [[nodiscard]] float GetTotalProductionRate(ResourceType resource) const;

    /**
     * @brief Get production rate for a specific building
     */
    [[nodiscard]] float GetBuildingProductionRate(Building* building) const;

    /**
     * @brief Get total food consumption rate
     */
    [[nodiscard]] float GetFoodConsumptionRate() const;

    /**
     * @brief Get net food rate (production - consumption)
     */
    [[nodiscard]] float GetNetFoodRate() const;

    // =========================================================================
    // Bonuses
    // =========================================================================

    /**
     * @brief Add a production bonus
     */
    void AddBonus(ResourceType resource, const ProductionBonus& bonus);

    /**
     * @brief Remove a bonus by source name
     */
    void RemoveBonus(ResourceType resource, const std::string& source);

    /**
     * @brief Get total bonus multiplier for a resource
     */
    [[nodiscard]] float GetBonusMultiplier(ResourceType resource) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    using ProductionCallback = std::function<void(Building*, ResourceType, float amount)>;
    void SetOnProduction(ProductionCallback callback) { m_onProduction = std::move(callback); }

    using CraftingCallback = std::function<void(Building*, CraftedItemType)>;
    void SetOnCraftingComplete(CraftingCallback callback) {
        m_onCraftingComplete = std::move(callback);
    }

    using StarvationCallback = std::function<void(int affectedWorkers)>;
    void SetOnStarvation(StarvationCallback callback) { m_onStarvation = std::move(callback); }

private:
    void UpdateBuilding(Building* building, float deltaTime);
    void UpdateCrafting(Building* workshop, float deltaTime);
    void ProcessFoodConsumption(float deltaTime);

    Construction* m_construction = nullptr;
    ResourceStockpile* m_resources = nullptr;

    // Production state per building
    struct BuildingProductionState {
        bool paused = false;
        float accumulatedProduction = 0.0f;

        // Crafting state (for workshops)
        bool isCrafting = false;
        CraftedItemType craftingItem = CraftedItemType::Pickaxe;
        float craftingProgress = 0.0f;
    };
    std::unordered_map<Building*, BuildingProductionState> m_buildingStates;

    // Production bonuses
    std::unordered_map<ResourceType, std::vector<ProductionBonus>> m_bonuses;

    // Callbacks
    ProductionCallback m_onProduction;
    CraftingCallback m_onCraftingComplete;
    StarvationCallback m_onStarvation;

    // Tracking
    float m_totalFoodConsumption = 0.0f;
    bool m_isStarving = false;
};

// ============================================================================
// Production Manager Implementation (Inline for header-only convenience)
// ============================================================================

inline void ProductionManager::Initialize(Construction* construction, ResourceStockpile* resources) {
    m_construction = construction;
    m_resources = resources;
}

inline void ProductionManager::Update(float deltaTime) {
    if (!m_construction || !m_resources) return;

    // Update production for all buildings
    for (const auto& building : m_construction->GetBuildings()) {
        if (building && building->IsOperational()) {
            UpdateBuilding(building.get(), deltaTime);
        }
    }

    // Process food consumption
    ProcessFoodConsumption(deltaTime);
}

inline void ProductionManager::UpdateBuilding(Building* building, float deltaTime) {
    if (!building || !IsProductionBuilding(building->GetBuildingType())) {
        // Check for workshop crafting
        if (building && building->GetBuildingType() == BuildingType::Workshop) {
            UpdateCrafting(building, deltaTime);
        }
        return;
    }

    auto& state = m_buildingStates[building];
    if (state.paused) return;

    // Get production info
    ProductionOutput output = GetBuildingProduction(building->GetBuildingType());

    // Calculate worker contribution
    int workerCount = building->GetAssignedWorkerCount();
    float totalSkill = 0.0f;
    for (const Worker* worker : building->GetAssignedWorkers()) {
        if (worker) {
            totalSkill += worker->GetSkillLevel();
        }
    }

    // Calculate production
    float productionPerMinute = output.CalculateProduction(workerCount, totalSkill);

    // Apply level bonus
    productionPerMinute *= 1.0f + (building->GetLevel() - 1) * 0.3f;

    // Apply global bonus
    productionPerMinute *= GetBonusMultiplier(output.resource);

    // Accumulate production (convert to per-second)
    state.accumulatedProduction += (productionPerMinute / 60.0f) * deltaTime;

    // When we accumulate at least 1 unit, add to resources
    if (state.accumulatedProduction >= 1.0f) {
        int wholeUnits = static_cast<int>(state.accumulatedProduction);
        state.accumulatedProduction -= wholeUnits;

        // Add to resources
        switch (output.resource) {
            case ResourceType::Wood:
                m_resources->AddWood(wholeUnits);
                break;
            case ResourceType::Stone:
                m_resources->AddStone(wholeUnits);
                break;
            case ResourceType::Metal:
                m_resources->AddMetal(wholeUnits);
                break;
            case ResourceType::Food:
                m_resources->AddFood(wholeUnits);
                break;
            default:
                break;
        }

        if (m_onProduction) {
            m_onProduction(building, output.resource, static_cast<float>(wholeUnits));
        }
    }
}

inline void ProductionManager::UpdateCrafting(Building* workshop, float deltaTime) {
    auto it = m_buildingStates.find(workshop);
    if (it == m_buildingStates.end() || !it->second.isCrafting) {
        return;
    }

    auto& state = it->second;

    // Calculate crafting speed based on workers
    int workerCount = workshop->GetAssignedWorkerCount();
    if (workerCount == 0) return;  // Need workers to craft

    float totalSkill = 0.0f;
    for (const Worker* worker : workshop->GetAssignedWorkers()) {
        if (worker) {
            totalSkill += worker->GetSkillLevel();
        }
    }

    CraftingRecipe recipe = GetCraftingRecipe(state.craftingItem);
    float craftSpeed = (100.0f / recipe.craftTime) * (totalSkill / workerCount);

    state.craftingProgress += craftSpeed * deltaTime;

    if (state.craftingProgress >= 100.0f) {
        // Crafting complete
        state.isCrafting = false;
        state.craftingProgress = 0.0f;

        if (m_onCraftingComplete) {
            m_onCraftingComplete(workshop, state.craftingItem);
        }
    }
}

inline void ProductionManager::ProcessFoodConsumption(float deltaTime) {
    if (!m_construction || !m_resources) return;

    // Count workers and calculate consumption
    int totalWorkers = 0;
    for (const auto& building : m_construction->GetBuildings()) {
        if (building && building->IsOperational()) {
            totalWorkers += building->GetAssignedWorkerCount();
        }
    }

    m_totalFoodConsumption = FoodConsumption::CalculateConsumption(totalWorkers, 0);
    float foodNeeded = (m_totalFoodConsumption / 60.0f) * deltaTime;

    // Try to consume food
    int foodToConsume = static_cast<int>(std::ceil(foodNeeded));
    if (foodToConsume > 0 && m_resources->GetFood() < foodToConsume) {
        // Starvation!
        m_isStarving = true;
        if (m_onStarvation) {
            m_onStarvation(totalWorkers);
        }
    } else {
        m_isStarving = false;
        m_resources->SpendFood(foodToConsume);
    }
}

inline void ProductionManager::PauseProduction(Building* building) {
    if (building) {
        m_buildingStates[building].paused = true;
    }
}

inline void ProductionManager::ResumeProduction(Building* building) {
    if (building) {
        m_buildingStates[building].paused = false;
    }
}

inline bool ProductionManager::IsProductionPaused(Building* building) const {
    auto it = m_buildingStates.find(building);
    return it != m_buildingStates.end() && it->second.paused;
}

inline bool ProductionManager::StartCrafting(Building* workshop, CraftedItemType item) {
    if (!workshop || workshop->GetBuildingType() != BuildingType::Workshop) {
        return false;
    }
    if (!workshop->IsOperational()) {
        return false;
    }

    // Check workshop level
    CraftingRecipe recipe = GetCraftingRecipe(item);
    if (workshop->GetLevel() < recipe.workshopLevel) {
        return false;
    }

    // Check resources
    if (!recipe.CanAfford(m_resources->GetWood(), m_resources->GetStone(),
                          m_resources->GetMetal())) {
        return false;
    }

    // Spend resources
    m_resources->SpendWood(recipe.woodCost);
    m_resources->SpendStone(recipe.stoneCost);
    m_resources->SpendMetal(recipe.metalCost);

    // Start crafting
    auto& state = m_buildingStates[workshop];
    state.isCrafting = true;
    state.craftingItem = item;
    state.craftingProgress = 0.0f;

    return true;
}

inline void ProductionManager::CancelCrafting(Building* workshop) {
    auto it = m_buildingStates.find(workshop);
    if (it != m_buildingStates.end() && it->second.isCrafting) {
        // Refund partial resources
        CraftingRecipe recipe = GetCraftingRecipe(it->second.craftingItem);
        float refundPercent = 1.0f - (it->second.craftingProgress / 100.0f);

        m_resources->AddWood(static_cast<int>(recipe.woodCost * refundPercent));
        m_resources->AddStone(static_cast<int>(recipe.stoneCost * refundPercent));
        m_resources->AddMetal(static_cast<int>(recipe.metalCost * refundPercent));

        it->second.isCrafting = false;
        it->second.craftingProgress = 0.0f;
    }
}

inline float ProductionManager::GetCraftingProgress(Building* workshop) const {
    auto it = m_buildingStates.find(workshop);
    if (it != m_buildingStates.end() && it->second.isCrafting) {
        return it->second.craftingProgress;
    }
    return 0.0f;
}

inline std::optional<CraftedItemType> ProductionManager::GetCraftingItem(Building* workshop) const {
    auto it = m_buildingStates.find(workshop);
    if (it != m_buildingStates.end() && it->second.isCrafting) {
        return it->second.craftingItem;
    }
    return std::nullopt;
}

inline float ProductionManager::GetTotalProductionRate(ResourceType resource) const {
    float total = 0.0f;

    if (!m_construction) return total;

    for (const auto& building : m_construction->GetBuildings()) {
        if (building && building->IsOperational()) {
            ProductionOutput output = GetBuildingProduction(building->GetBuildingType());
            if (output.resource == resource && output.baseAmountPerMinute > 0) {
                int workerCount = building->GetAssignedWorkerCount();
                float totalSkill = 0.0f;
                for (const Worker* worker : building->GetAssignedWorkers()) {
                    if (worker) totalSkill += worker->GetSkillLevel();
                }

                float rate = output.CalculateProduction(workerCount, totalSkill);
                rate *= 1.0f + (building->GetLevel() - 1) * 0.3f;
                rate *= GetBonusMultiplier(resource);
                total += rate;
            }
        }
    }

    return total;
}

inline float ProductionManager::GetBuildingProductionRate(Building* building) const {
    if (!building || !building->IsOperational()) return 0.0f;

    ProductionOutput output = GetBuildingProduction(building->GetBuildingType());
    if (output.baseAmountPerMinute == 0) return 0.0f;

    int workerCount = building->GetAssignedWorkerCount();
    float totalSkill = 0.0f;
    for (const Worker* worker : building->GetAssignedWorkers()) {
        if (worker) totalSkill += worker->GetSkillLevel();
    }

    float rate = output.CalculateProduction(workerCount, totalSkill);
    rate *= 1.0f + (building->GetLevel() - 1) * 0.3f;
    rate *= GetBonusMultiplier(output.resource);

    return rate;
}

inline float ProductionManager::GetFoodConsumptionRate() const {
    return m_totalFoodConsumption;
}

inline float ProductionManager::GetNetFoodRate() const {
    return GetTotalProductionRate(ResourceType::Food) - m_totalFoodConsumption;
}

inline void ProductionManager::AddBonus(ResourceType resource, const ProductionBonus& bonus) {
    m_bonuses[resource].push_back(bonus);
}

inline void ProductionManager::RemoveBonus(ResourceType resource, const std::string& source) {
    auto& bonuses = m_bonuses[resource];
    bonuses.erase(
        std::remove_if(bonuses.begin(), bonuses.end(),
            [&source](const ProductionBonus& b) { return b.source == source; }),
        bonuses.end()
    );
}

inline float ProductionManager::GetBonusMultiplier(ResourceType resource) const {
    float multiplier = 1.0f;

    auto it = m_bonuses.find(resource);
    if (it != m_bonuses.end()) {
        for (const auto& bonus : it->second) {
            multiplier *= bonus.multiplier;
        }
    }

    return multiplier;
}

} // namespace Vehement
