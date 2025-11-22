#include "EventBindingEditor.hpp"
#include <imgui.h>
#include <algorithm>
#include <sstream>

namespace Vehement {
namespace Editor {

EventBindingEditor::EventBindingEditor() = default;
EventBindingEditor::~EventBindingEditor() = default;

void EventBindingEditor::Initialize(Nova::Events::EventBindingManager& manager, const Config& config) {
    m_manager = &manager;
    m_config = config;
    m_initialized = true;

    // Initialize cached data
    m_availableEventTypes = {
        "OnDamage", "OnDeath", "OnSpawn", "OnHeal", "OnLevelUp",
        "OnKill", "OnMove", "OnAttack", "OnCreate", "OnDestroy",
        "OnHealthChanged", "OnExperienceGained", "OnTechResearched",
        "OnBuildingPlaced", "OnBuildingComplete", "OnResourceGathered",
        "OnResourceDepleted", "OnTileChanged", "OnFogRevealed"
    };

    m_availableSourceTypes = {
        "*", "Unit", "Building", "Resource", "Tile", "Player", "World"
    };

    m_availableCategories = {
        "Combat", "Progression", "World", "AI", "UI", "Debug"
    };
}

void EventBindingEditor::Shutdown() {
    if (m_hasUnsavedChanges && m_config.autoSave) {
        SaveBindings();
    }
    m_initialized = false;
}

void EventBindingEditor::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update notification timer
    if (m_notificationTimer > 0.0f) {
        m_notificationTimer -= deltaTime;
    }

    // Auto-save
    if (m_config.autoSave && m_hasUnsavedChanges) {
        m_autoSaveTimer += deltaTime;
        if (m_autoSaveTimer >= m_config.autoSaveIntervalSeconds) {
            SaveBindings();
            m_autoSaveTimer = 0.0f;
        }
    }
}

void EventBindingEditor::Render() {
    if (!m_initialized || !m_visible) return;

    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Event Binding Editor", &m_visible, ImGuiWindowFlags_MenuBar)) {
        RenderMenuBar();
        RenderToolbar();

        // Split view: list on left, details on right
        ImGui::Columns(2, "binding_columns", true);
        ImGui::SetColumnWidth(0, 300);

        RenderBindingList();
        ImGui::NextColumn();
        RenderBindingDetails();

        ImGui::Columns(1);
        RenderStatusBar();
    }
    ImGui::End();

    // Render popups
    if (m_showNewBindingWizard) {
        RenderNewBindingWizard();
    }
    if (m_showImportExport) {
        RenderImportExportDialog();
    }
    if (m_showPythonEditor) {
        RenderPythonEditor();
    }
    if (m_showTestResults) {
        RenderTestResults();
    }
}

void EventBindingEditor::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Binding", "Ctrl+N")) {
                CreateNewBinding();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import...", "Ctrl+I")) {
                m_showImportExport = true;
                m_isImporting = true;
            }
            if (ImGui::MenuItem("Export...", "Ctrl+E")) {
                m_showImportExport = true;
                m_isImporting = false;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save All", "Ctrl+S")) {
                SaveBindings();
            }
            if (ImGui::MenuItem("Reload All", "Ctrl+R")) {
                ReloadBindings();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, !m_selectedBindingId.empty())) {
                DuplicateBinding(m_selectedBindingId);
            }
            if (ImGui::MenuItem("Delete", "Delete", false, !m_selectedBindingId.empty())) {
                DeleteBinding(m_selectedBindingId);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) {
                // Select all for export
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Debug Info", nullptr, &m_config.showDebugInfo);
            ImGui::MenuItem("Show Enabled Only", nullptr, &m_showEnabledOnly);
            ImGui::MenuItem("Show Disabled Only", nullptr, &m_showDisabledOnly);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Test Selected", "F5", false, !m_selectedBindingId.empty())) {
                TestBinding(m_selectedBindingId);
            }
            if (ImGui::MenuItem("Validate All")) {
                auto errors = m_manager->ValidateAllBindings();
                if (errors.empty()) {
                    ShowNotification("All bindings are valid");
                } else {
                    ShowNotification(std::to_string(errors.size()) + " binding(s) have errors", true);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Open Python Editor")) {
                m_showPythonEditor = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void EventBindingEditor::RenderToolbar() {
    if (ImGui::Button("+ New")) {
        CreateNewBinding();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        ReloadBindings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        SaveBindings();
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Filter
    ImGui::SetNextItemWidth(150);
    ImGui::InputTextWithHint("##filter", "Filter...", &m_filterText[0], m_filterText.capacity());

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::BeginCombo("##category", m_filterCategory.empty() ? "All Categories" : m_filterCategory.c_str())) {
        if (ImGui::Selectable("All Categories", m_filterCategory.empty())) {
            m_filterCategory.clear();
        }
        for (const auto& cat : m_availableCategories) {
            if (ImGui::Selectable(cat.c_str(), m_filterCategory == cat)) {
                m_filterCategory = cat;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();
}

void EventBindingEditor::RenderBindingList() {
    ImGui::Text("Bindings (%zu)", m_manager->GetBindingCount());
    ImGui::Separator();

    ImGui::BeginChild("binding_list", ImVec2(0, -30));

    auto bindings = GetFilteredBindings();

    for (const auto* binding : bindings) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (binding->id == m_selectedBindingId) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        // Color based on state
        if (!binding->enabled) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        } else if (binding->hasError) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        }

        ImGui::TreeNodeEx(binding->id.c_str(), flags, "%s %s",
            binding->enabled ? "[ON]" : "[OFF]",
            binding->GetDisplayName().c_str());

        if (!binding->enabled || binding->hasError) {
            ImGui::PopStyleColor();
        }

        if (ImGui::IsItemClicked()) {
            SelectBinding(binding->id);
        }

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Test")) {
                TestBinding(binding->id);
            }
            if (ImGui::MenuItem(binding->enabled ? "Disable" : "Enable")) {
                m_manager->SetBindingEnabled(binding->id, !binding->enabled);
                m_hasUnsavedChanges = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Duplicate")) {
                DuplicateBinding(binding->id);
            }
            if (ImGui::MenuItem("Delete")) {
                DeleteBinding(binding->id);
            }
            ImGui::EndPopup();
        }

        // Tooltip
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("ID: %s", binding->id.c_str());
            ImGui::Text("Event: %s", binding->condition.eventName.c_str());
            ImGui::Text("Executions: %d", binding->executionCount.load());
            if (binding->hasError) {
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error: %s", binding->lastError.c_str());
            }
            ImGui::EndTooltip();
        }
    }

    ImGui::EndChild();

    // Summary
    ImGui::Text("Showing %zu of %zu", bindings.size(), m_manager->GetBindingCount());
}

void EventBindingEditor::RenderBindingDetails() {
    if (m_selectedBindingId.empty()) {
        ImGui::TextDisabled("Select a binding to edit");
        return;
    }

    auto* binding = m_manager->GetBinding(m_selectedBindingId);
    if (!binding) {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Binding not found");
        return;
    }

    ImGui::BeginChild("binding_details");

    // Header
    ImGui::Text("Binding: %s", binding->id.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
    if (ImGui::Button("Test")) {
        TestBinding(binding->id);
    }

    ImGui::Separator();

    // Basic info
    if (ImGui::CollapsingHeader("Basic Info", ImGuiTreeNodeFlags_DefaultOpen)) {
        char nameBuffer[256];
        strncpy(nameBuffer, binding->name.c_str(), sizeof(nameBuffer));
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
            binding->name = nameBuffer;
            m_hasUnsavedChanges = true;
        }

        char descBuffer[512];
        strncpy(descBuffer, binding->description.c_str(), sizeof(descBuffer));
        if (ImGui::InputTextMultiline("Description", descBuffer, sizeof(descBuffer), ImVec2(0, 60))) {
            binding->description = descBuffer;
            m_hasUnsavedChanges = true;
        }

        if (ImGui::BeginCombo("Category", binding->category.c_str())) {
            for (const auto& cat : m_availableCategories) {
                if (ImGui::Selectable(cat.c_str(), binding->category == cat)) {
                    binding->category = cat;
                    m_hasUnsavedChanges = true;
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Checkbox("Enabled", &binding->enabled)) {
            m_hasUnsavedChanges = true;
        }
    }

    // Condition
    if (ImGui::CollapsingHeader("Condition", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderConditionEditor(binding->condition);
    }

    // Callback
    if (ImGui::CollapsingHeader("Callback", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderCallbackEditor(*binding);
    }

    // Execution settings
    if (ImGui::CollapsingHeader("Execution Settings")) {
        if (ImGui::SliderInt("Priority", &binding->priority, -100, 100)) {
            m_hasUnsavedChanges = true;
        }
        if (ImGui::Checkbox("Async", &binding->async)) {
            m_hasUnsavedChanges = true;
        }
        if (ImGui::InputFloat("Delay (s)", &binding->delay, 0.1f, 1.0f)) {
            m_hasUnsavedChanges = true;
        }
        if (ImGui::InputFloat("Cooldown (s)", &binding->cooldown, 0.1f, 1.0f)) {
            m_hasUnsavedChanges = true;
        }
        if (ImGui::InputInt("Max Executions", &binding->maxExecutions)) {
            m_hasUnsavedChanges = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(-1 = unlimited)");
        if (ImGui::Checkbox("One Shot", &binding->oneShot)) {
            m_hasUnsavedChanges = true;
        }
    }

    // Debug info
    if (m_config.showDebugInfo && ImGui::CollapsingHeader("Debug Info")) {
        ImGui::Text("Execution Count: %d", binding->executionCount.load());
        if (binding->hasError) {
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Last Error: %s", binding->lastError.c_str());
        }
        if (ImGui::Checkbox("Log Execution", &binding->logExecution)) {
            m_hasUnsavedChanges = true;
        }
    }

    ImGui::EndChild();
}

void EventBindingEditor::RenderConditionEditor(Nova::Events::EventCondition& condition) {
    RenderSourceTypeSelector(condition);
    RenderEventNameSelector(condition);
    RenderPropertyCondition(condition);

    ImGui::Separator();
    ImGui::Text("Advanced Conditions");

    // Python condition
    char pythonBuffer[1024];
    strncpy(pythonBuffer, condition.pythonCondition.c_str(), sizeof(pythonBuffer));
    if (ImGui::InputTextMultiline("Python Condition", pythonBuffer, sizeof(pythonBuffer), ImVec2(0, 60))) {
        condition.pythonCondition = pythonBuffer;
        m_hasUnsavedChanges = true;
    }

    if (ImGui::Checkbox("Negate Condition", &condition.negate)) {
        m_hasUnsavedChanges = true;
    }
}

void EventBindingEditor::RenderSourceTypeSelector(Nova::Events::EventCondition& condition) {
    if (ImGui::BeginCombo("Source Type", condition.sourceType.c_str())) {
        for (const auto& type : m_availableSourceTypes) {
            if (ImGui::Selectable(type.c_str(), condition.sourceType == type)) {
                condition.sourceType = type;
                m_hasUnsavedChanges = true;
            }
        }
        ImGui::EndCombo();
    }
}

void EventBindingEditor::RenderEventNameSelector(Nova::Events::EventCondition& condition) {
    if (ImGui::BeginCombo("Event Name", condition.eventName.c_str())) {
        for (const auto& event : m_availableEventTypes) {
            if (ImGui::Selectable(event.c_str(), condition.eventName == event)) {
                condition.eventName = event;
                m_hasUnsavedChanges = true;
            }
        }
        ImGui::EndCombo();
    }
}

void EventBindingEditor::RenderPropertyCondition(Nova::Events::EventCondition& condition) {
    ImGui::Text("Property Condition (optional)");

    char pathBuffer[256];
    strncpy(pathBuffer, condition.propertyPath.c_str(), sizeof(pathBuffer));
    if (ImGui::InputText("Property Path", pathBuffer, sizeof(pathBuffer))) {
        condition.propertyPath = pathBuffer;
        m_hasUnsavedChanges = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("e.g., health.current, position.x");
    }

    if (!condition.propertyPath.empty()) {
        RenderComparatorSelector(condition.comparator);
        RenderValueEditor(condition.compareValue, "Compare Value");
    }
}

void EventBindingEditor::RenderComparatorSelector(Nova::Events::Comparator& comparator) {
    const char* comparators[] = {
        "==", "!=", "<", "<=", ">", ">=", "changed", "contains", "inRange"
    };
    int current = static_cast<int>(comparator);

    if (ImGui::Combo("Comparator", &current, comparators, IM_ARRAYSIZE(comparators))) {
        comparator = static_cast<Nova::Events::Comparator>(current);
        m_hasUnsavedChanges = true;
    }
}

void EventBindingEditor::RenderValueEditor(Nova::Events::ConditionValue& value, const std::string& label) {
    // Determine current type
    int typeIndex = 0;
    if (std::holds_alternative<bool>(value)) typeIndex = 1;
    else if (std::holds_alternative<int>(value)) typeIndex = 2;
    else if (std::holds_alternative<double>(value)) typeIndex = 3;
    else if (std::holds_alternative<std::string>(value)) typeIndex = 4;

    const char* types[] = {"null", "bool", "int", "float", "string"};
    if (ImGui::Combo((label + " Type").c_str(), &typeIndex, types, IM_ARRAYSIZE(types))) {
        switch (typeIndex) {
            case 0: value = std::monostate{}; break;
            case 1: value = false; break;
            case 2: value = 0; break;
            case 3: value = 0.0; break;
            case 4: value = std::string(""); break;
        }
        m_hasUnsavedChanges = true;
    }

    // Value editor based on type
    if (auto* b = std::get_if<bool>(&value)) {
        if (ImGui::Checkbox(label.c_str(), b)) {
            m_hasUnsavedChanges = true;
        }
    } else if (auto* i = std::get_if<int>(&value)) {
        if (ImGui::InputInt(label.c_str(), i)) {
            m_hasUnsavedChanges = true;
        }
    } else if (auto* d = std::get_if<double>(&value)) {
        float f = static_cast<float>(*d);
        if (ImGui::InputFloat(label.c_str(), &f)) {
            *d = static_cast<double>(f);
            m_hasUnsavedChanges = true;
        }
    } else if (auto* s = std::get_if<std::string>(&value)) {
        char buffer[256];
        strncpy(buffer, s->c_str(), sizeof(buffer));
        if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer))) {
            *s = buffer;
            m_hasUnsavedChanges = true;
        }
    }
}

void EventBindingEditor::RenderCallbackEditor(Nova::Events::EventBinding& binding) {
    const char* types[] = {"Python", "Native", "Event", "Command", "Script"};
    int currentType = static_cast<int>(binding.callbackType);

    if (ImGui::Combo("Callback Type", &currentType, types, IM_ARRAYSIZE(types))) {
        binding.callbackType = static_cast<Nova::Events::CallbackType>(currentType);
        m_hasUnsavedChanges = true;
    }

    switch (binding.callbackType) {
        case Nova::Events::CallbackType::Python: {
            char moduleBuffer[256];
            strncpy(moduleBuffer, binding.pythonModule.c_str(), sizeof(moduleBuffer));
            if (ImGui::InputText("Module", moduleBuffer, sizeof(moduleBuffer))) {
                binding.pythonModule = moduleBuffer;
                m_hasUnsavedChanges = true;
            }

            char funcBuffer[256];
            strncpy(funcBuffer, binding.pythonFunction.c_str(), sizeof(funcBuffer));
            if (ImGui::InputText("Function", funcBuffer, sizeof(funcBuffer))) {
                binding.pythonFunction = funcBuffer;
                m_hasUnsavedChanges = true;
            }

            ImGui::Text("Or inline script:");
            char scriptBuffer[2048];
            strncpy(scriptBuffer, binding.pythonScript.c_str(), sizeof(scriptBuffer));
            if (ImGui::InputTextMultiline("Script", scriptBuffer, sizeof(scriptBuffer), ImVec2(0, 100))) {
                binding.pythonScript = scriptBuffer;
                m_hasUnsavedChanges = true;
            }

            if (ImGui::Button("Open in Python Editor")) {
                m_pythonEditorContent = binding.pythonScript;
                m_showPythonEditor = true;
            }
            break;
        }

        case Nova::Events::CallbackType::Event: {
            char eventBuffer[256];
            strncpy(eventBuffer, binding.emitEventType.c_str(), sizeof(eventBuffer));
            if (ImGui::InputText("Emit Event Type", eventBuffer, sizeof(eventBuffer))) {
                binding.emitEventType = eventBuffer;
                m_hasUnsavedChanges = true;
            }
            break;
        }

        case Nova::Events::CallbackType::Command: {
            char cmdBuffer[512];
            strncpy(cmdBuffer, binding.command.c_str(), sizeof(cmdBuffer));
            if (ImGui::InputText("Command", cmdBuffer, sizeof(cmdBuffer))) {
                binding.command = cmdBuffer;
                m_hasUnsavedChanges = true;
            }
            break;
        }

        default:
            ImGui::TextDisabled("Configure callback settings");
            break;
    }
}

void EventBindingEditor::RenderNewBindingWizard() {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("New Binding Wizard", &m_showNewBindingWizard)) {
        ImGui::Text("Step %d of 3", m_wizardStep + 1);
        ImGui::ProgressBar((m_wizardStep + 1) / 3.0f);
        ImGui::Separator();

        switch (m_wizardStep) {
            case 0: // Basic info
                ImGui::Text("Enter basic information:");
                {
                    char buffer[256];
                    strncpy(buffer, m_newBinding.name.c_str(), sizeof(buffer));
                    ImGui::InputText("Name", buffer, sizeof(buffer));
                    m_newBinding.name = buffer;
                }
                if (ImGui::BeginCombo("Category", m_newBinding.category.c_str())) {
                    for (const auto& cat : m_availableCategories) {
                        if (ImGui::Selectable(cat.c_str())) {
                            m_newBinding.category = cat;
                        }
                    }
                    ImGui::EndCombo();
                }
                break;

            case 1: // Condition
                ImGui::Text("Define when this binding triggers:");
                RenderConditionEditor(m_newBinding.condition);
                break;

            case 2: // Callback
                ImGui::Text("Define what happens:");
                RenderCallbackEditor(m_newBinding);
                break;
        }

        ImGui::Separator();

        if (m_wizardStep > 0) {
            if (ImGui::Button("Previous")) {
                m_wizardStep--;
            }
            ImGui::SameLine();
        }

        if (m_wizardStep < 2) {
            if (ImGui::Button("Next")) {
                m_wizardStep++;
            }
        } else {
            if (ImGui::Button("Create")) {
                std::string id = m_manager->AddBindingAuto(m_newBinding);
                if (!id.empty()) {
                    SelectBinding(id);
                    ShowNotification("Binding created: " + id);
                    m_hasUnsavedChanges = true;
                }
                m_showNewBindingWizard = false;
                m_wizardStep = 0;
                m_newBinding = Nova::Events::EventBinding();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_showNewBindingWizard = false;
            m_wizardStep = 0;
            m_newBinding = Nova::Events::EventBinding();
        }
    }
    ImGui::End();
}

void EventBindingEditor::RenderPythonEditor() {
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Python Script Editor", &m_showPythonEditor)) {
        char buffer[8192];
        strncpy(buffer, m_pythonEditorContent.c_str(), sizeof(buffer));

        if (ImGui::InputTextMultiline("##python_editor", buffer, sizeof(buffer),
            ImVec2(-1, -50), ImGuiInputTextFlags_AllowTabInput)) {
            m_pythonEditorContent = buffer;
            m_pythonEditorDirty = true;
        }

        if (ImGui::Button("Apply to Binding") && m_editingBinding) {
            m_editingBinding->pythonScript = m_pythonEditorContent;
            m_hasUnsavedChanges = true;
            m_pythonEditorDirty = false;
            ShowNotification("Script applied");
        }

        ImGui::SameLine();
        if (ImGui::Button("Test Execute")) {
            // Would integrate with Python engine
            ShowNotification("Script test not implemented yet", true);
        }

        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            m_showPythonEditor = false;
        }
    }
    ImGui::End();
}

void EventBindingEditor::RenderImportExportDialog() {
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(m_isImporting ? "Import Bindings" : "Export Bindings", &m_showImportExport)) {
        char pathBuffer[512];
        strncpy(pathBuffer, m_importExportPath.c_str(), sizeof(pathBuffer));
        if (ImGui::InputText("File Path", pathBuffer, sizeof(pathBuffer))) {
            m_importExportPath = pathBuffer;
        }

        if (m_isImporting) {
            if (ImGui::Button("Import")) {
                ImportBindings(m_importExportPath);
                m_showImportExport = false;
            }
        } else {
            if (ImGui::Button("Export All")) {
                ExportBindings(m_importExportPath, {});
                m_showImportExport = false;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_showImportExport = false;
        }
    }
    ImGui::End();
}

void EventBindingEditor::RenderTestResults() {
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Test Results", &m_showTestResults)) {
        ImGui::Text("Binding: %s", m_testResultBindingId.c_str());
        if (m_testResultSuccess) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "SUCCESS");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "FAILED");
        }
        ImGui::TextWrapped("%s", m_testResultMessage.c_str());
    }
    ImGui::End();
}

void EventBindingEditor::RenderStatusBar() {
    ImGui::Separator();

    if (m_hasUnsavedChanges) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "* Unsaved changes");
    } else {
        ImGui::Text("Ready");
    }

    if (m_notificationTimer > 0.0f) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 300);
        if (m_notificationIsError) {
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", m_notificationMessage.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.3f, 1, 0.3f, 1), "%s", m_notificationMessage.c_str());
        }
    }
}

// ============================================================================
// Actions
// ============================================================================

void EventBindingEditor::CreateNewBinding() {
    m_newBinding = Nova::Events::EventBinding();
    m_wizardStep = 0;
    m_showNewBindingWizard = true;
}

void EventBindingEditor::SelectBinding(const std::string& bindingId) {
    m_selectedBindingId = bindingId;
    m_editingBinding = m_manager->GetBinding(bindingId);

    if (OnBindingSelected) {
        OnBindingSelected(bindingId);
    }

    // Update recent list
    auto it = std::find(m_recentBindings.begin(), m_recentBindings.end(), bindingId);
    if (it != m_recentBindings.end()) {
        m_recentBindings.erase(it);
    }
    m_recentBindings.insert(m_recentBindings.begin(), bindingId);
    if (m_recentBindings.size() > static_cast<size_t>(m_config.maxRecentBindings)) {
        m_recentBindings.pop_back();
    }
}

void EventBindingEditor::ClearSelection() {
    m_selectedBindingId.clear();
    m_editingBinding = nullptr;
}

void EventBindingEditor::DuplicateBinding(const std::string& bindingId) {
    const auto* original = m_manager->GetBinding(bindingId);
    if (!original) return;

    Nova::Events::EventBinding copy = *original;
    copy.id = "";
    copy.name = original->name + " (copy)";
    copy.executionCount = 0;
    copy.hasError = false;

    std::string newId = m_manager->AddBindingAuto(copy);
    if (!newId.empty()) {
        SelectBinding(newId);
        ShowNotification("Binding duplicated: " + newId);
        m_hasUnsavedChanges = true;
    }
}

void EventBindingEditor::DeleteBinding(const std::string& bindingId) {
    if (m_manager->RemoveBinding(bindingId)) {
        if (m_selectedBindingId == bindingId) {
            ClearSelection();
        }
        ShowNotification("Binding deleted: " + bindingId);
        m_hasUnsavedChanges = true;

        if (OnBindingDeleted) {
            OnBindingDeleted(bindingId);
        }
    }
}

void EventBindingEditor::TestBinding(const std::string& bindingId) {
    bool success = m_manager->TestBinding(bindingId);
    m_testResultBindingId = bindingId;
    m_testResultSuccess = success;
    m_testResultMessage = success ? "Binding is valid" : "Binding validation failed";
    m_showTestResults = true;
}

void EventBindingEditor::SaveBindings() {
    std::string path = m_config.defaultBindingsPath + "/editor_bindings.json";
    if (m_manager->SaveBindingsToFile(path, {})) {
        ShowNotification("Bindings saved");
        m_hasUnsavedChanges = false;
    } else {
        ShowNotification("Failed to save bindings", true);
    }
}

void EventBindingEditor::ReloadBindings() {
    m_manager->ReloadBindings();
    ClearSelection();
    ShowNotification("Bindings reloaded");
    m_hasUnsavedChanges = false;

    if (OnBindingsReloaded) {
        OnBindingsReloaded();
    }
}

void EventBindingEditor::ImportBindings(const std::string& filePath) {
    int count = m_manager->LoadBindingsFromFile(filePath);
    if (count > 0) {
        ShowNotification("Imported " + std::to_string(count) + " binding(s)");
        m_hasUnsavedChanges = true;
    } else {
        ShowNotification("Failed to import bindings", true);
    }
}

void EventBindingEditor::ExportBindings(const std::string& filePath, const std::vector<std::string>& bindingIds) {
    if (m_manager->SaveBindingsToFile(filePath, bindingIds)) {
        ShowNotification("Bindings exported");
    } else {
        ShowNotification("Failed to export bindings", true);
    }
}

std::vector<const Nova::Events::EventBinding*> EventBindingEditor::GetFilteredBindings() const {
    auto allBindings = m_manager->GetAllBindings();
    std::vector<const Nova::Events::EventBinding*> filtered;

    for (const auto* binding : allBindings) {
        // Filter by enabled state
        if (m_showEnabledOnly && !binding->enabled) continue;
        if (m_showDisabledOnly && binding->enabled) continue;

        // Filter by category
        if (!m_filterCategory.empty() && binding->category != m_filterCategory) continue;

        // Filter by event type
        if (!m_filterEventType.empty() && binding->condition.eventName != m_filterEventType) continue;

        // Filter by text
        if (!m_filterText.empty()) {
            bool matches = binding->id.find(m_filterText) != std::string::npos ||
                          binding->name.find(m_filterText) != std::string::npos ||
                          binding->description.find(m_filterText) != std::string::npos;
            if (!matches) continue;
        }

        filtered.push_back(binding);
    }

    return filtered;
}

void EventBindingEditor::ShowNotification(const std::string& message, bool isError) {
    m_notificationMessage = message;
    m_notificationTimer = 3.0f;
    m_notificationIsError = isError;
}

void EventBindingEditor::RenderCompoundConditions(Nova::Events::EventCondition& condition) {
    // Placeholder for compound condition UI
}

} // namespace Editor
} // namespace Vehement
