#include "Session.h"
#include "Exceptions.h"
#include "LlamaLogger.h"
#include "Logger.h"
#include <chrono>
#include <cstring>

namespace Core {

    Session::Session(const std::string& session_id, 
                     const std::string& client_id,
                     struct llama_model* model,
                     int ctx_size) 
        : session_id_(session_id), client_id_(client_id), model_(model) {
        
        if (!model_) {
            throw InferenceBackendException("Cannot create session with null model", session_id_);
        }

        // Create context for this session
        auto cparams = llama_context_default_params();
        cparams.n_ctx = ctx_size;
        cparams.n_batch = 512;   // Logical batch size
        cparams.n_ubatch = 512;  // Physical batch size
        
        ctx_ = llama_init_from_model(model_, cparams);
        if (!ctx_) {
            throw InferenceBackendException("Failed to create context for session " + session_id_, session_id_);
        }

        IC_LOG_INFO("Created session", {
            {"session_id", session_id_},
            {"client_id", client_id_}
        });
    }

    Session::~Session() {
        if (ctx_) {
            llama_free(ctx_);
            ctx_ = nullptr;
        }
        IC_LOG_INFO("Destroyed session", {{"session_id", session_id_}});
    }

    void Session::abort() {
        abort_flag_ = true;
    }

    void Session::setContext(Server::SessionContext ctx) {
        std::lock_guard<std::mutex> lock(context_mutex_);
        context_ = std::move(ctx);
        IC_LOG_INFO("Session context set", {
            {"session_id", session_id_},
            {"messages", (int)context_.messages.size()}
        });
    }

    Server::SessionContext Session::getContext() const {
        std::lock_guard<std::mutex> lock(context_mutex_);
        return context_;
    }

    std::vector<llama_token> Session::tokenize(const std::string& text, bool add_bos) {
        int n_tokens_max = text.length() + (add_bos ? 1 : 0) + 1;
        std::vector<llama_token> tokens(n_tokens_max);
        
        const llama_vocab* vocab = llama_model_get_vocab(model_);

        int n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), 
                                       tokens.data(), n_tokens_max, add_bos, false);
        
        if (n_tokens < 0) {
            tokens.resize(-n_tokens);
            n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), 
                                      tokens.data(), tokens.size(), add_bos, false);
        }

        if (n_tokens >= 0) {
            tokens.resize(n_tokens);
        } else {
            tokens.clear();
        }
        
        return tokens;
    }

    std::string Session::tokenToPiece(llama_token token) {
        if (!model_) return "";
        char buf[256];
        const llama_vocab* vocab = llama_model_get_vocab(model_);
        int n = llama_token_to_piece(vocab, token, buf, sizeof(buf), 0, true);
        if (n < 0) {
            return "";
        }
        return std::string(buf, n);
    }

    // --- RAII Guards for llama.cpp resources ---

    struct BatchGuard {
        llama_batch batch;
        BatchGuard(int n_tokens, int embd, int n_seq_max)
            : batch(llama_batch_init(n_tokens, embd, n_seq_max)) {}
        ~BatchGuard() { llama_batch_free(batch); }
        BatchGuard(const BatchGuard&) = delete;
        BatchGuard& operator=(const BatchGuard&) = delete;
    };

    struct SamplerGuard {
        struct llama_sampler* smpl;
        SamplerGuard(struct llama_sampler* s) : smpl(s) {}
        ~SamplerGuard() { if (smpl) llama_sampler_free(smpl); }
        SamplerGuard(const SamplerGuard&) = delete;
        SamplerGuard& operator=(const SamplerGuard&) = delete;
    };

    Metrics Session::generate(const std::string& prompt, TokenCallback callback,
                               float temp, float top_p, int max_tokens) {
        Metrics metrics;
        
        if (!ctx_) {
            state_ = SessionState::ERROR;
            throw InferenceBackendException("Session context is null", session_id_);
        }

        state_ = SessionState::GENERATING;
        abort_flag_ = false;

        // Clear KV cache
        llama_memory_clear(llama_get_memory(ctx_), false);

        auto start_time = std::chrono::high_resolution_clock::now();
        bool is_first_token = true;

        // 1. Tokenize
        std::vector<llama_token> tokens_list = tokenize(prompt, true);

        // 2. Prepare Sampler (RAII) — dynamic chain based on temp
        auto sparams = llama_sampler_chain_default_params();
        SamplerGuard samplerGuard(llama_sampler_chain_init(sparams));
        
        if (temp <= 0.0f) {
            // Deterministic: greedy sampling
            llama_sampler_chain_add(samplerGuard.smpl, llama_sampler_init_greedy());
        } else {
            // Stochastic: top_p → temperature → distribution
            llama_sampler_chain_add(samplerGuard.smpl, llama_sampler_init_top_p(top_p, 1));
            llama_sampler_chain_add(samplerGuard.smpl, llama_sampler_init_temp(temp));
            llama_sampler_chain_add(samplerGuard.smpl, llama_sampler_init_dist(0));
        }

        // 3. Prepare Batch (RAII)
        BatchGuard batchGuard(std::max((int)tokens_list.size(), 1), 0, 1);
        auto& batch = batchGuard.batch;

        // Load prompt
        batch.n_tokens = 0;
        for (size_t i = 0; i < tokens_list.size(); i++) {
            batch.token[i] = tokens_list[i];
            batch.pos[i] = i;
            batch.n_seq_id[i] = 1;
            batch.seq_id[i][0] = 0;
            batch.logits[i] = false;
            batch.n_tokens++;
        }
        
        if (batch.n_tokens > 0) {
            batch.logits[batch.n_tokens - 1] = true;
        }

        if (llama_decode(ctx_, batch) != 0) {
            state_ = SessionState::ERROR;
            IC_LOG_ERROR("llama_decode failed (prompt eval)", {{"session_id", session_id_}});
            if (wasMemorySlotError()) {
                throw MemoryFullException("KV Cache full during prompt evaluation", session_id_);
            }
            throw InferenceBackendException("llama_decode failed during prompt evaluation", session_id_);
        }

        // 4. Generation Loop
        int n_cur = batch.n_tokens;
        const llama_vocab* vocab = llama_model_get_vocab(model_);

        while (true) {
            if (abort_flag_) break;

            llama_token new_token_id = llama_sampler_sample(samplerGuard.smpl, ctx_, -1);
            llama_sampler_accept(samplerGuard.smpl, new_token_id);

            if (is_first_token) {
                auto now = std::chrono::high_resolution_clock::now();
                metrics.ttft_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                is_first_token = false;
            }

            if (llama_vocab_is_eog(vocab, new_token_id)) break;

            std::string piece = tokenToPiece(new_token_id);
            metrics.tokens_generated++;

            // Enforce max_tokens limit
            if (max_tokens > 0 && metrics.tokens_generated >= max_tokens) {
                if (callback) callback(piece);
                break;
            }

            if (callback) {
                if (!callback(piece)) break;
            }

            batch.n_tokens = 0;
            batch.token[0] = new_token_id;
            batch.pos[0] = n_cur;
            batch.n_seq_id[0] = 1;
            batch.seq_id[0][0] = 0;
            batch.logits[0] = true;
            batch.n_tokens = 1;
            n_cur++;

            if (llama_decode(ctx_, batch) != 0) {
                state_ = SessionState::ERROR;
                IC_LOG_ERROR("llama_decode failed during generation", {{"session_id", session_id_}});
                if (wasMemorySlotError()) {
                    throw MemoryFullException("KV Cache full during token generation", session_id_);
                }
                throw InferenceBackendException("llama_decode failed during token generation", session_id_);
            }
        }
        
        // Finalize Metrics
        auto end_time = std::chrono::high_resolution_clock::now();
        metrics.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        if (metrics.total_time_ms > 0) {
            metrics.tps = (double)metrics.tokens_generated / (metrics.total_time_ms / 1000.0);
        }

        // BatchGuard & SamplerGuard clean up automatically
        state_ = SessionState::IDLE;
        return metrics;
    }

}
