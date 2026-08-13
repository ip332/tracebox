#include <gtest/gtest.h>

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <vector>

#include "tcp_socket.h"

namespace {

using tracebox::logger::TcpSocket;

struct ScriptedCall {
    ssize_t result;
    int error;
};

struct SendScript {
    std::vector<ScriptedCall> calls;
    size_t next = 0;
    std::string bytes;
};

SendScript* send_script = nullptr;

ssize_t scriptedSend(int, const void* data, size_t size, int) {
    if (send_script == nullptr) return -1;
    const auto call = send_script->calls.at(send_script->next++);
    if (call.result < 0) {
        errno = call.error;
        return -1;
    }
    const size_t written = static_cast<size_t>(call.result);
    if (written > size) return -1;
    send_script->bytes.append(static_cast<const char*>(data), written);
    return call.result;
}

struct RecvScript {
    std::string bytes;
    std::vector<ScriptedCall> calls;
    size_t offset = 0;
    size_t next = 0;
};

RecvScript* recv_script = nullptr;

ssize_t scriptedRecv(int, void* data, size_t size, int) {
    if (recv_script == nullptr) return -1;
    const auto call = recv_script->calls.at(recv_script->next++);
    if (call.result < 0) {
        errno = call.error;
        return -1;
    }
    const size_t received = static_cast<size_t>(call.result);
    if (received > size ||
        recv_script->offset + received > recv_script->bytes.size()) {
        return -1;
    }
    memcpy(data, recv_script->bytes.data() + recv_script->offset, received);
    recv_script->offset += received;
    return call.result;
}

std::string frame(const std::string& payload) {
    const uint32_t size = static_cast<uint32_t>(payload.size());
    std::string result(sizeof(size), '\0');
    memcpy(result.data(), &size, sizeof(size));
    result += payload;
    return result;
}

TEST(TcpFramingTest, SendsACompleteFrame) {
    SendScript script{{{9, 0}}, 0, {}};
    send_script = &script;
    EXPECT_TRUE(TcpSocket::sendDataWith(1, "hello", scriptedSend));
    EXPECT_EQ(script.bytes, frame("hello"));
    send_script = nullptr;
}

TEST(TcpFramingTest, CompletesAFrameAcrossPartialWrites) {
    SendScript script{{{2, 0}, {3, 0}, {4, 0}}, 0, {}};
    send_script = &script;
    EXPECT_TRUE(TcpSocket::sendDataWith(1, "hello", scriptedSend));
    EXPECT_EQ(script.bytes, frame("hello"));
    EXPECT_EQ(script.next, 3U);
    send_script = nullptr;
}

TEST(TcpFramingTest, RetriesInterruptedWrites) {
    SendScript script{{{-1, EINTR}, {9, 0}}, 0, {}};
    send_script = &script;
    EXPECT_TRUE(TcpSocket::sendDataWith(1, "hello", scriptedSend));
    EXPECT_EQ(script.bytes, frame("hello"));
    send_script = nullptr;
}

TEST(TcpFramingTest, FailsAfterPartialWriteWithoutReportingSuccess) {
    SendScript script{{{3, 0}, {-1, EPIPE}}, 0, {}};
    send_script = &script;
    EXPECT_FALSE(TcpSocket::sendDataWith(1, "hello", scriptedSend));
    EXPECT_EQ(script.bytes, frame("hello").substr(0, 3));
    send_script = nullptr;
}

TEST(TcpFramingTest, ZeroLengthWriteFails) {
    SendScript script{{{0, 0}}, 0, {}};
    send_script = &script;
    EXPECT_FALSE(TcpSocket::sendDataWith(1, "hello", scriptedSend));
    send_script = nullptr;
}

TEST(TcpFramingTest, DoesNotRaiseSigpipeWhenPeerIsClosed) {
    int sockets[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
    ASSERT_EQ(close(sockets[1]), 0);
    EXPECT_FALSE(TcpSocket::sendData(sockets[0], "hello"));
    close(sockets[0]);
}

TEST(TcpFramingTest, ReadsSplitHeaderAndPayloadAndRetriesEintr) {
    const auto bytes = frame("hello");
    RecvScript script{bytes, {{2, 0}, {-1, EINTR}, {2, 0}, {5, 0}}, 0, 0};
    recv_script = &script;
    const auto result = TcpSocket::readDataWith(1, scriptedRecv);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, "hello");
    recv_script = nullptr;
}

TEST(TcpFramingTest, ReportsEofInTheMiddleOfAFrameAsFailure) {
    const auto bytes = frame("hello");
    RecvScript script{bytes.substr(0, bytes.size() - 1),
                      {{4, 0}, {4, 0}, {0, 0}}, 0, 0};
    recv_script = &script;
    const auto result = TcpSocket::readDataWith(1, scriptedRecv);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->empty());
    recv_script = nullptr;
}

TEST(TcpFramingTest, ReadsMultipleFramesFromAStreamSocket) {
    int sockets[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
    ASSERT_TRUE(TcpSocket::sendData(sockets[0], "one"));
    ASSERT_TRUE(TcpSocket::sendData(sockets[0], "two"));
    EXPECT_EQ(*TcpSocket::readData(sockets[1]), "one");
    EXPECT_EQ(*TcpSocket::readData(sockets[1]), "two");
    close(sockets[0]);
    close(sockets[1]);
}

}  // namespace
