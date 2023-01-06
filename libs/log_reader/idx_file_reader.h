#pragma once

#include "file_reader.h"

namespace embark {
namespace logger {

class IndexFileReader : public FileReader {
    // Total number of records
    uint32_t records_cnt_;
    // Time range in nanos
    uint64_t start_ns_;
    uint64_t end_ns_;

   public:
    IndexFileReader(const std::filesystem::path &path) : FileReader(path) {
        start_ns_ = timeOf(0);
        end_ns_ = timeOf(records_cnt_ - 1);
        records_cnt_ = (size_bytes_ - header_->header_size_) /
                       (sizeof(FixedRecordIdx) + header_->record_size_);
    }

    // Returns the number of records
    size_t records_cnt() const { return records_cnt_; }

    // Returns true if at least one record belongs to the given time range.
    bool matchTime(uint64_t start, uint64_t end) const {
        return (start < end) && (start_ns_ < end_ns_) && (start_ns_ >= start) &&
               (end_ns_ <= end);
    }

    // Tells if this index file stores fixed size records.
    bool fixedSize() const {
        return header_ ? header_->record_size_ > 0 : true;
    }

    // Sets position to the given record index and returns its timestamp (or 0
    // in case of an error).
    uint64_t timeOf(size_t index) {
        last_error_.clear();
        if (!header_) {
            log_error("File " + path_ + " was not opened");
            return 0;
        }
        if ((index >= records_cnt_) || (header_->file_type_ != kIndexFile)) {
            log_error("Invalid index " + std::to_string(index) + " for file " +
                      path_ + " which has " + std::to_string(size_bytes_));
            return 0;
        }
        size_t record_size;
        if (header_->record_size_) {
            record_size = sizeof(FixedRecordIdx) + header_->record_size_;
        } else {
            record_size = sizeof(VariableRecordIdx);
        }
        uint64_t result = 0;
        if_.seekg(header_->header_size_ + index * record_size);
        if (if_) {
            if_.read(reinterpret_cast<char *>(&result), sizeof(result));
            if (!if_) {
                log_error("Couldn't read timestamp from record " +
                          std::to_string(index) + " in " + path_);
                return 0;
            }
        }
        return result;
    }

    // Reads the payload into the given vector and returns the record length
    size_t readPayload(std::string *payload) {
        last_error_.clear();
        if (!header_) {
            log_error("File " + path_ + " was not opened");
            return 0;
        }
        if (header_->record_size_ == 0 || !payload) {
            log_error("Can't read fixed payload from a variable length file " +
                      path_);
            return 0;
        }
        if (payload->capacity() < header_->record_size_) {
            payload->resize(header_->record_size_);
        }
        if_.read(payload->data(), header_->record_size_);
        return header_->record_size_;
    }

    // Reads the offset (or 0 in case of an error)
    uint32_t readOffset() {
        if (!header_) {
            log_error("File " + path_ + " was not opened");
            return 0;
        }
        if (header_->record_size_) {
            log_error("Can't read offset from a fixed length file " + path_);
            return 0;
        }
        uint32_t result = 0;
        if_.read(reinterpret_cast<char *>(&result), sizeof(result));
        if (!if_) {
            log_error("Couldn't read offset from " + path_);
            return 0;
        }
        return result;
    }
};

}  // namespace logger
}  // namespace embark
