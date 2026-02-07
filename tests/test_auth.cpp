#include "catch_amalgamated.hpp"
#include "../src/server/ClientAuth.h"
#include <fstream>

using namespace Server;

TEST_CASE("ClientAuth: Authentication Logic", "[auth]") {
    // Setup: Create a temporary config file
    std::string configPath = "clients_test.json";
    std::ofstream out(configPath);
    out << R"({
        "clients": [
            {
                "client_id": "valid_user",
                "api_key": "secret_key_123",
                "max_sessions": 5
            }
        ]
    })";
    out.close();

    ClientAuth auth;
    
    SECTION("Load Config") {
        REQUIRE(auth.loadConfig(configPath) == true);
    }

    SECTION("Authenticate") {
        auth.loadConfig(configPath);
        
        // Valid credentials
        REQUIRE(auth.authenticate("valid_user", "secret_key_123") == true);
        
        // Invalid username
        REQUIRE(auth.authenticate("invalid_user", "secret_key_123") == false);
        
        // Invalid password
        REQUIRE(auth.authenticate("valid_user", "wrong_key") == false);
    }

    SECTION("Get Config") {
        auth.loadConfig(configPath);
        ClientConfig config = auth.getClientConfig("valid_user");
        
        REQUIRE(config.client_id == "valid_user");
        REQUIRE(config.max_sessions == 5);
    }
    
    // Cleanup
    remove(configPath.c_str());
}
