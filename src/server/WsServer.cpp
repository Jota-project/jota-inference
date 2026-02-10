#include "WsServer.h"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace Server {

WsServer::WsServer(Core::Engine& engine, Hardware::Monitor& monitor,
                   int port, int ctx_size)
    : engine_(engine)
    , monitor_(monitor)
    , port_(port)
{
    // Client authentication is now handled dynamically via JotaDB
    // No static config loading required

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
            .upgrade = [this](auto* res, auto* req, auto* context) {
                // Extract authentication headers from HTTP request
                auto client_id = req->getHeader("x-client-id");
                auto api_key = req->getHeader("x-api-key");
                
                // Validate presence of required headers
                if (client_id.empty() || api_key.empty()) {
                    std::cout << "Client connection rejected: Missing authentication headers" << std::endl;
                    res->writeStatus("401 Unauthorized");
                    res->writeHeader("Content-Type", "application/json");
                    res->end("{\"error\":\"Missing X-Client-ID or X-API-Key headers\"}");
                    return;
                }
                
                std::cout << "Client connecting with ID: " << client_id << std::endl;
                
                // Authenticate via JotaDB
                if (!clientAuth_.authenticate(std::string(client_id), std::string(api_key))) {
                    std::cout << "Client authentication failed: " << client_id << std::endl;
                    res->writeStatus("401 Unauthorized");
                    res->writeHeader("Content-Type", "application/json");
                    res->end("{\"error\":\"Invalid credentials\"}");
                    return;
                }
                
                // Authentication successful - prepare user data
                PerSocketData userData;
                userData.authenticated = true;
                userData.client_id = std::string(client_id);
                
                // Complete the WebSocket upgrade
                res->template upgrade<PerSocketData>(
                    std::move(userData),
                    req->getHeader("sec-websocket-key"),
                    req->getHeader("sec-websocket-protocol"),
                    req->getHeader("sec-websocket-extensions"),
                    context
                );
            },
            .open = [this](auto* ws) {
                auto* data = ws->getUserData();
                
                // Client is already authenticated via upgrade handler
                std::cout << "Client authenticated: " << data->client_id << std::endl;
                
                auto config = clientAuth_.getClientConfig(data->client_id);
                json response = {
                    {"op", Op::AUTH_SUCCESS},
                    {"client_id", data->client_id},
                    {"max_sessions", config.max_sessions}
                };
                ws->send(response.dump(), uWS::OpCode::TEXT);
                
                // Track connection
                {
                    std::lock_guard<std::mutex> lock(clientsMutex_);
                    connectedClients_.insert(ws);
                }
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
