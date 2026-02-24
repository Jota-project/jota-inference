#include "EnvLoader.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>

namespace Core {

    std::unordered_map<std::string, std::string> EnvLoader::env_map_;

    // Helper to trim whitespace
    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (std::string::npos == first) {
            return str;
        }
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    bool EnvLoader::load() {
        std::string env_path = ".env";
        std::ifstream file(env_path);
        
        if (!file.is_open()) {
            // Try parent directory (common when running from build/)
            env_path = "../.env";
            file.open(env_path);
            if (!file.is_open()) {
                IC_LOG_WARN(".env file not found in current or parent directory.");
                return false;
            }
        }
        
        IC_LOG_DEBUG("Loading configuration", {{"path", env_path}});

        std::string line;
        while (std::getline(file, line)) {
            // Remove comments
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }

            line = trim(line);
            if (line.empty()) continue;

            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = trim(line.substr(0, pos));
                std::string value = trim(line.substr(pos + 1));
                
                // Remove quotes if present
                if (value.size() >= 2 && 
                    ((value.front() == '"' && value.back() == '"') || 
                     (value.front() == '\'' && value.back() == '\''))) {
                    value = value.substr(1, value.size() - 2);
                }

                env_map_[key] = value;
                
                // Also set in system env for compatibility
                setenv(key.c_str(), value.c_str(), 1);
            }
        }
        
        IC_LOG_INFO("Loaded .env configuration");
        return true;
    }

    std::string EnvLoader::get(const std::string& key, const std::string& default_value) {
        // First check internal map (from .env)
        auto it = env_map_.find(key);
        if (it != env_map_.end()) {
            return it->second;
        }

        // Fallback to system env
        const char* val = std::getenv(key.c_str());
        if (val) {
            return std::string(val);
        }

        return default_value;
    }

}
