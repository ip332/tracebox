#pragma once

#include "storage_backend.h"

namespace tracebox {
namespace logger {

// This class implements standard interface to the file system objects in the
// storage folder. The storage folder is divided onto sub-folders - each one
// representing a single day recordings.
class Storage {
    // Keep track of the space available for writing
    size_t available_bytes_;

    // Storage owns policy; the backend owns location and persistence details.
    std::unique_ptr<StorageBackend> backend_;

   public:
    Storage(const std::string &folder, int max_size);

    int write(const LogRequest &request);
};

}  // namespace logger
}  // namespace tracebox
