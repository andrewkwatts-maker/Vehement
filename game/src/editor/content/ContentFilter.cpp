#include "ContentFilter.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <json/json.h>
#include <fstream>

namespace Vehement {
namespace Content {

// ============================================================================
// PropertyCondition
// ============================================================================

bool PropertyCondition::Evaluate(const AssetMetadata& asset) const {
    // Get property value
    auto it = asset.properties.find(propertyPath);
    if (it == asset.properties.end()) {
        return op == ComparisonOp::NotExists;
    }

    if (op == ComparisonOp::Exists) {
        return true;
    }

    std::string propValue = it->second;

    // Convert to appropriate type and compare
    if (std::holds_alternative<std::string>(value)) {
        std::string compareValue = std::get<std::string>(value);

        if (!caseSensitive) {
            std::transform(propValue.begin(), propValue.end(), propValue.begin(), ::tolower);
            std::transform(compareValue.begin(), compareValue.end(), compareValue.begin(), ::tolower);
        }

        switch (op) {
            case ComparisonOp::Equal:
                return propValue == compareValue;
            case ComparisonOp::NotEqual:
                return propValue != compareValue;
            case ComparisonOp::Contains:
                return propValue.find(compareValue) != std::string::npos;
            case ComparisonOp::NotContains:
                return propValue.find(compareValue) == std::string::npos;
            case ComparisonOp::StartsWith:
                return propValue.find(compareValue) == 0;
            case ComparisonOp::EndsWith:
                return propValue.length() >= compareValue.length() &&
                       propValue.substr(propValue.length() - compareValue.length()) == compareValue;
            case ComparisonOp::Matches:
                try {
                    std::regex pattern(compareValue);
                    return std::regex_search(propValue, pattern);
                } catch (...) {
                    return false;
                }
            default:
                return false;
        }
    } else if (std::holds_alternative<double>(value) || std::holds_alternative<int>(value)) {
        double numValue = 0.0;
        try {
            numValue = std::stod(propValue);
        } catch (...) {
            return false;
        }

        double compareValue = std::holds_alternative<double>(value) ?
                              std::get<double>(value) :
                              static_cast<double>(std::get<int>(value));

        switch (op) {
            case ComparisonOp::Equal:
                return std::abs(numValue - compareValue) < 0.0001;
            case ComparisonOp::NotEqual:
                return std::abs(numValue - compareValue) >= 0.0001;
            case ComparisonOp::LessThan:
                return numValue < compareValue;
            case ComparisonOp::LessOrEqual:
                return numValue <= compareValue;
            case ComparisonOp::GreaterThan:
                return numValue > compareValue;
            case ComparisonOp::GreaterOrEqual:
                return numValue >= compareValue;
            default:
                return false;
        }
    } else if (std::holds_alternative<bool>(value)) {
        bool boolValue = (propValue == "true" || propValue == "1" || propValue == "yes");
        bool compareValue = std::get<bool>(value);

        switch (op) {
            case ComparisonOp::Equal:
                return boolValue == compareValue;
            case ComparisonOp::NotEqual:
                return boolValue != compareValue;
            default:
                return false;
        }
    }

    return false;
}

// ============================================================================
// FilterExpression
// ============================================================================

FilterExpression::FilterExpression(const PropertyCondition& condition)
    : m_isEmpty(false), m_isLeaf(true), m_condition(condition)
{
}

FilterExpression::FilterExpression(LogicalOp op, std::vector<FilterExpression> children)
    : m_isEmpty(children.empty()), m_isLeaf(false), m_logicalOp(op), m_children(std::move(children))
{
}

FilterExpression FilterExpression::Not(FilterExpression child) {
    return FilterExpression(LogicalOp::Not, {std::move(child)});
}

FilterExpression FilterExpression::And(std::vector<FilterExpression> children) {
    return FilterExpression(LogicalOp::And, std::move(children));
}

FilterExpression FilterExpression::Or(std::vector<FilterExpression> children) {
    return FilterExpression(LogicalOp::Or, std::move(children));
}

bool FilterExpression::Evaluate(const AssetMetadata& asset) const {
    if (m_isEmpty) {
        return true;
    }

    if (m_isLeaf) {
        return m_condition.Evaluate(asset);
    }

    switch (m_logicalOp) {
        case LogicalOp::And:
            for (const auto& child : m_children) {
                if (!child.Evaluate(asset)) {
                    return false;
                }
            }
            return true;

        case LogicalOp::Or:
            for (const auto& child : m_children) {
                if (child.Evaluate(asset)) {
                    return true;
                }
            }
            return false;

        case LogicalOp::Not:
            return !m_children.empty() && !m_children[0].Evaluate(asset);

        default:
            return true;
    }
}

std::string FilterExpression::ToString() const {
    if (m_isEmpty) {
        return "";
    }

    if (m_isLeaf) {
        std::stringstream ss;
        ss << m_condition.propertyPath;

        switch (m_condition.op) {
            case ComparisonOp::Equal: ss << " = "; break;
            case ComparisonOp::NotEqual: ss << " != "; break;
            case ComparisonOp::LessThan: ss << " < "; break;
            case ComparisonOp::LessOrEqual: ss << " <= "; break;
            case ComparisonOp::GreaterThan: ss << " > "; break;
            case ComparisonOp::GreaterOrEqual: ss << " >= "; break;
            case ComparisonOp::Contains: ss << " contains "; break;
            case ComparisonOp::StartsWith: ss << " startsWith "; break;
            case ComparisonOp::EndsWith: ss << " endsWith "; break;
            default: ss << " ? "; break;
        }

        if (std::holds_alternative<std::string>(m_condition.value)) {
            ss << "'" << std::get<std::string>(m_condition.value) << "'";
        } else if (std::holds_alternative<double>(m_condition.value)) {
            ss << std::get<double>(m_condition.value);
        } else if (std::holds_alternative<int>(m_condition.value)) {
            ss << std::get<int>(m_condition.value);
        } else if (std::holds_alternative<bool>(m_condition.value)) {
            ss << (std::get<bool>(m_condition.value) ? "true" : "false");
        }

        return ss.str();
    }

    std::stringstream ss;
    std::string opStr;

    switch (m_logicalOp) {
        case LogicalOp::And: opStr = " AND "; break;
        case LogicalOp::Or: opStr = " OR "; break;
        case LogicalOp::Not: opStr = "NOT "; break;
    }

    if (m_logicalOp == LogicalOp::Not) {
        ss << opStr << "(" << m_children[0].ToString() << ")";
    } else {
        ss << "(";
        for (size_t i = 0; i < m_children.size(); ++i) {
            if (i > 0) {
                ss << opStr;
            }
            ss << m_children[i].ToString();
        }
        ss << ")";
    }

    return ss.str();
}

FilterExpression FilterExpression::Parse(const std::string& expression) {
    // Simple parser for basic expressions
    // Format: "property op value" or "expr AND expr" or "expr OR expr"

    std::string trimmed = expression;
    // Trim whitespace
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    if (trimmed.empty()) {
        return FilterExpression();
    }

    // Check for AND/OR
    size_t andPos = trimmed.find(" AND ");
    if (andPos != std::string::npos) {
        std::vector<FilterExpression> children;
        children.push_back(Parse(trimmed.substr(0, andPos)));
        children.push_back(Parse(trimmed.substr(andPos + 5)));
        return And(std::move(children));
    }

    size_t orPos = trimmed.find(" OR ");
    if (orPos != std::string::npos) {
        std::vector<FilterExpression> children;
        children.push_back(Parse(trimmed.substr(0, orPos)));
        children.push_back(Parse(trimmed.substr(orPos + 4)));
        return Or(std::move(children));
    }

    // Parse single condition
    PropertyCondition condition;

    // Find operator
    struct OpInfo {
        std::string str;
        ComparisonOp op;
    };
    std::vector<OpInfo> operators = {
        {">=", ComparisonOp::GreaterOrEqual},
        {"<=", ComparisonOp::LessOrEqual},
        {"!=", ComparisonOp::NotEqual},
        {"=", ComparisonOp::Equal},
        {">", ComparisonOp::GreaterThan},
        {"<", ComparisonOp::LessThan},
        {" contains ", ComparisonOp::Contains},
        {" startsWith ", ComparisonOp::StartsWith},
        {" endsWith ", ComparisonOp::EndsWith}
    };

    for (const auto& opInfo : operators) {
        size_t pos = trimmed.find(opInfo.str);
        if (pos != std::string::npos) {
            condition.propertyPath = trimmed.substr(0, pos);
            condition.op = opInfo.op;

            std::string valueStr = trimmed.substr(pos + opInfo.str.length());
            // Trim
            condition.propertyPath.erase(condition.propertyPath.find_last_not_of(" \t") + 1);
            valueStr.erase(0, valueStr.find_first_not_of(" \t"));

            // Parse value
            if (valueStr.front() == '\'' && valueStr.back() == '\'') {
                condition.value = valueStr.substr(1, valueStr.length() - 2);
            } else if (valueStr == "true" || valueStr == "false") {
                condition.value = (valueStr == "true");
            } else {
                try {
                    if (valueStr.find('.') != std::string::npos) {
                        condition.value = std::stod(valueStr);
                    } else {
                        condition.value = std::stoi(valueStr);
                    }
                } catch (...) {
                    condition.value = valueStr;
                }
            }

            return FilterExpression(condition);
        }
    }

    return FilterExpression();
}

// ============================================================================
// DateRange
// ============================================================================

bool DateRange::Contains(std::chrono::system_clock::time_point time) const {
    if (from.has_value() && time < from.value()) {
        return false;
    }
    if (to.has_value() && time > to.value()) {
        return false;
    }
    return true;
}

DateRange DateRange::Today() {
    auto now = std::chrono::system_clock::now();
    auto today = std::chrono::floor<std::chrono::days>(now);

    DateRange range;
    range.from = today;
    range.to = today + std::chrono::days(1);
    return range;
}

DateRange DateRange::Yesterday() {
    auto now = std::chrono::system_clock::now();
    auto today = std::chrono::floor<std::chrono::days>(now);

    DateRange range;
    range.from = today - std::chrono::days(1);
    range.to = today;
    return range;
}

DateRange DateRange::ThisWeek() {
    auto now = std::chrono::system_clock::now();
    auto today = std::chrono::floor<std::chrono::days>(now);

    DateRange range;
    range.from = today - std::chrono::days(7);
    range.to = now;
    return range;
}

DateRange DateRange::ThisMonth() {
    auto now = std::chrono::system_clock::now();
    auto today = std::chrono::floor<std::chrono::days>(now);

    DateRange range;
    range.from = today - std::chrono::days(30);
    range.to = now;
    return range;
}

DateRange DateRange::LastDays(int days) {
    auto now = std::chrono::system_clock::now();

    DateRange range;
    range.from = now - std::chrono::days(days);
    range.to = now;
    return range;
}

// ============================================================================
// FilterConfig
// ============================================================================

bool FilterConfig::HasActiveFilters() const {
    return !searchQuery.empty() ||
           !includeTypes.empty() ||
           !excludeTypes.empty() ||
           !requiredTags.empty() ||
           !anyTags.empty() ||
           !excludeTags.empty() ||
           !validationStatuses.empty() ||
           showDirtyOnly ||
           showFavoritesOnly ||
           !createdRange.IsEmpty() ||
           !modifiedRange.IsEmpty() ||
           minSize.has_value() ||
           maxSize.has_value() ||
           !directoryPath.empty() ||
           !propertyFilter.IsEmpty();
}

void FilterConfig::Clear() {
    searchQuery.clear();
    includeTypes.clear();
    excludeTypes.clear();
    requiredTags.clear();
    anyTags.clear();
    excludeTags.clear();
    validationStatuses.clear();
    showDirtyOnly = false;
    showFavoritesOnly = false;
    createdRange = DateRange();
    modifiedRange = DateRange();
    minSize.reset();
    maxSize.reset();
    directoryPath.clear();
    propertyFilter = FilterExpression();
    sortSpecs.clear();
}

std::string FilterConfig::GetSummary() const {
    std::vector<std::string> parts;

    if (!searchQuery.empty()) {
        parts.push_back("\"" + searchQuery + "\"");
    }
    if (!includeTypes.empty()) {
        parts.push_back(std::to_string(includeTypes.size()) + " type(s)");
    }
    if (!requiredTags.empty() || !anyTags.empty()) {
        parts.push_back(std::to_string(requiredTags.size() + anyTags.size()) + " tag(s)");
    }
    if (showDirtyOnly) {
        parts.push_back("modified");
    }
    if (showFavoritesOnly) {
        parts.push_back("favorites");
    }

    if (parts.empty()) {
        return "No filters";
    }

    std::stringstream ss;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << parts[i];
    }
    return ss.str();
}

// ============================================================================
// ContentFilter
// ============================================================================

ContentFilter::ContentFilter() {
    InitializeBuiltInPresets();
}

ContentFilter::~ContentFilter() = default;

void ContentFilter::SetConfig(const FilterConfig& config) {
    m_config = config;
    m_searchRegexValid = false;

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

void ContentFilter::SetSearchQuery(const std::string& query) {
    m_config.searchQuery = query;
    m_searchRegexValid = false;

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

void ContentFilter::AddTypeFilter(AssetType type) {
    if (std::find(m_config.includeTypes.begin(), m_config.includeTypes.end(), type) ==
        m_config.includeTypes.end()) {
        m_config.includeTypes.push_back(type);

        if (OnFilterChanged) {
            OnFilterChanged();
        }
    }
}

void ContentFilter::RemoveTypeFilter(AssetType type) {
    auto it = std::find(m_config.includeTypes.begin(), m_config.includeTypes.end(), type);
    if (it != m_config.includeTypes.end()) {
        m_config.includeTypes.erase(it);

        if (OnFilterChanged) {
            OnFilterChanged();
        }
    }
}

void ContentFilter::SetTypeFilters(const std::vector<AssetType>& types) {
    m_config.includeTypes = types;

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

void ContentFilter::RequireTag(const std::string& tag) {
    if (std::find(m_config.requiredTags.begin(), m_config.requiredTags.end(), tag) ==
        m_config.requiredTags.end()) {
        m_config.requiredTags.push_back(tag);

        if (OnFilterChanged) {
            OnFilterChanged();
        }
    }
}

void ContentFilter::AddTag(const std::string& tag) {
    if (std::find(m_config.anyTags.begin(), m_config.anyTags.end(), tag) ==
        m_config.anyTags.end()) {
        m_config.anyTags.push_back(tag);

        if (OnFilterChanged) {
            OnFilterChanged();
        }
    }
}

void ContentFilter::ExcludeTag(const std::string& tag) {
    if (std::find(m_config.excludeTags.begin(), m_config.excludeTags.end(), tag) ==
        m_config.excludeTags.end()) {
        m_config.excludeTags.push_back(tag);

        if (OnFilterChanged) {
            OnFilterChanged();
        }
    }
}

void ContentFilter::SetStatusFilter(ValidationStatus status) {
    m_config.validationStatuses = {status};

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

void ContentFilter::SetDateRange(const DateRange& range) {
    m_config.modifiedRange = range;

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

void ContentFilter::ShowFavoritesOnly(bool value) {
    m_config.showFavoritesOnly = value;

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

void ContentFilter::ShowDirtyOnly(bool value) {
    m_config.showDirtyOnly = value;

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

void ContentFilter::SetDirectory(const std::string& path, bool includeSubdirs) {
    m_config.directoryPath = path;
    m_config.includeSubdirectories = includeSubdirs;

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

void ContentFilter::Clear() {
    m_config.Clear();
    m_searchRegexValid = false;

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

void ContentFilter::AddPropertyCondition(const std::string& path, ComparisonOp op,
                                          const FilterValue& value) {
    PropertyCondition condition;
    condition.propertyPath = path;
    condition.op = op;
    condition.value = value;

    if (m_config.propertyFilter.IsEmpty()) {
        m_config.propertyFilter = FilterExpression(condition);
    } else {
        m_config.propertyFilter = FilterExpression::And({
            m_config.propertyFilter,
            FilterExpression(condition)
        });
    }

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

void ContentFilter::SetPropertyFilter(const FilterExpression& expression) {
    m_config.propertyFilter = expression;

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

void ContentFilter::SetPropertyFilterFromString(const std::string& expression) {
    m_config.propertyFilter = FilterExpression::Parse(expression);

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

void ContentFilter::SetSort(SortField field, SortDirection direction) {
    m_config.sortSpecs.clear();
    m_config.sortSpecs.push_back({field, direction});

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

void ContentFilter::AddSort(SortField field, SortDirection direction) {
    m_config.sortSpecs.push_back({field, direction});

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

void ContentFilter::ClearSort() {
    m_config.sortSpecs.clear();

    if (OnFilterChanged) {
        OnFilterChanged();
    }
}

std::vector<AssetMetadata> ContentFilter::Apply(const std::vector<AssetMetadata>& assets) const {
    std::vector<AssetMetadata> result;
    result.reserve(assets.size());

    for (const auto& asset : assets) {
        if (Matches(asset)) {
            result.push_back(asset);
        }
    }

    SortResults(result);
    return result;
}

bool ContentFilter::Matches(const AssetMetadata& asset) const {
    return MatchesSearch(asset) &&
           MatchesTypes(asset) &&
           MatchesTags(asset) &&
           MatchesStatus(asset) &&
           MatchesDateRange(asset) &&
           MatchesSize(asset) &&
           MatchesDirectory(asset) &&
           MatchesProperties(asset);
}

size_t ContentFilter::GetMatchCount(const std::vector<AssetMetadata>& assets) const {
    size_t count = 0;
    for (const auto& asset : assets) {
        if (Matches(asset)) {
            count++;
        }
    }
    return count;
}

bool ContentFilter::MatchesSearch(const AssetMetadata& asset) const {
    if (m_config.searchQuery.empty()) {
        return true;
    }

    std::string query = m_config.searchQuery;
    if (!m_config.caseSensitive) {
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);
    }

    auto contains = [this](const std::string& text, const std::string& query) {
        std::string searchText = text;
        if (!m_config.caseSensitive) {
            std::transform(searchText.begin(), searchText.end(), searchText.begin(), ::tolower);
        }
        return searchText.find(query) != std::string::npos;
    };

    if (m_config.searchInName && contains(asset.name, query)) {
        return true;
    }
    if (m_config.searchInDescription && contains(asset.description, query)) {
        return true;
    }
    if (m_config.searchInTags) {
        for (const auto& tag : asset.tags) {
            if (contains(tag, query)) {
                return true;
            }
        }
    }

    return false;
}

bool ContentFilter::MatchesTypes(const AssetMetadata& asset) const {
    // Check exclude list first
    for (const auto& type : m_config.excludeTypes) {
        if (asset.type == type) {
            return false;
        }
    }

    // If no include filter, allow all
    if (m_config.includeTypes.empty()) {
        return true;
    }

    // Check include list
    for (const auto& type : m_config.includeTypes) {
        if (asset.type == type) {
            return true;
        }
    }

    return false;
}

bool ContentFilter::MatchesTags(const AssetMetadata& asset) const {
    // Check exclude tags
    for (const auto& tag : m_config.excludeTags) {
        if (std::find(asset.tags.begin(), asset.tags.end(), tag) != asset.tags.end()) {
            return false;
        }
    }

    // Check required tags (must have ALL)
    for (const auto& tag : m_config.requiredTags) {
        if (std::find(asset.tags.begin(), asset.tags.end(), tag) == asset.tags.end()) {
            return false;
        }
    }

    // Check any tags (must have AT LEAST ONE)
    if (!m_config.anyTags.empty()) {
        bool hasAny = false;
        for (const auto& tag : m_config.anyTags) {
            if (std::find(asset.tags.begin(), asset.tags.end(), tag) != asset.tags.end()) {
                hasAny = true;
                break;
            }
        }
        if (!hasAny) {
            return false;
        }
    }

    return true;
}

bool ContentFilter::MatchesStatus(const AssetMetadata& asset) const {
    if (m_config.showDirtyOnly && !asset.isDirty) {
        return false;
    }

    if (m_config.showFavoritesOnly && !asset.isFavorite) {
        return false;
    }

    if (!m_config.validationStatuses.empty()) {
        bool found = false;
        for (const auto& status : m_config.validationStatuses) {
            if (asset.validationStatus == status) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

bool ContentFilter::MatchesDateRange(const AssetMetadata& asset) const {
    if (!m_config.createdRange.IsEmpty() && !m_config.createdRange.Contains(asset.createdTime)) {
        return false;
    }

    if (!m_config.modifiedRange.IsEmpty() && !m_config.modifiedRange.Contains(asset.modifiedTime)) {
        return false;
    }

    return true;
}

bool ContentFilter::MatchesSize(const AssetMetadata& asset) const {
    if (m_config.minSize.has_value() && asset.fileSize < m_config.minSize.value()) {
        return false;
    }

    if (m_config.maxSize.has_value() && asset.fileSize > m_config.maxSize.value()) {
        return false;
    }

    return true;
}

bool ContentFilter::MatchesDirectory(const AssetMetadata& asset) const {
    if (m_config.directoryPath.empty()) {
        return true;
    }

    if (m_config.includeSubdirectories) {
        return asset.filePath.find(m_config.directoryPath) != std::string::npos;
    } else {
        std::string assetDir = asset.filePath.substr(0, asset.filePath.rfind('/'));
        return assetDir == m_config.directoryPath;
    }
}

bool ContentFilter::MatchesProperties(const AssetMetadata& asset) const {
    if (m_config.propertyFilter.IsEmpty()) {
        return true;
    }

    return m_config.propertyFilter.Evaluate(asset);
}

void ContentFilter::SortResults(std::vector<AssetMetadata>& assets) const {
    if (m_config.sortSpecs.empty()) {
        // Default sort by name
        std::sort(assets.begin(), assets.end(),
                  [](const AssetMetadata& a, const AssetMetadata& b) {
                      return a.name < b.name;
                  });
        return;
    }

    std::sort(assets.begin(), assets.end(),
              [this](const AssetMetadata& a, const AssetMetadata& b) {
                  for (const auto& spec : m_config.sortSpecs) {
                      int cmp = CompareAssets(a, b, spec);
                      if (cmp != 0) {
                          return cmp < 0;
                      }
                  }
                  return false;
              });
}

int ContentFilter::CompareAssets(const AssetMetadata& a, const AssetMetadata& b,
                                  const SortSpec& spec) const {
    int result = 0;

    switch (spec.field) {
        case SortField::Name:
            result = a.name.compare(b.name);
            break;

        case SortField::Type:
            result = static_cast<int>(a.type) - static_cast<int>(b.type);
            break;

        case SortField::DateModified:
            if (a.modifiedTime < b.modifiedTime) result = -1;
            else if (a.modifiedTime > b.modifiedTime) result = 1;
            break;

        case SortField::DateCreated:
            if (a.createdTime < b.createdTime) result = -1;
            else if (a.createdTime > b.createdTime) result = 1;
            break;

        case SortField::Size:
            if (a.fileSize < b.fileSize) result = -1;
            else if (a.fileSize > b.fileSize) result = 1;
            break;

        case SortField::ValidationStatus:
            result = static_cast<int>(a.validationStatus) - static_cast<int>(b.validationStatus);
            break;
    }

    return spec.direction == SortDirection::Descending ? -result : result;
}

// ============================================================================
// Presets
// ============================================================================

void ContentFilter::InitializeBuiltInPresets() {
    // Units preset
    FilterPreset unitsPreset;
    unitsPreset.id = "builtin_units";
    unitsPreset.name = "All Units";
    unitsPreset.description = "Show all unit configurations";
    unitsPreset.isBuiltIn = true;
    unitsPreset.config.includeTypes = {AssetType::Unit};
    m_presets.push_back(unitsPreset);

    // Spells preset
    FilterPreset spellsPreset;
    spellsPreset.id = "builtin_spells";
    spellsPreset.name = "All Spells";
    spellsPreset.description = "Show all spell configurations";
    spellsPreset.isBuiltIn = true;
    spellsPreset.config.includeTypes = {AssetType::Spell};
    m_presets.push_back(spellsPreset);

    // Recent preset
    FilterPreset recentPreset;
    recentPreset.id = "builtin_recent";
    recentPreset.name = "Recently Modified";
    recentPreset.description = "Assets modified in the last 7 days";
    recentPreset.isBuiltIn = true;
    recentPreset.config.modifiedRange = DateRange::ThisWeek();
    recentPreset.config.sortSpecs = {{SortField::DateModified, SortDirection::Descending}};
    m_presets.push_back(recentPreset);

    // Favorites preset
    FilterPreset favoritesPreset;
    favoritesPreset.id = "builtin_favorites";
    favoritesPreset.name = "Favorites";
    favoritesPreset.description = "Show favorite assets";
    favoritesPreset.isBuiltIn = true;
    favoritesPreset.config.showFavoritesOnly = true;
    m_presets.push_back(favoritesPreset);

    // Invalid preset
    FilterPreset invalidPreset;
    invalidPreset.id = "builtin_invalid";
    invalidPreset.name = "Invalid Assets";
    invalidPreset.description = "Assets with validation errors";
    invalidPreset.isBuiltIn = true;
    invalidPreset.config.validationStatuses = {ValidationStatus::Error};
    m_presets.push_back(invalidPreset);
}

bool ContentFilter::SavePreset(const std::string& name, const std::string& description) {
    FilterPreset preset;
    preset.id = GeneratePresetId();
    preset.name = name;
    preset.description = description;
    preset.config = m_config;
    preset.lastUsed = std::chrono::system_clock::now();

    m_presets.push_back(preset);
    return true;
}

bool ContentFilter::LoadPreset(const std::string& name) {
    for (auto& preset : m_presets) {
        if (preset.name == name || preset.id == name) {
            m_config = preset.config;
            preset.lastUsed = std::chrono::system_clock::now();

            if (OnFilterChanged) {
                OnFilterChanged();
            }
            return true;
        }
    }
    return false;
}

bool ContentFilter::DeletePreset(const std::string& name) {
    auto it = std::find_if(m_presets.begin(), m_presets.end(),
                           [&name](const FilterPreset& p) {
                               return (p.name == name || p.id == name) && !p.isBuiltIn;
                           });

    if (it != m_presets.end()) {
        m_presets.erase(it);
        return true;
    }
    return false;
}

std::vector<FilterPreset> ContentFilter::GetPresets() const {
    return m_presets;
}

std::vector<FilterPreset> ContentFilter::GetBuiltInPresets() const {
    std::vector<FilterPreset> result;
    for (const auto& preset : m_presets) {
        if (preset.isBuiltIn) {
            result.push_back(preset);
        }
    }
    return result;
}

std::string ContentFilter::GeneratePresetId() const {
    return "preset_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

bool ContentFilter::LoadPresetsFromFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        Json::Value root;
        file >> root;

        // Don't clear built-in presets
        m_presets.erase(
            std::remove_if(m_presets.begin(), m_presets.end(),
                           [](const FilterPreset& p) { return !p.isBuiltIn; }),
            m_presets.end());

        if (root.isMember("presets") && root["presets"].isArray()) {
            for (const auto& presetJson : root["presets"]) {
                FilterPreset preset;
                preset.id = presetJson.get("id", "").asString();
                preset.name = presetJson.get("name", "").asString();
                preset.description = presetJson.get("description", "").asString();
                preset.isBuiltIn = false;

                // Load config
                if (presetJson.isMember("config")) {
                    const auto& cfg = presetJson["config"];
                    preset.config.searchQuery = cfg.get("searchQuery", "").asString();
                    // ... load other config fields
                }

                m_presets.push_back(preset);
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool ContentFilter::SavePresetsToFile(const std::string& path) {
    try {
        Json::Value root;
        Json::Value presetsJson(Json::arrayValue);

        for (const auto& preset : m_presets) {
            if (preset.isBuiltIn) {
                continue;
            }

            Json::Value presetJson;
            presetJson["id"] = preset.id;
            presetJson["name"] = preset.name;
            presetJson["description"] = preset.description;

            // Save config
            Json::Value cfg;
            cfg["searchQuery"] = preset.config.searchQuery;
            // ... save other config fields

            presetJson["config"] = cfg;
            presetsJson.append(presetJson);
        }

        root["presets"] = presetsJson;

        std::ofstream file(path);
        file << root;

        return true;
    } catch (...) {
        return false;
    }
}

std::vector<FilterConfig> ContentFilter::GetRecentFilters() const {
    return m_filterHistory;
}

void ContentFilter::AddToHistory() {
    // Don't add if empty
    if (!m_config.HasActiveFilters()) {
        return;
    }

    // Add to front
    m_filterHistory.insert(m_filterHistory.begin(), m_config);

    // Limit size
    if (m_filterHistory.size() > MAX_HISTORY) {
        m_filterHistory.resize(MAX_HISTORY);
    }
}

void ContentFilter::ClearHistory() {
    m_filterHistory.clear();
}

// ============================================================================
// FilterBuilder
// ============================================================================

FilterBuilder& FilterBuilder::Search(const std::string& query) {
    m_config.searchQuery = query;
    return *this;
}

FilterBuilder& FilterBuilder::Type(AssetType type) {
    m_config.includeTypes.push_back(type);
    return *this;
}

FilterBuilder& FilterBuilder::Types(std::initializer_list<AssetType> types) {
    m_config.includeTypes.insert(m_config.includeTypes.end(), types.begin(), types.end());
    return *this;
}

FilterBuilder& FilterBuilder::Tag(const std::string& tag) {
    m_config.requiredTags.push_back(tag);
    return *this;
}

FilterBuilder& FilterBuilder::Tags(std::initializer_list<std::string> tags) {
    m_config.requiredTags.insert(m_config.requiredTags.end(), tags.begin(), tags.end());
    return *this;
}

FilterBuilder& FilterBuilder::ExcludeTag(const std::string& tag) {
    m_config.excludeTags.push_back(tag);
    return *this;
}

FilterBuilder& FilterBuilder::Status(ValidationStatus status) {
    m_config.validationStatuses.push_back(status);
    return *this;
}

FilterBuilder& FilterBuilder::Favorites() {
    m_config.showFavoritesOnly = true;
    return *this;
}

FilterBuilder& FilterBuilder::Dirty() {
    m_config.showDirtyOnly = true;
    return *this;
}

FilterBuilder& FilterBuilder::InDirectory(const std::string& path) {
    m_config.directoryPath = path;
    return *this;
}

FilterBuilder& FilterBuilder::ModifiedAfter(std::chrono::system_clock::time_point time) {
    m_config.modifiedRange.from = time;
    return *this;
}

FilterBuilder& FilterBuilder::ModifiedBefore(std::chrono::system_clock::time_point time) {
    m_config.modifiedRange.to = time;
    return *this;
}

FilterBuilder& FilterBuilder::ModifiedInLast(int days) {
    m_config.modifiedRange = DateRange::LastDays(days);
    return *this;
}

FilterBuilder& FilterBuilder::Property(const std::string& path, ComparisonOp op,
                                         const FilterValue& value) {
    PropertyCondition condition;
    condition.propertyPath = path;
    condition.op = op;
    condition.value = value;

    if (m_config.propertyFilter.IsEmpty()) {
        m_config.propertyFilter = FilterExpression(condition);
    } else {
        m_config.propertyFilter = FilterExpression::And({
            m_config.propertyFilter,
            FilterExpression(condition)
        });
    }
    return *this;
}

FilterBuilder& FilterBuilder::SortBy(SortField field, SortDirection dir) {
    m_config.sortSpecs.push_back({field, dir});
    return *this;
}

} // namespace Content
} // namespace Vehement
