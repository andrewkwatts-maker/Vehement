#include "RaceDesigner.hpp"
#include "../../rts/race/UnitArchetype.hpp"
#include "../../rts/race/BuildingArchetype.hpp"
#include "../../rts/race/HeroArchetype.hpp"
#include "../../rts/race/SpellArchetype.hpp"
#include "../../rts/race/RacialBonus.hpp"
#include <algorithm>

namespace Vehement {
namespace Editor {

RaceDesigner::RaceDesigner() = default;
RaceDesigner::~RaceDesigner() = default;

bool RaceDesigner::Initialize() {
    if (m_initialized) return true;

    // Initialize registries
    UnitArchetypeRegistry::Instance().Initialize();
    BuildingArchetypeRegistry::Instance().Initialize();
    HeroArchetypeRegistry::Instance().Initialize();
    SpellArchetypeRegistry::Instance().Initialize();
    RacialBonusRegistry::Instance().Initialize();
    RaceRegistry::Instance().Initialize();

    NewRace();
    m_initialized = true;
    return true;
}

void RaceDesigner::Shutdown() {
    m_initialized = false;
}

void RaceDesigner::NewRace() {
    m_currentRace = CreateBlankRace();
    m_state = DesignerState();
    RecalculateBonuses();
}

void RaceDesigner::NewRaceFromTemplate(const std::string& templateName) {
    m_currentRace = RaceRegistry::Instance().CreateFromTemplate(templateName);
    m_state = DesignerState();
    RecalculateBonuses();
}

bool RaceDesigner::LoadRace(const std::string& filepath) {
    if (!m_currentRace.LoadFromFile(filepath)) {
        m_state.lastError = "Failed to load race from " + filepath;
        return false;
    }
    m_state = DesignerState();
    RecalculateBonuses();

    if (m_onLoad) {
        m_onLoad(m_currentRace);
    }
    return true;
}

bool RaceDesigner::SaveRace(const std::string& filepath) {
    if (!Validate()) {
        m_state.lastError = "Cannot save - validation failed";
        return false;
    }

    if (!m_currentRace.SaveToFile(filepath)) {
        m_state.lastError = "Failed to save race to " + filepath;
        return false;
    }

    m_state.isDirty = false;

    if (m_onSave) {
        m_onSave(m_currentRace);
    }
    return true;
}

void RaceDesigner::SetMilitaryPoints(int points) {
    m_currentRace.allocation.SetCategoryPoints(PointCategory::Military, points);
    MarkDirty();
}

void RaceDesigner::SetEconomyPoints(int points) {
    m_currentRace.allocation.SetCategoryPoints(PointCategory::Economy, points);
    MarkDirty();
}

void RaceDesigner::SetMagicPoints(int points) {
    m_currentRace.allocation.SetCategoryPoints(PointCategory::Magic, points);
    MarkDirty();
}

void RaceDesigner::SetTechnologyPoints(int points) {
    m_currentRace.allocation.SetCategoryPoints(PointCategory::Technology, points);
    MarkDirty();
}

void RaceDesigner::SetMilitarySubAllocation(int infantry, int ranged, int cavalry, int siege) {
    m_currentRace.allocation.militaryAlloc.infantry = infantry;
    m_currentRace.allocation.militaryAlloc.ranged = ranged;
    m_currentRace.allocation.militaryAlloc.cavalry = cavalry;
    m_currentRace.allocation.militaryAlloc.siege = siege;
    m_currentRace.allocation.militaryAlloc.ComputeBonuses();
    MarkDirty();
}

void RaceDesigner::SetEconomySubAllocation(int harvest, int build, int carry, int trade) {
    m_currentRace.allocation.economyAlloc.harvestSpeed = harvest;
    m_currentRace.allocation.economyAlloc.buildSpeed = build;
    m_currentRace.allocation.economyAlloc.carryCapacity = carry;
    m_currentRace.allocation.economyAlloc.tradeProfits = trade;
    m_currentRace.allocation.economyAlloc.ComputeBonuses();
    MarkDirty();
}

void RaceDesigner::SetMagicSubAllocation(int damage, int range, int mana, int cooldown) {
    m_currentRace.allocation.magicAlloc.spellDamage = damage;
    m_currentRace.allocation.magicAlloc.spellRange = range;
    m_currentRace.allocation.magicAlloc.manaCost = mana;
    m_currentRace.allocation.magicAlloc.cooldownReduction = cooldown;
    m_currentRace.allocation.magicAlloc.ComputeBonuses();
    MarkDirty();
}

void RaceDesigner::SetTechSubAllocation(int research, int ageUp, int unique) {
    m_currentRace.allocation.techAlloc.researchSpeed = research;
    m_currentRace.allocation.techAlloc.ageUpCost = ageUp;
    m_currentRace.allocation.techAlloc.uniqueTechs = unique;
    m_currentRace.allocation.techAlloc.ComputeBonuses();
    MarkDirty();
}

void RaceDesigner::AutoBalance() {
    m_currentRace.allocation.AutoBalance();
    RecalculateBonuses();
    MarkDirty();
}

void RaceDesigner::ApplyPreset(const std::string& presetName) {
    m_currentRace.allocation.ApplyPreset(presetName);
    RecalculateBonuses();
    MarkDirty();
}

void RaceDesigner::AddUnitArchetype(const std::string& archetypeId) {
    if (std::find(m_currentRace.unitArchetypes.begin(), m_currentRace.unitArchetypes.end(), archetypeId)
        == m_currentRace.unitArchetypes.end()) {
        m_currentRace.unitArchetypes.push_back(archetypeId);
        MarkDirty();
    }
}

void RaceDesigner::RemoveUnitArchetype(const std::string& archetypeId) {
    auto it = std::find(m_currentRace.unitArchetypes.begin(), m_currentRace.unitArchetypes.end(), archetypeId);
    if (it != m_currentRace.unitArchetypes.end()) {
        m_currentRace.unitArchetypes.erase(it);
        MarkDirty();
    }
}

void RaceDesigner::AddBuildingArchetype(const std::string& archetypeId) {
    if (std::find(m_currentRace.buildingArchetypes.begin(), m_currentRace.buildingArchetypes.end(), archetypeId)
        == m_currentRace.buildingArchetypes.end()) {
        m_currentRace.buildingArchetypes.push_back(archetypeId);
        MarkDirty();
    }
}

void RaceDesigner::RemoveBuildingArchetype(const std::string& archetypeId) {
    auto it = std::find(m_currentRace.buildingArchetypes.begin(), m_currentRace.buildingArchetypes.end(), archetypeId);
    if (it != m_currentRace.buildingArchetypes.end()) {
        m_currentRace.buildingArchetypes.erase(it);
        MarkDirty();
    }
}

void RaceDesigner::AddHeroArchetype(const std::string& archetypeId) {
    if (std::find(m_currentRace.heroArchetypes.begin(), m_currentRace.heroArchetypes.end(), archetypeId)
        == m_currentRace.heroArchetypes.end()) {
        m_currentRace.heroArchetypes.push_back(archetypeId);
        MarkDirty();
    }
}

void RaceDesigner::RemoveHeroArchetype(const std::string& archetypeId) {
    auto it = std::find(m_currentRace.heroArchetypes.begin(), m_currentRace.heroArchetypes.end(), archetypeId);
    if (it != m_currentRace.heroArchetypes.end()) {
        m_currentRace.heroArchetypes.erase(it);
        MarkDirty();
    }
}

void RaceDesigner::AddSpellArchetype(const std::string& archetypeId) {
    if (std::find(m_currentRace.spellArchetypes.begin(), m_currentRace.spellArchetypes.end(), archetypeId)
        == m_currentRace.spellArchetypes.end()) {
        m_currentRace.spellArchetypes.push_back(archetypeId);
        MarkDirty();
    }
}

void RaceDesigner::RemoveSpellArchetype(const std::string& archetypeId) {
    auto it = std::find(m_currentRace.spellArchetypes.begin(), m_currentRace.spellArchetypes.end(), archetypeId);
    if (it != m_currentRace.spellArchetypes.end()) {
        m_currentRace.spellArchetypes.erase(it);
        MarkDirty();
    }
}

std::vector<std::string> RaceDesigner::GetAvailableUnitArchetypes() const {
    std::vector<std::string> result;
    for (const auto* arch : UnitArchetypeRegistry::Instance().GetAllArchetypes()) {
        result.push_back(arch->id);
    }
    return result;
}

std::vector<std::string> RaceDesigner::GetAvailableBuildingArchetypes() const {
    std::vector<std::string> result;
    for (const auto* arch : BuildingArchetypeRegistry::Instance().GetAllArchetypes()) {
        result.push_back(arch->id);
    }
    return result;
}

std::vector<std::string> RaceDesigner::GetAvailableHeroArchetypes() const {
    std::vector<std::string> result;
    for (const auto* arch : HeroArchetypeRegistry::Instance().GetAllArchetypes()) {
        result.push_back(arch->id);
    }
    return result;
}

std::vector<std::string> RaceDesigner::GetAvailableSpellArchetypes() const {
    std::vector<std::string> result;
    for (const auto* arch : SpellArchetypeRegistry::Instance().GetAllArchetypes()) {
        result.push_back(arch->id);
    }
    return result;
}

void RaceDesigner::AddBonus(const std::string& bonusId) {
    if (std::find(m_currentRace.bonusIds.begin(), m_currentRace.bonusIds.end(), bonusId)
        == m_currentRace.bonusIds.end()) {
        m_currentRace.bonusIds.push_back(bonusId);
        MarkDirty();
    }
}

void RaceDesigner::RemoveBonus(const std::string& bonusId) {
    auto it = std::find(m_currentRace.bonusIds.begin(), m_currentRace.bonusIds.end(), bonusId);
    if (it != m_currentRace.bonusIds.end()) {
        m_currentRace.bonusIds.erase(it);
        MarkDirty();
    }
}

std::vector<std::string> RaceDesigner::GetAvailableBonuses() const {
    std::vector<std::string> result;
    for (const auto* bonus : RacialBonusRegistry::Instance().GetAllBonuses()) {
        result.push_back(bonus->id);
    }
    return result;
}

int RaceDesigner::GetBonusPointCost() const {
    int total = 0;
    for (const auto& bonusId : m_currentRace.bonusIds) {
        const auto* bonus = RacialBonusRegistry::Instance().GetBonus(bonusId);
        if (bonus) {
            total += bonus->pointCost;
        }
    }
    return total;
}

bool RaceDesigner::Validate() {
    auto errors = m_currentRace.GetValidationErrors();
    m_state.validationWarnings = errors;
    return errors.empty();
}

std::vector<std::string> RaceDesigner::GetValidationErrors() const {
    return m_currentRace.GetValidationErrors();
}

BalanceScore RaceDesigner::GetBalanceScore() const {
    return m_currentRace.GetBalanceScore();
}

float RaceDesigner::GetPowerLevel() const {
    return m_currentRace.CalculatePowerLevel();
}

void RaceDesigner::Render() {
    // ImGui rendering would go here - placeholder for now
}

void RaceDesigner::RenderOverviewTab() {}
void RaceDesigner::RenderPointAllocationTab() {}
void RaceDesigner::RenderUnitArchetypesTab() {}
void RaceDesigner::RenderBuildingArchetypesTab() {}
void RaceDesigner::RenderHeroArchetypesTab() {}
void RaceDesigner::RenderSpellArchetypesTab() {}
void RaceDesigner::RenderBonusesTab() {}
void RaceDesigner::RenderTalentTreeTab() {}
void RaceDesigner::RenderPreviewTab() {}
void RaceDesigner::RenderBalanceWarnings() {}

void RaceDesigner::MarkDirty() {
    m_state.isDirty = true;
    RecalculateBonuses();
}

void RaceDesigner::RecalculateBonuses() {
    m_currentRace.allocation.ComputeAllBonuses();
    m_currentRace.ApplyAllocationBonuses();
}

// HTML Bridge
RaceDesignerHTMLBridge& RaceDesignerHTMLBridge::Instance() {
    static RaceDesignerHTMLBridge instance;
    return instance;
}

void RaceDesignerHTMLBridge::Initialize(RaceDesigner* designer) {
    m_designer = designer;
}

std::string RaceDesignerHTMLBridge::GetRaceJson() const {
    if (!m_designer) return "{}";
    return m_designer->GetRace().ToJson().dump();
}

void RaceDesignerHTMLBridge::SetRaceJson(const std::string& json) {
    if (!m_designer) return;
    try {
        auto j = nlohmann::json::parse(json);
        m_designer->GetRace() = RaceDefinition::FromJson(j);
    } catch (...) {}
}

std::string RaceDesignerHTMLBridge::GetAvailableArchetypesJson() const {
    if (!m_designer) return "{}";

    nlohmann::json result;
    result["units"] = m_designer->GetAvailableUnitArchetypes();
    result["buildings"] = m_designer->GetAvailableBuildingArchetypes();
    result["heroes"] = m_designer->GetAvailableHeroArchetypes();
    result["spells"] = m_designer->GetAvailableSpellArchetypes();
    result["bonuses"] = m_designer->GetAvailableBonuses();
    return result.dump();
}

std::string RaceDesignerHTMLBridge::GetBalanceScoreJson() const {
    if (!m_designer) return "{}";
    return m_designer->GetBalanceScore().ToJson().dump();
}

std::string RaceDesignerHTMLBridge::GetValidationErrorsJson() const {
    if (!m_designer) return "[]";
    return nlohmann::json(m_designer->GetValidationErrors()).dump();
}

void RaceDesignerHTMLBridge::SetPointAllocation(int military, int economy, int magic, int tech) {
    if (!m_designer) return;
    m_designer->SetMilitaryPoints(military);
    m_designer->SetEconomyPoints(economy);
    m_designer->SetMagicPoints(magic);
    m_designer->SetTechnologyPoints(tech);
}

void RaceDesignerHTMLBridge::AddArchetype(const std::string& type, const std::string& id) {
    if (!m_designer) return;
    if (type == "unit") m_designer->AddUnitArchetype(id);
    else if (type == "building") m_designer->AddBuildingArchetype(id);
    else if (type == "hero") m_designer->AddHeroArchetype(id);
    else if (type == "spell") m_designer->AddSpellArchetype(id);
}

void RaceDesignerHTMLBridge::RemoveArchetype(const std::string& type, const std::string& id) {
    if (!m_designer) return;
    if (type == "unit") m_designer->RemoveUnitArchetype(id);
    else if (type == "building") m_designer->RemoveBuildingArchetype(id);
    else if (type == "hero") m_designer->RemoveHeroArchetype(id);
    else if (type == "spell") m_designer->RemoveSpellArchetype(id);
}

} // namespace Editor
} // namespace Vehement
