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
                    // Constant-time check for key match just to be safe (though cache should be trusted if owned)
                    if (it->second.api_key == api_key) {
                        std::cout << "[Auth] Cache hit for " << client_id 
                                  << " (Validated " << elapsed << " mins ago)" << std::endl;
                        return true;
                    }
                } else {
                    std::cout << "[Auth] Cache expired for " << client_id 
                              << ". Re-validating..." << std::endl;
                }
            }
        }

        std::cout << "[Auth] Validating " << client_id << " via JotaDB..." << std::endl;

        // 2. Parse and Sanitize URL
        std::string url = jota_db_url_;
        std::string scheme;
        std::string host_port;
        // Logic duplicated, could exact to helper method but for now inline is fine
        if (url.find("http://") == 0) {
            scheme = "http";
            host_port = url.substr(7);
        } else if (url.find("https://") == 0) {
            scheme = "https";
            host_port = url.substr(8);
        } else {
            scheme = "http"; 
            host_port = url;
        }

        size_t path_pos = host_port.find('/');
        std::string domain;
        std::string path_prefix;
        if (path_pos != std::string::npos) {
            domain = host_port.substr(0, path_pos);
            path_prefix = host_port.substr(path_pos);
        } else {
            domain = host_port;
            path_prefix = "";
        }
        
        if (!path_prefix.empty() && path_prefix.back() == '/') {
            path_prefix.pop_back();
        }
        
        std::string base_url = scheme + "://" + domain;

        // 3. Perform HTTP/HTTPS Request
        httplib::Client cli(base_url.c_str());
        cli.set_connection_timeout(2);
        cli.set_read_timeout(3);
        
        #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        if (scheme == "https") {
            cli.enable_server_certificate_verification(false); 
        }
        #endif

        std::string request_path = path_prefix + "/auth/internal";
        
        // Headers construction
        httplib::Headers headers;
        
        // 1. Client Credentials (X-Headers)
        headers.emplace("X-Client-ID", client_id);
        headers.emplace("X-API-Key", api_key);
        
        // 2. Server Identity (Bearer Token)
        if (!jota_db_sk_.empty()) {
            headers.emplace("Authorization", "Bearer " + jota_db_sk_);
        }
        
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
                        // Fallback
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

    bool ClientAuth::verifyConnection() {
        std::string url = jota_db_url_;
        // Basic URL parsing (simplified for brevity, should ideally share logic)
        std::string scheme;
        std::string host_port;
        if (url.find("http://") == 0) {
            scheme = "http";
            host_port = url.substr(7);
        } else if (url.find("https://") == 0) {
            scheme = "https";
            host_port = url.substr(8);
        } else {
            scheme = "http"; 
            host_port = url;
        }

        size_t path_pos = host_port.find('/');
        std::string domain;
        std::string path_prefix;
        if (path_pos != std::string::npos) {
            domain = host_port.substr(0, path_pos);
            path_prefix = host_port.substr(path_pos);
        } else {
            domain = host_port;
            path_prefix = "";
        }
        
        if (!path_prefix.empty() && path_prefix.back() == '/') {
            path_prefix.pop_back();
        }
        
        std::string base_url = scheme + "://" + domain;
        httplib::Client cli(base_url.c_str());
        cli.set_connection_timeout(3);
        cli.set_read_timeout(3);
        
        #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        if (scheme == "https") {
            cli.enable_server_certificate_verification(false); 
        }
        #endif

        std::string request_path = path_prefix + "/health"; 
        
        httplib::Headers headers;
        if (!jota_db_sk_.empty()) {
            headers.emplace("Authorization", "Bearer " + jota_db_sk_);
        } else {
            std::cerr << "[Auth] WARNING: JOTA_DB_SK is empty. Authorization will likely fail." << std::endl;
        }
        
        auto res = cli.Get(request_path.c_str(), headers);
        
        if (res && res->status == 200) {
             std::cout << "[Auth] JotaDB Connection Verified (Heartbeat OK)" << std::endl;
             return true;
        }
        
        if (res) {
             std::cerr << "[Auth] Connection Failed. Status: " << res->status << std::endl;
             if (res->status == 401 || res->status == 403) {
                 std::cerr << "[FATAL] Authorization Error: Check JOTA_DB_SK" << std::endl;
             }
        } else {
             std::cerr << "[Auth] Connection Failed. Network Error: " << res.error() << std::endl;
        }
        
        return false;
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
