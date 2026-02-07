#pragma once

#include "RequestContext.h"
#include "ClientAuth.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Server {

/**
 * AuthHandler - Handles authentication operations
 * 
 * Processes Op::AUTH requests and validates credentials.
 */
class AuthHandler {
public:
    explicit AuthHandler(ClientAuth& clientAuth)
        : clientAuth_(clientAuth)
    {}
    
    /**
     * Handle authentication request
     * @param ctx Request context
     * @param payload JSON payload with client_id and api_key
     */
    void handle(RequestContext& ctx, const json& payload) {
        // Extract credentials
        if (!payload.contains("client_id") || !payload.contains("api_key")) {
            json response = {
                {"op", Op::AUTH_FAILED},
                {"reason", "Missing client_id or api_key"}
            };
            ctx.send(response);
            return;
        }
        
        std::string client_id = payload["client_id"];
        std::string api_key = payload["api_key"];
        
        // Validate credentials
        if (clientAuth_.authenticate(client_id, api_key)) {
            // Authentication successful
            auto* data = ctx.getData();
            data->authenticated = true;
            data->client_id = client_id;
            
            auto config = clientAuth_.getClientConfig(client_id);
            json response = {
                {"op", Op::AUTH_SUCCESS},
                {"client_id", client_id},
                {"max_sessions", config.max_sessions}
            };
            ctx.send(response);
        } else {
            // Authentication failed
            json response = {
                {"op", Op::AUTH_FAILED},
                {"reason", "Invalid credentials"}
            };
            ctx.send(response);
        }
    }

private:
    ClientAuth& clientAuth_;
};

} // namespace Server
