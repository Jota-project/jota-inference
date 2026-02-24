#include "Monitor.h"
#include "Logger.h"

namespace Hardware {

    Monitor::Monitor() {
        // Initialize stats to 0
        currentStats = {};
    }

    Monitor::~Monitor() {
        shutdown();
    }

    bool Monitor::init() {
#ifdef USE_CUDA
        nvmlReturn_t result = nvmlInit();
        if (NVML_SUCCESS != result) {
            IC_LOG_ERROR("Failed to initialize NVML", {{"error", std::string(nvmlErrorString(result))}});
            return false;
        }

        // Get handle for first device (index 0)
        // Assuming single GPU setup (GTX 1060)
        result = nvmlDeviceGetHandleByIndex(0, &device);
        if (NVML_SUCCESS != result) {
            IC_LOG_ERROR("Failed to get GPU device handle", {{"error", std::string(nvmlErrorString(result))}});
            nvmlShutdown();
            return false;
        }

        // Verify device name
        char name[64];
        if (NVML_SUCCESS == nvmlDeviceGetName(device, name, sizeof(name))) {
            IC_LOG_INFO("GPU Monitor initialized", {{"gpu_name", std::string(name)}});
        }

        initialized = true;
        return true;
#else
        IC_LOG_INFO("CUDA support not compiled. Running in CPU-only mode.");
        return false;
#endif
    }

    void Monitor::shutdown() {
#ifdef USE_CUDA
        if (initialized) {
            nvmlShutdown();
            initialized = false;
        }
#endif
    }

    GpuStats Monitor::updateStats() {
        if (!initialized) return currentStats;

#ifdef USE_CUDA
        nvmlReturn_t result;
        nvmlMemory_t memory;
        unsigned int temp;
        unsigned int power;
        unsigned int fan;

        // Memory
        result = nvmlDeviceGetMemoryInfo(device, &memory);
        if (NVML_SUCCESS == result) {
            currentStats.memoryTotal = memory.total;
            currentStats.memoryFree = memory.free;
            currentStats.memoryUsed = memory.used;
        }

        // Temperature
        result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temp);
        if (NVML_SUCCESS == result) {
            currentStats.temp = temp;
        }

        // Power
        result = nvmlDeviceGetPowerUsage(device, &power);
        if (NVML_SUCCESS == result) {
            currentStats.powerUsage = power;
        }

        // Fan Speed
        result = nvmlDeviceGetFanSpeed(device, &fan);
        if (NVML_SUCCESS == result) {
            currentStats.fanSpeed = fan;
        }

        // Throttle Check
        if (currentStats.temp >= MAX_TEMP_SAFE) {
            if (!currentStats.throttle) {
                IC_LOG_WARN("GPU Temperature exceeds limit", {
                    {"temp", (int)currentStats.temp},
                    {"max_safe", (int)MAX_TEMP_SAFE}
                });
            }
            currentStats.throttle = true;
        } else {
            if (currentStats.throttle) {
                IC_LOG_INFO("GPU Temperature normalized", {{"temp", (int)currentStats.temp}});
            }
            currentStats.throttle = false;
        }
#endif

        return currentStats;
    }

    bool Monitor::isThrottling() const {
        return currentStats.throttle;
    }

    int Monitor::calculateOptimalGpuLayers(unsigned long long modelSizeBytes) {
#ifdef USE_CUDA
        if (!initialized) {
            IC_LOG_ERROR("Monitor not initialized. Cannot calculate GPU layers.");
            return -1;
        }

        // Update stats to get current VRAM
        updateStats();

        // Safety buffer: 500MB to prevent OOM
        const unsigned long long SAFETY_BUFFER_MB = 500;
        const unsigned long long SAFETY_BUFFER_BYTES = SAFETY_BUFFER_MB * 1024 * 1024;

        // Calculate available VRAM with buffer
        unsigned long long availableVRAM = 0;
        if (currentStats.memoryFree > SAFETY_BUFFER_BYTES) {
            availableVRAM = currentStats.memoryFree - SAFETY_BUFFER_BYTES;
        } else {
            IC_LOG_WARN("Insufficient VRAM available", {
                {"free_mb", (int)(currentStats.memoryFree / (1024*1024))}
            });
            return 0; // No GPU layers
        }

        IC_LOG_INFO("Smart Split Computing", {
            {"vram_total_mb", (int)(currentStats.memoryTotal / (1024*1024))},
            {"vram_free_mb", (int)(currentStats.memoryFree / (1024*1024))},
            {"safety_buffer_mb", (int)SAFETY_BUFFER_MB},
            {"available_mb", (int)(availableVRAM / (1024*1024))}
        });

        // If entire model fits in VRAM, use all layers (return 99 = max)
        if (modelSizeBytes <= availableVRAM) {
            IC_LOG_INFO("Model fits entirely in GPU", {
                {"model_size_mb", (int)(modelSizeBytes / (1024*1024))}
            });
            return 99;
        }

        // Estimate layers based on proportion
        // Typical 7B model has ~32 layers, 3B has ~28 layers, 1B has ~22 layers
        // We'll estimate based on model size
        int estimatedTotalLayers = 32; // Default assumption for 7B models
        if (modelSizeBytes < 2ULL * 1024 * 1024 * 1024) { // < 2GB
            estimatedTotalLayers = 22; // Small model (1B)
        } else if (modelSizeBytes < 4ULL * 1024 * 1024 * 1024) { // < 4GB
            estimatedTotalLayers = 28; // Medium model (3B)
        }

        // Calculate proportion of layers that fit
        double proportion = (double)availableVRAM / (double)modelSizeBytes;
        int recommendedLayers = (int)(proportion * estimatedTotalLayers);

        // Ensure at least 1 layer on GPU if there's any VRAM available
        if (recommendedLayers < 1 && availableVRAM > 0) {
            recommendedLayers = 1;
        }

        IC_LOG_INFO("GPU layer split calculated", {
            {"model_size_mb", (int)(modelSizeBytes / (1024*1024))},
            {"estimated_total_layers", estimatedTotalLayers},
            {"recommended_gpu_layers", recommendedLayers},
            {"gpu_percent", (int)(proportion * 100)}
        });

        return recommendedLayers;
#else
        IC_LOG_INFO("CUDA not compiled. Cannot use GPU layers.");
        return 0; // CPU-only
#endif
    }

}
