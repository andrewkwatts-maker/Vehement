#pragma once

#include "../../../engine/events/EventBinding.hpp"
#include "../../../engine/events/EventBindingManager.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace Vehement {
namespace Editor {

/**
 * @brief ImGui-based editor for event bindings
 *
 * Features:
 * - List all bindings with filters
 * - Create new binding wizard
 * - Condition builder UI
 * - Python script editor integration
 * - Test binding button
 * - Import/export bindings
 */
class EventBindingEditor {
public:
    /**
     * @brief Editor configuration
     */
    struct Config {
        bool showDebugInfo = false;
        bool autoSave = true;
        float autoSaveIntervalSeconds = 60.0f;
        std::string defaultBindingsPath = "assets/configs/bindings";
        int maxRecentBindings = 10;
    };

    EventBindingEditor();
    ~EventBindingEditor();

    /**
     * @brief Initialize the editor
     */
    void Initialize(Nova::Events::EventBindingManager& manager, const Config& config = {});

    /**
     * @brief Shutdown the editor
     */
    void Shutdown();

    /**
     * @brief Render the editor UI
     */
    void Render();

    /**
     * @brief Update the editor
     */
    void Update(float deltaTime);

    // =========================================================================
    // Visibility
    // =========================================================================

    void Show() { m_visible = true; }
    void Hide() { m_visible = false; }
    void Toggle() { m_visible = !m_visible; }
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    // =========================================================================
    // Selection
    // =========================================================================

    void SelectBinding(const std::string& bindingId);
    void ClearSelection();
    [[nodiscard]] const std::string& GetSelectedBindingId() const { return m_selectedBindingId; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnBindingSelected;
    std::function<void(const std::string&)> OnBindingModified;
    std::function<void(const std::string&)> OnBindingDeleted;
    std::function<void()> OnBindingsReloaded;

private:
    // UI Rendering
    void RenderMenuBar();
    void RenderToolbar();
    void RenderBindingList();
    void RenderBindingDetails();
    void RenderConditionEditor(Nova::Events::EventCondition& condition);
    void RenderCallbackEditor(Nova::Events::EventBinding& binding);
    void RenderPythonEditor();
    void RenderNewBindingWizard();
    void RenderImportExportDialog();
    void RenderTestResults();
    void RenderStatusBar();

    // Condition Builder UI
    void RenderSourceTypeSelector(Nova::Events::EventCondition& condition);
    void RenderEventNameSelector(Nova::Events::EventCondition& condition);
    void RenderPropertyCondition(Nova::Events::EventCondition& condition);
    void RenderComparatorSelector(Nova::Events::Comparator& comparator);
    void RenderValueEditor(Nova::Events::ConditionValue& value, const std::string& label);
    void RenderCompoundConditions(Nova::Events::EventCondition& condition);

    // Actions
    void CreateNewBinding();
    void DuplicateBinding(const std::string& bindingId);
    void DeleteBinding(const std::string& bindingId);
    void TestBinding(const std::string& bindingId);
    void SaveBindings();
    void ReloadBindings();
    void ImportBindings(const std::string& filePath);
    void ExportBindings(const std::string& filePath, const std::vector<std::string>& bindingIds);

    // Helpers
    void ApplyFilter();
    std::vector<const Nova::Events::EventBinding*> GetFilteredBindings() const;
    std::string GetEventTypeOptions() const;
    std::string GetSourceTypeOptions() const;
    void ShowNotification(const std::string& message, bool isError = false);

    // State
    bool m_initialized = false;
    bool m_visible = false;
    Nova::Events::EventBindingManager* m_manager = nullptr;
    Config m_config;

    // Selection
    std::string m_selectedBindingId;
    Nova::Events::EventBinding* m_editingBinding = nullptr;

    // Filtering
    std::string m_filterText;
    std::string m_filterCategory;
    std::string m_filterEventType;
    bool m_showEnabledOnly = false;
    bool m_showDisabledOnly = false;

    // New binding wizard
    bool m_showNewBindingWizard = false;
    Nova::Events::EventBinding m_newBinding;
    int m_wizardStep = 0;

    // Import/Export
    bool m_showImportExport = false;
    bool m_isImporting = true;
    std::string m_importExportPath;
    std::vector<std::string> m_selectedForExport;

    // Python editor
    bool m_showPythonEditor = false;
    std::string m_pythonEditorContent;
    bool m_pythonEditorDirty = false;

    // Test results
    bool m_showTestResults = false;
    std::string m_testResultBindingId;
    bool m_testResultSuccess = false;
    std::string m_testResultMessage;

    // Notifications
    std::string m_notificationMessage;
    float m_notificationTimer = 0.0f;
    bool m_notificationIsError = false;

    // Auto-save
    float m_autoSaveTimer = 0.0f;
    bool m_hasUnsavedChanges = false;

    // Recent bindings
    std::vector<std::string> m_recentBindings;

    // Cached data
    std::vector<std::string> m_availableEventTypes;
    std::vector<std::string> m_availableSourceTypes;
    std::vector<std::string> m_availableCategories;
};

} // namespace Editor
} // namespace Vehement
