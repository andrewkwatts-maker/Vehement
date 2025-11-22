#pragma once

#include "../entities/Entity.hpp"
#include "../entities/NPC.hpp"
#include "Needs.hpp"
#include <engine/pathfinding/Pathfinder.hpp>
#include <string>
#include <functional>
#include <memory>

namespace Nova {
    class Graph;
}

namespace Vehement {

// Forward declarations
class EntityManager;
class Building;
class WorkerAI;

/**
 * @brief Worker behavior states
 */
enum class WorkerState : uint8_t {
    Idle,           ///< Available for assignment, waiting at home or wandering
    Moving,         ///< Walking to destination (work, home, or command target)
    Working,        ///< Performing assigned task at workplace
    Resting,        ///< At housing, recovering energy
    Fleeing,        ///< Running from danger (zombies, combat)
    Injured,        ///< Needs medical attention, reduced mobility
    Dead            ///< Deceased - awaiting cleanup
};

/**
 * @brief Convert worker state to display string
 */
inline const char* WorkerStateToString(WorkerState state) {
    switch (state) {
        case WorkerState::Idle:     return "Idle";
        case WorkerState::Moving:   return "Moving";
        case WorkerState::Working:  return "Working";
        case WorkerState::Resting:  return "Resting";
        case WorkerState::Fleeing:  return "Fleeing";
        case WorkerState::Injured:  return "Injured";
        case WorkerState::Dead:     return "Dead";
        default:                    return "Unknown";
    }
}

/**
 * @brief Worker job/profession types
 */
enum class WorkerJob : uint8_t {
    None,           ///< Unassigned - will idle or do misc tasks
    Gatherer,       ///< Collects resources from the environment
    Builder,        ///< Constructs and repairs buildings
    Farmer,         ///< Works agricultural buildings (farms, orchting.)
    Guard,          ///< Defends area, patrols, doesn't flee from danger
    Crafter,        ///< Produces items at workshops
    Medic,          ///< Heals other workers and the player
    Scout,          ///< Explores fog of war, reveals map
    Trader          ///< Manages trade routes, market operations
};

/**
 * @brief Convert worker job to display string
 */
inline const char* WorkerJobToString(WorkerJob job) {
    switch (job) {
        case WorkerJob::None:      return "Unemployed";
        case WorkerJob::Gatherer:  return "Gatherer";
        case WorkerJob::Builder:   return "Builder";
        case WorkerJob::Farmer:    return "Farmer";
        case WorkerJob::Guard:     return "Guard";
        case WorkerJob::Crafter:   return "Crafter";
        case WorkerJob::Medic:     return "Medic";
        case WorkerJob::Scout:     return "Scout";
        case WorkerJob::Trader:    return "Trader";
        default:                   return "Unknown";
    }
}

/**
 * @brief Task being performed by a worker
 */
struct WorkTask {
    enum class Type : uint8_t {
        None,
        Gather,         ///< Collecting resources
        Build,          ///< Building/repairing structure
        Farm,           ///< Farming (planting, harvesting)
        Patrol,         ///< Guard patrol route
        Craft,          ///< Crafting items
        HealTarget,     ///< Healing another entity
        Scout,          ///< Exploring area
        Trade,          ///< Trading operation
        CarryResource,  ///< Transporting resources
        GoHome,         ///< Returning to housing
        GoToWork,       ///< Going to workplace
        FollowHero      ///< Following the player
    };

    Type type = Type::None;
    glm::vec3 targetPosition{0.0f};
    Entity::EntityId targetEntity = Entity::INVALID_ID;
    uint32_t targetBuilding = 0;
    float progress = 0.0f;           ///< Task completion progress (0-1)
    float duration = 0.0f;           ///< Time to complete task
    bool repeating = false;          ///< Does task repeat when done?

    /** @brief Check if task is complete */
    [[nodiscard]] bool IsComplete() const noexcept { return progress >= 1.0f; }

    /** @brief Reset task progress */
    void Reset() { progress = 0.0f; }

    /** @brief Clear the task */
    void Clear() {
        type = Type::None;
        targetPosition = glm::vec3(0.0f);
        targetEntity = Entity::INVALID_ID;
        targetBuilding = 0;
        progress = 0.0f;
        duration = 0.0f;
        repeating = false;
    }
};

/**
 * @brief Worker unit that can be assigned to tasks
 *
 * Workers are recruited NPCs that become part of the player's settlement.
 * They have needs that must be met, skills that improve with practice,
 * and can be assigned to various jobs at buildings.
 *
 * Key features:
 * - Inherits from Entity for position, health, collision
 * - Has needs (hunger, energy, morale) that affect productivity
 * - Can be assigned jobs and workplaces
 * - Has a home building where they rest
 * - Will flee from danger (unless Guard job)
 * - Loyalty affects desertion chance
 */
class Worker : public Entity {
public:
    // =========================================================================
    // Constants
    // =========================================================================

    static constexpr float DEFAULT_MOVE_SPEED = 3.5f;
    static constexpr float FLEE_SPEED_MULTIPLIER = 1.4f;
    static constexpr float INJURED_SPEED_MULTIPLIER = 0.5f;
    static constexpr float DETECTION_RADIUS = 15.0f;       ///< How far worker detects threats
    static constexpr float FLEE_DISTANCE = 25.0f;          ///< Distance to flee from threats
    static constexpr float WORK_RANGE = 2.0f;              ///< Distance to workplace to start working
    static constexpr float PATH_UPDATE_INTERVAL = 0.5f;
    static constexpr float NEEDS_UPDATE_INTERVAL = 1.0f;
    static constexpr float DEFAULT_LOYALTY = 50.0f;        ///< Starting loyalty (0-100)

    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a default worker
     */
    Worker();

    /**
     * @brief Construct a worker with specific appearance
     * @param appearanceIndex Person texture index (1-9)
     */
    explicit Worker(int appearanceIndex);

    /**
     * @brief Construct a worker from an NPC (recruitment)
     * @param npc The NPC being recruited
     */
    explicit Worker(const NPC& npc);

    ~Worker() override = default;

    // =========================================================================
    // Core Update/Render
    // =========================================================================

    /**
     * @brief Update worker state
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime) override;

    /**
     * @brief Render the worker
     */
    void Render(Nova::Renderer& renderer) override;

    /**
     * @brief Full AI update with access to game systems
     * @param deltaTime Time since last frame
     * @param entityManager For finding threats, other workers
     * @param navGraph Navigation graph for pathfinding
     */
    void UpdateAI(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph);

    // =========================================================================
    // State
    // =========================================================================

    /** @brief Get current worker state */
    [[nodiscard]] WorkerState GetWorkerState() const noexcept { return m_workerState; }

    /** @brief Set worker state (use with caution - prefer AI transitions) */
    void SetWorkerState(WorkerState state) noexcept { m_workerState = state; }

    /** @brief Check if worker is available for new tasks */
    [[nodiscard]] bool IsAvailable() const noexcept {
        return m_workerState == WorkerState::Idle && m_currentTask.type == WorkTask::Type::None;
    }

    /** @brief Check if worker can work (not dead, fleeing, or critically injured) */
    [[nodiscard]] bool CanWork() const noexcept {
        return m_workerState != WorkerState::Dead &&
               m_workerState != WorkerState::Fleeing &&
               !m_needs.IsCriticallyInjured();
    }

    // =========================================================================
    // Job Assignment
    // =========================================================================

    /** @brief Get current job */
    [[nodiscard]] WorkerJob GetJob() const noexcept { return m_job; }

    /** @brief Set job type */
    void SetJob(WorkerJob job) noexcept { m_job = job; }

    /** @brief Check if worker has a job assigned */
    [[nodiscard]] bool HasJob() const noexcept { return m_job != WorkerJob::None; }

    /** @brief Get workplace building ID */
    [[nodiscard]] uint32_t GetWorkplaceId() const noexcept { return m_workplaceId; }

    /** @brief Set workplace building */
    void SetWorkplace(uint32_t buildingId) noexcept { m_workplaceId = buildingId; }

    /** @brief Set workplace position */
    void SetWorkplacePosition(const glm::vec3& pos) noexcept { m_workplacePosition = pos; }

    /** @brief Get workplace position */
    [[nodiscard]] const glm::vec3& GetWorkplacePosition() const noexcept { return m_workplacePosition; }

    /** @brief Clear job and workplace assignment */
    void ClearJobAssignment();

    // =========================================================================
    // Housing
    // =========================================================================

    /** @brief Get home building ID */
    [[nodiscard]] uint32_t GetHomeId() const noexcept { return m_homeId; }

    /** @brief Set home building */
    void SetHome(uint32_t buildingId) noexcept { m_homeId = buildingId; }

    /** @brief Check if worker has housing assigned */
    [[nodiscard]] bool HasHome() const noexcept { return m_homeId != 0; }

    /** @brief Get home position for pathfinding */
    [[nodiscard]] const glm::vec3& GetHomePosition() const noexcept { return m_homePosition; }

    /** @brief Set home position */
    void SetHomePosition(const glm::vec3& pos) noexcept { m_homePosition = pos; }

    // =========================================================================
    // Tasks
    // =========================================================================

    /** @brief Get current task */
    [[nodiscard]] const WorkTask& GetCurrentTask() const noexcept { return m_currentTask; }

    /** @brief Get current task (mutable) */
    WorkTask& GetCurrentTask() noexcept { return m_currentTask; }

    /** @brief Assign a new task */
    void AssignTask(const WorkTask& task);

    /** @brief Clear current task */
    void ClearTask();

    /** @brief Check if worker has an active task */
    [[nodiscard]] bool HasTask() const noexcept { return m_currentTask.type != WorkTask::Type::None; }

    // =========================================================================
    // Needs System
    // =========================================================================

    /** @brief Get worker needs (const) */
    [[nodiscard]] const WorkerNeeds& GetNeeds() const noexcept { return m_needs; }

    /** @brief Get worker needs (mutable) */
    WorkerNeeds& GetNeeds() noexcept { return m_needs; }

    /** @brief Feed the worker */
    void Feed(float amount) { m_needs.Feed(amount); }

    /** @brief Get productivity based on needs and skills */
    [[nodiscard]] float GetProductivity() const noexcept;

    /** @brief Check if worker should seek rest */
    [[nodiscard]] bool ShouldRest() const noexcept { return m_needs.NeedsRest(); }

    /** @brief Check if worker should seek food */
    [[nodiscard]] bool ShouldEat() const noexcept { return m_needs.NeedsFood(); }

    // =========================================================================
    // Skills
    // =========================================================================

    /** @brief Get worker skills (const) */
    [[nodiscard]] const WorkerSkills& GetSkills() const noexcept { return m_skills; }

    /** @brief Get worker skills (mutable) */
    WorkerSkills& GetSkills() noexcept { return m_skills; }

    /** @brief Get skill level for current job */
    [[nodiscard]] float GetJobSkillLevel() const noexcept;

    /** @brief Improve skill for current job */
    void ImproveJobSkill(float amount);

    // =========================================================================
    // Personality
    // =========================================================================

    /** @brief Get personality traits */
    [[nodiscard]] const WorkerPersonality& GetPersonality() const noexcept { return m_personality; }

    /** @brief Set personality traits */
    void SetPersonality(const WorkerPersonality& personality) noexcept { m_personality = personality; }

    // =========================================================================
    // Loyalty
    // =========================================================================

    /** @brief Get loyalty level (0-100) */
    [[nodiscard]] float GetLoyalty() const noexcept { return m_loyalty; }

    /** @brief Set loyalty level */
    void SetLoyalty(float loyalty) noexcept { m_loyalty = std::clamp(loyalty, 0.0f, 100.0f); }

    /** @brief Modify loyalty (positive or negative) */
    void ModifyLoyalty(float amount) { m_loyalty = std::clamp(m_loyalty + amount, 0.0f, 100.0f); }

    /** @brief Check desertion - returns true if worker deserts */
    [[nodiscard]] bool CheckDesertion(float deltaTime);

    // =========================================================================
    // Combat / Damage
    // =========================================================================

    /**
     * @brief Take damage
     * @param amount Damage to apply
     * @param source Source entity ID
     * @return Actual damage dealt
     */
    float TakeDamage(float amount, EntityId source = INVALID_ID) override;

    /**
     * @brief Handle worker death
     */
    void Die() override;

    /** @brief Check if worker is injured */
    [[nodiscard]] bool IsInjured() const noexcept {
        return m_workerState == WorkerState::Injured || m_needs.IsInjured();
    }

    // =========================================================================
    // Appearance
    // =========================================================================

    /** @brief Get appearance index (1-9) */
    [[nodiscard]] int GetAppearanceIndex() const noexcept { return m_appearanceIndex; }

    /** @brief Set appearance index */
    void SetAppearanceIndex(int index);

    /** @brief Get unique worker name */
    [[nodiscard]] const std::string& GetWorkerName() const noexcept { return m_workerName; }

    /** @brief Set worker name */
    void SetWorkerName(const std::string& name) { m_workerName = name; }

    // =========================================================================
    // Pathfinding
    // =========================================================================

    /** @brief Check if worker has a valid path */
    [[nodiscard]] bool HasPath() const noexcept { return !m_currentPath.positions.empty(); }

    /** @brief Clear current path */
    void ClearPath();

    /**
     * @brief Request path to position
     * @param target Target position
     * @param navGraph Navigation graph
     * @return true if path was found
     */
    bool RequestPath(const glm::vec3& target, Nova::Graph& navGraph);

    /**
     * @brief Move to position (sets task and starts pathfinding)
     * @param position Target position
     * @param navGraph Navigation graph for pathfinding
     */
    void MoveTo(const glm::vec3& position, Nova::Graph* navGraph);

    // =========================================================================
    // Selection / Commands
    // =========================================================================

    /** @brief Check if worker is selected */
    [[nodiscard]] bool IsSelected() const noexcept { return m_selected; }

    /** @brief Set selection state */
    void SetSelected(bool selected) noexcept { m_selected = selected; }

    /** @brief Check if worker is following hero */
    [[nodiscard]] bool IsFollowingHero() const noexcept { return m_followingHero; }

    /** @brief Set following hero state */
    void SetFollowingHero(bool following) noexcept { m_followingHero = following; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    using DeathCallback = std::function<void(Worker&)>;
    using DesertionCallback = std::function<void(Worker&)>;
    using TaskCompleteCallback = std::function<void(Worker&, const WorkTask&)>;

    void SetDeathCallback(DeathCallback cb) { m_onDeath = std::move(cb); }
    void SetDesertionCallback(DesertionCallback cb) { m_onDesertion = std::move(cb); }
    void SetTaskCompleteCallback(TaskCompleteCallback cb) { m_onTaskComplete = std::move(cb); }

    /** @brief Check if death callback is set */
    [[nodiscard]] bool HasDeathCallback() const noexcept { return static_cast<bool>(m_onDeath); }

    /** @brief Get death callback (for checking if set) */
    [[nodiscard]] const DeathCallback& GetDeathCallback() const noexcept { return m_onDeath; }

private:
    // State machine updates
    void UpdateIdle(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph);
    void UpdateMoving(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph);
    void UpdateWorking(float deltaTime, EntityManager& entityManager);
    void UpdateResting(float deltaTime, EntityManager& entityManager);
    void UpdateFleeing(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph);
    void UpdateInjured(float deltaTime, EntityManager& entityManager);

    // Threat detection
    Entity::EntityId DetectThreat(EntityManager& entityManager);
    glm::vec3 CalculateFleeDirection(EntityManager& entityManager);

    // Path following
    void FollowPath(float deltaTime);
    void MoveToward(const glm::vec3& target, float deltaTime);

    // Needs management
    void UpdateNeeds(float deltaTime);

    // Job helpers
    [[nodiscard]] WorkTask::Type GetJobTaskType() const;
    [[nodiscard]] float GetJobTaskDuration() const;

    // State
    WorkerState m_workerState = WorkerState::Idle;
    WorkerState m_preFleeState = WorkerState::Idle;
    WorkerJob m_job = WorkerJob::None;
    WorkTask m_currentTask;

    // Building assignments
    uint32_t m_homeId = 0;
    uint32_t m_workplaceId = 0;
    glm::vec3 m_homePosition{0.0f};
    glm::vec3 m_workplacePosition{0.0f};

    // Needs, skills, personality
    WorkerNeeds m_needs;
    WorkerSkills m_skills;
    WorkerPersonality m_personality;

    // Loyalty and morale
    float m_loyalty = DEFAULT_LOYALTY;
    float m_desertionCheckTimer = 0.0f;

    // Threat handling
    Entity::EntityId m_threatId = Entity::INVALID_ID;
    glm::vec3 m_fleeTarget{0.0f};
    float m_fleeReassessTimer = 0.0f;

    // Pathfinding
    Nova::PathResult m_currentPath;
    size_t m_pathIndex = 0;
    float m_pathUpdateTimer = 0.0f;

    // Timers
    float m_needsUpdateTimer = 0.0f;
    float m_stateTimer = 0.0f;

    // Appearance
    int m_appearanceIndex = 1;
    std::string m_workerName = "Worker";

    // Selection / commands
    bool m_selected = false;
    bool m_followingHero = false;

    // Callbacks
    DeathCallback m_onDeath;
    DesertionCallback m_onDesertion;
    TaskCompleteCallback m_onTaskComplete;
};

} // namespace Vehement
