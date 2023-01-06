#pragma once

#include "data_types.h"
#include "stream_writer.h"
#include "logger.pb.h"
#include <stdint.h>

namespace embark {
namespace logger {

// This class declares the interface to create, use and change a folder within the file system.
class Folder {
    // Stores the day in a format YYYYMMDD
    uint32_t yyyymmdd_;

    // Absolute folder name
    std::string folder_;

    // Quick access StreamWriter container (stream name is used as a key)
    std::map<std::string, StreamWriter> files_;

    // Create folder on the first write() call.
    bool create(uint64_t time_ns);
public:
    Folder(const std::string &folder) : yyyymmdd_(0), folder_(folder) {}
    ~Folder() { close(); }

    // Checks if the provided time stamp belongs to the same day or not.
    bool sameDay(uint64_t time_ns) const {
        return yyyymmdd_ == Time2YYYYMMDD(time_ns);
    }

    // Close all opened files and makes any further write operations invalid.
    void close() {
        files_.clear();
        yyyymmdd_ = 0;
    }

    // Write request into the file and returns the number of bytes written or an error code.
    // This call does not check if the time stamp matches the folder name for performance reasons.
    // The caller should decide when to use sameDay() method and when it may be not needed.
    int write(const LogRequest & request);
};

}}
