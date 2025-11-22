#include "Zombie.hpp"
#include "EntityManager.hpp"
#include "NPC.hpp"
#include <engine/pathfinding/Graph.hpp>
#include <engine/math/Random.hpp>
#include <engine/graphics/Renderer.hpp>
#include <algorithm>
#include <cmath>

namespace Vehement {

// ============================================================================
// ZombieConfig Implementation
// ============================================================================

ZombieConfig ZombieConfig::GetConfig(ZombieType type) {
    ZombieConfig config;

    switch (type) {
        case ZombieType::Standard:
            config.moveSpeed = 3.0f;
            config.health = 50.0f;
            config.damage = 10.0f;
            config.attackRange = 1.5f;
            config.attackCooldown = 1.0f;
            config.detectionRadius = 15.0f;
            config.infectionChance = 0.3f;
            config.coinDropMin = 1;
            config.coinDropMax = 5;
            break;

        case ZombieType::Slow:
            config.moveSpeed = 1.5f;
            config.health = 80.0f;
            config.damage = 15.0f;
            config.attackRange = 1.5f;
            config.attackCooldown = 1.5f;
            config.detectionRadius = 10.0f;
            config.infectionChance = 0.4f;
            config.coinDropMin = 2;
            config.coinDropMax = 8;
            break;

        case ZombieType::Fast:
            config.moveSpeed = 6.0f;
            config.health = 30.0f;
            config.damage = 8.0f;
            config.attackRange = 1.2f;
            config.attackCooldown = 0.6f;
            config.detectionRadius = 20.0f;
            config.infectionChance = 0.2f;
            config.coinDropMin = 1;
            config.coinDropMax = 3;
            break;

        case ZombieType::Tank:
            config.moveSpeed = 1.0f;
            config.health = 200.0f;
            config.damage = 30.0f;
            config.attackRange = 2.0f;
            config.attackCooldown = 2.0f;
            config.detectionRadius = 12.0f;
            config.infectionChance = 0.5f;
            config.coinDropMin = 5;
            config.coinDropMax = 15;
            break;
    }

    return config;
}

// ============================================================================
// Zombie Implementation
// ============================================================================

Zombie::Zombie() : Entity(EntityType::Zombie) {
    ApplyConfig(ZombieType::Standard);
    m_texturePath = "Vehement2/images/People/ZombieA.png";
    m_name = "Zombie";
}

Zombie::Zombie(ZombieType type) : Entity(EntityType::Zombie) {
    ApplyConfig(type);
    m_texturePath = "Vehement2/images/People/ZombieA.png";
    m_name = ZombieTypeToString(type);
}

void Zombie::ApplyConfig(ZombieType type) {
    m_zombieType = type;
    m_config = ZombieConfig::GetConfig(type);
    m_moveSpeed = m_config.moveSpeed;
    m_maxHealth = m_config.health;
    m_health = m_maxHealth;
    m_collisionRadius = 0.4f;
}

void Zombie::Update(float deltaTime) {
    // Update cooldowns
    if (m_attackCooldownTimer > 0.0f) {
        m_attackCooldownTimer -= deltaTime;
    }

    // Apply velocity
    m_position += m_velocity * deltaTime;

    // Keep on ground
    m_position.y = m_groundLevel;

    Entity::Update(deltaTime);
}

void Zombie::Render(Nova::Renderer& renderer) {
    Entity::Render(renderer);
}

void Zombie::UpdateAI(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph) {
    // Update path timer
    m_pathUpdateTimer -= deltaTime;

    // State machine
    switch (m_state) {
        case ZombieState::Idle:
            UpdateIdle(deltaTime, entityManager);
            break;
        case ZombieState::Wander:
            UpdateWander(deltaTime, entityManager, navGraph);
            break;
        case ZombieState::Chase:
            UpdateChase(deltaTime, entityManager, navGraph);
            break;
        case ZombieState::Attack:
            UpdateAttack(deltaTime, entityManager);
            break;
        case ZombieState::Infecting:
            UpdateInfecting(deltaTime);
            break;
    }
}

void Zombie::UpdateIdle(float deltaTime, EntityManager& entityManager) {
    // Look for targets
    Entity::EntityId target = FindTarget(entityManager);

    if (target != Entity::INVALID_ID) {
        m_targetId = target;
        m_state = ZombieState::Chase;
        return;
    }

    // Occasionally start wandering
    m_wanderWaitTimer -= deltaTime;
    if (m_wanderWaitTimer <= 0.0f) {
        // Pick a random point near home
        float angle = Nova::Random::Angle();
        float distance = Nova::Random::Range(2.0f, WANDER_RADIUS);
        m_wanderTarget = m_homePosition + glm::vec3(
            std::cos(angle) * distance,
            0.0f,
            std::sin(angle) * distance
        );
        m_state = ZombieState::Wander;
    }
}

void Zombie::UpdateWander(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph) {
    // Check for targets while wandering
    Entity::EntityId target = FindTarget(entityManager);
    if (target != Entity::INVALID_ID) {
        m_targetId = target;
        m_state = ZombieState::Chase;
        ClearPath();
        return;
    }

    // Move toward wander target
    float distToTarget = glm::distance(m_position, m_wanderTarget);

    if (distToTarget < 1.0f) {
        // Reached target, wait then go idle
        m_velocity = glm::vec3(0.0f);
        m_wanderWaitTimer = WANDER_WAIT_TIME + Nova::Random::Range(0.0f, WANDER_WAIT_TIME);
        m_state = ZombieState::Idle;
        return;
    }

    // Use pathfinding if available
    if (navGraph && !HasPath() && m_pathUpdateTimer <= 0.0f) {
        RequestPath(m_wanderTarget, *navGraph);
        m_pathUpdateTimer = PATH_UPDATE_INTERVAL;
    }

    if (HasPath()) {
        FollowPath(deltaTime);
    } else {
        MoveToward(m_wanderTarget, deltaTime);
    }
}

void Zombie::UpdateChase(float deltaTime, EntityManager& entityManager, Nova::Graph* navGraph) {
    // Validate target
    if (!ValidateTarget(entityManager)) {
        ClearTarget();
        ClearPath();
        m_state = ZombieState::Idle;
        m_wanderWaitTimer = 1.0f;
        return;
    }

    Entity* target = entityManager.GetEntity(m_targetId);
    if (!target) {
        ClearTarget();
        m_state = ZombieState::Idle;
        return;
    }

    float distToTarget = DistanceTo(*target);

    // Check if target is out of range
    if (distToTarget > LOSE_TARGET_DISTANCE) {
        ClearTarget();
        ClearPath();
        m_state = ZombieState::Idle;
        m_wanderWaitTimer = 1.0f;
        return;
    }

    // Check if in attack range
    if (distToTarget <= m_config.attackRange) {
        m_velocity = glm::vec3(0.0f);
        m_state = ZombieState::Attack;
        return;
    }

    // Update path periodically
    if (navGraph && m_pathUpdateTimer <= 0.0f) {
        RequestPath(target->GetPosition(), *navGraph);
        m_pathUpdateTimer = PATH_UPDATE_INTERVAL;
    }

    // Follow path or move directly
    if (HasPath()) {
        FollowPath(deltaTime);
    } else {
        MoveToward(target->GetPosition(), deltaTime);
    }
}

void Zombie::UpdateAttack(float deltaTime, EntityManager& entityManager) {
    // Validate target
    if (!ValidateTarget(entityManager)) {
        ClearTarget();
        m_state = ZombieState::Idle;
        m_wanderWaitTimer = 0.5f;
        return;
    }

    Entity* target = entityManager.GetEntity(m_targetId);
    if (!target) {
        ClearTarget();
        m_state = ZombieState::Idle;
        return;
    }

    float distToTarget = DistanceTo(*target);

    // Target moved out of range
    if (distToTarget > m_config.attackRange * 1.2f) {
        m_state = ZombieState::Chase;
        return;
    }

    // Face the target
    LookAt(target->GetPosition());

    // Attack when cooldown ready
    if (CanAttack()) {
        float damage = Attack(*target);

        // Check if we infected an NPC
        if (target->GetType() == EntityType::NPC && damage > 0.0f) {
            // Roll for infection
            if (Nova::Random::Value() < m_config.infectionChance) {
                NPC* npc = static_cast<NPC*>(target);
                if (!npc->IsInfected()) {
                    npc->Infect();
                    m_state = ZombieState::Infecting;
                    m_infectingTimer = 0.5f;  // Brief pause after infecting
                }
            }
        }
    }
}

void Zombie::UpdateInfecting(float deltaTime) {
    m_infectingTimer -= deltaTime;
    if (m_infectingTimer <= 0.0f) {
        // Return to chase or idle
        if (m_targetId != Entity::INVALID_ID) {
            m_state = ZombieState::Chase;
        } else {
            m_state = ZombieState::Idle;
            m_wanderWaitTimer = 1.0f;
        }
    }
}

Entity::EntityId Zombie::FindTarget(EntityManager& entityManager) {
    // Prefer player over NPCs
    Entity* player = entityManager.GetPlayer();
    if (player && player->IsAlive()) {
        float dist = DistanceTo(*player);
        if (dist <= m_config.detectionRadius) {
            return player->GetId();
        }
    }

    // Find nearest NPC
    Entity* nearestNPC = entityManager.GetNearestEntity(m_position, EntityType::NPC);
    if (nearestNPC && nearestNPC->IsAlive()) {
        float dist = DistanceTo(*nearestNPC);
        if (dist <= m_config.detectionRadius) {
            // Skip already-turning NPCs
            NPC* npc = static_cast<NPC*>(nearestNPC);
            if (npc->GetNPCState() != NPCState::Turning) {
                return nearestNPC->GetId();
            }
        }
    }

    return Entity::INVALID_ID;
}

bool Zombie::ValidateTarget(EntityManager& entityManager) {
    if (m_targetId == Entity::INVALID_ID) {
        return false;
    }

    Entity* target = entityManager.GetEntity(m_targetId);
    if (!target || !target->IsAlive()) {
        return false;
    }

    // For NPCs, check they're not already turning
    if (target->GetType() == EntityType::NPC) {
        NPC* npc = static_cast<NPC*>(target);
        if (npc->GetNPCState() == NPCState::Turning) {
            return false;
        }
    }

    return true;
}

float Zombie::Attack(Entity& target) {
    if (!CanAttack()) {
        return 0.0f;
    }

    m_attackCooldownTimer = m_config.attackCooldown;

    // Deal damage
    return target.TakeDamage(m_config.damage, m_id);
}

void Zombie::Die() {
    Entity::Die();

    // Drop coins (handled by game logic)
    // TODO: Spawn coin pickup entity
}

int Zombie::GetCoinDrop() const {
    return Nova::Random::Range(m_config.coinDropMin, m_config.coinDropMax);
}

void Zombie::ClearPath() {
    m_currentPath = Nova::PathResult();
    m_pathIndex = 0;
}

bool Zombie::RequestPath(const glm::vec3& target, Nova::Graph& navGraph) {
    int startNode = navGraph.GetNearestWalkableNode(m_position);
    int endNode = navGraph.GetNearestWalkableNode(target);

    if (startNode < 0 || endNode < 0) {
        return false;
    }

    m_currentPath = Nova::Pathfinder::AStar(navGraph, startNode, endNode);
    m_pathIndex = 0;

    return m_currentPath.found;
}

void Zombie::FollowPath(float deltaTime) {
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

void Zombie::MoveToward(const glm::vec3& target, float deltaTime) {
    glm::vec3 direction = target - m_position;
    direction.y = 0.0f;  // Keep movement horizontal

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
