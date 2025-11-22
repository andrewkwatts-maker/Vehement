#include "ScriptEditor.hpp"
#include "Editor.hpp"
#include "../../engine/scripting/PythonEngine.hpp"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <unordered_set>

namespace Vehement {

// Python keywords for syntax highlighting
static const std::unordered_set<std::string> s_pythonKeywords = {
    "False", "None", "True", "and", "as", "assert", "async", "await",
    "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
    "while", "with", "yield"
};

// Python built-in functions
static const std::unordered_set<std::string> s_pythonBuiltins = {
    "abs", "all", "any", "bin", "bool", "bytes", "callable", "chr",
    "dict", "dir", "divmod", "enumerate", "eval", "exec", "filter",
    "float", "format", "getattr", "globals", "hasattr", "hash", "hex",
    "id", "input", "int", "isinstance", "iter", "len", "list", "locals",
    "map", "max", "min", "next", "object", "open", "ord", "pow", "print",
    "range", "repr", "reversed", "round", "set", "setattr", "slice",
    "sorted", "str", "sum", "super", "tuple", "type", "vars", "zip"
};

ScriptEditor::ScriptEditor(Editor* editor) : m_editor(editor) {}
ScriptEditor::~ScriptEditor() = default;

void ScriptEditor::Render() {
    if (!ImGui::Begin("Script Editor")) {
        ImGui::End();
        return;
    }

    RenderToolbar();
    ImGui::Separator();

    // Main content area
    float contentHeight = ImGui::GetContentRegionAvail().y;

    // Editor takes 70% of height
    ImGui::BeginChild("EditorArea", ImVec2(0, contentHeight * 0.7f), true);
    RenderEditor();
    ImGui::EndChild();

    // Output/variables take 30%
    if (ImGui::BeginTabBar("OutputTabs")) {
        if (ImGui::BeginTabItem("Output")) {
            RenderOutput();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Variables")) {
            RenderVariables();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void ScriptEditor::RenderToolbar() {
    if (ImGui::Button("Open")) {
        ImGui::OpenPopup("OpenScriptPopup");
    }
    ImGui::SameLine();
    if (ImGui::Button("Save") && !m_currentFile.empty()) {
        SaveScript();
    }
    ImGui::SameLine();
    if (ImGui::Button("Run")) {
        RunScript();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Output")) {
        m_output.clear();
        m_errors.clear();
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 300);
    if (!m_currentFile.empty()) {
        ImGui::Text("%s%s", m_currentFile.c_str(), m_modified ? "*" : "");
    } else {
        ImGui::TextDisabled("No file open");
    }

    // Open file popup
    if (ImGui::BeginPopup("OpenScriptPopup")) {
        static char pathBuffer[512] = "";
        ImGui::InputText("Path", pathBuffer, sizeof(pathBuffer));
        if (ImGui::Button("Open")) {
            OpenScript(pathBuffer);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ScriptEditor::RenderEditor() {
    // Text editor with syntax highlighting
    // We render line by line to apply different colors
    static char buffer[65536];
    if (m_scriptContent.size() < sizeof(buffer)) {
        strcpy(buffer, m_scriptContent.c_str());
    }

    // Syntax highlighting colors
    const ImVec4 colorDefault(0.9f, 0.9f, 0.9f, 1.0f);
    const ImVec4 colorKeyword(0.8f, 0.4f, 0.8f, 1.0f);      // Purple for keywords
    const ImVec4 colorBuiltin(0.4f, 0.7f, 0.9f, 1.0f);      // Blue for builtins
    const ImVec4 colorString(0.6f, 0.9f, 0.6f, 1.0f);       // Green for strings
    const ImVec4 colorComment(0.5f, 0.5f, 0.5f, 1.0f);      // Gray for comments
    const ImVec4 colorNumber(0.9f, 0.7f, 0.4f, 1.0f);       // Orange for numbers
    const ImVec4 colorFunction(0.9f, 0.9f, 0.4f, 1.0f);     // Yellow for function names

    // Show line numbers on the left
    ImGui::BeginChild("LineNumbers", ImVec2(40, -1), false);
    std::istringstream stream(m_scriptContent);
    std::string line;
    int lineNum = 1;
    while (std::getline(stream, line)) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%3d", lineNum++);
    }
    if (m_scriptContent.empty() || m_scriptContent.back() == '\n') {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%3d", lineNum);
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Render highlighted code in a read-only display, with editable input below
    ImGui::BeginChild("CodeHighlight", ImVec2(-1, -30), true);

    stream.clear();
    stream.str(m_scriptContent);
    while (std::getline(stream, line)) {
        size_t i = 0;
        while (i < line.size()) {
            // Check for comments
            if (line[i] == '#') {
                ImGui::TextColored(colorComment, "%s", line.substr(i).c_str());
                break;
            }
            // Check for strings
            else if (line[i] == '"' || line[i] == '\'') {
                char quote = line[i];
                size_t end = line.find(quote, i + 1);
                if (end == std::string::npos) end = line.size();
                else end++;
                ImGui::SameLine(0, 0);
                ImGui::TextColored(colorString, "%s", line.substr(i, end - i).c_str());
                i = end;
            }
            // Check for numbers
            else if (std::isdigit(line[i]) || (line[i] == '.' && i + 1 < line.size() && std::isdigit(line[i + 1]))) {
                size_t end = i;
                while (end < line.size() && (std::isdigit(line[end]) || line[end] == '.' || line[end] == 'e' || line[end] == 'E')) end++;
                ImGui::SameLine(0, 0);
                ImGui::TextColored(colorNumber, "%s", line.substr(i, end - i).c_str());
                i = end;
            }
            // Check for identifiers/keywords
            else if (std::isalpha(line[i]) || line[i] == '_') {
                size_t end = i;
                while (end < line.size() && (std::isalnum(line[end]) || line[end] == '_')) end++;
                std::string word = line.substr(i, end - i);
                ImGui::SameLine(0, 0);
                if (s_pythonKeywords.count(word)) {
                    ImGui::TextColored(colorKeyword, "%s", word.c_str());
                } else if (s_pythonBuiltins.count(word)) {
                    ImGui::TextColored(colorBuiltin, "%s", word.c_str());
                } else if (end < line.size() && line[end] == '(') {
                    ImGui::TextColored(colorFunction, "%s", word.c_str());
                } else {
                    ImGui::TextColored(colorDefault, "%s", word.c_str());
                }
                i = end;
            }
            // Other characters
            else {
                ImGui::SameLine(0, 0);
                char ch[2] = {line[i], '\0'};
                ImGui::TextColored(colorDefault, "%s", ch);
                i++;
            }
        }
        ImGui::NewLine();
    }
    ImGui::EndChild();

    // Editable input at bottom
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextMultiline("##code", buffer, sizeof(buffer),
        ImVec2(-1, -1), flags)) {
        m_scriptContent = buffer;
        m_modified = true;
    }
}

void ScriptEditor::RenderOutput() {
    ImGui::BeginChild("OutputScroll", ImVec2(0, 0), false);

    // Show errors first
    for (const auto& error : m_errors) {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "[ERROR] %s", error.c_str());
    }

    // Then output
    for (const auto& line : m_output) {
        ImGui::Text("%s", line.c_str());
    }

    // Auto-scroll
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

void ScriptEditor::RenderVariables() {
    if (m_watchVariables.empty()) {
        ImGui::TextDisabled("No variables to watch");
        ImGui::TextDisabled("Run a script to see variables");
        return;
    }

    if (ImGui::BeginTable("Variables", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        for (const auto& var : m_watchVariables) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", var.name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", var.type.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", var.value.c_str());
        }
        ImGui::EndTable();
    }
}

void ScriptEditor::OpenScript(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        m_errors.push_back("Failed to open file: " + path);
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    m_scriptContent = buffer.str();
    m_currentFile = path;
    m_modified = false;

    m_output.push_back("Opened: " + path);
}

void ScriptEditor::SaveScript() {
    if (m_currentFile.empty()) return;

    std::ofstream file(m_currentFile);
    if (!file.is_open()) {
        m_errors.push_back("Failed to save file: " + m_currentFile);
        return;
    }

    file << m_scriptContent;
    m_modified = false;
    m_output.push_back("Saved: " + m_currentFile);
}

void ScriptEditor::RunScript() {
    m_output.push_back("Running script...");
    m_errors.clear();

    // Execute the Python script using the Python engine
    auto& pythonEngine = Nova::Scripting::PythonEngine::Instance();

    if (!pythonEngine.IsInitialized()) {
        // Try to initialize with default config
        Nova::Scripting::PythonEngineConfig config;
        config.scriptPaths = {"scripts/", "game/scripts/"};
        config.enableHotReload = true;
        config.verboseErrors = true;

        if (!pythonEngine.Initialize(config)) {
            m_errors.push_back("Failed to initialize Python engine: " + pythonEngine.GetLastError());
            return;
        }
        m_output.push_back("Python engine initialized");
    }

    // Execute the script content
    Nova::Scripting::ScriptResult result;
    if (!m_currentFile.empty()) {
        // Execute from file for better error reporting
        result = pythonEngine.ExecuteFile(m_currentFile);
    } else {
        // Execute from string
        result = pythonEngine.ExecuteString(m_scriptContent, "editor_script");
    }

    if (result.success) {
        m_output.push_back("Script execution complete");

        // Get return value if any
        if (auto strVal = result.GetValue<std::string>()) {
            m_output.push_back("Return: " + *strVal);
        } else if (auto intVal = result.GetValue<int>()) {
            m_output.push_back("Return: " + std::to_string(*intVal));
        } else if (auto floatVal = result.GetValue<float>()) {
            m_output.push_back("Return: " + std::to_string(*floatVal));
        }
    } else {
        m_errors.push_back(result.errorMessage);
    }

    // Update watch variables from Python context
    m_watchVariables.clear();

    // Try to extract common PCG variables if they exist
    if (auto widthVal = pythonEngine.GetGlobal<int>("__main__", "width")) {
        m_watchVariables.push_back({"width", "int", std::to_string(*widthVal)});
    }
    if (auto heightVal = pythonEngine.GetGlobal<int>("__main__", "height")) {
        m_watchVariables.push_back({"height", "int", std::to_string(*heightVal)});
    }
    if (auto seedVal = pythonEngine.GetGlobal<int>("__main__", "seed")) {
        m_watchVariables.push_back({"seed", "int", std::to_string(*seedVal)});
    }

    // Add script context info
    if (auto* context = pythonEngine.GetContext()) {
        m_watchVariables.push_back({"ctx", "PCGContext", "<active>"});
    }

    // If no variables found, show defaults
    if (m_watchVariables.empty()) {
        m_watchVariables.push_back({"(no exposed variables)", "", ""});
    }

    // Report metrics
    const auto& metrics = pythonEngine.GetMetrics();
    m_output.push_back("Execution time: " + std::to_string(metrics.avgExecutionTimeMs) + " ms");
}

} // namespace Vehement
