#pragma once

#include "../Protocol.h"
#include "../../core/SessionManager.h"
#include "../../core/Metrics.h"
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

namespace Server {

/**
 * InferenceService - Manages thread pool for asynchronous inference execution
 * 
 * Extracts thread pool and task queue logic from WsServer.
 * Executes inference tasks on worker threads and invokes callbacks.
 */
class InferenceService {
public:
    // Token callback: called for each generated token
    // Called from worker thread - caller must handle thread-safety
    using TokenCallback = std::function<void(const std::string& session_id, const std::string& token)>;
    
    // Completion callback: called when inference finishes
    // Called from worker thread - caller must handle thread-safety
    using CompletionCallback = std::function<void(const std::string& session_id, const Core::Metrics& metrics)>;
    
    /**
     * Task submitted for inference execution
     */
    struct Task {
        std::string session_id;
        InferenceParams params;
        TokenCallback onToken;
        CompletionCallback onComplete;
    };
    
    /**
     * Constructor
     * @param sessionManager Pointer to session manager (must outlive this service)
     * @param numWorkers Number of worker threads (default: 4)
     */
    InferenceService(Core::SessionManager* sessionManager, int numWorkers = 4);
    
    /**
     * Destructor - automatically shuts down worker threads
     */
    ~InferenceService();
    
    /**
     * Enqueue a task for asynchronous execution
     * Thread-safe, can be called from any thread
     */
    void enqueueTask(Task task);
    
    /**
     * Gracefully shutdown the service
     * Waits for all pending tasks to complete
     */
    void shutdown();
    
    /**
     * Get number of currently active inference operations
     * Thread-safe
     */
    int getActiveGenerations() const;
    
    /**
     * Get last recorded metrics from any inference
     * Thread-safe
     */
    Core::Metrics getLastMetrics() const;

    /**
     * Abort a running task/session
     */
    bool abortTask(const std::string& session_id);
    
private:
    Core::SessionManager* sessionManager_;
    
    // Task queue
    std::queue<Task> taskQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    
    // Worker threads
    std::atomic<bool> running_{true};
    std::vector<std::thread> workerThreads_;
    
    // Metrics state
    std::atomic<int> activeGenerations_{0};
    Core::Metrics lastMetrics_;
    mutable std::mutex metricsMutex_;
    
    // Worker thread main loop
    void workerLoop();
    
    // Process a single task
    void processTask(Task& task);
};

} // namespace Server
