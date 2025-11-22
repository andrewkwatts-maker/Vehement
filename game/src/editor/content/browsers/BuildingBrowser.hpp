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
 * @brief Building category
 */
enum class BuildingCategory {
    Military,
    Economic,
    Research,
    Defense,
    Production,
    Support,
    Special
};

/**
 * @brief Building stats for preview
 */
struct BuildingStats {
    std::string id;
    std::string name;
    std::string faction;
    BuildingCategory category = BuildingCategory::Military;

    // Footprint
    int width = 1;
    int height = 1;
    int footprintSize = 1;  // width * height

    // Stats
    int health = 500;
    int maxHealth = 500;
    int armor = 10;
    float buildTime = 30.0f;

    // Costs
    int goldCost = 100;
    int woodCost = 50;
    int stoneCost = 0;
    int foodCost = 0;
    int populationProvided = 0;
    int populationCost = 0;

    // Production
    std::vector<std::string> trainableUnits;
    std::vector<std::string> researchableTechs;
    std::vector<std::string> providedUpgrades;

    // Resource generation
    float goldPerSecond = 0.0f;
    float woodPerSecond = 0.0f;
    float foodPerSecond = 0.0f;

    // Requirements
    std::vector<std::string> requiredBuildings;
    std::vector<std::string> requiredTechs;
    int requiredAge = 1;

    // Classification
    std::vector<std::string> tags;
    std::string description;
    std::string iconPath;
    bool isUnique = false;
    bool isWonderOrMonument = false;
};

/**
 * @brief Building filter options
 */
struct BuildingFilterOptions {
    std::string searchQuery;
    std::vector<std::string> factions;
    std::vector<BuildingCategory> categories;

    bool showMilitaryBuildings = true;
    bool showEconomicBuildings = true;
    bool showResearchBuildings = true;
    bool showDefenseBuildings = true;

    std::optional<int> minFootprint;
    std::optional<int> maxFootprint;
    std::optional<int> minCost;
    std::optional<int> maxCost;
    std::optional<int> requiredAge;

    bool onlyWithProduction = false;
    bool onlyWithResearch = false;
    bool onlyResourceGenerating = false;
};

/**
 * @brief Building Browser
 *
 * Specialized browser for building assets:
 * - Footprint size indicators
 * - Production capabilities
 * - Tech requirements
 * - Resource costs
 * - Tech tree visualization
 */
class BuildingBrowser {
public:
    explicit BuildingBrowser(Editor* editor, ContentBrowser* contentBrowser);
    ~BuildingBrowser();

    // Non-copyable
    BuildingBrowser(const BuildingBrowser&) = delete;
    BuildingBrowser& operator=(const BuildingBrowser&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    bool Initialize();
    void Shutdown();
    void Render();
    void Update(float deltaTime);

    // =========================================================================
    // Building Access
    // =========================================================================

    [[nodiscard]] std::vector<BuildingStats> GetAllBuildings() const;
    [[nodiscard]] std::optional<BuildingStats> GetBuilding(const std::string& id) const;
    [[nodiscard]] std::vector<BuildingStats> GetFilteredBuildings() const;
    void RefreshBuildings();

    // =========================================================================
    // Filtering
    // =========================================================================

    void SetFilter(const BuildingFilterOptions& filter);
    [[nodiscard]] const BuildingFilterOptions& GetFilter() const { return m_filter; }
    void FilterByFaction(const std::string& faction);
    void FilterByCategory(BuildingCategory category);
    void ClearFilters();

    // =========================================================================
    // Production Preview
    // =========================================================================

    /**
     * @brief Get units trainable at building
     */
    [[nodiscard]] std::vector<std::string> GetTrainableUnits(const std::string& buildingId) const;

    /**
     * @brief Get techs researchable at building
     */
    [[nodiscard]] std::vector<std::string> GetResearchableTechs(const std::string& buildingId) const;

    /**
     * @brief Get building's position in tech tree
     */
    struct TechTreePosition {
        int age = 1;
        int tier = 1;
        std::vector<std::string> prerequisites;
        std::vector<std::string> unlocks;
    };
    [[nodiscard]] TechTreePosition GetTechTreePosition(const std::string& buildingId) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] std::vector<std::string> GetFactions() const;
    [[nodiscard]] std::unordered_map<BuildingCategory, int> GetBuildingCountByCategory() const;
    [[nodiscard]] float GetAverageCost(BuildingCategory category) const;
    [[nodiscard]] float GetAverageFootprint(BuildingCategory category) const;

    // =========================================================================
    // Balance Analysis
    // =========================================================================

    /**
     * @brief Calculate building value score
     */
    [[nodiscard]] float CalculateBuildingValue(const std::string& buildingId) const;

    /**
     * @brief Calculate resource generation efficiency
     */
    [[nodiscard]] float CalculateROI(const std::string& buildingId) const;

    /**
     * @brief Get balance warnings
     */
    [[nodiscard]] std::vector<std::string> GetBalanceWarnings() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnBuildingSelected;
    std::function<void(const std::string&)> OnBuildingDoubleClicked;

private:
    // Rendering
    void RenderToolbar();
    void RenderFilters();
    void RenderBuildingGrid();
    void RenderBuildingCard(const BuildingStats& building);
    void RenderFootprintPreview(int width, int height);
    void RenderProductionPreview(const BuildingStats& building);
    void RenderTechRequirements(const BuildingStats& building);
    void RenderBuildingTooltip(const BuildingStats& building);

    // Data loading
    BuildingStats LoadBuildingStats(const std::string& assetId) const;
    void CacheBuildings();
    bool MatchesFilter(const BuildingStats& building) const;

    // Helpers
    std::string GetCategoryName(BuildingCategory category) const;
    glm::vec4 GetCategoryColor(BuildingCategory category) const;

    Editor* m_editor = nullptr;
    ContentBrowser* m_contentBrowser = nullptr;
    bool m_initialized = false;

    // Cached buildings
    std::vector<BuildingStats> m_allBuildings;
    std::vector<BuildingStats> m_filteredBuildings;
    bool m_needsRefresh = true;

    // Filter state
    BuildingFilterOptions m_filter;

    // Selection
    std::string m_selectedBuildingId;

    // View options
    int m_gridColumns = 3;
    bool m_showFootprint = true;
    bool m_showProduction = true;
    bool m_showCosts = true;
};

} // namespace Content
} // namespace Vehement
