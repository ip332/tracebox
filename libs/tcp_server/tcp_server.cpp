#include "tcp_server.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <thread>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "logger.pb.h"
#include "tcp_socket.h"

namespace embark {
namespace logger {

Server::Server(uint32_t port, data_callback_t cb) : callback_(cb) {
    epoll_fd_ = epoll_create(1);
    if (epoll_fd_ < 0) {
        std::cerr << "Error creating epoll " << errno << std::endl;
        return;
    }
    if (!start(port)) {
        std::cerr << "Error starting tcp_server " << errno << std::endl;
    }
}

bool Server::start(uint32_t port) {
    // Create tcp_server's socket and initialize it for incoming connections.
    listening_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    // Set options
    int options = 1;
    if (setsockopt(listening_fd_, SOL_SOCKET, SO_REUSEADDR, &options, sizeof(options)) < 0) {
        std::cerr << "setsockopt error: " << errno << std::endl;
        return false;
    }

    // Initialize address/port structure
    struct sockaddr_in self;
    memset(&self, 0, sizeof(self));
    self.sin_family = AF_INET;
    self.sin_port = htons((uint16_t)port);
    self.sin_addr.s_addr = htonl(INADDR_ANY);

    // Assign a port number to the socket
    if (bind(listening_fd_, (struct sockaddr *)&self, sizeof(self))) {
        std::cerr << "bind error: " << errno << std::endl;
        return false;
    }

    if (listen(listening_fd_, kMaxClients)) {
        std::cerr << "listen error: " << errno << std::endl;
        return false;
    }

    std::thread([this]() {
        running_ = true;
        epoll_event events[kMaxClients + 1];
        while(running_) {
            int events_count = epoll_wait(epoll_fd_, events, kMaxClients + 1, 1000);
            for (int i = 0; i < events_count; i++) {
                if (events[i].data.fd == listening_fd_) {
                    // Handle incoming connection request
                    sockaddr_in sockaddrIn;
                    uint32_t length = sizeof(sockaddrIn);
                    int fd = accept(listening_fd_,(sockaddr*)&sockaddrIn, &length);
                    if (fd == -1) {
                        std::cerr << "accept error: " << errno << std::endl;
                    } else {
                        registerFd(fd);
                    }
                } else {
                    if (events[i].events == EPOLLIN) {
                        if (!handleLogRequest(events[i].data.fd)) {
                            // Incomplete reading - close the socket.
                            unregisterFd(events[i].data.fd);
                        }
                    } else {
                        // treat any other event as a connection lost:
                        unregisterFd(events[i].data.fd);
                    }
                }
            }
        }
        for (auto fd : client_fd_) {
            close(fd);
        }
        close(listening_fd_);
        close(epoll_fd_);
    }).detach();
    // Add tcp_server's fd to the epoll.
    return registerFd(listening_fd_);
}

bool Server::registerFd(int fd) {
    // Socket should be in a non-blocking mode
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    epoll_event event;
    event.events = EPOLLIN  | // The associated file is available for read(2) operations.
                   EPOLLRDHUP  | // Stream socket peer closed connection, or shut down writing half of connection.
                   EPOLLPRI    | // There is an exceptional condition on the file descriptor.
                   EPOLLERR    | // Error condition happened on the associated file
                   EPOLLHUP;     // Hang up happened on the associated file descriptor.
    event.data.fd = fd;
    if(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event)) {
        return false;
    }
    client_fd_.push_back(fd);
    TcpSocket::setTimeout(fd);
    return true;

}

void Server::unregisterFd(int fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    auto it = std::find(client_fd_.begin(), client_fd_.end(), fd);
    if (it != client_fd_.end()) {
        client_fd_.erase(it);
    }
    close(fd);
}

bool Server::handleLogRequest(int fd) {
    auto buffer = TcpSocket::readData(fd);
    if (buffer->empty()) {
        std::cerr << "Error reading from client's socket: " << errno << std::endl;
        return false;
    }

    bool ret = true;
    auto result = callback_(*buffer);
    if (!result.empty()) {
        TcpSocket::sendData(fd, result);
    }
    return ret;
}

}}
