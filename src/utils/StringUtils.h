#pragma once

#include <string>

namespace Server {
namespace Utils {

// Helper: Validate and clean UTF-8 string to prevent JSON serialization errors
inline std::string sanitizeUtf8(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    
    for (size_t i = 0; i < input.size(); ) {
        unsigned char c = input[i];
        
        // ASCII (0x00-0x7F)
        if (c <= 0x7F) {
            output += c;
            i++;
        }
        // 2-byte UTF-8 (0xC0-0xDF)
        else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < input.size() && (input[i+1] & 0xC0) == 0x80) {
                output += input[i];
                output += input[i+1];
                i += 2;
            } else {
                i++; // Skip invalid sequence
            }
        }
        // 3-byte UTF-8 (0xE0-0xEF)
        else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < input.size() && (input[i+1] & 0xC0) == 0x80 && (input[i+2] & 0xC0) == 0x80) {
                output += input[i];
                output += input[i+1];
                output += input[i+2];
                i += 3;
            } else {
                i++; // Skip invalid sequence
            }
        }
        // 4-byte UTF-8 (0xF0-0xF7)
        else if ((c & 0xF8) == 0xF0) {
            if (i + 3 < input.size() && (input[i+1] & 0xC0) == 0x80 && 
                (input[i+2] & 0xC0) == 0x80 && (input[i+3] & 0xC0) == 0x80) {
                output += input[i];
                output += input[i+1];
                output += input[i+2];
                output += input[i+3];
                i += 4;
            } else {
                i++; // Skip invalid sequence
            }
        }
        else {
            i++; // Skip invalid start byte
        }
    }
    
    return output;
}

} // namespace Utils
} // namespace Server
