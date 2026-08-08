#include "tcp_client.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstring>
#include <iostream>

namespace tracebox {
namespace logger {

bool TcpClient::connect(const std::string &ip_addr, uint32_t port) {
    if (fd_ > 0) {
        return false;
    }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    // Initialize client address/port struct
    if (fd < 0) {
        std::cerr << "Error creating a client's socket, errno: " << errno
                  << std::endl;
        return false;
    }
    sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons((uint16_t)port);
    if (!inet_aton(ip_addr.c_str(), &dest.sin_addr)) {
        std::cerr << "Error parsing tcp_server address " << ip_addr
                  << ", errno: " << errno << std::endl;
        disconnect();
        return false;
    }
    // Limit connecting/sending attempt by 1 seconds
    TcpSocket::setTimeout(fd);
    if (::connect(fd, (struct sockaddr *)&dest, sizeof(dest))) {
        // Do not print an error message and let the application deciding if it
        // is needed or not.
        std::cerr << "Error: " << errno << std::endl;
        disconnect();
        return -1;
    }
    fd_ = fd;
    return true;
}

}  // namespace logger
}  // namespace tracebox