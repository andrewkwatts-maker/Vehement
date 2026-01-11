#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

namespace Nova {
namespace Archetypes {

// Forward declarations
class Archetype;
class BehaviorComponent;
class SpellArchetype;
class UnitArchetype;
class BuildingArchetype;

using ArchetypePtr = std::shared_ptr<Archetype>;
using BehaviorPtr = std::shared_ptr<BehaviorComponent>;

// =============================================================================
// Behavior Component System
// =============================================================================

/**
 * @brief Base class for all behavior components
 * Behaviors are composable pieces that define how entities act
 */
class BehaviorComponent {
public:
    virtual ~BehaviorComponent() = default;

    std::string GetId() const { return m_id; }
    std::string GetType() const { return m_type; }

    // Behavior lifecycle
    virtual void Initialize(nlohmann::json config) = 0;
    virtual void Update(float deltaTime) {}
    virtual void Execute() {}

    // Serialization
    virtual nlohmann::json Serialize() const;
    static BehaviorPtr Deserialize(const nlohmann::json& json);

protected:
    std::string m_id;
    std::string m_type;
    nlohmann::json m_config;
};

// =============================================================================
// Spell Behavior Components
// =============================================================================

/**
 * @brief Targeting behavior for spells
 */
class TargetingBehavior : public BehaviorComponent {
public:
    enum class TargetType {
        Self,           // Caster only
        SingleTarget,   // One enemy/ally
        GroundTarget,   // Point on ground
        Direction,      // Direction from caster
        Area,           // Area around point
        LineOfSight,    // All in view cone
        Aura            // Continuous area around caster
    };

    void Initialize(nlohmann::json config) override;

    TargetType GetTargetType() const { return m_targetType; }
    float GetRange() const { return m_range; }
    float GetRadius() const { return m_radius; }
    float GetAngle() const { return m_angle; }
    bool RequiresLineOfSight() const { return m_requiresLineOfSight; }
    bool CanTargetAllies() const { return m_canTargetAllies; }
    bool CanTargetEnemies() const { return m_canTargetEnemies; }
    bool CanTargetSelf() const { return m_canTargetSelf; }

private:
    TargetType m_targetType = TargetType::SingleTarget;
    float m_range = 10.0f;
    float m_radius = 5.0f;  // For AoE/Aura
    float m_angle = 60.0f;  // For cone/LoS
    bool m_requiresLineOfSight = true;
    bool m_canTargetAllies = false;
    bool m_canTargetEnemies = true;
    bool m_canTargetSelf = true;
};

/**
 * @brief Effect behavior for spells
 */
class EffectBehavior : public BehaviorComponent {
public:
    enum class EffectType {
        Damage,
        Heal,
        Buff,
        Debuff,
        Summon,
        Transform,
        Teleport,
        Custom
    };

    void Initialize(nlohmann::json config) override;

    EffectType GetEffectType() const { return m_effectType; }
    float GetValue() const { return m_value; }
    float GetDuration() const { return m_duration; }
    float GetTickInterval() const { return m_tickInterval; }
    bool IsInstant() const { return m_instant; }
    const std::vector<std::string>& GetStatusEffects() const { return m_statusEffects; }

private:
    EffectType m_effectType = EffectType::Damage;
    float m_value = 10.0f;
    float m_duration = 0.0f;  // 0 = instant
    float m_tickInterval = 1.0f;  // For DoT/HoT
    bool m_instant = true;
    std::vector<std::string> m_statusEffects;  // e.g., "stunned", "slowed"
};

/**
 * @brief Projectile behavior for spells
 */
class ProjectileBehavior : public BehaviorComponent {
public:
    enum class ProjectileType {
        None,       // Instant cast
        Linear,     // Straight line
        Homing,     // Seeks target
        Arc,        // Parabolic arc
        Bouncing,   // Bounces between targets
        Chaining    // Chains to nearby targets
    };

    void Initialize(nlohmann::json config) override;

    ProjectileType GetProjectileType() const { return m_projectileType; }
    float GetSpeed() const { return m_speed; }
    int GetMaxBounces() const { return m_maxBounces; }
    int GetMaxChains() const { return m_maxChains; }
    float GetChainRange() const { return m_chainRange; }
    std::string GetVisualEffect() const { return m_visualEffect; }

private:
    ProjectileType m_projectileType = ProjectileType::None;
    float m_speed = 15.0f;
    int m_maxBounces = 0;
    int m_maxChains = 0;
    float m_chainRange = 5.0f;
    std::string m_visualEffect;
};

/**
 * @brief Cooldown/Cost behavior for spells
 */
class ResourceBehavior : public BehaviorComponent {
public:
    void Initialize(nlohmann::json config) override;

    float GetCooldown() const { return m_cooldown; }
    float GetCastTime() const { return m_castTime; }
    const std::unordered_map<std::string, float>& GetCosts() const { return m_costs; }
    bool IsChanneled() const { return m_channeled; }
    int GetCharges() const { return m_charges; }
    float GetChargeRefreshTime() const { return m_chargeRefreshTime; }

private:
    float m_cooldown = 5.0f;
    float m_castTime = 0.0f;  // 0 = instant
    std::unordered_map<std::string, float> m_costs;  // "mana": 50, "health": 10
    bool m_channeled = false;
    int m_charges = 1;  // 0 = unlimited
    float m_chargeRefreshTime = 10.0f;
};

// =============================================================================
// Unit Behavior Components
// =============================================================================

/**
 * @brief Movement behavior for units
 */
class MovementBehavior : public BehaviorComponent {
public:
    enum class MovementType {
        Ground,
        Flying,
        Amphibious,
        Teleporting,
        Phasing,  // Can move through units
        Burrowing
    };

    void Initialize(nlohmann::json config) override;

    MovementType GetMovementType() const { return m_movementType; }
    float GetSpeed() const { return m_speed; }
    float GetAcceleration() const { return m_acceleration; }
    float GetTurnRate() const { return m_turnRate; }
    float GetFlyingHeight() const { return m_flyingHeight; }
    bool CanPassThroughUnits() const { return m_canPassThroughUnits; }

private:
    MovementType m_movementType = MovementType::Ground;
    float m_speed = 5.0f;
    float m_acceleration = 10.0f;
    float m_turnRate = 360.0f;  // degrees/second
    float m_flyingHeight = 3.0f;
    bool m_canPassThroughUnits = false;
};

/**
 * @brief Combat behavior for units
 */
class CombatBehavior : public BehaviorComponent {
public:
    enum class AttackType {
        Melee,
        Ranged,
        Siege,
        Magic
    };

    enum class DamageType {
        Physical,
        Magic,
        Pierce,
        Siege,
        True  // Ignores armor
    };

    void Initialize(nlohmann::json config) override;

    AttackType GetAttackType() const { return m_attackType; }
    DamageType GetDamageType() const { return m_damageType; }
    float GetDamage() const { return m_damage; }
    float GetAttackRange() const { return m_attackRange; }
    float GetAttackSpeed() const { return m_attackSpeed; }
    bool CanAttackGround() const { return m_canAttackGround; }
    bool CanAttackAir() const { return m_canAttackAir; }

private:
    AttackType m_attackType = AttackType::Melee;
    DamageType m_damageType = DamageType::Physical;
    float m_damage = 10.0f;
    float m_attackRange = 1.5f;
    float m_attackSpeed = 1.0f;  // attacks per second
    bool m_canAttackGround = true;
    bool m_canAttackAir = false;
};

/**
 * @brief Worker/Gatherer behavior for units
 */
class WorkerBehavior : public BehaviorComponent {
public:
    void Initialize(nlohmann::json config) override;

    bool CanGather(const std::string& resourceType) const;
    float GetGatherRate(const std::string& resourceType) const;
    float GetCarryCapacity(const std::string& resourceType) const;
    bool CanBuild() const { return m_canBuild; }
    bool CanRepair() const { return m_canRepair; }
    float GetBuildSpeed() const { return m_buildSpeed; }

private:
    std::unordered_map<std::string, float> m_gatherRates;  // "wood": 10/s
    std::unordered_map<std::string, float> m_carryCapacity;  // "wood": 100
    bool m_canBuild = true;
    bool m_canRepair = true;
    float m_buildSpeed = 1.0f;
};

/**
 * @brief Spellcaster behavior for units
 */
class SpellcasterBehavior : public BehaviorComponent {
public:
    void Initialize(nlohmann::json config) override;

    const std::vector<std::string>& GetSpells() const { return m_spells; }
    float GetManaPool() const { return m_manaPool; }
    float GetManaRegen() const { return m_manaRegen; }
    bool IsAutocast(const std::string& spellId) const;

private:
    std::vector<std::string> m_spells;  // Spell IDs
    float m_manaPool = 100.0f;
    float m_manaRegen = 1.0f;  // per second
    std::unordered_map<std::string, bool> m_autocast;
};

// =============================================================================
// Building Behavior Components
// =============================================================================

/**
 * @brief Resource generation behavior for buildings
 */
class ResourceGenerationBehavior : public BehaviorComponent {
public:
    void Initialize(nlohmann::json config) override;

    const std::unordered_map<std::string, float>& GetGenerationRates() const { return m_generationRates; }
    const std::unordered_map<std::string, float>& GetStorageCapacity() const { return m_storageCapacity; }
    bool RequiresWorkers() const { return m_requiresWorkers; }
    int GetMaxWorkers() const { return m_maxWorkers; }

private:
    std::unordered_map<std::string, float> m_generationRates;  // "food": 10/s
    std::unordered_map<std::string, float> m_storageCapacity;  // "food": 1000
    bool m_requiresWorkers = false;
    int m_maxWorkers = 5;
};

/**
 * @brief Unit spawning behavior for buildings
 */
class SpawnerBehavior : public BehaviorComponent {
public:
    void Initialize(nlohmann::json config) override;

    const std::vector<std::string>& GetSpawnableUnits() const { return m_spawnableUnits; }
    int GetQueueSize() const { return m_queueSize; }
    glm::vec3 GetRallyPoint() const { return m_rallyPoint; }
    void SetRallyPoint(const glm::vec3& point) { m_rallyPoint = point; }
    bool CanTrainMultiple() const { return m_canTrainMultiple; }

private:
    std::vector<std::string> m_spawnableUnits;  // Unit IDs
    int m_queueSize = 5;
    glm::vec3 m_rallyPoint{0, 0, 0};
    bool m_canTrainMultiple = false;
};

/**
 * @brief Housing/Population behavior for buildings
 */
class HousingBehavior : public BehaviorComponent {
public:
    void Initialize(nlohmann::json config) override;

    int GetPopulationProvided() const { return m_populationProvided; }
    int GetPopulationUsed() const { return m_populationUsed; }
    const std::vector<std::string>& GetAllowedUnitTypes() const { return m_allowedUnitTypes; }

private:
    int m_populationProvided = 10;
    int m_populationUsed = 0;
    std::vector<std::string> m_allowedUnitTypes;  // Empty = all types
};

/**
 * @brief Defense behavior for buildings
 */
class DefenseBehavior : public BehaviorComponent {
public:
    void Initialize(nlohmann::json config) override;

    float GetArmor() const { return m_armor; }
    float GetHealthRegen() const { return m_healthRegen; }
    bool HasAttackCapability() const { return m_hasAttack; }
    float GetAttackRange() const { return m_attackRange; }
    float GetAttackDamage() const { return m_attackDamage; }
    int GetGarrisonCapacity() const { return m_garrisonCapacity; }

private:
    float m_armor = 10.0f;
    float m_healthRegen = 0.5f;
    bool m_hasAttack = false;
    float m_attackRange = 12.0f;
    float m_attackDamage = 15.0f;
    int m_garrisonCapacity = 0;
};

// =============================================================================
// Base Archetype Class
// =============================================================================

/**
 * @brief Base archetype class with behavior composition
 */
class Archetype {
public:
    Archetype() = default;
    virtual ~Archetype() = default;

    // Identity
    std::string GetId() const { return m_id; }
    std::string GetName() const { return m_name; }
    std::string GetDescription() const { return m_description; }

    // Inheritance
    void SetParentArchetype(const std::string& parentId) { m_parentArchetype = parentId; }
    std::string GetParentArchetype() const { return m_parentArchetype; }
    void InheritFrom(ArchetypePtr parent);

    // Behavior composition
    void AddBehavior(BehaviorPtr behavior);
    void RemoveBehavior(const std::string& behaviorId);
    BehaviorPtr GetBehavior(const std::string& behaviorId) const;
    template<typename T>
    std::shared_ptr<T> GetBehavior() const;
    bool HasBehavior(const std::string& behaviorId) const;
    const std::vector<BehaviorPtr>& GetAllBehaviors() const { return m_behaviors; }

    // Stats
    void SetStat(const std::string& key, float value) { m_stats[key] = value; }
    float GetStat(const std::string& key, float defaultValue = 0.0f) const;
    const std::unordered_map<std::string, float>& GetAllStats() const { return m_stats; }

    // Properties
    void SetProperty(const std::string& key, const nlohmann::json& value) { m_properties[key] = value; }
    nlohmann::json GetProperty(const std::string& key) const;
    bool HasProperty(const std::string& key) const;

    // Serialization
    virtual nlohmann::json Serialize() const;
    static ArchetypePtr Deserialize(const nlohmann::json& json);

protected:
    std::string m_id;
    std::string m_name;
    std::string m_description;
    std::string m_parentArchetype;

    std::vector<BehaviorPtr> m_behaviors;
    std::unordered_map<std::string, float> m_stats;
    nlohmann::json m_properties;
};

// =============================================================================
// Specialized Archetype Classes
// =============================================================================

/**
 * @brief Spell archetype with targeting and effects
 */
class SpellArchetype : public Archetype {
public:
    static std::shared_ptr<SpellArchetype> Deserialize(const nlohmann::json& json);
};

/**
 * @brief Unit archetype with movement and combat
 */
class UnitArchetype : public Archetype {
public:
    static std::shared_ptr<UnitArchetype> Deserialize(const nlohmann::json& json);
};

/**
 * @brief Building archetype with production and housing
 */
class BuildingArchetype : public Archetype {
public:
    static std::shared_ptr<BuildingArchetype> Deserialize(const nlohmann::json& json);
};

// =============================================================================
// Archetype Registry
// =============================================================================

/**
 * @brief Global registry for all archetypes
 */
class ArchetypeRegistry {
public:
    static ArchetypeRegistry& Instance();

    // Registration
    void RegisterArchetype(ArchetypePtr archetype);
    void RegisterSpellArchetype(std::shared_ptr<SpellArchetype> archetype);
    void RegisterUnitArchetype(std::shared_ptr<UnitArchetype> archetype);
    void RegisterBuildingArchetype(std::shared_ptr<BuildingArchetype> archetype);

    // Retrieval
    ArchetypePtr GetArchetype(const std::string& id) const;
    std::shared_ptr<SpellArchetype> GetSpellArchetype(const std::string& id) const;
    std::shared_ptr<UnitArchetype> GetUnitArchetype(const std::string& id) const;
    std::shared_ptr<BuildingArchetype> GetBuildingArchetype(const std::string& id) const;

    // Loading
    void LoadArchetypesFromDirectory(const std::string& directory);
    void LoadArchetypeFromFile(const std::string& filepath);

    // Queries
    std::vector<std::shared_ptr<SpellArchetype>> GetAllSpellArchetypes() const;
    std::vector<std::shared_ptr<UnitArchetype>> GetAllUnitArchetypes() const;
    std::vector<std::shared_ptr<BuildingArchetype>> GetAllBuildingArchetypes() const;

    void Clear();

private:
    ArchetypeRegistry() = default;

    std::unordered_map<std::string, ArchetypePtr> m_archetypes;
    std::unordered_map<std::string, std::shared_ptr<SpellArchetype>> m_spellArchetypes;
    std::unordered_map<std::string, std::shared_ptr<UnitArchetype>> m_unitArchetypes;
    std::unordered_map<std::string, std::shared_ptr<BuildingArchetype>> m_buildingArchetypes;
};

} // namespace Archetypes
} // namespace Nova
