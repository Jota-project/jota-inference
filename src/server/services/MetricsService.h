#pragma once

#include "../../hardware/Monitor.h"
#include "../../core/SessionManager.h"
#include "../../core/Metrics.h"
#include "InferenceService.h"
#include <App.h> // uWebSockets
#include <functional>
#include <thread>
#include <atomic>

namespace Server {

class MetricsHandler;

/**
 * MetricsService - Periodic metrics broadcasting
 * 
 * Extracts metrics thread and broadcasting logic from WsServer.
 * Collects metrics from Monitor and InferenceService and invokes callback.
 */
class MetricsService {
public:
    // Broadcast callback: sends metrics JSON to all subscribers
    // Called from metrics thread - caller must handle thread-safety (uWS::Loop::defer)
    using BroadcastCallback = std::function<void(const std::string& metricsJson)>;
    
    /**
     * Constructor
     * @param monitor Reference to hardware monitor
     * @param sessionManager Pointer to session manager
     * @param inferenceService Pointer to inference service
     */
    MetricsService(
        Hardware::Monitor& monitor,
        Core::SessionManager* sessionManager,
        InferenceService* inferenceService
    );
    
    /**
     * Destructor - automatically shuts down metrics thread
     */
    ~MetricsService();
    
    /**
     * Start metrics broadcasting
     */
    void start();
    
    /**
     * Gracefully shutdown the metrics service
     */
    void shutdown();

    // Setters for dependencies
    void setMetricsHandler(MetricsHandler* handler);
    void setEventLoop(uWS::Loop* loop);
    
private:
    Hardware::Monitor& monitor_;
    Core::SessionManager* sessionManager_;
    InferenceService* inferenceService_;
    
    MetricsHandler* metricsHandler_ = nullptr;
    uWS::Loop* loop_ = nullptr;
    
    std::thread metricsThread_;
    std::atomic<bool> running_{true};
    
    // Main metrics loop (runs in separate thread)
    void metricsLoop();
    
    // Build metrics JSON string
    std::string buildMetricsJson();
};

} // namespace Server
