# Tutorial: Creating AI Behavior

This tutorial walks through creating a complete AI behavior system for enemy units in Nova3D Engine. You will learn how to build a state machine-based AI with perception, decision making, and combat behaviors.

## Prerequisites

- Completed [First Entity Tutorial](first_entity.md)
- Basic understanding of state machines
- Familiarity with the scripting system

## Overview

We will create an intelligent enemy AI that can:
- Patrol between waypoints
- Detect and track players
- Engage in combat with abilities
- Retreat when health is low
- Coordinate with other AI units

## Part 1: AI Component Structure

### Step 1: Define the AI Component

Create the header file for our AI component:

```cpp
// game/src/components/ai/EnemyAI.hpp
#pragma once

#include <engine/core/Engine.hpp>
#include <engine/reflection/TypeRegistry.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <functional>
#include <memory>

namespace Vehement {

/**
 * @brief AI states for the enemy behavior state machine
 */
enum class AIState {
    Idle,       ///< Standing still, no target
    Patrol,     ///< Moving between waypoints
    Chase,      ///< Pursuing a detected target
    Attack,     ///< Engaging target in combat
    Retreat,    ///< Fleeing when health is low
    Dead        ///< Unit has died
};

/**
 * @brief Configuration for AI perception
 */
struct PerceptionConfig {
    float sightRange = 15.0f;       ///< Maximum sight distance
    float sightAngle = 120.0f;      ///< Field of view in degrees
    float hearingRange = 8.0f;      ///< Range to hear sounds
    float memoryDuration = 5.0f;    ///< How long to remember last known position
    float updateInterval = 0.1f;    ///< Perception update frequency
};

/**
 * @brief Configuration for AI combat behavior
 */
struct CombatConfig {
    float attackRange = 2.0f;       ///< Melee attack range
    float preferredRange = 1.5f;    ///< Ideal combat distance
    float retreatThreshold = 0.25f; ///< Health % to trigger retreat
    float aggressionLevel = 0.7f;   ///< 0-1, affects decision making
    std::vector<std::string> abilities; ///< Available combat abilities
};

/**
 * @brief Target information tracked by perception
 */
struct TargetInfo {
    uint64_t entityId = 0;
    glm::vec3 lastKnownPosition{0};
    glm::vec3 lastKnownVelocity{0};
    float lastSeenTime = 0.0f;
    float threatLevel = 0.0f;
    bool isVisible = false;
    bool isInRange = false;
};

/**
 * @brief Enemy AI component with state machine behavior
 */
class EnemyAI {
public:
    EnemyAI() = default;
    explicit EnemyAI(uint64_t ownerEntity);

    // Lifecycle
    void Initialize();
    void Update(float deltaTime);
    void Shutdown();

    // State management
    [[nodiscard]] AIState GetState() const { return m_currentState; }
    void SetState(AIState state);
    [[nodiscard]] float GetStateTime() const { return m_stateTime; }

    // Configuration
    void SetPerceptionConfig(const PerceptionConfig& config) { m_perception = config; }
    void SetCombatConfig(const CombatConfig& config) { m_combat = config; }
    [[nodiscard]] const PerceptionConfig& GetPerceptionConfig() const { return m_perception; }
    [[nodiscard]] const CombatConfig& GetCombatConfig() const { return m_combat; }

    // Patrol
    void SetPatrolPoints(const std::vector<glm::vec3>& points);
    void AddPatrolPoint(const glm::vec3& point);
    void SetPatrolLoop(bool loop) { m_patrolLoop = loop; }

    // Targeting
    [[nodiscard]] bool HasTarget() const { return m_currentTarget.entityId != 0; }
    [[nodiscard]] const TargetInfo& GetCurrentTarget() const { return m_currentTarget; }
    void SetTarget(uint64_t entityId);
    void ClearTarget();

    // Callbacks
    using StateChangeCallback = std::function<void(AIState oldState, AIState newState)>;
    using TargetAcquiredCallback = std::function<void(uint64_t targetId)>;
    using TargetLostCallback = std::function<void(uint64_t targetId)>;

    void OnStateChange(StateChangeCallback callback) { m_onStateChange = callback; }
    void OnTargetAcquired(TargetAcquiredCallback callback) { m_onTargetAcquired = callback; }
    void OnTargetLost(TargetLostCallback callback) { m_onTargetLost = callback; }

    // Debug
    void SetDebugDraw(bool enabled) { m_debugDraw = enabled; }
    [[nodiscard]] bool IsDebugDrawEnabled() const { return m_debugDraw; }

private:
    // State handlers
    void UpdateIdle(float deltaTime);
    void UpdatePatrol(float deltaTime);
    void UpdateChase(float deltaTime);
    void UpdateAttack(float deltaTime);
    void UpdateRetreat(float deltaTime);

    void EnterState(AIState state);
    void ExitState(AIState state);

    // Perception
    void UpdatePerception(float deltaTime);
    bool CanSeeEntity(uint64_t entityId) const;
    bool CanHearEntity(uint64_t entityId) const;
    float CalculateThreatLevel(uint64_t entityId) const;
    std::vector<uint64_t> FindVisibleEnemies() const;

    // Navigation
    void MoveTo(const glm::vec3& destination);
    void StopMovement();
    [[nodiscard]] bool HasReachedDestination() const;
    [[nodiscard]] glm::vec3 GetNextPatrolPoint();

    // Combat
    void SelectBestAbility();
    void UseAbility(const std::string& abilityId);
    bool ShouldRetreat() const;
    glm::vec3 FindRetreatPosition() const;

    // Debug rendering
    void DrawDebug() const;

private:
    uint64_t m_ownerEntity = 0;

    // State machine
    AIState m_currentState = AIState::Idle;
    AIState m_previousState = AIState::Idle;
    float m_stateTime = 0.0f;

    // Configuration
    PerceptionConfig m_perception;
    CombatConfig m_combat;

    // Patrol
    std::vector<glm::vec3> m_patrolPoints;
    size_t m_currentPatrolIndex = 0;
    bool m_patrolLoop = true;
    bool m_patrolReversing = false;

    // Targeting
    TargetInfo m_currentTarget;
    std::vector<TargetInfo> m_knownTargets;
    float m_perceptionTimer = 0.0f;

    // Combat
    float m_attackCooldown = 0.0f;
    std::string m_selectedAbility;

    // Navigation
    glm::vec3 m_moveDestination{0};
    bool m_isMoving = false;

    // Callbacks
    StateChangeCallback m_onStateChange;
    TargetAcquiredCallback m_onTargetAcquired;
    TargetLostCallback m_onTargetLost;

    // Debug
    bool m_debugDraw = false;
};

} // namespace Vehement

// Reflection registration
REFLECT_TYPE(Vehement::EnemyAI)
    REFLECT_PROPERTY(m_currentState, "State", PropertyFlags::ReadOnly)
    REFLECT_PROPERTY(m_perception.sightRange, "Sight Range", PropertyFlags::None, 0.0f, 50.0f)
    REFLECT_PROPERTY(m_perception.sightAngle, "Sight Angle", PropertyFlags::None, 30.0f, 360.0f)
    REFLECT_PROPERTY(m_perception.hearingRange, "Hearing Range", PropertyFlags::None, 0.0f, 30.0f)
    REFLECT_PROPERTY(m_combat.attackRange, "Attack Range", PropertyFlags::None, 0.5f, 10.0f)
    REFLECT_PROPERTY(m_combat.retreatThreshold, "Retreat Threshold", PropertyFlags::None, 0.0f, 1.0f)
    REFLECT_PROPERTY(m_combat.aggressionLevel, "Aggression", PropertyFlags::None, 0.0f, 1.0f)
    REFLECT_PROPERTY(m_debugDraw, "Debug Draw")
REFLECT_END()
```

### Step 2: Implement the AI Component

```cpp
// game/src/components/ai/EnemyAI.cpp
#include "EnemyAI.hpp"
#include <engine/spatial/SpatialIndex.hpp>
#include <engine/graphics/debug/DebugDraw.hpp>
#include <game/src/systems/lifecycle/LifecycleManager.hpp>
#include <algorithm>
#include <cmath>

namespace Vehement {

EnemyAI::EnemyAI(uint64_t ownerEntity)
    : m_ownerEntity(ownerEntity)
{
}

void EnemyAI::Initialize() {
    // Set initial state
    SetState(AIState::Idle);

    // Register for damage events
    Nova::EventBus::Instance().Subscribe<DamageEvent>(
        [this](const DamageEvent& event) {
            if (event.targetId == m_ownerEntity) {
                OnDamageTaken(event);
            }
        });
}

void EnemyAI::Update(float deltaTime) {
    if (m_currentState == AIState::Dead) {
        return;
    }

    m_stateTime += deltaTime;

    // Update perception
    UpdatePerception(deltaTime);

    // Update current state
    switch (m_currentState) {
        case AIState::Idle:    UpdateIdle(deltaTime);    break;
        case AIState::Patrol:  UpdatePatrol(deltaTime);  break;
        case AIState::Chase:   UpdateChase(deltaTime);   break;
        case AIState::Attack:  UpdateAttack(deltaTime);  break;
        case AIState::Retreat: UpdateRetreat(deltaTime); break;
        default: break;
    }

    // Update cooldowns
    m_attackCooldown = std::max(0.0f, m_attackCooldown - deltaTime);

    // Debug visualization
    if (m_debugDraw) {
        DrawDebug();
    }
}

void EnemyAI::Shutdown() {
    StopMovement();
    ClearTarget();
}

// =============================================================================
// State Management
// =============================================================================

void EnemyAI::SetState(AIState state) {
    if (state == m_currentState) {
        return;
    }

    AIState oldState = m_currentState;

    // Exit current state
    ExitState(m_currentState);

    // Transition
    m_previousState = m_currentState;
    m_currentState = state;
    m_stateTime = 0.0f;

    // Enter new state
    EnterState(state);

    // Notify callback
    if (m_onStateChange) {
        m_onStateChange(oldState, state);
    }
}

void EnemyAI::EnterState(AIState state) {
    switch (state) {
        case AIState::Idle:
            StopMovement();
            break;

        case AIState::Patrol:
            if (!m_patrolPoints.empty()) {
                MoveTo(GetNextPatrolPoint());
            }
            break;

        case AIState::Chase:
            if (HasTarget()) {
                MoveTo(m_currentTarget.lastKnownPosition);
            }
            break;

        case AIState::Attack:
            StopMovement();
            SelectBestAbility();
            break;

        case AIState::Retreat:
            MoveTo(FindRetreatPosition());
            break;

        case AIState::Dead:
            StopMovement();
            ClearTarget();
            break;
    }
}

void EnemyAI::ExitState(AIState state) {
    // Cleanup for exiting states
    switch (state) {
        case AIState::Attack:
            m_selectedAbility.clear();
            break;
        default:
            break;
    }
}

// =============================================================================
// State Update Functions
// =============================================================================

void EnemyAI::UpdateIdle(float deltaTime) {
    // Check for targets
    if (HasTarget() && m_currentTarget.isVisible) {
        SetState(AIState::Chase);
        return;
    }

    // Start patrolling after idle timeout
    if (m_stateTime > 2.0f && !m_patrolPoints.empty()) {
        SetState(AIState::Patrol);
    }
}

void EnemyAI::UpdatePatrol(float deltaTime) {
    // Check for targets - higher priority
    if (HasTarget() && m_currentTarget.isVisible) {
        SetState(AIState::Chase);
        return;
    }

    // Check if reached patrol point
    if (HasReachedDestination()) {
        // Wait briefly at patrol point
        if (m_stateTime > 1.0f) {
            m_currentPatrolIndex++;

            if (m_currentPatrolIndex >= m_patrolPoints.size()) {
                if (m_patrolLoop) {
                    m_currentPatrolIndex = 0;
                } else {
                    SetState(AIState::Idle);
                    return;
                }
            }

            MoveTo(m_patrolPoints[m_currentPatrolIndex]);
            m_stateTime = 0.0f;
        }
    }
}

void EnemyAI::UpdateChase(float deltaTime) {
    // Check retreat condition
    if (ShouldRetreat()) {
        SetState(AIState::Retreat);
        return;
    }

    // Lost target?
    if (!HasTarget()) {
        SetState(AIState::Patrol);
        return;
    }

    // Update movement toward target
    if (m_currentTarget.isVisible) {
        MoveTo(m_currentTarget.lastKnownPosition);
    }

    // Check if in attack range
    auto* lifecycle = LifecycleManager::GetInstance();
    if (lifecycle) {
        glm::vec3 myPos = lifecycle->GetEntityPosition(m_ownerEntity);
        float distance = glm::distance(myPos, m_currentTarget.lastKnownPosition);

        if (distance <= m_combat.attackRange) {
            SetState(AIState::Attack);
            return;
        }
    }

    // Target memory expired?
    float timeSinceSeen = Nova::Engine::Instance().GetTime().GetTotalTime()
                         - m_currentTarget.lastSeenTime;
    if (!m_currentTarget.isVisible && timeSinceSeen > m_perception.memoryDuration) {
        ClearTarget();
        SetState(AIState::Patrol);
    }
}

void EnemyAI::UpdateAttack(float deltaTime) {
    // Check retreat condition
    if (ShouldRetreat()) {
        SetState(AIState::Retreat);
        return;
    }

    // Lost target?
    if (!HasTarget() || !m_currentTarget.isVisible) {
        SetState(AIState::Chase);
        return;
    }

    // Check range
    auto* lifecycle = LifecycleManager::GetInstance();
    if (lifecycle) {
        glm::vec3 myPos = lifecycle->GetEntityPosition(m_ownerEntity);
        float distance = glm::distance(myPos, m_currentTarget.lastKnownPosition);

        // Target moved out of range
        if (distance > m_combat.attackRange * 1.2f) {
            SetState(AIState::Chase);
            return;
        }

        // Face target
        glm::vec3 toTarget = glm::normalize(m_currentTarget.lastKnownPosition - myPos);
        float targetRotation = atan2(toTarget.x, toTarget.z);
        lifecycle->SetEntityRotation(m_ownerEntity, targetRotation);
    }

    // Use ability when ready
    if (m_attackCooldown <= 0.0f && !m_selectedAbility.empty()) {
        UseAbility(m_selectedAbility);
        SelectBestAbility(); // Select next ability
    }
}

void EnemyAI::UpdateRetreat(float deltaTime) {
    // Check if safe to stop retreating
    auto* lifecycle = LifecycleManager::GetInstance();
    if (lifecycle) {
        float healthPercent = lifecycle->GetEntityHealthPercent(m_ownerEntity);

        // Recovered enough health or reached safe distance
        if (healthPercent > m_combat.retreatThreshold * 1.5f || HasReachedDestination()) {
            if (HasTarget() && m_currentTarget.isVisible) {
                SetState(AIState::Chase);
            } else {
                SetState(AIState::Patrol);
            }
            return;
        }
    }

    // Keep retreating, find new position if needed
    if (HasReachedDestination()) {
        MoveTo(FindRetreatPosition());
    }
}

// =============================================================================
// Perception System
// =============================================================================

void EnemyAI::UpdatePerception(float deltaTime) {
    m_perceptionTimer += deltaTime;

    if (m_perceptionTimer < m_perception.updateInterval) {
        return;
    }
    m_perceptionTimer = 0.0f;

    // Find visible enemies
    auto visibleEnemies = FindVisibleEnemies();

    // Update known targets
    float currentTime = Nova::Engine::Instance().GetTime().GetTotalTime();

    for (auto& target : m_knownTargets) {
        target.isVisible = false;
    }

    for (uint64_t entityId : visibleEnemies) {
        auto it = std::find_if(m_knownTargets.begin(), m_knownTargets.end(),
            [entityId](const TargetInfo& t) { return t.entityId == entityId; });

        auto* lifecycle = LifecycleManager::GetInstance();
        glm::vec3 entityPos = lifecycle ? lifecycle->GetEntityPosition(entityId) : glm::vec3(0);

        if (it != m_knownTargets.end()) {
            // Update existing target
            it->lastKnownVelocity = entityPos - it->lastKnownPosition;
            it->lastKnownPosition = entityPos;
            it->lastSeenTime = currentTime;
            it->isVisible = true;
            it->threatLevel = CalculateThreatLevel(entityId);
        } else {
            // New target
            TargetInfo newTarget;
            newTarget.entityId = entityId;
            newTarget.lastKnownPosition = entityPos;
            newTarget.lastSeenTime = currentTime;
            newTarget.isVisible = true;
            newTarget.threatLevel = CalculateThreatLevel(entityId);
            m_knownTargets.push_back(newTarget);

            if (m_onTargetAcquired) {
                m_onTargetAcquired(entityId);
            }
        }
    }

    // Remove old targets
    m_knownTargets.erase(
        std::remove_if(m_knownTargets.begin(), m_knownTargets.end(),
            [currentTime, this](const TargetInfo& t) {
                return !t.isVisible &&
                       (currentTime - t.lastSeenTime) > m_perception.memoryDuration;
            }),
        m_knownTargets.end());

    // Select highest threat target
    if (!m_knownTargets.empty()) {
        auto bestTarget = std::max_element(m_knownTargets.begin(), m_knownTargets.end(),
            [](const TargetInfo& a, const TargetInfo& b) {
                return a.threatLevel < b.threatLevel;
            });

        if (m_currentTarget.entityId != bestTarget->entityId) {
            if (m_currentTarget.entityId != 0 && m_onTargetLost) {
                m_onTargetLost(m_currentTarget.entityId);
            }
            m_currentTarget = *bestTarget;
            if (m_onTargetAcquired) {
                m_onTargetAcquired(m_currentTarget.entityId);
            }
        } else {
            m_currentTarget = *bestTarget;
        }
    }
}

bool EnemyAI::CanSeeEntity(uint64_t entityId) const {
    auto* lifecycle = LifecycleManager::GetInstance();
    if (!lifecycle) return false;

    glm::vec3 myPos = lifecycle->GetEntityPosition(m_ownerEntity);
    glm::vec3 targetPos = lifecycle->GetEntityPosition(entityId);
    glm::vec3 myForward = lifecycle->GetEntityForward(m_ownerEntity);

    // Distance check
    float distance = glm::distance(myPos, targetPos);
    if (distance > m_perception.sightRange) {
        return false;
    }

    // Angle check
    glm::vec3 toTarget = glm::normalize(targetPos - myPos);
    float angle = acos(glm::dot(myForward, toTarget)) * 180.0f / 3.14159f;
    if (angle > m_perception.sightAngle / 2.0f) {
        return false;
    }

    // Line of sight check (raycast)
    Nova::Ray ray{myPos + glm::vec3(0, 1.5f, 0), toTarget};
    auto& spatial = Nova::Engine::Instance().GetSpatialIndex();

    auto hit = spatial.Raycast(ray, distance);
    if (hit && hit->entityId != entityId) {
        return false; // Blocked by something else
    }

    return true;
}

float EnemyAI::CalculateThreatLevel(uint64_t entityId) const {
    auto* lifecycle = LifecycleManager::GetInstance();
    if (!lifecycle) return 0.0f;

    float threat = 0.0f;

    // Distance factor (closer = more threatening)
    glm::vec3 myPos = lifecycle->GetEntityPosition(m_ownerEntity);
    glm::vec3 targetPos = lifecycle->GetEntityPosition(entityId);
    float distance = glm::distance(myPos, targetPos);
    threat += (1.0f - distance / m_perception.sightRange) * 50.0f;

    // Health factor (lower health targets are less threatening)
    float targetHealth = lifecycle->GetEntityHealthPercent(entityId);
    threat += (1.0f - targetHealth) * 20.0f;

    // Recent damage factor
    // (would check damage history here)

    return threat;
}

std::vector<uint64_t> EnemyAI::FindVisibleEnemies() const {
    std::vector<uint64_t> visible;

    auto* lifecycle = LifecycleManager::GetInstance();
    if (!lifecycle) return visible;

    glm::vec3 myPos = lifecycle->GetEntityPosition(m_ownerEntity);

    // Query spatial index for nearby entities
    Nova::AABB searchArea{
        myPos - glm::vec3(m_perception.sightRange),
        myPos + glm::vec3(m_perception.sightRange)
    };

    auto& spatial = Nova::Engine::Instance().GetSpatialIndex();
    auto nearby = spatial.Query(searchArea);

    for (uint64_t entityId : nearby) {
        if (entityId == m_ownerEntity) continue;

        // Check if entity is an enemy
        if (!lifecycle->IsEnemy(m_ownerEntity, entityId)) continue;

        // Check visibility
        if (CanSeeEntity(entityId)) {
            visible.push_back(entityId);
        }
    }

    return visible;
}

// =============================================================================
// Combat
// =============================================================================

void EnemyAI::SelectBestAbility() {
    if (m_combat.abilities.empty()) {
        m_selectedAbility = "BasicAttack";
        return;
    }

    auto* lifecycle = LifecycleManager::GetInstance();
    if (!lifecycle || !HasTarget()) {
        m_selectedAbility = m_combat.abilities[0];
        return;
    }

    glm::vec3 myPos = lifecycle->GetEntityPosition(m_ownerEntity);
    float distance = glm::distance(myPos, m_currentTarget.lastKnownPosition);

    // Select ability based on distance and cooldowns
    for (const auto& ability : m_combat.abilities) {
        auto* abilityData = lifecycle->GetAbilityData(ability);
        if (!abilityData) continue;

        if (abilityData->range >= distance &&
            lifecycle->IsAbilityReady(m_ownerEntity, ability)) {
            m_selectedAbility = ability;
            return;
        }
    }

    m_selectedAbility = m_combat.abilities[0];
}

void EnemyAI::UseAbility(const std::string& abilityId) {
    auto* lifecycle = LifecycleManager::GetInstance();
    if (!lifecycle) return;

    bool success = lifecycle->UseAbility(m_ownerEntity, abilityId, m_currentTarget.entityId);

    if (success) {
        auto* abilityData = lifecycle->GetAbilityData(abilityId);
        m_attackCooldown = abilityData ? abilityData->cooldown : 1.0f;
    }
}

bool EnemyAI::ShouldRetreat() const {
    auto* lifecycle = LifecycleManager::GetInstance();
    if (!lifecycle) return false;

    float healthPercent = lifecycle->GetEntityHealthPercent(m_ownerEntity);

    // Base retreat on health and aggression
    float retreatChance = (1.0f - m_combat.aggressionLevel) *
                          (1.0f - healthPercent / m_combat.retreatThreshold);

    return healthPercent < m_combat.retreatThreshold && retreatChance > 0.5f;
}

glm::vec3 EnemyAI::FindRetreatPosition() const {
    auto* lifecycle = LifecycleManager::GetInstance();
    if (!lifecycle) return glm::vec3(0);

    glm::vec3 myPos = lifecycle->GetEntityPosition(m_ownerEntity);
    glm::vec3 retreatDir;

    if (HasTarget()) {
        // Run away from target
        retreatDir = glm::normalize(myPos - m_currentTarget.lastKnownPosition);
    } else {
        // Random direction
        float angle = static_cast<float>(rand()) / RAND_MAX * 6.28318f;
        retreatDir = glm::vec3(cos(angle), 0, sin(angle));
    }

    return myPos + retreatDir * 15.0f;
}

// =============================================================================
// Debug Visualization
// =============================================================================

void EnemyAI::DrawDebug() const {
    auto* lifecycle = LifecycleManager::GetInstance();
    if (!lifecycle) return;

    glm::vec3 myPos = lifecycle->GetEntityPosition(m_ownerEntity);
    myPos.y += 2.0f; // Above entity

    // State text
    const char* stateNames[] = {"Idle", "Patrol", "Chase", "Attack", "Retreat", "Dead"};
    Nova::DebugDraw::Text(myPos, stateNames[static_cast<int>(m_currentState)]);

    // Sight cone
    glm::vec3 forward = lifecycle->GetEntityForward(m_ownerEntity);
    float halfAngle = m_perception.sightAngle * 0.5f * 3.14159f / 180.0f;

    glm::vec3 leftDir = glm::vec3(
        forward.x * cos(halfAngle) - forward.z * sin(halfAngle),
        0,
        forward.x * sin(halfAngle) + forward.z * cos(halfAngle)
    );
    glm::vec3 rightDir = glm::vec3(
        forward.x * cos(-halfAngle) - forward.z * sin(-halfAngle),
        0,
        forward.x * sin(-halfAngle) + forward.z * cos(-halfAngle)
    );

    glm::vec3 basePos = myPos - glm::vec3(0, 1, 0);
    Nova::DebugDraw::Line(basePos, basePos + leftDir * m_perception.sightRange,
                          glm::vec3(1, 1, 0));
    Nova::DebugDraw::Line(basePos, basePos + rightDir * m_perception.sightRange,
                          glm::vec3(1, 1, 0));

    // Target indicator
    if (HasTarget()) {
        glm::vec3 color = m_currentTarget.isVisible ? glm::vec3(1, 0, 0) : glm::vec3(1, 0.5, 0);
        Nova::DebugDraw::Line(basePos, m_currentTarget.lastKnownPosition, color);
        Nova::DebugDraw::Sphere(m_currentTarget.lastKnownPosition, 0.5f, color);
    }

    // Patrol path
    if (!m_patrolPoints.empty()) {
        for (size_t i = 0; i < m_patrolPoints.size(); i++) {
            size_t next = (i + 1) % m_patrolPoints.size();
            Nova::DebugDraw::Line(m_patrolPoints[i], m_patrolPoints[next],
                                  glm::vec3(0, 0, 1));
            Nova::DebugDraw::Sphere(m_patrolPoints[i], 0.3f,
                                    i == m_currentPatrolIndex ? glm::vec3(0, 1, 0) : glm::vec3(0, 0, 1));
        }
    }
}

} // namespace Vehement
```

## Part 2: Creating a Behavior Tree Alternative

For more complex AI, you can use behavior trees instead of state machines:

### Step 1: Define Behavior Tree Nodes

```cpp
// game/src/ai/BehaviorTree.hpp
#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace Vehement::AI {

/**
 * @brief Result of behavior tree node execution
 */
enum class NodeStatus {
    Success,    ///< Node completed successfully
    Failure,    ///< Node failed
    Running     ///< Node still executing
};

/**
 * @brief Blackboard for sharing data between nodes
 */
class Blackboard {
public:
    template<typename T>
    void Set(const std::string& key, const T& value);

    template<typename T>
    T Get(const std::string& key, const T& defaultValue = T{}) const;

    bool Has(const std::string& key) const;
    void Remove(const std::string& key);
    void Clear();

private:
    std::unordered_map<std::string, std::any> m_data;
};

/**
 * @brief Base class for all behavior tree nodes
 */
class BTNode {
public:
    virtual ~BTNode() = default;
    virtual NodeStatus Execute(Blackboard& blackboard) = 0;
    virtual void Reset() {}

    void SetName(const std::string& name) { m_name = name; }
    const std::string& GetName() const { return m_name; }

protected:
    std::string m_name;
};

/**
 * @brief Composite node that succeeds if all children succeed
 */
class Sequence : public BTNode {
public:
    void AddChild(std::unique_ptr<BTNode> child);
    NodeStatus Execute(Blackboard& blackboard) override;
    void Reset() override;

private:
    std::vector<std::unique_ptr<BTNode>> m_children;
    size_t m_currentChild = 0;
};

/**
 * @brief Composite node that succeeds if any child succeeds
 */
class Selector : public BTNode {
public:
    void AddChild(std::unique_ptr<BTNode> child);
    NodeStatus Execute(Blackboard& blackboard) override;
    void Reset() override;

private:
    std::vector<std::unique_ptr<BTNode>> m_children;
    size_t m_currentChild = 0;
};

/**
 * @brief Decorator that inverts child result
 */
class Inverter : public BTNode {
public:
    explicit Inverter(std::unique_ptr<BTNode> child);
    NodeStatus Execute(Blackboard& blackboard) override;

private:
    std::unique_ptr<BTNode> m_child;
};

/**
 * @brief Decorator that repeats child N times
 */
class Repeater : public BTNode {
public:
    Repeater(std::unique_ptr<BTNode> child, int count = -1);
    NodeStatus Execute(Blackboard& blackboard) override;
    void Reset() override;

private:
    std::unique_ptr<BTNode> m_child;
    int m_targetCount;
    int m_currentCount = 0;
};

/**
 * @brief Leaf node that executes a condition
 */
class Condition : public BTNode {
public:
    using ConditionFunc = std::function<bool(Blackboard&)>;
    explicit Condition(ConditionFunc func);
    NodeStatus Execute(Blackboard& blackboard) override;

private:
    ConditionFunc m_func;
};

/**
 * @brief Leaf node that executes an action
 */
class Action : public BTNode {
public:
    using ActionFunc = std::function<NodeStatus(Blackboard&)>;
    explicit Action(ActionFunc func);
    NodeStatus Execute(Blackboard& blackboard) override;

private:
    ActionFunc m_func;
};

} // namespace Vehement::AI
```

### Step 2: Build an Enemy Behavior Tree

```cpp
// game/src/ai/EnemyBehaviorTree.cpp
#include "BehaviorTree.hpp"
#include "EnemyAI.hpp"

namespace Vehement::AI {

std::unique_ptr<BTNode> CreateEnemyBehavior(EnemyAI* ai) {
    auto root = std::make_unique<Selector>();
    root->SetName("Root");

    // Priority 1: Handle death
    auto isDead = std::make_unique<Sequence>();
    isDead->SetName("Death Handler");
    isDead->AddChild(std::make_unique<Condition>([ai](Blackboard& bb) {
        return ai->GetState() == AIState::Dead;
    }));
    isDead->AddChild(std::make_unique<Action>([](Blackboard& bb) {
        return NodeStatus::Running; // Stay dead
    }));
    root->AddChild(std::move(isDead));

    // Priority 2: Retreat when low health
    auto retreat = std::make_unique<Sequence>();
    retreat->SetName("Retreat");
    retreat->AddChild(std::make_unique<Condition>([ai](Blackboard& bb) {
        auto* lifecycle = LifecycleManager::GetInstance();
        if (!lifecycle) return false;
        float health = lifecycle->GetEntityHealthPercent(ai->GetOwnerEntity());
        return health < ai->GetCombatConfig().retreatThreshold;
    }));
    retreat->AddChild(std::make_unique<Action>([ai](Blackboard& bb) {
        ai->SetState(AIState::Retreat);
        return NodeStatus::Success;
    }));
    root->AddChild(std::move(retreat));

    // Priority 3: Attack if target in range
    auto attack = std::make_unique<Sequence>();
    attack->SetName("Attack");
    attack->AddChild(std::make_unique<Condition>([ai](Blackboard& bb) {
        if (!ai->HasTarget()) return false;
        auto* lifecycle = LifecycleManager::GetInstance();
        if (!lifecycle) return false;

        glm::vec3 myPos = lifecycle->GetEntityPosition(ai->GetOwnerEntity());
        glm::vec3 targetPos = ai->GetCurrentTarget().lastKnownPosition;
        float dist = glm::distance(myPos, targetPos);
        return dist <= ai->GetCombatConfig().attackRange;
    }));
    attack->AddChild(std::make_unique<Action>([ai](Blackboard& bb) {
        ai->SetState(AIState::Attack);
        return NodeStatus::Success;
    }));
    root->AddChild(std::move(attack));

    // Priority 4: Chase visible target
    auto chase = std::make_unique<Sequence>();
    chase->SetName("Chase");
    chase->AddChild(std::make_unique<Condition>([ai](Blackboard& bb) {
        return ai->HasTarget() && ai->GetCurrentTarget().isVisible;
    }));
    chase->AddChild(std::make_unique<Action>([ai](Blackboard& bb) {
        ai->SetState(AIState::Chase);
        return NodeStatus::Success;
    }));
    root->AddChild(std::move(chase));

    // Priority 5: Patrol
    auto patrol = std::make_unique<Action>([ai](Blackboard& bb) {
        ai->SetState(AIState::Patrol);
        return NodeStatus::Success;
    });
    patrol->SetName("Patrol");
    root->AddChild(std::move(patrol));

    return root;
}

} // namespace Vehement::AI
```

## Part 3: Python Scripting for AI

You can also implement AI behaviors in Python for rapid iteration:

### Step 1: Create the Python AI Script

```python
# scripts/ai/advanced_enemy.py

from nova import entities, transform, events, time, math, debug
from enum import Enum

class AIState(Enum):
    IDLE = 0
    PATROL = 1
    CHASE = 2
    ATTACK = 3
    RETREAT = 4
    DEAD = 5

class AdvancedEnemyAI:
    """Advanced enemy AI with perception and combat behaviors."""

    def __init__(self, entity_id):
        self.entity = entity_id
        self.state = AIState.IDLE
        self.state_time = 0.0

        # Perception
        self.sight_range = 15.0
        self.sight_angle = 120.0
        self.hearing_range = 8.0
        self.memory_duration = 5.0

        # Combat
        self.attack_range = 2.0
        self.retreat_threshold = 0.25
        self.aggression = 0.7
        self.attack_cooldown = 0.0

        # Targeting
        self.target = None
        self.last_target_pos = None
        self.last_seen_time = 0.0

        # Patrol
        self.patrol_points = []
        self.patrol_index = 0

        # Subscribe to events
        events.subscribe("OnDamage", self.on_damage)

    def update(self, dt):
        """Main update function called each frame."""
        if self.state == AIState.DEAD:
            return

        self.state_time += dt
        self.attack_cooldown = max(0, self.attack_cooldown - dt)

        # Update perception
        self._update_perception()

        # State machine
        if self.state == AIState.IDLE:
            self._update_idle(dt)
        elif self.state == AIState.PATROL:
            self._update_patrol(dt)
        elif self.state == AIState.CHASE:
            self._update_chase(dt)
        elif self.state == AIState.ATTACK:
            self._update_attack(dt)
        elif self.state == AIState.RETREAT:
            self._update_retreat(dt)

    def set_state(self, new_state):
        """Transition to a new state."""
        if new_state == self.state:
            return

        old_state = self.state
        self._exit_state(old_state)

        self.state = new_state
        self.state_time = 0.0

        self._enter_state(new_state)

        debug.log(f"Enemy {self.entity}: {old_state.name} -> {new_state.name}")

    def _enter_state(self, state):
        """Handle state entry."""
        if state == AIState.PATROL:
            if self.patrol_points:
                self._move_to(self.patrol_points[self.patrol_index])
        elif state == AIState.CHASE:
            if self.last_target_pos:
                self._move_to(self.last_target_pos)
        elif state == AIState.RETREAT:
            retreat_pos = self._find_retreat_position()
            self._move_to(retreat_pos)

    def _exit_state(self, state):
        """Handle state exit."""
        pass

    # =========================================================================
    # State Updates
    # =========================================================================

    def _update_idle(self, dt):
        if self.target and self._can_see_target():
            self.set_state(AIState.CHASE)
            return

        if self.state_time > 2.0 and self.patrol_points:
            self.set_state(AIState.PATROL)

    def _update_patrol(self, dt):
        if self.target and self._can_see_target():
            self.set_state(AIState.CHASE)
            return

        if self._has_reached_destination():
            self.patrol_index = (self.patrol_index + 1) % len(self.patrol_points)
            self._move_to(self.patrol_points[self.patrol_index])

    def _update_chase(self, dt):
        if self._should_retreat():
            self.set_state(AIState.RETREAT)
            return

        if not self.target:
            self.set_state(AIState.PATROL)
            return

        # Update movement
        if self._can_see_target():
            target_pos = transform.get_position(self.target)
            self.last_target_pos = target_pos
            self.last_seen_time = time.total_time()
            self._move_to(target_pos)

        # Check attack range
        my_pos = transform.get_position(self.entity)
        if self.last_target_pos:
            dist = math.distance(my_pos, self.last_target_pos)
            if dist <= self.attack_range:
                self.set_state(AIState.ATTACK)
                return

        # Memory timeout
        if time.total_time() - self.last_seen_time > self.memory_duration:
            self.target = None
            self.set_state(AIState.PATROL)

    def _update_attack(self, dt):
        if self._should_retreat():
            self.set_state(AIState.RETREAT)
            return

        if not self.target or not self._can_see_target():
            self.set_state(AIState.CHASE)
            return

        # Face target
        my_pos = transform.get_position(self.entity)
        target_pos = transform.get_position(self.target)
        transform.look_at(self.entity, target_pos)

        # Attack when ready
        if self.attack_cooldown <= 0:
            self._perform_attack()
            self.attack_cooldown = 1.0

    def _update_retreat(self, dt):
        health = self._get_health_percent()

        if health > self.retreat_threshold * 1.5 or self._has_reached_destination():
            if self.target and self._can_see_target():
                self.set_state(AIState.CHASE)
            else:
                self.set_state(AIState.PATROL)

    # =========================================================================
    # Perception
    # =========================================================================

    def _update_perception(self):
        """Scan for enemies."""
        my_pos = transform.get_position(self.entity)
        my_forward = transform.get_forward(self.entity)

        # Find all players
        players = entities.find_all("Player")

        best_target = None
        best_threat = 0

        for player in players:
            if not entities.exists(player):
                continue

            player_pos = transform.get_position(player)

            # Distance check
            dist = math.distance(my_pos, player_pos)
            if dist > self.sight_range:
                continue

            # Angle check
            to_player = math.normalize([
                player_pos[0] - my_pos[0],
                player_pos[1] - my_pos[1],
                player_pos[2] - my_pos[2]
            ])
            dot = math.dot(my_forward, to_player)
            angle = math.acos(dot) * 180 / 3.14159

            if angle > self.sight_angle / 2:
                continue

            # Calculate threat
            threat = (1 - dist / self.sight_range) * 100

            if threat > best_threat:
                best_threat = threat
                best_target = player

        if best_target and best_target != self.target:
            self.target = best_target
            self.last_target_pos = transform.get_position(best_target)
            self.last_seen_time = time.total_time()
            debug.log(f"Enemy {self.entity}: Acquired target {best_target}")

    def _can_see_target(self):
        """Check if current target is visible."""
        if not self.target or not entities.exists(self.target):
            return False

        my_pos = transform.get_position(self.entity)
        target_pos = transform.get_position(self.target)

        dist = math.distance(my_pos, target_pos)
        if dist > self.sight_range:
            return False

        # TODO: Add line of sight check
        return True

    # =========================================================================
    # Combat
    # =========================================================================

    def _perform_attack(self):
        """Execute attack on target."""
        if not self.target:
            return

        events.publish("OnAttack", {
            "attacker": self.entity,
            "target": self.target,
            "damage": 10,
            "type": "melee"
        })

        debug.log(f"Enemy {self.entity}: Attacking {self.target}")

    def _should_retreat(self):
        """Check if should retreat based on health."""
        health = self._get_health_percent()
        retreat_chance = (1 - self.aggression) * (1 - health / self.retreat_threshold)
        return health < self.retreat_threshold and retreat_chance > 0.5

    def _find_retreat_position(self):
        """Find position to retreat to."""
        my_pos = transform.get_position(self.entity)

        if self.last_target_pos:
            # Run away from target
            dx = my_pos[0] - self.last_target_pos[0]
            dz = my_pos[2] - self.last_target_pos[2]
            length = (dx*dx + dz*dz) ** 0.5
            if length > 0:
                dx /= length
                dz /= length
        else:
            # Random direction
            import random
            angle = random.random() * 6.28
            dx = math.cos(angle)
            dz = math.sin(angle)

        return [my_pos[0] + dx * 15, my_pos[1], my_pos[2] + dz * 15]

    # =========================================================================
    # Helpers
    # =========================================================================

    def _move_to(self, position):
        """Move entity toward position."""
        # This would integrate with navigation system
        pass

    def _has_reached_destination(self):
        """Check if reached movement destination."""
        return True  # Simplified

    def _get_health_percent(self):
        """Get current health as percentage."""
        health = entities.get_component(self.entity, "HealthComponent")
        if health:
            return health["current"] / health["max"]
        return 1.0

    def on_damage(self, event):
        """Handle damage taken."""
        if event["target"] != self.entity:
            return

        # Remember attacker
        if "attacker" in event:
            self.target = event["attacker"]
            self.last_target_pos = transform.get_position(event["attacker"])
            self.last_seen_time = time.total_time()

        # Check for death
        if self._get_health_percent() <= 0:
            self.set_state(AIState.DEAD)

# ============================================================================
# AI Manager
# ============================================================================

_ai_instances = {}

def spawn_enemy(entity_id, patrol_points=None):
    """Create AI for spawned enemy."""
    ai = AdvancedEnemyAI(entity_id)
    if patrol_points:
        ai.patrol_points = patrol_points
    _ai_instances[entity_id] = ai
    return ai

def update_all(dt):
    """Update all AI instances."""
    for entity_id, ai in list(_ai_instances.items()):
        if not entities.exists(entity_id):
            del _ai_instances[entity_id]
            continue
        ai.update(dt)

def destroy_enemy(entity_id):
    """Remove AI instance."""
    if entity_id in _ai_instances:
        del _ai_instances[entity_id]
```

## Part 4: Coordinated Group AI

For coordinating multiple AI units:

```cpp
// game/src/ai/AISquad.hpp
#pragma once

#include "EnemyAI.hpp"
#include <vector>

namespace Vehement {

/**
 * @brief Coordinates a group of AI units
 */
class AISquad {
public:
    void AddMember(EnemyAI* ai);
    void RemoveMember(EnemyAI* ai);

    void Update(float deltaTime);

    // Coordination commands
    void SetSquadTarget(uint64_t targetId);
    void FormUp(const glm::vec3& position);
    void Attack(const glm::vec3& position);
    void Retreat();

    // Formation
    enum class Formation { Line, Circle, Wedge, Scatter };
    void SetFormation(Formation formation);
    glm::vec3 GetFormationPosition(size_t memberIndex) const;

private:
    std::vector<EnemyAI*> m_members;
    uint64_t m_squadTarget = 0;
    glm::vec3 m_squadPosition{0};
    Formation m_formation = Formation::Line;
    float m_formationSpacing = 2.0f;
};

} // namespace Vehement
```

## Summary

In this tutorial, you learned how to:

1. **Create a state machine AI** with idle, patrol, chase, attack, and retreat states
2. **Implement perception** with sight cones and line-of-sight checks
3. **Build combat behaviors** with ability selection and threat assessment
4. **Use behavior trees** for more complex decision-making
5. **Script AI in Python** for rapid iteration
6. **Coordinate group AI** with squad behaviors

## Next Steps

- Add pathfinding integration with the navigation system
- Implement more sophisticated perception (hearing, alerts)
- Create specialized AI types (ranged, support, boss)
- Add learning behaviors that adapt to player tactics
- Implement AI debugging tools in the editor

## See Also

- [Scripting Guide](../SCRIPTING_GUIDE.md)
- [Scripting API Reference](../api/Scripting.md)
- [Animation Guide](../ANIMATION_GUIDE.md) - For AI animation integration
