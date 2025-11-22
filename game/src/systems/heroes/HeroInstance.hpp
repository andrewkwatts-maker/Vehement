#pragma once

#include "HeroDefinition.hpp"
#include "../abilities/AbilityInstance.hpp"

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <functional>
#include <optional>
#include <glm/glm.hpp>

namespace Vehement {
namespace Heroes {

// Forward declarations
class HeroProgression;
class TalentTree;

// ============================================================================
// Combat Statistics
// ============================================================================

/**
 * @brief Kill/Death/Assist statistics for heroes
 */
struct HeroCombatStats {
    int kills = 0;
    int deaths = 0;
    int assists = 0;

    // Detailed stats
    int heroKills = 0;          // Kills on other heroes
    int creepKills = 0;         // Kills on creeps/minions
    int buildingKills = 0;      // Buildings destroyed
    int bossKills = 0;          // Boss monsters killed

    float damageDealt = 0.0f;
    float damageTaken = 0.0f;
    float healingDone = 0.0f;
    float healingReceived = 0.0f;

    int abilitiesCast = 0;
    int itemsUsed = 0;

    float goldEarned = 0.0f;
    float goldSpent = 0.0f;

    // Game time stats
    float timePlayed = 0.0f;
    float timeAlive = 0.0f;
    float timeDead = 0.0f;

    [[nodiscard]] float GetKDA() const {
        return deaths > 0 ? static_cast<float>(kills + assists) / deaths : static_cast<float>(kills + assists);
    }

    [[nodiscard]] float GetKillParticipation(int teamKills) const {
        return teamKills > 0 ? static_cast<float>(kills + assists) / teamKills : 0.0f;
    }

    void Reset() {
        *this = HeroCombatStats{};
    }
};

// ============================================================================
// Equipped Items
// ============================================================================

/**
 * @brief Item slot for hero inventory
 */
struct ItemSlot {
    std::string itemId;
    int charges = 0;            // For consumables
    float cooldown = 0.0f;      // For active items
    bool isEmpty = true;

    void Clear() {
        itemId.clear();
        charges = 0;
        cooldown = 0.0f;
        isEmpty = true;
    }
};

// ============================================================================
// Respawn State
// ============================================================================

/**
 * @brief Respawn handling state
 */
struct RespawnState {
    bool isDead = false;
    float timeOfDeath = 0.0f;       // Game time when died
    float respawnTimer = 0.0f;      // Time remaining until respawn
    float baseRespawnTime = 10.0f;  // Base respawn time
    glm::vec3 respawnPosition{0.0f};
    glm::vec3 deathPosition{0.0f};

    // Buyback
    bool canBuyback = true;
    float buybackCost = 0.0f;
    float buybackCooldown = 0.0f;

    [[nodiscard]] float GetRespawnProgress() const {
        if (!isDead || respawnTimer <= 0.0f) return 1.0f;
        return 1.0f - (respawnTimer / baseRespawnTime);
    }

    void StartRespawn(float duration, const glm::vec3& pos) {
        respawnTimer = duration;
        baseRespawnTime = duration;
        respawnPosition = pos;
    }
};

// ============================================================================
// Learned Ability State
// ============================================================================

/**
 * @brief State of a learned ability on a hero instance
 */
struct LearnedAbility {
    std::string abilityId;
    int currentLevel = 0;           // 0 = not learned
    int maxLevel = 4;
    float cooldownRemaining = 0.0f;
    int charges = 0;                // For charge-based abilities
    int maxCharges = 0;
    float chargeRestoreTime = 0.0f;
    bool isToggled = false;         // For toggle abilities
    bool isAutocast = false;        // For autocast abilities

    [[nodiscard]] bool IsLearned() const { return currentLevel > 0; }
    [[nodiscard]] bool IsReady() const { return IsLearned() && cooldownRemaining <= 0.0f && (maxCharges == 0 || charges > 0); }
    [[nodiscard]] bool IsMaxLevel() const { return currentLevel >= maxLevel; }
};

// ============================================================================
// Hero Instance
// ============================================================================

/**
 * @brief Runtime hero state instance
 *
 * Represents a single hero in the game with:
 * - Current level and experience
 * - Learned abilities with levels and cooldowns
 * - Equipped items (6 slots)
 * - Talent choices
 * - Combat statistics (K/D/A)
 * - Respawn handling
 *
 * Created from a HeroDefinition but maintains mutable game state.
 */
class HeroInstance {
public:
    static constexpr int MAX_LEVEL = 30;
    static constexpr int ABILITY_SLOT_COUNT = 4;
    static constexpr int ITEM_SLOT_COUNT = 6;
    static constexpr int TALENT_TIER_COUNT = 4;

    // Callbacks
    using LevelUpCallback = std::function<void(HeroInstance&, int newLevel)>;
    using DeathCallback = std::function<void(HeroInstance&, uint32_t killerId)>;
    using RespawnCallback = std::function<void(HeroInstance&)>;
    using AbilityLearnCallback = std::function<void(HeroInstance&, int slot, int newLevel)>;
    using TalentSelectCallback = std::function<void(HeroInstance&, int tier, int choice)>;

    HeroInstance();
    explicit HeroInstance(const std::string& definitionId);
    explicit HeroInstance(std::shared_ptr<HeroDefinition> definition);
    ~HeroInstance();

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize from hero definition
     */
    bool Initialize(const std::string& definitionId);
    bool Initialize(std::shared_ptr<HeroDefinition> definition);

    /**
     * @brief Reset hero to starting state
     */
    void Reset();

    /**
     * @brief Get hero definition
     */
    [[nodiscard]] std::shared_ptr<HeroDefinition> GetDefinition() const { return m_definition; }

    // =========================================================================
    // Identity
    // =========================================================================

    [[nodiscard]] uint32_t GetInstanceId() const noexcept { return m_instanceId; }
    [[nodiscard]] const std::string& GetDefinitionId() const noexcept { return m_definitionId; }

    [[nodiscard]] const std::string& GetPlayerName() const noexcept { return m_playerName; }
    void SetPlayerName(const std::string& name) { m_playerName = name; }

    [[nodiscard]] uint32_t GetOwnerId() const noexcept { return m_ownerId; }
    void SetOwnerId(uint32_t id) { m_ownerId = id; }

    [[nodiscard]] int GetTeam() const noexcept { return m_team; }
    void SetTeam(int team) { m_team = team; }

    // =========================================================================
    // Level and Experience
    // =========================================================================

    [[nodiscard]] int GetLevel() const noexcept { return m_level; }
    [[nodiscard]] int GetExperience() const noexcept { return m_experience; }
    [[nodiscard]] int GetExperienceToNextLevel() const;
    [[nodiscard]] float GetLevelProgress() const;

    /**
     * @brief Add experience to hero
     * @param amount XP to add
     * @return Number of levels gained
     */
    int AddExperience(int amount);

    /**
     * @brief Set hero level directly
     */
    void SetLevel(int level);

    /**
     * @brief Get unspent ability points
     */
    [[nodiscard]] int GetAbilityPoints() const noexcept { return m_abilityPoints; }

    // =========================================================================
    // Current Stats (computed from level + items + buffs)
    // =========================================================================

    [[nodiscard]] float GetCurrentHealth() const noexcept { return m_currentHealth; }
    [[nodiscard]] float GetMaxHealth() const;
    [[nodiscard]] float GetCurrentMana() const noexcept { return m_currentMana; }
    [[nodiscard]] float GetMaxMana() const;

    [[nodiscard]] float GetHealthPercent() const;
    [[nodiscard]] float GetManaPercent() const;

    void SetHealth(float health);
    void SetMana(float mana);

    void AddHealth(float amount);
    void AddMana(float amount);

    /**
     * @brief Take damage
     * @param amount Raw damage amount
     * @param sourceId Entity that dealt damage
     * @return Actual damage taken after reductions
     */
    float TakeDamage(float amount, uint32_t sourceId = 0);

    /**
     * @brief Consume mana for ability
     * @return true if had enough mana
     */
    bool ConsumeMana(float amount);

    // Calculated stats
    [[nodiscard]] float GetDamage() const;
    [[nodiscard]] float GetArmor() const;
    [[nodiscard]] float GetMagicResist() const;
    [[nodiscard]] float GetMoveSpeed() const;
    [[nodiscard]] float GetAttackSpeed() const;
    [[nodiscard]] float GetAttackRange() const;
    [[nodiscard]] float GetHealthRegen() const;
    [[nodiscard]] float GetManaRegen() const;

    // Attributes
    [[nodiscard]] float GetStrength() const;
    [[nodiscard]] float GetAgility() const;
    [[nodiscard]] float GetIntelligence() const;

    // =========================================================================
    // Abilities
    // =========================================================================

    /**
     * @brief Get ability state for slot
     */
    [[nodiscard]] const LearnedAbility& GetAbility(int slot) const;
    [[nodiscard]] LearnedAbility& GetAbility(int slot);

    /**
     * @brief Learn/level up ability in slot
     * @return true if ability was leveled
     */
    bool LearnAbility(int slot);

    /**
     * @brief Check if ability can be learned
     */
    [[nodiscard]] bool CanLearnAbility(int slot) const;

    /**
     * @brief Check if ability is ready to cast
     */
    [[nodiscard]] bool IsAbilityReady(int slot) const;

    /**
     * @brief Start ability cooldown
     */
    void StartAbilityCooldown(int slot, float duration);

    /**
     * @brief Reset ability cooldown
     */
    void ResetAbilityCooldown(int slot);

    /**
     * @brief Toggle ability on/off
     */
    void ToggleAbility(int slot);

    /**
     * @brief Set autocast state
     */
    void SetAutocast(int slot, bool enabled);

    /**
     * @brief Use ability charge
     */
    bool UseAbilityCharge(int slot);

    // =========================================================================
    // Items (6 slots)
    // =========================================================================

    [[nodiscard]] const ItemSlot& GetItemSlot(int slot) const;
    [[nodiscard]] ItemSlot& GetItemSlot(int slot);

    /**
     * @brief Equip item in slot
     * @return true if equipped successfully
     */
    bool EquipItem(int slot, const std::string& itemId);

    /**
     * @brief Remove item from slot
     * @return Item ID that was removed
     */
    std::string UnequipItem(int slot);

    /**
     * @brief Swap items between slots
     */
    void SwapItems(int slot1, int slot2);

    /**
     * @brief Find first empty slot
     * @return Slot index or -1 if full
     */
    [[nodiscard]] int FindEmptyItemSlot() const;

    /**
     * @brief Check if hero has item
     */
    [[nodiscard]] bool HasItem(const std::string& itemId) const;

    /**
     * @brief Use item in slot
     */
    bool UseItem(int slot);

    // =========================================================================
    // Talents
    // =========================================================================

    /**
     * @brief Get selected talent for tier (0-3)
     * @return -1 if not selected, 0 or 1 for choice
     */
    [[nodiscard]] int GetTalentChoice(int tier) const;

    /**
     * @brief Select talent for tier
     * @param tier Talent tier (0-3)
     * @param choice Choice index (0 or 1)
     * @return true if selection was successful
     */
    bool SelectTalent(int tier, int choice);

    /**
     * @brief Check if talent tier is unlocked (based on level)
     */
    [[nodiscard]] bool IsTalentTierUnlocked(int tier) const;

    /**
     * @brief Check if talent has been selected for tier
     */
    [[nodiscard]] bool HasSelectedTalent(int tier) const;

    /**
     * @brief Get ID of selected talent for tier
     */
    [[nodiscard]] std::string GetSelectedTalentId(int tier) const;

    // =========================================================================
    // Combat Stats
    // =========================================================================

    [[nodiscard]] const HeroCombatStats& GetCombatStats() const noexcept { return m_combatStats; }
    [[nodiscard]] HeroCombatStats& GetCombatStats() noexcept { return m_combatStats; }

    void RecordKill(bool isHero = false);
    void RecordDeath();
    void RecordAssist();
    void RecordDamageDealt(float amount);
    void RecordDamageTaken(float amount);
    void RecordHealingDone(float amount);
    void RecordGoldEarned(float amount);

    // =========================================================================
    // Death and Respawn
    // =========================================================================

    [[nodiscard]] const RespawnState& GetRespawnState() const noexcept { return m_respawnState; }

    [[nodiscard]] bool IsDead() const noexcept { return m_respawnState.isDead; }
    [[nodiscard]] float GetRespawnTimer() const noexcept { return m_respawnState.respawnTimer; }

    /**
     * @brief Kill the hero
     * @param killerId Entity that killed the hero
     */
    void Die(uint32_t killerId = 0);

    /**
     * @brief Respawn the hero
     */
    void Respawn();

    /**
     * @brief Respawn at specific position
     */
    void Respawn(const glm::vec3& position);

    /**
     * @brief Buyback to respawn immediately
     * @return true if buyback was successful
     */
    bool Buyback();

    /**
     * @brief Set respawn position
     */
    void SetRespawnPosition(const glm::vec3& position);

    /**
     * @brief Calculate respawn time based on level
     */
    [[nodiscard]] float CalculateRespawnTime() const;

    // =========================================================================
    // Position and Movement
    // =========================================================================

    [[nodiscard]] const glm::vec3& GetPosition() const noexcept { return m_position; }
    void SetPosition(const glm::vec3& pos) { m_position = pos; }

    [[nodiscard]] const glm::vec3& GetRotation() const noexcept { return m_rotation; }
    void SetRotation(const glm::vec3& rot) { m_rotation = rot; }

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update hero state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnLevelUp(LevelUpCallback callback) { m_onLevelUp = std::move(callback); }
    void SetOnDeath(DeathCallback callback) { m_onDeath = std::move(callback); }
    void SetOnRespawn(RespawnCallback callback) { m_onRespawn = std::move(callback); }
    void SetOnAbilityLearn(AbilityLearnCallback callback) { m_onAbilityLearn = std::move(callback); }
    void SetOnTalentSelect(TalentSelectCallback callback) { m_onTalentSelect = std::move(callback); }

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize hero state to JSON
     */
    [[nodiscard]] std::string ToJson() const;

    /**
     * @brief Load hero state from JSON
     */
    bool FromJson(const std::string& json);

private:
    void UpdateCooldowns(float deltaTime);
    void UpdateRegen(float deltaTime);
    void UpdateRespawn(float deltaTime);
    void OnLevelUp(int oldLevel, int newLevel);
    int CalculateXPForLevel(int level) const;

    // Definition reference
    std::shared_ptr<HeroDefinition> m_definition;
    std::string m_definitionId;

    // Instance identity
    uint32_t m_instanceId = 0;
    std::string m_playerName;
    uint32_t m_ownerId = 0;
    int m_team = 0;

    // Level and XP
    int m_level = 1;
    int m_experience = 0;
    int m_abilityPoints = 0;

    // Current resources
    float m_currentHealth = 0.0f;
    float m_currentMana = 0.0f;

    // Bonus stats (from items, buffs, talents)
    float m_bonusStrength = 0.0f;
    float m_bonusAgility = 0.0f;
    float m_bonusIntelligence = 0.0f;
    float m_bonusDamage = 0.0f;
    float m_bonusArmor = 0.0f;
    float m_bonusMoveSpeed = 0.0f;
    float m_bonusAttackSpeed = 0.0f;
    float m_bonusHealthRegen = 0.0f;
    float m_bonusManaRegen = 0.0f;

    // Abilities (4 slots)
    std::array<LearnedAbility, ABILITY_SLOT_COUNT> m_abilities;

    // Items (6 slots)
    std::array<ItemSlot, ITEM_SLOT_COUNT> m_items;

    // Talents (4 tiers, -1 = not selected, 0-1 = choice)
    std::array<int, TALENT_TIER_COUNT> m_talentChoices;

    // Combat stats
    HeroCombatStats m_combatStats;

    // Respawn state
    RespawnState m_respawnState;

    // Position
    glm::vec3 m_position{0.0f};
    glm::vec3 m_rotation{0.0f};

    // Game time
    float m_gameTime = 0.0f;

    // Callbacks
    LevelUpCallback m_onLevelUp;
    DeathCallback m_onDeath;
    RespawnCallback m_onRespawn;
    AbilityLearnCallback m_onAbilityLearn;
    TalentSelectCallback m_onTalentSelect;

    // Static instance counter
    static uint32_t s_nextInstanceId;
};

// ============================================================================
// Hero Instance Manager
// ============================================================================

/**
 * @brief Manages all active hero instances
 */
class HeroInstanceManager {
public:
    static HeroInstanceManager& Instance();

    /**
     * @brief Create a new hero instance
     */
    std::shared_ptr<HeroInstance> CreateInstance(const std::string& definitionId);

    /**
     * @brief Get instance by ID
     */
    [[nodiscard]] std::shared_ptr<HeroInstance> GetInstance(uint32_t instanceId) const;

    /**
     * @brief Get all instances
     */
    [[nodiscard]] std::vector<std::shared_ptr<HeroInstance>> GetAllInstances() const;

    /**
     * @brief Get instances by team
     */
    [[nodiscard]] std::vector<std::shared_ptr<HeroInstance>> GetInstancesByTeam(int team) const;

    /**
     * @brief Get instance by owner
     */
    [[nodiscard]] std::shared_ptr<HeroInstance> GetInstanceByOwner(uint32_t ownerId) const;

    /**
     * @brief Remove instance
     */
    void RemoveInstance(uint32_t instanceId);

    /**
     * @brief Update all instances
     */
    void Update(float deltaTime);

    /**
     * @brief Clear all instances
     */
    void Clear();

    /**
     * @brief Get instance count
     */
    [[nodiscard]] size_t Count() const { return m_instances.size(); }

private:
    HeroInstanceManager() = default;

    std::unordered_map<uint32_t, std::shared_ptr<HeroInstance>> m_instances;
};

} // namespace Heroes
} // namespace Vehement
