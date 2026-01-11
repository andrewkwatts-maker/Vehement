#include "JSONEditor.hpp"
#include "ModernUI.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>

JSONEditor::JSONEditor() {
    // Allocate text buffer
    m_textBuffer = new char[BUFFER_SIZE];
    memset(m_textBuffer, 0, BUFFER_SIZE);
}

JSONEditor::~JSONEditor() {
    delete[] m_textBuffer;
}

void JSONEditor::Open(const std::string& assetPath) {
    m_assetPath = assetPath;

    // Extract filename
    std::filesystem::path path(assetPath);
    m_fileName = path.filename().string();

    LoadJSON();
}

void JSONEditor::Render(bool* isOpen) {
    if (!isOpen || !*isOpen) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    std::string windowTitle = "JSON Editor - " + m_fileName;
    if (m_isDirty) {
        windowTitle += "*";
    }

    if (ImGui::Begin(windowTitle.c_str(), isOpen, ImGuiWindowFlags_MenuBar)) {
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save", "Ctrl+S", false, m_isDirty)) {
                    Save();
                }
                if (ImGui::MenuItem("Validate")) {
                    ValidateJSON();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Close")) {
                    *isOpen = false;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Format/Pretty Print")) {
                    FormatJSON();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, false)) {
                    // TODO: Undo functionality
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false)) {
                    // TODO: Redo functionality
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Text Editor", nullptr, m_viewMode == ViewMode::Text)) {
                    m_viewMode = ViewMode::Text;
                }
                if (ImGui::MenuItem("Tree View", nullptr, m_viewMode == ViewMode::Tree)) {
                    m_viewMode = ViewMode::Tree;
                    ParseJSONToTree();
                }
                ImGui::Separator();
                ImGui::MenuItem("Show Line Numbers", nullptr, &m_showLineNumbers);
                ImGui::MenuItem("Auto Validate", nullptr, &m_autoValidate);
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        if (!m_isLoaded) {
            ImVec2 windowSize = ImGui::GetContentRegionAvail();
            ImGui::SetCursorPos(ImVec2(
                windowSize.x * 0.5f - 50,
                windowSize.y * 0.5f - 10
            ));
            ImGui::TextDisabled("No file loaded");
        } else {
            // Toolbar
            RenderToolbar();

            ImGui::Spacing();

            // Validation status
            if (!m_isValid) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::Text("Error: %s", m_errorMessage.c_str());
                if (m_errorLine >= 0) {
                    ImGui::SameLine();
                    ImGui::Text("(Line %d)", m_errorLine);
                }
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
                ImGui::Text("Valid JSON");
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ModernUI::GradientSeparator();
            ImGui::Spacing();

            // Content area
            if (m_viewMode == ViewMode::Text) {
                RenderTextEditor();
            } else {
                RenderTreeView();
            }
        }
    }
    ImGui::End();
}

void JSONEditor::RenderToolbar() {
    if (ModernUI::GlowButton("Save", ImVec2(80, 0))) {
        Save();
    }

    ImGui::SameLine();
    if (ModernUI::GlowButton("Validate", ImVec2(80, 0))) {
        ValidateJSON();
    }

    ImGui::SameLine();
    if (ModernUI::GlowButton("Format", ImVec2(80, 0))) {
        FormatJSON();
    }

    ImGui::SameLine();
    if (ModernUI::GlowButton(m_viewMode == ViewMode::Text ? "Tree View" : "Text View", ImVec2(100, 0))) {
        m_viewMode = (m_viewMode == ViewMode::Text) ? ViewMode::Tree : ViewMode::Text;
        if (m_viewMode == ViewMode::Tree) {
            ParseJSONToTree();
        }
    }
}

void JSONEditor::RenderTextEditor() {
    ImVec2 editorSize = ImGui::GetContentRegionAvail();

    // Text editor with basic features
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;

    if (ImGui::InputTextMultiline(
        "##JSONEditor",
        m_textBuffer,
        BUFFER_SIZE,
        editorSize,
        flags))
    {
        // Check if content changed
        std::string newContent(m_textBuffer);
        if (newContent != m_content) {
            m_content = newContent;
            m_isDirty = true;

            if (m_autoValidate) {
                ValidateJSON();
            }
        }
    }
}

void JSONEditor::RenderTreeView() {
    if (ImGui::BeginChild("TreeView", ImVec2(0, 0), true)) {
        ImGui::TextDisabled("Tree view not fully implemented");
        ImGui::Text("Root Object");

        // TODO: Implement full tree rendering
        // This would recursively render the JSON structure as a tree

        if (ImGui::TreeNode("Example Object")) {
            ImGui::Text("key1: \"value1\"");
            ImGui::Text("key2: 123");

            if (ImGui::TreeNode("nested")) {
                ImGui::Text("nestedKey: true");
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
    }
    ImGui::EndChild();
}

void JSONEditor::LoadJSON() {
    spdlog::info("JSONEditor: Loading JSON file '{}'", m_assetPath);

    try {
        std::filesystem::path path(m_assetPath);

        if (!std::filesystem::exists(path)) {
            spdlog::error("JSONEditor: File does not exist: '{}'", m_assetPath);
            return;
        }

        // Read file content
        std::ifstream file(m_assetPath);
        if (!file.is_open()) {
            spdlog::error("JSONEditor: Failed to open file: '{}'", m_assetPath);
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        m_content = buffer.str();
        m_originalContent = m_content;

        // Copy to text buffer
        size_t copySize = std::min(m_content.size(), BUFFER_SIZE - 1);
        memcpy(m_textBuffer, m_content.c_str(), copySize);
        m_textBuffer[copySize] = '\0';

        m_isLoaded = true;
        m_isDirty = false;

        // Validate on load
        ValidateJSON();

        spdlog::info("JSONEditor: File loaded successfully ({} bytes)", m_content.size());

    } catch (const std::exception& e) {
        spdlog::error("JSONEditor: Failed to load file: {}", e.what());
        m_isLoaded = false;
    }
}

bool JSONEditor::ValidateJSON() {
    // Basic JSON validation (simplified)
    // In a real implementation, you would use a proper JSON parser like nlohmann/json

    m_isValid = true;
    m_errorMessage.clear();
    m_errorLine = -1;

    if (m_content.empty()) {
        m_isValid = false;
        m_errorMessage = "Empty file";
        return false;
    }

    // Simple bracket/brace matching
    int braceCount = 0;
    int bracketCount = 0;
    bool inString = false;
    bool escaped = false;

    for (size_t i = 0; i < m_content.length(); ++i) {
        char c = m_content[i];

        if (escaped) {
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if (c == '"' && !escaped) {
            inString = !inString;
        }

        if (!inString) {
            if (c == '{') braceCount++;
            if (c == '}') braceCount--;
            if (c == '[') bracketCount++;
            if (c == ']') bracketCount--;

            if (braceCount < 0) {
                m_isValid = false;
                m_errorMessage = "Unmatched closing brace '}'";
                return false;
            }

            if (bracketCount < 0) {
                m_isValid = false;
                m_errorMessage = "Unmatched closing bracket ']'";
                return false;
            }
        }
    }

    if (braceCount != 0) {
        m_isValid = false;
        m_errorMessage = "Unmatched braces";
        return false;
    }

    if (bracketCount != 0) {
        m_isValid = false;
        m_errorMessage = "Unmatched brackets";
        return false;
    }

    if (inString) {
        m_isValid = false;
        m_errorMessage = "Unterminated string";
        return false;
    }

    spdlog::info("JSONEditor: JSON validation passed");
    return true;
}

void JSONEditor::FormatJSON() {
    // Basic JSON formatting (simplified)
    // In a real implementation, you would use a proper JSON parser

    spdlog::info("JSONEditor: Formatting JSON");

    std::string formatted;
    formatted.reserve(m_content.size() * 2);

    int indentLevel = 0;
    bool inString = false;
    bool escaped = false;

    for (size_t i = 0; i < m_content.length(); ++i) {
        char c = m_content[i];

        if (escaped) {
            formatted += c;
            escaped = false;
            continue;
        }

        if (c == '\\') {
            formatted += c;
            escaped = true;
            continue;
        }

        if (c == '"') {
            inString = !inString;
            formatted += c;
            continue;
        }

        if (inString) {
            formatted += c;
            continue;
        }

        // Skip whitespace outside strings
        if (std::isspace(c)) {
            continue;
        }

        switch (c) {
            case '{':
            case '[':
                formatted += c;
                formatted += '\n';
                indentLevel++;
                formatted += std::string(indentLevel * 2, ' ');
                break;

            case '}':
            case ']':
                formatted += '\n';
                indentLevel--;
                formatted += std::string(indentLevel * 2, ' ');
                formatted += c;
                break;

            case ',':
                formatted += c;
                formatted += '\n';
                formatted += std::string(indentLevel * 2, ' ');
                break;

            case ':':
                formatted += c;
                formatted += ' ';
                break;

            default:
                formatted += c;
                break;
        }
    }

    m_content = formatted;

    // Update text buffer
    size_t copySize = std::min(m_content.size(), BUFFER_SIZE - 1);
    memcpy(m_textBuffer, m_content.c_str(), copySize);
    m_textBuffer[copySize] = '\0';

    m_isDirty = true;

    spdlog::info("JSONEditor: JSON formatted");
}

void JSONEditor::ParseJSONToTree() {
    // TODO: Implement JSON parsing to tree structure
    // This would parse the JSON and build the JSONNode tree for tree view

    spdlog::debug("JSONEditor: Parsing JSON to tree (not fully implemented)");

    m_rootNode.key = "root";
    m_rootNode.type = "object";
    m_rootNode.expanded = true;
}

void JSONEditor::Save() {
    if (!m_isDirty) {
        spdlog::info("JSONEditor: No changes to save");
        return;
    }

    // Validate before saving
    if (!ValidateJSON()) {
        spdlog::warn("JSONEditor: Cannot save invalid JSON");
        // In a real implementation, show a dialog asking if user wants to save anyway
        return;
    }

    spdlog::info("JSONEditor: Saving JSON to '{}'", m_assetPath);

    try {
        std::ofstream file(m_assetPath);
        if (!file.is_open()) {
            spdlog::error("JSONEditor: Failed to open file for writing: '{}'", m_assetPath);
            return;
        }

        file << m_content;
        file.close();

        m_originalContent = m_content;
        m_isDirty = false;

        spdlog::info("JSONEditor: File saved successfully");

    } catch (const std::exception& e) {
        spdlog::error("JSONEditor: Failed to save file: {}", e.what());
    }
}

std::string JSONEditor::GetEditorName() const {
    return "JSON Editor - " + m_fileName;
}

bool JSONEditor::IsDirty() const {
    return m_isDirty;
}

std::string JSONEditor::GetAssetPath() const {
    return m_assetPath;
}
