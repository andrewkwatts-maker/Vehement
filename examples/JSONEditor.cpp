#include "JSONEditor.hpp"
#include "ModernUI.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>
#include <functional>

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
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !m_undoStack.empty())) {
                    Undo();
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !m_redoStack.empty())) {
                    Redo();
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
            // Push current state to undo stack before modifying
            PushUndoState();
            m_content = newContent;
            m_isDirty = true;
            // Clear redo stack when new changes are made
            m_redoStack.clear();

            if (m_autoValidate) {
                ValidateJSON();
            }
        }
    }
}

void JSONEditor::RenderJSONNode(JSONNode& node, int depth) {
    ImGui::PushID(&node);

    // Color coding for different types
    ImVec4 typeColor;
    if (node.type == "object") {
        typeColor = ImVec4(0.6f, 0.8f, 1.0f, 1.0f); // Light blue
    } else if (node.type == "array") {
        typeColor = ImVec4(0.8f, 0.6f, 1.0f, 1.0f); // Light purple
    } else if (node.type == "string") {
        typeColor = ImVec4(0.6f, 1.0f, 0.6f, 1.0f); // Light green
    } else if (node.type == "number") {
        typeColor = ImVec4(1.0f, 0.8f, 0.4f, 1.0f); // Orange
    } else if (node.type == "boolean") {
        typeColor = ImVec4(1.0f, 0.6f, 0.6f, 1.0f); // Light red
    } else {
        typeColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray for null
    }

    bool hasChildren = !node.children.empty();

    if (hasChildren) {
        // Render as tree node for objects and arrays
        std::string label = node.key.empty() ? node.type : node.key;
        if (node.type == "object") {
            label += " {...}";
        } else if (node.type == "array") {
            label += " [" + std::to_string(node.children.size()) + "]";
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (node.expanded) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, typeColor);
        bool isOpen = ImGui::TreeNodeEx(label.c_str(), flags);
        ImGui::PopStyleColor();

        node.expanded = isOpen;

        if (isOpen) {
            for (auto& child : node.children) {
                RenderJSONNode(child, depth + 1);
            }
            ImGui::TreePop();
        }
    } else {
        // Render as leaf node for primitives
        ImGui::Indent();
        ImGui::PushStyleColor(ImGuiCol_Text, typeColor);

        std::string displayText;
        if (!node.key.empty()) {
            displayText = node.key + ": ";
        }

        if (node.type == "string") {
            displayText += "\"" + node.value + "\"";
        } else {
            displayText += node.value;
        }

        ImGui::Text("%s", displayText.c_str());
        ImGui::PopStyleColor();
        ImGui::Unindent();
    }

    ImGui::PopID();
}

void JSONEditor::RenderTreeView() {
    if (ImGui::BeginChild("TreeView", ImVec2(0, 0), true)) {
        if (m_rootNode.children.empty() && m_rootNode.value.empty()) {
            ImGui::TextDisabled("No JSON structure parsed. Click 'Validate' or switch views to parse.");
        } else {
            RenderJSONNode(m_rootNode, 0);
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
    spdlog::debug("JSONEditor: Parsing JSON to tree");

    // Clear existing tree
    m_rootNode = JSONNode();
    m_rootNode.key = "root";
    m_rootNode.expanded = true;

    if (m_content.empty()) {
        return;
    }

    // Simple recursive descent JSON parser
    size_t pos = 0;
    const std::string& content = m_content;

    // Helper lambdas for parsing
    std::function<void()> skipWhitespace = [&]() {
        while (pos < content.size() && std::isspace(content[pos])) {
            ++pos;
        }
    };

    std::function<char()> peek = [&]() -> char {
        return pos < content.size() ? content[pos] : '\0';
    };

    std::function<char()> consume = [&]() -> char {
        return pos < content.size() ? content[pos++] : '\0';
    };

    std::function<std::string()> parseString = [&]() -> std::string {
        std::string result;
        if (consume() != '"') return result; // Consume opening quote

        while (pos < content.size()) {
            char c = consume();
            if (c == '"') break;
            if (c == '\\' && pos < content.size()) {
                char escaped = consume();
                switch (escaped) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    default: result += escaped; break;
                }
            } else {
                result += c;
            }
        }
        return result;
    };

    std::function<std::string()> parseNumber = [&]() -> std::string {
        std::string result;
        while (pos < content.size()) {
            char c = peek();
            if (std::isdigit(c) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E') {
                result += consume();
            } else {
                break;
            }
        }
        return result;
    };

    std::function<JSONNode(const std::string&)> parseValue = [&](const std::string& key) -> JSONNode {
        JSONNode node;
        node.key = key;
        node.expanded = true;

        skipWhitespace();
        char c = peek();

        if (c == '{') {
            // Object
            node.type = "object";
            consume(); // Consume '{'
            skipWhitespace();

            while (peek() != '}' && pos < content.size()) {
                skipWhitespace();
                if (peek() == '"') {
                    std::string childKey = parseString();
                    skipWhitespace();
                    if (peek() == ':') consume(); // Consume ':'
                    skipWhitespace();
                    node.children.push_back(parseValue(childKey));
                    skipWhitespace();
                    if (peek() == ',') consume(); // Consume ','
                } else {
                    break;
                }
            }
            if (peek() == '}') consume(); // Consume '}'
        }
        else if (c == '[') {
            // Array
            node.type = "array";
            consume(); // Consume '['
            skipWhitespace();

            int index = 0;
            while (peek() != ']' && pos < content.size()) {
                skipWhitespace();
                node.children.push_back(parseValue("[" + std::to_string(index++) + "]"));
                skipWhitespace();
                if (peek() == ',') consume(); // Consume ','
            }
            if (peek() == ']') consume(); // Consume ']'
        }
        else if (c == '"') {
            // String
            node.type = "string";
            node.value = parseString();
        }
        else if (c == 't' || c == 'f') {
            // Boolean
            node.type = "boolean";
            if (content.substr(pos, 4) == "true") {
                node.value = "true";
                pos += 4;
            } else if (content.substr(pos, 5) == "false") {
                node.value = "false";
                pos += 5;
            }
        }
        else if (c == 'n') {
            // Null
            node.type = "null";
            node.value = "null";
            if (content.substr(pos, 4) == "null") {
                pos += 4;
            }
        }
        else if (std::isdigit(c) || c == '-') {
            // Number
            node.type = "number";
            node.value = parseNumber();
        }

        return node;
    };

    try {
        skipWhitespace();
        m_rootNode = parseValue("");
        m_rootNode.key = "root";
        m_rootNode.expanded = true;
        spdlog::debug("JSONEditor: JSON tree parsed successfully");
    } catch (const std::exception& e) {
        spdlog::warn("JSONEditor: Failed to parse JSON to tree: {}", e.what());
    }
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

void JSONEditor::PushUndoState() {
    // Add current content to undo stack
    m_undoStack.push_back(m_content);

    // Limit undo stack size
    if (m_undoStack.size() > MAX_UNDO_LEVELS) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

void JSONEditor::Undo() {
    if (m_undoStack.empty()) {
        return;
    }

    spdlog::debug("JSONEditor: Undo");

    // Push current state to redo stack
    m_redoStack.push_back(m_content);

    // Restore previous state
    m_content = m_undoStack.back();
    m_undoStack.pop_back();

    // Update text buffer
    size_t copySize = std::min(m_content.size(), BUFFER_SIZE - 1);
    memcpy(m_textBuffer, m_content.c_str(), copySize);
    m_textBuffer[copySize] = '\0';

    m_isDirty = (m_content != m_originalContent);

    if (m_autoValidate) {
        ValidateJSON();
    }
}

void JSONEditor::Redo() {
    if (m_redoStack.empty()) {
        return;
    }

    spdlog::debug("JSONEditor: Redo");

    // Push current state to undo stack
    m_undoStack.push_back(m_content);

    // Restore next state
    m_content = m_redoStack.back();
    m_redoStack.pop_back();

    // Update text buffer
    size_t copySize = std::min(m_content.size(), BUFFER_SIZE - 1);
    memcpy(m_textBuffer, m_content.c_str(), copySize);
    m_textBuffer[copySize] = '\0';

    m_isDirty = (m_content != m_originalContent);

    if (m_autoValidate) {
        ValidateJSON();
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
