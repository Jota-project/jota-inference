#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "../src/server/Protocol.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace Server;

TEST_CASE("Protocol: Operation Enum Strings", "[protocol]") {
    SECTION("Operation constants are correct") {
        REQUIRE(std::string(Op::AUTH) == "auth");
        REQUIRE(std::string(Op::AUTH_SUCCESS) == "auth_success");
        REQUIRE(std::string(Op::AUTH_FAILED) == "auth_failed");
        REQUIRE(std::string(Op::CREATE_SESSION) == "create_session");
        REQUIRE(std::string(Op::INFER) == "infer");
    }
}

TEST_CASE("Protocol: Message Structures", "[protocol]") {
    SECTION("Auth Message") {
        json msg = {
            {"op", Op::AUTH},
            {"client_id", "test_client"},
            {"api_key", "secret123"}
        };
        
        REQUIRE(msg["op"] == "auth");
        REQUIRE(msg["client_id"] == "test_client");
        REQUIRE(msg["api_key"] == "secret123");
    }

    SECTION("Inference Request") {
        json params = {
            {"temp", 0.7},
            {"max_tokens", 100}
        };
        
        json msg = {
            {"op", Op::INFER},
            {"session_id", "sess_123"},
            {"prompt", "Hello"},
            {"params", params}
        };

        REQUIRE(msg["op"] == "infer");
        REQUIRE(msg["session_id"] == "sess_123");
        REQUIRE(msg["params"]["temp"] == 0.7);
    }
}
