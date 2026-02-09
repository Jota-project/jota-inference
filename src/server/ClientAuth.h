#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Server {

    struct ClientConfig {
        std::string client_id;
        std::string api_key;
        int max_sessions = 1;
        std::string priority = "normal";
        std::string description;
        std::chrono::system_clock::time_point last_validated;
    };

    class ClientAuth {
    public:
        ClientAuth();
        ~ClientAuth() = default;

        // Authenticate a client with client_id and api_key via JotaDB
        // Returns true if valid, false otherwise or on error
        bool authenticate(const std::string& client_id, const std::string& api_key);

        // Get client configuration (must be authenticated first)
        // In JotaDB integration, this fetches or returns cached config
        ClientConfig getClientConfig(const std::string& client_id) const;

        // Check if a client_id exists (via JotaDB check)
        bool clientExists(const std::string& client_id) const;

    private:
        std::string jota_db_url_;
        std::string jota_db_usr_;
        std::string jota_db_sk_;
        
        // Simple cache to avoid hitting DB for every single token/operation if needed
        // For now, per requirements, we will validate dynamicially but might cache config
        // Mutable to allow modification in const methods if we implement caching
        mutable std::unordered_map<std::string, ClientConfig> client_cache_;
        mutable std::mutex mutex_;

        // Helper to parse JotaDB URL from env
        void initJotaDB();
    };

}
