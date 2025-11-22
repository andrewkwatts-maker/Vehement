#pragma once

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>
#include <optional>
#include <functional>
#include <glm/glm.hpp>

namespace Vehement {
namespace Heroes {

// ============================================================================
// Forward Declarations
// ============================================================================

class HeroInstance;
class TalentTree;

// ============================================================================
// Hero Attribute Types
// ============================================================================

/**
 * @brief Primary attribute types for heroes
 */
enum class PrimaryAttribute : uint8_t {
    Strength,       // Increases health, health regen, physical damage
    Agility,        // Increases armor, attack speed, movement speed
    Intelligence    // Increases mana, mana regen, spell damage
};

/**
 * @brief Hero class types
 */
enum class HeroClassType : uint8_t {
    Warrior,        // Frontline fighter, high durability
    Mage,           // Spell caster, high magic damage
    Support,        // Team utility, healing and buffs
    Assassin,       // High burst damage, squishy
    Tank,           // High defense, crowd control
    Marksman        // Ranged physical damage dealer
};

// ============================================================================
// Hero Stats Configuration
// ============================================================================

/**
 * @brief Base stats for a hero
 */
struct HeroBaseStats {
    // Core resources
    float health = 200.0f;
    float mana = 75.0f;

    // Combat stats
    float damage = 25.0f;
    float armor = 3.0f;
    float magicResist = 0.0f;

    // Movement
    float moveSpeed = 300.0f;
    float turnRate = 0.6f;

    // Attack properties
    float attackRange = 1.5f;
    float attackSpeed = 1.0f;
    float attackPoint = 0.4f;       // Animation point where damage is dealt
    float attackBackswing = 0.5f;   // Animation after attack point

    // Regeneration
    float healthRegen = 1.0f;
    float manaRegen = 0.5f;

    // Primary attributes
    float strength = 20.0f;
    float agility = 15.0f;
    float intelligence = 15.0f;

    // Vision
    float dayVision = 18.0f;
    float nightVision = 8.0f;
};

/**
 * @brief Stat growth per level
 */
struct HeroStatGrowth {
    float healthPerLevel = 25.0f;
    float manaPerLevel = 2.0f;
    float damagePerLevel = 0.0f;
    float armorPerLevel = 0.0f;

    float strengthPerLevel = 2.5f;
    float agilityPerLevel = 1.5f;
    float intelligencePerLevel = 1.5f;

    float healthRegenPerLevel = 0.1f;
    float manaRegenPerLevel = 0.05f;
};

// ============================================================================
// Ability Slot Configuration
// ============================================================================

/**
 * @brief Ability slot binding
 */
struct AbilitySlotBinding {
    int slot = 0;                   // 1-4 for standard, 4 usually ultimate
    std::string abilityId;          // Reference to ability config
    bool isUltimate = false;        // Ultimate abilities have special rules
    int unlockLevel = 1;            // Level required to learn
    int maxLevel = 4;               // Maximum ability level
};

// ============================================================================
// Visual Customization
// ============================================================================

/**
 * @brief Visual customization options for heroes
 */
struct HeroVisualOptions {
    // Model paths
    std::string modelPath;
    std::string skeletonPath;

    // Texture variants
    std::vector<std::string> skinVariants;
    std::string defaultSkin;

    // Effects
    std::string attackEffect;
    std::string castEffect;
    std::string deathEffect;
    std::string respawnEffect;

    // Audio
    std::string attackSound;
    std::string hurtSound;
    std::string deathSound;
    std::vector<std::string> voiceLines;

    // UI
    std::string portraitPath;
    std::string iconPath;
    std::string minimapIcon;

    // Animation set
    std::string animationSet;

    // Custom colors
    glm::vec4 primaryColor{1.0f};
    glm::vec4 secondaryColor{1.0f};

    // Scale
    float modelScale = 1.0f;
};

// ============================================================================
// Hero Events
// ============================================================================

/**
 * @brief Script event bindings for hero lifecycle
 */
struct HeroEventBindings {
    std::string onSpawn;            // scripts/heroes/{id}/on_spawn.py
    std::string onDeath;            // scripts/heroes/{id}/on_death.py
    std::string onRespawn;          // scripts/heroes/{id}/on_respawn.py
    std::string onLevelUp;          // scripts/heroes/{id}/on_level_up.py
    std::string onAbilityLearn;     // scripts/heroes/{id}/on_ability_learn.py
    std::string onKill;             // scripts/heroes/{id}/on_kill.py
    std::string onAssist;           // scripts/heroes/{id}/on_assist.py
    std::string onTalentSelect;     // scripts/heroes/{id}/on_talent_select.py
    std::string onItemEquip;        // scripts/heroes/{id}/on_item_equip.py
    std::string onCreate;           // Called when definition is created
    std::string onDestroy;          // Called when definition is destroyed
};

// ============================================================================
// Talent Configuration
// ============================================================================

/**
 * @brief Talent tier configuration
 */
struct TalentTierConfig {
    int tier = 1;                   // Tier number (1-4)
    int unlockLevel = 10;           // Level required to unlock
    std::array<std::string, 2> choices;  // Two talent choices per tier
};

// ============================================================================
// Hero Definition
// ============================================================================

/**
 * @brief Complete hero class definition loaded from JSON
 *
 * Defines all static properties of a hero type including:
 * - Base stats and growth curves
 * - Primary attribute bonuses
 * - Starting and ultimate abilities
 * - Talent tree configuration
 * - Visual customization options
 *
 * Supports Create/Tick/Destroy lifecycle for runtime management.
 */
class HeroDefinition {
public:
    // Lifecycle callbacks
    using CreateCallback = std::function<void(const HeroDefinition&)>;
    using TickCallback = std::function<void(const HeroDefinition&, float)>;
    using DestroyCallback = std::function<void(const HeroDefinition&)>;

    static constexpr int ABILITY_SLOT_COUNT = 4;
    static constexpr int MAX_LEVEL = 30;
    static constexpr int TALENT_TIER_COUNT = 4;
    static constexpr int ITEM_SLOT_COUNT = 6;

    HeroDefinition();
    ~HeroDefinition();

    // =========================================================================
    // Loading and Serialization
    // =========================================================================

    /**
     * @brief Load hero definition from JSON file
     * @param filePath Path to the JSON config file
     * @return true if loaded successfully
     */
    bool LoadFromFile(const std::string& filePath);

    /**
     * @brief Load hero definition from JSON string
     * @param jsonString JSON content as string
     * @return true if parsed successfully
     */
    bool LoadFromString(const std::string& jsonString);

    /**
     * @brief Save hero definition to JSON file
     * @param filePath Output file path
     * @return true if saved successfully
     */
    bool SaveToFile(const std::string& filePath) const;

    /**
     * @brief Serialize hero definition to JSON string
     * @return JSON string representation
     */
    std::string ToJsonString() const;

    /**
     * @brief Validate hero definition
     * @param outErrors List of validation errors
     * @return true if valid
     */
    bool Validate(std::vector<std::string>& outErrors) const;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Called when definition is created/loaded
     */
    void Create();

    /**
     * @brief Called each frame for definition updates
     * @param deltaTime Time since last tick
     */
    void Tick(float deltaTime);

    /**
     * @brief Called when definition is destroyed/unloaded
     */
    void Destroy();

    /**
     * @brief Check if definition is active
     */
    [[nodiscard]] bool IsActive() const noexcept { return m_isActive; }

    // =========================================================================
    // Identity
    // =========================================================================

    [[nodiscard]] const std::string& GetId() const noexcept { return m_id; }
    void SetId(const std::string& id) { m_id = id; }

    [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    [[nodiscard]] const std::string& GetTitle() const noexcept { return m_title; }
    void SetTitle(const std::string& title) { m_title = title; }

    [[nodiscard]] const std::string& GetDescription() const noexcept { return m_description; }
    void SetDescription(const std::string& desc) { m_description = desc; }

    [[nodiscard]] const std::string& GetLore() const noexcept { return m_lore; }
    void SetLore(const std::string& lore) { m_lore = lore; }

    [[nodiscard]] HeroClassType GetClassType() const noexcept { return m_classType; }
    void SetClassType(HeroClassType type) { m_classType = type; }

    [[nodiscard]] const std::string& GetClassTypeName() const noexcept { return m_classTypeName; }
    void SetClassTypeName(const std::string& name) { m_classTypeName = name; }

    // =========================================================================
    // Primary Attribute
    // =========================================================================

    [[nodiscard]] PrimaryAttribute GetPrimaryAttribute() const noexcept { return m_primaryAttribute; }
    void SetPrimaryAttribute(PrimaryAttribute attr) { m_primaryAttribute = attr; }

    /**
     * @brief Get bonus per point of primary attribute
     * - Strength: +20 health, +0.1 health regen
     * - Agility: +0.17 armor, +1% attack speed
     * - Intelligence: +12 mana, +0.05 mana regen
     */
    [[nodiscard]] float GetPrimaryAttributeBonus(PrimaryAttribute attr) const;

    // =========================================================================
    // Base Stats
    // =========================================================================

    [[nodiscard]] const HeroBaseStats& GetBaseStats() const noexcept { return m_baseStats; }
    void SetBaseStats(const HeroBaseStats& stats) { m_baseStats = stats; }

    [[nodiscard]] const HeroStatGrowth& GetStatGrowth() const noexcept { return m_statGrowth; }
    void SetStatGrowth(const HeroStatGrowth& growth) { m_statGrowth = growth; }

    /**
     * @brief Calculate stats at a given level
     * @param level Hero level (1-30)
     * @return Stats adjusted for level
     */
    [[nodiscard]] HeroBaseStats CalculateStatsAtLevel(int level) const;

    // =========================================================================
    // Abilities
    // =========================================================================

    [[nodiscard]] const std::vector<AbilitySlotBinding>& GetAbilities() const noexcept {
        return m_abilities;
    }
    void SetAbilities(const std::vector<AbilitySlotBinding>& abilities) {
        m_abilities = abilities;
    }

    /**
     * @brief Get ability binding for a slot
     * @param slot Slot index (0-3)
     * @return Ability binding or nullptr if empty
     */
    [[nodiscard]] const AbilitySlotBinding* GetAbilitySlot(int slot) const;

    /**
     * @brief Check if hero has ultimate ability
     */
    [[nodiscard]] bool HasUltimate() const;

    /**
     * @brief Get ultimate ability unlock level
     */
    [[nodiscard]] int GetUltimateUnlockLevel() const;

    // =========================================================================
    // Talents
    // =========================================================================

    [[nodiscard]] const std::array<TalentTierConfig, TALENT_TIER_COUNT>& GetTalentTiers() const noexcept {
        return m_talentTiers;
    }
    void SetTalentTiers(const std::array<TalentTierConfig, TALENT_TIER_COUNT>& tiers) {
        m_talentTiers = tiers;
    }

    /**
     * @brief Get talent tier configuration
     * @param tier Tier index (0-3)
     * @return Talent tier config
     */
    [[nodiscard]] const TalentTierConfig& GetTalentTier(int tier) const;

    /**
     * @brief Get level required to unlock a talent tier
     * @param tier Tier index (0-3)
     * @return Required level
     */
    [[nodiscard]] int GetTalentUnlockLevel(int tier) const;

    // =========================================================================
    // Visual Options
    // =========================================================================

    [[nodiscard]] const HeroVisualOptions& GetVisualOptions() const noexcept {
        return m_visualOptions;
    }
    void SetVisualOptions(const HeroVisualOptions& options) {
        m_visualOptions = options;
    }

    // =========================================================================
    // Events
    // =========================================================================

    [[nodiscard]] const HeroEventBindings& GetEventBindings() const noexcept {
        return m_eventBindings;
    }
    void SetEventBindings(const HeroEventBindings& bindings) {
        m_eventBindings = bindings;
    }

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnCreate(CreateCallback callback) { m_onCreate = std::move(callback); }
    void SetOnTick(TickCallback callback) { m_onTick = std::move(callback); }
    void SetOnDestroy(DestroyCallback callback) { m_onDestroy = std::move(callback); }

    // =========================================================================
    // Tags and Metadata
    // =========================================================================

    [[nodiscard]] const std::vector<std::string>& GetTags() const noexcept { return m_tags; }
    void SetTags(const std::vector<std::string>& tags) { m_tags = tags; }
    void AddTag(const std::string& tag) { m_tags.push_back(tag); }
    [[nodiscard]] bool HasTag(const std::string& tag) const;

    [[nodiscard]] const std::unordered_map<std::string, std::string>& GetMetadata() const noexcept {
        return m_metadata;
    }
    void SetMetadata(const std::string& key, const std::string& value) {
        m_metadata[key] = value;
    }
    [[nodiscard]] std::optional<std::string> GetMetadataValue(const std::string& key) const;

    // =========================================================================
    // Source Info
    // =========================================================================

    [[nodiscard]] const std::string& GetSourcePath() const noexcept { return m_sourcePath; }
    [[nodiscard]] int64_t GetLastModified() const noexcept { return m_lastModified; }

private:
    void ParseJson(const std::string& jsonContent);
    void ParseBaseStats(const std::string& jsonContent);
    void ParseStatGrowth(const std::string& jsonContent);
    void ParseAbilities(const std::string& jsonContent);
    void ParseTalents(const std::string& jsonContent);
    void ParseVisuals(const std::string& jsonContent);
    void ParseEvents(const std::string& jsonContent);

    // Identity
    std::string m_id;
    std::string m_name;
    std::string m_title;            // "The Warlord", "Master Engineer", etc.
    std::string m_description;
    std::string m_lore;
    HeroClassType m_classType = HeroClassType::Warrior;
    std::string m_classTypeName;    // "warrior", "mage", etc.

    // Primary attribute
    PrimaryAttribute m_primaryAttribute = PrimaryAttribute::Strength;

    // Stats
    HeroBaseStats m_baseStats;
    HeroStatGrowth m_statGrowth;

    // Abilities (4 slots)
    std::vector<AbilitySlotBinding> m_abilities;

    // Talents (4 tiers, 2 choices each)
    std::array<TalentTierConfig, TALENT_TIER_COUNT> m_talentTiers;

    // Visual customization
    HeroVisualOptions m_visualOptions;

    // Event bindings
    HeroEventBindings m_eventBindings;

    // Tags and metadata
    std::vector<std::string> m_tags;
    std::unordered_map<std::string, std::string> m_metadata;

    // Source info
    std::string m_sourcePath;
    int64_t m_lastModified = 0;

    // Lifecycle state
    bool m_isActive = false;

    // Callbacks
    CreateCallback m_onCreate;
    TickCallback m_onTick;
    DestroyCallback m_onDestroy;
};

// ============================================================================
// Hero Definition Registry
// ============================================================================

/**
 * @brief Registry for all hero definitions
 */
class HeroDefinitionRegistry {
public:
    static HeroDefinitionRegistry& Instance();

    /**
     * @brief Load all hero definitions from directory
     * @param configPath Path to hero configs directory
     * @return Number of definitions loaded
     */
    int LoadFromDirectory(const std::string& configPath);

    /**
     * @brief Register a hero definition
     * @param definition Hero definition to register
     */
    void Register(std::shared_ptr<HeroDefinition> definition);

    /**
     * @brief Get hero definition by ID
     * @param id Hero definition ID
     * @return Hero definition or nullptr
     */
    [[nodiscard]] std::shared_ptr<HeroDefinition> Get(const std::string& id) const;

    /**
     * @brief Get all registered hero definitions
     */
    [[nodiscard]] std::vector<std::shared_ptr<HeroDefinition>> GetAll() const;

    /**
     * @brief Get heroes by class type
     */
    [[nodiscard]] std::vector<std::shared_ptr<HeroDefinition>> GetByClassType(HeroClassType type) const;

    /**
     * @brief Get heroes by primary attribute
     */
    [[nodiscard]] std::vector<std::shared_ptr<HeroDefinition>> GetByPrimaryAttribute(PrimaryAttribute attr) const;

    /**
     * @brief Get heroes by tag
     */
    [[nodiscard]] std::vector<std::shared_ptr<HeroDefinition>> GetByTag(const std::string& tag) const;

    /**
     * @brief Check if hero exists
     */
    [[nodiscard]] bool Exists(const std::string& id) const;

    /**
     * @brief Get number of registered heroes
     */
    [[nodiscard]] size_t Count() const { return m_definitions.size(); }

    /**
     * @brief Unload all definitions
     */
    void Clear();

    /**
     * @brief Reload all definitions from disk
     */
    void Reload();

    /**
     * @brief Tick all active definitions
     */
    void Tick(float deltaTime);

private:
    HeroDefinitionRegistry() = default;

    std::unordered_map<std::string, std::shared_ptr<HeroDefinition>> m_definitions;
    std::string m_configPath;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Convert primary attribute to string
 */
[[nodiscard]] std::string PrimaryAttributeToString(PrimaryAttribute attr);

/**
 * @brief Parse primary attribute from string
 */
[[nodiscard]] PrimaryAttribute StringToPrimaryAttribute(const std::string& str);

/**
 * @brief Convert hero class type to string
 */
[[nodiscard]] std::string HeroClassTypeToString(HeroClassType type);

/**
 * @brief Parse hero class type from string
 */
[[nodiscard]] HeroClassType StringToHeroClassType(const std::string& str);

} // namespace Heroes
} // namespace Vehement
