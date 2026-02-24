#pragma once

#include <stdexcept>
#include <string>

namespace Core {

    // Base exception for all InferenceCenter errors
    class InferenceCenterException : public std::runtime_error {
    public:
        explicit InferenceCenterException(const std::string& message, 
                                          const std::string& session_id = "")
            : std::runtime_error(message), session_id_(session_id) {}

        const std::string& sessionId() const noexcept { return session_id_; }

    private:
        std::string session_id_;
    };

    // Errors from the llama.cpp backend (llama_decode failures, context corruption, etc.)
    class InferenceBackendException : public InferenceCenterException {
    public:
        explicit InferenceBackendException(const std::string& message,
                                           const std::string& session_id = "")
            : InferenceCenterException(message, session_id) {}
    };

    // KV Cache exhaustion — "failed to find a memory slot"
    class MemoryFullException : public InferenceCenterException {
    public:
        explicit MemoryFullException(const std::string& message,
                                     const std::string& session_id = "")
            : InferenceCenterException(message, session_id) {}
    };

    // Invalid input parameters from client JSON
    class ValidationException : public InferenceCenterException {
    public:
        explicit ValidationException(const std::string& message,
                                     const std::string& session_id = "")
            : InferenceCenterException(message, session_id) {}
    };

}
