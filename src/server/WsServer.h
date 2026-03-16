#pragma once

#include <App.h> // uWebSockets
#include <atomic>
#include <thread>
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
#include "handlers/ModelHandler.h"
#include "../core/Engine.h"
#include "../core/SessionManager.h"
#include "../hardware/Monitor.h"
#include "../config/AppConfig.h"

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
             int port = Core::AppConfig::get().server_port, 
             int ctx_size = Core::AppConfig::get().ctx_size);
    ~WsServer();

    // Start the server loop (blocking)
    void run();

    // Request a graceful shutdown from any thread (signal-safe via atomic flag).
    // Stops the uWS event loop after closing all sessions.
    void requestShutdown();

private:
    void watchdogLoop();

    // Watchdog config
    std::atomic<bool> running_{false};
    std::thread watchdogThread_;
    int watchdog_timeout_sec_ = 10;

    // Core dependencies
    Core::Engine& engine_;
    std::unique_ptr<Core::SessionManager> sessionManager_;
    ClientAuth clientAuth_;
    Hardware::Monitor& monitor_;
    int port_;
    
    // uWebSockets loop
    uWS::Loop* loop_ = nullptr;
    us_listen_socket_t* listenSocket_ = nullptr;
    
    // Services
    std::unique_ptr<InferenceService> inferenceService_;
    std::unique_ptr<MetricsService> metricsService_;
    
    // Handlers (shared_ptr for MessageDispatcher sharing)
    std::shared_ptr<PingHandler> pingHandler_;
    std::shared_ptr<AuthHandler> authHandler_;
    std::shared_ptr<SessionHandler> sessionHandler_;
    std::shared_ptr<InferenceHandler> inferenceHandler_;
    std::shared_ptr<MetricsHandler> metricsHandler_;
    std::shared_ptr<ModelResolver> modelResolver_;
    std::shared_ptr<ModelHandler> modelHandler_;
    
    // Message dispatcher
    std::unique_ptr<MessageDispatcher> dispatcher_;
    
    // Connection tracking
    std::set<uWS::WebSocket<false, true, PerSocketData>*> connectedClients_;
    std::mutex clientsMutex_;
};

} // namespace Server
