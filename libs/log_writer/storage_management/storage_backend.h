#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace tracebox {
namespace logger {

class Folder;
class LogRequest;

// Internal boundary between storage policy and a concrete persistence medium.
// It deliberately operates on existing Tracebox records and day semantics;
// it is not a second record-format or retention-policy abstraction.
class StorageBackend {
   public:
    virtual ~StorageBackend() = default;

    virtual std::size_t usedBytes() const = 0;
    virtual bool removeOldest(std::size_t& freed_bytes) = 0;
    virtual int write(const LogRequest& request) = 0;
};

// Current filesystem implementation. This class is intentionally internal
// and is not part of include/tracebox.
class FilesystemStorageBackend final : public StorageBackend {
   public:
    explicit FilesystemStorageBackend(std::string folder);
    ~FilesystemStorageBackend() override;

    std::size_t usedBytes() const override;
    bool removeOldest(std::size_t& freed_bytes) override;
    int write(const LogRequest& request) override;

   private:
    static constexpr int kMaxDay = 99999999;

    int findOldest() const;
    std::size_t deleteFolder(const std::string& folder);

    std::string folder_path_;
    std::unique_ptr<Folder> folder_;
};

}  // namespace logger
}  // namespace tracebox
