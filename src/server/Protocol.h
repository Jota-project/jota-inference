#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Server {

    // Opcodes for client -> server messages
    namespace Op {
        // Authentication
        constexpr const char* AUTH = "auth";
        
        // Session management
        constexpr const char* CREATE_SESSION = "create_session";
        constexpr const char* SET_CONTEXT = "set_context";
        constexpr const char* CLOSE_SESSION = "close_session";
        
        // Inference & Models
        constexpr const char* INFER = "infer";
        constexpr const char* ABORT = "abort";
        constexpr const char* LOAD_MODEL = "COMMAND_LOAD_MODEL";
        constexpr const char* LIST_MODELS = "COMMAND_LIST_MODELS";
        
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
        constexpr const char* CONTEXT_SET = "context_set";
        constexpr const char* CONTEXT_ERROR = "context_error";
        constexpr const char* TOKEN = "token";
        constexpr const char* END   = "end";
        constexpr const char* ERROR = "error";
        constexpr const char* METRICS = "metrics";  // Real-time system metrics
        constexpr const char* METRICS_SUBSCRIBED = "metrics_subscribed";
        constexpr const char* METRICS_UNSUBSCRIBED = "metrics_unsubscribed";
        constexpr const char* LOAD_MODEL_RESULT = "load_model_result";
        constexpr const char* LIST_MODELS_RESULT = "list_models_result";
    }

    // --- Session Context (extensible) ---

    struct ChatMessage {
        std::string role;     // "user", "assistant", "system"
        std::string content;
    };

    struct SessionContext {
        std::vector<ChatMessage> messages;  // Chat history
        json extra;                         // Reserved for future context types
    };

    inline SessionContext parseContext(const json& payload) {
        SessionContext ctx;
        if (payload.contains("context")) {
            auto& context = payload["context"];
            if (context.contains("messages") && context["messages"].is_array()) {
                for (auto& msg : context["messages"]) {
                    ChatMessage cm;
                    if (msg.contains("role"))    cm.role = msg["role"].get<std::string>();
                    if (msg.contains("content")) cm.content = msg["content"].get<std::string>();
                    ctx.messages.push_back(std::move(cm));
                }
            }
            // Store the full context JSON for future extensibility
            ctx.extra = context;
        }
        return ctx;
    }

    // --- Inference Parameters ---

    struct InferenceParams {
        std::string session_id;
        std::string prompt;
        std::string mode;           // Profile: "instant", "balanced", "creative"
        std::string system_prompt;  // Prepended to prompt (set by profile or client)
        std::string grammar;       // Optional GBNF grammar constraint
        float temp = 0.7f;
        float top_p = 0.9f;
        int max_tokens = -1;       // -1 = use profile/model default
    };

    inline InferenceParams parseInfer(const json& payload) {
        InferenceParams p;
        if (payload.contains("session_id")) p.session_id = payload["session_id"].get<std::string>();
        if (payload.contains("prompt"))     p.prompt = payload["prompt"].get<std::string>();
        if (payload.contains("mode"))       p.mode = payload["mode"].get<std::string>();
        if (payload.contains("grammar"))    p.grammar = payload["grammar"].get<std::string>();
        if (payload.contains("params")) {
            auto& params = payload["params"];
            if (params.contains("temp"))          p.temp = params["temp"].get<float>();
            if (params.contains("top_p"))         p.top_p = params["top_p"].get<float>();
            if (params.contains("max_tokens"))    p.max_tokens = params["max_tokens"].get<int>();
            if (params.contains("system_prompt")) p.system_prompt = params["system_prompt"].get<std::string>();
        }
        return p;
    }

}
