#include "BuildingBrowser.hpp"
#include "../ContentBrowser.hpp"
#include "../ContentDatabase.hpp"
#include "../../Editor.hpp"
#include <Json/Json.h>
#include <imgui.h>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace Content {

// =============================================================================
// Constructor / Destructor
// =============================================================================

BuildingBrowser::BuildingBrowser(Editor* editor, ContentBrowser* contentBrowser)
    : m_editor(editor)
    , m_contentBrowser(contentBrowser)
{
}

BuildingBrowser::~BuildingBrowser() {
    Shutdown();
}

// =============================================================================
// Lifecycle
// =============================================================================

bool BuildingBrowser::Initialize() {
    if (m_initialized) {
        return true;
    }

    CacheBuildings();

    m_initialized = true;
    return true;
}

void BuildingBrowser::Shutdown() {
    m_allBuildings.clear();
    m_filteredBuildings.clear();
    m_initialized = false;
}

void BuildingBrowser::Render() {
    ImGui::Begin("Building Browser", nullptr, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Footprint", nullptr, &m_showFootprint);
            ImGui::MenuItem("Show Production", nullptr, &m_showProduction);
            ImGui::MenuItem("Show Costs", nullptr, &m_showCosts);
            ImGui::Separator();
            if (ImGui::BeginMenu("Grid Columns")) {
                if (ImGui::MenuItem("2", nullptr, m_gridColumns == 2)) m_gridColumns = 2;
                if (ImGui::MenuItem("3", nullptr, m_gridColumns == 3)) m_gridColumns = 3;
                if (ImGui::MenuItem("4", nullptr, m_gridColumns == 4)) m_gridColumns = 4;
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

    RenderToolbar();

    // Filters panel
    ImGui::BeginChild("BuildingFilterPanel", ImVec2(200, 0), true);
    RenderFilters();
    ImGui::EndChild();

    ImGui::SameLine();

    // Content area
    ImGui::BeginChild("BuildingContent", ImVec2(0, 0), true);
    RenderBuildingGrid();
    ImGui::EndChild();

    ImGui::End();
}

void BuildingBrowser::Update(float deltaTime) {
    if (m_needsRefresh) {
        CacheBuildings();
        m_needsRefresh = false;
    }
}

// =============================================================================
// Building Access
// =============================================================================

std::vector<BuildingStats> BuildingBrowser::GetAllBuildings() const {
    return m_allBuildings;
}

std::optional<BuildingStats> BuildingBrowser::GetBuilding(const std::string& id) const {
    for (const auto& building : m_allBuildings) {
        if (building.id == id) {
            return building;
        }
    }
    return std::nullopt;
}

std::vector<BuildingStats> BuildingBrowser::GetFilteredBuildings() const {
    return m_filteredBuildings;
}

void BuildingBrowser::RefreshBuildings() {
    m_needsRefresh = true;
}

// =============================================================================
// Filtering
// =============================================================================

void BuildingBrowser::SetFilter(const BuildingFilterOptions& filter) {
    m_filter = filter;

    m_filteredBuildings.clear();
    for (const auto& building : m_allBuildings) {
        if (MatchesFilter(building)) {
            m_filteredBuildings.push_back(building);
        }
    }
}

void BuildingBrowser::FilterByFaction(const std::string& faction) {
    m_filter.factions.clear();
    m_filter.factions.push_back(faction);
    SetFilter(m_filter);
}

void BuildingBrowser::FilterByCategory(BuildingCategory category) {
    m_filter.categories.clear();
    m_filter.categories.push_back(category);
    SetFilter(m_filter);
}

void BuildingBrowser::ClearFilters() {
    m_filter = BuildingFilterOptions();
    m_filteredBuildings = m_allBuildings;
}

// =============================================================================
// Production Preview
// =============================================================================

std::vector<std::string> BuildingBrowser::GetTrainableUnits(const std::string& buildingId) const {
    auto buildingOpt = GetBuilding(buildingId);
    if (!buildingOpt) {
        return {};
    }
    return buildingOpt->trainableUnits;
}

std::vector<std::string> BuildingBrowser::GetResearchableTechs(const std::string& buildingId) const {
    auto buildingOpt = GetBuilding(buildingId);
    if (!buildingOpt) {
        return {};
    }
    return buildingOpt->researchableTechs;
}

BuildingBrowser::TechTreePosition BuildingBrowser::GetTechTreePosition(const std::string& buildingId) const {
    TechTreePosition pos;

    auto buildingOpt = GetBuilding(buildingId);
    if (!buildingOpt) {
        return pos;
    }

    pos.age = buildingOpt->requiredAge;
    pos.prerequisites = buildingOpt->requiredBuildings;

    // Find what this building unlocks
    for (const auto& other : m_allBuildings) {
        for (const auto& req : other.requiredBuildings) {
            if (req == buildingId) {
                pos.unlocks.push_back(other.id);
                break;
            }
        }
    }

    // Determine tier based on prerequisites depth
    std::function<int(const std::string&, int)> calculateTier = [&](const std::string& id, int depth) -> int {
        if (depth > 10) return depth;  // Prevent infinite loops

        auto bldg = GetBuilding(id);
        if (!bldg || bldg->requiredBuildings.empty()) {
            return 1;
        }

        int maxTier = 0;
        for (const auto& prereq : bldg->requiredBuildings) {
            int tier = calculateTier(prereq, depth + 1);
            maxTier = std::max(maxTier, tier);
        }
        return maxTier + 1;
    };

    pos.tier = calculateTier(buildingId, 0);

    return pos;
}

// =============================================================================
// Statistics
// =============================================================================

std::vector<std::string> BuildingBrowser::GetFactions() const {
    std::vector<std::string> factions;
    for (const auto& building : m_allBuildings) {
        if (std::find(factions.begin(), factions.end(), building.faction) == factions.end()) {
            factions.push_back(building.faction);
        }
    }
    std::sort(factions.begin(), factions.end());
    return factions;
}

std::unordered_map<BuildingCategory, int> BuildingBrowser::GetBuildingCountByCategory() const {
    std::unordered_map<BuildingCategory, int> counts;
    for (const auto& building : m_allBuildings) {
        counts[building.category]++;
    }
    return counts;
}

float BuildingBrowser::GetAverageCost(BuildingCategory category) const {
    float total = 0.0f;
    int count = 0;

    for (const auto& building : m_allBuildings) {
        if (building.category == category || category == BuildingCategory::Special) {
            total += building.goldCost + building.woodCost + building.stoneCost;
            count++;
        }
    }

    return count > 0 ? total / count : 0.0f;
}

float BuildingBrowser::GetAverageFootprint(BuildingCategory category) const {
    float total = 0.0f;
    int count = 0;

    for (const auto& building : m_allBuildings) {
        if (building.category == category || category == BuildingCategory::Special) {
            total += building.footprintSize;
            count++;
        }
    }

    return count > 0 ? total / count : 0.0f;
}

// =============================================================================
// Balance Analysis
// =============================================================================

float BuildingBrowser::CalculateBuildingValue(const std::string& buildingId) const {
    auto buildingOpt = GetBuilding(buildingId);
    if (!buildingOpt) {
        return 0.0f;
    }

    const auto& building = *buildingOpt;
    float value = 0.0f;

    // Base value from stats
    value += building.health * 0.1f;
    value += building.armor * 5.0f;

    // Production value
    value += building.trainableUnits.size() * 50.0f;
    value += building.researchableTechs.size() * 75.0f;

    // Resource generation value
    value += building.goldPerSecond * 100.0f;
    value += building.woodPerSecond * 80.0f;
    value += building.foodPerSecond * 60.0f;

    // Population value
    value += building.populationProvided * 25.0f;

    return value;
}

float BuildingBrowser::CalculateROI(const std::string& buildingId) const {
    auto buildingOpt = GetBuilding(buildingId);
    if (!buildingOpt) {
        return 0.0f;
    }

    const auto& building = *buildingOpt;

    // Total resource generation per second
    float resourcePerSec = building.goldPerSecond + building.woodPerSecond * 0.8f + building.foodPerSecond * 0.6f;

    if (resourcePerSec <= 0) {
        return 0.0f;  // No income generating building
    }

    // Total cost
    float totalCost = building.goldCost + building.woodCost * 0.8f + building.stoneCost * 1.2f;

    // Time to pay back (in seconds)
    float paybackTime = totalCost / resourcePerSec;

    // Include build time
    paybackTime += building.buildTime;

    // Return efficiency (lower is better, but we invert for display)
    return paybackTime > 0 ? 1000.0f / paybackTime : 0.0f;
}

std::vector<std::string> BuildingBrowser::GetBalanceWarnings() const {
    std::vector<std::string> warnings;

    // Calculate average value per cost
    float totalValuePerCost = 0.0f;
    int count = 0;

    for (const auto& building : m_allBuildings) {
        float cost = building.goldCost + building.woodCost + building.stoneCost;
        if (cost > 0) {
            float value = CalculateBuildingValue(building.id);
            totalValuePerCost += value / cost;
            count++;
        }
    }

    float avgValuePerCost = count > 0 ? totalValuePerCost / count : 1.0f;

    for (const auto& building : m_allBuildings) {
        float cost = building.goldCost + building.woodCost + building.stoneCost;
        if (cost > 0) {
            float value = CalculateBuildingValue(building.id);
            float valuePerCost = value / cost;

            if (valuePerCost > avgValuePerCost * 1.5f) {
                warnings.push_back(building.name + " provides high value for its cost");
            } else if (valuePerCost < avgValuePerCost * 0.5f) {
                warnings.push_back(building.name + " may be overpriced for its value");
            }
        }

        // Check for overly long build times
        if (building.buildTime > 120.0f) {
            warnings.push_back(building.name + " has very long build time (" +
                               std::to_string(static_cast<int>(building.buildTime)) + "s)");
        }

        // Check for very large footprints
        if (building.footprintSize > 16) {
            warnings.push_back(building.name + " has large footprint (" +
                               std::to_string(building.width) + "x" + std::to_string(building.height) + ")");
        }

        // Check for unique buildings without prerequisites
        if (building.isUnique && building.requiredBuildings.empty() && building.requiredAge <= 1) {
            warnings.push_back(building.name + " is unique but has no prerequisites");
        }
    }

    return warnings;
}

// =============================================================================
// Private - Rendering
// =============================================================================

void BuildingBrowser::RenderToolbar() {
    // Search
    char searchBuffer[256] = {0};
    strncpy(searchBuffer, m_filter.searchQuery.c_str(), sizeof(searchBuffer) - 1);

    ImGui::PushItemWidth(200);
    if (ImGui::InputText("Search##BuildingSearch", searchBuffer, sizeof(searchBuffer))) {
        m_filter.searchQuery = searchBuffer;
        SetFilter(m_filter);
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();

    if (ImGui::Button("Refresh")) {
        RefreshBuildings();
    }

    ImGui::SameLine();

    // Quick filter buttons
    ImGui::Text("Quick:");
    ImGui::SameLine();
    if (ImGui::SmallButton("Military")) {
        FilterByCategory(BuildingCategory::Military);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Economic")) {
        FilterByCategory(BuildingCategory::Economic);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Defense")) {
        FilterByCategory(BuildingCategory::Defense);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("All")) {
        ClearFilters();
    }

    ImGui::Separator();
}

void BuildingBrowser::RenderFilters() {
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

    // Category filter
    if (ImGui::CollapsingHeader("Category", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* categoryNames[] = {
            "Military", "Economic", "Research", "Defense", "Production", "Support", "Special"
        };

        for (int i = 0; i < 7; ++i) {
            BuildingCategory cat = static_cast<BuildingCategory>(i);
            bool selected = std::find(m_filter.categories.begin(), m_filter.categories.end(), cat)
                            != m_filter.categories.end();

            ImVec4 color = GetCategoryColor(cat);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            if (ImGui::Checkbox(categoryNames[i], &selected)) {
                if (selected) {
                    m_filter.categories.push_back(cat);
                } else {
                    m_filter.categories.erase(
                        std::remove(m_filter.categories.begin(), m_filter.categories.end(), cat),
                        m_filter.categories.end());
                }
                SetFilter(m_filter);
            }
            ImGui::PopStyleColor();
        }
    }

    // Category toggles
    if (ImGui::CollapsingHeader("Type Toggles")) {
        if (ImGui::Checkbox("Military Buildings", &m_filter.showMilitaryBuildings)) {
            SetFilter(m_filter);
        }
        if (ImGui::Checkbox("Economic Buildings", &m_filter.showEconomicBuildings)) {
            SetFilter(m_filter);
        }
        if (ImGui::Checkbox("Research Buildings", &m_filter.showResearchBuildings)) {
            SetFilter(m_filter);
        }
        if (ImGui::Checkbox("Defense Buildings", &m_filter.showDefenseBuildings)) {
            SetFilter(m_filter);
        }

        ImGui::Separator();

        if (ImGui::Checkbox("Only with Production", &m_filter.onlyWithProduction)) {
            SetFilter(m_filter);
        }
        if (ImGui::Checkbox("Only with Research", &m_filter.onlyWithResearch)) {
            SetFilter(m_filter);
        }
        if (ImGui::Checkbox("Only Resource Generating", &m_filter.onlyResourceGenerating)) {
            SetFilter(m_filter);
        }
    }

    // Age filter
    if (ImGui::CollapsingHeader("Required Age")) {
        static int ageFilter = 0;
        if (ImGui::RadioButton("All Ages", &ageFilter, 0)) {
            m_filter.requiredAge = std::nullopt;
            SetFilter(m_filter);
        }
        if (ImGui::RadioButton("Age 1", &ageFilter, 1)) {
            m_filter.requiredAge = 1;
            SetFilter(m_filter);
        }
        if (ImGui::RadioButton("Age 2", &ageFilter, 2)) {
            m_filter.requiredAge = 2;
            SetFilter(m_filter);
        }
        if (ImGui::RadioButton("Age 3", &ageFilter, 3)) {
            m_filter.requiredAge = 3;
            SetFilter(m_filter);
        }
        if (ImGui::RadioButton("Age 4", &ageFilter, 4)) {
            m_filter.requiredAge = 4;
            SetFilter(m_filter);
        }
    }

    ImGui::Separator();

    // Statistics
    if (ImGui::CollapsingHeader("Statistics")) {
        ImGui::Text("Total Buildings: %zu", m_allBuildings.size());
        ImGui::Text("Filtered: %zu", m_filteredBuildings.size());

        auto counts = GetBuildingCountByCategory();
        for (const auto& [category, count] : counts) {
            ImVec4 color = GetCategoryColor(category);
            ImGui::TextColored(color, "  %s: %d", GetCategoryName(category).c_str(), count);
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

void BuildingBrowser::RenderBuildingGrid() {
    const auto& buildings = m_filteredBuildings.empty() && m_filter.searchQuery.empty()
                            ? m_allBuildings : m_filteredBuildings;

    int col = 0;
    for (const auto& building : buildings) {
        ImGui::PushID(building.id.c_str());
        RenderBuildingCard(building);
        ImGui::PopID();

        col++;
        if (col < m_gridColumns && col < static_cast<int>(buildings.size())) {
            ImGui::SameLine();
        } else {
            col = 0;
        }
    }

    if (buildings.empty()) {
        ImGui::TextDisabled("No buildings found");
    }
}

void BuildingBrowser::RenderBuildingCard(const BuildingStats& building) {
    bool selected = (building.id == m_selectedBuildingId);

    float cardWidth = (ImGui::GetContentRegionAvail().x - (m_gridColumns - 1) * 10) / m_gridColumns;

    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.4f, 0.3f, 0.2f, 0.5f));
    }

    ImGui::BeginChild(("BuildingCard_" + building.id).c_str(), ImVec2(cardWidth, 220), true);

    // Category indicator
    ImVec4 catColor = GetCategoryColor(building.category);
    ImGui::TextColored(catColor, "[%s]", GetCategoryName(building.category).c_str());

    ImGui::SameLine();

    // Unique indicator
    if (building.isUnique) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "*");
    }

    // Building name
    ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.6f, 1.0f), "%s", building.name.c_str());

    // Faction and age
    ImGui::TextDisabled("%s | Age %d", building.faction.c_str(), building.requiredAge);

    ImGui::Separator();

    if (m_showFootprint) {
        // Footprint preview
        ImGui::Text("Footprint: %dx%d", building.width, building.height);
        RenderFootprintPreview(building.width, building.height);
    }

    // Stats
    ImGui::Text("HP: %d | Armor: %d", building.health, building.armor);
    ImGui::Text("Build Time: %.0fs", building.buildTime);

    if (m_showCosts) {
        ImGui::Separator();
        // Costs
        std::string costStr;
        if (building.goldCost > 0) costStr += std::to_string(building.goldCost) + "g ";
        if (building.woodCost > 0) costStr += std::to_string(building.woodCost) + "w ";
        if (building.stoneCost > 0) costStr += std::to_string(building.stoneCost) + "s";
        ImGui::Text("Cost: %s", costStr.c_str());

        if (building.populationProvided > 0) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "+%d population", building.populationProvided);
        }

        // Resource generation
        if (building.goldPerSecond > 0 || building.woodPerSecond > 0 || building.foodPerSecond > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "Income: %.1fg/s %.1fw/s %.1ff/s",
                               building.goldPerSecond, building.woodPerSecond, building.foodPerSecond);
        }
    }

    if (m_showProduction) {
        RenderProductionPreview(building);
    }

    // Requirements
    if (!building.requiredBuildings.empty()) {
        RenderTechRequirements(building);
    }

    // Value score
    float value = CalculateBuildingValue(building.id);
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Value: %.0f", value);

    ImGui::EndChild();

    if (selected) {
        ImGui::PopStyleColor();
    }

    // Click handling
    if (ImGui::IsItemClicked()) {
        m_selectedBuildingId = building.id;
        if (OnBuildingSelected) {
            OnBuildingSelected(building.id);
        }
    }

    // Double-click
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        if (OnBuildingDoubleClicked) {
            OnBuildingDoubleClicked(building.id);
        }
    }

    // Tooltip
    if (ImGui::IsItemHovered()) {
        RenderBuildingTooltip(building);
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Open in Editor")) {
            if (OnBuildingDoubleClicked) {
                OnBuildingDoubleClicked(building.id);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate")) {
            // TODO
        }
        if (ImGui::MenuItem("Delete")) {
            // TODO
        }
        ImGui::EndPopup();
    }
}

void BuildingBrowser::RenderFootprintPreview(int width, int height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 startPos = ImGui::GetCursorScreenPos();

    float cellSize = 10.0f;
    float padding = 2.0f;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            ImVec2 cellStart(startPos.x + x * (cellSize + padding),
                             startPos.y + y * (cellSize + padding));
            ImVec2 cellEnd(cellStart.x + cellSize, cellStart.y + cellSize);

            drawList->AddRectFilled(cellStart, cellEnd, IM_COL32(100, 80, 60, 255));
            drawList->AddRect(cellStart, cellEnd, IM_COL32(150, 120, 90, 255));
        }
    }

    ImGui::Dummy(ImVec2(width * (cellSize + padding), height * (cellSize + padding)));
}

void BuildingBrowser::RenderProductionPreview(const BuildingStats& building) {
    if (!building.trainableUnits.empty()) {
        ImGui::Separator();
        ImGui::Text("Trains:");
        for (size_t i = 0; i < std::min(building.trainableUnits.size(), size_t(3)); ++i) {
            ImGui::BulletText("%s", building.trainableUnits[i].c_str());
        }
        if (building.trainableUnits.size() > 3) {
            ImGui::TextDisabled("  +%zu more", building.trainableUnits.size() - 3);
        }
    }

    if (!building.researchableTechs.empty()) {
        ImGui::Separator();
        ImGui::Text("Researches:");
        for (size_t i = 0; i < std::min(building.researchableTechs.size(), size_t(3)); ++i) {
            ImGui::BulletText("%s", building.researchableTechs[i].c_str());
        }
        if (building.researchableTechs.size() > 3) {
            ImGui::TextDisabled("  +%zu more", building.researchableTechs.size() - 3);
        }
    }
}

void BuildingBrowser::RenderTechRequirements(const BuildingStats& building) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Requires:");
    for (const auto& req : building.requiredBuildings) {
        ImGui::SameLine();
        ImGui::SmallButton(req.c_str());
    }
}

void BuildingBrowser::RenderBuildingTooltip(const BuildingStats& building) {
    ImGui::BeginTooltip();

    ImVec4 catColor = GetCategoryColor(building.category);
    ImGui::TextColored(catColor, "%s", building.name.c_str());
    ImGui::TextDisabled("%s building", GetCategoryName(building.category).c_str());

    ImGui::Separator();

    if (!building.description.empty()) {
        ImGui::TextWrapped("%s", building.description.c_str());
        ImGui::Separator();
    }

    ImGui::Text("Faction: %s", building.faction.c_str());
    ImGui::Text("Required Age: %d", building.requiredAge);
    ImGui::Text("Footprint: %dx%d (%d tiles)", building.width, building.height, building.footprintSize);

    ImGui::Separator();

    ImGui::Text("Health: %d / %d", building.health, building.maxHealth);
    ImGui::Text("Armor: %d", building.armor);
    ImGui::Text("Build Time: %.1f seconds", building.buildTime);

    ImGui::Separator();

    ImGui::Text("Costs:");
    ImGui::BulletText("Gold: %d", building.goldCost);
    ImGui::BulletText("Wood: %d", building.woodCost);
    ImGui::BulletText("Stone: %d", building.stoneCost);

    if (building.populationProvided > 0) {
        ImGui::Text("Provides: +%d population", building.populationProvided);
    }

    if (building.goldPerSecond > 0 || building.woodPerSecond > 0 || building.foodPerSecond > 0) {
        ImGui::Separator();
        ImGui::Text("Resource Generation:");
        if (building.goldPerSecond > 0) ImGui::BulletText("Gold: %.1f/s", building.goldPerSecond);
        if (building.woodPerSecond > 0) ImGui::BulletText("Wood: %.1f/s", building.woodPerSecond);
        if (building.foodPerSecond > 0) ImGui::BulletText("Food: %.1f/s", building.foodPerSecond);

        float roi = CalculateROI(building.id);
        ImGui::Text("ROI Score: %.1f", roi);
    }

    if (!building.trainableUnits.empty()) {
        ImGui::Separator();
        ImGui::Text("Trainable Units (%zu):", building.trainableUnits.size());
        for (const auto& unit : building.trainableUnits) {
            ImGui::BulletText("%s", unit.c_str());
        }
    }

    if (!building.researchableTechs.empty()) {
        ImGui::Separator();
        ImGui::Text("Researchable Techs (%zu):", building.researchableTechs.size());
        for (const auto& tech : building.researchableTechs) {
            ImGui::BulletText("%s", tech.c_str());
        }
    }

    if (!building.requiredBuildings.empty()) {
        ImGui::Separator();
        ImGui::Text("Required Buildings:");
        for (const auto& req : building.requiredBuildings) {
            ImGui::BulletText("%s", req.c_str());
        }
    }

    ImGui::EndTooltip();
}

// =============================================================================
// Private - Data Loading
// =============================================================================

BuildingStats BuildingBrowser::LoadBuildingStats(const std::string& assetId) const {
    BuildingStats stats;
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
    stats.description = root.get("description", "").asString();
    stats.iconPath = root.get("icon", "").asString();
    stats.isUnique = root.get("isUnique", false).asBool();
    stats.isWonderOrMonument = root.get("isWonder", false).asBool();

    // Category
    std::string categoryStr = root.get("category", "military").asString();
    if (categoryStr == "military") stats.category = BuildingCategory::Military;
    else if (categoryStr == "economic") stats.category = BuildingCategory::Economic;
    else if (categoryStr == "research") stats.category = BuildingCategory::Research;
    else if (categoryStr == "defense") stats.category = BuildingCategory::Defense;
    else if (categoryStr == "production") stats.category = BuildingCategory::Production;
    else if (categoryStr == "support") stats.category = BuildingCategory::Support;
    else stats.category = BuildingCategory::Special;

    // Footprint
    if (root.isMember("footprint")) {
        const auto& footprint = root["footprint"];
        stats.width = footprint.get("width", 1).asInt();
        stats.height = footprint.get("height", 1).asInt();
        stats.footprintSize = stats.width * stats.height;
    }

    // Stats
    if (root.isMember("stats")) {
        const auto& statsNode = root["stats"];
        stats.health = statsNode.get("health", 500).asInt();
        stats.maxHealth = statsNode.get("maxHealth", stats.health).asInt();
        stats.armor = statsNode.get("armor", 0).asInt();
    }

    // Construction
    if (root.isMember("construction")) {
        stats.buildTime = root["construction"].get("buildTime", 30.0f).asFloat();
    }

    // Costs
    if (root.isMember("costs")) {
        const auto& costs = root["costs"];
        stats.goldCost = costs.get("gold", 0).asInt();
        stats.woodCost = costs.get("wood", 0).asInt();
        stats.stoneCost = costs.get("stone", 0).asInt();
        stats.foodCost = costs.get("food", 0).asInt();
    }

    // Population
    stats.populationProvided = root.get("populationProvided", 0).asInt();
    stats.populationCost = root.get("populationCost", 0).asInt();

    // Production
    if (root.isMember("trains")) {
        for (const auto& unit : root["trains"]) {
            stats.trainableUnits.push_back(unit.asString());
        }
    }

    if (root.isMember("researches")) {
        for (const auto& tech : root["researches"]) {
            stats.researchableTechs.push_back(tech.asString());
        }
    }

    if (root.isMember("upgrades")) {
        for (const auto& upgrade : root["upgrades"]) {
            if (upgrade.isMember("id")) {
                stats.providedUpgrades.push_back(upgrade["id"].asString());
            }
        }
    }

    // Resource generation
    if (root.isMember("income")) {
        const auto& income = root["income"];
        stats.goldPerSecond = income.get("gold", 0.0f).asFloat();
        stats.woodPerSecond = income.get("wood", 0.0f).asFloat();
        stats.foodPerSecond = income.get("food", 0.0f).asFloat();
    }

    // Requirements
    if (root.isMember("requirements")) {
        const auto& reqs = root["requirements"];
        stats.requiredAge = reqs.get("age", 1).asInt();

        if (reqs.isMember("buildings")) {
            for (const auto& bldg : reqs["buildings"]) {
                stats.requiredBuildings.push_back(bldg.asString());
            }
        }

        if (reqs.isMember("techs")) {
            for (const auto& tech : reqs["techs"]) {
                stats.requiredTechs.push_back(tech.asString());
            }
        }
    }

    // Tags
    if (root.isMember("tags")) {
        for (const auto& tag : root["tags"]) {
            stats.tags.push_back(tag.asString());
        }
    }

    return stats;
}

void BuildingBrowser::CacheBuildings() {
    m_allBuildings.clear();

    auto* database = m_contentBrowser->GetDatabase();
    auto allAssets = database->GetAllAssets();

    for (const auto& asset : allAssets) {
        if (asset.type == AssetType::Building) {
            BuildingStats stats = LoadBuildingStats(asset.id);
            m_allBuildings.push_back(stats);
        }
    }

    SetFilter(m_filter);
}

bool BuildingBrowser::MatchesFilter(const BuildingStats& building) const {
    // Search query
    if (!m_filter.searchQuery.empty()) {
        std::string query = m_filter.searchQuery;
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);

        std::string name = building.name;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        if (name.find(query) == std::string::npos) {
            return false;
        }
    }

    // Faction filter
    if (!m_filter.factions.empty()) {
        if (std::find(m_filter.factions.begin(), m_filter.factions.end(), building.faction)
            == m_filter.factions.end()) {
            return false;
        }
    }

    // Category filter
    if (!m_filter.categories.empty()) {
        if (std::find(m_filter.categories.begin(), m_filter.categories.end(), building.category)
            == m_filter.categories.end()) {
            return false;
        }
    }

    // Category toggles
    switch (building.category) {
        case BuildingCategory::Military:
            if (!m_filter.showMilitaryBuildings) return false;
            break;
        case BuildingCategory::Economic:
            if (!m_filter.showEconomicBuildings) return false;
            break;
        case BuildingCategory::Research:
            if (!m_filter.showResearchBuildings) return false;
            break;
        case BuildingCategory::Defense:
            if (!m_filter.showDefenseBuildings) return false;
            break;
        default:
            break;
    }

    // Special filters
    if (m_filter.onlyWithProduction && building.trainableUnits.empty()) return false;
    if (m_filter.onlyWithResearch && building.researchableTechs.empty()) return false;
    if (m_filter.onlyResourceGenerating &&
        (building.goldPerSecond <= 0 && building.woodPerSecond <= 0 && building.foodPerSecond <= 0)) {
        return false;
    }

    // Stat ranges
    if (m_filter.requiredAge && building.requiredAge != *m_filter.requiredAge) return false;
    if (m_filter.minFootprint && building.footprintSize < *m_filter.minFootprint) return false;
    if (m_filter.maxFootprint && building.footprintSize > *m_filter.maxFootprint) return false;

    int totalCost = building.goldCost + building.woodCost + building.stoneCost;
    if (m_filter.minCost && totalCost < *m_filter.minCost) return false;
    if (m_filter.maxCost && totalCost > *m_filter.maxCost) return false;

    return true;
}

// =============================================================================
// Private - Helpers
// =============================================================================

std::string BuildingBrowser::GetCategoryName(BuildingCategory category) const {
    switch (category) {
        case BuildingCategory::Military: return "Military";
        case BuildingCategory::Economic: return "Economic";
        case BuildingCategory::Research: return "Research";
        case BuildingCategory::Defense: return "Defense";
        case BuildingCategory::Production: return "Production";
        case BuildingCategory::Support: return "Support";
        case BuildingCategory::Special: return "Special";
        default: return "Unknown";
    }
}

glm::vec4 BuildingBrowser::GetCategoryColor(BuildingCategory category) const {
    switch (category) {
        case BuildingCategory::Military: return glm::vec4(0.8f, 0.3f, 0.3f, 1.0f);
        case BuildingCategory::Economic: return glm::vec4(0.9f, 0.8f, 0.2f, 1.0f);
        case BuildingCategory::Research: return glm::vec4(0.3f, 0.5f, 0.9f, 1.0f);
        case BuildingCategory::Defense: return glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
        case BuildingCategory::Production: return glm::vec4(0.6f, 0.4f, 0.2f, 1.0f);
        case BuildingCategory::Support: return glm::vec4(0.3f, 0.8f, 0.5f, 1.0f);
        case BuildingCategory::Special: return glm::vec4(0.8f, 0.5f, 0.9f, 1.0f);
        default: return glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
    }
}

} // namespace Content
} // namespace Vehement
