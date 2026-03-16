#pragma once

#include "llama.h"
#include "Metrics.h"
#include "Protocol.h"
#include <string>
#include <functional>
#include <atomic>
#include <memory>
#include <mutex>

namespace Core {

    // Callback for streaming tokens. Returns true to continue, false to abort.
    using TokenCallback = std::function<bool(const std::string& token)>;

    enum class SessionState {
        IDLE,
        PREFILLING,   // Actively running prompt evaluation (prefill) — not yet generating tokens
        GENERATING,   // Generating tokens (decode loop)
        ERROR
    };

    class Session {
    public:
        Session(const std::string& session_id, 
                const std::string& client_id,
                const std::string& model_id,
                struct llama_model* model,
                int ctx_size);
        ~Session();

        // Prevent copying
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        // Run inference on this session.
        // Set parse_special=true when the prompt was produced by formatWithChatTemplate
        // (special tokens like <|eot_id|> must be tokenized as their token IDs, and
        // the template already embeds BOS so add_bos is skipped in that case).
        Metrics generate(const std::string& prompt, TokenCallback callback,
                         float temp = 0.7f, float top_p = 0.9f,
                         int max_tokens = -1, const std::string& grammar = "",
                         bool parse_special = false);

        // Abort current generation
        void abort();

        // Session context (chat history, etc.)
        void setContext(Server::SessionContext ctx);
        Server::SessionContext getContext() const;

        // Getters
        std::string getSessionId() const { return session_id_; }
        std::string getClientId() const { return client_id_; }
        std::string getModelId() const { return model_id_; }
        SessionState getState() const { return state_; }
        bool isPrefilling() const { return state_ == SessionState::PREFILLING; }
        bool isGenerating() const { return state_ == SessionState::GENERATING || state_ == SessionState::PREFILLING; }
        
        // Timeout tracking
        int64_t getLastActivityMs() const { return last_activity_ms_.load(); }
        void updateActivity();

        // Format a list of messages using the model's embedded chat template.
        // Returns the formatted prompt string.
        // Returns empty string if the model has no embedded template (caller should fall back).
        std::string formatWithChatTemplate(
            const std::vector<Server::ChatMessage>& messages,
            bool add_assistant_start = true) const;

    private:
        std::string session_id_;
        std::string client_id_;
        std::string model_id_;
        struct llama_context* ctx_ = nullptr;
        struct llama_model* model_ = nullptr;  // Reference to shared model
        SessionState state_ = SessionState::IDLE;
        std::atomic<bool> abort_flag_{false};
        std::atomic<int64_t> last_activity_ms_{0};
        Server::SessionContext context_;
        mutable std::mutex context_mutex_;

        // Helper methods (similar to Engine)
        std::vector<llama_token> tokenize(const std::string& text, bool add_bos, bool parse_special = false);
        std::string tokenToPiece(llama_token token);
    };

}
