#pragma once

#include "ArchetypeSystem.hpp"
#include <memory>

namespace Nova {
namespace Archetypes {

/**
 * @brief Helper system for mixing behaviors across archetype categories
 *
 * This allows:
 * - Buildings with auras
 * - Ranged units with melee fallback
 * - Units that can transform into buildings (Treant rooting)
 * - Buildings that spawn and move (mobile siege engines)
 */

// =============================================================================
// Transformation Behavior
// =============================================================================

/**
 * @brief Allows entities to transform between forms (unit ↔ building, etc.)
 */
class TransformationBehavior : public BehaviorComponent {
public:
    enum class TransformationType {
        UnitToBuilding,      // Root (Treant → Ancient)
        BuildingToUnit,      // Uproot (Ancient → Treant)
        FormChange,          // Shapeshift (Bear → Elf)
        TemporaryTransform   // Time-limited transformation
    };

    void Initialize(nlohmann::json config) override;

    TransformationType GetTransformationType() const { return m_transformType; }
    std::string GetTargetFormId() const { return m_targetFormId; }
    float GetTransformTime() const { return m_transformTime; }
    float GetTransformDuration() const { return m_transformDuration; }  // 0 = permanent
    bool CanTransformInCombat() const { return m_canTransformInCombat; }
    bool PreservesHealth() const { return m_preservesHealth; }
    bool PreservesMana() const { return m_preservesMana; }

    // Costs
    const std::unordered_map<std::string, float>& GetTransformCost() const { return m_transformCost; }
    float GetCooldown() const { return m_cooldown; }

private:
    TransformationType m_transformType = TransformationType::FormChange;
    std::string m_targetFormId;
    float m_transformTime = 3.0f;
    float m_transformDuration = 0.0f;
    bool m_canTransformInCombat = false;
    bool m_preservesHealth = true;
    bool m_preservesMana = true;
    std::unordered_map<std::string, float> m_transformCost;
    float m_cooldown = 0.0f;
};

// =============================================================================
// Dual Attack Behavior
// =============================================================================

/**
 * @brief Allows units to have both ranged and melee attacks
 */
class DualAttackBehavior : public BehaviorComponent {
public:
    enum class AttackMode {
        PreferRanged,     // Use ranged when possible
        PreferMelee,      // Close to melee range when possible
        AutoSwitch,       // Switch based on distance
        ManualSwitch      // Player controls mode
    };

    void Initialize(nlohmann::json config) override;

    AttackMode GetAttackMode() const { return m_attackMode; }
    float GetMeleeDamage() const { return m_meleeDamage; }
    float GetRangedDamage() const { return m_rangedDamage; }
    float GetMeleeRange() const { return m_meleeRange; }
    float GetRangedRange() const { return m_rangedRange; }
    float GetSwitchRange() const { return m_switchRange; }  // Auto-switch threshold
    bool HasMeleeBonus() const { return m_hasMeleeBonus; }
    bool HasRangedBonus() const { return m_hasRangedBonus; }

private:
    AttackMode m_attackMode = AttackMode::AutoSwitch;
    float m_meleeDamage = 15.0f;
    float m_rangedDamage = 10.0f;
    float m_meleeRange = 1.5f;
    float m_rangedRange = 8.0f;
    float m_switchRange = 3.0f;
    bool m_hasMeleeBonus = false;
    bool m_hasRangedBonus = false;
};

// =============================================================================
// Building Aura Behavior
// =============================================================================

/**
 * @brief Allows buildings to emit auras (like unit auras)
 */
class BuildingAuraBehavior : public BehaviorComponent {
public:
    void Initialize(nlohmann::json config) override;

    std::string GetAuraName() const { return m_auraName; }
    float GetRadius() const { return m_radius; }
    bool AffectsAllies() const { return m_affectsAllies; }
    bool AffectsEnemies() const { return m_affectsEnemies; }
    bool RequiresPower() const { return m_requiresPower; }  // Needs to be "on"

    // Stat modifications
    const std::unordered_map<std::string, float>& GetStatModifiers() const { return m_statModifiers; }
    const std::vector<std::string>& GetGrantedAbilities() const { return m_grantedAbilities; }

private:
    std::string m_auraName;
    float m_radius = 10.0f;
    bool m_affectsAllies = true;
    bool m_affectsEnemies = false;
    bool m_requiresPower = true;
    std::unordered_map<std::string, float> m_statModifiers;  // "attackSpeed": 1.15
    std::vector<std::string> m_grantedAbilities;
};

// =============================================================================
// Mobile Building Behavior
// =============================================================================

/**
 * @brief Allows buildings to move (siege engines, mobile bases)
 */
class MobileBuildingBehavior : public BehaviorComponent {
public:
    enum class MobilityMode {
        AlwaysMobile,     // Can always move (hover base)
        DeploySystem,     // Must pack/unpack (siege engine)
        TetheredMovement  // Moves along rails/paths only
    };

    void Initialize(nlohmann::json config) override;

    MobilityMode GetMobilityMode() const { return m_mobilityMode; }
    float GetMoveSpeed() const { return m_moveSpeed; }
    float GetPackTime() const { return m_packTime; }
    float GetUnpackTime() const { return m_unpackTime; }
    bool CanFunctionWhileMoving() const { return m_canFunctionWhileMoving; }
    bool CanAttackWhileMoving() const { return m_canAttackWhileMoving; }

private:
    MobilityMode m_mobilityMode = MobilityMode::DeploySystem;
    float m_moveSpeed = 2.0f;
    float m_packTime = 5.0f;
    float m_unpackTime = 5.0f;
    bool m_canFunctionWhileMoving = false;
    bool m_canAttackWhileMoving = false;
};

// =============================================================================
// Archetype Mixer Utility
// =============================================================================

/**
 * @brief Utility class to help combine behaviors from multiple archetypes
 */
class ArchetypeMixer {
public:
    /**
     * @brief Merge multiple parent archetypes into a new archetype
     * Behaviors from later parents override earlier ones if they conflict
     */
    static ArchetypePtr MergeArchetypes(
        const std::string& newId,
        const std::string& newName,
        const std::vector<ArchetypePtr>& parents
    );

    /**
     * @brief Create an entity with behaviors from multiple categories
     * Example: CreateHybrid("treant", {unitBehaviors, buildingBehaviors, transformBehavior})
     */
    static ArchetypePtr CreateHybrid(
        const std::string& id,
        const std::string& name,
        const std::vector<BehaviorPtr>& behaviors
    );

    /**
     * @brief Check if behavior combination is valid
     * Some behaviors are mutually exclusive
     */
    static bool ValidateBehaviorCombination(const std::vector<BehaviorPtr>& behaviors);

    /**
     * @brief Resolve behavior conflicts (e.g., multiple movement types)
     * Returns the dominant behavior based on priority rules
     */
    static BehaviorPtr ResolveBehaviorConflict(
        const std::vector<BehaviorPtr>& conflictingBehaviors
    );

private:
    // Internal validation rules
    static bool IsCompatible(BehaviorPtr a, BehaviorPtr b);
    static int GetBehaviorPriority(BehaviorPtr behavior);
};

// =============================================================================
// Example Hybrid Entity Definitions
// =============================================================================

/**
 * @brief Treant: Unit that can root into a building
 *
 * Behaviors when in Unit form:
 * - MovementBehavior (Ground, slow)
 * - CombatBehavior (Melee, nature damage)
 * - TransformationBehavior (can root)
 *
 * Behaviors when in Building form:
 * - DefenseBehavior (high armor, HP regen)
 * - BuildingAuraBehavior (nature blessing to nearby units)
 * - TransformationBehavior (can uproot)
 */
struct TreantArchetypeBuilder {
    static std::shared_ptr<UnitArchetype> CreateUnitForm();
    static std::shared_ptr<BuildingArchetype> CreateBuildingForm();
};

/**
 * @brief Siege Tank: Building that can move when packed
 *
 * Behaviors when Deployed (building):
 * - DefenseBehavior (attack range 20, high damage)
 * - MobileBuildingBehavior (can pack)
 *
 * Behaviors when Packed (unit):
 * - MovementBehavior (Ground)
 * - CombatBehavior (short range, lower damage)
 * - MobileBuildingBehavior (can deploy)
 */
struct SiegeTankArchetypeBuilder {
    static std::shared_ptr<UnitArchetype> CreateMobileForm();
    static std::shared_ptr<BuildingArchetype> CreateDeployedForm();
};

/**
 * @brief Temple: Building with aura effects
 *
 * Behaviors:
 * - ResourceGenerationBehavior (mana generation)
 * - BuildingAuraBehavior (blessing aura)
 * - DefenseBehavior (standard building defense)
 * - SpawnerBehavior (trains priests)
 */
struct TempleArchetypeBuilder {
    static std::shared_ptr<BuildingArchetype> CreateArchetype();
};

/**
 * @brief Ranger: Unit with both ranged and melee attacks
 *
 * Behaviors:
 * - MovementBehavior (Ground, fast)
 * - DualAttackBehavior (bow + sword)
 * - CombatBehavior (base attack stats)
 */
struct RangerArchetypeBuilder {
    static std::shared_ptr<UnitArchetype> CreateArchetype();
};

} // namespace Archetypes
} // namespace Nova
