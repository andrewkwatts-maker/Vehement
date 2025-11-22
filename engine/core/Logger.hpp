#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <string>

namespace Nova {

/**
 * @brief Logging system wrapper around spdlog
 *
 * Provides convenient logging macros and initialization.
 */
class Logger {
public:
    /**
     * @brief Initialize the logging system
     * @param logFile Optional file path for logging
     * @param consoleOutput Enable console output
     */
    static void Initialize(const std::string& logFile = "",
                          bool consoleOutput = true);

    /**
     * @brief Shutdown the logging system
     */
    static void Shutdown();

    /**
     * @brief Set the minimum log level
     */
    static void SetLevel(spdlog::level::level_enum level);

    /**
     * @brief Get the engine logger
     */
    static std::shared_ptr<spdlog::logger>& GetEngineLogger() {
        return s_engineLogger;
    }

    /**
     * @brief Get the application logger
     */
    static std::shared_ptr<spdlog::logger>& GetAppLogger() {
        return s_appLogger;
    }

private:
    static std::shared_ptr<spdlog::logger> s_engineLogger;
    static std::shared_ptr<spdlog::logger> s_appLogger;
    static bool s_initialized;
};

} // namespace Nova

// Convenience macros for engine logging
#define NOVA_LOG_TRACE(...)    ::Nova::Logger::GetEngineLogger()->trace(__VA_ARGS__)
#define NOVA_LOG_DEBUG(...)    ::Nova::Logger::GetEngineLogger()->debug(__VA_ARGS__)
#define NOVA_LOG_INFO(...)     ::Nova::Logger::GetEngineLogger()->info(__VA_ARGS__)
#define NOVA_LOG_WARN(...)     ::Nova::Logger::GetEngineLogger()->warn(__VA_ARGS__)
#define NOVA_LOG_ERROR(...)    ::Nova::Logger::GetEngineLogger()->error(__VA_ARGS__)
#define NOVA_LOG_CRITICAL(...) ::Nova::Logger::GetEngineLogger()->critical(__VA_ARGS__)

// Convenience macros for application logging
#define APP_LOG_TRACE(...)    ::Nova::Logger::GetAppLogger()->trace(__VA_ARGS__)
#define APP_LOG_DEBUG(...)    ::Nova::Logger::GetAppLogger()->debug(__VA_ARGS__)
#define APP_LOG_INFO(...)     ::Nova::Logger::GetAppLogger()->info(__VA_ARGS__)
#define APP_LOG_WARN(...)     ::Nova::Logger::GetAppLogger()->warn(__VA_ARGS__)
#define APP_LOG_ERROR(...)    ::Nova::Logger::GetAppLogger()->error(__VA_ARGS__)
#define APP_LOG_CRITICAL(...) ::Nova::Logger::GetAppLogger()->critical(__VA_ARGS__)
