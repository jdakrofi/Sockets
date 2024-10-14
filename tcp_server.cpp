#include "tcp_server.h"

namespace Common{
    auto TCPServer::addToEpollList(TCPSocket *socket){
        struct kevent ev;
        EV_SET(&ev, socket->socket_fd_, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, reinterpret_cast<void *>(socket));

        return kevent(kqueue_fd_, &ev, 1, nullptr, 0, nullptr) != 1;
    }// auto TCPServer::addToEpollList()

    auto TCPServer::listen(const string &iface, int port) -> void{
        kqueue_fd_ = kqueue();
        ASSERT(kqueue_fd_ >= 0, "epoll_create() failed error:" + string(strerror(errno)));

        ASSERT(listener_socket_.connect("", iface, port, true) >=0, "Listener socket failed to connect. iface:" + iface + " port:" + to_string(port)
                                         + " error:" + string(strerror(errno)));

        ASSERT(addToEpollList(&listener_socket_), "epoll_ctl() failed. error:" + string(strerror(errno)));

    } // auto TCPServer::listen()

    auto TCPServer::sendAndRecv() noexcept -> void{
        auto recv = false;

        for_each(receive_sockets_.begin(), receive_sockets_.end(), [&recv](auto socket){
            recv | = socket -> sendAndRecv();
        });

        if(recv) recv_finished_callback_();

        for_each(send_sockets_.begin(), send_sockets_.end(), [](auto socket){
            socket -> sendAndRecv();
        });
    }//  auto TCPServer::sendAndRecv()

    auto TCPServer::poll() noexcept -> void {
        const int max_events = 1 + send_sockets_.size() + receive_sockets_.size();

        const int n  =  kevent(kqueue_fd_, nullptr, 0, events_, max_events, {0});
        bool have_new_connection = false;
        for(int i  = 0; i<n; ++i){
            const auto &event = events_[i];
            auto socket = reinterpret_cast<TCPSocket *>(event.udata);

            if (event.filter == EVFILT_READ){
                if(socket == &listener_socket_){
                    logger_.log("%:% %() % EVFILT_READ listener_socket:%\n", __FILE__, __LINE__, __FUNCTION__,
                  Common::getCurrentTimeStr(&time_str_), socket->socket_fd_);

                  have_new_connection = true;
                  continue;
                }// if(socket == &listener_socket_)
                logger_.log("%:% %() % EVFILT_READ socket:%\n", __FILE__, __LINE__, __FUNCTION__, 
                Common::getCurrentTimeStr(&time_str_), socket->socket_fd_);
                
                if(find(receive_sockets_.begin(), receive_sockets_.end(), socket) == receive_sockets_.end()){
                    receive_sockets_.push_back(socket);
                }// if(find(receive_sockets_.begin(), receive_sockets_.end(), socket) == receive_sockets_.end())
            }// if(event.filter == EVFILT_READ)

            if(event.filter== EVFILT_WRITE){
                logger_.log("%:% %() % EVFILT_WRITE socket:%\n", __FILE__, __LINE__, __FUNCTION__, 
                Common::getCurrentTimeStr(&time_str_), socket->socket_fd_);

                if(find(send_sockets_.begin(), send_sockets_.end(), socket) == send_sockets_.end()){
                    send_sockets_.push_back(socket);
                }// if(find(send_sockets_.begin(), send_sockets_.end(), socket) == send_sockets_.end())
            }// if(event.filter== EVFILT_WRITE)

            if(event.flags & EV_ERROR){
                logger_.log("%:% %() % EV_ERROR socket:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket->socket_fd_);
                if(find(receive_sockets_.begin(), receive_sockets_.end(), socket) == receive_sockets_.end()){
                    receive_sockets_.push_back(socket);
                }
            }

        }//for
       
        while(have_new_connection){
            logger_.log("%:% %() % have_new_connection\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));

              sockaddr_storage addr;
              socklen_t addr_len = sizeof(addr);
              int fd = accept(listener_socket_.socket_fd_, reinterpret_cast<sockaddr *>(&addr), &addr_len);
              if(fd == -1) break;

              ASSERT(setNonBlocking(fd) && disableNagle(fd), "Failed to set non-blocking or no-delay on socket:" + to_string(fd));
              logger_.log("%:% %() % accepted socket:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), fd);

              auto socket = new TCPSocket(logger_);
              socket->socket_fd_ = fd;
              socket->recv_callback_ = recv_callback_;

              struct kevent evSet;

              EV_SET(&evSet, socket->socket_fd_, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, reinterpret_cast<void *>(socket));
              ASSERT(kevent(kqueue_fd_, &evSet, 1, nullptr, 0, nullptr) != -1, "Unable to add socket. error:" + string(strerror(errno)));

              if(find(receive_sockets_.begin(), receive_sockets_.end(), socket) == receive_sockets_.end()) receive_sockets_.push_back(socket);
             

        }//while(have_new_connection)

    }// auto TCPServer::poll()


}// namespace Common
