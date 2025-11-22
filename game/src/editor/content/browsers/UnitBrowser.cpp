#include "UnitBrowser.hpp"
#include "../ContentBrowser.hpp"
#include "../ContentDatabase.hpp"
#include "../../Editor.hpp"
#include <Json/Json.h>
#include <imgui.h>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace Vehement {
namespace Content {

// =============================================================================
// Constructor / Destructor
// =============================================================================

UnitBrowser::UnitBrowser(Editor* editor, ContentBrowser* contentBrowser)
    : m_editor(editor)
    , m_contentBrowser(contentBrowser)
{
}

UnitBrowser::~UnitBrowser() {
    Shutdown();
}

// =============================================================================
// Lifecycle
// =============================================================================

bool UnitBrowser::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Cache all units
    CacheUnits();

    m_initialized = true;
    return true;
}

void UnitBrowser::Shutdown() {
    m_allUnits.clear();
    m_filteredUnits.clear();
    m_initialized = false;
}

void UnitBrowser::Render() {
    ImGui::Begin("Unit Browser", nullptr, ImGuiWindowFlags_MenuBar);

    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Stats", nullptr, &m_showStats);
            ImGui::MenuItem("Show Costs", nullptr, &m_showCosts);
            ImGui::Separator();
            if (ImGui::BeginMenu("Grid Columns")) {
                if (ImGui::MenuItem("2", nullptr, m_gridColumns == 2)) m_gridColumns = 2;
                if (ImGui::MenuItem("3", nullptr, m_gridColumns == 3)) m_gridColumns = 3;
                if (ImGui::MenuItem("4", nullptr, m_gridColumns == 4)) m_gridColumns = 4;
                if (ImGui::MenuItem("5", nullptr, m_gridColumns == 5)) m_gridColumns = 5;
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Filter")) {
            if (ImGui::MenuItem("Clear Filters")) {
                ClearFilters();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // Toolbar
    RenderToolbar();

    // Main area with filters on left
    ImGui::BeginChild("FilterPanel", ImVec2(200, 0), true);
    RenderFilters();
    ImGui::EndChild();

    ImGui::SameLine();

    // Content area
    ImGui::BeginChild("UnitContent", ImVec2(0, 0), true);

    if (m_comparing) {
        RenderComparisonView();
    } else {
        RenderUnitGrid();
    }

    ImGui::EndChild();

    ImGui::End();
}

void UnitBrowser::Update(float deltaTime) {
    if (m_needsRefresh) {
        CacheUnits();
        m_needsRefresh = false;
    }
}

// =============================================================================
// Unit Access
// =============================================================================

std::vector<UnitStats> UnitBrowser::GetAllUnits() const {
    return m_allUnits;
}

std::optional<UnitStats> UnitBrowser::GetUnit(const std::string& id) const {
    for (const auto& unit : m_allUnits) {
        if (unit.id == id) {
            return unit;
        }
    }
    return std::nullopt;
}

std::vector<UnitStats> UnitBrowser::GetFilteredUnits() const {
    return m_filteredUnits;
}

void UnitBrowser::RefreshUnits() {
    m_needsRefresh = true;
}

// =============================================================================
// Filtering
// =============================================================================

void UnitBrowser::SetFilter(const UnitFilterOptions& filter) {
    m_filter = filter;

    // Apply filter
    m_filteredUnits.clear();
    for (const auto& unit : m_allUnits) {
        if (MatchesFilter(unit)) {
            m_filteredUnits.push_back(unit);
        }
    }
}

void UnitBrowser::FilterByFaction(const std::string& faction) {
    m_filter.factions.clear();
    m_filter.factions.push_back(faction);
    SetFilter(m_filter);
}

void UnitBrowser::FilterByTier(int tier) {
    m_filter.tiers.clear();
    m_filter.tiers.push_back(tier);
    SetFilter(m_filter);
}

void UnitBrowser::FilterByRole(const std::string& role) {
    m_filter.roles.clear();
    m_filter.roles.push_back(role);
    SetFilter(m_filter);
}

void UnitBrowser::ClearFilters() {
    m_filter = UnitFilterOptions();
    m_filteredUnits = m_allUnits;
}

// =============================================================================
// Comparison
// =============================================================================

UnitComparison UnitBrowser::Compare(const std::string& unitId1, const std::string& unitId2) const {
    UnitComparison comparison;

    auto unit1Opt = GetUnit(unitId1);
    auto unit2Opt = GetUnit(unitId2);

    if (!unit1Opt || !unit2Opt) {
        return comparison;
    }

    comparison.unit1 = *unit1Opt;
    comparison.unit2 = *unit2Opt;

    // Calculate stat differences
    auto addDiff = [&](const std::string& name, float v1, float v2) {
        UnitComparison::StatDiff diff;
        diff.name = name;
        diff.value1 = v1;
        diff.value2 = v2;
        diff.difference = v2 - v1;
        diff.percentDiff = (v1 != 0) ? ((v2 - v1) / v1) * 100.0f : 0.0f;
        comparison.differences.push_back(diff);
    };

    addDiff("Health", static_cast<float>(comparison.unit1.health), static_cast<float>(comparison.unit2.health));
    addDiff("Armor", static_cast<float>(comparison.unit1.armor), static_cast<float>(comparison.unit2.armor));
    addDiff("Attack Damage", static_cast<float>(comparison.unit1.attackDamage), static_cast<float>(comparison.unit2.attackDamage));
    addDiff("Attack Speed", comparison.unit1.attackSpeed, comparison.unit2.attackSpeed);
    addDiff("Attack Range", comparison.unit1.attackRange, comparison.unit2.attackRange);
    addDiff("Move Speed", comparison.unit1.moveSpeed, comparison.unit2.moveSpeed);
    addDiff("Gold Cost", static_cast<float>(comparison.unit1.goldCost), static_cast<float>(comparison.unit2.goldCost));
    addDiff("Training Time", comparison.unit1.trainingTime, comparison.unit2.trainingTime);

    return comparison;
}

void UnitBrowser::SetCompareUnits(const std::string& unitId1, const std::string& unitId2) {
    m_compareUnit1 = unitId1;
    m_compareUnit2 = unitId2;
    m_comparing = true;
}

void UnitBrowser::ClearComparison() {
    m_comparing = false;
    m_compareUnit1.clear();
    m_compareUnit2.clear();
    m_comparisonQueue.clear();
}

void UnitBrowser::AddToComparison(const std::string& unitId) {
    // Add to queue
    m_comparisonQueue.push_back(unitId);

    // If we have 2 units, start comparison
    if (m_comparisonQueue.size() >= 2) {
        SetCompareUnits(m_comparisonQueue[0], m_comparisonQueue[1]);
        m_comparisonQueue.clear();
    }
}

// =============================================================================
// Quick Edit
// =============================================================================

bool UnitBrowser::QuickEditProperty(const std::string& unitId, const std::string& property,
                                     const std::string& value) {
    auto* database = m_contentBrowser->GetDatabase();
    auto metadata = database->GetAssetMetadata(unitId);
    if (!metadata) {
        return false;
    }

    // Read JSON
    std::ifstream file(metadata->path);
    if (!file.is_open()) {
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errors;
    if (!Json::parseFromStream(reader, file, &root, &errors)) {
        return false;
    }
    file.close();

    // Update property
    if (property == "health" || property == "maxHealth") {
        root["combat"]["health"] = std::stoi(value);
        root["combat"]["maxHealth"] = std::stoi(value);
    } else if (property == "attackDamage") {
        root["combat"]["attackDamage"] = std::stoi(value);
    } else if (property == "attackSpeed") {
        root["combat"]["attackSpeed"] = std::stof(value);
    } else if (property == "attackRange") {
        root["combat"]["attackRange"] = std::stof(value);
    } else if (property == "moveSpeed") {
        root["movement"]["speed"] = std::stof(value);
    } else if (property == "goldCost") {
        root["properties"]["goldCost"] = std::stoi(value);
    } else if (property == "name") {
        root["name"] = value;
    } else {
        return false;
    }

    // Write back
    std::ofstream outFile(metadata->path);
    if (!outFile.is_open()) {
        return false;
    }

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "    ";
    outFile << Json::writeString(writer, root);
    outFile.close();

    // Refresh
    m_needsRefresh = true;
    return true;
}

std::vector<std::string> UnitBrowser::GetEditableProperties() const {
    return {
        "name",
        "health",
        "armor",
        "attackDamage",
        "attackSpeed",
        "attackRange",
        "moveSpeed",
        "goldCost",
        "woodCost",
        "trainingTime"
    };
}

bool UnitBrowser::BatchEditProperty(const std::vector<std::string>& unitIds,
                                     const std::string& property, const std::string& value) {
    bool success = true;
    for (const auto& id : unitIds) {
        if (!QuickEditProperty(id, property, value)) {
            success = false;
        }
    }
    return success;
}

// =============================================================================
// Statistics
// =============================================================================

std::vector<std::string> UnitBrowser::GetFactions() const {
    std::vector<std::string> factions;
    for (const auto& unit : m_allUnits) {
        if (std::find(factions.begin(), factions.end(), unit.faction) == factions.end()) {
            factions.push_back(unit.faction);
        }
    }
    std::sort(factions.begin(), factions.end());
    return factions;
}

std::vector<int> UnitBrowser::GetTiers() const {
    std::vector<int> tiers;
    for (const auto& unit : m_allUnits) {
        if (std::find(tiers.begin(), tiers.end(), unit.tier) == tiers.end()) {
            tiers.push_back(unit.tier);
        }
    }
    std::sort(tiers.begin(), tiers.end());
    return tiers;
}

std::vector<std::string> UnitBrowser::GetRoles() const {
    std::vector<std::string> roles;
    for (const auto& unit : m_allUnits) {
        if (std::find(roles.begin(), roles.end(), unit.role) == roles.end()) {
            roles.push_back(unit.role);
        }
    }
    std::sort(roles.begin(), roles.end());
    return roles;
}

std::unordered_map<std::string, int> UnitBrowser::GetUnitCountByFaction() const {
    std::unordered_map<std::string, int> counts;
    for (const auto& unit : m_allUnits) {
        counts[unit.faction]++;
    }
    return counts;
}

std::unordered_map<std::string, UnitStats> UnitBrowser::GetAverageStatsByFaction() const {
    std::unordered_map<std::string, std::vector<UnitStats>> byFaction;

    for (const auto& unit : m_allUnits) {
        byFaction[unit.faction].push_back(unit);
    }

    std::unordered_map<std::string, UnitStats> averages;
    for (const auto& [faction, units] : byFaction) {
        if (units.empty()) continue;

        UnitStats avg;
        avg.faction = faction;
        avg.name = "Average";

        float n = static_cast<float>(units.size());
        for (const auto& u : units) {
            avg.health += u.health;
            avg.armor += u.armor;
            avg.attackDamage += u.attackDamage;
            avg.attackSpeed += u.attackSpeed;
            avg.attackRange += u.attackRange;
            avg.moveSpeed += u.moveSpeed;
            avg.goldCost += u.goldCost;
        }

        avg.health = static_cast<int>(avg.health / n);
        avg.armor = static_cast<int>(avg.armor / n);
        avg.attackDamage = static_cast<int>(avg.attackDamage / n);
        avg.attackSpeed /= n;
        avg.attackRange /= n;
        avg.moveSpeed /= n;
        avg.goldCost = static_cast<int>(avg.goldCost / n);

        averages[faction] = avg;
    }

    return averages;
}

// =============================================================================
// Balance Analysis
// =============================================================================

float UnitBrowser::CalculatePowerScore(const std::string& unitId) const {
    auto unitOpt = GetUnit(unitId);
    if (!unitOpt) {
        return 0.0f;
    }

    const auto& unit = *unitOpt;

    // Simple power formula considering various stats
    float dps = unit.attackDamage * unit.attackSpeed;
    float survivability = unit.health * (1.0f + unit.armor / 100.0f);
    float mobility = unit.moveSpeed * 0.5f;
    float range = unit.attackRange > 1.0f ? unit.attackRange * 0.3f : 0.0f;

    float powerScore = (dps * 2.0f) + (survivability * 0.1f) + mobility + range;

    return powerScore;
}

std::vector<std::string> UnitBrowser::GetBalanceWarnings() const {
    std::vector<std::string> warnings;

    // Calculate average power per cost
    float totalPowerPerCost = 0.0f;
    int count = 0;

    for (const auto& unit : m_allUnits) {
        if (unit.goldCost > 0) {
            float power = CalculatePowerScore(unit.id);
            totalPowerPerCost += power / unit.goldCost;
            count++;
        }
    }

    float avgPowerPerCost = count > 0 ? totalPowerPerCost / count : 0.0f;

    // Find outliers
    for (const auto& unit : m_allUnits) {
        if (unit.goldCost > 0) {
            float power = CalculatePowerScore(unit.id);
            float powerPerCost = power / unit.goldCost;

            // Flag units that are significantly over/under powered for their cost
            if (powerPerCost > avgPowerPerCost * 1.5f) {
                warnings.push_back(unit.name + " may be overpowered for its cost (" +
                                   std::to_string(static_cast<int>(powerPerCost / avgPowerPerCost * 100)) + "% of average)");
            } else if (powerPerCost < avgPowerPerCost * 0.5f) {
                warnings.push_back(unit.name + " may be underpowered for its cost (" +
                                   std::to_string(static_cast<int>(powerPerCost / avgPowerPerCost * 100)) + "% of average)");
            }
        }

        // Check for extreme stat values
        if (unit.health > 500 && unit.armor > 20) {
            warnings.push_back(unit.name + " has very high survivability (HP: " +
                               std::to_string(unit.health) + ", Armor: " + std::to_string(unit.armor) + ")");
        }

        if (unit.attackDamage > 50 && unit.attackSpeed > 1.5f) {
            warnings.push_back(unit.name + " has very high DPS potential");
        }
    }

    return warnings;
}

std::unordered_map<std::string, float> UnitBrowser::CompareToFactionAverage(const std::string& unitId) const {
    std::unordered_map<std::string, float> comparison;

    auto unitOpt = GetUnit(unitId);
    if (!unitOpt) {
        return comparison;
    }

    const auto& unit = *unitOpt;
    auto averages = GetAverageStatsByFaction();

    auto it = averages.find(unit.faction);
    if (it == averages.end()) {
        return comparison;
    }

    const auto& avg = it->second;

    // Calculate percentage difference from faction average
    if (avg.health != 0) {
        comparison["health"] = (static_cast<float>(unit.health) - avg.health) / avg.health * 100.0f;
    }
    if (avg.armor != 0) {
        comparison["armor"] = (static_cast<float>(unit.armor) - avg.armor) / avg.armor * 100.0f;
    }
    if (avg.attackDamage != 0) {
        comparison["attackDamage"] = (static_cast<float>(unit.attackDamage) - avg.attackDamage) / avg.attackDamage * 100.0f;
    }
    if (avg.attackSpeed != 0) {
        comparison["attackSpeed"] = (unit.attackSpeed - avg.attackSpeed) / avg.attackSpeed * 100.0f;
    }
    if (avg.attackRange != 0) {
        comparison["attackRange"] = (unit.attackRange - avg.attackRange) / avg.attackRange * 100.0f;
    }
    if (avg.moveSpeed != 0) {
        comparison["moveSpeed"] = (unit.moveSpeed - avg.moveSpeed) / avg.moveSpeed * 100.0f;
    }
    if (avg.goldCost != 0) {
        comparison["goldCost"] = (static_cast<float>(unit.goldCost) - avg.goldCost) / avg.goldCost * 100.0f;
    }

    return comparison;
}

// =============================================================================
// Private - Rendering
// =============================================================================

void UnitBrowser::RenderToolbar() {
    // Search
    char searchBuffer[256] = {0};
    strncpy(searchBuffer, m_filter.searchQuery.c_str(), sizeof(searchBuffer) - 1);

    ImGui::PushItemWidth(200);
    if (ImGui::InputText("Search##UnitSearch", searchBuffer, sizeof(searchBuffer))) {
        m_filter.searchQuery = searchBuffer;
        SetFilter(m_filter);
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();

    if (ImGui::Button("Refresh")) {
        RefreshUnits();
    }

    ImGui::SameLine();

    if (m_comparing) {
        if (ImGui::Button("Exit Compare Mode")) {
            ClearComparison();
        }
    } else {
        if (ImGui::Button("Compare Mode")) {
            m_comparisonQueue.clear();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Click on two units to compare them");
        }
    }

    ImGui::Separator();
}

void UnitBrowser::RenderFilters() {
    ImGui::Text("Filters");
    ImGui::Separator();

    // Faction filter
    if (ImGui::CollapsingHeader("Faction", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto factions = GetFactions();
        for (const auto& faction : factions) {
            bool selected = std::find(m_filter.factions.begin(), m_filter.factions.end(), faction)
                            != m_filter.factions.end();
            if (ImGui::Checkbox(faction.c_str(), &selected)) {
                if (selected) {
                    m_filter.factions.push_back(faction);
                } else {
                    m_filter.factions.erase(
                        std::remove(m_filter.factions.begin(), m_filter.factions.end(), faction),
                        m_filter.factions.end());
                }
                SetFilter(m_filter);
            }
        }
    }

    // Tier filter
    if (ImGui::CollapsingHeader("Tier", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto tiers = GetTiers();
        for (int tier : tiers) {
            bool selected = std::find(m_filter.tiers.begin(), m_filter.tiers.end(), tier)
                            != m_filter.tiers.end();
            std::string label = "Tier " + std::to_string(tier);
            if (ImGui::Checkbox(label.c_str(), &selected)) {
                if (selected) {
                    m_filter.tiers.push_back(tier);
                } else {
                    m_filter.tiers.erase(
                        std::remove(m_filter.tiers.begin(), m_filter.tiers.end(), tier),
                        m_filter.tiers.end());
                }
                SetFilter(m_filter);
            }
        }
    }

    // Role filter
    if (ImGui::CollapsingHeader("Role", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto roles = GetRoles();
        for (const auto& role : roles) {
            bool selected = std::find(m_filter.roles.begin(), m_filter.roles.end(), role)
                            != m_filter.roles.end();
            if (ImGui::Checkbox(role.c_str(), &selected)) {
                if (selected) {
                    m_filter.roles.push_back(role);
                } else {
                    m_filter.roles.erase(
                        std::remove(m_filter.roles.begin(), m_filter.roles.end(), role),
                        m_filter.roles.end());
                }
                SetFilter(m_filter);
            }
        }
    }

    // Stat ranges
    if (ImGui::CollapsingHeader("Stat Ranges")) {
        // Health range
        static int minHealth = 0, maxHealth = 1000;
        ImGui::Text("Health:");
        ImGui::PushItemWidth(80);
        if (ImGui::InputInt("Min##Health", &minHealth)) {
            m_filter.minHealth = minHealth > 0 ? std::make_optional(minHealth) : std::nullopt;
            SetFilter(m_filter);
        }
        ImGui::SameLine();
        if (ImGui::InputInt("Max##Health", &maxHealth)) {
            m_filter.maxHealth = maxHealth > 0 ? std::make_optional(maxHealth) : std::nullopt;
            SetFilter(m_filter);
        }
        ImGui::PopItemWidth();

        // Damage range
        static int minDamage = 0, maxDamage = 100;
        ImGui::Text("Damage:");
        ImGui::PushItemWidth(80);
        if (ImGui::InputInt("Min##Damage", &minDamage)) {
            m_filter.minDamage = minDamage > 0 ? std::make_optional(minDamage) : std::nullopt;
            SetFilter(m_filter);
        }
        ImGui::SameLine();
        if (ImGui::InputInt("Max##Damage", &maxDamage)) {
            m_filter.maxDamage = maxDamage > 0 ? std::make_optional(maxDamage) : std::nullopt;
            SetFilter(m_filter);
        }
        ImGui::PopItemWidth();
    }

    ImGui::Separator();

    // Statistics
    if (ImGui::CollapsingHeader("Statistics")) {
        ImGui::Text("Total Units: %zu", m_allUnits.size());
        ImGui::Text("Filtered: %zu", m_filteredUnits.size());

        auto counts = GetUnitCountByFaction();
        for (const auto& [faction, count] : counts) {
            ImGui::Text("  %s: %d", faction.c_str(), count);
        }
    }

    // Balance Warnings
    if (ImGui::CollapsingHeader("Balance Warnings")) {
        auto warnings = GetBalanceWarnings();
        if (warnings.empty()) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "No warnings");
        } else {
            for (const auto& warning : warnings) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "! %s", warning.c_str());
            }
        }
    }
}

void UnitBrowser::RenderUnitGrid() {
    const auto& units = m_filteredUnits.empty() && m_filter.searchQuery.empty() ? m_allUnits : m_filteredUnits;

    float windowWidth = ImGui::GetContentRegionAvail().x;
    float cardWidth = (windowWidth - (m_gridColumns - 1) * 10) / m_gridColumns;
    float cardHeight = 180.0f;

    int col = 0;
    for (const auto& unit : units) {
        ImGui::PushID(unit.id.c_str());

        RenderUnitCard(unit);

        ImGui::PopID();

        col++;
        if (col < m_gridColumns && col < static_cast<int>(units.size())) {
            ImGui::SameLine();
        } else {
            col = 0;
        }
    }

    if (units.empty()) {
        ImGui::TextDisabled("No units found");
    }
}

void UnitBrowser::RenderUnitCard(const UnitStats& unit) {
    bool selected = (unit.id == m_selectedUnitId);

    ImGui::BeginGroup();

    // Card background
    float cardWidth = (ImGui::GetContentRegionAvail().x - (m_gridColumns - 1) * 10) / m_gridColumns;

    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.4f, 0.6f, 0.5f));
    }

    ImGui::BeginChild(("Card_" + unit.id).c_str(), ImVec2(cardWidth, 180), true);

    // Unit name
    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "%s", unit.name.c_str());

    // Faction and tier
    ImGui::TextDisabled("%s | Tier %d | %s", unit.faction.c_str(), unit.tier, unit.role.c_str());

    ImGui::Separator();

    if (m_showStats) {
        // Health bar
        ImGui::Text("HP:");
        ImGui::SameLine();
        RenderStatBar("Health", static_cast<float>(unit.health), 500.0f, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        ImGui::SameLine();
        ImGui::Text("%d", unit.health);

        // Attack
        ImGui::Text("ATK:");
        ImGui::SameLine();
        RenderStatBar("Attack", static_cast<float>(unit.attackDamage), 50.0f, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::SameLine();
        ImGui::Text("%d", unit.attackDamage);

        // Speed
        ImGui::Text("SPD:");
        ImGui::SameLine();
        RenderStatBar("Speed", unit.moveSpeed, 10.0f, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
        ImGui::SameLine();
        ImGui::Text("%.1f", unit.moveSpeed);

        // Range
        ImGui::Text("RNG:");
        ImGui::SameLine();
        RenderStatBar("Range", unit.attackRange, 20.0f, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
        ImGui::SameLine();
        ImGui::Text("%.1f", unit.attackRange);
    }

    if (m_showCosts) {
        ImGui::Separator();
        ImGui::Text("Cost: %d gold, %d wood", unit.goldCost, unit.woodCost);
        ImGui::Text("Train: %.1fs | Pop: %d", unit.trainingTime, unit.populationCost);
    }

    // Power score
    float power = CalculatePowerScore(unit.id);
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Power: %.0f", power);

    ImGui::EndChild();

    if (selected) {
        ImGui::PopStyleColor();
    }

    // Click handling
    if (ImGui::IsItemClicked()) {
        m_selectedUnitId = unit.id;

        // Add to comparison queue if in compare mode
        if (!m_comparing && !m_comparisonQueue.empty()) {
            AddToComparison(unit.id);
        }

        if (OnUnitSelected) {
            OnUnitSelected(unit.id);
        }
    }

    // Double-click
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        if (OnUnitDoubleClicked) {
            OnUnitDoubleClicked(unit.id);
        }
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Open in Editor")) {
            if (OnUnitDoubleClicked) {
                OnUnitDoubleClicked(unit.id);
            }
        }
        if (ImGui::MenuItem("Compare...")) {
            AddToComparison(unit.id);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate")) {
            // TODO: Duplicate
        }
        if (ImGui::MenuItem("Delete")) {
            // TODO: Delete
        }
        ImGui::EndPopup();
    }

    ImGui::EndGroup();
}

void UnitBrowser::RenderComparisonView() {
    auto comparison = Compare(m_compareUnit1, m_compareUnit2);

    ImGui::Columns(3, "ComparisonColumns");

    // Unit 1 header
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s", comparison.unit1.name.c_str());
    ImGui::TextDisabled("%s | Tier %d", comparison.unit1.faction.c_str(), comparison.unit1.tier);
    ImGui::NextColumn();

    // Stat names
    ImGui::Text("Stat");
    ImGui::NextColumn();

    // Unit 2 header
    ImGui::TextColored(ImVec4(0.2f, 0.6f, 0.8f, 1.0f), "%s", comparison.unit2.name.c_str());
    ImGui::TextDisabled("%s | Tier %d", comparison.unit2.faction.c_str(), comparison.unit2.tier);
    ImGui::NextColumn();

    ImGui::Separator();

    // Stat comparisons
    for (const auto& diff : comparison.differences) {
        // Unit 1 value
        ImGui::Text("%.1f", diff.value1);
        ImGui::NextColumn();

        // Stat name with difference
        ImVec4 diffColor;
        if (diff.difference > 0) {
            diffColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);  // Green - unit2 is higher
        } else if (diff.difference < 0) {
            diffColor = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);  // Red - unit2 is lower
        } else {
            diffColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);  // Gray - equal
        }

        ImGui::Text("%s", diff.name.c_str());
        if (diff.difference != 0) {
            ImGui::SameLine();
            ImGui::TextColored(diffColor, "(%+.1f%%)", diff.percentDiff);
        }
        ImGui::NextColumn();

        // Unit 2 value
        ImGui::Text("%.1f", diff.value2);
        ImGui::NextColumn();
    }

    ImGui::Columns(1);

    ImGui::Separator();

    // Power comparison
    float power1 = CalculatePowerScore(m_compareUnit1);
    float power2 = CalculatePowerScore(m_compareUnit2);

    ImGui::Text("Power Scores:");
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "  %s: %.0f", comparison.unit1.name.c_str(), power1);
    ImGui::TextColored(ImVec4(0.2f, 0.6f, 0.8f, 1.0f), "  %s: %.0f", comparison.unit2.name.c_str(), power2);

    if (power1 > power2) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "  %s is %.0f%% stronger",
                           comparison.unit1.name.c_str(), (power1 / power2 - 1) * 100);
    } else if (power2 > power1) {
        ImGui::TextColored(ImVec4(0.2f, 0.6f, 0.8f, 1.0f), "  %s is %.0f%% stronger",
                           comparison.unit2.name.c_str(), (power2 / power1 - 1) * 100);
    } else {
        ImGui::Text("  Units are equally powerful");
    }
}

void UnitBrowser::RenderStatBar(const std::string& label, float value, float maxValue, const glm::vec4& color) {
    float percent = std::clamp(value / maxValue, 0.0f, 1.0f);

    ImVec4 imColor(color.r, color.g, color.b, color.a);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, imColor);
    ImGui::ProgressBar(percent, ImVec2(60, 12), "");
    ImGui::PopStyleColor();
}

// =============================================================================
// Private - Data Loading
// =============================================================================

UnitStats UnitBrowser::LoadUnitStats(const std::string& assetId) const {
    UnitStats stats;
    stats.id = assetId;

    auto* database = m_contentBrowser->GetDatabase();
    auto metadata = database->GetAssetMetadata(assetId);
    if (!metadata) {
        return stats;
    }

    std::ifstream file(metadata->path);
    if (!file.is_open()) {
        return stats;
    }

    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errors;
    if (!Json::parseFromStream(reader, file, &root, &errors)) {
        return stats;
    }

    stats.name = root.get("name", "Unknown").asString();
    stats.faction = root.get("faction", "neutral").asString();
    stats.tier = root.get("tier", 1).asInt();
    stats.description = root.get("description", "").asString();

    // Try to determine role from tags or class
    stats.role = root.get("class", "infantry").asString();

    // Combat stats
    if (root.isMember("combat")) {
        const auto& combat = root["combat"];
        stats.health = combat.get("health", 100).asInt();
        stats.maxHealth = combat.get("maxHealth", stats.health).asInt();
        stats.armor = combat.get("armor", 0).asInt();
        stats.attackDamage = combat.get("attackDamage", 10).asInt();
        stats.attackSpeed = combat.get("attackSpeed", 1.0f).asFloat();
        stats.attackRange = combat.get("attackRange", 1.0f).asFloat();
    }

    // Movement
    if (root.isMember("movement")) {
        stats.moveSpeed = root["movement"].get("speed", 5.0f).asFloat();
    }

    // Properties/Costs
    if (root.isMember("properties")) {
        const auto& props = root["properties"];
        stats.goldCost = props.get("goldCost", 0).asInt();
        stats.woodCost = props.get("woodCost", 0).asInt();
        stats.foodCost = props.get("foodCost", 0).asInt();
        stats.trainingTime = props.get("trainingTime", 10.0f).asFloat();
        stats.populationCost = props.get("populationCost", 1).asInt();
    }

    // Tags
    if (root.isMember("tags")) {
        for (const auto& tag : root["tags"]) {
            stats.tags.push_back(tag.asString());
        }
    }

    return stats;
}

void UnitBrowser::CacheUnits() {
    m_allUnits.clear();

    auto* database = m_contentBrowser->GetDatabase();
    auto allAssets = database->GetAllAssets();

    for (const auto& asset : allAssets) {
        if (asset.type == AssetType::Unit) {
            UnitStats stats = LoadUnitStats(asset.id);
            m_allUnits.push_back(stats);
        }
    }

    // Initial filter
    SetFilter(m_filter);
}

bool UnitBrowser::MatchesFilter(const UnitStats& unit) const {
    // Search query
    if (!m_filter.searchQuery.empty()) {
        std::string query = m_filter.searchQuery;
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);

        std::string name = unit.name;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        if (name.find(query) == std::string::npos) {
            return false;
        }
    }

    // Faction filter
    if (!m_filter.factions.empty()) {
        if (std::find(m_filter.factions.begin(), m_filter.factions.end(), unit.faction)
            == m_filter.factions.end()) {
            return false;
        }
    }

    // Tier filter
    if (!m_filter.tiers.empty()) {
        if (std::find(m_filter.tiers.begin(), m_filter.tiers.end(), unit.tier)
            == m_filter.tiers.end()) {
            return false;
        }
    }

    // Role filter
    if (!m_filter.roles.empty()) {
        if (std::find(m_filter.roles.begin(), m_filter.roles.end(), unit.role)
            == m_filter.roles.end()) {
            return false;
        }
    }

    // Stat ranges
    if (m_filter.minHealth && unit.health < *m_filter.minHealth) return false;
    if (m_filter.maxHealth && unit.health > *m_filter.maxHealth) return false;
    if (m_filter.minDamage && unit.attackDamage < *m_filter.minDamage) return false;
    if (m_filter.maxDamage && unit.attackDamage > *m_filter.maxDamage) return false;
    if (m_filter.minSpeed && unit.moveSpeed < *m_filter.minSpeed) return false;
    if (m_filter.maxSpeed && unit.moveSpeed > *m_filter.maxSpeed) return false;
    if (m_filter.minCost && unit.goldCost < *m_filter.minCost) return false;
    if (m_filter.maxCost && unit.goldCost > *m_filter.maxCost) return false;

    return true;
}

std::string UnitBrowser::FormatStat(float value, const std::string& suffix) const {
    char buffer[64];
    if (std::floor(value) == value) {
        snprintf(buffer, sizeof(buffer), "%.0f%s", value, suffix.c_str());
    } else {
        snprintf(buffer, sizeof(buffer), "%.1f%s", value, suffix.c_str());
    }
    return buffer;
}

} // namespace Content
} // namespace Vehement
