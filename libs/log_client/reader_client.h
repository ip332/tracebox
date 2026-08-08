#pragma once

#include <unistd.h>

#include <string>

#include "logger.pb.h"
#include "tcp_client.h"

namespace tracebox {
namespace logger {

// This class implements a client to send reading requests to the log reading
// server.
class LogReadClient : public TcpClient {
    template <typename S, typename R>
    std::shared_ptr<R> getResponse(const S& request) {
        std::string serialized;
        request.SerializePartialToString(&serialized);

        auto result = std::make_shared<R>();
        if (!sendData(serialized)) {
            return result;
        }
        auto response = readData();
        if (response->empty()) {
            return result;
        }
        if (result->ParseFromArray(response->data(), response->size())) {
            return result;
        } else {
            std::cerr << "Error parsing DataStreamsResponse" << std::endl;
        }
        return result;
    }

   public:
    // Requests list of log streams for the given time range
    // Note: It must be done after successful connect() call (see
    // Client::connect()).
    std::shared_ptr<DataStreamsResponse> getStreams(
        uint64_t start, uint64_t end, uint32_t start_idx = 0,
        uint32_t max_size = UINT32_MAX);

    // Returns the data according to the requested parameters
    std::shared_ptr<DataStreamsResponse> getData(
        const std::string& file, uint64_t start_time, uint64_t end_time,
        uint32_t start_idx = 0, uint32_t max_size = UINT32_MAX);
};

}  // namespace logger
}  // namespace tracebox
