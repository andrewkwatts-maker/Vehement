#include "HeroInstance.hpp"
#include "HeroDefinition.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Vehement {
namespace Heroes {

// Static member initialization
uint32_t HeroInstance::s_nextInstanceId = 1;

// ============================================================================
// HeroInstance Implementation
// ============================================================================

HeroInstance::HeroInstance()
    : m_instanceId(s_nextInstanceId++)
{
    m_talentChoices.fill(-1);
}

HeroInstance::HeroInstance(const std::string& definitionId)
    : m_instanceId(s_nextInstanceId++)
{
    m_talentChoices.fill(-1);
    Initialize(definitionId);
}

HeroInstance::HeroInstance(std::shared_ptr<HeroDefinition> definition)
    : m_instanceId(s_nextInstanceId++)
{
    m_talentChoices.fill(-1);
    Initialize(definition);
}

HeroInstance::~HeroInstance() = default;

bool HeroInstance::Initialize(const std::string& definitionId) {
    auto definition = HeroDefinitionRegistry::Instance().Get(definitionId);
    if (!definition) {
        return false;
    }
    return Initialize(definition);
}

bool HeroInstance::Initialize(std::shared_ptr<HeroDefinition> definition) {
    if (!definition) {
        return false;
    }

    m_definition = definition;
    m_definitionId = definition->GetId();

    // Initialize abilities from definition
    const auto& abilityBindings = definition->GetAbilities();
    for (const auto& binding : abilityBindings) {
        if (binding.slot >= 1 && binding.slot <= ABILITY_SLOT_COUNT) {
            int idx = binding.slot - 1;
            m_abilities[idx].abilityId = binding.abilityId;
            m_abilities[idx].currentLevel = 0;
            m_abilities[idx].maxLevel = binding.maxLevel;
        }
    }

    // Set initial stats
    Reset();

    return true;
}

void HeroInstance::Reset() {
    m_level = 1;
    m_experience = 0;
    m_abilityPoints = 1; // Start with 1 ability point

    // Reset abilities
    for (auto& ability : m_abilities) {
        ability.currentLevel = 0;
        ability.cooldownRemaining = 0.0f;
        ability.charges = ability.maxCharges;
        ability.isToggled = false;
        ability.isAutocast = false;
    }

    // Clear items
    for (auto& item : m_items) {
        item.Clear();
    }

    // Reset talents
    m_talentChoices.fill(-1);

    // Reset combat stats
    m_combatStats.Reset();

    // Reset respawn state
    m_respawnState = RespawnState{};

    // Reset bonus stats
    m_bonusStrength = 0.0f;
    m_bonusAgility = 0.0f;
    m_bonusIntelligence = 0.0f;
    m_bonusDamage = 0.0f;
    m_bonusArmor = 0.0f;
    m_bonusMoveSpeed = 0.0f;
    m_bonusAttackSpeed = 0.0f;
    m_bonusHealthRegen = 0.0f;
    m_bonusManaRegen = 0.0f;

    // Set initial health and mana
    m_currentHealth = GetMaxHealth();
    m_currentMana = GetMaxMana();

    m_gameTime = 0.0f;
}

// ============================================================================
// Level and Experience
// ============================================================================

int HeroInstance::GetExperienceToNextLevel() const {
    if (m_level >= MAX_LEVEL) return 0;
    return CalculateXPForLevel(m_level + 1) - CalculateXPForLevel(m_level);
}

float HeroInstance::GetLevelProgress() const {
    if (m_level >= MAX_LEVEL) return 1.0f;
    int currentLevelXP = CalculateXPForLevel(m_level);
    int nextLevelXP = CalculateXPForLevel(m_level + 1);
    int xpInLevel = m_experience - currentLevelXP;
    int xpNeeded = nextLevelXP - currentLevelXP;
    return xpNeeded > 0 ? static_cast<float>(xpInLevel) / xpNeeded : 1.0f;
}

int HeroInstance::AddExperience(int amount) {
    if (m_level >= MAX_LEVEL || amount <= 0) return 0;

    m_experience += amount;
    int levelsGained = 0;

    while (m_level < MAX_LEVEL && m_experience >= CalculateXPForLevel(m_level + 1)) {
        int oldLevel = m_level;
        m_level++;
        m_abilityPoints++;
        levelsGained++;
        OnLevelUp(oldLevel, m_level);
    }

    return levelsGained;
}

void HeroInstance::SetLevel(int level) {
    int targetLevel = std::clamp(level, 1, MAX_LEVEL);
    int oldLevel = m_level;

    m_level = targetLevel;
    m_experience = CalculateXPForLevel(targetLevel);

    // Recalculate ability points
    m_abilityPoints = targetLevel;
    for (const auto& ability : m_abilities) {
        m_abilityPoints -= ability.currentLevel;
    }

    if (targetLevel > oldLevel) {
        OnLevelUp(oldLevel, targetLevel);
    }
}

int HeroInstance::CalculateXPForLevel(int level) const {
    // XP curve: each level requires more XP
    // Level 1: 0 XP, Level 2: 100 XP, Level 3: 300 XP, etc.
    if (level <= 1) return 0;
    return 100 * (level - 1) * level / 2;
}

void HeroInstance::OnLevelUp(int oldLevel, int newLevel) {
    // Update max health/mana
    float healthPercent = GetHealthPercent();
    float manaPercent = GetManaPercent();

    // Restore proportional health/mana
    m_currentHealth = GetMaxHealth() * healthPercent;
    m_currentMana = GetMaxMana() * manaPercent;

    if (m_onLevelUp) {
        m_onLevelUp(*this, newLevel);
    }
}

// ============================================================================
// Stats Calculation
// ============================================================================

float HeroInstance::GetMaxHealth() const {
    if (!m_definition) return 100.0f;

    auto stats = m_definition->CalculateStatsAtLevel(m_level);
    return stats.health + (m_bonusStrength * 20.0f);
}

float HeroInstance::GetMaxMana() const {
    if (!m_definition) return 50.0f;

    auto stats = m_definition->CalculateStatsAtLevel(m_level);
    return stats.mana + (m_bonusIntelligence * 12.0f);
}

float HeroInstance::GetHealthPercent() const {
    float maxHealth = GetMaxHealth();
    return maxHealth > 0.0f ? m_currentHealth / maxHealth : 0.0f;
}

float HeroInstance::GetManaPercent() const {
    float maxMana = GetMaxMana();
    return maxMana > 0.0f ? m_currentMana / maxMana : 0.0f;
}

void HeroInstance::SetHealth(float health) {
    m_currentHealth = std::clamp(health, 0.0f, GetMaxHealth());
}

void HeroInstance::SetMana(float mana) {
    m_currentMana = std::clamp(mana, 0.0f, GetMaxMana());
}

void HeroInstance::AddHealth(float amount) {
    SetHealth(m_currentHealth + amount);
}

void HeroInstance::AddMana(float amount) {
    SetMana(m_currentMana + amount);
}

float HeroInstance::TakeDamage(float amount, uint32_t sourceId) {
    if (IsDead() || amount <= 0.0f) return 0.0f;

    // Calculate damage reduction from armor
    float armor = GetArmor();
    float reduction = armor / (armor + 100.0f); // Armor formula
    float actualDamage = amount * (1.0f - reduction);

    m_currentHealth -= actualDamage;
    RecordDamageTaken(actualDamage);

    if (m_currentHealth <= 0.0f) {
        m_currentHealth = 0.0f;
        Die(sourceId);
    }

    return actualDamage;
}

bool HeroInstance::ConsumeMana(float amount) {
    if (m_currentMana < amount) return false;
    m_currentMana -= amount;
    return true;
}

float HeroInstance::GetDamage() const {
    if (!m_definition) return 10.0f;
    auto stats = m_definition->CalculateStatsAtLevel(m_level);

    // Primary attribute bonus to damage
    float attrBonus = 0.0f;
    switch (m_definition->GetPrimaryAttribute()) {
        case PrimaryAttribute::Strength:
            attrBonus = GetStrength();
            break;
        case PrimaryAttribute::Agility:
            attrBonus = GetAgility();
            break;
        case PrimaryAttribute::Intelligence:
            attrBonus = GetIntelligence();
            break;
    }

    return stats.damage + attrBonus + m_bonusDamage;
}

float HeroInstance::GetArmor() const {
    if (!m_definition) return 0.0f;
    auto stats = m_definition->CalculateStatsAtLevel(m_level);
    return stats.armor + (GetAgility() * 0.17f) + m_bonusArmor;
}

float HeroInstance::GetMagicResist() const {
    if (!m_definition) return 0.0f;
    auto stats = m_definition->CalculateStatsAtLevel(m_level);
    return stats.magicResist;
}

float HeroInstance::GetMoveSpeed() const {
    if (!m_definition) return 300.0f;
    auto stats = m_definition->CalculateStatsAtLevel(m_level);
    return stats.moveSpeed + m_bonusMoveSpeed;
}

float HeroInstance::GetAttackSpeed() const {
    if (!m_definition) return 1.0f;
    auto stats = m_definition->CalculateStatsAtLevel(m_level);
    float agiBonus = GetAgility() * 0.01f; // 1% per agility
    return stats.attackSpeed * (1.0f + agiBonus + m_bonusAttackSpeed);
}

float HeroInstance::GetAttackRange() const {
    if (!m_definition) return 1.5f;
    return m_definition->GetBaseStats().attackRange;
}

float HeroInstance::GetHealthRegen() const {
    if (!m_definition) return 1.0f;
    auto stats = m_definition->CalculateStatsAtLevel(m_level);
    return stats.healthRegen + (GetStrength() * 0.1f) + m_bonusHealthRegen;
}

float HeroInstance::GetManaRegen() const {
    if (!m_definition) return 0.5f;
    auto stats = m_definition->CalculateStatsAtLevel(m_level);
    return stats.manaRegen + (GetIntelligence() * 0.05f) + m_bonusManaRegen;
}

float HeroInstance::GetStrength() const {
    if (!m_definition) return 20.0f;
    auto stats = m_definition->CalculateStatsAtLevel(m_level);
    return stats.strength + m_bonusStrength;
}

float HeroInstance::GetAgility() const {
    if (!m_definition) return 15.0f;
    auto stats = m_definition->CalculateStatsAtLevel(m_level);
    return stats.agility + m_bonusAgility;
}

float HeroInstance::GetIntelligence() const {
    if (!m_definition) return 15.0f;
    auto stats = m_definition->CalculateStatsAtLevel(m_level);
    return stats.intelligence + m_bonusIntelligence;
}

// ============================================================================
// Abilities
// ============================================================================

const LearnedAbility& HeroInstance::GetAbility(int slot) const {
    static LearnedAbility empty;
    if (slot < 0 || slot >= ABILITY_SLOT_COUNT) return empty;
    return m_abilities[slot];
}

LearnedAbility& HeroInstance::GetAbility(int slot) {
    static LearnedAbility empty;
    if (slot < 0 || slot >= ABILITY_SLOT_COUNT) return empty;
    return m_abilities[slot];
}

bool HeroInstance::LearnAbility(int slot) {
    if (!CanLearnAbility(slot)) return false;

    auto& ability = m_abilities[slot];
    ability.currentLevel++;
    m_abilityPoints--;

    if (m_onAbilityLearn) {
        m_onAbilityLearn(*this, slot, ability.currentLevel);
    }

    return true;
}

bool HeroInstance::CanLearnAbility(int slot) const {
    if (slot < 0 || slot >= ABILITY_SLOT_COUNT) return false;
    if (m_abilityPoints <= 0) return false;

    const auto& ability = m_abilities[slot];
    if (ability.abilityId.empty()) return false;
    if (ability.IsMaxLevel()) return false;

    // Check unlock level for ultimate
    if (m_definition) {
        const auto* binding = m_definition->GetAbilitySlot(slot + 1);
        if (binding && binding->isUltimate) {
            // Ultimate requires specific level to learn
            int requiredLevel = binding->unlockLevel;
            // And can only be leveled at specific intervals
            int currentUltLevel = ability.currentLevel;
            int levelForNext = requiredLevel + (currentUltLevel * 6);
            if (m_level < levelForNext) return false;
        }
    }

    return true;
}

bool HeroInstance::IsAbilityReady(int slot) const {
    if (slot < 0 || slot >= ABILITY_SLOT_COUNT) return false;
    return m_abilities[slot].IsReady();
}

void HeroInstance::StartAbilityCooldown(int slot, float duration) {
    if (slot >= 0 && slot < ABILITY_SLOT_COUNT) {
        m_abilities[slot].cooldownRemaining = duration;
    }
}

void HeroInstance::ResetAbilityCooldown(int slot) {
    if (slot >= 0 && slot < ABILITY_SLOT_COUNT) {
        m_abilities[slot].cooldownRemaining = 0.0f;
    }
}

void HeroInstance::ToggleAbility(int slot) {
    if (slot >= 0 && slot < ABILITY_SLOT_COUNT) {
        m_abilities[slot].isToggled = !m_abilities[slot].isToggled;
    }
}

void HeroInstance::SetAutocast(int slot, bool enabled) {
    if (slot >= 0 && slot < ABILITY_SLOT_COUNT) {
        m_abilities[slot].isAutocast = enabled;
    }
}

bool HeroInstance::UseAbilityCharge(int slot) {
    if (slot < 0 || slot >= ABILITY_SLOT_COUNT) return false;
    auto& ability = m_abilities[slot];
    if (ability.charges <= 0) return false;
    ability.charges--;
    return true;
}

// ============================================================================
// Items
// ============================================================================

const ItemSlot& HeroInstance::GetItemSlot(int slot) const {
    static ItemSlot empty;
    if (slot < 0 || slot >= ITEM_SLOT_COUNT) return empty;
    return m_items[slot];
}

ItemSlot& HeroInstance::GetItemSlot(int slot) {
    static ItemSlot empty;
    if (slot < 0 || slot >= ITEM_SLOT_COUNT) return empty;
    return m_items[slot];
}

bool HeroInstance::EquipItem(int slot, const std::string& itemId) {
    if (slot < 0 || slot >= ITEM_SLOT_COUNT) return false;
    if (itemId.empty()) return false;

    m_items[slot].itemId = itemId;
    m_items[slot].isEmpty = false;
    m_items[slot].cooldown = 0.0f;

    return true;
}

std::string HeroInstance::UnequipItem(int slot) {
    if (slot < 0 || slot >= ITEM_SLOT_COUNT) return "";

    std::string itemId = m_items[slot].itemId;
    m_items[slot].Clear();
    return itemId;
}

void HeroInstance::SwapItems(int slot1, int slot2) {
    if (slot1 < 0 || slot1 >= ITEM_SLOT_COUNT) return;
    if (slot2 < 0 || slot2 >= ITEM_SLOT_COUNT) return;
    std::swap(m_items[slot1], m_items[slot2]);
}

int HeroInstance::FindEmptyItemSlot() const {
    for (int i = 0; i < ITEM_SLOT_COUNT; i++) {
        if (m_items[i].isEmpty) return i;
    }
    return -1;
}

bool HeroInstance::HasItem(const std::string& itemId) const {
    for (const auto& item : m_items) {
        if (!item.isEmpty && item.itemId == itemId) return true;
    }
    return false;
}

bool HeroInstance::UseItem(int slot) {
    if (slot < 0 || slot >= ITEM_SLOT_COUNT) return false;
    auto& item = m_items[slot];
    if (item.isEmpty) return false;
    if (item.cooldown > 0.0f) return false;

    // Consume charge if applicable
    if (item.charges > 0) {
        item.charges--;
        if (item.charges <= 0) {
            item.Clear();
        }
    }

    m_combatStats.itemsUsed++;
    return true;
}

// ============================================================================
// Talents
// ============================================================================

int HeroInstance::GetTalentChoice(int tier) const {
    if (tier < 0 || tier >= TALENT_TIER_COUNT) return -1;
    return m_talentChoices[tier];
}

bool HeroInstance::SelectTalent(int tier, int choice) {
    if (tier < 0 || tier >= TALENT_TIER_COUNT) return false;
    if (choice < 0 || choice > 1) return false;
    if (!IsTalentTierUnlocked(tier)) return false;
    if (HasSelectedTalent(tier)) return false;

    m_talentChoices[tier] = choice;

    if (m_onTalentSelect) {
        m_onTalentSelect(*this, tier, choice);
    }

    return true;
}

bool HeroInstance::IsTalentTierUnlocked(int tier) const {
    if (!m_definition) return false;
    if (tier < 0 || tier >= TALENT_TIER_COUNT) return false;

    int requiredLevel = m_definition->GetTalentUnlockLevel(tier);
    return m_level >= requiredLevel;
}

bool HeroInstance::HasSelectedTalent(int tier) const {
    if (tier < 0 || tier >= TALENT_TIER_COUNT) return false;
    return m_talentChoices[tier] >= 0;
}

std::string HeroInstance::GetSelectedTalentId(int tier) const {
    if (!m_definition) return "";
    if (tier < 0 || tier >= TALENT_TIER_COUNT) return "";

    int choice = m_talentChoices[tier];
    if (choice < 0) return "";

    const auto& talentTier = m_definition->GetTalentTier(tier);
    if (choice < 2) {
        return talentTier.choices[choice];
    }
    return "";
}

// ============================================================================
// Combat Stats
// ============================================================================

void HeroInstance::RecordKill(bool isHero) {
    m_combatStats.kills++;
    if (isHero) m_combatStats.heroKills++;
    else m_combatStats.creepKills++;
}

void HeroInstance::RecordDeath() {
    m_combatStats.deaths++;
}

void HeroInstance::RecordAssist() {
    m_combatStats.assists++;
}

void HeroInstance::RecordDamageDealt(float amount) {
    m_combatStats.damageDealt += amount;
}

void HeroInstance::RecordDamageTaken(float amount) {
    m_combatStats.damageTaken += amount;
}

void HeroInstance::RecordHealingDone(float amount) {
    m_combatStats.healingDone += amount;
}

void HeroInstance::RecordGoldEarned(float amount) {
    m_combatStats.goldEarned += amount;
}

// ============================================================================
// Death and Respawn
// ============================================================================

void HeroInstance::Die(uint32_t killerId) {
    if (IsDead()) return;

    m_respawnState.isDead = true;
    m_respawnState.timeOfDeath = m_gameTime;
    m_respawnState.deathPosition = m_position;

    float respawnTime = CalculateRespawnTime();
    m_respawnState.StartRespawn(respawnTime, m_respawnState.respawnPosition);

    // Calculate buyback cost
    m_respawnState.buybackCost = 100.0f + (m_level * 50.0f);
    m_respawnState.canBuyback = true;

    RecordDeath();

    if (m_onDeath) {
        m_onDeath(*this, killerId);
    }
}

void HeroInstance::Respawn() {
    Respawn(m_respawnState.respawnPosition);
}

void HeroInstance::Respawn(const glm::vec3& position) {
    m_respawnState.isDead = false;
    m_respawnState.respawnTimer = 0.0f;
    m_position = position;

    // Restore full health and mana
    m_currentHealth = GetMaxHealth();
    m_currentMana = GetMaxMana();

    // Reset all cooldowns
    for (auto& ability : m_abilities) {
        ability.cooldownRemaining = 0.0f;
        ability.charges = ability.maxCharges;
    }

    if (m_onRespawn) {
        m_onRespawn(*this);
    }
}

bool HeroInstance::Buyback() {
    if (!IsDead()) return false;
    if (!m_respawnState.canBuyback) return false;
    if (m_respawnState.buybackCooldown > 0.0f) return false;

    // In a real implementation, check gold and deduct buyback cost
    m_combatStats.goldSpent += m_respawnState.buybackCost;

    // Set buyback cooldown (8 minutes)
    m_respawnState.buybackCooldown = 480.0f;
    m_respawnState.canBuyback = false;

    Respawn();
    return true;
}

void HeroInstance::SetRespawnPosition(const glm::vec3& position) {
    m_respawnState.respawnPosition = position;
}

float HeroInstance::CalculateRespawnTime() const {
    // Respawn time scales with level
    // Base: 5 seconds + 2 seconds per level
    return 5.0f + (m_level * 2.0f);
}

// ============================================================================
// Update
// ============================================================================

void HeroInstance::Update(float deltaTime) {
    m_gameTime += deltaTime;

    if (IsDead()) {
        UpdateRespawn(deltaTime);
        m_combatStats.timeDead += deltaTime;
    } else {
        UpdateCooldowns(deltaTime);
        UpdateRegen(deltaTime);
        m_combatStats.timeAlive += deltaTime;
    }

    m_combatStats.timePlayed += deltaTime;
}

void HeroInstance::UpdateCooldowns(float deltaTime) {
    for (auto& ability : m_abilities) {
        if (ability.cooldownRemaining > 0.0f) {
            ability.cooldownRemaining = std::max(0.0f, ability.cooldownRemaining - deltaTime);
        }

        // Restore charges
        if (ability.maxCharges > 0 && ability.charges < ability.maxCharges) {
            ability.chargeRestoreTime -= deltaTime;
            if (ability.chargeRestoreTime <= 0.0f) {
                ability.charges++;
                ability.chargeRestoreTime = 10.0f; // Reset charge timer
            }
        }
    }

    // Item cooldowns
    for (auto& item : m_items) {
        if (!item.isEmpty && item.cooldown > 0.0f) {
            item.cooldown = std::max(0.0f, item.cooldown - deltaTime);
        }
    }
}

void HeroInstance::UpdateRegen(float deltaTime) {
    // Health regen
    float healthRegen = GetHealthRegen();
    AddHealth(healthRegen * deltaTime);

    // Mana regen
    float manaRegen = GetManaRegen();
    AddMana(manaRegen * deltaTime);
}

void HeroInstance::UpdateRespawn(float deltaTime) {
    if (m_respawnState.respawnTimer > 0.0f) {
        m_respawnState.respawnTimer -= deltaTime;

        if (m_respawnState.respawnTimer <= 0.0f) {
            Respawn();
        }
    }

    // Buyback cooldown
    if (m_respawnState.buybackCooldown > 0.0f) {
        m_respawnState.buybackCooldown -= deltaTime;
        if (m_respawnState.buybackCooldown <= 0.0f) {
            m_respawnState.canBuyback = true;
        }
    }
}

// ============================================================================
// Serialization
// ============================================================================

std::string HeroInstance::ToJson() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"instance_id\": " << m_instanceId << ",\n";
    ss << "  \"definition_id\": \"" << m_definitionId << "\",\n";
    ss << "  \"player_name\": \"" << m_playerName << "\",\n";
    ss << "  \"owner_id\": " << m_ownerId << ",\n";
    ss << "  \"team\": " << m_team << ",\n";
    ss << "  \"level\": " << m_level << ",\n";
    ss << "  \"experience\": " << m_experience << ",\n";
    ss << "  \"ability_points\": " << m_abilityPoints << ",\n";
    ss << "  \"health\": " << m_currentHealth << ",\n";
    ss << "  \"mana\": " << m_currentMana << ",\n";

    // Abilities
    ss << "  \"abilities\": [\n";
    for (int i = 0; i < ABILITY_SLOT_COUNT; i++) {
        const auto& ability = m_abilities[i];
        ss << "    {\"level\": " << ability.currentLevel;
        ss << ", \"cooldown\": " << ability.cooldownRemaining;
        ss << ", \"toggled\": " << (ability.isToggled ? "true" : "false");
        ss << "}";
        if (i < ABILITY_SLOT_COUNT - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    // Items
    ss << "  \"items\": [\n";
    for (int i = 0; i < ITEM_SLOT_COUNT; i++) {
        const auto& item = m_items[i];
        ss << "    {\"id\": \"" << item.itemId << "\", \"empty\": " << (item.isEmpty ? "true" : "false") << "}";
        if (i < ITEM_SLOT_COUNT - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    // Talents
    ss << "  \"talents\": [" << m_talentChoices[0] << ", " << m_talentChoices[1] << ", "
       << m_talentChoices[2] << ", " << m_talentChoices[3] << "],\n";

    // Combat stats
    ss << "  \"stats\": {\n";
    ss << "    \"kills\": " << m_combatStats.kills << ",\n";
    ss << "    \"deaths\": " << m_combatStats.deaths << ",\n";
    ss << "    \"assists\": " << m_combatStats.assists << "\n";
    ss << "  }\n";

    ss << "}\n";
    return ss.str();
}

bool HeroInstance::FromJson(const std::string& json) {
    // Implementation would parse JSON and restore state
    // Simplified for brevity
    return false;
}

// ============================================================================
// HeroInstanceManager Implementation
// ============================================================================

HeroInstanceManager& HeroInstanceManager::Instance() {
    static HeroInstanceManager instance;
    return instance;
}

std::shared_ptr<HeroInstance> HeroInstanceManager::CreateInstance(const std::string& definitionId) {
    auto instance = std::make_shared<HeroInstance>(definitionId);
    if (instance->GetDefinition()) {
        m_instances[instance->GetInstanceId()] = instance;
        return instance;
    }
    return nullptr;
}

std::shared_ptr<HeroInstance> HeroInstanceManager::GetInstance(uint32_t instanceId) const {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<HeroInstance>> HeroInstanceManager::GetAllInstances() const {
    std::vector<std::shared_ptr<HeroInstance>> result;
    result.reserve(m_instances.size());
    for (const auto& [id, instance] : m_instances) {
        result.push_back(instance);
    }
    return result;
}

std::vector<std::shared_ptr<HeroInstance>> HeroInstanceManager::GetInstancesByTeam(int team) const {
    std::vector<std::shared_ptr<HeroInstance>> result;
    for (const auto& [id, instance] : m_instances) {
        if (instance->GetTeam() == team) {
            result.push_back(instance);
        }
    }
    return result;
}

std::shared_ptr<HeroInstance> HeroInstanceManager::GetInstanceByOwner(uint32_t ownerId) const {
    for (const auto& [id, instance] : m_instances) {
        if (instance->GetOwnerId() == ownerId) {
            return instance;
        }
    }
    return nullptr;
}

void HeroInstanceManager::RemoveInstance(uint32_t instanceId) {
    m_instances.erase(instanceId);
}

void HeroInstanceManager::Update(float deltaTime) {
    for (auto& [id, instance] : m_instances) {
        instance->Update(deltaTime);
    }
}

void HeroInstanceManager::Clear() {
    m_instances.clear();
}

} // namespace Heroes
} // namespace Vehement
