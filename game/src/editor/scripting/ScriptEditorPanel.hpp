#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <regex>

namespace Nova {
namespace Scripting {
    class PythonEngine;
    class ScriptValidator;
    class ScriptStorage;
    struct ScriptInfo;
}
}

namespace Vehement {

class Editor;

/**
 * @brief Syntax token types for highlighting
 */
enum class TokenType {
    None,
    Keyword,
    Builtin,
    String,
    Number,
    Comment,
    Operator,
    Decorator,
    Function,
    Class,
    Variable,
    Parameter,
    GameAPI,
    Error
};

/**
 * @brief Represents a syntax-highlighted token
 */
struct SyntaxToken {
    size_t start;
    size_t length;
    TokenType type;
};

/**
 * @brief Auto-completion suggestion
 */
struct CompletionItem {
    std::string text;
    std::string displayText;
    std::string description;
    std::string signature;
    std::string category;
    int priority = 0;
    bool isGameAPI = false;
};

/**
 * @brief Represents an error/warning in the code
 */
struct CodeDiagnostic {
    enum class Severity { Error, Warning, Info, Hint };

    int line;
    int column;
    int endColumn;
    Severity severity;
    std::string message;
    std::string source;  // "syntax", "lint", "security"
    std::string quickFix;  // Suggested fix
};

/**
 * @brief Breakpoint marker for future debugging
 */
struct Breakpoint {
    int line;
    bool enabled = true;
    std::string condition;
    int hitCount = 0;
    std::string logMessage;
};

/**
 * @brief Find/replace options
 */
struct FindReplaceOptions {
    std::string searchText;
    std::string replaceText;
    bool caseSensitive = false;
    bool wholeWord = false;
    bool useRegex = false;
    bool searchInAllFiles = false;
};

/**
 * @brief Code folding region
 */
struct FoldRegion {
    int startLine;
    int endLine;
    bool folded = false;
    std::string preview;  // First line preview when folded
};

/**
 * @brief Open file tab
 */
struct EditorTab {
    std::string filePath;
    std::string fileName;
    std::string content;
    std::string originalContent;  // For detecting changes
    bool modified = false;
    bool isNew = false;  // Unsaved new file
    int cursorLine = 0;
    int cursorColumn = 0;
    int scrollY = 0;
    std::vector<SyntaxToken> tokens;
    std::vector<CodeDiagnostic> diagnostics;
    std::vector<Breakpoint> breakpoints;
    std::vector<FoldRegion> foldRegions;
    std::chrono::system_clock::time_point lastValidation;
};

/**
 * @brief Output console message
 */
struct ConsoleMessage {
    enum class Type { Info, Output, Warning, Error, Debug };

    Type type;
    std::string text;
    std::string source;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Script execution result
 */
struct ExecutionResult {
    bool success;
    std::string output;
    std::string error;
    double executionTimeMs;
    int exitCode;
};

/**
 * @brief Full-featured Python editor panel for in-editor scripting
 *
 * Features:
 * - Syntax highlighting (Python + Game API)
 * - Auto-completion for game API
 * - Error highlighting and linting
 * - Multiple file tabs
 * - Find/replace with regex support
 * - Code folding
 * - Breakpoint markers
 * - Run/test button
 * - Output console
 *
 * Usage:
 * @code
 * ScriptEditorPanel panel;
 * panel.Initialize(editor);
 * panel.OpenFile("scripts/ai/zombie_ai.py");
 *
 * // In render loop
 * panel.Render();
 * @endcode
 */
class ScriptEditorPanel {
public:
    ScriptEditorPanel();
    ~ScriptEditorPanel();

    // Non-copyable
    ScriptEditorPanel(const ScriptEditorPanel&) = delete;
    ScriptEditorPanel& operator=(const ScriptEditorPanel&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the editor panel
     * @param editor Parent editor reference
     * @return true if initialization succeeded
     */
    bool Initialize(Editor* editor);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render the editor panel
     */
    void Render();

    /**
     * @brief Update state (call each frame)
     */
    void Update(float deltaTime);

    // =========================================================================
    // File Operations
    // =========================================================================

    /**
     * @brief Create a new empty script
     * @param templateName Optional template to use
     * @return Index of the new tab
     */
    int NewFile(const std::string& templateName = "");

    /**
     * @brief Open a script file
     * @param filePath Path to the file
     * @return true if successful
     */
    bool OpenFile(const std::string& filePath);

    /**
     * @brief Save the current file
     * @return true if successful
     */
    bool SaveCurrentFile();

    /**
     * @brief Save file to a new path
     * @param filePath New file path
     * @return true if successful
     */
    bool SaveFileAs(const std::string& filePath);

    /**
     * @brief Save all open files
     * @return Number of files saved
     */
    int SaveAllFiles();

    /**
     * @brief Close a tab
     * @param tabIndex Tab index to close (-1 for current)
     * @param force Close without save confirmation
     * @return true if closed
     */
    bool CloseTab(int tabIndex = -1, bool force = false);

    /**
     * @brief Close all tabs
     * @param force Close without save confirmation
     * @return true if all closed
     */
    bool CloseAllTabs(bool force = false);

    /**
     * @brief Check if any files have unsaved changes
     */
    [[nodiscard]] bool HasUnsavedChanges() const;

    /**
     * @brief Get current tab index
     */
    [[nodiscard]] int GetCurrentTabIndex() const { return m_currentTab; }

    /**
     * @brief Set current tab
     */
    void SetCurrentTab(int index);

    /**
     * @brief Get number of open tabs
     */
    [[nodiscard]] size_t GetTabCount() const { return m_tabs.size(); }

    /**
     * @brief Get tab at index
     */
    [[nodiscard]] const EditorTab* GetTab(int index) const;

    // =========================================================================
    // Editing
    // =========================================================================

    /**
     * @brief Set content of current tab
     */
    void SetContent(const std::string& content);

    /**
     * @brief Get content of current tab
     */
    [[nodiscard]] std::string GetContent() const;

    /**
     * @brief Insert text at cursor
     */
    void InsertText(const std::string& text);

    /**
     * @brief Delete selected text
     */
    void DeleteSelection();

    /**
     * @brief Select all text
     */
    void SelectAll();

    /**
     * @brief Cut selected text
     */
    void Cut();

    /**
     * @brief Copy selected text
     */
    void Copy();

    /**
     * @brief Paste from clipboard
     */
    void Paste();

    /**
     * @brief Undo last edit
     */
    void Undo();

    /**
     * @brief Redo last undone edit
     */
    void Redo();

    /**
     * @brief Go to line number
     */
    void GoToLine(int line);

    /**
     * @brief Set cursor position
     */
    void SetCursorPosition(int line, int column);

    /**
     * @brief Get cursor position
     */
    void GetCursorPosition(int& line, int& column) const;

    // =========================================================================
    // Find/Replace
    // =========================================================================

    /**
     * @brief Show find dialog
     */
    void ShowFindDialog();

    /**
     * @brief Show replace dialog
     */
    void ShowReplaceDialog();

    /**
     * @brief Find next occurrence
     */
    bool FindNext();

    /**
     * @brief Find previous occurrence
     */
    bool FindPrevious();

    /**
     * @brief Replace current match
     */
    bool Replace();

    /**
     * @brief Replace all matches
     * @return Number of replacements
     */
    int ReplaceAll();

    /**
     * @brief Set find/replace options
     */
    void SetFindReplaceOptions(const FindReplaceOptions& options);

    /**
     * @brief Get find/replace options
     */
    [[nodiscard]] const FindReplaceOptions& GetFindReplaceOptions() const { return m_findOptions; }

    // =========================================================================
    // Code Folding
    // =========================================================================

    /**
     * @brief Toggle fold at line
     */
    void ToggleFold(int line);

    /**
     * @brief Fold all regions
     */
    void FoldAll();

    /**
     * @brief Unfold all regions
     */
    void UnfoldAll();

    /**
     * @brief Update fold regions for current content
     */
    void UpdateFoldRegions();

    // =========================================================================
    // Breakpoints
    // =========================================================================

    /**
     * @brief Toggle breakpoint at line
     */
    void ToggleBreakpoint(int line);

    /**
     * @brief Add breakpoint
     */
    void AddBreakpoint(const Breakpoint& breakpoint);

    /**
     * @brief Remove breakpoint at line
     */
    void RemoveBreakpoint(int line);

    /**
     * @brief Clear all breakpoints
     */
    void ClearBreakpoints();

    /**
     * @brief Get all breakpoints in current file
     */
    [[nodiscard]] std::vector<Breakpoint> GetBreakpoints() const;

    // =========================================================================
    // Auto-completion
    // =========================================================================

    /**
     * @brief Trigger auto-completion
     */
    void TriggerAutoComplete();

    /**
     * @brief Hide auto-complete popup
     */
    void HideAutoComplete();

    /**
     * @brief Accept current auto-complete suggestion
     */
    void AcceptAutoComplete();

    /**
     * @brief Register custom completion items
     */
    void RegisterCompletionItems(const std::vector<CompletionItem>& items);

    // =========================================================================
    // Diagnostics
    // =========================================================================

    /**
     * @brief Validate current script
     * @return true if valid
     */
    bool ValidateScript();

    /**
     * @brief Get diagnostics for current file
     */
    [[nodiscard]] std::vector<CodeDiagnostic> GetDiagnostics() const;

    /**
     * @brief Go to next error
     */
    void GoToNextError();

    /**
     * @brief Go to previous error
     */
    void GoToPreviousError();

    // =========================================================================
    // Execution
    // =========================================================================

    /**
     * @brief Run the current script
     */
    ExecutionResult RunScript();

    /**
     * @brief Test the current script (dry run)
     */
    ExecutionResult TestScript();

    /**
     * @brief Stop running script
     */
    void StopScript();

    /**
     * @brief Check if script is running
     */
    [[nodiscard]] bool IsScriptRunning() const { return m_scriptRunning; }

    // =========================================================================
    // Console
    // =========================================================================

    /**
     * @brief Clear the output console
     */
    void ClearConsole();

    /**
     * @brief Add message to console
     */
    void AddConsoleMessage(const ConsoleMessage& message);

    /**
     * @brief Get console messages
     */
    [[nodiscard]] const std::vector<ConsoleMessage>& GetConsoleMessages() const { return m_consoleMessages; }

    // =========================================================================
    // Settings
    // =========================================================================

    /**
     * @brief Set font size
     */
    void SetFontSize(float size);

    /**
     * @brief Get font size
     */
    [[nodiscard]] float GetFontSize() const { return m_fontSize; }

    /**
     * @brief Set tab size (spaces)
     */
    void SetTabSize(int size);

    /**
     * @brief Get tab size
     */
    [[nodiscard]] int GetTabSize() const { return m_tabSize; }

    /**
     * @brief Enable/disable line numbers
     */
    void SetShowLineNumbers(bool show);

    /**
     * @brief Enable/disable word wrap
     */
    void SetWordWrap(bool wrap);

    /**
     * @brief Enable/disable auto-indent
     */
    void SetAutoIndent(bool enable);

    // =========================================================================
    // Callbacks
    // =========================================================================

    using FileCallback = std::function<void(const std::string& filePath)>;
    using ContentCallback = std::function<void(const std::string& content)>;

    void SetOnFileSaved(FileCallback callback) { m_onFileSaved = callback; }
    void SetOnFileOpened(FileCallback callback) { m_onFileOpened = callback; }
    void SetOnContentChanged(ContentCallback callback) { m_onContentChanged = callback; }

private:
    // UI rendering
    void RenderToolbar();
    void RenderTabBar();
    void RenderEditor();
    void RenderLineNumbers();
    void RenderCodeArea();
    void RenderMinimap();
    void RenderConsole();
    void RenderFindReplaceBar();
    void RenderAutoCompletePopup();
    void RenderStatusBar();

    // Dialogs
    void RenderSaveConfirmDialog();
    void RenderGoToLineDialog();

    // Syntax highlighting
    void TokenizeLine(const std::string& line, int lineNum, std::vector<SyntaxToken>& tokens);
    void TokenizeContent(EditorTab& tab);
    void ApplyHighlighting(const std::string& text, const std::vector<SyntaxToken>& tokens);
    uint32_t GetTokenColor(TokenType type) const;

    // Auto-completion
    void UpdateAutoComplete();
    std::vector<CompletionItem> GetCompletionsForContext(const std::string& prefix);
    void BuildCompletionIndex();

    // Validation
    void ScheduleValidation();
    void RunValidation();

    // Folding
    void DetectFoldRegions(EditorTab& tab);

    // Input handling
    void HandleKeyInput();
    void HandleTextInput(const std::string& text);
    void HandleMouseInput();

    // Utility
    std::string GetLineText(int line) const;
    int GetLineCount() const;
    void MarkModified();
    std::string GetWordAtCursor() const;
    void EnsureCursorVisible();

    // State
    bool m_initialized = false;
    Editor* m_editor = nullptr;
    Nova::Scripting::PythonEngine* m_pythonEngine = nullptr;
    Nova::Scripting::ScriptValidator* m_validator = nullptr;
    Nova::Scripting::ScriptStorage* m_storage = nullptr;

    // Tabs
    std::vector<EditorTab> m_tabs;
    int m_currentTab = -1;

    // Find/replace
    FindReplaceOptions m_findOptions;
    bool m_showFindBar = false;
    bool m_showReplaceBar = false;
    std::vector<std::pair<int, int>> m_findMatches;  // line, column
    int m_currentMatch = -1;

    // Auto-complete
    bool m_showAutoComplete = false;
    std::vector<CompletionItem> m_completions;
    std::vector<CompletionItem> m_filteredCompletions;
    int m_selectedCompletion = 0;
    std::string m_completionPrefix;
    std::vector<CompletionItem> m_gameAPICompletions;
    std::vector<CompletionItem> m_builtinCompletions;
    std::vector<CompletionItem> m_customCompletions;

    // Console
    std::vector<ConsoleMessage> m_consoleMessages;
    bool m_consoleAutoScroll = true;
    static constexpr size_t MAX_CONSOLE_MESSAGES = 1000;

    // Execution
    bool m_scriptRunning = false;
    std::chrono::system_clock::time_point m_scriptStartTime;

    // Dialogs
    bool m_showSaveConfirmDialog = false;
    bool m_showGoToLineDialog = false;
    int m_pendingCloseTab = -1;
    int m_goToLineNumber = 0;

    // Settings
    float m_fontSize = 14.0f;
    int m_tabSize = 4;
    bool m_showLineNumbers = true;
    bool m_wordWrap = false;
    bool m_autoIndent = true;
    bool m_showMinimap = true;
    bool m_showConsole = true;

    // Validation
    float m_validationDelay = 0.5f;
    float m_validationTimer = 0.0f;
    bool m_validationPending = false;

    // Python keywords for highlighting
    static const std::vector<std::string> s_pythonKeywords;
    static const std::vector<std::string> s_pythonBuiltins;

    // Callbacks
    FileCallback m_onFileSaved;
    FileCallback m_onFileOpened;
    ContentCallback m_onContentChanged;

    // Clipboard (internal)
    std::string m_clipboard;

    // Selection
    bool m_hasSelection = false;
    int m_selectionStartLine = 0;
    int m_selectionStartCol = 0;
    int m_selectionEndLine = 0;
    int m_selectionEndCol = 0;

    // Undo/Redo
    struct EditAction {
        enum class Type { Insert, Delete, Replace };
        Type type;
        int line, column;
        std::string text;
        std::string oldText;
    };
    std::vector<EditAction> m_undoStack;
    std::vector<EditAction> m_redoStack;
    static constexpr size_t MAX_UNDO_HISTORY = 100;

    // Syntax highlighting cache
    std::unordered_map<std::string, TokenType> m_keywordMap;
};

} // namespace Vehement
