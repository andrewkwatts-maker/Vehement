#include "FunctionBindingPanel.hpp"
#include "FunctionBrowser.hpp"
#include "../Editor.hpp"

#include <imgui.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Vehement {

FunctionBindingPanel::FunctionBindingPanel() = default;
FunctionBindingPanel::~FunctionBindingPanel() {
    Shutdown();
}

bool FunctionBindingPanel::Initialize(Editor* editor) {
    if (m_initialized) return true;
    m_editor = editor;
    m_initialized = true;
    return true;
}

void FunctionBindingPanel::Shutdown() {
    m_bindings.clear();
    m_bindingIndex.clear();
    m_selectedBinding = nullptr;
    m_initialized = false;
}

void FunctionBindingPanel::Render() {
    if (!m_initialized) return;

    if (ImGui::Begin("Function Bindings", nullptr, ImGuiWindowFlags_MenuBar)) {
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Binding", "Ctrl+N")) CreateNewBinding();
                ImGui::Separator();
                if (ImGui::MenuItem("Import...")) m_showImportDialog = true;
                if (ImGui::MenuItem("Export Selected...", nullptr, false, m_selectedBinding != nullptr)) {
                    m_showExportDialog = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save All", "Ctrl+S")) SaveBindings("bindings.json");
                if (ImGui::MenuItem("Load...")) LoadBindings("bindings.json");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, m_selectedBinding != nullptr)) {
                    DuplicateBinding(m_selectedBinding->id);
                }
                if (ImGui::MenuItem("Delete", "Delete", false, m_selectedBinding != nullptr)) {
                    RemoveBinding(m_selectedBinding->id);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Enable All")) {
                    for (auto& b : m_bindings) b.enabled = true;
                }
                if (ImGui::MenuItem("Disable All")) {
                    for (auto& b : m_bindings) b.enabled = false;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        // Toolbar
        RenderToolbar();

        // Main layout - list on left, editor on right
        float listWidth = m_listWidth;
        float editorWidth = ImGui::GetContentRegionAvail().x - listWidth - 8;

        // Binding list
        ImGui::BeginChild("BindingList", ImVec2(listWidth, 0), true);
        RenderBindingList();
        ImGui::EndChild();

        ImGui::SameLine();

        // Splitter (simple)
        ImGui::Button("##splitter", ImVec2(4, ImGui::GetContentRegionAvail().y));
        if (ImGui::IsItemActive()) {
            m_listWidth += ImGui::GetIO().MouseDelta.x;
            m_listWidth = std::clamp(m_listWidth, 150.0f, 400.0f);
        }

        ImGui::SameLine();

        // Binding editor
        ImGui::BeginChild("BindingEditor", ImVec2(0, 0), true);
        RenderBindingEditor();
        ImGui::EndChild();
    }
    ImGui::End();

    // Handle drag-drop from function browser
    HandleFunctionDrop();

    // Dialogs
    if (m_showNewBindingDialog) RenderNewBindingDialog();
    if (m_showImportDialog) RenderImportExportDialog();
}

void FunctionBindingPanel::Update(float deltaTime) {
    // Update cooldown timers, etc.
}

void FunctionBindingPanel::RenderToolbar() {
    if (ImGui::Button("New Binding")) {
        CreateNewBinding();
    }
    ImGui::SameLine();

    ImGui::BeginDisabled(m_selectedBinding == nullptr);
    if (ImGui::Button("Test")) {
        if (m_selectedBinding) TestBinding(m_selectedBinding->id);
    }
    ImGui::SameLine();

    if (ImGui::Button("Duplicate")) {
        if (m_selectedBinding) DuplicateBinding(m_selectedBinding->id);
    }
    ImGui::SameLine();

    if (ImGui::Button("Delete")) {
        if (m_selectedBinding) RemoveBinding(m_selectedBinding->id);
    }
    ImGui::EndDisabled();

    // Search
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::InputTextWithHint("##Search", "Search...", m_searchBuffer, sizeof(m_searchBuffer));

    ImGui::Separator();
}

void FunctionBindingPanel::RenderBindingList() {
    std::string searchFilter = m_searchBuffer;
    std::transform(searchFilter.begin(), searchFilter.end(), searchFilter.begin(), ::tolower);

    for (auto& binding : m_bindings) {
        // Search filter
        if (!searchFilter.empty()) {
            std::string nameLower = binding.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (nameLower.find(searchFilter) == std::string::npos) continue;
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (m_selectedBinding && m_selectedBinding->id == binding.id) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        // Status indicator
        const char* statusIcon = binding.enabled ? "[+]" : "[-]";
        ImVec4 statusColor = binding.enabled ?
            ImVec4(0.3f, 0.8f, 0.3f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
        std::string label = std::string(statusIcon) + " " + binding.name + "##" + binding.id;
        ImGui::TreeNodeEx(label.c_str(), flags);
        ImGui::PopStyleColor();

        if (ImGui::IsItemClicked()) {
            SelectBinding(binding.id);
        }

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Edit")) SelectBinding(binding.id);
            if (ImGui::MenuItem(binding.enabled ? "Disable" : "Enable")) {
                binding.enabled = !binding.enabled;
            }
            if (ImGui::MenuItem("Test")) TestBinding(binding.id);
            ImGui::Separator();
            if (ImGui::MenuItem("Duplicate")) DuplicateBinding(binding.id);
            if (ImGui::MenuItem("Delete")) RemoveBinding(binding.id);
            ImGui::EndPopup();
        }

        // Tooltip
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Event: %s", GetEventTypeName(binding.eventType));
            ImGui::Text("Function: %s", binding.functionQualifiedName.c_str());
            ImGui::Text("Source: %s", binding.sourceType.c_str());
            if (!binding.description.empty()) {
                ImGui::Separator();
                ImGui::TextWrapped("%s", binding.description.c_str());
            }
            ImGui::EndTooltip();
        }
    }

    if (m_bindings.empty()) {
        ImGui::TextDisabled("No bindings created");
        ImGui::TextDisabled("Click 'New Binding' to create one");
    }
}

void FunctionBindingPanel::RenderBindingEditor() {
    if (!m_selectedBinding) {
        ImGui::TextDisabled("Select a binding to edit");
        ImGui::TextDisabled("or drag a function here to create a new binding");

        // Drop target for creating new bindings
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FUNCTION_REF")) {
                // Create new binding with dropped function
                FunctionBinding newBinding;
                newBinding.name = "New Binding";
                // Would extract function info from payload
                AddBinding(newBinding);
            }
            ImGui::EndDragDropTarget();
        }
        return;
    }

    auto& binding = *m_selectedBinding;

    // Header with name and enable toggle
    ImGui::PushItemWidth(200);
    char nameBuffer[128];
    strncpy(nameBuffer, binding.name.c_str(), sizeof(nameBuffer));
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        binding.name = nameBuffer;
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::Checkbox("Enabled", &binding.enabled);

    ImGui::Separator();

    // Tabbed sections
    if (ImGui::BeginTabBar("BindingTabs")) {
        if (ImGui::BeginTabItem("Source & Event")) {
            RenderSourceSelector();
            ImGui::Spacing();
            RenderEventSelector();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Function")) {
            RenderFunctionSelector();
            ImGui::Spacing();
            RenderParameterMappings();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Conditions")) {
            RenderConditionEditor();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Options")) {
            RenderOptionsPanel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Test")) {
            RenderTestPanel();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void FunctionBindingPanel::RenderSourceSelector() {
    auto& binding = *m_selectedBinding;

    ImGui::Text("Source Configuration");
    ImGui::Separator();

    // Source type dropdown
    static const char* sourceTypes[] = {
        "*", "player", "zombie", "worker", "soldier", "building", "projectile", "custom"
    };
    int currentSource = 0;
    for (int i = 0; i < IM_ARRAYSIZE(sourceTypes); i++) {
        if (binding.sourceType == sourceTypes[i]) {
            currentSource = i;
            break;
        }
    }

    ImGui::SetNextItemWidth(200);
    if (ImGui::Combo("Source Type", &currentSource, sourceTypes, IM_ARRAYSIZE(sourceTypes))) {
        binding.sourceType = sourceTypes[currentSource];
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("The type of entity that can trigger this binding.\n'*' means any entity.");
    }

    // Specific entity ID (optional)
    if (binding.sourceType != "*") {
        int entityId = static_cast<int>(binding.sourceEntityId);
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputInt("Specific Entity ID", &entityId)) {
            binding.sourceEntityId = static_cast<uint32_t>(std::max(0, entityId));
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Optionally restrict to a specific entity ID.\n0 = apply to all entities of this type.");
        }
    }
}

void FunctionBindingPanel::RenderEventSelector() {
    auto& binding = *m_selectedBinding;

    ImGui::Text("Event Configuration");
    ImGui::Separator();

    // Event type dropdown
    static const char* eventNames[] = {
        "OnCreate", "OnDestroy", "OnTick", "OnDamage", "OnDeath", "OnHeal",
        "OnCollision", "OnTriggerEnter", "OnTriggerExit",
        "OnAttackStart", "OnAttackHit", "OnAttackMiss", "OnKill", "OnSpellCast", "OnSpellHit", "OnAbilityUse",
        "OnBuildStart", "OnBuildComplete", "OnBuildingDestroyed", "OnProductionComplete", "OnUpgradeComplete",
        "OnResourceGather", "OnResourceDepleted", "OnTradeComplete",
        "OnLevelUp", "OnQuestComplete", "OnAchievementUnlock", "OnDialogStart", "OnDialogChoice",
        "OnDayStart", "OnNightStart", "OnSeasonChange", "OnWorldEvent",
        "Custom"
    };

    int currentEvent = static_cast<int>(binding.eventType);
    ImGui::SetNextItemWidth(200);
    if (ImGui::Combo("Event Type", &currentEvent, eventNames, IM_ARRAYSIZE(eventNames))) {
        binding.eventType = static_cast<GameEventType>(currentEvent);
    }

    // Description of selected event
    ImGui::TextDisabled("%s", GetEventDescription(binding.eventType));

    // Custom event name
    if (binding.eventType == GameEventType::Custom) {
        char customName[128];
        strncpy(customName, binding.customEventName.c_str(), sizeof(customName));
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputText("Custom Event Name", customName, sizeof(customName))) {
            binding.customEventName = customName;
        }
    }

    // Show available event data fields
    auto fields = GetEventDataFields(binding.eventType);
    if (!fields.empty()) {
        ImGui::Spacing();
        ImGui::Text("Available Event Data:");
        ImGui::BeginChild("EventFields", ImVec2(0, 60), true);
        for (const auto& field : fields) {
            ImGui::BulletText("%s", field.c_str());
        }
        ImGui::EndChild();
    }
}

void FunctionBindingPanel::RenderFunctionSelector() {
    auto& binding = *m_selectedBinding;

    ImGui::Text("Function Configuration");
    ImGui::Separator();

    // Function name
    char funcName[256];
    strncpy(funcName, binding.functionQualifiedName.c_str(), sizeof(funcName));
    ImGui::SetNextItemWidth(300);
    if (ImGui::InputText("Function", funcName, sizeof(funcName))) {
        binding.functionQualifiedName = funcName;
    }

    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        // Would open function browser
    }

    // Drop target for function
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FUNCTION_REF")) {
            // Would extract function name from payload
        }
        ImGui::EndDragDropTarget();
    }

    // Show function info if valid
    if (!binding.functionQualifiedName.empty()) {
        ImGui::TextDisabled("Function will be called when event triggers");
    }
}

void FunctionBindingPanel::RenderParameterMappings() {
    auto& binding = *m_selectedBinding;

    ImGui::Text("Parameter Mappings");
    ImGui::Separator();

    if (binding.parameterMappings.empty()) {
        ImGui::TextDisabled("No parameters to map");
        if (ImGui::Button("Add Parameter")) {
            ParameterMapping mapping;
            mapping.parameterName = "param" + std::to_string(binding.parameterMappings.size());
            binding.parameterMappings.push_back(mapping);
        }
        return;
    }

    for (size_t i = 0; i < binding.parameterMappings.size(); i++) {
        ImGui::PushID(static_cast<int>(i));
        RenderParameterMapping(binding.parameterMappings[i], static_cast<int>(i));

        // Remove button
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            binding.parameterMappings.erase(binding.parameterMappings.begin() + i);
            i--;
        }

        ImGui::PopID();
        ImGui::Separator();
    }

    if (ImGui::Button("Add Parameter")) {
        ParameterMapping mapping;
        mapping.parameterName = "param" + std::to_string(binding.parameterMappings.size());
        binding.parameterMappings.push_back(mapping);
    }
}

void FunctionBindingPanel::RenderParameterMapping(ParameterMapping& mapping, int index) {
    ImGui::Text("Parameter: %s (%s)", mapping.parameterName.c_str(), mapping.parameterType.c_str());

    // Mapping mode
    static const char* modes[] = { "Constant", "Event Data", "Entity Property", "Expression" };
    int currentMode = static_cast<int>(mapping.mode);
    ImGui::SetNextItemWidth(150);
    if (ImGui::Combo("Source##mode", &currentMode, modes, IM_ARRAYSIZE(modes))) {
        mapping.mode = static_cast<ParameterMappingMode>(currentMode);
    }

    // Value input based on mode
    switch (mapping.mode) {
        case ParameterMappingMode::Constant:
            RenderConstantValueInput(mapping);
            break;
        case ParameterMappingMode::EventData:
            RenderEventDataSelector(mapping);
            break;
        case ParameterMappingMode::EntityProperty:
            RenderEntityPropertySelector(mapping);
            break;
        case ParameterMappingMode::Expression:
            RenderExpressionInput(mapping);
            break;
    }
}

void FunctionBindingPanel::RenderConstantValueInput(ParameterMapping& mapping) {
    // Type-specific input
    if (mapping.parameterType == "int") {
        int val = std::holds_alternative<int>(mapping.constantValue) ?
                  std::get<int>(mapping.constantValue) : 0;
        if (ImGui::InputInt("Value", &val)) {
            mapping.constantValue = val;
        }
    } else if (mapping.parameterType == "float") {
        float val = std::holds_alternative<float>(mapping.constantValue) ?
                    std::get<float>(mapping.constantValue) : 0.0f;
        if (ImGui::InputFloat("Value", &val)) {
            mapping.constantValue = val;
        }
    } else if (mapping.parameterType == "bool") {
        bool val = std::holds_alternative<bool>(mapping.constantValue) ?
                   std::get<bool>(mapping.constantValue) : false;
        if (ImGui::Checkbox("Value", &val)) {
            mapping.constantValue = val;
        }
    } else {
        // String
        char buffer[256] = {0};
        if (std::holds_alternative<std::string>(mapping.constantValue)) {
            strncpy(buffer, std::get<std::string>(mapping.constantValue).c_str(), sizeof(buffer));
        }
        if (ImGui::InputText("Value", buffer, sizeof(buffer))) {
            mapping.constantValue = std::string(buffer);
        }
    }
}

void FunctionBindingPanel::RenderEventDataSelector(ParameterMapping& mapping) {
    if (m_selectedBinding) {
        auto fields = GetEventDataFields(m_selectedBinding->eventType);

        int current = 0;
        std::vector<const char*> fieldPtrs;
        for (size_t i = 0; i < fields.size(); i++) {
            fieldPtrs.push_back(fields[i].c_str());
            if (mapping.sourceField == fields[i]) current = static_cast<int>(i);
        }

        if (!fieldPtrs.empty()) {
            ImGui::SetNextItemWidth(150);
            if (ImGui::Combo("Event Field", &current, fieldPtrs.data(), static_cast<int>(fieldPtrs.size()))) {
                mapping.sourceField = fields[current];
            }
        } else {
            ImGui::TextDisabled("No event data fields available");
        }
    }
}

void FunctionBindingPanel::RenderEntityPropertySelector(ParameterMapping& mapping) {
    static const char* properties[] = {
        "position.x", "position.y", "position.z",
        "health", "maxHealth", "mana", "maxMana",
        "attack", "defense", "speed",
        "id", "type", "team"
    };

    int current = 0;
    for (int i = 0; i < IM_ARRAYSIZE(properties); i++) {
        if (mapping.entityProperty == properties[i]) {
            current = i;
            break;
        }
    }

    ImGui::SetNextItemWidth(150);
    if (ImGui::Combo("Property", &current, properties, IM_ARRAYSIZE(properties))) {
        mapping.entityProperty = properties[current];
    }
}

void FunctionBindingPanel::RenderExpressionInput(ParameterMapping& mapping) {
    char buffer[512];
    strncpy(buffer, mapping.expression.c_str(), sizeof(buffer));

    ImGui::SetNextItemWidth(300);
    if (ImGui::InputText("Expression", buffer, sizeof(buffer))) {
        mapping.expression = buffer;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Python expression to evaluate.\nAvailable variables: entity, event, game");
    }
}

void FunctionBindingPanel::RenderConditionEditor() {
    auto& binding = *m_selectedBinding;

    ImGui::Text("Execution Conditions");
    ImGui::Separator();

    // Condition mode
    ImGui::Checkbox("Require ALL conditions (AND)", &binding.requireAllConditions);
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("If checked, ALL conditions must be true.\nIf unchecked, ANY condition can be true.");
    }

    ImGui::Spacing();

    // Condition list
    for (size_t i = 0; i < binding.conditions.size(); i++) {
        ImGui::PushID(static_cast<int>(i));
        RenderCondition(binding.conditions[i], static_cast<int>(i));

        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            binding.conditions.erase(binding.conditions.begin() + i);
            i--;
        }
        ImGui::PopID();
    }

    if (ImGui::Button("Add Condition")) {
        BindingCondition condition;
        condition.leftOperand = "entity.health";
        condition.operator_ = ">";
        condition.rightOperand = "0";
        binding.conditions.push_back(condition);
    }

    if (binding.conditions.empty()) {
        ImGui::TextDisabled("No conditions - binding will always execute");
    }
}

void FunctionBindingPanel::RenderCondition(BindingCondition& condition, int index) {
    ImGui::BeginGroup();

    if (condition.useExpression) {
        // Full expression mode
        char buffer[512];
        strncpy(buffer, condition.expression.c_str(), sizeof(buffer));
        ImGui::SetNextItemWidth(400);
        if (ImGui::InputText("##expr", buffer, sizeof(buffer))) {
            condition.expression = buffer;
        }
    } else {
        // Simple comparison mode
        char leftBuffer[128], rightBuffer[128];
        strncpy(leftBuffer, condition.leftOperand.c_str(), sizeof(leftBuffer));
        strncpy(rightBuffer, condition.rightOperand.c_str(), sizeof(rightBuffer));

        ImGui::SetNextItemWidth(150);
        if (ImGui::InputText("##left", leftBuffer, sizeof(leftBuffer))) {
            condition.leftOperand = leftBuffer;
        }

        ImGui::SameLine();

        static const char* operators[] = { "==", "!=", "<", ">", "<=", ">=", "contains" };
        int currentOp = 0;
        for (int i = 0; i < IM_ARRAYSIZE(operators); i++) {
            if (condition.operator_ == operators[i]) {
                currentOp = i;
                break;
            }
        }
        ImGui::SetNextItemWidth(80);
        if (ImGui::Combo("##op", &currentOp, operators, IM_ARRAYSIZE(operators))) {
            condition.operator_ = operators[currentOp];
        }

        ImGui::SameLine();

        ImGui::SetNextItemWidth(150);
        if (ImGui::InputText("##right", rightBuffer, sizeof(rightBuffer))) {
            condition.rightOperand = rightBuffer;
        }
    }

    ImGui::SameLine();
    ImGui::Checkbox("Expr", &condition.useExpression);

    ImGui::EndGroup();
}

void FunctionBindingPanel::RenderOptionsPanel() {
    auto& binding = *m_selectedBinding;

    ImGui::Text("Execution Options");
    ImGui::Separator();

    // Priority
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Priority", &binding.priority);
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Higher priority bindings execute first.\nDefault is 0.");
    }

    // Cooldown
    ImGui::SetNextItemWidth(100);
    ImGui::InputFloat("Cooldown (s)", &binding.cooldown, 0.1f, 1.0f, "%.1f");

    // Max triggers
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Max Triggers", &binding.maxTriggers);
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("-1 = unlimited\n0 = disabled\n>0 = limited triggers");
    }

    if (binding.maxTriggers > 0) {
        ImGui::Text("Triggers remaining: %d / %d", binding.maxTriggers - binding.currentTriggers, binding.maxTriggers);
        if (ImGui::SmallButton("Reset Counter")) {
            binding.currentTriggers = 0;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Description
    ImGui::Text("Description");
    char descBuffer[512];
    strncpy(descBuffer, binding.description.c_str(), sizeof(descBuffer));
    if (ImGui::InputTextMultiline("##desc", descBuffer, sizeof(descBuffer), ImVec2(0, 60))) {
        binding.description = descBuffer;
    }
}

void FunctionBindingPanel::RenderTestPanel() {
    auto& binding = *m_selectedBinding;

    ImGui::Text("Test Binding");
    ImGui::Separator();

    ImGui::TextWrapped("Test this binding by simulating an event trigger.");
    ImGui::Spacing();

    // Test parameters
    static int testEntityId = 1;
    ImGui::InputInt("Test Entity ID", &testEntityId);

    ImGui::Spacing();

    if (ImGui::Button("Test Binding", ImVec2(150, 30))) {
        TestBinding(binding.id);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Binding status
    ImGui::Text("Binding Status:");
    ImGui::BulletText("Enabled: %s", binding.enabled ? "Yes" : "No");
    ImGui::BulletText("Triggers: %d", binding.currentTriggers);
    if (binding.maxTriggers > 0) {
        ImGui::BulletText("Remaining: %d", binding.maxTriggers - binding.currentTriggers);
    }
}

void FunctionBindingPanel::RenderNewBindingDialog() {
    ImGui::OpenPopup("New Binding");

    if (ImGui::BeginPopupModal("New Binding", &m_showNewBindingDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char nameBuffer[128] = "New Binding";
        ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer));

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            FunctionBinding binding;
            binding.name = nameBuffer;
            binding.eventType = GameEventType::OnTick;
            binding.sourceType = "*";
            AddBinding(binding);
            m_showNewBindingDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showNewBindingDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void FunctionBindingPanel::RenderImportExportDialog() {
    // Import/Export dialog implementation
}

void FunctionBindingPanel::HandleFunctionDrop() {
    // Handle function drop from browser to create binding
}

std::string FunctionBindingPanel::AddBinding(const FunctionBinding& binding) {
    FunctionBinding newBinding = binding;
    newBinding.id = GenerateBindingId();

    m_bindings.push_back(newBinding);
    m_bindingIndex[newBinding.id] = m_bindings.size() - 1;

    SelectBinding(newBinding.id);

    if (m_onBindingCreated) {
        m_onBindingCreated(newBinding);
    }

    return newBinding.id;
}

void FunctionBindingPanel::RemoveBinding(const std::string& id) {
    auto it = m_bindingIndex.find(id);
    if (it != m_bindingIndex.end()) {
        FunctionBinding removed = m_bindings[it->second];

        m_bindings.erase(m_bindings.begin() + it->second);
        m_bindingIndex.erase(it);

        // Rebuild index
        m_bindingIndex.clear();
        for (size_t i = 0; i < m_bindings.size(); i++) {
            m_bindingIndex[m_bindings[i].id] = i;
        }

        if (m_selectedBinding && m_selectedBinding->id == id) {
            m_selectedBinding = nullptr;
        }

        if (m_onBindingDeleted) {
            m_onBindingDeleted(removed);
        }
    }
}

void FunctionBindingPanel::UpdateBinding(const std::string& id, const FunctionBinding& binding) {
    auto it = m_bindingIndex.find(id);
    if (it != m_bindingIndex.end()) {
        m_bindings[it->second] = binding;

        if (m_onBindingModified) {
            m_onBindingModified(binding);
        }
    }
}

FunctionBinding* FunctionBindingPanel::GetBinding(const std::string& id) {
    auto it = m_bindingIndex.find(id);
    if (it != m_bindingIndex.end()) {
        return &m_bindings[it->second];
    }
    return nullptr;
}

const FunctionBinding* FunctionBindingPanel::GetBinding(const std::string& id) const {
    auto it = m_bindingIndex.find(id);
    if (it != m_bindingIndex.end()) {
        return &m_bindings[it->second];
    }
    return nullptr;
}

std::vector<FunctionBinding*> FunctionBindingPanel::GetBindingsForEvent(GameEventType eventType) {
    std::vector<FunctionBinding*> result;
    for (auto& binding : m_bindings) {
        if (binding.eventType == eventType && binding.enabled) {
            result.push_back(&binding);
        }
    }
    // Sort by priority
    std::sort(result.begin(), result.end(), [](FunctionBinding* a, FunctionBinding* b) {
        return a->priority > b->priority;
    });
    return result;
}

std::vector<FunctionBinding*> FunctionBindingPanel::GetBindingsForSource(const std::string& sourceType) {
    std::vector<FunctionBinding*> result;
    for (auto& binding : m_bindings) {
        if ((binding.sourceType == "*" || binding.sourceType == sourceType) && binding.enabled) {
            result.push_back(&binding);
        }
    }
    return result;
}

void FunctionBindingPanel::ClearBindings() {
    m_bindings.clear();
    m_bindingIndex.clear();
    m_selectedBinding = nullptr;
}

void FunctionBindingPanel::SelectBinding(const std::string& id) {
    m_selectedBinding = GetBinding(id);
}

void FunctionBindingPanel::ClearSelection() {
    m_selectedBinding = nullptr;
}

void FunctionBindingPanel::SetBindingEnabled(const std::string& id, bool enabled) {
    if (auto* binding = GetBinding(id)) {
        binding->enabled = enabled;
    }
}

bool FunctionBindingPanel::TestBinding(const std::string& id) {
    auto* binding = GetBinding(id);
    if (!binding) return false;

    // Would execute the binding function with test data
    return true;
}

std::string FunctionBindingPanel::DuplicateBinding(const std::string& id) {
    auto* original = GetBinding(id);
    if (!original) return "";

    FunctionBinding copy = *original;
    copy.name = copy.name + " (Copy)";
    copy.currentTriggers = 0;

    return AddBinding(copy);
}

bool FunctionBindingPanel::SaveBindings(const std::string& filePath) {
    json j = json::array();

    for (const auto& binding : m_bindings) {
        json b;
        b["id"] = binding.id;
        b["name"] = binding.name;
        b["sourceType"] = binding.sourceType;
        b["sourceEntityId"] = binding.sourceEntityId;
        b["eventType"] = static_cast<int>(binding.eventType);
        b["customEventName"] = binding.customEventName;
        b["functionQualifiedName"] = binding.functionQualifiedName;
        b["enabled"] = binding.enabled;
        b["priority"] = binding.priority;
        b["cooldown"] = binding.cooldown;
        b["maxTriggers"] = binding.maxTriggers;
        b["description"] = binding.description;

        // Parameter mappings
        b["parameterMappings"] = json::array();
        for (const auto& pm : binding.parameterMappings) {
            json pmj;
            pmj["parameterName"] = pm.parameterName;
            pmj["parameterType"] = pm.parameterType;
            pmj["mode"] = static_cast<int>(pm.mode);
            pmj["sourceField"] = pm.sourceField;
            pmj["entityProperty"] = pm.entityProperty;
            pmj["expression"] = pm.expression;
            b["parameterMappings"].push_back(pmj);
        }

        // Conditions
        b["conditions"] = json::array();
        for (const auto& cond : binding.conditions) {
            json cj;
            cj["leftOperand"] = cond.leftOperand;
            cj["operator"] = cond.operator_;
            cj["rightOperand"] = cond.rightOperand;
            cj["useExpression"] = cond.useExpression;
            cj["expression"] = cond.expression;
            b["conditions"].push_back(cj);
        }
        b["requireAllConditions"] = binding.requireAllConditions;

        j.push_back(b);
    }

    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << j.dump(2);
    return true;
}

bool FunctionBindingPanel::LoadBindings(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    json j;
    try {
        file >> j;
    } catch (...) {
        return false;
    }

    ClearBindings();

    for (const auto& b : j) {
        FunctionBinding binding;
        binding.id = b.value("id", GenerateBindingId());
        binding.name = b.value("name", "Unnamed");
        binding.sourceType = b.value("sourceType", "*");
        binding.sourceEntityId = b.value("sourceEntityId", 0);
        binding.eventType = static_cast<GameEventType>(b.value("eventType", 0));
        binding.customEventName = b.value("customEventName", "");
        binding.functionQualifiedName = b.value("functionQualifiedName", "");
        binding.enabled = b.value("enabled", true);
        binding.priority = b.value("priority", 0);
        binding.cooldown = b.value("cooldown", 0.0f);
        binding.maxTriggers = b.value("maxTriggers", -1);
        binding.description = b.value("description", "");
        binding.requireAllConditions = b.value("requireAllConditions", true);

        // Load parameter mappings
        if (b.contains("parameterMappings")) {
            for (const auto& pmj : b["parameterMappings"]) {
                ParameterMapping pm;
                pm.parameterName = pmj.value("parameterName", "");
                pm.parameterType = pmj.value("parameterType", "any");
                pm.mode = static_cast<ParameterMappingMode>(pmj.value("mode", 0));
                pm.sourceField = pmj.value("sourceField", "");
                pm.entityProperty = pmj.value("entityProperty", "");
                pm.expression = pmj.value("expression", "");
                binding.parameterMappings.push_back(pm);
            }
        }

        // Load conditions
        if (b.contains("conditions")) {
            for (const auto& cj : b["conditions"]) {
                BindingCondition cond;
                cond.leftOperand = cj.value("leftOperand", "");
                cond.operator_ = cj.value("operator", "==");
                cond.rightOperand = cj.value("rightOperand", "");
                cond.useExpression = cj.value("useExpression", false);
                cond.expression = cj.value("expression", "");
                binding.conditions.push_back(cond);
            }
        }

        m_bindings.push_back(binding);
        m_bindingIndex[binding.id] = m_bindings.size() - 1;
    }

    return true;
}

std::string FunctionBindingPanel::ExportBinding(const std::string& id) const {
    auto* binding = GetBinding(id);
    if (!binding) return "";

    json j;
    j["name"] = binding->name;
    j["function"] = binding->functionQualifiedName;
    j["event"] = GetEventTypeName(binding->eventType);
    j["source"] = binding->sourceType;

    return j.dump(2);
}

std::string FunctionBindingPanel::ImportBinding(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        FunctionBinding binding;
        binding.name = j.value("name", "Imported Binding");
        binding.functionQualifiedName = j.value("function", "");
        binding.sourceType = j.value("source", "*");

        std::string eventName = j.value("event", "OnTick");
        binding.eventType = ParseEventType(eventName);

        return AddBinding(binding);
    } catch (...) {
        return "";
    }
}

void FunctionBindingPanel::CreateNewBinding() {
    m_showNewBindingDialog = true;
}

std::string FunctionBindingPanel::GenerateBindingId() {
    return "binding_" + std::to_string(m_nextBindingId++);
}

const char* FunctionBindingPanel::GetEventTypeName(GameEventType type) {
    switch (type) {
        case GameEventType::OnCreate: return "OnCreate";
        case GameEventType::OnDestroy: return "OnDestroy";
        case GameEventType::OnTick: return "OnTick";
        case GameEventType::OnDamage: return "OnDamage";
        case GameEventType::OnDeath: return "OnDeath";
        case GameEventType::OnHeal: return "OnHeal";
        case GameEventType::OnCollision: return "OnCollision";
        case GameEventType::OnTriggerEnter: return "OnTriggerEnter";
        case GameEventType::OnTriggerExit: return "OnTriggerExit";
        case GameEventType::OnAttackStart: return "OnAttackStart";
        case GameEventType::OnAttackHit: return "OnAttackHit";
        case GameEventType::OnAttackMiss: return "OnAttackMiss";
        case GameEventType::OnKill: return "OnKill";
        case GameEventType::OnSpellCast: return "OnSpellCast";
        case GameEventType::OnSpellHit: return "OnSpellHit";
        case GameEventType::OnAbilityUse: return "OnAbilityUse";
        case GameEventType::OnBuildStart: return "OnBuildStart";
        case GameEventType::OnBuildComplete: return "OnBuildComplete";
        case GameEventType::OnBuildingDestroyed: return "OnBuildingDestroyed";
        case GameEventType::OnProductionComplete: return "OnProductionComplete";
        case GameEventType::OnUpgradeComplete: return "OnUpgradeComplete";
        case GameEventType::OnResourceGather: return "OnResourceGather";
        case GameEventType::OnResourceDepleted: return "OnResourceDepleted";
        case GameEventType::OnTradeComplete: return "OnTradeComplete";
        case GameEventType::OnLevelUp: return "OnLevelUp";
        case GameEventType::OnQuestComplete: return "OnQuestComplete";
        case GameEventType::OnAchievementUnlock: return "OnAchievementUnlock";
        case GameEventType::OnDialogStart: return "OnDialogStart";
        case GameEventType::OnDialogChoice: return "OnDialogChoice";
        case GameEventType::OnDayStart: return "OnDayStart";
        case GameEventType::OnNightStart: return "OnNightStart";
        case GameEventType::OnSeasonChange: return "OnSeasonChange";
        case GameEventType::OnWorldEvent: return "OnWorldEvent";
        case GameEventType::Custom: return "Custom";
        default: return "Unknown";
    }
}

GameEventType FunctionBindingPanel::ParseEventType(const std::string& name) {
    static const std::unordered_map<std::string, GameEventType> eventMap = {
        {"OnCreate", GameEventType::OnCreate},
        {"OnDestroy", GameEventType::OnDestroy},
        {"OnTick", GameEventType::OnTick},
        {"OnDamage", GameEventType::OnDamage},
        {"OnDeath", GameEventType::OnDeath},
        {"OnHeal", GameEventType::OnHeal},
        {"OnCollision", GameEventType::OnCollision},
        {"OnTriggerEnter", GameEventType::OnTriggerEnter},
        {"OnTriggerExit", GameEventType::OnTriggerExit},
        {"OnAttackStart", GameEventType::OnAttackStart},
        {"OnAttackHit", GameEventType::OnAttackHit},
        {"OnAttackMiss", GameEventType::OnAttackMiss},
        {"OnKill", GameEventType::OnKill},
        {"OnSpellCast", GameEventType::OnSpellCast},
        {"OnSpellHit", GameEventType::OnSpellHit},
        {"OnAbilityUse", GameEventType::OnAbilityUse},
        {"OnBuildStart", GameEventType::OnBuildStart},
        {"OnBuildComplete", GameEventType::OnBuildComplete},
        {"OnBuildingDestroyed", GameEventType::OnBuildingDestroyed},
        {"OnProductionComplete", GameEventType::OnProductionComplete},
        {"OnUpgradeComplete", GameEventType::OnUpgradeComplete},
        {"OnResourceGather", GameEventType::OnResourceGather},
        {"OnResourceDepleted", GameEventType::OnResourceDepleted},
        {"OnTradeComplete", GameEventType::OnTradeComplete},
        {"OnLevelUp", GameEventType::OnLevelUp},
        {"OnQuestComplete", GameEventType::OnQuestComplete},
        {"OnAchievementUnlock", GameEventType::OnAchievementUnlock},
        {"OnDialogStart", GameEventType::OnDialogStart},
        {"OnDialogChoice", GameEventType::OnDialogChoice},
        {"OnDayStart", GameEventType::OnDayStart},
        {"OnNightStart", GameEventType::OnNightStart},
        {"OnSeasonChange", GameEventType::OnSeasonChange},
        {"OnWorldEvent", GameEventType::OnWorldEvent},
    };

    auto it = eventMap.find(name);
    return it != eventMap.end() ? it->second : GameEventType::Custom;
}

std::vector<std::string> FunctionBindingPanel::GetEventDataFields(GameEventType eventType) {
    switch (eventType) {
        case GameEventType::OnDamage:
            return {"damage", "sourceId", "damageType", "isCritical"};
        case GameEventType::OnDeath:
            return {"killerId", "damageType", "position"};
        case GameEventType::OnHeal:
            return {"healAmount", "sourceId", "healType"};
        case GameEventType::OnCollision:
            return {"otherId", "otherType", "contactPoint", "normal"};
        case GameEventType::OnAttackHit:
            return {"targetId", "damage", "isCritical"};
        case GameEventType::OnSpellCast:
            return {"spellId", "spellName", "targetId", "manaCost"};
        case GameEventType::OnBuildComplete:
            return {"buildingId", "buildingType", "position"};
        case GameEventType::OnResourceGather:
            return {"resourceType", "amount", "sourceId"};
        case GameEventType::OnLevelUp:
            return {"newLevel", "previousLevel", "experienceGained"};
        case GameEventType::OnTick:
            return {"deltaTime", "gameTime"};
        default:
            return {"entityId", "entityType", "position"};
    }
}

std::string FunctionBindingPanel::GetEventDescription(GameEventType eventType) {
    switch (eventType) {
        case GameEventType::OnCreate: return "Triggered when entity is spawned";
        case GameEventType::OnDestroy: return "Triggered when entity is removed";
        case GameEventType::OnTick: return "Triggered every frame";
        case GameEventType::OnDamage: return "Triggered when entity takes damage";
        case GameEventType::OnDeath: return "Triggered when entity dies";
        case GameEventType::OnHeal: return "Triggered when entity is healed";
        case GameEventType::OnCollision: return "Triggered on physics collision";
        case GameEventType::OnAttackHit: return "Triggered when attack lands";
        case GameEventType::OnSpellCast: return "Triggered when spell is cast";
        case GameEventType::OnBuildComplete: return "Triggered when building finishes";
        case GameEventType::OnLevelUp: return "Triggered when entity levels up";
        default: return "Game event trigger";
    }
}

} // namespace Vehement
