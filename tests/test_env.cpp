#include "catch_amalgamated.hpp"
#include "EnvLoader.h"
#include <fstream>
#include <cstdlib>

using namespace Core;

TEST_CASE("EnvLoader: Environment Variables", "[env]") {
    std::string envPath = ".env";
    
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
        REQUIRE(EnvLoader::load() == true);
        
        CHECK(EnvLoader::get("TEST_KEY") == "test_value");
        CHECK(EnvLoader::get("TEST_INT") == "123");
        CHECK(EnvLoader::get("TEST_QUOTED") == "quoted value");
        CHECK(EnvLoader::get("TEST_SPACED") == "spaced value");
        
        CHECK(EnvLoader::get("NON_EXISTENT", "default") == "default");
    }

    SECTION("System Environment Override or Fallback") {
        setenv("SYSTEM_KEY", "system_value", 1);
        CHECK(EnvLoader::get("SYSTEM_KEY") == "system_value");
        
        EnvLoader::load();
        CHECK(EnvLoader::get("TEST_KEY") == "test_value");
    }

    remove(envPath.c_str());
    if (existed) {
        std::rename(backupPath.c_str(), envPath.c_str());
    }
}
