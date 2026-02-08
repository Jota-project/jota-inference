#include "ClientAuth.h"
#include <iostream>
#include <cstdlib>
#include <httplib.h>

namespace Server {

    ClientAuth::ClientAuth() {
        initJotaDB();
    }

    void ClientAuth::initJotaDB() {
        const char* env_url = std::getenv("JOTA_DB_URL");
        if (env_url) {
            jota_db_url_ = env_url;
        } else {
            jota_db_url_ = "http://localhost:8000/api/db";
        }
        std::cout << "[Auth] JotaDB URL configured: " << jota_db_url_ << std::endl;
    }

    bool ClientAuth::authenticate(const std::string& client_id, const std::string& api_key) {
        std::cout << "[Auth] Validating " << client_id << " via JotaDB..." << std::endl;

        // Parse URL to host and port
        // Expected format: http://host:port/path/to/api
        // We use httplib for this.
        
        // Find scheme
        std::string url = jota_db_url_;
        std::string scheme;
        std::string host_port;
        std::string path_prefix;

        if (url.find("http://") == 0) {
            scheme = "http";
            host_port = url.substr(7);
        } else if (url.find("https://") == 0) {
            scheme = "https";
            host_port = url.substr(8);
        } else {
            // Default to http if no scheme
            scheme = "http"; 
            host_port = url;
            std::cerr << "[Auth] Warning: No scheme in JOTA_DB_URL, assuming http://" << std::endl;
        }

        // Split host_port into host:port and path
        size_t path_pos = host_port.find('/');
        std::string domain;
        if (path_pos != std::string::npos) {
            domain = host_port.substr(0, path_pos);
            path_prefix = host_port.substr(path_pos);
        } else {
            domain = host_port;
            path_prefix = "";
        }
        
        // Remove trailing slash from path_prefix if exists? 
        // Actually we enter /auth/internal relative to it.
        // Let's construct the full client URL.
        std::string base_url = scheme + "://" + domain;

        httplib::Client cli(base_url.c_str());
        cli.set_connection_timeout(2); // 2 seconds timeout
        cli.set_read_timeout(3);       // 3 seconds read timeout

        // Construct request path
        // path_prefix might be "/api/db"
        // Target: {JOTA_DB_URL}/auth/internal?client_id={id}&api_key={key}
        std::string request_path = path_prefix + "/auth/internal";
        
        // httplib handles query params via Params or manual string
        // Manual string is easier here
        std::string query = "?client_id=" + client_id + "&api_key=" + api_key;
        std::string full_path = request_path + query;
        
        auto res = cli.Get(full_path.c_str());

        if (res && res->status == 200) {
            try {
                auto json_res = json::parse(res->body);
                
                // Expecting response struct like:
                // { "authorized": true, "config": { "max_sessions": 2, ... } }
                // OR just the config if authorized?
                // The prompt says: "Utiliza nlohmann/json para procesar la respuesta"
                // Let's assume standard success response has the config.
                
                // For robustness, let's assume if we get 200 OK it implies valid auth 
                // BUT better check content if API returns 200 with { "error": ... }
                
                if (json_res.contains("error")) {
                     std::cout << "[Auth] Validation failed for " << client_id << ": " 
                               << json_res["error"] << std::endl;
                     return false;
                }

                if (json_res.value("authorized", true)) {
                    // Update cache
                    std::lock_guard<std::mutex> lock(mutex_);
                    ClientConfig cfg;
                    cfg.client_id = client_id;
                    cfg.api_key = api_key; // Don't strictly need to store this but ok
                    
                    if (json_res.contains("config")) {
                        auto& config_data = json_res["config"];
                        cfg.max_sessions = config_data.value("max_sessions", 1);
                        cfg.priority = config_data.value("priority", "normal");
                        cfg.description = config_data.value("description", "");
                    } else {
                         // Fallback if config is flat in response or missing
                        cfg.max_sessions = json_res.value("max_sessions", 1);
                        cfg.priority = json_res.value("priority", "normal");
                        cfg.description = json_res.value("description", "");
                    }
                    
                    client_cache_[client_id] = cfg;
                    
                    std::cout << "[Auth] Validation success for " << client_id << " (max_sessions: " << cfg.max_sessions << ")" << std::endl;
                    return true;
                }
                
                std::cout << "[Auth] Validation failed (authorized=false) for " << client_id << std::endl;
                return false;

            } catch (const std::exception& e) {
                std::cerr << "[Auth] Error parsing JotaDB response: " << e.what() << std::endl;
                return false;
            }
        } else {
            auto err = res.error();
            std::cerr << "[Auth] JotaDB request failed. Status: " << (res ? std::to_string(res->status) : "Connection Error") 
                      << " Error: " << err << std::endl;
            return false;
        }
    }

    ClientConfig ClientAuth::getClientConfig(const std::string& client_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = client_cache_.find(client_id);
        if (it != client_cache_.end()) {
            return it->second;
        }
        
        // If not in cache, we could try to fetch again, but getClientConfig 
        // is usually called AFTER authenticate.
        // Returning empty default.
        return ClientConfig(); 
    }

    bool ClientAuth::clientExists(const std::string& client_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return client_cache_.find(client_id) != client_cache_.end();
    }

}
