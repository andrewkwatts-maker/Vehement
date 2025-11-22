#pragma once

#include <string>
#include <vector>
#include <functional>

namespace Vehement {

class Editor;

/**
 * @brief Debug console panel
 *
 * Features:
 * - Command input
 * - Log output with filtering
 * - Python REPL mode
 */
class Console {
public:
    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Error
    };

    explicit Console(Editor* editor);
    ~Console();

    void Render();

    // Logging
    void Log(const std::string& message, LogLevel level = LogLevel::Info);
    void Clear();

    // Command handling
    void ExecuteCommand(const std::string& command);

    std::function<std::string(const std::string&)> OnCommand;

private:
    void RenderToolbar();
    void RenderLog();
    void RenderInput();

    int TextEditCallback(void* data);

    Editor* m_editor = nullptr;

    struct LogEntry {
        std::string message;
        LogLevel level;
        std::string timestamp;
    };
    std::vector<LogEntry> m_log;

    // Input
    char m_inputBuffer[512] = "";
    std::vector<std::string> m_history;
    int m_historyIndex = -1;

    // Filters
    bool m_showDebug = true;
    bool m_showInfo = true;
    bool m_showWarning = true;
    bool m_showError = true;
    bool m_autoScroll = true;
    std::string m_filter;

    // Python REPL mode
    bool m_pythonMode = false;
};

} // namespace Vehement
