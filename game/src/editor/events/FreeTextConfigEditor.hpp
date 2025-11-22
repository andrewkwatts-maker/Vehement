#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <deque>
#include <optional>

namespace Vehement {
namespace WebEditor {
    class JSBridge;
    class WebViewManager;
}

/**
 * @brief Validation error for JSON editing
 */
struct ValidationError {
    int line;
    int column;
    std::string message;
    std::string code;      // Error code for categorization
    std::string severity;  // "error", "warning", "info"
    std::string path;      // JSON path to error location
};

/**
 * @brief Document change for undo/redo
 */
struct DocumentChange {
    std::string before;
    std::string after;
    int cursorLine;
    int cursorColumn;
    std::string description;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief Search result in config
 */
struct SearchResult {
    std::string filePath;
    int line;
    int column;
    int length;
    std::string lineContent;
    std::string matchText;
};

/**
 * @brief JSON schema info for autocompletion
 */
struct SchemaProperty {
    std::string name;
    std::string type;
    std::string description;
    bool required = false;
    std::vector<std::string> enumValues;
    std::string defaultValue;
    std::string pattern;
};

/**
 * @brief Completion suggestion
 */
struct CompletionItem {
    std::string label;
    std::string insertText;
    std::string kind;  // "property", "value", "snippet"
    std::string detail;
    std::string documentation;
    bool isSnippet = false;
};

/**
 * @brief Diff change for version control
 */
struct DiffChange {
    enum class Type { Added, Removed, Modified, Unchanged };
    Type type;
    int lineNumber;
    std::string oldContent;
    std::string newContent;
};

/**
 * @brief Free-text JSON Config Editor
 *
 * Monaco-style text editor with JSON syntax highlighting:
 * - Schema-aware auto-completion
 * - Real-time validation with error markers
 * - Diff view for changes
 * - Search/replace across all configs
 * - Undo/redo with history
 * - Format/prettify JSON
 * - Collapse/expand sections
 */
class FreeTextConfigEditor {
public:
    /**
     * @brief Configuration
     */
    struct Config {
        int maxUndoHistory = 100;
        bool autoValidate = true;
        bool autoFormat = false;
        float validationDelay = 0.3f;  // seconds
        int tabSize = 2;
        bool insertSpaces = true;
        bool wordWrap = true;
        std::string schemaBasePath = "assets/schemas/";
    };

    /**
     * @brief Editor state
     */
    struct EditorState {
        std::string filePath;
        std::string content;
        std::string schemaPath;
        bool isDirty = false;
        int cursorLine = 0;
        int cursorColumn = 0;
        int selectionStartLine = 0;
        int selectionStartColumn = 0;
        int selectionEndLine = 0;
        int selectionEndColumn = 0;
        std::vector<int> collapsedRanges;
    };

    /**
     * @brief Callbacks
     */
    using OnContentChangedCallback = std::function<void(const std::string& content)>;
    using OnValidationCallback = std::function<void(const std::vector<ValidationError>&)>;
    using OnSavedCallback = std::function<void(const std::string& path)>;

    FreeTextConfigEditor();
    ~FreeTextConfigEditor();

    // Non-copyable
    FreeTextConfigEditor(const FreeTextConfigEditor&) = delete;
    FreeTextConfigEditor& operator=(const FreeTextConfigEditor&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the editor
     * @param bridge Reference to JSBridge
     * @param config Configuration
     * @return true if successful
     */
    bool Initialize(WebEditor::JSBridge& bridge, const Config& config = {});

    /**
     * @brief Shutdown
     */
    void Shutdown();

    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Update and Rendering
    // =========================================================================

    /**
     * @brief Update state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render the editor UI (ImGui)
     */
    void Render();

    /**
     * @brief Render the full web-based editor
     * @param webViewManager Reference to WebViewManager
     */
    void RenderWebEditor(WebEditor::WebViewManager& webViewManager);

    // =========================================================================
    // File Operations
    // =========================================================================

    /**
     * @brief Open a config file
     * @param path File path
     * @return true if successful
     */
    bool OpenFile(const std::string& path);

    /**
     * @brief Save current file
     * @return true if successful
     */
    bool SaveFile();

    /**
     * @brief Save to a new path
     * @param path New file path
     * @return true if successful
     */
    bool SaveFileAs(const std::string& path);

    /**
     * @brief Close current file
     */
    void CloseFile();

    /**
     * @brief Create a new file
     * @param schemaPath Optional schema path
     */
    void NewFile(const std::string& schemaPath = "");

    /**
     * @brief Get current file path
     */
    [[nodiscard]] const std::string& GetFilePath() const { return m_currentState.filePath; }

    /**
     * @brief Check if file has unsaved changes
     */
    [[nodiscard]] bool IsDirty() const { return m_currentState.isDirty; }

    // =========================================================================
    // Content Operations
    // =========================================================================

    /**
     * @brief Get current content
     */
    [[nodiscard]] const std::string& GetContent() const { return m_currentState.content; }

    /**
     * @brief Set content
     * @param content New content
     * @param recordUndo Record in undo history
     */
    void SetContent(const std::string& content, bool recordUndo = true);

    /**
     * @brief Insert text at cursor
     * @param text Text to insert
     */
    void InsertText(const std::string& text);

    /**
     * @brief Delete selection or character
     */
    void DeleteSelection();

    /**
     * @brief Get selected text
     */
    [[nodiscard]] std::string GetSelectedText() const;

    /**
     * @brief Select all
     */
    void SelectAll();

    // =========================================================================
    // Formatting
    // =========================================================================

    /**
     * @brief Format/prettify the JSON
     * @return true if successful
     */
    bool Format();

    /**
     * @brief Minify the JSON
     * @return true if successful
     */
    bool Minify();

    /**
     * @brief Sort keys alphabetically
     * @return true if successful
     */
    bool SortKeys();

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate content against schema
     * @return Vector of validation errors
     */
    std::vector<ValidationError> Validate();

    /**
     * @brief Set schema for validation
     * @param schemaPath Path to JSON schema
     */
    void SetSchema(const std::string& schemaPath);

    /**
     * @brief Get current validation errors
     */
    [[nodiscard]] const std::vector<ValidationError>& GetErrors() const { return m_errors; }

    /**
     * @brief Check if content is valid
     */
    [[nodiscard]] bool IsValid() const { return m_errors.empty(); }

    // =========================================================================
    // Auto-completion
    // =========================================================================

    /**
     * @brief Get completion suggestions at position
     * @param line Line number
     * @param column Column number
     * @return Vector of completion items
     */
    std::vector<CompletionItem> GetCompletions(int line, int column);

    /**
     * @brief Apply completion
     * @param item Completion item to apply
     */
    void ApplyCompletion(const CompletionItem& item);

    /**
     * @brief Get hover information at position
     * @param line Line number
     * @param column Column number
     * @return Hover info string
     */
    std::string GetHoverInfo(int line, int column);

    // =========================================================================
    // Search and Replace
    // =========================================================================

    /**
     * @brief Find text in current file
     * @param query Search query
     * @param caseSensitive Case sensitive search
     * @param useRegex Use regular expressions
     * @param wholeWord Match whole words only
     * @return Vector of search results
     */
    std::vector<SearchResult> Find(const std::string& query,
                                    bool caseSensitive = false,
                                    bool useRegex = false,
                                    bool wholeWord = false);

    /**
     * @brief Find in all config files
     * @param query Search query
     * @param paths Paths to search
     * @return Vector of search results
     */
    std::vector<SearchResult> FindInFiles(const std::string& query,
                                           const std::vector<std::string>& paths);

    /**
     * @brief Replace text
     * @param query Search query
     * @param replacement Replacement text
     * @param caseSensitive Case sensitive search
     * @param useRegex Use regular expressions
     * @return Number of replacements made
     */
    int Replace(const std::string& query,
                const std::string& replacement,
                bool caseSensitive = false,
                bool useRegex = false);

    /**
     * @brief Replace all occurrences
     */
    int ReplaceAll(const std::string& query,
                   const std::string& replacement,
                   bool caseSensitive = false,
                   bool useRegex = false);

    /**
     * @brief Go to next search result
     */
    void FindNext();

    /**
     * @brief Go to previous search result
     */
    void FindPrevious();

    // =========================================================================
    // Undo/Redo
    // =========================================================================

    /**
     * @brief Undo last change
     */
    void Undo();

    /**
     * @brief Redo last undone change
     */
    void Redo();

    /**
     * @brief Check if undo is available
     */
    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }

    /**
     * @brief Check if redo is available
     */
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }

    /**
     * @brief Get undo history
     */
    [[nodiscard]] std::vector<std::string> GetUndoHistory() const;

    /**
     * @brief Clear undo history
     */
    void ClearUndoHistory();

    // =========================================================================
    // Diff View
    // =========================================================================

    /**
     * @brief Show diff against saved version
     * @return Vector of diff changes
     */
    std::vector<DiffChange> GetDiffAgainstSaved();

    /**
     * @brief Show diff against another file
     * @param otherPath Path to other file
     * @return Vector of diff changes
     */
    std::vector<DiffChange> GetDiffAgainstFile(const std::string& otherPath);

    /**
     * @brief Enable/disable diff view
     */
    void SetDiffViewEnabled(bool enabled) { m_showDiff = enabled; }

    /**
     * @brief Check if diff view is enabled
     */
    [[nodiscard]] bool IsDiffViewEnabled() const { return m_showDiff; }

    // =========================================================================
    // Folding
    // =========================================================================

    /**
     * @brief Fold a range
     * @param startLine Start line
     */
    void FoldRange(int startLine);

    /**
     * @brief Unfold a range
     * @param startLine Start line
     */
    void UnfoldRange(int startLine);

    /**
     * @brief Fold all
     */
    void FoldAll();

    /**
     * @brief Unfold all
     */
    void UnfoldAll();

    /**
     * @brief Get foldable ranges
     */
    [[nodiscard]] std::vector<std::pair<int, int>> GetFoldableRanges() const;

    // =========================================================================
    // Navigation
    // =========================================================================

    /**
     * @brief Go to line
     * @param line Line number
     */
    void GoToLine(int line);

    /**
     * @brief Go to position
     * @param line Line number
     * @param column Column number
     */
    void GoToPosition(int line, int column);

    /**
     * @brief Go to definition (for $ref)
     */
    void GoToDefinition();

    /**
     * @brief Find all references
     * @return Vector of search results
     */
    std::vector<SearchResult> FindReferences();

    /**
     * @brief Get document outline (tree structure)
     */
    [[nodiscard]] std::string GetOutline() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    OnContentChangedCallback OnContentChanged;
    OnValidationCallback OnValidation;
    OnSavedCallback OnSaved;
    std::function<void()> OnDirtyStateChanged;

private:
    // ImGui rendering
    void RenderMenuBar();
    void RenderToolbar();
    void RenderEditor();
    void RenderSidePanel();
    void RenderOutline();
    void RenderErrorList();
    void RenderSearchPanel();
    void RenderDiffView();
    void RenderStatusBar();

    // Text editing helpers
    void HandleInput();
    void UpdateLineNumbers();
    void UpdateSyntaxHighlighting();
    void UpdateValidation();

    // Schema helpers
    void LoadSchema(const std::string& path);
    std::vector<SchemaProperty> GetSchemaProperties(const std::string& jsonPath);

    // JSON helpers
    std::string GetJsonPath(int line, int column);
    std::pair<int, int> GetPositionFromPath(const std::string& path);

    // Diff helpers
    std::vector<DiffChange> ComputeDiff(const std::string& oldText,
                                         const std::string& newText);

    // JSBridge registration
    void RegisterBridgeFunctions();

    // Utility
    std::vector<std::string> SplitLines(const std::string& text);
    std::string JoinLines(const std::vector<std::string>& lines);
    void RecordChange(const std::string& description);

    // State
    bool m_initialized = false;
    Config m_config;
    WebEditor::JSBridge* m_bridge = nullptr;

    // Current editor state
    EditorState m_currentState;
    std::string m_savedContent;  // For diff

    // Schema
    std::string m_schemaContent;
    std::unordered_map<std::string, std::vector<SchemaProperty>> m_schemaCache;

    // Validation
    std::vector<ValidationError> m_errors;
    float m_validationTimer = 0.0f;
    bool m_needsValidation = false;

    // Undo/Redo
    std::deque<DocumentChange> m_undoStack;
    std::deque<DocumentChange> m_redoStack;

    // Search
    std::string m_searchQuery;
    std::vector<SearchResult> m_searchResults;
    int m_currentSearchIndex = -1;
    bool m_searchCaseSensitive = false;
    bool m_searchRegex = false;
    bool m_searchWholeWord = false;

    // UI state
    bool m_showOutline = true;
    bool m_showErrors = true;
    bool m_showSearch = false;
    bool m_showDiff = false;
    char m_searchBuffer[256] = "";
    char m_replaceBuffer[256] = "";
    char m_gotoLineBuffer[32] = "";

    // Web view ID
    std::string m_webViewId = "config_text_editor";
};

} // namespace Vehement
