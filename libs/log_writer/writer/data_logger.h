#pragma once

#include <condition_variable>
#include <queue>

#include "log_writer.h"
#include "logger.pb.h"
#include "storage.h"
#include "tcp_server.h"

namespace embark {
namespace logger {

// This class uses Server class to serialize multiple asynchronous logging
// requests and add them into a queue of LogRequests objects which will be
// written to the file system using Storage class.
class DataLogger {
    // Handles incoming request into a LogRequest object.
    Server server_;
    // Handles queuing incoming requests and writing them to the file system.
    LogWriter writer_;
    // Makes sure no incoming requests will be processed during initialization.
    bool ready_ = false;

    // Handles incoming request.
    // Note: it always returns empty string because there is no need to send
    // anything back.
    std::string handleLogRequest(const std::string_view& data) {
        if (ready_) {
            LogRequest request;
            if (request.ParseFromArray(data.data(), data.size())) {
                writer_.add(request);
                return "";
            } else {
                std::cerr << "Error parsing LogRequest" << std::endl;
            }
        }
        return "";
    }

   public:
    explicit DataLogger(uint32_t port, std::shared_ptr<Storage> storage)
        : server_(port, std::bind(&DataLogger::handleLogRequest, this,
                                  std::placeholders::_1)),
          writer_(storage),
          ready_(true) {}

    // Delete copy / assignment constructors
    DataLogger(const DataLogger&) = delete;
    DataLogger& operator=(const DataLogger&) = delete;
    DataLogger(DataLogger&&) = delete;
    DataLogger& operator=(DataLogger&&) = delete;
};

}  // namespace logger
}  // namespace embark
