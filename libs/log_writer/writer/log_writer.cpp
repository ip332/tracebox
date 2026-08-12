#include "log_writer.h"

namespace tracebox {
namespace logger {

void LogWriter::writingThread() {
    while (true) {
        auto request = wait();
        if (!request) {
            return;
        }
        storage_->write(*request);
    }
}

LogWriter::LogWriter(std::shared_ptr<Storage> storage)
    : storage_(std::move(storage)), thread_(&LogWriter::writingThread, this) {}

LogWriter::~LogWriter() { stop(); }

void LogWriter::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    condition_.notify_one();
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool LogWriter::add(const LogRequest& request) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            return false;
        }
        queue_.push(request);
    }
    condition_.notify_one();
    return true;
}

}  // namespace logger
}  // namespace tracebox
