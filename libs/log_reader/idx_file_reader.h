#pragma once

#include "file_reader.h"

namespace tracebox {
namespace logger {

class IndexFileReader : public FileReader {
    // Total number of records
    uint32_t records_cnt_;
    // Time range in nanos
    uint64_t start_ns_;
    uint64_t end_ns_;

    bool selectRecord(uint32_t index) {
        size_t record_size;
        if (header_->record_size_) {
            record_size = sizeof(FixedRecordIdx) + header_->record_size_;
        } else {
            record_size = sizeof(VariableRecordIdx);
        }
        if_.seekg(header_->header_size_ + index * record_size);
        if (!if_) {
            return false;
        }
        return true;
    }

   public:
    IndexFileReader(const std::filesystem::path &path)
        : FileReader(path), records_cnt_(0) {
        if (size_bytes_ && header_ && header_->header_size_ <= size_bytes_) {
            auto record_size =
                header_->record_size_
                    ? sizeof(FixedRecordIdx) + header_->record_size_
                    : sizeof(VariableRecordIdx);
            records_cnt_ = (size_bytes_ - header_->header_size_) / record_size;
            start_ns_ = timeOf(0);
            end_ns_ = timeOf(records_cnt_ - 1);
        } else {
            start_ns_ = end_ns_ = 0;
        }
    }

    uint64_t start_ns() const { return start_ns_; }
    uint64_t end_ns() const { return end_ns_; }

    // Returns the number of records
    size_t records_cnt() const { return records_cnt_; }

    // Returns true if at least one record belongs to the given time range.
    bool matchTime(uint64_t start, uint64_t end) const {
        return records_cnt_ && (start <= end) && (start_ns_ <= end) &&
               (end_ns_ >= start);
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
            return false;
        }
        if ((index >= records_cnt_) || (header_->file_type_ != kIndexFile)) {
            log_error("Invalid index " + std::to_string(index) + " for file " +
                      path_ + " which has " + std::to_string(size_bytes_) +
                      " bytes");
            return false;
        }
        if (!selectRecord(index)) {
            return 0;
        }
        uint64_t result;
        if_.read(reinterpret_cast<char *>(&result), sizeof(result));
        if (!if_) {
            log_error("Couldn't read timestamp from record " +
                      std::to_string(index) + " in " + path_);
            return 0;
        }
        return result;
    }

    // Reads the payload into the given vector and returns the record length
    size_t readPayload(uint32_t index, std::string *payload) {
        last_error_.clear();
        if (!header_) {
            log_error("File " + path_ + " was not opened");
            return 0;
        }
        if (header_->record_size_ == 0 || !payload || index >= records_cnt_) {
            log_error("Can't read fixed payload from a variable length file " +
                      path_);
            return 0;
        }
        if (!selectRecord(index)) {
            return 0;
        }
        FixedRecordIdx record;
        if_.read(reinterpret_cast<char *>(&record), sizeof(record));
        if (!if_) {
            log_error("Couldn't read fixed record " + std::to_string(index) +
                      " from " + path_);
            return 0;
        }
        std::vector<char> buffer(header_->record_size_);
        if_.read(buffer.data(), header_->record_size_);
        if (!if_) {
            log_error("Couldn't read fixed payload " + std::to_string(index) +
                      " from " + path_);
            return 0;
        }
        payload->assign(buffer.data(), header_->record_size_);
        return header_->record_size_;
    }

    // Reads the offset (or 0 in case of an error)
    uint32_t readOffset(uint32_t index) {
        last_error_.clear();
        if (!header_) {
            log_error("File " + path_ + " was not opened");
            return 0;
        }
        if (header_->record_size_ || index >= records_cnt_) {
            log_error("Can't read offset from a fixed length file " + path_);
            return 0;
        }
        if (!selectRecord(index)) {
            return 0;
        }
        VariableRecordIdx record;
        if_.read(reinterpret_cast<char *>(&record), sizeof(record));
        if (!if_) {
            log_error("Couldn't read variable index record " +
                      std::to_string(index) + " from " + path_);
            return 0;
        }
        return record.offset_;
    }
};

}  // namespace logger
}  // namespace tracebox
