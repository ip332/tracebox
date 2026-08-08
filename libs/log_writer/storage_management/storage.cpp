#include "storage.h"

#include <memory>

#include "data_types.h"
#include "logger.pb.h"

namespace tracebox {
namespace logger {

Storage::Storage(const std::string& folder, int max_size)
    : available_bytes_(max_size),
      backend_(std::make_unique<FilesystemStorageBackend>(folder)) {
    auto used = backend_->usedBytes();
    while (used > available_bytes_) {
        std::size_t freed_bytes = 0;
        backend_->removeOldest(freed_bytes);
        available_bytes_ += freed_bytes;
    }
    available_bytes_ -= used;
}

int Storage::write(const LogRequest& request) {
    // Check available size.
    while (maxRecordSize(request.data().size()) > available_bytes_) {
        std::size_t freed_bytes = 0;
        if (!backend_->removeOldest(freed_bytes)) {
            return 0;
        }
        available_bytes_ += freed_bytes;
    }

    const int written = backend_->write(request);
    if (written > 0) {
        available_bytes_ -= static_cast<std::size_t>(written);
    }
    return written;
}

}  // namespace logger
}  // namespace tracebox
