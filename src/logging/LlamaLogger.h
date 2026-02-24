#pragma once

namespace Core {

    // Initializes the llama.cpp log callback.
    // Must be called BEFORE llama_backend_init().
    void initLlamaLogging();

    // Returns true (and clears the flag) if llama.cpp recently 
    // reported a "failed to find a memory slot" error.
    // Used by Session::generate() to throw MemoryFullException.
    bool wasMemorySlotError();

}
