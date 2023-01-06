#pragma once

#include "logger.pb.h"
#include "data_types.h"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace embark {
namespace logger {

// A single read-only file representation
class FileReader {
protected:
    // File path
    std::string path_;
    // Input stream.
    std::ifstream if_;
    // File header
    std::unique_ptr<LogFileHeader> header_;
    // File size
    uint32_t size_bytes_;
    // In case of an error keep it here.
    std::string last_error_;

    // Wraps a standard file_size() call to return 0 on error.
    void fileSize(const std::filesystem::path &path) {
        size_bytes_ = 0;
        try {
            auto size = std::filesystem::file_size(path);
            if (size != static_cast<std::uintmax_t>(-1)) {
                size_bytes_ = size;
            }
        } catch (...) {
        }
    }

    // Reads the file's header and returns it.
    bool getHeader() {
        std::unique_ptr<LogFileHeader> result;
        if (!if_.is_open()) {
            log_error("Couldn't open file " + path_);
            return false;
        }
        uint16_t size = 0;
        // Read header's size
        if_.read(reinterpret_cast<char *>(&size), sizeof(size));
        if (!if_) {
            log_error("Couldn't read header's size from " + path_);
            return false;
        }
        header_ = std::make_unique<LogFileHeader>();
        if_.read(reinterpret_cast<char *>(result.get()), size);
        if (!if_) {
            log_error("Couldn't read LogFileHeader from " + path_);
            result.reset();
            return false;
        }
        return true;
    }

    void log_error(const std::string &error) {
        last_error_ = error;
        std::cerr << error << std::endl;
    }
public:
    // The first argument is the absolute path of the file, and the second should be set to false if it is a data file.
    FileReader(const std::filesystem::path &path) : path_(path) {
        // Initialize the stream.
        if_.open(path, std::ios::binary);
        if (!getHeader()) {
            return;
        }
        fileSize(path);
    }

    const std::string & last_error() const { return last_error_; }

    // Returns the stream name from the file header.
    std::string name() const { return header_ ? header_->name_ : ""; }
};

}}