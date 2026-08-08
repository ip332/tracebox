#pragma once

#include <filesystem>
#include <fstream>
#include <memory>

namespace tracebox {
namespace logger {

// Small file-system boundary used by StreamWriter. Keeping this interface
// separate makes write failures deterministic in tests and allows platform
// specific implementations without changing the logger logic.
class FileIO {
   public:
    virtual ~FileIO() = default;

    virtual bool open(const std::filesystem::path& path,
                      std::ios::openmode mode) = 0;
    virtual bool isOpen() const = 0;
    virtual bool write(const char* data, std::size_t size) = 0;
    virtual bool flush() = 0;
    virtual void close() = 0;
};

class StandardFileIO final : public FileIO {
   public:
    bool open(const std::filesystem::path& path, std::ios::openmode mode) override {
        stream_.open(path, mode);
        return stream_.is_open();
    }

    bool isOpen() const override { return stream_.is_open(); }

    bool write(const char* data, std::size_t size) override {
        stream_.write(data, static_cast<std::streamsize>(size));
        return static_cast<bool>(stream_);
    }

    bool flush() override {
        stream_.flush();
        return static_cast<bool>(stream_);
    }

    void close() override { stream_.close(); }

   private:
    std::ofstream stream_;
};

inline std::unique_ptr<FileIO> makeStandardFileIO() {
    return std::make_unique<StandardFileIO>();
}

}  // namespace logger
}  // namespace tracebox
