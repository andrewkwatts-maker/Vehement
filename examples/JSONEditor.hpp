#pragma once

#include "AssetEditor.hpp"
#include <imgui.h>
#include <string>

/**
 * @brief JSON file editor
 *
 * Features:
 * - Multi-line text editor with basic syntax highlighting
 * - Tree view mode for structured navigation
 * - JSON validation on save
 * - Format/Pretty-print button
 * - Find and replace
 * - Error highlighting for invalid JSON
 */
class JSONEditor : public IAssetEditor {
public:
    JSONEditor();
    ~JSONEditor() override;

    void Open(const std::string& assetPath) override;
    void Render(bool* isOpen) override;
    std::string GetEditorName() const override;
    bool IsDirty() const override;
    void Save() override;
    std::string GetAssetPath() const override;

private:
    void RenderTextEditor();
    void RenderTreeView();
    void RenderToolbar();
    void LoadJSON();
    bool ValidateJSON();
    void FormatJSON();
    void ParseJSONToTree();

    std::string m_assetPath;
    std::string m_fileName;
    bool m_isDirty = false;
    bool m_isLoaded = false;

    // Editor content
    std::string m_content;
    std::string m_originalContent;

    // Validation
    bool m_isValid = true;
    std::string m_errorMessage;
    int m_errorLine = -1;

    // View mode
    enum class ViewMode {
        Text,
        Tree
    };
    ViewMode m_viewMode = ViewMode::Text;

    // UI state
    bool m_autoValidate = true;
    bool m_showLineNumbers = true;
    int m_fontSize = 14;

    // Text editor buffer (ImGui requires char buffer)
    static constexpr size_t BUFFER_SIZE = 1024 * 1024; // 1MB
    char* m_textBuffer = nullptr;

    // Undo/Redo stacks
    std::vector<std::string> m_undoStack;
    std::vector<std::string> m_redoStack;
    static constexpr size_t MAX_UNDO_LEVELS = 50;
    void PushUndoState();
    void Undo();
    void Redo();
    void RenderJSONNode(struct JSONNode& node, int depth = 0);

    // Tree view structure (simplified)
    struct JSONNode {
        std::string key;
        std::string value;
        std::string type; // "object", "array", "string", "number", "boolean", "null"
        std::vector<JSONNode> children;
        bool expanded = false;
    };
    JSONNode m_rootNode;
};
