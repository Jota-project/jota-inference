#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Server {

    struct ClientConfig {
        std::string client_id;
        std::string api_key;
        int max_sessions = 1;
        std::string priority = "normal";
        std::string description;
    };

    class ClientAuth {
    public:
        ClientAuth();
        ~ClientAuth() = default;

        // Load client configuration from JSON file
        bool loadConfig(const std::string& configPath);

        // Authenticate a client with client_id and api_key
        bool authenticate(const std::string& client_id, const std::string& api_key);

        // Get client configuration (must be authenticated first)
        ClientConfig getClientConfig(const std::string& client_id) const;

        // Check if a client_id exists in the config
        bool clientExists(const std::string& client_id) const;

    private:
        std::unordered_map<std::string, ClientConfig> clients_;
        mutable std::mutex mutex_;
    };

}
