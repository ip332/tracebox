#pragma once

#include <time.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace tracebox {
namespace logger {

constexpr static uint8_t kIndexFile = 1;
constexpr static uint8_t kDataFile = 2;

struct LogFileHeader {
    constexpr static uint8_t kDlFormatMajor = 1;
    constexpr static uint8_t kDlFormatMinor = 0;

    uint16_t header_size_;   	// Offset to the first byte of the first record.
    uint64_t timestamp_ns_;	// Local time when the file was created.
    uint32_t record_size_; 	// Number of data bytes per record in this file or 0
                            	// for variable lengths.
    uint8_t major_version_ = kDlFormatMajor;
    uint8_t minor_version_ = kDlFormatMinor;
    uint8_t file_type_;
    char name_[32];  		// Human readable data stream name

    LogFileHeader() : header_size_(0), timestamp_ns_(0), record_size_(0) {
        memset(name_, 0, sizeof(name_));
    }
} __attribute__((packed));

// Fixed length record format (stored in the index file)
struct FixedRecordIdx {
    uint64_t time_ns_;
    // Followed by the fixed number of bytes.
} __attribute__((packed));

// Variable length record format (stored in index file)
struct VariableRecordIdx {
    uint64_t time_ns_;
    uint32_t offset_;  // Offset in the data file
} __attribute__((packed));

// Variable length record format (stored in data file)
struct VariableRecordDataHeader {
    uint64_t time_ns_;
    uint32_t size_;
    // Followed by the `size_` bytes
} __attribute__((packed));

inline size_t maxRecordSize(int size) {
    // In the worst case scenario, we will create two files (index, data) and
    // add one record in each.
    size_t result = 2 * sizeof(LogFileHeader) + sizeof(VariableRecordIdx) +
           sizeof(VariableRecordDataHeader) + size;
    return result;
}

// Returns the day, extracted from the given time_ns, in YYYYMMDD format
inline uint32_t Time2YYYYMMDD(uint64_t time_ns) {
    time_t t(time_ns / 1E9);
    tm *now = localtime(&t);

    return (now->tm_year + 1900) * 10000 + (now->tm_mon + 1) * 100 +
           now->tm_mday;
};

}  // namespace logger
}  // namespace tracebox
