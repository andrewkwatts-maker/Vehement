#include "TriggerEditor.hpp"
#include "InGameEditor.hpp"
#include "MapFile.hpp"
#include "../../world/World.hpp"

#include <imgui.h>
#include <algorithm>
#include <sstream>

namespace Vehement {

const char* GetEventTypeName(TriggerEventType type) {
    switch (type) {
        case TriggerEventType::MapInit: return "Map Initialization";
        case TriggerEventType::TimerExpires: return "Timer Expires";
        case TriggerEventType::PeriodicEvent: return "Periodic Event";
        case TriggerEventType::UnitEntersRegion: return "Unit Enters Region";
        case TriggerEventType::UnitLeavesRegion: return "Unit Leaves Region";
        case TriggerEventType::UnitDies: return "Unit Dies";
        case TriggerEventType::UnitSpawns: return "Unit Spawns";
        case TriggerEventType::UnitAttacked: return "Unit Takes Damage";
        case TriggerEventType::UnitStartsAbility: return "Unit Starts Ability";
        case TriggerEventType::UnitFinishesAbility: return "Unit Finishes Ability";
        case TriggerEventType::UnitAcquiresItem: return "Unit Acquires Item";
        case TriggerEventType::UnitSellsItem: return "Unit Sells Item";
        case TriggerEventType::PlayerLeavesGame: return "Player Leaves Game";
        case TriggerEventType::PlayerChats: return "Player Sends Chat";
        case TriggerEventType::PlayerSelectsUnit: return "Player Selects Unit";
        case TriggerEventType::PlayerIssuesOrder: return "Player Issues Order";
        case TriggerEventType::ConstructionStarts: return "Construction Starts";
        case TriggerEventType::ConstructionFinishes: return "Construction Finishes";
        case TriggerEventType::BuildingDestroyed: return "Building Destroyed";
        case TriggerEventType::UpgradeStarts: return "Upgrade Starts";
        case TriggerEventType::UpgradeFinishes: return "Upgrade Finishes";
        case TriggerEventType::ResearchStarts: return "Research Starts";
        case TriggerEventType::ResearchFinishes: return "Research Finishes";
        case TriggerEventType::ResourceDepleted: return "Resource Depleted";
        case TriggerEventType::ResourceGathered: return "Resource Gathered";
        case TriggerEventType::GameOver: return "Game Over";
        case TriggerEventType::DialogButtonClicked: return "Dialog Button Clicked";
        case TriggerEventType::Custom: return "Custom Event";
        default: return "Unknown";
    }
}

const char* GetConditionTypeName(TriggerConditionType type) {
    switch (type) {
        case TriggerConditionType::IntegerCompare: return "Integer Comparison";
        case TriggerConditionType::RealCompare: return "Real Comparison";
        case TriggerConditionType::BooleanCompare: return "Boolean Comparison";
        case TriggerConditionType::StringCompare: return "String Comparison";
        case TriggerConditionType::UnitTypeIs: return "Unit Type Is";
        case TriggerConditionType::UnitBelongsTo: return "Unit Belongs To Player";
        case TriggerConditionType::UnitInRegion: return "Unit In Region";
        case TriggerConditionType::UnitIsAlive: return "Unit Is Alive";
        case TriggerConditionType::UnitHasAbility: return "Unit Has Ability";
        case TriggerConditionType::UnitHasItem: return "Unit Has Item";
        case TriggerConditionType::UnitHealthPercent: return "Unit Health Percentage";
        case TriggerConditionType::PlayerHasResources: return "Player Has Resources";
        case TriggerConditionType::PlayerHasUnits: return "Player Has Units";
        case TriggerConditionType::PlayerHasBuilding: return "Player Has Building";
        case TriggerConditionType::PlayerIsAlly: return "Player Is Ally";
        case TriggerConditionType::PlayerIsEnemy: return "Player Is Enemy";
        case TriggerConditionType::GameTimeElapsed: return "Game Time Elapsed";
        case TriggerConditionType::VariableIsSet: return "Variable Equals";
        case TriggerConditionType::And: return "And";
        case TriggerConditionType::Or: return "Or";
        case TriggerConditionType::Not: return "Not";
        case TriggerConditionType::Custom: return "Custom Condition";
        default: return "Unknown";
    }
}

const char* GetActionTypeName(TriggerActionType type) {
    switch (type) {
        case TriggerActionType::CreateUnit: return "Create Unit";
        case TriggerActionType::RemoveUnit: return "Remove Unit";
        case TriggerActionType::KillUnit: return "Kill Unit";
        case TriggerActionType::MoveUnit: return "Move Unit";
        case TriggerActionType::OrderUnit: return "Order Unit";
        case TriggerActionType::SetUnitOwner: return "Set Unit Owner";
        case TriggerActionType::DamageUnit: return "Damage Unit";
        case TriggerActionType::HealUnit: return "Heal Unit";
        case TriggerActionType::AddAbility: return "Add Ability";
        case TriggerActionType::RemoveAbility: return "Remove Ability";
        case TriggerActionType::AddItem: return "Add Item";
        case TriggerActionType::RemoveItem: return "Remove Item";
        case TriggerActionType::SetResources: return "Set Resources";
        case TriggerActionType::AddResources: return "Add Resources";
        case TriggerActionType::RemoveResources: return "Remove Resources";
        case TriggerActionType::SetAlliance: return "Set Alliance";
        case TriggerActionType::Defeat: return "Defeat Player";
        case TriggerActionType::Victory: return "Victory";
        case TriggerActionType::PanCamera: return "Pan Camera";
        case TriggerActionType::SetCameraTarget: return "Set Camera Target";
        case TriggerActionType::CinematicMode: return "Cinematic Mode";
        case TriggerActionType::FadeScreen: return "Fade Screen";
        case TriggerActionType::ShowDialog: return "Show Dialog";
        case TriggerActionType::HideDialog: return "Hide Dialog";
        case TriggerActionType::ShowMessage: return "Show Message";
        case TriggerActionType::DisplayText: return "Display Text";
        case TriggerActionType::ClearMessages: return "Clear Messages";
        case TriggerActionType::PlaySound: return "Play Sound";
        case TriggerActionType::PlayMusic: return "Play Music";
        case TriggerActionType::StopMusic: return "Stop Music";
        case TriggerActionType::SetVolume: return "Set Volume";
        case TriggerActionType::CreateEffect: return "Create Effect";
        case TriggerActionType::DestroyEffect: return "Destroy Effect";
        case TriggerActionType::AddWeather: return "Add Weather";
        case TriggerActionType::RemoveWeather: return "Remove Weather";
        case TriggerActionType::StartTimer: return "Start Timer";
        case TriggerActionType::PauseTimer: return "Pause Timer";
        case TriggerActionType::ResumeTimer: return "Resume Timer";
        case TriggerActionType::DestroyTimer: return "Destroy Timer";
        case TriggerActionType::SetVariable: return "Set Variable";
        case TriggerActionType::ModifyVariable: return "Modify Variable";
        case TriggerActionType::Wait: return "Wait";
        case TriggerActionType::RunTrigger: return "Run Trigger";
        case TriggerActionType::EnableTrigger: return "Enable Trigger";
        case TriggerActionType::DisableTrigger: return "Disable Trigger";
        case TriggerActionType::IfThenElse: return "If/Then/Else";
        case TriggerActionType::ForLoop: return "For Loop";
        case TriggerActionType::ForEachUnit: return "For Each Unit";
        case TriggerActionType::WhileLoop: return "While Loop";
        case TriggerActionType::EndGame: return "End Game";
        case TriggerActionType::PauseGame: return "Pause Game";
        case TriggerActionType::ResumeGame: return "Resume Game";
        case TriggerActionType::SetTimeOfDay: return "Set Time of Day";
        case TriggerActionType::SetGameSpeed: return "Set Game Speed";
        case TriggerActionType::Custom: return "Custom Action";
        default: return "Unknown";
    }
}

const char* GetVariableTypeName(TriggerVariableType type) {
    switch (type) {
        case TriggerVariableType::Integer: return "Integer";
        case TriggerVariableType::Real: return "Real";
        case TriggerVariableType::Boolean: return "Boolean";
        case TriggerVariableType::String: return "String";
        case TriggerVariableType::Unit: return "Unit";
        case TriggerVariableType::UnitGroup: return "Unit Group";
        case TriggerVariableType::Player: return "Player";
        case TriggerVariableType::Point: return "Point";
        case TriggerVariableType::Region: return "Region";
        case TriggerVariableType::Timer: return "Timer";
        case TriggerVariableType::Dialog: return "Dialog";
        case TriggerVariableType::Sound: return "Sound";
        case TriggerVariableType::Effect: return "Effect";
        case TriggerVariableType::Ability: return "Ability";
        default: return "Unknown";
    }
}

TriggerEditor::TriggerEditor() = default;
TriggerEditor::~TriggerEditor() = default;

bool TriggerEditor::Initialize(InGameEditor& parent) {
    if (m_initialized) {
        return true;
    }

    m_parent = &parent;

    // Create default "Map Initialization" trigger
    uint32_t initTriggerId = CreateTrigger("Map Initialization");
    if (Trigger* initTrigger = GetTrigger(initTriggerId)) {
        TriggerEvent mapInitEvent;
        mapInitEvent.type = TriggerEventType::MapInit;
        initTrigger->events.push_back(mapInitEvent);
    }

    // Initialize event templates
    {
        TriggerEvent evt;
        evt.type = TriggerEventType::UnitEntersRegion;
        evt.parameters.push_back({"Region", TriggerVariableType::Region, uint32_t(0)});
        evt.parameters.push_back({"Unit Filter", TriggerVariableType::String, std::string("all")});
        m_eventTemplates["Unit Enters Region"] = evt;
    }
    {
        TriggerEvent evt;
        evt.type = TriggerEventType::TimerExpires;
        evt.parameters.push_back({"Timer", TriggerVariableType::Timer, uint32_t(0)});
        m_eventTemplates["Timer Expires"] = evt;
    }
    {
        TriggerEvent evt;
        evt.type = TriggerEventType::PeriodicEvent;
        evt.parameters.push_back({"Interval", TriggerVariableType::Real, 1.0f});
        m_eventTemplates["Periodic Event"] = evt;
    }
    {
        TriggerEvent evt;
        evt.type = TriggerEventType::UnitDies;
        evt.parameters.push_back({"Dying Unit Filter", TriggerVariableType::String, std::string("all")});
        m_eventTemplates["Unit Dies"] = evt;
    }

    // Initialize condition templates
    {
        TriggerCondition cond;
        cond.type = TriggerConditionType::UnitTypeIs;
        cond.parameters.push_back({"Unit", TriggerVariableType::Unit, uint32_t(0)});
        cond.parameters.push_back({"Unit Type", TriggerVariableType::String, std::string("")});
        m_conditionTemplates["Unit Type Is"] = cond;
    }
    {
        TriggerCondition cond;
        cond.type = TriggerConditionType::PlayerHasResources;
        cond.parameters.push_back({"Player", TriggerVariableType::Player, uint32_t(0)});
        cond.parameters.push_back({"Resource Type", TriggerVariableType::String, std::string("gold")});
        cond.parameters.push_back({"Amount", TriggerVariableType::Integer, int32_t(0)});
        cond.parameters.push_back({"Comparison", TriggerVariableType::String, std::string(">=")});
        m_conditionTemplates["Player Has Resources"] = cond;
    }
    {
        TriggerCondition cond;
        cond.type = TriggerConditionType::UnitIsAlive;
        cond.parameters.push_back({"Unit", TriggerVariableType::Unit, uint32_t(0)});
        m_conditionTemplates["Unit Is Alive"] = cond;
    }

    // Initialize action templates
    {
        TriggerAction act;
        act.type = TriggerActionType::CreateUnit;
        act.parameters.push_back({"Unit Type", TriggerVariableType::String, std::string("")});
        act.parameters.push_back({"Position", TriggerVariableType::Point, glm::vec2(0.0f)});
        act.parameters.push_back({"Player", TriggerVariableType::Player, uint32_t(0)});
        act.parameters.push_back({"Facing", TriggerVariableType::Real, 0.0f});
        m_actionTemplates["Create Unit"] = act;
    }
    {
        TriggerAction act;
        act.type = TriggerActionType::DisplayText;
        act.parameters.push_back({"Text", TriggerVariableType::String, std::string("")});
        act.parameters.push_back({"Duration", TriggerVariableType::Real, 5.0f});
        act.parameters.push_back({"Player", TriggerVariableType::Player, uint32_t(0)});
        m_actionTemplates["Display Text"] = act;
    }
    {
        TriggerAction act;
        act.type = TriggerActionType::Wait;
        act.parameters.push_back({"Duration", TriggerVariableType::Real, 1.0f});
        m_actionTemplates["Wait"] = act;
    }
    {
        TriggerAction act;
        act.type = TriggerActionType::Victory;
        act.parameters.push_back({"Player", TriggerVariableType::Player, uint32_t(0)});
        m_actionTemplates["Victory"] = act;
    }
    {
        TriggerAction act;
        act.type = TriggerActionType::Defeat;
        act.parameters.push_back({"Player", TriggerVariableType::Player, uint32_t(0)});
        m_actionTemplates["Defeat"] = act;
    }

    m_initialized = true;
    return true;
}

void TriggerEditor::Shutdown() {
    m_initialized = false;
    m_parent = nullptr;
    m_triggers.clear();
    m_groups.clear();
    m_variables.clear();
    ClearHistory();
}

void TriggerEditor::LoadFromFile(const MapFile& file) {
    m_triggers = file.GetTriggers();
    m_groups = file.GetTriggerGroups();
    m_variables = file.GetTriggerVariables();

    // Update ID counters
    m_nextTriggerId = 1;
    for (const auto& trigger : m_triggers) {
        m_nextTriggerId = std::max(m_nextTriggerId, trigger.id + 1);
    }
    m_nextGroupId = 1;
    for (const auto& group : m_groups) {
        m_nextGroupId = std::max(m_nextGroupId, group.id + 1);
    }
}

void TriggerEditor::SaveToFile(MapFile& file) const {
    file.SetTriggers(m_triggers);
    file.SetTriggerGroups(m_groups);
    file.SetTriggerVariables(m_variables);
}

void TriggerEditor::ApplyTriggers(World& world) {
    // Register triggers with the world's trigger system
    for (const auto& trigger : m_triggers) {
        if (trigger.enabled && trigger.initiallyOn) {
            world.RegisterTrigger(trigger);
        }
    }

    // Initialize variables
    for (const auto& var : m_variables) {
        world.SetTriggerVariable(var.name, var.value);
    }
}

void TriggerEditor::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update any real-time preview or debug info
}

void TriggerEditor::Render() {
    if (!m_initialized) {
        return;
    }

    // Main trigger editor window layout
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Trigger Editor")) {
        // Split view: tree on left, details on right
        float treeWidth = 250.0f;

        // Left panel - Trigger tree
        ImGui::BeginChild("TriggerTree", ImVec2(treeWidth, 0), true);
        RenderTriggerTree();
        ImGui::EndChild();

        ImGui::SameLine();

        // Right panel - Trigger details
        ImGui::BeginChild("TriggerDetails", ImVec2(0, 0), true);
        RenderTriggerDetails();
        ImGui::EndChild();
    }
    ImGui::End();

    // Variable manager window
    if (m_showVariableManager) {
        RenderVariableManager();
    }

    // Debug panel
    if (m_showDebugPanel && m_debugMode) {
        RenderDebugPanel();
    }
}

void TriggerEditor::ProcessInput() {
    if (!m_initialized) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    if (!io.WantCaptureKeyboard) {
        // Delete selected trigger
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_selectedTriggerId != 0) {
            DeleteTrigger(m_selectedTriggerId);
        }

        // New trigger
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
            CreateTrigger("New Trigger");
        }

        // Toggle variable manager
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
            m_showVariableManager = !m_showVariableManager;
        }
    }
}

uint32_t TriggerEditor::CreateTrigger(const std::string& name) {
    Trigger trigger;
    trigger.id = GenerateTriggerId();
    trigger.name = name;
    trigger.enabled = true;
    trigger.initiallyOn = true;

    m_triggers.push_back(trigger);

    if (OnTriggerCreated) {
        OnTriggerCreated(trigger.id);
    }

    if (OnTriggersModified) {
        OnTriggersModified();
    }

    return trigger.id;
}

void TriggerEditor::DeleteTrigger(uint32_t id) {
    auto it = std::find_if(m_triggers.begin(), m_triggers.end(),
                          [id](const Trigger& t) { return t.id == id; });
    if (it != m_triggers.end()) {
        m_triggers.erase(it);

        if (m_selectedTriggerId == id) {
            m_selectedTriggerId = 0;
        }

        if (OnTriggerDeleted) {
            OnTriggerDeleted(id);
        }

        if (OnTriggersModified) {
            OnTriggersModified();
        }
    }
}

void TriggerEditor::RenameTrigger(uint32_t id, const std::string& name) {
    if (Trigger* trigger = GetTrigger(id)) {
        trigger->name = name;

        if (OnTriggersModified) {
            OnTriggersModified();
        }
    }
}

void TriggerEditor::EnableTrigger(uint32_t id, bool enabled) {
    if (Trigger* trigger = GetTrigger(id)) {
        trigger->enabled = enabled;

        if (OnTriggersModified) {
            OnTriggersModified();
        }
    }
}

Trigger* TriggerEditor::GetTrigger(uint32_t id) {
    auto it = std::find_if(m_triggers.begin(), m_triggers.end(),
                          [id](const Trigger& t) { return t.id == id; });
    return it != m_triggers.end() ? &(*it) : nullptr;
}

void TriggerEditor::SelectTrigger(uint32_t id) {
    m_selectedTriggerId = id;
    m_selectedEventIndex = -1;
    m_selectedConditionIndex = -1;
    m_selectedActionIndex = -1;

    if (OnTriggerSelected) {
        OnTriggerSelected(id);
    }
}

Trigger* TriggerEditor::GetSelectedTrigger() {
    return GetTrigger(m_selectedTriggerId);
}

uint32_t TriggerEditor::CreateGroup(const std::string& name, uint32_t parentId) {
    TriggerGroup group;
    group.id = GenerateGroupId();
    group.name = name;
    group.parentGroupId = parentId;
    group.expanded = true;

    m_groups.push_back(group);

    if (parentId != 0) {
        if (TriggerGroup* parent = GetGroup(parentId)) {
            parent->childGroupIds.push_back(group.id);
        }
    }

    return group.id;
}

void TriggerEditor::DeleteGroup(uint32_t id) {
    auto it = std::find_if(m_groups.begin(), m_groups.end(),
                          [id](const TriggerGroup& g) { return g.id == id; });
    if (it != m_groups.end()) {
        // Move triggers to parent or root
        for (uint32_t triggerId : it->triggerIds) {
            if (Trigger* trigger = GetTrigger(triggerId)) {
                trigger->parentGroupId = it->parentGroupId;
            }
        }

        // Remove from parent's child list
        if (it->parentGroupId != 0) {
            if (TriggerGroup* parent = GetGroup(it->parentGroupId)) {
                parent->childGroupIds.erase(
                    std::remove(parent->childGroupIds.begin(), parent->childGroupIds.end(), id),
                    parent->childGroupIds.end());
            }
        }

        m_groups.erase(it);
    }
}

void TriggerEditor::RenameGroup(uint32_t id, const std::string& name) {
    if (TriggerGroup* group = GetGroup(id)) {
        group->name = name;
    }
}

void TriggerEditor::MoveToGroup(uint32_t triggerId, uint32_t groupId) {
    Trigger* trigger = GetTrigger(triggerId);
    if (!trigger) {
        return;
    }

    // Remove from old group
    if (trigger->parentGroupId != 0) {
        if (TriggerGroup* oldGroup = GetGroup(trigger->parentGroupId)) {
            oldGroup->triggerIds.erase(
                std::remove(oldGroup->triggerIds.begin(), oldGroup->triggerIds.end(), triggerId),
                oldGroup->triggerIds.end());
        }
    }

    // Add to new group
    trigger->parentGroupId = groupId;
    if (groupId != 0) {
        if (TriggerGroup* newGroup = GetGroup(groupId)) {
            newGroup->triggerIds.push_back(triggerId);
        }
    }
}

TriggerGroup* TriggerEditor::GetGroup(uint32_t id) {
    auto it = std::find_if(m_groups.begin(), m_groups.end(),
                          [id](const TriggerGroup& g) { return g.id == id; });
    return it != m_groups.end() ? &(*it) : nullptr;
}

void TriggerEditor::CreateVariable(const TriggerVariable& variable) {
    // Check if variable already exists
    auto it = std::find_if(m_variables.begin(), m_variables.end(),
                          [&variable](const TriggerVariable& v) { return v.name == variable.name; });
    if (it == m_variables.end()) {
        m_variables.push_back(variable);
    }
}

void TriggerEditor::DeleteVariable(const std::string& name) {
    m_variables.erase(
        std::remove_if(m_variables.begin(), m_variables.end(),
                      [&name](const TriggerVariable& v) { return v.name == name; }),
        m_variables.end());
}

void TriggerEditor::UpdateVariable(const std::string& name, const TriggerVariable& variable) {
    auto it = std::find_if(m_variables.begin(), m_variables.end(),
                          [&name](const TriggerVariable& v) { return v.name == name; });
    if (it != m_variables.end()) {
        *it = variable;
    }
}

TriggerVariable* TriggerEditor::GetVariable(const std::string& name) {
    auto it = std::find_if(m_variables.begin(), m_variables.end(),
                          [&name](const TriggerVariable& v) { return v.name == name; });
    return it != m_variables.end() ? &(*it) : nullptr;
}

void TriggerEditor::AddEvent(uint32_t triggerId, const TriggerEvent& event) {
    if (Trigger* trigger = GetTrigger(triggerId)) {
        trigger->events.push_back(event);
        if (OnTriggersModified) OnTriggersModified();
    }
}

void TriggerEditor::RemoveEvent(uint32_t triggerId, size_t index) {
    if (Trigger* trigger = GetTrigger(triggerId)) {
        if (index < trigger->events.size()) {
            trigger->events.erase(trigger->events.begin() + index);
            if (OnTriggersModified) OnTriggersModified();
        }
    }
}

void TriggerEditor::UpdateEvent(uint32_t triggerId, size_t index, const TriggerEvent& event) {
    if (Trigger* trigger = GetTrigger(triggerId)) {
        if (index < trigger->events.size()) {
            trigger->events[index] = event;
            if (OnTriggersModified) OnTriggersModified();
        }
    }
}

void TriggerEditor::AddCondition(uint32_t triggerId, const TriggerCondition& condition) {
    if (Trigger* trigger = GetTrigger(triggerId)) {
        trigger->conditions.push_back(condition);
        if (OnTriggersModified) OnTriggersModified();
    }
}

void TriggerEditor::RemoveCondition(uint32_t triggerId, size_t index) {
    if (Trigger* trigger = GetTrigger(triggerId)) {
        if (index < trigger->conditions.size()) {
            trigger->conditions.erase(trigger->conditions.begin() + index);
            if (OnTriggersModified) OnTriggersModified();
        }
    }
}

void TriggerEditor::UpdateCondition(uint32_t triggerId, size_t index, const TriggerCondition& condition) {
    if (Trigger* trigger = GetTrigger(triggerId)) {
        if (index < trigger->conditions.size()) {
            trigger->conditions[index] = condition;
            if (OnTriggersModified) OnTriggersModified();
        }
    }
}

void TriggerEditor::AddAction(uint32_t triggerId, const TriggerAction& action) {
    if (Trigger* trigger = GetTrigger(triggerId)) {
        trigger->actions.push_back(action);
        if (OnTriggersModified) OnTriggersModified();
    }
}

void TriggerEditor::RemoveAction(uint32_t triggerId, size_t index) {
    if (Trigger* trigger = GetTrigger(triggerId)) {
        if (index < trigger->actions.size()) {
            trigger->actions.erase(trigger->actions.begin() + index);
            if (OnTriggersModified) OnTriggersModified();
        }
    }
}

void TriggerEditor::UpdateAction(uint32_t triggerId, size_t index, const TriggerAction& action) {
    if (Trigger* trigger = GetTrigger(triggerId)) {
        if (index < trigger->actions.size()) {
            trigger->actions[index] = action;
            if (OnTriggersModified) OnTriggersModified();
        }
    }
}

void TriggerEditor::MoveAction(uint32_t triggerId, size_t fromIndex, size_t toIndex) {
    if (Trigger* trigger = GetTrigger(triggerId)) {
        if (fromIndex < trigger->actions.size() && toIndex < trigger->actions.size()) {
            TriggerAction action = std::move(trigger->actions[fromIndex]);
            trigger->actions.erase(trigger->actions.begin() + fromIndex);
            trigger->actions.insert(trigger->actions.begin() + toIndex, std::move(action));
            if (OnTriggersModified) OnTriggersModified();
        }
    }
}

bool TriggerEditor::ValidateTrigger(uint32_t id, std::string& errorMessage) const {
    auto it = std::find_if(m_triggers.begin(), m_triggers.end(),
                          [id](const Trigger& t) { return t.id == id; });
    if (it == m_triggers.end()) {
        errorMessage = "Trigger not found";
        return false;
    }

    const Trigger& trigger = *it;

    // Check for events
    if (trigger.events.empty()) {
        errorMessage = "Trigger has no events";
        return false;
    }

    // Check for actions
    if (trigger.actions.empty()) {
        errorMessage = "Trigger has no actions";
        return false;
    }

    // Validate event parameters
    for (const auto& event : trigger.events) {
        // Check for required parameters based on event type
        // This is a simplified check
    }

    return true;
}

bool TriggerEditor::ValidateAll(std::vector<std::string>& errors) const {
    errors.clear();

    for (const auto& trigger : m_triggers) {
        std::string error;
        if (!ValidateTrigger(trigger.id, error)) {
            errors.push_back(trigger.name + ": " + error);
        }
    }

    return errors.empty();
}

void TriggerEditor::TestTrigger(uint32_t id) {
    // Fire the trigger in the test environment
    if (m_parent && m_debugMode) {
        // Trigger test execution
    }
}

void TriggerEditor::SetDebugMode(bool enabled) {
    m_debugMode = enabled;
    m_showDebugPanel = enabled;
}

std::vector<std::string> TriggerEditor::GetEventTemplates() const {
    std::vector<std::string> templates;
    for (const auto& [name, _] : m_eventTemplates) {
        templates.push_back(name);
    }
    return templates;
}

std::vector<std::string> TriggerEditor::GetConditionTemplates() const {
    std::vector<std::string> templates;
    for (const auto& [name, _] : m_conditionTemplates) {
        templates.push_back(name);
    }
    return templates;
}

std::vector<std::string> TriggerEditor::GetActionTemplates() const {
    std::vector<std::string> templates;
    for (const auto& [name, _] : m_actionTemplates) {
        templates.push_back(name);
    }
    return templates;
}

TriggerEvent TriggerEditor::CreateEventFromTemplate(const std::string& templateName) const {
    auto it = m_eventTemplates.find(templateName);
    if (it != m_eventTemplates.end()) {
        return it->second;
    }
    return TriggerEvent{};
}

TriggerCondition TriggerEditor::CreateConditionFromTemplate(const std::string& templateName) const {
    auto it = m_conditionTemplates.find(templateName);
    if (it != m_conditionTemplates.end()) {
        return it->second;
    }
    return TriggerCondition{};
}

TriggerAction TriggerEditor::CreateActionFromTemplate(const std::string& templateName) const {
    auto it = m_actionTemplates.find(templateName);
    if (it != m_actionTemplates.end()) {
        return it->second;
    }
    return TriggerAction{};
}

void TriggerEditor::ExecuteCommand(std::unique_ptr<TriggerEditorCommand> command) {
    command->Execute();
    m_undoStack.push_back(std::move(command));
    m_redoStack.clear();

    if (m_undoStack.size() > MAX_UNDO_HISTORY) {
        m_undoStack.pop_front();
    }
}

void TriggerEditor::Undo() {
    if (m_undoStack.empty()) return;

    auto command = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    command->Undo();
    m_redoStack.push_back(std::move(command));
}

void TriggerEditor::Redo() {
    if (m_redoStack.empty()) return;

    auto command = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    command->Execute();
    m_undoStack.push_back(std::move(command));
}

void TriggerEditor::ClearHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
}

void TriggerEditor::RenderTriggerTree() {
    // Toolbar
    if (ImGui::Button("+Trigger")) {
        CreateTrigger("New Trigger");
    }
    ImGui::SameLine();
    if (ImGui::Button("+Group")) {
        CreateGroup("New Group");
    }
    ImGui::SameLine();
    if (ImGui::Button("Variables")) {
        m_showVariableManager = !m_showVariableManager;
    }

    ImGui::Separator();

    // Render root-level triggers and groups
    for (auto& group : m_groups) {
        if (group.parentGroupId == 0) {
            RenderGroupNode(group);
        }
    }

    for (auto& trigger : m_triggers) {
        if (trigger.parentGroupId == 0) {
            RenderTriggerNode(trigger);
        }
    }
}

void TriggerEditor::RenderGroupNode(TriggerGroup& group) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (m_selectedGroupId == group.id) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool isOpen = ImGui::TreeNodeEx(group.name.c_str(), flags);

    if (ImGui::IsItemClicked()) {
        m_selectedGroupId = group.id;
        m_selectedTriggerId = 0;
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Rename")) {
            // Show rename dialog
        }
        if (ImGui::MenuItem("Delete")) {
            DeleteGroup(group.id);
        }
        ImGui::EndPopup();
    }

    if (isOpen) {
        // Child groups
        for (uint32_t childId : group.childGroupIds) {
            if (TriggerGroup* childGroup = GetGroup(childId)) {
                RenderGroupNode(*childGroup);
            }
        }

        // Triggers in group
        for (uint32_t triggerId : group.triggerIds) {
            if (Trigger* trigger = GetTrigger(triggerId)) {
                RenderTriggerNode(*trigger);
            }
        }

        ImGui::TreePop();
    }
}

void TriggerEditor::RenderTriggerNode(Trigger& trigger) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (m_selectedTriggerId == trigger.id) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // Icon based on state
    const char* icon = trigger.enabled ? "[T]" : "[x]";
    std::string label = std::string(icon) + " " + trigger.name;

    ImGui::TreeNodeEx(label.c_str(), flags);

    if (ImGui::IsItemClicked()) {
        SelectTrigger(trigger.id);
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem(trigger.enabled ? "Disable" : "Enable")) {
            EnableTrigger(trigger.id, !trigger.enabled);
        }
        if (ImGui::MenuItem("Rename")) {
            // Show rename dialog
        }
        if (ImGui::MenuItem("Duplicate")) {
            // Duplicate trigger
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
            DeleteTrigger(trigger.id);
        }
        ImGui::EndPopup();
    }
}

void TriggerEditor::RenderTriggerDetails() {
    Trigger* trigger = GetSelectedTrigger();
    if (!trigger) {
        ImGui::Text("Select a trigger to edit");
        return;
    }

    // Trigger name and settings
    char nameBuffer[256];
    strncpy(nameBuffer, trigger->name.c_str(), sizeof(nameBuffer) - 1);
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        trigger->name = nameBuffer;
    }

    ImGui::Checkbox("Enabled", &trigger->enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Initially On", &trigger->initiallyOn);
    ImGui::SameLine();
    ImGui::Checkbox("Run Once", &trigger->runOnce);

    char commentBuffer[512];
    strncpy(commentBuffer, trigger->comment.c_str(), sizeof(commentBuffer) - 1);
    if (ImGui::InputTextMultiline("Comment", commentBuffer, sizeof(commentBuffer), ImVec2(0, 40))) {
        trigger->comment = commentBuffer;
    }

    ImGui::Separator();

    // Tabs for Events, Conditions, Actions
    if (ImGui::BeginTabBar("TriggerTabs")) {
        if (ImGui::BeginTabItem("Events")) {
            RenderEventEditor();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Conditions")) {
            RenderConditionEditor();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Actions")) {
            RenderActionEditor();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Preview")) {
            std::string syntax = GenerateTriggerSyntax(*trigger);
            ImGui::TextWrapped("%s", syntax.c_str());
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void TriggerEditor::RenderEventEditor() {
    Trigger* trigger = GetSelectedTrigger();
    if (!trigger) return;

    if (ImGui::Button("Add Event")) {
        ImGui::OpenPopup("AddEventPopup");
    }

    if (ImGui::BeginPopup("AddEventPopup")) {
        for (const auto& name : GetEventTemplates()) {
            if (ImGui::MenuItem(name.c_str())) {
                AddEvent(trigger->id, CreateEventFromTemplate(name));
            }
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // List events
    for (size_t i = 0; i < trigger->events.size(); ++i) {
        TriggerEvent& event = trigger->events[i];
        ImGui::PushID(static_cast<int>(i));

        bool isSelected = (m_selectedEventIndex == static_cast<int>(i));
        if (ImGui::Selectable(GetEventTypeName(event.type), isSelected)) {
            m_selectedEventIndex = static_cast<int>(i);
        }

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete")) {
                RemoveEvent(trigger->id, i);
            }
            ImGui::EndPopup();
        }

        // Show parameters if selected
        if (isSelected) {
            ImGui::Indent();
            for (auto& param : event.parameters) {
                RenderParameterEditor(param);
            }
            ImGui::Unindent();
        }

        ImGui::PopID();
    }
}

void TriggerEditor::RenderConditionEditor() {
    Trigger* trigger = GetSelectedTrigger();
    if (!trigger) return;

    if (ImGui::Button("Add Condition")) {
        ImGui::OpenPopup("AddConditionPopup");
    }

    if (ImGui::BeginPopup("AddConditionPopup")) {
        for (const auto& name : GetConditionTemplates()) {
            if (ImGui::MenuItem(name.c_str())) {
                AddCondition(trigger->id, CreateConditionFromTemplate(name));
            }
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // List conditions
    for (size_t i = 0; i < trigger->conditions.size(); ++i) {
        TriggerCondition& condition = trigger->conditions[i];
        ImGui::PushID(static_cast<int>(i));

        bool isSelected = (m_selectedConditionIndex == static_cast<int>(i));
        if (ImGui::Selectable(GetConditionTypeName(condition.type), isSelected)) {
            m_selectedConditionIndex = static_cast<int>(i);
        }

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete")) {
                RemoveCondition(trigger->id, i);
            }
            ImGui::EndPopup();
        }

        if (isSelected) {
            ImGui::Indent();
            for (auto& param : condition.parameters) {
                RenderParameterEditor(param);
            }
            ImGui::Unindent();
        }

        ImGui::PopID();
    }
}

void TriggerEditor::RenderActionEditor() {
    Trigger* trigger = GetSelectedTrigger();
    if (!trigger) return;

    if (ImGui::Button("Add Action")) {
        ImGui::OpenPopup("AddActionPopup");
    }

    if (ImGui::BeginPopup("AddActionPopup")) {
        for (const auto& name : GetActionTemplates()) {
            if (ImGui::MenuItem(name.c_str())) {
                AddAction(trigger->id, CreateActionFromTemplate(name));
            }
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // List actions
    for (size_t i = 0; i < trigger->actions.size(); ++i) {
        TriggerAction& action = trigger->actions[i];
        ImGui::PushID(static_cast<int>(i));

        bool isSelected = (m_selectedActionIndex == static_cast<int>(i));
        if (ImGui::Selectable(GetActionTypeName(action.type), isSelected)) {
            m_selectedActionIndex = static_cast<int>(i);
        }

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Move Up") && i > 0) {
                MoveAction(trigger->id, i, i - 1);
            }
            if (ImGui::MenuItem("Move Down") && i < trigger->actions.size() - 1) {
                MoveAction(trigger->id, i, i + 1);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                RemoveAction(trigger->id, i);
            }
            ImGui::EndPopup();
        }

        if (isSelected) {
            ImGui::Indent();
            for (auto& param : action.parameters) {
                RenderParameterEditor(param);
            }
            ImGui::Unindent();
        }

        ImGui::PopID();
    }
}

void TriggerEditor::RenderVariableManager() {
    if (ImGui::Begin("Variables", &m_showVariableManager)) {
        if (ImGui::Button("Add Variable")) {
            TriggerVariable var;
            var.name = "NewVariable";
            var.type = TriggerVariableType::Integer;
            var.value = int32_t(0);
            CreateVariable(var);
        }

        ImGui::Separator();

        // List variables
        for (size_t i = 0; i < m_variables.size(); ++i) {
            TriggerVariable& var = m_variables[i];
            ImGui::PushID(static_cast<int>(i));

            ImGui::Text("%s (%s)", var.name.c_str(), GetVariableTypeName(var.type));

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete")) {
                    DeleteVariable(var.name);
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }
    }
    ImGui::End();
}

void TriggerEditor::RenderDebugPanel() {
    if (ImGui::Begin("Trigger Debug", &m_showDebugPanel)) {
        ImGui::Text("Debug Mode: %s", m_debugMode ? "ON" : "OFF");

        if (ImGui::Button("Test Selected")) {
            if (m_selectedTriggerId != 0) {
                TestTrigger(m_selectedTriggerId);
            }
        }
    }
    ImGui::End();
}

void TriggerEditor::RenderParameterEditor(TriggerParameter& param) {
    ImGui::PushID(param.name.c_str());

    switch (param.type) {
        case TriggerVariableType::Integer: {
            int32_t value = std::get<int32_t>(param.value);
            if (ImGui::InputInt(param.name.c_str(), &value)) {
                param.value = value;
            }
            break;
        }
        case TriggerVariableType::Real: {
            float value = std::get<float>(param.value);
            if (ImGui::InputFloat(param.name.c_str(), &value)) {
                param.value = value;
            }
            break;
        }
        case TriggerVariableType::Boolean: {
            bool value = std::get<bool>(param.value);
            if (ImGui::Checkbox(param.name.c_str(), &value)) {
                param.value = value;
            }
            break;
        }
        case TriggerVariableType::String: {
            std::string& value = std::get<std::string>(param.value);
            char buffer[256];
            strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText(param.name.c_str(), buffer, sizeof(buffer))) {
                value = buffer;
            }
            break;
        }
        case TriggerVariableType::Point: {
            glm::vec2& value = std::get<glm::vec2>(param.value);
            ImGui::InputFloat2(param.name.c_str(), &value.x);
            break;
        }
        default:
            ImGui::Text("%s: [Complex Type]", param.name.c_str());
            break;
    }

    ImGui::PopID();
}

std::string TriggerEditor::GenerateTriggerSyntax(const Trigger& trigger) const {
    std::stringstream ss;

    ss << "Trigger: " << trigger.name << "\n";
    ss << "  Enabled: " << (trigger.enabled ? "Yes" : "No") << "\n";
    ss << "  Initially On: " << (trigger.initiallyOn ? "Yes" : "No") << "\n\n";

    ss << "Events:\n";
    for (const auto& event : trigger.events) {
        ss << "  - " << GenerateEventSyntax(event) << "\n";
    }

    ss << "\nConditions:\n";
    for (const auto& condition : trigger.conditions) {
        ss << "  - " << GenerateConditionSyntax(condition) << "\n";
    }

    ss << "\nActions:\n";
    for (const auto& action : trigger.actions) {
        ss << "  - " << GenerateActionSyntax(action) << "\n";
    }

    return ss.str();
}

std::string TriggerEditor::GenerateEventSyntax(const TriggerEvent& event) const {
    return GetEventTypeName(event.type);
}

std::string TriggerEditor::GenerateConditionSyntax(const TriggerCondition& condition) const {
    return GetConditionTypeName(condition.type);
}

std::string TriggerEditor::GenerateActionSyntax(const TriggerAction& action) const {
    return GetActionTypeName(action.type);
}

uint32_t TriggerEditor::GenerateTriggerId() {
    return m_nextTriggerId++;
}

uint32_t TriggerEditor::GenerateGroupId() {
    return m_nextGroupId++;
}

} // namespace Vehement
