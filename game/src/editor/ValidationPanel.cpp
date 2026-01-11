#include "ValidationPanel.hpp"
#include "Editor.hpp"
#include "../config/ConfigRegistry.hpp"
#include <imgui.h>
#include <algorithm>

namespace Vehement {

ValidationPanel::ValidationPanel(Editor* editor)
    : m_editor(editor) {
}

ValidationPanel::~ValidationPanel() = default;

void ValidationPanel::Render() {
    if (!ImGui::Begin("Validation")) {
        ImGui::End();
        return;
    }

    RenderToolbar();
    ImGui::Separator();

    // Split view: message list on top, details on bottom
    ImGui::BeginChild("MessageList", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.7f), true);
    RenderMessageList();
    ImGui::EndChild();

    ImGui::BeginChild("MessageDetails", ImVec2(0, 0), true);
    RenderMessageDetails();
    ImGui::EndChild();

    ImGui::End();
}

void ValidationPanel::RenderToolbar() {
    if (ImGui::Button("Validate All")) {
        ValidateAllConfigs();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        ClearValidation();
    }
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Filter toggles
    ImVec4 errorColor(1.0f, 0.3f, 0.3f, 1.0f);
    ImVec4 warningColor(1.0f, 0.8f, 0.2f, 1.0f);
    ImVec4 infoColor(0.3f, 0.8f, 1.0f, 1.0f);

    if (m_showErrors) {
        ImGui::PushStyleColor(ImGuiCol_Button, errorColor);
    }
    if (ImGui::Button("Errors")) {
        m_showErrors = !m_showErrors;
    }
    if (m_showErrors) {
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    ImGui::Text("(%zu)", m_errorCount);

    ImGui::SameLine();
    if (m_showWarnings) {
        ImGui::PushStyleColor(ImGuiCol_Button, warningColor);
    }
    if (ImGui::Button("Warnings")) {
        m_showWarnings = !m_showWarnings;
    }
    if (m_showWarnings) {
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    ImGui::Text("(%zu)", m_warningCount);

    ImGui::SameLine();
    if (m_showInfo) {
        ImGui::PushStyleColor(ImGuiCol_Button, infoColor);
    }
    if (ImGui::Button("Info")) {
        m_showInfo = !m_showInfo;
    }
    if (m_showInfo) {
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    ImGui::Text("(%zu)", m_infoCount);
}

void ValidationPanel::RenderMessageList() {
    if (ImGui::BeginTable("ValidationMessages", 5,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                         ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {

        ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Config", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableHeadersRow();

        size_t idx = 0;
        for (const auto& msg : m_messages) {
            // Filter by severity
            if (!m_showErrors && msg.severity == Severity::Error) continue;
            if (!m_showWarnings && msg.severity == Severity::Warning) continue;
            if (!m_showInfo && msg.severity == Severity::Info) continue;

            // Filter by config
            if (!m_configFilter.empty() && msg.configId != m_configFilter) continue;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // Severity with color
            ImVec4 severityColor;
            const char* severityText;
            switch (msg.severity) {
                case Severity::Error:
                    severityColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    severityText = "Error";
                    break;
                case Severity::Warning:
                    severityColor = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                    severityText = "Warning";
                    break;
                case Severity::Info:
                    severityColor = ImVec4(0.3f, 0.8f, 1.0f, 1.0f);
                    severityText = "Info";
                    break;
            }

            ImGui::TextColored(severityColor, "%s", severityText);

            ImGui::TableNextColumn();
            if (ImGui::Selectable(msg.configId.c_str(), m_selectedMessageIdx == idx,
                                 ImGuiSelectableFlags_SpanAllColumns)) {
                m_selectedMessageIdx = idx;

                // Navigate to error on click
                if (OnErrorClicked) {
                    OnErrorClicked(msg.configId, msg.lineNumber);
                }
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", msg.field.c_str());

            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", msg.message.c_str());

            ImGui::TableNextColumn();
            if (msg.lineNumber > 0) {
                ImGui::Text("%d", msg.lineNumber);
            }

            idx++;
        }

        ImGui::EndTable();
    }
}

void ValidationPanel::RenderMessageDetails() {
    if (m_selectedMessageIdx >= m_messages.size()) {
        ImGui::TextDisabled("No message selected");
        return;
    }

    const auto& msg = m_messages[m_selectedMessageIdx];

    ImGui::Text("Details");
    ImGui::Separator();

    ImGui::Text("Config: %s", msg.configId.c_str());
    ImGui::Text("Path: %s", msg.configPath.c_str());
    ImGui::Text("Field: %s", msg.field.c_str());

    if (msg.lineNumber > 0) {
        ImGui::Text("Line: %d", msg.lineNumber);
    }

    ImGui::Separator();
    ImGui::TextWrapped("Message: %s", msg.message.c_str());

    ImGui::Separator();

    if (ImGui::Button("Go to Config")) {
        if (OnErrorClicked) {
            OnErrorClicked(msg.configId, msg.lineNumber);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy Message")) {
        ImGui::SetClipboardText(msg.message.c_str());
    }
}

void ValidationPanel::ValidateConfig(const std::string& configId) {
    auto& registry = Config::ConfigRegistry::Instance();
    auto result = registry.ValidateConfig(configId);
    ConvertValidationResult(configId, result);
}

void ValidationPanel::ValidateAllConfigs() {
    ClearValidation();

    auto& registry = Config::ConfigRegistry::Instance();
    auto results = registry.ValidateAll();

    for (const auto& [configId, result] : results) {
        ConvertValidationResult(configId, result);
    }
}

void ValidationPanel::ClearValidation() {
    m_messages.clear();
    m_errorCount = 0;
    m_warningCount = 0;
    m_infoCount = 0;
    m_selectedMessageIdx = -1;
}

size_t ValidationPanel::GetErrorCount() const {
    return m_errorCount;
}

size_t ValidationPanel::GetWarningCount() const {
    return m_warningCount;
}

void ValidationPanel::AddMessage(const ValidationMessage& msg) {
    m_messages.push_back(msg);

    switch (msg.severity) {
        case Severity::Error:
            m_errorCount++;
            break;
        case Severity::Warning:
            m_warningCount++;
            break;
        case Severity::Info:
            m_infoCount++;
            break;
    }
}

void ValidationPanel::ConvertValidationResult(const std::string& configId,
                                              const Config::ValidationResult& result) {
    auto& registry = Config::ConfigRegistry::Instance();
    auto config = registry.Get(configId);
    std::string configPath = config ? config->GetSourcePath() : "";

    // Convert errors
    for (const auto& error : result.GetErrors()) {
        ValidationMessage msg;
        msg.severity = Severity::Error;
        msg.configId = configId;
        msg.configPath = configPath;
        msg.field = error.field;
        msg.message = error.message;
        msg.lineNumber = 0;  // TODO: Parse JSON to find line number
        AddMessage(msg);
    }

    // Convert warnings
    for (const auto& warning : result.GetWarnings()) {
        ValidationMessage msg;
        msg.severity = Severity::Warning;
        msg.configId = configId;
        msg.configPath = configPath;
        msg.field = warning.field;
        msg.message = warning.message;
        msg.lineNumber = 0;
        AddMessage(msg);
    }

    // Convert info messages if present
    // (EntityConfig's ValidationResult might not have info messages, this is for future extension)
}

} // namespace Vehement
