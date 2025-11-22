#pragma once

#include "../lifecycle/ILifecycle.hpp"
#include "../lifecycle/GameEvent.hpp"
#include "../lifecycle/ComponentLifecycle.hpp"
#include "../lifecycle/ScriptedLifecycle.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <queue>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// Building Type
// ============================================================================

/**
 * @brief Building classification
 */
enum class BaseBuildingType : uint8_t {
    Resource,       // Produces resources
    Production,     // Produces units
    Defense,        // Defensive structure
    Research,       // Unlocks upgrades
    Storage,        // Increases capacity
    Housing,        // Increases population cap
    Utility,        // Special function

    Custom = 255
};

// ============================================================================
// Building State
// ============================================================================

/**
 * @brief Current building state
 */
enum class BaseBuildingState : uint8_t {
    Blueprint,          // Placed but not started
    UnderConstruction,  // Being built
    Operational,        // Fully functional
    Damaged,            // Reduced efficiency
    Destroyed,          // Non-functional
    Upgrading           // Being upgraded
};

// ============================================================================
// Production Queue Item
// ============================================================================

/**
 * @brief Item in the building's production queue
 */
struct ProductionQueueItem {
    std::string unitType;       // Type to produce
    float timeRequired = 10.0f; // Total time
    float timeRemaining = 10.0f;// Time left
    int priority = 0;
};

// ============================================================================
// Building Stats
// ============================================================================

/**
 * @brief Building statistics
 */
struct BuildingStats {
    // Construction
    float constructionTime = 30.0f;     // Seconds to build
    float constructionProgress = 0.0f;

    // Production
    float productionSpeed = 1.0f;       // Production multiplier
    int maxQueueSize = 5;

    // Resources
    std::string producesResource;       // Resource type produced
    float productionRate = 1.0f;        // Per second

    // Defense
    float attackDamage = 0.0f;
    float attackRange = 0.0f;
    float attackSpeed = 1.0f;

    // Vision
    float visionRange = 10.0f;

    // Capacity
    int housingCapacity = 0;            // Population provided
    int workerSlots = 0;                // Workers that can work here
    int garrisonCapacity = 0;           // Units that can garrison

    // Level
    int level = 1;
    int maxLevel = 3;
};

// ============================================================================
// GarrisonComponent
// ============================================================================

/**
 * @brief Manages garrisoned units
 */
class GarrisonComponent : public ComponentBase<GarrisonComponent> {
public:
    [[nodiscard]] const char* GetTypeName() const override { return "Garrison"; }

    void OnInitialize() override;
    void OnTick(float deltaTime) override;

    bool CanGarrison(LifecycleHandle unit) const;
    bool Garrison(LifecycleHandle unit);
    bool Ungarrison(LifecycleHandle unit);
    void UngarrisonAll();

    [[nodiscard]] int GetGarrisonCount() const { return static_cast<int>(garrisonedUnits.size()); }
    [[nodiscard]] bool IsFull() const { return GetGarrisonCount() >= capacity; }

    std::vector<LifecycleHandle> garrisonedUnits;
    int capacity = 0;
    glm::vec3 rallyPoint{0.0f};
};

// ============================================================================
// ProductionComponent
// ============================================================================

/**
 * @brief Handles unit production
 */
class ProductionComponent : public ComponentBase<ProductionComponent> {
public:
    [[nodiscard]] const char* GetTypeName() const override { return "Production"; }

    void OnInitialize() override;
    void OnTick(float deltaTime) override;

    bool QueueProduction(const std::string& unitType, float time);
    bool CancelProduction(size_t index);
    void CancelAll();

    [[nodiscard]] bool IsProducing() const { return !queue.empty(); }
    [[nodiscard]] const ProductionQueueItem* GetCurrentProduction() const;
    [[nodiscard]] float GetProgress() const;

    std::vector<ProductionQueueItem> queue;
    int maxQueueSize = 5;
    float productionSpeed = 1.0f;
    glm::vec3 spawnPoint{0.0f};

    // Callback when production completes
    std::function<void(const std::string&)> onProductionComplete;
};

// ============================================================================
// BaseBuilding
// ============================================================================

/**
 * @brief Base class for all buildings
 *
 * Provides:
 * - Construction system
 * - Production queues
 * - Garrison functionality
 * - Resource generation
 * - Upgrade system
 *
 * JSON Config:
 * {
 *   "id": "building_barracks",
 *   "type": "building",
 *   "building_type": "Production",
 *   "stats": {
 *     "max_health": 500,
 *     "construction_time": 30,
 *     "production_speed": 1.2,
 *     "garrison_capacity": 4
 *   },
 *   "produces": ["unit_soldier", "unit_archer"],
 *   "components": ["transform", "health", "garrison", "production"]
 * }
 */
class BaseBuilding : public ScriptedLifecycle {
public:
    BaseBuilding();
    ~BaseBuilding() override;

    // =========================================================================
    // ILifecycle Implementation
    // =========================================================================

    void OnCreate(const nlohmann::json& config) override;
    void OnTick(float deltaTime) override;
    bool OnEvent(const GameEvent& event) override;
    void OnDestroy() override;

    [[nodiscard]] const char* GetTypeName() const override { return "BaseBuilding"; }

    // =========================================================================
    // Building Properties
    // =========================================================================

    [[nodiscard]] BaseBuildingType GetBuildingType() const { return m_buildingType; }
    void SetBuildingType(BaseBuildingType type) { m_buildingType = type; }

    [[nodiscard]] BaseBuildingState GetBuildingState() const { return m_buildingState; }
    void SetBuildingState(BaseBuildingState state);

    [[nodiscard]] BuildingStats& GetStats() { return m_stats; }
    [[nodiscard]] const BuildingStats& GetStats() const { return m_stats; }

    [[nodiscard]] bool IsOperational() const {
        return m_buildingState == BaseBuildingState::Operational;
    }
    [[nodiscard]] bool IsUnderConstruction() const {
        return m_buildingState == BaseBuildingState::UnderConstruction ||
               m_buildingState == BaseBuildingState::Blueprint;
    }

    // =========================================================================
    // Construction
    // =========================================================================

    void StartConstruction();
    void AddConstructionProgress(float amount);
    void CompleteConstruction();
    void CancelConstruction();

    [[nodiscard]] float GetConstructionProgress() const {
        return m_stats.constructionProgress;
    }

    // =========================================================================
    // Production
    // =========================================================================

    bool QueueUnit(const std::string& unitType);
    bool CancelProduction(size_t index = 0);
    [[nodiscard]] bool IsProducing() const;

    [[nodiscard]] const std::vector<std::string>& GetProducibleUnits() const {
        return m_producibleUnits;
    }
    void SetProducibleUnits(const std::vector<std::string>& units) {
        m_producibleUnits = units;
    }

    // =========================================================================
    // Garrison
    // =========================================================================

    bool GarrisonUnit(LifecycleHandle unit);
    bool UngarrisonUnit(LifecycleHandle unit);
    void UngarrisonAll();

    [[nodiscard]] int GetGarrisonCount() const;
    [[nodiscard]] bool CanGarrison() const;

    // =========================================================================
    // Grid Position
    // =========================================================================

    [[nodiscard]] const glm::ivec2& GetGridPosition() const { return m_gridPosition; }
    void SetGridPosition(const glm::ivec2& pos);
    void SetGridPosition(int x, int y) { SetGridPosition(glm::ivec2(x, y)); }

    [[nodiscard]] const glm::ivec2& GetSize() const { return m_size; }
    void SetSize(const glm::ivec2& size) { m_size = size; }

    [[nodiscard]] std::vector<glm::ivec2> GetOccupiedTiles() const;

    // =========================================================================
    // Upgrades
    // =========================================================================

    bool CanUpgrade() const;
    void StartUpgrade();
    void CompleteUpgrade();

    // =========================================================================
    // Team
    // =========================================================================

    [[nodiscard]] int GetTeamId() const { return m_teamId; }
    void SetTeamId(int teamId) { m_teamId = teamId; }

    // =========================================================================
    // Components
    // =========================================================================

    [[nodiscard]] ComponentContainer& GetComponents() { return m_components; }

    template<typename T>
    T* GetComponent() { return m_components.Get<T>(); }

    // =========================================================================
    // Script Context Override
    // =========================================================================

    [[nodiscard]] ScriptContext BuildContext() const override;

protected:
    virtual void ParseBuildingConfig(const nlohmann::json& config);
    virtual void OnConstructionComplete();
    virtual void OnProductionComplete(const std::string& unitType);
    virtual void OnUpgradeComplete();
    virtual void OnBuildingDestroyed();

    void UpdateConstruction(float deltaTime);
    void UpdateProduction(float deltaTime);
    void UpdateResourceGeneration(float deltaTime);

    BaseBuildingType m_buildingType = BaseBuildingType::Utility;
    BaseBuildingState m_buildingState = BaseBuildingState::Blueprint;
    BuildingStats m_stats;

    glm::ivec2 m_gridPosition{0, 0};
    glm::ivec2 m_size{1, 1};

    int m_teamId = 0;

    std::vector<std::string> m_producibleUnits;
    ComponentContainer m_components;
};

} // namespace Lifecycle
} // namespace Vehement
