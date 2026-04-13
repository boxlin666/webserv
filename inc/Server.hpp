#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>

#include "PassiveSocket.hpp"

class Server {
   private:
    PassiveSocket*     _listener;
    int         _port;
    std::string _root;

    Server(const Server& other);
    Server& operator=(const Server& other);

   public:
    Server(int port, std::string root);
    ~Server();

    void setupListener();
    int  getListenFd() const;

    const std::string &getRoot() const;
};

#endif