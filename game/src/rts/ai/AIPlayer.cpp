#include "AIPlayer.hpp"
#include "../../entities/EntityManager.hpp"
#include "../../world/World.hpp"
#include <engine/pathfinding/Graph.hpp>
#include <engine/math/Random.hpp>
#include <algorithm>
#include <sstream>
#include <cmath>

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

    // Update military counts from entity manager
    // Query entities that are considered military (Guards in this RTS system)
    m_state.militaryUnits = 0;
    m_state.armyStrength = 0;

    // Count workers with Guard job as military units
    const auto& workers = population.GetWorkers();
    for (const auto& worker : workers) {
        if (worker && worker->GetJob() == WorkerJob::Guard && worker->IsAlive()) {
            m_state.militaryUnits++;
            // Calculate unit strength based on health and skills
            float unitStrength = worker->GetHealth() * worker->GetProductivity();
            m_state.armyStrength += static_cast<int>(unitStrength);
        }
    }

    // Also count NPCs that could be military entities (zombies are enemies, not ours)
    auto militaryEntities = entityManager.GetEntitiesByType(EntityType::NPC);
    for (Entity* entity : militaryEntities) {
        if (entity && entity->IsAlive()) {
            // NPCs could be allied military units
            // Add their health as army strength contribution
            m_state.armyStrength += static_cast<int>(entity->GetHealth());
        }
    }
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

        // Worker training cost (50 Food per worker)
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
        // Queue military unit training - convert workers to guards/military
        // Calculate urgency based on deficit
        int deficit = targetMilitary - m_state.militaryUnits;
        float urgency = std::min(1.0f, deficit / static_cast<float>(m_config.minMilitaryUnits));

        // Higher urgency if under attack or enemy detected
        if (m_state.underAttack) {
            urgency = std::min(1.0f, urgency + 0.3f);
        }

        // Need to have idle workers to train as military
        if (m_state.idleWorkerCount > 0 || m_state.workerCount > m_config.targetWorkers * 0.8f) {
            AIDecision decision;
            decision.type = DecisionType::TrainMilitaryUnit;
            decision.priority = m_state.underAttack ? DecisionPriority::Critical : DecisionPriority::Medium;
            decision.urgency = urgency;
            decision.reason = "Need more military units";
            decision.count = std::min(deficit, std::max(1, m_state.idleWorkerCount / 2));
            AddDecision(decision);
        }

        // Also consider building military buildings if we don't have enough
        int armoryCount = m_buildingCounts[static_cast<int>(ProductionBuildingType::Armory)];
        if (armoryCount < 1 && m_state.phase >= StrategyPhase::MidGame) {
            ResourceCost cost = GetBuildingCost(ProductionBuildingType::Armory);
            if (m_state.CanAfford(cost)) {
                AIDecision decision;
                decision.type = DecisionType::BuildMilitaryBuilding;
                decision.priority = DecisionPriority::Medium;
                decision.urgency = 0.6f;
                decision.reason = "Need armory for military production";
                decision.buildingType = static_cast<int>(ProductionBuildingType::Armory);
                AddDecision(decision);
            }
        }
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

        case DecisionType::BuildMilitaryBuilding:
            ExecuteBuildProductionBuilding(decision, productionSystem, resourceStock);
            break;

        case DecisionType::Scout:
            // Send a worker or military unit to scout unexplored areas
            {
                auto idleWorkers = population.GetIdleWorkers();
                if (!idleWorkers.empty()) {
                    Worker* scout = idleWorkers[0];
                    // Set scout to explore in a direction away from base
                    glm::vec2 scoutDirection = glm::normalize(
                        glm::vec2(Nova::Random::Range(-1.0f, 1.0f), Nova::Random::Range(-1.0f, 1.0f))
                    );
                    glm::vec3 scoutTarget = glm::vec3(
                        m_state.mainBaseLocation.x + scoutDirection.x * 50.0f,
                        0.0f,
                        m_state.mainBaseLocation.y + scoutDirection.y * 50.0f
                    );
                    scout->MoveTo(scoutTarget, navGraph);
                }
            }
            break;

        case DecisionType::ExpandToNewLocation:
            // Find and establish a new base location
            {
                glm::vec2 expansionLocation = m_state.mainBaseLocation + glm::vec2(
                    Nova::Random::Range(-30.0f, 30.0f),
                    Nova::Random::Range(-30.0f, 30.0f)
                );
                m_state.expansionLocations.push_back(expansionLocation);
                m_timeSinceLastExpansion = m_state.gameTime;
            }
            break;

        case DecisionType::BuildDefenses:
            // Build defensive structures near base
            ExecuteBuildProductionBuilding(decision, productionSystem, resourceStock);
            break;

        case DecisionType::BalanceWorkers:
            // Rebalance workers across resource types
            // This is handled by the worker assignment system
            break;

        case DecisionType::AssignWorkerToBuild:
            // Assign idle workers to construction tasks
            {
                auto idleWorkers = population.GetIdleWorkers();
                for (Worker* worker : idleWorkers) {
                    if (worker && worker->IsAvailable()) {
                        worker->SetJob(WorkerJob::Builder);
                        break;  // Assign one at a time
                    }
                }
            }
            break;

        default:
            // Unhandled decision type - log for debugging
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
            // Set worker job to Gatherer and assign to node
            worker->SetJob(WorkerJob::Gatherer);

            // Create a gatherer in the gathering system at worker's position
            Gatherer* gatherer = gatheringSystem.CreateGatherer(
                glm::vec2(worker->GetPosition().x, worker->GetPosition().z)
            );

            if (gatherer) {
                // Assign the gatherer to the resource node
                gatheringSystem.AssignGathererToNode(gatherer->id, node->id);

                // Set worker's workplace to the node position
                worker->SetWorkplacePosition(glm::vec3(node->position.x, 0.0f, node->position.y));

                // Create a task for the worker to move to the node
                WorkTask gatherTask;
                gatherTask.type = WorkTask::Type::Gather;
                gatherTask.targetPosition = glm::vec3(node->position.x, 0.0f, node->position.y);
                gatherTask.repeating = true;
                worker->AssignTask(gatherTask);
            }
        }
    }
}

void AIPlayer::ExecuteTrainWorker(
    const AIDecision& decision,
    Population& population,
    ResourceStock& resourceStock
) {
    // Training cost for a new worker
    ResourceCost cost;
    cost.Add(ResourceType::Food, 50);

    // Check if we have housing capacity before training
    if (population.GetAvailableHousing() <= 0) {
        // No housing available, can't train new workers
        return;
    }

    if (resourceStock.Spend(cost)) {
        // Create a new worker and add to population
        auto newWorker = std::make_unique<Worker>();

        // Set initial position near main base
        glm::vec3 spawnPos(
            m_state.mainBaseLocation.x + Nova::Random::Range(-5.0f, 5.0f),
            0.0f,
            m_state.mainBaseLocation.y + Nova::Random::Range(-5.0f, 5.0f)
        );
        newWorker->SetPosition(spawnPos);

        // Generate a name for the worker
        newWorker->SetWorkerName("Worker " + std::to_string(population.GetTotalPopulation() + 1));

        // Add to population system
        population.AddWorker(std::move(newWorker));

        // Try to find and assign housing for the new worker
        Worker* worker = population.GetWorker(population.GetTotalPopulation());
        if (worker) {
            population.FindAndAssignHousing(worker);
        }
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
    // Military unit training cost
    ResourceCost cost;
    cost.Add(ResourceType::Food, 75);
    cost.Add(ResourceType::Metal, 25);

    if (!resourceStock.Spend(cost)) {
        return;  // Can't afford to train
    }

    // In this RTS system, military units are NPCs or workers with Guard job
    // We'll use the entity manager to track military units
    // For now, increment the military count - actual unit creation would be done
    // by converting workers to guards or spawning NPC military units
    m_state.militaryUnits++;

    // Calculate unit strength contribution (base 50 + random variance)
    int unitStrength = 50 + static_cast<int>(Nova::Random::Range(0.0f, 25.0f));
    m_state.armyStrength += unitStrength;
}

void AIPlayer::ExecuteSendAttackGroup(
    const AIDecision& decision,
    EntityManager& entityManager
) {
    // Group military units and send them to attack the target position
    glm::vec2 targetPos = decision.position;

    // If no specific target, use enemy base location
    if (targetPos.x == 0.0f && targetPos.y == 0.0f) {
        if (m_state.enemyDetected) {
            targetPos = m_state.enemyBaseLocation;
        } else {
            // No known enemy location - pick a random direction to attack
            float angle = Nova::Random::Range(0.0f, glm::two_pi<float>());
            targetPos = m_state.mainBaseLocation + glm::vec2(
                std::cos(angle) * 50.0f,
                std::sin(angle) * 50.0f
            );
        }
    }

    // Find all military entities (NPCs) near our base and send them to attack
    glm::vec3 basePos3D(m_state.mainBaseLocation.x, 0.0f, m_state.mainBaseLocation.y);
    auto nearbyEntities = entityManager.FindEntitiesInRadius(basePos3D, 30.0f, EntityType::NPC);

    // Calculate attack formation - spread units in a line
    int unitIndex = 0;
    for (Entity* entity : nearbyEntities) {
        if (entity && entity->IsAlive()) {
            // Calculate offset position for formation
            float offsetX = (unitIndex % 5 - 2) * 2.0f;
            float offsetZ = (unitIndex / 5) * 2.0f;

            glm::vec3 attackTarget(
                targetPos.x + offsetX,
                0.0f,
                targetPos.y + offsetZ
            );

            // Set entity velocity towards target
            glm::vec3 direction = glm::normalize(attackTarget - entity->GetPosition());
            entity->SetVelocity(direction * entity->GetMoveSpeed());
            entity->LookAt(attackTarget);

            unitIndex++;
        }
    }
}

void AIPlayer::ExecuteDefendBase(
    const AIDecision& decision,
    EntityManager& entityManager
) {
    // Rally all military units to defend the attack location
    glm::vec2 defendPos = decision.position;

    // If no specific location, defend the main base
    if (defendPos.x == 0.0f && defendPos.y == 0.0f) {
        defendPos = m_state.attackLocation;
        if (defendPos.x == 0.0f && defendPos.y == 0.0f) {
            defendPos = m_state.mainBaseLocation;
        }
    }

    // Find all our military units and rally them to defense point
    // Search a wider radius to gather all available units
    auto allEntities = entityManager.GetEntitiesByType(EntityType::NPC);

    int unitIndex = 0;
    for (Entity* entity : allEntities) {
        if (entity && entity->IsAlive()) {
            // Calculate defensive formation position (circle around defend point)
            float angle = (unitIndex / static_cast<float>(allEntities.size())) * glm::two_pi<float>();
            float radius = 5.0f + (unitIndex / 8) * 3.0f;  // Concentric circles

            glm::vec3 defensePosition(
                defendPos.x + std::cos(angle) * radius,
                0.0f,
                defendPos.y + std::sin(angle) * radius
            );

            // Move unit towards defense position
            glm::vec3 direction = glm::normalize(defensePosition - entity->GetPosition());
            entity->SetVelocity(direction * entity->GetMoveSpeed() * 1.2f);  // Move faster when defending
            entity->LookAt(glm::vec3(defendPos.x, 0.0f, defendPos.y));  // Face the attack direction

            unitIndex++;
        }
    }

    // Clear the under attack flag after rallying defense
    // (will be set again if attack continues)
    m_state.underAttack = false;
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
    // Building size estimation based on type
    float buildingSize = 3.0f;  // Default building footprint
    if (buildingType == static_cast<int>(ProductionBuildingType::Warehouse)) {
        buildingSize = 5.0f;  // Warehouses are larger
    } else if (buildingType == static_cast<int>(ProductionBuildingType::Farm)) {
        buildingSize = 4.0f;  // Farms need more space
    }

    // Try to find valid placement using spiral search pattern
    const int maxAttempts = 20;
    const float searchRadius = 15.0f;

    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        // Spiral outward from near position
        float angle = attempt * 0.5f;  // Golden angle approximation for good distribution
        float radius = 3.0f + (attempt / 4) * 2.0f;

        glm::vec2 candidatePos = nearPosition + glm::vec2(
            std::cos(angle) * radius,
            std::sin(angle) * radius
        );

        // Check if position is within base bounds (not too far from main base)
        float distFromBase = glm::length(candidatePos - m_state.mainBaseLocation);
        if (distFromBase > 50.0f) {
            continue;  // Too far from base
        }

        // Check if position doesn't overlap with existing buildings
        bool validPosition = true;
        for (const auto& [type, count] : m_buildingCounts) {
            if (count > 0) {
                // Simple collision check - assume buildings are spaced
                // In a real implementation, this would query the world grid
                float minSpacing = buildingSize + 2.0f;
                // Position is valid if it's not on top of main base
                if (glm::length(candidatePos - m_state.mainBaseLocation) < minSpacing) {
                    validPosition = false;
                    break;
                }
            }
        }

        if (validPosition) {
            // Add some randomness to prevent grid-like placement
            candidatePos += glm::vec2(
                Nova::Random::Range(-1.0f, 1.0f),
                Nova::Random::Range(-1.0f, 1.0f)
            );
            return candidatePos;
        }
    }

    // Fallback: random placement if no valid position found
    return nearPosition + glm::vec2(
        Nova::Random::Range(-searchRadius, searchRadius),
        Nova::Random::Range(-searchRadius, searchRadius)
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
    // Update building counts by searching for the building type
    // Since we track counts by type, we need to find which type this building was

    // Decrement total building count
    m_state.totalBuildings = std::max(0, m_state.totalBuildings - 1);

    // Try to identify building type from our tracking
    // In practice, the caller should provide the building type, but we can
    // iterate through our counts and decrement intelligently
    bool found = false;
    for (auto& [type, count] : m_buildingCounts) {
        if (count > 0) {
            // Determine category based on building type
            ProductionBuildingType buildingType = static_cast<ProductionBuildingType>(type);

            // Check if this building type matches what was likely destroyed
            // We prioritize decrementing counts for buildings we have
            if (!found) {
                count--;
                found = true;

                // Update category counts based on building type
                switch (buildingType) {
                    case ProductionBuildingType::Farm:
                    case ProductionBuildingType::LumberMill:
                    case ProductionBuildingType::Quarry:
                    case ProductionBuildingType::Foundry:
                    case ProductionBuildingType::Workshop:
                    case ProductionBuildingType::Refinery:
                        m_state.productionBuildings = std::max(0, m_state.productionBuildings - 1);
                        break;

                    case ProductionBuildingType::Armory:
                        m_state.militaryBuildings = std::max(0, m_state.militaryBuildings - 1);
                        break;

                    case ProductionBuildingType::Warehouse:
                        // Warehouses don't fit neatly into categories
                        break;

                    case ProductionBuildingType::Hospital:
                    case ProductionBuildingType::Mint:
                        m_state.productionBuildings = std::max(0, m_state.productionBuildings - 1);
                        break;

                    default:
                        break;
                }
                break;  // Found and decremented, exit loop
            }
        }
    }

    // If building was housing, update housing count
    m_state.housingBuildings = std::max(0, m_state.housingBuildings - 1);
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
