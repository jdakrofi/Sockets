#include "mcast_socket.h"

namespace Common{
    auto McastSocket::init(const string &ip, const string &iface, int port, bool is_listening) -> int{
        const SocketCfg socket_cfg{ip, iface, port, true, is_listening, false};
        socket_fd_ = createSocket(logger_, socket_cfg);
        return socket_fd_;  
    }// auto McastSocket::init()

    bool McastSocket::join(const string &ip){
        return Common::join(socket_fd_, ip);
    }// bool McastSocket::join()

    auto McastSocket::leave(const string &, int) -> void{
        close(socket_fd_);
        socket_fd_ = -1;
    }//  auto McastSocket::leave()

    auto McastSocket::sendAndRecv() noexcept -> bool {
        const ssize_t n_rcv = recv(socket_fd_, inbound_data_.data() + next_rcv_valid_index_, McastBufferSize - next_rcv_valid_index_, MSG_DONTWAIT);
        if(n_rcv > 0){
            next_rcv_valid_index_ += n_rcv;
            logger_.log("%:% %() % read socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket_fd_,
             next_rcv_valid_index_);
             recv_callback_(this);

        }// if(n_rcv > 0)

        if(next_send_valid_index_ > 0){
            ssize_t n = ::send(socket_fd_, outbound_data_.data(), next_send_valid_index_, MSG_DONTWAIT | SO_NOSIGPIPE);
            logger_.log("%:% %() % send socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket_fd_, n);

        }// if(next_send_valid_index_ > 0)
        next_send_valid_index_ = 0;
        return (n_rcv > 0);
    }// auto McastSocket::sendAndRecv()

    auto McastSocket::send(const void *data, size_t len) noexcept -> void{
        memcpy(outbound_data_.data() + next_send_valid_index_, data, len);
        next_send_valid_index_ += len;
        ASSERT(next_send_valid_index_ < McastBufferSize, "Mcast socket buffer filled up and sendAndRecv() not called.");

    }// auto McastSocket::send()
}