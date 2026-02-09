#include "catch_amalgamated.hpp"
#include "../src/server/ClientAuth.h"
#include <httplib.h>
#include <thread>
#include <atomic>
#include <chrono>

using namespace Server;

// Helper class to run a simple mock server
class MockJotaDB {
public:
    MockJotaDB(int port) : svr_(), port_(port), running_(false) {
        svr_.Get("/auth/internal", [](const httplib::Request& req, httplib::Response& res) {
            std::string client_id = req.get_param_value("client_id");
            std::string api_key = req.get_param_value("api_key");
            
            // Check Authorization Header
            if (req.has_header("Authorization")) {
                std::string auth = req.get_header_value("Authorization");
                if (auth != "Bearer test_sk") {
                     res.status = 401;
                     return;
                }
            } else {
                // For test simplicity, we might enforce header check
                // res.status = 401; return; 
            }

            json response;
            if (client_id == "valid_user" && api_key == "secret123") {
                response["authorized"] = true;
                response["config"] = {
                    {"max_sessions", 5},
                    {"priority", "high"}
                };
            } else {
                response["authorized"] = false;
                response["error"] = "Invalid credentials";
            }
            
            res.set_content(response.dump(), "application/json");
        });
    }

    void start() {
        running_ = true;
        thread_ = std::thread([this]() {
            svr_.listen("localhost", port_);
        });
        // Give it a moment to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void stop() {
        if (running_) {
            svr_.stop();
            if (thread_.joinable()) {
                thread_.join();
            }
            running_ = false;
        }
    }

    ~MockJotaDB() {
        stop();
    }

private:
    httplib::Server svr_;
    int port_;
    std::atomic<bool> running_;
    std::thread thread_;
};

TEST_CASE("ClientAuth: JotaDB Integration", "[auth]") {
    // Start Mock Server
    int port = 8090;
    MockJotaDB mockDb(port);
    mockDb.start();

    // Setup Environment for ClientAuth
    std::string url = "http://localhost:" + std::to_string(port);
    setenv("JOTA_DB_URL", url.c_str(), 1);
    setenv("JOTA_SK", "test_sk", 1);

    ClientAuth auth; // Constructor calls initJotaDB which reads env

    SECTION("Authenticate Valid User") {
        REQUIRE(auth.authenticate("valid_user", "secret123") == true);
        
        // Verify Config Loaded
        ClientConfig cfg = auth.getClientConfig("valid_user");
        REQUIRE(cfg.max_sessions == 5);
        REQUIRE(cfg.priority == "high");
    }

    SECTION("Authenticate Invalid User") {
        REQUIRE(auth.authenticate("invalid_user", "wrong") == false);
        REQUIRE(auth.clientExists("invalid_user") == false);
    }

    SECTION("Cache Hit") {
        // First call hits DB
        REQUIRE(auth.authenticate("valid_user", "secret123") == true);
        
        // Stop DB to prove cache is used
        mockDb.stop();
        
        // Second call should succeed from cache
        REQUIRE(auth.authenticate("valid_user", "secret123") == true);
    }
}
