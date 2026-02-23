#pragma once

#include <string>
#include <mutex>
#include <nlohmann/json.hpp>

namespace Server {
namespace Utils {

enum class LogLevel {
    DEBUG = 0,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static std::mutex& getMutex() {
        static std::mutex mtx;
        return mtx;
    }

    static LogLevel getMinLogLevel();

    static void debug(const char* file, int line, const std::string& message, const nlohmann::json& metadata = nullptr) {
        log(LogLevel::DEBUG, "DEBUG", file, line, message, metadata);
    }

    static void info(const char* file, int line, const std::string& message, const nlohmann::json& metadata = nullptr) {
        log(LogLevel::INFO, "INFO", file, line, message, metadata);
    }

    static void warn(const char* file, int line, const std::string& message, const nlohmann::json& metadata = nullptr) {
        log(LogLevel::WARN, "WARN", file, line, message, metadata);
    }

    static void error(const char* file, int line, const std::string& message, const nlohmann::json& metadata = nullptr) {
        log(LogLevel::ERROR, "ERROR", file, line, message, metadata);
    }

private:
    static void log(LogLevel levelEnum, const std::string& levelStr, const char* file, int line, const std::string& message, const nlohmann::json& metadata);
};

// --- Macros for Logging ---
#define IC_LOG_DEBUG(msg, ...) Server::Utils::Logger::debug(__FILE__, __LINE__, msg, ##__VA_ARGS__)
#define IC_LOG_INFO(msg, ...)  Server::Utils::Logger::info(__FILE__, __LINE__, msg, ##__VA_ARGS__)
#define IC_LOG_WARN(msg, ...)  Server::Utils::Logger::warn(__FILE__, __LINE__, msg, ##__VA_ARGS__)
#define IC_LOG_ERROR(msg, ...) Server::Utils::Logger::error(__FILE__, __LINE__, msg, ##__VA_ARGS__)

} // namespace Utils
} // namespace Server
