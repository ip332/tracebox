#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace tracebox {
namespace logger {

// This header implements a generic TCP functionality commons for client and
// server.
class TcpSocket {
   public:
    using send_function_t = ssize_t (*)(int, const void*, size_t, int);
    using recv_function_t = ssize_t (*)(int, void*, size_t, int);

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
        return sendDataWith(fd, data, ::send);
    }

    // The syscall arguments are injectable for deterministic transport tests.
    // Production callers should use sendData().
    static bool sendDataWith(int fd, const std::string_view& data,
                             send_function_t send_function) {
        if (data.size() > std::numeric_limits<uint32_t>::max()) {
            return false;
        }
        const uint32_t payload_size = static_cast<uint32_t>(data.size());
        // Place everything into a single buffer.
        const size_t buffer_size = sizeof(payload_size) + payload_size;
        std::vector<uint8_t> buffer(buffer_size);
        memcpy(buffer.data(), &payload_size, sizeof(payload_size));
        memcpy(&buffer[sizeof(payload_size)], data.data(), payload_size);

        size_t sent = 0;
        while (sent < buffer.size()) {
            const ssize_t result = send_function(
                fd, buffer.data() + sent, buffer.size() - sent, MSG_NOSIGNAL);
            if (result > 0) {
                sent += static_cast<size_t>(result);
                continue;
            }
            if (result < 0 && errno == EINTR) {
                continue;
            }
            std::cerr << "Error sending " << payload_size
                      << " to the log_writer" << std::endl;
            return false;
        }
        return true;
    }

    // Reads data piece from the socket and returns it.
    // Returns empty string in case of any error.
    static std::unique_ptr<std::string> readData(int fd) {
        return readDataWith(fd, ::recv);
    }

    // The syscall arguments are injectable for deterministic transport tests.
    // Production callers should use readData().
    static std::unique_ptr<std::string> readDataWith(
        int fd, recv_function_t recv_function) {
        auto result = std::make_unique<std::string>();
        uint32_t payload_size = 0;
        size_t header_loaded = 0;
        auto* header = reinterpret_cast<uint8_t*>(&payload_size);
        while (header_loaded < sizeof(payload_size)) {
            const ssize_t received = recv_function(
                fd, header + header_loaded, sizeof(payload_size) - header_loaded,
                0);
            if (received > 0) {
                header_loaded += static_cast<size_t>(received);
                continue;
            }
            if (received < 0 && errno == EINTR) {
                continue;
            }
            std::cerr << "Error reading from client's socket: " << errno
                      << std::endl;
            return result;
        }
        // Read the payload into the local buffer.
        std::string buffer(payload_size, 0);
        size_t loaded = 0;

        while (loaded < payload_size) {
            auto res = recv_function(fd, &buffer[loaded],
                                     payload_size - loaded, 0);
            if (res < 0 && errno == EINTR) {
                continue;
            }
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
