#include "InferenceService.h"
#include <iostream>

namespace Server {

// Helper: Validate and clean UTF-8 string to prevent JSON serialization errors
static std::string sanitizeUtf8(const std::string& input) {
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

InferenceService::InferenceService(Core::SessionManager* sessionManager, int numWorkers)
    : sessionManager_(sessionManager) {
    
    if (!sessionManager_) {
        throw std::invalid_argument("SessionManager cannot be null");
    }
    
    // Start worker threads
    for (int i = 0; i < numWorkers; ++i) {
        workerThreads_.emplace_back([this]() { workerLoop(); });
    }
    
    std::cout << "InferenceService: Started with " << numWorkers << " worker threads" << std::endl;
}

InferenceService::~InferenceService() {
    shutdown();
}

void InferenceService::enqueueTask(Task task) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        taskQueue_.push(std::move(task));
    }
    queueCv_.notify_one();
}

void InferenceService::shutdown() {
    if (!running_.exchange(false)) {
        return; // Already shutting down
    }
    
    // Wake up all worker threads
    queueCv_.notify_all();
    
    // Wait for all workers to finish
    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    std::cout << "InferenceService: Shutdown complete" << std::endl;
}

int InferenceService::getActiveGenerations() const {
    return activeGenerations_.load();
}

Core::Metrics InferenceService::getLastMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return lastMetrics_;
}

void InferenceService::workerLoop() {
    while (running_) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this] { return !taskQueue_.empty() || !running_; });

            if (!running_) break;

            task = std::move(taskQueue_.front());
            taskQueue_.pop();
        }

        processTask(task);
    }
}

void InferenceService::processTask(Task& task) {
    // Get the session
    auto* session = sessionManager_->getSession(task.session_id);
    if (!session) {
        std::cerr << "InferenceService: Session not found: " << task.session_id << std::endl;
        return;
    }

    activeGenerations_++;

    // Execute inference with token callback
    auto metrics = session->generate(task.params.prompt, [&task](const std::string& token) {
        // Sanitize UTF-8 to prevent JSON serialization errors
        std::string validToken = sanitizeUtf8(token);
        
        // Call user callback
        if (task.onToken) {
            task.onToken(task.session_id, validToken);
        }
        
        return true; // Continue generation
    });

    // Store metrics for broadcasting
    {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        lastMetrics_ = metrics;
    }

    // Call completion callback
    if (task.onComplete) {
        task.onComplete(task.session_id, metrics);
    }
    
    activeGenerations_--;
}

} // namespace Server
