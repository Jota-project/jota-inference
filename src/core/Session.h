#pragma once

#include "llama.h"
#include "Metrics.h"
#include <string>
#include <functional>
#include <atomic>
#include <memory>

namespace Core {

    // Callback for streaming tokens. Returns true to continue, false to abort.
    using TokenCallback = std::function<bool(const std::string& token)>;

    enum class SessionState {
        IDLE,
        GENERATING,
        ERROR
    };

    class Session {
    public:
        Session(const std::string& session_id, 
                const std::string& client_id,
                struct llama_model* model,
                int ctx_size);
        ~Session();

        // Prevent copying
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        // Run inference on this session
        Metrics generate(const std::string& prompt, TokenCallback callback);

        // Abort current generation
        void abort();

        // Getters
        std::string getSessionId() const { return session_id_; }
        std::string getClientId() const { return client_id_; }
        SessionState getState() const { return state_; }
        bool isGenerating() const { return state_ == SessionState::GENERATING; }

    private:
        std::string session_id_;
        std::string client_id_;
        struct llama_context* ctx_ = nullptr;
        struct llama_model* model_ = nullptr;  // Reference to shared model
        SessionState state_ = SessionState::IDLE;
        std::atomic<bool> abort_flag_{false};

        // Helper methods (similar to Engine)
        std::vector<llama_token> tokenize(const std::string& text, bool add_bos);
        std::string tokenToPiece(llama_token token);
    };

}
