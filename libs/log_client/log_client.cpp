#include <iostream>
#include "log_client.h"
#include "logger.pb.h"

namespace embark {
namespace logger {

bool LogClient::logData(const std::string &data, uint64_t time_ns) {
    if (record_size_ && (data.size() != record_size_)) {
        std::cerr << "Invalid record size: " << data.size() << ", expected " << record_size_ << std::endl;
        return false;
    }
    // Put all parameters into a request structure.
    logger::LogRequest request;
    if (record_size_) {
        request.set_size(record_size_);
    }
    request.set_channel(stream_name_);
    request.set_data(data);
    request.set_time_ns(time_ns);

    // Serialize request
    std::string serialized;
    request.SerializePartialToString(&serialized);
    return sendData(serialized);
}

}}