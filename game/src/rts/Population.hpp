#pragma once

#include "Worker.hpp"
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <string>

namespace Vehement {

// Forward declarations
class EntityManager;
class Building;

/**
 * @brief Statistics for population tracking
 */
struct PopulationStats {
    // Counts
    int totalWorkers = 0;
    int idleWorkers = 0;
    int workingWorkers = 0;
    int restingWorkers = 0;
    int injuredWorkers = 0;
    int fleeingWorkers = 0;

    // Capacity
    int housingCapacity = 0;
    int availableHousing = 0;

    // Resources
    float totalFoodConsumption = 0.0f;  ///< Food consumed per day
    float averageProductivity = 0.0f;   ///< Average worker productivity
    float averageMorale = 0.0f;         ///< Average morale
    float averageHealth = 0.0f;         ///< Average health
    float averageLoyalty = 0.0f;        ///< Average loyalty

    // Job distribution
    std::unordered_map<WorkerJob, int> workersByJob;

    // Events
    int deathsToday = 0;
    int desertionsToday = 0;
    int recruitsToday = 0;
};

/**
 * @brief Happiness/morale factors
 */
struct MoraleFactors {
    float foodQuality = 0.0f;       ///< Bonus/penalty from food variety
    float housingQuality = 0.0f;    ///< Bonus/penalty from housing conditions
    float safety = 0.0f;            ///< Bonus/penalty from recent attacks
    float overwork = 0.0f;          ///< Penalty from excessive work
    float leadership = 0.0f;        ///< Bonus from player actions/buildings

    /** @brief Get total morale modifier */
    [[nodiscard]] float GetTotal() const noexcept {
        return foodQuality + housingQuality + safety + overwork + leadership;
    }
};

/**
 * @brief Population growth conditions
 */
struct GrowthConditions {
    bool hasExcessFood = false;     ///< Food production > consumption
    bool hasExcessHousing = false;  ///< Housing > population
    bool isHighMorale = false;      ///< Average morale > 70
    bool isSafe = false;            ///< No recent attacks

    /** @brief Check if conditions are met for population growth */
    [[nodiscard]] bool CanGrow() const noexcept {
        return hasExcessFood && hasExcessHousing && isHighMorale && isSafe;
    }
};

/**
 * @brief Population management system
 *
 * Tracks all workers under player control and manages:
 * - Housing capacity and assignments
 * - Food consumption and distribution
 * - Happiness/morale system
 * - Population growth (if conditions are good)
 * - Worker death and desertion tracking
 * - Job assignments and workforce queries
 */
class Population {
public:
    // =========================================================================
    // Constants
    // =========================================================================

    static constexpr float FOOD_PER_WORKER_PER_DAY = 1.0f;  ///< Base food consumption
    static constexpr float DAY_DURATION = 60.0f;            ///< Seconds per in-game day
    static constexpr float MORALE_UPDATE_INTERVAL = 5.0f;   ///< How often to recalculate morale
    static constexpr float GROWTH_CHECK_INTERVAL = 60.0f;   ///< How often to check for growth
    static constexpr float BASE_GROWTH_CHANCE = 0.1f;       ///< Chance per day for new survivor

    // =========================================================================
    // Construction
    // =========================================================================

    Population();
    ~Population();

    // Non-copyable
    Population(const Population&) = delete;
    Population& operator=(const Population&) = delete;

    // =========================================================================
    // Core Update
    // =========================================================================

    /**
     * @brief Update population systems
     * @param deltaTime Time since last update
     * @param entityManager Entity manager for worker updates
     * @param navGraph Navigation graph for pathfinding
     */
    void Update(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph);

    // =========================================================================
    // Worker Management
    // =========================================================================

    /**
     * @brief Add a worker to the population
     * @param worker Worker to add (ownership transferred)
     * @return true if added successfully
     */
    bool AddWorker(std::unique_ptr<Worker> worker);

    /**
     * @brief Remove a worker from the population
     * @param workerId Worker's entity ID
     * @return true if removed
     */
    bool RemoveWorker(Entity::EntityId workerId);

    /**
     * @brief Get a worker by ID
     * @param workerId Worker's entity ID
     * @return Pointer to worker or nullptr
     */
    [[nodiscard]] Worker* GetWorker(Entity::EntityId workerId);
    [[nodiscard]] const Worker* GetWorker(Entity::EntityId workerId) const;

    /**
     * @brief Get all workers
     */
    [[nodiscard]] const std::vector<std::unique_ptr<Worker>>& GetWorkers() const { return m_workers; }

    /**
     * @brief Remove workers marked for removal (dead, deserted)
     */
    void CleanupWorkers();

    // =========================================================================
    // Housing
    // =========================================================================

    /**
     * @brief Register a housing building
     * @param buildingId Building ID
     * @param capacity Number of workers it can house
     * @param position Building position
     */
    void RegisterHousing(uint32_t buildingId, int capacity, const glm::vec3& position);

    /**
     * @brief Unregister a housing building (destroyed)
     * @param buildingId Building ID
     */
    void UnregisterHousing(uint32_t buildingId);

    /**
     * @brief Get total housing capacity
     */
    [[nodiscard]] int GetHousingCapacity() const noexcept { return m_housingCapacity; }

    /**
     * @brief Get available housing slots
     */
    [[nodiscard]] int GetAvailableHousing() const noexcept;

    /**
     * @brief Assign worker to housing
     * @param worker Worker to assign
     * @param buildingId Housing building ID
     * @return true if assigned successfully
     */
    bool AssignHousing(Worker* worker, uint32_t buildingId);

    /**
     * @brief Find and assign available housing for a worker
     * @param worker Worker to house
     * @return true if housing found and assigned
     */
    bool FindAndAssignHousing(Worker* worker);

    // =========================================================================
    // Workplace Assignment
    // =========================================================================

    /**
     * @brief Register a workplace building
     * @param buildingId Building ID
     * @param jobType Type of job at this workplace
     * @param maxWorkers Maximum workers for this building
     * @param position Building position
     */
    void RegisterWorkplace(uint32_t buildingId, WorkerJob jobType, int maxWorkers,
                          const glm::vec3& position);

    /**
     * @brief Unregister a workplace building
     * @param buildingId Building ID
     */
    void UnregisterWorkplace(uint32_t buildingId);

    /**
     * @brief Assign a worker to a job at a workplace
     * @param worker Worker to assign
     * @param job Job type
     * @param buildingId Workplace building ID
     * @return true if assigned successfully
     */
    bool AssignJob(Worker* worker, WorkerJob job, uint32_t buildingId);

    /**
     * @brief Unassign a worker from their current job
     * @param worker Worker to unassign
     */
    void UnassignWorker(Worker* worker);

    /**
     * @brief Get workers assigned to a building
     * @param buildingId Building ID
     * @return Vector of workers at that building
     */
    [[nodiscard]] std::vector<Worker*> GetWorkersAtBuilding(uint32_t buildingId) const;

    // =========================================================================
    // Food System
    // =========================================================================

    /**
     * @brief Add food to storage
     * @param amount Amount of food to add
     */
    void AddFood(float amount) { m_foodStorage += amount; }

    /**
     * @brief Get current food storage
     */
    [[nodiscard]] float GetFoodStorage() const noexcept { return m_foodStorage; }

    /**
     * @brief Set food storage directly
     */
    void SetFoodStorage(float amount) { m_foodStorage = std::max(0.0f, amount); }

    /**
     * @brief Get daily food consumption
     */
    [[nodiscard]] float GetDailyFoodConsumption() const noexcept;

    /**
     * @brief Get days of food remaining
     */
    [[nodiscard]] float GetDaysOfFoodRemaining() const noexcept;

    /**
     * @brief Distribute food to workers (called automatically in Update)
     * @param deltaTime Time since last distribution
     */
    void DistributeFood(float deltaTime);

    // =========================================================================
    // Morale System
    // =========================================================================

    /**
     * @brief Get current morale factors
     */
    [[nodiscard]] const MoraleFactors& GetMoraleFactors() const noexcept { return m_moraleFactors; }

    /**
     * @brief Set morale factor
     * @param factor Which factor to set
     * @param value New value
     */
    void SetMoraleFactor(const std::string& factor, float value);

    /**
     * @brief Apply morale event to all workers
     * @param amount Morale change (positive or negative)
     * @param reason Description of event (for UI/logging)
     */
    void ApplyMoraleEvent(float amount, const std::string& reason);

    /**
     * @brief Record an attack (reduces safety morale)
     */
    void RecordAttack();

    // =========================================================================
    // Population Growth
    // =========================================================================

    /**
     * @brief Get current growth conditions
     */
    [[nodiscard]] const GrowthConditions& GetGrowthConditions() const noexcept { return m_growthConditions; }

    /**
     * @brief Set population growth enabled/disabled
     */
    void SetGrowthEnabled(bool enabled) noexcept { m_growthEnabled = enabled; }

    /**
     * @brief Check if growth is enabled
     */
    [[nodiscard]] bool IsGrowthEnabled() const noexcept { return m_growthEnabled; }

    // =========================================================================
    // Queries
    // =========================================================================

    /**
     * @brief Get all idle workers
     */
    [[nodiscard]] std::vector<Worker*> GetIdleWorkers() const;

    /**
     * @brief Get workers by job type
     * @param job Job type to filter by
     */
    [[nodiscard]] std::vector<Worker*> GetWorkersByJob(WorkerJob job) const;

    /**
     * @brief Get workers by state
     * @param state Worker state to filter by
     */
    [[nodiscard]] std::vector<Worker*> GetWorkersByState(WorkerState state) const;

    /**
     * @brief Get total population count
     */
    [[nodiscard]] int GetTotalPopulation() const noexcept { return static_cast<int>(m_workers.size()); }

    /**
     * @brief Get population statistics
     */
    [[nodiscard]] const PopulationStats& GetStats() const noexcept { return m_stats; }

    /**
     * @brief Get workers in radius of position
     * @param position Center position
     * @param radius Search radius
     */
    [[nodiscard]] std::vector<Worker*> GetWorkersInRadius(const glm::vec3& position, float radius) const;

    /**
     * @brief Get nearest idle worker to position
     * @param position Reference position
     */
    [[nodiscard]] Worker* GetNearestIdleWorker(const glm::vec3& position) const;

    /**
     * @brief Get best worker for a job based on skills
     * @param job Job type
     */
    [[nodiscard]] Worker* GetBestWorkerForJob(WorkerJob job) const;

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * @brief Select workers in area
     * @param min Minimum corner of selection box
     * @param max Maximum corner of selection box
     */
    void SelectWorkersInArea(const glm::vec2& min, const glm::vec2& max);

    /**
     * @brief Get currently selected workers
     */
    [[nodiscard]] std::vector<Worker*> GetSelectedWorkers() const;

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Command selected workers to move
     * @param position Target position
     * @param navGraph Navigation graph for pathfinding
     */
    void CommandSelectedMoveTo(const glm::vec3& position, Nova::Graph* navGraph);

    /**
     * @brief Command selected workers to follow hero
     * @param follow true to follow, false to stop following
     */
    void CommandSelectedFollowHero(bool follow);

    // =========================================================================
    // Callbacks
    // =========================================================================

    using WorkerEventCallback = std::function<void(Worker&)>;
    using GrowthCallback = std::function<void(const glm::vec3& position)>;

    void SetOnWorkerDeath(WorkerEventCallback cb) { m_onWorkerDeath = std::move(cb); }
    void SetOnWorkerDesertion(WorkerEventCallback cb) { m_onWorkerDesertion = std::move(cb); }
    void SetOnPopulationGrowth(GrowthCallback cb) { m_onPopulationGrowth = std::move(cb); }

private:
    // Internal update methods
    void UpdateStatistics();
    void UpdateMoraleFactors(float deltaTime);
    void UpdateGrowthConditions();
    void CheckPopulationGrowth(float deltaTime);
    void UpdateWorkers(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph);

    // Housing tracking
    struct HousingInfo {
        uint32_t buildingId = 0;
        int capacity = 0;
        int occupancy = 0;
        glm::vec3 position{0.0f};
        std::vector<Entity::EntityId> residents;
    };

    // Workplace tracking
    struct WorkplaceInfo {
        uint32_t buildingId = 0;
        WorkerJob jobType = WorkerJob::None;
        int maxWorkers = 0;
        int currentWorkers = 0;
        glm::vec3 position{0.0f};
        std::vector<Entity::EntityId> workers;
    };

    // Workers
    std::vector<std::unique_ptr<Worker>> m_workers;
    std::unordered_map<Entity::EntityId, size_t> m_workerIndex;  // ID to vector index

    // Buildings
    std::unordered_map<uint32_t, HousingInfo> m_housing;
    std::unordered_map<uint32_t, WorkplaceInfo> m_workplaces;
    int m_housingCapacity = 0;

    // Resources
    float m_foodStorage = 0.0f;
    float m_foodDistributionTimer = 0.0f;

    // Morale
    MoraleFactors m_moraleFactors;
    float m_moraleUpdateTimer = 0.0f;
    float m_lastAttackTime = 0.0f;

    // Growth
    GrowthConditions m_growthConditions;
    bool m_growthEnabled = true;
    float m_growthCheckTimer = 0.0f;

    // Statistics
    PopulationStats m_stats;
    float m_totalTime = 0.0f;

    // Callbacks
    WorkerEventCallback m_onWorkerDeath;
    WorkerEventCallback m_onWorkerDesertion;
    GrowthCallback m_onPopulationGrowth;
};

} // namespace Vehement
