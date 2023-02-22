#pragma once

#include <vector>
#include <string>
#include <map>
#include <ostream>
#include "logger.pb.h"

// This class publishes several CAN streams in ASC format.
// Note: the data records should be already sorted by time.
class AscWriter {
    // The output stream
    std::ostream &out_;
    // Mapping between the stream name and the channel index.
    const std::map<std::string, int> & streams_;
    // First time stamp
    uint64_t start_time_ns_ = 0;

    // Write header and store the first time stamp
    bool initStream(uint64_t time_ns);

public:
    AscWriter(std::ostream & out,  const std::map<std::string, int> &streams)
            : out_(out), streams_(streams) { }

    // Writes the given data into the ASC stream.
    void write(const std::vector<embark::logger::DataPiece> &data);
};
