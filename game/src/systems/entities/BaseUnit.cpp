#include "BaseUnit.hpp"
#include "../lifecycle/ObjectFactory.hpp"
#include <algorithm>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// AIComponent Implementation
// ============================================================================

void AIComponent::OnInitialize() {
    ComponentBase<AIComponent>::OnInitialize();
}

void AIComponent::OnTick(float deltaTime) {
    // Update attack cooldown
    if (attackCooldown > 0.0f) {
        attackCooldown -= deltaTime;
    }
}

bool AIComponent::OnEvent(const GameEvent& event) {
    if (event.type == EventType::Damaged) {
        // React to damage - acquire attacker as target if no target
        if (!target.IsValid()) {
            target = event.source;
        }
        return true;
    }
    return false;
}

// ============================================================================
// CombatComponent Implementation
// ============================================================================

void CombatComponent::OnInitialize() {
    ComponentBase<CombatComponent>::OnInitialize();

    // Get required components
    // Note: In a full implementation, this would use proper component lookup
}

void CombatComponent::OnTick(float deltaTime) {
    if (attackTimer > 0.0f) {
        attackTimer -= deltaTime;
    }

    if (isAttacking && currentTarget.IsValid()) {
        if (attackTimer <= 0.0f && stats) {
            // Perform attack
            float attackInterval = stats->attackSpeed > 0.0f ?
                                   1.0f / stats->attackSpeed : 1.0f;
            attackTimer = attackInterval;

            // Fire attack event
            DamageEventData data;
            data.amount = stats->attackDamage * stats->damageMultiplier;
            data.sourceHandle = GetOwner();
            data.targetHandle = currentTarget;

            GameEvent attackEvent(EventType::AttackLanded, GetOwner(), currentTarget);
            attackEvent.SetData(data);
            QueueEvent(std::move(attackEvent));
        }
    }
}

bool CombatComponent::OnEvent(const GameEvent& event) {
    if (event.type == EventType::Killed && event.target == GetOwner()) {
        isAttacking = false;
        currentTarget = LifecycleHandle::Invalid;
        return true;
    }
    return false;
}

bool CombatComponent::CanAttack() const {
    return stats != nullptr && stats->attackDamage > 0.0f;
}

void CombatComponent::StartAttack(LifecycleHandle target) {
    if (!CanAttack()) return;

    currentTarget = target;
    isAttacking = true;

    GameEvent event(EventType::AttackStarted, GetOwner(), target);
    QueueEvent(std::move(event));
}

void CombatComponent::CancelAttack() {
    isAttacking = false;
    currentTarget = LifecycleHandle::Invalid;
}

// ============================================================================
// BaseUnit Implementation
// ============================================================================

BaseUnit::BaseUnit() {
    // Add default components
    m_components.Add<TransformComponent>();
    m_components.Add<HealthComponent>();
    m_components.Add<MovementComponent>();
    m_components.Add<AIComponent>();
    m_components.Add<CombatComponent>();
}

BaseUnit::~BaseUnit() = default;

void BaseUnit::OnCreate(const nlohmann::json& config) {
    ScriptedLifecycle::OnCreate(config);

    // Set component owner
    m_components.SetOwner(GetHandle());

    // Parse unit-specific config
    ParseUnitConfig(config);

    // Initialize components
    m_components.InitializeAll();

    // Link combat component to stats
    if (auto* combat = m_components.Get<CombatComponent>()) {
        combat->stats = &m_stats;
    }

    // Apply stats to health component
    if (auto* health = m_components.Get<HealthComponent>()) {
        health->maxHealth = m_stats.maxHealth;
        health->health = m_stats.maxHealth;
        health->armor = m_stats.armor * m_stats.armorMultiplier;
    }

    // Apply stats to movement component
    if (auto* movement = m_components.Get<MovementComponent>()) {
        movement->maxSpeed = m_stats.moveSpeed * m_stats.speedMultiplier;
    }

    // Fire spawned event
    GameEvent event(EventType::Spawned, GetHandle());
    QueueEvent(std::move(event));
}

void BaseUnit::OnTick(float deltaTime) {
    ScriptedLifecycle::OnTick(deltaTime);

    // Skip if dead
    if (m_unitState == UnitState::Dead) return;

    // Check for death
    if (auto* health = GetHealth()) {
        if (!health->IsAlive() && m_unitState != UnitState::Dead) {
            OnDeath(LifecycleHandle::Invalid);
            return;
        }

        // Health regeneration
        if (m_stats.healthRegen > 0.0f) {
            health->Heal(m_stats.healthRegen * deltaTime);
        }
    }

    // Tick components
    m_components.TickAll(deltaTime);

    // Update systems
    UpdateAI(deltaTime);
    UpdateCombat(deltaTime);
    UpdateMovement(deltaTime);
}

bool BaseUnit::OnEvent(const GameEvent& event) {
    if (ScriptedLifecycle::OnEvent(event)) {
        return true;
    }

    // Forward to components
    if (m_components.SendEvent(event)) {
        return true;
    }

    // Handle unit-specific events
    switch (event.type) {
        case EventType::Damaged:
            // Already handled by health component
            break;

        case EventType::Killed:
            if (event.target == GetHandle()) {
                OnDeath(event.source);
                return true;
            }
            break;

        case EventType::AttackLanded:
            // We landed an attack
            if (event.source == GetHandle()) {
                // Apply damage to target
                DealDamage(event.target);
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

void BaseUnit::OnDestroy() {
    m_components.Clear();
    ScriptedLifecycle::OnDestroy();
}

void BaseUnit::SetUnitState(UnitState state) {
    if (m_unitState == state) return;

    UnitState oldState = m_unitState;
    m_unitState = state;

    OnStateChanged(oldState, state);

    // Fire state change event
    GameEvent event(EventType::StateChanged, GetHandle());
    QueueEvent(std::move(event));
}

void BaseUnit::AddExperience(int amount) {
    if (amount <= 0) return;

    m_experience += amount;

    // Simple level-up check (100 XP per level)
    int newLevel = 1 + m_experience / 100;
    while (newLevel > m_level) {
        m_level++;
        OnLevelUp(m_level);
    }
}

void BaseUnit::Attack(LifecycleHandle target) {
    if (auto* combat = m_components.Get<CombatComponent>()) {
        combat->StartAttack(target);
        SetUnitState(UnitState::Attacking);
    }

    if (auto* ai = m_components.Get<AIComponent>()) {
        ai->target = target;
    }
}

void BaseUnit::StopAttack() {
    if (auto* combat = m_components.Get<CombatComponent>()) {
        combat->CancelAttack();
    }

    if (m_unitState == UnitState::Attacking) {
        SetUnitState(UnitState::Idle);
    }
}

bool BaseUnit::IsInCombat() const {
    if (auto* combat = m_components.Get<CombatComponent>()) {
        return combat->isAttacking;
    }
    return false;
}

bool BaseUnit::CanAttackTarget(LifecycleHandle target) const {
    // Check range
    // In a full implementation, this would check target position
    return target.IsValid();
}

float BaseUnit::DealDamage(LifecycleHandle target) {
    float damage = m_stats.attackDamage * m_stats.damageMultiplier;

    DamageEventData data;
    data.amount = damage;
    data.sourceHandle = GetHandle();
    data.targetHandle = target;

    GameEvent event(EventType::Damaged, GetHandle(), target);
    event.SetData(data);

    // Dispatch to target
    auto& manager = GetGlobalLifecycleManager();
    manager.SendEvent(target, event);

    return damage;
}

void BaseUnit::MoveTo(const glm::vec3& position) {
    if (auto* ai = m_components.Get<AIComponent>()) {
        ai->targetPosition = position;
        ai->hasTargetPosition = true;
    }

    if (m_unitState != UnitState::Moving && m_unitState != UnitState::Attacking) {
        SetUnitState(UnitState::Moving);
    }
}

void BaseUnit::MoveInDirection(const glm::vec3& direction) {
    if (auto* movement = m_components.Get<MovementComponent>()) {
        float speed = m_stats.moveSpeed * m_stats.speedMultiplier;
        movement->SetTargetVelocity(glm::normalize(direction) * speed);
    }

    if (m_unitState != UnitState::Moving && m_unitState != UnitState::Attacking) {
        SetUnitState(UnitState::Moving);
    }
}

void BaseUnit::Stop() {
    if (auto* movement = m_components.Get<MovementComponent>()) {
        movement->Stop();
    }

    if (auto* ai = m_components.Get<AIComponent>()) {
        ai->hasTargetPosition = false;
    }

    if (m_unitState == UnitState::Moving) {
        SetUnitState(UnitState::Idle);
    }
}

bool BaseUnit::IsMoving() const {
    if (auto* movement = m_components.Get<MovementComponent>()) {
        return movement->GetSpeed() > 0.1f;
    }
    return false;
}

float BaseUnit::GetDistanceTo(const glm::vec3& position) const {
    if (auto* transform = m_components.Get<TransformComponent>()) {
        return glm::length(position - transform->position);
    }
    return 0.0f;
}

bool BaseUnit::IsAlly(const BaseUnit* other) const {
    return other && other->m_teamId == m_teamId;
}

bool BaseUnit::IsEnemy(const BaseUnit* other) const {
    return other && other->m_teamId != m_teamId;
}

ScriptContext BaseUnit::BuildContext() const {
    ScriptContext ctx = ScriptedLifecycle::BuildContext();

    ctx.entityType = "unit";

    if (auto* transform = m_components.Get<TransformComponent>()) {
        ctx.transform.x = transform->position.x;
        ctx.transform.y = transform->position.y;
        ctx.transform.z = transform->position.z;
        ctx.transform.rotY = transform->rotation.y;
    }

    if (auto* health = m_components.Get<HealthComponent>()) {
        ctx.health.current = health->health;
        ctx.health.max = health->maxHealth;
        ctx.health.armor = health->armor;
    }

    return ctx;
}

void BaseUnit::ParseUnitConfig(const nlohmann::json& config) {
    // In a full implementation, parse JSON config here
    // For now, use default values
    (void)config;
}

void BaseUnit::OnStateChanged(UnitState oldState, UnitState newState) {
    (void)oldState;
    (void)newState;
    // Override in derived classes
}

void BaseUnit::OnLevelUp(int newLevel) {
    // Scale stats with level
    float levelBonus = 1.0f + (newLevel - 1) * 0.1f;

    // Increase max health
    m_stats.maxHealth *= levelBonus;
    if (auto* health = GetHealth()) {
        float healthPercent = health->GetHealthPercent();
        health->maxHealth = m_stats.maxHealth;
        health->health = m_stats.maxHealth * healthPercent;
    }

    // Increase damage
    m_stats.attackDamage *= levelBonus;
}

void BaseUnit::OnDeath(LifecycleHandle killer) {
    SetUnitState(UnitState::Dead);

    // Fire killed event
    DamageEventData data;
    data.sourceHandle = killer;
    data.targetHandle = GetHandle();

    GameEvent event(EventType::Killed, killer, GetHandle());
    event.SetData(data);
    QueueEvent(std::move(event));

    // Schedule destruction
    auto& manager = GetGlobalLifecycleManager();
    manager.Destroy(GetHandle(), false);
}

void BaseUnit::UpdateAI(float deltaTime) {
    (void)deltaTime;

    auto* ai = m_components.Get<AIComponent>();
    if (!ai) return;

    // Simple AI behavior
    if (m_unitState == UnitState::Dead || m_unitState == UnitState::Stunned) {
        return;
    }

    // Move to target position
    if (ai->hasTargetPosition && !IsInCombat()) {
        auto* transform = GetTransform();
        if (transform) {
            float distance = glm::length(ai->targetPosition - transform->position);
            if (distance > 0.5f) {
                glm::vec3 direction = glm::normalize(ai->targetPosition - transform->position);
                MoveInDirection(direction);
            } else {
                ai->hasTargetPosition = false;
                Stop();
            }
        }
    }
}

void BaseUnit::UpdateCombat(float deltaTime) {
    (void)deltaTime;
    // Combat is handled by CombatComponent
}

void BaseUnit::UpdateMovement(float deltaTime) {
    (void)deltaTime;
    // Movement is handled by MovementComponent
}

// ============================================================================
// Factory Registration
// ============================================================================

namespace {
    struct BaseUnitRegistrar {
        BaseUnitRegistrar() {
            GetGlobalObjectFactory().RegisterType<BaseUnit>("unit");
        }
    };
    static BaseUnitRegistrar g_baseUnitRegistrar;
}

// Component registration
REGISTER_COMPONENT("ai", AIComponent)
REGISTER_COMPONENT("combat", CombatComponent)

} // namespace Lifecycle
} // namespace Vehement
