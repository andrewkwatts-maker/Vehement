# Tutorial: Creating Custom UI Panels

This tutorial guides you through creating custom UI panels for the Nova3D Engine editor using Dear ImGui. You will learn how to build interactive panels, handle user input, and integrate with the engine systems.

## Prerequisites

- Basic understanding of ImGui
- Familiarity with C++ lambdas and callbacks
- Completed [First Entity Tutorial](first_entity.md)

## Overview

We will create a custom "Unit Inspector" panel that displays:
- Unit stats and attributes
- Ability cooldowns
- Equipment slots
- Real-time health/mana bars
- Editable properties

## Part 1: Basic Panel Structure

### Step 1: Create the Panel Class

```cpp
// game/src/editor/panels/UnitInspector.hpp
#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace Vehement {

// Forward declarations
struct UnitData;
struct AbilityData;
struct EquipmentSlot;

/**
 * @brief Custom editor panel for inspecting unit properties
 */
class UnitInspector {
public:
    UnitInspector();
    ~UnitInspector();

    // Lifecycle
    void Initialize();
    void Shutdown();
    void Render();

    // Selection
    void SetSelectedUnit(uint64_t entityId);
    void ClearSelection();
    [[nodiscard]] uint64_t GetSelectedUnit() const { return m_selectedUnit; }
    [[nodiscard]] bool HasSelection() const { return m_selectedUnit != 0; }

    // Visibility
    void SetVisible(bool visible) { m_visible = visible; }
    [[nodiscard]] bool IsVisible() const { return m_visible; }
    void ToggleVisible() { m_visible = !m_visible; }

    // Callbacks
    using UnitChangedCallback = std::function<void(uint64_t entityId)>;
    using PropertyChangedCallback = std::function<void(const std::string& property, float oldValue, float newValue)>;

    void OnUnitChanged(UnitChangedCallback callback) { m_onUnitChanged = callback; }
    void OnPropertyChanged(PropertyChangedCallback callback) { m_onPropertyChanged = callback; }

private:
    // Rendering sections
    void RenderHeader();
    void RenderStatsSection();
    void RenderAbilitiesSection();
    void RenderEquipmentSection();
    void RenderStatusEffectsSection();
    void RenderDebugSection();

    // Helper functions
    void DrawProgressBar(const char* label, float current, float max,
                        const ImVec4& color, bool showText = true);
    void DrawStatRow(const char* label, float value, float baseValue = -1.0f);
    bool DrawEditableStatRow(const char* label, float* value, float min, float max);
    void DrawAbilityButton(const AbilityData& ability);
    void DrawEquipmentSlot(const EquipmentSlot& slot, int index);

    // Data
    uint64_t m_selectedUnit = 0;
    bool m_visible = true;
    bool m_showDebug = false;

    // UI state
    char m_searchBuffer[128] = {0};
    int m_selectedAbilityIndex = -1;
    bool m_editMode = false;

    // Callbacks
    UnitChangedCallback m_onUnitChanged;
    PropertyChangedCallback m_onPropertyChanged;

    // Cached data
    std::string m_unitName;
    std::string m_unitType;
};

} // namespace Vehement
```

### Step 2: Implement the Panel

```cpp
// game/src/editor/panels/UnitInspector.cpp
#include "UnitInspector.hpp"
#include <game/src/systems/lifecycle/LifecycleManager.hpp>
#include <engine/core/Engine.hpp>
#include <imgui_internal.h>
#include <cmath>

namespace Vehement {

// Color scheme
namespace Colors {
    const ImVec4 Health     = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
    const ImVec4 HealthLow  = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
    const ImVec4 Mana       = ImVec4(0.2f, 0.4f, 0.9f, 1.0f);
    const ImVec4 Experience = ImVec4(0.9f, 0.8f, 0.2f, 1.0f);
    const ImVec4 Cooldown   = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    const ImVec4 Ready      = ImVec4(0.3f, 0.9f, 0.3f, 1.0f);
    const ImVec4 Buff       = ImVec4(0.2f, 0.8f, 0.4f, 1.0f);
    const ImVec4 Debuff     = ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
}

UnitInspector::UnitInspector() = default;
UnitInspector::~UnitInspector() = default;

void UnitInspector::Initialize() {
    // Load any saved preferences
}

void UnitInspector::Shutdown() {
    // Save preferences
}

void UnitInspector::SetSelectedUnit(uint64_t entityId) {
    if (m_selectedUnit != entityId) {
        m_selectedUnit = entityId;
        m_selectedAbilityIndex = -1;

        // Cache unit info
        if (auto* lifecycle = LifecycleManager::GetInstance()) {
            m_unitName = lifecycle->GetEntityName(entityId);
            m_unitType = lifecycle->GetEntityTypeName(entityId);
        }

        if (m_onUnitChanged) {
            m_onUnitChanged(entityId);
        }
    }
}

void UnitInspector::ClearSelection() {
    m_selectedUnit = 0;
    m_unitName.clear();
    m_unitType.clear();
    m_selectedAbilityIndex = -1;
}

void UnitInspector::Render() {
    if (!m_visible) return;

    ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Unit Inspector", &m_visible, flags)) {
        if (!HasSelection()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                              "No unit selected");
            ImGui::TextWrapped("Select a unit in the world or hierarchy to inspect its properties.");
        } else {
            RenderHeader();

            ImGui::Separator();

            // Tab bar for different sections
            if (ImGui::BeginTabBar("UnitTabs")) {
                if (ImGui::BeginTabItem("Stats")) {
                    RenderStatsSection();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Abilities")) {
                    RenderAbilitiesSection();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Equipment")) {
                    RenderEquipmentSection();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Effects")) {
                    RenderStatusEffectsSection();
                    ImGui::EndTabItem();
                }

                if (m_showDebug && ImGui::BeginTabItem("Debug")) {
                    RenderDebugSection();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
    }
    ImGui::End();
}

// =============================================================================
// Section Renderers
// =============================================================================

void UnitInspector::RenderHeader() {
    auto* lifecycle = LifecycleManager::GetInstance();
    if (!lifecycle) return;

    // Unit icon and name
    ImGui::BeginGroup();
    {
        // Icon placeholder
        ImGui::Button("##Icon", ImVec2(64, 64));

        ImGui::SameLine();

        ImGui::BeginGroup();
        {
            // Name (editable in edit mode)
            if (m_editMode) {
                char nameBuf[128];
                strncpy(nameBuf, m_unitName.c_str(), sizeof(nameBuf));
                if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf))) {
                    m_unitName = nameBuf;
                    lifecycle->SetEntityName(m_selectedUnit, m_unitName);
                }
            } else {
                ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", m_unitName.c_str());
            }

            // Type
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "%s", m_unitType.c_str());

            // Level
            int level = lifecycle->GetEntityLevel(m_selectedUnit);
            ImGui::Text("Level %d", level);
        }
        ImGui::EndGroup();
    }
    ImGui::EndGroup();

    // Health bar
    float health = lifecycle->GetEntityHealth(m_selectedUnit);
    float maxHealth = lifecycle->GetEntityMaxHealth(m_selectedUnit);
    float healthPercent = maxHealth > 0 ? health / maxHealth : 0;

    ImVec4 healthColor = healthPercent > 0.3f ? Colors::Health : Colors::HealthLow;
    DrawProgressBar("HP", health, maxHealth, healthColor);

    // Mana bar
    float mana = lifecycle->GetEntityMana(m_selectedUnit);
    float maxMana = lifecycle->GetEntityMaxMana(m_selectedUnit);
    if (maxMana > 0) {
        DrawProgressBar("MP", mana, maxMana, Colors::Mana);
    }

    // Experience bar
    float exp = lifecycle->GetEntityExperience(m_selectedUnit);
    float expToLevel = lifecycle->GetExperienceToNextLevel(m_selectedUnit);
    if (expToLevel > 0) {
        DrawProgressBar("XP", exp, expToLevel, Colors::Experience);
    }

    // Edit mode toggle
    ImGui::SameLine(ImGui::GetWindowWidth() - 80);
    if (ImGui::Checkbox("Edit", &m_editMode)) {
        // Could lock/unlock editing here
    }
}

void UnitInspector::RenderStatsSection() {
    auto* lifecycle = LifecycleManager::GetInstance();
    if (!lifecycle) return;

    ImGui::BeginChild("Stats", ImVec2(0, 0), true);

    // Primary stats
    if (ImGui::CollapsingHeader("Primary Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        float strength = lifecycle->GetEntityStat(m_selectedUnit, "strength");
        float baseStr = lifecycle->GetEntityBaseStat(m_selectedUnit, "strength");

        float agility = lifecycle->GetEntityStat(m_selectedUnit, "agility");
        float baseAgi = lifecycle->GetEntityBaseStat(m_selectedUnit, "agility");

        float intelligence = lifecycle->GetEntityStat(m_selectedUnit, "intelligence");
        float baseInt = lifecycle->GetEntityBaseStat(m_selectedUnit, "intelligence");

        if (m_editMode) {
            if (DrawEditableStatRow("Strength", &strength, 1, 999)) {
                lifecycle->SetEntityStat(m_selectedUnit, "strength", strength);
            }
            if (DrawEditableStatRow("Agility", &agility, 1, 999)) {
                lifecycle->SetEntityStat(m_selectedUnit, "agility", agility);
            }
            if (DrawEditableStatRow("Intelligence", &intelligence, 1, 999)) {
                lifecycle->SetEntityStat(m_selectedUnit, "intelligence", intelligence);
            }
        } else {
            DrawStatRow("Strength", strength, baseStr);
            DrawStatRow("Agility", agility, baseAgi);
            DrawStatRow("Intelligence", intelligence, baseInt);
        }

        ImGui::Unindent();
    }

    // Combat stats
    if (ImGui::CollapsingHeader("Combat Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        float damage = lifecycle->GetEntityStat(m_selectedUnit, "damage");
        float armor = lifecycle->GetEntityStat(m_selectedUnit, "armor");
        float attackSpeed = lifecycle->GetEntityStat(m_selectedUnit, "attackSpeed");
        float moveSpeed = lifecycle->GetEntityStat(m_selectedUnit, "moveSpeed");
        float critChance = lifecycle->GetEntityStat(m_selectedUnit, "critChance");

        DrawStatRow("Damage", damage);
        DrawStatRow("Armor", armor);
        DrawStatRow("Attack Speed", attackSpeed);
        DrawStatRow("Move Speed", moveSpeed);
        DrawStatRow("Crit Chance", critChance * 100.0f);

        ImGui::Unindent();
    }

    // Resistances
    if (ImGui::CollapsingHeader("Resistances")) {
        ImGui::Indent();

        float fireRes = lifecycle->GetEntityStat(m_selectedUnit, "fireResist");
        float coldRes = lifecycle->GetEntityStat(m_selectedUnit, "coldResist");
        float lightningRes = lifecycle->GetEntityStat(m_selectedUnit, "lightningResist");
        float poisonRes = lifecycle->GetEntityStat(m_selectedUnit, "poisonResist");

        DrawStatRow("Fire", fireRes * 100.0f);
        DrawStatRow("Cold", coldRes * 100.0f);
        DrawStatRow("Lightning", lightningRes * 100.0f);
        DrawStatRow("Poison", poisonRes * 100.0f);

        ImGui::Unindent();
    }

    ImGui::EndChild();
}

void UnitInspector::RenderAbilitiesSection() {
    auto* lifecycle = LifecycleManager::GetInstance();
    if (!lifecycle) return;

    auto abilities = lifecycle->GetEntityAbilities(m_selectedUnit);

    ImGui::BeginChild("Abilities", ImVec2(0, 0), true);

    // Search filter
    ImGui::InputTextWithHint("##Search", "Search abilities...",
                             m_searchBuffer, sizeof(m_searchBuffer));

    ImGui::Separator();

    // Ability grid
    float buttonSize = 64.0f;
    float windowWidth = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, static_cast<int>(windowWidth / (buttonSize + 10)));

    int index = 0;
    for (const auto& ability : abilities) {
        // Filter by search
        if (m_searchBuffer[0] != '\0') {
            std::string lowerName = ability.name;
            std::string lowerSearch = m_searchBuffer;
            // Simple case-insensitive search
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);
            if (lowerName.find(lowerSearch) == std::string::npos) {
                continue;
            }
        }

        if (index > 0 && index % columns != 0) {
            ImGui::SameLine();
        }

        DrawAbilityButton(ability);
        index++;
    }

    // Selected ability details
    if (m_selectedAbilityIndex >= 0 &&
        m_selectedAbilityIndex < static_cast<int>(abilities.size())) {

        ImGui::Separator();
        const auto& ability = abilities[m_selectedAbilityIndex];

        ImGui::TextColored(ImVec4(1, 1, 0.5f, 1), "%s", ability.name.c_str());
        ImGui::TextWrapped("%s", ability.description.c_str());

        ImGui::Spacing();
        ImGui::Text("Cooldown: %.1fs", ability.cooldown);
        ImGui::Text("Mana Cost: %.0f", ability.manaCost);
        ImGui::Text("Range: %.1f", ability.range);

        if (ability.damage > 0) {
            ImGui::Text("Damage: %.0f %s", ability.damage, ability.damageType.c_str());
        }
    }

    ImGui::EndChild();
}

void UnitInspector::RenderEquipmentSection() {
    auto* lifecycle = LifecycleManager::GetInstance();
    if (!lifecycle) return;

    auto equipment = lifecycle->GetEntityEquipment(m_selectedUnit);

    ImGui::BeginChild("Equipment", ImVec2(0, 0), true);

    // Equipment layout
    // Typical slots: Head, Chest, Legs, Feet, MainHand, OffHand, Ring1, Ring2, Amulet

    ImGui::Columns(2, "EquipmentColumns", false);

    // Left column - character silhouette area
    ImGui::Text("Equipment Slots");
    ImGui::Separator();

    const char* slotNames[] = {
        "Head", "Chest", "Legs", "Feet",
        "Main Hand", "Off Hand", "Ring 1", "Ring 2", "Amulet"
    };

    for (int i = 0; i < 9; i++) {
        ImGui::PushID(i);

        bool hasItem = (i < static_cast<int>(equipment.size()) &&
                       !equipment[i].itemId.empty());

        if (hasItem) {
            if (ImGui::Button(equipment[i].itemName.c_str(), ImVec2(-1, 30))) {
                // Show item tooltip/details
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            ImGui::Button(slotNames[i], ImVec2(-1, 30));
            ImGui::PopStyleColor();
        }

        ImGui::PopID();
    }

    ImGui::NextColumn();

    // Right column - stats from equipment
    ImGui::Text("Equipment Stats");
    ImGui::Separator();

    float totalArmor = 0, totalDamage = 0;
    for (const auto& slot : equipment) {
        if (!slot.itemId.empty()) {
            totalArmor += slot.armor;
            totalDamage += slot.damage;
        }
    }

    ImGui::Text("Total Armor: +%.0f", totalArmor);
    ImGui::Text("Total Damage: +%.0f", totalDamage);

    ImGui::Columns(1);

    ImGui::EndChild();
}

void UnitInspector::RenderStatusEffectsSection() {
    auto* lifecycle = LifecycleManager::GetInstance();
    if (!lifecycle) return;

    auto effects = lifecycle->GetEntityStatusEffects(m_selectedUnit);

    ImGui::BeginChild("Effects", ImVec2(0, 0), true);

    // Buffs
    ImGui::TextColored(Colors::Buff, "Buffs");
    ImGui::Separator();

    bool hasBuffs = false;
    for (const auto& effect : effects) {
        if (!effect.isDebuff) {
            hasBuffs = true;

            ImGui::PushID(effect.id.c_str());

            // Effect name and duration
            ImGui::TextColored(Colors::Buff, "%s", effect.name.c_str());
            ImGui::SameLine(ImGui::GetWindowWidth() - 80);
            ImGui::Text("%.1fs", effect.remainingDuration);

            // Effect description
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "%s", effect.description.c_str());

            // Duration bar
            float progress = effect.remainingDuration / effect.totalDuration;
            ImGui::ProgressBar(progress, ImVec2(-1, 3), "");

            ImGui::PopID();
            ImGui::Spacing();
        }
    }

    if (!hasBuffs) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "No active buffs");
    }

    ImGui::Spacing();

    // Debuffs
    ImGui::TextColored(Colors::Debuff, "Debuffs");
    ImGui::Separator();

    bool hasDebuffs = false;
    for (const auto& effect : effects) {
        if (effect.isDebuff) {
            hasDebuffs = true;

            ImGui::PushID(effect.id.c_str());

            ImGui::TextColored(Colors::Debuff, "%s", effect.name.c_str());
            ImGui::SameLine(ImGui::GetWindowWidth() - 80);
            ImGui::Text("%.1fs", effect.remainingDuration);

            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "%s", effect.description.c_str());

            float progress = effect.remainingDuration / effect.totalDuration;
            ImGui::ProgressBar(progress, ImVec2(-1, 3), "");

            ImGui::PopID();
            ImGui::Spacing();
        }
    }

    if (!hasDebuffs) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "No active debuffs");
    }

    ImGui::EndChild();
}

void UnitInspector::RenderDebugSection() {
    auto* lifecycle = LifecycleManager::GetInstance();
    if (!lifecycle) return;

    ImGui::BeginChild("Debug", ImVec2(0, 0), true);

    // Entity info
    ImGui::Text("Entity ID: %llu", m_selectedUnit);

    auto pos = lifecycle->GetEntityPosition(m_selectedUnit);
    ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);

    float rotation = lifecycle->GetEntityRotation(m_selectedUnit);
    ImGui::Text("Rotation: %.2f", rotation);

    // Component list
    ImGui::Separator();
    ImGui::Text("Components:");

    auto components = lifecycle->GetEntityComponentNames(m_selectedUnit);
    for (const auto& comp : components) {
        if (ImGui::TreeNode(comp.c_str())) {
            // Show component properties
            auto props = lifecycle->GetComponentProperties(m_selectedUnit, comp);
            for (const auto& [name, value] : props) {
                ImGui::Text("%s: %s", name.c_str(), value.c_str());
            }
            ImGui::TreePop();
        }
    }

    // Actions
    ImGui::Separator();
    if (ImGui::Button("Kill Unit")) {
        lifecycle->SetEntityHealth(m_selectedUnit, 0);
    }
    ImGui::SameLine();
    if (ImGui::Button("Full Heal")) {
        float maxHp = lifecycle->GetEntityMaxHealth(m_selectedUnit);
        lifecycle->SetEntityHealth(m_selectedUnit, maxHp);
    }

    if (ImGui::Button("Reset Cooldowns")) {
        lifecycle->ResetAbilityCooldowns(m_selectedUnit);
    }

    ImGui::EndChild();
}

// =============================================================================
// Helper Functions
// =============================================================================

void UnitInspector::DrawProgressBar(const char* label, float current, float max,
                                    const ImVec4& color, bool showText) {
    float progress = max > 0 ? current / max : 0;

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);

    char overlay[64];
    if (showText) {
        snprintf(overlay, sizeof(overlay), "%.0f / %.0f", current, max);
    } else {
        overlay[0] = '\0';
    }

    ImGui::ProgressBar(progress, ImVec2(-1, 18), overlay);

    ImGui::PopStyleColor();

    // Label on the left
    ImVec2 cursorPos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cursorPos.x + 5, cursorPos.y - 18));
    ImGui::TextColored(ImVec4(1, 1, 1, 0.8f), "%s", label);
    ImGui::SetCursorPos(cursorPos);
}

void UnitInspector::DrawStatRow(const char* label, float value, float baseValue) {
    ImGui::Text("%s:", label);
    ImGui::SameLine(150);

    if (baseValue >= 0 && std::abs(value - baseValue) > 0.01f) {
        // Show modified value with color
        ImVec4 color = value > baseValue ?
            ImVec4(0.3f, 1.0f, 0.3f, 1.0f) :  // Green for buff
            ImVec4(1.0f, 0.3f, 0.3f, 1.0f);   // Red for debuff

        ImGui::TextColored(color, "%.1f", value);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(%.1f)", baseValue);
    } else {
        ImGui::Text("%.1f", value);
    }
}

bool UnitInspector::DrawEditableStatRow(const char* label, float* value,
                                         float min, float max) {
    ImGui::Text("%s:", label);
    ImGui::SameLine(150);

    ImGui::PushItemWidth(100);
    ImGui::PushID(label);

    bool changed = ImGui::DragFloat("##value", value, 1.0f, min, max, "%.1f");

    ImGui::PopID();
    ImGui::PopItemWidth();

    return changed;
}

void UnitInspector::DrawAbilityButton(const AbilityData& ability) {
    ImVec2 buttonSize(64, 64);

    // Calculate cooldown overlay
    float cooldownProgress = 0.0f;
    if (ability.currentCooldown > 0) {
        cooldownProgress = ability.currentCooldown / ability.cooldown;
    }

    ImGui::PushID(ability.id.c_str());

    // Button with custom drawing
    ImVec2 pos = ImGui::GetCursorScreenPos();

    bool clicked = ImGui::Button("##ability", buttonSize);

    // Draw cooldown overlay
    if (cooldownProgress > 0) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Dark overlay for cooldown portion
        float overlayHeight = buttonSize.y * cooldownProgress;
        drawList->AddRectFilled(
            pos,
            ImVec2(pos.x + buttonSize.x, pos.y + overlayHeight),
            IM_COL32(0, 0, 0, 180)
        );

        // Cooldown text
        char cdText[16];
        snprintf(cdText, sizeof(cdText), "%.1f", ability.currentCooldown);
        ImVec2 textSize = ImGui::CalcTextSize(cdText);
        drawList->AddText(
            ImVec2(pos.x + (buttonSize.x - textSize.x) * 0.5f,
                   pos.y + (buttonSize.y - textSize.y) * 0.5f),
            IM_COL32(255, 255, 255, 255),
            cdText
        );
    }

    // Tooltip
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextColored(ImVec4(1, 1, 0.5f, 1), "%s", ability.name.c_str());
        ImGui::TextWrapped("%s", ability.description.c_str());
        ImGui::Separator();
        ImGui::Text("Cooldown: %.1fs", ability.cooldown);
        ImGui::Text("Mana: %.0f", ability.manaCost);
        ImGui::EndTooltip();
    }

    if (clicked) {
        // Select this ability
        m_selectedAbilityIndex = &ability - &ability; // Get index
    }

    ImGui::PopID();
}

void UnitInspector::DrawEquipmentSlot(const EquipmentSlot& slot, int index) {
    ImVec2 buttonSize(48, 48);

    ImGui::PushID(index);

    if (slot.itemId.empty()) {
        // Empty slot
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::Button("Empty", buttonSize);
        ImGui::PopStyleColor();
    } else {
        // Filled slot
        if (ImGui::Button(slot.itemName.c_str(), buttonSize)) {
            // Handle click
        }

        // Item tooltip
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s", slot.itemName.c_str());
            if (slot.armor > 0) ImGui::Text("Armor: +%.0f", slot.armor);
            if (slot.damage > 0) ImGui::Text("Damage: +%.0f", slot.damage);
            ImGui::EndTooltip();
        }
    }

    ImGui::PopID();
}

} // namespace Vehement
```

## Part 2: Registering the Panel with the Editor

### Step 1: Add to Editor Class

```cpp
// In Editor.hpp - Add member
#include "panels/UnitInspector.hpp"

class Editor {
    // ... existing code ...

private:
    std::unique_ptr<UnitInspector> m_unitInspector;

public:
    [[nodiscard]] UnitInspector* GetUnitInspector() { return m_unitInspector.get(); }
};

// In Editor.cpp - Initialize in Initialize()
bool Editor::Initialize(Nova::Engine& engine, const EditorConfig& config) {
    // ... existing initialization ...

    m_unitInspector = std::make_unique<UnitInspector>();
    m_unitInspector->Initialize();

    // Register shortcut to toggle panel
    RegisterShortcut({
        GLFW_KEY_U,
        GLFW_MOD_CONTROL,
        "ToggleUnitInspector",
        "Toggle Unit Inspector",
        [this]() { m_unitInspector->ToggleVisible(); }
    });

    return true;
}

// In Editor.cpp - Render in Render()
void Editor::Render() {
    // ... existing rendering ...

    if (m_unitInspector) {
        m_unitInspector->Render();
    }
}
```

### Step 2: Connect to Selection System

```cpp
// In Editor.cpp - Handle selection changes
void Editor::OnEntitySelected(uint64_t entityId) {
    if (m_unitInspector) {
        m_unitInspector->SetSelectedUnit(entityId);
    }
}
```

## Part 3: Creating Reusable UI Components

### Custom Widgets

```cpp
// game/src/editor/widgets/EditorWidgets.hpp
#pragma once

#include <imgui.h>
#include <string>
#include <functional>

namespace Vehement::Widgets {

/**
 * @brief Searchable dropdown list
 */
bool SearchableCombo(const char* label, int* currentItem,
                     const std::vector<std::string>& items,
                     const char* previewValue = nullptr);

/**
 * @brief Color picker with presets
 */
bool ColorPickerWithPresets(const char* label, float col[4],
                            const std::vector<ImVec4>& presets);

/**
 * @brief Draggable curve editor
 */
bool CurveEditor(const char* label, std::vector<ImVec2>& points,
                 const ImVec2& size = ImVec2(0, 150));

/**
 * @brief Asset picker with preview
 */
bool AssetPicker(const char* label, std::string& assetPath,
                 const char* assetType);

/**
 * @brief Timeline widget for animation
 */
bool Timeline(const char* label, float* currentTime, float duration,
              const std::vector<float>& keyframes);

/**
 * @brief Property grid for reflection-based editing
 */
void PropertyGrid(void* object, const Nova::TypeInfo& typeInfo,
                  std::function<void(const std::string&)> onChanged = nullptr);

} // namespace Vehement::Widgets
```

### Implementation Examples

```cpp
// game/src/editor/widgets/EditorWidgets.cpp
#include "EditorWidgets.hpp"
#include <algorithm>

namespace Vehement::Widgets {

bool SearchableCombo(const char* label, int* currentItem,
                     const std::vector<std::string>& items,
                     const char* previewValue) {
    bool valueChanged = false;
    static char searchBuffer[256] = "";

    const char* preview = previewValue;
    if (!preview && *currentItem >= 0 && *currentItem < static_cast<int>(items.size())) {
        preview = items[*currentItem].c_str();
    }

    if (ImGui::BeginCombo(label, preview)) {
        // Search input
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##search", "Search...", searchBuffer, sizeof(searchBuffer));

        ImGui::Separator();

        // Filtered list
        std::string searchLower = searchBuffer;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

        for (int i = 0; i < static_cast<int>(items.size()); i++) {
            std::string itemLower = items[i];
            std::transform(itemLower.begin(), itemLower.end(), itemLower.begin(), ::tolower);

            if (searchBuffer[0] != '\0' && itemLower.find(searchLower) == std::string::npos) {
                continue;
            }

            bool isSelected = (*currentItem == i);
            if (ImGui::Selectable(items[i].c_str(), isSelected)) {
                *currentItem = i;
                valueChanged = true;
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    return valueChanged;
}

bool CurveEditor(const char* label, std::vector<ImVec2>& points, const ImVec2& size) {
    bool modified = false;

    ImVec2 actualSize = size;
    if (actualSize.x <= 0) actualSize.x = ImGui::GetContentRegionAvail().x;

    ImGui::Text("%s", label);

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = actualSize;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Background
    drawList->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(30, 30, 30, 255));

    // Grid
    for (int i = 0; i <= 10; i++) {
        float x = canvasPos.x + canvasSize.x * i / 10.0f;
        float y = canvasPos.y + canvasSize.y * i / 10.0f;

        drawList->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + canvasSize.y),
                         IM_COL32(50, 50, 50, 255));
        drawList->AddLine(ImVec2(canvasPos.x, y), ImVec2(canvasPos.x + canvasSize.x, y),
                         IM_COL32(50, 50, 50, 255));
    }

    // Curve
    if (points.size() >= 2) {
        for (size_t i = 0; i < points.size() - 1; i++) {
            ImVec2 p1(canvasPos.x + points[i].x * canvasSize.x,
                      canvasPos.y + (1.0f - points[i].y) * canvasSize.y);
            ImVec2 p2(canvasPos.x + points[i+1].x * canvasSize.x,
                      canvasPos.y + (1.0f - points[i+1].y) * canvasSize.y);

            drawList->AddLine(p1, p2, IM_COL32(100, 200, 100, 255), 2.0f);
        }
    }

    // Control points
    for (size_t i = 0; i < points.size(); i++) {
        ImVec2 pointPos(canvasPos.x + points[i].x * canvasSize.x,
                        canvasPos.y + (1.0f - points[i].y) * canvasSize.y);

        ImGui::SetCursorScreenPos(ImVec2(pointPos.x - 5, pointPos.y - 5));
        ImGui::PushID(static_cast<int>(i));

        ImGui::InvisibleButton("point", ImVec2(10, 10));

        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);

            points[i].x += delta.x / canvasSize.x;
            points[i].y -= delta.y / canvasSize.y;

            points[i].x = std::clamp(points[i].x, 0.0f, 1.0f);
            points[i].y = std::clamp(points[i].y, 0.0f, 1.0f);

            modified = true;
        }

        ImU32 pointColor = active ? IM_COL32(255, 200, 100, 255) :
                           hovered ? IM_COL32(200, 200, 100, 255) :
                                     IM_COL32(255, 255, 255, 255);

        drawList->AddCircleFilled(pointPos, 5.0f, pointColor);

        ImGui::PopID();
    }

    // Border
    drawList->AddRect(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(100, 100, 100, 255));

    ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + canvasSize.y + 5));
    ImGui::Dummy(ImVec2(0, 5));

    return modified;
}

void PropertyGrid(void* object, const Nova::TypeInfo& typeInfo,
                  std::function<void(const std::string&)> onChanged) {

    ImGui::Columns(2, "PropertyColumns", true);

    for (const auto& prop : typeInfo.properties) {
        if (prop.attributes & Nova::PropertyAttribute::Hidden) {
            continue;
        }

        ImGui::PushID(prop.name.c_str());

        // Property name
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", prop.displayName.c_str());

        if (!prop.tooltip.empty() && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", prop.tooltip.c_str());
        }

        ImGui::NextColumn();

        // Property value
        std::any value = prop.getterAny(object);
        bool changed = false;

        if (prop.typeName == "float") {
            float v = std::any_cast<float>(value);
            if (prop.hasRange) {
                changed = ImGui::SliderFloat("##value", &v, prop.minValue, prop.maxValue);
            } else {
                changed = ImGui::DragFloat("##value", &v, 0.1f);
            }
            if (changed) prop.setterAny(object, v);

        } else if (prop.typeName == "int") {
            int v = std::any_cast<int>(value);
            if (prop.hasRange) {
                changed = ImGui::SliderInt("##value", &v,
                    static_cast<int>(prop.minValue), static_cast<int>(prop.maxValue));
            } else {
                changed = ImGui::DragInt("##value", &v);
            }
            if (changed) prop.setterAny(object, v);

        } else if (prop.typeName == "bool") {
            bool v = std::any_cast<bool>(value);
            changed = ImGui::Checkbox("##value", &v);
            if (changed) prop.setterAny(object, v);

        } else if (prop.typeName == "std::string") {
            std::string v = std::any_cast<std::string>(value);
            char buffer[256];
            strncpy(buffer, v.c_str(), sizeof(buffer));
            changed = ImGui::InputText("##value", buffer, sizeof(buffer));
            if (changed) prop.setterAny(object, std::string(buffer));

        } else if (prop.typeName == "glm::vec3") {
            glm::vec3 v = std::any_cast<glm::vec3>(value);
            changed = ImGui::DragFloat3("##value", &v.x, 0.1f);
            if (changed) prop.setterAny(object, v);

        } else if (prop.typeName == "glm::vec4") {
            glm::vec4 v = std::any_cast<glm::vec4>(value);
            if (prop.isColor) {
                changed = ImGui::ColorEdit4("##value", &v.x);
            } else {
                changed = ImGui::DragFloat4("##value", &v.x, 0.1f);
            }
            if (changed) prop.setterAny(object, v);
        }

        if (changed && onChanged) {
            onChanged(prop.name);
        }

        ImGui::NextColumn();
        ImGui::PopID();
    }

    ImGui::Columns(1);
}

} // namespace Vehement::Widgets
```

## Part 4: Docking and Layout

### Making Panels Dockable

```cpp
// In Editor.cpp - Setup docking
void Editor::SetupDocking() {
    // Enable docking
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Create dockspace
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags dockspaceFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, dockspaceFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f));

    // Setup initial layout on first run
    static bool firstRun = true;
    if (firstRun) {
        firstRun = false;

        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

        // Split layout
        ImGuiID dockLeft, dockRight, dockBottom;
        ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.2f, &dockLeft, &dockRight);
        ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.25f, &dockBottom, &dockRight);

        // Assign windows to docks
        ImGui::DockBuilderDockWindow("Hierarchy", dockLeft);
        ImGui::DockBuilderDockWindow("Unit Inspector", dockLeft);
        ImGui::DockBuilderDockWindow("World View", dockRight);
        ImGui::DockBuilderDockWindow("Console", dockBottom);
        ImGui::DockBuilderDockWindow("Asset Browser", dockBottom);

        ImGui::DockBuilderFinish(dockspaceId);
    }

    ImGui::End();
}
```

## Summary

In this tutorial, you learned how to:

1. **Create custom editor panels** with proper initialization and lifecycle
2. **Render ImGui widgets** including progress bars, stat rows, and buttons
3. **Handle user interaction** with selection, editing, and callbacks
4. **Build reusable widgets** for common UI patterns
5. **Set up docking layouts** for flexible panel arrangement

## Best Practices

1. **Separate Data from UI**: Keep panel logic separate from game logic
2. **Cache Frequently Used Data**: Avoid querying the same data every frame
3. **Use Proper ImGui IDs**: Always use PushID/PopID for repeated elements
4. **Handle Missing Data**: Check for null pointers and invalid states
5. **Provide Tooltips**: Add helpful hints for complex controls

## Next Steps

- Add drag-and-drop support for equipment
- Create custom icons for abilities and items
- Implement undo/redo for property changes
- Add keyboard shortcuts for common actions

## See Also

- [Editor Guide](../EDITOR_GUIDE.md)
- [UI System API](../api/UI.md)
- [Reflection API](../api/Reflection.md)
