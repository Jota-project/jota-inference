#include "LlamaLogger.h"
#include "Logger.h"
#include "llama.h"
#include <string>

namespace Core {

    // --- llama.cpp Log Interception ---
    // Static line buffer to accumulate fragmented messages from llama.cpp.
    // llama.cpp may send a single logical message across multiple callback
    // invocations (using GGML_LOG_LEVEL_CONT), so we buffer until we see '\n'.
    static std::string s_log_buffer;
    static ggml_log_level s_log_buffer_level = GGML_LOG_LEVEL_INFO;

    static void flushLogBuffer(const std::string& message, ggml_log_level level) {
        if (message.empty()) return;

        nlohmann::json meta = {{"origin", "llama.cpp_internal"}};

        // Detect KV Cache exhaustion
        if (message.find("failed to find a memory slot") != std::string::npos) {
            meta["error_category"] = "KV_CACHE_FULL";
            meta["action_required"] = "clear_session";
        }

        // Dispatch based on level
        switch (level) {
            case GGML_LOG_LEVEL_ERROR:
                IC_LOG_ERROR(message, meta);
                break;
            case GGML_LOG_LEVEL_WARN:
                IC_LOG_WARN(message, meta);
                break;
            case GGML_LOG_LEVEL_INFO: {
                // Filter verbose INFO: only emit if it contains success keywords
                // or if LOG_LEVEL is DEBUG
                bool is_important = (message.find("model loaded") != std::string::npos)
                                 || (message.find("offloaded") != std::string::npos)
                                 || (message.find("layers") != std::string::npos);
                if (is_important) {
                    IC_LOG_INFO(message, meta);
                } else {
                    IC_LOG_DEBUG(message, meta);
                }
                break;
            }
            default:
                IC_LOG_DEBUG(message, meta);
                break;
        }
    }

    static std::string stripTrailing(std::string s) {
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) {
            s.pop_back();
        }
        return s;
    }

    static void llama_log_callback(ggml_log_level level, const char* text, void* /*user_data*/) {
        if (!text) return;

        std::string fragment(text);

        // CONT means "continuation of previous message" — just append.
        if (level == GGML_LOG_LEVEL_CONT) {
            s_log_buffer += fragment;
        } else {
            // If there was a pending buffered message from a previous level, flush it first.
            if (!s_log_buffer.empty()) {
                std::string pending = stripTrailing(std::move(s_log_buffer));
                s_log_buffer.clear();
                flushLogBuffer(pending, s_log_buffer_level);
            }
            // Start a new buffer with the current fragment
            s_log_buffer = fragment;
            s_log_buffer_level = level;
        }

        // Check if the buffer contains a complete line (ends with '\n')
        if (!s_log_buffer.empty() && s_log_buffer.back() == '\n') {
            std::string message = stripTrailing(std::move(s_log_buffer));
            ggml_log_level msg_level = s_log_buffer_level;
            s_log_buffer.clear();
            flushLogBuffer(message, msg_level);
        }
    }

    void initLlamaLogging() {
        llama_log_set(llama_log_callback, nullptr);
    }

}
