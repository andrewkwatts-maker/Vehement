#pragma once

/**
 * @file RaceDesigner.hpp
 * @brief Race creation UI for the editor
 */

#include "../../rts/race/RaceDefinition.hpp"
#include "../../rts/race/PointAllocation.hpp"
#include <string>
#include <vector>
#include <functional>

namespace Vehement {
namespace Editor {

using namespace RTS::Race;

// ============================================================================
// Designer State
// ============================================================================

enum class DesignerTab : uint8_t {
    Overview = 0,
    PointAllocation,
    UnitArchetypes,
    BuildingArchetypes,
    HeroArchetypes,
    SpellArchetypes,
    Bonuses,
    TalentTree,
    Preview,

    COUNT
};

struct DesignerState {
    DesignerTab currentTab = DesignerTab::Overview;
    bool isDirty = false;
    std::string lastError;
    std::vector<std::string> validationWarnings;
};

// ============================================================================
// Race Designer
// ============================================================================

class RaceDesigner {
public:
    using SaveCallback = std::function<void(const RaceDefinition&)>;
    using LoadCallback = std::function<void(RaceDefinition&)>;

    RaceDesigner();
    ~RaceDesigner();

    // =========================================================================
    // Initialization
    // =========================================================================

    bool Initialize();
    void Shutdown();

    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Race Management
    // =========================================================================

    void NewRace();
    void NewRaceFromTemplate(const std::string& templateName);
    bool LoadRace(const std::string& filepath);
    bool SaveRace(const std::string& filepath);

    RaceDefinition& GetRace() { return m_currentRace; }
    [[nodiscard]] const RaceDefinition& GetRace() const { return m_currentRace; }

    // =========================================================================
    // Point Allocation
    // =========================================================================

    void SetMilitaryPoints(int points);
    void SetEconomyPoints(int points);
    void SetMagicPoints(int points);
    void SetTechnologyPoints(int points);

    void SetMilitarySubAllocation(int infantry, int ranged, int cavalry, int siege);
    void SetEconomySubAllocation(int harvest, int build, int carry, int trade);
    void SetMagicSubAllocation(int damage, int range, int mana, int cooldown);
    void SetTechSubAllocation(int research, int ageUp, int unique);

    void AutoBalance();
    void ApplyPreset(const std::string& presetName);

    // =========================================================================
    // Archetype Selection
    // =========================================================================

    void AddUnitArchetype(const std::string& archetypeId);
    void RemoveUnitArchetype(const std::string& archetypeId);
    void AddBuildingArchetype(const std::string& archetypeId);
    void RemoveBuildingArchetype(const std::string& archetypeId);
    void AddHeroArchetype(const std::string& archetypeId);
    void RemoveHeroArchetype(const std::string& archetypeId);
    void AddSpellArchetype(const std::string& archetypeId);
    void RemoveSpellArchetype(const std::string& archetypeId);

    [[nodiscard]] std::vector<std::string> GetAvailableUnitArchetypes() const;
    [[nodiscard]] std::vector<std::string> GetAvailableBuildingArchetypes() const;
    [[nodiscard]] std::vector<std::string> GetAvailableHeroArchetypes() const;
    [[nodiscard]] std::vector<std::string> GetAvailableSpellArchetypes() const;

    // =========================================================================
    // Bonuses
    // =========================================================================

    void AddBonus(const std::string& bonusId);
    void RemoveBonus(const std::string& bonusId);
    [[nodiscard]] std::vector<std::string> GetAvailableBonuses() const;
    [[nodiscard]] int GetBonusPointCost() const;

    // =========================================================================
    // Validation
    // =========================================================================

    [[nodiscard]] bool Validate();
    [[nodiscard]] std::vector<std::string> GetValidationErrors() const;
    [[nodiscard]] BalanceScore GetBalanceScore() const;
    [[nodiscard]] float GetPowerLevel() const;

    // =========================================================================
    // UI State
    // =========================================================================

    DesignerState& GetState() { return m_state; }
    void SetCurrentTab(DesignerTab tab) { m_state.currentTab = tab; }
    [[nodiscard]] DesignerTab GetCurrentTab() const { return m_state.currentTab; }

    // =========================================================================
    // Rendering (for ImGui integration)
    // =========================================================================

    void Render();
    void RenderOverviewTab();
    void RenderPointAllocationTab();
    void RenderUnitArchetypesTab();
    void RenderBuildingArchetypesTab();
    void RenderHeroArchetypesTab();
    void RenderSpellArchetypesTab();
    void RenderBonusesTab();
    void RenderTalentTreeTab();
    void RenderPreviewTab();
    void RenderBalanceWarnings();

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnSave(SaveCallback callback) { m_onSave = std::move(callback); }
    void SetOnLoad(LoadCallback callback) { m_onLoad = std::move(callback); }

private:
    bool m_initialized = false;
    RaceDefinition m_currentRace;
    DesignerState m_state;

    SaveCallback m_onSave;
    LoadCallback m_onLoad;

    void MarkDirty();
    void RecalculateBonuses();
};

// ============================================================================
// HTML Bridge
// ============================================================================

/**
 * @brief Bridge for HTML-based race designer
 */
class RaceDesignerHTMLBridge {
public:
    [[nodiscard]] static RaceDesignerHTMLBridge& Instance();

    void Initialize(RaceDesigner* designer);

    // JavaScript callbacks
    [[nodiscard]] std::string GetRaceJson() const;
    void SetRaceJson(const std::string& json);
    [[nodiscard]] std::string GetAvailableArchetypesJson() const;
    [[nodiscard]] std::string GetBalanceScoreJson() const;
    [[nodiscard]] std::string GetValidationErrorsJson() const;

    void SetPointAllocation(int military, int economy, int magic, int tech);
    void AddArchetype(const std::string& type, const std::string& id);
    void RemoveArchetype(const std::string& type, const std::string& id);

private:
    RaceDesignerHTMLBridge() = default;
    RaceDesigner* m_designer = nullptr;
};

} // namespace Editor
} // namespace Vehement
