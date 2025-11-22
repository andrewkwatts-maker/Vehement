#pragma once

#include "../lifecycle/ILifecycle.hpp"
#include "../lifecycle/GameEvent.hpp"
#include "../lifecycle/ComponentLifecycle.hpp"
#include "../lifecycle/ScriptedLifecycle.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// Unit Type
// ============================================================================

/**
 * @brief Unit classification
 */
enum class UnitType : uint8_t {
    Infantry,
    Ranged,
    Cavalry,
    Siege,
    Hero,
    Worker,
    Monster,
    Boss,

    Custom = 255
};

inline const char* UnitTypeToString(UnitType type) {
    switch (type) {
        case UnitType::Infantry:  return "Infantry";
        case UnitType::Ranged:    return "Ranged";
        case UnitType::Cavalry:   return "Cavalry";
        case UnitType::Siege:     return "Siege";
        case UnitType::Hero:      return "Hero";
        case UnitType::Worker:    return "Worker";
        case UnitType::Monster:   return "Monster";
        case UnitType::Boss:      return "Boss";
        case UnitType::Custom:    return "Custom";
        default:                  return "Unknown";
    }
}

// ============================================================================
// Unit State
// ============================================================================

/**
 * @brief Current unit behavior state
 */
enum class UnitState : uint8_t {
    Idle,
    Moving,
    Attacking,
    Defending,
    Fleeing,
    Dead,
    Stunned,
    Channeling
};

// ============================================================================
// Unit Stats
// ============================================================================

/**
 * @brief Combat and movement statistics
 */
struct UnitStats {
    // Movement
    float moveSpeed = 5.0f;
    float turnSpeed = 360.0f;       // Degrees per second

    // Combat
    float attackDamage = 10.0f;
    float attackSpeed = 1.0f;       // Attacks per second
    float attackRange = 1.5f;       // Melee range
    float armor = 0.0f;

    // Perception
    float sightRange = 10.0f;
    float aggroRange = 8.0f;

    // Health (base values)
    float maxHealth = 100.0f;
    float healthRegen = 0.0f;       // Per second

    // Apply multipliers (for buffs/debuffs)
    float damageMultiplier = 1.0f;
    float speedMultiplier = 1.0f;
    float armorMultiplier = 1.0f;
};

// ============================================================================
// AI Component (for BaseUnit)
// ============================================================================

/**
 * @brief Simple AI component for units
 */
class AIComponent : public ComponentBase<AIComponent> {
public:
    [[nodiscard]] const char* GetTypeName() const override { return "AI"; }

    void OnInitialize() override;
    void OnTick(float deltaTime) override;
    bool OnEvent(const GameEvent& event) override;

    // AI State
    LifecycleHandle target = LifecycleHandle::Invalid;
    glm::vec3 targetPosition{0.0f};
    bool hasTargetPosition = false;

    // Behavior flags
    bool aggressive = true;
    bool canFlee = false;
    float fleeHealthPercent = 0.2f;

    // Attack timing
    float attackCooldown = 0.0f;
};

// ============================================================================
// Combat Component
// ============================================================================

/**
 * @brief Combat handling component
 */
class CombatComponent : public ComponentBase<CombatComponent>,
                        public IComponentDependencies {
public:
    [[nodiscard]] const char* GetTypeName() const override { return "Combat"; }

    void OnInitialize() override;
    void OnTick(float deltaTime) override;
    bool OnEvent(const GameEvent& event) override;

    std::vector<ComponentDependency> GetDependencies() const override {
        return {
            { GetComponentTypeId<TransformComponent>(), true, true },
            { GetComponentTypeId<HealthComponent>(), true, true }
        };
    }

    // Combat interface
    bool CanAttack() const;
    void StartAttack(LifecycleHandle target);
    void CancelAttack();

    // Stats reference
    UnitStats* stats = nullptr;

    // State
    LifecycleHandle currentTarget = LifecycleHandle::Invalid;
    float attackTimer = 0.0f;
    bool isAttacking = false;

private:
    TransformComponent* m_transform = nullptr;
    HealthComponent* m_health = nullptr;
};

// ============================================================================
// BaseUnit
// ============================================================================

/**
 * @brief Base class for all game units
 *
 * Provides:
 * - Component-based architecture
 * - Script integration
 * - Combat system
 * - AI behavior
 * - Event handling
 *
 * JSON Config:
 * {
 *   "id": "unit_soldier",
 *   "type": "unit",
 *   "unit_type": "Infantry",
 *   "stats": {
 *     "max_health": 100,
 *     "attack_damage": 15,
 *     "attack_speed": 1.2,
 *     "move_speed": 6.0,
 *     "armor": 5
 *   },
 *   "lifecycle": {
 *     "tick_group": "AI",
 *     "tick_interval": 0.1
 *   },
 *   "components": ["transform", "health", "movement", "combat", "ai"]
 * }
 */
class BaseUnit : public ScriptedLifecycle {
public:
    BaseUnit();
    ~BaseUnit() override;

    // =========================================================================
    // ILifecycle Implementation
    // =========================================================================

    void OnCreate(const nlohmann::json& config) override;
    void OnTick(float deltaTime) override;
    bool OnEvent(const GameEvent& event) override;
    void OnDestroy() override;

    [[nodiscard]] const char* GetTypeName() const override { return "BaseUnit"; }

    // =========================================================================
    // Unit Properties
    // =========================================================================

    [[nodiscard]] UnitType GetUnitType() const { return m_unitType; }
    void SetUnitType(UnitType type) { m_unitType = type; }

    [[nodiscard]] UnitState GetUnitState() const { return m_unitState; }
    void SetUnitState(UnitState state);

    [[nodiscard]] UnitStats& GetStats() { return m_stats; }
    [[nodiscard]] const UnitStats& GetStats() const { return m_stats; }

    [[nodiscard]] int GetLevel() const { return m_level; }
    void SetLevel(int level) { m_level = level; }

    [[nodiscard]] int GetExperience() const { return m_experience; }
    void AddExperience(int amount);

    // =========================================================================
    // Components
    // =========================================================================

    [[nodiscard]] ComponentContainer& GetComponents() { return m_components; }

    template<typename T>
    T* GetComponent() { return m_components.Get<T>(); }

    template<typename T>
    const T* GetComponent() const { return m_components.Get<T>(); }

    // Convenience accessors
    [[nodiscard]] TransformComponent* GetTransform() {
        return m_components.Get<TransformComponent>();
    }
    [[nodiscard]] HealthComponent* GetHealth() {
        return m_components.Get<HealthComponent>();
    }
    [[nodiscard]] MovementComponent* GetMovement() {
        return m_components.Get<MovementComponent>();
    }

    // =========================================================================
    // Combat
    // =========================================================================

    void Attack(LifecycleHandle target);
    void StopAttack();

    [[nodiscard]] bool IsInCombat() const;
    [[nodiscard]] bool CanAttackTarget(LifecycleHandle target) const;

    float DealDamage(LifecycleHandle target);

    // =========================================================================
    // Movement
    // =========================================================================

    void MoveTo(const glm::vec3& position);
    void MoveInDirection(const glm::vec3& direction);
    void Stop();

    [[nodiscard]] bool IsMoving() const;
    [[nodiscard]] float GetDistanceTo(const glm::vec3& position) const;

    // =========================================================================
    // Team/Faction
    // =========================================================================

    [[nodiscard]] int GetTeamId() const { return m_teamId; }
    void SetTeamId(int teamId) { m_teamId = teamId; }

    [[nodiscard]] bool IsAlly(const BaseUnit* other) const;
    [[nodiscard]] bool IsEnemy(const BaseUnit* other) const;

    // =========================================================================
    // Script Context Override
    // =========================================================================

    [[nodiscard]] ScriptContext BuildContext() const override;

protected:
    // Parse unit-specific config
    virtual void ParseUnitConfig(const nlohmann::json& config);

    // State change handlers
    virtual void OnStateChanged(UnitState oldState, UnitState newState);
    virtual void OnLevelUp(int newLevel);
    virtual void OnDeath(LifecycleHandle killer);

    // Update functions
    virtual void UpdateAI(float deltaTime);
    virtual void UpdateCombat(float deltaTime);
    virtual void UpdateMovement(float deltaTime);

    UnitType m_unitType = UnitType::Infantry;
    UnitState m_unitState = UnitState::Idle;
    UnitStats m_stats;

    int m_level = 1;
    int m_experience = 0;
    int m_teamId = 0;

    ComponentContainer m_components;
};

// ============================================================================
// Registration
// ============================================================================

// Factory registration handled in cpp file
// Use REGISTER_LIFECYCLE_TYPE("unit", BaseUnit) in a source file

} // namespace Lifecycle
} // namespace Vehement
