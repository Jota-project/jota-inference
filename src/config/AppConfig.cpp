#include "AppConfig.h"
#include "EnvLoader.h"
#include <string>

namespace Core {

    void AppConfig::load() {
        std::string portStr = EnvLoader::get("IC_PORT", "");
        if (!portStr.empty()) {
            server_port = std::stoi(portStr);
        }

        std::string ctxSizeStr = EnvLoader::get("IC_CTX_SIZE", "");
        if (!ctxSizeStr.empty()) {
            ctx_size = std::stoi(ctxSizeStr);
        }

        std::string nBatchStr = EnvLoader::get("IC_N_BATCH", "");
        if (!nBatchStr.empty()) {
            n_batch = std::stoi(nBatchStr);
        }

        std::string nUbatchStr = EnvLoader::get("IC_N_UBATCH", "");
        if (!nUbatchStr.empty()) {
            n_ubatch = std::stoi(nUbatchStr);
        }
    }

}
