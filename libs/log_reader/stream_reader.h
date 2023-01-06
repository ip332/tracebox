#pragma once

#include "logger.pb.h"
#include "data_types.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include "idx_file_reader.h"
#include "data_file_reader.h"

namespace embark {
namespace logger {

// A single read-only log stream representation
class StreamReader {
    // Index file
    std::unique_ptr<IndexFileReader> index_;
    // Data file (optional)
    std::unique_ptr<DataFileReader> data_;
    // Overall stream status
    StreamStatus status_;

public:
    // The only argument is the absolute path to the .idx file
    StreamReader(const std::filesystem::path & path);

    // Returns true if at least one record belongs to the given time range.
    bool matchTime(uint64_t start, uint64_t end) const {
        return status_ == StreamStatus::kHealthy && index_ && index_->matchTime(start, end);
    }

    std::string streamName() const { return index_->name(); }

    StreamStatus status() const { return status_; }

    // Returns the number of records
    size_t records_cnt() const { return index_->records_cnt(); }

    // Returns the payload for a given record index.
    bool read(size_t index, DataPiece *payload);

    // Returns the last error reported by the input stream.
    const std::string & last_error() const {
        return index_->last_error().empty() ? data_->last_error() : index_->last_error();
    }
};

}}

