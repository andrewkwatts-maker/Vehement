#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <string>
#include <string_view>

namespace Nova {

/**
 * @brief Logging system wrapper around spdlog
 *
 * Provides convenient logging macros and initialization.
 * Thread-safe singleton pattern with lazy initialization.
 */
class Logger {
public:
    /**
     * @brief Log level enum for clearer API
     */
    enum class Level {
        Trace = spdlog::level::trace,
        Debug = spdlog::level::debug,
        Info = spdlog::level::info,
        Warn = spdlog::level::warn,
        Error = spdlog::level::err,
        Critical = spdlog::level::critical,
        Off = spdlog::level::off
    };

    /**
     * @brief Initialize the logging system
     * @param logFile Optional file path for logging (empty = no file logging)
     * @param consoleOutput Enable console output
     * @return true if initialization succeeded
     */
    [[nodiscard]] static bool Initialize(std::string_view logFile = "",
                                         bool consoleOutput = true);

    /**
     * @brief Shutdown the logging system and flush all buffers
     */
    static void Shutdown() noexcept;

    /**
     * @brief Set the minimum log level for all loggers
     * @param level Minimum level to log
     */
    static void SetLevel(Level level) noexcept;

    /**
     * @brief Set the minimum log level using spdlog enum directly
     * @param level Minimum level to log
     */
    static void SetLevel(spdlog::level::level_enum level) noexcept;

    /**
     * @brief Get the engine logger
     * @return Shared pointer to engine logger (never null after initialization)
     */
    [[nodiscard]] static spdlog::logger* GetEngineLogger() noexcept {
        return s_engineLogger.get();
    }

    /**
     * @brief Get the application logger
     * @return Shared pointer to application logger (never null after initialization)
     */
    [[nodiscard]] static spdlog::logger* GetAppLogger() noexcept {
        return s_appLogger.get();
    }

    /**
     * @brief Check if logger is initialized
     */
    [[nodiscard]] static bool IsInitialized() noexcept { return s_initialized; }

    // Delete copy/move - static-only class
    Logger() = delete;
    ~Logger() = delete;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    // Using inline static for C++17 header-only initialization
    inline static std::shared_ptr<spdlog::logger> s_engineLogger;
    inline static std::shared_ptr<spdlog::logger> s_appLogger;
    inline static bool s_initialized = false;
};

} // namespace Nova

// Convenience macros for engine logging
// These check for null to allow logging before initialization (will no-op)
#define NOVA_LOG_TRACE(...)                                                    \
    do {                                                                       \
        if (auto* logger = ::Nova::Logger::GetEngineLogger())                  \
            logger->trace(__VA_ARGS__);                                        \
    } while (0)

#define NOVA_LOG_DEBUG(...)                                                    \
    do {                                                                       \
        if (auto* logger = ::Nova::Logger::GetEngineLogger())                  \
            logger->debug(__VA_ARGS__);                                        \
    } while (0)

#define NOVA_LOG_INFO(...)                                                     \
    do {                                                                       \
        if (auto* logger = ::Nova::Logger::GetEngineLogger())                  \
            logger->info(__VA_ARGS__);                                         \
    } while (0)

#define NOVA_LOG_WARN(...)                                                     \
    do {                                                                       \
        if (auto* logger = ::Nova::Logger::GetEngineLogger())                  \
            logger->warn(__VA_ARGS__);                                         \
    } while (0)

#define NOVA_LOG_ERROR(...)                                                    \
    do {                                                                       \
        if (auto* logger = ::Nova::Logger::GetEngineLogger())                  \
            logger->error(__VA_ARGS__);                                        \
    } while (0)

#define NOVA_LOG_CRITICAL(...)                                                 \
    do {                                                                       \
        if (auto* logger = ::Nova::Logger::GetEngineLogger())                  \
            logger->critical(__VA_ARGS__);                                     \
    } while (0)

// Convenience macros for application logging
#define APP_LOG_TRACE(...)                                                     \
    do {                                                                       \
        if (auto* logger = ::Nova::Logger::GetAppLogger())                     \
            logger->trace(__VA_ARGS__);                                        \
    } while (0)

#define APP_LOG_DEBUG(...)                                                     \
    do {                                                                       \
        if (auto* logger = ::Nova::Logger::GetAppLogger())                     \
            logger->debug(__VA_ARGS__);                                        \
    } while (0)

#define APP_LOG_INFO(...)                                                      \
    do {                                                                       \
        if (auto* logger = ::Nova::Logger::GetAppLogger())                     \
            logger->info(__VA_ARGS__);                                         \
    } while (0)

#define APP_LOG_WARN(...)                                                      \
    do {                                                                       \
        if (auto* logger = ::Nova::Logger::GetAppLogger())                     \
            logger->warn(__VA_ARGS__);                                         \
    } while (0)

#define APP_LOG_ERROR(...)                                                     \
    do {                                                                       \
        if (auto* logger = ::Nova::Logger::GetAppLogger())                     \
            logger->error(__VA_ARGS__);                                        \
    } while (0)

#define APP_LOG_CRITICAL(...)                                                  \
    do {                                                                       \
        if (auto* logger = ::Nova::Logger::GetAppLogger())                     \
            logger->critical(__VA_ARGS__);                                     \
    } while (0)

// Conditional logging macros (only in debug builds)
#ifdef NOVA_DEBUG
    #define NOVA_LOG_DEBUG_ONLY(...) NOVA_LOG_DEBUG(__VA_ARGS__)
    #define APP_LOG_DEBUG_ONLY(...) APP_LOG_DEBUG(__VA_ARGS__)
#else
    #define NOVA_LOG_DEBUG_ONLY(...) ((void)0)
    #define APP_LOG_DEBUG_ONLY(...) ((void)0)
#endif
