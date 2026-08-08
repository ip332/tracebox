#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "tracebox/timestamp.h"

namespace tracebox {

enum class StreamStatus {
    Unknown = 0,
    Healthy = 1,
    Empty = 2,
    HeaderMismatch = 3,
    HeadersDifferent = 4,
    IndexMissing = 5,
};

struct Sample {
    Timestamp time_ns = 0;
    std::string data;
    std::uint32_t stream_index = 0;
};

struct Stream {
    std::string name;
    std::uint32_t day = 0;
    std::string file;
    std::uint32_t records_count = 0;
    StreamStatus status = StreamStatus::Unknown;
    Timestamp start_time_ns = 0;
    Timestamp end_time_ns = 0;
};

struct ReadResult {
    std::vector<Sample> records;
    std::string error;
};

}  // namespace tracebox
