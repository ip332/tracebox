#include "storage.h"

#include <filesystem>

namespace tracebox {
namespace logger {

size_t Storage::findUsedBytes() {
    size_t size = 0;
    for (const auto& entry :
            std::filesystem::directory_iterator(folder_path_)) {
        if (entry.is_directory()) {
            try {
                for (const auto& file :
                        std::filesystem::directory_iterator(entry)) {
                    if (!file.is_directory()) {
                        // TODO: add check for the file name pattern
                        size += file.file_size();
                    }
                }
            } catch (...) {
            }
        }
    }
    return size;
}

int Storage::findOldest() {
    int oldest = kMaxDay;
    for (const auto& entry :
         std::filesystem::directory_iterator(folder_path_)) {
        if (entry.is_directory()) {
            try {
                int day = std::stoi(entry.path().filename().string());
                if (day < oldest) {
                    oldest = day;
                }
            } catch (...) {
            }
        }
    }
    return oldest;
}

// Removes the folder and it content
void Storage::deleteFolder(const std::string& folder) {
    for (const auto& entry :
         std::filesystem::directory_iterator(folder_path_ + "/" + folder)) {
        if (!entry.is_directory()) {
            available_bytes_ += entry.file_size();
        }
    }
    std::filesystem::remove_all(folder_path_ + "/" + folder);
}

bool Storage::removeOldest() {
    auto oldest = findOldest();
    if (oldest == kMaxDay) {
        std::cerr << "Couldn't find folder to free some space" << std::endl;
        return false;
    }
    deleteFolder(std::to_string(oldest));
    return true;
}

int Storage::write(const LogRequest& request) {
    // Check available size
    while (maxRecordSize(request.data().size()) > available_bytes_) {
        if (!removeOldest()) {
            return 0;
        }
    }
    // Check if the date has changed (or the folder was not created yet)
    if (!folder_->sameDay(request.time_ns())) {
        folder_ = std::make_unique<Folder>(folder_path_);
    }
    const int written = folder_->write(request);
    if (written > 0) {
        available_bytes_ -= static_cast<size_t>(written);
    }
    return written;
}

}  // namespace logger
}  // namespace tracebox
