#pragma once

#include "RequestContext.h"
#include "handlers/PingHandler.h"
#include "handlers/AuthHandler.h"
#include "handlers/SessionHandler.h"
#include "handlers/InferenceHandler.h"
#include "handlers/MetricsHandler.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>

using json = nlohmann::json;

namespace Server {

/**
 * MessageDispatcher - Routes incoming JSON messages to appropriate handlers
 * 
 * Parses operation from JSON and delegates to the correct handler.
 * Centralized error handling for malformed messages.
 */
class MessageDispatcher {
public:
    MessageDispatcher(
        std::shared_ptr<PingHandler> pingHandler,
        std::shared_ptr<AuthHandler> authHandler,
        std::shared_ptr<SessionHandler> sessionHandler,
        std::shared_ptr<InferenceHandler> inferenceHandler,
        std::shared_ptr<MetricsHandler> metricsHandler
    )
        : pingHandler_(pingHandler)
        , authHandler_(authHandler)
        , sessionHandler_(sessionHandler)
        , inferenceHandler_(inferenceHandler)
        , metricsHandler_(metricsHandler)
    {
        if (!pingHandler_ || !authHandler_ || !sessionHandler_ || !inferenceHandler_ || !metricsHandler_) {
            throw std::invalid_argument("All handlers must be provided");
        }
    }
    
    /**
     * Dispatch incoming message to appropriate handler
     * @param ctx Request context
     * @param message Raw JSON string from client
     */
    void dispatch(RequestContext& ctx, const std::string& message) {
        try {
            // Parse JSON
            json data = json::parse(message);
            
            // Extract operation
            if (!data.contains("op")) {
                handleError(ctx, "Missing 'op' field");
                return;
            }
            
            std::string op = data["op"];
            
            // Route to appropriate handler
            // HELLO does not require authentication
            if (op == Op::HELLO) {
                pingHandler_->handle(ctx, data);
            }
            else if (op == Op::AUTH) {
                authHandler_->handle(ctx, data);
            }
            else if (op == Op::CREATE_SESSION) {
                sessionHandler_->handleCreate(ctx, data);
            }
            else if (op == Op::CLOSE_SESSION) {
                sessionHandler_->handleClose(ctx, data);
            }
            else if (op == Op::INFER) {
                inferenceHandler_->handleInfer(ctx, data);
            }
            else if (op == Op::ABORT) {
                inferenceHandler_->handleAbort(ctx, data);
            }
            else if (op == Op::SUBSCRIBE_METRICS) {
                metricsHandler_->handleSubscribe(ctx, data);
            }
            else if (op == Op::UNSUBSCRIBE_METRICS) {
                metricsHandler_->handleUnsubscribe(ctx, data);
            }
            else {
                handleError(ctx, "Unknown operation: " + op);
            }
            
        } catch (const json::parse_error& e) {
            handleError(ctx, "Invalid JSON: " + std::string(e.what()));
        } catch (const std::exception& e) {
            handleError(ctx, "Error processing message: " + std::string(e.what()));
        }
    }

private:
    std::shared_ptr<PingHandler> pingHandler_;
    std::shared_ptr<AuthHandler> authHandler_;
    std::shared_ptr<SessionHandler> sessionHandler_;
    std::shared_ptr<InferenceHandler> inferenceHandler_;
    std::shared_ptr<MetricsHandler> metricsHandler_;
    
    /**
     * Send error response to client
     */
    void handleError(RequestContext& ctx, const std::string& error) {
        std::cerr << "MessageDispatcher error: " << error << std::endl;
        
        json response = {
            {"op", Op::ERROR},
            {"error", error}
        };
        ctx.send(response);
    }
};

} // namespace Server
