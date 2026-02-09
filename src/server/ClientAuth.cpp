#include "ClientAuth.h"
#include "EnvLoader.h"
#include <iostream>
#include <cstdlib>
#include <httplib.h>

namespace Server {

    ClientAuth::ClientAuth() {
        initJotaDB();
    }

    void ClientAuth::initJotaDB() {
        // Load configuration using EnvLoader
        jota_db_url_ = Core::EnvLoader::get("JOTA_DB_URL", "https://green-house.local/api/db");
        jota_db_usr_ = Core::EnvLoader::get("JOTA_DB_USR", "");
        jota_db_sk_ = Core::EnvLoader::get("JOTA_DB_SK", "");
        

        std::cout << "[Auth] JotaDB URL configured: " << jota_db_url_ << std::endl;
        if (jota_db_sk_.empty() || jota_db_usr_.empty()) {
            std::cerr << "[Auth] WARNING: JOTA_DB_SK or JOTA_DB_USR is not set! JotaDB authentication requests may fail." << std::endl;
        }
    }

    bool ClientAuth::authenticate(const std::string& client_id, const std::string& api_key) {
        // 1. Check Cache with TTL
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = client_cache_.find(client_id);
            if (it != client_cache_.end()) {
                auto now = std::chrono::system_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - it->second.last_validated).count();
                
                if (elapsed < 15) {
                    // Check if the provided key matches the cached key
                    if (it->second.api_key == api_key) {
                        return true;
                    }
                    // If key doesn't match, we fall through to DB to check if it was rotated
                } else {
                    std::cout << "[Auth] Cache expired for " << client_id 
                              << " (" << elapsed << " mins). Re-validating..." << std::endl;
                }
            }
        }

        std::cout << "[Auth] Validating " << client_id << " via JotaDB..." << std::endl;

        // 2. Parse and Sanitize URL
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
            // Default to http if no scheme provided, though warning was logged
            scheme = "http"; 
            host_port = url;
        }

        size_t path_pos = host_port.find('/');
        std::string domain;
        if (path_pos != std::string::npos) {
            domain = host_port.substr(0, path_pos);
            path_prefix = host_port.substr(path_pos);
        } else {
            domain = host_port;
            path_prefix = "";
        }
        
        // Remove trailing slash from path_prefix
        if (!path_prefix.empty() && path_prefix.back() == '/') {
            path_prefix.pop_back();
        }
        
        std::string base_url = scheme + "://" + domain;

        // 3. Perform HTTP/HTTPS Request
        httplib::Client cli(base_url.c_str());
        cli.set_connection_timeout(2); // 2 seconds timeout
        cli.set_read_timeout(3);       // 3 seconds read timeout
        
        #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        if (scheme == "https") {
            cli.enable_server_certificate_verification(false); // Validating self-signed certs might require CA setup
        }
        #endif

        std::string request_path = path_prefix + "/auth/internal";

        
        // Use Headers for authentication (Security Best Practice)
        httplib::Headers headers = {
            {"X-Client-ID", client_id},
            {"X-API-Key", api_key}
        };

        auto res = cli.Get(request_path.c_str(), headers);

        if (res && res->status == 200) {
            try {
                auto json_res = json::parse(res->body);
                
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
                    cfg.api_key = api_key;
                    cfg.last_validated = std::chrono::system_clock::now();
                    
                    if (json_res.contains("config")) {
                        auto& config_data = json_res["config"];
                        cfg.max_sessions = config_data.value("max_sessions", 1);
                        cfg.priority = config_data.value("priority", "normal");
                        cfg.description = config_data.value("description", "");
                    } else {
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
        
        return ClientConfig(); 
    }

    bool ClientAuth::clientExists(const std::string& client_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return client_cache_.find(client_id) != client_cache_.end();
    }

}
