#pragma once

#include <condition_variable>
#include <queue>
#include <thread>

#include "logger.pb.h"
#include "storage.h"

namespace embark {
namespace logger {

// This class stores incoming logging requests into a queue and implements a
// dedicated thread to flush the queue content into the file system using
// Storage class.
class LogWriter {
    // Thread controlling flag
    bool running_ = false;
    // All incoming requests will be added to the queue first.
    std::queue<LogRequest> queue_;
    // Push/pop operations will be invoked from different threads hence we need
    // a mutex
    std::mutex mutex_;
    // A condition variable is required to wake up the writing thread when new
    // item is added to the queue
    std::condition_variable condition_;
    // An extra boolean flag to interrupt waiting
    bool force_wakeup_ = false;
    // This object implements all file system related functionality.
    std::shared_ptr<Storage> storage_;
    // Writing thread
    std::thread thread_;

    // Waiting for a new entry in the queue
    std::unique_ptr<LogRequest> wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock,
                        [this] { return force_wakeup_ || !queue_.empty(); });
        std::unique_ptr<LogRequest> result;
        if (!queue_.empty()) {
            result = std::make_unique<LogRequest>(queue_.front());
            queue_.pop();
        }
        return result;
    }
    // Notifies waiting thread about a new entry in the queue
    void notify() {
        std::lock_guard<std::mutex> lck(mutex_);
        condition_.notify_one();
    }
    // Dedicated thread to push request to the file system.
    void writingThread();

   public:
    explicit LogWriter(std::shared_ptr<Storage> storage);
    ~LogWriter();

    // Adds request to the queue and triggers flushing it to the file system.
    void add(const LogRequest& request);

    // Delete copy / assignment constructors
    LogWriter(const LogWriter&) = delete;
    LogWriter& operator=(const LogWriter&) = delete;
    LogWriter(LogWriter&&) = delete;
    LogWriter& operator=(LogWriter&&) = delete;
};

}  // namespace logger
}  // namespace embark
