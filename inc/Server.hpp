#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>

#include "PassiveSocket.hpp"

class Server {
   private:
    PassiveSocket*     _listener;
    std::string _root;
    // Config   _config;

    Server(const Server& other);
    Server& operator=(const Server& other);

   public:
    Server(PassiveSocket* listener, std::string root);
    ~Server();

    int  getListenFd() const;

    const std::string &getRoot() const;
};

#endif