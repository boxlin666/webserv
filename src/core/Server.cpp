#include "Server.hpp"
#include <iostream>

Server::Server(PassiveSocket *listener,std::string root) 
    : _listener(listener), _root(root) 
{
    std::cout << "[Server] Port " << this->_listener->getPort() << " is running." << std::endl;    
}

Server::~Server() 
{
    if (this->_listener) {
        delete this->_listener;
        std::cout << "[Server] Port " << this->_listener->getPort() << " has been shut down." << std::endl;
    }
}

int Server::getListenFd() const 
{
    if (!this->_listener) return -1;
    return this->_listener->getFd();
}

const std::string &Server::getRoot() const 
{
    return this->_root;
}