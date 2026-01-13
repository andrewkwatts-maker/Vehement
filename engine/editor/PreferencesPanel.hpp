/**
 * @file PreferencesPanel.hpp
 * @brief Preferences/Settings modal dialog panel for Nova3D/Vehement Editor
 *
 * Provides a comprehensive settings UI with:
 * - Category list on left side
 * - Settings content on right side
 * - Search/filter functionality
 * - Apply/Cancel/OK buttons
 * - Reset to defaults
 * - Import/Export settings
 * - Shortcut conflict detection
 * - Real-time validation
 */

#pragma once

#include "../ui/EditorPanel.hpp"
#include "../ui/EditorTheme.hpp"
#include "../ui/EditorWidgets.hpp"
#include "EditorSettings.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Nova {

/**
 * @brief Preferences panel for editing all editor settings
 *
 * This is a modal dialog panel that displays when the user opens
 * Edit > Preferences (Ctrl+,). It provides a comprehensive UI for
 * modifying all editor settings with validation and conflict detection.
 */
class PreferencesPanel : public EditorPanel {
public:
    PreferencesPanel();
    ~PreferencesPanel() override;

    /**
     * @brief Show the preferences panel as a modal dialog
     */
    void ShowModal();

    /**
     * @brief Hide the preferences panel
     */
    void HideModal();

    /**
     * @brief Check if panel is showing
     */
    [[nodiscard]] bool IsShowing() const { return m_showing; }

    /**
     * @brief Set callback for when settings are applied
     */
    void SetOnApplied(std::function<void()> callback) { m_onApplied = std::move(callback); }

    /**
     * @brief Jump to a specific category
     */
    void SelectCategory(SettingsCategory category);

    /**
     * @brief Jump to a specific setting by key
     */
    void FocusSetting(const std::string& settingKey);

    // EditorPanel overrides
    void Update(float deltaTime) override;

protected:
    void OnRender() override;
    void OnInitialize() override;
    void OnShutdown() override;

private:
    // ==========================================================================
    // UI Rendering Methods
    // ==========================================================================

    /**
     * @brief Render the category list (left side)
     */
    void RenderCategoryList();

    /**
     * @brief Render the settings content (right side)
     */
    void RenderSettingsContent();

    /**
     * @brief Render the search bar
     */
    void RenderSearchBar();

    /**
     * @brief Render the button bar (Apply, Cancel, OK)
     */
    void RenderButtonBar();

    /**
     * @brief Render settings for a specific category
     */
    void RenderCategorySettings(SettingsCategory category);

    // Category-specific rendering
    void RenderGeneralSettings();
    void RenderAppearanceSettings();
    void RenderViewportSettings();
    void RenderInputSettings();
    void RenderPerformanceSettings();
    void RenderPathSettings();
    void RenderPluginSettings();

    /**
     * @brief Render the keyboard shortcuts editor
     */
    void RenderShortcutsEditor();

    /**
     * @brief Render the shortcut conflict dialog
     */
    void RenderConflictDialog();

    /**
     * @brief Render validation messages
     */
    void RenderValidationMessages();

    // ==========================================================================
    // Settings Helpers
    // ==========================================================================

    /**
     * @brief Check if a setting matches the current search filter
     */
    [[nodiscard]] bool MatchesSearch(const std::string& label, const std::string& description = "") const;

    /**
     * @brief Apply pending changes
     */
    void ApplyChanges();

    /**
     * @brief Discard pending changes
     */
    void DiscardChanges();

    /**
     * @brief Reset current category to defaults
     */
    void ResetCurrentCategory();

    /**
     * @brief Reset all settings to defaults
     */
    void ResetAllSettings();

    /**
     * @brief Import settings from file
     */
    void ImportSettings();

    /**
     * @brief Export settings to file
     */
    void ExportSettings();

    /**
     * @brief Validate current settings
     */
    void ValidateSettings();

    /**
     * @brief Check for shortcut conflicts
     */
    void CheckShortcutConflicts();

    // ==========================================================================
    // Widget Helpers
    // ==========================================================================

    /**
     * @brief Render a section header
     */
    void SectionHeader(const char* label);

    /**
     * @brief Begin a settings group with optional collapsing
     */
    bool BeginSettingsGroup(const char* label, bool defaultOpen = true);
    void EndSettingsGroup();

    /**
     * @brief Render a setting row with label and value
     */
    template<typename T>
    bool SettingRow(const char* label, T& value, const char* tooltip = nullptr);

    /**
     * @brief Render a bool setting with checkbox
     */
    bool BoolSetting(const char* label, bool& value, const char* tooltip = nullptr);

    /**
     * @brief Render an int setting with spinner or slider
     */
    bool IntSetting(const char* label, int& value, int min = 0, int max = 100, const char* tooltip = nullptr);

    /**
     * @brief Render a float setting with slider
     */
    bool FloatSetting(const char* label, float& value, float min = 0.0f, float max = 1.0f, const char* tooltip = nullptr);

    /**
     * @brief Render a string setting with text input
     */
    bool StringSetting(const char* label, std::string& value, const char* tooltip = nullptr);

    /**
     * @brief Render a path setting with browse button
     */
    bool PathSetting(const char* label, std::string& value, bool isFolder = true, const char* tooltip = nullptr);

    /**
     * @brief Render a color setting with picker
     */
    bool ColorSetting(const char* label, glm::vec4& color, const char* tooltip = nullptr);

    /**
     * @brief Render an enum setting with dropdown
     */
    template<typename E>
    bool EnumSetting(const char* label, E& value, const char* const* names, int count, const char* tooltip = nullptr);

    /**
     * @brief Render a shortcut setting with key capture
     */
    bool ShortcutSetting(const char* label, KeyboardShortcut& shortcut, const char* tooltip = nullptr);

    // ==========================================================================
    // State
    // ==========================================================================

    bool m_showing = false;
    bool m_shouldClose = false;
    SettingsCategory m_selectedCategory = SettingsCategory::General;

    // Search
    char m_searchBuffer[256] = "";
    std::string m_searchFilter;
    bool m_searchFocused = false;

    // Pending changes (copy of settings being edited)
    CompleteEditorSettings m_pendingSettings;
    bool m_hasChanges = false;

    // Validation
    SettingsValidationResult m_validationResult;
    bool m_showValidation = false;

    // Shortcut editing
    bool m_capturingShortcut = false;
    std::string m_capturingAction;
    KeyboardShortcut m_capturedShortcut;

    // Conflict resolution
    bool m_showConflictDialog = false;
    std::string m_conflictAction1;
    std::string m_conflictAction2;

    // Callbacks
    std::function<void()> m_onApplied;

    // UI state
    float m_categoryListWidth = 180.0f;
    float m_animationProgress = 0.0f;
    bool m_scrollToSetting = false;
    std::string m_settingToScrollTo;

    // Expanded groups
    std::unordered_map<std::string, bool> m_expandedGroups;
};

/**
 * @brief Global preferences panel instance
 *
 * Use this to show/hide the preferences panel from anywhere in the editor.
 */
class PreferencesManager {
public:
    static PreferencesManager& Instance();

    /**
     * @brief Show the preferences panel
     */
    void ShowPreferences();

    /**
     * @brief Hide the preferences panel
     */
    void HidePreferences();

    /**
     * @brief Toggle preferences panel visibility
     */
    void TogglePreferences();

    /**
     * @brief Check if preferences are showing
     */
    [[nodiscard]] bool IsShowing() const;

    /**
     * @brief Update the preferences panel
     */
    void Update(float deltaTime);

    /**
     * @brief Render the preferences panel
     */
    void Render();

    /**
     * @brief Show preferences and jump to a specific category
     */
    void ShowCategory(SettingsCategory category);

    /**
     * @brief Show preferences and focus a specific setting
     */
    void ShowSetting(const std::string& settingKey);

private:
    PreferencesManager();
    ~PreferencesManager();

    std::unique_ptr<PreferencesPanel> m_panel;
};

} // namespace Nova
