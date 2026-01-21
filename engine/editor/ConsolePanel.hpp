/**
 * @file ConsolePanel.hpp
 * @brief Console/Log Panel for the Nova3D/Vehement editor
 *
 * Provides a comprehensive logging console with:
 * - Real-time log display with virtual scrolling
 * - Log level filtering (Trace, Debug, Info, Warning, Error, Fatal)
 * - Category/channel filtering
 * - Text and regex search
 * - Duplicate message collapsing
 * - Click to copy, double-click to open source
 * - Command input with history and auto-complete
 * - Log export functionality
 * - Thread-safe async log collection
 *
 * @author Nova Engine Team
 * @version 1.0.0
 */

#pragma once

#include "../core/Logger.hpp"
#include "../ui/EditorPanel.hpp"
#include "../ui/EditorWidgets.hpp"
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <regex>
#include <optional>

namespace Nova {

// Forward declarations
class LogManager;

// =============================================================================
// Console Log Level (mirrors Logger but for panel filtering)
// =============================================================================

/**
 * @brief Log severity levels for console filtering
 */
enum class ConsoleLogLevel : uint8_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Fatal = 5
};

/**
 * @brief Convert ConsoleLogLevel to display string
 */
[[nodiscard]] constexpr std::string_view ConsoleLogLevelToString(ConsoleLogLevel level) noexcept {
    switch (level) {
        case ConsoleLogLevel::Trace:   return "Trace";
        case ConsoleLogLevel::Debug:   return "Debug";
        case ConsoleLogLevel::Info:    return "Info";
        case ConsoleLogLevel::Warning: return "Warning";
        case ConsoleLogLevel::Error:   return "Error";
        case ConsoleLogLevel::Fatal:   return "Fatal";
        default:                       return "Unknown";
    }
}

/**
 * @brief Convert Nova LogLevel to ConsoleLogLevel
 */
[[nodiscard]] constexpr ConsoleLogLevel LogLevelToConsoleLevel(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return ConsoleLogLevel::Trace;
        case LogLevel::Debug: return ConsoleLogLevel::Debug;
        case LogLevel::Info:  return ConsoleLogLevel::Info;
        case LogLevel::Warn:  return ConsoleLogLevel::Warning;
        case LogLevel::Error: return ConsoleLogLevel::Error;
        case LogLevel::Fatal: return ConsoleLogLevel::Fatal;
        default:              return ConsoleLogLevel::Info;
    }
}

// =============================================================================
// Console Log Entry
// =============================================================================

/**
 * @brief Complete log entry for console display
 */
struct ConsoleLogEntry {
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

    uint64_t id = 0;                            ///< Unique entry ID
    TimePoint timestamp;                        ///< When the log was created
    ConsoleLogLevel level = ConsoleLogLevel::Info;  ///< Severity level
    std::string category;                       ///< Logger category (e.g., "Graphics", "Physics")
    std::string message;                        ///< The log message
    std::string sourceFile;                     ///< Source file (optional)
    uint32_t sourceLine = 0;                    ///< Source line number (optional)
    std::string functionName;                   ///< Function name (optional)
    std::string stackTrace;                     ///< Stack trace for errors (optional)
    std::thread::id threadId;                   ///< Thread that created the log

    // Display state
    uint32_t duplicateCount = 1;                ///< Count of collapsed duplicates
    bool isCollapsed = false;                   ///< Is this entry representing collapsed duplicates
    bool isSelected = false;                    ///< Is this entry selected
    bool matchesFilter = true;                  ///< Does this entry match current filters

    /**
     * @brief Get formatted timestamp string
     */
    [[nodiscard]] std::string GetFormattedTimestamp() const;

    /**
     * @brief Get short source location string (file:line)
     */
    [[nodiscard]] std::string GetSourceLocation() const;

    /**
     * @brief Check if source location is valid
     */
    [[nodiscard]] bool HasSourceLocation() const {
        return !sourceFile.empty() && sourceLine > 0;
    }

    /**
     * @brief Create entry from Nova LogEntry
     */
    static ConsoleLogEntry FromLogEntry(const LogEntry& entry, uint64_t entryId);
};

// =============================================================================
// Console Command
// =============================================================================

/**
 * @brief Console command registration
 */
struct ConsoleCommand {
    std::string name;                           ///< Command name (e.g., "clear", "help")
    std::string description;                    ///< Brief description
    std::string usage;                          ///< Usage string (e.g., "clear [all]")
    std::vector<std::string> aliases;           ///< Alternative names
    std::function<void(const std::vector<std::string>&)> handler;  ///< Command handler
};

/**
 * @brief Auto-complete suggestion
 */
struct AutoCompleteSuggestion {
    std::string text;                           ///< Suggestion text
    std::string description;                    ///< Optional description
    int relevance = 0;                          ///< Relevance score for sorting
};

// =============================================================================
// Console Configuration
// =============================================================================

/**
 * @brief Console panel configuration
 */
struct ConsolePanelConfig {
    size_t maxEntries = 10000;                  ///< Maximum log entries (ring buffer)
    size_t commandHistorySize = 100;            ///< Maximum command history entries
    bool collapseDuplicates = true;             ///< Collapse consecutive duplicate messages
    bool showTimestamps = true;                 ///< Show timestamps by default
    bool showCategories = true;                 ///< Show categories by default
    bool showSourceLocations = false;           ///< Show source locations by default
    bool autoScroll = true;                     ///< Auto-scroll to bottom on new messages
    float errorNotificationDuration = 5.0f;     ///< Duration for error popups (seconds)
    bool showErrorNotifications = true;         ///< Show popup for errors/fatal
};

// =============================================================================
// Console Statistics
// =============================================================================

/**
 * @brief Console statistics for status bar
 */
struct ConsoleStats {
    std::atomic<uint32_t> traceCount{0};
    std::atomic<uint32_t> debugCount{0};
    std::atomic<uint32_t> infoCount{0};
    std::atomic<uint32_t> warningCount{0};
    std::atomic<uint32_t> errorCount{0};
    std::atomic<uint32_t> fatalCount{0};
    std::atomic<uint32_t> totalCount{0};

    void Reset() {
        traceCount = 0;
        debugCount = 0;
        infoCount = 0;
        warningCount = 0;
        errorCount = 0;
        fatalCount = 0;
        totalCount = 0;
    }

    void Increment(ConsoleLogLevel level) {
        ++totalCount;
        switch (level) {
            case ConsoleLogLevel::Trace:   ++traceCount; break;
            case ConsoleLogLevel::Debug:   ++debugCount; break;
            case ConsoleLogLevel::Info:    ++infoCount; break;
            case ConsoleLogLevel::Warning: ++warningCount; break;
            case ConsoleLogLevel::Error:   ++errorCount; break;
            case ConsoleLogLevel::Fatal:   ++fatalCount; break;
        }
    }
};

// =============================================================================
// Console Panel Callbacks
// =============================================================================

/**
 * @brief Callback signatures for console events
 */
struct ConsolePanelCallbacks {
    /// Called when user wants to open a source file
    std::function<void(const std::string& file, uint32_t line)> onOpenSourceFile;

    /// Called when a command is executed
    std::function<void(const std::string& command)> onCommandExecuted;

    /// Called when an error occurs (for external notification)
    std::function<void(const ConsoleLogEntry& entry)> onErrorOccurred;

    /// Called when console is cleared
    std::function<void()> onCleared;
};

// =============================================================================
// Console Panel
// =============================================================================

/**
 * @brief Console/Log Panel for the Nova3D/Vehement editor
 *
 * Features:
 * - Real-time log display with virtual scrolling for performance
 * - Log level filtering (toggle buttons per level)
 * - Category filter dropdown
 * - Text search with regex option
 * - Duplicate message collapsing with count badge
 * - Color coding per log level
 * - Click to copy, double-click to open source file
 * - Right-click context menu
 * - Command input with history (up/down arrows)
 * - Auto-complete suggestions
 * - Export to file or clipboard
 * - Thread-safe async log collection
 * - Error badge on panel tab
 * - Notification popup for critical errors
 * - Status bar summary
 *
 * Usage:
 *   ConsolePanel console;
 *   console.Initialize({.title = "Console"});
 *   console.HookIntoLogger();  // Connect to Nova Logger
 *
 *   // Register custom commands
 *   console.RegisterCommand({
 *       .name = "mycommand",
 *       .handler = [](const auto& args) { ... }
 *   });
 *
 *   // In render loop:
 *   console.Render();
 */
class ConsolePanel : public EditorPanel {
public:
    ConsolePanel();
    ~ConsolePanel() override;

    // Non-copyable
    ConsolePanel(const ConsolePanel&) = delete;
    ConsolePanel& operator=(const ConsolePanel&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize with configuration
     */
    bool Initialize(const Config& config, const ConsolePanelConfig& consoleConfig = {});

    /**
     * @brief Hook into the Nova Logger system
     */
    void HookIntoLogger();

    /**
     * @brief Unhook from the Nova Logger system
     */
    void UnhookFromLogger();

    // =========================================================================
    // Message Management
    // =========================================================================

    /**
     * @brief Add a log message
     */
    void AddMessage(ConsoleLogLevel level, const std::string& message,
                   const std::string& category = "General");

    /**
     * @brief Add a log message with source location
     */
    void AddMessage(ConsoleLogLevel level, const std::string& message,
                   const std::string& category, const std::string& sourceFile,
                   uint32_t sourceLine, const std::string& functionName = "");

    /**
     * @brief Add a log message with full entry data
     */
    void AddEntry(ConsoleLogEntry entry);

    /**
     * @brief Clear all log entries
     */
    void Clear();

    /**
     * @brief Get total entry count (including filtered)
     */
    [[nodiscard]] size_t GetEntryCount() const;

    /**
     * @brief Get visible entry count (after filtering)
     */
    [[nodiscard]] size_t GetVisibleEntryCount() const;

    /**
     * @brief Get console statistics
     */
    [[nodiscard]] const ConsoleStats& GetStats() const { return m_stats; }

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set maximum number of log entries
     */
    void SetMaxEntries(size_t max);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const ConsolePanelConfig& GetConsoleConfig() const { return m_consoleConfig; }

    /**
     * @brief Update configuration
     */
    void SetConsoleConfig(const ConsolePanelConfig& config);

    // =========================================================================
    // Filtering
    // =========================================================================

    /**
     * @brief Set level filter (show/hide specific level)
     */
    void SetLevelFilter(ConsoleLogLevel level, bool show);

    /**
     * @brief Get level filter state
     */
    [[nodiscard]] bool GetLevelFilter(ConsoleLogLevel level) const;

    /**
     * @brief Set category filter (empty string = show all)
     */
    void SetCategoryFilter(const std::string& category);

    /**
     * @brief Get current category filter
     */
    [[nodiscard]] const std::string& GetCategoryFilter() const { return m_categoryFilter; }

    /**
     * @brief Get all known categories
     */
    [[nodiscard]] std::vector<std::string> GetKnownCategories() const;

    /**
     * @brief Set text search filter
     */
    void SetTextFilter(const std::string& text, bool useRegex = false);

    /**
     * @brief Clear all filters
     */
    void ClearFilters();

    // =========================================================================
    // Display Options
    // =========================================================================

    /**
     * @brief Show/hide timestamps
     */
    void SetShowTimestamps(bool show);

    /**
     * @brief Show/hide categories
     */
    void SetShowCategories(bool show);

    /**
     * @brief Show/hide source locations
     */
    void SetShowSourceLocations(bool show);

    /**
     * @brief Enable/disable duplicate collapsing
     */
    void SetCollapseDuplicates(bool collapse);

    /**
     * @brief Enable/disable auto-scroll
     */
    void SetAutoScroll(bool autoScroll);

    /**
     * @brief Scroll to bottom
     */
    void ScrollToBottom();

    /**
     * @brief Scroll to top
     */
    void ScrollToTop();

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * @brief Select an entry
     */
    void SelectEntry(size_t index, bool addToSelection = false);

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Get selected entries
     */
    [[nodiscard]] std::vector<const ConsoleLogEntry*> GetSelectedEntries() const;

    /**
     * @brief Copy selected entries to clipboard
     */
    void CopySelectedToClipboard();

    /**
     * @brief Copy all visible entries to clipboard
     */
    void CopyAllToClipboard();

    // =========================================================================
    // Commands
    // =========================================================================

    /**
     * @brief Register a console command
     */
    void RegisterCommand(const ConsoleCommand& command);

    /**
     * @brief Unregister a console command
     */
    void UnregisterCommand(const std::string& name);

    /**
     * @brief Execute a command string
     */
    void ExecuteCommand(const std::string& commandLine);

    /**
     * @brief Get auto-complete suggestions
     */
    [[nodiscard]] std::vector<AutoCompleteSuggestion> GetAutoComplete(const std::string& partial) const;

    // =========================================================================
    // Export
    // =========================================================================

    /**
     * @brief Save log to file
     */
    bool SaveToFile(const std::string& filePath, bool filteredOnly = false) const;

    /**
     * @brief Export as JSON
     */
    [[nodiscard]] std::string ExportAsJson(bool filteredOnly = false) const;

    /**
     * @brief Export as plain text
     */
    [[nodiscard]] std::string ExportAsText(bool filteredOnly = false) const;

    // =========================================================================
    // Notifications
    // =========================================================================

    /**
     * @brief Check if there are unread errors
     */
    [[nodiscard]] bool HasUnreadErrors() const { return m_unreadErrorCount > 0; }

    /**
     * @brief Get unread error count
     */
    [[nodiscard]] uint32_t GetUnreadErrorCount() const { return m_unreadErrorCount; }

    /**
     * @brief Mark all errors as read
     */
    void MarkErrorsAsRead() { m_unreadErrorCount = 0; }

    // =========================================================================
    // AI Diagnostics
    // =========================================================================

    /**
     * @brief Analyze recent errors using AI and generate fix suggestions
     *
     * Scans recent error messages in the console and provides:
     * - Root cause analysis
     * - Specific fix suggestions
     * - Related error patterns
     * - Performance improvement recommendations
     */
    void AnalyzeErrorsWithAI();

    /**
     * @brief Show AI diagnostics panel with suggestions
     *
     * Displays a dedicated panel showing:
     * - Error analysis results
     * - AI-generated suggestions
     * - Pattern detection in error logs
     * - Performance bottleneck recommendations
     */
    void ShowAIDiagnosticsPanel();

    /**
     * @brief Check if AI diagnostics panel is visible
     */
    [[nodiscard]] bool IsAIDiagnosticsPanelVisible() const { return m_showAIDiagnostics; }

    /**
     * @brief Get AI-generated suggestions
     */
    [[nodiscard]] const std::vector<std::string>& GetAISuggestions() const { return m_aiSuggestions; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    ConsolePanelCallbacks Callbacks;

protected:
    // =========================================================================
    // EditorPanel Overrides
    // =========================================================================

    void OnRender() override;
    void OnRenderToolbar() override;
    void OnRenderStatusBar() override;
    void OnSearchChanged(const std::string& filter) override;
    void OnInitialize() override;
    void OnShutdown() override;
    void Update(float deltaTime) override;

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    // Rendering
    void RenderFilterBar();
    void RenderLogEntries();
    void RenderLogEntry(const ConsoleLogEntry& entry, size_t visualIndex, float yOffset);
    void RenderEntryText(const ConsoleLogEntry& entry, float x, float y, float maxWidth);
    void RenderContextMenu();
    void RenderCommandInput();
    void RenderAutoComplete();
    void RenderErrorNotification();

    // Entry management
    void ProcessPendingEntries();
    void AddEntryInternal(ConsoleLogEntry entry);
    void CollapseDuplicatesIfNeeded();
    void UpdateFilteredEntries();
    bool EntryMatchesFilters(const ConsoleLogEntry& entry) const;
    void TrimToMaxEntries();

    // Color helpers
    [[nodiscard]] glm::vec4 GetLevelColor(ConsoleLogLevel level) const;
    [[nodiscard]] const char* GetLevelIcon(ConsoleLogLevel level) const;

    // Interaction
    void HandleEntryClick(const ConsoleLogEntry& entry, size_t index, bool ctrlHeld, bool shiftHeld);
    void HandleEntryDoubleClick(const ConsoleLogEntry& entry);
    void HandleKeyboardInput();
    void HandleCommandHistory(bool up);
    void HandleAutoCompleteSelection();

    // Commands
    void RegisterBuiltInCommands();
    std::vector<std::string> ParseCommandLine(const std::string& commandLine) const;

    // Clipboard
    [[nodiscard]] std::string FormatEntryForClipboard(const ConsoleLogEntry& entry) const;
    void SetClipboardText(const std::string& text);

    // =========================================================================
    // Member Variables
    // =========================================================================

    // Configuration
    ConsolePanelConfig m_consoleConfig;

    // Log entries (ring buffer)
    std::deque<ConsoleLogEntry> m_entries;
    std::vector<size_t> m_filteredIndices;  // Indices of entries matching current filters
    uint64_t m_nextEntryId = 1;
    mutable std::mutex m_entriesMutex;

    // Pending entries from other threads
    std::vector<ConsoleLogEntry> m_pendingEntries;
    mutable std::mutex m_pendingMutex;

    // Statistics
    ConsoleStats m_stats;

    // Level filters (true = show)
    bool m_levelFilters[6] = {true, true, true, true, true, true};

    // Category filtering
    std::string m_categoryFilter;
    std::unordered_set<std::string> m_knownCategories;
    mutable std::mutex m_categoriesMutex;

    // Text filtering
    std::string m_textFilter;
    char m_textFilterBuffer[256] = "";
    bool m_useRegexFilter = false;
    std::optional<std::regex> m_filterRegex;

    // Selection
    std::unordered_set<uint64_t> m_selectedEntryIds;
    uint64_t m_lastSelectedId = 0;

    // Scrolling
    bool m_autoScrollEnabled = true;
    bool m_scrollToBottomRequested = false;
    bool m_userScrolledUp = false;
    float m_scrollY = 0.0f;

    // Virtual scrolling
    float m_rowHeight = 20.0f;
    size_t m_visibleStartIndex = 0;
    size_t m_visibleEndIndex = 0;

    // Display options
    bool m_showTimestamps = true;
    bool m_showCategories = true;
    bool m_showSourceLocations = false;
    bool m_collapseDuplicates = true;

    // Command input
    char m_commandBuffer[1024] = "";
    std::vector<std::string> m_commandHistory;
    int m_commandHistoryIndex = -1;
    bool m_commandInputFocused = false;

    // Auto-complete
    std::vector<AutoCompleteSuggestion> m_autoCompleteSuggestions;
    int m_autoCompleteSelectedIndex = -1;
    bool m_showAutoComplete = false;

    // Commands
    std::unordered_map<std::string, ConsoleCommand> m_commands;
    std::unordered_map<std::string, std::string> m_commandAliases;

    // Context menu
    bool m_showContextMenu = false;
    size_t m_contextMenuEntryIndex = 0;

    // Notifications
    std::atomic<uint32_t> m_unreadErrorCount{0};
    ConsoleLogEntry m_lastError;
    float m_errorNotificationTimer = 0.0f;
    bool m_showErrorNotification = false;

    // Logger hook
    std::shared_ptr<ILogSink> m_logSink;
    bool m_isHookedToLogger = false;

    // Filter state dirty flag
    bool m_filtersDirty = true;

    // AI Diagnostics
    bool m_showAIDiagnostics = false;              ///< Toggle for AI diagnostics panel visibility
    std::vector<std::string> m_aiSuggestions;      ///< AI-generated fix suggestions and analysis
};

// =============================================================================
// Console Log Sink
// =============================================================================

/**
 * @brief Log sink that forwards entries to ConsolePanel
 */
class ConsolePanelLogSink : public LogSinkBase {
public:
    explicit ConsolePanelLogSink(ConsolePanel* panel);

    void Write(const LogEntry& entry) override;
    void Flush() override;

private:
    ConsolePanel* m_panel;
};

} // namespace Nova
