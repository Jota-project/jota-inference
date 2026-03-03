#include "catch_amalgamated.hpp"
#include "../src/server/Protocol.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace Server;

TEST_CASE("Protocol: Operation Enum Strings", "[protocol]") {
    SECTION("Client -> Server Operations") {
        CHECK(std::string(Op::AUTH) == "auth");
        CHECK(std::string(Op::CREATE_SESSION) == "create_session");
        CHECK(std::string(Op::SET_CONTEXT) == "set_context");
        CHECK(std::string(Op::CLOSE_SESSION) == "close_session");
        CHECK(std::string(Op::INFER) == "infer");
        CHECK(std::string(Op::ABORT) == "abort");
        CHECK(std::string(Op::LOAD_MODEL) == "COMMAND_LOAD_MODEL");
        CHECK(std::string(Op::LIST_MODELS) == "COMMAND_LIST_MODELS");
        CHECK(std::string(Op::SUBSCRIBE_METRICS) == "subscribe_metrics");
        CHECK(std::string(Op::UNSUBSCRIBE_METRICS) == "unsubscribe_metrics");
    }

    SECTION("Server -> Client Operations") {
        CHECK(std::string(Op::HELLO) == "hello");
        CHECK(std::string(Op::AUTH_SUCCESS) == "auth_success");
        CHECK(std::string(Op::AUTH_FAILED) == "auth_failed");
        CHECK(std::string(Op::SESSION_CREATED) == "session_created");
        CHECK(std::string(Op::SESSION_CLOSED) == "session_closed");
        CHECK(std::string(Op::SESSION_ERROR) == "session_error");
        CHECK(std::string(Op::CONTEXT_SET) == "context_set");
        CHECK(std::string(Op::CONTEXT_ERROR) == "context_error");
        CHECK(std::string(Op::TOKEN) == "token");
        CHECK(std::string(Op::END) == "end");
        CHECK(std::string(Op::ERROR) == "error");
        CHECK(std::string(Op::METRICS) == "metrics");
        CHECK(std::string(Op::METRICS_SUBSCRIBED) == "metrics_subscribed");
        CHECK(std::string(Op::METRICS_UNSUBSCRIBED) == "metrics_unsubscribed");
        CHECK(std::string(Op::LOAD_MODEL_RESULT) == "load_model_result");
        CHECK(std::string(Op::LIST_MODELS_RESULT) == "list_models_result");
    }
}

TEST_CASE("Protocol: Message Structures", "[protocol]") {
    SECTION("Auth Message") {
        json msg = {
            {"op", Op::AUTH},
            {"client_id", "test_client"},
            {"api_key", "secret123"}
        };
        
        REQUIRE(msg.contains("op"));
        CHECK(msg["op"] == "auth");
        CHECK(msg["client_id"] == "test_client");
        CHECK(msg["api_key"] == "secret123");
    }

    SECTION("Inference Request & Params Parser") {
        json params = {
            {"temp", 0.7f},
            {"top_p", 0.9f},
            {"max_tokens", 100},
            {"system_prompt", "You are an AI."}
        };
        
        json msg = {
            {"op", Op::INFER},
            {"session_id", "sess_123"},
            {"prompt", "Hello"},
            {"mode", "instant"},
            {"grammar", "root ::= .*"},
            {"params", params}
        };

        REQUIRE(msg.contains("op"));
        CHECK(msg["op"] == "infer");
        
        // Test the parser logic explicitly
        InferenceParams parsedParams = parseInfer(msg);
        
        CHECK(parsedParams.session_id == "sess_123");
        CHECK(parsedParams.prompt == "Hello");
        CHECK(parsedParams.mode == "instant");
        CHECK(parsedParams.grammar == "root ::= .*");
        CHECK(parsedParams.temp == 0.7f);
        CHECK(parsedParams.top_p == 0.9f);
        CHECK(parsedParams.max_tokens == 100);
        CHECK(parsedParams.system_prompt == "You are an AI.");
    }
    
    SECTION("Session Context Parser") {
        json msg = {
            {"op", Op::SET_CONTEXT},
            {"context", {
                {"messages", {
                    {{"role", "system"}, {"content", "Act as a helpful assistant."}},
                    {{"role", "user"}, {"content", "What is the capital of France?"}}
                }}
            }}
        };
        
        SessionContext ctx = parseContext(msg);
        
        REQUIRE(ctx.messages.size() == 2);
        CHECK(ctx.messages[0].role == "system");
        CHECK(ctx.messages[0].content == "Act as a helpful assistant.");
        CHECK(ctx.messages[1].role == "user");
        CHECK(ctx.messages[1].content == "What is the capital of France?");
    }
}
