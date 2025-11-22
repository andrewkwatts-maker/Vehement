#include "InlinePythonEditor.hpp"
#include "ScriptEditorPanel.hpp"
#include "../Editor.hpp"

#include <imgui.h>
#include <algorithm>
#include <regex>
#include <cctype>

namespace Vehement {

InlinePythonEditor::InlinePythonEditor() {
    // Initialize default completions
    m_completions = {
        {"spawn_entity", "spawn_entity()", "Spawn a new entity", "spawn_entity(type, x, y, z)", "Entity", 200, true},
        {"despawn_entity", "despawn_entity()", "Remove entity", "despawn_entity(id)", "Entity", 200, true},
        {"get_position", "get_position()", "Get entity position", "get_position(id)", "Entity", 200, true},
        {"set_position", "set_position()", "Set entity position", "set_position(id, x, y, z)", "Entity", 200, true},
        {"damage", "damage()", "Apply damage", "damage(target, amount, source)", "Combat", 200, true},
        {"heal", "heal()", "Heal entity", "heal(target, amount)", "Combat", 200, true},
        {"get_health", "get_health()", "Get health", "get_health(id)", "Combat", 200, true},
        {"is_alive", "is_alive()", "Check if alive", "is_alive(id)", "Combat", 200, true},
        {"find_entities_in_radius", "find_entities_in_radius()", "Find nearby entities", "find_entities_in_radius(x, y, z, r)", "Query", 200, true},
        {"get_distance", "get_distance()", "Get distance", "get_distance(e1, e2)", "Query", 200, true},
        {"play_sound", "play_sound()", "Play sound", "play_sound(name, x, y, z)", "Audio", 200, true},
        {"spawn_effect", "spawn_effect()", "Spawn effect", "spawn_effect(name, x, y, z)", "Visual", 200, true},
        {"show_notification", "show_notification()", "Show notification", "show_notification(msg, duration)", "UI", 200, true},
        {"get_delta_time", "get_delta_time()", "Get frame time", "get_delta_time()", "Time", 200, true},
        {"random", "random()", "Random 0-1", "random()", "Math", 200, true},
        {"random_range", "random_range()", "Random in range", "random_range(min, max)", "Math", 200, true},
        {"log", "log()", "Log message", "log(message)", "Debug", 200, true},
    };
}

InlinePythonEditor::~InlinePythonEditor() {
    Shutdown();
}

bool InlinePythonEditor::Initialize(Editor* editor) {
    if (m_initialized) return true;
    m_editor = editor;
    m_initialized = true;
    return true;
}

void InlinePythonEditor::Shutdown() {
    m_initialized = false;
}

bool InlinePythonEditor::Render(const char* label, std::string& script, float width) {
    return RenderInputField(label, script, width, false, 0);
}

bool InlinePythonEditor::RenderMultiline(const char* label, std::string& script,
                                         float width, float height) {
    return RenderInputField(label, script, width, true, height);
}

bool InlinePythonEditor::RenderExpandable(const char* label, std::string& script, float width) {
    ImGui::PushID(label);

    bool modified = false;

    if (m_expanded) {
        // Expanded multi-line mode
        modified = RenderInputField(label, script, width, true, 150);

        // Collapse button
        if (ImGui::SmallButton("Collapse")) {
            m_expanded = false;
        }
    } else {
        // Single-line mode
        modified = RenderInputField(label, script, width, false, 0);

        // Expand button
        ImGui::SameLine();
        if (ImGui::SmallButton("+")) {
            m_expanded = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Expand to multi-line editor");
        }
    }

    ImGui::PopID();
    return modified;
}

bool InlinePythonEditor::RenderInputField(const char* label, std::string& script,
                                          float width, bool multiline, float height) {
    bool modified = false;

    ImGui::PushID(label);

    // Calculate width
    float availWidth = width > 0 ? width : ImGui::GetContentRegionAvail().x;
    float buttonWidth = 0;
    if (m_showValidateButton) buttonWidth += 60;
    if (m_showOpenInEditor) buttonWidth += 60;
    float inputWidth = availWidth - buttonWidth - 8;

    // Copy script to buffer
    strncpy(m_buffer, script.c_str(), sizeof(m_buffer) - 1);
    m_buffer[sizeof(m_buffer) - 1] = '\0';

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.12f, 1.0f));

    // Input field
    ImGui::SetNextItemWidth(inputWidth);

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackCompletion |
                                ImGuiInputTextFlags_CallbackEdit;

    auto callback = [](ImGuiInputTextCallbackData* data) -> int {
        InlinePythonEditor* editor = static_cast<InlinePythonEditor*>(data->UserData);

        if (data->EventFlag == ImGuiInputTextCallbackFlags_CallbackCompletion) {
            // Trigger auto-complete
            editor->m_showAutoComplete = true;
        } else if (data->EventFlag == ImGuiInputTextCallbackFlags_CallbackEdit) {
            // Update auto-complete on edit
            editor->UpdateAutoComplete(std::string(data->Buf), data->CursorPos);
        }

        return 0;
    };

    if (multiline) {
        if (ImGui::InputTextMultiline("##input", m_buffer, sizeof(m_buffer),
                                      ImVec2(inputWidth, height), flags, callback, this)) {
            script = m_buffer;
            modified = true;

            if (m_autoValidate) {
                m_lastValidation = Validate(script);
            }
        }
    } else {
        if (ImGui::InputTextWithHint("##input", m_placeholder.c_str(), m_buffer,
                                     sizeof(m_buffer), flags, callback, this)) {
            script = m_buffer;
            modified = true;

            if (m_autoValidate) {
                m_lastValidation = Validate(script);
            }
        }
    }

    ImGui::PopStyleColor();

    // Render auto-complete popup
    if (m_showAutoComplete && m_autoCompleteEnabled) {
        RenderAutoComplete(m_completionPrefix);
    }

    // Buttons
    ImGui::SameLine();
    RenderButtons(script);

    // Validation status
    RenderValidationStatus();

    ImGui::PopID();

    return modified;
}

void InlinePythonEditor::RenderButtons(const std::string& script) {
    ImGui::BeginGroup();

    // Validate button
    if (m_showValidateButton) {
        if (ImGui::SmallButton("Validate")) {
            m_lastValidation = Validate(script);
            if (m_onValidated) {
                m_onValidated(script);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Validate Python syntax");
        }
        ImGui::SameLine();
    }

    // Open in editor button
    if (m_showOpenInEditor) {
        if (ImGui::SmallButton("Edit")) {
            OpenInFullEditor(script);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Open in full script editor");
        }
    }

    ImGui::EndGroup();
}

void InlinePythonEditor::RenderAutoComplete(const std::string& currentWord) {
    if (m_filteredCompletions.empty()) {
        m_showAutoComplete = false;
        return;
    }

    ImVec2 pos = ImGui::GetItemRectMin();
    pos.y += ImGui::GetItemRectSize().y;

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(ImVec2(250, 150));

    if (ImGui::Begin("##InlineAutoComplete", nullptr,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_Popup)) {

        for (size_t i = 0; i < m_filteredCompletions.size() && i < 8; ++i) {
            const auto& item = m_filteredCompletions[i];

            bool isSelected = static_cast<int>(i) == m_selectedCompletion;

            // Different color for game API
            if (item.isGameAPI) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
            }

            if (ImGui::Selectable(item.displayText.c_str(), isSelected)) {
                // Insert completion
                // Would need to modify the input buffer
                m_showAutoComplete = false;
            }

            if (item.isGameAPI) {
                ImGui::PopStyleColor();
            }

            // Tooltip with signature
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", item.signature.c_str());
                if (!item.description.empty()) {
                    ImGui::TextDisabled("%s", item.description.c_str());
                }
                ImGui::EndTooltip();
            }
        }

        // Handle keyboard navigation
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            m_selectedCompletion = std::min(m_selectedCompletion + 1,
                                           static_cast<int>(m_filteredCompletions.size()) - 1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            m_selectedCompletion = std::max(m_selectedCompletion - 1, 0);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_showAutoComplete = false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Tab)) {
            // Accept selection
            m_showAutoComplete = false;
        }
    }
    ImGui::End();
}

void InlinePythonEditor::RenderValidationStatus() {
    if (!m_lastValidation.valid) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::Text("Error: %s", m_lastValidation.errorMessage.c_str());
        ImGui::PopStyleColor();
    }
}

InlineValidationResult InlinePythonEditor::Validate(const std::string& script) {
    InlineValidationResult result;
    result.valid = true;

    if (script.empty()) {
        return result;
    }

    // Basic syntax validation
    int parenCount = 0, bracketCount = 0, braceCount = 0;
    bool inString = false;
    char stringChar = 0;

    for (size_t i = 0; i < script.length(); ++i) {
        char c = script[i];

        if (!inString) {
            if (c == '"' || c == '\'') {
                inString = true;
                stringChar = c;
            } else if (c == '(') parenCount++;
            else if (c == ')') {
                parenCount--;
                if (parenCount < 0) {
                    result.valid = false;
                    result.errorMessage = "Unmatched closing parenthesis";
                    result.errorColumn = static_cast<int>(i);
                    return result;
                }
            }
            else if (c == '[') bracketCount++;
            else if (c == ']') {
                bracketCount--;
                if (bracketCount < 0) {
                    result.valid = false;
                    result.errorMessage = "Unmatched closing bracket";
                    result.errorColumn = static_cast<int>(i);
                    return result;
                }
            }
            else if (c == '{') braceCount++;
            else if (c == '}') {
                braceCount--;
                if (braceCount < 0) {
                    result.valid = false;
                    result.errorMessage = "Unmatched closing brace";
                    result.errorColumn = static_cast<int>(i);
                    return result;
                }
            }
        } else {
            if (c == stringChar && (i == 0 || script[i-1] != '\\')) {
                inString = false;
            }
        }
    }

    if (inString) {
        result.valid = false;
        result.errorMessage = "Unterminated string";
        return result;
    }

    if (parenCount != 0) {
        result.valid = false;
        result.errorMessage = "Unmatched parentheses";
        return result;
    }

    if (bracketCount != 0) {
        result.valid = false;
        result.errorMessage = "Unmatched brackets";
        return result;
    }

    if (braceCount != 0) {
        result.valid = false;
        result.errorMessage = "Unmatched braces";
        return result;
    }

    // Check for common Python errors
    if (script.find("def ") != std::string::npos && script.find(":") == std::string::npos) {
        result.valid = false;
        result.errorMessage = "Function definition missing colon";
        return result;
    }

    return result;
}

void InlinePythonEditor::RegisterCompletions(const std::vector<CompletionItem>& items) {
    m_customCompletions.insert(m_customCompletions.end(), items.begin(), items.end());
}

void InlinePythonEditor::OpenInFullEditor(const std::string& script) {
    if (m_onOpenInEditor) {
        m_onOpenInEditor(script);
    }

    // Would open script editor panel with this content
    if (m_editor) {
        // m_editor->GetScriptEditor()->NewFile();
        // m_editor->GetScriptEditor()->SetContent(script);
    }
}

void InlinePythonEditor::UpdateAutoComplete(const std::string& text, int cursorPos) {
    std::string word = GetWordAtPosition(text, cursorPos);

    if (word.empty() || word.length() < 2) {
        m_showAutoComplete = false;
        return;
    }

    m_completionPrefix = word;
    m_filteredCompletions = GetCompletions(word);
    m_selectedCompletion = 0;
    m_showAutoComplete = !m_filteredCompletions.empty();
}

std::vector<CompletionItem> InlinePythonEditor::GetCompletions(const std::string& prefix) {
    std::vector<CompletionItem> results;

    std::string lowerPrefix = prefix;
    std::transform(lowerPrefix.begin(), lowerPrefix.end(), lowerPrefix.begin(), ::tolower);

    auto searchSource = [&](const std::vector<CompletionItem>& source) {
        for (const auto& item : source) {
            std::string lowerText = item.text;
            std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);

            if (lowerText.find(lowerPrefix) == 0) {
                results.push_back(item);
            }
        }
    };

    searchSource(m_completions);
    searchSource(m_customCompletions);

    // Sort by priority
    std::sort(results.begin(), results.end(), [](const CompletionItem& a, const CompletionItem& b) {
        return a.priority > b.priority;
    });

    return results;
}

std::string InlinePythonEditor::GetWordAtPosition(const std::string& text, int position) {
    if (position <= 0 || position > static_cast<int>(text.length())) {
        return "";
    }

    int start = position - 1;
    while (start >= 0 && (std::isalnum(text[start]) || text[start] == '_')) {
        start--;
    }
    start++;

    int end = position;
    while (end < static_cast<int>(text.length()) && (std::isalnum(text[end]) || text[end] == '_')) {
        end++;
    }

    if (start < end) {
        return text.substr(start, end - start);
    }

    return "";
}

} // namespace Vehement
