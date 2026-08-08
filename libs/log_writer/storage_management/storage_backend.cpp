#include "storage_backend.h"

#include <filesystem>
#include <iostream>
#include <utility>

#include "folder.h"

namespace tracebox {
namespace logger {

FilesystemStorageBackend::FilesystemStorageBackend(std::string folder)
    : folder_path_(std::move(folder)),
      folder_(std::make_unique<Folder>(folder_path_)) {}

FilesystemStorageBackend::~FilesystemStorageBackend() = default;

std::size_t FilesystemStorageBackend::usedBytes() const {
    std::size_t size = 0;
    for (const auto& entry :
         std::filesystem::directory_iterator(folder_path_)) {
        if (!entry.is_directory()) {
            continue;
        }
        try {
            for (const auto& file : std::filesystem::directory_iterator(entry)) {
                if (!file.is_directory()) {
                    // TODO: add check for the file name pattern.
                    size += file.file_size();
                }
            }
        } catch (...) {
        }
    }
    return size;
}

int FilesystemStorageBackend::findOldest() const {
    int oldest = kMaxDay;
    for (const auto& entry :
         std::filesystem::directory_iterator(folder_path_)) {
        if (!entry.is_directory()) {
            continue;
        }
        try {
            int day = std::stoi(entry.path().filename().string());
            if (day < oldest) {
                oldest = day;
            }
        } catch (...) {
        }
    }
    return oldest;
}

std::size_t FilesystemStorageBackend::deleteFolder(const std::string& folder) {
    std::size_t freed_bytes = 0;
    for (const auto& entry :
         std::filesystem::directory_iterator(folder_path_ + "/" + folder)) {
        if (!entry.is_directory()) {
            freed_bytes += entry.file_size();
        }
    }
    std::filesystem::remove_all(folder_path_ + "/" + folder);
    return freed_bytes;
}

bool FilesystemStorageBackend::removeOldest(std::size_t& freed_bytes) {
    const auto oldest = findOldest();
    if (oldest == kMaxDay) {
        std::cerr << "Couldn't find folder to free some space" << std::endl;
        freed_bytes = 0;
        return false;
    }
    freed_bytes = deleteFolder(std::to_string(oldest));
    return true;
}

int FilesystemStorageBackend::write(const LogRequest& request) {
    if (!folder_->sameDay(request.time_ns())) {
        folder_ = std::make_unique<Folder>(folder_path_);
    }
    return folder_->write(request);
}

}  // namespace logger
}  // namespace tracebox
