#include "AIPlayer.hpp"
#include "../../entities/EntityManager.hpp"
#include "../../world/World.hpp"
#include <engine/pathfinding/Graph.hpp>
#include <engine/math/Random.hpp>
#include <algorithm>
#include <sstream>

namespace Vehement {
namespace RTS {

// ============================================================================
// Construction
// ============================================================================

AIPlayer::AIPlayer()
    : m_playerName("AI Player")
    , m_race("Humans") {
}

AIPlayer::AIPlayer(const std::string& playerName)
    : m_playerName(playerName)
    , m_race("Humans") {
}

AIPlayer::~AIPlayer() = default;

// ============================================================================
// Initialization
// ============================================================================

void AIPlayer::Initialize(const AIConfig& config) {
    m_config = config;
    m_initialized = true;
    m_state = AIState{};
    m_state.phase = StrategyPhase::EarlyGame;
    m_state.behavior = AIBehavior::Balanced;
}

void AIPlayer::SetRace(const std::string& race) {
    m_race = race;
}

void AIPlayer::SetBaseLocation(const glm::vec2& location) {
    m_state.mainBaseLocation = location;
}

// ============================================================================
// Core Update
// ============================================================================

void AIPlayer::Update(
    float deltaTime,
    Population& population,
    EntityManager& entityManager,
    ResourceStock& resourceStock,
    ProductionSystem& productionSystem,
    GatheringSystem& gatheringSystem,
    Nova::Graph* navGraph,
    World* world
) {
    if (!m_initialized) return;

    // Update game time
    m_state.gameTime += deltaTime;

    // Update timers
    m_updateTimer -= deltaTime;
    m_decisionTimer -= deltaTime;
    m_actionTimer -= deltaTime;
    m_apmTimer += deltaTime;

    // Reset APM counter every minute
    if (m_apmTimer >= 60.0f) {
        m_actionsThisMinute = 0;
        m_apmTimer = 0.0f;
    }

    // Update AI state periodically
    if (m_updateTimer <= 0.0f) {
        UpdateState(population, entityManager, resourceStock, productionSystem, gatheringSystem);
        UpdateStrategyPhase();
        m_updateTimer = m_config.updateInterval;
    }

    // Evaluate decisions periodically
    if (m_decisionTimer <= 0.0f) {
        EvaluateDecisions(population, entityManager, resourceStock, productionSystem, gatheringSystem, navGraph, world);
        m_decisionTimer = m_config.decisionInterval;
    }

    // Execute decisions within APM limits
    if (m_actionTimer <= 0.0f && m_actionsThisMinute < m_config.maxActionsPerMinute) {
        ExecuteDecisions(deltaTime, population, entityManager, resourceStock, productionSystem, gatheringSystem, navGraph, world);
        m_actionTimer = m_config.actionDelay;
    }
}

// ============================================================================
// State Updates
// ============================================================================

void AIPlayer::UpdateState(
    Population& population,
    EntityManager& entityManager,
    ResourceStock& resourceStock,
    ProductionSystem& productionSystem,
    GatheringSystem& gatheringSystem
) {
    // Update worker counts
    m_state.workerCount = static_cast<int>(population.GetWorkers().size());
    m_state.idleWorkerCount = static_cast<int>(population.GetIdleWorkers().size());

    // Update resource counts
    m_state.wood = resourceStock.GetAmount(ResourceType::Wood);
    m_state.stone = resourceStock.GetAmount(ResourceType::Stone);
    m_state.metal = resourceStock.GetAmount(ResourceType::Metal);
    m_state.food = resourceStock.GetAmount(ResourceType::Food);
    m_state.coins = resourceStock.GetAmount(ResourceType::Coins);

    // Update resource rates
    m_state.woodRate = gatheringSystem.GetCurrentGatherRate(ResourceType::Wood);
    m_state.stoneRate = gatheringSystem.GetCurrentGatherRate(ResourceType::Stone);
    m_state.metalRate = gatheringSystem.GetCurrentGatherRate(ResourceType::Metal);
    m_state.foodRate = gatheringSystem.GetCurrentGatherRate(ResourceType::Food);

    // Update building counts
    const auto& buildings = productionSystem.GetBuildings();
    m_state.totalBuildings = static_cast<int>(buildings.size());
    m_state.productionBuildings = 0;
    m_state.militaryBuildings = 0;
    m_state.defenseBuildings = 0;

    for (const auto& building : buildings) {
        // Count building types (simplified - you'd check actual building types)
        m_buildingCounts[static_cast<int>(building.type)]++;
    }

    // Update gatherer distribution
    const auto& gatherers = gatheringSystem.GetGatherers();
    m_state.gatherersOnWood = 0;
    m_state.gatherersOnStone = 0;
    m_state.gatherersOnMetal = 0;
    m_state.gatherersOnFood = 0;

    for (const auto& gatherer : gatherers) {
        if (gatherer.state == GathererState::Gathering || gatherer.state == GathererState::MovingToNode) {
            switch (gatherer.carryingType) {
                case ResourceType::Wood: m_state.gatherersOnWood++; break;
                case ResourceType::Stone: m_state.gatherersOnStone++; break;
                case ResourceType::Metal: m_state.gatherersOnMetal++; break;
                case ResourceType::Food: m_state.gatherersOnFood++; break;
                default: break;
            }
        }
    }

    // TODO: Update military counts from entity manager
    // This would involve querying military units, calculating army strength, etc.
}

void AIPlayer::UpdateStrategyPhase() {
    // Transition based on game time and state
    if (m_state.underAttack && m_state.threatLevel > 0.7f) {
        m_state.phase = StrategyPhase::Survival;
    } else if (m_state.armyStrength > m_state.enemyArmyStrength * 2 && m_state.enemyDetected) {
        m_state.phase = StrategyPhase::Domination;
    } else if (m_state.gameTime < m_config.earlyGameDuration) {
        m_state.phase = StrategyPhase::EarlyGame;
    } else if (m_state.gameTime < m_config.earlyGameDuration + m_config.midGameDuration) {
        m_state.phase = StrategyPhase::MidGame;
    } else {
        m_state.phase = StrategyPhase::LateGame;
    }
}

// ============================================================================
// Decision Evaluation
// ============================================================================

void AIPlayer::EvaluateDecisions(
    Population& population,
    EntityManager& entityManager,
    ResourceStock& resourceStock,
    ProductionSystem& productionSystem,
    GatheringSystem& gatheringSystem,
    Nova::Graph* navGraph,
    World* world
) {
    // Clear old executed decisions
    m_executedDecisions.clear();

    // Evaluate different categories of decisions
    EvaluateEconomyDecisions(population, resourceStock, gatheringSystem);
    EvaluateProductionDecisions(productionSystem, resourceStock);
    EvaluateMilitaryDecisions(entityManager, productionSystem, resourceStock);
    EvaluateExpansionDecisions(world, productionSystem, resourceStock);
}

// ============================================================================
// Economy Decision Evaluation
// ============================================================================

void AIPlayer::EvaluateEconomyDecisions(
    Population& population,
    ResourceStock& resourceStock,
    GatheringSystem& gatheringSystem
) {
    // Worker production
    EvaluateWorkerProduction(population, resourceStock);

    // Worker assignment
    EvaluateWorkerAssignment(population, gatheringSystem);

    // Resource balance
    EvaluateResourceBalance(gatheringSystem);
}

void AIPlayer::EvaluateWorkerProduction(Population& population, ResourceStock& resourceStock) {
    // Check if we need more workers
    if (m_state.workerCount < m_config.targetWorkers) {
        // Higher urgency if we're far below target
        float urgency = 1.0f - (static_cast<float>(m_state.workerCount) / m_config.targetWorkers);

        // TODO: Get actual worker training cost
        ResourceCost workerCost;
        workerCost.Add(ResourceType::Food, 50);

        if (m_state.CanAfford(workerCost)) {
            AIDecision decision;
            decision.type = DecisionType::TrainWorker;
            decision.priority = DecisionPriority::High;
            decision.urgency = urgency;
            decision.reason = "Need more workers for economy";
            decision.count = 1;
            AddDecision(decision);
        }
    }

    // Build housing if approaching population cap
    int housingCapacity = m_state.housingBuildings * 5;  // Assume 5 workers per housing
    if (m_state.workerCount >= housingCapacity * 0.8f) {
        AIDecision decision;
        decision.type = DecisionType::BuildEconomyBuilding;
        decision.priority = DecisionPriority::High;
        decision.urgency = 0.8f;
        decision.reason = "Need more housing capacity";
        decision.buildingType = static_cast<int>(ProductionBuildingType::Farm);  // Placeholder
        AddDecision(decision);
    }
}

void AIPlayer::EvaluateWorkerAssignment(Population& population, GatheringSystem& gatheringSystem) {
    // Calculate optimal distribution
    int targetWood, targetStone, targetMetal, targetFood;
    CalculateOptimalWorkerDistribution(targetWood, targetStone, targetMetal, targetFood);

    // Assign idle workers to resources with biggest deficit
    if (m_state.idleWorkerCount > 0) {
        struct ResourceDeficit {
            ResourceType type;
            int deficit;
            float priority;
        };

        std::vector<ResourceDeficit> deficits = {
            {ResourceType::Wood, targetWood - m_state.gatherersOnWood, m_config.woodGatherRatio},
            {ResourceType::Stone, targetStone - m_state.gatherersOnStone, m_config.stoneGatherRatio},
            {ResourceType::Metal, targetMetal - m_state.gatherersOnMetal, m_config.metalGatherRatio},
            {ResourceType::Food, targetFood - m_state.gatherersOnFood, m_config.foodGatherRatio}
        };

        // Sort by deficit * priority
        std::sort(deficits.begin(), deficits.end(),
            [](const ResourceDeficit& a, const ResourceDeficit& b) {
                return (a.deficit * a.priority) > (b.deficit * b.priority);
            });

        // Assign to resource with biggest weighted deficit
        if (deficits[0].deficit > 0) {
            AIDecision decision;
            decision.type = DecisionType::AssignWorkerToGather;
            decision.priority = DecisionPriority::Medium;
            decision.urgency = std::min(1.0f, deficits[0].deficit / 5.0f);
            decision.reason = "Assign idle worker to gathering";
            decision.resourceType = static_cast<int>(deficits[0].type);
            decision.count = std::min(m_state.idleWorkerCount, deficits[0].deficit);
            AddDecision(decision);
        }
    }
}

void AIPlayer::EvaluateResourceBalance(GatheringSystem& gatheringSystem) {
    // Check if resource rates are balanced properly
    float totalGatherRate = m_state.woodRate + m_state.stoneRate + m_state.metalRate + m_state.foodRate;
    if (totalGatherRate < 0.1f) return;  // No gathering happening

    // Calculate current ratios
    float woodRatio = m_state.woodRate / totalGatherRate;
    float stoneRatio = m_state.stoneRate / totalGatherRate;
    float metalRatio = m_state.metalRate / totalGatherRate;
    float foodRatio = m_state.foodRate / totalGatherRate;

    // Find biggest imbalance
    float woodImbalance = std::abs(woodRatio - m_config.woodGatherRatio);
    float stoneImbalance = std::abs(stoneRatio - m_config.stoneGatherRatio);
    float metalImbalance = std::abs(metalRatio - m_config.metalGatherRatio);
    float foodImbalance = std::abs(foodRatio - m_config.foodGatherRatio);

    // If imbalance is significant, queue a rebalance decision
    float maxImbalance = std::max({woodImbalance, stoneImbalance, metalImbalance, foodImbalance});
    if (maxImbalance > 0.15f) {
        AIDecision decision;
        decision.type = DecisionType::BalanceWorkers;
        decision.priority = DecisionPriority::Low;
        decision.urgency = maxImbalance;
        decision.reason = "Rebalance gatherer distribution";
        AddDecision(decision);
    }
}

// ============================================================================
// Production Decision Evaluation
// ============================================================================

void AIPlayer::EvaluateProductionDecisions(
    ProductionSystem& productionSystem,
    ResourceStock& resourceStock
) {
    EvaluateBuildingConstruction(productionSystem, resourceStock);
    EvaluateUnitProduction(productionSystem, resourceStock);
    EvaluateUpgrades(productionSystem, resourceStock);
}

void AIPlayer::EvaluateBuildingConstruction(
    ProductionSystem& productionSystem,
    ResourceStock& resourceStock
) {
    // Early game: Build farms for food
    if (m_state.phase == StrategyPhase::EarlyGame) {
        int farmCount = m_buildingCounts[static_cast<int>(ProductionBuildingType::Farm)];
        if (farmCount < 2) {
            ResourceCost cost = GetBuildingCost(ProductionBuildingType::Farm);
            if (m_state.CanAfford(cost)) {
                AIDecision decision;
                decision.type = DecisionType::BuildProductionBuilding;
                decision.priority = DecisionPriority::High;
                decision.urgency = 0.7f;
                decision.reason = "Early game: need farms";
                decision.buildingType = static_cast<int>(ProductionBuildingType::Farm);
                AddDecision(decision);
            }
        }

        // Build lumber mill
        int lumberMillCount = m_buildingCounts[static_cast<int>(ProductionBuildingType::LumberMill)];
        if (lumberMillCount < 1 && m_state.workerCount >= 10) {
            ResourceCost cost = GetBuildingCost(ProductionBuildingType::LumberMill);
            if (m_state.CanAfford(cost)) {
                AIDecision decision;
                decision.type = DecisionType::BuildProductionBuilding;
                decision.priority = DecisionPriority::Medium;
                decision.urgency = 0.6f;
                decision.reason = "Need lumber mill for wood processing";
                decision.buildingType = static_cast<int>(ProductionBuildingType::LumberMill);
                AddDecision(decision);
            }
        }
    }

    // Mid game: Expand production
    if (m_state.phase == StrategyPhase::MidGame) {
        // More production buildings
        if (m_state.productionBuildings < m_state.workerCount * m_config.productionPerWorker) {
            // Decide which building to build based on resource needs
            ProductionBuildingType buildingType = ProductionBuildingType::Workshop;

            ResourceCost cost = GetBuildingCost(buildingType);
            if (m_state.CanAfford(cost)) {
                AIDecision decision;
                decision.type = DecisionType::BuildProductionBuilding;
                decision.priority = DecisionPriority::Medium;
                decision.urgency = 0.5f;
                decision.reason = "Mid game: expand production";
                decision.buildingType = static_cast<int>(buildingType);
                AddDecision(decision);
            }
        }

        // Build armory for military units
        int armoryCount = m_buildingCounts[static_cast<int>(ProductionBuildingType::Armory)];
        if (armoryCount < 1 && m_state.militaryUnits > 5) {
            ResourceCost cost = GetBuildingCost(ProductionBuildingType::Armory);
            if (m_state.CanAfford(cost)) {
                AIDecision decision;
                decision.type = DecisionType::BuildMilitaryBuilding;
                decision.priority = DecisionPriority::High;
                decision.urgency = 0.7f;
                decision.reason = "Need armory for military";
                decision.buildingType = static_cast<int>(ProductionBuildingType::Armory);
                AddDecision(decision);
            }
        }
    }
}

void AIPlayer::EvaluateUnitProduction(
    ProductionSystem& productionSystem,
    ResourceStock& resourceStock
) {
    // Check available production buildings
    const auto& buildings = productionSystem.GetBuildings();

    for (const auto& building : buildings) {
        if (!building.operational || building.paused) continue;
        if (building.productionQueue.size() >= 3) continue;  // Don't overfill queues

        // Get recipes for this building
        auto recipes = productionSystem.GetRecipesForBuilding(building.type);

        for (const auto* recipe : recipes) {
            if (!recipe || !recipe->unlocked) continue;

            // Skip if can't afford
            if (!recipe->CanProduce(resourceStock)) continue;

            // Decide if we want to produce this
            bool shouldProduce = false;
            float urgency = 0.3f;
            std::string reason;

            // Always produce food if low
            for (const auto& [type, amount] : recipe->outputs) {
                if (type == ResourceType::Food && m_state.food < 200) {
                    shouldProduce = true;
                    urgency = 1.0f - (m_state.food / 200.0f);
                    reason = "Low on food";
                    break;
                }

                // Produce ammunition if we have military units
                if (type == ResourceType::Ammunition && m_state.militaryUnits > 0) {
                    shouldProduce = true;
                    urgency = 0.5f;
                    reason = "Need ammunition for army";
                    break;
                }
            }

            if (shouldProduce) {
                AIDecision decision;
                decision.type = DecisionType::QueueUnitProduction;
                decision.priority = DecisionPriority::Medium;
                decision.urgency = urgency;
                decision.reason = reason;
                decision.targetId = building.id;
                decision.unitType = recipe->id;
                AddDecision(decision);
            }
        }
    }
}

void AIPlayer::EvaluateUpgrades(
    ProductionSystem& productionSystem,
    ResourceStock& resourceStock
) {
    // Mid/late game: Upgrade important buildings
    if (m_state.phase < StrategyPhase::MidGame) return;

    const auto& buildings = productionSystem.GetBuildings();
    for (const auto& building : buildings) {
        if (!building.CanUpgrade()) continue;

        ResourceCost upgradeCost = building.GetUpgradeCost();
        if (!m_state.CanAfford(upgradeCost)) continue;

        // Prioritize upgrading production buildings
        float urgency = 0.3f;
        if (building.type == ProductionBuildingType::Farm ||
            building.type == ProductionBuildingType::LumberMill) {
            urgency = 0.5f;
        }

        AIDecision decision;
        decision.type = DecisionType::UpgradeBuilding;
        decision.priority = DecisionPriority::Low;
        decision.urgency = urgency;
        decision.reason = "Upgrade building for efficiency";
        decision.targetId = building.id;
        AddDecision(decision);
    }
}

// ============================================================================
// Military Decision Evaluation
// ============================================================================

void AIPlayer::EvaluateMilitaryDecisions(
    EntityManager& entityManager,
    ProductionSystem& productionSystem,
    ResourceStock& resourceStock
) {
    EvaluateMilitaryProduction(productionSystem, resourceStock);
    EvaluateDefenseDecisions(entityManager, productionSystem);
    EvaluateAttackDecisions(entityManager);
    EvaluateScoutingDecisions(entityManager);
}

void AIPlayer::EvaluateMilitaryProduction(
    ProductionSystem& productionSystem,
    ResourceStock& resourceStock
) {
    // Calculate target military size
    int targetMilitary = static_cast<int>(m_state.workerCount * m_config.militaryPerWorker);

    // Adjust based on behavior
    switch (m_state.behavior) {
        case AIBehavior::Aggressive:
        case AIBehavior::Rush:
            targetMilitary *= 1.5f;
            break;
        case AIBehavior::Defensive:
        case AIBehavior::Turtle:
            targetMilitary *= 0.8f;
            break;
        default:
            break;
    }

    // Ensure minimum military
    targetMilitary = std::max(targetMilitary, m_config.minMilitaryUnits);

    if (m_state.militaryUnits < targetMilitary) {
        // TODO: Queue military unit training
        // This would require integration with a military unit system
    }
}

void AIPlayer::EvaluateDefenseDecisions(
    EntityManager& entityManager,
    ProductionSystem& productionSystem
) {
    // If under attack, prioritize defense
    if (m_state.underAttack) {
        AIDecision decision;
        decision.type = DecisionType::DefendBase;
        decision.priority = DecisionPriority::Critical;
        decision.urgency = m_state.threatLevel;
        decision.reason = "Base under attack!";
        decision.position = m_state.attackLocation;
        AddDecision(decision);
    }

    // Build defensive structures based on phase
    if (m_state.phase >= StrategyPhase::MidGame) {
        // Check if we have enough defense buildings
        int targetDefenseBuildings = m_state.totalBuildings / 5;  // 20% of buildings should be defense
        if (m_state.defenseBuildings < targetDefenseBuildings) {
            AIDecision decision;
            decision.type = DecisionType::BuildDefenses;
            decision.priority = DecisionPriority::Medium;
            decision.urgency = 0.4f;
            decision.reason = "Need more defenses";
            AddDecision(decision);
        }
    }
}

void AIPlayer::EvaluateAttackDecisions(EntityManager& entityManager) {
    // Don't attack in early game (unless Rush behavior)
    if (m_state.phase == StrategyPhase::EarlyGame && m_state.behavior != AIBehavior::Rush) {
        return;
    }

    // Attack if we have military advantage
    if (m_state.militaryUnits >= m_config.minMilitaryUnits * 2) {
        float timeSinceLastAttack = m_state.gameTime - m_timeSinceLastAttack;

        // Attack periodically
        float attackInterval = 120.0f;  // 2 minutes
        if (m_state.behavior == AIBehavior::Aggressive) {
            attackInterval = 60.0f;  // 1 minute
        } else if (m_state.behavior == AIBehavior::Rush) {
            attackInterval = 30.0f;  // 30 seconds
        }

        if (timeSinceLastAttack >= attackInterval) {
            AIDecision decision;
            decision.type = DecisionType::SendAttackGroup;
            decision.priority = DecisionPriority::High;
            decision.urgency = 0.6f + (m_state.armyStrength / 1000.0f);  // More urgent with bigger army
            decision.reason = "Send attack wave";
            decision.position = m_state.enemyBaseLocation;
            AddDecision(decision);
        }
    }
}

void AIPlayer::EvaluateScoutingDecisions(EntityManager& entityManager) {
    // Scout early to find enemy
    if (!m_state.enemyDetected && m_state.phase == StrategyPhase::EarlyGame) {
        AIDecision decision;
        decision.type = DecisionType::Scout;
        decision.priority = DecisionPriority::Medium;
        decision.urgency = 0.5f;
        decision.reason = "Scout for enemy base";
        AddDecision(decision);
    }
}

// ============================================================================
// Expansion Decision Evaluation
// ============================================================================

void AIPlayer::EvaluateExpansionDecisions(
    World* world,
    ProductionSystem& productionSystem,
    ResourceStock& resourceStock
) {
    // Don't expand in early game (unless Economic behavior)
    if (m_state.phase == StrategyPhase::EarlyGame && m_state.behavior != AIBehavior::Economic) {
        return;
    }

    // Expand if we have excess resources
    bool hasExcessResources = m_state.wood > 500 && m_state.stone > 300;

    float timeSinceLastExpansion = m_state.gameTime - m_timeSinceLastExpansion;
    if (hasExcessResources && timeSinceLastExpansion > 300.0f) {  // 5 minutes
        AIDecision decision;
        decision.type = DecisionType::ExpandToNewLocation;
        decision.priority = DecisionPriority::Low;
        decision.urgency = 0.3f;
        decision.reason = "Expand to new location";
        AddDecision(decision);
    }
}

// ============================================================================
// Decision Execution
// ============================================================================

void AIPlayer::ExecuteDecisions(
    float deltaTime,
    Population& population,
    EntityManager& entityManager,
    ResourceStock& resourceStock,
    ProductionSystem& productionSystem,
    GatheringSystem& gatheringSystem,
    Nova::Graph* navGraph,
    World* world
) {
    // Execute top decision if available
    if (m_decisionQueue.empty()) return;

    AIDecision decision = m_decisionQueue.top();
    m_decisionQueue.pop();

    // Execute the decision
    ExecuteDecision(decision, population, entityManager, resourceStock, productionSystem, gatheringSystem, navGraph, world);

    // Track for debugging
    decision.executed = true;
    m_executedDecisions.push_back(decision);

    // Increment APM counter
    m_actionsThisMinute++;
}

void AIPlayer::ExecuteDecision(
    const AIDecision& decision,
    Population& population,
    EntityManager& entityManager,
    ResourceStock& resourceStock,
    ProductionSystem& productionSystem,
    GatheringSystem& gatheringSystem,
    Nova::Graph* navGraph,
    World* world
) {
    switch (decision.type) {
        case DecisionType::AssignWorkerToGather:
            ExecuteAssignWorkerToGather(decision, population, gatheringSystem);
            break;

        case DecisionType::TrainWorker:
            ExecuteTrainWorker(decision, population, resourceStock);
            break;

        case DecisionType::BuildEconomyBuilding:
        case DecisionType::BuildProductionBuilding:
            ExecuteBuildProductionBuilding(decision, productionSystem, resourceStock);
            break;

        case DecisionType::QueueUnitProduction:
            ExecuteQueueUnitProduction(decision, productionSystem);
            break;

        case DecisionType::UpgradeBuilding:
            ExecuteUpgradeBuilding(decision, productionSystem, resourceStock);
            break;

        case DecisionType::TrainMilitaryUnit:
            ExecuteTrainMilitaryUnit(decision, entityManager, resourceStock);
            break;

        case DecisionType::SendAttackGroup:
            ExecuteSendAttackGroup(decision, entityManager);
            m_timeSinceLastAttack = m_state.gameTime;
            m_attackWaveCount++;
            break;

        case DecisionType::DefendBase:
            ExecuteDefendBase(decision, entityManager);
            break;

        default:
            // TODO: Implement other decision types
            break;
    }
}

// ============================================================================
// Economy Execution
// ============================================================================

void AIPlayer::ExecuteAssignWorkerToGather(
    const AIDecision& decision,
    Population& population,
    GatheringSystem& gatheringSystem
) {
    auto idleWorkers = population.GetIdleWorkers();
    if (idleWorkers.empty()) return;

    ResourceType resourceType = static_cast<ResourceType>(decision.resourceType);

    // Find nearest node of this resource type
    for (Worker* worker : idleWorkers) {
        if (decision.count <= 0) break;

        ResourceNode* node = gatheringSystem.FindNearestNode(
            glm::vec2(worker->GetPosition().x, worker->GetPosition().z),
            resourceType
        );

        if (node && node->CanAssignGatherer()) {
            // Assign worker to this node
            // TODO: Create gatherer from worker and assign to node
            // This requires integration between Worker and Gatherer systems
        }
    }
}

void AIPlayer::ExecuteTrainWorker(
    const AIDecision& decision,
    Population& population,
    ResourceStock& resourceStock
) {
    // TODO: Train a new worker
    // This requires integration with worker training system
    ResourceCost cost;
    cost.Add(ResourceType::Food, 50);

    if (resourceStock.Spend(cost)) {
        // Worker will be created by the population system
        // population.TrainWorker();
    }
}

void AIPlayer::ExecuteBuildEconomyBuilding(
    const AIDecision& decision,
    ProductionSystem& productionSystem,
    ResourceStock& resourceStock
) {
    ProductionBuildingType buildingType = static_cast<ProductionBuildingType>(decision.buildingType);

    // Find placement near main base
    glm::vec2 placement = m_state.mainBaseLocation + glm::vec2(
        Nova::Random::Range(-10.0f, 10.0f),
        Nova::Random::Range(-10.0f, 10.0f)
    );

    ProductionBuilding* building = productionSystem.CreateBuilding(buildingType, placement, resourceStock);
    if (building) {
        m_buildingCounts[decision.buildingType]++;
    }
}

// ============================================================================
// Production Execution
// ============================================================================

void AIPlayer::ExecuteBuildProductionBuilding(
    const AIDecision& decision,
    ProductionSystem& productionSystem,
    ResourceStock& resourceStock
) {
    ExecuteBuildEconomyBuilding(decision, productionSystem, resourceStock);
}

void AIPlayer::ExecuteQueueUnitProduction(
    const AIDecision& decision,
    ProductionSystem& productionSystem
) {
    productionSystem.QueueProduction(decision.targetId, decision.unitType, -1);  // Infinite repeat
}

void AIPlayer::ExecuteUpgradeBuilding(
    const AIDecision& decision,
    ProductionSystem& productionSystem,
    ResourceStock& resourceStock
) {
    productionSystem.UpgradeBuilding(decision.targetId, resourceStock);
}

// ============================================================================
// Military Execution
// ============================================================================

void AIPlayer::ExecuteTrainMilitaryUnit(
    const AIDecision& decision,
    EntityManager& entityManager,
    ResourceStock& resourceStock
) {
    // TODO: Train military unit
}

void AIPlayer::ExecuteSendAttackGroup(
    const AIDecision& decision,
    EntityManager& entityManager
) {
    // TODO: Group military units and send to attack position
}

void AIPlayer::ExecuteDefendBase(
    const AIDecision& decision,
    EntityManager& entityManager
) {
    // TODO: Rally military units to defend attack location
}

// ============================================================================
// Helper Functions
// ============================================================================

void AIPlayer::AddDecision(const AIDecision& decision) {
    m_decisionQueue.push(decision);
}

void AIPlayer::CalculateOptimalWorkerDistribution(
    int& wood, int& stone, int& metal, int& food
) const {
    int totalWorkers = m_state.workerCount - m_state.idleWorkerCount;
    if (totalWorkers <= 0) {
        wood = stone = metal = food = 0;
        return;
    }

    wood = static_cast<int>(totalWorkers * m_config.woodGatherRatio);
    stone = static_cast<int>(totalWorkers * m_config.stoneGatherRatio);
    metal = static_cast<int>(totalWorkers * m_config.metalGatherRatio);
    food = static_cast<int>(totalWorkers * m_config.foodGatherRatio);

    // Ensure we use all workers
    int assigned = wood + stone + metal + food;
    int remaining = totalWorkers - assigned;
    wood += remaining;  // Add remainder to wood
}

glm::vec2 AIPlayer::FindBuildingPlacement(
    int buildingType,
    const glm::vec2& nearPosition,
    World* world
) const {
    // Simple random placement near position
    // TODO: Use world grid to find valid placement
    return nearPosition + glm::vec2(
        Nova::Random::Range(-15.0f, 15.0f),
        Nova::Random::Range(-15.0f, 15.0f)
    );
}

float AIPlayer::EstimateTimeToAfford(const ResourceCost& cost) const {
    ResourceCost deficit = m_state.GetDeficit(cost);

    float maxTime = 0.0f;

    if (deficit.GetAmount(ResourceType::Wood) > 0 && m_state.woodRate > 0) {
        maxTime = std::max(maxTime, deficit.GetAmount(ResourceType::Wood) / m_state.woodRate);
    }
    if (deficit.GetAmount(ResourceType::Stone) > 0 && m_state.stoneRate > 0) {
        maxTime = std::max(maxTime, deficit.GetAmount(ResourceType::Stone) / m_state.stoneRate);
    }
    if (deficit.GetAmount(ResourceType::Metal) > 0 && m_state.metalRate > 0) {
        maxTime = std::max(maxTime, deficit.GetAmount(ResourceType::Metal) / m_state.metalRate);
    }
    if (deficit.GetAmount(ResourceType::Food) > 0 && m_state.foodRate > 0) {
        maxTime = std::max(maxTime, deficit.GetAmount(ResourceType::Food) / m_state.foodRate);
    }

    return maxTime;
}

// ============================================================================
// Callbacks
// ============================================================================

void AIPlayer::OnUnderAttack(const glm::vec2& location, float threatLevel) {
    m_state.underAttack = true;
    m_state.attackLocation = location;
    m_state.threatLevel = threatLevel;
}

void AIPlayer::OnEnemyDetected(const glm::vec2& location, int armyStrength) {
    m_state.enemyDetected = true;
    m_state.enemyBaseLocation = location;
    m_state.enemyArmyStrength = armyStrength;
}

void AIPlayer::OnBuildingDestroyed(uint32_t buildingId) {
    // Update building counts
    // TODO: Track which building was destroyed and update counts
}

void AIPlayer::OnUnitKilled(uint32_t unitId) {
    // Update military counts
    m_state.militaryUnits = std::max(0, m_state.militaryUnits - 1);
}

// ============================================================================
// Debug
// ============================================================================

std::vector<AIDecision> AIPlayer::GetPendingDecisions() const {
    std::vector<AIDecision> decisions;
    std::priority_queue<AIDecision> queueCopy = m_decisionQueue;

    while (!queueCopy.empty()) {
        decisions.push_back(queueCopy.top());
        queueCopy.pop();
    }

    return decisions;
}

std::string AIPlayer::GetDecisionTreeDebug() const {
    std::ostringstream oss;

    oss << "=== AI Player: " << m_playerName << " ===\n";
    oss << "Race: " << m_race << "\n";
    oss << "Phase: " << StrategyPhaseToString(m_state.phase) << "\n";
    oss << "Behavior: " << AIBehaviorToString(m_state.behavior) << "\n\n";

    oss << "--- Economy ---\n";
    oss << "Workers: " << m_state.workerCount << " (Idle: " << m_state.idleWorkerCount << ")\n";
    oss << "Wood: " << m_state.wood << " (+" << m_state.woodRate << "/s, " << m_state.gatherersOnWood << " gatherers)\n";
    oss << "Stone: " << m_state.stone << " (+" << m_state.stoneRate << "/s, " << m_state.gatherersOnStone << " gatherers)\n";
    oss << "Metal: " << m_state.metal << " (+" << m_state.metalRate << "/s, " << m_state.gatherersOnMetal << " gatherers)\n";
    oss << "Food: " << m_state.food << " (+" << m_state.foodRate << "/s, " << m_state.gatherersOnFood << " gatherers)\n\n";

    oss << "--- Buildings ---\n";
    oss << "Total: " << m_state.totalBuildings << "\n";
    oss << "Production: " << m_state.productionBuildings << "\n";
    oss << "Military: " << m_state.militaryBuildings << "\n";
    oss << "Defense: " << m_state.defenseBuildings << "\n\n";

    oss << "--- Military ---\n";
    oss << "Units: " << m_state.militaryUnits << "\n";
    oss << "Army Strength: " << m_state.armyStrength << "\n";
    oss << "Under Attack: " << (m_state.underAttack ? "YES" : "no") << "\n\n";

    oss << "--- Decisions (Top 10) ---\n";
    auto decisions = GetPendingDecisions();
    for (size_t i = 0; i < std::min(size_t(10), decisions.size()); ++i) {
        const auto& d = decisions[i];
        oss << i + 1 << ". [P" << static_cast<int>(d.priority) << " U" << d.urgency << "] " << d.reason << "\n";
    }

    oss << "\n--- Recent Actions ---\n";
    for (size_t i = 0; i < std::min(size_t(5), m_executedDecisions.size()); ++i) {
        const auto& d = m_executedDecisions[m_executedDecisions.size() - 1 - i];
        oss << "- " << d.reason << "\n";
    }

    return oss.str();
}

} // namespace RTS
} // namespace Vehement
