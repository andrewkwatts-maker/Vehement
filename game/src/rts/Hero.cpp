#include "Hero.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace RTS {

// ============================================================================
// Constructor / Initialization
// ============================================================================

Hero::Hero() : Entity(EntityType::Player) {
    m_heroClass = HeroClass::Warlord;
    InitializeFromClass();
}

Hero::Hero(HeroClass heroClass) : Entity(EntityType::Player), m_heroClass(heroClass) {
    InitializeFromClass();
}

void Hero::InitializeFromClass() {
    const HeroClassDefinition& classDef = GetClassDefinition();

    // Set hero name to class name by default
    m_heroName = classDef.name;

    // Initialize base stats from class
    m_baseStats = classDef.baseStats;

    // Initialize health and mana
    m_baseHealth = classDef.baseHealth;
    m_baseMana = classDef.baseMana;
    m_health = GetMaxHealth();
    m_mana = GetMaxMana();

    // Set movement speed
    m_moveSpeed = 8.0f * (1.0f + classDef.passives.moveSpeedBonus);

    // Set collision radius (heroes are slightly larger)
    m_collisionRadius = 0.6f;

    // Set texture
    m_texturePath = classDef.texturePath;

    // Initialize abilities from class starting abilities
    for (int i = 0; i < ABILITY_SLOT_COUNT && i < static_cast<int>(classDef.startingAbilities.size()); ++i) {
        if (classDef.startingAbilities[i] >= 0) {
            m_abilities[i].abilityId = classDef.startingAbilities[i];
            m_abilities[i].currentLevel = 0; // Not learned yet
        }
    }

    // Set up inventory owner
    m_inventory.SetOwner(this);

    // Set up experience callbacks
    m_experience.SetOnLevelUp([this](int level, int statPoints, int abilityPoints) {
        HandleLevelUp(level, statPoints, abilityPoints);
    });

    // Set respawn position to current position
    m_revival.respawnPosition = m_position;
}

// ============================================================================
// Update / Render
// ============================================================================

void Hero::Update(float deltaTime) {
    // Don't update if dead
    if (m_revival.isDead) {
        UpdateRevival(deltaTime);
        return;
    }

    // Update abilities (cooldowns, channeling, toggles)
    UpdateAbilities(deltaTime);

    // Update status effects
    UpdateStatusEffects(deltaTime);

    // Update health/mana regeneration
    UpdateRegen(deltaTime);

    // Update movement
    UpdateMovement(deltaTime);

    // Update inventory cooldowns
    m_inventory.Update(deltaTime);

    // Call base update
    Entity::Update(deltaTime);
}

void Hero::Render(Nova::Renderer& renderer) {
    // Don't render if dead
    if (m_revival.isDead) {
        return;
    }

    // Could add special rendering for status effects, auras, etc.
    Entity::Render(renderer);
}

void Hero::UpdateAbilities(float deltaTime) {
    for (int i = 0; i < ABILITY_SLOT_COUNT; ++i) {
        AbilityState& state = m_abilities[i];

        // Update cooldowns
        if (state.cooldownRemaining > 0.0f) {
            // Apply cooldown reduction from intelligence
            float cdr = 1.0f - GetStats().GetCooldownReduction();
            cdr = std::max(0.5f, cdr); // Cap at 50% CDR
            state.cooldownRemaining -= deltaTime / cdr;
            if (state.cooldownRemaining < 0.0f) {
                state.cooldownRemaining = 0.0f;
            }
        }

        // Update channeling
        if (state.isChanneling && m_channelingAbility == i) {
            state.channelTimeRemaining -= deltaTime;

            // Continue channeling effect
            const AbilityData* data = AbilityManager::Instance().GetAbility(state.abilityId);
            AbilityBehavior* behavior = AbilityManager::Instance().GetBehavior(state.abilityId);
            if (data && behavior) {
                AbilityCastContext context;
                context.caster = this;
                context.abilityLevel = state.currentLevel;
                context.deltaTime = deltaTime;
                behavior->Update(context, *data, deltaTime);
            }

            if (state.channelTimeRemaining <= 0.0f) {
                CancelChanneling();
            }
        }

        // Update toggle abilities (mana drain)
        if (state.isToggled) {
            const AbilityData* data = AbilityManager::Instance().GetAbility(state.abilityId);
            if (data) {
                // Drain mana over time for toggle abilities
                float manaDrain = data->GetLevelData(state.currentLevel).manaCost * 0.1f * deltaTime;
                if (!ConsumeMana(manaDrain)) {
                    // Not enough mana, disable toggle
                    ToggleAbility(static_cast<AbilitySlot>(i));
                }
            }
        }
    }
}

void Hero::UpdateStatusEffects(float deltaTime) {
    // Update durations and remove expired effects
    auto it = m_statusEffects.begin();
    while (it != m_statusEffects.end()) {
        it->duration -= deltaTime;
        if (it->IsExpired()) {
            it = m_statusEffects.erase(it);
        } else {
            ++it;
        }
    }

    // Apply damage-over-time effects
    for (const auto& effect : m_statusEffects) {
        if (effect.effect == StatusEffect::Burning) {
            TakeDamage(effect.strength * deltaTime, effect.sourceId);
        }
    }
}

void Hero::UpdateRegen(float deltaTime) {
    // Health regeneration
    float healthRegen = GetHealthRegen();
    if (healthRegen > 0.0f && m_health < GetMaxHealth()) {
        Heal(healthRegen * deltaTime);
    }

    // Mana regeneration
    float manaRegen = GetManaRegen();
    if (manaRegen > 0.0f && m_mana < GetMaxMana()) {
        AddMana(manaRegen * deltaTime);
    }
}

void Hero::UpdateMovement(float deltaTime) {
    if (!m_isMoving) {
        return;
    }

    // Check for movement-preventing status effects
    if (HasStatusEffect(StatusEffect::Frozen) || HasStatusEffect(StatusEffect::Stunned)) {
        return;
    }

    glm::vec3 toTarget = m_moveTarget - m_position;
    toTarget.y = 0.0f; // Keep on ground plane

    float distance = glm::length(toTarget);
    float speed = GetEffectiveMoveSpeed();

    if (distance < 0.1f) {
        // Reached target
        m_isMoving = false;
        m_velocity = glm::vec3(0.0f);
        return;
    }

    // Move towards target
    glm::vec3 direction = glm::normalize(toTarget);
    m_velocity = direction * speed;

    // Face movement direction
    LookAt(m_moveTarget);

    // Update position
    m_position += m_velocity * deltaTime;

    // Check if we overshot
    glm::vec3 newToTarget = m_moveTarget - m_position;
    if (glm::dot(toTarget, newToTarget) < 0.0f) {
        m_position = m_moveTarget;
        m_isMoving = false;
        m_velocity = glm::vec3(0.0f);
    }
}

void Hero::UpdateRevival(float deltaTime) {
    if (!m_revival.isDead) {
        return;
    }

    m_revival.deathTimer += deltaTime;

    // Check for auto-revive
    if (m_revival.autoRevive && m_revival.deathTimer >= m_revival.respawnTime) {
        Revive();
    }
}

// ============================================================================
// Hero Class
// ============================================================================

const HeroClassDefinition& Hero::GetClassDefinition() const {
    return HeroClassRegistry::Instance().GetClass(m_heroClass);
}

const std::string& Hero::GetHeroName() const {
    return m_heroName.empty() ? GetClassDefinition().name : m_heroName;
}

// ============================================================================
// Stats
// ============================================================================

HeroStats Hero::GetStats() const {
    HeroStats total = m_baseStats;

    // Add per-level stat gains
    const HeroClassDefinition& classDef = GetClassDefinition();
    int level = GetLevel();
    total.strength += classDef.statGains.strengthPerLevel * (level - 1);
    total.agility += classDef.statGains.agilityPerLevel * (level - 1);
    total.intelligence += classDef.statGains.intelligencePerLevel * (level - 1);

    // Add bonus stats (from buffs)
    total = total + m_bonusStats;

    // Add item stats
    const ItemStats& itemStats = m_inventory.GetCachedStats();
    total.strength += itemStats.strength;
    total.agility += itemStats.agility;
    total.intelligence += itemStats.intelligence;

    // Add status effect bonuses
    if (HasStatusEffect(StatusEffect::Inspired)) {
        float bonus = GetStatusEffectStrength(StatusEffect::Inspired);
        total.strength *= (1.0f + bonus);
        total.agility *= (1.0f + bonus);
        total.intelligence *= (1.0f + bonus);
    }

    return total;
}

void Hero::AddStatPoints(float strength, float agility, float intelligence) {
    m_baseStats.strength += strength;
    m_baseStats.agility += agility;
    m_baseStats.intelligence += intelligence;
}

bool Hero::AllocateStatPoint(int statIndex) {
    if (!m_experience.SpendStatPoint()) {
        return false;
    }

    switch (statIndex) {
        case 0: m_baseStats.strength += 1.0f; break;
        case 1: m_baseStats.agility += 1.0f; break;
        case 2: m_baseStats.intelligence += 1.0f; break;
        default: return false;
    }

    return true;
}

// ============================================================================
// Health & Mana
// ============================================================================

float Hero::GetMaxHealth() const noexcept {
    const HeroClassDefinition& classDef = GetClassDefinition();
    HeroStats stats = GetStats();

    float maxHealth = m_baseHealth;
    maxHealth += stats.GetBonusHealth();
    maxHealth += m_inventory.GetCachedStats().health;
    maxHealth *= (1.0f + classDef.passives.buildingHealthBonus); // Some classes boost own health

    return maxHealth;
}

float Hero::GetMaxMana() const noexcept {
    HeroStats stats = GetStats();

    float maxMana = m_baseMana;
    maxMana += stats.GetBonusMana();
    maxMana += m_inventory.GetCachedStats().mana;

    return maxMana;
}

void Hero::SetMana(float mana) {
    m_mana = std::clamp(mana, 0.0f, GetMaxMana());
}

void Hero::AddMana(float amount) {
    SetMana(m_mana + amount);
}

bool Hero::ConsumeMana(float amount) {
    if (m_mana >= amount) {
        m_mana -= amount;
        return true;
    }
    return false;
}

float Hero::GetHealthRegen() const {
    const HeroClassDefinition& classDef = GetClassDefinition();
    HeroStats stats = GetStats();

    float regen = 1.0f; // Base regen
    regen += stats.strength * HEALTH_REGEN_PER_STR;
    regen += classDef.passives.healthRegenBonus;
    regen += m_inventory.GetCachedStats().healthRegen;

    // Regeneration buff
    if (HasStatusEffect(StatusEffect::Regeneration)) {
        regen += GetStatusEffectStrength(StatusEffect::Regeneration);
    }

    return regen;
}

float Hero::GetManaRegen() const {
    HeroStats stats = GetStats();

    float regen = 0.5f; // Base regen
    regen += stats.intelligence * MANA_REGEN_PER_INT;
    regen += m_inventory.GetCachedStats().manaRegen;

    return regen;
}

// ============================================================================
// Combat
// ============================================================================

float Hero::GetAttackDamage() const {
    const HeroClassDefinition& classDef = GetClassDefinition();
    HeroStats stats = GetStats();

    float damage = 10.0f; // Base damage
    damage += stats.GetBonusMeleeDamage();
    damage += m_inventory.GetCachedStats().damage;
    damage *= (1.0f + classDef.passives.damageBonus);

    // Might buff
    if (HasStatusEffect(StatusEffect::Might)) {
        damage *= (1.0f + GetStatusEffectStrength(StatusEffect::Might));
    }

    // Weakened debuff
    if (HasStatusEffect(StatusEffect::Weakened)) {
        damage *= (1.0f - GetStatusEffectStrength(StatusEffect::Weakened));
    }

    return damage;
}

float Hero::GetAttackSpeed() const {
    HeroStats stats = GetStats();

    float attackSpeed = BASE_ATTACK_SPEED;
    attackSpeed *= (1.0f + stats.GetAttackSpeedBonus());
    attackSpeed *= (1.0f + m_inventory.GetCachedStats().attackSpeed);

    // Haste buff
    if (HasStatusEffect(StatusEffect::Haste)) {
        attackSpeed *= (1.0f + GetStatusEffectStrength(StatusEffect::Haste) * 0.5f);
    }

    // Slowed debuff
    if (HasStatusEffect(StatusEffect::Slowed)) {
        attackSpeed *= (1.0f - GetStatusEffectStrength(StatusEffect::Slowed) * 0.5f);
    }

    return attackSpeed;
}

float Hero::GetArmor() const {
    const HeroClassDefinition& classDef = GetClassDefinition();

    float armor = classDef.baseArmor;
    armor += classDef.passives.armorBonus;
    armor += m_inventory.GetCachedStats().armor;

    // Fortified buff
    if (HasStatusEffect(StatusEffect::Fortified)) {
        armor += GetStatusEffectStrength(StatusEffect::Fortified);
    }

    // Vulnerable debuff
    if (HasStatusEffect(StatusEffect::Vulnerable)) {
        armor -= GetStatusEffectStrength(StatusEffect::Vulnerable);
    }

    return std::max(0.0f, armor);
}

float Hero::GetDamageReduction() const {
    float armor = GetArmor();
    // Armor formula: reduction = armor / (armor + 100)
    // 10 armor = 9% reduction, 50 armor = 33% reduction, 100 armor = 50% reduction
    return armor / (armor + 100.0f);
}

float Hero::TakeDamage(float amount, EntityId source) {
    // Check for stun immunity or shields
    if (HasStatusEffect(StatusEffect::Shield)) {
        float shieldStrength = GetStatusEffectStrength(StatusEffect::Shield);
        if (shieldStrength >= amount) {
            // Shield absorbs all damage
            // Reduce shield strength
            for (auto& effect : m_statusEffects) {
                if (effect.effect == StatusEffect::Shield) {
                    effect.strength -= amount;
                    if (effect.strength <= 0.0f) {
                        effect.duration = 0.0f; // Remove shield
                    }
                    break;
                }
            }
            return 0.0f;
        } else {
            // Shield absorbs partial damage
            amount -= shieldStrength;
            RemoveStatusEffect(StatusEffect::Shield);
        }
    }

    // Apply armor reduction
    float reduction = GetDamageReduction();
    float actualDamage = amount * (1.0f - reduction);

    // Apply dodge chance from agility
    HeroStats stats = GetStats();
    float dodgeChance = stats.GetDodgeChance();
    if (dodgeChance > 0.0f) {
        // Simple random check (would use proper RNG in real implementation)
        float roll = static_cast<float>(rand()) / RAND_MAX;
        if (roll < dodgeChance) {
            return 0.0f; // Dodged!
        }
    }

    // Cancel channeling on damage
    if (m_channelingAbility >= 0) {
        CancelChanneling();
    }

    return Entity::TakeDamage(actualDamage, source);
}

void Hero::Die() {
    Entity::Die();

    m_revival.isDead = true;
    m_revival.deathTimer = 0.0f;
    m_revival.respawnTime = CalculateRespawnTime();

    // Clear status effects
    ClearStatusEffects();

    // Cancel any active abilities
    CancelChanneling();
    for (auto& ability : m_abilities) {
        ability.isToggled = false;
    }

    if (m_onDeath) {
        m_onDeath(*this);
    }
}

// ============================================================================
// Experience & Leveling
// ============================================================================

int Hero::AddExperience(int amount, ExperienceSource source, int enemyLevel) {
    // Apply item XP bonus
    float xpMultiplier = 1.0f + m_inventory.GetCachedStats().experienceBonus;
    amount = static_cast<int>(amount * xpMultiplier);

    return m_experience.AddExperience(amount, source, enemyLevel);
}

void Hero::HandleLevelUp(int newLevel, int statPoints, int abilityPoints) {
    // Increase base health and mana slightly per level
    m_baseHealth += 20.0f;
    m_baseMana += 10.0f;

    // Restore health and mana on level up
    m_health = GetMaxHealth();
    m_mana = GetMaxMana();

    if (m_onLevelUp) {
        m_onLevelUp(*this, newLevel);
    }
}

// ============================================================================
// Abilities
// ============================================================================

const AbilityState& Hero::GetAbilityState(AbilitySlot slot) const {
    static AbilityState empty;
    int index = static_cast<int>(slot);
    if (index < 0 || index >= ABILITY_SLOT_COUNT) {
        return empty;
    }
    return m_abilities[index];
}

const AbilityData* Hero::GetAbilityData(AbilitySlot slot) const {
    const AbilityState& state = GetAbilityState(slot);
    if (state.abilityId < 0) {
        return nullptr;
    }
    return AbilityManager::Instance().GetAbility(state.abilityId);
}

void Hero::SetAbility(AbilitySlot slot, int abilityId) {
    int index = static_cast<int>(slot);
    if (index < 0 || index >= ABILITY_SLOT_COUNT) {
        return;
    }
    m_abilities[index].abilityId = abilityId;
    m_abilities[index].currentLevel = 0;
    m_abilities[index].cooldownRemaining = 0.0f;
    m_abilities[index].isToggled = false;
    m_abilities[index].isChanneling = false;
}

bool Hero::LevelUpAbility(AbilitySlot slot) {
    int index = static_cast<int>(slot);
    if (index < 0 || index >= ABILITY_SLOT_COUNT) {
        return false;
    }

    AbilityState& state = m_abilities[index];
    if (state.abilityId < 0) {
        return false;
    }

    const AbilityData* data = AbilityManager::Instance().GetAbility(state.abilityId);
    if (!data) {
        return false;
    }

    // Check if can level up
    if (state.IsMaxLevel(*data)) {
        return false;
    }

    // Check if we have ability points
    if (!m_experience.SpendAbilityPoint()) {
        return false;
    }

    // Check hero level requirement
    int requiredLevel = data->requiredLevel + state.currentLevel * 2;
    if (GetLevel() < requiredLevel) {
        // Refund the point
        m_experience.AddAbilityPoints(1);
        return false;
    }

    state.currentLevel++;
    return true;
}

AbilityCastResult Hero::CastAbility(AbilitySlot slot) {
    AbilityCastContext context;
    context.caster = this;
    context.targetPoint = m_position;
    context.direction = GetForward();

    return ExecuteAbility(slot, context);
}

AbilityCastResult Hero::CastAbilityAtPoint(AbilitySlot slot, const glm::vec3& point) {
    AbilityCastContext context;
    context.caster = this;
    context.targetPoint = point;
    context.direction = glm::normalize(point - m_position);

    return ExecuteAbility(slot, context);
}

AbilityCastResult Hero::CastAbilityOnTarget(AbilitySlot slot, Entity* target) {
    AbilityCastContext context;
    context.caster = this;
    context.targetUnit = target;
    if (target) {
        context.targetPoint = target->GetPosition();
        context.direction = glm::normalize(target->GetPosition() - m_position);
    }

    return ExecuteAbility(slot, context);
}

AbilityCastResult Hero::ExecuteAbility(AbilitySlot slot, const AbilityCastContext& context) {
    AbilityCastResult result;
    result.success = false;

    int index = static_cast<int>(slot);
    if (index < 0 || index >= ABILITY_SLOT_COUNT) {
        result.failReason = "Invalid slot";
        return result;
    }

    AbilityState& state = m_abilities[index];
    if (!state.IsLearned()) {
        result.failReason = "Ability not learned";
        return result;
    }

    if (!state.IsReady()) {
        result.failReason = "Ability on cooldown";
        return result;
    }

    // Check for silenced
    if (HasStatusEffect(StatusEffect::Silenced)) {
        result.failReason = "Silenced";
        return result;
    }

    // Check for stunned/frozen
    if (HasStatusEffect(StatusEffect::Stunned) || HasStatusEffect(StatusEffect::Frozen)) {
        result.failReason = "Cannot act";
        return result;
    }

    const AbilityData* data = AbilityManager::Instance().GetAbility(state.abilityId);
    if (!data) {
        result.failReason = "Invalid ability data";
        return result;
    }

    AbilityBehavior* behavior = AbilityManager::Instance().GetBehavior(state.abilityId);
    if (!behavior) {
        result.failReason = "No behavior";
        return result;
    }

    // Set up context with ability level
    AbilityCastContext castContext = context;
    castContext.abilityLevel = state.currentLevel;

    // Check if can cast
    if (!behavior->CanCast(castContext, *data)) {
        result.failReason = "Cannot cast";
        return result;
    }

    // Execute the ability
    result = behavior->Execute(castContext, *data);

    if (result.success) {
        // Apply cooldown
        const auto& levelData = data->GetLevelData(state.currentLevel);
        state.cooldownRemaining = levelData.cooldown;

        // Handle channeled abilities
        if (data->type == AbilityType::Channeled) {
            state.isChanneling = true;
            state.channelTimeRemaining = levelData.duration;
            m_channelingAbility = index;
        }

        // Notify
        if (m_onAbilityCast) {
            m_onAbilityCast(*this, slot, result);
        }
    }

    return result;
}

bool Hero::CanCastAbility(AbilitySlot slot) const {
    const AbilityState& state = GetAbilityState(slot);
    if (!state.IsLearned() || !state.IsReady()) {
        return false;
    }

    if (HasStatusEffect(StatusEffect::Silenced) ||
        HasStatusEffect(StatusEffect::Stunned) ||
        HasStatusEffect(StatusEffect::Frozen)) {
        return false;
    }

    const AbilityData* data = GetAbilityData(slot);
    if (!data) {
        return false;
    }

    const auto& levelData = data->GetLevelData(state.currentLevel);
    return m_mana >= levelData.manaCost;
}

float Hero::GetAbilityCooldown(AbilitySlot slot) const {
    return GetAbilityState(slot).cooldownRemaining;
}

bool Hero::ToggleAbility(AbilitySlot slot) {
    int index = static_cast<int>(slot);
    if (index < 0 || index >= ABILITY_SLOT_COUNT) {
        return false;
    }

    AbilityState& state = m_abilities[index];
    const AbilityData* data = AbilityManager::Instance().GetAbility(state.abilityId);
    if (!data || data->type != AbilityType::Toggle) {
        return false;
    }

    if (!state.IsLearned()) {
        return false;
    }

    if (state.isToggled) {
        // Turn off
        state.isToggled = false;
        AbilityBehavior* behavior = AbilityManager::Instance().GetBehavior(state.abilityId);
        if (behavior) {
            AbilityCastContext context;
            context.caster = this;
            context.abilityLevel = state.currentLevel;
            behavior->OnEnd(context, *data);
        }
    } else {
        // Turn on
        if (!CanCastAbility(slot)) {
            return false;
        }
        state.isToggled = true;
        // Initial mana cost
        const auto& levelData = data->GetLevelData(state.currentLevel);
        ConsumeMana(levelData.manaCost);
    }

    return true;
}

void Hero::CancelChanneling() {
    if (m_channelingAbility < 0) {
        return;
    }

    AbilityState& state = m_abilities[m_channelingAbility];
    state.isChanneling = false;

    const AbilityData* data = AbilityManager::Instance().GetAbility(state.abilityId);
    AbilityBehavior* behavior = AbilityManager::Instance().GetBehavior(state.abilityId);
    if (data && behavior) {
        AbilityCastContext context;
        context.caster = this;
        context.abilityLevel = state.currentLevel;
        behavior->OnEnd(context, *data);
    }

    m_channelingAbility = -1;
}

// ============================================================================
// Inventory
// ============================================================================

bool Hero::UseItem(int slot) {
    return m_inventory.UseItem(slot);
}

// ============================================================================
// Status Effects
// ============================================================================

void Hero::ApplyStatusEffect(StatusEffect effect, float duration, float strength, uint32_t sourceId) {
    // Check if already has this effect
    for (auto& existing : m_statusEffects) {
        if (existing.effect == effect) {
            // Refresh duration if new is longer, take stronger effect
            existing.duration = std::max(existing.duration, duration);
            existing.strength = std::max(existing.strength, strength);
            return;
        }
    }

    StatusEffectInstance instance;
    instance.effect = effect;
    instance.duration = duration;
    instance.strength = strength;
    instance.sourceId = sourceId;
    m_statusEffects.push_back(instance);
}

void Hero::RemoveStatusEffect(StatusEffect effect) {
    auto it = std::remove_if(m_statusEffects.begin(), m_statusEffects.end(),
        [effect](const StatusEffectInstance& e) { return e.effect == effect; });
    m_statusEffects.erase(it, m_statusEffects.end());
}

bool Hero::HasStatusEffect(StatusEffect effect) const {
    for (const auto& e : m_statusEffects) {
        if (e.effect == effect) {
            return true;
        }
    }
    return false;
}

float Hero::GetStatusEffectStrength(StatusEffect effect) const {
    for (const auto& e : m_statusEffects) {
        if (e.effect == effect) {
            return e.strength;
        }
    }
    return 0.0f;
}

void Hero::ClearStatusEffects() {
    m_statusEffects.clear();
}

// ============================================================================
// Auras
// ============================================================================

void Hero::AddAura(const HeroAura& aura) {
    m_auras.push_back(aura);
}

void Hero::RemoveAura(const std::string& auraName) {
    auto it = std::remove_if(m_auras.begin(), m_auras.end(),
        [&auraName](const HeroAura& a) { return a.name == auraName; });
    m_auras.erase(it, m_auras.end());
}

float Hero::GetCommandRadius() const {
    const HeroClassDefinition& classDef = GetClassDefinition();
    float radius = classDef.baseCommandRadius;
    radius *= (1.0f + classDef.passives.commandRadiusBonus);
    radius += m_inventory.GetCachedStats().commandRadius;
    return radius;
}

float Hero::GetAuraRadius() const {
    const HeroClassDefinition& classDef = GetClassDefinition();
    float radius = classDef.baseAuraRadius;
    radius *= (1.0f + classDef.passives.auraRadiusBonus);
    return radius;
}

float Hero::GetVisionRange() const {
    const HeroClassDefinition& classDef = GetClassDefinition();
    float range = classDef.baseVisionRange;
    range *= (1.0f + classDef.passives.visionRangeBonus);
    range += m_inventory.GetCachedStats().visionRange;
    return range;
}

// ============================================================================
// Movement
// ============================================================================

float Hero::GetEffectiveMoveSpeed() const {
    float speed = m_moveSpeed;
    speed *= (1.0f + GetStats().GetMoveSpeedBonus());
    speed *= (1.0f + m_inventory.GetCachedStats().moveSpeed);

    // Haste buff
    if (HasStatusEffect(StatusEffect::Haste)) {
        speed *= (1.0f + GetStatusEffectStrength(StatusEffect::Haste));
    }

    // Slowed debuff
    if (HasStatusEffect(StatusEffect::Slowed)) {
        speed *= (1.0f - GetStatusEffectStrength(StatusEffect::Slowed));
    }

    return speed;
}

void Hero::MoveTo(const glm::vec3& target) {
    m_moveTarget = target;
    m_moveTarget.y = m_groundLevel;
    m_isMoving = true;
}

void Hero::Stop() {
    m_isMoving = false;
    m_velocity = glm::vec3(0.0f);
}

// ============================================================================
// Revival
// ============================================================================

void Hero::Revive(const glm::vec3& position) {
    m_position = position;
    m_revival.isDead = false;
    m_revival.deathTimer = 0.0f;

    // Restore health and mana
    m_health = GetMaxHealth();
    m_mana = GetMaxMana();

    // Re-enable
    m_active = true;
    m_markedForRemoval = false;

    if (m_onRevive) {
        m_onRevive(*this);
    }
}

void Hero::Revive() {
    Revive(m_revival.respawnPosition);
}

float Hero::CalculateRespawnTime() const {
    // Respawn time increases with level
    // Base time + 5 seconds per level
    return m_baseRespawnTime + (GetLevel() - 1) * 5.0f;
}

// ============================================================================
// Reset
// ============================================================================

void Hero::Reset() {
    InitializeFromClass();
    m_experience.Reset();
    m_inventory.Clear();
    m_statusEffects.clear();
    m_auras.clear();
    m_revival = RevivalState{};
    m_isMoving = false;
    m_channelingAbility = -1;
}

} // namespace RTS
} // namespace Vehement
