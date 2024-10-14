#pragma once
#include <functional>
#include "socket_utils.h"
#include "logging.h"

using namespace std;
namespace Common{
    constexpr size_t TCPBufferSize = 64 * 1024 * 1024;

    struct TCPSocket{
        explicit TCPSocket(Logger &logger) : logger_(logger){
            outbound_data_.resize(TCPBufferSize);
            inbound_data_.resize(TCPBufferSize);
        }

        auto connect(const string &ip, const string &iface, int port, bool is_listening) -> int;

        auto sendAndRecv() noexcept -> bool;

        auto send(const void *data, size_t len) noexcept -> void;

        TCPSocket() = delete;
        TCPSocket(const TCPSocket &) = delete;
        TCPSocket(const TCPSocket &&) = delete;
        TCPSocket &operator = (const TCPSocket &) = delete;
        TCPSocket &operator = (const TCPSocket &&) = delete;

        int socket_fd_ = -1;

        vector<char> outbound_data_;
        size_t next_send_valid_index_ = 0;
        vector<char> inbound_data_;
        size_t next_rcv_valid_index_ = 0;

        struct sockaddr_in socket_attrib_{};

        function<void(TCPSocket *s, Nanos rx_time)> recv_callback_ = nullptr;
        string time_str_;
        Logger &logger_;

    };// struct TCPSocket

}//  Namespace Common
