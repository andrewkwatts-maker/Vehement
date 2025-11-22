#include "Worker.hpp"
#include "../entities/EntityManager.hpp"
#include "../entities/Player.hpp"
#include "../entities/Zombie.hpp"
#include <engine/pathfinding/Graph.hpp>
#include <engine/math/Random.hpp>
#include <engine/graphics/Renderer.hpp>
#include <algorithm>
#include <cmath>

namespace Vehement {

// ============================================================================
// Static name generation data
// ============================================================================

static const char* s_workerFirstNames[] = {
    "Alex", "Jordan", "Casey", "Riley", "Morgan", "Taylor", "Quinn", "Avery",
    "Sam", "Charlie", "Jamie", "Drew", "Pat", "Jesse", "Robin", "Kerry",
    "Dana", "Lee", "Kim", "Terry", "Chris", "Angel", "Blake", "Sydney",
    "Skyler", "Dakota", "Reese", "Cameron", "Finley", "Rowan"
};

static const char* s_workerLastNames[] = {
    "Smith", "Johnson", "Williams", "Brown", "Jones", "Garcia", "Miller",
    "Davis", "Rodriguez", "Martinez", "Anderson", "Taylor", "Thomas",
    "Moore", "Jackson", "Martin", "Lee", "Thompson", "White", "Harris"
};

static std::string GenerateRandomName() {
    int firstIdx = Nova::Random::Range(0, static_cast<int>(sizeof(s_workerFirstNames) / sizeof(char*)) - 1);
    int lastIdx = Nova::Random::Range(0, static_cast<int>(sizeof(s_workerLastNames) / sizeof(char*)) - 1);
    return std::string(s_workerFirstNames[firstIdx]) + " " + s_workerLastNames[lastIdx];
}

// ============================================================================
// Construction
// ============================================================================

Worker::Worker() : Entity(EntityType::NPC) {
    m_moveSpeed = DEFAULT_MOVE_SPEED;
    m_maxHealth = 75.0f;
    m_health = m_maxHealth;
    m_collisionRadius = 0.35f;
    m_name = "Worker";

    // Random appearance
    m_appearanceIndex = Nova::Random::Range(1, 9);
    m_texturePath = NPC::GetAppearanceTexturePath(m_appearanceIndex);

    // Generate random name
    m_workerName = GenerateRandomName();

    // Random personality
    m_personality = WorkerPersonality::GenerateRandom();

    // Random starting skills (some variation)
    m_skills.gathering = Nova::Random::Range(5.0f, 20.0f);
    m_skills.building = Nova::Random::Range(5.0f, 20.0f);
    m_skills.farming = Nova::Random::Range(5.0f, 20.0f);
    m_skills.combat = Nova::Random::Range(2.0f, 15.0f);
    m_skills.crafting = Nova::Random::Range(5.0f, 20.0f);
    m_skills.medical = Nova::Random::Range(2.0f, 10.0f);
    m_skills.scouting = Nova::Random::Range(5.0f, 20.0f);
    m_skills.trading = Nova::Random::Range(5.0f, 20.0f);

    // Starting loyalty with some variation
    m_loyalty = Nova::Random::Range(40.0f, 70.0f);
}

Worker::Worker(int appearanceIndex) : Worker() {
    SetAppearanceIndex(appearanceIndex);
}

Worker::Worker(const NPC& npc) : Entity(EntityType::NPC) {
    // Copy basic properties from NPC
    m_position = npc.GetPosition();
    m_rotation = npc.GetRotation();
    m_moveSpeed = DEFAULT_MOVE_SPEED;
    m_maxHealth = 75.0f;
    m_health = std::min(npc.GetHealth(), m_maxHealth);
    m_collisionRadius = 0.35f;
    m_name = "Worker";

    // Copy appearance
    m_appearanceIndex = npc.GetAppearanceIndex();
    m_texturePath = NPC::GetAppearanceTexturePath(m_appearanceIndex);

    // Generate identity
    m_workerName = GenerateRandomName();
    m_personality = WorkerPersonality::GenerateRandom();

    // Random starting skills
    m_skills.gathering = Nova::Random::Range(5.0f, 20.0f);
    m_skills.building = Nova::Random::Range(5.0f, 20.0f);
    m_skills.farming = Nova::Random::Range(5.0f, 20.0f);
    m_skills.combat = Nova::Random::Range(2.0f, 15.0f);
    m_skills.crafting = Nova::Random::Range(5.0f, 20.0f);
    m_skills.medical = Nova::Random::Range(2.0f, 10.0f);
    m_skills.scouting = Nova::Random::Range(5.0f, 20.0f);
    m_skills.trading = Nova::Random::Range(5.0f, 20.0f);

    // If NPC was infected, start with lower health/morale
    if (npc.IsInfected()) {
        m_needs.health = 30.0f;
        m_needs.morale = 40.0f;
    }

    m_loyalty = Nova::Random::Range(35.0f, 60.0f);  // Lower initial loyalty for recruits
}

// ============================================================================
// Core Update/Render
// ============================================================================

void Worker::Update(float deltaTime) {
    // Apply velocity
    m_position += m_velocity * deltaTime;

    // Keep on ground
    m_position.y = m_groundLevel;

    // Base entity update
    Entity::Update(deltaTime);

    // Check for death from starvation/exhaustion
    if (m_needs.IsDead() && m_workerState != WorkerState::Dead) {
        Die();
    }
}

void Worker::Render(Nova::Renderer& renderer) {
    // Could add visual indicators here:
    // - Selection highlight
    // - Job icon
    // - Health bar
    // - Status effects (hungry, tired, etc.)

    Entity::Render(renderer);
}

void Worker::UpdateAI(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph) {
    // Update timers
    m_pathUpdateTimer -= deltaTime;
    m_stateTimer += deltaTime;
    m_needsUpdateTimer -= deltaTime;

    // Update needs periodically (not every frame for performance)
    if (m_needsUpdateTimer <= 0.0f) {
        UpdateNeeds(NEEDS_UPDATE_INTERVAL);
        m_needsUpdateTimer = NEEDS_UPDATE_INTERVAL;

        // Check desertion
        if (CheckDesertion(NEEDS_UPDATE_INTERVAL)) {
            return;  // Worker deserted
        }
    }

    // Check if should transition to injured state
    if (m_needs.IsCriticallyInjured() && m_workerState != WorkerState::Injured &&
        m_workerState != WorkerState::Dead && m_workerState != WorkerState::Fleeing) {
        m_preFleeState = m_workerState;
        m_workerState = WorkerState::Injured;
    }

    // State machine
    switch (m_workerState) {
        case WorkerState::Idle:
            UpdateIdle(deltaTime, entityManager, navGraph);
            break;
        case WorkerState::Moving:
            UpdateMoving(deltaTime, entityManager, navGraph);
            break;
        case WorkerState::Working:
            UpdateWorking(deltaTime, entityManager);
            break;
        case WorkerState::Resting:
            UpdateResting(deltaTime, entityManager);
            break;
        case WorkerState::Fleeing:
            UpdateFleeing(deltaTime, entityManager, navGraph);
            break;
        case WorkerState::Injured:
            UpdateInjured(deltaTime, entityManager);
            break;
        case WorkerState::Dead:
            // No update for dead workers
            m_velocity = glm::vec3(0.0f);
            break;
    }
}

// ============================================================================
// State Machine Updates
// ============================================================================

void Worker::UpdateIdle(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph) {
    m_velocity = glm::vec3(0.0f);

    // Check for threats (unless Guard)
    if (m_job != WorkerJob::Guard) {
        Entity::EntityId threat = DetectThreat(entityManager);
        if (threat != Entity::INVALID_ID) {
            m_threatId = threat;
            m_preFleeState = WorkerState::Idle;
            m_workerState = WorkerState::Fleeing;
            m_fleeReassessTimer = 0.0f;
            return;
        }
    }

    // Check if needs are critical and should rest
    if (ShouldRest() && HasHome()) {
        // Go home to rest
        WorkTask goHome;
        goHome.type = WorkTask::Type::GoHome;
        goHome.targetPosition = m_homePosition;
        AssignTask(goHome);
        m_workerState = WorkerState::Moving;
        return;
    }

    // Check if following hero
    if (m_followingHero) {
        // Get hero position and follow
        if (Player* player = entityManager.GetPlayer()) {
            float distToHero = glm::distance(
                glm::vec2(m_position.x, m_position.z),
                glm::vec2(player->GetPosition().x, player->GetPosition().z)
            );

            if (distToHero > 3.0f) {  // Follow at distance
                WorkTask follow;
                follow.type = WorkTask::Type::FollowHero;
                follow.targetPosition = player->GetPosition();
                follow.targetEntity = player->GetId();
                AssignTask(follow);
                m_workerState = WorkerState::Moving;
            }
        }
        return;
    }

    // If has a job and workplace, go to work
    if (HasJob() && m_workplaceId != 0 && !ShouldRest()) {
        WorkTask goToWork;
        goToWork.type = WorkTask::Type::GoToWork;
        goToWork.targetPosition = m_workplacePosition;
        goToWork.targetBuilding = m_workplaceId;
        AssignTask(goToWork);
        m_workerState = WorkerState::Moving;
        return;
    }

    // Idle wandering behavior - occasionally move a little
    if (Nova::Random::Value() < 0.005f) {
        glm::vec2 randomDir = Nova::Random::Direction2D();
        glm::vec3 target = m_position + glm::vec3(randomDir.x, 0.0f, randomDir.y) * 3.0f;

        if (navGraph) {
            RequestPath(target, *navGraph);
            if (HasPath()) {
                m_workerState = WorkerState::Moving;
            }
        }
    }
}

void Worker::UpdateMoving(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph) {
    // Check for threats (unless Guard)
    if (m_job != WorkerJob::Guard) {
        Entity::EntityId threat = DetectThreat(entityManager);
        if (threat != Entity::INVALID_ID) {
            m_threatId = threat;
            m_preFleeState = WorkerState::Moving;
            m_workerState = WorkerState::Fleeing;
            m_fleeReassessTimer = 0.0f;
            ClearPath();
            return;
        }
    }

    // Handle FollowHero task - update target periodically
    if (m_currentTask.type == WorkTask::Type::FollowHero) {
        if (Player* player = entityManager.GetPlayer()) {
            float distToHero = glm::distance(
                glm::vec2(m_position.x, m_position.z),
                glm::vec2(player->GetPosition().x, player->GetPosition().z)
            );

            // Close enough - stop following
            if (distToHero <= 2.5f) {
                m_velocity = glm::vec3(0.0f);
                ClearPath();
                ClearTask();
                m_workerState = WorkerState::Idle;
                return;
            }

            // Update path periodically
            if (m_pathUpdateTimer <= 0.0f) {
                m_currentTask.targetPosition = player->GetPosition();
                if (navGraph) {
                    RequestPath(player->GetPosition(), *navGraph);
                }
                m_pathUpdateTimer = PATH_UPDATE_INTERVAL;
            }
        }
    }

    // Check if reached destination
    float distToTarget = glm::distance(
        glm::vec2(m_position.x, m_position.z),
        glm::vec2(m_currentTask.targetPosition.x, m_currentTask.targetPosition.z)
    );

    if (distToTarget < WORK_RANGE) {
        // Arrived at destination
        m_velocity = glm::vec3(0.0f);
        ClearPath();

        switch (m_currentTask.type) {
            case WorkTask::Type::GoHome:
                m_workerState = WorkerState::Resting;
                break;

            case WorkTask::Type::GoToWork:
                // Start working
                m_currentTask.type = GetJobTaskType();
                m_currentTask.progress = 0.0f;
                m_currentTask.duration = GetJobTaskDuration();
                m_workerState = WorkerState::Working;
                break;

            case WorkTask::Type::FollowHero:
                m_workerState = WorkerState::Idle;
                ClearTask();
                break;

            default:
                // General movement complete
                if (m_onTaskComplete) {
                    m_onTaskComplete(*this, m_currentTask);
                }
                ClearTask();
                m_workerState = WorkerState::Idle;
                break;
        }
        return;
    }

    // Follow path or move directly
    if (HasPath()) {
        FollowPath(deltaTime);
    } else if (navGraph && m_pathUpdateTimer <= 0.0f) {
        RequestPath(m_currentTask.targetPosition, *navGraph);
        m_pathUpdateTimer = PATH_UPDATE_INTERVAL;
    } else {
        MoveToward(m_currentTask.targetPosition, deltaTime);
    }
}

void Worker::UpdateWorking(float deltaTime, EntityManager& entityManager) {
    m_velocity = glm::vec3(0.0f);

    // Check for threats (unless Guard - guards fight instead of flee)
    if (m_job != WorkerJob::Guard) {
        Entity::EntityId threat = DetectThreat(entityManager);
        if (threat != Entity::INVALID_ID) {
            m_threatId = threat;
            m_preFleeState = WorkerState::Working;
            m_workerState = WorkerState::Fleeing;
            m_fleeReassessTimer = 0.0f;
            return;
        }
    }

    // Check if needs are too critical to continue working
    if (m_needs.IsExhausted() || m_needs.IsStarving()) {
        // Must stop working
        m_workerState = WorkerState::Idle;
        return;
    }

    // Progress on task
    float productivity = GetProductivity();
    float taskProgress = (deltaTime / m_currentTask.duration) * productivity;
    m_currentTask.progress += taskProgress;

    // Improve job skill while working
    ImproveJobSkill(WorkerSkills::SKILL_GAIN_RATE * deltaTime * productivity);

    // Check if task complete
    if (m_currentTask.IsComplete()) {
        if (m_onTaskComplete) {
            m_onTaskComplete(*this, m_currentTask);
        }

        if (m_currentTask.repeating) {
            m_currentTask.Reset();
        } else {
            ClearTask();
            m_workerState = WorkerState::Idle;
        }
    }
}

void Worker::UpdateResting(float deltaTime, EntityManager& entityManager) {
    m_velocity = glm::vec3(0.0f);

    // Check for threats even while resting
    if (m_job != WorkerJob::Guard) {
        Entity::EntityId threat = DetectThreat(entityManager);
        if (threat != Entity::INVALID_ID) {
            m_threatId = threat;
            m_preFleeState = WorkerState::Resting;
            m_workerState = WorkerState::Fleeing;
            m_fleeReassessTimer = 0.0f;
            return;
        }
    }

    // Resting recovers energy and health (needs system handles this)
    // Check if fully rested
    if (m_needs.energy >= WorkerNeeds::GOOD_THRESHOLD &&
        m_needs.health >= WorkerNeeds::MODERATE_THRESHOLD) {
        // Done resting
        ClearTask();
        m_workerState = WorkerState::Idle;
    }
}

void Worker::UpdateFleeing(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph) {
    // Reassess threat periodically
    m_fleeReassessTimer -= deltaTime;

    if (m_fleeReassessTimer <= 0.0f) {
        m_fleeReassessTimer = 0.5f;

        // Check if threat still exists
        Entity* threat = entityManager.GetEntity(m_threatId);
        if (!threat || !threat->IsAlive()) {
            // Threat eliminated
            m_threatId = Entity::INVALID_ID;
            ClearPath();
            m_workerState = m_preFleeState;
            return;
        }

        float distToThreat = DistanceTo(*threat);
        float fleeDistance = m_personality.GetFleeDistance(FLEE_DISTANCE);

        if (distToThreat > fleeDistance) {
            // Safe distance reached
            m_threatId = Entity::INVALID_ID;
            ClearPath();
            m_workerState = m_preFleeState;
            return;
        }

        // Find new flee target
        m_fleeTarget = m_position + CalculateFleeDirection(entityManager) * fleeDistance;
        ClearPath();
    }

    // Move toward flee target with speed bonus
    float originalSpeed = m_moveSpeed;
    m_moveSpeed *= FLEE_SPEED_MULTIPLIER;

    if (navGraph && !HasPath() && m_pathUpdateTimer <= 0.0f) {
        RequestPath(m_fleeTarget, *navGraph);
        m_pathUpdateTimer = PATH_UPDATE_INTERVAL;
    }

    if (HasPath()) {
        FollowPath(deltaTime);
    } else {
        MoveToward(m_fleeTarget, deltaTime);
    }

    m_moveSpeed = originalSpeed;
}

void Worker::UpdateInjured(float deltaTime, EntityManager& entityManager) {
    // Injured workers move slowly and seek help
    m_velocity = glm::vec3(0.0f);

    // Still check for threats
    Entity::EntityId threat = DetectThreat(entityManager);
    if (threat != Entity::INVALID_ID) {
        // Even injured workers try to flee (slowly)
        m_threatId = threat;
        m_preFleeState = WorkerState::Injured;
        m_workerState = WorkerState::Fleeing;
        m_moveSpeed *= INJURED_SPEED_MULTIPLIER;
        return;
    }

    // Recovery over time (slow without medic)
    // Needs system handles base recovery

    // Check if recovered enough to function
    if (!m_needs.IsCriticallyInjured()) {
        m_workerState = m_preFleeState;
    }
}

// ============================================================================
// Threat Detection
// ============================================================================

Entity::EntityId Worker::DetectThreat(EntityManager& entityManager) {
    float detectionRange = DETECTION_RADIUS;

    // Scouts have better detection
    if (m_job == WorkerJob::Scout) {
        detectionRange *= 1.5f;
    }

    // Guards don't detect threats (they fight them)
    if (m_job == WorkerJob::Guard) {
        return Entity::INVALID_ID;
    }

    // Find nearest zombie
    Entity* nearestZombie = entityManager.GetNearestEntity(m_position, EntityType::Zombie);

    if (nearestZombie && nearestZombie->IsAlive()) {
        float dist = DistanceTo(*nearestZombie);
        float triggerDist = m_personality.GetFleeDistance(detectionRange);

        if (dist <= triggerDist) {
            return nearestZombie->GetId();
        }
    }

    return Entity::INVALID_ID;
}

glm::vec3 Worker::CalculateFleeDirection(EntityManager& entityManager) {
    glm::vec3 fleeDir{0.0f};
    int threatCount = 0;

    // Get all nearby zombies
    auto zombies = entityManager.FindEntitiesInRadius(m_position, DETECTION_RADIUS * 1.5f, EntityType::Zombie);

    for (Entity* zombie : zombies) {
        if (zombie && zombie->IsAlive()) {
            glm::vec3 awayDir = m_position - zombie->GetPosition();
            awayDir.y = 0.0f;

            float dist = glm::length(awayDir);
            if (dist > 0.01f) {
                awayDir = glm::normalize(awayDir) / (dist + 1.0f);
                fleeDir += awayDir;
                threatCount++;
            }
        }
    }

    if (threatCount > 0) {
        fleeDir /= static_cast<float>(threatCount);
        if (glm::length(fleeDir) > 0.01f) {
            return glm::normalize(fleeDir);
        }
    }

    // Random direction if no clear flee direction
    glm::vec2 dir = Nova::Random::Direction2D();
    return glm::vec3(dir.x, 0.0f, dir.y);
}

// ============================================================================
// Path Following
// ============================================================================

void Worker::ClearPath() {
    m_currentPath = Nova::PathResult();
    m_pathIndex = 0;
}

bool Worker::RequestPath(const glm::vec3& target, Nova::Graph& navGraph) {
    int startNode = navGraph.GetNearestWalkableNode(m_position);
    int endNode = navGraph.GetNearestWalkableNode(target);

    if (startNode < 0 || endNode < 0) {
        return false;
    }

    m_currentPath = Nova::Pathfinder::AStar(navGraph, startNode, endNode);
    m_pathIndex = 0;

    return m_currentPath.found;
}

void Worker::FollowPath(float deltaTime) {
    if (!HasPath() || m_pathIndex >= m_currentPath.positions.size()) {
        ClearPath();
        return;
    }

    glm::vec3 waypoint = m_currentPath.positions[m_pathIndex];
    float distToWaypoint = glm::distance(
        glm::vec2(m_position.x, m_position.z),
        glm::vec2(waypoint.x, waypoint.z)
    );

    if (distToWaypoint < 0.5f) {
        m_pathIndex++;
        if (m_pathIndex >= m_currentPath.positions.size()) {
            ClearPath();
            return;
        }
        waypoint = m_currentPath.positions[m_pathIndex];
    }

    MoveToward(waypoint, deltaTime);
}

void Worker::MoveToward(const glm::vec3& target, float deltaTime) {
    glm::vec3 direction = target - m_position;
    direction.y = 0.0f;

    float distance = glm::length(direction);
    if (distance > 0.01f) {
        direction = glm::normalize(direction);
        m_velocity = direction * m_moveSpeed;
        LookAt(target);
    } else {
        m_velocity = glm::vec3(0.0f);
    }
}

void Worker::MoveTo(const glm::vec3& position, Nova::Graph* navGraph) {
    WorkTask moveTask;
    moveTask.type = WorkTask::Type::None;  // Generic move
    moveTask.targetPosition = position;
    AssignTask(moveTask);

    if (navGraph) {
        RequestPath(position, *navGraph);
    }

    m_workerState = WorkerState::Moving;
}

// ============================================================================
// Needs Management
// ============================================================================

void Worker::UpdateNeeds(float deltaTime) {
    bool isWorking = (m_workerState == WorkerState::Working);
    bool isMoving = (m_workerState == WorkerState::Moving || m_workerState == WorkerState::Fleeing);
    bool isResting = (m_workerState == WorkerState::Resting);

    m_needs.Update(deltaTime, isWorking, isMoving, isResting);

    // Sync health with Entity health
    if (m_needs.health < m_health) {
        m_health = m_needs.health;
    }

    // Check for death conditions
    if (m_needs.IsDead()) {
        Die();
    }
}

float Worker::GetProductivity() const noexcept {
    // Base productivity from needs
    float productivity = m_needs.GetProductivityModifier();

    // Multiply by skill modifier
    float skillLevel = GetJobSkillLevel();
    productivity *= WorkerSkills::GetSkillModifier(skillLevel);

    // Personality modifier
    productivity *= m_personality.GetWorkSpeedModifier();

    return productivity;
}

float Worker::GetJobSkillLevel() const noexcept {
    switch (m_job) {
        case WorkerJob::Gatherer:  return m_skills.gathering;
        case WorkerJob::Builder:   return m_skills.building;
        case WorkerJob::Farmer:    return m_skills.farming;
        case WorkerJob::Guard:     return m_skills.combat;
        case WorkerJob::Crafter:   return m_skills.crafting;
        case WorkerJob::Medic:     return m_skills.medical;
        case WorkerJob::Scout:     return m_skills.scouting;
        case WorkerJob::Trader:    return m_skills.trading;
        default:                   return 10.0f;
    }
}

void Worker::ImproveJobSkill(float amount) {
    switch (m_job) {
        case WorkerJob::Gatherer:  WorkerSkills::ImproveSkill(m_skills.gathering, amount); break;
        case WorkerJob::Builder:   WorkerSkills::ImproveSkill(m_skills.building, amount); break;
        case WorkerJob::Farmer:    WorkerSkills::ImproveSkill(m_skills.farming, amount); break;
        case WorkerJob::Guard:     WorkerSkills::ImproveSkill(m_skills.combat, amount); break;
        case WorkerJob::Crafter:   WorkerSkills::ImproveSkill(m_skills.crafting, amount); break;
        case WorkerJob::Medic:     WorkerSkills::ImproveSkill(m_skills.medical, amount); break;
        case WorkerJob::Scout:     WorkerSkills::ImproveSkill(m_skills.scouting, amount); break;
        case WorkerJob::Trader:    WorkerSkills::ImproveSkill(m_skills.trading, amount); break;
        default: break;
    }
}

// ============================================================================
// Job Assignment
// ============================================================================

void Worker::ClearJobAssignment() {
    m_job = WorkerJob::None;
    m_workplaceId = 0;
    m_workplacePosition = glm::vec3(0.0f);
    ClearTask();
}

// ============================================================================
// Tasks
// ============================================================================

void Worker::AssignTask(const WorkTask& task) {
    m_currentTask = task;
    m_stateTimer = 0.0f;
}

void Worker::ClearTask() {
    m_currentTask.Clear();
}

// Helper to get task type for job
WorkTask::Type Worker::GetJobTaskType() const {
    switch (m_job) {
        case WorkerJob::Gatherer:  return WorkTask::Type::Gather;
        case WorkerJob::Builder:   return WorkTask::Type::Build;
        case WorkerJob::Farmer:    return WorkTask::Type::Farm;
        case WorkerJob::Guard:     return WorkTask::Type::Patrol;
        case WorkerJob::Crafter:   return WorkTask::Type::Craft;
        case WorkerJob::Medic:     return WorkTask::Type::HealTarget;
        case WorkerJob::Scout:     return WorkTask::Type::Scout;
        case WorkerJob::Trader:    return WorkTask::Type::Trade;
        default:                   return WorkTask::Type::None;
    }
}

// Helper to get task duration for job
float Worker::GetJobTaskDuration() const {
    switch (m_job) {
        case WorkerJob::Gatherer:  return 10.0f;
        case WorkerJob::Builder:   return 30.0f;
        case WorkerJob::Farmer:    return 20.0f;
        case WorkerJob::Guard:     return 60.0f;  // Patrol duration
        case WorkerJob::Crafter:   return 15.0f;
        case WorkerJob::Medic:     return 5.0f;
        case WorkerJob::Scout:     return 45.0f;
        case WorkerJob::Trader:    return 25.0f;
        default:                   return 10.0f;
    }
}

// ============================================================================
// Loyalty / Desertion
// ============================================================================

bool Worker::CheckDesertion(float deltaTime) {
    m_desertionCheckTimer += deltaTime;

    // Check once per in-game day (let's say 60 seconds = 1 day)
    if (m_desertionCheckTimer < 60.0f) {
        return false;
    }
    m_desertionCheckTimer = 0.0f;

    // Calculate desertion chance
    float baseChance = m_needs.GetDesertionChance();

    // Loyalty reduces desertion
    float loyaltyMod = 1.0f - (m_loyalty / 100.0f) * 0.8f;
    baseChance *= loyaltyMod;

    // Personality affects desertion
    baseChance *= m_personality.GetLoyaltyModifier();

    // Roll for desertion
    if (Nova::Random::Value() < baseChance) {
        // Worker deserts!
        if (m_onDesertion) {
            m_onDesertion(*this);
        }
        MarkForRemoval();
        return true;
    }

    return false;
}

// ============================================================================
// Combat / Damage
// ============================================================================

float Worker::TakeDamage(float amount, EntityId source) {
    float damage = Entity::TakeDamage(amount, source);

    // Update needs health
    m_needs.TakeDamage(damage);

    // Reduce morale when hurt
    m_needs.ModifyMorale(-damage * 0.5f);

    // Being attacked reduces loyalty (player failed to protect them)
    ModifyLoyalty(-damage * 0.1f);

    // If attacked and not already fleeing/dead, start fleeing
    if (damage > 0.0f && m_workerState != WorkerState::Fleeing &&
        m_workerState != WorkerState::Dead && m_job != WorkerJob::Guard) {
        m_threatId = source;
        m_preFleeState = m_workerState;
        m_workerState = WorkerState::Fleeing;
        m_fleeReassessTimer = 0.0f;
    }

    return damage;
}

void Worker::Die() {
    m_workerState = WorkerState::Dead;
    m_velocity = glm::vec3(0.0f);
    ClearTask();
    ClearPath();

    if (m_onDeath) {
        m_onDeath(*this);
    }

    Entity::Die();
}

// ============================================================================
// Appearance
// ============================================================================

void Worker::SetAppearanceIndex(int index) {
    m_appearanceIndex = std::clamp(index, 1, 9);
    m_texturePath = NPC::GetAppearanceTexturePath(m_appearanceIndex);
}

// ============================================================================
// Personality Generation
// ============================================================================

WorkerPersonality WorkerPersonality::GenerateRandom() {
    WorkerPersonality p;
    p.bravery = Nova::Random::Range(-1.0f, 1.0f);
    p.diligence = Nova::Random::Range(-1.0f, 1.0f);
    p.sociability = Nova::Random::Range(-1.0f, 1.0f);
    p.optimism = Nova::Random::Range(-1.0f, 1.0f);
    p.loyalty = Nova::Random::Range(-1.0f, 1.0f);
    return p;
}

} // namespace Vehement
