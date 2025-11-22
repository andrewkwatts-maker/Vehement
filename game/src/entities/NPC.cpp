#include "NPC.hpp"
#include "EntityManager.hpp"
#include "Zombie.hpp"
#include <engine/pathfinding/Graph.hpp>
#include <engine/math/Random.hpp>
#include <engine/graphics/Renderer.hpp>
#include <algorithm>
#include <cmath>

namespace Vehement {

NPC::NPC() : Entity(EntityType::NPC) {
    m_moveSpeed = DEFAULT_MOVE_SPEED;
    m_maxHealth = 50.0f;
    m_health = m_maxHealth;
    m_collisionRadius = 0.35f;
    m_name = "Civilian";

    // Random appearance
    m_appearanceIndex = Nova::Random::Range(1, 9);
    m_texturePath = GetAppearanceTexturePath(m_appearanceIndex);
}

NPC::NPC(int appearanceIndex) : Entity(EntityType::NPC) {
    m_moveSpeed = DEFAULT_MOVE_SPEED;
    m_maxHealth = 50.0f;
    m_health = m_maxHealth;
    m_collisionRadius = 0.35f;
    m_name = "Civilian";

    SetAppearanceIndex(appearanceIndex);
}

void NPC::Update(float deltaTime) {
    // Apply velocity
    m_position += m_velocity * deltaTime;

    // Keep on ground
    m_position.y = m_groundLevel;

    Entity::Update(deltaTime);
}

void NPC::Render(Nova::Renderer& renderer) {
    // Infected NPCs could have a visual indicator
    // TODO: Add tint or overlay for infected state

    Entity::Render(renderer);
}

void NPC::UpdateAI(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph) {
    // Update path timer
    m_pathUpdateTimer -= deltaTime;

    // State machine
    switch (m_state) {
        case NPCState::Idle:
            UpdateIdle(deltaTime, entityManager);
            break;
        case NPCState::Wander:
            UpdateWander(deltaTime, entityManager, navGraph);
            break;
        case NPCState::Flee:
            UpdateFlee(deltaTime, entityManager, navGraph);
            break;
        case NPCState::Infected:
            UpdateInfected(deltaTime);
            break;
        case NPCState::Turning:
            UpdateTurning(deltaTime);
            break;
    }
}

void NPC::UpdateIdle(float deltaTime, EntityManager& entityManager) {
    m_velocity = glm::vec3(0.0f);

    // Check for threats
    Entity::EntityId threat = DetectThreat(entityManager);
    if (threat != Entity::INVALID_ID) {
        m_threatId = threat;
        m_state = NPCState::Flee;
        return;
    }

    // If we have a routine, follow it
    if (HasRoutine()) {
        m_state = NPCState::Wander;
        return;
    }

    // Occasionally start random wandering
    if (Nova::Random::Value() < 0.01f) {  // 1% chance per frame
        m_state = NPCState::Wander;
    }
}

void NPC::UpdateWander(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph) {
    // Always check for threats
    Entity::EntityId threat = DetectThreat(entityManager);
    if (threat != Entity::INVALID_ID) {
        m_threatId = threat;
        m_preInfectionState = NPCState::Wander;
        m_state = NPCState::Flee;
        ClearPath();
        return;
    }

    // Follow routine if available
    if (HasRoutine()) {
        const NPCWaypoint* waypoint = m_routine.GetCurrentWaypoint();
        if (waypoint) {
            float distToWaypoint = glm::distance(
                glm::vec2(m_position.x, m_position.z),
                glm::vec2(waypoint->position.x, waypoint->position.z)
            );

            // At waypoint - wait
            if (distToWaypoint < 0.5f) {
                m_velocity = glm::vec3(0.0f);
                m_waypointWaitTimer -= deltaTime;

                if (m_waypointWaitTimer <= 0.0f) {
                    m_routine.NextWaypoint();
                    const NPCWaypoint* next = m_routine.GetCurrentWaypoint();
                    if (next) {
                        m_waypointWaitTimer = next->waitTime;
                        ClearPath();
                    }
                }
                return;
            }

            // Move to waypoint
            if (navGraph && !HasPath() && m_pathUpdateTimer <= 0.0f) {
                RequestPath(waypoint->position, *navGraph);
                m_pathUpdateTimer = PATH_UPDATE_INTERVAL;
            }

            if (HasPath()) {
                FollowPath(deltaTime);
            } else {
                MoveToward(waypoint->position, deltaTime);
            }
        }
    } else {
        // Random wandering without routine
        // Just stand around or walk randomly
        m_velocity = glm::vec3(0.0f);

        if (Nova::Random::Value() < 0.005f) {
            m_state = NPCState::Idle;
        }
    }
}

void NPC::UpdateFlee(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph) {
    // Reassess threat periodically
    m_fleeReassessTimer -= deltaTime;

    if (m_fleeReassessTimer <= 0.0f) {
        m_fleeReassessTimer = 0.5f;

        // Check if threat still exists and is close
        Entity* threat = entityManager.GetEntity(m_threatId);
        if (!threat || !threat->IsAlive()) {
            // Threat gone
            ClearThreat();
            ClearPath();
            m_state = m_preInfectionState;
            return;
        }

        float distToThreat = DistanceTo(*threat);
        if (distToThreat > SAFE_DISTANCE) {
            // Safe enough
            ClearThreat();
            ClearPath();
            m_state = m_preInfectionState;
            return;
        }

        // Find new flee target
        m_fleeTarget = FindSafePosition(entityManager);
        ClearPath();
    }

    // Check for new/closer threats
    Entity::EntityId newThreat = DetectThreat(entityManager);
    if (newThreat != Entity::INVALID_ID && newThreat != m_threatId) {
        Entity* current = entityManager.GetEntity(m_threatId);
        Entity* newEntity = entityManager.GetEntity(newThreat);

        if (newEntity && (!current || DistanceTo(*newEntity) < DistanceTo(*current))) {
            m_threatId = newThreat;
            m_fleeReassessTimer = 0.0f;  // Reassess immediately
        }
    }

    // Move toward flee target
    float distToFleeTarget = glm::distance(
        glm::vec2(m_position.x, m_position.z),
        glm::vec2(m_fleeTarget.x, m_fleeTarget.z)
    );

    if (distToFleeTarget < 1.0f) {
        // Reached flee target, reassess
        m_fleeReassessTimer = 0.0f;
    }

    // Use pathfinding if available
    if (navGraph && !HasPath() && m_pathUpdateTimer <= 0.0f) {
        RequestPath(m_fleeTarget, *navGraph);
        m_pathUpdateTimer = PATH_UPDATE_INTERVAL;
    }

    // Move with flee speed bonus
    float originalSpeed = m_moveSpeed;
    m_moveSpeed *= FLEE_SPEED_MULTIPLIER;

    if (HasPath()) {
        FollowPath(deltaTime);
    } else {
        MoveToward(m_fleeTarget, deltaTime);
    }

    m_moveSpeed = originalSpeed;
}

void NPC::UpdateInfected(float deltaTime) {
    // Continue previous behavior but with infection timer
    m_infectionTimer -= deltaTime;

    // Visual effects as infection progresses
    // TODO: Implement visual degradation

    // Check for threats (infected NPCs still flee)
    // Note: UpdateAI handles threat detection before state-specific updates

    if (m_infectionTimer <= m_infectionDuration * 0.1f) {
        // Last 10% - start turning
        m_state = NPCState::Turning;
        m_velocity = glm::vec3(0.0f);
    }
}

void NPC::UpdateTurning(float deltaTime) {
    // NPC stops moving while turning
    m_velocity = glm::vec3(0.0f);

    m_infectionTimer -= deltaTime;

    if (m_infectionTimer <= 0.0f) {
        // Turn into zombie - notify game logic
        if (m_onTurn) {
            m_onTurn(*this);
        }

        // Mark for removal (game logic should spawn zombie)
        MarkForRemoval();
    }
}

void NPC::Infect() {
    if (IsInfected()) return;

    m_preInfectionState = m_state;
    m_state = NPCState::Infected;
    m_infectionTimer = m_infectionDuration;
}

bool NPC::Cure() {
    if (m_state == NPCState::Infected) {
        m_state = m_preInfectionState;
        m_infectionTimer = 0.0f;
        return true;
    }
    return false;  // Can't cure if already turning
}

Entity::EntityId NPC::DetectThreat(EntityManager& entityManager) {
    // Find nearest zombie within detection radius
    Entity* nearestZombie = entityManager.GetNearestEntity(m_position, EntityType::Zombie);

    if (nearestZombie && nearestZombie->IsAlive()) {
        float dist = DistanceTo(*nearestZombie);
        if (dist <= DETECTION_RADIUS) {
            return nearestZombie->GetId();
        }
    }

    return Entity::INVALID_ID;
}

glm::vec3 NPC::CalculateFleeDirection(EntityManager& entityManager) {
    glm::vec3 fleeDir{0.0f};
    int threatCount = 0;

    // Get all nearby zombies and calculate combined flee direction
    auto zombies = entityManager.FindEntitiesInRadius(m_position, DETECTION_RADIUS, EntityType::Zombie);

    for (Entity* zombie : zombies) {
        if (zombie && zombie->IsAlive()) {
            glm::vec3 awayDir = m_position - zombie->GetPosition();
            awayDir.y = 0.0f;

            float dist = glm::length(awayDir);
            if (dist > 0.01f) {
                // Weight by inverse distance (closer threats are more important)
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
    return glm::vec3(Nova::Random::Direction2D().x, 0.0f, Nova::Random::Direction2D().y);
}

glm::vec3 NPC::FindSafePosition(EntityManager& entityManager) {
    glm::vec3 fleeDir = CalculateFleeDirection(entityManager);

    // Project flee target at safe distance
    glm::vec3 target = m_position + fleeDir * SAFE_DISTANCE;

    return target;
}

void NPC::SetAppearanceIndex(int index) {
    m_appearanceIndex = std::clamp(index, 1, 9);
    m_texturePath = GetAppearanceTexturePath(m_appearanceIndex);
}

std::string NPC::GetAppearanceTexturePath(int index) {
    return "Vehement2/images/People/Person" + std::to_string(std::clamp(index, 1, 9)) + ".png";
}

void NPC::Die() {
    Entity::Die();
    // NPCs don't drop coins
}

float NPC::TakeDamage(float amount, EntityId source) {
    float damage = Entity::TakeDamage(amount, source);

    // If attacked and not already fleeing/infected, start fleeing
    if (damage > 0.0f && m_state != NPCState::Flee && !IsInfected()) {
        m_threatId = source;
        m_state = NPCState::Flee;
        m_fleeReassessTimer = 0.0f;  // Immediately assess threat
    }

    return damage;
}

void NPC::ClearPath() {
    m_currentPath = Nova::PathResult();
    m_pathIndex = 0;
}

bool NPC::RequestPath(const glm::vec3& target, Nova::Graph& navGraph) {
    int startNode = navGraph.GetNearestWalkableNode(m_position);
    int endNode = navGraph.GetNearestWalkableNode(target);

    if (startNode < 0 || endNode < 0) {
        return false;
    }

    m_currentPath = Nova::Pathfinder::AStar(navGraph, startNode, endNode);
    m_pathIndex = 0;

    return m_currentPath.found;
}

void NPC::FollowPath(float deltaTime) {
    if (!HasPath() || m_pathIndex >= m_currentPath.positions.size()) {
        ClearPath();
        return;
    }

    glm::vec3 waypoint = m_currentPath.positions[m_pathIndex];
    float distToWaypoint = glm::distance(
        glm::vec2(m_position.x, m_position.z),
        glm::vec2(waypoint.x, waypoint.z)
    );

    // Move to next waypoint if close enough
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

void NPC::MoveToward(const glm::vec3& target, float deltaTime) {
    glm::vec3 direction = target - m_position;
    direction.y = 0.0f;

    float distance = glm::length(direction);
    if (distance > 0.01f) {
        direction = glm::normalize(direction);
        m_velocity = direction * m_moveSpeed;

        // Face movement direction
        LookAt(target);
    } else {
        m_velocity = glm::vec3(0.0f);
    }
}

} // namespace Vehement
