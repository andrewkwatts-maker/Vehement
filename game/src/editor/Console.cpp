#include "Console.hpp"
#include "Editor.hpp"
#include <imgui.h>
#include <ctime>
#include <algorithm>

namespace Vehement {

Console::Console(Editor* editor) : m_editor(editor) {
    Log("Console initialized", LogLevel::Info);
    Log("Type 'help' for available commands", LogLevel::Debug);
}

Console::~Console() = default;

void Console::Render() {
    if (!ImGui::Begin("Console")) {
        ImGui::End();
        return;
    }

    RenderToolbar();
    ImGui::Separator();
    RenderLog();
    ImGui::Separator();
    RenderInput();

    ImGui::End();
}

void Console::RenderToolbar() {
    if (ImGui::Button("Clear")) {
        Clear();
    }
    ImGui::SameLine();

    // Filter checkboxes
    ImGui::Checkbox("Debug", &m_showDebug);
    ImGui::SameLine();
    ImGui::Checkbox("Info", &m_showInfo);
    ImGui::SameLine();
    ImGui::Checkbox("Warning", &m_showWarning);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &m_showError);

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    ImGui::Checkbox("Auto-scroll", &m_autoScroll);

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Python mode toggle
    ImGui::Checkbox("Python REPL", &m_pythonMode);

    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    static char filterBuffer[128] = "";
    ImGui::SetNextItemWidth(180);
    if (ImGui::InputTextWithHint("##filter", "Filter...", filterBuffer, sizeof(filterBuffer))) {
        m_filter = filterBuffer;
    }
}

void Console::RenderLog() {
    float logHeight = ImGui::GetContentRegionAvail().y - 35;  // Leave room for input
    ImGui::BeginChild("LogArea", ImVec2(0, logHeight), false);

    for (const auto& entry : m_log) {
        // Apply level filter
        bool show = false;
        switch (entry.level) {
            case LogLevel::Debug: show = m_showDebug; break;
            case LogLevel::Info: show = m_showInfo; break;
            case LogLevel::Warning: show = m_showWarning; break;
            case LogLevel::Error: show = m_showError; break;
        }
        if (!show) continue;

        // Apply text filter
        if (!m_filter.empty()) {
            std::string msgLower = entry.message;
            std::string filterLower = m_filter;
            std::transform(msgLower.begin(), msgLower.end(), msgLower.begin(), ::tolower);
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
            if (msgLower.find(filterLower) == std::string::npos) continue;
        }

        // Color by level
        ImVec4 color;
        const char* prefix;
        switch (entry.level) {
            case LogLevel::Debug:
                color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                prefix = "[DBG]";
                break;
            case LogLevel::Info:
                color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                prefix = "[INF]";
                break;
            case LogLevel::Warning:
                color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                prefix = "[WRN]";
                break;
            case LogLevel::Error:
                color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                prefix = "[ERR]";
                break;
        }

        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "[%s]", entry.timestamp.c_str());
        ImGui::SameLine();
        ImGui::TextColored(color, "%s %s", prefix, entry.message.c_str());
    }

    // Auto-scroll
    if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

void Console::RenderInput() {
    // Input prefix
    if (m_pythonMode) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), ">>>");
    } else {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f), ">");
    }
    ImGui::SameLine();

    // Input field
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_CallbackHistory |
                                ImGuiInputTextFlags_CallbackCompletion;

    ImGui::SetNextItemWidth(-1);
    bool reclaim_focus = false;

    if (ImGui::InputText("##input", m_inputBuffer, sizeof(m_inputBuffer), flags,
        [](ImGuiInputTextCallbackData* data) -> int {
            Console* console = (Console*)data->UserData;
            return console->TextEditCallback(data);
        }, this)) {
        if (m_inputBuffer[0] != '\0') {
            ExecuteCommand(m_inputBuffer);
            m_inputBuffer[0] = '\0';
        }
        reclaim_focus = true;
    }

    // Auto-focus on window apparance
    ImGui::SetItemDefaultFocus();
    if (reclaim_focus) {
        ImGui::SetKeyboardFocusHere(-1);
    }
}

int Console::TextEditCallback(void* data) {
    ImGuiInputTextCallbackData* cbData = (ImGuiInputTextCallbackData*)data;

    switch (cbData->EventFlag) {
        case ImGuiInputTextFlags_CallbackHistory: {
            if (m_history.empty()) break;

            if (cbData->EventKey == ImGuiKey_UpArrow) {
                if (m_historyIndex == -1) {
                    m_historyIndex = (int)m_history.size() - 1;
                } else if (m_historyIndex > 0) {
                    m_historyIndex--;
                }
            } else if (cbData->EventKey == ImGuiKey_DownArrow) {
                if (m_historyIndex != -1) {
                    m_historyIndex++;
                    if (m_historyIndex >= (int)m_history.size()) {
                        m_historyIndex = -1;
                    }
                }
            }

            if (m_historyIndex != -1) {
                const std::string& historyStr = m_history[m_historyIndex];
                cbData->DeleteChars(0, cbData->BufTextLen);
                cbData->InsertChars(0, historyStr.c_str());
            }
            break;
        }
        case ImGuiInputTextFlags_CallbackCompletion: {
            // TODO: Tab completion
            break;
        }
    }
    return 0;
}

void Console::Log(const std::string& message, LogLevel level) {
    // Get timestamp
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);

    m_log.push_back({message, level, timestamp});

    // Limit log size
    const size_t MAX_LOG_SIZE = 1000;
    if (m_log.size() > MAX_LOG_SIZE) {
        m_log.erase(m_log.begin(), m_log.begin() + (m_log.size() - MAX_LOG_SIZE));
    }
}

void Console::Clear() {
    m_log.clear();
}

void Console::ExecuteCommand(const std::string& command) {
    // Add to history
    if (m_history.empty() || m_history.back() != command) {
        m_history.push_back(command);
    }
    m_historyIndex = -1;

    // Log command
    Log("> " + command, LogLevel::Info);

    // Handle built-in commands
    if (command == "help") {
        Log("Available commands:", LogLevel::Info);
        Log("  help      - Show this help", LogLevel::Debug);
        Log("  clear     - Clear console", LogLevel::Debug);
        Log("  python    - Toggle Python REPL mode", LogLevel::Debug);
        Log("  lua       - Execute Lua command (if available)", LogLevel::Debug);
        Log("  spawn <type> - Spawn entity", LogLevel::Debug);
        Log("  teleport <x> <y> <z> - Move camera", LogLevel::Debug);
        Log("  reload    - Hot-reload configs", LogLevel::Debug);
    }
    else if (command == "clear") {
        Clear();
    }
    else if (command == "python") {
        m_pythonMode = !m_pythonMode;
        Log(m_pythonMode ? "Python REPL enabled" : "Python REPL disabled", LogLevel::Info);
    }
    else if (command == "reload") {
        Log("Reloading configs...", LogLevel::Info);
        // TODO: Trigger hot-reload
        Log("Configs reloaded", LogLevel::Info);
    }
    else if (m_pythonMode) {
        // Execute as Python
        Log("Executing Python: " + command, LogLevel::Debug);
        // TODO: Actually execute Python
        Log("Python execution not implemented", LogLevel::Warning);
    }
    else {
        // Custom command handler
        if (OnCommand) {
            std::string result = OnCommand(command);
            if (!result.empty()) {
                Log(result, LogLevel::Info);
            }
        } else {
            Log("Unknown command: " + command, LogLevel::Error);
        }
    }
}

} // namespace Vehement
