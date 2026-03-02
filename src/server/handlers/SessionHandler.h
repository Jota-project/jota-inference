#pragma once

#include "../RequestContext.h"
#include "../../core/SessionManager.h"
#include "Logger.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Server {

/**
 * SessionHandler - Handles session lifecycle operations
 * 
 * Processes Op::CREATE_SESSION and Op::CLOSE_SESSION requests.
 */
class SessionHandler {
public:
    explicit SessionHandler(Core::SessionManager* sessionManager)
        : sessionManager_(sessionManager)
    {
        if (!sessionManager_) {
            throw std::invalid_argument("SessionManager cannot be null");
        }
    }
    
    /**
     * Handle session creation request
     * @param ctx Request context
     * @param payload JSON payload (empty for create)
     */
    void handleCreate(RequestContext& ctx, const json& /*payload*/) {
        auto* data = ctx.getData();
        
        // Check authentication
        if (!data->authenticated) {
            json response = {
                {"op", Op::SESSION_ERROR},
                {"error", "Not authenticated"}
            };
            ctx.send(response);
            return;
        }
        
        // Create session
        auto session_id = sessionManager_->createSession(data->client_id);
        
        if (!session_id.empty()) {
            auto* session = sessionManager_->getSession(session_id);
            std::string model_id = session ? session->getModelId() : "unknown";

            json response = {
                {"op", Op::SESSION_CREATED},
                {"session_id", session_id},
                {"model_id", model_id}
            };
            ctx.send(response);
            
            IC_LOG_INFO("Session created", {
                {"session_id", session_id},
                {"client_id", data->client_id}
            });
        } else {
            json response = {
                {"op", Op::SESSION_ERROR},
                {"error", "Failed to create session (limit reached)"}
            };
            ctx.send(response);
        }
    }
    
    /**
     * Handle session close request
     * @param ctx Request context
     * @param payload JSON payload with session_id
     */
    void handleClose(RequestContext& ctx, const json& payload) {
        auto* data = ctx.getData();
        
        // Check authentication
        if (!data->authenticated) {
            json response = {
                {"op", Op::ERROR},
                {"error", "Not authenticated"}
            };
            ctx.send(response);
            return;
        }
        
        // Extract session_id
        if (!payload.contains("session_id")) {
            json response = {
                {"op", Op::ERROR},
                {"error", "Missing session_id"}
            };
            ctx.send(response);
            return;
        }
        
        std::string session_id = payload["session_id"];
        
        // Verify ownership - get session and check client_id
        auto* session = sessionManager_->getSession(session_id);
        if (!session || session->getClientId() != data->client_id) {
            json response = {
                {"op", Op::ERROR},
                {"error", "Session not found or access denied"}
            };
            ctx.send(response);
            return;
        }
        
        // Close session
        if (sessionManager_->closeSession(session_id)) {
            json response = {
                {"op", Op::SESSION_CLOSED},
                {"session_id", session_id}
            };
            ctx.send(response);
            
            IC_LOG_INFO("Session closed", {{"session_id", session_id}});
        } else {
            json response = {
                {"op", Op::ERROR},
                {"error", "Failed to close session"}
            };
            ctx.send(response);
        }
    }

    /**
     * Handle set_context request
     * @param ctx Request context
     * @param payload JSON payload with session_id and context
     */
    void handleSetContext(RequestContext& ctx, const json& payload) {
        auto* data = ctx.getData();
        
        // Check authentication
        if (!data->authenticated) {
            json response = {
                {"op", Op::CONTEXT_ERROR},
                {"error", "Not authenticated"}
            };
            ctx.send(response);
            return;
        }
        
        // Extract session_id
        if (!payload.contains("session_id")) {
            json response = {
                {"op", Op::CONTEXT_ERROR},
                {"error", "Missing session_id"}
            };
            ctx.send(response);
            return;
        }
        
        std::string session_id = payload["session_id"];
        
        // Verify ownership
        auto* session = sessionManager_->getSession(session_id);
        if (!session || session->getClientId() != data->client_id) {
            json response = {
                {"op", Op::CONTEXT_ERROR},
                {"session_id", session_id},
                {"error", "Session not found or access denied"}
            };
            ctx.send(response);
            return;
        }
        
        // Validate context field exists
        if (!payload.contains("context")) {
            json response = {
                {"op", Op::CONTEXT_ERROR},
                {"session_id", session_id},
                {"error", "Missing context field"}
            };
            ctx.send(response);
            return;
        }
        
        // Parse and set context
        auto sessionContext = parseContext(payload);
        
        if (sessionManager_->setSessionContext(session_id, std::move(sessionContext))) {
            json response = {
                {"op", Op::CONTEXT_SET},
                {"session_id", session_id}
            };
            ctx.send(response);
            
            IC_LOG_INFO("Context set", {
                {"session_id", session_id},
                {"client_id", data->client_id}
            });
        } else {
            json response = {
                {"op", Op::CONTEXT_ERROR},
                {"session_id", session_id},
                {"error", "Failed to set context"}
            };
            ctx.send(response);
        }
    }

private:
    Core::SessionManager* sessionManager_;
};

} // namespace Server
