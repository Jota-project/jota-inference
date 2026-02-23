#include "Engine.h"
#include "LlamaLogger.h"
#include <vector>
#include <cstring>
#include <mutex>

namespace Core {

    static std::once_flag backend_init_flag;

    Engine::Engine() {
        std::call_once(backend_init_flag, []() {
            // Register log callback BEFORE backend init so we capture everything
            initLlamaLogging();
            llama_backend_init();
        });
    }

    Engine::~Engine() {
        if (model) llama_model_free(model);
    }

    bool Engine::isLoaded() const {
        return model != nullptr;
    }

    bool Engine::loadModel(const EngineConfig& config) {
        if (isLoaded()) return false;

        // Store context size for later use by SessionManager
        ctx_size_ = config.ctx_size;

        // Model Parameters
        auto mparams = llama_model_default_params();
        mparams.n_gpu_layers = config.n_gpu_layers;
        mparams.use_mmap = config.use_mmap;
        mparams.use_mlock = config.use_mlock;

        // Load Model (llama.cpp output is now captured by our log callback)
        model = llama_model_load_from_file(config.modelPath.c_str(), mparams);

        if (!model) {
            return false;
        }

        return true;
    }

    
    std::string Engine::getSystemInfo() const {
        return llama_print_system_info();
    }

}
