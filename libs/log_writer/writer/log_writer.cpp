#include "log_writer.h"

namespace embark {
namespace logger {

void LogWriter::writingThread() {
    running_ = true;
    while (running_) {
        std::unique_ptr<LogRequest> request = wait();
        if (!request) {
            continue;
        }
        storage_->write(*request);
    }
}

LogWriter::LogWriter(std::shared_ptr<Storage> storage)
    : storage_(storage), thread_(&LogWriter::writingThread, this) {
}

LogWriter::~LogWriter() {
    running_ = false;
    force_wakeup_ = true;
    notify();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void LogWriter::add(const LogRequest & request) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(request);
    }
    notify();
}

}}
