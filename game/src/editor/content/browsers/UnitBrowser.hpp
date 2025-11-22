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
 * @brief Unit stats for preview
 */
struct UnitStats {
    std::string id;
    std::string name;
    std::string faction;
    int tier = 1;
    std::string role;       // infantry, cavalry, ranged, support, hero

    // Combat stats
    int health = 100;
    int maxHealth = 100;
    int armor = 0;
    int attackDamage = 10;
    float attackSpeed = 1.0f;
    float attackRange = 1.0f;
    float moveSpeed = 5.0f;

    // Resource costs
    int goldCost = 0;
    int woodCost = 0;
    int foodCost = 0;
    float trainingTime = 10.0f;
    int populationCost = 1;

    // Classification
    std::vector<std::string> tags;
    std::string description;
};

/**
 * @brief Unit comparison data
 */
struct UnitComparison {
    UnitStats unit1;
    UnitStats unit2;

    struct StatDiff {
        std::string name;
        float value1;
        float value2;
        float difference;
        float percentDiff;
    };
    std::vector<StatDiff> differences;
};

/**
 * @brief Unit filter options
 */
struct UnitFilterOptions {
    std::string searchQuery;
    std::vector<std::string> factions;
    std::vector<int> tiers;
    std::vector<std::string> roles;

    // Stat ranges
    std::optional<int> minHealth;
    std::optional<int> maxHealth;
    std::optional<int> minDamage;
    std::optional<int> maxDamage;
    std::optional<float> minSpeed;
    std::optional<float> maxSpeed;
    std::optional<int> minCost;
    std::optional<int> maxCost;
};

/**
 * @brief Unit Browser
 *
 * Specialized browser for unit assets:
 * - Preview unit stats in grid
 * - Compare units side-by-side
 * - Filter by faction/tier/role
 * - Quick edit common properties
 * - Stat visualizations
 * - Balance analysis tools
 */
class UnitBrowser {
public:
    explicit UnitBrowser(Editor* editor, ContentBrowser* contentBrowser);
    ~UnitBrowser();

    // Non-copyable
    UnitBrowser(const UnitBrowser&) = delete;
    UnitBrowser& operator=(const UnitBrowser&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialize
     */
    bool Initialize();

    /**
     * @brief Shutdown
     */
    void Shutdown();

    /**
     * @brief Render the browser
     */
    void Render();

    /**
     * @brief Update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Unit Access
    // =========================================================================

    /**
     * @brief Get all units
     */
    [[nodiscard]] std::vector<UnitStats> GetAllUnits() const;

    /**
     * @brief Get unit by ID
     */
    [[nodiscard]] std::optional<UnitStats> GetUnit(const std::string& id) const;

    /**
     * @brief Get filtered units
     */
    [[nodiscard]] std::vector<UnitStats> GetFilteredUnits() const;

    /**
     * @brief Refresh unit cache
     */
    void RefreshUnits();

    // =========================================================================
    // Filtering
    // =========================================================================

    /**
     * @brief Set filter options
     */
    void SetFilter(const UnitFilterOptions& filter);

    /**
     * @brief Get current filter
     */
    [[nodiscard]] const UnitFilterOptions& GetFilter() const { return m_filter; }

    /**
     * @brief Filter by faction
     */
    void FilterByFaction(const std::string& faction);

    /**
     * @brief Filter by tier
     */
    void FilterByTier(int tier);

    /**
     * @brief Filter by role
     */
    void FilterByRole(const std::string& role);

    /**
     * @brief Clear filters
     */
    void ClearFilters();

    // =========================================================================
    // Comparison
    // =========================================================================

    /**
     * @brief Compare two units
     */
    [[nodiscard]] UnitComparison Compare(const std::string& unitId1, const std::string& unitId2) const;

    /**
     * @brief Set units for comparison
     */
    void SetCompareUnits(const std::string& unitId1, const std::string& unitId2);

    /**
     * @brief Clear comparison
     */
    void ClearComparison();

    /**
     * @brief Check if in comparison mode
     */
    [[nodiscard]] bool IsComparing() const { return m_comparing; }

    /**
     * @brief Add unit to comparison queue
     */
    void AddToComparison(const std::string& unitId);

    // =========================================================================
    // Quick Edit
    // =========================================================================

    /**
     * @brief Quick edit unit property
     */
    bool QuickEditProperty(const std::string& unitId, const std::string& property, const std::string& value);

    /**
     * @brief Get editable properties
     */
    [[nodiscard]] std::vector<std::string> GetEditableProperties() const;

    /**
     * @brief Batch edit property for selected units
     */
    bool BatchEditProperty(const std::vector<std::string>& unitIds,
                           const std::string& property, const std::string& value);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get faction list
     */
    [[nodiscard]] std::vector<std::string> GetFactions() const;

    /**
     * @brief Get tier list
     */
    [[nodiscard]] std::vector<int> GetTiers() const;

    /**
     * @brief Get role list
     */
    [[nodiscard]] std::vector<std::string> GetRoles() const;

    /**
     * @brief Get unit count by faction
     */
    [[nodiscard]] std::unordered_map<std::string, int> GetUnitCountByFaction() const;

    /**
     * @brief Get average stats by faction
     */
    [[nodiscard]] std::unordered_map<std::string, UnitStats> GetAverageStatsByFaction() const;

    // =========================================================================
    // Balance Analysis
    // =========================================================================

    /**
     * @brief Calculate unit power score
     */
    [[nodiscard]] float CalculatePowerScore(const std::string& unitId) const;

    /**
     * @brief Get balance warnings
     */
    [[nodiscard]] std::vector<std::string> GetBalanceWarnings() const;

    /**
     * @brief Compare unit to faction average
     */
    [[nodiscard]] std::unordered_map<std::string, float> CompareToFactionAverage(const std::string& unitId) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnUnitSelected;
    std::function<void(const std::string&)> OnUnitDoubleClicked;
    std::function<void(const std::string&, const std::string&)> OnCompareRequested;

private:
    // Rendering
    void RenderToolbar();
    void RenderFilters();
    void RenderUnitGrid();
    void RenderUnitCard(const UnitStats& unit);
    void RenderComparisonView();
    void RenderStatBar(const std::string& label, float value, float maxValue, const glm::vec4& color);

    // Data loading
    UnitStats LoadUnitStats(const std::string& assetId) const;
    void CacheUnits();

    // Helpers
    bool MatchesFilter(const UnitStats& unit) const;
    std::string FormatStat(float value, const std::string& suffix = "") const;

    Editor* m_editor = nullptr;
    ContentBrowser* m_contentBrowser = nullptr;
    bool m_initialized = false;

    // Cached units
    std::vector<UnitStats> m_allUnits;
    std::vector<UnitStats> m_filteredUnits;
    bool m_needsRefresh = true;

    // Filter state
    UnitFilterOptions m_filter;

    // Selection
    std::string m_selectedUnitId;
    std::vector<std::string> m_multiSelection;

    // Comparison
    bool m_comparing = false;
    std::string m_compareUnit1;
    std::string m_compareUnit2;
    std::vector<std::string> m_comparisonQueue;

    // View options
    int m_gridColumns = 4;
    bool m_showStats = true;
    bool m_showCosts = true;
};

} // namespace Content
} // namespace Vehement
