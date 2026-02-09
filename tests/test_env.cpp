#include "catch_amalgamated.hpp"
#include "../src/core/EnvLoader.h"
#include <fstream>
#include <cstdlib>

using namespace Core;

TEST_CASE("EnvLoader: Environment Variables", "[env]") {
    // Setup: Create a temporary .env file
    std::string envPath = ".env";
    
    // Backup existing .env if it exists
    bool existed = false;
    std::string backupPath = ".env.bak";
    if (std::ifstream(envPath).good()) {
        existed = true;
        std::rename(envPath.c_str(), backupPath.c_str());
    }

    std::ofstream out(envPath);
    out << "TEST_KEY=test_value\n";
    out << "TEST_INT=123\n";
    out << "# This is a comment\n";
    out << "TEST_QUOTED=\"quoted value\"\n";
    out << "   TEST_SPACED  =  spaced value  \n";
    out.close();

    SECTION("Load .env file") {
        REQUIRE(EnvLoader::load() == true);
    }

    SECTION("Get Values") {
        EnvLoader::load();
        
        REQUIRE(EnvLoader::get("TEST_KEY") == "test_value");
        REQUIRE(EnvLoader::get("TEST_INT") == "123");
        REQUIRE(EnvLoader::get("TEST_QUOTED") == "quoted value");
        REQUIRE(EnvLoader::get("TEST_SPACED") == "spaced value");
        
        // Default value
        REQUIRE(EnvLoader::get("NON_EXISTENT", "default") == "default");
    }

    SECTION("System Environment Override or Fallback") {
        // Here we test that if not in .env, it checks system
        setenv("SYSTEM_KEY", "system_value", 1);
        REQUIRE(EnvLoader::get("SYSTEM_KEY") == "system_value");
        
        // .env should take precedence if loaded (implementation specific, currently map checks first)
        EnvLoader::load();
        REQUIRE(EnvLoader::get("TEST_KEY") == "test_value");
    }

    // Cleanup
    remove(envPath.c_str());
    if (existed) {
        std::rename(backupPath.c_str(), envPath.c_str());
    }
}
