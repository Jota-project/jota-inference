#pragma once

#include <string>
#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <nlohmann/json.hpp>

namespace Server {
namespace Utils {

class Logger {
public:
    static std::mutex& getMutex() {
        static std::mutex mtx;
        return mtx;
    }

    static void info(const std::string& message, const nlohmann::json& metadata = nullptr) {
        log("INFO", message, metadata);
    }

    static void warn(const std::string& message, const nlohmann::json& metadata = nullptr) {
        log("WARN", message, metadata);
    }

    static void error(const std::string& message, const nlohmann::json& metadata = nullptr) {
        log("ERROR", message, metadata);
    }

    static void debug(const std::string& message, const nlohmann::json& metadata = nullptr) {
        log("DEBUG", message, metadata);
    }

private:
    static void log(const std::string& level, const std::string& message, const nlohmann::json& metadata) {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%S") 
           << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';

        nlohmann::json j;
        j["timestamp"] = ss.str();
        j["level"] = level;
        j["message"] = message;
        
        // Getting an integer thread ID
        j["thread_id"] = std::hash<std::thread::id>{}(std::this_thread::get_id());

        if (!metadata.is_null() && !metadata.empty()) {
            j["extra"] = metadata;
        }

        std::string output = j.dump();

        std::lock_guard<std::mutex> lock(getMutex());
        std::cout << output << std::endl;
    }
};


// Helper: Validate and clean UTF-8 string to prevent JSON serialization errors
inline std::string sanitizeUtf8(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    
    for (size_t i = 0; i < input.size(); ) {
        unsigned char c = input[i];
        
        // ASCII (0x00-0x7F)
        if (c <= 0x7F) {
            output += c;
            i++;
        }
        // 2-byte UTF-8 (0xC0-0xDF)
        else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < input.size() && (input[i+1] & 0xC0) == 0x80) {
                output += input[i];
                output += input[i+1];
                i += 2;
            } else {
                i++; // Skip invalid sequence
            }
        }
        // 3-byte UTF-8 (0xE0-0xEF)
        else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < input.size() && (input[i+1] & 0xC0) == 0x80 && (input[i+2] & 0xC0) == 0x80) {
                output += input[i];
                output += input[i+1];
                output += input[i+2];
                i += 3;
            } else {
                i++; // Skip invalid sequence
            }
        }
        // 4-byte UTF-8 (0xF0-0xF7)
        else if ((c & 0xF8) == 0xF0) {
            if (i + 3 < input.size() && (input[i+1] & 0xC0) == 0x80 && 
                (input[i+2] & 0xC0) == 0x80 && (input[i+3] & 0xC0) == 0x80) {
                output += input[i];
                output += input[i+1];
                output += input[i+2];
                output += input[i+3];
                i += 4;
            } else {
                i++; // Skip invalid sequence
            }
        }
        else {
            i++; // Skip invalid start byte
        }
    }
    
    return output;
}

} // namespace Utils
} // namespace Server
