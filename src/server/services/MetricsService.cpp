#include "MetricsService.h"
#include "../Protocol.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <iostream>

using json = nlohmann::json;

namespace Server {

MetricsService::MetricsService(
    Hardware::Monitor& monitor,
    Core::SessionManager* sessionManager,
    InferenceService* inferenceService
)
    : monitor_(monitor)
    , sessionManager_(sessionManager)
    , inferenceService_(inferenceService) 
{
    if (!sessionManager_) {
        throw std::invalid_argument("SessionManager cannot be null");
    }
    if (!inferenceService_) {
        throw std::invalid_argument("InferenceService cannot be null");
    }
}

MetricsService::~MetricsService() {
    shutdown();
}

void MetricsService::start(BroadcastCallback callback) {
    broadcastCallback_ = callback;
    
    // Start metrics thread
    metricsThread_ = std:: thread([this]() { metricsLoop(); });
    
    std::cout << "MetricsService: Started" << std::endl;
}

void MetricsService::shutdown() {
    if (!running_.exchange(false)) {
        return; // Already shutting down
    }
    
    // Wait for metrics thread to finish
    if (metricsThread_.joinable()) {
        metricsThread_.join();
    }
    
    std::cout << "MetricsService: Shutdown complete" << std::endl;
}

void MetricsService::metricsLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if (!running_) break;
        
        // Build and broadcast metrics
        std::string metricsJson = buildMetricsJson();
        
        if (broadcastCallback_) {
            broadcastCallback_(metricsJson);
        }
    }
}

std::string MetricsService::buildMetricsJson() {
    // Get current GPU stats
    auto gpuStats = monitor_.updateStats();
    
    // Get current inference metrics
    auto currentMetrics = inferenceService_->getLastMetrics();
    int activeGens = inferenceService_->getActiveGenerations();
    
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
            {"active_generations", activeGens},
            {"total_sessions", sessionManager_->getTotalSessionCount()},
            {"last_tps", currentMetrics.tps},
            {"last_ttft_ms", currentMetrics.ttft_ms},
            {"total_tokens_generated", currentMetrics.tokens_generated}
        }}
    };
    
    return metricsJson.dump();
}

} // namespace Server
