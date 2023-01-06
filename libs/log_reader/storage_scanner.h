#pragma once

#include <filesystem>
#include <string>

#include "logger.pb.h"

namespace embark {
namespace logger {

// This class scans a given storage to find all streams for a given time range.
class StorageScanner {
    // Storage folder
    std::string folder_;

    // Extracts all files names from the given directory matching the time range
    void extractStreams(const std::filesystem::path& path, uint64_t start,
                        uint64_t end,
                        std::shared_ptr<DataStreamsResponse> result);

   public:
    explicit StorageScanner(std::string folder) : folder_(folder) {}

    // Delete copy / assignment constructors
    StorageScanner(const StorageScanner&) = delete;
    StorageScanner& operator=(const StorageScanner&) = delete;
    StorageScanner(StorageScanner&&) = delete;
    StorageScanner& operator=(StorageScanner&&) = delete;

    // Returns list of data channels available for the given time interval.
    std::shared_ptr<DataStreamsResponse> getStreams(uint64_t start,
                                                    uint64_t end);

    // Returns the data according to the requested parameters
    std::shared_ptr<DataStreamsResponse> getData(const std::string& file,
                                                 uint64_t start_time,
                                                 uint64_t end_time,
                                                 uint32_t start_idx,
                                                 uint32_t max_size);
};

}  // namespace logger
}  // namespace embark
