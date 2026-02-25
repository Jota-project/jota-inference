#pragma once

#include <string>
#include <functional>
#include <vector>

namespace Utils {

    /**
     * ThoughtFilter — Streaming tag detector for <think>/<thought> blocks.
     *
     * Tokens arrive one-at-a-time. A tag like "<think>" may span multiple
     * tokens (e.g. "<", "think", ">"). This class buffers when it sees '<'
     * and resolves once the tag is confirmed or rejected.
     *
     * Usage:
     *   ThoughtFilter filter([&](const std::string& text, const std::string& type) {
     *       // type is "thought" or "content"
     *       sendToClient(text, type);
     *   });
     *   for each token: filter.feed(token);
     *   filter.flush(); // after generation ends
     */
    class ThoughtFilter {
    public:
        using EmitCallback = std::function<void(const std::string& text, const std::string& type)>;

        explicit ThoughtFilter(EmitCallback cb)
            : emit_(std::move(cb)) {}

        void feed(const std::string& token) {
            // Append to working buffer
            buffer_ += token;

            // Process buffer character by character
            processBuffer();
        }

        /// Call after generation ends to flush any remaining buffered text
        void flush() {
            if (!buffer_.empty()) {
                emit_(buffer_, currentType());
                buffer_.clear();
            }
        }

    private:
        EmitCallback emit_;
        std::string buffer_;
        bool in_thought_ = false;

        // Known tags (case-insensitive matching done via lowered buffer)
        static constexpr const char* OPEN_TAGS[] = {"<think>", "<thought>"};
        static constexpr const char* CLOSE_TAGS[] = {"</think>", "</thought>"};

        std::string currentType() const {
            return in_thought_ ? "thought" : "content";
        }

        static std::string toLower(const std::string& s) {
            std::string r = s;
            for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return r;
        }

        void processBuffer() {
            while (!buffer_.empty()) {
                // Find the next '<' in the buffer
                size_t lt = buffer_.find('<');

                if (lt == std::string::npos) {
                    // No '<' at all — emit everything
                    emit_(buffer_, currentType());
                    buffer_.clear();
                    return;
                }

                // Emit text before the '<'
                if (lt > 0) {
                    emit_(buffer_.substr(0, lt), currentType());
                    buffer_.erase(0, lt);
                }

                // Now buffer_ starts with '<'. Try to match a tag.
                std::string lower = toLower(buffer_);

                // Check if buffer matches a complete tag
                bool matched = false;

                // Try open tags
                for (const char* tag : OPEN_TAGS) {
                    std::string t(tag);
                    if (lower == t) {
                        // Exact match — consume tag, switch to thought
                        in_thought_ = true;
                        buffer_.clear();
                        matched = true;
                        break;
                    }
                    if (t.substr(0, lower.size()) == lower && lower.size() < t.size()) {
                        // Partial prefix match — need more tokens
                        return;
                    }
                }
                if (matched) continue;

                // Try close tags
                for (const char* tag : CLOSE_TAGS) {
                    std::string t(tag);
                    if (lower == t) {
                        // Exact match — consume tag, switch to content
                        in_thought_ = false;
                        buffer_.clear();
                        matched = true;
                        break;
                    }
                    if (t.substr(0, lower.size()) == lower && lower.size() < t.size()) {
                        // Partial prefix match — need more tokens
                        return;
                    }
                }
                if (matched) continue;

                // No match and no prefix match — the '<' is just literal text.
                // Emit it and continue processing the rest.
                emit_(buffer_.substr(0, 1), currentType());
                buffer_.erase(0, 1);
            }
        }
    };

    // Static member definitions (C++14-compatible)
    constexpr const char* ThoughtFilter::OPEN_TAGS[];
    constexpr const char* ThoughtFilter::CLOSE_TAGS[];

}
