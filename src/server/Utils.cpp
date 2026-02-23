#include "Utils.h"
#include "../core/EnvLoader.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace Server {
namespace Utils {

LogLevel Logger::getMinLogLevel() {
    static LogLevel minLevel = []() {
        std::string levelStr = Core::EnvLoader::get("LOG_LEVEL", "INFO");
        if (levelStr == "DEBUG") return LogLevel::DEBUG;
        if (levelStr == "WARN") return LogLevel::WARN;
        if (levelStr == "ERROR") return LogLevel::ERROR;
        return LogLevel::INFO; // Default
    }();
    return minLevel;
}

void Logger::log(LogLevel levelEnum, const std::string& levelStr, const char* file, int line, const std::string& message, const nlohmann::json& metadata) {
    if (levelEnum < getMinLogLevel()) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%S") 
       << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';

    nlohmann::json j;
    j["timestamp"] = ss.str();
    j["level"] = levelStr;
    j["message"] = message;
    
    // Context
    std::string filename(file);
    size_t last_slash = filename.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        filename = filename.substr(last_slash + 1);
    }
    j["file"] = filename;
    j["line"] = line;

    // Getting an integer thread ID
    j["thread_id"] = std::hash<std::thread::id>{}(std::this_thread::get_id());

    if (!metadata.is_null() && !metadata.empty()) {
        j["extra"] = metadata;
    }

    std::string output = j.dump();

    std::lock_guard<std::mutex> lock(getMutex());
    std::cout << output << std::endl;
}

} // namespace Utils
} // namespace Server
