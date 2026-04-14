#include "Server.hpp"
#include <iostream>

Server::Server(int port, std::string root) 
    : _listener(NULL), _port(port), _root(root) {}

Server::~Server() 
{
    if (this->_listener) {
        delete this->_listener;
        std::cout << "[Server] Port " << this->_port << " has been shut down." << std::endl;
    }
}

void Server::setupListener() 
{
    this->_listener = new PassiveSocket(this->_port);
    std::cout << "[Server] Port " << this->_port << " is up and running." << std::endl;
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