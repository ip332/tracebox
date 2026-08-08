#pragma once

#include <unistd.h>

#include <string>

#include "tcp_client.h"

namespace tracebox {
namespace logger {

// This class implements a client to send logging requests to the log_writer.
class LogClient : public TcpClient {
    // Stream name
    std::string stream_name_;
    // Stores size for a fixed length stream (0 indicates variable size)
    uint32_t record_size_;

   public:
    explicit LogClient(const std::string& stream_name, uint32_t record_size = 0)
        : stream_name_(stream_name), record_size_(record_size) {}

    // Delete copy / assignment constructors
    LogClient(const LogClient&) = delete;
    LogClient& operator=(const LogClient&) = delete;
    LogClient(LogClient&&) = delete;
    LogClient& operator=(LogClient&&) = delete;

    // Sends a logging request to the log_writer.
    // It must be done after successful connect() and is_connected() calls.
    bool logData(const std::string& data, uint64_t time);
};

}  // namespace logger
}  // namespace tracebox
