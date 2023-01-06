#pragma once

#include "folder.h"

namespace embark {
namespace logger {

// This class implements standard interface to the file system objects in the storage folder.
// The storage folder is divided onto sub-folders - each one representing a single day recordings.
class Storage {
    // The maximum value we can fit into 8 digit (YYYYMMDD) integer
    constexpr static int kMaxDay = 99999999;
    // Keep track of the space available for writing
    size_t available_bytes_;

    // Absolute path for the root folder.
    std::string folder_path_;
    // The active sub-folder which typically represents the today's day.
    std::unique_ptr<Folder> folder_;

    // Finds the oldest day (YYYYMMDD format) and returns it.
    int findOldest();

    // Removes the folder and it content
    void deleteFolder(const std::string &folder);

    // Finds the oldest folder and removes it from the storage.
    bool removeOldest();

public:
    Storage(const std::string &folder, int max_size)
        : available_bytes_(max_size), folder_path_(folder), folder_(std::make_unique<Folder>(folder)) {}

    int write(const LogRequest & request);
};

}}
