#include "core/Logger.hpp"
#include <spdlog/sinks/rotating_file_sink.h>
#include <vector>

namespace Nova {

std::shared_ptr<spdlog::logger> Logger::s_engineLogger;
std::shared_ptr<spdlog::logger> Logger::s_appLogger;
bool Logger::s_initialized = false;

void Logger::Initialize(const std::string& logFile, bool consoleOutput) {
    if (s_initialized) {
        return;
    }

    std::vector<spdlog::sink_ptr> sinks;

    // Console sink
    if (consoleOutput) {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_pattern("%^[%T] [%n] [%l]%$ %v");
        sinks.push_back(consoleSink);
    }

    // File sink
    if (!logFile.empty()) {
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logFile, 5 * 1024 * 1024, 3);  // 5MB max, 3 files
        fileSink->set_pattern("[%Y-%m-%d %T.%e] [%n] [%l] %v");
        sinks.push_back(fileSink);
    }

    // Create engine logger
    s_engineLogger = std::make_shared<spdlog::logger>("NOVA", sinks.begin(), sinks.end());
    s_engineLogger->set_level(spdlog::level::trace);
    s_engineLogger->flush_on(spdlog::level::warn);
    spdlog::register_logger(s_engineLogger);

    // Create application logger
    s_appLogger = std::make_shared<spdlog::logger>("APP", sinks.begin(), sinks.end());
    s_appLogger->set_level(spdlog::level::trace);
    s_appLogger->flush_on(spdlog::level::warn);
    spdlog::register_logger(s_appLogger);

    // Set as default
    spdlog::set_default_logger(s_engineLogger);

    s_initialized = true;
}

void Logger::Shutdown() {
    if (!s_initialized) {
        return;
    }

    s_engineLogger->flush();
    s_appLogger->flush();

    spdlog::drop_all();

    s_engineLogger.reset();
    s_appLogger.reset();
    s_initialized = false;
}

void Logger::SetLevel(spdlog::level::level_enum level) {
    if (s_engineLogger) {
        s_engineLogger->set_level(level);
    }
    if (s_appLogger) {
        s_appLogger->set_level(level);
    }
}

} // namespace Nova
