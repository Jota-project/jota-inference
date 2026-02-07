#include "WsServer.h"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace Server {

WsServer::WsServer(Core::Engine& engine, Hardware::Monitor& monitor,
                   const std::string& clientConfigPath, int port)
    : engine_(engine)
    , monitor_(monitor)
    , port_(port)
{
    // Load client authentication
    if (!clientAuth_.loadConfig(clientConfigPath)) {
        throw std::runtime_error("Failed to load client configuration from: " + clientConfigPath);
    }

    // Create session manager
    sessionManager_ = new Core::SessionManager(engine_.getModel(), 512);  // 512 ctx_size
    sessionManager_->setClientAuth(&clientAuth_);

    // Create services
    inferenceService_ = std::make_unique<InferenceService>(sessionManager_, 4); // 4 worker threads
    metricsService_ = std::make_unique<MetricsService>(monitor_, sessionManager_, inferenceService_.get());

    // Create handlers
    authHandler_ = std::make_shared<AuthHandler>(clientAuth_);
    sessionHandler_ = std::make_shared<SessionHandler>(sessionManager_);
    inferenceHandler_ = std::make_shared<InferenceHandler>(inferenceService_.get());
    metricsHandler_ = std::make_shared<MetricsHandler>();

    // Create message dispatcher
    dispatcher_ = std::make_unique<MessageDispatcher>(
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
    delete sessionManager_;

    std::cout << "WsServer destroyed" << std::endl;
}

void WsServer::run() {
    struct uWS::Loop* loop = uWS::Loop::get();
    this->loop_ = loop;

    // Start metrics service with broadcast callback
    metricsService_->start([this](const std::string& metricsJson) {
        // Get current subscribers from metrics handler
        auto subscribers = metricsHandler_->getSubscribers();

        // Broadcast to all subscribers (must use loop->defer for thread safety)
        loop_->defer([this, subscribers, metricsJson]() {
            for (auto* ws : subscribers) {
                ws->send(metricsJson, uWS::OpCode::TEXT);
            }
        });
    });

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
