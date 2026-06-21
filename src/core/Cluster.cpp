#include "Cluster.hpp"

#include <algorithm>
#include <iterator>
#include <set>

Cluster::Cluster(void)
{ _is_running = false; }

Cluster::~Cluster()
{
    std::map<int, PassiveSocket*>::iterator sock_it;
    for (sock_it = this->_socket_map.begin(); sock_it != this->_socket_map.end(); sock_it++)
        delete sock_it->second;
    this->_socket_map.clear();

    std::map<int, Connection*>::iterator conn_it;
    for (conn_it = this->_connection_map.begin(); conn_it != this->_connection_map.end(); conn_it++)
        delete conn_it->second;
    this->_connection_map.clear();
}

void Cluster::setup(const ConfigParser& config)
{
    if (!_sig_pipe.init()) {
        throw std::runtime_error("Failed to initialize signal notification pipe");
    }

    Webserv::g_signal_bridge = &_sig_pipe;
    std::signal(SIGINT, Webserv::cSignalHandler);
    std::signal(SIGTERM, Webserv::cSignalHandler);
    std::signal(SIGPIPE, SIG_IGN);

    this->init_servers_map(config);
    this->open_listener(config);
    this->init_poll_listen_fds();

    this->print_socket_map();
    this->print_servers_map();
    this->print_pfds();

    add_to_poll_fds(_sig_pipe.getReadFd());
}

void Cluster::open_listener(const ConfigParser& config)
{
    std::vector<ServerConfig*>::const_iterator         vector_it;
    std::set<ServerConfig::ListenAddr>::const_iterator set_it;
    std::set<ServerConfig::ListenAddr>                 set_listen_addrs;

    for (vector_it = config.get_servers().begin(); vector_it != config.get_servers().end();
         vector_it++) {
        std::vector<ServerConfig::ListenAddr>::const_iterator listen_it;

        if (*vector_it == NULL) continue;
        for (listen_it = (*vector_it)->get_listen_addrs().begin();
             listen_it != (*vector_it)->get_listen_addrs().end(); listen_it++) {
            set_listen_addrs.insert(*listen_it);
        }
    }

    for (set_it = set_listen_addrs.begin(); set_it != set_listen_addrs.end(); set_it++) {
        try {
            PassiveSocket* listener = new PassiveSocket(*set_it);
            this->_socket_map.insert(std::make_pair(listener->getFd(), listener));
        } catch (const std::exception& e) {
            std::cerr << "Failed to open listener " << set_it->first << ":" << set_it->second
                      << std::endl;
        }
    }
    if (this->_socket_map.empty())
        throw std::runtime_error("No listener could be opened, Webserv cannot start!");
}

void Cluster::init_servers_map(const ConfigParser& config)
{
    int port_num;

    for (std::size_t k = 0; k < config.get_servers().size(); k++) {
        const std::vector<ServerConfig::ListenAddr>& listen_addrs =
            config.get_servers()[k]->get_listen_addrs();
        for (std::size_t i = 0; i < listen_addrs.size(); i++) {
            port_num = listen_addrs[i].second;
            this->_servers_map[port_num].push_back(config.get_servers()[k]);
        }
    }
}

void Cluster::init_poll_listen_fds()
{
    std::map<int, PassiveSocket*>::const_iterator map_it;

    for (map_it = this->_socket_map.begin(); map_it != this->_socket_map.end(); map_it++) {
        this->add_to_poll_fds(map_it->first);
    }
}

void Cluster::add_to_poll_fds(int fd)
{
    struct pollfd pfd;

    std::memset(&pfd, 0, sizeof(pfd));
    pfd.fd      = fd;
    pfd.events  = POLLIN;
    pfd.revents = 0;
    this->_poll_fds.push_back(pfd);
}

void Cluster::handle_new_connection(int listen_fd, PassiveSocket* passive_socket)
{
    struct sockaddr_in client_addr;
    socklen_t          client_len = sizeof(client_addr);
    int                client_fd  = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
    std::map<int, std::vector<ServerConfig*> >::const_iterator it_servers;

    if (client_fd < 0) {
        std::cerr << "[Warning] accept() failed to check out a new client connection. Skipping..."
                  << std::endl;
        return;
    }

    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    it_servers = this->_servers_map.find(passive_socket->getPort());

    if (it_servers != this->_servers_map.end()) {
        Connection* conn = new Connection(client_fd, passive_socket, it_servers->second, this);
        _connection_map[client_fd] = conn;
        this->add_to_poll_fds(client_fd);
    } else {
        std::cerr << "Warning: No configurtion found for port " << passive_socket->getPort()
                  << std::endl;
        close(client_fd);
    }
}

void Cluster::close_connection(size_t poll_idx)
{
    int fd = _poll_fds[poll_idx].fd;

    std::map<int, Connection*>::iterator it = _connection_map.find(fd);
    if (it != _connection_map.end()) {
        delete it->second;
        _connection_map.erase(it);
    }

    close(fd);
    this->_poll_fds[poll_idx].fd      = -1;
    this->_poll_fds[poll_idx].revents = 0;
}

bool Cluster::handle_client_read_event(size_t poll_idx)
{
    int fd = _poll_fds[poll_idx].fd;

    std::map<int, Connection*>::iterator it = _connection_map.find(fd);
    if (it == _connection_map.end() || it->second == NULL) { return false; }
    Connection& conn = *(it->second);
    conn.handle_read_event();

    short poll_event = conn.get_poll_events();

    if (poll_event == POLLIN)
        _poll_fds[poll_idx].events = POLLIN;
    else if (poll_event == POLLOUT)
        _poll_fds[poll_idx].events = POLLOUT;
    else
        return (false);
    return (true);
}

bool Cluster::handle_client_write_event(size_t poll_idx)
{
    int fd = _poll_fds[poll_idx].fd;

    std::map<int, Connection*>::iterator it = _connection_map.find(fd);
    if (it == _connection_map.end() || it->second == NULL) { return false; }
    Connection& conn = *(it->second);

    conn.handle_write_event();

    short poll_event = conn.get_poll_events();

    if (!conn.get_out_buff().empty()) { poll_event |= POLLOUT; }

    if (conn.get_state() == Connection::CLOSED) { return (false); }
    _poll_fds[poll_idx].events = poll_event;
    return true;
}

void Cluster::run()
{
    time_t last_check_time = std::time(NULL);

    _is_running = true;
    while (_is_running) {
        int ret = poll(_poll_fds.data(), _poll_fds.size(), 1000);
        if (ret == -1) {
            if (_handle_poll_error() == true) break;
        } else if (ret > 0) {
            for (size_t i = 0; i < _poll_fds.size(); ++i) {
                if (_poll_fds[i].fd == -1) continue;

                if (_poll_fds[i].fd == _sig_pipe.getReadFd()) {
                    if (_poll_fds[i].revents & POLLIN) {
                        _handleSignalEvent();
                        break;
                    }
                    continue;
                }
                if (_poll_fds[i].revents & (POLLERR | POLLNVAL)) {
                    this->_process_poll_errors(i);
                    continue;
                }

                if (_poll_fds[i].revents & POLLIN) this->_dispatch_read_event(i);

                if (_poll_fds[i].revents & POLLHUP) this->_dispatch_pollhup_event(i);

                if (_poll_fds[i].fd != -1 && (_poll_fds[i].revents & POLLOUT))
                    this->_dispatch_write_event(i);
            }
        }

        this->cleanup_inactive_fds();

        time_t current_time = std::time(NULL);
        if (current_time - last_check_time >= 1) {
            _check_timeouts(current_time);
            _manage_cgi_lifecycle();
            last_check_time = current_time;
        }
    }
}

bool Cluster::_handle_poll_error()
{
    char    check_buf[1];
    ssize_t sig_check = read(_sig_pipe.getReadFd(), check_buf, 1);

    if (sig_check > 0 && check_buf[0] == 'W') {
        std::cout << "\n[Signal Detected] Initiating graceful shutdown sequence..." << std::endl;
        _sig_pipe.clearNotification();
        return (true);
    }
    throw std::runtime_error("Fatal: Core multiplexing system (poll) collapsed by internal error!");
}

void Cluster::_handleSignalEvent()
{
    std::cout << "\n[Signal Detected] Initiating graceful shutdown sequence..." << std::endl;
    _sig_pipe.clearNotification();
    _is_running = false;
}

void Cluster::_manage_cgi_lifecycle()
{
    if (_cgi_fd_map.empty()) return;

    typedef std::map<int, Connection*>::iterator CGI_Iterator;
    for (CGI_Iterator it = _cgi_fd_map.begin(); it != _cgi_fd_map.end();) {
        Connection* conn   = it->second;
        int         cgi_fd = it->first;

        if (conn && conn->isCGITimedOut()) {
            for (size_t i = 0; i < _poll_fds.size(); ++i) {
                if (_poll_fds[i].fd == cgi_fd) {
                    _poll_fds[i].fd     = -1;
                    _poll_fds[i].events = 0;
                    break;
                }
            }

            close(cgi_fd);
            conn->set_state(Connection::ERROR);
            _cgi_fd_map.erase(it++);
            conn->buildErrorResponse(504);
            this->update_client_events(conn->get_client_fd(), POLLOUT);

        } else {
            ++it;
        }
        conn->checkCGI();
    }
}

void Cluster::_check_timeouts(time_t now)
{
    std::map<int, Connection*>::iterator it;

    for (it = _connection_map.begin(); it != _connection_map.end(); ++it) {
        Connection* conn = it->second;
        if (!conn->is_waiting_request_msg()) continue;
        if (now - conn->get_last_recv_time() > REQUEST_TIMEOUT_LIMIT &&
            conn->get_last_recv_time() != 0) {
            conn->set_request_keep_alive(false);
            conn->buildErrorResponse(408);
            this->update_client_events(conn->get_client_fd(), POLLOUT);
        }
    }
}

void Cluster::_process_poll_errors(size_t index)
{
    int fd = _poll_fds[index].fd;

    if (this->_cgi_fd_map.count(fd) > 0) {
        std::cerr << "[Debug] Error on FD: " << fd;

        if (_socket_map.count(fd))
            std::cerr << " (Type: Listen Socket)";
        else if (_cgi_fd_map.count(fd))
            std::cerr << " (Type: CGI Pipe)";
        else
            std::cerr << " (Type: Client Socket)";

        if (_poll_fds[index].revents & POLLERR) std::cerr << " [POLLERR]";
        if (_poll_fds[index].revents & POLLNVAL) std::cerr << " [POLLNVAL]";

        // TODO: handle cgi error
    }

    // 普通客户端套接字连接出错，直接断开
    this->_poll_fds[index].fd = -1;
    this->close_connection(index);
}

void Cluster::_dispatch_read_event(size_t index)
{
    int fd = _poll_fds[index].fd;

    if (this->_socket_map.count(fd)) {
        this->handle_new_connection(fd, _socket_map[fd]);
        return;
    }
    if (this->_cgi_fd_map.count(fd)) {
        Connection* conn = this->_cgi_fd_map[fd];
        conn->handle_cgi_read();

        Connection::State current_state = conn->get_state();
        if (current_state == Connection::CGI_FINISH || current_state == Connection::WRITING_RESP) {
            this->_poll_fds[index].fd = -1;
            this->_cgi_fd_map.erase(fd);
            // TODO: 清理CGI中的输入输出fd 以及 waitpid
        }
        return;
    }

    if (this->handle_client_read_event(index) == false) this->close_connection(index);
}

void Cluster::_dispatch_write_event(size_t index)
{
    int fd = _poll_fds[index].fd;

    if (this->_cgi_fd_map.count(fd)) {
        Connection* conn = _cgi_fd_map[fd];

        conn->handle_cgi_write();
        if (conn->get_state() == Connection::CGI_FINISH || conn->get_state() == Connection::ERROR) {
            this->_poll_fds[index].fd = -1;
            this->_cgi_fd_map.erase(fd);
            // TODO: 这里务必确保 Connection 内部已经 close(cgi_write_fd)
            // 只有 close 了写端，cgi 才会意识到输入结束
        }
        return;
    }
    if (this->handle_client_write_event(index) == false) { this->close_connection(index); }
}

void Cluster::_dispatch_pollhup_event(size_t index)
{
    int fd = _poll_fds[index].fd;

    if (this->_cgi_fd_map.count(fd) > 0) {
        Connection* conn = _cgi_fd_map[fd];

        conn->handle_cgi_read();
        conn->finalize_cgi_success(fd);
        if (conn->get_state() == Connection::WRITING_RESP) {
            this->_cgi_fd_map.erase(fd);
            conn->clean_up_cgi_handler();
            std::cout << "[Server] CGI Pipe POLLHUP handled and removed successfully." << std::endl;
        }
        return;
    }

    this->_poll_fds[index].fd = -1;
    this->close_connection(index);
}

void Cluster::update_client_events(int client_fd, short new_events)
{
    bool found = false;

    for (size_t i = 0; i < this->_poll_fds.size(); ++i) {
        if (this->_poll_fds[i].fd == client_fd) {
            this->_poll_fds[i].events = new_events;
            found                     = true;
            std::cout << "[Cluster] Successfully set existing fd " << client_fd << " to POLLOUT"
                      << std::endl;
            break;
        }
    }

    if (!found) {
        struct pollfd pfd;
        pfd.fd      = client_fd;
        pfd.events  = new_events;
        pfd.revents = 0;

        this->_poll_fds.push_back(pfd);
        std::cout << "[Cluster] Client fd " << client_fd
                  << " was non-existent or wiped! Re-inserted to poll_fds." << std::endl;
    }
}

bool Cluster::is_invalid_fd(const struct pollfd& pfd)
{ return (pfd.fd == -1); }

void Cluster::cleanup_inactive_fds(void)
{
    std::vector<struct pollfd>::iterator it;

    it = std::remove_if(this->_poll_fds.begin(), this->_poll_fds.end(), is_invalid_fd);
    this->_poll_fds.erase(it, this->_poll_fds.end());
}

// print test check
void Cluster::print_socket_map(void) const
{
    std::map<int, PassiveSocket*>::const_iterator it;

    for (it = this->_socket_map.begin(); it != this->_socket_map.end(); it++) {
        std::cout << "listen fd: " << it->first << " Port No:" << it->second->getPort()
                  << " listen fd: " << it->second->getFd() << " Host: " << it->second->get_host()
                  << std::endl;
    }
}

void Cluster::print_pfds(void) const
{
    std::vector<struct pollfd>::const_iterator it;

    for (it = this->_poll_fds.begin(); it != this->_poll_fds.end(); it++) {
        if (it->events == POLLIN) std::cout << "pfd.events = POLLIN ";
        std::cout << "pfd.fd = " << it->fd << " pfd.revents = " << it->revents << std::endl;
    }
}

void Cluster::print_servers_map(void) const
{
    std::cout << "=========INIT SERVER MAP TEST ========" << std::endl;

    for (std::map<int, std::vector<ServerConfig*> >::const_iterator it = _servers_map.begin();
         it != _servers_map.end(); ++it) {
        std::cout << "Key = " << it->first << std::endl;
        const std::vector<ServerConfig*> _servers = it->second;
        for (std::size_t i = 0; i < _servers.size(); i++) {
            const std::vector<std::string> _names = it->second[i]->get_servers_name();
            for (std::size_t j = 0; j < _names.size(); j++) {
                std::cout << " ServerConfig server name index[" << j << "]" << _names[j]
                          << std::endl;
            }
        }
        std::cout << std::endl;
    }
    std::cout << "=========INIT SERVER MAP TEST ========" << std::endl;
}

void Cluster::register_cgi_fd(int fd, short events, Connection* conn)
{
    struct pollfd pfd;
    pfd.fd      = fd;
    pfd.events  = events;
    pfd.revents = 0;

    _poll_fds.push_back(pfd);
    _cgi_fd_map[fd] = conn;
}

const std::vector<struct pollfd>& Cluster::get_poll_fds() const
{ return (this->_poll_fds); }

void Cluster::unregister_cgi_fd(int fd)
{
    bool found = false;

    if (fd < 0) return;
    _cgi_fd_map.erase(fd);

    for (size_t i = 0; i < _poll_fds.size(); ++i) {
        if (_poll_fds[i].fd == fd) {
            _poll_fds[i].fd      = -1;
            _poll_fds[i].events  = 0;
            _poll_fds[i].revents = 0;
            found                = true;
            std::cout << "[Cluster] Marked fd to unregiste from poll_fds " << fd
                      << " as -1 for deferred cleanup." << std::endl;
            break;
        }
    }
    if (!found) return;
    close(fd);
}
