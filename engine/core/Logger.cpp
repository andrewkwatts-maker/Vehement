#include "core/Logger.hpp"
#include <spdlog/sinks/rotating_file_sink.h>
#include <vector>
#include <mutex>

namespace Nova {

namespace {
    // Mutex for thread-safe initialization/shutdown
    std::mutex s_loggerMutex;
}

bool Logger::Initialize(std::string_view logFile, bool consoleOutput) {
    std::lock_guard lock(s_loggerMutex);

    if (s_initialized) {
        return true;  // Already initialized
    }

    try {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.reserve(2);  // Pre-allocate for console + file

        // Console sink with color support
        if (consoleOutput) {
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_pattern("%^[%T] [%n] [%l]%$ %v");
            sinks.push_back(std::move(consoleSink));
        }

        // Rotating file sink (5MB max, 3 backup files)
        if (!logFile.empty()) {
            constexpr std::size_t maxFileSize = 5 * 1024 * 1024;  // 5MB
            constexpr std::size_t maxFiles = 3;

            auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                std::string(logFile), maxFileSize, maxFiles);
            fileSink->set_pattern("[%Y-%m-%d %T.%e] [%n] [%l] %v");
            sinks.push_back(std::move(fileSink));
        }

        // Create engine logger
        s_engineLogger = std::make_shared<spdlog::logger>("NOVA", sinks.begin(), sinks.end());
        s_engineLogger->set_level(spdlog::level::trace);
        s_engineLogger->flush_on(spdlog::level::warn);
        spdlog::register_logger(s_engineLogger);

        // Create application logger (shares sinks for unified output)
        s_appLogger = std::make_shared<spdlog::logger>("APP", sinks.begin(), sinks.end());
        s_appLogger->set_level(spdlog::level::trace);
        s_appLogger->flush_on(spdlog::level::warn);
        spdlog::register_logger(s_appLogger);

        // Set engine logger as default for spdlog::info() etc.
        spdlog::set_default_logger(s_engineLogger);

        s_initialized = true;
        return true;

    } catch (const spdlog::spdlog_ex& ex) {
        // Log to stderr if spdlog initialization fails
        std::fprintf(stderr, "Logger initialization failed: %s\n", ex.what());
        return false;
    }
}

void Logger::Shutdown() noexcept {
    std::lock_guard lock(s_loggerMutex);

    if (!s_initialized) {
        return;
    }

    try {
        // Flush any remaining log messages
        if (s_engineLogger) {
            s_engineLogger->flush();
        }
        if (s_appLogger) {
            s_appLogger->flush();
        }

        // Drop all loggers from spdlog's registry
        spdlog::drop_all();

        // Release our references
        s_engineLogger.reset();
        s_appLogger.reset();
        s_initialized = false;

    } catch (...) {
        // Swallow exceptions during shutdown
    }
}

void Logger::SetLevel(Level level) noexcept {
    SetLevel(static_cast<spdlog::level::level_enum>(level));
}

void Logger::SetLevel(spdlog::level::level_enum level) noexcept {
    std::lock_guard lock(s_loggerMutex);

    if (s_engineLogger) {
        s_engineLogger->set_level(level);
    }
    if (s_appLogger) {
        s_appLogger->set_level(level);
    }
}

} // namespace Nova
