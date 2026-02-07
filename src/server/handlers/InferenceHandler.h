#pragma once

#include "../RequestContext.h"
#include "../services/InferenceService.h"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace Server {

/**
 * InferenceHandler - Handles inference operations
 * 
 * Processes Op::INFER and Op::ABORT requests.
 * Delegates actual inference to InferenceService.
 */
class InferenceHandler {
public:
    explicit InferenceHandler(InferenceService* inferenceService)
        : inferenceService_(inferenceService)
    {
        if (!inferenceService_) {
            throw std::invalid_argument("InferenceService cannot be null");
        }
    }
    
    /**
     * Handle inference request
     * @param ctx Request context
     * @param payload JSON payload with session_id, prompt, and params
     */
    void handleInfer(RequestContext& ctx, const json& payload) {
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
        
        // Extract required fields
        if (!payload.contains("session_id") || !payload.contains("prompt")) {
            json response = {
                {"op", Op::ERROR},
                {"error", "Missing session_id or prompt"}
            };
            ctx.send(response);
            return;
        }
        
        std::string session_id = payload["session_id"];
        std::string prompt = payload["prompt"];
        
        // Parse inference parameters
        InferenceParams params;
        params.prompt = prompt;
        
        if (payload.contains("params")) {
            auto p = payload["params"];
            if (p.contains("temp")) {
                params.temp = p["temp"];
            }
            if (p.contains("max_tokens")) {
                params.max_tokens = p["max_tokens"];
            }
        }
        
        // Create callbacks that use RequestContext
        auto onToken = [&ctx, session_id](const std::string& sid, const std::string& token) {
            json msg = {
                {"op", Op::TOKEN},
                {"session_id", sid},
                {"content", token}
            };
            ctx.send(msg);
        };
        
        auto onComplete = [&ctx, session_id](const std::string& sid, const Core::Metrics& metrics) {
            json msg = {
                {"op", Op::END},
                {"session_id", sid},
                {"stats", {
                    {"ttft_ms", metrics.ttft_ms},
                    {"total_ms", metrics.total_time_ms},
                    {"tokens", metrics.tokens_generated},
                    {"tps", metrics.tps}
                }}
            };
            ctx.send(msg);
        };
        
        // Enqueue task to InferenceService
        InferenceService::Task task{
            session_id,
            params,
            onToken,
            onComplete
        };
        
        inferenceService_->enqueueTask(std::move(task));
        
        std::cout << "Inference enqueued for session: " << session_id << std::endl;
    }
    
    /**
     * Handle abort request
     * @param ctx Request context
     * @param payload JSON payload with session_id
     */
    void handleAbort(RequestContext& ctx, const json& /*payload*/) {
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
        
        // TODO: Implement abort functionality in Session/SessionManager
        // For now, just acknowledge
        std::cout << "Abort requested (not yet implemented)" << std::endl;
    }

private:
    InferenceService* inferenceService_;
};

} // namespace Server
