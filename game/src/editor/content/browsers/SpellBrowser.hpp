#pragma once

#include "../ContentDatabase.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>

namespace Vehement {

class Editor;

namespace Content {

class ContentBrowser;

/**
 * @brief Spell targeting type
 */
enum class SpellTargetType {
    None,
    Self,
    SingleTarget,
    PointTarget,
    AreaOfEffect,
    Cone,
    Line,
    Chain,
    Global
};

/**
 * @brief Spell damage type
 */
enum class SpellDamageType {
    None,
    Physical,
    Fire,
    Ice,
    Lightning,
    Holy,
    Shadow,
    Nature,
    Arcane,
    True
};

/**
 * @brief Spell stats for preview
 */
struct SpellStats {
    std::string id;
    std::string name;
    std::string school;         // fire, ice, holy, shadow, etc.
    SpellTargetType targetType = SpellTargetType::None;
    SpellDamageType damageType = SpellDamageType::None;

    // Damage/Healing
    float damage = 0.0f;
    float healing = 0.0f;
    float damageOverTime = 0.0f;
    float healOverTime = 0.0f;
    float duration = 0.0f;

    // Costs
    float manaCost = 0.0f;
    float healthCost = 0.0f;
    float cooldown = 0.0f;
    float castTime = 0.0f;

    // Range and Area
    float range = 0.0f;
    float radius = 0.0f;
    int maxTargets = 1;

    // Effects
    std::vector<std::string> appliedEffects;
    std::vector<std::string> effectChain;  // For chain spells
    std::string summonedUnit;               // For summon spells

    // Classification
    std::vector<std::string> tags;
    std::string description;
    std::string iconPath;
    bool isPassive = false;
    bool isChanneled = false;
    bool isToggle = false;
};

/**
 * @brief Spell filter options
 */
struct SpellFilterOptions {
    std::string searchQuery;
    std::vector<std::string> schools;
    std::vector<SpellTargetType> targetTypes;
    std::vector<SpellDamageType> damageTypes;

    bool showDamageSpells = true;
    bool showHealingSpells = true;
    bool showBuffSpells = true;
    bool showDebuffSpells = true;
    bool showSummonSpells = true;
    bool showPassives = true;

    std::optional<float> minDamage;
    std::optional<float> maxDamage;
    std::optional<float> minCooldown;
    std::optional<float> maxCooldown;
    std::optional<float> minManaCost;
    std::optional<float> maxManaCost;
};

/**
 * @brief Spell Browser
 *
 * Specialized browser for spell assets:
 * - Visual targeting type icons
 * - Damage/healing indicators
 * - Cooldown/cost preview
 * - Effect chain preview
 * - School-based organization
 * - Balance comparison tools
 */
class SpellBrowser {
public:
    explicit SpellBrowser(Editor* editor, ContentBrowser* contentBrowser);
    ~SpellBrowser();

    // Non-copyable
    SpellBrowser(const SpellBrowser&) = delete;
    SpellBrowser& operator=(const SpellBrowser&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    bool Initialize();
    void Shutdown();
    void Render();
    void Update(float deltaTime);

    // =========================================================================
    // Spell Access
    // =========================================================================

    [[nodiscard]] std::vector<SpellStats> GetAllSpells() const;
    [[nodiscard]] std::optional<SpellStats> GetSpell(const std::string& id) const;
    [[nodiscard]] std::vector<SpellStats> GetFilteredSpells() const;
    void RefreshSpells();

    // =========================================================================
    // Filtering
    // =========================================================================

    void SetFilter(const SpellFilterOptions& filter);
    [[nodiscard]] const SpellFilterOptions& GetFilter() const { return m_filter; }
    void FilterBySchool(const std::string& school);
    void FilterByTargetType(SpellTargetType type);
    void FilterByDamageType(SpellDamageType type);
    void ClearFilters();

    // =========================================================================
    // Effect Chain Preview
    // =========================================================================

    /**
     * @brief Get effect chain visualization data
     */
    struct EffectChainNode {
        std::string effectId;
        std::string name;
        float delay = 0.0f;
        std::vector<EffectChainNode> children;
    };
    [[nodiscard]] EffectChainNode GetEffectChain(const std::string& spellId) const;

    /**
     * @brief Preview spell effects
     */
    void PreviewEffects(const std::string& spellId);

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] std::vector<std::string> GetSchools() const;
    [[nodiscard]] std::unordered_map<std::string, int> GetSpellCountBySchool() const;
    [[nodiscard]] float GetAverageDamage(const std::string& school = "") const;
    [[nodiscard]] float GetAverageManaCost(const std::string& school = "") const;

    // =========================================================================
    // Balance Analysis
    // =========================================================================

    /**
     * @brief Calculate damage per mana efficiency
     */
    [[nodiscard]] float CalculateEfficiency(const std::string& spellId) const;

    /**
     * @brief Calculate damage per second (considering cooldown)
     */
    [[nodiscard]] float CalculateDPS(const std::string& spellId) const;

    /**
     * @brief Get balance warnings
     */
    [[nodiscard]] std::vector<std::string> GetBalanceWarnings() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnSpellSelected;
    std::function<void(const std::string&)> OnSpellDoubleClicked;

private:
    // Rendering
    void RenderToolbar();
    void RenderFilters();
    void RenderSpellGrid();
    void RenderSpellCard(const SpellStats& spell);
    void RenderTargetTypeIcon(SpellTargetType type);
    void RenderDamageTypeIcon(SpellDamageType type);
    void RenderEffectChainPreview(const std::string& spellId);
    void RenderSpellTooltip(const SpellStats& spell);

    // Data loading
    SpellStats LoadSpellStats(const std::string& assetId) const;
    void CacheSpells();
    bool MatchesFilter(const SpellStats& spell) const;

    // Icon helpers
    std::string GetTargetTypeIcon(SpellTargetType type) const;
    std::string GetDamageTypeIcon(SpellDamageType type) const;
    glm::vec4 GetSchoolColor(const std::string& school) const;

    Editor* m_editor = nullptr;
    ContentBrowser* m_contentBrowser = nullptr;
    bool m_initialized = false;

    // Cached spells
    std::vector<SpellStats> m_allSpells;
    std::vector<SpellStats> m_filteredSpells;
    bool m_needsRefresh = true;

    // Filter state
    SpellFilterOptions m_filter;

    // Selection
    std::string m_selectedSpellId;

    // View options
    int m_gridColumns = 3;
    bool m_showDamageIndicators = true;
    bool m_showCostIndicators = true;
    bool m_showEffectChain = false;
};

} // namespace Content
} // namespace Vehement
