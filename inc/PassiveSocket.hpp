#ifndef PASSIVE_SOCKET_HPP
#define PASSIVE_SOCKET_HPP

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

class PassiveSocket {
   private:
    int _fd;
    int _port;

    void _init_socket();
    void _set_options();
    void _bind_and_listen();

    PassiveSocket(const PassiveSocket& other);
    PassiveSocket& operator=(const PassiveSocket& other);

   public:
    PassiveSocket(int port);
    ~PassiveSocket();

    int getFd() const;
};

#endif