#pragma once

#include <vector>
#include <functional>
#include <string>

// Callback to handle an incoming packet with arbitrary data.
// If it returns non-empty result it will be sent out to the client.
using data_callback_t = std::function<std::string(const std::string_view &data)>;

namespace embark {
namespace logger {

// This class implements a typical TCP/IP tcp_server which uses a single thread to handle all incoming packets.
class Server {
    // Make sure that an error in the client's code will not result in an unlimited number of connections.
    // The real number of clients is still TBD.
    static constexpr uint8_t kMaxClients = 100;
    // epoll fd
    int epoll_fd_;
    // Thread control.
    bool running_;
    // Transport: socket details.
    int listening_fd_;
    // File descriptors of all connected clients.
    std::vector<int> client_fd_;
    // Data callback
    data_callback_t callback_;

    // Initializes the tcp_server and starts a thread to listen for the incoming requests.
    bool start(uint32_t port);

    // Registers given fd on the epoll and adds it to the client_fd_ vector.
    bool registerFd(int fd);
    // Unregisters the given fd
    void unregisterFd(int fd);

    // Reads the request from the socket and handles it to the callback.
    bool handleLogRequest(int fd);
public:
    explicit Server(uint32_t port, data_callback_t cb);
    virtual ~Server() { stop(); }

    // Delete copy / assignment constructors
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(Server&&) = delete;

    void stop() { running_ = false; }
};

}}
