#include "Server.hpp"
#include <iostream>

Server::Server(int port, std::string root) 
    : _listener(NULL), _port(port), _root(root) {}

Server::~Server() 
{
    if (_listener) {
        delete _listener;
        std::cout << "[Server] Port " << _port << " has been shut down." << std::endl;
    }
}

void Server::setupListener() 
{
    _listener = new PassiveSocket(_port); 
    std::cout << "[Server] Port " << _port << " is up and running." << std::endl;
}

int Server::getListenFd() const 
{
    if (!_listener) return -1;
    return _listener->getFd();
}

const std::string &Server::getRoot() const 
{
    return _root;
}