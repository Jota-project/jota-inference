#include "WsServer.h"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace Server {

WsServer::WsServer(Core::Engine& engine, Hardware::Monitor& monitor,
                   const std::string& clientConfigPath, int port, int ctx_size)
    : engine_(engine)
    , monitor_(monitor)
    , port_(port)
{
    // Load client authentication
    if (!clientAuth_.loadConfig(clientConfigPath)) {
        throw std::runtime_error("Failed to load client configuration from: " + clientConfigPath);
    }

    // Create session manager
    sessionManager_ = std::make_unique<Core::SessionManager>(engine_.getModel(), ctx_size);
    sessionManager_->setClientAuth(&clientAuth_);

    // Create services
    inferenceService_ = std::make_unique<InferenceService>(sessionManager_.get(), 4); // 4 worker threads
    metricsService_ = std::make_unique<MetricsService>(monitor_, sessionManager_.get(), inferenceService_.get());

    // Create handlers
    pingHandler_ = std::make_shared<PingHandler>();
    authHandler_ = std::make_shared<AuthHandler>(clientAuth_);
    sessionHandler_ = std::make_shared<SessionHandler>(sessionManager_.get());
    inferenceHandler_ = std::make_shared<InferenceHandler>(inferenceService_.get());
    metricsHandler_ = std::make_shared<MetricsHandler>();

    // Create message dispatcher
    dispatcher_ = std::make_unique<MessageDispatcher>(
        pingHandler_,
        authHandler_,
        sessionHandler_,
        inferenceHandler_,
        metricsHandler_
    );

    std::cout << "WsServer initialized on port " << port_ << std::endl;
}

WsServer::~WsServer() {
    // Shutdown services
    if (metricsService_) {
        metricsService_->shutdown();
    }
    if (inferenceService_) {
        inferenceService_->shutdown();
    }

    // Cleanup
    // sessionManager_ is unique_ptr, will be deleted automatically

    std::cout << "WsServer destroyed" << std::endl;
}

void WsServer::run() {
    struct uWS::Loop* loop = uWS::Loop::get();
    this->loop_ = loop;

    // Start metrics service with broadcast callback
    metricsService_->setMetricsHandler(metricsHandler_.get());
    metricsService_->setEventLoop(loop_);
    metricsService_->start();

    uWS::App()
        .ws<PerSocketData>("/*", {
            .open = [this](auto* ws) {
                std::cout << "Client connected (unauthenticated)" << std::endl;

                // Track connection
                {
                    std::lock_guard<std::mutex> lock(clientsMutex_);
                    connectedClients_.insert(ws);
                }

                // Send hello message
                json hello = {
                    {"op", Op::HELLO},
                    {"status", "ready"},
                    {"message", "Please authenticate with AUTH operation"}
                };
                ws->send(hello.dump(), uWS::OpCode::TEXT);
            },
            .message = [this](auto* ws, std::string_view message, uWS::OpCode) {
                // Create request context
                RequestContext ctx(ws, loop_);

                // Delegate to dispatcher
                dispatcher_->dispatch(ctx, std::string(message));
            },
            .close = [this](auto* ws, int, std::string_view) {
                auto* data = ws->getUserData();
                std::cout << "Client disconnected";
                if (data->authenticated) {
                    std::cout << ": " << data->client_id;
                }
                std::cout << std::endl;

                // Remove from metrics subscribers
                // SAFETY: Must remove immediately to prevent use-after-free in MetricsService broadcast.
                // MetricsService::metricsLoop uses loop->defer which runs on this same thread.
                // By removing here, the next defer execution will not see this socket.
                metricsHandler_->removeSubscriber(ws);

                // Remove from connected clients
                {
                    std::lock_guard<std::mutex> lock(clientsMutex_);
                    connectedClients_.erase(ws);
                }
            }
        })
        .listen(port_, [this](auto* listenSocket) {
            if (listenSocket) {
                std::cout << "WebSocket server listening on port " << port_ << std::endl;
            } else {
                std::cerr << "Failed to listen on port " << port_ << std::endl;
            }
        })
        .run();
    
    std::cout << "Server stopped" << std::endl;
}

} // namespace Server
