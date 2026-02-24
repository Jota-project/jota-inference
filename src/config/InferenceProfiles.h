#pragma once

#include <string>
#include <unordered_map>

namespace Core {

    struct InferenceProfile {
        float temp        = 0.7f;
        float top_p       = 0.9f;
        int   max_tokens  = 512;
        std::string system_prompt;
    };

    inline const InferenceProfile& getProfile(const std::string& mode) {
        static const std::unordered_map<std::string, InferenceProfile> profiles = {
            {"instant", {
                0.1f,   // temp — near-deterministic
                0.9f,   // top_p
                32,     // max_tokens — ultra short
                "Respond in the fewest words possible. Be direct and concise. "
                "If the answer is yes or no, reply with just that word."
            }},
            {"balanced", {
                0.7f,   // temp
                0.9f,   // top_p
                512,    // max_tokens
                "You are a helpful assistant. Provide clear and informative responses."
            }},
            {"creative", {
                1.0f,   // temp — high creativity
                0.95f,  // top_p — wide sampling
                1024,   // max_tokens — longer output
                "You are a creative and expressive assistant. "
                "Feel free to elaborate, use analogies, and explore ideas."
            }}
        };

        static const InferenceProfile fallback = profiles.at("balanced");

        auto it = profiles.find(mode);
        return (it != profiles.end()) ? it->second : fallback;
    }

}
