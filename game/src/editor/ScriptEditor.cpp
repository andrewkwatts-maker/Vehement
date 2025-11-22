#include "ScriptEditor.hpp"
#include "Editor.hpp"
#include <imgui.h>
#include <fstream>
#include <sstream>

namespace Vehement {

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
    // Simple text editor
    // TODO: Implement syntax highlighting
    static char buffer[65536];
    if (m_scriptContent.size() < sizeof(buffer)) {
        strcpy(buffer, m_scriptContent.c_str());
    }

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
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

    // TODO: Actually run the Python script
    // For now, just simulate
    m_output.push_back("Script execution complete");

    // Add some dummy variables
    m_watchVariables.clear();
    m_watchVariables.push_back({"ctx", "PCGContext", "<context>"});
    m_watchVariables.push_back({"width", "int", "64"});
    m_watchVariables.push_back({"height", "int", "64"});
    m_watchVariables.push_back({"seed", "int", "12345"});
}

} // namespace Vehement
