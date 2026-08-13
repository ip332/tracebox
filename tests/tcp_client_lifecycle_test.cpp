#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <filesystem>
#include <string>

#include "tcp_client.h"
#include "tcp_server.h"

namespace {

using tracebox::logger::Server;
using tracebox::logger::TcpClient;

int countOpenDescriptors() {
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator("/proc/self/fd")) {
        (void)entry;
        ++count;
    }
    return count;
}

int reserveUnusedLoopbackPort() {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(fd);
        return -1;
    }
    socklen_t length = sizeof(address);
    getsockname(fd, reinterpret_cast<sockaddr*>(&address), &length);
    const int port = ntohs(address.sin_port);
    close(fd);
    return port;
}

TEST(TcpClientLifecycleTest, SuccessfulLoopbackConnection) {
    Server server(0, [](const std::string_view&) { return std::string(); });
    ASSERT_TRUE(server.start());
    TcpClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", server.port()));
    EXPECT_TRUE(client.is_connected());
    client.disconnect();
    EXPECT_FALSE(client.is_connected());
    server.stop();
}

TEST(TcpClientLifecycleTest, InvalidAddressRefusedAndInvalidPortsReturnFalse) {
    TcpClient client;
    EXPECT_FALSE(client.connect("not-an-ipv4-address", 1));
    EXPECT_FALSE(client.connect("127.0.0.1", 0));
    EXPECT_FALSE(client.connect("127.0.0.1", 65536));

    const int refused_port = reserveUnusedLoopbackPort();
    ASSERT_GT(refused_port, 0);
    EXPECT_FALSE(client.connect("127.0.0.1", refused_port));
    EXPECT_FALSE(client.is_connected());
}

TEST(TcpClientLifecycleTest, DescriptorZeroIsValidAndDisconnectClosesIt) {
    const int saved_stdin = dup(STDIN_FILENO);
    if (saved_stdin >= 0) {
        ASSERT_EQ(close(STDIN_FILENO), 0);
    }
    const int reserved_stdin = open("/dev/null", O_RDWR);
    ASSERT_EQ(reserved_stdin, STDIN_FILENO);

    Server server(0, [](const std::string_view&) { return std::string(); });
    ASSERT_TRUE(server.start());
    ASSERT_EQ(close(reserved_stdin), 0);

    TcpClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", server.port()));
    EXPECT_TRUE(client.is_connected());
    server.stop();
    client.disconnect();
    EXPECT_FALSE(client.is_connected());
    EXPECT_EQ(fcntl(STDIN_FILENO, F_GETFD), -1);

    if (saved_stdin >= 0) {
        ASSERT_EQ(dup2(saved_stdin, STDIN_FILENO), STDIN_FILENO);
        close(saved_stdin);
    }
}

TEST(TcpClientLifecycleTest, RepeatedDisconnectAndDestructionAreSafe) {
    Server server(0, [](const std::string_view&) { return std::string(); });
    ASSERT_TRUE(server.start());
    {
        TcpClient client;
        ASSERT_TRUE(client.connect("127.0.0.1", server.port()));
        client.disconnect();
        client.disconnect();
    }
    server.stop();
}

TEST(TcpClientLifecycleTest, RepeatedFailedAttemptsDoNotLeakDescriptors) {
    const int before = countOpenDescriptors();
    const int refused_port = reserveUnusedLoopbackPort();
    ASSERT_GT(refused_port, 0);
    TcpClient client;
    for (int i = 0; i < 20; ++i) {
        EXPECT_FALSE(client.connect("127.0.0.1", refused_port));
        EXPECT_FALSE(client.connect("bad-address", 1));
    }
    EXPECT_FALSE(client.is_connected());
    EXPECT_EQ(countOpenDescriptors(), before);
}

TEST(TcpClientLifecycleTest, SuccessfulReconnectClosesTheFirstDescriptor) {
    Server server(0, [](const std::string_view&) { return std::string(); });
    ASSERT_TRUE(server.start());
    TcpClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", server.port()));
    const int after_first = countOpenDescriptors();
    ASSERT_TRUE(client.connect("127.0.0.1", server.port()));
    EXPECT_TRUE(client.is_connected());
    // The server may retain the first accepted peer until shutdown.  One
    // additional descriptor is therefore allowed; a leaked client descriptor
    // would add a second one.
    EXPECT_LE(countOpenDescriptors(), after_first + 1);
    client.disconnect();
    server.stop();
}

TEST(TcpClientLifecycleTest, FailedReconnectLeavesClientDisconnected) {
    Server server(0, [](const std::string_view&) { return std::string(); });
    ASSERT_TRUE(server.start());
    TcpClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", server.port()));
    const int before_failed_attempt = countOpenDescriptors();
    EXPECT_FALSE(client.connect("bad-address", 1));
    EXPECT_FALSE(client.is_connected());
    EXPECT_LE(countOpenDescriptors(), before_failed_attempt);
    server.stop();
}

}  // namespace
