#pragma once

#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <utility>

#include "data_types.h"
#include "file_io.h"
#include "logger.pb.h"

namespace tracebox {
namespace logger {

// A single log stream handler.
class StreamWriter {
   public:
    enum class WriteFaultPoint {
        kAfterDataWriteBeforeIndexWrite,
    };

    using FaultInjector = std::function<void(WriteFaultPoint)>;
    using FileIOFactory = std::function<std::unique_ptr<FileIO>()>;

    class SimulatedPowerLoss : public std::runtime_error {
       public:
        SimulatedPowerLoss()
            : std::runtime_error("simulated power loss during log write") {}
    };

   private:
    // TODO: increase this const to reduce CPU/IO usage under realistic load.
    constexpr static size_t kFlushIntervalSeconds = 1;

    // Absolute parent folder path
    std::string folder_;
    // Stream name
    std::string name_;
    // Record size (or 0 if it is expected to handle variable length records).
    uint32_t record_size_;
    // Data file position (in case of variable length records).
    uint32_t offset_;
    // Actual streams:
    std::unique_ptr<FileIO> index_;  // always created
    std::unique_ptr<FileIO> data_;   // only for variable length records.
    // Last write operation when the streams were flushed to the file system
    std::chrono::system_clock::time_point last_flush_;
    FaultInjector fault_injector_;
    FileIOFactory file_io_factory_;

    // Creates file name from the current time (HHMMSS), stream name and the
    // record size.
    std::string fileName(uint64_t time_ns);

    // Writes file header and returns number of bytes written or a negative
    // error code.
    int writeHeader(FileIO *stream, uint64_t time_ns, uint8_t type);

    // Opens file using the instance variables.
    int reopenFile(uint64_t time_ns);

    void closeFile();

   public:
    // Record size is only used for the fixed size records.
    StreamWriter(const std::string &folder,
                 FileIOFactory file_io_factory = makeStandardFileIO)
        : folder_(folder), offset_(0), file_io_factory_(std::move(file_io_factory)) {}

    ~StreamWriter() { closeFile(); }

    // Opens the file and returns the number of bytes added to the file system
    // or negative error code.
    int openFile(const LogRequest &request);

    // Test instrumentation. The callback may throw SimulatedPowerLoss to
    // emulate a power loss at a precise point in the write sequence.
    void setFaultInjector(FaultInjector injector) {
        fault_injector_ = std::move(injector);
    }

    // Writes the data and returns total number of bytes written.
    int write(uint64_t time_ns, const std::string &data);
};

}  // namespace logger
}  // namespace tracebox
