#include "InferenceService.h"
#include "Logger.h"
#include "StringUtils.h"
#include "Exceptions.h"

namespace Server {

InferenceService::InferenceService(Core::SessionManager* sessionManager, int numWorkers)
    : sessionManager_(sessionManager) {
    
    if (!sessionManager_) {
        throw std::invalid_argument("SessionManager cannot be null");
    }
    
    // Start worker threads
    for (int i = 0; i < numWorkers; ++i) {
        workerThreads_.emplace_back([this]() { workerLoop(); });
    }
    
    IC_LOG_INFO("InferenceService started", {{"worker_threads", numWorkers}});
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
    
    IC_LOG_INFO("InferenceService: Shutdown complete");
}

int InferenceService::getActiveGenerations() const {
    return activeGenerations_.load();
}

Core::Metrics InferenceService::getLastMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return lastMetrics_;
}

bool InferenceService::abortTask(const std::string& session_id) {
    if (sessionManager_) {
        return sessionManager_->abortSession(session_id);
    }
    return false;
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
        IC_LOG_ERROR("InferenceService: Session not found", {{"session_id", task.session_id}});
        if (task.onError) {
            task.onError(task.session_id, "Session not found");
        }
        return;
    }

    activeGenerations_++;

    try {
        // Execute inference with token callback
        auto metrics = session->generate(task.params.prompt, [&task](const std::string& token) {
            // Sanitize UTF-8 to prevent JSON serialization errors
            std::string validToken = Utils::sanitizeUtf8(token);
            
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

    } catch (const Core::InferenceCenterException& e) {
        IC_LOG_ERROR("Inference failed", {
            {"session_id", task.session_id},
            {"error", e.what()},
            {"error_type", "InferenceCenterException"}
        });
        if (task.onError) {
            task.onError(task.session_id, e.what());
        }

    } catch (const std::exception& e) {
        IC_LOG_ERROR("Unexpected error during inference", {
            {"session_id", task.session_id},
            {"error", e.what()},
            {"error_type", "std::exception"}
        });
        if (task.onError) {
            task.onError(task.session_id, "Internal server error");
        }
    }
    
    activeGenerations_--;
}

} // namespace Server
