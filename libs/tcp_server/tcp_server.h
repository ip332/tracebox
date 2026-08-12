#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using data_callback_t =
    std::function<std::string(const std::string_view& data)>;

namespace tracebox {
namespace logger {

class Server {
   public:
    enum class State { kStopped, kStarting, kRunning, kStopping };

   private:
    static constexpr uint8_t kMaxClients = 100;
    int epoll_fd_ = -1;
    int wakeup_fd_ = -1;
    int listening_fd_ = -1;
    uint32_t requested_port_;
    uint16_t bound_port_ = 0;
    std::vector<int> client_fd_;
    data_callback_t callback_;
    mutable std::mutex lifecycle_mutex_;
    std::mutex callback_mutex_;
    std::atomic<State> state_{State::kStopped};
    std::thread thread_;

    bool registerFd(int fd, bool client);
    void unregisterFd(int fd);
    void closeClientFds();
    void closeStartupDescriptors();
    void run();
    bool handleLogRequest(int fd);

   public:
    explicit Server(uint32_t port, data_callback_t cb);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(Server&&) = delete;

    bool start();
    void stop();
    State state() const { return state_.load(std::memory_order_acquire); }
    uint16_t port() const;
};

}  // namespace logger
}  // namespace tracebox
