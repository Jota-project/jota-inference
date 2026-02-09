#pragma once

#include <string>
#include <unordered_map>

namespace Core {

    class EnvLoader {
    public:
        // Load .env file from the current directory.
        // Returns true if successful, false otherwise.
        static bool load();

        // Get value of an environment variable.
        // Falls back to std::getenv if not found in the loaded map.
        static std::string get(const std::string& key, const std::string& default_value = "");

    private:
        static std::unordered_map<std::string, std::string> env_map_;
    };

}
