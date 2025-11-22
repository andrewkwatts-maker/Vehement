#include "FreeTextConfigEditor.hpp"
#include "../web/JSBridge.hpp"
#include "../web/WebViewManager.hpp"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

namespace Vehement {

FreeTextConfigEditor::FreeTextConfigEditor() = default;

FreeTextConfigEditor::~FreeTextConfigEditor() {
    if (m_initialized) {
        Shutdown();
    }
}

bool FreeTextConfigEditor::Initialize(WebEditor::JSBridge& bridge, const Config& config) {
    if (m_initialized) {
        return false;
    }

    m_bridge = &bridge;
    m_config = config;

    RegisterBridgeFunctions();

    m_initialized = true;
    return true;
}

void FreeTextConfigEditor::Shutdown() {
    if (!m_initialized) {
        return;
    }

    CloseFile();
    m_initialized = false;
}

void FreeTextConfigEditor::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Delayed validation
    if (m_config.autoValidate && m_needsValidation) {
        m_validationTimer += deltaTime;
        if (m_validationTimer >= m_config.validationDelay) {
            UpdateValidation();
            m_validationTimer = 0.0f;
            m_needsValidation = false;
        }
    }
}

void FreeTextConfigEditor::Render() {
    if (!m_initialized) {
        return;
    }

    ImGui::Begin("Config Editor", nullptr, ImGuiWindowFlags_MenuBar);

    RenderMenuBar();
    RenderToolbar();

    // Main content area
    float contentWidth = ImGui::GetContentRegionAvail().x;
    float sidebarWidth = m_showOutline ? 200.0f : 0.0f;
    float editorWidth = contentWidth - sidebarWidth;

    // Editor area
    ImGui::BeginChild("EditorArea", ImVec2(editorWidth, -50), true);
    if (m_showDiff) {
        RenderDiffView();
    } else {
        RenderEditor();
    }
    ImGui::EndChild();

    // Side panel
    if (m_showOutline) {
        ImGui::SameLine();
        ImGui::BeginChild("SidePanel", ImVec2(sidebarWidth - 10, -50), true);
        RenderSidePanel();
        ImGui::EndChild();
    }

    // Bottom panels
    if (m_showErrors) {
        ImGui::BeginChild("ErrorPanel", ImVec2(0, 40), true);
        RenderErrorList();
        ImGui::EndChild();
    }

    if (m_showSearch) {
        RenderSearchPanel();
    }

    RenderStatusBar();

    ImGui::End();
}

void FreeTextConfigEditor::RenderWebEditor(WebEditor::WebViewManager& webViewManager) {
    if (!m_initialized) {
        return;
    }

    // Create web view if not exists
    if (!webViewManager.HasWebView(m_webViewId)) {
        WebEditor::WebViewConfig config;
        config.id = m_webViewId;
        config.title = "JSON Config Editor";
        config.width = 1200;
        config.height = 800;
        config.debug = true;

        auto* webView = webViewManager.CreateWebView(config);
        if (webView) {
            webView->LoadFile("editor/html/config_text_editor.html");
            webView->SetMessageHandler([this](const std::string& type, const std::string& payload) {
                if (type == "contentChanged") {
                    SetContent(payload, true);
                } else if (type == "save") {
                    SaveFile();
                } else if (type == "validate") {
                    UpdateValidation();
                }
            });
        }
    }

    webViewManager.RenderImGuiWindow(m_webViewId, "JSON Editor", nullptr);
}

void FreeTextConfigEditor::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) {
                NewFile();
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                // Would show file dialog
            }
            if (ImGui::MenuItem("Save", "Ctrl+S", false, m_currentState.isDirty)) {
                SaveFile();
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                // Would show file dialog
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close")) {
                CloseFile();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo())) {
                Undo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CanRedo())) {
                Redo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Find", "Ctrl+F")) {
                m_showSearch = !m_showSearch;
            }
            if (ImGui::MenuItem("Replace", "Ctrl+H")) {
                m_showSearch = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Format", "Alt+Shift+F")) {
                Format();
            }
            if (ImGui::MenuItem("Minify")) {
                Minify();
            }
            if (ImGui::MenuItem("Sort Keys")) {
                SortKeys();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Outline", nullptr, &m_showOutline);
            ImGui::MenuItem("Errors", nullptr, &m_showErrors);
            ImGui::MenuItem("Diff View", nullptr, &m_showDiff);
            ImGui::Separator();
            if (ImGui::MenuItem("Fold All")) {
                FoldAll();
            }
            if (ImGui::MenuItem("Unfold All")) {
                UnfoldAll();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Go")) {
            if (ImGui::MenuItem("Go to Line...", "Ctrl+G")) {
                ImGui::OpenPopup("GoToLine");
            }
            if (ImGui::MenuItem("Go to Definition", "F12")) {
                GoToDefinition();
            }
            if (ImGui::MenuItem("Find References", "Shift+F12")) {
                FindReferences();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // Go to line popup
    if (ImGui::BeginPopup("GoToLine")) {
        ImGui::Text("Go to Line:");
        if (ImGui::InputText("##Line", m_gotoLineBuffer, sizeof(m_gotoLineBuffer),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
            int line = std::atoi(m_gotoLineBuffer);
            GoToLine(line);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void FreeTextConfigEditor::RenderToolbar() {
    if (ImGui::Button("New")) {
        NewFile();
    }
    ImGui::SameLine();
    if (ImGui::Button("Open")) {
        // Would show file dialog
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        SaveFile();
    }
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    if (ImGui::Button("Undo")) {
        Undo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Redo")) {
        Redo();
    }
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    if (ImGui::Button("Format")) {
        Format();
    }
    ImGui::SameLine();
    if (ImGui::Button("Validate")) {
        UpdateValidation();
    }
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Validation status
    if (m_errors.empty()) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Valid");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                          "%zu error(s)", m_errors.size());
    }

    // Dirty indicator
    if (m_currentState.isDirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "*");
    }
}

void FreeTextConfigEditor::RenderEditor() {
    // Simple text editor - in production would use a proper editor widget
    auto lines = SplitLines(m_currentState.content);

    // Line numbers column
    ImGui::BeginChild("LineNumbers", ImVec2(50, 0), false);
    for (size_t i = 0; i < lines.size(); ++i) {
        // Check for errors on this line
        bool hasError = false;
        for (const auto& error : m_errors) {
            if (error.line == static_cast<int>(i + 1)) {
                hasError = true;
                break;
            }
        }

        if (hasError) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%4zu", i + 1);
        } else {
            ImGui::TextDisabled("%4zu", i + 1);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Editor content
    static char buffer[65536] = "";
    strncpy(buffer, m_currentState.content.c_str(), sizeof(buffer) - 1);

    ImGui::BeginChild("EditorContent", ImVec2(0, 0), false);
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput |
                                 ImGuiInputTextFlags_CallbackAlways;

    if (ImGui::InputTextMultiline("##Editor", buffer, sizeof(buffer),
                                   ImGui::GetContentRegionAvail(), flags)) {
        std::string newContent = buffer;
        if (newContent != m_currentState.content) {
            SetContent(newContent, true);
        }
    }
    ImGui::EndChild();
}

void FreeTextConfigEditor::RenderSidePanel() {
    if (ImGui::CollapsingHeader("Outline", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderOutline();
    }

    if (ImGui::CollapsingHeader("Schema")) {
        if (!m_currentState.schemaPath.empty()) {
            ImGui::Text("Schema: %s", m_currentState.schemaPath.c_str());
        } else {
            ImGui::TextDisabled("No schema loaded");
        }

        if (ImGui::Button("Set Schema...")) {
            // Would show schema selection dialog
        }
    }
}

void FreeTextConfigEditor::RenderOutline() {
    // Parse JSON and show tree structure
    // This is a simplified version
    auto lines = SplitLines(m_currentState.content);
    int indent = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];

        // Count braces for indent
        for (char c : line) {
            if (c == '{' || c == '[') indent++;
            if (c == '}' || c == ']') indent--;
        }

        // Find property names
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            size_t quoteStart = line.find('"');
            size_t quoteEnd = line.find('"', quoteStart + 1);
            if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                std::string key = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

                // Indent
                for (int j = 0; j < indent; ++j) {
                    ImGui::Text("  ");
                    ImGui::SameLine();
                }

                if (ImGui::Selectable(key.c_str())) {
                    GoToLine(static_cast<int>(i + 1));
                }
            }
        }
    }
}

void FreeTextConfigEditor::RenderErrorList() {
    ImGui::Text("Problems (%zu)", m_errors.size());
    ImGui::SameLine();
    ImGui::Separator();

    for (const auto& error : m_errors) {
        ImVec4 color;
        if (error.severity == "error") {
            color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        } else if (error.severity == "warning") {
            color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
        } else {
            color = ImVec4(0.5f, 0.7f, 1.0f, 1.0f);
        }

        ImGui::TextColored(color, "[%d:%d] %s",
                          error.line, error.column, error.message.c_str());

        if (ImGui::IsItemClicked()) {
            GoToPosition(error.line, error.column);
        }
    }
}

void FreeTextConfigEditor::RenderSearchPanel() {
    ImGui::BeginChild("SearchPanel", ImVec2(0, 80), true);

    ImGui::Text("Find:");
    ImGui::SameLine();
    if (ImGui::InputText("##Search", m_searchBuffer, sizeof(m_searchBuffer))) {
        m_searchQuery = m_searchBuffer;
        m_searchResults = Find(m_searchQuery, m_searchCaseSensitive,
                               m_searchRegex, m_searchWholeWord);
    }
    ImGui::SameLine();
    if (ImGui::Button("Find Next")) {
        FindNext();
    }
    ImGui::SameLine();
    if (ImGui::Button("Find Previous")) {
        FindPrevious();
    }

    ImGui::Text("Replace:");
    ImGui::SameLine();
    ImGui::InputText("##Replace", m_replaceBuffer, sizeof(m_replaceBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Replace")) {
        Replace(m_searchQuery, m_replaceBuffer, m_searchCaseSensitive, m_searchRegex);
    }
    ImGui::SameLine();
    if (ImGui::Button("Replace All")) {
        ReplaceAll(m_searchQuery, m_replaceBuffer, m_searchCaseSensitive, m_searchRegex);
    }

    ImGui::Checkbox("Case Sensitive", &m_searchCaseSensitive);
    ImGui::SameLine();
    ImGui::Checkbox("Regex", &m_searchRegex);
    ImGui::SameLine();
    ImGui::Checkbox("Whole Word", &m_searchWholeWord);

    if (!m_searchResults.empty()) {
        ImGui::SameLine();
        ImGui::Text("| %d / %zu results",
                   m_currentSearchIndex + 1, m_searchResults.size());
    }

    ImGui::EndChild();
}

void FreeTextConfigEditor::RenderDiffView() {
    auto diff = GetDiffAgainstSaved();

    for (const auto& change : diff) {
        ImVec4 color;
        std::string prefix;

        switch (change.type) {
            case DiffChange::Type::Added:
                color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
                prefix = "+ ";
                break;
            case DiffChange::Type::Removed:
                color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                prefix = "- ";
                break;
            case DiffChange::Type::Modified:
                color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                prefix = "~ ";
                break;
            default:
                color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                prefix = "  ";
                break;
        }

        ImGui::TextColored(color, "%s%4d: %s",
                          prefix.c_str(), change.lineNumber,
                          change.newContent.c_str());
    }
}

void FreeTextConfigEditor::RenderStatusBar() {
    ImGui::Text("Ln %d, Col %d | %s | %s",
               m_currentState.cursorLine + 1,
               m_currentState.cursorColumn + 1,
               m_currentState.filePath.empty() ? "Untitled" : m_currentState.filePath.c_str(),
               m_currentState.isDirty ? "Modified" : "Saved");
}

// File Operations
bool FreeTextConfigEditor::OpenFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    m_currentState.filePath = path;
    m_currentState.content = buffer.str();
    m_currentState.isDirty = false;
    m_savedContent = m_currentState.content;

    // Clear undo history for new file
    ClearUndoHistory();

    // Trigger validation
    m_needsValidation = true;

    return true;
}

bool FreeTextConfigEditor::SaveFile() {
    if (m_currentState.filePath.empty()) {
        return false;
    }

    std::ofstream file(m_currentState.filePath);
    if (!file.is_open()) {
        return false;
    }

    file << m_currentState.content;

    m_currentState.isDirty = false;
    m_savedContent = m_currentState.content;

    if (OnSaved) {
        OnSaved(m_currentState.filePath);
    }

    return true;
}

bool FreeTextConfigEditor::SaveFileAs(const std::string& path) {
    m_currentState.filePath = path;
    return SaveFile();
}

void FreeTextConfigEditor::CloseFile() {
    m_currentState = EditorState{};
    m_savedContent.clear();
    m_errors.clear();
    ClearUndoHistory();
}

void FreeTextConfigEditor::NewFile(const std::string& schemaPath) {
    CloseFile();
    m_currentState.content = "{\n  \n}";
    m_currentState.schemaPath = schemaPath;

    if (!schemaPath.empty()) {
        LoadSchema(schemaPath);
    }
}

// Content Operations
void FreeTextConfigEditor::SetContent(const std::string& content, bool recordUndo) {
    if (recordUndo && content != m_currentState.content) {
        RecordChange("Content changed");
    }

    m_currentState.content = content;
    m_currentState.isDirty = (content != m_savedContent);
    m_needsValidation = true;

    if (OnContentChanged) {
        OnContentChanged(content);
    }

    if (OnDirtyStateChanged) {
        OnDirtyStateChanged();
    }
}

void FreeTextConfigEditor::InsertText(const std::string& text) {
    // Insert at cursor position
    auto lines = SplitLines(m_currentState.content);

    if (m_currentState.cursorLine < static_cast<int>(lines.size())) {
        auto& line = lines[m_currentState.cursorLine];
        if (m_currentState.cursorColumn <= static_cast<int>(line.size())) {
            line.insert(m_currentState.cursorColumn, text);
            SetContent(JoinLines(lines), true);
        }
    }
}

void FreeTextConfigEditor::DeleteSelection() {
    // Simplified - just delete character before cursor
    auto lines = SplitLines(m_currentState.content);

    if (m_currentState.cursorLine < static_cast<int>(lines.size())) {
        auto& line = lines[m_currentState.cursorLine];
        if (m_currentState.cursorColumn > 0 &&
            m_currentState.cursorColumn <= static_cast<int>(line.size())) {
            line.erase(m_currentState.cursorColumn - 1, 1);
            m_currentState.cursorColumn--;
            SetContent(JoinLines(lines), true);
        }
    }
}

std::string FreeTextConfigEditor::GetSelectedText() const {
    // Simplified - return empty for now
    return "";
}

void FreeTextConfigEditor::SelectAll() {
    m_currentState.selectionStartLine = 0;
    m_currentState.selectionStartColumn = 0;
    auto lines = SplitLines(m_currentState.content);
    m_currentState.selectionEndLine = static_cast<int>(lines.size()) - 1;
    m_currentState.selectionEndColumn = static_cast<int>(lines.back().size());
}

// Formatting
bool FreeTextConfigEditor::Format() {
    // Simple JSON formatting
    std::string result;
    int indent = 0;
    bool inString = false;

    for (size_t i = 0; i < m_currentState.content.size(); ++i) {
        char c = m_currentState.content[i];

        if (c == '"' && (i == 0 || m_currentState.content[i - 1] != '\\')) {
            inString = !inString;
        }

        if (inString) {
            result += c;
            continue;
        }

        switch (c) {
            case '{':
            case '[':
                result += c;
                result += '\n';
                indent++;
                result += std::string(indent * m_config.tabSize, ' ');
                break;

            case '}':
            case ']':
                result += '\n';
                indent--;
                result += std::string(indent * m_config.tabSize, ' ');
                result += c;
                break;

            case ',':
                result += c;
                result += '\n';
                result += std::string(indent * m_config.tabSize, ' ');
                break;

            case ':':
                result += ": ";
                break;

            case ' ':
            case '\n':
            case '\t':
            case '\r':
                // Skip whitespace
                break;

            default:
                result += c;
                break;
        }
    }

    SetContent(result, true);
    return true;
}

bool FreeTextConfigEditor::Minify() {
    std::string result;
    bool inString = false;

    for (size_t i = 0; i < m_currentState.content.size(); ++i) {
        char c = m_currentState.content[i];

        if (c == '"' && (i == 0 || m_currentState.content[i - 1] != '\\')) {
            inString = !inString;
        }

        if (inString) {
            result += c;
        } else if (c != ' ' && c != '\n' && c != '\t' && c != '\r') {
            result += c;
        }
    }

    SetContent(result, true);
    return true;
}

bool FreeTextConfigEditor::SortKeys() {
    // Would need proper JSON parsing to sort keys
    return false;
}

// Validation
std::vector<ValidationError> FreeTextConfigEditor::Validate() {
    std::vector<ValidationError> errors;

    // Basic JSON syntax validation
    int braceCount = 0;
    int bracketCount = 0;
    bool inString = false;
    int line = 1;
    int column = 1;

    for (size_t i = 0; i < m_currentState.content.size(); ++i) {
        char c = m_currentState.content[i];

        if (c == '\n') {
            line++;
            column = 1;
            continue;
        }
        column++;

        if (c == '"' && (i == 0 || m_currentState.content[i - 1] != '\\')) {
            inString = !inString;
            continue;
        }

        if (!inString) {
            if (c == '{') braceCount++;
            else if (c == '}') braceCount--;
            else if (c == '[') bracketCount++;
            else if (c == ']') bracketCount--;

            if (braceCount < 0) {
                errors.push_back({line, column, "Unexpected '}'", "syntax", "error", ""});
            }
            if (bracketCount < 0) {
                errors.push_back({line, column, "Unexpected ']'", "syntax", "error", ""});
            }
        }
    }

    if (braceCount != 0) {
        errors.push_back({line, column, "Unmatched braces", "syntax", "error", ""});
    }
    if (bracketCount != 0) {
        errors.push_back({line, column, "Unmatched brackets", "syntax", "error", ""});
    }

    return errors;
}

void FreeTextConfigEditor::SetSchema(const std::string& schemaPath) {
    m_currentState.schemaPath = schemaPath;
    LoadSchema(schemaPath);
    m_needsValidation = true;
}

void FreeTextConfigEditor::UpdateValidation() {
    m_errors = Validate();

    if (OnValidation) {
        OnValidation(m_errors);
    }
}

// Auto-completion
std::vector<CompletionItem> FreeTextConfigEditor::GetCompletions(int line, int column) {
    std::vector<CompletionItem> completions;

    // Get JSON path at position
    std::string path = GetJsonPath(line, column);

    // Get schema properties for this path
    auto properties = GetSchemaProperties(path);

    for (const auto& prop : properties) {
        CompletionItem item;
        item.label = prop.name;
        item.kind = "property";
        item.detail = prop.type;
        item.documentation = prop.description;
        item.insertText = "\"" + prop.name + "\": ";

        // Add value based on type
        if (prop.type == "string") {
            item.insertText += "\"\"";
        } else if (prop.type == "number" || prop.type == "integer") {
            item.insertText += "0";
        } else if (prop.type == "boolean") {
            item.insertText += "false";
        } else if (prop.type == "object") {
            item.insertText += "{}";
        } else if (prop.type == "array") {
            item.insertText += "[]";
        }

        completions.push_back(item);
    }

    return completions;
}

void FreeTextConfigEditor::ApplyCompletion(const CompletionItem& item) {
    InsertText(item.insertText);
}

std::string FreeTextConfigEditor::GetHoverInfo(int line, int column) {
    std::string path = GetJsonPath(line, column);
    auto properties = GetSchemaProperties(path);

    for (const auto& prop : properties) {
        // Find matching property
        // Return its documentation
    }

    return "";
}

// Search and Replace
std::vector<SearchResult> FreeTextConfigEditor::Find(const std::string& query,
                                                       bool caseSensitive,
                                                       bool useRegex,
                                                       bool wholeWord) {
    std::vector<SearchResult> results;

    if (query.empty()) {
        return results;
    }

    auto lines = SplitLines(m_currentState.content);
    std::string searchQuery = query;

    if (!caseSensitive && !useRegex) {
        std::transform(searchQuery.begin(), searchQuery.end(),
                      searchQuery.begin(), ::tolower);
    }

    for (size_t lineNum = 0; lineNum < lines.size(); ++lineNum) {
        std::string line = lines[lineNum];
        std::string searchLine = line;

        if (!caseSensitive && !useRegex) {
            std::transform(searchLine.begin(), searchLine.end(),
                          searchLine.begin(), ::tolower);
        }

        size_t pos = 0;
        while ((pos = searchLine.find(searchQuery, pos)) != std::string::npos) {
            SearchResult result;
            result.filePath = m_currentState.filePath;
            result.line = static_cast<int>(lineNum + 1);
            result.column = static_cast<int>(pos + 1);
            result.length = static_cast<int>(query.size());
            result.lineContent = line;
            result.matchText = line.substr(pos, query.size());
            results.push_back(result);
            pos += query.size();
        }
    }

    return results;
}

std::vector<SearchResult> FreeTextConfigEditor::FindInFiles(
    const std::string& query,
    const std::vector<std::string>& paths) {

    std::vector<SearchResult> results;

    for (const auto& path : paths) {
        std::ifstream file(path);
        if (!file.is_open()) continue;

        std::string line;
        int lineNum = 0;

        while (std::getline(file, line)) {
            lineNum++;
            size_t pos = line.find(query);
            if (pos != std::string::npos) {
                SearchResult result;
                result.filePath = path;
                result.line = lineNum;
                result.column = static_cast<int>(pos + 1);
                result.length = static_cast<int>(query.size());
                result.lineContent = line;
                result.matchText = query;
                results.push_back(result);
            }
        }
    }

    return results;
}

int FreeTextConfigEditor::Replace(const std::string& query,
                                    const std::string& replacement,
                                    bool caseSensitive,
                                    bool useRegex) {
    if (m_currentSearchIndex < 0 ||
        m_currentSearchIndex >= static_cast<int>(m_searchResults.size())) {
        return 0;
    }

    auto& result = m_searchResults[m_currentSearchIndex];
    auto lines = SplitLines(m_currentState.content);

    if (result.line > 0 && result.line <= static_cast<int>(lines.size())) {
        auto& line = lines[result.line - 1];
        line.replace(result.column - 1, result.length, replacement);
        SetContent(JoinLines(lines), true);

        // Update search results
        m_searchResults = Find(query, caseSensitive, useRegex, m_searchWholeWord);
        return 1;
    }

    return 0;
}

int FreeTextConfigEditor::ReplaceAll(const std::string& query,
                                       const std::string& replacement,
                                       bool caseSensitive,
                                       bool useRegex) {
    std::string content = m_currentState.content;
    int count = 0;

    if (useRegex) {
        try {
            std::regex pattern(query, caseSensitive ?
                              std::regex_constants::ECMAScript :
                              std::regex_constants::icase);
            content = std::regex_replace(content, pattern, replacement);
            count = static_cast<int>(m_searchResults.size());
        } catch (...) {
            return 0;
        }
    } else {
        size_t pos = 0;
        while ((pos = content.find(query, pos)) != std::string::npos) {
            content.replace(pos, query.size(), replacement);
            pos += replacement.size();
            count++;
        }
    }

    SetContent(content, true);
    m_searchResults.clear();
    m_currentSearchIndex = -1;
    return count;
}

void FreeTextConfigEditor::FindNext() {
    if (m_searchResults.empty()) return;

    m_currentSearchIndex++;
    if (m_currentSearchIndex >= static_cast<int>(m_searchResults.size())) {
        m_currentSearchIndex = 0;
    }

    auto& result = m_searchResults[m_currentSearchIndex];
    GoToPosition(result.line, result.column);
}

void FreeTextConfigEditor::FindPrevious() {
    if (m_searchResults.empty()) return;

    m_currentSearchIndex--;
    if (m_currentSearchIndex < 0) {
        m_currentSearchIndex = static_cast<int>(m_searchResults.size()) - 1;
    }

    auto& result = m_searchResults[m_currentSearchIndex];
    GoToPosition(result.line, result.column);
}

// Undo/Redo
void FreeTextConfigEditor::Undo() {
    if (m_undoStack.empty()) return;

    auto change = m_undoStack.back();
    m_undoStack.pop_back();

    m_redoStack.push_back({
        m_currentState.content,
        change.before,
        m_currentState.cursorLine,
        m_currentState.cursorColumn,
        change.description,
        std::chrono::steady_clock::now()
    });

    m_currentState.content = change.before;
    m_currentState.cursorLine = change.cursorLine;
    m_currentState.cursorColumn = change.cursorColumn;
    m_currentState.isDirty = (m_currentState.content != m_savedContent);
    m_needsValidation = true;
}

void FreeTextConfigEditor::Redo() {
    if (m_redoStack.empty()) return;

    auto change = m_redoStack.back();
    m_redoStack.pop_back();

    m_undoStack.push_back({
        m_currentState.content,
        change.before,
        m_currentState.cursorLine,
        m_currentState.cursorColumn,
        change.description,
        std::chrono::steady_clock::now()
    });

    m_currentState.content = change.before;
    m_currentState.isDirty = (m_currentState.content != m_savedContent);
    m_needsValidation = true;
}

std::vector<std::string> FreeTextConfigEditor::GetUndoHistory() const {
    std::vector<std::string> history;
    for (const auto& change : m_undoStack) {
        history.push_back(change.description);
    }
    return history;
}

void FreeTextConfigEditor::ClearUndoHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
}

// Diff
std::vector<DiffChange> FreeTextConfigEditor::GetDiffAgainstSaved() {
    return ComputeDiff(m_savedContent, m_currentState.content);
}

std::vector<DiffChange> FreeTextConfigEditor::GetDiffAgainstFile(const std::string& otherPath) {
    std::ifstream file(otherPath);
    if (!file.is_open()) return {};

    std::stringstream buffer;
    buffer << file.rdbuf();
    return ComputeDiff(buffer.str(), m_currentState.content);
}

std::vector<DiffChange> FreeTextConfigEditor::ComputeDiff(const std::string& oldText,
                                                           const std::string& newText) {
    std::vector<DiffChange> diff;

    auto oldLines = SplitLines(oldText);
    auto newLines = SplitLines(newText);

    // Simple line-by-line diff
    size_t maxLines = std::max(oldLines.size(), newLines.size());

    for (size_t i = 0; i < maxLines; ++i) {
        DiffChange change;
        change.lineNumber = static_cast<int>(i + 1);

        if (i >= oldLines.size()) {
            change.type = DiffChange::Type::Added;
            change.newContent = newLines[i];
        } else if (i >= newLines.size()) {
            change.type = DiffChange::Type::Removed;
            change.oldContent = oldLines[i];
        } else if (oldLines[i] != newLines[i]) {
            change.type = DiffChange::Type::Modified;
            change.oldContent = oldLines[i];
            change.newContent = newLines[i];
        } else {
            change.type = DiffChange::Type::Unchanged;
            change.oldContent = oldLines[i];
            change.newContent = newLines[i];
        }

        diff.push_back(change);
    }

    return diff;
}

// Folding
void FreeTextConfigEditor::FoldRange(int startLine) {
    m_currentState.collapsedRanges.push_back(startLine);
}

void FreeTextConfigEditor::UnfoldRange(int startLine) {
    auto& ranges = m_currentState.collapsedRanges;
    ranges.erase(std::remove(ranges.begin(), ranges.end(), startLine), ranges.end());
}

void FreeTextConfigEditor::FoldAll() {
    auto ranges = GetFoldableRanges();
    m_currentState.collapsedRanges.clear();
    for (const auto& range : ranges) {
        m_currentState.collapsedRanges.push_back(range.first);
    }
}

void FreeTextConfigEditor::UnfoldAll() {
    m_currentState.collapsedRanges.clear();
}

std::vector<std::pair<int, int>> FreeTextConfigEditor::GetFoldableRanges() const {
    std::vector<std::pair<int, int>> ranges;
    std::vector<int> stack;

    auto lines = SplitLines(m_currentState.content);

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];

        for (char c : line) {
            if (c == '{' || c == '[') {
                stack.push_back(static_cast<int>(i));
            } else if (c == '}' || c == ']') {
                if (!stack.empty()) {
                    int start = stack.back();
                    stack.pop_back();
                    if (static_cast<int>(i) > start) {
                        ranges.push_back({start + 1, static_cast<int>(i) + 1});
                    }
                }
            }
        }
    }

    return ranges;
}

// Navigation
void FreeTextConfigEditor::GoToLine(int line) {
    m_currentState.cursorLine = line - 1;
    m_currentState.cursorColumn = 0;
}

void FreeTextConfigEditor::GoToPosition(int line, int column) {
    m_currentState.cursorLine = line - 1;
    m_currentState.cursorColumn = column - 1;
}

void FreeTextConfigEditor::GoToDefinition() {
    // Find $ref at cursor and navigate to it
}

std::vector<SearchResult> FreeTextConfigEditor::FindReferences() {
    // Find all references to definition at cursor
    return {};
}

std::string FreeTextConfigEditor::GetOutline() const {
    return "";  // Would return tree structure as JSON
}

// Private helpers
void FreeTextConfigEditor::LoadSchema(const std::string& path) {
    std::ifstream file(m_config.schemaBasePath + path);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        m_schemaContent = buffer.str();
    }
}

std::vector<SchemaProperty> FreeTextConfigEditor::GetSchemaProperties(const std::string& jsonPath) {
    std::vector<SchemaProperty> properties;
    // Would parse schema and return properties for given path
    return properties;
}

std::string FreeTextConfigEditor::GetJsonPath(int line, int column) {
    // Would parse JSON and return path like "$.properties.name"
    return "";
}

std::pair<int, int> FreeTextConfigEditor::GetPositionFromPath(const std::string& path) {
    return {1, 1};
}

void FreeTextConfigEditor::RegisterBridgeFunctions() {
    if (!m_bridge) return;

    m_bridge->RegisterFunction("configEditor.getContent", [this](const auto&) {
        return WebEditor::JSResult::Success(WebEditor::JSValue(m_currentState.content));
    });

    m_bridge->RegisterFunction("configEditor.setContent", [this](const auto& args) {
        if (!args.empty()) {
            SetContent(args[0].GetString(), true);
        }
        return WebEditor::JSResult::Success();
    });

    m_bridge->RegisterFunction("configEditor.validate", [this](const auto&) {
        UpdateValidation();
        WebEditor::JSValue::Array errors;
        for (const auto& error : m_errors) {
            WebEditor::JSValue::Object obj;
            obj["line"] = error.line;
            obj["column"] = error.column;
            obj["message"] = error.message;
            obj["severity"] = error.severity;
            errors.push_back(WebEditor::JSValue(std::move(obj)));
        }
        return WebEditor::JSResult::Success(WebEditor::JSValue(std::move(errors)));
    });

    m_bridge->RegisterFunction("configEditor.format", [this](const auto&) {
        Format();
        return WebEditor::JSResult::Success(WebEditor::JSValue(m_currentState.content));
    });

    m_bridge->RegisterFunction("configEditor.getCompletions", [this](const auto& args) {
        if (args.size() < 2) {
            return WebEditor::JSResult::Error("Missing line/column");
        }
        auto completions = GetCompletions(args[0].GetInt(), args[1].GetInt());
        WebEditor::JSValue::Array result;
        for (const auto& item : completions) {
            WebEditor::JSValue::Object obj;
            obj["label"] = item.label;
            obj["insertText"] = item.insertText;
            obj["kind"] = item.kind;
            obj["detail"] = item.detail;
            result.push_back(WebEditor::JSValue(std::move(obj)));
        }
        return WebEditor::JSResult::Success(WebEditor::JSValue(std::move(result)));
    });
}

std::vector<std::string> FreeTextConfigEditor::SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    if (lines.empty()) {
        lines.push_back("");
    }
    return lines;
}

std::string FreeTextConfigEditor::JoinLines(const std::vector<std::string>& lines) {
    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) result += '\n';
        result += lines[i];
    }
    return result;
}

void FreeTextConfigEditor::RecordChange(const std::string& description) {
    DocumentChange change;
    change.before = m_currentState.content;
    change.cursorLine = m_currentState.cursorLine;
    change.cursorColumn = m_currentState.cursorColumn;
    change.description = description;
    change.timestamp = std::chrono::steady_clock::now();

    m_undoStack.push_back(change);
    m_redoStack.clear();

    // Trim undo history
    while (m_undoStack.size() > static_cast<size_t>(m_config.maxUndoHistory)) {
        m_undoStack.pop_front();
    }
}

} // namespace Vehement
