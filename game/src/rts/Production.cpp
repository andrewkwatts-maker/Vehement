#include "Production.hpp"
#include <algorithm>

namespace Vehement {
namespace RTS {

// ============================================================================
// Building Type Helpers
// ============================================================================

const char* GetBuildingTypeName(ProductionBuildingType type) {
    switch (type) {
        case ProductionBuildingType::Farm:       return "Farm";
        case ProductionBuildingType::LumberMill: return "Lumber Mill";
        case ProductionBuildingType::Quarry:     return "Quarry";
        case ProductionBuildingType::Foundry:    return "Foundry";
        case ProductionBuildingType::Workshop:   return "Workshop";
        case ProductionBuildingType::Refinery:   return "Refinery";
        case ProductionBuildingType::Hospital:   return "Hospital";
        case ProductionBuildingType::Armory:     return "Armory";
        case ProductionBuildingType::Mint:       return "Mint";
        case ProductionBuildingType::Warehouse:  return "Warehouse";
        default:                                 return "Unknown";
    }
}

const char* GetBuildingTypeDescription(ProductionBuildingType type) {
    switch (type) {
        case ProductionBuildingType::Farm:
            return "Grows food for your workers. No inputs required.";
        case ProductionBuildingType::LumberMill:
            return "Processes raw wood into refined lumber.";
        case ProductionBuildingType::Quarry:
            return "Cuts raw stone into usable building blocks.";
        case ProductionBuildingType::Foundry:
            return "Smelts metal ore into refined ingots. Requires fuel.";
        case ProductionBuildingType::Workshop:
            return "Crafts equipment and goods from raw materials.";
        case ProductionBuildingType::Refinery:
            return "Refines crude fuel into usable form.";
        case ProductionBuildingType::Hospital:
            return "Heals workers and produces medicine.";
        case ProductionBuildingType::Armory:
            return "Manufactures ammunition for defense.";
        case ProductionBuildingType::Mint:
            return "Converts precious materials into currency.";
        case ProductionBuildingType::Warehouse:
            return "Increases storage capacity for all resources.";
        default:
            return "Unknown building type.";
    }
}

ResourceCost GetBuildingCost(ProductionBuildingType type) {
    switch (type) {
        case ProductionBuildingType::Farm:
            return ResourceCost(ResourceType::Wood, 30)
                .Add(ResourceType::Stone, 10);

        case ProductionBuildingType::LumberMill:
            return ResourceCost(ResourceType::Wood, 50)
                .Add(ResourceType::Stone, 20)
                .Add(ResourceType::Metal, 10);

        case ProductionBuildingType::Quarry:
            return ResourceCost(ResourceType::Wood, 40)
                .Add(ResourceType::Stone, 30)
                .Add(ResourceType::Metal, 15);

        case ProductionBuildingType::Foundry:
            return ResourceCost(ResourceType::Wood, 30)
                .Add(ResourceType::Stone, 50)
                .Add(ResourceType::Metal, 25);

        case ProductionBuildingType::Workshop:
            return ResourceCost(ResourceType::Wood, 60)
                .Add(ResourceType::Stone, 20)
                .Add(ResourceType::Metal, 20);

        case ProductionBuildingType::Refinery:
            return ResourceCost(ResourceType::Stone, 40)
                .Add(ResourceType::Metal, 30);

        case ProductionBuildingType::Hospital:
            return ResourceCost(ResourceType::Wood, 40)
                .Add(ResourceType::Stone, 30)
                .Add(ResourceType::Metal, 20)
                .Add(ResourceType::Medicine, 5);

        case ProductionBuildingType::Armory:
            return ResourceCost(ResourceType::Wood, 30)
                .Add(ResourceType::Stone, 40)
                .Add(ResourceType::Metal, 40);

        case ProductionBuildingType::Mint:
            return ResourceCost(ResourceType::Wood, 50)
                .Add(ResourceType::Stone, 50)
                .Add(ResourceType::Metal, 50)
                .Add(ResourceType::Coins, 100);

        case ProductionBuildingType::Warehouse:
            return ResourceCost(ResourceType::Wood, 80)
                .Add(ResourceType::Stone, 40);

        default:
            return ResourceCost(ResourceType::Wood, 50);
    }
}

// ============================================================================
// ProductionRecipe Implementation
// ============================================================================

bool ProductionRecipe::CanProduce(const ResourceStock& stock) const {
    for (const auto& [type, amount] : inputs) {
        if (!stock.CanAfford(type, amount)) return false;
    }
    return true;
}

bool ProductionRecipe::ConsumeInputs(ResourceStock& stock) const {
    if (!CanProduce(stock)) return false;

    for (const auto& [type, amount] : inputs) {
        stock.Remove(type, amount);
    }
    return true;
}

int ProductionRecipe::AddOutputs(ResourceStock& stock) const {
    int totalAdded = 0;
    for (const auto& [type, amount] : outputs) {
        totalAdded += stock.Add(type, amount);
    }
    return totalAdded;
}

ResourceCost ProductionRecipe::GetInputCost() const {
    ResourceCost cost;
    for (const auto& [type, amount] : inputs) {
        cost.Add(type, amount);
    }
    return cost;
}

float ProductionRecipe::GetEfficiency() const {
    if (productionTime <= 0.0f) return 0.0f;

    const auto& values = GetResourceValues();

    float inputValue = 0.0f;
    for (const auto& [type, amount] : inputs) {
        inputValue += amount * values.GetBaseValue(type);
    }

    float outputValue = 0.0f;
    for (const auto& [type, amount] : outputs) {
        outputValue += amount * values.GetBaseValue(type);
    }

    float netValue = outputValue - inputValue;
    return netValue / productionTime;
}

// ============================================================================
// ProductionBuilding Implementation
// ============================================================================

ResourceCost ProductionBuilding::GetUpgradeCost() const {
    if (level >= MAX_LEVEL) return ResourceCost();

    // Upgrade cost scales with level
    float multiplier = 1.0f + (level - 1) * 0.5f;
    ResourceCost baseCost = GetBuildingCost(type);

    return baseCost * multiplier;
}

// ============================================================================
// ProductionSystem Implementation
// ============================================================================

ProductionSystem::ProductionSystem() = default;

ProductionSystem::~ProductionSystem() {
    Shutdown();
}

void ProductionSystem::Initialize(const ProductionConfig& config) {
    m_config = config;
    m_scarcitySettings = ScarcitySettings::Normal();

    // Initialize rate tracking
    for (int i = 0; i < static_cast<int>(ResourceType::COUNT); ++i) {
        auto type = static_cast<ResourceType>(i);
        m_productionRates[type] = 0.0f;
        m_consumptionRates[type] = 0.0f;
    }

    InitializeDefaultRecipes();
    m_initialized = true;
}

void ProductionSystem::Shutdown() {
    m_buildings.clear();
    m_recipes.clear();
    m_initialized = false;
}

void ProductionSystem::Update(float deltaTime) {
    if (!m_initialized || !m_resourceStock) return;

    // Reset rate tracking
    for (auto& [type, rate] : m_productionRates) {
        rate = 0.0f;
    }
    for (auto& [type, rate] : m_consumptionRates) {
        rate = 0.0f;
    }

    for (auto& building : m_buildings) {
        UpdateBuilding(building, deltaTime);
    }
}

// -------------------------------------------------------------------------
// Recipe Management
// -------------------------------------------------------------------------

uint32_t ProductionSystem::RegisterRecipe(const ProductionRecipe& recipe) {
    ProductionRecipe r = recipe;
    r.id = GenerateRecipeId();
    m_recipes.push_back(std::move(r));
    return m_recipes.back().id;
}

std::vector<const ProductionRecipe*> ProductionSystem::GetRecipesForBuilding(ProductionBuildingType type) const {
    std::vector<const ProductionRecipe*> result;
    for (const auto& recipe : m_recipes) {
        if (recipe.buildingType == type && recipe.unlocked) {
            result.push_back(&recipe);
        }
    }
    return result;
}

const ProductionRecipe* ProductionSystem::GetRecipe(uint32_t recipeId) const {
    for (const auto& recipe : m_recipes) {
        if (recipe.id == recipeId) return &recipe;
    }
    return nullptr;
}

void ProductionSystem::UnlockRecipe(uint32_t recipeId) {
    for (auto& recipe : m_recipes) {
        if (recipe.id == recipeId) {
            recipe.unlocked = true;
            return;
        }
    }
}

void ProductionSystem::InitializeDefaultRecipes() {
    RegisterRecipe(DefaultRecipes::FarmFood());
    RegisterRecipe(DefaultRecipes::ProcessWood());
    RegisterRecipe(DefaultRecipes::ProcessStone());
    RegisterRecipe(DefaultRecipes::SmeltMetal());
    RegisterRecipe(DefaultRecipes::CraftEquipment());
    RegisterRecipe(DefaultRecipes::RefineFuel());
    RegisterRecipe(DefaultRecipes::CreateMedicine());
    RegisterRecipe(DefaultRecipes::ManufactureAmmo());
    RegisterRecipe(DefaultRecipes::MintCoins());
}

// -------------------------------------------------------------------------
// Building Management
// -------------------------------------------------------------------------

ProductionBuilding* ProductionSystem::CreateBuilding(
    ProductionBuildingType type,
    const glm::vec2& position,
    ResourceStock& stock
) {
    // Check if we can afford it
    ResourceCost cost = GetBuildingCost(type);
    if (!stock.CanAfford(cost)) return nullptr;

    // Check building limit
    if (GetBuildingCount(type) >= m_config.maxBuildingsPerType) return nullptr;

    // Spend resources
    if (!stock.Spend(cost)) return nullptr;

    return CreateBuildingFree(type, position);
}

ProductionBuilding* ProductionSystem::CreateBuildingFree(
    ProductionBuildingType type,
    const glm::vec2& position
) {
    ProductionBuilding building;
    building.id = GenerateBuildingId();
    building.type = type;
    building.position = position;
    building.name = GetBuildingTypeName(type);

    // Type-specific defaults
    switch (type) {
        case ProductionBuildingType::Farm:
            building.maxWorkers = 4;
            break;
        case ProductionBuildingType::LumberMill:
        case ProductionBuildingType::Quarry:
            building.maxWorkers = 3;
            break;
        case ProductionBuildingType::Foundry:
        case ProductionBuildingType::Workshop:
            building.maxWorkers = 4;
            break;
        case ProductionBuildingType::Refinery:
            building.maxWorkers = 2;
            break;
        case ProductionBuildingType::Hospital:
            building.maxWorkers = 3;
            break;
        case ProductionBuildingType::Armory:
            building.maxWorkers = 3;
            break;
        case ProductionBuildingType::Mint:
            building.maxWorkers = 2;
            break;
        case ProductionBuildingType::Warehouse:
            building.maxWorkers = 1;
            building.storageBonus = 200; // Per resource type
            break;
        default:
            break;
    }

    m_buildings.push_back(std::move(building));

    if (m_onBuildingCreated) {
        m_onBuildingCreated(m_buildings.back());
    }

    return &m_buildings.back();
}

void ProductionSystem::RemoveBuilding(uint32_t buildingId) {
    auto it = std::find_if(m_buildings.begin(), m_buildings.end(),
        [buildingId](const ProductionBuilding& b) { return b.id == buildingId; });

    if (it != m_buildings.end()) {
        if (m_onBuildingDestroyed) {
            m_onBuildingDestroyed(*it);
        }
        m_buildings.erase(it);
    }
}

ProductionBuilding* ProductionSystem::GetBuilding(uint32_t buildingId) {
    for (auto& b : m_buildings) {
        if (b.id == buildingId) return &b;
    }
    return nullptr;
}

const ProductionBuilding* ProductionSystem::GetBuilding(uint32_t buildingId) const {
    for (const auto& b : m_buildings) {
        if (b.id == buildingId) return &b;
    }
    return nullptr;
}

std::vector<ProductionBuilding*> ProductionSystem::GetBuildingsByType(ProductionBuildingType type) {
    std::vector<ProductionBuilding*> result;
    for (auto& b : m_buildings) {
        if (b.type == type) result.push_back(&b);
    }
    return result;
}

bool ProductionSystem::UpgradeBuilding(uint32_t buildingId, ResourceStock& stock) {
    ProductionBuilding* building = GetBuilding(buildingId);
    if (!building || !building->CanUpgrade()) return false;

    ResourceCost cost = building->GetUpgradeCost();
    if (!stock.Spend(cost)) return false;

    building->level++;

    // Warehouse upgrade increases storage bonus
    if (building->type == ProductionBuildingType::Warehouse) {
        building->storageBonus = 200 * building->level;
    }

    return true;
}

bool ProductionSystem::RepairBuilding(uint32_t buildingId, ResourceStock& stock) {
    ProductionBuilding* building = GetBuilding(buildingId);
    if (!building) return false;

    float damagePercent = 1.0f - (building->health / building->maxHealth);
    if (damagePercent <= 0.0f) return true; // Already fully repaired

    // Repair cost is proportional to damage
    ResourceCost baseCost = GetBuildingCost(building->type);
    ResourceCost repairCost = baseCost * (damagePercent * 0.5f);

    if (!stock.Spend(repairCost)) return false;

    building->health = building->maxHealth;
    return true;
}

// -------------------------------------------------------------------------
// Production Queue
// -------------------------------------------------------------------------

bool ProductionSystem::QueueProduction(uint32_t buildingId, uint32_t recipeId, int repeat) {
    ProductionBuilding* building = GetBuilding(buildingId);
    const ProductionRecipe* recipe = GetRecipe(recipeId);

    if (!building || !recipe) return false;
    if (recipe->buildingType != building->type) return false;
    if (!building->CanQueueProduction()) return false;

    ProductionQueueItem item;
    item.recipe = recipe;
    item.progress = 0.0f;
    item.paused = false;
    item.repeatCount = repeat;

    building->productionQueue.push_back(item);
    return true;
}

void ProductionSystem::CancelProduction(uint32_t buildingId, int queueIndex) {
    ProductionBuilding* building = GetBuilding(buildingId);
    if (!building) return;

    if (queueIndex >= 0 && queueIndex < static_cast<int>(building->productionQueue.size())) {
        // If canceling current production, refund partial inputs
        if (queueIndex == 0 && building->productionQueue[0].progress > 0.0f) {
            const ProductionRecipe* recipe = building->productionQueue[0].recipe;
            if (recipe && m_resourceStock) {
                // Refund 50% of inputs
                for (const auto& [type, amount] : recipe->inputs) {
                    int refund = static_cast<int>(amount * 0.5f);
                    m_resourceStock->Add(type, refund);
                }
            }
        }

        building->productionQueue.erase(building->productionQueue.begin() + queueIndex);
    }
}

void ProductionSystem::SetBuildingPaused(uint32_t buildingId, bool paused) {
    ProductionBuilding* building = GetBuilding(buildingId);
    if (building) {
        building->paused = paused;
    }
}

void ProductionSystem::ClearQueue(uint32_t buildingId) {
    ProductionBuilding* building = GetBuilding(buildingId);
    if (building) {
        building->productionQueue.clear();
    }
}

// -------------------------------------------------------------------------
// Worker Management
// -------------------------------------------------------------------------

bool ProductionSystem::AssignWorker(uint32_t buildingId) {
    ProductionBuilding* building = GetBuilding(buildingId);
    if (!building || !building->CanAssignWorker()) return false;

    building->assignedWorkers++;
    return true;
}

bool ProductionSystem::RemoveWorker(uint32_t buildingId) {
    ProductionBuilding* building = GetBuilding(buildingId);
    if (!building || building->assignedWorkers <= 0) return false;

    building->assignedWorkers--;
    return true;
}

// -------------------------------------------------------------------------
// Storage Management
// -------------------------------------------------------------------------

int ProductionSystem::CalculateTotalStorageBonus() const {
    int total = 0;
    for (const auto& building : m_buildings) {
        if (building.type == ProductionBuildingType::Warehouse && building.operational) {
            total += building.storageBonus;
        }
    }
    return total;
}

// -------------------------------------------------------------------------
// Statistics
// -------------------------------------------------------------------------

float ProductionSystem::GetProductionRate(ResourceType type) const {
    auto it = m_productionRates.find(type);
    return it != m_productionRates.end() ? it->second : 0.0f;
}

float ProductionSystem::GetConsumptionRate(ResourceType type) const {
    auto it = m_consumptionRates.find(type);
    return it != m_consumptionRates.end() ? it->second : 0.0f;
}

int ProductionSystem::GetBuildingCount(ProductionBuildingType type) const {
    return static_cast<int>(std::count_if(m_buildings.begin(), m_buildings.end(),
        [type](const ProductionBuilding& b) { return b.type == type; }));
}

int ProductionSystem::GetTotalProductionValue() const {
    int total = 0;
    for (const auto& building : m_buildings) {
        total += static_cast<int>(GetResourceValues().CalculateValue(GetBuildingCost(building.type)));
    }
    return total;
}

// -------------------------------------------------------------------------
// Configuration
// -------------------------------------------------------------------------

void ProductionSystem::ApplyScarcitySettings(const ScarcitySettings& settings) {
    m_scarcitySettings = settings;
}

// -------------------------------------------------------------------------
// Private Methods
// -------------------------------------------------------------------------

void ProductionSystem::UpdateBuilding(ProductionBuilding& building, float deltaTime) {
    if (!building.operational || building.paused) return;
    if (building.productionQueue.empty()) return;
    if (building.assignedWorkers <= 0) return;

    auto& currentItem = building.productionQueue[0];
    if (currentItem.paused || !currentItem.recipe) return;

    // Check if we have enough workers
    if (building.assignedWorkers < currentItem.recipe->workersRequired) return;

    // If just starting, consume inputs
    if (currentItem.progress <= 0.0f) {
        if (!currentItem.recipe->ConsumeInputs(*m_resourceStock)) {
            return; // Can't start - missing inputs
        }

        // Track consumption rate
        for (const auto& [type, amount] : currentItem.recipe->inputs) {
            m_consumptionRates[type] += amount / currentItem.recipe->productionTime;
        }
    }

    // Progress production
    float speed = building.GetEffectiveSpeed() * m_config.baseProductionSpeed;
    float progressDelta = deltaTime / currentItem.recipe->productionTime * speed;
    currentItem.progress += progressDelta;

    // Track production rate
    for (const auto& [type, amount] : currentItem.recipe->outputs) {
        m_productionRates[type] += (amount / currentItem.recipe->productionTime) * speed;
    }

    // Check completion
    if (currentItem.progress >= 1.0f) {
        CompleteProduction(building, currentItem);
    }
}

void ProductionSystem::CompleteProduction(ProductionBuilding& building, ProductionQueueItem& item) {
    if (!item.recipe || !m_resourceStock) return;

    // Add outputs
    item.recipe->AddOutputs(*m_resourceStock);

    if (m_onProductionComplete) {
        m_onProductionComplete(building, *item.recipe);
    }

    // Handle repeat
    if (item.ShouldRepeat()) {
        item.progress = 0.0f;
        if (item.repeatCount > 0) {
            item.repeatCount--;
        }
    } else {
        // Remove completed item
        building.productionQueue.erase(building.productionQueue.begin());
    }
}

uint32_t ProductionSystem::GenerateBuildingId() {
    return m_nextBuildingId++;
}

uint32_t ProductionSystem::GenerateRecipeId() {
    return m_nextRecipeId++;
}

} // namespace RTS
} // namespace Vehement
