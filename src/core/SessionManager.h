#pragma once

#include "Session.h"
#include "ClientAuth.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace Core {

    class SessionManager {
    public:
        SessionManager(struct llama_model* model, int ctx_size);
        ~SessionManager();

        // Set client auth reference for validation
        void setClientAuth(Server::ClientAuth* auth) { client_auth_ = auth; }

        // Create a new session for a client
        // Returns session_id on success, empty string on failure
        std::string createSession(const std::string& client_id);

        // Get a session by ID
        Session* getSession(const std::string& session_id);

        // Close a specific session
        bool closeSession(const std::string& session_id);

        // Close all sessions for a specific client
        void closeClientSessions(const std::string& client_id);

        // Close all sessions (cleanup on shutdown)
        void closeAllSessions();

        // Get count of active sessions for a client
        int getClientSessionCount(const std::string& client_id) const;

        // Get total session count
        int getTotalSessionCount() const;

    private:
        struct llama_model* model_;
        int ctx_size_;
        Server::ClientAuth* client_auth_ = nullptr;

        std::unordered_map<std::string, std::unique_ptr<Session>> sessions_;
        std::unordered_map<std::string, std::vector<std::string>> client_sessions_; // client_id -> [session_ids]
        
        mutable std::mutex mutex_;

        // Generate unique session ID
        std::string generateSessionId();
    };

}
