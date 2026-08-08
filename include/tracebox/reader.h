#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "tracebox/stream.h"

namespace tracebox {

class Reader {
   public:
    Reader();
    ~Reader();

    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    Reader(Reader&&) noexcept;
    Reader& operator=(Reader&&) noexcept;

    bool connect(const std::string& address, std::uint32_t port);
    bool connected() const;
    void disconnect();

    std::vector<Stream> streams(
        Timestamp start, Timestamp end, std::uint32_t start_index = 0,
        std::uint32_t max_count = UINT32_MAX);

    ReadResult read(const std::string& file, Timestamp start,
                    Timestamp end, std::uint32_t start_index = 0,
                    std::uint32_t max_count = UINT32_MAX);

   private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tracebox
