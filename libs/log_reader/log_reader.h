#pragma once

#include "logger.pb.h"
#include "storage_scanner.h"
#include "tcp_server.h"

#include <utility>

namespace tracebox {
namespace logger {

// This class spins a TCP server and listens for DataStreamsRequest or
// DataStream requests, handles them and responses with DataStreams or
// DataStreamPieces data_protos.
class LogReader {
    // Actual reader
    StorageScanner scanner_;
    // Declared after the callback target and explicitly stopped first.
    Server server_;

    std::string handleReadingRequest(const std::string_view& data) {
        DataStreamsRequest request;
        if (request.ParseFromArray(data.data(), data.size())) {
            std::shared_ptr<DataStreamsResponse> response;
            if (request.has_file()) {
                uint32_t start =
                    request.has_start_idx() ? request.start_idx() : 0;
                uint32_t max = request.has_max_count() ? request.max_count()
                                                       : 0;
                response = scanner_.getData(request.file(), request.start_ns(),
                                            request.end_ns(), start, max);
            } else {
                response = scanner_.getStreams(request.start_ns(),
                                               request.end_ns());
            }
            std::string serialized;
            response->SerializeToString(&serialized);
            return serialized;
        } else {
            std::cerr << "Error parsing DataStreamsRequest" << std::endl;
        }
        return "";
    }

   public:
    explicit LogReader(uint32_t port, const std::string& storage)
        : scanner_(storage),
          server_(port, std::bind(&LogReader::handleReadingRequest, this,
                                  std::placeholders::_1)) {
        server_.start();
    }

    ~LogReader() { server_.stop(); }

    // Delete copy / assignment constructors
    LogReader(const LogReader&) = delete;
    LogReader& operator=(const LogReader&) = delete;
    LogReader(LogReader&&) = delete;
    LogReader& operator=(LogReader&&) = delete;
};

}  // namespace logger
}  // namespace tracebox
