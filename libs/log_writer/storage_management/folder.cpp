#include "folder.h"

#include <filesystem>

namespace tracebox {
namespace logger {

bool Folder::create(uint64_t time_ns) {
    yyyymmdd_ = Time2YYYYMMDD(time_ns);
    folder_ = folder_ + "/" + std::to_string(yyyymmdd_);
    if (!std::filesystem::exists(folder_) &&
        !std::filesystem::create_directories(folder_)) {
        std::cerr << "Couldn't create folder " << folder_ << std::endl;
        yyyymmdd_ = 0;
        return false;
    }
    return true;
}

// Write request into the file and returns the number of bytes written or an
// error code.
int Folder::write(const LogRequest& request) {
    if (!yyyymmdd_) {
        if (!create(request.time_ns())) {
            return -EINVAL;
        }
    }
    int result = 0;
    auto it = files_.find(request.channel());
    if (it == files_.end()) {
        // Create a new logging channel
        files_.emplace(request.channel(), folder_);
        it = files_.find(request.channel());
        result = it->second.openFile(request);
        if (result <= 0) {
            return result;
        }
    }
    return it->second.write(request.time_ns(), request.data());
}

}  // namespace logger
}  // namespace tracebox
