#pragma once

#include "file_reader.h"

namespace tracebox {
namespace logger {

class DataFileReader : public FileReader {
   public:
    DataFileReader(const std::filesystem::path &path) : FileReader(path) {}

    // Sets position to the given record index, reads the data and returns its
    // size (or 0 in case of an error).
    uint32_t read(size_t offset, DataPiece *result) {
        last_error_.clear();
        if (!header_) {
            log_error("File " + path_ + " was not opened");
            return 0;
        }
        if (size_bytes_ == 0 ||
            offset >= (size_bytes_ - sizeof(VariableRecordDataHeader) - 1) ||
            offset < header_->header_size_ ||
            header_->file_type_ != kDataFile) {
            log_error("Invalid offset " + std::to_string(offset) +
                      " for file " + path_ + " which has " +
                      std::to_string(size_bytes_) + " bytes");
            return 0;
        }
        if_.seekg(offset);
        if (!if_) {
            log_error("Couldn't position file " + path_ + " to the offset " +
                      std::to_string(offset));
            return 0;
        }
        VariableRecordDataHeader rec_header;
        if_.read(reinterpret_cast<char *>(&rec_header), sizeof(rec_header));
        if (!if_) {
            log_error("Couldn't read " + std::to_string(sizeof(rec_header)) +
                      " from " + path_ + " at " + std::to_string(offset));
            return 0;
        }
        std::vector<char> buffer(rec_header.size_);
        if_.read(buffer.data(), rec_header.size_);
        if (!if_) {
            log_error("Couldn't read " + std::to_string(rec_header.size_) +
                      " from " + path_ + " at " +
                      std::to_string(offset + sizeof(rec_header)));
            return 0;
        }
        result->set_time_ns(rec_header.time_ns_);
        result->set_data(buffer.data(), rec_header.size_);
        return rec_header.size_;
    }
};

}  // namespace logger
}  // namespace tracebox
