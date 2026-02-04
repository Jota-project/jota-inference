#include "Session.h"
#include <iostream>
#include <chrono>
#include <cstring>

namespace Core {

    Session::Session(const std::string& session_id, 
                     const std::string& client_id,
                     struct llama_model* model,
                     int ctx_size) 
        : session_id_(session_id), client_id_(client_id), model_(model) {
        
        if (!model_) {
            throw std::runtime_error("Cannot create session with null model");
        }

        // Create context for this session
        auto cparams = llama_context_default_params();
        cparams.n_ctx = ctx_size;
        
        ctx_ = llama_init_from_model(model_, cparams);
        if (!ctx_) {
            throw std::runtime_error("Failed to create context for session " + session_id_);
        }

        std::cout << "Created session " << session_id_ 
                  << " for client " << client_id_ << std::endl;
    }

    Session::~Session() {
        if (ctx_) {
            llama_free(ctx_);
            ctx_ = nullptr;
        }
        std::cout << "Destroyed session " << session_id_ << std::endl;
    }

    void Session::abort() {
        abort_flag_ = true;
    }

    std::vector<llama_token> Session::tokenize(const std::string& text, bool add_bos) {
        // Upper limit for the number of tokens
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

    Metrics Session::generate(const std::string& prompt, TokenCallback callback) {
        Metrics metrics;
        
        if (!ctx_) {
            state_ = SessionState::ERROR;
            return metrics;
        }

        state_ = SessionState::GENERATING;
        abort_flag_ = false;

        // Clear KV cache
        llama_memory_clear(llama_get_memory(ctx_), false);

        auto start_time = std::chrono::high_resolution_clock::now();
        bool is_first_token = true;

        // 1. Tokenize
        std::vector<llama_token> tokens_list = tokenize(prompt, true);

        // 2. Prepare Sampler
        auto sparams = llama_sampler_chain_default_params();
        struct llama_sampler* smpl = llama_sampler_chain_init(sparams);
        llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

        // 3. Prepare Batch
        llama_batch batch = llama_batch_init(std::max((int)tokens_list.size(), 1), 0, 1);

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
        
        // Precise logits for the last token
        if (batch.n_tokens > 0) {
            batch.logits[batch.n_tokens - 1] = true;
        }

        if (llama_decode(ctx_, batch) != 0) {
            std::cerr << "llama_decode failed for session " << session_id_ << std::endl;
            llama_batch_free(batch);
            llama_sampler_free(smpl);
            state_ = SessionState::ERROR;
            return metrics;
        }

        // 4. Generation Loop
        int n_cur = batch.n_tokens;
        const llama_vocab* vocab = llama_model_get_vocab(model_);

        while (true) {
            // Check abort
            if (abort_flag_) {
                break;
            }

            // Sample
            llama_token new_token_id = llama_sampler_sample(smpl, ctx_, -1);
            
            // Accept/Callbacks
            llama_sampler_accept(smpl, new_token_id);

            // Time to First Token
            if (is_first_token) {
                auto now = std::chrono::high_resolution_clock::now();
                metrics.ttft_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
                is_first_token = false;
            }

            if (llama_vocab_is_eog(vocab, new_token_id)) {
                break;
            }

            std::string piece = tokenToPiece(new_token_id);
            metrics.tokens_generated++;

            if (callback) {
                if (!callback(piece)) break; // User aborted
            }

            // Prepare next batch for single token
            batch.n_tokens = 0;
            
            batch.token[0] = new_token_id;
            batch.pos[0] = n_cur;
            batch.n_seq_id[0] = 1;
            batch.seq_id[0][0] = 0;
            batch.logits[0] = true;
            batch.n_tokens = 1;
            
            n_cur++;

            if (llama_decode(ctx_, batch) != 0) {
                std::cerr << "llama_decode failed during generation for session " 
                          << session_id_ << std::endl;
                break;
            }
        }
        
        // Finalize Metrics
        auto end_time = std::chrono::high_resolution_clock::now();
        metrics.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        if (metrics.total_time_ms > 0) {
            metrics.tps = (double)metrics.tokens_generated / (metrics.total_time_ms / 1000.0);
        }

        // Cleanup
        llama_batch_free(batch);
        llama_sampler_free(smpl);
        
        state_ = SessionState::IDLE;
        return metrics;
    }

}
