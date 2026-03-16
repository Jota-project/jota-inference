#include "WsServer.h"
#include "Logger.h"
#include <nlohmann/json.hpp>

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
    sessionManager_ = std::make_unique<Core::SessionManager>(engine_, ctx_size);
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

    modelResolver_ = std::make_shared<ModelResolver>();
    modelHandler_ = std::make_shared<ModelHandler>(modelResolver_, engine_, inferenceService_.get());

    // Create message dispatcher
    dispatcher_ = std::make_unique<MessageDispatcher>(
        pingHandler_,
        authHandler_,
        sessionHandler_,
        inferenceHandler_,
        metricsHandler_,
        modelHandler_
    );

    IC_LOG_INFO("WsServer initialized", {{"port", port_}});
}

WsServer::~WsServer() {
    // Shutdown services
    if (metricsService_) {
        metricsService_->shutdown();
    }
    if (inferenceService_) {
        inferenceService_->shutdown();
    }

    running_ = false;
    if (watchdogThread_.joinable()) {
        watchdogThread_.join();
    }

    // Cleanup
    // sessionManager_ is unique_ptr, will be deleted automatically

    IC_LOG_INFO("WsServer destroyed");
}

void WsServer::requestShutdown() {
    // Spin briefly if run() hasn't set loop_ yet (signal arriving at startup)
    for (int i = 0; i < 50 && !loop_; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!loop_) {
        IC_LOG_WARN("requestShutdown: loop not ready, ignoring");
        return;
    }

    // loop_->defer runs the callback on the uWS thread — safe to call from any thread.
    // Closing the listen socket reduces num_polls to 0, which exits us_loop_run().
    loop_->defer([this]() {
        IC_LOG_INFO("Graceful shutdown initiated: closing all sessions");
        if (sessionManager_) {
            sessionManager_->closeAllSessions();
        }
        running_ = false;
        if (listenSocket_) {
            us_listen_socket_close(0, listenSocket_);
            listenSocket_ = nullptr;
        }
    });
}

void WsServer::watchdogLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!running_) break;

        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        auto sessions = sessionManager_->getAllSessions();
        for (auto* session : sessions) {
            if (session && session->isGenerating() && !session->isPrefilling()) {
                auto lastActivity = session->getLastActivityMs();
                if (lastActivity > 0 && (now - lastActivity) > watchdog_timeout_sec_ * 1000) {
                    std::string session_id = session->getSessionId();
                    std::string client_id = session->getClientId();

                    IC_LOG_WARN("Watchdog: Session timeout detected, aborting", {
                        {"session_id", session_id},
                        {"timeout_sec", watchdog_timeout_sec_}
                    });

                    // Abort inference backend
                    inferenceService_->abortTask(session_id);

                    // Send error via uWS loop safely
                    if (loop_) {
                        loop_->defer([this, session_id, client_id]() {
                            std::lock_guard<std::mutex> lock(clientsMutex_);
                            for (auto* ws : connectedClients_) {
                                auto* data = ws->getUserData();
                                if (data && data->client_id == client_id) {
                                    json response = {
                                        {"op", Op::ERROR},
                                        {"session_id", session_id},
                                        {"error", Err::SESSION_TIMEOUT}
                                    };
                                    ws->send(response.dump(), uWS::OpCode::TEXT);
                                }
                            }
                        });
                    }
                }
            }
        }
    }
}

void WsServer::run() {
    struct uWS::Loop* loop = uWS::Loop::get();
    this->loop_ = loop;

    running_ = true;
    watchdogThread_ = std::thread([this]() { watchdogLoop(); });

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
                    IC_LOG_WARN("Client connection rejected: Missing authentication headers");
                    res->writeStatus("401 Unauthorized");
                    res->writeHeader("Content-Type", "application/json");
                    res->end("{\"error\":\"Missing X-Client-ID or X-API-Key headers\"}");
                    return;
                }
                
                IC_LOG_INFO("Client connecting", {{"client_id", std::string(client_id)}});
                
                // Authenticate via JotaDB
                if (!clientAuth_.authenticate(std::string(client_id), std::string(api_key))) {
                    IC_LOG_WARN("Client authentication failed", {{"client_id", std::string(client_id)}});
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
                IC_LOG_INFO("Client authenticated", {{"client_id", data->client_id}});
                
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
                if (data->authenticated) {
                    IC_LOG_INFO("Client disconnected", {{"client_id", data->client_id}});
                } else {
                    IC_LOG_INFO("Client disconnected");
                }

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
        .listen(port_, [this](auto* token) {
            if (token) {
                listenSocket_ = token;
                IC_LOG_INFO("WebSocket server listening", {{"port", port_}});
            } else {
                IC_LOG_ERROR("Failed to listen", {{"port", port_}});
            }
        })
        .run();
    
    IC_LOG_INFO("Server stopped");
}

} // namespace Server
