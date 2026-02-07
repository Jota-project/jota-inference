#pragma once
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Server {

    // Opcodes for client -> server messages
    namespace Op {
        // Authentication
        constexpr const char* AUTH = "auth";
        
        // Session management
        constexpr const char* CREATE_SESSION = "create_session";
        constexpr const char* CLOSE_SESSION = "close_session";
        
        // Inference
        constexpr const char* INFER = "infer";
        constexpr const char* ABORT = "abort";
        
        // Metrics subscription
        constexpr const char* SUBSCRIBE_METRICS = "subscribe_metrics";
        constexpr const char* UNSUBSCRIBE_METRICS = "unsubscribe_metrics";
        
        // Server -> Client
        constexpr const char* HELLO = "hello";
        constexpr const char* AUTH_SUCCESS = "auth_success";
        constexpr const char* AUTH_FAILED = "auth_failed";
        constexpr const char* SESSION_CREATED = "session_created";
        constexpr const char* SESSION_CLOSED = "session_closed";
        constexpr const char* SESSION_ERROR = "session_error";
        constexpr const char* TOKEN = "token";
        constexpr const char* END   = "end";
        constexpr const char* ERROR = "error";
        constexpr const char* METRICS = "metrics";  // Real-time system metrics
        constexpr const char* METRICS_SUBSCRIBED = "metrics_subscribed";
        constexpr const char* METRICS_UNSUBSCRIBED = "metrics_unsubscribed";
    }

    struct InferenceParams {
        std::string session_id;
        std::string prompt;
        float temp = 0.7f;
        int max_tokens = -1;
    };

    inline InferenceParams parseInfer(const json& payload) {
        InferenceParams p;
        if (payload.contains("session_id")) p.session_id = payload["session_id"].get<std::string>();
        if (payload.contains("prompt")) p.prompt = payload["prompt"].get<std::string>();
        if (payload.contains("params")) {
            auto& params = payload["params"];
            if (params.contains("temp")) p.temp = params["temp"].get<float>();
            if (params.contains("max_tokens")) p.max_tokens = params["max_tokens"].get<int>();
        }
        return p;
    }

}
