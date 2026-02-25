#include "InferenceService.h"
#include "Logger.h"
#include "StringUtils.h"
#include "Exceptions.h"
#include "InferenceProfiles.h"
#include "ThoughtFilter.h"

namespace Server {

InferenceService::InferenceService(Core::SessionManager* sessionManager, int numWorkers)
    : sessionManager_(sessionManager) {
    
    if (!sessionManager_) {
        throw std::invalid_argument("SessionManager cannot be null");
    }
    
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
        return;
    }
    
    queueCv_.notify_all();
    
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
    auto* session = sessionManager_->getSession(task.session_id);
    if (!session) {
        IC_LOG_ERROR("InferenceService: Session not found", {{"session_id", task.session_id}});
        if (task.onError) {
            task.onError(task.session_id, "Session not found");
        }
        return;
    }

    // --- Profile Resolution ---
    float temp       = task.params.temp;
    float top_p      = task.params.top_p;
    int   max_tokens = task.params.max_tokens;
    std::string system_prompt = task.params.system_prompt;

    if (!task.params.mode.empty()) {
        const auto& profile = Core::getProfile(task.params.mode);
        // Profile defaults apply only when client didn't override
        if (task.params.temp == 0.7f)       temp = profile.temp;
        if (task.params.top_p == 0.9f)      top_p = profile.top_p;
        if (task.params.max_tokens == -1)   max_tokens = profile.max_tokens;
        if (system_prompt.empty())          system_prompt = profile.system_prompt;

        IC_LOG_DEBUG("Profile resolved", {
            {"mode", task.params.mode},
            {"temp", temp},
            {"top_p", top_p},
            {"max_tokens", max_tokens}
        });
    }

    // --- System Prompt Concatenation ---
    std::string final_prompt = task.params.prompt;
    if (!system_prompt.empty()) {
        final_prompt = system_prompt + "\n\n" + final_prompt;
    }

    activeGenerations_++;

    // --- ThoughtFilter wraps the token callback ---
    ::Utils::ThoughtFilter filter([&task](const std::string& text, const std::string& type) {
        if (task.onToken) {
            task.onToken(task.session_id, text, type);
        }
    });

    try {
        auto metrics = session->generate(final_prompt, [&filter](const std::string& token) {
            std::string validToken = Utils::sanitizeUtf8(token);
            filter.feed(validToken);
            return true;
        }, temp, top_p, max_tokens);

        // Flush any remaining buffered tokens (e.g. partial tag at end)
        filter.flush();

        {
            std::lock_guard<std::mutex> lock(metricsMutex_);
            lastMetrics_ = metrics;
        }

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
