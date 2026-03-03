#pragma once

#include <string>

namespace Core {

    struct AppConfig {
        // Network
        int server_port = 3000;

        // Inference / Memory Parameters
        int ctx_size = 4096;
        int n_batch = 4096;
        int n_ubatch = 512;

        // Singleton access
        static AppConfig& get() {
            static AppConfig instance;
            return instance;
        }

        // Load values from environment
        void load();

    private:
        AppConfig() = default;
    };

}
