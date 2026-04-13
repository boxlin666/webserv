#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

class Socket {
   private:
    int _fd;
    int _port;

    void _init_socket();
    void _set_options();
    void _bind_and_listen();

    Socket(const Socket& other);
    Socket& operator=(const Socket& other);

   public:
    Socket(int port);
    ~Socket();

    int getFd() const;
};

#endif