#pragma once

#include "../RequestContext.h"
#include "../Protocol.h"
#include <nlohmann/json.hpp>
#include <chrono>

using json = nlohmann::json;

namespace Server {

/**
 * PingHandler - Handles HELLO/ping operations
 * 
 * Processes Op::HELLO requests without requiring authentication.
 * Allows clients to check server availability and status.
 */
class PingHandler {
public:
    PingHandler()
        : startTime_(std::chrono::steady_clock::now())
    {}
    
    /**
     * Handle HELLO/ping request
     * @param ctx Request context
     * @param payload JSON payload (unused, but kept for consistency)
     */
    void handle(RequestContext& ctx, const json& /* payload */) {
        // Calculate uptime
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - startTime_).count();
        
        // Send hello response
        json response = {
            {"op", Op::HELLO},
            {"status", "ready"},
            {"message", "Server is available"},
            {"uptime_seconds", uptime},
            {"requires_auth", true}
        };
        
        ctx.send(response);
    }

private:
    std::chrono::steady_clock::time_point startTime_;
};

} // namespace Server
