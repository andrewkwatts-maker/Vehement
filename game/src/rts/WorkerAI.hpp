#pragma once

#include "Worker.hpp"
#include "Population.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <queue>

namespace Nova {
    class Graph;
}

namespace Vehement {

// Forward declarations
class EntityManager;
class Player;
class World;

/**
 * @brief AI behavior priorities for workers
 */
enum class AIBehaviorPriority : uint8_t {
    Survival = 0,      ///< Flee from danger, seek shelter (highest)
    BasicNeeds = 1,    ///< Eat, rest when critical
    Assignment = 2,    ///< Follow job/task assignments
    SelfCare = 3,      ///< Rest when tired, eat when hungry
    Idle = 4           ///< Wander, socialize (lowest)
};

/**
 * @brief Decision result from AI evaluation
 */
struct AIDecision {
    WorkerState newState = WorkerState::Idle;
    WorkTask task;
    glm::vec3 targetPosition{0.0f};
    Entity::EntityId targetEntity = Entity::INVALID_ID;
    float urgency = 0.0f;  ///< How urgent (0-1)
    std::string reason;    ///< Debug description
};

/**
 * @brief Configuration for worker AI behavior
 */
struct WorkerAIConfig {
    // Threat response
    float threatDetectionRange = 15.0f;
    float fleeDistance = 25.0f;
    float guardEngageRange = 10.0f;
    float groupFleeThreshold = 0.5f;  ///< If this % of workers flee, others follow

    // Needs thresholds for behavior changes
    float seekFoodThreshold = 30.0f;   ///< Seek food when hunger below this
    float seekRestThreshold = 25.0f;   ///< Seek rest when energy below this
    float criticalHealthThreshold = 20.0f;

    // Work behavior
    float workStartTime = 6.0f;    ///< Hour of day to start work
    float workEndTime = 18.0f;     ///< Hour of day to end work
    bool enforceWorkHours = false; ///< If true, workers only work during hours

    // Group behavior
    float groupCohesionRange = 8.0f;   ///< Workers try to stay this close to group
    float socialInteractionChance = 0.02f;  ///< Chance to seek nearby worker

    // Pathfinding
    float pathUpdateInterval = 0.5f;
    int maxPathfindAttempts = 3;
};

/**
 * @brief Group formation for selected workers
 */
struct WorkerFormation {
    enum class Type : uint8_t {
        None,
        Line,       ///< Workers in a line
        Box,        ///< Workers in a square/rectangle
        Circle,     ///< Workers in a circle
        Wedge       ///< V-formation
    };

    Type type = Type::Box;
    float spacing = 1.5f;
    glm::vec3 center{0.0f};
    float rotation = 0.0f;

    /**
     * @brief Get position for worker at index in formation
     * @param index Worker index
     * @param totalWorkers Total workers in formation
     * @return World position for this worker
     */
    glm::vec3 GetPositionForWorker(int index, int totalWorkers) const;
};

/**
 * @brief Command that can be issued to workers
 */
struct WorkerCommand {
    enum class Type : uint8_t {
        Move,           ///< Move to position
        Attack,         ///< Attack target (guards only)
        Gather,         ///< Gather resources at position
        Build,          ///< Build/repair at position
        Follow,         ///< Follow target entity
        Patrol,         ///< Patrol between points
        Hold,           ///< Stay in place
        Stop            ///< Cancel current action
    };

    Type type = Type::Move;
    glm::vec3 position{0.0f};
    Entity::EntityId targetEntity = Entity::INVALID_ID;
    uint32_t targetBuilding = 0;
    std::vector<glm::vec3> patrolPoints;
    bool queued = false;  ///< Add to queue instead of replacing
};

/**
 * @brief Worker AI management system
 *
 * Provides higher-level AI control for workers including:
 * - Behavior tree decisions
 * - Group coordination
 * - Formation movement
 * - Command processing
 * - Automatic task assignment
 */
class WorkerAI {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    WorkerAI();
    explicit WorkerAI(const WorkerAIConfig& config);
    ~WorkerAI();

    // Non-copyable
    WorkerAI(const WorkerAI&) = delete;
    WorkerAI& operator=(const WorkerAI&) = delete;

    // =========================================================================
    // Configuration
    // =========================================================================

    /** @brief Get configuration */
    [[nodiscard]] const WorkerAIConfig& GetConfig() const noexcept { return m_config; }

    /** @brief Set configuration */
    void SetConfig(const WorkerAIConfig& config) { m_config = config; }

    // =========================================================================
    // Core Update
    // =========================================================================

    /**
     * @brief Update AI for all workers in population
     * @param deltaTime Time since last update
     * @param population Population manager
     * @param entityManager Entity manager
     * @param navGraph Navigation graph
     * @param world World for collision/pathfinding
     */
    void Update(float deltaTime, Population& population, EntityManager& entityManager,
                Nova::Graph* navGraph, World* world);

    // =========================================================================
    // Commands
    // =========================================================================

    /**
     * @brief Issue command to specific worker
     * @param worker Worker to command
     * @param command Command to execute
     * @param navGraph Navigation graph for pathfinding
     */
    void IssueCommand(Worker* worker, const WorkerCommand& command, Nova::Graph* navGraph);

    /**
     * @brief Issue command to multiple workers
     * @param workers Workers to command
     * @param command Command to execute
     * @param navGraph Navigation graph for pathfinding
     */
    void IssueGroupCommand(const std::vector<Worker*>& workers, const WorkerCommand& command,
                          Nova::Graph* navGraph);

    /**
     * @brief Cancel all pending commands for worker
     * @param worker Worker to clear commands for
     */
    void CancelCommands(Worker* worker);

    /**
     * @brief Get pending commands for worker
     * @param worker Worker to query
     */
    [[nodiscard]] std::vector<WorkerCommand> GetPendingCommands(Worker* worker) const;

    // =========================================================================
    // Formation
    // =========================================================================

    /**
     * @brief Set formation type for selected workers
     * @param type Formation type
     */
    void SetFormation(WorkerFormation::Type type) { m_currentFormation.type = type; }

    /**
     * @brief Get current formation
     */
    [[nodiscard]] const WorkerFormation& GetFormation() const noexcept { return m_currentFormation; }

    /**
     * @brief Move workers in formation to position
     * @param workers Workers to move
     * @param position Target center position
     * @param navGraph Navigation graph
     */
    void MoveInFormation(const std::vector<Worker*>& workers, const glm::vec3& position,
                        Nova::Graph* navGraph);

    // =========================================================================
    // Group Behavior
    // =========================================================================

    /**
     * @brief Create a worker group
     * @param workers Workers to group
     * @return Group ID
     */
    uint32_t CreateGroup(const std::vector<Worker*>& workers);

    /**
     * @brief Disband a group
     * @param groupId Group to disband
     */
    void DisbandGroup(uint32_t groupId);

    /**
     * @brief Add worker to group
     * @param worker Worker to add
     * @param groupId Group to add to
     */
    void AddToGroup(Worker* worker, uint32_t groupId);

    /**
     * @brief Remove worker from their group
     * @param worker Worker to remove
     */
    void RemoveFromGroup(Worker* worker);

    /**
     * @brief Get workers in a group
     * @param groupId Group ID
     */
    [[nodiscard]] std::vector<Worker*> GetGroupMembers(uint32_t groupId) const;

    /**
     * @brief Issue command to entire group
     * @param groupId Group to command
     * @param command Command to execute
     * @param navGraph Navigation graph
     */
    void IssueGroupCommandById(uint32_t groupId, const WorkerCommand& command, Nova::Graph* navGraph);

    // =========================================================================
    // Automatic Behavior
    // =========================================================================

    /**
     * @brief Enable/disable automatic job-seeking for idle workers
     */
    void SetAutoAssignJobs(bool enable) { m_autoAssignJobs = enable; }

    /**
     * @brief Check if auto job assignment is enabled
     */
    [[nodiscard]] bool IsAutoAssignJobsEnabled() const noexcept { return m_autoAssignJobs; }

    /**
     * @brief Enable/disable automatic rest-seeking
     */
    void SetAutoSeekRest(bool enable) { m_autoSeekRest = enable; }

    /**
     * @brief Enable/disable automatic food-seeking
     */
    void SetAutoSeekFood(bool enable) { m_autoSeekFood = enable; }

    // =========================================================================
    // Hero Following
    // =========================================================================

    /**
     * @brief Set workers to follow the hero
     * @param workers Workers to assign
     * @param follow true to follow, false to stop
     */
    void SetFollowHero(const std::vector<Worker*>& workers, bool follow);

    /**
     * @brief Update hero following behavior
     * @param population Population for getting followers
     * @param player Player entity
     * @param navGraph Navigation graph
     */
    void UpdateHeroFollowing(Population& population, Player* player, Nova::Graph* navGraph);

    // =========================================================================
    // Threat Response
    // =========================================================================

    /**
     * @brief Alert all workers of a threat at position
     * @param threatPosition Position of threat
     * @param population Population manager
     * @param entityManager Entity manager
     */
    void AlertWorkersOfThreat(const glm::vec3& threatPosition, Population& population,
                              EntityManager& entityManager);

    /**
     * @brief Get workers currently fleeing
     * @param population Population manager
     */
    [[nodiscard]] std::vector<Worker*> GetFleeingWorkers(Population& population) const;

    /**
     * @brief Designate rally point for fleeing workers
     * @param position Rally position
     */
    void SetRallyPoint(const glm::vec3& position) { m_rallyPoint = position; m_hasRallyPoint = true; }

    /**
     * @brief Clear rally point
     */
    void ClearRallyPoint() { m_hasRallyPoint = false; }

    // =========================================================================
    // Work Scheduling
    // =========================================================================

    /**
     * @brief Set current time of day (0-24)
     * @param hour Current hour
     */
    void SetTimeOfDay(float hour) { m_currentHour = hour; }

    /**
     * @brief Check if it's work hours
     */
    [[nodiscard]] bool IsWorkHours() const noexcept {
        return m_currentHour >= m_config.workStartTime && m_currentHour < m_config.workEndTime;
    }

    // =========================================================================
    // Decision Making
    // =========================================================================

    /**
     * @brief Evaluate AI decision for a single worker
     * @param worker Worker to evaluate
     * @param entityManager Entity manager for context
     * @param population Population for context
     * @return Decision result
     */
    [[nodiscard]] AIDecision EvaluateWorker(Worker* worker, EntityManager& entityManager,
                                            Population& population) const;

private:
    // Internal update methods
    void UpdateWorkerAI(Worker* worker, float deltaTime, EntityManager& entityManager,
                       Population& population, Nova::Graph* navGraph);
    void ProcessCommandQueue(Worker* worker, Nova::Graph* navGraph);
    void UpdateGroupCohesion(float deltaTime, Population& population, Nova::Graph* navGraph);
    void UpdateAutoAssignment(float deltaTime, Population& population);

    // Decision helpers
    bool ShouldFlee(Worker* worker, EntityManager& entityManager) const;
    bool ShouldSeekFood(Worker* worker) const;
    bool ShouldSeekRest(Worker* worker) const;
    bool ShouldWork(Worker* worker) const;

    // Pathfinding helpers
    bool FindPathToPosition(Worker* worker, const glm::vec3& target, Nova::Graph* navGraph);
    glm::vec3 FindSafePosition(const glm::vec3& from, const glm::vec3& threat, float distance) const;

    // Configuration
    WorkerAIConfig m_config;

    // Command queues per worker
    std::unordered_map<Entity::EntityId, std::queue<WorkerCommand>> m_commandQueues;

    // Worker groups
    struct WorkerGroup {
        uint32_t id = 0;
        std::vector<Entity::EntityId> memberIds;
    };
    std::unordered_map<uint32_t, WorkerGroup> m_groups;
    std::unordered_map<Entity::EntityId, uint32_t> m_workerToGroup;
    uint32_t m_nextGroupId = 1;

    // Formation
    WorkerFormation m_currentFormation;

    // Automatic behavior flags
    bool m_autoAssignJobs = true;
    bool m_autoSeekRest = true;
    bool m_autoSeekFood = true;

    // Rally point
    glm::vec3 m_rallyPoint{0.0f};
    bool m_hasRallyPoint = false;

    // Time
    float m_currentHour = 12.0f;

    // Timers
    float m_groupUpdateTimer = 0.0f;
    float m_autoAssignTimer = 0.0f;

    static constexpr float GROUP_UPDATE_INTERVAL = 1.0f;
    static constexpr float AUTO_ASSIGN_INTERVAL = 5.0f;
};

} // namespace Vehement
