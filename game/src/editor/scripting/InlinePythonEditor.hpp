#pragma once

#include <string>
#include <functional>
#include <vector>
#include <memory>

namespace Vehement {

class Editor;
struct CompletionItem;

/**
 * @brief Validation result for inline scripts
 */
struct InlineValidationResult {
    bool valid = true;
    std::string errorMessage;
    int errorLine = -1;
    int errorColumn = -1;
};

/**
 * @brief Small inline Python editor for quick scripts
 *
 * Features:
 * - Single-line or expandable multi-line mode
 * - Auto-complete popup for game API
 * - Validate button with error display
 * - Link to open in full editor
 * - Syntax highlighting (basic)
 *
 * Usage:
 * @code
 * InlinePythonEditor editor;
 * editor.Initialize(editorRef);
 *
 * // In render loop
 * if (editor.Render("##script", script, width)) {
 *     // Script was modified
 *     UpdateConfig(script);
 * }
 * @endcode
 */
class InlinePythonEditor {
public:
    InlinePythonEditor();
    ~InlinePythonEditor();

    // =========================================================================
    // Initialization
    // =========================================================================

    bool Initialize(Editor* editor);
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render single-line inline editor
     * @param label ImGui label
     * @param script Reference to script string
     * @param width Widget width (-1 for auto)
     * @return true if script was modified
     */
    bool Render(const char* label, std::string& script, float width = -1);

    /**
     * @brief Render multi-line inline editor
     * @param label ImGui label
     * @param script Reference to script string
     * @param width Widget width
     * @param height Widget height
     * @return true if script was modified
     */
    bool RenderMultiline(const char* label, std::string& script,
                         float width = -1, float height = 100);

    /**
     * @brief Render expandable editor (single line that can expand)
     * @param label ImGui label
     * @param script Reference to script string
     * @param width Widget width
     * @return true if script was modified
     */
    bool RenderExpandable(const char* label, std::string& script, float width = -1);

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate a script
     */
    InlineValidationResult Validate(const std::string& script);

    /**
     * @brief Set auto-validation on change
     */
    void SetAutoValidate(bool enable) { m_autoValidate = enable; }

    /**
     * @brief Get last validation result
     */
    [[nodiscard]] const InlineValidationResult& GetLastValidation() const { return m_lastValidation; }

    // =========================================================================
    // Auto-completion
    // =========================================================================

    /**
     * @brief Enable/disable auto-complete
     */
    void SetAutoCompleteEnabled(bool enable) { m_autoCompleteEnabled = enable; }

    /**
     * @brief Register custom completion items
     */
    void RegisterCompletions(const std::vector<CompletionItem>& items);

    // =========================================================================
    // Actions
    // =========================================================================

    /**
     * @brief Open current script in full editor
     */
    void OpenInFullEditor(const std::string& script);

    // =========================================================================
    // Settings
    // =========================================================================

    void SetPlaceholder(const std::string& placeholder) { m_placeholder = placeholder; }
    void SetMaxLength(size_t maxLength) { m_maxLength = maxLength; }
    void SetShowValidateButton(bool show) { m_showValidateButton = show; }
    void SetShowExpandButton(bool show) { m_showExpandButton = show; }
    void SetShowOpenInEditorButton(bool show) { m_showOpenInEditor = show; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    using ScriptCallback = std::function<void(const std::string&)>;
    void SetOnValidated(ScriptCallback callback) { m_onValidated = callback; }
    void SetOnOpenInEditor(ScriptCallback callback) { m_onOpenInEditor = callback; }

private:
    // Internal rendering
    bool RenderInputField(const char* label, std::string& script, float width,
                         bool multiline, float height);
    void RenderButtons(const std::string& script);
    void RenderAutoComplete(const std::string& currentWord);
    void RenderValidationStatus();

    // Auto-complete helpers
    void UpdateAutoComplete(const std::string& text, int cursorPos);
    std::vector<CompletionItem> GetCompletions(const std::string& prefix);
    std::string GetWordAtPosition(const std::string& text, int position);

    // State
    bool m_initialized = false;
    Editor* m_editor = nullptr;

    // Validation
    bool m_autoValidate = true;
    InlineValidationResult m_lastValidation;
    float m_validationTimer = 0.0f;
    static constexpr float VALIDATION_DELAY = 0.3f;

    // Auto-complete
    bool m_autoCompleteEnabled = true;
    bool m_showAutoComplete = false;
    std::vector<CompletionItem> m_completions;
    std::vector<CompletionItem> m_filteredCompletions;
    std::vector<CompletionItem> m_customCompletions;
    int m_selectedCompletion = 0;
    std::string m_completionPrefix;

    // UI state
    bool m_expanded = false;
    std::string m_placeholder = "Enter Python code...";
    size_t m_maxLength = 4096;
    bool m_showValidateButton = true;
    bool m_showExpandButton = true;
    bool m_showOpenInEditor = true;

    // Callbacks
    ScriptCallback m_onValidated;
    ScriptCallback m_onOpenInEditor;

    // Internal buffer
    char m_buffer[4096] = {0};
};

} // namespace Vehement
