#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "data_types.h"
#include "log_writer.h"
#include "storage.h"
#include "storage_scanner.h"
#include "tcp_server.h"
#include "tcp_socket.h"

namespace {

using namespace std::chrono_literals;
using tracebox::logger::LogRequest;
using tracebox::logger::Server;

class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        path_ = std::filesystem::temp_directory_path() /
                ("tracebox_lifecycle_" +
                 std::to_string(std::chrono::steady_clock::now()
                                    .time_since_epoch()
                                    .count()));
        if (!std::filesystem::create_directories(path_)) {
            throw std::runtime_error("failed to create test directory");
        }
    }
    ~TemporaryDirectory() { std::filesystem::remove_all(path_); }
    const std::filesystem::path& path() const { return path_; }

   private:
    std::filesystem::path path_;
};

int openFdCount() {
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator("/proc/self/fd")) {
        (void)entry;
        ++count;
    }
    return count;
}

int connectTo(const Server& server) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    timeval timeout{};
    timeout.tv_sec = 1;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(server.port());
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

LogRequest makeRequest(uint64_t timestamp, const std::string& data = "abc") {
    LogRequest request;
    request.set_channel("lifecycle");
    request.set_size(static_cast<uint32_t>(data.size()));
    request.set_time_ns(timestamp);
    request.set_data(data);
    return request;
}

TEST(ServerLifecycleTest, StopBeforeStartAndRepeatedStartStopAreDeterministic) {
    Server server(0, [](const std::string_view&) { return std::string(); });
    server.stop();
    EXPECT_EQ(server.state(), Server::State::kStopped);
    ASSERT_TRUE(server.start());
    EXPECT_FALSE(server.start());
    ASSERT_GT(server.port(), 0);
    server.stop();
    EXPECT_EQ(server.state(), Server::State::kStopped);
    server.stop();
    ASSERT_TRUE(server.start());
    server.stop();
    EXPECT_EQ(server.state(), Server::State::kStopped);
}

TEST(ServerLifecycleTest, DestructionWakesBlockedEpollAndClosesIdleClient) {
    int client = -1;
    std::mutex mutex;
    std::condition_variable condition;
    bool handled = false;
    {
        Server server(0, [&](const std::string_view&) {
            std::lock_guard<std::mutex> lock(mutex);
            handled = true;
            condition.notify_all();
            return std::string();
        });
        ASSERT_TRUE(server.start());
        client = connectTo(server);
        ASSERT_GE(client, 0);
        ASSERT_TRUE(tracebox::logger::TcpSocket::sendData(client, "accept"));
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(condition.wait_for(lock, 1s, [&] { return handled; }));
    }
    char byte = 0;
    EXPECT_EQ(recv(client, &byte, sizeof(byte), 0), 0);
    close(client);
}

TEST(ServerLifecycleTest, ActiveCallbackFinishesBeforeStopCompletes) {
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
    bool release = false;
    Server server(0, [&](const std::string_view&) {
        std::unique_lock<std::mutex> lock(mutex);
        entered = true;
        condition.notify_all();
        condition.wait(lock, [&] { return release; });
        return std::string();
    });
    ASSERT_TRUE(server.start());
    const int client = connectTo(server);
    ASSERT_GE(client, 0);
    ASSERT_TRUE(tracebox::logger::TcpSocket::sendData(client, "callback"));
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(condition.wait_for(lock, 1s, [&] { return entered; }));
    }
    auto stop_future = std::async(std::launch::async, [&] { server.stop(); });
    EXPECT_EQ(stop_future.wait_for(100ms), std::future_status::timeout);
    {
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
    }
    condition.notify_all();
    EXPECT_EQ(stop_future.wait_for(1s), std::future_status::ready);
    close(client);
}

TEST(ServerLifecycleTest, CallbackExceptionClosesOnlyAffectedConnection) {
    std::atomic<int> calls{0};
    Server server(0, [&](const std::string_view&) -> std::string {
        ++calls;
        throw std::runtime_error("test callback failure");
    });
    ASSERT_TRUE(server.start());
    const int client = connectTo(server);
    ASSERT_GE(client, 0);
    ASSERT_TRUE(tracebox::logger::TcpSocket::sendData(client, "throws"));
    char byte = 0;
    EXPECT_EQ(recv(client, &byte, sizeof(byte), 0), 0);
    EXPECT_EQ(calls.load(), 1);
    close(client);
    EXPECT_EQ(server.state(), Server::State::kRunning);
}

TEST(ServerLifecycleTest, FailedBindDoesNotLeakDescriptors) {
    const int before = openFdCount();
    Server first(0, [](const std::string_view&) { return std::string(); });
    ASSERT_TRUE(first.start());
    Server second(first.port(), [](const std::string_view&) { return std::string(); });
    EXPECT_FALSE(second.start());
    second.stop();
    first.stop();
    EXPECT_EQ(openFdCount(), before);
}

TEST(ServerLifecycleTest, RepeatedEphemeralConstructionDestruction) {
    for (int i = 0; i < 25; ++i) {
        Server server(0, [](const std::string_view&) { return std::string(); });
        ASSERT_TRUE(server.start());
        ASSERT_GT(server.port(), 0);
        server.stop();
    }
}

TEST(LogWriterLifecycleTest, DrainsQueuedRecordsBeforeDestruction) {
    TemporaryDirectory directory;
    auto storage = std::make_shared<tracebox::logger::Storage>(
        directory.path().string(), 1024 * 1024);
    constexpr uint64_t timestamp = 1704067200000000000ULL;
    ASSERT_GT(storage->write(makeRequest(timestamp)), 0);
    {
        tracebox::logger::LogWriter writer(storage);
        for (int i = 0; i < 12; ++i) {
            ASSERT_TRUE(writer.add(makeRequest(timestamp + i)));
        }
    }
    storage.reset();
    tracebox::logger::StorageScanner scanner(directory.path().string());
    const auto streams = scanner.getStreams(timestamp, timestamp + 12);
    ASSERT_EQ(streams->stream_size(), 1);
    EXPECT_EQ(streams->stream(0).records_cnt(), 13U);
}

TEST(LogWriterLifecycleTest, EmptyWakeupAndAdmissionAfterStopAreDeterministic) {
    TemporaryDirectory directory;
    auto storage = std::make_shared<tracebox::logger::Storage>(
        directory.path().string(), 1024 * 1024);
    ASSERT_GT(storage->write(makeRequest(1704067200000000000ULL)), 0);
    tracebox::logger::LogWriter writer(storage);
    writer.stop();
    EXPECT_FALSE(writer.add(makeRequest(1704067200000000000ULL)));
    writer.stop();
}

TEST(LogWriterLifecycleTest, ConcurrentEnqueueAndStopDrainsEveryAcceptedEntry) {
    TemporaryDirectory directory;
    auto storage = std::make_shared<tracebox::logger::Storage>(
        directory.path().string(), 1024 * 1024);
    ASSERT_GT(storage->write(makeRequest(1704067200000000000ULL)), 0);
    auto writer = std::make_unique<tracebox::logger::LogWriter>(storage);
    std::atomic<bool> produce{true};
    std::atomic<int> accepted{0};
    std::promise<void> first_accepted;
    std::thread producer([&] {
        for (uint64_t i = 0; produce.load(); ++i) {
            if (writer->add(makeRequest(1704067200000000000ULL + i))) {
                if (accepted.fetch_add(1) == 0) {
                    first_accepted.set_value();
                }
            }
        }
    });
    EXPECT_EQ(first_accepted.get_future().wait_for(1s),
              std::future_status::ready);
    auto stop_future = std::async(std::launch::async, [&] { writer->stop(); });
    EXPECT_EQ(stop_future.wait_for(1s), std::future_status::ready);
    produce.store(false);
    producer.join();
    EXPECT_FALSE(writer->add(makeRequest(1704067200000000000ULL)));
    EXPECT_GT(accepted.load(), 0);
    writer.reset();
    storage.reset();
    tracebox::logger::StorageScanner scanner(directory.path().string());
    const auto streams = scanner.getStreams(1704067200000000000ULL,
                                            1704067200000000000ULL + 1000000);
    ASSERT_EQ(streams->stream_size(), 1);
    EXPECT_EQ(streams->stream(0).records_cnt(),
              static_cast<uint32_t>(accepted.load() + 1));
}

}  // namespace
