#pragma once

/**
 * @file AIPlayer.hpp
 * @brief AI decision tree logic for controlling AI factions in Solo Play mode
 *
 * This system implements a hierarchical decision tree that allows AI players to:
 * - Manage resources and worker assignments
 * - Build and expand their base
 * - Train units and manage armies
 * - Attack, defend, and execute strategic behaviors
 * - Adapt to early/mid/late game phases
 */

#include "../Resource.hpp"
#include "../Production.hpp"
#include "../Construction.hpp"
#include "../Gathering.hpp"
#include "../Worker.hpp"
#include "../WorkerAI.hpp"
#include "../Faction.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <queue>

namespace Vehement {

// Forward declarations
class EntityManager;
class Population;
class World;
namespace Nova { class Graph; }

namespace RTS {

// ============================================================================
// AI Strategy States
// ============================================================================

/**
 * @brief Strategic phases of the game
 */
enum class StrategyPhase : uint8_t {
    EarlyGame,      ///< Focus on economy and basic defense (0-10 min)
    MidGame,        ///< Expansion, tech, and army building (10-25 min)
    LateGame,       ///< Large armies, advanced tech, aggressive (25+ min)
    Survival,       ///< Under heavy attack, defensive stance
    Domination      ///< Overwhelming advantage, push for victory
};

/**
 * @brief Get phase name
 */
inline const char* StrategyPhaseToString(StrategyPhase phase) {
    switch (phase) {
        case StrategyPhase::EarlyGame: return "Early Game";
        case StrategyPhase::MidGame: return "Mid Game";
        case StrategyPhase::LateGame: return "Late Game";
        case StrategyPhase::Survival: return "Survival";
        case StrategyPhase::Domination: return "Domination";
        default: return "Unknown";
    }
}

// ============================================================================
// AI Behaviors
// ============================================================================

/**
 * @brief Primary behavior modes for the AI
 */
enum class AIBehavior : uint8_t {
    Defensive,      ///< Focus on defense, turtling
    Balanced,       ///< Mix of economy, defense, and offense
    Aggressive,     ///< Early pressure, frequent attacks
    Economic,       ///< Heavy resource focus, fast expansion
    Rush,           ///< Quick attack with minimal units
    Turtle          ///< Extreme defense, slow build-up
};

/**
 * @brief Get behavior name
 */
inline const char* AIBehaviorToString(AIBehavior behavior) {
    switch (behavior) {
        case AIBehavior::Defensive: return "Defensive";
        case AIBehavior::Balanced: return "Balanced";
        case AIBehavior::Aggressive: return "Aggressive";
        case AIBehavior::Economic: return "Economic";
        case AIBehavior::Rush: return "Rush";
        case AIBehavior::Turtle: return "Turtle";
        default: return "Unknown";
    }
}

// ============================================================================
// Decision Tree Nodes
// ============================================================================

/**
 * @brief Priority levels for decisions
 */
enum class DecisionPriority : uint8_t {
    Critical = 0,   ///< Must execute immediately (defense, survival)
    High = 1,       ///< Important tasks (worker production, key buildings)
    Medium = 2,     ///< Normal operations (resource gathering, unit training)
    Low = 3,        ///< Nice to have (upgrades, optimization)
    Idle = 4        ///< Filler tasks when nothing else to do
};

/**
 * @brief Types of decisions the AI can make
 */
enum class DecisionType : uint8_t {
    // Economy
    AssignWorkerToGather,
    AssignWorkerToBuild,
    TrainWorker,
    BuildEconomyBuilding,
    ExpandToNewLocation,

    // Production
    BuildProductionBuilding,
    QueueUnitProduction,
    UpgradeBuilding,
    ResearchTechnology,

    // Military
    BuildMilitaryBuilding,
    TrainMilitaryUnit,
    FormAttackGroup,
    SendAttackGroup,
    DefendBase,
    Scout,

    // Construction
    PlaceBuilding,
    AssignBuilders,
    ExpandBase,
    BuildDefenses,

    // Resource Management
    BalanceWorkers,
    OptimizeProduction,
    ManageUpkeep,

    COUNT
};

/**
 * @brief A decision node in the AI decision tree
 */
struct AIDecision {
    DecisionType type;
    DecisionPriority priority;
    float urgency = 0.0f;  ///< 0-1 how urgent this decision is
    std::string reason;    ///< Debug description

    // Decision parameters (varies by type)
    glm::vec2 position{0.0f};
    uint32_t targetId = 0;
    int resourceType = 0;
    int unitType = 0;
    int buildingType = 0;
    int count = 1;

    // Execution state
    bool executed = false;
    float timeQueued = 0.0f;

    /**
     * @brief Get priority score (lower is higher priority)
     */
    [[nodiscard]] float GetPriorityScore() const {
        return static_cast<float>(priority) + (1.0f - urgency);
    }

    /**
     * @brief Compare for priority queue
     */
    bool operator<(const AIDecision& other) const {
        return GetPriorityScore() > other.GetPriorityScore();  // Reverse for priority queue
    }
};

// ============================================================================
// AI State Tracking
// ============================================================================

/**
 * @brief Tracks the AI's current state and resources
 */
struct AIState {
    // Game phase
    StrategyPhase phase = StrategyPhase::EarlyGame;
    AIBehavior behavior = AIBehavior::Balanced;
    float gameTime = 0.0f;

    // Economy
    int workerCount = 0;
    int idleWorkerCount = 0;
    int gatherersOnWood = 0;
    int gatherersOnStone = 0;
    int gatherersOnMetal = 0;
    int gatherersOnFood = 0;

    // Resources
    int wood = 0;
    int stone = 0;
    int metal = 0;
    int food = 0;
    int coins = 0;

    // Resource rates (per second)
    float woodRate = 0.0f;
    float stoneRate = 0.0f;
    float metalRate = 0.0f;
    float foodRate = 0.0f;

    // Buildings
    int housingBuildings = 0;
    int productionBuildings = 0;
    int militaryBuildings = 0;
    int defenseBuildings = 0;
    int totalBuildings = 0;

    // Military
    int militaryUnits = 0;
    int armyStrength = 0;  ///< Sum of unit values
    int defenseStrength = 0;

    // Enemy intel
    glm::vec2 enemyBaseLocation{0.0f};
    int enemyArmyStrength = 0;
    int enemyBuildingCount = 0;
    bool enemyDetected = false;

    // Threats
    bool underAttack = false;
    float threatLevel = 0.0f;  ///< 0-1
    glm::vec2 attackLocation{0.0f};

    // Base locations
    glm::vec2 mainBaseLocation{0.0f};
    std::vector<glm::vec2> expansionLocations;

    /**
     * @brief Check if can afford a cost
     */
    [[nodiscard]] bool CanAfford(const ResourceCost& cost) const {
        return wood >= cost.GetAmount(ResourceType::Wood) &&
               stone >= cost.GetAmount(ResourceType::Stone) &&
               metal >= cost.GetAmount(ResourceType::Metal) &&
               food >= cost.GetAmount(ResourceType::Food);
    }

    /**
     * @brief Get resource deficit for a cost
     */
    [[nodiscard]] ResourceCost GetDeficit(const ResourceCost& cost) const {
        ResourceCost deficit;
        if (wood < cost.GetAmount(ResourceType::Wood))
            deficit.Add(ResourceType::Wood, cost.GetAmount(ResourceType::Wood) - wood);
        if (stone < cost.GetAmount(ResourceType::Stone))
            deficit.Add(ResourceType::Stone, cost.GetAmount(ResourceType::Stone) - stone);
        if (metal < cost.GetAmount(ResourceType::Metal))
            deficit.Add(ResourceType::Metal, cost.GetAmount(ResourceType::Metal) - metal);
        if (food < cost.GetAmount(ResourceType::Food))
            deficit.Add(ResourceType::Food, cost.GetAmount(ResourceType::Food) - food);
        return deficit;
    }
};

// ============================================================================
// AI Configuration
// ============================================================================

/**
 * @brief Configuration for AI behavior
 */
struct AIConfig {
    // Difficulty
    float difficulty = 1.0f;  ///< 0.5 = Easy, 1.0 = Normal, 1.5 = Hard

    // Economy targets
    int targetWorkers = 20;
    int maxWorkers = 50;
    float woodGatherRatio = 0.4f;
    float stoneGatherRatio = 0.3f;
    float metalGatherRatio = 0.2f;
    float foodGatherRatio = 0.1f;

    // Military targets
    int minMilitaryUnits = 5;
    int militaryPerWorker = 0.3f;  ///< Ratio of military to workers

    // Building ratios
    int housingPerWorker = 0.1f;
    int productionPerWorker = 0.05f;

    // Timing
    float earlyGameDuration = 600.0f;   ///< 10 minutes
    float midGameDuration = 900.0f;     ///< 15 minutes
    float decisionInterval = 1.0f;      ///< Time between decision evaluations
    float updateInterval = 0.5f;        ///< Time between state updates

    // Behavior weights
    float aggressionWeight = 0.5f;      ///< 0 = defensive, 1 = aggressive
    float expansionWeight = 0.5f;       ///< 0 = turtle, 1 = expand
    float economyWeight = 0.5f;         ///< 0 = military focus, 1 = economy focus

    // APM limits (for realism)
    int maxActionsPerMinute = 120;      ///< Human-like APM limit
    float actionDelay = 0.5f;           ///< Delay between actions (seconds)
};

// ============================================================================
// AI Player Class
// ============================================================================

/**
 * @brief AI controller for a computer player
 *
 * Uses a decision tree system to make strategic and tactical decisions:
 * 1. State Evaluation: Analyze current game state
 * 2. Decision Generation: Create possible decisions based on state
 * 3. Priority Sorting: Order decisions by priority and urgency
 * 4. Execution: Perform top-priority decisions within APM limits
 */
class AIPlayer {
public:
    AIPlayer();
    explicit AIPlayer(const std::string& playerName);
    ~AIPlayer();

    // Non-copyable
    AIPlayer(const AIPlayer&) = delete;
    AIPlayer& operator=(const AIPlayer&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the AI player
     */
    void Initialize(const AIConfig& config = AIConfig{});

    /**
     * @brief Set the race/faction the AI is playing
     */
    void SetRace(const std::string& race);

    /**
     * @brief Set AI behavior mode
     */
    void SetBehavior(AIBehavior behavior) { m_state.behavior = behavior; }

    /**
     * @brief Set main base location
     */
    void SetBaseLocation(const glm::vec2& location);

    // =========================================================================
    // Core Update
    // =========================================================================

    /**
     * @brief Main AI update - evaluates state and makes decisions
     * @param deltaTime Time since last update
     * @param population Worker population manager
     * @param entityManager Entity manager for units
     * @param resourceStock Resource stockpile
     * @param productionSystem Production building system
     * @param gatheringSystem Resource gathering system
     * @param navGraph Navigation graph for pathfinding
     * @param world World reference
     */
    void Update(
        float deltaTime,
        Population& population,
        EntityManager& entityManager,
        ResourceStock& resourceStock,
        ProductionSystem& productionSystem,
        GatheringSystem& gatheringSystem,
        Nova::Graph* navGraph,
        World* world
    );

    // =========================================================================
    // State Access
    // =========================================================================

    /**
     * @brief Get current AI state
     */
    [[nodiscard]] const AIState& GetState() const { return m_state; }

    /**
     * @brief Get current strategy phase
     */
    [[nodiscard]] StrategyPhase GetPhase() const { return m_state.phase; }

    /**
     * @brief Get current behavior
     */
    [[nodiscard]] AIBehavior GetBehavior() const { return m_state.behavior; }

    /**
     * @brief Get player name
     */
    [[nodiscard]] const std::string& GetName() const { return m_playerName; }

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get configuration
     */
    [[nodiscard]] const AIConfig& GetConfig() const { return m_config; }

    /**
     * @brief Set configuration
     */
    void SetConfig(const AIConfig& config) { m_config = config; }

    /**
     * @brief Set difficulty level
     */
    void SetDifficulty(float difficulty) { m_config.difficulty = difficulty; }

    // =========================================================================
    // Callbacks for Events
    // =========================================================================

    /**
     * @brief Notify AI of an attack
     */
    void OnUnderAttack(const glm::vec2& location, float threatLevel);

    /**
     * @brief Notify AI that an enemy has been detected
     */
    void OnEnemyDetected(const glm::vec2& location, int armyStrength);

    /**
     * @brief Notify AI that a building was destroyed
     */
    void OnBuildingDestroyed(uint32_t buildingId);

    /**
     * @brief Notify AI that a unit was killed
     */
    void OnUnitKilled(uint32_t unitId);

    // =========================================================================
    // Debug / Visualization
    // =========================================================================

    /**
     * @brief Get pending decisions for debug display
     */
    [[nodiscard]] std::vector<AIDecision> GetPendingDecisions() const;

    /**
     * @brief Get decision tree as string for debugging
     */
    [[nodiscard]] std::string GetDecisionTreeDebug() const;

private:
    // -------------------------------------------------------------------------
    // Decision Tree Evaluation
    // -------------------------------------------------------------------------

    /**
     * @brief Update AI state from game systems
     */
    void UpdateState(
        Population& population,
        EntityManager& entityManager,
        ResourceStock& resourceStock,
        ProductionSystem& productionSystem,
        GatheringSystem& gatheringSystem
    );

    /**
     * @brief Update strategy phase based on game state
     */
    void UpdateStrategyPhase();

    /**
     * @brief Evaluate all possible decisions and add to queue
     */
    void EvaluateDecisions(
        Population& population,
        EntityManager& entityManager,
        ResourceStock& resourceStock,
        ProductionSystem& productionSystem,
        GatheringSystem& gatheringSystem,
        Nova::Graph* navGraph,
        World* world
    );

    /**
     * @brief Execute top-priority decisions within APM limits
     */
    void ExecuteDecisions(
        float deltaTime,
        Population& population,
        EntityManager& entityManager,
        ResourceStock& resourceStock,
        ProductionSystem& productionSystem,
        GatheringSystem& gatheringSystem,
        Nova::Graph* navGraph,
        World* world
    );

    // -------------------------------------------------------------------------
    // Economy Decisions
    // -------------------------------------------------------------------------

    void EvaluateEconomyDecisions(
        Population& population,
        ResourceStock& resourceStock,
        GatheringSystem& gatheringSystem
    );

    void EvaluateWorkerProduction(Population& population, ResourceStock& resourceStock);
    void EvaluateWorkerAssignment(Population& population, GatheringSystem& gatheringSystem);
    void EvaluateResourceBalance(GatheringSystem& gatheringSystem);

    // -------------------------------------------------------------------------
    // Production Decisions
    // -------------------------------------------------------------------------

    void EvaluateProductionDecisions(
        ProductionSystem& productionSystem,
        ResourceStock& resourceStock
    );

    void EvaluateBuildingConstruction(ProductionSystem& productionSystem, ResourceStock& resourceStock);
    void EvaluateUnitProduction(ProductionSystem& productionSystem, ResourceStock& resourceStock);
    void EvaluateUpgrades(ProductionSystem& productionSystem, ResourceStock& resourceStock);

    // -------------------------------------------------------------------------
    // Military Decisions
    // -------------------------------------------------------------------------

    void EvaluateMilitaryDecisions(
        EntityManager& entityManager,
        ProductionSystem& productionSystem,
        ResourceStock& resourceStock
    );

    void EvaluateMilitaryProduction(ProductionSystem& productionSystem, ResourceStock& resourceStock);
    void EvaluateAttackDecisions(EntityManager& entityManager);
    void EvaluateDefenseDecisions(EntityManager& entityManager, ProductionSystem& productionSystem);
    void EvaluateScoutingDecisions(EntityManager& entityManager);

    // -------------------------------------------------------------------------
    // Expansion Decisions
    // -------------------------------------------------------------------------

    void EvaluateExpansionDecisions(
        World* world,
        ProductionSystem& productionSystem,
        ResourceStock& resourceStock
    );

    // -------------------------------------------------------------------------
    // Decision Execution
    // -------------------------------------------------------------------------

    void ExecuteDecision(
        const AIDecision& decision,
        Population& population,
        EntityManager& entityManager,
        ResourceStock& resourceStock,
        ProductionSystem& productionSystem,
        GatheringSystem& gatheringSystem,
        Nova::Graph* navGraph,
        World* world
    );

    // Economy execution
    void ExecuteAssignWorkerToGather(const AIDecision& decision, Population& population, GatheringSystem& gatheringSystem);
    void ExecuteTrainWorker(const AIDecision& decision, Population& population, ResourceStock& resourceStock);
    void ExecuteBuildEconomyBuilding(const AIDecision& decision, ProductionSystem& productionSystem, ResourceStock& resourceStock);

    // Production execution
    void ExecuteBuildProductionBuilding(const AIDecision& decision, ProductionSystem& productionSystem, ResourceStock& resourceStock);
    void ExecuteQueueUnitProduction(const AIDecision& decision, ProductionSystem& productionSystem);
    void ExecuteUpgradeBuilding(const AIDecision& decision, ProductionSystem& productionSystem, ResourceStock& resourceStock);

    // Military execution
    void ExecuteTrainMilitaryUnit(const AIDecision& decision, EntityManager& entityManager, ResourceStock& resourceStock);
    void ExecuteSendAttackGroup(const AIDecision& decision, EntityManager& entityManager);
    void ExecuteDefendBase(const AIDecision& decision, EntityManager& entityManager);

    // -------------------------------------------------------------------------
    // Helper Functions
    // -------------------------------------------------------------------------

    /**
     * @brief Add a decision to the queue
     */
    void AddDecision(const AIDecision& decision);

    /**
     * @brief Calculate optimal worker distribution across resources
     */
    void CalculateOptimalWorkerDistribution(int& wood, int& stone, int& metal, int& food) const;

    /**
     * @brief Find best location for a new building
     */
    glm::vec2 FindBuildingPlacement(
        int buildingType,
        const glm::vec2& nearPosition,
        World* world
    ) const;

    /**
     * @brief Estimate time to afford a cost
     */
    float EstimateTimeToAfford(const ResourceCost& cost) const;

    // -------------------------------------------------------------------------
    // Member Variables
    // -------------------------------------------------------------------------

    std::string m_playerName;
    std::string m_race;

    AIConfig m_config;
    AIState m_state;

    // Decision queue (priority queue)
    std::priority_queue<AIDecision> m_decisionQueue;
    std::vector<AIDecision> m_executedDecisions;  ///< For debugging

    // Timers
    float m_decisionTimer = 0.0f;
    float m_updateTimer = 0.0f;
    float m_actionTimer = 0.0f;

    // APM tracking
    int m_actionsThisMinute = 0;
    float m_apmTimer = 0.0f;

    // Strategy tracking
    float m_timeSinceLastAttack = 0.0f;
    float m_timeSinceLastExpansion = 0.0f;
    int m_attackWaveCount = 0;

    // Building tracking (for decision making)
    std::unordered_map<int, int> m_buildingCounts;  ///< BuildingType -> count
    std::unordered_map<int, float> m_buildingTimers;  ///< BuildingType -> last build time

    bool m_initialized = false;
};

} // namespace RTS
} // namespace Vehement
