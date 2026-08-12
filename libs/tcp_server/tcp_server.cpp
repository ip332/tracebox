#include "tcp_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <algorithm>
#include <iostream>
#include <utility>

#include "tcp_socket.h"

namespace tracebox {
namespace logger {

namespace {

constexpr int kEpollTimeout = -1;

bool setNonBlocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return false;
    }
    return true;
}

bool setSocketTimeouts(int fd) {
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

void drainEventFd(int fd) {
    uint64_t value = 0;
    while (read(fd, &value, sizeof(value)) == sizeof(value)) {
    }
}

}  // namespace

Server::Server(uint32_t port, data_callback_t cb)
    : requested_port_(port), callback_(std::move(cb)) {}

Server::~Server() { stop(); }

uint16_t Server::port() const {
    std::lock_guard<std::mutex> lock(lifecycle_mutex_);
    return bound_port_;
}

bool Server::start() {
    std::lock_guard<std::mutex> lock(lifecycle_mutex_);
    if (state_.load(std::memory_order_acquire) != State::kStopped) {
        return false;
    }
    state_.store(State::kStarting, std::memory_order_release);

    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        closeStartupDescriptors();
        state_.store(State::kStopped, std::memory_order_release);
        return false;
    }
    wakeup_fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wakeup_fd_ < 0 || !setNonBlocking(wakeup_fd_)) {
        closeStartupDescriptors();
        state_.store(State::kStopped, std::memory_order_release);
        return false;
    }
    listening_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_fd_ < 0 || !setNonBlocking(listening_fd_)) {
        closeStartupDescriptors();
        state_.store(State::kStopped, std::memory_order_release);
        return false;
    }

    int reuse = 1;
    if (setsockopt(listening_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse,
                   sizeof(reuse)) < 0 ||
        !setSocketTimeouts(listening_fd_)) {
        closeStartupDescriptors();
        state_.store(State::kStopped, std::memory_order_release);
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(requested_port_));
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listening_fd_, reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) < 0 || listen(listening_fd_, kMaxClients) < 0) {
        closeStartupDescriptors();
        state_.store(State::kStopped, std::memory_order_release);
        return false;
    }

    sockaddr_in bound{};
    socklen_t bound_length = sizeof(bound);
    if (getsockname(listening_fd_, reinterpret_cast<sockaddr*>(&bound),
                    &bound_length) < 0) {
        closeStartupDescriptors();
        state_.store(State::kStopped, std::memory_order_release);
        return false;
    }
    bound_port_ = ntohs(bound.sin_port);

    if (!registerFd(wakeup_fd_, false) || !registerFd(listening_fd_, false)) {
        closeStartupDescriptors();
        state_.store(State::kStopped, std::memory_order_release);
        return false;
    }

    state_.store(State::kRunning, std::memory_order_release);
    try {
        thread_ = std::thread(&Server::run, this);
    } catch (...) {
        closeStartupDescriptors();
        state_.store(State::kStopped, std::memory_order_release);
        return false;
    }
    return true;
}

void Server::stop() {
    {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        const auto current = state_.load(std::memory_order_acquire);
        if (current == State::kStarting) {
            return;
        }
        if (current == State::kRunning) {
            state_.store(State::kStopping, std::memory_order_release);
            if (wakeup_fd_ >= 0) {
                const uint64_t wake = 1;
                const ssize_t result = write(wakeup_fd_, &wake, sizeof(wake));
                if (result != sizeof(wake) && errno != EAGAIN) {
                    std::cerr << "eventfd wakeup error: " << errno << std::endl;
                }
            }
        }
    }

    if (thread_.joinable()) {
        thread_.join();
    }

    std::lock_guard<std::mutex> lock(lifecycle_mutex_);
    closeStartupDescriptors();
    bound_port_ = 0;
    state_.store(State::kStopped, std::memory_order_release);
}

bool Server::registerFd(int fd, bool client) {
    if (!setNonBlocking(fd)) {
        return false;
    }
    epoll_event event{};
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLPRI | EPOLLERR | EPOLLHUP;
    event.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) {
        return false;
    }
    if (client) {
        if (!setSocketTimeouts(fd)) {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
            return false;
        }
        client_fd_.push_back(fd);
    }
    return true;
}

void Server::unregisterFd(int fd) {
    if (epoll_fd_ >= 0) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    }
    const auto it = std::find(client_fd_.begin(), client_fd_.end(), fd);
    if (it != client_fd_.end()) {
        client_fd_.erase(it);
    }
    close(fd);
}

void Server::closeClientFds() {
    while (!client_fd_.empty()) {
        unregisterFd(client_fd_.back());
    }
}

void Server::closeStartupDescriptors() {
    closeClientFds();
    if (listening_fd_ >= 0) {
        close(listening_fd_);
        listening_fd_ = -1;
    }
    if (wakeup_fd_ >= 0) {
        if (epoll_fd_ >= 0) {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, wakeup_fd_, nullptr);
        }
        close(wakeup_fd_);
        wakeup_fd_ = -1;
    }
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

void Server::run() {
    epoll_event events[kMaxClients + 2];
    try {
        bool shutting_down = false;
        while (!shutting_down) {
            const int events_count =
                epoll_wait(epoll_fd_, events, kMaxClients + 2, kEpollTimeout);
            if (events_count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            for (int i = 0; i < events_count; ++i) {
                const int fd = events[i].data.fd;
                if (fd == wakeup_fd_) {
                    drainEventFd(wakeup_fd_);
                    shutting_down = true;
                    break;
                }
                if (state_.load(std::memory_order_acquire) != State::kRunning) {
                    shutting_down = true;
                    break;
                }
                if (fd == listening_fd_) {
                    sockaddr_in peer{};
                    socklen_t length = sizeof(peer);
                    const int client = accept(listening_fd_,
                                              reinterpret_cast<sockaddr*>(&peer),
                                              &length);
                    if (client < 0) {
                        if (errno != EINTR && errno != EAGAIN &&
                            errno != EWOULDBLOCK) {
                            std::cerr << "accept error: " << errno << std::endl;
                        }
                    } else if (state_.load(std::memory_order_acquire) !=
                               State::kRunning ||
                               !registerFd(client, true)) {
                        close(client);
                    }
                } else if ((events[i].events & EPOLLIN) != 0) {
                    if (!handleLogRequest(fd)) {
                        unregisterFd(fd);
                    }
                } else {
                    unregisterFd(fd);
                }
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "TCP server loop exception: " << error.what() << std::endl;
    } catch (...) {
        std::cerr << "TCP server loop exception" << std::endl;
    }
    closeClientFds();
    if (listening_fd_ >= 0) {
        close(listening_fd_);
        listening_fd_ = -1;
    }
}

bool Server::handleLogRequest(int fd) {
    auto buffer = TcpSocket::readData(fd);
    if (buffer->empty()) {
        return false;
    }
    if (state_.load(std::memory_order_acquire) != State::kRunning) {
        return false;
    }
    try {
        std::lock_guard<std::mutex> callback_lock(callback_mutex_);
        if (state_.load(std::memory_order_acquire) != State::kRunning) {
            return false;
        }
        const auto result = callback_(*buffer);
        if (!result.empty() &&
            state_.load(std::memory_order_acquire) == State::kRunning) {
            TcpSocket::sendData(fd, result);
        }
    } catch (const std::exception& error) {
        std::cerr << "TCP callback exception: " << error.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "TCP callback exception" << std::endl;
        return false;
    }
    return true;
}

}  // namespace logger
}  // namespace tracebox
