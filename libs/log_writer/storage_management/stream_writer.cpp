#include "stream_writer.h"

#include <cstring>
#include <ctime>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string_view>

namespace embark {
namespace logger {

int StreamWriter::openFile(const LogRequest &request) {
    name_ = request.channel();
    record_size_ = request.size();
    return reopenFile(request.time_ns());
}

int StreamWriter::reopenFile(uint64_t time_ns) {
    offset_ = 0;
    // Create index file
    auto file_name = fileName(time_ns);
    auto path = folder_ + "/" + file_name + ".idx";
    index_.open(path, std::ios::binary | std::ios::app);
    if (!index_) {
        std::cerr << "Couldn't create file " << path << std::endl;
        return -EINVAL;
    }
    int result = writeHeader(&index_, time_ns, kIndexFile);
    if (result && record_size_ == 0) {
        // Create data file
        data_.open(folder_ + "/" + file_name + ".data",
                   std::ios::binary | std::ios::app);
        int data_result = writeHeader(&data_, time_ns, kDataFile);
        offset_ = sizeof(LogFileHeader);
        if (data_result < 0) {
            std::cerr << "Error writing header file into " << path << std::endl;
            index_.close();
            return data_result;
        }
        result += data_result;
    }
    last_flush_ = std::chrono::system_clock::now();
    return result;
}

void StreamWriter::closeFile() {
    if (index_.is_open()) {
        index_.flush();
        index_.close();
    }
    if (data_.is_open()) {
        data_.flush();
        data_.close();
    }
}

std::string StreamWriter::fileName(uint64_t time_ns) {
    std::time_t t(time_ns / 1E9);
    std::tm *now = std::localtime(&t);

    std::stringstream ss;
    ss << std::setw(2) << std::setfill('0') << now->tm_hour;
    ss << std::setw(2) << std::setfill('0') << now->tm_min;
    ss << std::setw(2) << std::setfill('0') << now->tm_sec;
    ss << "_" << name_ << "_" << record_size_;
    return ss.str();
}

int StreamWriter::writeHeader(std::ofstream *stream, uint64_t time_ns,
                              uint8_t type) {
    if (stream->is_open()) {
        LogFileHeader header;
        header.header_size_ = sizeof(header);
        header.timestamp_ns_ = time_ns;
        header.record_size_ = record_size_;
        memset(header.name_, 0, sizeof(header.name_));
        header.file_type_ = type;
        memcpy(header.name_, name_.data(),
               std::min<size_t>(sizeof(header.name_), name_.length()));
        stream->write(reinterpret_cast<char *>(&header), sizeof(header));
        stream->flush();
        return sizeof(header);
    }
    return -EINVAL;
}

int StreamWriter::write(uint64_t time_ns, const std::string &data) {
    int result = 0;
    if (!index_.is_open()) {
        return -EINVAL;
    }
    bool flush = false;
    std::chrono::duration<double> seconds_from_last_flush =
        std::chrono::system_clock::now() - last_flush_;
    if (seconds_from_last_flush.count() > kFlushIntervalSeconds) {
        flush = true;
    }
    if (record_size_) {
        // Fixed length record
        if (data.size() == record_size_) {
            // Write time, then the actual bytes.
            index_.write(reinterpret_cast<char *>(&time_ns), sizeof(time_ns));
            index_.write(data.data(), record_size_);
            if (flush) {
                index_.flush();
            }
            result = record_size_ + sizeof(time_ns);
        } else {
            std::cerr << "Couldn't write record of " << data.size()
                      << " bytes into file with records size " << record_size_
                      << std::endl;
            return -EINVAL;
        }
    } else {
        if (!data_.is_open()) {
            return -EINVAL;
        }
        // Check for offset overflow.
        uint32_t size = data.size();
        size_t total_length = sizeof(time_ns) + sizeof(offset_);
        total_length += sizeof(time_ns) + sizeof(size) + size;
        if ((total_length + offset_) > UINT32_MAX) {
            // Reopen the files to avoid offset overflow
            closeFile();
            int ret = reopenFile(time_ns);
            if (ret <= 0) {
                return ret;
            }
            result += ret;
        }
        // Variable length record:
        // 1. Write time and offset into the index file.
        index_.write(reinterpret_cast<char *>(&time_ns), sizeof(time_ns));
        index_.write(reinterpret_cast<char *>(&offset_), sizeof(offset_));
        if (flush) {
            index_.flush();
        }
        // 2. Write time, size and the data into the data file.
        data_.write(reinterpret_cast<char *>(&time_ns), sizeof(time_ns));
        data_.write(reinterpret_cast<char *>(&size), sizeof(size));
        data_.write(data.data(), size);
        if (flush) {
            data_.flush();
        }
        // Update the offset in data file.
        offset_ += sizeof(time_ns) + sizeof(size) + size;
        // Update the total number of bytes written.
        result += total_length;
    }
    return result;
}

}  // namespace logger
}  // namespace embark