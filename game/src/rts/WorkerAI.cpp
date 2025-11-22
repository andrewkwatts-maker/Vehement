#include "WorkerAI.hpp"
#include "../entities/EntityManager.hpp"
#include "../entities/Player.hpp"
#include "../entities/Zombie.hpp"
#include "../world/World.hpp"
#include <engine/pathfinding/Graph.hpp>
#include <engine/math/Random.hpp>
#include <algorithm>
#include <cmath>

namespace Vehement {

// ============================================================================
// Formation Implementation
// ============================================================================

glm::vec3 WorkerFormation::GetPositionForWorker(int index, int totalWorkers) const {
    if (totalWorkers <= 0) return center;

    glm::vec3 offset{0.0f};

    switch (type) {
        case Type::Line: {
            // Workers in a horizontal line
            float totalWidth = (totalWorkers - 1) * spacing;
            float startX = -totalWidth / 2.0f;
            offset.x = startX + index * spacing;
            break;
        }

        case Type::Box: {
            // Workers in a square/rectangle grid
            int gridSize = static_cast<int>(std::ceil(std::sqrt(totalWorkers)));
            int row = index / gridSize;
            int col = index % gridSize;
            float halfWidth = (gridSize - 1) * spacing / 2.0f;
            float halfHeight = ((totalWorkers - 1) / gridSize) * spacing / 2.0f;
            offset.x = col * spacing - halfWidth;
            offset.z = row * spacing - halfHeight;
            break;
        }

        case Type::Circle: {
            // Workers in a circle
            float angle = (2.0f * 3.14159f * index) / totalWorkers;
            float radius = (totalWorkers * spacing) / (2.0f * 3.14159f);
            radius = std::max(radius, spacing);
            offset.x = std::cos(angle) * radius;
            offset.z = std::sin(angle) * radius;
            break;
        }

        case Type::Wedge: {
            // V-formation
            int side = index % 2;  // 0 = left, 1 = right
            int depth = (index + 1) / 2;
            float sideOffset = side == 0 ? -1.0f : 1.0f;
            offset.x = sideOffset * depth * spacing * 0.7f;
            offset.z = -depth * spacing;
            break;
        }

        default:
            break;
    }

    // Apply rotation
    if (rotation != 0.0f) {
        float cosR = std::cos(rotation);
        float sinR = std::sin(rotation);
        float newX = offset.x * cosR - offset.z * sinR;
        float newZ = offset.x * sinR + offset.z * cosR;
        offset.x = newX;
        offset.z = newZ;
    }

    return center + offset;
}

// ============================================================================
// Construction
// ============================================================================

WorkerAI::WorkerAI() = default;

WorkerAI::WorkerAI(const WorkerAIConfig& config)
    : m_config(config) {
}

WorkerAI::~WorkerAI() = default;

// ============================================================================
// Core Update
// ============================================================================

void WorkerAI::Update(float deltaTime, Population& population, EntityManager& entityManager,
                      Nova::Graph* navGraph, World* /*world*/) {
    // Update timers
    m_groupUpdateTimer -= deltaTime;
    m_autoAssignTimer -= deltaTime;

    // Get all workers
    const auto& workers = population.GetWorkers();

    // Update each worker's AI
    for (const auto& workerPtr : workers) {
        if (workerPtr && !workerPtr->IsMarkedForRemoval()) {
            UpdateWorkerAI(workerPtr.get(), deltaTime, entityManager, population, navGraph);
        }
    }

    // Update group cohesion periodically
    if (m_groupUpdateTimer <= 0.0f) {
        UpdateGroupCohesion(deltaTime, population, navGraph);
        m_groupUpdateTimer = GROUP_UPDATE_INTERVAL;
    }

    // Update automatic job assignment periodically
    if (m_autoAssignJobs && m_autoAssignTimer <= 0.0f) {
        UpdateAutoAssignment(deltaTime, population);
        m_autoAssignTimer = AUTO_ASSIGN_INTERVAL;
    }
}

void WorkerAI::UpdateWorkerAI(Worker* worker, float deltaTime, EntityManager& entityManager,
                              Population& population, Nova::Graph* navGraph) {
    if (!worker) return;

    // Process command queue first
    ProcessCommandQueue(worker, navGraph);

    // Check survival behaviors (highest priority)
    if (ShouldFlee(worker, entityManager)) {
        // Worker's built-in flee behavior will handle this
        return;
    }

    // Check basic needs
    if (m_autoSeekFood && ShouldSeekFood(worker)) {
        // TODO: Find food source and eat
        // For now, workers will handle this through the needs system
    }

    if (m_autoSeekRest && ShouldSeekRest(worker)) {
        // If worker has a home and isn't already resting, go rest
        if (worker->HasHome() && worker->GetWorkerState() != WorkerState::Resting) {
            WorkTask restTask;
            restTask.type = WorkTask::Type::GoHome;
            restTask.targetPosition = worker->GetHomePosition();
            worker->AssignTask(restTask);
            worker->SetWorkerState(WorkerState::Moving);

            if (navGraph) {
                worker->RequestPath(worker->GetHomePosition(), *navGraph);
            }
        }
    }

    // Check work scheduling
    if (m_config.enforceWorkHours && !IsWorkHours()) {
        // Send workers home if not work hours
        if (worker->GetWorkerState() == WorkerState::Working) {
            worker->ClearTask();
            worker->SetWorkerState(WorkerState::Idle);
        }
    }
}

void WorkerAI::ProcessCommandQueue(Worker* worker, Nova::Graph* navGraph) {
    if (!worker) return;

    auto it = m_commandQueues.find(worker->GetId());
    if (it == m_commandQueues.end() || it->second.empty()) {
        return;
    }

    // Check if worker can accept new commands
    if (worker->GetWorkerState() == WorkerState::Dead ||
        worker->GetWorkerState() == WorkerState::Fleeing) {
        return;
    }

    // Check if current task is done
    if (worker->HasTask() && !worker->GetCurrentTask().IsComplete()) {
        return;  // Still executing current command
    }

    // Get next command
    WorkerCommand& cmd = it->second.front();

    switch (cmd.type) {
        case WorkerCommand::Type::Move: {
            WorkTask moveTask;
            moveTask.type = WorkTask::Type::None;
            moveTask.targetPosition = cmd.position;
            worker->AssignTask(moveTask);
            worker->SetWorkerState(WorkerState::Moving);

            if (navGraph) {
                worker->RequestPath(cmd.position, *navGraph);
            }
            break;
        }

        case WorkerCommand::Type::Follow: {
            worker->SetFollowingHero(true);
            break;
        }

        case WorkerCommand::Type::Stop: {
            worker->ClearTask();
            worker->ClearPath();
            worker->SetWorkerState(WorkerState::Idle);
            worker->SetFollowingHero(false);
            break;
        }

        case WorkerCommand::Type::Hold: {
            worker->ClearTask();
            worker->ClearPath();
            worker->SetWorkerState(WorkerState::Idle);
            worker->SetFollowingHero(false);
            // Worker will stay in place
            break;
        }

        case WorkerCommand::Type::Gather: {
            WorkTask gatherTask;
            gatherTask.type = WorkTask::Type::Gather;
            gatherTask.targetPosition = cmd.position;
            gatherTask.duration = 10.0f;
            gatherTask.repeating = true;
            worker->AssignTask(gatherTask);
            worker->SetWorkerState(WorkerState::Moving);

            if (navGraph) {
                worker->RequestPath(cmd.position, *navGraph);
            }
            break;
        }

        case WorkerCommand::Type::Build: {
            WorkTask buildTask;
            buildTask.type = WorkTask::Type::Build;
            buildTask.targetPosition = cmd.position;
            buildTask.targetBuilding = cmd.targetBuilding;
            buildTask.duration = 30.0f;
            worker->AssignTask(buildTask);
            worker->SetWorkerState(WorkerState::Moving);

            if (navGraph) {
                worker->RequestPath(cmd.position, *navGraph);
            }
            break;
        }

        case WorkerCommand::Type::Patrol: {
            if (!cmd.patrolPoints.empty()) {
                WorkTask patrolTask;
                patrolTask.type = WorkTask::Type::Patrol;
                patrolTask.targetPosition = cmd.patrolPoints[0];
                patrolTask.duration = 60.0f;
                patrolTask.repeating = true;
                worker->AssignTask(patrolTask);
                worker->SetWorkerState(WorkerState::Moving);

                if (navGraph) {
                    worker->RequestPath(cmd.patrolPoints[0], *navGraph);
                }
            }
            break;
        }

        case WorkerCommand::Type::Attack: {
            // Only guards can attack
            if (worker->GetJob() == WorkerJob::Guard) {
                // TODO: Implement attack behavior
            }
            break;
        }
    }

    // Remove processed command
    it->second.pop();
}

void WorkerAI::UpdateGroupCohesion(float /*deltaTime*/, Population& population, Nova::Graph* navGraph) {
    // For each group, ensure workers stay together
    for (auto& [groupId, group] : m_groups) {
        if (group.memberIds.empty()) continue;

        // Calculate group center
        glm::vec3 center{0.0f};
        int validCount = 0;

        for (Entity::EntityId id : group.memberIds) {
            Worker* worker = population.GetWorker(id);
            if (worker && !worker->IsMarkedForRemoval()) {
                center += worker->GetPosition();
                validCount++;
            }
        }

        if (validCount == 0) continue;
        center /= static_cast<float>(validCount);

        // Check if any worker is too far from center
        for (Entity::EntityId id : group.memberIds) {
            Worker* worker = population.GetWorker(id);
            if (!worker || worker->IsMarkedForRemoval()) continue;

            // Skip if worker is busy
            if (worker->GetWorkerState() != WorkerState::Idle) continue;

            float distToCenter = glm::distance(
                glm::vec2(worker->GetPosition().x, worker->GetPosition().z),
                glm::vec2(center.x, center.z)
            );

            if (distToCenter > m_config.groupCohesionRange) {
                // Move toward group center
                glm::vec3 targetPos = center;
                targetPos.x += Nova::Random::Range(-1.0f, 1.0f);
                targetPos.z += Nova::Random::Range(-1.0f, 1.0f);

                worker->MoveTo(targetPos, navGraph);
            }
        }
    }
}

void WorkerAI::UpdateAutoAssignment(float /*deltaTime*/, Population& population) {
    // Get idle workers
    auto idleWorkers = population.GetIdleWorkers();

    for (Worker* worker : idleWorkers) {
        if (!worker || worker->HasJob()) continue;

        // TODO: Find available jobs and assign
        // This would integrate with a building/job system
    }
}

// ============================================================================
// Commands
// ============================================================================

void WorkerAI::IssueCommand(Worker* worker, const WorkerCommand& command, Nova::Graph* navGraph) {
    if (!worker) return;

    Entity::EntityId id = worker->GetId();

    if (command.queued) {
        // Add to queue
        m_commandQueues[id].push(command);
    } else {
        // Clear queue and add this command
        std::queue<WorkerCommand> empty;
        m_commandQueues[id].swap(empty);
        m_commandQueues[id].push(command);

        // Process immediately
        ProcessCommandQueue(worker, navGraph);
    }
}

void WorkerAI::IssueGroupCommand(const std::vector<Worker*>& workers, const WorkerCommand& command,
                                 Nova::Graph* navGraph) {
    if (workers.empty()) return;

    // For move commands, use formation
    if (command.type == WorkerCommand::Type::Move) {
        MoveInFormation(workers, command.position, navGraph);
        return;
    }

    // For other commands, issue to each worker
    for (Worker* worker : workers) {
        IssueCommand(worker, command, navGraph);
    }
}

void WorkerAI::CancelCommands(Worker* worker) {
    if (!worker) return;

    auto it = m_commandQueues.find(worker->GetId());
    if (it != m_commandQueues.end()) {
        std::queue<WorkerCommand> empty;
        it->second.swap(empty);
    }

    // Also stop current activity
    worker->ClearTask();
    worker->ClearPath();
    worker->SetWorkerState(WorkerState::Idle);
}

std::vector<WorkerCommand> WorkerAI::GetPendingCommands(Worker* worker) const {
    std::vector<WorkerCommand> result;

    if (!worker) return result;

    auto it = m_commandQueues.find(worker->GetId());
    if (it == m_commandQueues.end()) return result;

    // Copy queue to vector (unfortunately requires copying the queue)
    std::queue<WorkerCommand> tempQueue = it->second;
    while (!tempQueue.empty()) {
        result.push_back(tempQueue.front());
        tempQueue.pop();
    }

    return result;
}

// ============================================================================
// Formation
// ============================================================================

void WorkerAI::MoveInFormation(const std::vector<Worker*>& workers, const glm::vec3& position,
                               Nova::Graph* navGraph) {
    if (workers.empty()) return;

    // Update formation center
    m_currentFormation.center = position;

    // Calculate formation positions
    int totalWorkers = static_cast<int>(workers.size());

    for (int i = 0; i < totalWorkers; ++i) {
        Worker* worker = workers[i];
        if (!worker) continue;

        glm::vec3 targetPos = m_currentFormation.GetPositionForWorker(i, totalWorkers);

        WorkerCommand moveCmd;
        moveCmd.type = WorkerCommand::Type::Move;
        moveCmd.position = targetPos;
        moveCmd.queued = false;

        IssueCommand(worker, moveCmd, navGraph);
    }
}

// ============================================================================
// Groups
// ============================================================================

uint32_t WorkerAI::CreateGroup(const std::vector<Worker*>& workers) {
    uint32_t groupId = m_nextGroupId++;

    WorkerGroup group;
    group.id = groupId;

    for (Worker* worker : workers) {
        if (!worker) continue;

        // Remove from existing group
        RemoveFromGroup(worker);

        group.memberIds.push_back(worker->GetId());
        m_workerToGroup[worker->GetId()] = groupId;
    }

    m_groups[groupId] = std::move(group);
    return groupId;
}

void WorkerAI::DisbandGroup(uint32_t groupId) {
    auto it = m_groups.find(groupId);
    if (it == m_groups.end()) return;

    // Remove all workers from group mapping
    for (Entity::EntityId id : it->second.memberIds) {
        m_workerToGroup.erase(id);
    }

    m_groups.erase(it);
}

void WorkerAI::AddToGroup(Worker* worker, uint32_t groupId) {
    if (!worker) return;

    auto it = m_groups.find(groupId);
    if (it == m_groups.end()) return;

    // Remove from existing group
    RemoveFromGroup(worker);

    it->second.memberIds.push_back(worker->GetId());
    m_workerToGroup[worker->GetId()] = groupId;
}

void WorkerAI::RemoveFromGroup(Worker* worker) {
    if (!worker) return;

    auto groupIt = m_workerToGroup.find(worker->GetId());
    if (groupIt == m_workerToGroup.end()) return;

    uint32_t groupId = groupIt->second;
    auto groupDataIt = m_groups.find(groupId);
    if (groupDataIt != m_groups.end()) {
        auto& members = groupDataIt->second.memberIds;
        members.erase(
            std::remove(members.begin(), members.end(), worker->GetId()),
            members.end()
        );
    }

    m_workerToGroup.erase(groupIt);
}

std::vector<Worker*> WorkerAI::GetGroupMembers(uint32_t groupId) const {
    std::vector<Worker*> result;

    // Note: This requires access to population to get Worker*, which we don't have here
    // In practice, this would need to be called with a Population reference

    return result;
}

void WorkerAI::IssueGroupCommandById(uint32_t groupId, const WorkerCommand& command,
                                      Nova::Graph* /*navGraph*/) {
    auto it = m_groups.find(groupId);
    if (it == m_groups.end()) return;

    // Store command for each member
    for (Entity::EntityId id : it->second.memberIds) {
        if (command.queued) {
            m_commandQueues[id].push(command);
        } else {
            std::queue<WorkerCommand> empty;
            m_commandQueues[id].swap(empty);
            m_commandQueues[id].push(command);
        }
    }
}

// ============================================================================
// Hero Following
// ============================================================================

void WorkerAI::SetFollowHero(const std::vector<Worker*>& workers, bool follow) {
    for (Worker* worker : workers) {
        if (worker) {
            worker->SetFollowingHero(follow);
        }
    }
}

void WorkerAI::UpdateHeroFollowing(Population& population, Player* player, Nova::Graph* navGraph) {
    if (!player) return;

    const auto& workers = population.GetWorkers();
    glm::vec3 heroPos = player->GetPosition();

    // Collect all following workers
    std::vector<Worker*> followers;
    for (const auto& workerPtr : workers) {
        if (workerPtr && !workerPtr->IsMarkedForRemoval() && workerPtr->IsFollowingHero()) {
            followers.push_back(workerPtr.get());
        }
    }

    if (followers.empty()) return;

    // Position followers in formation behind hero
    m_currentFormation.center = heroPos - player->GetForward() * 3.0f;
    m_currentFormation.rotation = player->GetRotation();

    int totalFollowers = static_cast<int>(followers.size());
    for (int i = 0; i < totalFollowers; ++i) {
        Worker* worker = followers[i];

        glm::vec3 targetPos = m_currentFormation.GetPositionForWorker(i, totalFollowers);

        float distToTarget = glm::distance(
            glm::vec2(worker->GetPosition().x, worker->GetPosition().z),
            glm::vec2(targetPos.x, targetPos.z)
        );

        // Only move if too far from formation position
        if (distToTarget > 2.0f) {
            if (worker->GetWorkerState() == WorkerState::Idle ||
                worker->GetCurrentTask().type == WorkTask::Type::FollowHero) {
                WorkTask followTask;
                followTask.type = WorkTask::Type::FollowHero;
                followTask.targetPosition = targetPos;
                followTask.targetEntity = player->GetId();
                worker->AssignTask(followTask);
                worker->SetWorkerState(WorkerState::Moving);

                if (navGraph) {
                    worker->RequestPath(targetPos, *navGraph);
                }
            }
        }
    }
}

// ============================================================================
// Threat Response
// ============================================================================

void WorkerAI::AlertWorkersOfThreat(const glm::vec3& threatPosition, Population& population,
                                    EntityManager& /*entityManager*/) {
    const auto& workers = population.GetWorkers();

    for (const auto& workerPtr : workers) {
        if (!workerPtr || workerPtr->IsMarkedForRemoval()) continue;

        Worker* worker = workerPtr.get();

        // Skip guards (they don't flee)
        if (worker->GetJob() == WorkerJob::Guard) continue;

        // Check if threat is within detection range
        float distToThreat = glm::distance(
            glm::vec2(worker->GetPosition().x, worker->GetPosition().z),
            glm::vec2(threatPosition.x, threatPosition.z)
        );

        if (distToThreat <= m_config.threatDetectionRange) {
            // Worker should flee
            // The worker's built-in AI will handle fleeing behavior
            // We can optionally set a rally point

            if (m_hasRallyPoint) {
                // Direct fleeing workers toward rally point
                WorkTask fleeTask;
                fleeTask.type = WorkTask::Type::GoHome;  // Reuse GoHome for fleeing
                fleeTask.targetPosition = m_rallyPoint;
                worker->AssignTask(fleeTask);
            }
        }
    }

    // Notify population of attack (for morale)
    population.RecordAttack();
}

std::vector<Worker*> WorkerAI::GetFleeingWorkers(Population& population) const {
    return population.GetWorkersByState(WorkerState::Fleeing);
}

// ============================================================================
// Decision Making
// ============================================================================

AIDecision WorkerAI::EvaluateWorker(Worker* worker, EntityManager& entityManager,
                                    Population& /*population*/) const {
    AIDecision decision;
    decision.newState = WorkerState::Idle;
    decision.urgency = 0.0f;
    decision.reason = "Default idle";

    if (!worker) return decision;

    // Priority 1: Survival (flee from threats)
    if (ShouldFlee(worker, entityManager)) {
        decision.newState = WorkerState::Fleeing;
        decision.urgency = 1.0f;
        decision.reason = "Threat detected - fleeing";

        // Find safe position
        Entity* threat = entityManager.GetNearestEntity(worker->GetPosition(), EntityType::Zombie);
        if (threat) {
            decision.targetPosition = FindSafePosition(worker->GetPosition(),
                                                       threat->GetPosition(),
                                                       m_config.fleeDistance);
        }
        return decision;
    }

    // Priority 2: Critical needs
    if (worker->GetNeeds().IsCriticallyInjured()) {
        decision.newState = WorkerState::Injured;
        decision.urgency = 0.9f;
        decision.reason = "Critically injured";
        return decision;
    }

    if (worker->GetNeeds().IsStarving()) {
        decision.urgency = 0.85f;
        decision.reason = "Starving - seek food";
        // TODO: Set target to food source
        return decision;
    }

    if (worker->GetNeeds().IsExhausted()) {
        if (worker->HasHome()) {
            decision.newState = WorkerState::Moving;
            decision.targetPosition = worker->GetHomePosition();
            decision.task.type = WorkTask::Type::GoHome;
            decision.task.targetPosition = worker->GetHomePosition();
            decision.urgency = 0.8f;
            decision.reason = "Exhausted - going home to rest";
        }
        return decision;
    }

    // Priority 3: Work assignment
    if (worker->HasJob() && worker->GetWorkplaceId() != 0) {
        if (!m_config.enforceWorkHours || IsWorkHours()) {
            decision.newState = WorkerState::Moving;
            decision.task.type = WorkTask::Type::GoToWork;
            // decision.targetPosition would be workplace position
            decision.urgency = 0.5f;
            decision.reason = "Going to work";
            return decision;
        }
    }

    // Priority 4: Moderate needs
    if (ShouldSeekRest(worker) && worker->HasHome()) {
        decision.newState = WorkerState::Moving;
        decision.targetPosition = worker->GetHomePosition();
        decision.task.type = WorkTask::Type::GoHome;
        decision.task.targetPosition = worker->GetHomePosition();
        decision.urgency = 0.4f;
        decision.reason = "Tired - seeking rest";
        return decision;
    }

    // Priority 5: Idle behavior
    decision.newState = WorkerState::Idle;
    decision.urgency = 0.0f;
    decision.reason = "No pressing needs";

    return decision;
}

// ============================================================================
// Decision Helpers
// ============================================================================

bool WorkerAI::ShouldFlee(Worker* worker, EntityManager& entityManager) const {
    if (!worker) return false;

    // Guards don't flee
    if (worker->GetJob() == WorkerJob::Guard) return false;

    // Check for nearby threats
    Entity* threat = entityManager.GetNearestEntity(worker->GetPosition(), EntityType::Zombie);
    if (!threat || !threat->IsAlive()) return false;

    float distToThreat = worker->DistanceTo(*threat);
    float fleeThreshold = worker->GetPersonality().GetFleeDistance(m_config.threatDetectionRange);

    return distToThreat <= fleeThreshold;
}

bool WorkerAI::ShouldSeekFood(Worker* worker) const {
    if (!worker) return false;
    return worker->GetNeeds().hunger <= m_config.seekFoodThreshold;
}

bool WorkerAI::ShouldSeekRest(Worker* worker) const {
    if (!worker) return false;
    return worker->GetNeeds().energy <= m_config.seekRestThreshold;
}

bool WorkerAI::ShouldWork(Worker* worker) const {
    if (!worker) return false;

    // Can't work if critical needs
    if (worker->GetNeeds().IsExhausted() || worker->GetNeeds().IsStarving()) {
        return false;
    }

    // Check work hours
    if (m_config.enforceWorkHours && !IsWorkHours()) {
        return false;
    }

    // Has job assignment
    return worker->HasJob() && worker->GetWorkplaceId() != 0;
}

// ============================================================================
// Pathfinding Helpers
// ============================================================================

bool WorkerAI::FindPathToPosition(Worker* worker, const glm::vec3& target, Nova::Graph* navGraph) {
    if (!worker || !navGraph) return false;
    return worker->RequestPath(target, *navGraph);
}

glm::vec3 WorkerAI::FindSafePosition(const glm::vec3& from, const glm::vec3& threat,
                                     float distance) const {
    // Calculate direction away from threat
    glm::vec3 awayDir = from - threat;
    awayDir.y = 0.0f;

    float len = glm::length(awayDir);
    if (len > 0.01f) {
        awayDir = glm::normalize(awayDir);
    } else {
        // Random direction if on top of threat
        glm::vec2 dir = Nova::Random::Direction2D();
        awayDir = glm::vec3(dir.x, 0.0f, dir.y);
    }

    // If there's a rally point, bias toward it
    if (m_hasRallyPoint) {
        glm::vec3 toRally = m_rallyPoint - from;
        toRally.y = 0.0f;
        if (glm::length(toRally) > 0.01f) {
            toRally = glm::normalize(toRally);
            awayDir = glm::normalize(awayDir + toRally * 0.5f);
        }
    }

    return from + awayDir * distance;
}

} // namespace Vehement
