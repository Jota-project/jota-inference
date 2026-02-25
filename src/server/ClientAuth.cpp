#include "ClientAuth.h"
#include "EnvLoader.h"
#include "Logger.h"
#include <cstdlib>
#include <httplib.h>

namespace Server {

    ClientAuth::ClientAuth() {
        initJotaDB();
    }

    void ClientAuth::initJotaDB() {
        // Load configuration using EnvLoader
        jota_db_url_ = Core::EnvLoader::get("JOTA_DB_URL", "https://green-house.local/api/db");
        jota_db_sk_ = Core::EnvLoader::get("JOTA_DB_SK", "");
        

        IC_LOG_INFO("JotaDB URL configured", {{"url", jota_db_url_}});
        if (jota_db_sk_.empty()) {
            IC_LOG_WARN("JOTA_DB_SK or JOTA_DB_USR is not set. JotaDB auth requests may fail.");
        }

        verifyConnection();
    }

    void ClientAuth::parseUrl(const std::string& url, std::string& scheme, std::string& domain, std::string& path_prefix) const {
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
                        IC_LOG_DEBUG("Auth cache hit", {{"client_id", client_id}, {"age_min", (int)elapsed}});
                        return true;
                    }
                } else {
                    IC_LOG_DEBUG("Auth cache expired, re-validating", {{"client_id", client_id}});
                }
            }
        }

        IC_LOG_DEBUG("Validating client via JotaDB", {{"client_id", client_id}});

        // 2. Parse and Sanitize URL
        std::string url = jota_db_url_;
        std::string scheme, domain, path_prefix;
        parseUrl(url, scheme, domain, path_prefix);
        
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
        headers.emplace("X-Service-ID", client_id);
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
                     IC_LOG_WARN("Auth validation failed", {
                         {"client_id", client_id},
                         {"error", json_res["error"].dump()}
                     });
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
                    
                    IC_LOG_INFO("Auth validation success", {
                        {"client_id", client_id},
                        {"max_sessions", cfg.max_sessions}
                    });
                    return true;
                }
                
                IC_LOG_WARN("Auth validation failed (authorized=false)", {{"client_id", client_id}});
                return false;

            } catch (const std::exception& e) {
                IC_LOG_ERROR("Error parsing JotaDB response", {{"error", std::string(e.what())}});
                return false;
            }
        } else {
            auto err = res.error();
            IC_LOG_ERROR("JotaDB request failed", {
                {"status", res ? std::to_string(res->status) : "Connection Error"},
                {"error", std::to_string(static_cast<int>(err))}
            });
            return false;
        }
    }

    bool ClientAuth::verifyConnection() {


        if (authenticate(Core::EnvLoader::get("INFERENCE_CENTER_ID"), Core::EnvLoader::get("INFERENCE_CENTER_SK"))) {

            IC_LOG_INFO("JotaDB Connection Verified (Heartbeat OK)");
            return true;
        }
        // std::string url = jota_db_url_;
        // std::string scheme, domain, path_prefix;
        // parseUrl(url, scheme, domain, path_prefix);
        
        // std::string base_url = scheme + "://" + domain;
        // httplib::Client cli(base_url.c_str());
        // cli.set_connection_timeout(3);
        // cli.set_read_timeout(3);
        
        // #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        // if (scheme == "https") {
        //     cli.enable_server_certificate_verification(false); 
        // }
        // #endif

        // std::string request_path = path_prefix + "/health"; 
        
        // // No auth needed for heartbeat
        // httplib::Headers headers;
        // if (!jota_db_sk_.empty()) {
        //     headers.emplace("Authorization", "Bearer " + jota_db_sk_);
        // } else {
        //     IC_LOG_WARN("JOTA_DB_SK is empty. Authorization will likely fail.");
        // }
        
        // auto res = cli.Get(request_path.c_str());
        
        // if (res && res->status == 200) {
        //      IC_LOG_INFO("JotaDB Connection Verified (Heartbeat OK)");
        //      return true;
        // }
        
        // if (res) {
        //      IC_LOG_ERROR("JotaDB connection failed", {{"status", res->status}});
        //      if (res->status == 401 || res->status == 403) {
        //          IC_LOG_ERROR("Authorization Error: Check JOTA_DB_SK");
        //      }
        // } else {
        //      IC_LOG_ERROR("JotaDB connection failed", {{"error", std::to_string(static_cast<int>(res.error()))}});
        // }
        
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
