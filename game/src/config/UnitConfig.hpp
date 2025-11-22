#pragma once

#include "EntityConfig.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace Vehement {
namespace Config {

// ============================================================================
// Unit Combat Stats
// ============================================================================

/**
 * @brief Combat statistics for a unit
 */
struct CombatStats {
    float health = 100.0f;
    float maxHealth = 100.0f;
    float armor = 0.0f;
    float magicResist = 0.0f;

    float attackDamage = 10.0f;
    float attackSpeed = 1.0f;        // Attacks per second
    float attackRange = 1.5f;        // Attack range in world units
    float critChance = 0.0f;         // 0-1
    float critMultiplier = 2.0f;

    // Damage type flags
    bool physicalDamage = true;
    bool magicalDamage = false;
    bool trueDamage = false;         // Ignores armor/resist
};

// ============================================================================
// Movement Configuration
// ============================================================================

/**
 * @brief Movement capabilities for a unit
 */
struct MovementConfig {
    float moveSpeed = 5.0f;          // Base movement speed
    float turnRate = 360.0f;         // Degrees per second
    float acceleration = 20.0f;
    float deceleration = 30.0f;

    bool canFly = false;
    bool canSwim = false;
    bool canClimb = false;
    bool canBurrow = false;

    float flyHeight = 5.0f;          // Height when flying
    float jumpHeight = 0.0f;         // Max jump height (0 = no jumping)

    // Terrain modifiers
    std::unordered_map<std::string, float> terrainSpeedModifiers;
};

// ============================================================================
// Projectile Configuration
// ============================================================================

/**
 * @brief Configuration for unit's projectile attacks
 */
struct ProjectileConfig {
    std::string projectileId;        // Reference to projectile config
    std::string modelPath;
    float speed = 20.0f;
    float lifetime = 5.0f;
    float gravity = 0.0f;            // 0 = straight line

    bool homing = false;
    float homingStrength = 0.0f;

    // Visual
    std::string trailEffect;
    std::string impactEffect;
    std::string soundOnFire;
    std::string soundOnImpact;
};

// ============================================================================
// Ability Configuration
// ============================================================================

/**
 * @brief Single ability definition
 */
struct AbilityConfig {
    std::string id;
    std::string name;
    std::string description;
    std::string iconPath;

    float cooldown = 10.0f;          // Seconds
    float manaCost = 0.0f;
    float castTime = 0.0f;           // Instant if 0
    float range = 0.0f;              // Self-cast if 0
    float radius = 0.0f;             // For AoE abilities

    // Targeting
    enum class TargetType {
        None,           // Passive
        Self,
        GroundPoint,
        Unit,
        UnitOrGround,
        Direction
    };
    TargetType targetType = TargetType::None;

    bool targetsFriendly = false;
    bool targetsEnemy = true;
    bool targetsSelf = false;

    // Effect script
    std::string scriptPath;
    std::string scriptFunction;

    // Visual/Audio
    std::string castAnimation;
    std::string castEffect;
    std::string castSound;
};

// ============================================================================
// Animation Configuration
// ============================================================================

/**
 * @brief Animation state mapping
 */
struct AnimationMapping {
    std::string stateName;           // e.g., "idle", "walk", "attack"
    std::string animationPath;
    float speed = 1.0f;
    bool loop = true;

    // Blend settings
    float blendInTime = 0.2f;
    float blendOutTime = 0.2f;

    // Events
    std::vector<std::pair<float, std::string>> animationEvents;  // time, event_name
};

// ============================================================================
// Sound Configuration
// ============================================================================

/**
 * @brief Sound effect mappings
 */
struct SoundMapping {
    std::string eventName;           // e.g., "footstep", "attack_hit", "death"
    std::vector<std::string> soundPaths;  // Random selection
    float volume = 1.0f;
    float pitchVariation = 0.1f;     // +/- pitch randomization
    float minDistance = 1.0f;
    float maxDistance = 50.0f;
    bool is3D = true;
};

// ============================================================================
// Unit Configuration
// ============================================================================

/**
 * @brief Complete configuration for a game unit
 *
 * Supports:
 * - Movement: speed, turn rate, acceleration
 * - Combat: health, armor, damage, attack speed/range
 * - Projectiles: for ranged units
 * - AI: behavior tree reference
 * - Abilities: with cooldowns
 * - Animations: state mappings
 * - Sounds: effect mappings
 * - Python hooks: on_spawn, on_death, on_attack, on_damaged, on_idle, on_target_acquired
 */
class UnitConfig : public EntityConfig {
public:
    UnitConfig() = default;
    ~UnitConfig() override = default;

    [[nodiscard]] std::string GetConfigType() const override { return "unit"; }

    ValidationResult Validate() const override;
    void ApplyBaseConfig(const EntityConfig& baseConfig) override;

    // =========================================================================
    // Movement
    // =========================================================================

    [[nodiscard]] const MovementConfig& GetMovement() const { return m_movement; }
    void SetMovement(const MovementConfig& movement) { m_movement = movement; }

    [[nodiscard]] float GetMoveSpeed() const { return m_movement.moveSpeed; }
    [[nodiscard]] float GetTurnRate() const { return m_movement.turnRate; }
    [[nodiscard]] float GetAcceleration() const { return m_movement.acceleration; }

    // =========================================================================
    // Combat
    // =========================================================================

    [[nodiscard]] const CombatStats& GetCombatStats() const { return m_combat; }
    void SetCombatStats(const CombatStats& combat) { m_combat = combat; }

    [[nodiscard]] float GetHealth() const { return m_combat.health; }
    [[nodiscard]] float GetMaxHealth() const { return m_combat.maxHealth; }
    [[nodiscard]] float GetArmor() const { return m_combat.armor; }
    [[nodiscard]] float GetAttackDamage() const { return m_combat.attackDamage; }
    [[nodiscard]] float GetAttackSpeed() const { return m_combat.attackSpeed; }
    [[nodiscard]] float GetAttackRange() const { return m_combat.attackRange; }

    // =========================================================================
    // Projectile
    // =========================================================================

    [[nodiscard]] bool HasProjectile() const { return !m_projectile.projectileId.empty(); }
    [[nodiscard]] const ProjectileConfig& GetProjectile() const { return m_projectile; }
    void SetProjectile(const ProjectileConfig& projectile) { m_projectile = projectile; }

    // =========================================================================
    // AI
    // =========================================================================

    [[nodiscard]] const std::string& GetBehaviorTreePath() const { return m_behaviorTreePath; }
    void SetBehaviorTreePath(const std::string& path) { m_behaviorTreePath = path; }

    [[nodiscard]] const std::string& GetAIProfile() const { return m_aiProfile; }
    void SetAIProfile(const std::string& profile) { m_aiProfile = profile; }

    // AI parameters
    [[nodiscard]] float GetAggroRange() const { return m_aggroRange; }
    void SetAggroRange(float range) { m_aggroRange = range; }

    [[nodiscard]] float GetLeashRange() const { return m_leashRange; }
    void SetLeashRange(float range) { m_leashRange = range; }

    // =========================================================================
    // Abilities
    // =========================================================================

    [[nodiscard]] const std::vector<AbilityConfig>& GetAbilities() const { return m_abilities; }
    void SetAbilities(const std::vector<AbilityConfig>& abilities) { m_abilities = abilities; }
    void AddAbility(const AbilityConfig& ability) { m_abilities.push_back(ability); }

    [[nodiscard]] const AbilityConfig* GetAbility(const std::string& id) const;

    // =========================================================================
    // Animations
    // =========================================================================

    [[nodiscard]] const std::vector<AnimationMapping>& GetAnimations() const { return m_animations; }
    void SetAnimations(const std::vector<AnimationMapping>& anims) { m_animations = anims; }

    [[nodiscard]] const AnimationMapping* GetAnimation(const std::string& state) const;

    // =========================================================================
    // Sounds
    // =========================================================================

    [[nodiscard]] const std::vector<SoundMapping>& GetSounds() const { return m_sounds; }
    void SetSounds(const std::vector<SoundMapping>& sounds) { m_sounds = sounds; }

    [[nodiscard]] const SoundMapping* GetSound(const std::string& event) const;

    // =========================================================================
    // Python Script Hooks
    // =========================================================================

    // Convenience accessors for common hooks
    [[nodiscard]] std::string GetOnSpawnScript() const { return GetScriptHook("on_spawn"); }
    [[nodiscard]] std::string GetOnDeathScript() const { return GetScriptHook("on_death"); }
    [[nodiscard]] std::string GetOnAttackScript() const { return GetScriptHook("on_attack"); }
    [[nodiscard]] std::string GetOnDamagedScript() const { return GetScriptHook("on_damaged"); }
    [[nodiscard]] std::string GetOnIdleScript() const { return GetScriptHook("on_idle"); }
    [[nodiscard]] std::string GetOnTargetAcquiredScript() const { return GetScriptHook("on_target_acquired"); }

    void SetOnSpawnScript(const std::string& path) { SetScriptHook("on_spawn", path); }
    void SetOnDeathScript(const std::string& path) { SetScriptHook("on_death", path); }
    void SetOnAttackScript(const std::string& path) { SetScriptHook("on_attack", path); }
    void SetOnDamagedScript(const std::string& path) { SetScriptHook("on_damaged", path); }
    void SetOnIdleScript(const std::string& path) { SetScriptHook("on_idle", path); }
    void SetOnTargetAcquiredScript(const std::string& path) { SetScriptHook("on_target_acquired", path); }

    // =========================================================================
    // Unit Classification
    // =========================================================================

    [[nodiscard]] const std::string& GetUnitClass() const { return m_unitClass; }
    void SetUnitClass(const std::string& unitClass) { m_unitClass = unitClass; }

    [[nodiscard]] const std::string& GetFaction() const { return m_faction; }
    void SetFaction(const std::string& faction) { m_faction = faction; }

    [[nodiscard]] int GetTier() const { return m_tier; }
    void SetTier(int tier) { m_tier = tier; }

    [[nodiscard]] bool IsHero() const { return m_isHero; }
    void SetIsHero(bool isHero) { m_isHero = isHero; }

protected:
    void ParseTypeSpecificFields(const std::string& jsonContent) override;
    std::string SerializeTypeSpecificFields() const override;

private:
    std::string GetScriptHook(const std::string& hookName) const;
    void SetScriptHook(const std::string& hookName, const std::string& path);

    // Movement
    MovementConfig m_movement;

    // Combat
    CombatStats m_combat;

    // Projectile (for ranged units)
    ProjectileConfig m_projectile;

    // AI
    std::string m_behaviorTreePath;
    std::string m_aiProfile;
    float m_aggroRange = 10.0f;
    float m_leashRange = 30.0f;

    // Abilities
    std::vector<AbilityConfig> m_abilities;

    // Animations
    std::vector<AnimationMapping> m_animations;

    // Sounds
    std::vector<SoundMapping> m_sounds;

    // Script hooks
    std::unordered_map<std::string, std::string> m_scriptHooks;

    // Classification
    std::string m_unitClass;    // e.g., "infantry", "cavalry", "ranged"
    std::string m_faction;
    int m_tier = 1;
    bool m_isHero = false;
};

// Register the unit config type
REGISTER_CONFIG_TYPE("unit", UnitConfig)

} // namespace Config
} // namespace Vehement
