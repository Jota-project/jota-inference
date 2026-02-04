#include "ClientAuth.h"
#include <fstream>
#include <iostream>

namespace Server {

    ClientAuth::ClientAuth() {
        // Constructor
    }

    bool ClientAuth::loadConfig(const std::string& configPath) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            std::ifstream file(configPath);
            if (!file.is_open()) {
                std::cerr << "Failed to open client config file: " << configPath << std::endl;
                return false;
            }

            json config;
            file >> config;

            if (!config.contains("clients")) {
                std::cerr << "Invalid config format: missing 'clients' key" << std::endl;
                return false;
            }

            clients_.clear();

            for (auto& [client_id, client_data] : config["clients"].items()) {
                ClientConfig cfg;
                cfg.client_id = client_id;
                
                if (client_data.contains("api_key")) {
                    cfg.api_key = client_data["api_key"].get<std::string>();
                } else {
                    std::cerr << "Client " << client_id << " missing api_key" << std::endl;
                    continue;
                }

                if (client_data.contains("max_sessions")) {
                    cfg.max_sessions = client_data["max_sessions"].get<int>();
                }
                
                if (client_data.contains("priority")) {
                    cfg.priority = client_data["priority"].get<std::string>();
                }
                
                if (client_data.contains("description")) {
                    cfg.description = client_data["description"].get<std::string>();
                }

                clients_[client_id] = cfg;
                std::cout << "Loaded client: " << client_id 
                          << " (max_sessions: " << cfg.max_sessions << ")" << std::endl;
            }

            std::cout << "Loaded " << clients_.size() << " client(s) from config" << std::endl;
            return true;

        } catch (const std::exception& e) {
            std::cerr << "Error loading client config: " << e.what() << std::endl;
            return false;
        }
    }

    bool ClientAuth::authenticate(const std::string& client_id, const std::string& api_key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = clients_.find(client_id);
        if (it == clients_.end()) {
            return false;
        }

        // Constant-time comparison to prevent timing attacks
        const std::string& stored_key = it->second.api_key;
        if (stored_key.length() != api_key.length()) {
            return false;
        }

        volatile unsigned char result = 0;
        for (size_t i = 0; i < stored_key.length(); i++) {
            result |= stored_key[i] ^ api_key[i];
        }

        return result == 0;
    }

    ClientConfig ClientAuth::getClientConfig(const std::string& client_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            return it->second;
        }
        
        return ClientConfig(); // Return empty config if not found
    }

    bool ClientAuth::clientExists(const std::string& client_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return clients_.find(client_id) != clients_.end();
    }

}
