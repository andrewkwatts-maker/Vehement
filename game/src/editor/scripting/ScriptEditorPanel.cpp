#include "ScriptEditorPanel.hpp"
#include "../Editor.hpp"
#include "engine/scripting/PythonEngine.hpp"
#include "engine/scripting/ScriptValidator.hpp"
#include "engine/scripting/ScriptStorage.hpp"
#include "engine/scripting/GameAPI.hpp"

#include <imgui.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <regex>
#include <cctype>

namespace Vehement {

// Python keywords
const std::vector<std::string> ScriptEditorPanel::s_pythonKeywords = {
    "False", "None", "True", "and", "as", "assert", "async", "await",
    "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
    "while", "with", "yield"
};

// Python builtins
const std::vector<std::string> ScriptEditorPanel::s_pythonBuiltins = {
    "abs", "all", "any", "ascii", "bin", "bool", "bytearray", "bytes",
    "callable", "chr", "classmethod", "compile", "complex", "delattr",
    "dict", "dir", "divmod", "enumerate", "eval", "exec", "filter",
    "float", "format", "frozenset", "getattr", "globals", "hasattr",
    "hash", "help", "hex", "id", "input", "int", "isinstance", "issubclass",
    "iter", "len", "list", "locals", "map", "max", "memoryview", "min",
    "next", "object", "oct", "open", "ord", "pow", "print", "property",
    "range", "repr", "reversed", "round", "set", "setattr", "slice",
    "sorted", "staticmethod", "str", "sum", "super", "tuple", "type",
    "vars", "zip", "__import__"
};

ScriptEditorPanel::ScriptEditorPanel() {
    // Build keyword map for fast lookup
    for (const auto& kw : s_pythonKeywords) {
        m_keywordMap[kw] = TokenType::Keyword;
    }
    for (const auto& bi : s_pythonBuiltins) {
        m_keywordMap[bi] = TokenType::Builtin;
    }
}

ScriptEditorPanel::~ScriptEditorPanel() {
    Shutdown();
}

bool ScriptEditorPanel::Initialize(Editor* editor) {
    if (m_initialized) return true;

    m_editor = editor;

    // Get Python engine reference
    m_pythonEngine = &Nova::Scripting::PythonEngine::Instance();

    // Build auto-completion index
    BuildCompletionIndex();

    // Add built-in completions
    for (const auto& kw : s_pythonKeywords) {
        CompletionItem item;
        item.text = kw;
        item.displayText = kw;
        item.description = "Python keyword";
        item.category = "Keywords";
        item.priority = 100;
        m_builtinCompletions.push_back(item);
    }

    for (const auto& bi : s_pythonBuiltins) {
        CompletionItem item;
        item.text = bi;
        item.displayText = bi + "()";
        item.description = "Python builtin function";
        item.category = "Builtins";
        item.priority = 90;
        m_builtinCompletions.push_back(item);
    }

    m_initialized = true;
    return true;
}

void ScriptEditorPanel::Shutdown() {
    if (!m_initialized) return;

    // Prompt to save unsaved files
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].modified) {
            // In real implementation, would show save dialog
        }
    }

    m_tabs.clear();
    m_currentTab = -1;
    m_consoleMessages.clear();
    m_initialized = false;
}

void ScriptEditorPanel::Render() {
    if (!m_initialized) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("Script Editor", nullptr, ImGuiWindowFlags_MenuBar)) {
        ImGui::PopStyleVar();

        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Script", "Ctrl+N")) {
                    NewFile();
                }
                if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                    // Would show file dialog
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save", "Ctrl+S", false, m_currentTab >= 0)) {
                    SaveCurrentFile();
                }
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, m_currentTab >= 0)) {
                    // Would show save dialog
                }
                if (ImGui::MenuItem("Save All", "Ctrl+Alt+S")) {
                    SaveAllFiles();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Close", "Ctrl+W", false, m_currentTab >= 0)) {
                    CloseTab();
                }
                if (ImGui::MenuItem("Close All")) {
                    CloseAllTabs();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z")) Undo();
                if (ImGui::MenuItem("Redo", "Ctrl+Y")) Redo();
                ImGui::Separator();
                if (ImGui::MenuItem("Cut", "Ctrl+X")) Cut();
                if (ImGui::MenuItem("Copy", "Ctrl+C")) Copy();
                if (ImGui::MenuItem("Paste", "Ctrl+V")) Paste();
                ImGui::Separator();
                if (ImGui::MenuItem("Find", "Ctrl+F")) ShowFindDialog();
                if (ImGui::MenuItem("Replace", "Ctrl+H")) ShowReplaceDialog();
                if (ImGui::MenuItem("Go to Line", "Ctrl+G")) m_showGoToLineDialog = true;
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Line Numbers", nullptr, &m_showLineNumbers);
                ImGui::MenuItem("Minimap", nullptr, &m_showMinimap);
                ImGui::MenuItem("Word Wrap", nullptr, &m_wordWrap);
                ImGui::Separator();
                ImGui::MenuItem("Console", nullptr, &m_showConsole);
                ImGui::Separator();
                if (ImGui::MenuItem("Fold All")) FoldAll();
                if (ImGui::MenuItem("Unfold All")) UnfoldAll();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Run")) {
                if (ImGui::MenuItem("Run Script", "F5", false, m_currentTab >= 0 && !m_scriptRunning)) {
                    RunScript();
                }
                if (ImGui::MenuItem("Test Script", "F6", false, m_currentTab >= 0)) {
                    TestScript();
                }
                if (ImGui::MenuItem("Stop", "Shift+F5", false, m_scriptRunning)) {
                    StopScript();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Validate", "F7", false, m_currentTab >= 0)) {
                    ValidateScript();
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        // Toolbar
        RenderToolbar();

        // Main content area with splitter
        float consoleHeight = m_showConsole ? 150.0f : 0.0f;
        float editorHeight = ImGui::GetContentRegionAvail().y - consoleHeight;

        // Tab bar and editor
        ImGui::BeginChild("EditorRegion", ImVec2(0, editorHeight), false);
        RenderTabBar();

        if (m_currentTab >= 0 && m_currentTab < static_cast<int>(m_tabs.size())) {
            RenderEditor();
        } else {
            // Empty state
            ImVec2 center = ImVec2(
                ImGui::GetWindowPos().x + ImGui::GetWindowWidth() * 0.5f,
                ImGui::GetWindowPos().y + ImGui::GetWindowHeight() * 0.5f
            );
            ImGui::SetCursorScreenPos(ImVec2(center.x - 150, center.y - 50));
            ImGui::BeginGroup();
            ImGui::TextDisabled("No file open");
            ImGui::Spacing();
            if (ImGui::Button("New Script", ImVec2(120, 0))) NewFile();
            ImGui::SameLine();
            if (ImGui::Button("Open File", ImVec2(120, 0))) {
                // Would show file dialog
            }
            ImGui::EndGroup();
        }
        ImGui::EndChild();

        // Console
        if (m_showConsole) {
            RenderConsole();
        }

        // Find/Replace bar
        if (m_showFindBar || m_showReplaceBar) {
            RenderFindReplaceBar();
        }

        // Dialogs
        if (m_showSaveConfirmDialog) RenderSaveConfirmDialog();
        if (m_showGoToLineDialog) RenderGoToLineDialog();

        // Auto-complete popup
        if (m_showAutoComplete) {
            RenderAutoCompletePopup();
        }
    } else {
        ImGui::PopStyleVar();
    }
    ImGui::End();
}

void ScriptEditorPanel::Update(float deltaTime) {
    if (!m_initialized) return;

    // Handle validation timer
    if (m_validationPending) {
        m_validationTimer -= deltaTime;
        if (m_validationTimer <= 0.0f) {
            RunValidation();
            m_validationPending = false;
        }
    }

    // Handle keyboard shortcuts
    HandleKeyInput();
}

void ScriptEditorPanel::RenderToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));

    // New file
    if (ImGui::Button("New")) NewFile();
    ImGui::SameLine();

    // Save
    ImGui::BeginDisabled(m_currentTab < 0);
    if (ImGui::Button("Save")) SaveCurrentFile();
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Run controls
    ImGui::BeginDisabled(m_currentTab < 0 || m_scriptRunning);
    if (ImGui::Button("Run")) RunScript();
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(!m_scriptRunning);
    if (ImGui::Button("Stop")) StopScript();
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(m_currentTab < 0);
    if (ImGui::Button("Test")) TestScript();
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Validate
    ImGui::BeginDisabled(m_currentTab < 0);
    if (ImGui::Button("Validate")) ValidateScript();
    ImGui::EndDisabled();

    // Show error count if any
    if (m_currentTab >= 0 && m_currentTab < static_cast<int>(m_tabs.size())) {
        const auto& tab = m_tabs[m_currentTab];
        int errorCount = 0, warningCount = 0;
        for (const auto& diag : tab.diagnostics) {
            if (diag.severity == CodeDiagnostic::Severity::Error) errorCount++;
            else if (diag.severity == CodeDiagnostic::Severity::Warning) warningCount++;
        }

        if (errorCount > 0 || warningCount > 0) {
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();

            if (errorCount > 0) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::Text("%d errors", errorCount);
                ImGui::PopStyleColor();
                ImGui::SameLine();
            }
            if (warningCount > 0) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
                ImGui::Text("%d warnings", warningCount);
                ImGui::PopStyleColor();
            }
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::Separator();
}

void ScriptEditorPanel::RenderTabBar() {
    if (ImGui::BeginTabBar("ScriptTabs", ImGuiTabBarFlags_Reorderable |
                           ImGuiTabBarFlags_AutoSelectNewTabs |
                           ImGuiTabBarFlags_TabListPopupButton)) {
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            auto& tab = m_tabs[i];

            ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
            if (tab.modified) {
                flags |= ImGuiTabItemFlags_UnsavedDocument;
            }

            bool open = true;
            std::string tabLabel = tab.fileName + (tab.modified ? "*" : "") + "###tab" + std::to_string(i);

            if (ImGui::BeginTabItem(tabLabel.c_str(), &open, flags)) {
                m_currentTab = static_cast<int>(i);
                ImGui::EndTabItem();
            }

            // Handle tab close
            if (!open) {
                CloseTab(static_cast<int>(i));
            }

            // Tooltip with full path
            if (ImGui::IsItemHovered() && !tab.filePath.empty()) {
                ImGui::SetTooltip("%s", tab.filePath.c_str());
            }
        }
        ImGui::EndTabBar();
    }
}

void ScriptEditorPanel::RenderEditor() {
    if (m_currentTab < 0 || m_currentTab >= static_cast<int>(m_tabs.size())) return;

    auto& tab = m_tabs[m_currentTab];

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    float availWidth = ImGui::GetContentRegionAvail().x;
    float minimapWidth = m_showMinimap ? 100.0f : 0.0f;
    float lineNumWidth = m_showLineNumbers ? 50.0f : 0.0f;
    float codeWidth = availWidth - minimapWidth - lineNumWidth;

    // Line numbers
    if (m_showLineNumbers) {
        ImGui::BeginChild("LineNumbers", ImVec2(lineNumWidth, 0), false,
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        RenderLineNumbers();
        ImGui::EndChild();
        ImGui::SameLine();
    }

    // Code area
    ImGui::BeginChild("CodeArea", ImVec2(codeWidth, 0), false);
    RenderCodeArea();
    ImGui::EndChild();

    // Minimap
    if (m_showMinimap) {
        ImGui::SameLine();
        ImGui::BeginChild("Minimap", ImVec2(minimapWidth, 0), true);
        RenderMinimap();
        ImGui::EndChild();
    }

    ImGui::PopStyleVar();
}

void ScriptEditorPanel::RenderLineNumbers() {
    if (m_currentTab < 0) return;

    auto& tab = m_tabs[m_currentTab];
    int lineCount = GetLineCount();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

    for (int i = 1; i <= lineCount; ++i) {
        // Check for breakpoint
        bool hasBreakpoint = false;
        for (const auto& bp : tab.breakpoints) {
            if (bp.line == i) {
                hasBreakpoint = true;
                break;
            }
        }

        // Check for error on this line
        bool hasError = false;
        for (const auto& diag : tab.diagnostics) {
            if (diag.line == i && diag.severity == CodeDiagnostic::Severity::Error) {
                hasError = true;
                break;
            }
        }

        // Draw breakpoint marker or line number
        if (hasBreakpoint) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::Text(" @ %3d", i);
            ImGui::PopStyleColor();
        } else if (hasError) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::Text(" ! %3d", i);
            ImGui::PopStyleColor();
        } else {
            ImGui::Text("   %3d", i);
        }

        // Handle click to toggle breakpoint
        if (ImGui::IsItemClicked()) {
            ToggleBreakpoint(i);
        }
    }

    ImGui::PopStyleColor();
}

void ScriptEditorPanel::RenderCodeArea() {
    if (m_currentTab < 0) return;

    auto& tab = m_tabs[m_currentTab];

    // Create text editor with syntax highlighting
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput |
                                ImGuiInputTextFlags_CallbackAlways |
                                ImGuiInputTextFlags_CallbackCompletion;

    // Use a buffer for the text input
    static std::string editBuffer;
    editBuffer = tab.content;
    editBuffer.resize(editBuffer.size() + 1024 * 1024);  // Allow growth

    ImVec2 size = ImGui::GetContentRegionAvail();

    // Custom callback for syntax highlighting and auto-complete
    auto callback = [](ImGuiInputTextCallbackData* data) -> int {
        ScriptEditorPanel* panel = static_cast<ScriptEditorPanel*>(data->UserData);

        if (data->EventFlag == ImGuiInputTextCallbackFlags_CallbackCompletion) {
            panel->TriggerAutoComplete();
        }

        return 0;
    };

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.12f, 1.0f));
    ImGui::PushFont(nullptr);  // Would use monospace font

    if (ImGui::InputTextMultiline("##code", editBuffer.data(), editBuffer.size(),
                                  size, flags, callback, this)) {
        // Content changed
        tab.content = editBuffer.c_str();
        MarkModified();
        ScheduleValidation();
        TokenizeContent(tab);

        if (m_onContentChanged) {
            m_onContentChanged(tab.content);
        }
    }

    ImGui::PopFont();
    ImGui::PopStyleColor();

    // Draw error underlines (overlay)
    // In a full implementation, we would draw squiggly underlines
    // under error positions using ImGui draw list
}

void ScriptEditorPanel::RenderMinimap() {
    if (m_currentTab < 0) return;

    auto& tab = m_tabs[m_currentTab];

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();

    // Background
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                           IM_COL32(30, 30, 35, 255));

    // Draw miniature code representation
    float lineHeight = 2.0f;
    float y = pos.y;

    std::istringstream stream(tab.content);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line) && y < pos.y + size.y) {
        // Simple representation - different colors for different token types
        float x = pos.x + 2;
        float lineWidth = std::min(static_cast<float>(line.length()), size.x - 4);

        // Check if this line has errors
        bool hasError = false;
        for (const auto& diag : tab.diagnostics) {
            if (diag.line == lineNum + 1 && diag.severity == CodeDiagnostic::Severity::Error) {
                hasError = true;
                break;
            }
        }

        uint32_t color = hasError ? IM_COL32(255, 80, 80, 180) : IM_COL32(150, 150, 150, 100);
        drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + lineWidth, y + lineHeight), color);

        y += lineHeight + 1;
        lineNum++;
    }

    // Draw visible region indicator
    // Would calculate based on scroll position
    float viewportHeight = 50.0f;
    float viewportY = pos.y;  // Would be based on scroll
    drawList->AddRect(ImVec2(pos.x, viewportY),
                     ImVec2(pos.x + size.x, viewportY + viewportHeight),
                     IM_COL32(100, 150, 255, 100), 0.0f, 0, 2.0f);
}

void ScriptEditorPanel::RenderConsole() {
    ImGui::BeginChild("Console", ImVec2(0, 0), true);

    // Console header
    ImGui::Text("Output");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
    if (ImGui::SmallButton("Clear")) {
        ClearConsole();
    }
    ImGui::Separator();

    // Console messages
    ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false);

    for (const auto& msg : m_consoleMessages) {
        ImVec4 color;
        const char* prefix = "";

        switch (msg.type) {
            case ConsoleMessage::Type::Error:
                color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                prefix = "[ERROR] ";
                break;
            case ConsoleMessage::Type::Warning:
                color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                prefix = "[WARN] ";
                break;
            case ConsoleMessage::Type::Debug:
                color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                prefix = "[DEBUG] ";
                break;
            case ConsoleMessage::Type::Output:
                color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
                prefix = "";
                break;
            default:
                color = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
                prefix = "";
                break;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped("%s%s", prefix, msg.text.c_str());
        ImGui::PopStyleColor();
    }

    // Auto-scroll
    if (m_consoleAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::EndChild();
}

void ScriptEditorPanel::RenderFindReplaceBar() {
    ImGui::BeginChild("FindReplaceBar", ImVec2(0, m_showReplaceBar ? 60 : 30), true);

    ImGui::PushItemWidth(200);

    // Find field
    if (ImGui::InputText("Find", &m_findOptions.searchText[0], 256,
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
        FindNext();
    }
    ImGui::SameLine();

    if (ImGui::Button("Find Next")) FindNext();
    ImGui::SameLine();
    if (ImGui::Button("Find Prev")) FindPrevious();
    ImGui::SameLine();

    ImGui::Checkbox("Case", &m_findOptions.caseSensitive);
    ImGui::SameLine();
    ImGui::Checkbox("Regex", &m_findOptions.useRegex);
    ImGui::SameLine();
    ImGui::Checkbox("Word", &m_findOptions.wholeWord);

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
    if (ImGui::SmallButton("X")) {
        m_showFindBar = false;
        m_showReplaceBar = false;
    }

    // Replace field (if showing)
    if (m_showReplaceBar) {
        if (ImGui::InputText("Replace", &m_findOptions.replaceText[0], 256)) {
            // Update replace text
        }
        ImGui::SameLine();

        if (ImGui::Button("Replace")) Replace();
        ImGui::SameLine();
        if (ImGui::Button("Replace All")) {
            int count = ReplaceAll();
            AddConsoleMessage({ConsoleMessage::Type::Info,
                             "Replaced " + std::to_string(count) + " occurrences",
                             "Find/Replace",
                             std::chrono::system_clock::now()});
        }
    }

    ImGui::PopItemWidth();
    ImGui::EndChild();
}

void ScriptEditorPanel::RenderAutoCompletePopup() {
    if (m_filteredCompletions.empty()) {
        m_showAutoComplete = false;
        return;
    }

    ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
    ImGui::SetNextWindowSize(ImVec2(300, 200));

    if (ImGui::Begin("##AutoComplete", nullptr,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {

        for (size_t i = 0; i < m_filteredCompletions.size() && i < 10; ++i) {
            const auto& item = m_filteredCompletions[i];

            bool isSelected = static_cast<int>(i) == m_selectedCompletion;

            if (ImGui::Selectable(item.displayText.c_str(), isSelected)) {
                m_selectedCompletion = static_cast<int>(i);
                AcceptAutoComplete();
            }

            // Show description on hover
            if (ImGui::IsItemHovered() && !item.description.empty()) {
                ImGui::SetTooltip("%s\n\n%s", item.signature.c_str(), item.description.c_str());
            }
        }
    }
    ImGui::End();
}

void ScriptEditorPanel::RenderStatusBar() {
    if (m_currentTab < 0) return;

    auto& tab = m_tabs[m_currentTab];

    ImGui::Text("Ln %d, Col %d", tab.cursorLine + 1, tab.cursorColumn + 1);
    ImGui::SameLine();
    ImGui::Text(" | ");
    ImGui::SameLine();
    ImGui::Text("UTF-8");
    ImGui::SameLine();
    ImGui::Text(" | ");
    ImGui::SameLine();
    ImGui::Text("Python");
}

void ScriptEditorPanel::RenderSaveConfirmDialog() {
    ImGui::OpenPopup("Save Changes?");

    if (ImGui::BeginPopupModal("Save Changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Do you want to save changes before closing?");
        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(100, 0))) {
            SaveCurrentFile();
            CloseTab(m_pendingCloseTab, true);
            m_showSaveConfirmDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        if (ImGui::Button("Don't Save", ImVec2(100, 0))) {
            CloseTab(m_pendingCloseTab, true);
            m_showSaveConfirmDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_showSaveConfirmDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ScriptEditorPanel::RenderGoToLineDialog() {
    ImGui::OpenPopup("Go to Line");

    if (ImGui::BeginPopupModal("Go to Line", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputInt("Line number", &m_goToLineNumber);

        if (ImGui::Button("Go", ImVec2(100, 0))) {
            GoToLine(m_goToLineNumber);
            m_showGoToLineDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_showGoToLineDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// File operations
int ScriptEditorPanel::NewFile(const std::string& templateName) {
    EditorTab tab;
    tab.fileName = "untitled.py";
    tab.isNew = true;
    tab.modified = true;

    // Load template if specified
    if (!templateName.empty()) {
        // Would load from ScriptTemplate system
        tab.content = "# New Python Script\n\ndef main():\n    pass\n\nif __name__ == \"__main__\":\n    main()\n";
    } else {
        tab.content = "# New Python Script\n\n";
    }

    tab.originalContent = tab.content;
    TokenizeContent(tab);
    DetectFoldRegions(tab);

    m_tabs.push_back(std::move(tab));
    m_currentTab = static_cast<int>(m_tabs.size()) - 1;

    return m_currentTab;
}

bool ScriptEditorPanel::OpenFile(const std::string& filePath) {
    // Check if already open
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].filePath == filePath) {
            m_currentTab = static_cast<int>(i);
            return true;
        }
    }

    // Read file
    std::ifstream file(filePath);
    if (!file.is_open()) {
        AddConsoleMessage({ConsoleMessage::Type::Error,
                         "Failed to open file: " + filePath,
                         "File",
                         std::chrono::system_clock::now()});
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    EditorTab tab;
    tab.filePath = filePath;
    tab.fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    tab.content = buffer.str();
    tab.originalContent = tab.content;
    tab.modified = false;
    tab.isNew = false;

    TokenizeContent(tab);
    DetectFoldRegions(tab);

    m_tabs.push_back(std::move(tab));
    m_currentTab = static_cast<int>(m_tabs.size()) - 1;

    if (m_onFileOpened) {
        m_onFileOpened(filePath);
    }

    // Validate the newly opened file
    ScheduleValidation();

    return true;
}

bool ScriptEditorPanel::SaveCurrentFile() {
    if (m_currentTab < 0 || m_currentTab >= static_cast<int>(m_tabs.size())) {
        return false;
    }

    auto& tab = m_tabs[m_currentTab];

    if (tab.isNew || tab.filePath.empty()) {
        // Would show save-as dialog
        return SaveFileAs("scripts/" + tab.fileName);
    }

    std::ofstream file(tab.filePath);
    if (!file.is_open()) {
        AddConsoleMessage({ConsoleMessage::Type::Error,
                         "Failed to save file: " + tab.filePath,
                         "File",
                         std::chrono::system_clock::now()});
        return false;
    }

    file << tab.content;
    file.close();

    tab.modified = false;
    tab.isNew = false;
    tab.originalContent = tab.content;

    AddConsoleMessage({ConsoleMessage::Type::Info,
                     "Saved: " + tab.filePath,
                     "File",
                     std::chrono::system_clock::now()});

    if (m_onFileSaved) {
        m_onFileSaved(tab.filePath);
    }

    return true;
}

bool ScriptEditorPanel::SaveFileAs(const std::string& filePath) {
    if (m_currentTab < 0) return false;

    auto& tab = m_tabs[m_currentTab];
    tab.filePath = filePath;
    tab.fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    tab.isNew = false;

    return SaveCurrentFile();
}

int ScriptEditorPanel::SaveAllFiles() {
    int savedCount = 0;
    int originalTab = m_currentTab;

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].modified) {
            m_currentTab = static_cast<int>(i);
            if (SaveCurrentFile()) {
                savedCount++;
            }
        }
    }

    m_currentTab = originalTab;
    return savedCount;
}

bool ScriptEditorPanel::CloseTab(int tabIndex, bool force) {
    if (tabIndex < 0) tabIndex = m_currentTab;
    if (tabIndex < 0 || tabIndex >= static_cast<int>(m_tabs.size())) {
        return false;
    }

    auto& tab = m_tabs[tabIndex];

    // Check for unsaved changes
    if (!force && tab.modified) {
        m_pendingCloseTab = tabIndex;
        m_showSaveConfirmDialog = true;
        return false;
    }

    m_tabs.erase(m_tabs.begin() + tabIndex);

    // Adjust current tab
    if (m_currentTab >= static_cast<int>(m_tabs.size())) {
        m_currentTab = static_cast<int>(m_tabs.size()) - 1;
    }

    return true;
}

bool ScriptEditorPanel::CloseAllTabs(bool force) {
    if (!force && HasUnsavedChanges()) {
        // Would prompt to save
    }

    m_tabs.clear();
    m_currentTab = -1;
    return true;
}

bool ScriptEditorPanel::HasUnsavedChanges() const {
    for (const auto& tab : m_tabs) {
        if (tab.modified) return true;
    }
    return false;
}

void ScriptEditorPanel::SetCurrentTab(int index) {
    if (index >= 0 && index < static_cast<int>(m_tabs.size())) {
        m_currentTab = index;
    }
}

const EditorTab* ScriptEditorPanel::GetTab(int index) const {
    if (index >= 0 && index < static_cast<int>(m_tabs.size())) {
        return &m_tabs[index];
    }
    return nullptr;
}

// Editing
void ScriptEditorPanel::SetContent(const std::string& content) {
    if (m_currentTab < 0) return;
    m_tabs[m_currentTab].content = content;
    TokenizeContent(m_tabs[m_currentTab]);
    MarkModified();
}

std::string ScriptEditorPanel::GetContent() const {
    if (m_currentTab < 0) return "";
    return m_tabs[m_currentTab].content;
}

void ScriptEditorPanel::Undo() {
    if (m_undoStack.empty()) return;

    auto action = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    // Reverse the action
    // Would implement proper undo logic

    m_redoStack.push_back(std::move(action));
}

void ScriptEditorPanel::Redo() {
    if (m_redoStack.empty()) return;

    auto action = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    // Re-apply the action
    // Would implement proper redo logic

    m_undoStack.push_back(std::move(action));
}

void ScriptEditorPanel::GoToLine(int line) {
    if (m_currentTab < 0) return;

    auto& tab = m_tabs[m_currentTab];
    int maxLine = GetLineCount();

    line = std::clamp(line, 1, maxLine);
    tab.cursorLine = line - 1;
    tab.cursorColumn = 0;

    EnsureCursorVisible();
}

void ScriptEditorPanel::SetCursorPosition(int line, int column) {
    if (m_currentTab < 0) return;

    auto& tab = m_tabs[m_currentTab];
    tab.cursorLine = line;
    tab.cursorColumn = column;

    EnsureCursorVisible();
}

void ScriptEditorPanel::GetCursorPosition(int& line, int& column) const {
    if (m_currentTab < 0) {
        line = column = 0;
        return;
    }

    const auto& tab = m_tabs[m_currentTab];
    line = tab.cursorLine;
    column = tab.cursorColumn;
}

// Find/Replace
void ScriptEditorPanel::ShowFindDialog() {
    m_showFindBar = true;
    m_showReplaceBar = false;
}

void ScriptEditorPanel::ShowReplaceDialog() {
    m_showFindBar = true;
    m_showReplaceBar = true;
}

bool ScriptEditorPanel::FindNext() {
    if (m_currentTab < 0 || m_findOptions.searchText.empty()) return false;

    const auto& tab = m_tabs[m_currentTab];
    const std::string& content = tab.content;
    const std::string& search = m_findOptions.searchText;

    size_t startPos = 0;  // Would start from cursor position
    size_t pos;

    if (m_findOptions.useRegex) {
        std::regex pattern(search, m_findOptions.caseSensitive ?
                          std::regex::ECMAScript : std::regex::icase);
        std::smatch match;
        std::string::const_iterator searchStart(content.cbegin() + startPos);
        if (std::regex_search(searchStart, content.cend(), match, pattern)) {
            pos = match.position() + startPos;
        } else {
            return false;
        }
    } else {
        if (m_findOptions.caseSensitive) {
            pos = content.find(search, startPos);
        } else {
            std::string lowerContent = content;
            std::string lowerSearch = search;
            std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);
            std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);
            pos = lowerContent.find(lowerSearch, startPos);
        }
    }

    if (pos != std::string::npos) {
        // Calculate line and column from position
        int line = 0, col = 0;
        for (size_t i = 0; i < pos; ++i) {
            if (content[i] == '\n') {
                line++;
                col = 0;
            } else {
                col++;
            }
        }
        SetCursorPosition(line, col);
        return true;
    }

    return false;
}

bool ScriptEditorPanel::FindPrevious() {
    // Similar to FindNext but searches backwards
    return false;  // Placeholder
}

bool ScriptEditorPanel::Replace() {
    if (m_currentTab < 0) return false;
    // Would implement replace at current match
    return false;
}

int ScriptEditorPanel::ReplaceAll() {
    if (m_currentTab < 0) return 0;

    auto& tab = m_tabs[m_currentTab];
    int count = 0;

    if (m_findOptions.useRegex) {
        std::regex pattern(m_findOptions.searchText,
                          m_findOptions.caseSensitive ?
                          std::regex::ECMAScript : std::regex::icase);
        std::string result = std::regex_replace(tab.content, pattern, m_findOptions.replaceText);

        // Count replacements
        std::sregex_iterator begin(tab.content.begin(), tab.content.end(), pattern);
        std::sregex_iterator end;
        count = std::distance(begin, end);

        tab.content = result;
    } else {
        // Simple string replacement
        std::string& content = tab.content;
        const std::string& search = m_findOptions.searchText;
        const std::string& replace = m_findOptions.replaceText;

        size_t pos = 0;
        while ((pos = content.find(search, pos)) != std::string::npos) {
            content.replace(pos, search.length(), replace);
            pos += replace.length();
            count++;
        }
    }

    if (count > 0) {
        MarkModified();
        TokenizeContent(tab);
    }

    return count;
}

// Code Folding
void ScriptEditorPanel::ToggleFold(int line) {
    if (m_currentTab < 0) return;

    auto& tab = m_tabs[m_currentTab];
    for (auto& region : tab.foldRegions) {
        if (region.startLine == line) {
            region.folded = !region.folded;
            break;
        }
    }
}

void ScriptEditorPanel::FoldAll() {
    if (m_currentTab < 0) return;

    auto& tab = m_tabs[m_currentTab];
    for (auto& region : tab.foldRegions) {
        region.folded = true;
    }
}

void ScriptEditorPanel::UnfoldAll() {
    if (m_currentTab < 0) return;

    auto& tab = m_tabs[m_currentTab];
    for (auto& region : tab.foldRegions) {
        region.folded = false;
    }
}

void ScriptEditorPanel::UpdateFoldRegions() {
    if (m_currentTab < 0) return;
    DetectFoldRegions(m_tabs[m_currentTab]);
}

// Breakpoints
void ScriptEditorPanel::ToggleBreakpoint(int line) {
    if (m_currentTab < 0) return;

    auto& tab = m_tabs[m_currentTab];

    // Check if breakpoint exists
    for (auto it = tab.breakpoints.begin(); it != tab.breakpoints.end(); ++it) {
        if (it->line == line) {
            tab.breakpoints.erase(it);
            return;
        }
    }

    // Add new breakpoint
    Breakpoint bp;
    bp.line = line;
    bp.enabled = true;
    tab.breakpoints.push_back(bp);
}

void ScriptEditorPanel::AddBreakpoint(const Breakpoint& breakpoint) {
    if (m_currentTab < 0) return;
    m_tabs[m_currentTab].breakpoints.push_back(breakpoint);
}

void ScriptEditorPanel::RemoveBreakpoint(int line) {
    if (m_currentTab < 0) return;

    auto& tab = m_tabs[m_currentTab];
    tab.breakpoints.erase(
        std::remove_if(tab.breakpoints.begin(), tab.breakpoints.end(),
                      [line](const Breakpoint& bp) { return bp.line == line; }),
        tab.breakpoints.end()
    );
}

void ScriptEditorPanel::ClearBreakpoints() {
    if (m_currentTab < 0) return;
    m_tabs[m_currentTab].breakpoints.clear();
}

std::vector<Breakpoint> ScriptEditorPanel::GetBreakpoints() const {
    if (m_currentTab < 0) return {};
    return m_tabs[m_currentTab].breakpoints;
}

// Auto-completion
void ScriptEditorPanel::TriggerAutoComplete() {
    if (m_currentTab < 0) return;

    std::string word = GetWordAtCursor();
    if (word.empty()) {
        HideAutoComplete();
        return;
    }

    m_completionPrefix = word;
    m_filteredCompletions = GetCompletionsForContext(word);
    m_selectedCompletion = 0;
    m_showAutoComplete = !m_filteredCompletions.empty();
}

void ScriptEditorPanel::HideAutoComplete() {
    m_showAutoComplete = false;
    m_filteredCompletions.clear();
}

void ScriptEditorPanel::AcceptAutoComplete() {
    if (!m_showAutoComplete || m_filteredCompletions.empty()) return;
    if (m_selectedCompletion < 0 || m_selectedCompletion >= static_cast<int>(m_filteredCompletions.size())) return;

    const auto& item = m_filteredCompletions[m_selectedCompletion];

    // Insert completion (would replace prefix with full text)
    InsertText(item.text.substr(m_completionPrefix.length()));

    HideAutoComplete();
}

void ScriptEditorPanel::RegisterCompletionItems(const std::vector<CompletionItem>& items) {
    m_customCompletions.insert(m_customCompletions.end(), items.begin(), items.end());
}

// Diagnostics
bool ScriptEditorPanel::ValidateScript() {
    if (m_currentTab < 0) return false;

    auto& tab = m_tabs[m_currentTab];
    tab.diagnostics.clear();

    // Basic syntax validation
    // In a full implementation, would use the ScriptValidator

    // Check for basic Python syntax errors
    int parenCount = 0, bracketCount = 0, braceCount = 0;
    bool inString = false;
    char stringChar = 0;
    int lineNum = 1;

    for (size_t i = 0; i < tab.content.length(); ++i) {
        char c = tab.content[i];

        if (c == '\n') {
            lineNum++;
            continue;
        }

        if (!inString) {
            if (c == '"' || c == '\'') {
                inString = true;
                stringChar = c;
            } else if (c == '(') parenCount++;
            else if (c == ')') parenCount--;
            else if (c == '[') bracketCount++;
            else if (c == ']') bracketCount--;
            else if (c == '{') braceCount++;
            else if (c == '}') braceCount--;
        } else {
            if (c == stringChar && (i == 0 || tab.content[i-1] != '\\')) {
                inString = false;
            }
        }
    }

    // Add diagnostics for mismatched brackets
    if (parenCount != 0) {
        CodeDiagnostic diag;
        diag.line = lineNum;
        diag.column = 0;
        diag.severity = CodeDiagnostic::Severity::Error;
        diag.message = "Mismatched parentheses";
        diag.source = "syntax";
        tab.diagnostics.push_back(diag);
    }

    if (bracketCount != 0) {
        CodeDiagnostic diag;
        diag.line = lineNum;
        diag.column = 0;
        diag.severity = CodeDiagnostic::Severity::Error;
        diag.message = "Mismatched square brackets";
        diag.source = "syntax";
        tab.diagnostics.push_back(diag);
    }

    if (braceCount != 0) {
        CodeDiagnostic diag;
        diag.line = lineNum;
        diag.column = 0;
        diag.severity = CodeDiagnostic::Severity::Error;
        diag.message = "Mismatched curly braces";
        diag.source = "syntax";
        tab.diagnostics.push_back(diag);
    }

    if (inString) {
        CodeDiagnostic diag;
        diag.line = lineNum;
        diag.column = 0;
        diag.severity = CodeDiagnostic::Severity::Error;
        diag.message = "Unterminated string literal";
        diag.source = "syntax";
        tab.diagnostics.push_back(diag);
    }

    bool isValid = tab.diagnostics.empty();

    AddConsoleMessage({
        isValid ? ConsoleMessage::Type::Info : ConsoleMessage::Type::Error,
        isValid ? "Validation passed" : "Found " + std::to_string(tab.diagnostics.size()) + " issues",
        "Validator",
        std::chrono::system_clock::now()
    });

    return isValid;
}

std::vector<CodeDiagnostic> ScriptEditorPanel::GetDiagnostics() const {
    if (m_currentTab < 0) return {};
    return m_tabs[m_currentTab].diagnostics;
}

void ScriptEditorPanel::GoToNextError() {
    if (m_currentTab < 0) return;

    const auto& tab = m_tabs[m_currentTab];
    if (tab.diagnostics.empty()) return;

    // Find next error after current line
    for (const auto& diag : tab.diagnostics) {
        if (diag.line > tab.cursorLine + 1 &&
            diag.severity == CodeDiagnostic::Severity::Error) {
            GoToLine(diag.line);
            return;
        }
    }

    // Wrap around to first error
    for (const auto& diag : tab.diagnostics) {
        if (diag.severity == CodeDiagnostic::Severity::Error) {
            GoToLine(diag.line);
            return;
        }
    }
}

void ScriptEditorPanel::GoToPreviousError() {
    if (m_currentTab < 0) return;

    const auto& tab = m_tabs[m_currentTab];
    if (tab.diagnostics.empty()) return;

    // Find previous error before current line
    for (auto it = tab.diagnostics.rbegin(); it != tab.diagnostics.rend(); ++it) {
        if (it->line < tab.cursorLine + 1 &&
            it->severity == CodeDiagnostic::Severity::Error) {
            GoToLine(it->line);
            return;
        }
    }
}

// Execution
ExecutionResult ScriptEditorPanel::RunScript() {
    ExecutionResult result;
    result.success = false;

    if (m_currentTab < 0) {
        result.error = "No script open";
        return result;
    }

    const auto& tab = m_tabs[m_currentTab];

    // Validate first
    if (!ValidateScript()) {
        result.error = "Script has validation errors";
        return result;
    }

    m_scriptRunning = true;
    m_scriptStartTime = std::chrono::system_clock::now();

    AddConsoleMessage({ConsoleMessage::Type::Info,
                      "Running script: " + tab.fileName,
                      "Runtime",
                      std::chrono::system_clock::now()});

    // Execute script
    if (m_pythonEngine && m_pythonEngine->IsInitialized()) {
        auto scriptResult = m_pythonEngine->ExecuteString(tab.content, tab.fileName);

        result.success = scriptResult.success;
        if (!scriptResult.success) {
            result.error = scriptResult.errorMessage;
            AddConsoleMessage({ConsoleMessage::Type::Error,
                              scriptResult.errorMessage,
                              "Runtime",
                              std::chrono::system_clock::now()});
        } else {
            AddConsoleMessage({ConsoleMessage::Type::Output,
                              "Script completed successfully",
                              "Runtime",
                              std::chrono::system_clock::now()});
        }
    } else {
        result.error = "Python engine not initialized";
        AddConsoleMessage({ConsoleMessage::Type::Error,
                          result.error,
                          "Runtime",
                          std::chrono::system_clock::now()});
    }

    auto endTime = std::chrono::system_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(
        endTime - m_scriptStartTime).count();

    m_scriptRunning = false;

    return result;
}

ExecutionResult ScriptEditorPanel::TestScript() {
    ExecutionResult result;

    // Test mode - validate without side effects
    AddConsoleMessage({ConsoleMessage::Type::Info,
                      "Testing script (dry run)...",
                      "Test",
                      std::chrono::system_clock::now()});

    result.success = ValidateScript();

    if (result.success) {
        AddConsoleMessage({ConsoleMessage::Type::Output,
                          "Test passed - script is valid",
                          "Test",
                          std::chrono::system_clock::now()});
    } else {
        result.error = "Test failed - script has errors";
    }

    return result;
}

void ScriptEditorPanel::StopScript() {
    if (!m_scriptRunning) return;

    // Would interrupt Python execution
    m_scriptRunning = false;

    AddConsoleMessage({ConsoleMessage::Type::Warning,
                      "Script execution stopped by user",
                      "Runtime",
                      std::chrono::system_clock::now()});
}

// Console
void ScriptEditorPanel::ClearConsole() {
    m_consoleMessages.clear();
}

void ScriptEditorPanel::AddConsoleMessage(const ConsoleMessage& message) {
    m_consoleMessages.push_back(message);

    // Limit message count
    while (m_consoleMessages.size() > MAX_CONSOLE_MESSAGES) {
        m_consoleMessages.erase(m_consoleMessages.begin());
    }
}

// Settings
void ScriptEditorPanel::SetFontSize(float size) {
    m_fontSize = std::clamp(size, 8.0f, 32.0f);
}

void ScriptEditorPanel::SetTabSize(int size) {
    m_tabSize = std::clamp(size, 1, 8);
}

void ScriptEditorPanel::SetShowLineNumbers(bool show) {
    m_showLineNumbers = show;
}

void ScriptEditorPanel::SetWordWrap(bool wrap) {
    m_wordWrap = wrap;
}

void ScriptEditorPanel::SetAutoIndent(bool enable) {
    m_autoIndent = enable;
}

// Private helpers
void ScriptEditorPanel::TokenizeLine(const std::string& line, int lineNum,
                                     std::vector<SyntaxToken>& tokens) {
    size_t i = 0;
    while (i < line.length()) {
        // Skip whitespace
        while (i < line.length() && std::isspace(line[i])) i++;
        if (i >= line.length()) break;

        SyntaxToken token;
        token.start = i;

        // Comment
        if (line[i] == '#') {
            token.type = TokenType::Comment;
            token.length = line.length() - i;
            tokens.push_back(token);
            break;
        }

        // String
        if (line[i] == '"' || line[i] == '\'') {
            char quote = line[i];
            token.type = TokenType::String;
            i++;
            while (i < line.length() && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < line.length()) i++;
                i++;
            }
            if (i < line.length()) i++;
            token.length = i - token.start;
            tokens.push_back(token);
            continue;
        }

        // Number
        if (std::isdigit(line[i]) || (line[i] == '.' && i + 1 < line.length() && std::isdigit(line[i + 1]))) {
            token.type = TokenType::Number;
            while (i < line.length() && (std::isdigit(line[i]) || line[i] == '.' ||
                   line[i] == 'e' || line[i] == 'E' || line[i] == 'x' || line[i] == 'X' ||
                   (line[i] >= 'a' && line[i] <= 'f') || (line[i] >= 'A' && line[i] <= 'F'))) {
                i++;
            }
            token.length = i - token.start;
            tokens.push_back(token);
            continue;
        }

        // Identifier or keyword
        if (std::isalpha(line[i]) || line[i] == '_') {
            size_t start = i;
            while (i < line.length() && (std::isalnum(line[i]) || line[i] == '_')) i++;

            std::string word = line.substr(start, i - start);

            // Check if keyword/builtin
            auto it = m_keywordMap.find(word);
            if (it != m_keywordMap.end()) {
                token.type = it->second;
            } else if (word == "self") {
                token.type = TokenType::Variable;
            } else {
                // Check if it's followed by '(' - function call
                size_t j = i;
                while (j < line.length() && std::isspace(line[j])) j++;
                if (j < line.length() && line[j] == '(') {
                    token.type = TokenType::Function;
                } else {
                    token.type = TokenType::Variable;
                }
            }

            token.length = i - start;
            tokens.push_back(token);
            continue;
        }

        // Decorator
        if (line[i] == '@') {
            token.type = TokenType::Decorator;
            i++;
            while (i < line.length() && (std::isalnum(line[i]) || line[i] == '_')) i++;
            token.length = i - token.start;
            tokens.push_back(token);
            continue;
        }

        // Operators
        if (std::strchr("+-*/%=<>!&|^~", line[i])) {
            token.type = TokenType::Operator;
            token.length = 1;
            // Handle multi-char operators
            if (i + 1 < line.length()) {
                std::string op = line.substr(i, 2);
                if (op == "==" || op == "!=" || op == "<=" || op == ">=" ||
                    op == "+=" || op == "-=" || op == "*=" || op == "/=" ||
                    op == "**" || op == "//" || op == "<<" || op == ">>" ||
                    op == "&&" || op == "||" || op == "->") {
                    token.length = 2;
                }
            }
            i += token.length;
            tokens.push_back(token);
            continue;
        }

        // Other characters
        i++;
    }
}

void ScriptEditorPanel::TokenizeContent(EditorTab& tab) {
    tab.tokens.clear();

    std::istringstream stream(tab.content);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        TokenizeLine(line, lineNum, tab.tokens);
        lineNum++;
    }
}

uint32_t ScriptEditorPanel::GetTokenColor(TokenType type) const {
    switch (type) {
        case TokenType::Keyword:   return IM_COL32(86, 156, 214, 255);   // Blue
        case TokenType::Builtin:   return IM_COL32(220, 220, 170, 255);  // Yellow
        case TokenType::String:    return IM_COL32(206, 145, 120, 255);  // Orange
        case TokenType::Number:    return IM_COL32(181, 206, 168, 255);  // Green
        case TokenType::Comment:   return IM_COL32(106, 153, 85, 255);   // Dark green
        case TokenType::Operator:  return IM_COL32(212, 212, 212, 255);  // Light gray
        case TokenType::Decorator: return IM_COL32(220, 220, 170, 255);  // Yellow
        case TokenType::Function:  return IM_COL32(220, 220, 170, 255);  // Yellow
        case TokenType::Class:     return IM_COL32(78, 201, 176, 255);   // Cyan
        case TokenType::Variable:  return IM_COL32(156, 220, 254, 255);  // Light blue
        case TokenType::GameAPI:   return IM_COL32(197, 134, 192, 255);  // Purple
        case TokenType::Error:     return IM_COL32(255, 80, 80, 255);    // Red
        default:                   return IM_COL32(212, 212, 212, 255);  // Default
    }
}

void ScriptEditorPanel::BuildCompletionIndex() {
    // Add game API completions
    m_gameAPICompletions = {
        {"spawn_entity", "spawn_entity(type, x, y, z)", "Spawn a new entity at position", "spawn_entity(type: str, x: float, y: float, z: float) -> int", "Entity", 200, true},
        {"get_entity", "get_entity(id)", "Get entity by ID", "get_entity(id: int) -> Entity", "Entity", 200, true},
        {"despawn_entity", "despawn_entity(id)", "Remove entity from world", "despawn_entity(id: int) -> None", "Entity", 200, true},
        {"get_position", "get_position(entity_id)", "Get entity position", "get_position(entity_id: int) -> Vec3", "Entity", 200, true},
        {"set_position", "set_position(entity_id, x, y, z)", "Set entity position", "set_position(entity_id: int, x: float, y: float, z: float) -> None", "Entity", 200, true},
        {"damage", "damage(target_id, amount, source_id)", "Apply damage to entity", "damage(target_id: int, amount: float, source_id: int = 0) -> None", "Combat", 200, true},
        {"heal", "heal(target_id, amount)", "Heal entity", "heal(target_id: int, amount: float) -> None", "Combat", 200, true},
        {"get_health", "get_health(entity_id)", "Get entity health", "get_health(entity_id: int) -> float", "Combat", 200, true},
        {"is_alive", "is_alive(entity_id)", "Check if entity is alive", "is_alive(entity_id: int) -> bool", "Combat", 200, true},
        {"find_entities_in_radius", "find_entities_in_radius(x, y, z, radius)", "Find entities within radius", "find_entities_in_radius(x: float, y: float, z: float, radius: float) -> List[int]", "Query", 200, true},
        {"get_distance", "get_distance(e1, e2)", "Get distance between entities", "get_distance(entity1: int, entity2: int) -> float", "Query", 200, true},
        {"play_sound", "play_sound(name, x, y, z)", "Play sound at position", "play_sound(name: str, x: float = 0, y: float = 0, z: float = 0) -> None", "Audio", 200, true},
        {"spawn_effect", "spawn_effect(name, x, y, z)", "Spawn visual effect", "spawn_effect(name: str, x: float, y: float, z: float) -> None", "Visual", 200, true},
        {"show_notification", "show_notification(message, duration)", "Show UI notification", "show_notification(message: str, duration: float = 3.0) -> None", "UI", 200, true},
        {"get_delta_time", "get_delta_time()", "Get frame delta time", "get_delta_time() -> float", "Time", 200, true},
        {"get_game_time", "get_game_time()", "Get total game time", "get_game_time() -> float", "Time", 200, true},
        {"random", "random()", "Get random float 0-1", "random() -> float", "Math", 200, true},
        {"random_range", "random_range(min, max)", "Get random in range", "random_range(min: float, max: float) -> float", "Math", 200, true},
        {"log", "log(message)", "Log message to console", "log(message: str) -> None", "Debug", 200, true},
        {"log_warning", "log_warning(message)", "Log warning", "log_warning(message: str) -> None", "Debug", 200, true},
        {"log_error", "log_error(message)", "Log error", "log_error(message: str) -> None", "Debug", 200, true},
    };
}

std::vector<CompletionItem> ScriptEditorPanel::GetCompletionsForContext(const std::string& prefix) {
    std::vector<CompletionItem> results;

    std::string lowerPrefix = prefix;
    std::transform(lowerPrefix.begin(), lowerPrefix.end(), lowerPrefix.begin(), ::tolower);

    // Search all completion sources
    auto searchCompletions = [&](const std::vector<CompletionItem>& items) {
        for (const auto& item : items) {
            std::string lowerText = item.text;
            std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);

            if (lowerText.find(lowerPrefix) == 0) {
                results.push_back(item);
            }
        }
    };

    searchCompletions(m_gameAPICompletions);
    searchCompletions(m_builtinCompletions);
    searchCompletions(m_customCompletions);

    // Sort by priority and alphabetically
    std::sort(results.begin(), results.end(), [](const CompletionItem& a, const CompletionItem& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.text < b.text;
    });

    return results;
}

void ScriptEditorPanel::DetectFoldRegions(EditorTab& tab) {
    tab.foldRegions.clear();

    std::vector<std::pair<int, int>> indentStack;  // line, indent

    std::istringstream stream(tab.content);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        // Calculate indent
        int indent = 0;
        for (char c : line) {
            if (c == ' ') indent++;
            else if (c == '\t') indent += m_tabSize;
            else break;
        }

        // Check for fold-starting keywords
        std::string trimmed = line;
        size_t start = trimmed.find_first_not_of(" \t");
        if (start != std::string::npos) {
            trimmed = trimmed.substr(start);
        }

        bool startsFold = false;
        if (trimmed.find("def ") == 0 || trimmed.find("class ") == 0 ||
            trimmed.find("if ") == 0 || trimmed.find("for ") == 0 ||
            trimmed.find("while ") == 0 || trimmed.find("try:") == 0 ||
            trimmed.find("with ") == 0) {
            startsFold = true;
        }

        if (startsFold) {
            indentStack.push_back({lineNum, indent});
        }

        // Close regions when indent decreases
        while (!indentStack.empty() && indent <= indentStack.back().second && lineNum > indentStack.back().first) {
            FoldRegion region;
            region.startLine = indentStack.back().first;
            region.endLine = lineNum - 1;
            region.folded = false;

            // Get preview text
            std::istringstream previewStream(tab.content);
            std::string previewLine;
            for (int i = 0; i <= region.startLine && std::getline(previewStream, previewLine); i++) {}
            region.preview = previewLine;

            if (region.endLine > region.startLine) {
                tab.foldRegions.push_back(region);
            }

            indentStack.pop_back();
        }

        lineNum++;
    }

    // Close remaining regions
    for (auto it = indentStack.rbegin(); it != indentStack.rend(); ++it) {
        FoldRegion region;
        region.startLine = it->first;
        region.endLine = lineNum - 1;
        region.folded = false;
        if (region.endLine > region.startLine) {
            tab.foldRegions.push_back(region);
        }
    }
}

void ScriptEditorPanel::HandleKeyInput() {
    // Handle keyboard shortcuts
    ImGuiIO& io = ImGui::GetIO();

    bool ctrl = io.KeyCtrl;
    bool shift = io.KeyShift;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) NewFile();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        if (shift) SaveFileAs("");
        else SaveCurrentFile();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_W)) CloseTab();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_F)) ShowFindDialog();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_H)) ShowReplaceDialog();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_G)) m_showGoToLineDialog = true;

    if (ImGui::IsKeyPressed(ImGuiKey_F5) && !shift) RunScript();
    if (ImGui::IsKeyPressed(ImGuiKey_F5) && shift) StopScript();
    if (ImGui::IsKeyPressed(ImGuiKey_F6)) TestScript();
    if (ImGui::IsKeyPressed(ImGuiKey_F7)) ValidateScript();

    // Auto-complete navigation
    if (m_showAutoComplete) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            m_selectedCompletion = std::min(m_selectedCompletion + 1,
                                           static_cast<int>(m_filteredCompletions.size()) - 1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            m_selectedCompletion = std::max(m_selectedCompletion - 1, 0);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Tab)) {
            AcceptAutoComplete();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            HideAutoComplete();
        }
    }
}

void ScriptEditorPanel::InsertText(const std::string& text) {
    if (m_currentTab < 0) return;

    auto& tab = m_tabs[m_currentTab];

    // Would insert at cursor position
    // For now, just append
    tab.content += text;
    MarkModified();
    TokenizeContent(tab);
}

void ScriptEditorPanel::ScheduleValidation() {
    m_validationPending = true;
    m_validationTimer = m_validationDelay;
}

void ScriptEditorPanel::RunValidation() {
    ValidateScript();
}

std::string ScriptEditorPanel::GetLineText(int line) const {
    if (m_currentTab < 0) return "";

    const auto& tab = m_tabs[m_currentTab];
    std::istringstream stream(tab.content);
    std::string lineText;

    for (int i = 0; i <= line && std::getline(stream, lineText); i++) {}

    return lineText;
}

int ScriptEditorPanel::GetLineCount() const {
    if (m_currentTab < 0) return 0;

    const auto& tab = m_tabs[m_currentTab];
    return static_cast<int>(std::count(tab.content.begin(), tab.content.end(), '\n')) + 1;
}

void ScriptEditorPanel::MarkModified() {
    if (m_currentTab < 0) return;
    m_tabs[m_currentTab].modified = true;
}

std::string ScriptEditorPanel::GetWordAtCursor() const {
    if (m_currentTab < 0) return "";

    const auto& tab = m_tabs[m_currentTab];
    std::string line = GetLineText(tab.cursorLine);

    if (line.empty() || tab.cursorColumn >= static_cast<int>(line.length())) {
        return "";
    }

    // Find word boundaries
    int start = tab.cursorColumn;
    int end = tab.cursorColumn;

    while (start > 0 && (std::isalnum(line[start - 1]) || line[start - 1] == '_')) {
        start--;
    }
    while (end < static_cast<int>(line.length()) && (std::isalnum(line[end]) || line[end] == '_')) {
        end++;
    }

    return line.substr(start, end - start);
}

void ScriptEditorPanel::EnsureCursorVisible() {
    // Would scroll to make cursor visible
}

void ScriptEditorPanel::Cut() {
    Copy();
    DeleteSelection();
}

void ScriptEditorPanel::Copy() {
    // Would copy selection to clipboard
}

void ScriptEditorPanel::Paste() {
    // Would paste from clipboard
}

void ScriptEditorPanel::SelectAll() {
    if (m_currentTab < 0) return;

    auto& tab = m_tabs[m_currentTab];
    m_hasSelection = true;
    m_selectionStartLine = 0;
    m_selectionStartCol = 0;

    // Find end of content
    int lastLine = GetLineCount() - 1;
    std::string lastLineText = GetLineText(lastLine);

    m_selectionEndLine = lastLine;
    m_selectionEndCol = static_cast<int>(lastLineText.length());
}

void ScriptEditorPanel::DeleteSelection() {
    // Would delete selected text
}

} // namespace Vehement
