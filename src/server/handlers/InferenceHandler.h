#pragma once

#include "../RequestContext.h"
#include "../services/InferenceService.h"
#include "Logger.h"
#include <nlohmann/json.hpp>

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
        
        // Parse all inference parameters (mode, temp, top_p, max_tokens, system_prompt, grammar)
        InferenceParams params = parseInfer(payload);
        
        // Create callbacks that use RequestContext
        auto onToken = [ctx, session_id](const std::string& sid, const std::string& token, const std::string& type) {
            json msg = {
                {"op", Op::TOKEN},
                {"session_id", sid},
                {"content", token},
                {"type", type}
            };
            ctx.send(msg);
        };
        
        auto onComplete = [ctx, session_id](const std::string& sid, const Core::Metrics& metrics) {
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
        
        auto onError = [ctx, session_id](const std::string& sid, const std::string& error_msg) {
            json msg = {
                {"op", Op::ERROR},
                {"session_id", sid},
                {"error", error_msg}
            };
            try {
                ctx.send(msg);
            } catch (...) {
                // WebSocket send failed — client already disconnected, nothing to do
            }
        };
        
        // Enqueue task to InferenceService
        InferenceService::Task task{
            session_id,
            params,
            onToken,
            onComplete,
            onError
        };
        
        inferenceService_->enqueueTask(std::move(task));
        
        IC_LOG_INFO("Inference enqueued", {{"session_id", session_id}});
    }
    
    /**
     * Handle abort request
     * @param ctx Request context
     * @param payload JSON payload with session_id
     */
    void handleAbort(RequestContext& ctx, const json& payload) {
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

        std::string session_id;
        if (payload.contains("session_id")) {
            session_id = payload["session_id"];
        } else {
             json response = {
                {"op", Op::ERROR},
                {"error", "Missing session_id"}
            };
            ctx.send(response);
            return;
        }
        
        bool success = inferenceService_->abortTask(session_id);
        
        json response = {
            {"op", Op::ABORT},
            {"session_id", session_id},
            {"status", success ? "aborted" : "not_found"}
        };
        ctx.send(response);
        
        IC_LOG_INFO("Abort requested", {
            {"session_id", session_id},
            {"status", success ? "aborted" : "not_found"}
        });
    }

private:
    InferenceService* inferenceService_;
};

} // namespace Server
