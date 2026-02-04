#pragma once

#include "llama.h"
#include "Metrics.h"
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>

namespace Core {

    // Callback for streaming tokens. Returns true to continue, false to abort.
    using TokenCallback = std::function<bool(const std::string& token)>;

    struct EngineConfig {
        std::string modelPath;
        int n_gpu_layers = -1; // -1 = auto-detect, 0 = CPU only, >0 = specific count
        int ctx_size = 512;    // Reduced for short conversations
        bool use_mmap = true;
        bool use_mlock = false;
    };

    class Engine {
    public:
        Engine();
        ~Engine();

        // Prevent copying
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // Initialize the engine with the specific model
        bool loadModel(const EngineConfig& config);

        // Check internal status
        bool isLoaded() const;

        // Get system info
        std::string getSystemInfo() const;

        // Get the loaded model (for SessionManager to create contexts)
        struct llama_model* getModel() { return model; }
        
        // Get context size from config
        int getCtxSize() const { return ctx_size_; }

    private:
        struct llama_model* model = nullptr;
        int ctx_size_ = 512;
    };

}
