#include "SessionManager.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

namespace Core {

    SessionManager::SessionManager(struct llama_model* model, int ctx_size)
        : model_(model), ctx_size_(ctx_size) {
        if (!model_) {
            throw std::runtime_error("SessionManager requires a valid model");
        }
    }

    SessionManager::~SessionManager() {
        closeAllSessions();
    }

    std::string SessionManager::generateSessionId() {
        // Generate UUID-like session ID
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        static std::uniform_int_distribution<> dis2(8, 11);

        std::stringstream ss;
        ss << "sess_";
        
        for (int i = 0; i < 8; i++) {
            ss << std::hex << dis(gen);
        }
        ss << "_";
        for (int i = 0; i < 4; i++) {
            ss << std::hex << dis(gen);
        }

        return ss.str();
    }

    std::string SessionManager::createSession(const std::string& client_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Check if client exists and get their config
        if (!client_auth_ || !client_auth_->clientExists(client_id)) {
            std::cerr << "Cannot create session: client " << client_id << " not found" << std::endl;
            return "";
        }

        auto client_config = client_auth_->getClientConfig(client_id);

        // Check client's session limit
        auto it = client_sessions_.find(client_id);
        int current_count = (it != client_sessions_.end()) ? it->second.size() : 0;
        
        if (current_count >= client_config.max_sessions) {
            std::cerr << "Client " << client_id << " has reached max sessions limit (" 
                      << client_config.max_sessions << ")" << std::endl;
            return "";
        }

        // Generate unique session ID
        std::string session_id = generateSessionId();
        while (sessions_.find(session_id) != sessions_.end()) {
            session_id = generateSessionId(); // Ensure uniqueness
        }

        try {
            // Create new session
            auto session = std::make_unique<Session>(session_id, client_id, model_, ctx_size_);
            sessions_[session_id] = std::move(session);

            // Track client -> sessions mapping
            client_sessions_[client_id].push_back(session_id);

            std::cout << "Created session " << session_id << " for client " << client_id 
                      << " (" << (current_count + 1) << "/" << client_config.max_sessions << ")" 
                      << std::endl;

            return session_id;

        } catch (const std::exception& e) {
            std::cerr << "Failed to create session: " << e.what() << std::endl;
            return "";
        }
    }

    Session* SessionManager::getSession(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    bool SessionManager::closeSession(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            return false;
        }

        std::string client_id = it->second->getClientId();

        // Remove from sessions map
        sessions_.erase(it);

        // Remove from client_sessions mapping
        auto client_it = client_sessions_.find(client_id);
        if (client_it != client_sessions_.end()) {
            auto& session_list = client_it->second;
            session_list.erase(
                std::remove(session_list.begin(), session_list.end(), session_id),
                session_list.end()
            );

            // Clean up empty client entries
            if (session_list.empty()) {
                client_sessions_.erase(client_it);
            }
        }

        std::cout << "Closed session " << session_id << " for client " << client_id << std::endl;
        return true;
    }

    bool SessionManager::abortSession(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            it->second->abort();
            return true;
        }
        return false;
    }

    void SessionManager::closeClientSessions(const std::string& client_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = client_sessions_.find(client_id);
        if (it == client_sessions_.end()) {
            return;
        }

        // Copy session IDs to avoid iterator invalidation
        std::vector<std::string> session_ids = it->second;

        // Remove all sessions for this client
        for (const auto& session_id : session_ids) {
            sessions_.erase(session_id);
        }

        client_sessions_.erase(it);

        std::cout << "Closed " << session_ids.size() << " session(s) for client " 
                  << client_id << std::endl;
    }

    void SessionManager::closeAllSessions() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int count = sessions_.size();
        sessions_.clear();
        client_sessions_.clear();

        if (count > 0) {
            std::cout << "Closed all " << count << " session(s)" << std::endl;
        }
    }

    int SessionManager::getClientSessionCount(const std::string& client_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = client_sessions_.find(client_id);
        if (it != client_sessions_.end()) {
            return it->second.size();
        }
        return 0;
    }

    int SessionManager::getTotalSessionCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return sessions_.size();
    }

}
