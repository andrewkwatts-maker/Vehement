/**
 * @file ConsolePanel.cpp
 * @brief Implementation of Console/Log Panel for the Nova3D/Vehement editor
 */

#include "ConsolePanel.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstring>

// ImGui includes (assuming ImGui is used for UI)
#ifdef NOVA_USE_IMGUI
#include <imgui.h>
#include <imgui_internal.h>
#endif

namespace Nova {

// =============================================================================
// ConsoleLogEntry Implementation
// =============================================================================

std::string ConsoleLogEntry::GetFormattedTimestamp() const {
    auto time = Clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string ConsoleLogEntry::GetSourceLocation() const {
    if (!HasSourceLocation()) {
        return "";
    }

    // Extract just the filename from the path
    std::string_view path(sourceFile);
    auto pos = path.find_last_of("/\\");
    std::string_view filename = (pos != std::string_view::npos) ? path.substr(pos + 1) : path;

    std::ostringstream oss;
    oss << filename << ":" << sourceLine;
    return oss.str();
}

ConsoleLogEntry ConsoleLogEntry::FromLogEntry(const LogEntry& entry, uint64_t entryId) {
    ConsoleLogEntry consoleEntry;
    consoleEntry.id = entryId;
    consoleEntry.timestamp = entry.timestamp;
    consoleEntry.level = LogLevelToConsoleLevel(entry.level);
    consoleEntry.category = entry.category;
    consoleEntry.message = entry.message;
    consoleEntry.threadId = entry.threadId;

    if (entry.location.IsValid()) {
        consoleEntry.sourceFile = entry.location.file ? entry.location.file : "";
        consoleEntry.sourceLine = entry.location.line;
        consoleEntry.functionName = entry.location.function ? entry.location.function : "";
    }

    return consoleEntry;
}

// =============================================================================
// ConsolePanelLogSink Implementation
// =============================================================================

ConsolePanelLogSink::ConsolePanelLogSink(ConsolePanel* panel)
    : m_panel(panel) {
    SetFormatter(std::make_shared<TextLogFormatter>("%m"));
}

void ConsolePanelLogSink::Write(const LogEntry& entry) {
    if (!m_panel || !ShouldLog(entry.level)) return;

    // Forward to console panel (thread-safe)
    m_panel->AddEntry(ConsoleLogEntry::FromLogEntry(entry, 0));
}

void ConsolePanelLogSink::Flush() {
    // Console panel handles its own flushing
}

// =============================================================================
// ConsolePanel Implementation
// =============================================================================

ConsolePanel::ConsolePanel() {
    // Initialize level filters to show all
    for (int i = 0; i < 6; ++i) {
        m_levelFilters[i] = true;
    }
}

ConsolePanel::~ConsolePanel() {
    UnhookFromLogger();
}

bool ConsolePanel::Initialize(const Config& config, const ConsolePanelConfig& consoleConfig) {
    m_consoleConfig = consoleConfig;
    m_showTimestamps = consoleConfig.showTimestamps;
    m_showCategories = consoleConfig.showCategories;
    m_showSourceLocations = consoleConfig.showSourceLocations;
    m_collapseDuplicates = consoleConfig.collapseDuplicates;
    m_autoScrollEnabled = consoleConfig.autoScroll;

    return EditorPanel::Initialize(config);
}

void ConsolePanel::OnInitialize() {
    RegisterBuiltInCommands();
}

void ConsolePanel::OnShutdown() {
    UnhookFromLogger();
    Clear();
    m_commands.clear();
    m_commandAliases.clear();
}

void ConsolePanel::HookIntoLogger() {
    if (m_isHookedToLogger) return;

    m_logSink = std::make_shared<ConsolePanelLogSink>(this);
    LogManager::Instance().AddSink(m_logSink);
    m_isHookedToLogger = true;
}

void ConsolePanel::UnhookFromLogger() {
    if (!m_isHookedToLogger || !m_logSink) return;

    LogManager::Instance().RemoveSink(m_logSink.get());
    m_logSink.reset();
    m_isHookedToLogger = false;
}

// =============================================================================
// Message Management
// =============================================================================

void ConsolePanel::AddMessage(ConsoleLogLevel level, const std::string& message,
                              const std::string& category) {
    ConsoleLogEntry entry;
    entry.timestamp = ConsoleLogEntry::Clock::now();
    entry.level = level;
    entry.message = message;
    entry.category = category;
    entry.threadId = std::this_thread::get_id();

    AddEntry(std::move(entry));
}

void ConsolePanel::AddMessage(ConsoleLogLevel level, const std::string& message,
                              const std::string& category, const std::string& sourceFile,
                              uint32_t sourceLine, const std::string& functionName) {
    ConsoleLogEntry entry;
    entry.timestamp = ConsoleLogEntry::Clock::now();
    entry.level = level;
    entry.message = message;
    entry.category = category;
    entry.sourceFile = sourceFile;
    entry.sourceLine = sourceLine;
    entry.functionName = functionName;
    entry.threadId = std::this_thread::get_id();

    AddEntry(std::move(entry));
}

void ConsolePanel::AddEntry(ConsoleLogEntry entry) {
    // Assign ID if not set
    if (entry.id == 0) {
        entry.id = m_nextEntryId++;
    }

    // Thread-safe: add to pending queue
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pendingEntries.push_back(std::move(entry));
    }
}

void ConsolePanel::ProcessPendingEntries() {
    std::vector<ConsoleLogEntry> pending;
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        if (m_pendingEntries.empty()) return;
        pending = std::move(m_pendingEntries);
        m_pendingEntries.clear();
    }

    for (auto& entry : pending) {
        AddEntryInternal(std::move(entry));
    }
}

void ConsolePanel::AddEntryInternal(ConsoleLogEntry entry) {
    // Update statistics
    m_stats.Increment(entry.level);

    // Track category
    {
        std::lock_guard<std::mutex> lock(m_categoriesMutex);
        m_knownCategories.insert(entry.category);
    }

    // Handle error notifications
    if (entry.level >= ConsoleLogLevel::Error) {
        ++m_unreadErrorCount;
        m_lastError = entry;
        if (m_consoleConfig.showErrorNotifications) {
            m_showErrorNotification = true;
            m_errorNotificationTimer = m_consoleConfig.errorNotificationDuration;
        }
        if (Callbacks.onErrorOccurred) {
            Callbacks.onErrorOccurred(entry);
        }
    }

    // Check if we should collapse with previous entry
    bool collapsed = false;
    if (m_collapseDuplicates && !m_entries.empty()) {
        auto& last = m_entries.back();
        if (last.level == entry.level &&
            last.category == entry.category &&
            last.message == entry.message) {
            last.duplicateCount++;
            last.isCollapsed = true;
            last.timestamp = entry.timestamp;  // Update to latest timestamp
            collapsed = true;
        }
    }

    if (!collapsed) {
        std::lock_guard<std::mutex> lock(m_entriesMutex);
        entry.id = m_nextEntryId++;
        m_entries.push_back(std::move(entry));
        TrimToMaxEntries();
    }

    // Mark filters as dirty to rebuild filtered list
    m_filtersDirty = true;

    // Request scroll to bottom if auto-scroll is enabled
    if (m_autoScrollEnabled && !m_userScrolledUp) {
        m_scrollToBottomRequested = true;
    }
}

void ConsolePanel::Clear() {
    {
        std::lock_guard<std::mutex> lock(m_entriesMutex);
        m_entries.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pendingEntries.clear();
    }
    m_filteredIndices.clear();
    m_selectedEntryIds.clear();
    m_stats.Reset();
    m_unreadErrorCount = 0;
    m_filtersDirty = true;

    if (Callbacks.onCleared) {
        Callbacks.onCleared();
    }
}

size_t ConsolePanel::GetEntryCount() const {
    std::lock_guard<std::mutex> lock(m_entriesMutex);
    return m_entries.size();
}

size_t ConsolePanel::GetVisibleEntryCount() const {
    return m_filteredIndices.size();
}

void ConsolePanel::SetMaxEntries(size_t max) {
    m_consoleConfig.maxEntries = max;
    TrimToMaxEntries();
}

void ConsolePanel::TrimToMaxEntries() {
    // Assumes m_entriesMutex is already held
    while (m_entries.size() > m_consoleConfig.maxEntries) {
        m_entries.pop_front();
    }
}

void ConsolePanel::SetConsoleConfig(const ConsolePanelConfig& config) {
    m_consoleConfig = config;
    m_filtersDirty = true;
}

// =============================================================================
// Filtering
// =============================================================================

void ConsolePanel::SetLevelFilter(ConsoleLogLevel level, bool show) {
    int index = static_cast<int>(level);
    if (index >= 0 && index < 6) {
        m_levelFilters[index] = show;
        m_filtersDirty = true;
    }
}

bool ConsolePanel::GetLevelFilter(ConsoleLogLevel level) const {
    int index = static_cast<int>(level);
    if (index >= 0 && index < 6) {
        return m_levelFilters[index];
    }
    return true;
}

void ConsolePanel::SetCategoryFilter(const std::string& category) {
    m_categoryFilter = category;
    m_filtersDirty = true;
}

std::vector<std::string> ConsolePanel::GetKnownCategories() const {
    std::lock_guard<std::mutex> lock(m_categoriesMutex);
    return std::vector<std::string>(m_knownCategories.begin(), m_knownCategories.end());
}

void ConsolePanel::SetTextFilter(const std::string& text, bool useRegex) {
    m_textFilter = text;
    m_useRegexFilter = useRegex;

    if (useRegex && !text.empty()) {
        try {
            m_filterRegex = std::regex(text, std::regex::icase);
        } catch (const std::regex_error&) {
            m_filterRegex.reset();
        }
    } else {
        m_filterRegex.reset();
    }

    m_filtersDirty = true;
}

void ConsolePanel::ClearFilters() {
    for (int i = 0; i < 6; ++i) {
        m_levelFilters[i] = true;
    }
    m_categoryFilter.clear();
    m_textFilter.clear();
    m_textFilterBuffer[0] = '\0';
    m_useRegexFilter = false;
    m_filterRegex.reset();
    m_filtersDirty = true;
}

void ConsolePanel::UpdateFilteredEntries() {
    if (!m_filtersDirty) return;

    m_filteredIndices.clear();

    std::lock_guard<std::mutex> lock(m_entriesMutex);
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (EntryMatchesFilters(m_entries[i])) {
            m_filteredIndices.push_back(i);
        }
    }

    m_filtersDirty = false;
}

bool ConsolePanel::EntryMatchesFilters(const ConsoleLogEntry& entry) const {
    // Level filter
    int levelIndex = static_cast<int>(entry.level);
    if (levelIndex >= 0 && levelIndex < 6 && !m_levelFilters[levelIndex]) {
        return false;
    }

    // Category filter
    if (!m_categoryFilter.empty() && entry.category != m_categoryFilter) {
        return false;
    }

    // Text filter
    if (!m_textFilter.empty()) {
        if (m_useRegexFilter && m_filterRegex.has_value()) {
            if (!std::regex_search(entry.message, m_filterRegex.value())) {
                return false;
            }
        } else {
            // Case-insensitive substring search
            std::string lowerMessage = entry.message;
            std::string lowerFilter = m_textFilter;
            std::transform(lowerMessage.begin(), lowerMessage.end(), lowerMessage.begin(), ::tolower);
            std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
            if (lowerMessage.find(lowerFilter) == std::string::npos) {
                return false;
            }
        }
    }

    return true;
}

// =============================================================================
// Display Options
// =============================================================================

void ConsolePanel::SetShowTimestamps(bool show) {
    m_showTimestamps = show;
}

void ConsolePanel::SetShowCategories(bool show) {
    m_showCategories = show;
}

void ConsolePanel::SetShowSourceLocations(bool show) {
    m_showSourceLocations = show;
}

void ConsolePanel::SetCollapseDuplicates(bool collapse) {
    m_collapseDuplicates = collapse;
    // Note: This doesn't uncollapse existing entries
}

void ConsolePanel::SetAutoScroll(bool autoScroll) {
    m_autoScrollEnabled = autoScroll;
}

void ConsolePanel::ScrollToBottom() {
    m_scrollToBottomRequested = true;
    m_userScrolledUp = false;
}

void ConsolePanel::ScrollToTop() {
    m_scrollY = 0.0f;
    m_userScrolledUp = true;
}

// =============================================================================
// Selection
// =============================================================================

void ConsolePanel::SelectEntry(size_t index, bool addToSelection) {
    if (index >= m_filteredIndices.size()) return;

    std::lock_guard<std::mutex> lock(m_entriesMutex);
    size_t entryIndex = m_filteredIndices[index];
    if (entryIndex >= m_entries.size()) return;

    uint64_t entryId = m_entries[entryIndex].id;

    if (!addToSelection) {
        m_selectedEntryIds.clear();
    }

    m_selectedEntryIds.insert(entryId);
    m_lastSelectedId = entryId;
}

void ConsolePanel::ClearSelection() {
    m_selectedEntryIds.clear();
    m_lastSelectedId = 0;
}

std::vector<const ConsoleLogEntry*> ConsolePanel::GetSelectedEntries() const {
    std::vector<const ConsoleLogEntry*> result;
    std::lock_guard<std::mutex> lock(m_entriesMutex);

    for (const auto& entry : m_entries) {
        if (m_selectedEntryIds.count(entry.id) > 0) {
            result.push_back(&entry);
        }
    }

    return result;
}

void ConsolePanel::CopySelectedToClipboard() {
    auto selected = GetSelectedEntries();
    if (selected.empty()) return;

    std::ostringstream oss;
    for (const auto* entry : selected) {
        oss << FormatEntryForClipboard(*entry) << "\n";
    }

    SetClipboardText(oss.str());
}

void ConsolePanel::CopyAllToClipboard() {
    std::lock_guard<std::mutex> lock(m_entriesMutex);

    std::ostringstream oss;
    for (size_t idx : m_filteredIndices) {
        if (idx < m_entries.size()) {
            oss << FormatEntryForClipboard(m_entries[idx]) << "\n";
        }
    }

    SetClipboardText(oss.str());
}

std::string ConsolePanel::FormatEntryForClipboard(const ConsoleLogEntry& entry) const {
    std::ostringstream oss;

    if (m_showTimestamps) {
        oss << "[" << entry.GetFormattedTimestamp() << "] ";
    }

    oss << "[" << ConsoleLogLevelToString(entry.level) << "] ";

    if (m_showCategories && !entry.category.empty()) {
        oss << "[" << entry.category << "] ";
    }

    oss << entry.message;

    if (m_showSourceLocations && entry.HasSourceLocation()) {
        oss << " (" << entry.GetSourceLocation() << ")";
    }

    if (entry.duplicateCount > 1) {
        oss << " (x" << entry.duplicateCount << ")";
    }

    return oss.str();
}

void ConsolePanel::SetClipboardText(const std::string& text) {
#ifdef NOVA_USE_IMGUI
    ImGui::SetClipboardText(text.c_str());
#elif defined(_WIN32)
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hMem) {
            memcpy(GlobalLock(hMem), text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
#endif
}

// =============================================================================
// Commands
// =============================================================================

void ConsolePanel::RegisterBuiltInCommands() {
    // Clear command
    RegisterCommand({
        .name = "clear",
        .description = "Clear the console",
        .usage = "clear",
        .aliases = {"cls"},
        .handler = [this](const std::vector<std::string>&) {
            Clear();
        }
    });

    // Help command
    RegisterCommand({
        .name = "help",
        .description = "Show available commands",
        .usage = "help [command]",
        .aliases = {"?"},
        .handler = [this](const std::vector<std::string>& args) {
            if (args.size() > 1) {
                // Show help for specific command
                auto it = m_commands.find(args[1]);
                if (it == m_commands.end()) {
                    // Check aliases
                    auto aliasIt = m_commandAliases.find(args[1]);
                    if (aliasIt != m_commandAliases.end()) {
                        it = m_commands.find(aliasIt->second);
                    }
                }

                if (it != m_commands.end()) {
                    AddMessage(ConsoleLogLevel::Info,
                        it->second.name + ": " + it->second.description, "Console");
                    AddMessage(ConsoleLogLevel::Info,
                        "Usage: " + it->second.usage, "Console");
                } else {
                    AddMessage(ConsoleLogLevel::Warning,
                        "Unknown command: " + args[1], "Console");
                }
            } else {
                // Show all commands
                AddMessage(ConsoleLogLevel::Info, "Available commands:", "Console");
                for (const auto& [name, cmd] : m_commands) {
                    AddMessage(ConsoleLogLevel::Info,
                        "  " + name + " - " + cmd.description, "Console");
                }
            }
        }
    });

    // Echo command
    RegisterCommand({
        .name = "echo",
        .description = "Print a message",
        .usage = "echo <message>",
        .aliases = {"print"},
        .handler = [this](const std::vector<std::string>& args) {
            if (args.size() > 1) {
                std::string message;
                for (size_t i = 1; i < args.size(); ++i) {
                    if (i > 1) message += " ";
                    message += args[i];
                }
                AddMessage(ConsoleLogLevel::Info, message, "Console");
            }
        }
    });

    // Filter command
    RegisterCommand({
        .name = "filter",
        .description = "Set text filter",
        .usage = "filter [text] | filter -regex <pattern> | filter -clear",
        .aliases = {},
        .handler = [this](const std::vector<std::string>& args) {
            if (args.size() == 1 || args[1] == "-clear") {
                SetTextFilter("");
                AddMessage(ConsoleLogLevel::Info, "Filter cleared", "Console");
            } else if (args.size() > 2 && args[1] == "-regex") {
                SetTextFilter(args[2], true);
                AddMessage(ConsoleLogLevel::Info, "Regex filter set: " + args[2], "Console");
            } else {
                SetTextFilter(args[1], false);
                AddMessage(ConsoleLogLevel::Info, "Filter set: " + args[1], "Console");
            }
        }
    });

    // Save command
    RegisterCommand({
        .name = "save",
        .description = "Save log to file",
        .usage = "save <filepath>",
        .aliases = {"export"},
        .handler = [this](const std::vector<std::string>& args) {
            if (args.size() > 1) {
                if (SaveToFile(args[1])) {
                    AddMessage(ConsoleLogLevel::Info, "Log saved to: " + args[1], "Console");
                } else {
                    AddMessage(ConsoleLogLevel::Error, "Failed to save log to: " + args[1], "Console");
                }
            } else {
                AddMessage(ConsoleLogLevel::Warning, "Usage: save <filepath>", "Console");
            }
        }
    });

    // Stats command
    RegisterCommand({
        .name = "stats",
        .description = "Show log statistics",
        .usage = "stats",
        .aliases = {},
        .handler = [this](const std::vector<std::string>&) {
            std::ostringstream oss;
            oss << "Log Statistics:\n";
            oss << "  Total: " << m_stats.totalCount.load() << "\n";
            oss << "  Trace: " << m_stats.traceCount.load() << "\n";
            oss << "  Debug: " << m_stats.debugCount.load() << "\n";
            oss << "  Info: " << m_stats.infoCount.load() << "\n";
            oss << "  Warning: " << m_stats.warningCount.load() << "\n";
            oss << "  Error: " << m_stats.errorCount.load() << "\n";
            oss << "  Fatal: " << m_stats.fatalCount.load();
            AddMessage(ConsoleLogLevel::Info, oss.str(), "Console");
        }
    });

    // Level command
    RegisterCommand({
        .name = "level",
        .description = "Toggle level filter",
        .usage = "level <trace|debug|info|warning|error|fatal> [on|off]",
        .aliases = {},
        .handler = [this](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                AddMessage(ConsoleLogLevel::Warning, "Usage: level <level> [on|off]", "Console");
                return;
            }

            ConsoleLogLevel level;
            std::string levelStr = args[1];
            std::transform(levelStr.begin(), levelStr.end(), levelStr.begin(), ::tolower);

            if (levelStr == "trace") level = ConsoleLogLevel::Trace;
            else if (levelStr == "debug") level = ConsoleLogLevel::Debug;
            else if (levelStr == "info") level = ConsoleLogLevel::Info;
            else if (levelStr == "warning" || levelStr == "warn") level = ConsoleLogLevel::Warning;
            else if (levelStr == "error") level = ConsoleLogLevel::Error;
            else if (levelStr == "fatal") level = ConsoleLogLevel::Fatal;
            else {
                AddMessage(ConsoleLogLevel::Warning, "Unknown level: " + args[1], "Console");
                return;
            }

            bool show = true;
            if (args.size() > 2) {
                std::string onoff = args[2];
                std::transform(onoff.begin(), onoff.end(), onoff.begin(), ::tolower);
                show = (onoff == "on" || onoff == "true" || onoff == "1");
            } else {
                show = !GetLevelFilter(level);  // Toggle
            }

            SetLevelFilter(level, show);
            AddMessage(ConsoleLogLevel::Info,
                std::string(ConsoleLogLevelToString(level)) + " filter: " + (show ? "ON" : "OFF"),
                "Console");
        }
    });
}

void ConsolePanel::RegisterCommand(const ConsoleCommand& command) {
    m_commands[command.name] = command;

    // Register aliases
    for (const auto& alias : command.aliases) {
        m_commandAliases[alias] = command.name;
    }
}

void ConsolePanel::UnregisterCommand(const std::string& name) {
    auto it = m_commands.find(name);
    if (it != m_commands.end()) {
        // Remove aliases
        for (const auto& alias : it->second.aliases) {
            m_commandAliases.erase(alias);
        }
        m_commands.erase(it);
    }
}

void ConsolePanel::ExecuteCommand(const std::string& commandLine) {
    if (commandLine.empty()) return;

    // Add to history
    if (m_commandHistory.empty() || m_commandHistory.back() != commandLine) {
        m_commandHistory.push_back(commandLine);
        if (m_commandHistory.size() > m_consoleConfig.commandHistorySize) {
            m_commandHistory.erase(m_commandHistory.begin());
        }
    }
    m_commandHistoryIndex = -1;

    // Parse command
    auto args = ParseCommandLine(commandLine);
    if (args.empty()) return;

    std::string cmdName = args[0];
    std::transform(cmdName.begin(), cmdName.end(), cmdName.begin(), ::tolower);

    // Check for alias
    auto aliasIt = m_commandAliases.find(cmdName);
    if (aliasIt != m_commandAliases.end()) {
        cmdName = aliasIt->second;
    }

    // Find and execute command
    auto it = m_commands.find(cmdName);
    if (it != m_commands.end()) {
        try {
            it->second.handler(args);
        } catch (const std::exception& e) {
            AddMessage(ConsoleLogLevel::Error,
                "Command error: " + std::string(e.what()), "Console");
        }
    } else {
        AddMessage(ConsoleLogLevel::Warning,
            "Unknown command: " + args[0] + ". Type 'help' for available commands.", "Console");
    }

    if (Callbacks.onCommandExecuted) {
        Callbacks.onCommandExecuted(commandLine);
    }
}

std::vector<std::string> ConsolePanel::ParseCommandLine(const std::string& commandLine) const {
    std::vector<std::string> args;
    std::string current;
    bool inQuotes = false;
    char quoteChar = 0;

    for (char c : commandLine) {
        if (inQuotes) {
            if (c == quoteChar) {
                inQuotes = false;
            } else {
                current += c;
            }
        } else {
            if (c == '"' || c == '\'') {
                inQuotes = true;
                quoteChar = c;
            } else if (c == ' ' || c == '\t') {
                if (!current.empty()) {
                    args.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
    }

    if (!current.empty()) {
        args.push_back(current);
    }

    return args;
}

std::vector<AutoCompleteSuggestion> ConsolePanel::GetAutoComplete(const std::string& partial) const {
    std::vector<AutoCompleteSuggestion> suggestions;

    if (partial.empty()) {
        // Show all commands
        for (const auto& [name, cmd] : m_commands) {
            suggestions.push_back({name, cmd.description, 0});
        }
    } else {
        std::string lowerPartial = partial;
        std::transform(lowerPartial.begin(), lowerPartial.end(), lowerPartial.begin(), ::tolower);

        // Match commands and aliases
        for (const auto& [name, cmd] : m_commands) {
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            if (lowerName.find(lowerPartial) == 0) {
                suggestions.push_back({name, cmd.description, 100 - static_cast<int>(name.length())});
            } else if (lowerName.find(lowerPartial) != std::string::npos) {
                suggestions.push_back({name, cmd.description, 50 - static_cast<int>(name.length())});
            }
        }

        for (const auto& [alias, cmdName] : m_commandAliases) {
            std::string lowerAlias = alias;
            std::transform(lowerAlias.begin(), lowerAlias.end(), lowerAlias.begin(), ::tolower);

            if (lowerAlias.find(lowerPartial) == 0) {
                auto it = m_commands.find(cmdName);
                if (it != m_commands.end()) {
                    suggestions.push_back({alias, it->second.description + " (alias for " + cmdName + ")",
                                          90 - static_cast<int>(alias.length())});
                }
            }
        }
    }

    // Sort by relevance
    std::sort(suggestions.begin(), suggestions.end(),
        [](const auto& a, const auto& b) { return a.relevance > b.relevance; });

    return suggestions;
}

// =============================================================================
// Export
// =============================================================================

bool ConsolePanel::SaveToFile(const std::string& filePath, bool filteredOnly) const {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    file << ExportAsText(filteredOnly);
    return true;
}

std::string ConsolePanel::ExportAsJson(bool filteredOnly) const {
    std::lock_guard<std::mutex> lock(m_entriesMutex);

    std::ostringstream oss;
    oss << "[\n";

    bool first = true;
    auto outputEntry = [&](const ConsoleLogEntry& entry) {
        if (!first) oss << ",\n";
        first = false;

        oss << "  {\n";
        oss << "    \"id\": " << entry.id << ",\n";
        oss << "    \"timestamp\": \"" << entry.GetFormattedTimestamp() << "\",\n";
        oss << "    \"level\": \"" << ConsoleLogLevelToString(entry.level) << "\",\n";
        oss << "    \"category\": \"" << entry.category << "\",\n";
        oss << "    \"message\": \"" << entry.message << "\"";  // TODO: escape JSON

        if (entry.HasSourceLocation()) {
            oss << ",\n    \"source\": \"" << entry.GetSourceLocation() << "\"";
        }

        if (entry.duplicateCount > 1) {
            oss << ",\n    \"count\": " << entry.duplicateCount;
        }

        oss << "\n  }";
    };

    if (filteredOnly) {
        for (size_t idx : m_filteredIndices) {
            if (idx < m_entries.size()) {
                outputEntry(m_entries[idx]);
            }
        }
    } else {
        for (const auto& entry : m_entries) {
            outputEntry(entry);
        }
    }

    oss << "\n]";
    return oss.str();
}

std::string ConsolePanel::ExportAsText(bool filteredOnly) const {
    std::lock_guard<std::mutex> lock(m_entriesMutex);

    std::ostringstream oss;

    auto outputEntry = [&](const ConsoleLogEntry& entry) {
        oss << FormatEntryForClipboard(entry) << "\n";
    };

    if (filteredOnly) {
        for (size_t idx : m_filteredIndices) {
            if (idx < m_entries.size()) {
                outputEntry(m_entries[idx]);
            }
        }
    } else {
        for (const auto& entry : m_entries) {
            outputEntry(entry);
        }
    }

    return oss.str();
}

// =============================================================================
// Color Helpers
// =============================================================================

glm::vec4 ConsolePanel::GetLevelColor(ConsoleLogLevel level) const {
    switch (level) {
        case ConsoleLogLevel::Trace:   return {0.5f, 0.5f, 0.5f, 1.0f};   // Gray
        case ConsoleLogLevel::Debug:   return {0.4f, 0.8f, 0.9f, 1.0f};   // Cyan
        case ConsoleLogLevel::Info:    return {0.8f, 0.8f, 0.8f, 1.0f};   // Light gray
        case ConsoleLogLevel::Warning: return {1.0f, 0.8f, 0.2f, 1.0f};   // Yellow
        case ConsoleLogLevel::Error:   return {1.0f, 0.4f, 0.4f, 1.0f};   // Red
        case ConsoleLogLevel::Fatal:   return {1.0f, 0.2f, 0.6f, 1.0f};   // Magenta
        default:                       return {1.0f, 1.0f, 1.0f, 1.0f};   // White
    }
}

const char* ConsolePanel::GetLevelIcon(ConsoleLogLevel level) const {
    switch (level) {
        case ConsoleLogLevel::Trace:   return "[T]";
        case ConsoleLogLevel::Debug:   return "[D]";
        case ConsoleLogLevel::Info:    return "[I]";
        case ConsoleLogLevel::Warning: return "[W]";
        case ConsoleLogLevel::Error:   return "[E]";
        case ConsoleLogLevel::Fatal:   return "[F]";
        default:                       return "[?]";
    }
}

// =============================================================================
// Update
// =============================================================================

void ConsolePanel::Update(float deltaTime) {
    EditorPanel::Update(deltaTime);

    // Process pending entries from other threads
    ProcessPendingEntries();

    // Update error notification timer
    if (m_showErrorNotification) {
        m_errorNotificationTimer -= deltaTime;
        if (m_errorNotificationTimer <= 0.0f) {
            m_showErrorNotification = false;
        }
    }

    // Update filtered entries if needed
    UpdateFilteredEntries();
}

// =============================================================================
// Rendering
// =============================================================================

void ConsolePanel::OnRender() {
#ifdef NOVA_USE_IMGUI
    // Filter bar
    RenderFilterBar();

    ImGui::Separator();

    // Log entries area
    float footerHeight = ImGui::GetFrameHeightWithSpacing() + 4;  // Command input
    ImVec2 logRegion = ImGui::GetContentRegionAvail();
    logRegion.y -= footerHeight;

    if (ImGui::BeginChild("LogScrollRegion", logRegion, false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        RenderLogEntries();
    }
    ImGui::EndChild();

    // Check if user scrolled up
    if (ImGui::IsItemHovered()) {
        if (ImGui::GetIO().MouseWheel != 0.0f) {
            float scrollY = ImGui::GetScrollY();
            float maxScrollY = ImGui::GetScrollMaxY();
            m_userScrolledUp = (scrollY < maxScrollY - 10.0f);
        }
    }

    // Auto-scroll to bottom
    if (m_scrollToBottomRequested) {
        ImGui::SetScrollHereY(1.0f);
        m_scrollToBottomRequested = false;
    }

    ImGui::Separator();

    // Command input
    RenderCommandInput();

    // Context menu
    if (m_showContextMenu) {
        RenderContextMenu();
    }

    // Error notification popup
    if (m_showErrorNotification) {
        RenderErrorNotification();
    }
#else
    // Fallback for non-ImGui builds
    // This would use EditorWidgets
#endif
}

void ConsolePanel::RenderFilterBar() {
#ifdef NOVA_USE_IMGUI
    // Level filter toggle buttons
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 3));

    const char* levelNames[] = {"T", "D", "I", "W", "E", "F"};
    const ImVec4 levelColors[] = {
        {0.5f, 0.5f, 0.5f, 1.0f},   // Trace
        {0.4f, 0.8f, 0.9f, 1.0f},   // Debug
        {0.8f, 0.8f, 0.8f, 1.0f},   // Info
        {1.0f, 0.8f, 0.2f, 1.0f},   // Warning
        {1.0f, 0.4f, 0.4f, 1.0f},   // Error
        {1.0f, 0.2f, 0.6f, 1.0f}    // Fatal
    };

    for (int i = 0; i < 6; ++i) {
        if (i > 0) ImGui::SameLine();

        if (m_levelFilters[i]) {
            ImGui::PushStyleColor(ImGuiCol_Button, levelColors[i]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(levelColors[i].x * 1.1f, levelColors[i].y * 1.1f,
                       levelColors[i].z * 1.1f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        }

        if (ImGui::Button(levelNames[i], ImVec2(24, 0))) {
            m_levelFilters[i] = !m_levelFilters[i];
            m_filtersDirty = true;
        }

        ImGui::PopStyleColor(2);

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s (%s)",
                std::string(ConsoleLogLevelToString(static_cast<ConsoleLogLevel>(i))).c_str(),
                m_levelFilters[i] ? "shown" : "hidden");
        }
    }

    ImGui::PopStyleVar(2);

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Category dropdown
    ImGui::SetNextItemWidth(120);
    auto categories = GetKnownCategories();
    std::sort(categories.begin(), categories.end());

    if (ImGui::BeginCombo("##CategoryFilter",
                          m_categoryFilter.empty() ? "All Categories" : m_categoryFilter.c_str())) {
        if (ImGui::Selectable("All Categories", m_categoryFilter.empty())) {
            SetCategoryFilter("");
        }
        ImGui::Separator();
        for (const auto& cat : categories) {
            if (ImGui::Selectable(cat.c_str(), m_categoryFilter == cat)) {
                SetCategoryFilter(cat);
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Text filter
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputTextWithHint("##TextFilter", "Search...", m_textFilterBuffer,
                                  sizeof(m_textFilterBuffer),
                                  ImGuiInputTextFlags_EnterReturnsTrue)) {
        SetTextFilter(m_textFilterBuffer, m_useRegexFilter);
    }

    if (ImGui::IsItemDeactivatedAfterEdit()) {
        SetTextFilter(m_textFilterBuffer, m_useRegexFilter);
    }

    ImGui::SameLine();

    // Regex toggle
    if (ImGui::Checkbox("Regex", &m_useRegexFilter)) {
        SetTextFilter(m_textFilterBuffer, m_useRegexFilter);
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Clear filters button
    if (ImGui::Button("Clear Filters")) {
        ClearFilters();
    }

    ImGui::SameLine();

    // Options dropdown
    if (ImGui::BeginMenu("Options")) {
        ImGui::Checkbox("Show Timestamps", &m_showTimestamps);
        ImGui::Checkbox("Show Categories", &m_showCategories);
        ImGui::Checkbox("Show Source Locations", &m_showSourceLocations);
        ImGui::Checkbox("Collapse Duplicates", &m_collapseDuplicates);
        ImGui::Separator();
        if (ImGui::Checkbox("Auto-Scroll", &m_autoScrollEnabled)) {
            if (m_autoScrollEnabled) {
                m_userScrolledUp = false;
                m_scrollToBottomRequested = true;
            }
        }
        ImGui::EndMenu();
    }
#endif
}

void ConsolePanel::RenderLogEntries() {
#ifdef NOVA_USE_IMGUI
    std::lock_guard<std::mutex> lock(m_entriesMutex);

    if (m_filteredIndices.empty()) {
        ImGui::TextDisabled("No log entries");
        return;
    }

    // Use clipper for virtual scrolling
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_filteredIndices.size()), m_rowHeight);

    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            size_t entryIndex = m_filteredIndices[i];
            if (entryIndex >= m_entries.size()) continue;

            const auto& entry = m_entries[entryIndex];
            RenderLogEntry(entry, i, i * m_rowHeight);
        }
    }
#endif
}

void ConsolePanel::RenderLogEntry(const ConsoleLogEntry& entry, size_t visualIndex, float yOffset) {
#ifdef NOVA_USE_IMGUI
    ImGui::PushID(static_cast<int>(entry.id));

    // Determine if selected
    bool isSelected = m_selectedEntryIds.count(entry.id) > 0;

    // Calculate row rect
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImVec2 availWidth = ImGui::GetContentRegionAvail();
    ImRect rowRect(cursorPos, ImVec2(cursorPos.x + availWidth.x, cursorPos.y + m_rowHeight));

    // Handle selection
    if (ImGui::IsMouseHoveringRect(rowRect.Min, rowRect.Max)) {
        if (ImGui::IsMouseClicked(0)) {
            HandleEntryClick(entry, visualIndex,
                             ImGui::GetIO().KeyCtrl, ImGui::GetIO().KeyShift);
        } else if (ImGui::IsMouseDoubleClicked(0)) {
            HandleEntryDoubleClick(entry);
        } else if (ImGui::IsMouseClicked(1)) {
            m_showContextMenu = true;
            m_contextMenuEntryIndex = visualIndex;
            if (!isSelected) {
                ClearSelection();
                m_selectedEntryIds.insert(entry.id);
            }
        }
    }

    // Draw selection background
    if (isSelected) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            rowRect.Min, rowRect.Max,
            ImGui::GetColorU32(ImGuiCol_Header));
    } else if (ImGui::IsMouseHoveringRect(rowRect.Min, rowRect.Max)) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            rowRect.Min, rowRect.Max,
            ImGui::GetColorU32(ImGuiCol_HeaderHovered, 0.5f));
    }

    // Render entry content
    glm::vec4 color = GetLevelColor(entry.level);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color.r, color.g, color.b, color.a));

    float x = cursorPos.x + 4;

    // Level icon
    ImGui::SetCursorScreenPos(ImVec2(x, cursorPos.y + 2));
    ImGui::TextUnformatted(GetLevelIcon(entry.level));
    x += 30;

    // Timestamp
    if (m_showTimestamps) {
        ImGui::SameLine();
        ImGui::SetCursorScreenPos(ImVec2(x, cursorPos.y + 2));
        ImGui::TextUnformatted(entry.GetFormattedTimestamp().c_str());
        x += 90;
    }

    // Category
    if (m_showCategories && !entry.category.empty()) {
        ImGui::SameLine();
        ImGui::SetCursorScreenPos(ImVec2(x, cursorPos.y + 2));
        ImGui::TextDisabled("[%s]", entry.category.c_str());
        x += ImGui::CalcTextSize(entry.category.c_str()).x + 20;
    }

    // Message
    ImGui::SameLine();
    ImGui::SetCursorScreenPos(ImVec2(x, cursorPos.y + 2));
    ImGui::TextUnformatted(entry.message.c_str());

    // Duplicate count badge
    if (entry.duplicateCount > 1) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.7f));
        ImGui::Text("(x%u)", entry.duplicateCount);
        ImGui::PopStyleColor();
    }

    // Source location
    if (m_showSourceLocations && entry.HasSourceLocation()) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Text("(%s)", entry.GetSourceLocation().c_str());
        ImGui::PopStyleColor();
    }

    ImGui::PopStyleColor();

    // Advance cursor for next row
    ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + m_rowHeight));
    ImGui::Dummy(ImVec2(0, 0));

    ImGui::PopID();
#endif
}

void ConsolePanel::RenderContextMenu() {
#ifdef NOVA_USE_IMGUI
    if (ImGui::BeginPopup("ConsoleContextMenu")) {
        if (ImGui::MenuItem("Copy", "Ctrl+C")) {
            CopySelectedToClipboard();
        }
        if (ImGui::MenuItem("Copy All")) {
            CopyAllToClipboard();
        }
        ImGui::Separator();

        // Get entry for context
        if (m_contextMenuEntryIndex < m_filteredIndices.size()) {
            std::lock_guard<std::mutex> lock(m_entriesMutex);
            size_t entryIndex = m_filteredIndices[m_contextMenuEntryIndex];
            if (entryIndex < m_entries.size()) {
                const auto& entry = m_entries[entryIndex];
                if (entry.HasSourceLocation()) {
                    if (ImGui::MenuItem("Open Source File")) {
                        if (Callbacks.onOpenSourceFile) {
                            Callbacks.onOpenSourceFile(entry.sourceFile, entry.sourceLine);
                        }
                    }
                }
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Clear Console")) {
            Clear();
        }
        if (ImGui::MenuItem("Save Log...")) {
            // Would open file dialog
        }

        ImGui::EndPopup();
    } else {
        m_showContextMenu = false;
    }

    // Open the popup if we just requested it
    if (m_showContextMenu) {
        ImGui::OpenPopup("ConsoleContextMenu");
    }
#endif
}

void ConsolePanel::RenderCommandInput() {
#ifdef NOVA_USE_IMGUI
    ImGui::PushItemWidth(-1);

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_CallbackHistory |
                                ImGuiInputTextFlags_CallbackCompletion;

    // Custom callback for history and auto-complete
    struct CallbackData {
        ConsolePanel* panel;
    };
    CallbackData cbData{this};

    auto callback = [](ImGuiInputTextCallbackData* data) -> int {
        auto* panel = static_cast<CallbackData*>(data->UserData)->panel;

        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            panel->HandleCommandHistory(data->EventKey == ImGuiKey_UpArrow);

            if (!panel->m_commandHistory.empty() && panel->m_commandHistoryIndex >= 0) {
                const std::string& histCmd = panel->m_commandHistory[panel->m_commandHistoryIndex];
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, histCmd.c_str());
            }
        } else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
            // Auto-complete
            std::string partial(data->Buf, data->CursorPos);
            auto suggestions = panel->GetAutoComplete(partial);
            if (!suggestions.empty()) {
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, suggestions[0].text.c_str());
            }
        }

        return 0;
    };

    if (ImGui::InputText("##CommandInput", m_commandBuffer, sizeof(m_commandBuffer),
                         flags, callback, &cbData)) {
        if (m_commandBuffer[0] != '\0') {
            ExecuteCommand(m_commandBuffer);
            m_commandBuffer[0] = '\0';
        }
    }

    // Set focus to command input if requested
    if (m_commandInputFocused) {
        ImGui::SetKeyboardFocusHere(-1);
        m_commandInputFocused = false;
    }

    ImGui::PopItemWidth();

    // Show auto-complete suggestions
    if (ImGui::IsItemActive() && m_commandBuffer[0] != '\0') {
        m_autoCompleteSuggestions = GetAutoComplete(m_commandBuffer);
        if (!m_autoCompleteSuggestions.empty()) {
            RenderAutoComplete();
        }
    }
#endif
}

void ConsolePanel::RenderAutoComplete() {
#ifdef NOVA_USE_IMGUI
    ImVec2 inputPos = ImGui::GetItemRectMin();
    ImVec2 inputSize = ImGui::GetItemRectSize();

    ImGui::SetNextWindowPos(ImVec2(inputPos.x, inputPos.y - 150), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(inputSize.x, 150), ImGuiCond_Always);

    if (ImGui::Begin("##AutoComplete", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing)) {
        for (size_t i = 0; i < m_autoCompleteSuggestions.size() && i < 10; ++i) {
            const auto& suggestion = m_autoCompleteSuggestions[i];
            bool isSelected = (static_cast<int>(i) == m_autoCompleteSelectedIndex);

            if (ImGui::Selectable(suggestion.text.c_str(), isSelected)) {
                std::strncpy(m_commandBuffer, suggestion.text.c_str(),
                            sizeof(m_commandBuffer) - 1);
            }

            if (!suggestion.description.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("- %s", suggestion.description.c_str());
            }
        }
    }
    ImGui::End();
#endif
}

void ConsolePanel::RenderErrorNotification() {
#ifdef NOVA_USE_IMGUI
    ImVec2 windowSize = ImGui::GetIO().DisplaySize;
    float notifWidth = 400;
    float notifHeight = 60;

    ImGui::SetNextWindowPos(ImVec2(windowSize.x - notifWidth - 20,
                                   windowSize.y - notifHeight - 20),
                           ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(notifWidth, notifHeight), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.8f, 0.2f, 0.2f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));

    if (ImGui::Begin("##ErrorNotification", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
        ImGui::TextUnformatted("[ERROR]");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", m_lastError.message.c_str());

        if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered()) {
            m_showErrorNotification = false;
            Focus();  // Focus the console panel
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
#endif
}

void ConsolePanel::OnRenderToolbar() {
#ifdef NOVA_USE_IMGUI
    if (ImGui::Button("Clear")) {
        Clear();
    }
    ImGui::SameLine();

    if (ImGui::Button("Copy All")) {
        CopyAllToClipboard();
    }
    ImGui::SameLine();

    if (ImGui::Button("Scroll to Bottom")) {
        ScrollToBottom();
    }

    // Unread error indicator
    if (m_unreadErrorCount > 0) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::Text("(%u unread errors)", m_unreadErrorCount.load());
        ImGui::PopStyleColor();

        if (ImGui::IsItemClicked()) {
            MarkErrorsAsRead();
        }
    }
#endif
}

void ConsolePanel::OnRenderStatusBar() {
#ifdef NOVA_USE_IMGUI
    // Entry counts
    ImGui::Text("Total: %zu | Visible: %zu",
                m_entries.size(), m_filteredIndices.size());

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Level breakdown
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
    ImGui::Text("W: %u", m_stats.warningCount.load());
    ImGui::PopStyleColor();

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    ImGui::Text("E: %u", m_stats.errorCount.load() + m_stats.fatalCount.load());
    ImGui::PopStyleColor();

    // Auto-scroll indicator
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    if (m_autoScrollEnabled && !m_userScrolledUp) {
        ImGui::TextDisabled("Auto-scroll ON");
    } else {
        ImGui::TextDisabled("Auto-scroll OFF");
    }
#endif
}

void ConsolePanel::OnSearchChanged(const std::string& filter) {
    std::strncpy(m_textFilterBuffer, filter.c_str(), sizeof(m_textFilterBuffer) - 1);
    SetTextFilter(filter, m_useRegexFilter);
}

// =============================================================================
// Interaction
// =============================================================================

void ConsolePanel::HandleEntryClick(const ConsoleLogEntry& entry, size_t index,
                                    bool ctrlHeld, bool shiftHeld) {
    if (ctrlHeld) {
        // Toggle selection
        if (m_selectedEntryIds.count(entry.id)) {
            m_selectedEntryIds.erase(entry.id);
        } else {
            m_selectedEntryIds.insert(entry.id);
            m_lastSelectedId = entry.id;
        }
    } else if (shiftHeld && m_lastSelectedId != 0) {
        // Range selection
        std::lock_guard<std::mutex> lock(m_entriesMutex);

        // Find indices of last selected and current
        size_t lastIndex = SIZE_MAX;
        size_t currentIndex = SIZE_MAX;
        for (size_t i = 0; i < m_filteredIndices.size(); ++i) {
            size_t entryIdx = m_filteredIndices[i];
            if (entryIdx < m_entries.size()) {
                if (m_entries[entryIdx].id == m_lastSelectedId) lastIndex = i;
                if (m_entries[entryIdx].id == entry.id) currentIndex = i;
            }
        }

        if (lastIndex != SIZE_MAX && currentIndex != SIZE_MAX) {
            size_t start = std::min(lastIndex, currentIndex);
            size_t end = std::max(lastIndex, currentIndex);
            for (size_t i = start; i <= end; ++i) {
                if (m_filteredIndices[i] < m_entries.size()) {
                    m_selectedEntryIds.insert(m_entries[m_filteredIndices[i]].id);
                }
            }
        }
    } else {
        // Single selection
        m_selectedEntryIds.clear();
        m_selectedEntryIds.insert(entry.id);
        m_lastSelectedId = entry.id;
    }
}

void ConsolePanel::HandleEntryDoubleClick(const ConsoleLogEntry& entry) {
    if (entry.HasSourceLocation() && Callbacks.onOpenSourceFile) {
        Callbacks.onOpenSourceFile(entry.sourceFile, entry.sourceLine);
    }
}

void ConsolePanel::HandleCommandHistory(bool up) {
    if (m_commandHistory.empty()) return;

    if (up) {
        if (m_commandHistoryIndex < 0) {
            m_commandHistoryIndex = static_cast<int>(m_commandHistory.size()) - 1;
        } else if (m_commandHistoryIndex > 0) {
            --m_commandHistoryIndex;
        }
    } else {
        if (m_commandHistoryIndex >= 0 &&
            m_commandHistoryIndex < static_cast<int>(m_commandHistory.size()) - 1) {
            ++m_commandHistoryIndex;
        } else {
            m_commandHistoryIndex = -1;
        }
    }
}

void ConsolePanel::HandleKeyboardInput() {
    // Keyboard shortcuts would be handled here
    // Ctrl+C for copy, Ctrl+A for select all, etc.
}

void ConsolePanel::HandleAutoCompleteSelection() {
    // Handle Tab/Enter for auto-complete selection
}

} // namespace Nova
