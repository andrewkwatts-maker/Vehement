#pragma once

#include "../entities/Entity.hpp"
#include "HeroClass.hpp"
#include "Experience.hpp"
#include "Ability.hpp"
#include "HeroInventory.hpp"

#include <array>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Vehement {
namespace RTS {

/**
 * @brief Hero ability slots (keyboard binding)
 */
enum class AbilitySlot : uint8_t {
    Q = 0,      // Primary ability
    W = 1,      // Secondary ability
    E = 2,      // Tertiary ability
    R = 3,      // Ultimate ability

    COUNT
};

/**
 * @brief Status effect instance applied to hero
 */
struct StatusEffectInstance {
    StatusEffect effect = StatusEffect::None;
    float duration = 0.0f;          // Remaining duration
    float strength = 0.0f;          // Effect strength (speed %, damage %, etc.)
    uint32_t sourceId = 0;          // Who applied this effect

    [[nodiscard]] bool IsExpired() const { return duration <= 0.0f; }
};

/**
 * @brief Aura definition for hero passive effects
 */
struct HeroAura {
    std::string name;
    StatusEffect effect = StatusEffect::None;
    float radius = 8.0f;
    float strength = 0.0f;          // Buff/debuff strength
    bool affectsAllies = true;
    bool affectsEnemies = false;
    bool affectsSelf = false;
    bool requiresToggle = false;    // True if ability toggles this aura
    int sourceAbilityId = -1;       // Ability that creates this aura
};

/**
 * @brief Death/revival state
 */
struct RevivalState {
    bool isDead = false;
    float deathTimer = 0.0f;        // Time since death
    float respawnTime = 30.0f;      // Time until respawn
    glm::vec3 respawnPosition{0.0f};
    bool autoRevive = true;         // Automatically respawn at base

    [[nodiscard]] float GetRespawnProgress() const {
        if (!isDead) return 1.0f;
        return std::min(1.0f, deathTimer / respawnTime);
    }

    [[nodiscard]] float GetTimeUntilRespawn() const {
        if (!isDead) return 0.0f;
        return std::max(0.0f, respawnTime - deathTimer);
    }
};

/**
 * @brief Hero class - the player's main character in RTS mode
 *
 * The hero is a powerful unit like in Warcraft 3 that levels up,
 * has abilities, carries items, and provides aura buffs to nearby allies.
 * The hero needs workers and buildings to succeed but is a key unit
 * that can turn the tide of battle.
 */
class Hero : public Entity {
public:
    static constexpr int ABILITY_SLOT_COUNT = 4;
    static constexpr float BASE_COMMAND_RADIUS = 15.0f;
    static constexpr float BASE_AURA_RADIUS = 8.0f;
    static constexpr float BASE_VISION_RANGE = 12.0f;
    static constexpr float BASE_ATTACK_SPEED = 1.0f;
    static constexpr float MANA_REGEN_PER_INT = 0.05f;
    static constexpr float HEALTH_REGEN_PER_STR = 0.1f;

    using DeathCallback = std::function<void(Hero& hero)>;
    using ReviveCallback = std::function<void(Hero& hero)>;
    using LevelUpCallback = std::function<void(Hero& hero, int newLevel)>;
    using AbilityCallback = std::function<void(Hero& hero, AbilitySlot slot, const AbilityCastResult& result)>;

    Hero();
    explicit Hero(HeroClass heroClass);
    ~Hero() override = default;

    // =========================================================================
    // Core Update/Render
    // =========================================================================

    /**
     * @brief Update hero state
     */
    void Update(float deltaTime) override;

    /**
     * @brief Render the hero
     */
    void Render(Nova::Renderer& renderer) override;

    // =========================================================================
    // Hero Class
    // =========================================================================

    /**
     * @brief Get hero class
     */
    [[nodiscard]] HeroClass GetHeroClass() const noexcept { return m_heroClass; }

    /**
     * @brief Get class definition
     */
    [[nodiscard]] const HeroClassDefinition& GetClassDefinition() const;

    /**
     * @brief Get hero name (custom or class default)
     */
    [[nodiscard]] const std::string& GetHeroName() const;

    /**
     * @brief Set custom hero name
     */
    void SetHeroName(const std::string& name) { m_heroName = name; }

    // =========================================================================
    // Stats
    // =========================================================================

    /**
     * @brief Get base stats (before items and buffs)
     */
    [[nodiscard]] const HeroStats& GetBaseStats() const noexcept { return m_baseStats; }

    /**
     * @brief Get total stats (base + items + buffs)
     */
    [[nodiscard]] HeroStats GetStats() const;

    /**
     * @brief Get strength
     */
    [[nodiscard]] float GetStrength() const { return GetStats().strength; }

    /**
     * @brief Get agility
     */
    [[nodiscard]] float GetAgility() const { return GetStats().agility; }

    /**
     * @brief Get intelligence
     */
    [[nodiscard]] float GetIntelligence() const { return GetStats().intelligence; }

    /**
     * @brief Add stat points (from leveling)
     */
    void AddStatPoints(float strength, float agility, float intelligence);

    /**
     * @brief Allocate a single stat point
     */
    bool AllocateStatPoint(int statIndex); // 0=str, 1=agi, 2=int

    // =========================================================================
    // Health & Mana
    // =========================================================================

    /**
     * @brief Get maximum health (base + stat bonus + items)
     */
    [[nodiscard]] float GetMaxHealth() const noexcept override;

    /**
     * @brief Get current mana
     */
    [[nodiscard]] float GetMana() const noexcept { return m_mana; }

    /**
     * @brief Get maximum mana
     */
    [[nodiscard]] float GetMaxMana() const noexcept;

    /**
     * @brief Get mana percentage (0.0 to 1.0)
     */
    [[nodiscard]] float GetManaPercent() const noexcept {
        float maxMana = GetMaxMana();
        return maxMana > 0.0f ? m_mana / maxMana : 0.0f;
    }

    /**
     * @brief Set mana (clamped to max)
     */
    void SetMana(float mana);

    /**
     * @brief Add mana (clamped to max)
     */
    void AddMana(float amount);

    /**
     * @brief Consume mana for ability
     * @return true if had enough mana
     */
    bool ConsumeMana(float amount);

    /**
     * @brief Get health regeneration rate (per second)
     */
    [[nodiscard]] float GetHealthRegen() const;

    /**
     * @brief Get mana regeneration rate (per second)
     */
    [[nodiscard]] float GetManaRegen() const;

    // =========================================================================
    // Combat
    // =========================================================================

    /**
     * @brief Get attack damage
     */
    [[nodiscard]] float GetAttackDamage() const;

    /**
     * @brief Get attack speed multiplier
     */
    [[nodiscard]] float GetAttackSpeed() const;

    /**
     * @brief Get armor value
     */
    [[nodiscard]] float GetArmor() const;

    /**
     * @brief Calculate damage reduction from armor
     */
    [[nodiscard]] float GetDamageReduction() const;

    /**
     * @brief Override take damage to apply armor and effects
     */
    float TakeDamage(float amount, EntityId source = INVALID_ID) override;

    /**
     * @brief Handle hero death
     */
    void Die() override;

    // =========================================================================
    // Level & Experience
    // =========================================================================

    /**
     * @brief Get current level
     */
    [[nodiscard]] int GetLevel() const noexcept { return m_experience.GetLevel(); }

    /**
     * @brief Get experience system
     */
    [[nodiscard]] ExperienceSystem& GetExperience() noexcept { return m_experience; }
    [[nodiscard]] const ExperienceSystem& GetExperience() const noexcept { return m_experience; }

    /**
     * @brief Add experience from a source
     */
    int AddExperience(int amount, ExperienceSource source, int enemyLevel = 0);

    /**
     * @brief Get unspent stat points
     */
    [[nodiscard]] int GetUnspentStatPoints() const { return m_experience.GetUnspentStatPoints(); }

    /**
     * @brief Get unspent ability points
     */
    [[nodiscard]] int GetUnspentAbilityPoints() const { return m_experience.GetUnspentAbilityPoints(); }

    // =========================================================================
    // Abilities
    // =========================================================================

    /**
     * @brief Get ability state for a slot
     */
    [[nodiscard]] const AbilityState& GetAbilityState(AbilitySlot slot) const;

    /**
     * @brief Get ability data for a slot
     */
    [[nodiscard]] const AbilityData* GetAbilityData(AbilitySlot slot) const;

    /**
     * @brief Set ability in a slot
     */
    void SetAbility(AbilitySlot slot, int abilityId);

    /**
     * @brief Level up an ability
     * @return true if ability was leveled up
     */
    bool LevelUpAbility(AbilitySlot slot);

    /**
     * @brief Cast ability in slot
     */
    AbilityCastResult CastAbility(AbilitySlot slot);

    /**
     * @brief Cast ability at target point
     */
    AbilityCastResult CastAbilityAtPoint(AbilitySlot slot, const glm::vec3& point);

    /**
     * @brief Cast ability on target unit
     */
    AbilityCastResult CastAbilityOnTarget(AbilitySlot slot, Entity* target);

    /**
     * @brief Check if ability can be cast
     */
    [[nodiscard]] bool CanCastAbility(AbilitySlot slot) const;

    /**
     * @brief Get cooldown remaining on ability
     */
    [[nodiscard]] float GetAbilityCooldown(AbilitySlot slot) const;

    /**
     * @brief Toggle ability (for toggle-type abilities)
     */
    bool ToggleAbility(AbilitySlot slot);

    /**
     * @brief Cancel channeling ability
     */
    void CancelChanneling();

    // =========================================================================
    // Inventory
    // =========================================================================

    /**
     * @brief Get hero inventory
     */
    [[nodiscard]] HeroInventory& GetInventory() noexcept { return m_inventory; }
    [[nodiscard]] const HeroInventory& GetInventory() const noexcept { return m_inventory; }

    /**
     * @brief Use item in slot
     */
    bool UseItem(int slot);

    // =========================================================================
    // Status Effects
    // =========================================================================

    /**
     * @brief Apply a status effect
     */
    void ApplyStatusEffect(StatusEffect effect, float duration, float strength = 1.0f, uint32_t sourceId = 0);

    /**
     * @brief Remove a status effect
     */
    void RemoveStatusEffect(StatusEffect effect);

    /**
     * @brief Check if affected by status
     */
    [[nodiscard]] bool HasStatusEffect(StatusEffect effect) const;

    /**
     * @brief Get status effect strength (0 if not present)
     */
    [[nodiscard]] float GetStatusEffectStrength(StatusEffect effect) const;

    /**
     * @brief Get all active status effects
     */
    [[nodiscard]] const std::vector<StatusEffectInstance>& GetStatusEffects() const { return m_statusEffects; }

    /**
     * @brief Clear all status effects
     */
    void ClearStatusEffects();

    // =========================================================================
    // Auras
    // =========================================================================

    /**
     * @brief Get active auras
     */
    [[nodiscard]] const std::vector<HeroAura>& GetAuras() const { return m_auras; }

    /**
     * @brief Add an aura
     */
    void AddAura(const HeroAura& aura);

    /**
     * @brief Remove an aura
     */
    void RemoveAura(const std::string& auraName);

    /**
     * @brief Get command radius (for giving orders)
     */
    [[nodiscard]] float GetCommandRadius() const;

    /**
     * @brief Get aura radius
     */
    [[nodiscard]] float GetAuraRadius() const;

    /**
     * @brief Get vision range
     */
    [[nodiscard]] float GetVisionRange() const;

    // =========================================================================
    // Movement
    // =========================================================================

    /**
     * @brief Get effective move speed (with bonuses)
     */
    [[nodiscard]] float GetEffectiveMoveSpeed() const;

    /**
     * @brief Move towards a point
     */
    void MoveTo(const glm::vec3& target);

    /**
     * @brief Stop movement
     */
    void Stop();

    /**
     * @brief Check if hero is moving
     */
    [[nodiscard]] bool IsMoving() const noexcept { return m_isMoving; }

    /**
     * @brief Get movement target
     */
    [[nodiscard]] const glm::vec3& GetMoveTarget() const noexcept { return m_moveTarget; }

    // =========================================================================
    // Revival
    // =========================================================================

    /**
     * @brief Get revival state
     */
    [[nodiscard]] const RevivalState& GetRevivalState() const noexcept { return m_revival; }

    /**
     * @brief Set respawn position
     */
    void SetRespawnPosition(const glm::vec3& position) { m_revival.respawnPosition = position; }

    /**
     * @brief Revive the hero at position
     */
    void Revive(const glm::vec3& position);

    /**
     * @brief Revive at default respawn point
     */
    void Revive();

    /**
     * @brief Get time until respawn
     */
    [[nodiscard]] float GetRespawnTimer() const { return m_revival.GetTimeUntilRespawn(); }

    /**
     * @brief Check if hero is dead
     */
    [[nodiscard]] bool IsDead() const noexcept { return m_revival.isDead; }

    /**
     * @brief Set respawn time (scales with level)
     */
    void SetBaseRespawnTime(float time) { m_baseRespawnTime = time; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnDeath(DeathCallback callback) { m_onDeath = std::move(callback); }
    void SetOnRevive(ReviveCallback callback) { m_onRevive = std::move(callback); }
    void SetOnLevelUp(LevelUpCallback callback) { m_onLevelUp = std::move(callback); }
    void SetOnAbilityCast(AbilityCallback callback) { m_onAbilityCast = std::move(callback); }

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Reset hero to starting state
     */
    void Reset();

private:
    void InitializeFromClass();
    void UpdateAbilities(float deltaTime);
    void UpdateStatusEffects(float deltaTime);
    void UpdateRegen(float deltaTime);
    void UpdateMovement(float deltaTime);
    void UpdateRevival(float deltaTime);
    void HandleLevelUp(int newLevel, int statPoints, int abilityPoints);
    float CalculateRespawnTime() const;

    AbilityCastResult ExecuteAbility(AbilitySlot slot, const AbilityCastContext& context);

    // Hero identity
    HeroClass m_heroClass = HeroClass::Warlord;
    std::string m_heroName;

    // Stats
    HeroStats m_baseStats;
    HeroStats m_bonusStats;         // From temporary buffs

    // Resources
    float m_mana = 100.0f;
    float m_baseMana = 100.0f;
    float m_baseHealth = 300.0f;

    // Experience system
    ExperienceSystem m_experience;

    // Abilities (4 slots: Q, W, E, R)
    std::array<AbilityState, ABILITY_SLOT_COUNT> m_abilities;
    int m_channelingAbility = -1;   // Slot index of currently channeling ability

    // Inventory (6 slots)
    HeroInventory m_inventory;

    // Status effects
    std::vector<StatusEffectInstance> m_statusEffects;

    // Auras
    std::vector<HeroAura> m_auras;

    // Movement
    bool m_isMoving = false;
    glm::vec3 m_moveTarget{0.0f};

    // Revival
    RevivalState m_revival;
    float m_baseRespawnTime = 30.0f;

    // Callbacks
    DeathCallback m_onDeath;
    ReviveCallback m_onRevive;
    LevelUpCallback m_onLevelUp;
    AbilityCallback m_onAbilityCast;
};

} // namespace RTS
} // namespace Vehement
