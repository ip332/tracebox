#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "tracebox/timestamp.h"

namespace tracebox {

struct Record {
    std::string channel;
    std::uint32_t record_size = 0;
    std::string data;
    Timestamp timestamp = 0;
};

class Writer;

class Storage {
   public:
    Storage(std::string folder, std::size_t max_size_bytes);
    ~Storage();

    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;
    Storage(Storage&&) noexcept;
    Storage& operator=(Storage&&) noexcept;

    // Returns the number of bytes accepted, or the existing implementation's
    // error/zero result when the request cannot be stored.
    int write(const Record& record);

   private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    friend class Writer;
};

}  // namespace tracebox
