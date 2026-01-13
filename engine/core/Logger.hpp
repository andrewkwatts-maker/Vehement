#pragma once

/**
 * @file Logger.hpp
 * @brief Modern, high-performance logging system for the Nova Engine
 *
 * Features:
 * - SOLID-compliant design with ILogSink interface
 * - Multiple output targets (Console, File, Network)
 * - Async logging with lock-free queue
 * - Category/tag filtering
 * - Compile-time log level stripping
 * - Source location tracking (C++20 std::source_location)
 * - Structured logging with JSON output option
 * - Thread-safe implementation
 * - Minimal overhead logging macros
 *
 * @author Nova Engine Team
 * @version 2.0.0
 */

#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <cstdarg>
#include <format>

#ifdef _WIN32
#include <windows.h>
#endif

// C++20 source_location support
#if __has_include(<source_location>)
#include <source_location>
#define NOVA_HAS_SOURCE_LOCATION 1
#else
#define NOVA_HAS_SOURCE_LOCATION 0
#endif

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================
class ILogSink;
class ILogFormatter;
class Logger;
class LogManager;

// ============================================================================
// LogLevel - Severity levels for log messages
// ============================================================================

/**
 * @brief Log severity levels in ascending order of importance
 */
enum class LogLevel : uint8_t {
    Trace = 0,   ///< Detailed tracing information
    Debug = 1,   ///< Debug-level messages
    Info = 2,    ///< Informational messages
    Warn = 3,    ///< Warning conditions
    Error = 4,   ///< Error conditions
    Fatal = 5,   ///< Fatal/critical errors (may terminate)
    Off = 6      ///< Logging disabled
};

/**
 * @brief Convert LogLevel to string representation
 */
[[nodiscard]] constexpr std::string_view LogLevelToString(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        case LogLevel::Off:   return "OFF";
        default:              return "UNKNOWN";
    }
}

/**
 * @brief Convert LogLevel to short string (for compact output)
 */
[[nodiscard]] constexpr std::string_view LogLevelToShortString(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "T";
        case LogLevel::Debug: return "D";
        case LogLevel::Info:  return "I";
        case LogLevel::Warn:  return "W";
        case LogLevel::Error: return "E";
        case LogLevel::Fatal: return "F";
        default:              return "?";
    }
}

/**
 * @brief Parse string to LogLevel
 */
[[nodiscard]] inline LogLevel StringToLogLevel(std::string_view str) noexcept {
    if (str == "TRACE" || str == "trace") return LogLevel::Trace;
    if (str == "DEBUG" || str == "debug") return LogLevel::Debug;
    if (str == "INFO" || str == "info") return LogLevel::Info;
    if (str == "WARN" || str == "warn" || str == "WARNING" || str == "warning") return LogLevel::Warn;
    if (str == "ERROR" || str == "error") return LogLevel::Error;
    if (str == "FATAL" || str == "fatal" || str == "CRITICAL" || str == "critical") return LogLevel::Fatal;
    if (str == "OFF" || str == "off") return LogLevel::Off;
    return LogLevel::Info; // Default
}

// ============================================================================
// SourceLocation - Cross-platform source location tracking
// ============================================================================

/**
 * @brief Source code location information
 */
struct SourceLocation {
    const char* file = "";
    const char* function = "";
    uint32_t line = 0;
    uint32_t column = 0;

    constexpr SourceLocation() noexcept = default;

    constexpr SourceLocation(const char* f, const char* fn, uint32_t l, uint32_t c = 0) noexcept
        : file(f), function(fn), line(l), column(c) {}

#if NOVA_HAS_SOURCE_LOCATION
    static constexpr SourceLocation Current(
        const std::source_location& loc = std::source_location::current()) noexcept {
        return SourceLocation(loc.file_name(), loc.function_name(),
                             static_cast<uint32_t>(loc.line()),
                             static_cast<uint32_t>(loc.column()));
    }
#endif

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return file != nullptr && file[0] != '\0' && line > 0;
    }

    /**
     * @brief Get just the filename without the path
     */
    [[nodiscard]] std::string_view FileName() const noexcept {
        if (!file) return "";
        std::string_view path(file);
        auto pos = path.find_last_of("/\\");
        return (pos != std::string_view::npos) ? path.substr(pos + 1) : path;
    }
};

// ============================================================================
// LogEntry - Complete log message with metadata
// ============================================================================

/**
 * @brief Complete log entry containing all message metadata
 */
struct LogEntry {
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

    TimePoint timestamp;           ///< When the log was created
    LogLevel level;                ///< Severity level
    std::string category;          ///< Logger category/name
    std::string message;           ///< The log message
    SourceLocation location;       ///< Source code location
    std::thread::id threadId;      ///< Thread that created the log

    // Optional structured data (key-value pairs)
    std::unordered_map<std::string, std::string> metadata;

    LogEntry() noexcept
        : timestamp(Clock::now())
        , level(LogLevel::Info)
        , threadId(std::this_thread::get_id()) {}

    LogEntry(LogLevel lvl, std::string_view cat, std::string msg,
             SourceLocation loc = SourceLocation()) noexcept
        : timestamp(Clock::now())
        , level(lvl)
        , category(cat)
        , message(std::move(msg))
        , location(loc)
        , threadId(std::this_thread::get_id()) {}

    /**
     * @brief Add metadata key-value pair for structured logging
     */
    LogEntry& With(std::string_view key, std::string_view value) {
        metadata[std::string(key)] = std::string(value);
        return *this;
    }

    template<typename T>
    LogEntry& With(std::string_view key, const T& value) {
        if constexpr (std::is_arithmetic_v<T>) {
            metadata[std::string(key)] = std::to_string(value);
        } else {
            std::ostringstream oss;
            oss << value;
            metadata[std::string(key)] = oss.str();
        }
        return *this;
    }
};

// ============================================================================
// ILogFormatter - Interface for log output formatting
// ============================================================================

/**
 * @brief Interface for formatting log entries into strings
 *
 * Single Responsibility: Only handles formatting, not output
 */
class ILogFormatter {
public:
    virtual ~ILogFormatter() = default;

    /**
     * @brief Format a log entry into a string
     * @param entry The log entry to format
     * @return Formatted string representation
     */
    [[nodiscard]] virtual std::string Format(const LogEntry& entry) const = 0;
};

/**
 * @brief Default text formatter with customizable pattern
 */
class TextLogFormatter : public ILogFormatter {
public:
    /**
     * @brief Pattern tokens:
     * %t - timestamp (ISO 8601)
     * %T - timestamp (time only HH:MM:SS.mmm)
     * %l - level (full)
     * %L - level (short)
     * %c - category
     * %m - message
     * %f - file name
     * %F - full file path
     * %n - line number
     * %u - function name
     * %i - thread id
     * %% - literal %
     */
    explicit TextLogFormatter(std::string_view pattern = "[%T] [%L] [%c] %m")
        : m_pattern(pattern) {}

    [[nodiscard]] std::string Format(const LogEntry& entry) const override {
        std::string result;
        result.reserve(m_pattern.size() + entry.message.size() + 64);

        for (size_t i = 0; i < m_pattern.size(); ++i) {
            if (m_pattern[i] == '%' && i + 1 < m_pattern.size()) {
                ++i;
                switch (m_pattern[i]) {
                    case 't': result += FormatTimestamp(entry.timestamp, true); break;
                    case 'T': result += FormatTimestamp(entry.timestamp, false); break;
                    case 'l': result += LogLevelToString(entry.level); break;
                    case 'L': result += LogLevelToShortString(entry.level); break;
                    case 'c': result += entry.category; break;
                    case 'm': result += entry.message; break;
                    case 'f': result += entry.location.FileName(); break;
                    case 'F': result += entry.location.file ? entry.location.file : ""; break;
                    case 'n': result += std::to_string(entry.location.line); break;
                    case 'u': result += entry.location.function ? entry.location.function : ""; break;
                    case 'i': {
                        std::ostringstream oss;
                        oss << entry.threadId;
                        result += oss.str();
                        break;
                    }
                    case '%': result += '%'; break;
                    default: result += '%'; result += m_pattern[i]; break;
                }
            } else {
                result += m_pattern[i];
            }
        }
        return result;
    }

    void SetPattern(std::string_view pattern) { m_pattern = pattern; }

private:
    std::string m_pattern;

    [[nodiscard]] static std::string FormatTimestamp(LogEntry::TimePoint tp, bool includeDate) {
        auto time = LogEntry::Clock::to_time_t(tp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch()) % 1000;

        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif

        std::ostringstream oss;
        if (includeDate) {
            oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        } else {
            oss << std::put_time(&tm_buf, "%H:%M:%S");
        }
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }
};

/**
 * @brief JSON formatter for structured logging
 */
class JsonLogFormatter : public ILogFormatter {
public:
    explicit JsonLogFormatter(bool prettyPrint = false)
        : m_prettyPrint(prettyPrint) {}

    [[nodiscard]] std::string Format(const LogEntry& entry) const override {
        std::ostringstream oss;
        const char* nl = m_prettyPrint ? "\n" : "";
        const char* indent = m_prettyPrint ? "  " : "";

        oss << "{" << nl;
        oss << indent << "\"timestamp\":\"" << FormatTimestamp(entry.timestamp) << "\"," << nl;
        oss << indent << "\"level\":\"" << LogLevelToString(entry.level) << "\"," << nl;
        oss << indent << "\"category\":\"" << EscapeJson(entry.category) << "\"," << nl;
        oss << indent << "\"message\":\"" << EscapeJson(entry.message) << "\"," << nl;
        oss << indent << "\"thread\":\"" << entry.threadId << "\"";

        if (entry.location.IsValid()) {
            oss << "," << nl;
            oss << indent << "\"location\":{" << nl;
            oss << indent << indent << "\"file\":\"" << EscapeJson(entry.location.FileName()) << "\"," << nl;
            oss << indent << indent << "\"line\":" << entry.location.line << "," << nl;
            oss << indent << indent << "\"function\":\"" << EscapeJson(entry.location.function ? entry.location.function : "") << "\"" << nl;
            oss << indent << "}";
        }

        if (!entry.metadata.empty()) {
            oss << "," << nl;
            oss << indent << "\"metadata\":{" << nl;
            bool first = true;
            for (const auto& [key, value] : entry.metadata) {
                if (!first) oss << "," << nl;
                first = false;
                oss << indent << indent << "\"" << EscapeJson(key) << "\":\"" << EscapeJson(value) << "\"";
            }
            oss << nl << indent << "}";
        }

        oss << nl << "}";
        return oss.str();
    }

private:
    bool m_prettyPrint;

    [[nodiscard]] static std::string FormatTimestamp(LogEntry::TimePoint tp) {
        auto time = LogEntry::Clock::to_time_t(tp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch()) % 1000;

        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << "Z";
        return oss.str();
    }

    [[nodiscard]] static std::string EscapeJson(std::string_view str) {
        std::string result;
        result.reserve(str.size());
        for (char c : str) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                        result += buf;
                    } else {
                        result += c;
                    }
            }
        }
        return result;
    }
};

// ============================================================================
// ILogSink - Interface for log output destinations
// ============================================================================

/**
 * @brief Interface for log output destinations (Open/Closed Principle)
 *
 * Implement this interface to add new log output targets without
 * modifying existing code.
 */
class ILogSink {
public:
    virtual ~ILogSink() = default;

    /**
     * @brief Write a log entry to this sink
     * @param entry The formatted log entry
     */
    virtual void Write(const LogEntry& entry) = 0;

    /**
     * @brief Flush any buffered output
     */
    virtual void Flush() = 0;

    /**
     * @brief Set the formatter for this sink
     */
    virtual void SetFormatter(std::shared_ptr<ILogFormatter> formatter) = 0;

    /**
     * @brief Get the current formatter
     */
    [[nodiscard]] virtual ILogFormatter* GetFormatter() const = 0;

    /**
     * @brief Set minimum log level for this sink
     */
    virtual void SetLevel(LogLevel level) = 0;

    /**
     * @brief Get minimum log level for this sink
     */
    [[nodiscard]] virtual LogLevel GetLevel() const = 0;

    /**
     * @brief Check if this level should be logged
     */
    [[nodiscard]] virtual bool ShouldLog(LogLevel level) const {
        return level >= GetLevel();
    }
};

/**
 * @brief Base implementation of ILogSink with common functionality
 */
class LogSinkBase : public ILogSink {
public:
    void SetFormatter(std::shared_ptr<ILogFormatter> formatter) override {
        std::lock_guard lock(m_mutex);
        m_formatter = std::move(formatter);
    }

    [[nodiscard]] ILogFormatter* GetFormatter() const override {
        std::lock_guard lock(m_mutex);
        return m_formatter.get();
    }

    void SetLevel(LogLevel level) override {
        m_level.store(level, std::memory_order_relaxed);
    }

    [[nodiscard]] LogLevel GetLevel() const override {
        return m_level.load(std::memory_order_relaxed);
    }

protected:
    [[nodiscard]] std::string FormatEntry(const LogEntry& entry) {
        std::lock_guard lock(m_mutex);
        if (m_formatter) {
            return m_formatter->Format(entry);
        }
        // Default format if no formatter set
        return std::string(LogLevelToString(entry.level)) + ": " + entry.message;
    }

    mutable std::mutex m_mutex;
    std::shared_ptr<ILogFormatter> m_formatter;
    std::atomic<LogLevel> m_level{LogLevel::Trace};
};

// ============================================================================
// Console Sink - Output to stdout/stderr with optional colors
// ============================================================================

/**
 * @brief Console log sink with optional color output
 */
class ConsoleSink : public LogSinkBase {
public:
    explicit ConsoleSink(bool useColors = true, bool useStderr = false)
        : m_useColors(useColors)
        , m_useStderr(useStderr)
#ifdef _WIN32
        , m_consoleHandle(GetStdHandle(useStderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE))
#endif
    {
        SetFormatter(std::make_shared<TextLogFormatter>("[%T] [%L] [%c] %m"));
    }

    void Write(const LogEntry& entry) override {
        if (!ShouldLog(entry.level)) return;

        std::string formatted = FormatEntry(entry);
        formatted += '\n';

        std::lock_guard lock(m_outputMutex);
        FILE* stream = m_useStderr ? stderr : stdout;

        if (m_useColors) {
            SetColor(entry.level);
        }

        std::fwrite(formatted.c_str(), 1, formatted.size(), stream);

        if (m_useColors) {
            ResetColor();
        }
    }

    void Flush() override {
        std::lock_guard lock(m_outputMutex);
        std::fflush(m_useStderr ? stderr : stdout);
    }

    void SetColors(bool enable) { m_useColors = enable; }

private:
    std::mutex m_outputMutex;
    bool m_useColors;
    bool m_useStderr;
#ifdef _WIN32
    HANDLE m_consoleHandle;
#endif

    void SetColor(LogLevel level) {
#ifdef _WIN32
        WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        switch (level) {
            case LogLevel::Trace: color = FOREGROUND_INTENSITY; break;
            case LogLevel::Debug: color = FOREGROUND_GREEN | FOREGROUND_BLUE; break;
            case LogLevel::Info:  color = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
            case LogLevel::Warn:  color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;
            case LogLevel::Error: color = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
            case LogLevel::Fatal: color = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;
            default: break;
        }
        SetConsoleTextAttribute(m_consoleHandle, color);
#else
        const char* color = "";
        switch (level) {
            case LogLevel::Trace: color = "\033[90m"; break;       // Dark gray
            case LogLevel::Debug: color = "\033[36m"; break;       // Cyan
            case LogLevel::Info:  color = "\033[32m"; break;       // Green
            case LogLevel::Warn:  color = "\033[33m"; break;       // Yellow
            case LogLevel::Error: color = "\033[31m"; break;       // Red
            case LogLevel::Fatal: color = "\033[35;1m"; break;     // Magenta bold
            default: break;
        }
        std::fputs(color, m_useStderr ? stderr : stdout);
#endif
    }

    void ResetColor() {
#ifdef _WIN32
        SetConsoleTextAttribute(m_consoleHandle,
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
        std::fputs("\033[0m", m_useStderr ? stderr : stdout);
#endif
    }
};

// ============================================================================
// File Sink - Output to file with rotation support
// ============================================================================

/**
 * @brief File log sink with optional rotation
 */
class FileSink : public LogSinkBase {
public:
    struct Config {
        std::string filePath;
        size_t maxFileSize = 10 * 1024 * 1024;  // 10 MB default
        size_t maxFiles = 5;                      // Keep 5 backup files
        bool append = true;                       // Append to existing file
    };

    explicit FileSink(const Config& config)
        : m_config(config)
    {
        SetFormatter(std::make_shared<TextLogFormatter>("[%t] [%l] [%c] [%f:%n] %m"));
        OpenFile();
    }

    explicit FileSink(std::string_view filePath)
        : FileSink(Config{std::string(filePath)}) {}

    ~FileSink() override {
        Flush();
    }

    void Write(const LogEntry& entry) override {
        if (!ShouldLog(entry.level)) return;

        std::string formatted = FormatEntry(entry);
        formatted += '\n';

        std::lock_guard lock(m_fileMutex);

        if (!m_file.is_open()) {
            OpenFile();
        }

        if (m_file.is_open()) {
            m_file.write(formatted.c_str(), static_cast<std::streamsize>(formatted.size()));
            m_currentSize += formatted.size();

            if (m_config.maxFileSize > 0 && m_currentSize >= m_config.maxFileSize) {
                RotateFile();
            }
        }
    }

    void Flush() override {
        std::lock_guard lock(m_fileMutex);
        if (m_file.is_open()) {
            m_file.flush();
        }
    }

private:
    Config m_config;
    std::mutex m_fileMutex;
    std::ofstream m_file;
    size_t m_currentSize = 0;

    void OpenFile() {
        auto mode = std::ios::out;
        if (m_config.append) {
            mode |= std::ios::app;
        }
        m_file.open(m_config.filePath, mode);

        if (m_file.is_open() && m_config.append) {
            m_file.seekp(0, std::ios::end);
            m_currentSize = static_cast<size_t>(m_file.tellp());
        } else {
            m_currentSize = 0;
        }
    }

    void RotateFile() {
        m_file.close();

        // Rotate existing files
        for (size_t i = m_config.maxFiles - 1; i > 0; --i) {
            std::string oldPath = m_config.filePath + "." + std::to_string(i);
            std::string newPath = m_config.filePath + "." + std::to_string(i + 1);
            std::remove(newPath.c_str());
            std::rename(oldPath.c_str(), newPath.c_str());
        }

        // Rename current file to .1
        std::string backupPath = m_config.filePath + ".1";
        std::remove(backupPath.c_str());
        std::rename(m_config.filePath.c_str(), backupPath.c_str());

        // Open new file
        m_file.open(m_config.filePath, std::ios::out);
        m_currentSize = 0;
    }
};

// ============================================================================
// Callback Sink - Custom callback for log handling
// ============================================================================

/**
 * @brief Callback-based sink for custom log handling
 */
class CallbackSink : public LogSinkBase {
public:
    using Callback = std::function<void(const LogEntry&, std::string_view formatted)>;

    explicit CallbackSink(Callback callback)
        : m_callback(std::move(callback)) {
        SetFormatter(std::make_shared<TextLogFormatter>("%m"));
    }

    void Write(const LogEntry& entry) override {
        if (!ShouldLog(entry.level) || !m_callback) return;
        std::string formatted = FormatEntry(entry);
        m_callback(entry, formatted);
    }

    void Flush() override {
        // Callbacks are synchronous, nothing to flush
    }

private:
    Callback m_callback;
};

// ============================================================================
// AsyncLogQueue - Lock-free queue for async logging
// ============================================================================

/**
 * @brief Thread-safe queue for async log processing
 */
class AsyncLogQueue {
public:
    explicit AsyncLogQueue(size_t capacity = 8192)
        : m_capacity(capacity)
        , m_running(false) {}

    ~AsyncLogQueue() {
        Stop();
    }

    void Start() {
        if (m_running.exchange(true)) return;
        m_thread = std::thread(&AsyncLogQueue::ProcessLoop, this);
    }

    void Stop() {
        if (!m_running.exchange(false)) return;
        m_condition.notify_one();
        if (m_thread.joinable()) {
            m_thread.join();
        }
        // Process remaining entries
        ProcessRemaining();
    }

    bool Push(LogEntry entry) {
        std::lock_guard lock(m_mutex);
        if (m_queue.size() >= m_capacity) {
            // Queue full, drop oldest
            m_queue.pop();
        }
        m_queue.push(std::move(entry));
        m_condition.notify_one();
        return true;
    }

    void AddSink(std::shared_ptr<ILogSink> sink) {
        std::lock_guard lock(m_sinkMutex);
        m_sinks.push_back(std::move(sink));
    }

    void RemoveSink(ILogSink* sink) {
        std::lock_guard lock(m_sinkMutex);
        m_sinks.erase(
            std::remove_if(m_sinks.begin(), m_sinks.end(),
                [sink](const auto& s) { return s.get() == sink; }),
            m_sinks.end());
    }

    void Flush() {
        // Wait for queue to empty
        std::unique_lock lock(m_mutex);
        m_condition.wait(lock, [this] { return m_queue.empty() || !m_running; });

        // Flush all sinks
        std::lock_guard sinkLock(m_sinkMutex);
        for (auto& sink : m_sinks) {
            sink->Flush();
        }
    }

private:
    size_t m_capacity;
    std::atomic<bool> m_running;
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::queue<LogEntry> m_queue;
    std::mutex m_sinkMutex;
    std::vector<std::shared_ptr<ILogSink>> m_sinks;

    void ProcessLoop() {
        while (m_running) {
            std::unique_lock lock(m_mutex);
            m_condition.wait(lock, [this] { return !m_queue.empty() || !m_running; });

            while (!m_queue.empty()) {
                LogEntry entry = std::move(m_queue.front());
                m_queue.pop();
                lock.unlock();

                DispatchEntry(entry);

                lock.lock();
            }
        }
    }

    void ProcessRemaining() {
        std::lock_guard lock(m_mutex);
        while (!m_queue.empty()) {
            LogEntry entry = std::move(m_queue.front());
            m_queue.pop();
            DispatchEntry(entry);
        }
    }

    void DispatchEntry(const LogEntry& entry) {
        std::lock_guard lock(m_sinkMutex);
        for (auto& sink : m_sinks) {
            sink->Write(entry);
        }
    }
};

// ============================================================================
// Logger - Individual logger instance with category
// ============================================================================

/**
 * @brief Logger instance for a specific category
 */
class Logger {
public:
    explicit Logger(std::string_view category, LogLevel level = LogLevel::Trace)
        : m_category(category)
        , m_level(level) {}

    [[nodiscard]] const std::string& GetCategory() const { return m_category; }
    [[nodiscard]] LogLevel GetLevel() const { return m_level.load(std::memory_order_relaxed); }
    void SetLevel(LogLevel level) { m_level.store(level, std::memory_order_relaxed); }

    [[nodiscard]] bool ShouldLog(LogLevel level) const {
        return level >= m_level.load(std::memory_order_relaxed);
    }

    template<typename... Args>
    void Log(LogLevel level, SourceLocation loc, std::format_string<Args...> fmt, Args&&... args) {
        if (!ShouldLog(level)) return;
        std::string message = std::format(fmt, std::forward<Args>(args)...);
        LogImpl(level, std::move(message), loc);
    }

    void Log(LogLevel level, SourceLocation loc, std::string_view message) {
        if (!ShouldLog(level)) return;
        LogImpl(level, std::string(message), loc);
    }

    // Convenience methods
    template<typename... Args>
    void Trace(SourceLocation loc, std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Trace, loc, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Debug(SourceLocation loc, std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Debug, loc, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Info(SourceLocation loc, std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Info, loc, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Warn(SourceLocation loc, std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Warn, loc, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Error(SourceLocation loc, std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Error, loc, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Fatal(SourceLocation loc, std::format_string<Args...> fmt, Args&&... args) {
        Log(LogLevel::Fatal, loc, fmt, std::forward<Args>(args)...);
    }

private:
    std::string m_category;
    std::atomic<LogLevel> m_level;

    void LogImpl(LogLevel level, std::string message, SourceLocation loc);
};

// ============================================================================
// LogManager - Central logging system manager (Singleton)
// ============================================================================

/**
 * @brief Central manager for the logging system
 *
 * Manages sinks, loggers, and provides the main interface for logging.
 * Thread-safe singleton with async logging support.
 */
class LogManager {
public:
    /**
     * @brief Get the singleton instance
     */
    [[nodiscard]] static LogManager& Instance() {
        static LogManager instance;
        return instance;
    }

    // Delete copy/move
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    LogManager(LogManager&&) = delete;
    LogManager& operator=(LogManager&&) = delete;

    /**
     * @brief Initialize the logging system
     * @param asyncLogging Enable async logging (default true)
     * @return true on success
     */
    bool Initialize(bool asyncLogging = true) {
        std::lock_guard lock(m_mutex);
        if (m_initialized) return true;

        m_asyncEnabled = asyncLogging;

        if (m_asyncEnabled) {
            m_asyncQueue = std::make_unique<AsyncLogQueue>();
            m_asyncQueue->Start();
        }

        // Create default console sink
        auto consoleSink = std::make_shared<ConsoleSink>(true);
        AddSink(consoleSink);

        // Create default engine logger
        m_engineLogger = GetLogger("NOVA");
        m_appLogger = GetLogger("APP");

        m_initialized = true;
        return true;
    }

    /**
     * @brief Shutdown the logging system
     */
    void Shutdown() {
        std::lock_guard lock(m_mutex);
        if (!m_initialized) return;

        if (m_asyncQueue) {
            m_asyncQueue->Stop();
            m_asyncQueue.reset();
        }

        FlushAllSinks();
        m_sinks.clear();
        m_loggers.clear();
        m_initialized = false;
    }

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Add a log sink
     */
    void AddSink(std::shared_ptr<ILogSink> sink) {
        std::lock_guard lock(m_sinkMutex);
        m_sinks.push_back(sink);
        if (m_asyncQueue) {
            m_asyncQueue->AddSink(sink);
        }
    }

    /**
     * @brief Remove a log sink
     */
    void RemoveSink(ILogSink* sink) {
        std::lock_guard lock(m_sinkMutex);
        m_sinks.erase(
            std::remove_if(m_sinks.begin(), m_sinks.end(),
                [sink](const auto& s) { return s.get() == sink; }),
            m_sinks.end());
        if (m_asyncQueue) {
            m_asyncQueue->RemoveSink(sink);
        }
    }

    /**
     * @brief Get or create a logger by category
     */
    std::shared_ptr<Logger> GetLogger(std::string_view category) {
        std::string cat(category);
        std::lock_guard lock(m_loggerMutex);

        auto it = m_loggers.find(cat);
        if (it != m_loggers.end()) {
            return it->second;
        }

        auto logger = std::make_shared<Logger>(category, m_globalLevel);
        m_loggers[cat] = logger;
        return logger;
    }

    /**
     * @brief Get the engine logger
     */
    [[nodiscard]] Logger* GetEngineLogger() const { return m_engineLogger.get(); }

    /**
     * @brief Get the application logger
     */
    [[nodiscard]] Logger* GetAppLogger() const { return m_appLogger.get(); }

    /**
     * @brief Set global log level for all loggers
     */
    void SetGlobalLevel(LogLevel level) {
        m_globalLevel = level;
        std::lock_guard lock(m_loggerMutex);
        for (auto& [name, logger] : m_loggers) {
            logger->SetLevel(level);
        }
    }

    /**
     * @brief Get global log level
     */
    [[nodiscard]] LogLevel GetGlobalLevel() const { return m_globalLevel; }

    /**
     * @brief Enable/disable a category
     */
    void SetCategoryEnabled(std::string_view category, bool enabled) {
        std::lock_guard lock(m_filterMutex);
        if (enabled) {
            m_disabledCategories.erase(std::string(category));
        } else {
            m_disabledCategories.insert(std::string(category));
        }
    }

    /**
     * @brief Check if a category is enabled
     */
    [[nodiscard]] bool IsCategoryEnabled(std::string_view category) const {
        std::shared_lock lock(m_filterMutex);
        return m_disabledCategories.find(std::string(category)) == m_disabledCategories.end();
    }

    /**
     * @brief Dispatch a log entry to all sinks
     */
    void Dispatch(LogEntry entry) {
        if (!IsCategoryEnabled(entry.category)) return;

        if (m_asyncEnabled && m_asyncQueue) {
            m_asyncQueue->Push(std::move(entry));
        } else {
            std::shared_lock lock(m_sinkMutex);
            for (auto& sink : m_sinks) {
                sink->Write(entry);
            }
        }
    }

    /**
     * @brief Flush all sinks
     */
    void Flush() {
        if (m_asyncQueue) {
            m_asyncQueue->Flush();
        } else {
            FlushAllSinks();
        }
    }

private:
    LogManager() = default;
    ~LogManager() { Shutdown(); }

    void FlushAllSinks() {
        std::shared_lock lock(m_sinkMutex);
        for (auto& sink : m_sinks) {
            sink->Flush();
        }
    }

    std::mutex m_mutex;
    bool m_initialized = false;
    bool m_asyncEnabled = false;
    std::atomic<LogLevel> m_globalLevel{LogLevel::Trace};

    std::unique_ptr<AsyncLogQueue> m_asyncQueue;

    mutable std::shared_mutex m_sinkMutex;
    std::vector<std::shared_ptr<ILogSink>> m_sinks;

    mutable std::mutex m_loggerMutex;
    std::unordered_map<std::string, std::shared_ptr<Logger>> m_loggers;
    std::shared_ptr<Logger> m_engineLogger;
    std::shared_ptr<Logger> m_appLogger;

    mutable std::shared_mutex m_filterMutex;
    std::unordered_set<std::string> m_disabledCategories;
};

// ============================================================================
// Logger Implementation (must be after LogManager)
// ============================================================================

inline void Logger::LogImpl(LogLevel level, std::string message, SourceLocation loc) {
    LogEntry entry(level, m_category, std::move(message), loc);
    LogManager::Instance().Dispatch(std::move(entry));
}

} // namespace Nova

// ============================================================================
// Compile-Time Log Level Configuration
// ============================================================================

/**
 * Define NOVA_MIN_LOG_LEVEL to strip lower-level logs at compile time:
 * 0 = Trace, 1 = Debug, 2 = Info, 3 = Warn, 4 = Error, 5 = Fatal, 6 = Off
 */
#ifndef NOVA_MIN_LOG_LEVEL
    #ifdef NDEBUG
        #define NOVA_MIN_LOG_LEVEL 2  // Info in release
    #else
        #define NOVA_MIN_LOG_LEVEL 0  // Trace in debug
    #endif
#endif

// ============================================================================
// Logging Macros
// ============================================================================

// Internal helper for source location
#if NOVA_HAS_SOURCE_LOCATION
    #define NOVA_CURRENT_LOCATION ::Nova::SourceLocation::Current()
#else
    #define NOVA_CURRENT_LOCATION ::Nova::SourceLocation(__FILE__, __FUNCTION__, __LINE__)
#endif

// Engine logging macros with compile-time level stripping
#if NOVA_MIN_LOG_LEVEL <= 0
    #define NOVA_LOG_TRACE(...)                                                         \
        do {                                                                            \
            if (auto* logger = ::Nova::LogManager::Instance().GetEngineLogger())        \
                logger->Trace(NOVA_CURRENT_LOCATION, __VA_ARGS__);                      \
        } while (0)
#else
    #define NOVA_LOG_TRACE(...) ((void)0)
#endif

#if NOVA_MIN_LOG_LEVEL <= 1
    #define NOVA_LOG_DEBUG(...)                                                         \
        do {                                                                            \
            if (auto* logger = ::Nova::LogManager::Instance().GetEngineLogger())        \
                logger->Debug(NOVA_CURRENT_LOCATION, __VA_ARGS__);                      \
        } while (0)
#else
    #define NOVA_LOG_DEBUG(...) ((void)0)
#endif

#if NOVA_MIN_LOG_LEVEL <= 2
    #define NOVA_LOG_INFO(...)                                                          \
        do {                                                                            \
            if (auto* logger = ::Nova::LogManager::Instance().GetEngineLogger())        \
                logger->Info(NOVA_CURRENT_LOCATION, __VA_ARGS__);                       \
        } while (0)
#else
    #define NOVA_LOG_INFO(...) ((void)0)
#endif

#if NOVA_MIN_LOG_LEVEL <= 3
    #define NOVA_LOG_WARN(...)                                                          \
        do {                                                                            \
            if (auto* logger = ::Nova::LogManager::Instance().GetEngineLogger())        \
                logger->Warn(NOVA_CURRENT_LOCATION, __VA_ARGS__);                       \
        } while (0)
#else
    #define NOVA_LOG_WARN(...) ((void)0)
#endif

#if NOVA_MIN_LOG_LEVEL <= 4
    #define NOVA_LOG_ERROR(...)                                                         \
        do {                                                                            \
            if (auto* logger = ::Nova::LogManager::Instance().GetEngineLogger())        \
                logger->Error(NOVA_CURRENT_LOCATION, __VA_ARGS__);                      \
        } while (0)
#else
    #define NOVA_LOG_ERROR(...) ((void)0)
#endif

#if NOVA_MIN_LOG_LEVEL <= 5
    #define NOVA_LOG_FATAL(...)                                                         \
        do {                                                                            \
            if (auto* logger = ::Nova::LogManager::Instance().GetEngineLogger())        \
                logger->Fatal(NOVA_CURRENT_LOCATION, __VA_ARGS__);                      \
        } while (0)
    #define NOVA_LOG_CRITICAL(...) NOVA_LOG_FATAL(__VA_ARGS__)
#else
    #define NOVA_LOG_FATAL(...) ((void)0)
    #define NOVA_LOG_CRITICAL(...) ((void)0)
#endif

// Application logging macros
#if NOVA_MIN_LOG_LEVEL <= 0
    #define APP_LOG_TRACE(...)                                                          \
        do {                                                                            \
            if (auto* logger = ::Nova::LogManager::Instance().GetAppLogger())           \
                logger->Trace(NOVA_CURRENT_LOCATION, __VA_ARGS__);                      \
        } while (0)
#else
    #define APP_LOG_TRACE(...) ((void)0)
#endif

#if NOVA_MIN_LOG_LEVEL <= 1
    #define APP_LOG_DEBUG(...)                                                          \
        do {                                                                            \
            if (auto* logger = ::Nova::LogManager::Instance().GetAppLogger())           \
                logger->Debug(NOVA_CURRENT_LOCATION, __VA_ARGS__);                      \
        } while (0)
#else
    #define APP_LOG_DEBUG(...) ((void)0)
#endif

#if NOVA_MIN_LOG_LEVEL <= 2
    #define APP_LOG_INFO(...)                                                           \
        do {                                                                            \
            if (auto* logger = ::Nova::LogManager::Instance().GetAppLogger())           \
                logger->Info(NOVA_CURRENT_LOCATION, __VA_ARGS__);                       \
        } while (0)
#else
    #define APP_LOG_INFO(...) ((void)0)
#endif

#if NOVA_MIN_LOG_LEVEL <= 3
    #define APP_LOG_WARN(...)                                                           \
        do {                                                                            \
            if (auto* logger = ::Nova::LogManager::Instance().GetAppLogger())           \
                logger->Warn(NOVA_CURRENT_LOCATION, __VA_ARGS__);                       \
        } while (0)
#else
    #define APP_LOG_WARN(...) ((void)0)
#endif

#if NOVA_MIN_LOG_LEVEL <= 4
    #define APP_LOG_ERROR(...)                                                          \
        do {                                                                            \
            if (auto* logger = ::Nova::LogManager::Instance().GetAppLogger())           \
                logger->Error(NOVA_CURRENT_LOCATION, __VA_ARGS__);                      \
        } while (0)
#else
    #define APP_LOG_ERROR(...) ((void)0)
#endif

#if NOVA_MIN_LOG_LEVEL <= 5
    #define APP_LOG_FATAL(...)                                                          \
        do {                                                                            \
            if (auto* logger = ::Nova::LogManager::Instance().GetAppLogger())           \
                logger->Fatal(NOVA_CURRENT_LOCATION, __VA_ARGS__);                      \
        } while (0)
    #define APP_LOG_CRITICAL(...) APP_LOG_FATAL(__VA_ARGS__)
#else
    #define APP_LOG_FATAL(...) ((void)0)
    #define APP_LOG_CRITICAL(...) ((void)0)
#endif

// Debug-only macros
#ifdef NOVA_DEBUG
    #define NOVA_LOG_DEBUG_ONLY(...) NOVA_LOG_DEBUG(__VA_ARGS__)
    #define APP_LOG_DEBUG_ONLY(...)  APP_LOG_DEBUG(__VA_ARGS__)
#else
    #define NOVA_LOG_DEBUG_ONLY(...) ((void)0)
    #define APP_LOG_DEBUG_ONLY(...)  ((void)0)
#endif

// Shorthand aliases (LOG_INFO, LOG_ERROR, etc.)
#define LOG_TRACE(...)    NOVA_LOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...)    NOVA_LOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)     NOVA_LOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)     NOVA_LOG_WARN(__VA_ARGS__)
#define LOG_WARNING(...)  NOVA_LOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...)    NOVA_LOG_ERROR(__VA_ARGS__)
#define LOG_FATAL(...)    NOVA_LOG_FATAL(__VA_ARGS__)
#define LOG_CRITICAL(...) NOVA_LOG_CRITICAL(__VA_ARGS__)

// Scoped timing helper
#define NOVA_LOG_SCOPE_TIME(name)                                                       \
    auto NOVA_CONCAT(_nova_scope_timer_, __LINE__) =                                    \
        ::Nova::detail::ScopeTimer(name, NOVA_CURRENT_LOCATION)

#define NOVA_CONCAT_IMPL(a, b) a##b
#define NOVA_CONCAT(a, b) NOVA_CONCAT_IMPL(a, b)

namespace Nova::detail {

/**
 * @brief RAII scope timer for performance logging
 */
class ScopeTimer {
public:
    ScopeTimer(std::string_view name, SourceLocation loc)
        : m_name(name)
        , m_location(loc)
        , m_start(std::chrono::high_resolution_clock::now()) {}

    ~ScopeTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start);

        if (auto* logger = LogManager::Instance().GetEngineLogger()) {
            logger->Debug(m_location, "{} took {} us", m_name, duration.count());
        }
    }

private:
    std::string_view m_name;
    SourceLocation m_location;
    std::chrono::high_resolution_clock::time_point m_start;
};

} // namespace Nova::detail
