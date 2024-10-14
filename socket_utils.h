/**
 * This code provides a set of utility functions to create, configure and manage both TCP and UDP sockets 
 * It handless complex socket configurations like non-blocking mode, disabling Nagle's algorithm 
 * mulitcast group subscripton, and software receive timestamps.
 * The core function createSocket() automates the process of creating and setting up a socket based on the 
 * user-provided configuration in SocketCfg, and uses the logger to track the process and errors.
 */

#pragma once
#include <iostream>
#include <string>
#include <unordered_set>
#include <sstream>

#include "macros.h"
#include "logging.h"

// Unix system headers. These include various system-level headers for working with epoll
// sockets, address resolution, and interface addresses
#include <sys/event.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <fcntl.h>


using namespace std;
namespace Common{
    /**
     *  This struct defines the configuration for a socket
     */
    struct SocketCfg{
        // The IP address to bind or connect to
        string ip_;

        // The network interface to  use e.g. "eth0"
        string iface_;

        // The port number to bind to or connect through (-1 by default)
        int port_ = -1;

        // Whether this is a UDP socket (default is false for TCP)
        bool is_udp_ = false;

        // Whether the socket should be a listening server socket
        bool is_listening_ = false;

        // Whether software receive timestamps should be enabled
        bool needs_so_timestamp_ = false;

        /**
         * This method provides a human-readbale description of the socket configuration, converting it into a string
         */
        auto toString() const{
            stringstream ss;
            ss << "SocketCfg[ip:]" << ip_
            << " iface:" << iface_
            << " port:" << port_
            << " is_udp:" << is_udp_
            << " is_listening:" << is_listening_
            << " needs_SO_timestamp:" << needs_so_timestamp_
            << "]";

            return ss.str();
        }
    };

    // This defines the maximum number of pending (unaccepted) TCO connections for a listening socket.
    /// The value is set to 1024, which is fairly standard for a server socket
    constexpr int MaxTCPServerBacklog = 1024;

    /*---------------------------------------------------------------------------------------------------------------------------------------*/
    //                                          NOTES ON NETWORK INTERFACE
    /*---------------------------------------------------------------------------------------------------------------------------------------*/
    /* 
     * Boundary between the computer and network, allowing data to be transmitted back and forth.
     * a network interface usually includes several important pieces of information such as:
     * IP address: The address assigned to the interface that is used for network communication.
     * MAC address: The hardware identifier assigned to the network card (for physical interfaces).
     * Netmask/Subnet mask: Information that determines the network portion of the IP address.
     * MTU (Maximum Transmission Unit): The largest size a packet can be for transmission.
     * Flags: Status indicators, such as whether the interface is up or down.
     * The ifaddrs structureis a data structure that holds information about the system's network interfaces. 
     * This structure is typically used with functions like getifaddrs() to retrieve detailed information about all the network interfaces on a system.
     * Each entry in the ifaddrs structure provides information about:
     * Interface name (e.g., eth0, wlan0, lo for loopback).
     * Address: IP address (either IPv4 or IPv6), broadcast address, or other related addresses.
     * Flags: Status flags, indicating whether the interface is up, running, has multicast support, etc.
     * Next interface: Pointer to the next ifaddrs structure in the linked list, allowing the user to traverse the list of interfaces.
     */
    /*---------------------------------------------------------------------------------------------------------------------------------------*/

    /**
     * The function takes a string parameter iface representing the name of a network interface (like "eth0")
     *  It returns the IP address associated with that interface as a string.
     */
    inline auto getIfaceIP(const string &iface) -> string{
        // Character buffer is allocated to store the IP address as a string
        // NI_MAXHOST is a constant tha defines the maximum size for a hostname or IP address.
        // The buffer is intialised to all zeros 
        char buf[NI_MAXHOST] = {'\0'};

        // ifaddrs is a structure that holds information about network interface.
        // Pointer is declared, which will be used to point to the list of network interfaces
        ifaddrs *ifaddr = nullptr;

        // getifaddrs() is a system call that retieves a linked list of network interfaces on the system
        // It stores the pointer to the list in ifaddr
        // If getifaddrs() returns -1, it indicates an error, and the function will skip the logic inside the block
        if(getifaddrs(&ifaddr) != -1){

            // Iterating through the list if interfaces where ifa is a pointer to the current network interface inthe linked list
            // Loop continues as long as ifa is not a nullptr
            // The list of interfaces is traversed using the ifa->ifa_next point, which points to the next interface in the list
            for(ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next){

                // This condition checks whether the current interface has an address (ifa->ifa_addr is not a nullptr)
                // And whether the address is an IPv4 address(sa_Family == AF_INET)
                // It also checks if the name of the current interface (ifa->ifa_name) matches the input parameter iface.
                // If all these conditions are true, we proceed to extract the IP address
                if(ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && iface == ifa->ifa_name){

                    // getnameinfo() is called to convert the socket address (ifa->ifa_addr) into a human-readable IP address
                    // The character buffer (buf) is filled with the IP address in numeric form (e.g. 192.168.1.1)
                    // sizeof(sockaddr_in) is passed as the size of the address because this function only deals with IPv4 addresses
                    // The sockaddr_in structis a fundamental data structure used in socket programming in  C++ for handling IPv4 addresses.
                    // The NULL and 0 parameters indicate that no service name is required
                    //NI_NUMERICHOST ensures that the address is returned in numeric form rather than as a hostname
                    getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
                    break;
                }
            }
            // To prevent memory leaks, after processing the interfaces, we free the memory allocated by getifaddrs() using freeifaddrs.
            freeifaddrs(ifaddr);
        }

        // Return character buffer containing the IP address.
        return buf;
    }

    /**
     * This function, setNonBlocking() is used to set a socket or file descriptor (fd) to non-blocking mode in a Unix-like system.
     * The function utilises the fcntl() system call to modify the file descriptor's flags.
     */
    inline auto setNonBlocking (int fd) -> bool{
        // The function fcntl() is called with the F_GETFL command to retrieve the current flags for the file descriptor fd
        // These flags control the behaviour of the file descriptor (blocking or non-blocking). The result is stored in flags
        const auto flags = fcntl(fd, F_GETFL, 0);
         
        // The bitwise & operation is used to check whether the 0_NONBLOCK flag is already set,
        // If 0_NONBLOCK is set, the function returns true, indicating that the file descriptor is already in non-blocking mode, so changes are needed
        if(flags & O_NONBLOCK) 
            return true;

        // If the file descriptor is not in non-blocking mode, the function calls fcntl() again, this time with the F_SETFL command to modify the flags
        // The 0_NONBLOCK flag is added to the existing flags (flags | 0_NONBLOCK) using the bitwise operation.
        // The return value of fcntl() is compared to -1. If the result is not -1, the operations was successful, and the function returns true.
        // Otherwise it returns false, indicating failure.
        return(fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1);         
    }
    /*---------------------------------------------------------------------------------------------------------------------------------------*/
    //                                          NOTES ON FILE DESCRIPTORS
    /*---------------------------------------------------------------------------------------------------------------------------------------*/
    /* 
     * File descriptors are a general abstraction in Unix-like systems for referencing open resources.
     * The reason sockets are treated similarly to files is because Unix follows the philosophy of "everything is a file."
     * This means that many system resources, including files, devices, and sockets, can be accessed using the same system calls 
     * like read(), write(), close(), and fcntl().
     * File descriptors allow the operating system to abstract away the details of different types of resources, 
     * enabling uniform operations on various objects, like files, sockets, and pipes.
     */
    /*---------------------------------------------------------------------------------------------------------------------------------------*/
    //                                          NOTES ON NON-BLOCKING MODE
    /*---------------------------------------------------------------------------------------------------------------------------------------*/
    /*
     * In non-blocking mode, system calls like read() or write() on the file descriptor will return immediately,
     * even if no data is available (for read()) or if the output buffer is full (for write()),
     * instead of blocking the program's execution until the operation can proceed.
     * This is useful in scenarios where you want to perform asynchronous I/O operations without halting the execution 
     * of your program while waiting for data or output availability, such as in network programming or event-driven architectures
     */
    /*---------------------------------------------------------------------------------------------------------------------------------------*/
    /*---------------------------------------------------------------------------------------------------------------------------------------*/

    /**
     * The disbaleNagle() function is designed to disable Nagle's algorithm on a TCP socket. 
     * Nagle's algorithm is used to optimise the network traffic by reducing the number of small packets sent over the network
     * It bundles small packets of data together before sending them over the network. This reduces the overhead of sending many small packets
     * In certain real-time or low-latency applications, disabling it cna improve performance
     */
    inline auto disableNagle(int fd)-> bool{
        int one = 1;
        /**
         * The setsockopt() function is used to configure options on a socket
         * fd: The file descriptor of the socket
         * IPPROTO_TCP: This indicates that protocol level for which the option is being set is related to TCP
         * TCP_NODELAY: The socket option that disables Nagle's algorithm when set.
         * reinterpret_cast<void *>(&one): The option value (in this case, 1) is passed as a pointer
         * This is used to indicate that Nagle's algorithm should be disabled.
         * reinterpret_cast<void *> us used to cast the address of integer one to a void * as required by setsockopt().
         * sizeof(one): Specifies the size of the option values (in this casem the size of the integer one)
         * The setsockopt() function returns 0 on a success and -1 on failure, The condition checks if the call was successfuel
         * If successful, the function returns true, (Nagle's algo was disabled). otherwise false
         */
        return (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void *>(&one), sizeof(one)) != -1);
    }

    /**
     * The setSOTimestamp() function is designed to enable software receive timestamps for a socket by setting the SO_TIMESTAMP option.
     * This feature enables one to capture the time at which the kernel received incoming packets on a socket.
     * SOL_SOCKET: This indicates that the option being set applies to the socket layer itself as opposed to a specific protocol like IPPROTO_TCP   
     * SO_TIMESTAMP: The specific option being set. This is a socket option, that when enabled allows the kernel to timestamp incoming packets. 
     * This is useful for measuring latency or tracking the time of arrival of packets.
     * The functions returns true if the call to setsockopt() was successful (i.e. the SO_TIMESTAMP option was successfully enabled) otherwise false
     */
    inline auto setSOTimestamp(int fd) -> bool {
        int one = 1;
        // Setting SO_TIMESTAMP to one to enable timestamping
        return (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, reinterpret_cast<void *>(&one), sizeof(one)) != -1);
    }

    /**
     * The join() funtion is designed to make a socket join a mulicast group by using the IP_ADD_MEMBERSHIP option to setsockopt().
     * Multicast is a communication method where data is sent to a group of hosts 
     * As opposed to unicast, which sends data to one specific host, or broadcast which sends data to all hosts in a network.
     * In multicast communication, data is sent sent to a multicast IP address
     * Multiple hosts can subscribe to the multicast group associated with that IP address
     * The IP address range 224.0.0.0 to 239.255.255.255 is reserved for multicast traffic (Class D addresses).
     * The function returns true if the setsockopt() call succeeds (i.e., it returns a value other than -1),
     * Meaning that the socket successfully joined the multicast group. If setsockopt() fails, the function returns false.
     */
    inline auto join(int fd, const string &ip) ->bool{
        /** ip_mreq is a struct used to configure the multicast options in the setsockopt() call
         * struct ip_mreq {
                struct in_addr imr_multiaddr;  // Multicast group address
                struct in_addr imr_interface;  // Local interface address
           };
         *  imr_multiaddr: This is the IP address of the multicast group that the socket will join.
         * The IP address is passed into the function as string ip and is converted to binary from using inet_addr()
         * imr_interface: This specifies the local interface on which to join the multicast group. 
         * INADDR_ANY is used, which means the socket will join the multicast group on all available network interfaces
         */ 
        const ip_mreq mreq{{inet_addr(ip.c_str())}, {htonl(INADDR_ANY)}};

        // IPPROTO_IP: This indicates that the option being set is for IP protocol level
        // IP_ADD_MEMBERSHIP: This option tells the socket to join a multicast groip
        // &mreq: The address of the ip_mreq structure which contains the multicast group IP address and the interface.
        return (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != -1);
    }

    /**
     * The createSocket() function, is responsible for creating a network socket based on the configuration provided in by the SocketCfg struct.
     * It supports both TCP and UDP and it handles various socket configurations, such as setting non-blocking mode, disabling Nagle's algo
     * enabling timestamping and configuring the socket to either listen for incoming connections or connect to a remote server
     * The return value is an int which is the file descriptor of the created socket or -1 if the socket creation fails
     */

    /*---------------------------------------------------------------------------------------------------------------------------------------*/
    //                                          NOTES ON NODISCARD
    /*---------------------------------------------------------------------------------------------------------------------------------------*/
    /*
     * A C++17 feature that is used to indicate that the return value of the function should not be ignored by the caller
     * If it is ignores the compiler will issue a warning
     * This is important for functions where ignoring the return value could lead to errors.
     * In this case where a socket is being created, the file descriptor needs to be checked. Ignoring this value could lead to problems
     */
    /*---------------------------------------------------------------------------------------------------------------------------------------*/

    // Logger &logger: A logger instance used to log the progress of socket creation and any error
    // const SocketCfg& socket_cfg: A struct that holds the socket settings including IP address, interface, port, UDP or TCP, listening socket etc
    [[nodiscard]] inline auto createSocket(Logger &logger, const SocketCfg& socket_cfg) -> int {
        string time_str;

        // If socket_cfg.iP) is empty, getIfaceIP() is called to get the IP address associated with the network interface socket.cfg.iface_
        // Otherwise socket_cfg.ip_ provides the IP address
        const auto ip = socket_cfg.ip_.empty() ? getIfaceIP(socket_cfg.iface_) : socket_cfg.ip_;

        // Logging the details of the socket configuration, including file name, line number and the current timestamp
        logger.log("%:% %() % cfg:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str), socket_cfg.toString());

        // socket_cfg.is_listening_: Boolean flag inthe SocketCfg struct that determines whether the socket should be created for listening (server side)
        // AI_PASSIVE: This flag is used to to indicated that the returned address will be used for binding a listening socket.
        // E.g. a server waiting for a incoming connections.
        // If the socket is not for listening (client socket) the value 0 is used. That is, no special flags for listening are needed.
        // AI_NUMERICHOST: indicates the IP address is in numeric form rather than a hostname that needs to be recolved by DNS
        // AI_NUMERICSERV: indicating the port is numeric rather than a service name e.g. http
        // The bitwise OR combines the flags together, creating a single integer value where all the flags are set
        // The result of this line is an integer input_flags that contains:
        // AI_PASSIVE (if the socket is for listening) or 0 (if it's for connecting).
        // AI_NUMERICHOST (indicating the IP address is in numeric form).
        // AI_NUMERICSERV (indicating the port is numeric)
        // These flags help configure how the system resolves the address and port for the socket.
        const int input_flags = (socket_cfg.is_listening_ ? AI_PASSIVE : 0) | (AI_NUMERICHOST | AI_NUMERICSERV);

        /*---------------------------------------------------------------------------------------------------------------------------------------*/
        //                                          NOTES ON ADDRINFO STRUCT
        /*---------------------------------------------------------------------------------------------------------------------------------------*/
        /**
        struct addrinfo {
            int ai_flags: Input flags (e.g., AI_PASSIVE, AI_NUMERICHOST)
            int ai_family: Address family (e.g., AF_INET for IPv4, AF_INET6 for IPv6)
            int ai_socktype: Socket type e.g., SOCK_STREAM for TCP Sockets(connection-oriented and ensure reliable delivery), 
                            SOCK_DGRAM for UDP (connectionless and do not guarantee delivery)
            int ai_protocol: Protocol (e.g., IPPROTO_TCP for TCP, IPPROTO_UDP for UDP)
            size_t ai_addrlen: Size of the socket address. getaddrinfo() will populate this field with the returned addrinfo struct, so it is set to 0 (null).
            struct sockaddr *ai_addr: Pointer to the sockaddrs structure. getaddrinfo() will populate this field with the returned addrinfo struct, so it is set to 0.
            char *ai_canonname: Canonical name for the host (optional). Its set to nullptr since we're not interested in resolving hostnames in this context (we're using numeric addresses)
            struct addrinfo *ai_next: Pointer to the next addrinfo structure. Used to chain multiple addrinfo structures together if multiple addresses are returned. 
                                      Initially set to nullptr as this just the hints structure and not a result
        };
         */
        /*---------------------------------------------------------------------------------------------------------------------------------------*/
        // Defining and initialising an addrinfo struct named hints which will be passed to getaddrinfo(). 
        // This line of code is configuring the hints structure, which will guide the getaddrinfo() function in resolving the IP address and port
        // into the correct format for creating a socket. 
        const addrinfo hints {input_flags, AF_INET, socket_cfg.is_udp_ ? SOCK_DGRAM : SOCK_STREAM, socket_cfg.is_udp_ ? IPPROTO_UDP : IPPROTO_TCP,
                              0, 0, nullptr, nullptr};

        // Pointer to struct addrinfo. Will store a linked list of addrinfo structures with resolved addresses and port returned by getaddrinfo().
        addrinfo *result =  nullptr;

        /**
         * This is an important step in network programming, where resolving an IP address and port into something the system can work with is crucial
         * for establishing connections or listening for incoming connections.
         * getaddrinfo() is used to convert human-readable IP address (hostname) and port to a structured format (addrinfo) that can be used for creating
         * and connecting sockets.
         * ip.c_str(): Converts the ip string into a C-style string (const char*) as expected by getaddrinfo()
         * to_string(socket_cfg.port_).c_str(): Converts the port integer to a  C-style string 
         * &hints: Pointer to the addrinfo struct called hints
         * &result: Pointer to the addrinfo struct that will hold the results of the address resolution
         * rc: getaddrinfo() returns 0 on success, meaning that is successfull resolved the IP and port into the appropriated address structures
         * if it fails, a non-zero error code indicating what went wrong is returned
         */
        const auto rc = getaddrinfo(ip.c_str(), to_string(socket_cfg.port_).c_str(), &hints, &result);

        /**
         * This ASSERT() is used to veriy that getaddrinfo() as succesful (rc = 0)
         * If the assertions fails (rc is non-zero) and error message is generated
         * gai_strerror(rc): Converts the error code returned by getaddrinfo() into a human-readable error message.
         * strerror(errno): This provides the error number in string format 
         * In the case that the error is related to a system call, errno is used to capture it. This adds additional information about the failure
         */ 
        ASSERT(!rc, "getaddrinfo() failed. error:" + string(gai_strerror(rc)) + "errno:" + strerror(errno));
        

        /**
         * This portion of code is responsible for iterating over the linked list of addrinfo structs stored in the result variable
         * This was obtained from the getaddrinfo() call above
         * Subsequently, a socket is created using the best matching address
         * Various socket options are applied depending on the configuration specified in the socket_cfg object parameter passed into the createSocket() function
         */

        // File descriptor for the socket. It is initialised to -1 indicating the socket has not been successfully created
        int socket_fd = -1;

        // This integer is used in later setsockopt() calls to enable certain socket options like reusing the address for binding
        int one = 1;

        // This loop iterates over the linked list of addrinfo structs  stored in the result variable
        // rp points the current addrinfo structure
        // loop moves through the linked list by following the ai_next pointer
        for(addrinfo *rp = result; rp; rp = rp->ai_next){

            // Creating a socket using the socket() function. 
            // The socket file descriptor is stored in socket_fd. If socket() fails (returns -1), the ASSERT() macro logs the error and aborts the program
            ASSERT((socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) != -1, "socket() failed. errno:" 
                                + string(strerror(errno)));
            
            // The socket is configured to operate in non-blocking mode. If this operation fails, the program logs the error and aborts
            ASSERT(setNonBlocking(socket_fd), "setNonBlocking() failed. errno:" + string(strerror(errno)));

            // Nagle's algorithm is disabled for TCP sockets. This algorithm delays the sending of small packets to optimise network efficiency
            // The UDP protocol does not use Nagle's algorithm
            if(!socket_cfg.is_udp_){
                ASSERT(disableNagle(socket_fd), "disableNagle() failed. errno:" + string(strerror(errno)));
            }

            // If the socket is not configured as a listening socket (i.e. client socket) it attempts to connect to a remote server using
            // the address stored in rp->ai_addr. If the connect() call fails, the error is logged, and the program terminates
            if(!socket_cfg.is_listening_){
                ASSERT(connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != 1, "connect() failed. errno:" + string(strerror(errno)));
            }

            // The SO_REUSEADDR: This option allows the socket to bind to a port that might still be in the TIME_WAIT state left over from previous connections
            // This is useful for servers that restart frequently
            // The setsockopt() function is used to enable this option for listening sockets
            // If the call fails, the error is logged, and the program terminates
            if(socket_cfg.is_listening_){
                ASSERT(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&one), sizeof(one)) == 0, 
                       "setsockopt() SO_REUSEADDR failed. errno:" + string(strerror(errno)));
            }

            // The socket is bound to a INADDR_ANY, which allows it to listen on all available networks  for incoming connections.
            // After binding, the socket is set to listen for incoming connections, using a backlog of up to MaxRCPServerBacklog connections
            // If the call fails, the error is logged, and the program terminates
            // htons(): This function is used to convert a 16-bit port number from host byte order to network byte order.
            // htonl(): This function converts a 32-bit integer from host byte order to network byte order (big-endian). 
            // INADDR_ANY: This constant represents a wildcard address (0.0.0.0), which means the socket will bind to all available network interfaces
            // on the machine (e.g., Ethernet, Wi-Fi, etc.). It allows the socket to accept connections on any network interface.
            if(socket_cfg.is_listening_){
                const sockaddr_in addr{AF_INET, htons(socket_cfg.port_), {htonl(INADDR_ANY)}, {}};
                ASSERT(listen(socket_fd, MaxTCPServerBacklog) == 0, "listen() failed. errno:" + string(strerror(errno)));
            }
            
        /*---------------------------------------------------------------------------------------------------------------------------------------*/
        //                                          NOTES ON SOCKADDR_IN STRUCT
        /*---------------------------------------------------------------------------------------------------------------------------------------*/
        /**
        struct sockaddr_in {
            sa_family_t    sin_family: Address family (AF_INET for IPv4)
            in_port_t      sin_port: Port number (in network byte order)
            struct in_addr sin_addr: IP address (in network byte order)
            char           sin_zero[8]: Padding to match the size of sockaddr
};
         */
        /*---------------------------------------------------------------------------------------------------------------------------------------*/
        // Defining and initialising an addrinfo struct named hints which will be passed to getaddrinfo(). 
            
            // This is essentially a redundant check to ensure that TCP listening sockets are set to listen
            // As this is missions-critical application this check is useful for covering one's back
            // If the call fails, the error is logged, and the program terminates
            if(!socket_cfg.is_udp_ && socket_cfg.is_listening_){
                ASSERT(listen(socket_fd, MaxTCPServerBacklog) == 0, "listen() failed. errno:" + string(strerror(errno)));
            }

            // This funtion enables software-level receive timestampes on the socket, which maybe needed in applications where precise timing 
            // information about incoming packets is required.
            // If this option is requested viasocket_cfg object parameter passed into createSocket() (i.e. socket_cfg.needs_so_timestamp = true)
            // Then it is enabled using setsockopt() via a call to setSOTimestamp()
            // If the call fails, the error is logged, and the program terminates
            if(socket_cfg.needs_so_timestamp_){
                ASSERT(setSOTimestamp(socket_fd), "setSOTimestamp() failed. errno:" + string(strerror(errno)));
            }

            return socket_fd;

        }
    }
}//Common