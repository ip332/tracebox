#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace tracebox {
namespace logger {

// This header implements a generic TCP functionality commons for client and
// server.
class TcpSocket {
   public:
    // Sets the timeout for the given socket handle
    static void setTimeout(int fd, int sec = 2, int usec = 0) {
        struct timeval timeout;
        timeout.tv_sec = sec;
        timeout.tv_usec = usec;
        // Sending timeout
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        // Reading timeout
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    }

    // Sends a data piece to the given socket handle, size first.
    static bool sendData(int fd, const std::string_view& data) {
        uint32_t payload_size = data.size();
        // Place everything into a single buffer.
        const size_t buffer_size = sizeof(payload_size) + payload_size;
        std::vector<uint8_t> buffer(buffer_size);
        memcpy(buffer.data(), &payload_size, sizeof(payload_size));
        memcpy(&buffer[sizeof(payload_size)], data.data(), payload_size);

        // Send this buffer.
        int result = send(fd, buffer.data(), buffer_size, MSG_NOSIGNAL);
        if (static_cast<uint32_t>(result) != buffer_size) {
            std::cerr << "Error sending " << payload_size
                      << " to the log_writer" << std::endl;
            // Let application decide if the connection should be closed.
            return false;
        }
        return true;
    }

    // Reads data piece from the socket and returns it.
    // Returns empty string in case of any error.
    static std::unique_ptr<std::string> readData(int fd) {
        auto result = std::make_unique<std::string>();
        uint32_t payload_size;
        if (recv(fd, &payload_size, sizeof(payload_size), 0) !=
            sizeof(payload_size)) {
            std::cerr << "Error reading from client's socket: " << errno
                      << std::endl;
            return result;
        }
        // Read the payload into the local buffer.
        std::string buffer(payload_size, 0);
        size_t loaded = 0;

        while (loaded < payload_size) {
            auto res = recv(fd, &buffer[loaded], payload_size - loaded, 0);
            if (res <= 0) {
                std::cerr << "Error reading from client's socket: " << errno
                          << std::endl;
                return result;
            }
            loaded += res;
        }
        *result = std::move(buffer);
        return result;
    }
};

}  // namespace logger
}  // namespace tracebox
