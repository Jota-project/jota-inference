#pragma once

#include <App.h> // uWebSockets
#include "Protocol.h"
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Server {

struct PerSocketData {
    std::string client_id;
    bool authenticated = false;
};

/**
 * RequestContext - Abstraction over WebSocket operations
 * 
 * Encapsulates uWS::Loop::defer pattern for thread-safe sends.
 * Provides clean interface for handlers without exposing uWebSockets details.
 */
class RequestContext {
public:
    RequestContext(
        uWS::WebSocket<false, true, PerSocketData>* ws,
        uWS::Loop* loop
    )
        : ws_(ws)
        , loop_(loop)
    {}
    
    /**
     * Send JSON message to client (thread-safe via loop->defer)
     * Can be called from any thread
     */
    void send(const json& message) {
        std::string msg = message.dump();
        loop_->defer([this, msg]() {
            ws_->send(msg, uWS::OpCode::TEXT);
        });
    }
    
    /**
     * Send raw string message to client (thread-safe via loop->defer)
     * Can be called from any thread
     */
    void sendRaw(const std::string& message) {
        loop_->defer([this, message]() {
            ws_->send(message, uWS::OpCode::TEXT);
        });
    }
    
    /**
     * Get mutable access to per-socket data
     */
    PerSocketData* getData() {
        return ws_->getUserData();
    }
    
    /**
     * Get const access to per-socket data
     */
    const PerSocketData* getData() const {
        return ws_->getUserData();
    }
    
    /**
     * Get direct socket access for advanced operations
     * Use with caution - prefer send() methods
     */
    uWS::WebSocket<false, true, PerSocketData>* getRawSocket() {
        return ws_;
    }
    
    /**
     * Get loop for manual defer operations
     * Use with caution - prefer send() methods
     */
    uWS::Loop* getLoop() {
        return loop_;
    }

private:
    uWS::WebSocket<false, true, PerSocketData>* ws_;
    uWS::Loop* loop_;
};

} // namespace Server
