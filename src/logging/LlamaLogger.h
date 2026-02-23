#pragma once

namespace Core {

    // Initializes the llama.cpp log callback.
    // Must be called BEFORE llama_backend_init().
    // Redirects all llama.cpp output through the IC_LOG system,
    // with special handling for KV Cache exhaustion errors.
    void initLlamaLogging();

}
