#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <functional>
#include <unordered_map>
#include <optional>

namespace Vehement {
namespace Heroes {

// Forward declarations
class HeroInstance;

// ============================================================================
// Talent Types
// ============================================================================

/**
 * @brief Types of talent bonuses
 */
enum class TalentBonusType : uint8_t {
    // Stat bonuses
    BonusStrength,
    BonusAgility,
    BonusIntelligence,
    BonusHealth,
    BonusMana,
    BonusDamage,
    BonusArmor,
    BonusMoveSpeed,
    BonusAttackSpeed,
    BonusHealthRegen,
    BonusManaRegen,
    BonusCastRange,
    BonusAttackRange,

    // Ability modifiers
    AbilityDamage,          // Increase ability damage
    AbilityCooldown,        // Reduce ability cooldown
    AbilityManaCost,        // Reduce ability mana cost
    AbilityRange,           // Increase ability range
    AbilityDuration,        // Increase ability duration
    AbilityRadius,          // Increase ability radius
    AbilityCharges,         // Add ability charges

    // Special modifiers
    GoldIncome,             // Bonus gold per second
    XPGain,                 // Bonus XP from kills
    CooldownReduction,      // Global CDR
    SpellAmplification,     // Spell damage amp
    StatusResistance,       // Status duration reduction
    Lifesteal,              // Attack lifesteal
    SpellLifesteal,         // Spell lifesteal
    Evasion,                // Chance to evade
    CriticalChance,         // Chance to crit
    CriticalDamage,         // Crit damage multiplier

    // Ability specific
    AbilitySpecial,         // Custom ability-specific bonus
    AbilityUpgrade,         // Ability upgrade/modification

    // Other
    Custom                  // Custom script-defined bonus
};

// ============================================================================
// Talent Bonus
// ============================================================================

/**
 * @brief Single bonus from a talent
 */
struct TalentBonus {
    TalentBonusType type = TalentBonusType::BonusDamage;
    float value = 0.0f;                 // Bonus value (flat or percentage)
    bool isPercentage = false;          // True if value is a percentage
    std::string targetAbilityId;        // For ability-specific bonuses
    std::string customKey;              // For custom bonuses
};

// ============================================================================
// Talent Definition
// ============================================================================

/**
 * @brief Definition of a single talent choice
 */
class TalentDefinition {
public:
    TalentDefinition();
    ~TalentDefinition();

    // =========================================================================
    // Loading
    // =========================================================================

    bool LoadFromJson(const std::string& json);
    std::string ToJson() const;

    // =========================================================================
    // Identity
    // =========================================================================

    [[nodiscard]] const std::string& GetId() const noexcept { return m_id; }
    void SetId(const std::string& id) { m_id = id; }

    [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    [[nodiscard]] const std::string& GetDescription() const noexcept { return m_description; }
    void SetDescription(const std::string& desc) { m_description = desc; }

    [[nodiscard]] const std::string& GetIconPath() const noexcept { return m_iconPath; }
    void SetIconPath(const std::string& path) { m_iconPath = path; }

    // =========================================================================
    // Tier and Position
    // =========================================================================

    [[nodiscard]] int GetTier() const noexcept { return m_tier; }
    void SetTier(int tier) { m_tier = tier; }

    [[nodiscard]] int GetChoice() const noexcept { return m_choice; }
    void SetChoice(int choice) { m_choice = choice; }

    [[nodiscard]] int GetRequiredLevel() const noexcept { return m_requiredLevel; }
    void SetRequiredLevel(int level) { m_requiredLevel = level; }

    // =========================================================================
    // Bonuses
    // =========================================================================

    [[nodiscard]] const std::vector<TalentBonus>& GetBonuses() const noexcept { return m_bonuses; }
    void SetBonuses(const std::vector<TalentBonus>& bonuses) { m_bonuses = bonuses; }
    void AddBonus(const TalentBonus& bonus) { m_bonuses.push_back(bonus); }

    /**
     * @brief Get description with resolved values
     */
    [[nodiscard]] std::string GetFormattedDescription() const;

    // =========================================================================
    // Ability Modification
    // =========================================================================

    [[nodiscard]] const std::string& GetModifiedAbilityId() const noexcept { return m_modifiedAbilityId; }
    void SetModifiedAbilityId(const std::string& id) { m_modifiedAbilityId = id; }

    [[nodiscard]] bool ModifiesAbility() const { return !m_modifiedAbilityId.empty(); }

    // =========================================================================
    // Script
    // =========================================================================

    [[nodiscard]] const std::string& GetOnSelectScript() const noexcept { return m_onSelectScript; }
    void SetOnSelectScript(const std::string& script) { m_onSelectScript = script; }

private:
    std::string m_id;
    std::string m_name;
    std::string m_description;
    std::string m_iconPath;

    int m_tier = 0;             // 0-3
    int m_choice = 0;           // 0-1
    int m_requiredLevel = 10;

    std::vector<TalentBonus> m_bonuses;

    std::string m_modifiedAbilityId;    // Ability this talent modifies
    std::string m_onSelectScript;       // Script to run when selected
};

// ============================================================================
// Talent Tree
// ============================================================================

/**
 * @brief Complete talent tree for a hero
 *
 * Talent system:
 * - 4 tiers, unlocked at levels 10, 15, 20, 25
 * - 2 choices per tier (mutually exclusive)
 * - Permanent bonuses once selected
 * - Can modify abilities or provide stat bonuses
 */
class TalentTree {
public:
    static constexpr int TIER_COUNT = 4;
    static constexpr int CHOICES_PER_TIER = 2;
    static constexpr std::array<int, TIER_COUNT> UNLOCK_LEVELS = {10, 15, 20, 25};

    using SelectCallback = std::function<void(int tier, int choice, const TalentDefinition&)>;

    TalentTree();
    explicit TalentTree(const std::string& heroId);
    ~TalentTree();

    // =========================================================================
    // Loading
    // =========================================================================

    bool LoadFromJson(const std::string& json);
    bool LoadForHero(const std::string& heroId);
    std::string ToJson() const;

    // =========================================================================
    // Identity
    // =========================================================================

    [[nodiscard]] const std::string& GetHeroId() const noexcept { return m_heroId; }
    void SetHeroId(const std::string& id) { m_heroId = id; }

    // =========================================================================
    // Talent Access
    // =========================================================================

    /**
     * @brief Get talent definition for tier and choice
     * @param tier Tier index (0-3)
     * @param choice Choice index (0-1)
     * @return Talent definition or nullptr
     */
    [[nodiscard]] const TalentDefinition* GetTalent(int tier, int choice) const;
    [[nodiscard]] TalentDefinition* GetTalent(int tier, int choice);

    /**
     * @brief Set talent definition for tier and choice
     */
    void SetTalent(int tier, int choice, std::shared_ptr<TalentDefinition> talent);

    /**
     * @brief Get all talents for a tier
     */
    [[nodiscard]] std::array<std::shared_ptr<TalentDefinition>, CHOICES_PER_TIER> GetTierTalents(int tier) const;

    // =========================================================================
    // Selection State
    // =========================================================================

    /**
     * @brief Get selected choice for tier (-1 if none)
     */
    [[nodiscard]] int GetSelection(int tier) const;

    /**
     * @brief Check if tier has a selection
     */
    [[nodiscard]] bool HasSelection(int tier) const;

    /**
     * @brief Get all selections
     */
    [[nodiscard]] const std::array<int, TIER_COUNT>& GetSelections() const noexcept { return m_selections; }

    /**
     * @brief Get selected talent definition for tier
     */
    [[nodiscard]] const TalentDefinition* GetSelectedTalent(int tier) const;

    // =========================================================================
    // Selection Operations
    // =========================================================================

    /**
     * @brief Check if tier is unlocked at level
     */
    [[nodiscard]] bool IsTierUnlocked(int tier, int heroLevel) const;

    /**
     * @brief Check if selection is valid
     */
    [[nodiscard]] bool CanSelect(int tier, int choice, int heroLevel) const;

    /**
     * @brief Select talent for tier
     * @param tier Tier index (0-3)
     * @param choice Choice index (0-1)
     * @return true if selection was successful
     */
    bool Select(int tier, int choice);

    /**
     * @brief Reset all selections (for testing/respec)
     */
    void ResetSelections();

    // =========================================================================
    // Bonus Calculation
    // =========================================================================

    /**
     * @brief Get total bonus of a type from all selected talents
     */
    [[nodiscard]] float GetTotalBonus(TalentBonusType type) const;

    /**
     * @brief Get total bonus for specific ability
     */
    [[nodiscard]] float GetAbilityBonus(const std::string& abilityId, TalentBonusType type) const;

    /**
     * @brief Get all bonuses from selected talents
     */
    [[nodiscard]] std::vector<TalentBonus> GetAllSelectedBonuses() const;

    /**
     * @brief Check if any selected talent modifies an ability
     */
    [[nodiscard]] bool ModifiesAbility(const std::string& abilityId) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnSelect(SelectCallback callback) { m_onSelect = std::move(callback); }

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Get unlock level for tier
     */
    [[nodiscard]] static int GetUnlockLevel(int tier);

    /**
     * @brief Get tier for unlock level
     */
    [[nodiscard]] static int GetTierForLevel(int level);

private:
    std::string m_heroId;

    // Talents: [tier][choice]
    std::array<std::array<std::shared_ptr<TalentDefinition>, CHOICES_PER_TIER>, TIER_COUNT> m_talents;

    // Current selections (-1 = not selected, 0-1 = choice index)
    std::array<int, TIER_COUNT> m_selections;

    SelectCallback m_onSelect;
};

// ============================================================================
// Talent Registry
// ============================================================================

/**
 * @brief Registry for all talent definitions
 */
class TalentRegistry {
public:
    static TalentRegistry& Instance();

    /**
     * @brief Load talents from directory
     */
    int LoadFromDirectory(const std::string& configPath);

    /**
     * @brief Register a talent definition
     */
    void Register(std::shared_ptr<TalentDefinition> talent);

    /**
     * @brief Get talent by ID
     */
    [[nodiscard]] std::shared_ptr<TalentDefinition> Get(const std::string& id) const;

    /**
     * @brief Get talents for a hero
     */
    [[nodiscard]] std::vector<std::shared_ptr<TalentDefinition>> GetForHero(const std::string& heroId) const;

    /**
     * @brief Check if talent exists
     */
    [[nodiscard]] bool Exists(const std::string& id) const;

    /**
     * @brief Get talent count
     */
    [[nodiscard]] size_t Count() const { return m_talents.size(); }

    /**
     * @brief Clear all talents
     */
    void Clear();

private:
    TalentRegistry() = default;

    std::unordered_map<std::string, std::shared_ptr<TalentDefinition>> m_talents;
    std::unordered_map<std::string, std::vector<std::string>> m_heroTalents; // heroId -> talent IDs
};

// ============================================================================
// Helper Functions
// ============================================================================

[[nodiscard]] std::string TalentBonusTypeToString(TalentBonusType type);
[[nodiscard]] TalentBonusType StringToTalentBonusType(const std::string& str);

} // namespace Heroes
} // namespace Vehement
