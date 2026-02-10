#include "Engine.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <mutex>
#include <unistd.h>
#include <fcntl.h>

namespace Core {

    static std::once_flag backend_init_flag;

    Engine::Engine() {
        std::call_once(backend_init_flag, []() {
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

        // Silence llama.cpp verbose output (both stdout and stderr)
        int stdout_backup = dup(STDOUT_FILENO);
        int stderr_backup = dup(STDERR_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        close(devnull);

        // Load Model
        model = llama_model_load_from_file(config.modelPath.c_str(), mparams);
        
        // Restore stdout and stderr
        dup2(stdout_backup, STDOUT_FILENO);
        dup2(stderr_backup, STDERR_FILENO);
        close(stdout_backup);
        close(stderr_backup);

        if (!model) {
            return false;
        }

        return true;
    }

    
    std::string Engine::getSystemInfo() const {
        return llama_print_system_info();
    }

}
