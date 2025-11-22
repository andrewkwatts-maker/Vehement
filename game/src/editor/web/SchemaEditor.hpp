#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <optional>
#include <stack>

namespace Vehement {

class Editor;

namespace Config {
struct ConfigSchemaDefinition;
struct SchemaField;
struct ValidationResult;
}

namespace WebEditor {

class WebView;
class JSBridge;
class JSValue;

/**
 * @brief Validation error with location info
 */
struct ValidationError {
    std::string path;        // JSON path (e.g., "stats.health.max")
    std::string message;     // Error message
    std::string expected;    // Expected type/value
    std::string actual;      // Actual type/value
    int line = -1;           // Line number in JSON (if available)
    int column = -1;         // Column number

    enum class Severity {
        Error,
        Warning,
        Info
    };
    Severity severity = Severity::Error;
};

/**
 * @brief Edit operation for undo/redo
 */
struct EditOperation {
    std::string path;
    std::string oldValue;   // JSON string
    std::string newValue;   // JSON string
    std::string description;
};

/**
 * @brief Diff entry for comparing JSON values
 */
struct DiffEntry {
    std::string path;
    std::string leftValue;
    std::string rightValue;

    enum class Type {
        Added,
        Removed,
        Modified,
        Unchanged
    };
    Type type;
};

/**
 * @brief Generic JSON schema editor panel
 *
 * Auto-generates UI from JSON schema definitions:
 * - Builds form controls from schema field types
 * - Validates input in real-time with error highlighting
 * - Supports undo/redo for all edits
 * - Diff view to compare versions
 * - Works with any JSON config type
 *
 * Integrates with ConfigSchema.hpp for validation rules.
 */
class SchemaEditor {
public:
    explicit SchemaEditor(Editor* editor);
    ~SchemaEditor();

    // Non-copyable
    SchemaEditor(const SchemaEditor&) = delete;
    SchemaEditor& operator=(const SchemaEditor&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialize the editor
     * @return true if successful
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render the panel
     */
    void Render();

    // =========================================================================
    // Schema Management
    // =========================================================================

    /**
     * @brief Register a schema for a config type
     * @param typeId Type identifier (e.g., "unit", "spell")
     * @param schema Schema definition
     */
    void RegisterSchema(const std::string& typeId,
                        const Config::ConfigSchemaDefinition& schema);

    /**
     * @brief Get schema for a type
     */
    [[nodiscard]] const Config::ConfigSchemaDefinition* GetSchema(const std::string& typeId) const;

    /**
     * @brief Get all registered type IDs
     */
    [[nodiscard]] std::vector<std::string> GetRegisteredTypes() const;

    // =========================================================================
    // Document Editing
    // =========================================================================

    /**
     * @brief Load JSON data for editing
     * @param typeId Schema type to use
     * @param jsonData JSON string to edit
     * @param documentId Optional document identifier
     * @return true if successful
     */
    bool LoadDocument(const std::string& typeId,
                      const std::string& jsonData,
                      const std::string& documentId = "");

    /**
     * @brief Load JSON from file
     * @param typeId Schema type
     * @param filePath Path to JSON file
     * @return true if successful
     */
    bool LoadFromFile(const std::string& typeId, const std::string& filePath);

    /**
     * @brief Get current document as JSON string
     */
    [[nodiscard]] std::string GetDocument() const;

    /**
     * @brief Save current document to file
     * @param filePath Path to save to
     * @return true if successful
     */
    bool SaveToFile(const std::string& filePath);

    /**
     * @brief Check if document has unsaved changes
     */
    [[nodiscard]] bool IsDirty() const { return m_isDirty; }

    /**
     * @brief Mark document as saved
     */
    void MarkClean() { m_isDirty = false; }

    /**
     * @brief Get current document ID
     */
    [[nodiscard]] const std::string& GetDocumentId() const { return m_documentId; }

    // =========================================================================
    // Value Editing
    // =========================================================================

    /**
     * @brief Get value at JSON path
     * @param path JSON path (e.g., "stats.health")
     * @return Value as JSON string, or empty if not found
     */
    [[nodiscard]] std::string GetValue(const std::string& path) const;

    /**
     * @brief Set value at JSON path
     * @param path JSON path
     * @param value New value as JSON string
     * @param createPath Create intermediate objects if path doesn't exist
     * @return true if successful
     */
    bool SetValue(const std::string& path, const std::string& value, bool createPath = true);

    /**
     * @brief Delete value at JSON path
     * @param path JSON path
     * @return true if successful
     */
    bool DeleteValue(const std::string& path);

    /**
     * @brief Add item to array at path
     * @param arrayPath Path to array
     * @param value Value to add
     * @return Index of added item, or -1 on failure
     */
    int AddArrayItem(const std::string& arrayPath, const std::string& value);

    /**
     * @brief Remove item from array
     * @param arrayPath Path to array
     * @param index Index to remove
     * @return true if successful
     */
    bool RemoveArrayItem(const std::string& arrayPath, int index);

    /**
     * @brief Move array item
     * @param arrayPath Path to array
     * @param fromIndex Source index
     * @param toIndex Destination index
     * @return true if successful
     */
    bool MoveArrayItem(const std::string& arrayPath, int fromIndex, int toIndex);

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate current document
     * @return Validation result with errors/warnings
     */
    [[nodiscard]] Config::ValidationResult Validate() const;

    /**
     * @brief Validate specific value against schema field
     */
    [[nodiscard]] Config::ValidationResult ValidateValue(const std::string& path,
                                                          const std::string& value) const;

    /**
     * @brief Get all validation errors
     */
    [[nodiscard]] const std::vector<ValidationError>& GetErrors() const { return m_errors; }

    /**
     * @brief Check if document is valid
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief Enable/disable real-time validation
     */
    void SetRealtimeValidation(bool enabled) { m_realtimeValidation = enabled; }

    // =========================================================================
    // Undo/Redo
    // =========================================================================

    /**
     * @brief Undo last edit
     */
    void Undo();

    /**
     * @brief Redo last undone edit
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
     * @brief Get undo history descriptions
     */
    [[nodiscard]] std::vector<std::string> GetUndoHistory() const;

    /**
     * @brief Clear undo/redo history
     */
    void ClearHistory();

    // =========================================================================
    // Diff View
    // =========================================================================

    /**
     * @brief Compare two JSON documents
     * @param leftJson Left document
     * @param rightJson Right document
     * @return List of differences
     */
    [[nodiscard]] std::vector<DiffEntry> ComputeDiff(const std::string& leftJson,
                                                      const std::string& rightJson) const;

    /**
     * @brief Show diff between current document and another
     * @param otherJson JSON to compare with
     */
    void ShowDiff(const std::string& otherJson);

    /**
     * @brief Hide diff view
     */
    void HideDiff();

    /**
     * @brief Check if diff view is active
     */
    [[nodiscard]] bool IsDiffActive() const { return m_showDiff; }

    // =========================================================================
    // Field Focus
    // =========================================================================

    /**
     * @brief Focus on a specific field
     * @param path JSON path
     */
    void FocusField(const std::string& path);

    /**
     * @brief Get currently focused field path
     */
    [[nodiscard]] const std::string& GetFocusedField() const { return m_focusedPath; }

    /**
     * @brief Expand all collapsible sections
     */
    void ExpandAll();

    /**
     * @brief Collapse all collapsible sections
     */
    void CollapseAll();

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&, const std::string&)> OnValueChanged;
    std::function<void(const std::vector<ValidationError>&)> OnValidationChanged;
    std::function<void()> OnDocumentLoaded;
    std::function<void()> OnDocumentSaved;

private:
    // JavaScript bridge setup
    void SetupJSBridge();
    void RegisterBridgeFunctions();

    // Schema to HTML generation
    std::string GenerateFormHtml(const Config::ConfigSchemaDefinition& schema);
    std::string GenerateFieldHtml(const Config::SchemaField& field, const std::string& path);

    // Edit operation management
    void PushEdit(const EditOperation& op);

    // Validation
    void ValidateDocument();
    void UpdateErrorHighlighting();

    Editor* m_editor = nullptr;

    // Web view
    std::unique_ptr<WebView> m_webView;
    std::unique_ptr<JSBridge> m_bridge;

    // Schemas
    std::unordered_map<std::string, Config::ConfigSchemaDefinition> m_schemas;

    // Current document
    std::string m_currentTypeId;
    std::string m_documentId;
    std::string m_documentJson;
    bool m_isDirty = false;

    // Validation
    std::vector<ValidationError> m_errors;
    bool m_realtimeValidation = true;

    // Undo/Redo
    std::stack<EditOperation> m_undoStack;
    std::stack<EditOperation> m_redoStack;
    static constexpr size_t MAX_UNDO_HISTORY = 100;

    // Diff view
    bool m_showDiff = false;
    std::string m_diffJson;
    std::vector<DiffEntry> m_diffEntries;

    // UI state
    std::string m_focusedPath;
    std::unordered_map<std::string, bool> m_expandedSections;
};

} // namespace WebEditor
} // namespace Vehement
