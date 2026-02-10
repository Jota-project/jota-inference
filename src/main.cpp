#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include "Engine.h"
#include "WsServer.h"
#include "Monitor.h"
#include "EnvLoader.h"
#include "ClientAuth.h"

// Helper to get file size
unsigned long long getFileSize(const std::string& filename) {
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : 0;
}

int main(int argc, char** argv) {
    // 0. Load Environment Variables
    if (!Core::EnvLoader::load()) {
        std::cerr << "WARNING: Failed to load .env file. using system environment or defaults." << std::endl;
    }

    // 0.1 Verify JotaDB Connection (Heartbeat)
    std::cout << "========================================" << std::endl;
    std::cout << "  JOTADB AUTHENTICATION VERIFICATION" << std::endl;
    std::cout << "========================================" << std::endl;
    {
        Server::ClientAuth auth;
        std::cout << "Connecting to JotaDB..." << std::endl;
        
        if (!auth.verifyConnection()) {
            std::cerr << std::endl;
            std::cerr << "❌ [FATAL] AUTHENTICATION FAILED" << std::endl;
            std::cerr << "   InferenceCenter could not authorize with JotaDB." << std::endl;
            std::cerr << "   Please check your JOTA_DB_SK and JOTA_DB_URL configuration." << std::endl;
            std::cerr << "========================================" << std::endl;
            return 1;
        }
        
        std::cout << std::endl;
        std::cout << "✅ [SUCCESS] AUTHENTICATION VERIFIED" << std::endl;
        std::cout << "   InferenceCenter is authorized with JotaDB." << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;
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
    
    if (modelPath.empty()) {
        std::cerr << "Usage: " << argv[0] << " --model <path_to_model.gguf> [--prompt \"text\"] [--port 3000] [--gpu-layers N] [--ctx-size 512]" << std::endl;
        std::cerr << "  Or (legacy): " << argv[0] << " <path_to_model.gguf> [port]" << std::endl;
        return 1;
    }

    // 0. Initialize Hardware Monitor
    Hardware::Monitor monitor;
    bool monitorInitialized = monitor.init();
    
    if (!monitorInitialized) {
        std::cerr << "WARNING: Failed to initialize Hardware Monitor (NVML)." << std::endl;
    } else {
        auto stats = monitor.updateStats();
        std::cout << "--- GPU STATUS ---" << std::endl;
        std::cout << "VRAM Total: " << stats.memoryTotal / (1024*1024) << " MB" << std::endl;
        std::cout << "VRAM Free:  " << stats.memoryFree / (1024*1024) << " MB" << std::endl;
        std::cout << "Temp:       " << stats.temp << " C" << std::endl;
        std::cout << "------------------" << std::endl;
    }

    // 1. Initialize Engine
    Core::Engine engine;
    
    std::cout << "--- INFERENCE CORE SERVER ---" << std::endl;
    std::cout << engine.getSystemInfo() << std::endl;

    Core::EngineConfig config;
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
                std::cerr << "WARNING: Could not determine model size. Using CPU-only." << std::endl;
                config.n_gpu_layers = 0;
            }
        } else {
            // No monitor, default to CPU-only
            std::cout << "Monitor not available. Using CPU-only mode." << std::endl;
            config.n_gpu_layers = 0;
        }
    } else {
        // User specified GPU layers explicitly
        config.n_gpu_layers = gpuLayers;
    }
    
    
    // Load model silently
    if (!engine.loadModel(config)) {
        std::cerr << std::endl;
        std::cerr << "========================================" << std::endl;
        std::cerr << "❌ [FATAL] MODEL LOADING FAILED" << std::endl;
        std::cerr << "   Could not load model: " << modelPath << std::endl;
        std::cerr << "========================================" << std::endl;
        monitor.shutdown();
        return 1;
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "✅ MODEL LOADED SUCCESSFULLY" << std::endl;
    std::cout << "   GPU Layers: " << config.n_gpu_layers << std::endl;
    std::cout << "   Context Size: " << config.ctx_size << " tokens" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // 2. Start WebSocket Server
    Server::WsServer server(engine, monitor, port, config.ctx_size);
    server.run();
    
    monitor.shutdown();
    return 0;
}