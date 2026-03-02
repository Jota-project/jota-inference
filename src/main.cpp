#include <fstream>
#include <sys/stat.h>
#include "Engine.h"
#include "WsServer.h"
#include "Monitor.h"
#include "EnvLoader.h"
#include "ClientAuth.h"
#include "services/ModelResolver.h"
#include "Logger.h"

// Helper to get file size
unsigned long long getFileSize(const std::string& filename) {
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : 0;
}

int main(int argc, char** argv) {
    // 0. Load Environment Variables
    if (!Core::EnvLoader::load()) {
        IC_LOG_WARN("Failed to load .env file. using system environment or defaults.");
    }

    // 0.1 Verify JotaDB Connection (Heartbeat)
    IC_LOG_INFO("JOTADB AUTHENTICATION VERIFICATION started");
    {
        Server::ClientAuth auth;
        IC_LOG_INFO("Connecting to JotaDB...");
        
        if (!auth.verifyConnection()) {
            IC_LOG_ERROR("[FATAL] AUTHENTICATION FAILED. InferenceCenter could not authorize with JotaDB. Please check your JOTA_DB_SK and JOTA_DB_URL configuration.");
            return 1;
        }
        
        IC_LOG_INFO("[SUCCESS] AUTHENTICATION VERIFIED. InferenceCenter is authorized with JotaDB.");
    }

    std::string modelPath;
    std::string initialPrompt;
    int port = 3000;
    int gpuLayers = -1;  // -1 = auto-detect
    int ctxSize = 512;
    
    // Parse arguments
    bool hasNamedArgs = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc) {
            modelPath = argv[++i];
            hasNamedArgs = true;
        } else if (arg == "--prompt" && i + 1 < argc) {
            initialPrompt = argv[++i];
            hasNamedArgs = true;
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
            hasNamedArgs = true;
        } else if (arg == "--gpu-layers" && i + 1 < argc) {
            gpuLayers = std::atoi(argv[++i]);
            hasNamedArgs = true;
        } else if (arg == "--ctx-size" && i + 1 < argc) {
            ctxSize = std::atoi(argv[++i]);
            hasNamedArgs = true;
        } else if (!hasNamedArgs && i == 1) {
            // Backward compatibility: first positional arg is model path
            modelPath = arg;
        } else if (!hasNamedArgs && i == 2) {
            // Backward compatibility: second positional arg is port
            port = std::atoi(arg.c_str());
        }
    }
    
    // 0.2 Check for Model Path / Default Fetching
    Server::ModelResolver resolver;
    std::string finalModelId = "local_model";

    if (modelPath.empty()) {
        IC_LOG_INFO("No --model provided. Attempting to fetch default model from JotaDB...");
        json availableModels;
        if (resolver.fetchAvailableModels(availableModels)) {
            if (availableModels.is_array() && !availableModels.empty()) {
                auto firstModel = availableModels[0];
                if (firstModel.contains("id")) {
                    finalModelId = firstModel["id"].get<std::string>();
                    IC_LOG_INFO("Selected default model", {{"model_id", finalModelId}});
                }
            }
        }

        if (finalModelId != "local_model") {
            Core::EngineConfig dbConfig;
            if (resolver.fetchModelConfig(finalModelId, dbConfig)) {
                IC_LOG_INFO("Retrieved default model configuration from DB.");
                modelPath = dbConfig.modelPath;
                if (gpuLayers == -1) gpuLayers = dbConfig.n_gpu_layers;
                if (ctxSize == 512) ctxSize = dbConfig.ctx_size;
            } else {
                IC_LOG_ERROR("Failed to fetch configuration for default model.");
                return 1;
            }
        } else {
            IC_LOG_ERROR("Could not determine a default model from DB and no --model provided.");
            return 1;
        }
    }

    // 0. Initialize Hardware Monitor
    Hardware::Monitor monitor;
    bool monitorInitialized = monitor.init();
    
    if (!monitorInitialized) {
        IC_LOG_WARN("Failed to initialize Hardware Monitor (NVML).");
    } else {
        auto stats = monitor.updateStats();
        IC_LOG_INFO("GPU STATUS", {
            {"vram_total_mb", stats.memoryTotal / (1024*1024)},
            {"vram_free_mb",  stats.memoryFree / (1024*1024)},
            {"temp_c",        stats.temp}
        });
    }

    // 1. Initialize Engine
    Core::Engine engine;
    
    IC_LOG_INFO("INFERENCE CORE SERVER", {
        {"system_info", engine.getSystemInfo()}
    });

    Core::EngineConfig config;
    config.modelId = finalModelId;
    config.modelPath = modelPath;
    config.ctx_size = ctxSize;
    
    // Smart Split Computing: Auto-detect GPU layers if user didn't specify
    if (gpuLayers == -1) {
        if (monitorInitialized) {
            // Get model file size
            unsigned long long modelSize = getFileSize(modelPath);
            if (modelSize > 0) {
                config.n_gpu_layers = monitor.calculateOptimalGpuLayers(modelSize);
            } else {
                IC_LOG_WARN("Could not determine model size. Using CPU-only.");
                config.n_gpu_layers = 0;
            }
        } else {
            // No monitor, default to CPU-only
            IC_LOG_INFO("Monitor not available. Using CPU-only mode.");
            config.n_gpu_layers = 0;
        }
    } else {
        // User specified GPU layers explicitly
        config.n_gpu_layers = gpuLayers;
    }
    
    
    // Load model silently
    if (!engine.loadModel(config)) {
        IC_LOG_ERROR("[FATAL] MODEL LOADING FAILED", {{"model_path", modelPath}});
        monitor.shutdown();
        return 1;
    }
    
    IC_LOG_INFO("MODEL LOADED SUCCESSFULLY", {
        {"gpu_layers", config.n_gpu_layers},
        {"ctx_size", config.ctx_size}
    });

    // 2. Start WebSocket Server
    Server::WsServer server(engine, monitor, port, config.ctx_size);
    server.run();
    
    monitor.shutdown();
    return 0;
}