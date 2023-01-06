#pragma once

#include "logger.pb.h"
#include "storage_scanner.h"
#include "tcp_server.h"

namespace embark {
namespace logger {

// This class spins a TCP server and listens for DataStreamsRequest or
// DataStream requests, handles them and responses with DataStreams or
// DataStreamPieces protos.
class LogReader {
    // Handles incoming requests
    logger::Server server_;
    // Actual reader
    StorageScanner scanner_;
    // Makes sure no incoming requests will be processed during initialization.
    bool ready_ = false;

    std::string handleReadingRequest(const std::string_view& data) {
        if (ready_) {
            DataStreamsRequest request;
            if (request.ParseFromArray(data.data(), data.size())) {
                std::shared_ptr<DataStreamsResponse> response;
                if (request.has_file()) {
                    uint32_t start =
                        request.has_start_idx() ? request.start_idx() : 0;
                    uint32_t max = request.has_max_count() ? request.max_count()
                                                           : 0;
                    response =
                        scanner_.getData(request.file(), request.start_ns(),
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
        }
        return "";
    }

   public:
    explicit LogReader(uint32_t port, const std::string& storage)
        : server_(port, std::bind(&LogReader::handleReadingRequest, this,
                                  std::placeholders::_1)),
          scanner_(storage),
          ready_(true) {}

    // Delete copy / assignment constructors
    LogReader(const LogReader&) = delete;
    LogReader& operator=(const LogReader&) = delete;
    LogReader(LogReader&&) = delete;
    LogReader& operator=(LogReader&&) = delete;
};

}  // namespace logger
}  // namespace embark
