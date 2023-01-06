#pragma once

#include <string>
#include <unistd.h>
#include "tcp_socket.h"

namespace embark {
namespace logger {

// This class implements a generic TCP/IP client to send data to a tcp_server.
class TcpClient {
    // Socket handle
    int fd_ = -1;
public:
    TcpClient() {}

    virtual ~TcpClient() { disconnect(); }

    // Delete copy / assignment constructors
    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;
    TcpClient(TcpClient&&) = delete;
    TcpClient& operator=(TcpClient&&) = delete;

    // Connects to the tcp_server. Returns false if it is unavailable.
    // It is up to the application to decide what to do in such case with the data it needs to log.
    bool connect(const std::string &ip, uint32_t port);

    bool is_connected() const { return fd_ > 0; }

    // Closes connections to the tcp_server.
    void disconnect() {
        if (fd_ > 0) {
            close(fd_);
        }
        fd_ = -1;
    }

    void setTimeout(int sec, int usec ) {
        TcpSocket::setTimeout(fd_, sec, usec);
    }

    // Sends a data piece to the tcp_server.
    bool sendData(const std::string & data) {
        return TcpSocket::sendData(fd_, data);
    }

    std::unique_ptr<std::string> readData() {
        return TcpSocket::readData(fd_);
    }
};

}}
