#include "Socket.hpp"

Socket::Socket(int port) : _fd(-1), _port(port)
{
    try {
        _init_socket();
        _set_options();
        _bind_and_listen();
    } catch (...) {
        if (_fd != -1) close(_fd);
        throw;
    }
}

void Socket::_init_socket()
{
    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_fd < 0) throw std::runtime_error("socket creation failed");
}

void Socket::_set_options()
{
    int opt = 1;
    if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error("setsockopt failed");
    // 暂时注释掉这一行 用于测试
    // if (fcntl(_fd, F_SETFL, O_NONBLOCK) < 0) throw std::runtime_error("fcntl non-block failed");
}

void Socket::_bind_and_listen()
{
    struct addrinfo hints, *result;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    std::stringstream ss;
    ss << _port;
    std::string port_str = ss.str();

    int status = getaddrinfo(NULL, port_str.c_str(), &hints, &result);
    if (status != 0) throw std::runtime_error("getaddrinfo: " + std::string(gai_strerror(status)));

    if (bind(_fd, result->ai_addr, result->ai_addrlen) < 0) {
        freeaddrinfo(result);
        throw std::runtime_error("bind failed on port " + port_str);
    }

    freeaddrinfo(result);

    if (listen(_fd, 128) < 0) throw std::runtime_error("listen failed");
}

int Socket::getFd() const {
    return this->_fd;
}

Socket::~Socket()
{
    if (_fd != -1) {
        close(_fd);
        std::cout << "Socket on port " << _port << " closed." << std::endl;
    }
}
