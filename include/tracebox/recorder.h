#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "tracebox/timestamp.h"

namespace tracebox {

class Recorder {
   public:
    Recorder(std::string stream_name, std::uint32_t record_size = 0);
    ~Recorder();

    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;
    Recorder(Recorder&&) noexcept;
    Recorder& operator=(Recorder&&) noexcept;

    bool connect(const std::string& address, std::uint32_t port);
    bool connected() const;
    void disconnect();

    // Queues a record for transmission. A true result means the framed
    // message was accepted by the TCP client; it does not confirm persistence.
    bool record(const std::string& data, Timestamp timestamp);

   private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tracebox
