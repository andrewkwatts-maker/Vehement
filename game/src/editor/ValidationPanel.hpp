#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "../config/EntityConfig.hpp"

namespace Vehement {

class Editor;

/**
 * @brief Validation panel for displaying config errors and warnings
 *
 * Features:
 * - Real-time validation as configs are edited
 * - Grouped errors by severity (error, warning, info)
 * - Click to navigate to error location
 * - Batch validation of all configs
 * - Export validation report
 */
class ValidationPanel {
public:
    enum class Severity {
        Error,
        Warning,
        Info
    };

    struct ValidationMessage {
        Severity severity;
        std::string configId;
        std::string configPath;
        std::string field;
        std::string message;
        int lineNumber;  // For JSON files
    };

    explicit ValidationPanel(Editor* editor);
    ~ValidationPanel();

    void Render();

    // Validation operations
    void ValidateConfig(const std::string& configId);
    void ValidateAllConfigs();
    void ClearValidation();

    // Results
    [[nodiscard]] size_t GetErrorCount() const;
    [[nodiscard]] size_t GetWarningCount() const;
    [[nodiscard]] bool HasErrors() const { return GetErrorCount() > 0; }
    [[nodiscard]] const std::vector<ValidationMessage>& GetMessages() const { return m_messages; }

    // Filtering
    void SetShowErrors(bool show) { m_showErrors = show; }
    void SetShowWarnings(bool show) { m_showWarnings = show; }
    void SetShowInfo(bool show) { m_showInfo = show; }
    void SetConfigFilter(const std::string& configId) { m_configFilter = configId; }

    // Callbacks
    std::function<void(const std::string&, int)> OnErrorClicked;  // configId, lineNumber

private:
    void RenderToolbar();
    void RenderMessageList();
    void RenderMessageDetails();

    void AddMessage(const ValidationMessage& msg);
    void ConvertValidationResult(const std::string& configId,
                                const Config::ValidationResult& result);

    Editor* m_editor = nullptr;

    std::vector<ValidationMessage> m_messages;
    size_t m_selectedMessageIdx = -1;

    // Filtering
    bool m_showErrors = true;
    bool m_showWarnings = true;
    bool m_showInfo = true;
    std::string m_configFilter;

    // Stats
    size_t m_errorCount = 0;
    size_t m_warningCount = 0;
    size_t m_infoCount = 0;
};

} // namespace Vehement
