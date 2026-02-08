#pragma once

#include <App.h> // uWebSockets
#include <memory>
#include <set>
#include <mutex>
#include "Protocol.h"
#include "ClientAuth.h"
#include "RequestContext.h"
#include "MessageDispatcher.h"
#include "services/InferenceService.h"
#include "services/MetricsService.h"
#include "handlers/PingHandler.h"
#include "handlers/AuthHandler.h"
#include "handlers/SessionHandler.h"
#include "handlers/InferenceHandler.h"
#include "handlers/MetricsHandler.h"
#include "../core/Engine.h"
#include "../core/SessionManager.h"
#include "../hardware/Monitor.h"

namespace Server {

/**
 * WsServer - Minimal WebSocket server (network layer only)
 * 
 * Responsibilities:
 * - uWebSockets lifecycle (open, message, close)
 * - Connection tracking
 * - Delegate message handling to MessageDispatcher
 * - Coordinate services (Inference, Metrics)
 */
class WsServer {
public:
    WsServer(Core::Engine& engine, Hardware::Monitor& monitor, 
             const std::string& clientConfigPath, int port = 3000, int ctx_size = 512);
    ~WsServer();

    // Start the server loop (blocking)
    void run();

private:
    // Core dependencies
    Core::Engine& engine_;
    std::unique_ptr<Core::SessionManager> sessionManager_;
    ClientAuth clientAuth_;
    Hardware::Monitor& monitor_;
    int port_;
    
    // uWebSockets loop
    uWS::Loop* loop_ = nullptr;
    
    // Services
    std::unique_ptr<InferenceService> inferenceService_;
    std::unique_ptr<MetricsService> metricsService_;
    
    // Handlers (shared_ptr for MessageDispatcher sharing)
    std::shared_ptr<PingHandler> pingHandler_;
    std::shared_ptr<AuthHandler> authHandler_;
    std::shared_ptr<SessionHandler> sessionHandler_;
    std::shared_ptr<InferenceHandler> inferenceHandler_;
    std::shared_ptr<MetricsHandler> metricsHandler_;
    
    // Message dispatcher
    std::unique_ptr<MessageDispatcher> dispatcher_;
    
    // Connection tracking
    std::set<uWS::WebSocket<false, true, PerSocketData>*> connectedClients_;
    std::mutex clientsMutex_;
};

} // namespace Server
