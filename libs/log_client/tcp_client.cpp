#include "tcp_client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace tracebox {
namespace logger {

namespace {

class ScopedFd {
   public:
    explicit ScopedFd(int fd) : fd_(fd) {}
    ~ScopedFd() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;

    int get() const { return fd_; }
    int release() {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

   private:
    int fd_;
};

bool setClientTimeouts(int fd) {
    timeval timeout{};
    timeout.tv_sec = 2;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        return false;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        return false;
    }
    return true;
}

}  // namespace

bool TcpClient::connect(const std::string& ip_addr, uint32_t port) {
    disconnect();

    if (port == 0 || port > 65535) {
        return false;
    }

    ScopedFd fd(socket(AF_INET, SOCK_STREAM, 0));
    if (fd.get() < 0) {
        return false;
    }

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_aton(ip_addr.c_str(), &destination.sin_addr) == 0) {
        return false;
    }

    // These options are required for the synchronous client. Failure aborts
    // the attempt; ScopedFd closes the local descriptor.
    if (!setClientTimeouts(fd.get())) {
        return false;
    }

    int result = -1;
    do {
        result = ::connect(fd.get(), reinterpret_cast<sockaddr*>(&destination),
                           sizeof(destination));
    } while (result < 0 && errno == EINTR);
    if (result < 0) {
        return false;
    }

    fd_ = fd.release();
    return true;
}

}  // namespace logger
}  // namespace tracebox
