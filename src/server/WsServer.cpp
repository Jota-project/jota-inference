#include "WsServer.h"
#include <chrono>

namespace {
    // Helper: Validate and clean UTF-8 string to prevent JSON serialization errors
    std::string sanitizeUtf8(const std::string& input) {
        std::string output;
        output.reserve(input.size());
        
        for (size_t i = 0; i < input.size(); ) {
            unsigned char c = input[i];
            if (c < 0x80) {
                // ASCII
                output += c;
                i++;
            } else if ((c & 0xE0) == 0xC0 && i + 1 < input.size()) {
                // 2-byte UTF-8
                output += input.substr(i, 2);
                i += 2;
            } else if ((c & 0xF0) == 0xE0 && i + 2 < input.size()) {
                // 3-byte UTF-8
                output += input.substr(i, 3);
                i += 3;
            } else if ((c & 0xF8) == 0xF0 && i + 3 < input.size()) {
                // 4-byte UTF-8
                output += input.substr(i, 4);
                i += 4;
            } else {
                // Invalid UTF-8, skip this byte
                i++;
            }
        }
        return output;
    }
}

namespace Server {

    WsServer::WsServer(Core::Engine& engine, Hardware::Monitor& monitor,
                       const std::string& clientConfigPath, int port) 
        : engine(engine), monitor(monitor), port(port) {
        
        // Load client authentication config
        if (!clientAuth.loadConfig(clientConfigPath)) {
            throw std::runtime_error("Failed to load client configuration from: " + clientConfigPath);
        }

        // Create SessionManager
        sessionManager = new Core::SessionManager(engine.getModel(), engine.getCtxSize());
        sessionManager->setClientAuth(&clientAuth);

        // Start worker threads (thread pool for parallel processing)
        int numWorkers = 4;
        for (int i = 0; i < numWorkers; i++) {
            workerThreads.emplace_back(&WsServer::workerLoop, this);
        }
        
        // Start metrics broadcasting thread
        metricsThread = std::thread(&WsServer::metricsLoop, this);

        std::cout << "WsServer initialized with " << numWorkers << " worker threads" << std::endl;
    }

    WsServer::~WsServer() {
        running = false;
        queueCv.notify_all();
        
        for (auto& thread : workerThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        if (metricsThread.joinable()) {
            metricsThread.join();
        }

        if (sessionManager) {
            delete sessionManager;
        }
    }

    void WsServer::run() {
        struct uWS::Loop* loop = uWS::Loop::get(); 
        this->loop = loop;

        uWS::App()
            .ws<PerSocketData>("/*", {
                .open = [this](auto* ws) {
                    std::cout << "Client connected (unauthenticated)" << std::endl;
                    
                    // Add to connected clients
                    {
                        std::lock_guard<std::mutex> lock(clientsMutex);
                        connectedClients.insert(ws);
                    }
                    
                    json hello = {
                        {"op", Op::HELLO},
                        {"status", "ready"},
                        {"message", "Please authenticate with AUTH operation"}
                    };
                    ws->send(hello.dump(), uWS::OpCode::TEXT);
                },
                .message = [this](auto* ws, std::string_view message, uWS::OpCode) {
                    try {
                        auto j = json::parse(message);
                        if (!j.contains("op")) return;

                        std::string op = j["op"];
                        auto* data = ws->getUserData();
                        
                        // AUTH operation - always allowed
                        if (op == Op::AUTH) {
                            if (!j.contains("client_id") || !j.contains("api_key")) {
                                json response = {
                                    {"op", Op::AUTH_FAILED},
                                    {"reason", "Missing client_id or api_key"}
                                };
                                ws->send(response.dump(), uWS::OpCode::TEXT);
                                return;
                            }

                            std::string client_id = j["client_id"];
                            std::string api_key = j["api_key"];

                            if (clientAuth.authenticate(client_id, api_key)) {
                                data->client_id = client_id;
                                data->authenticated = true;

                                auto config = clientAuth.getClientConfig(client_id);
                                json response = {
                                    {"op", Op::AUTH_SUCCESS},
                                    {"client_id", client_id},
                                    {"max_sessions", config.max_sessions}
                                };
                                ws->send(response.dump(), uWS::OpCode::TEXT);
                                std::cout << "Client authenticated: " << client_id << std::endl;
                            } else {
                                json response = {
                                    {"op", Op::AUTH_FAILED},
                                    {"reason", "Invalid credentials"}
                                };
                                ws->send(response.dump(), uWS::OpCode::TEXT);
                                std::cout << "Authentication failed for: " << client_id << std::endl;
                            }
                            return;
                        }

                        // All other operations require authentication
                        if (!data->authenticated) {
                            json response = {
                                {"op", Op::ERROR},
                                {"message", "Not authenticated. Please send AUTH operation first."}
                            };
                            ws->send(response.dump(), uWS::OpCode::TEXT);
                            return;
                        }

                        // CREATE_SESSION
                        if (op == Op::CREATE_SESSION) {
                            std::string session_id = sessionManager->createSession(data->client_id);
                            
                            if (session_id.empty()) {
                                json response = {
                                    {"op", Op::SESSION_ERROR},
                                    {"message", "Failed to create session (limit reached or error)"}
                                };
                                ws->send(response.dump(), uWS::OpCode::TEXT);
                            } else {
                                json response = {
                                    {"op", Op::SESSION_CREATED},
                                    {"session_id", session_id}
                                };
                                ws->send(response.dump(), uWS::OpCode::TEXT);
                            }
                            return;
                        }

                        // CLOSE_SESSION
                        if (op == Op::CLOSE_SESSION) {
                            if (!j.contains("session_id")) {
                                json response = {
                                    {"op", Op::SESSION_ERROR},
                                    {"message", "Missing session_id"}
                                };
                                ws->send(response.dump(), uWS::OpCode::TEXT);
                                return;
                            }

                            std::string session_id = j["session_id"];
                            
                            // Verify session belongs to this client
                            auto* session = sessionManager->getSession(session_id);
                            if (session && session->getClientId() == data->client_id) {
                                sessionManager->closeSession(session_id);
                                json response = {
                                    {"op", Op::SESSION_CLOSED},
                                    {"session_id", session_id}
                                };
                                ws->send(response.dump(), uWS::OpCode::TEXT);
                            } else {
                                json response = {
                                    {"op", Op::SESSION_ERROR},
                                    {"message", "Session not found or access denied"}
                                };
                                ws->send(response.dump(), uWS::OpCode::TEXT);
                            }
                            return;
                        }

                        // INFER
                        if (op == Op::INFER) {
                            InferenceParams params = parseInfer(j);
                            
                            if (params.session_id.empty()) {
                                json response = {
                                    {"op", Op::ERROR},
                                    {"message", "Missing session_id in INFER request"}
                                };
                                ws->send(response.dump(), uWS::OpCode::TEXT);
                                return;
                            }

                            // Verify session exists and belongs to this client
                            auto* session = sessionManager->getSession(params.session_id);
                            if (!session || session->getClientId() != data->client_id) {
                                json response = {
                                    {"op", Op::ERROR},
                                    {"message", "Session not found or access denied"}
                                };
                                ws->send(response.dump(), uWS::OpCode::TEXT);
                                return;
                            }

                            // Queue the inference task
                            std::lock_guard<std::mutex> lock(queueMutex);
                            taskQueue.push({ws, params.session_id, params});
                            queueCv.notify_one();
                            return;
                        }

                        // SUBSCRIBE_METRICS
                        if (op == Op::SUBSCRIBE_METRICS) {
                            {
                                std::lock_guard<std::mutex> lock(metricsSubscribersMutex);
                                metricsSubscribers.insert(ws);
                            }
                            
                            json response = {
                                {"op", Op::METRICS_SUBSCRIBED},
                                {"message", "Subscribed to metrics updates"}
                            };
                            ws->send(response.dump(), uWS::OpCode::TEXT);
                            std::cout << "Client " << data->client_id << " subscribed to metrics" << std::endl;
                            return;
                        }

                        // UNSUBSCRIBE_METRICS
                        if (op == Op::UNSUBSCRIBE_METRICS) {
                            {
                                std::lock_guard<std::mutex> lock(metricsSubscribersMutex);
                                metricsSubscribers.erase(ws);
                            }
                            
                            json response = {
                                {"op", Op::METRICS_UNSUBSCRIBED},
                                {"message", "Unsubscribed from metrics updates"}
                            };
                            ws->send(response.dump(), uWS::OpCode::TEXT);
                            std::cout << "Client " << data->client_id << " unsubscribed from metrics" << std::endl;
                            return;
                        }

                        // ABORT
                        if (op == Op::ABORT) {
                            if (!j.contains("session_id")) return;
                            
                            std::string session_id = j["session_id"];
                            auto* session = sessionManager->getSession(session_id);
                            if (session && session->getClientId() == data->client_id) {
                                session->abort();
                            }
                            return;
                        }

                    } catch (const std::exception& e) {
                        std::cerr << "JSON Error: " << e.what() << std::endl;
                    }
                },
                .close = [this](auto* ws, int, std::string_view) {
                    auto* data = ws->getUserData();
                    
                    if (data->authenticated) {
                        std::cout << "Client disconnected: " << data->client_id << std::endl;
                        
                        // Close all sessions for this client
                        sessionManager->closeClientSessions(data->client_id);
                    } else {
                        std::cout << "Unauthenticated client disconnected" << std::endl;
                    }
                    
                    // Remove from connected clients
                    {
                        std::lock_guard<std::mutex> lock(clientsMutex);
                        connectedClients.erase(ws);
                    }
                    
                    // Remove from metrics subscribers
                    {
                        std::lock_guard<std::mutex> lock(metricsSubscribersMutex);
                        metricsSubscribers.erase(ws);
                    }
                }
            })
            .listen(port, [this](auto* token) {
                if (token) {
                    std::cout << "Listening on port " << port << std::endl;
                } else {
                    std::cerr << "Failed to listen on port " << port << std::endl;
                }
            })
            .run();
    }

    void WsServer::workerLoop() {
        while (running) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                queueCv.wait(lock, [this] { return !taskQueue.empty() || !running; });

                if (!running) break;

                task = taskQueue.front();
                taskQueue.pop();
            }

            processInference(task);
        }
    }

    void WsServer::processInference(Task& task) {
        // Get the session
        auto* session = sessionManager->getSession(task.session_id);
        if (!session) {
            std::cerr << "Session not found: " << task.session_id << std::endl;
            return;
        }

        activeGenerations++;

        auto metrics = session->generate(task.params.prompt, [this, &task](const std::string& token) {
            // Send to main thread
            std::string tokenCopy = token;
            
            loop->defer([this, task, tokenCopy]() {
                // Sanitize UTF-8 to prevent JSON serialization errors
                std::string validToken = sanitizeUtf8(tokenCopy);
                
                json msg = {
                    {"op", Op::TOKEN},
                    {"session_id", task.session_id},
                    {"content", validToken}
                };
                task.ws->send(msg.dump(), uWS::OpCode::TEXT);
            });
            
            return true;
        });

        // Store metrics for broadcasting
        {
            std::lock_guard<std::mutex> lock(metricsMutex);
            lastMetrics = metrics;
        }

        // Finished
        loop->defer([this, task, metrics]() {
            json msg = {
                {"op", Op::END},
                {"session_id", task.session_id},
                {"stats", {
                    {"ttft_ms", metrics.ttft_ms},
                    {"total_ms", metrics.total_time_ms},
                    {"tokens", metrics.tokens_generated},
                    {"tps", metrics.tps}
                }}
            };
            task.ws->send(msg.dump(), uWS::OpCode::TEXT);
        });
        
        activeGenerations--;
    }
    
    void WsServer::metricsLoop() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            if (!running) break;
            
            broadcastMetrics();
        }
    }
    
    void WsServer::broadcastMetrics() {
        // Get current GPU stats
        auto gpuStats = monitor.updateStats();
        
        // Get current metrics
        Core::Metrics currentMetrics;
        {
            std::lock_guard<std::mutex> lock(metricsMutex);
            currentMetrics = lastMetrics;
        }
        
        // Build metrics JSON
        json metricsJson = {
            {"op", Op::METRICS},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
            {"gpu", {
                {"temp", gpuStats.temp},
                {"vram_total_mb", gpuStats.memoryTotal / (1024*1024)},
                {"vram_used_mb", gpuStats.memoryUsed / (1024*1024)},
                {"vram_free_mb", gpuStats.memoryFree / (1024*1024)},
                {"power_watts", gpuStats.powerUsage / 1000},
                {"fan_percent", gpuStats.fanSpeed},
                {"throttling", gpuStats.throttle}
            }},
            {"inference", {
                {"active_generations", activeGenerations.load()},
                {"total_sessions", sessionManager->getTotalSessionCount()},
                {"last_tps", currentMetrics.tps},
                {"last_ttft_ms", currentMetrics.ttft_ms},
                {"total_tokens_generated", currentMetrics.tokens_generated}
            }}
        };
        
        std::string metricsStr = metricsJson.dump();
        
        // Broadcast to subscribed clients only
        loop->defer([this, metricsStr]() {
            std::lock_guard<std::mutex> lock(metricsSubscribersMutex);
            for (auto* ws : metricsSubscribers) {
                ws->send(metricsStr, uWS::OpCode::TEXT);
            }
        });
    }
}
