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
#include <vector>
#include <iterator>

#include "ServerConfig.hpp"

class PassiveSocket {
   private:
    int _fd;
    std::string _host;
    int _port;

    std::vector<int> _listen_ports;

    std::vector<ServerConfig*> _server_configs;

    void _init_socket();
    void _set_options();
    void _bind_and_listen();

    PassiveSocket(const PassiveSocket& other);
    PassiveSocket& operator=(const PassiveSocket& other);

   public:
    PassiveSocket(const ServerConfig::ListenAddr &listen_addr);
    ~PassiveSocket();

    int getFd() const;
    int getPort() const;
    const std::string &get_host() const;
    ServerConfig* match_server(std::string hostname);
};

#endif