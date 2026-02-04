#pragma once

#include <App.h> // uWebSockets
#include <mutex>
#include <thread>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <set>
#include <vector>
#include "Protocol.h"
#include "ClientAuth.h"
#include "../core/Engine.h"
#include "../core/SessionManager.h"
#include "../hardware/Monitor.h"

namespace Server {

    struct PerSocketData {
        std::string client_id;
        bool authenticated = false;
    };

    class WsServer {
    public:
        WsServer(Core::Engine& engine, Hardware::Monitor& monitor, 
                 const std::string& clientConfigPath, int port = 3000);
        ~WsServer();

        // Start the server loop (blocking)
        void run();

    private:
        Core::Engine& engine;
        Core::SessionManager* sessionManager = nullptr;
        ClientAuth clientAuth;
        Hardware::Monitor& monitor;
        int port;
        
        // uWebSockets LOOP
        struct uWS::Loop* loop = nullptr;
        
        // Thread Safe Queue for Inference Tasks
        struct Task {
            uWS::WebSocket<false, true, PerSocketData>* ws;
            std::string session_id;
            InferenceParams params;
        };
        
        std::queue<Task> taskQueue;
        std::mutex queueMutex;
        std::condition_variable queueCv;
        
        std::atomic<bool> running{true};
        std::vector<std::thread> workerThreads;  // Thread pool
        std::thread metricsThread;

        // Track all connected clients for metrics broadcasting
        std::set<uWS::WebSocket<false, true, PerSocketData>*> connectedClients;
        std::mutex clientsMutex;
        
        // Metrics state
        std::atomic<int> activeGenerations{0};
        Core::Metrics lastMetrics;
        std::mutex metricsMutex;

        void workerLoop();
        void processInference(Task& task);
        void metricsLoop();
        void broadcastMetrics();
    };

}
