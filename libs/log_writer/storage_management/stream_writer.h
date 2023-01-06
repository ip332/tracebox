#pragma once

#include "data_types.h"

#include <iostream>
#include <fstream>
#include "logger.pb.h"

namespace embark {
namespace logger {

// A single log stream handler.
class StreamWriter {
    // Absolute parent folder path
    std::string folder_;
    // Stream name
    std::string name_;
    // Record size (or 0 if it is expected to handle variable length records).
    uint32_t record_size_;
    // Data file position (in case of variable length records).
    uint32_t offset_;
    // Actual streams:
    std::ofstream index_;   // always created
    std::ofstream data_;    // only for variable length records.

    // Creates file name from the current time (HHMMSS), stream name and the record size.
    std::string fileName(uint64_t time_ns);

    // Writes file header and returns number of bytes written or a negative error code.
    int writeHeader(std::ofstream *stream, uint64_t time_ns, uint8_t type);

    // Opens file using the instance variables.
    int reopenFile(uint64_t time_ns);

    void closeFile();
public:
    // Record size is only used for the fixed size records.
    StreamWriter(const std::string &folder) : folder_(folder), offset_(0) {}

    ~StreamWriter() {
        closeFile();
    }

    // Opens the file and returns the number of bytes added to the file system or negative error code.
    int openFile(const LogRequest & request);

    // Writes the data and returns total number of bytes written.
    int write(uint64_t time_ns, const std::string &data);
};

}}

