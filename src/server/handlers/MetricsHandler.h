#pragma once

#include "../RequestContext.h"
#include <nlohmann/json.hpp>
#include <set>
#include <mutex>
#include <iostream>

using json = nlohmann::json;

namespace Server {

/**
 * MetricsHandler - Handles metrics subscription operations
 * 
 * Processes Op::SUBSCRIBE_METRICS and Op::UNSUBSCRIBE_METRICS requests.
 * Maintains list of subscribed clients.
 */
class MetricsHandler {
public:
    MetricsHandler() = default;
    
    /**
     * Handle metrics subscription request
     * @param ctx Request context
     * @param payload JSON payload (empty)
     */
    void handleSubscribe(RequestContext& ctx, const json& /*payload*/) {
        auto* data = ctx.getData();
        
        // Check authentication
        if (!data->authenticated) {
            json response = {
                {"op", Op::ERROR},
                {"error", "Not authenticated"}
            };
            ctx.send(response);
            return;
        }
        
        // Add to subscribers
        {
            std::lock_guard<std::mutex> lock(subscribersMutex_);
            subscribers_.insert(ctx.getRawSocket());
        }
        
        json response = {
            {"op", Op::METRICS_SUBSCRIBED},
            {"message", "Subscribed to metrics updates"}
        };
        ctx.send(response);
        
        std::cout << "Client subscribed to metrics: " << data->client_id << std::endl;
    }
    
    /**
     * Handle metrics unsubscription request
     * @param ctx Request context
     * @param payload JSON payload (empty)
     */
    void handleUnsubscribe(RequestContext& ctx, const json& /*payload*/) {
        auto* data = ctx.getData();
        
        // Check authentication
        if (!data->authenticated) {
            json response = {
                {"op", Op::ERROR},
                {"error", "Not authenticated"}
            };
            ctx.send(response);
            return;
        }
        
        // Remove from subscribers
        {
            std::lock_guard<std::mutex> lock(subscribersMutex_);
            subscribers_.erase(ctx.getRawSocket());
        }
        
        json response = {
            {"op", Op::METRICS_UNSUBSCRIBED},
            {"message", "Unsubscribed from metrics updates"}
        };
        ctx.send(response);
        
        std::cout << "Client unsubscribed from metrics: " << data->client_id << std::endl;
    }
    
    /**
     * Get copy of subscribers set (thread-safe)
     * Used by WsServer to broadcast metrics
     */
    std::set<uWS::WebSocket<false, true, PerSocketData>*> getSubscribers() {
        std::lock_guard<std::mutex> lock(subscribersMutex_);
        return subscribers_;
    }
    
    /**
     * Remove subscriber (called on disconnect)
     */
    void removeSubscriber(uWS::WebSocket<false, true, PerSocketData>* ws) {
        std::lock_guard<std::mutex> lock(subscribersMutex_);
        subscribers_.erase(ws);
    }

private:
    std::set<uWS::WebSocket<false, true, PerSocketData>*> subscribers_;
    std::mutex subscribersMutex_;
};

} // namespace Server
