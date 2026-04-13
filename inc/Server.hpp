#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>

#include "Socket.hpp"

class Server {
   private:
    Socket*     _listener;
    int         _port;
    std::string _root;

    Server(const Server& other);
    Server& operator=(const Server& other);

   public:
    Server(int port, std::string root);
    ~Server();

    void setup();
    int  getFd() const;

    std::string getRoot() const;
};

#endif