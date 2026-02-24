#include "LlamaLogger.h"
#include "Logger.h"
#include "llama.h"
#include <string>
#include <mutex>
#include <atomic>

namespace Core {

    // --- Thread-safe llama.cpp Log Interception ---
    static std::mutex s_log_mutex;
    static std::string s_log_buffer;
    static ggml_log_level s_log_buffer_level = GGML_LOG_LEVEL_INFO;

    // Atomic flag set when "failed to find a memory slot" is detected.
    // Session::generate() checks and clears this after a llama_decode failure
    // to throw the correct exception type (MemoryFullException vs InferenceBackendException).
    static std::atomic<bool> s_memory_slot_error{false};

    bool wasMemorySlotError() {
        return s_memory_slot_error.exchange(false); // read & clear
    }

    static void flushLogBuffer(const std::string& message, ggml_log_level level) {
        if (message.empty()) return;

        nlohmann::json meta = {{"origin", "llama.cpp_internal"}};

        // Detect KV Cache exhaustion
        if (message.find("failed to find a memory slot") != std::string::npos) {
            meta["error_category"] = "KV_CACHE_FULL";
            meta["action_required"] = "clear_session";
            s_memory_slot_error.store(true);
        }

        switch (level) {
            case GGML_LOG_LEVEL_ERROR:
                IC_LOG_ERROR(message, meta);
                break;
            case GGML_LOG_LEVEL_WARN:
                IC_LOG_WARN(message, meta);
                break;
            case GGML_LOG_LEVEL_INFO: {
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

        std::lock_guard<std::mutex> lock(s_log_mutex);

        std::string fragment(text);

        if (level == GGML_LOG_LEVEL_CONT) {
            s_log_buffer += fragment;
        } else {
            if (!s_log_buffer.empty()) {
                std::string pending = stripTrailing(std::move(s_log_buffer));
                s_log_buffer.clear();
                flushLogBuffer(pending, s_log_buffer_level);
            }
            s_log_buffer = fragment;
            s_log_buffer_level = level;
        }

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
