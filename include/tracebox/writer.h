#pragma once

#include <memory>

#include "tracebox/storage.h"

namespace tracebox {

class Writer {
   public:
    explicit Writer(std::shared_ptr<Storage> storage);
    ~Writer();

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    Writer(Writer&&) noexcept;
    Writer& operator=(Writer&&) noexcept;

    // Adds a record to the asynchronous storage queue.
    void add(const Record& record);

   private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tracebox
