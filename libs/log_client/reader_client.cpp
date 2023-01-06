#include <iostream>
#include "reader_client.h"
#include "logger.pb.h"

namespace embark {
namespace logger {

std::shared_ptr<DataStreamsResponse> LogReadClient::getStreams(uint64_t start, uint64_t end,
                                                uint32_t start_idx, uint32_t max_size) {
    DataStreamsRequest request;
    request.set_start_ns(start);
    request.set_end_ns(end);
    request.set_start_idx(start_idx);
    request.set_max_count(max_size);

    return getResponse<DataStreamsRequest, DataStreamsResponse>(request);
}

std::shared_ptr<DataStreamsResponse> LogReadClient::getData(const std::string & file, uint64_t start, uint64_t end,
                                             uint32_t start_idx, uint32_t max_size) {
    DataStreamsRequest request;
    request.set_file(file);
    request.set_start_ns(start);
    request.set_end_ns(end);
    request.set_start_idx(start_idx);
    request.set_max_count(max_size);

    return getResponse<DataStreamsRequest, DataStreamsResponse>(request);

}

}}