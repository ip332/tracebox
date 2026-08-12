#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "logger.pb.h"
#include "storage.h"

namespace tracebox {
namespace logger {

class LogWriter {
    std::queue<LogRequest> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool stopping_ = false;
    std::shared_ptr<Storage> storage_;
    std::thread thread_;

    // The mutex protects stopping_ and queue_ for the entire predicate.
    std::unique_ptr<LogRequest> wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
        if (queue_.empty()) {
            return nullptr;
        }
        auto result = std::make_unique<LogRequest>(queue_.front());
        queue_.pop();
        return result;
    }

    void writingThread();

   public:
    explicit LogWriter(std::shared_ptr<Storage> storage);
    ~LogWriter();

    // Accepted entries are drained. Returns false after shutdown begins.
    bool add(const LogRequest& request);
    // Stops admission, drains accepted entries, and joins the worker.
    void stop();

    LogWriter(const LogWriter&) = delete;
    LogWriter& operator=(const LogWriter&) = delete;
    LogWriter(LogWriter&&) = delete;
    LogWriter& operator=(LogWriter&&) = delete;
};

}  // namespace logger
}  // namespace tracebox
