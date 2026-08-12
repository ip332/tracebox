#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "logger.pb.h"

namespace tracebox {
namespace logger {

// This class scans a given storage to find all streams for a given time range.
class StorageScanner {
    // Storage folder
    std::filesystem::path folder_;

    // Resolves a client-supplied stream identifier to a confined, valid index
    // file. The returned path is canonical and is never exposed to clients.
    std::optional<std::filesystem::path> resolveIndexPath(
        const std::string& file) const;

    // Returns the storage-root-relative identifier used by discovery.
    std::string streamIdentifier(const std::filesystem::path& path) const;

    // Extracts all files names from the given directory matching the time range
    // Parameter `day` will be added to the result.
    void extractStreams(const std::filesystem::path& path, uint64_t start,
                        uint64_t end, uint32_t day,
                        std::shared_ptr<DataStreamsResponse> result);

   public:
    explicit StorageScanner(std::string folder)
        : folder_(std::filesystem::weakly_canonical(folder)) {}

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
}  // namespace tracebox
