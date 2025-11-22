#include "SpellBrowser.hpp"
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

SpellBrowser::SpellBrowser(Editor* editor, ContentBrowser* contentBrowser)
    : m_editor(editor)
    , m_contentBrowser(contentBrowser)
{
}

SpellBrowser::~SpellBrowser() {
    Shutdown();
}

// =============================================================================
// Lifecycle
// =============================================================================

bool SpellBrowser::Initialize() {
    if (m_initialized) {
        return true;
    }

    CacheSpells();

    m_initialized = true;
    return true;
}

void SpellBrowser::Shutdown() {
    m_allSpells.clear();
    m_filteredSpells.clear();
    m_initialized = false;
}

void SpellBrowser::Render() {
    ImGui::Begin("Spell Browser", nullptr, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Damage Indicators", nullptr, &m_showDamageIndicators);
            ImGui::MenuItem("Show Cost Indicators", nullptr, &m_showCostIndicators);
            ImGui::MenuItem("Show Effect Chain", nullptr, &m_showEffectChain);
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
    ImGui::BeginChild("SpellFilterPanel", ImVec2(200, 0), true);
    RenderFilters();
    ImGui::EndChild();

    ImGui::SameLine();

    // Content area
    ImGui::BeginChild("SpellContent", ImVec2(0, 0), true);

    if (m_showEffectChain && !m_selectedSpellId.empty()) {
        RenderEffectChainPreview(m_selectedSpellId);
    } else {
        RenderSpellGrid();
    }

    ImGui::EndChild();

    ImGui::End();
}

void SpellBrowser::Update(float deltaTime) {
    if (m_needsRefresh) {
        CacheSpells();
        m_needsRefresh = false;
    }
}

// =============================================================================
// Spell Access
// =============================================================================

std::vector<SpellStats> SpellBrowser::GetAllSpells() const {
    return m_allSpells;
}

std::optional<SpellStats> SpellBrowser::GetSpell(const std::string& id) const {
    for (const auto& spell : m_allSpells) {
        if (spell.id == id) {
            return spell;
        }
    }
    return std::nullopt;
}

std::vector<SpellStats> SpellBrowser::GetFilteredSpells() const {
    return m_filteredSpells;
}

void SpellBrowser::RefreshSpells() {
    m_needsRefresh = true;
}

// =============================================================================
// Filtering
// =============================================================================

void SpellBrowser::SetFilter(const SpellFilterOptions& filter) {
    m_filter = filter;

    m_filteredSpells.clear();
    for (const auto& spell : m_allSpells) {
        if (MatchesFilter(spell)) {
            m_filteredSpells.push_back(spell);
        }
    }
}

void SpellBrowser::FilterBySchool(const std::string& school) {
    m_filter.schools.clear();
    m_filter.schools.push_back(school);
    SetFilter(m_filter);
}

void SpellBrowser::FilterByTargetType(SpellTargetType type) {
    m_filter.targetTypes.clear();
    m_filter.targetTypes.push_back(type);
    SetFilter(m_filter);
}

void SpellBrowser::FilterByDamageType(SpellDamageType type) {
    m_filter.damageTypes.clear();
    m_filter.damageTypes.push_back(type);
    SetFilter(m_filter);
}

void SpellBrowser::ClearFilters() {
    m_filter = SpellFilterOptions();
    m_filteredSpells = m_allSpells;
}

// =============================================================================
// Effect Chain Preview
// =============================================================================

SpellBrowser::EffectChainNode SpellBrowser::GetEffectChain(const std::string& spellId) const {
    EffectChainNode root;

    auto spellOpt = GetSpell(spellId);
    if (!spellOpt) {
        return root;
    }

    root.effectId = spellId;
    root.name = spellOpt->name;

    // Build chain from spell effects
    for (size_t i = 0; i < spellOpt->effectChain.size(); ++i) {
        EffectChainNode child;
        child.effectId = spellOpt->effectChain[i];
        child.name = spellOpt->effectChain[i];
        child.delay = static_cast<float>(i) * 0.5f;  // Assume 0.5s between chain effects
        root.children.push_back(child);
    }

    // Add applied effects
    for (const auto& effect : spellOpt->appliedEffects) {
        EffectChainNode child;
        child.effectId = effect;
        child.name = effect;
        child.delay = 0.0f;
        root.children.push_back(child);
    }

    return root;
}

void SpellBrowser::PreviewEffects(const std::string& spellId) {
    m_selectedSpellId = spellId;
    m_showEffectChain = true;
}

// =============================================================================
// Statistics
// =============================================================================

std::vector<std::string> SpellBrowser::GetSchools() const {
    std::vector<std::string> schools;
    for (const auto& spell : m_allSpells) {
        if (std::find(schools.begin(), schools.end(), spell.school) == schools.end()) {
            schools.push_back(spell.school);
        }
    }
    std::sort(schools.begin(), schools.end());
    return schools;
}

std::unordered_map<std::string, int> SpellBrowser::GetSpellCountBySchool() const {
    std::unordered_map<std::string, int> counts;
    for (const auto& spell : m_allSpells) {
        counts[spell.school]++;
    }
    return counts;
}

float SpellBrowser::GetAverageDamage(const std::string& school) const {
    float total = 0.0f;
    int count = 0;

    for (const auto& spell : m_allSpells) {
        if (school.empty() || spell.school == school) {
            if (spell.damage > 0) {
                total += spell.damage;
                count++;
            }
        }
    }

    return count > 0 ? total / count : 0.0f;
}

float SpellBrowser::GetAverageManaCost(const std::string& school) const {
    float total = 0.0f;
    int count = 0;

    for (const auto& spell : m_allSpells) {
        if (school.empty() || spell.school == school) {
            if (spell.manaCost > 0) {
                total += spell.manaCost;
                count++;
            }
        }
    }

    return count > 0 ? total / count : 0.0f;
}

// =============================================================================
// Balance Analysis
// =============================================================================

float SpellBrowser::CalculateEfficiency(const std::string& spellId) const {
    auto spellOpt = GetSpell(spellId);
    if (!spellOpt || spellOpt->manaCost <= 0) {
        return 0.0f;
    }

    float totalEffect = spellOpt->damage + spellOpt->healing;
    if (spellOpt->duration > 0) {
        totalEffect += (spellOpt->damageOverTime + spellOpt->healOverTime) * spellOpt->duration;
    }

    return totalEffect / spellOpt->manaCost;
}

float SpellBrowser::CalculateDPS(const std::string& spellId) const {
    auto spellOpt = GetSpell(spellId);
    if (!spellOpt) {
        return 0.0f;
    }

    float totalDamage = spellOpt->damage;
    if (spellOpt->duration > 0) {
        totalDamage += spellOpt->damageOverTime * spellOpt->duration;
    }

    // Account for cooldown and cast time
    float cycleTime = spellOpt->cooldown + spellOpt->castTime;
    if (cycleTime <= 0) {
        cycleTime = 1.0f;  // Minimum 1 second cycle
    }

    return totalDamage / cycleTime;
}

std::vector<std::string> SpellBrowser::GetBalanceWarnings() const {
    std::vector<std::string> warnings;

    float avgEfficiency = 0.0f;
    int count = 0;
    for (const auto& spell : m_allSpells) {
        float eff = CalculateEfficiency(spell.id);
        if (eff > 0) {
            avgEfficiency += eff;
            count++;
        }
    }
    avgEfficiency = count > 0 ? avgEfficiency / count : 1.0f;

    for (const auto& spell : m_allSpells) {
        float efficiency = CalculateEfficiency(spell.id);

        // Check for overpowered spells
        if (efficiency > avgEfficiency * 1.5f) {
            warnings.push_back(spell.name + " has very high efficiency (" +
                               std::to_string(static_cast<int>(efficiency / avgEfficiency * 100)) + "% of average)");
        }

        // Check for underpowered spells
        if (efficiency > 0 && efficiency < avgEfficiency * 0.5f) {
            warnings.push_back(spell.name + " has low efficiency (" +
                               std::to_string(static_cast<int>(efficiency / avgEfficiency * 100)) + "% of average)");
        }

        // Check for very long cooldowns
        if (spell.cooldown > 60.0f) {
            warnings.push_back(spell.name + " has very long cooldown (" +
                               std::to_string(static_cast<int>(spell.cooldown)) + "s)");
        }

        // Check for zero cost damage spells
        if (spell.damage > 0 && spell.manaCost <= 0) {
            warnings.push_back(spell.name + " deals damage with no mana cost");
        }

        // Check for instant high-damage spells
        if (spell.damage > 100 && spell.castTime <= 0) {
            warnings.push_back(spell.name + " is an instant high-damage spell (Damage: " +
                               std::to_string(static_cast<int>(spell.damage)) + ")");
        }
    }

    return warnings;
}

// =============================================================================
// Private - Rendering
// =============================================================================

void SpellBrowser::RenderToolbar() {
    // Search
    char searchBuffer[256] = {0};
    strncpy(searchBuffer, m_filter.searchQuery.c_str(), sizeof(searchBuffer) - 1);

    ImGui::PushItemWidth(200);
    if (ImGui::InputText("Search##SpellSearch", searchBuffer, sizeof(searchBuffer))) {
        m_filter.searchQuery = searchBuffer;
        SetFilter(m_filter);
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();

    if (ImGui::Button("Refresh")) {
        RefreshSpells();
    }

    ImGui::SameLine();

    // Quick filter buttons
    ImGui::Text("Quick:");
    ImGui::SameLine();
    if (ImGui::SmallButton("Damage")) {
        m_filter.showHealingSpells = false;
        m_filter.showDamageSpells = true;
        SetFilter(m_filter);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Healing")) {
        m_filter.showDamageSpells = false;
        m_filter.showHealingSpells = true;
        SetFilter(m_filter);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("All")) {
        ClearFilters();
    }

    ImGui::Separator();
}

void SpellBrowser::RenderFilters() {
    ImGui::Text("Filters");
    ImGui::Separator();

    // School filter
    if (ImGui::CollapsingHeader("School", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto schools = GetSchools();
        for (const auto& school : schools) {
            bool selected = std::find(m_filter.schools.begin(), m_filter.schools.end(), school)
                            != m_filter.schools.end();

            ImVec4 color = GetSchoolColor(school);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            if (ImGui::Checkbox(school.c_str(), &selected)) {
                if (selected) {
                    m_filter.schools.push_back(school);
                } else {
                    m_filter.schools.erase(
                        std::remove(m_filter.schools.begin(), m_filter.schools.end(), school),
                        m_filter.schools.end());
                }
                SetFilter(m_filter);
            }
            ImGui::PopStyleColor();
        }
    }

    // Target type filter
    if (ImGui::CollapsingHeader("Target Type", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* targetTypeNames[] = {
            "None", "Self", "Single Target", "Point Target",
            "Area of Effect", "Cone", "Line", "Chain", "Global"
        };

        for (int i = 0; i < 9; ++i) {
            SpellTargetType type = static_cast<SpellTargetType>(i);
            bool selected = std::find(m_filter.targetTypes.begin(), m_filter.targetTypes.end(), type)
                            != m_filter.targetTypes.end();

            if (ImGui::Checkbox(targetTypeNames[i], &selected)) {
                if (selected) {
                    m_filter.targetTypes.push_back(type);
                } else {
                    m_filter.targetTypes.erase(
                        std::remove(m_filter.targetTypes.begin(), m_filter.targetTypes.end(), type),
                        m_filter.targetTypes.end());
                }
                SetFilter(m_filter);
            }
        }
    }

    // Spell type toggles
    if (ImGui::CollapsingHeader("Spell Type", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("Damage Spells", &m_filter.showDamageSpells)) {
            SetFilter(m_filter);
        }
        if (ImGui::Checkbox("Healing Spells", &m_filter.showHealingSpells)) {
            SetFilter(m_filter);
        }
        if (ImGui::Checkbox("Buff Spells", &m_filter.showBuffSpells)) {
            SetFilter(m_filter);
        }
        if (ImGui::Checkbox("Debuff Spells", &m_filter.showDebuffSpells)) {
            SetFilter(m_filter);
        }
        if (ImGui::Checkbox("Summon Spells", &m_filter.showSummonSpells)) {
            SetFilter(m_filter);
        }
        if (ImGui::Checkbox("Passives", &m_filter.showPassives)) {
            SetFilter(m_filter);
        }
    }

    // Stat ranges
    if (ImGui::CollapsingHeader("Stat Ranges")) {
        static float minDamage = 0, maxDamage = 1000;
        ImGui::Text("Damage:");
        ImGui::PushItemWidth(60);
        if (ImGui::DragFloat("Min##Dmg", &minDamage, 1.0f, 0, 1000)) {
            m_filter.minDamage = minDamage > 0 ? std::make_optional(minDamage) : std::nullopt;
            SetFilter(m_filter);
        }
        ImGui::SameLine();
        if (ImGui::DragFloat("Max##Dmg", &maxDamage, 1.0f, 0, 1000)) {
            m_filter.maxDamage = maxDamage > 0 ? std::make_optional(maxDamage) : std::nullopt;
            SetFilter(m_filter);
        }
        ImGui::PopItemWidth();

        static float minCooldown = 0, maxCooldown = 300;
        ImGui::Text("Cooldown:");
        ImGui::PushItemWidth(60);
        if (ImGui::DragFloat("Min##CD", &minCooldown, 1.0f, 0, 300)) {
            m_filter.minCooldown = minCooldown > 0 ? std::make_optional(minCooldown) : std::nullopt;
            SetFilter(m_filter);
        }
        ImGui::SameLine();
        if (ImGui::DragFloat("Max##CD", &maxCooldown, 1.0f, 0, 300)) {
            m_filter.maxCooldown = maxCooldown > 0 ? std::make_optional(maxCooldown) : std::nullopt;
            SetFilter(m_filter);
        }
        ImGui::PopItemWidth();
    }

    ImGui::Separator();

    // Statistics
    if (ImGui::CollapsingHeader("Statistics")) {
        ImGui::Text("Total Spells: %zu", m_allSpells.size());
        ImGui::Text("Filtered: %zu", m_filteredSpells.size());

        auto counts = GetSpellCountBySchool();
        for (const auto& [school, count] : counts) {
            ImVec4 color = GetSchoolColor(school);
            ImGui::TextColored(color, "  %s: %d", school.c_str(), count);
        }

        ImGui::Separator();
        ImGui::Text("Avg Damage: %.1f", GetAverageDamage());
        ImGui::Text("Avg Mana Cost: %.1f", GetAverageManaCost());
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

void SpellBrowser::RenderSpellGrid() {
    const auto& spells = m_filteredSpells.empty() && m_filter.searchQuery.empty() ? m_allSpells : m_filteredSpells;

    int col = 0;
    for (const auto& spell : spells) {
        ImGui::PushID(spell.id.c_str());
        RenderSpellCard(spell);
        ImGui::PopID();

        col++;
        if (col < m_gridColumns && col < static_cast<int>(spells.size())) {
            ImGui::SameLine();
        } else {
            col = 0;
        }
    }

    if (spells.empty()) {
        ImGui::TextDisabled("No spells found");
    }
}

void SpellBrowser::RenderSpellCard(const SpellStats& spell) {
    bool selected = (spell.id == m_selectedSpellId);

    float cardWidth = (ImGui::GetContentRegionAvail().x - (m_gridColumns - 1) * 10) / m_gridColumns;

    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.3f, 0.2f, 0.5f, 0.5f));
    }

    ImGui::BeginChild(("SpellCard_" + spell.id).c_str(), ImVec2(cardWidth, 200), true);

    // School color indicator
    ImVec4 schoolColor = GetSchoolColor(spell.school);
    ImGui::PushStyleColor(ImGuiCol_Text, schoolColor);
    ImGui::Text("[%s]", spell.school.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // Target type icon
    RenderTargetTypeIcon(spell.targetType);

    // Spell name
    ImGui::TextColored(ImVec4(0.9f, 0.8f, 1.0f, 1.0f), "%s", spell.name.c_str());

    // Passive/Channeled/Toggle indicators
    if (spell.isPassive) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(Passive)");
    }
    if (spell.isChanneled) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "(Channeled)");
    }
    if (spell.isToggle) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.3f, 1.0f), "(Toggle)");
    }

    ImGui::Separator();

    if (m_showDamageIndicators) {
        // Damage/Healing display
        if (spell.damage > 0) {
            RenderDamageTypeIcon(spell.damageType);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%.0f damage", spell.damage);
        }
        if (spell.healing > 0) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%.0f healing", spell.healing);
        }
        if (spell.damageOverTime > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "+%.0f DoT/s (%.1fs)",
                               spell.damageOverTime, spell.duration);
        }
        if (spell.healOverTime > 0) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "+%.0f HoT/s (%.1fs)",
                               spell.healOverTime, spell.duration);
        }
    }

    if (m_showCostIndicators) {
        ImGui::Separator();
        // Costs
        if (spell.manaCost > 0) {
            ImGui::TextColored(ImVec4(0.3f, 0.5f, 1.0f, 1.0f), "Mana: %.0f", spell.manaCost);
        }
        if (spell.healthCost > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Health: %.0f", spell.healthCost);
        }

        // Timing
        ImGui::Text("Cast: %.1fs | CD: %.1fs", spell.castTime, spell.cooldown);

        // Range
        if (spell.range > 0) {
            ImGui::Text("Range: %.0f", spell.range);
            if (spell.radius > 0) {
                ImGui::SameLine();
                ImGui::Text("| Radius: %.0f", spell.radius);
            }
        }
    }

    // Efficiency/DPS indicators
    float efficiency = CalculateEfficiency(spell.id);
    float dps = CalculateDPS(spell.id);

    if (efficiency > 0 || dps > 0) {
        ImGui::Separator();
        if (efficiency > 0) {
            ImGui::Text("Efficiency: %.2f", efficiency);
        }
        if (dps > 0) {
            ImGui::Text("DPS: %.1f", dps);
        }
    }

    ImGui::EndChild();

    if (selected) {
        ImGui::PopStyleColor();
    }

    // Click handling
    if (ImGui::IsItemClicked()) {
        m_selectedSpellId = spell.id;
        if (OnSpellSelected) {
            OnSpellSelected(spell.id);
        }
    }

    // Double-click
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        if (OnSpellDoubleClicked) {
            OnSpellDoubleClicked(spell.id);
        }
    }

    // Tooltip
    if (ImGui::IsItemHovered()) {
        RenderSpellTooltip(spell);
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Open in Editor")) {
            if (OnSpellDoubleClicked) {
                OnSpellDoubleClicked(spell.id);
            }
        }
        if (ImGui::MenuItem("View Effect Chain")) {
            PreviewEffects(spell.id);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate")) {
            // Duplicate the spell with a new ID
            SpellEntry newSpell = spell;
            newSpell.id = "copy_" + spell.id + "_" + std::to_string(rand() % 1000);
            newSpell.name = spell.name + " (Copy)";
            m_allSpells.push_back(newSpell);
            ApplyFilters();
            if (m_editor) m_editor->MarkDirty();
        }
        if (ImGui::MenuItem("Delete")) {
            // Delete the spell from the list
            auto it = std::find_if(m_allSpells.begin(), m_allSpells.end(),
                [&spell](const SpellEntry& s) { return s.id == spell.id; });
            if (it != m_allSpells.end()) {
                m_allSpells.erase(it);
                ApplyFilters();
                if (m_editor) m_editor->MarkDirty();
            }
        }
        ImGui::EndPopup();
    }
}

void SpellBrowser::RenderTargetTypeIcon(SpellTargetType type) {
    const char* icon = GetTargetTypeIcon(type).c_str();
    ImGui::Text("[%s]", icon);
}

void SpellBrowser::RenderDamageTypeIcon(SpellDamageType type) {
    const char* icon = GetDamageTypeIcon(type).c_str();
    ImGui::Text("[%s]", icon);
}

void SpellBrowser::RenderEffectChainPreview(const std::string& spellId) {
    auto chain = GetEffectChain(spellId);

    ImGui::Text("Effect Chain: %s", chain.name.c_str());
    ImGui::Separator();

    if (ImGui::Button("Back to Grid")) {
        m_showEffectChain = false;
    }

    ImGui::Separator();

    // Render chain as tree
    std::function<void(const EffectChainNode&, int)> renderNode = [&](const EffectChainNode& node, int depth) {
        std::string indent(depth * 2, ' ');
        ImGui::Text("%s[%.1fs] %s", indent.c_str(), node.delay, node.name.c_str());

        for (const auto& child : node.children) {
            renderNode(child, depth + 1);
        }
    };

    renderNode(chain, 0);

    // Visual timeline
    ImGui::Separator();
    ImGui::Text("Timeline:");

    float totalDuration = 0.0f;
    std::function<void(const EffectChainNode&)> calcDuration = [&](const EffectChainNode& node) {
        if (node.delay > totalDuration) {
            totalDuration = node.delay;
        }
        for (const auto& child : node.children) {
            calcDuration(child);
        }
    };
    calcDuration(chain);
    totalDuration += 1.0f;  // Add padding

    // Draw timeline
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    float timelineWidth = ImGui::GetContentRegionAvail().x - 20;
    float timelineHeight = 30.0f;

    // Background
    drawList->AddRectFilled(startPos,
                            ImVec2(startPos.x + timelineWidth, startPos.y + timelineHeight),
                            IM_COL32(40, 40, 40, 255));

    // Time markers
    for (float t = 0; t <= totalDuration; t += 0.5f) {
        float x = startPos.x + (t / totalDuration) * timelineWidth;
        drawList->AddLine(ImVec2(x, startPos.y),
                          ImVec2(x, startPos.y + timelineHeight),
                          IM_COL32(80, 80, 80, 255));
    }

    // Effect markers
    std::function<void(const EffectChainNode&, int)> renderTimelineNode = [&](const EffectChainNode& node, int row) {
        float x = startPos.x + (node.delay / totalDuration) * timelineWidth;
        float y = startPos.y + 5 + row * 8;

        drawList->AddCircleFilled(ImVec2(x, y), 4.0f, IM_COL32(100, 150, 255, 255));

        for (size_t i = 0; i < node.children.size(); ++i) {
            renderTimelineNode(node.children[i], row + 1 + static_cast<int>(i));
        }
    };
    renderTimelineNode(chain, 0);

    ImGui::Dummy(ImVec2(timelineWidth, timelineHeight + 10));
}

void SpellBrowser::RenderSpellTooltip(const SpellStats& spell) {
    ImGui::BeginTooltip();

    ImVec4 schoolColor = GetSchoolColor(spell.school);
    ImGui::TextColored(schoolColor, "%s", spell.name.c_str());
    ImGui::TextDisabled("%s spell", spell.school.c_str());
    ImGui::Separator();

    if (!spell.description.empty()) {
        ImGui::TextWrapped("%s", spell.description.c_str());
        ImGui::Separator();
    }

    // All stats
    if (spell.damage > 0) {
        ImGui::Text("Damage: %.0f", spell.damage);
    }
    if (spell.healing > 0) {
        ImGui::Text("Healing: %.0f", spell.healing);
    }
    if (spell.damageOverTime > 0) {
        ImGui::Text("DoT: %.0f/s for %.1fs", spell.damageOverTime, spell.duration);
    }
    if (spell.healOverTime > 0) {
        ImGui::Text("HoT: %.0f/s for %.1fs", spell.healOverTime, spell.duration);
    }

    ImGui::Separator();

    ImGui::Text("Mana Cost: %.0f", spell.manaCost);
    ImGui::Text("Cast Time: %.1fs", spell.castTime);
    ImGui::Text("Cooldown: %.1fs", spell.cooldown);
    ImGui::Text("Range: %.0f", spell.range);
    if (spell.radius > 0) {
        ImGui::Text("Radius: %.0f", spell.radius);
    }

    if (!spell.appliedEffects.empty()) {
        ImGui::Separator();
        ImGui::Text("Applied Effects:");
        for (const auto& effect : spell.appliedEffects) {
            ImGui::BulletText("%s", effect.c_str());
        }
    }

    ImGui::EndTooltip();
}

// =============================================================================
// Private - Data Loading
// =============================================================================

SpellStats SpellBrowser::LoadSpellStats(const std::string& assetId) const {
    SpellStats stats;
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
    stats.school = root.get("school", "arcane").asString();
    stats.description = root.get("description", "").asString();
    stats.iconPath = root.get("icon", "").asString();

    stats.isPassive = root.get("isPassive", false).asBool();
    stats.isChanneled = root.get("isChanneled", false).asBool();
    stats.isToggle = root.get("isToggle", false).asBool();

    // Targeting
    if (root.isMember("targeting")) {
        const auto& targeting = root["targeting"];
        std::string targetStr = targeting.get("type", "none").asString();

        if (targetStr == "self") stats.targetType = SpellTargetType::Self;
        else if (targetStr == "single_target") stats.targetType = SpellTargetType::SingleTarget;
        else if (targetStr == "point_target") stats.targetType = SpellTargetType::PointTarget;
        else if (targetStr == "aoe" || targetStr == "area") stats.targetType = SpellTargetType::AreaOfEffect;
        else if (targetStr == "cone") stats.targetType = SpellTargetType::Cone;
        else if (targetStr == "line") stats.targetType = SpellTargetType::Line;
        else if (targetStr == "chain") stats.targetType = SpellTargetType::Chain;
        else if (targetStr == "global") stats.targetType = SpellTargetType::Global;

        stats.range = targeting.get("range", 0.0f).asFloat();
        stats.radius = targeting.get("radius", 0.0f).asFloat();
        stats.maxTargets = targeting.get("maxTargets", 1).asInt();
    }

    // Damage
    if (root.isMember("damage")) {
        const auto& damage = root["damage"];
        stats.damage = damage.get("amount", 0.0f).asFloat();
        stats.damageOverTime = damage.get("dot", 0.0f).asFloat();
        stats.duration = damage.get("duration", 0.0f).asFloat();

        std::string typeStr = damage.get("type", "none").asString();
        if (typeStr == "physical") stats.damageType = SpellDamageType::Physical;
        else if (typeStr == "fire") stats.damageType = SpellDamageType::Fire;
        else if (typeStr == "ice") stats.damageType = SpellDamageType::Ice;
        else if (typeStr == "lightning") stats.damageType = SpellDamageType::Lightning;
        else if (typeStr == "holy") stats.damageType = SpellDamageType::Holy;
        else if (typeStr == "shadow") stats.damageType = SpellDamageType::Shadow;
        else if (typeStr == "nature") stats.damageType = SpellDamageType::Nature;
        else if (typeStr == "arcane") stats.damageType = SpellDamageType::Arcane;
        else if (typeStr == "true") stats.damageType = SpellDamageType::True;
    }

    // Healing
    if (root.isMember("healing")) {
        const auto& healing = root["healing"];
        stats.healing = healing.get("amount", 0.0f).asFloat();
        stats.healOverTime = healing.get("hot", 0.0f).asFloat();
        if (stats.duration <= 0) {
            stats.duration = healing.get("duration", 0.0f).asFloat();
        }
    }

    // Costs
    if (root.isMember("costs")) {
        const auto& costs = root["costs"];
        stats.manaCost = costs.get("manaCost", costs.get("mana", 0.0f).asFloat()).asFloat();
        stats.healthCost = costs.get("healthCost", 0.0f).asFloat();
        stats.cooldown = costs.get("cooldown", 0.0f).asFloat();
        stats.castTime = costs.get("castTime", 0.0f).asFloat();
    }

    // Effects
    if (root.isMember("effects")) {
        for (const auto& effect : root["effects"]) {
            stats.appliedEffects.push_back(effect.asString());
        }
    }

    if (root.isMember("effectChain")) {
        for (const auto& effect : root["effectChain"]) {
            stats.effectChain.push_back(effect.asString());
        }
    }

    // Summon
    stats.summonedUnit = root.get("summonedUnit", "").asString();

    // Tags
    if (root.isMember("tags")) {
        for (const auto& tag : root["tags"]) {
            stats.tags.push_back(tag.asString());
        }
    }

    return stats;
}

void SpellBrowser::CacheSpells() {
    m_allSpells.clear();

    auto* database = m_contentBrowser->GetDatabase();
    auto allAssets = database->GetAllAssets();

    for (const auto& asset : allAssets) {
        if (asset.type == AssetType::Spell) {
            SpellStats stats = LoadSpellStats(asset.id);
            m_allSpells.push_back(stats);
        }
    }

    SetFilter(m_filter);
}

bool SpellBrowser::MatchesFilter(const SpellStats& spell) const {
    // Search query
    if (!m_filter.searchQuery.empty()) {
        std::string query = m_filter.searchQuery;
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);

        std::string name = spell.name;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        if (name.find(query) == std::string::npos) {
            return false;
        }
    }

    // School filter
    if (!m_filter.schools.empty()) {
        if (std::find(m_filter.schools.begin(), m_filter.schools.end(), spell.school)
            == m_filter.schools.end()) {
            return false;
        }
    }

    // Target type filter
    if (!m_filter.targetTypes.empty()) {
        if (std::find(m_filter.targetTypes.begin(), m_filter.targetTypes.end(), spell.targetType)
            == m_filter.targetTypes.end()) {
            return false;
        }
    }

    // Damage type filter
    if (!m_filter.damageTypes.empty()) {
        if (std::find(m_filter.damageTypes.begin(), m_filter.damageTypes.end(), spell.damageType)
            == m_filter.damageTypes.end()) {
            return false;
        }
    }

    // Spell type filters
    bool isDamageSpell = spell.damage > 0 || spell.damageOverTime > 0;
    bool isHealingSpell = spell.healing > 0 || spell.healOverTime > 0;
    bool isSummonSpell = !spell.summonedUnit.empty();

    if (!m_filter.showDamageSpells && isDamageSpell && !isHealingSpell) return false;
    if (!m_filter.showHealingSpells && isHealingSpell && !isDamageSpell) return false;
    if (!m_filter.showSummonSpells && isSummonSpell) return false;
    if (!m_filter.showPassives && spell.isPassive) return false;

    // Stat ranges
    if (m_filter.minDamage && spell.damage < *m_filter.minDamage) return false;
    if (m_filter.maxDamage && spell.damage > *m_filter.maxDamage) return false;
    if (m_filter.minCooldown && spell.cooldown < *m_filter.minCooldown) return false;
    if (m_filter.maxCooldown && spell.cooldown > *m_filter.maxCooldown) return false;
    if (m_filter.minManaCost && spell.manaCost < *m_filter.minManaCost) return false;
    if (m_filter.maxManaCost && spell.manaCost > *m_filter.maxManaCost) return false;

    return true;
}

// =============================================================================
// Private - Helpers
// =============================================================================

std::string SpellBrowser::GetTargetTypeIcon(SpellTargetType type) const {
    switch (type) {
        case SpellTargetType::Self: return "S";
        case SpellTargetType::SingleTarget: return "T";
        case SpellTargetType::PointTarget: return "P";
        case SpellTargetType::AreaOfEffect: return "A";
        case SpellTargetType::Cone: return "C";
        case SpellTargetType::Line: return "L";
        case SpellTargetType::Chain: return "CH";
        case SpellTargetType::Global: return "G";
        default: return "?";
    }
}

std::string SpellBrowser::GetDamageTypeIcon(SpellDamageType type) const {
    switch (type) {
        case SpellDamageType::Physical: return "PHY";
        case SpellDamageType::Fire: return "FIR";
        case SpellDamageType::Ice: return "ICE";
        case SpellDamageType::Lightning: return "LTN";
        case SpellDamageType::Holy: return "HOL";
        case SpellDamageType::Shadow: return "SHD";
        case SpellDamageType::Nature: return "NAT";
        case SpellDamageType::Arcane: return "ARC";
        case SpellDamageType::True: return "TRU";
        default: return "???";
    }
}

glm::vec4 SpellBrowser::GetSchoolColor(const std::string& school) const {
    if (school == "fire") return glm::vec4(1.0f, 0.4f, 0.2f, 1.0f);
    if (school == "ice" || school == "frost") return glm::vec4(0.4f, 0.8f, 1.0f, 1.0f);
    if (school == "lightning") return glm::vec4(1.0f, 1.0f, 0.4f, 1.0f);
    if (school == "holy" || school == "light") return glm::vec4(1.0f, 1.0f, 0.8f, 1.0f);
    if (school == "shadow" || school == "dark") return glm::vec4(0.5f, 0.3f, 0.6f, 1.0f);
    if (school == "nature") return glm::vec4(0.4f, 0.8f, 0.3f, 1.0f);
    if (school == "arcane") return glm::vec4(0.7f, 0.4f, 0.9f, 1.0f);
    if (school == "physical") return glm::vec4(0.8f, 0.6f, 0.4f, 1.0f);

    return glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);  // Default gray
}

} // namespace Content
} // namespace Vehement
