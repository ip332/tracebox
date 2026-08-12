#pragma once

#include <condition_variable>
#include <queue>
#include <utility>

#include "log_writer.h"
#include "logger.pb.h"
#include "storage.h"
#include "tcp_server.h"

namespace tracebox {
namespace logger {

// This class uses Server class to serialize multiple asynchronous logging
// requests and add them into a queue of LogRequests objects which will be
// written to the file system using Storage class.
class TraceboxLogger {
    // Handles queuing incoming requests and writing them to the file system.
    LogWriter writer_;
    // Declared after the callback target and explicitly stopped first.
    Server server_;

    // Handles incoming request.
    // Note: it always returns empty string because there is no need to send
    // anything back.
    std::string handleLogRequest(const std::string_view& data) {
        LogRequest request;
        if (request.ParseFromArray(data.data(), data.size())) {
            writer_.add(request);
        } else {
            std::cerr << "Error parsing LogRequest" << std::endl;
        }
        return "";
    }

   public:
    explicit TraceboxLogger(uint32_t port, std::shared_ptr<Storage> storage)
        : writer_(std::move(storage)),
          server_(port, std::bind(&TraceboxLogger::handleLogRequest, this,
                                  std::placeholders::_1)) {
        server_.start();
    }

    ~TraceboxLogger() { server_.stop(); }

    // Delete copy / assignment constructors
    TraceboxLogger(const TraceboxLogger&) = delete;
    TraceboxLogger& operator=(const TraceboxLogger&) = delete;
    TraceboxLogger(TraceboxLogger&&) = delete;
    TraceboxLogger& operator=(TraceboxLogger&&) = delete;
};

}  // namespace logger
}  // namespace tracebox
