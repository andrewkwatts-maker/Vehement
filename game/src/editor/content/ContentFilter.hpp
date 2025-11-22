#pragma once

#include "ContentDatabase.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <variant>
#include <optional>
#include <regex>

namespace Vehement {
namespace Content {

/**
 * @brief Comparison operators for property filters
 */
enum class ComparisonOp {
    Equal,
    NotEqual,
    LessThan,
    LessOrEqual,
    GreaterThan,
    GreaterOrEqual,
    Contains,
    NotContains,
    StartsWith,
    EndsWith,
    Matches,  // Regex match
    Exists,
    NotExists
};

/**
 * @brief Property filter value types
 */
using FilterValue = std::variant<std::nullptr_t, bool, int, double, std::string>;

/**
 * @brief Single property filter condition
 */
struct PropertyCondition {
    std::string propertyPath;  // e.g., "combat.damage" or "tags[0]"
    ComparisonOp op = ComparisonOp::Equal;
    FilterValue value;
    bool caseSensitive = false;

    /**
     * @brief Evaluate condition against asset metadata
     */
    [[nodiscard]] bool Evaluate(const AssetMetadata& asset) const;
};

/**
 * @brief Logical combination of conditions
 */
enum class LogicalOp {
    And,
    Or,
    Not
};

/**
 * @brief Composite filter expression (tree structure)
 */
class FilterExpression {
public:
    FilterExpression() = default;

    // Leaf node: single condition
    explicit FilterExpression(const PropertyCondition& condition);

    // Composite node: logical combination
    FilterExpression(LogicalOp op, std::vector<FilterExpression> children);

    // NOT operation (single child)
    static FilterExpression Not(FilterExpression child);

    // AND operation
    static FilterExpression And(std::vector<FilterExpression> children);

    // OR operation
    static FilterExpression Or(std::vector<FilterExpression> children);

    /**
     * @brief Evaluate expression against asset
     */
    [[nodiscard]] bool Evaluate(const AssetMetadata& asset) const;

    /**
     * @brief Check if expression is empty
     */
    [[nodiscard]] bool IsEmpty() const { return m_isEmpty; }

    /**
     * @brief Serialize to string
     */
    [[nodiscard]] std::string ToString() const;

    /**
     * @brief Parse from string
     */
    static FilterExpression Parse(const std::string& expression);

private:
    bool m_isEmpty = true;
    bool m_isLeaf = false;
    PropertyCondition m_condition;
    LogicalOp m_logicalOp = LogicalOp::And;
    std::vector<FilterExpression> m_children;
};

/**
 * @brief Date range specification
 */
struct DateRange {
    std::optional<std::chrono::system_clock::time_point> from;
    std::optional<std::chrono::system_clock::time_point> to;

    [[nodiscard]] bool Contains(std::chrono::system_clock::time_point time) const;
    [[nodiscard]] bool IsEmpty() const { return !from.has_value() && !to.has_value(); }

    // Preset ranges
    static DateRange Today();
    static DateRange Yesterday();
    static DateRange ThisWeek();
    static DateRange ThisMonth();
    static DateRange LastDays(int days);
};

/**
 * @brief Sort field for results
 */
enum class SortField {
    Name,
    Type,
    DateModified,
    DateCreated,
    Size,
    ValidationStatus
};

/**
 * @brief Sort direction
 */
enum class SortDirection {
    Ascending,
    Descending
};

/**
 * @brief Sort specification
 */
struct SortSpec {
    SortField field = SortField::Name;
    SortDirection direction = SortDirection::Ascending;
};

/**
 * @brief Complete filter configuration
 */
struct FilterConfig {
    // Text search
    std::string searchQuery;
    bool searchInName = true;
    bool searchInDescription = true;
    bool searchInTags = true;
    bool searchInProperties = false;
    bool caseSensitive = false;
    bool useRegex = false;

    // Type filters
    std::vector<AssetType> includeTypes;
    std::vector<AssetType> excludeTypes;

    // Tag filters
    std::vector<std::string> requiredTags;      // Asset must have ALL of these
    std::vector<std::string> anyTags;           // Asset must have ANY of these
    std::vector<std::string> excludeTags;       // Asset must not have ANY of these

    // Status filters
    std::vector<ValidationStatus> validationStatuses;
    bool showDirtyOnly = false;
    bool showFavoritesOnly = false;

    // Date filters
    DateRange createdRange;
    DateRange modifiedRange;

    // Size filters
    std::optional<size_t> minSize;
    std::optional<size_t> maxSize;

    // Directory filter
    std::string directoryPath;
    bool includeSubdirectories = true;

    // Property filters (advanced)
    FilterExpression propertyFilter;

    // Sorting
    std::vector<SortSpec> sortSpecs;  // Multiple sort fields (primary, secondary, etc.)

    /**
     * @brief Check if any filters are active
     */
    [[nodiscard]] bool HasActiveFilters() const;

    /**
     * @brief Clear all filters
     */
    void Clear();

    /**
     * @brief Get human-readable filter summary
     */
    [[nodiscard]] std::string GetSummary() const;
};

/**
 * @brief Filter preset for saving/loading
 */
struct FilterPreset {
    std::string id;
    std::string name;
    std::string description;
    std::string icon;
    FilterConfig config;
    bool isBuiltIn = false;
    std::chrono::system_clock::time_point lastUsed;
};

/**
 * @brief Advanced Content Filter
 *
 * Provides comprehensive filtering capabilities:
 * - Type filters (Units, Buildings, Spells, etc.)
 * - Tag filters with AND/OR/NOT logic
 * - Status filters (Valid, Invalid, Modified)
 * - Date range filters
 * - Property filters (e.g., "damage > 50")
 * - Save/load filter presets
 * - Sort by multiple fields
 */
class ContentFilter {
public:
    ContentFilter();
    ~ContentFilter();

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set filter configuration
     */
    void SetConfig(const FilterConfig& config);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const FilterConfig& GetConfig() const { return m_config; }

    /**
     * @brief Modify configuration
     */
    FilterConfig& ModifyConfig() { return m_config; }

    // =========================================================================
    // Quick Filters
    // =========================================================================

    /**
     * @brief Set search query
     */
    void SetSearchQuery(const std::string& query);

    /**
     * @brief Add type filter
     */
    void AddTypeFilter(AssetType type);

    /**
     * @brief Remove type filter
     */
    void RemoveTypeFilter(AssetType type);

    /**
     * @brief Set type filters
     */
    void SetTypeFilters(const std::vector<AssetType>& types);

    /**
     * @brief Add required tag
     */
    void RequireTag(const std::string& tag);

    /**
     * @brief Add any-of tag
     */
    void AddTag(const std::string& tag);

    /**
     * @brief Exclude tag
     */
    void ExcludeTag(const std::string& tag);

    /**
     * @brief Set validation status filter
     */
    void SetStatusFilter(ValidationStatus status);

    /**
     * @brief Filter by modified date range
     */
    void SetDateRange(const DateRange& range);

    /**
     * @brief Show only favorites
     */
    void ShowFavoritesOnly(bool value);

    /**
     * @brief Show only dirty (unsaved) assets
     */
    void ShowDirtyOnly(bool value);

    /**
     * @brief Set directory filter
     */
    void SetDirectory(const std::string& path, bool includeSubdirs = true);

    /**
     * @brief Clear all filters
     */
    void Clear();

    // =========================================================================
    // Property Filters
    // =========================================================================

    /**
     * @brief Add property condition
     * @param path Property path (e.g., "combat.damage")
     * @param op Comparison operator
     * @param value Value to compare against
     */
    void AddPropertyCondition(const std::string& path, ComparisonOp op, const FilterValue& value);

    /**
     * @brief Set property filter expression
     */
    void SetPropertyFilter(const FilterExpression& expression);

    /**
     * @brief Parse and set property filter from string
     * @param expression Expression string (e.g., "damage > 50 AND type = 'infantry'")
     */
    void SetPropertyFilterFromString(const std::string& expression);

    // =========================================================================
    // Sorting
    // =========================================================================

    /**
     * @brief Set primary sort
     */
    void SetSort(SortField field, SortDirection direction = SortDirection::Ascending);

    /**
     * @brief Add secondary sort
     */
    void AddSort(SortField field, SortDirection direction = SortDirection::Ascending);

    /**
     * @brief Clear sort specifications
     */
    void ClearSort();

    // =========================================================================
    // Filtering
    // =========================================================================

    /**
     * @brief Apply filter to asset list
     * @param assets Input assets
     * @return Filtered and sorted assets
     */
    [[nodiscard]] std::vector<AssetMetadata> Apply(const std::vector<AssetMetadata>& assets) const;

    /**
     * @brief Check if asset matches filter
     */
    [[nodiscard]] bool Matches(const AssetMetadata& asset) const;

    /**
     * @brief Get match count for current filter
     */
    [[nodiscard]] size_t GetMatchCount(const std::vector<AssetMetadata>& assets) const;

    // =========================================================================
    // Presets
    // =========================================================================

    /**
     * @brief Save current configuration as preset
     */
    bool SavePreset(const std::string& name, const std::string& description = "");

    /**
     * @brief Load preset by name
     */
    bool LoadPreset(const std::string& name);

    /**
     * @brief Delete preset
     */
    bool DeletePreset(const std::string& name);

    /**
     * @brief Get all presets
     */
    [[nodiscard]] std::vector<FilterPreset> GetPresets() const;

    /**
     * @brief Get built-in presets
     */
    [[nodiscard]] std::vector<FilterPreset> GetBuiltInPresets() const;

    /**
     * @brief Load presets from file
     */
    bool LoadPresetsFromFile(const std::string& path);

    /**
     * @brief Save presets to file
     */
    bool SavePresetsToFile(const std::string& path);

    // =========================================================================
    // Filter History
    // =========================================================================

    /**
     * @brief Get recent filter configurations
     */
    [[nodiscard]] std::vector<FilterConfig> GetRecentFilters() const;

    /**
     * @brief Add current config to history
     */
    void AddToHistory();

    /**
     * @brief Clear filter history
     */
    void ClearHistory();

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void()> OnFilterChanged;

private:
    // Matching helpers
    [[nodiscard]] bool MatchesSearch(const AssetMetadata& asset) const;
    [[nodiscard]] bool MatchesTypes(const AssetMetadata& asset) const;
    [[nodiscard]] bool MatchesTags(const AssetMetadata& asset) const;
    [[nodiscard]] bool MatchesStatus(const AssetMetadata& asset) const;
    [[nodiscard]] bool MatchesDateRange(const AssetMetadata& asset) const;
    [[nodiscard]] bool MatchesSize(const AssetMetadata& asset) const;
    [[nodiscard]] bool MatchesDirectory(const AssetMetadata& asset) const;
    [[nodiscard]] bool MatchesProperties(const AssetMetadata& asset) const;

    // Sorting helpers
    void SortResults(std::vector<AssetMetadata>& assets) const;
    [[nodiscard]] int CompareAssets(const AssetMetadata& a, const AssetMetadata& b, const SortSpec& spec) const;

    // Preset management
    void InitializeBuiltInPresets();
    std::string GeneratePresetId() const;

    FilterConfig m_config;
    std::vector<FilterPreset> m_presets;
    std::vector<FilterConfig> m_filterHistory;
    static constexpr size_t MAX_HISTORY = 20;

    // Cached regex for search
    mutable std::optional<std::regex> m_searchRegex;
    mutable bool m_searchRegexValid = false;
};

/**
 * @brief Filter builder for fluent API
 */
class FilterBuilder {
public:
    FilterBuilder& Search(const std::string& query);
    FilterBuilder& Type(AssetType type);
    FilterBuilder& Types(std::initializer_list<AssetType> types);
    FilterBuilder& Tag(const std::string& tag);
    FilterBuilder& Tags(std::initializer_list<std::string> tags);
    FilterBuilder& ExcludeTag(const std::string& tag);
    FilterBuilder& Status(ValidationStatus status);
    FilterBuilder& Favorites();
    FilterBuilder& Dirty();
    FilterBuilder& InDirectory(const std::string& path);
    FilterBuilder& ModifiedAfter(std::chrono::system_clock::time_point time);
    FilterBuilder& ModifiedBefore(std::chrono::system_clock::time_point time);
    FilterBuilder& ModifiedInLast(int days);
    FilterBuilder& Property(const std::string& path, ComparisonOp op, const FilterValue& value);
    FilterBuilder& SortBy(SortField field, SortDirection dir = SortDirection::Ascending);

    FilterConfig Build() const { return m_config; }

private:
    FilterConfig m_config;
};

} // namespace Content
} // namespace Vehement
