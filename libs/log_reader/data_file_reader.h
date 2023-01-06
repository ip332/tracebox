#pragma once

#include "file_reader.h"

namespace embark {
namespace logger {

class DataFileReader : public FileReader {
public:
    DataFileReader(const std::filesystem::path & path) : FileReader(path) {}

    // Sets position to the given record index, reads the data and returns its size (or 0 in case of an error).
    uint32_t read(size_t offset, DataPiece *result) {
        last_error_.clear();
        if (!header_) {
            log_error("File " + path_ + " was not opened");
            return 0;
        }
        if (offset >= (size_bytes_ - sizeof(VariableRecordDataHeader) - 1) || offset < header_->header_size_ || header_->file_type_ != kDataFile) {
            log_error("Invalid offset " + std::to_string(offset) + " for file " + path_ + " which has " + std::to_string(size_bytes_));
            return 0;
        }
        if_.seekg(offset);
        if (!if_) {
            log_error("Couldn't position file " + path_ + " to the offset " + std::to_string(offset));
            return 0;
        }
        VariableRecordDataHeader rec_header;
        if_.read(reinterpret_cast<char *>(&rec_header), sizeof(rec_header));
        if (!if_) {
            log_error("Couldn't read " + std::to_string(sizeof(rec_header)) + " from " + path_ + " at " + std::to_string(offset));
            return 0;
        }
        if (!result->has_data()) {
            result->set_data("");
        }
        if (rec_header.size_ > result->data().capacity()) {
            result->mutable_data()->resize(rec_header.size_);
        }
        result->set_time_ns(rec_header.time_ns_);
        if_.read(result->mutable_data()->data(), rec_header.size_);
        if (!if_) {
            log_error("Couldn't read " + std::to_string(rec_header.size_) + " from " + path_ + " at " + std::to_string(offset + sizeof(rec_header)));
            return 0;
        }
        return rec_header.size_;
    }
};

}}

