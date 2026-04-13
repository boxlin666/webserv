#include "Server.hpp"
#include <iostream>

Server::Server(int port, std::string root) 
    : _listener(NULL), _port(port), _root(root) {}

Server::~Server() {
    if (_listener) {
        delete _listener;
        std::cout << "[Server] Port " << _port << " has been shut down." << std::endl;
    }
}

void Server::setup() {
    _listener = new Socket(_port); 
    std::cout << "[Server] Port " << _port << " is up and running." << std::endl;
}

int Server::getFd() const {
    if (!_listener) return -1;
    return _listener->getFd();
}

std::string Server::getRoot() const {
    return _root;
}