#include "Cluster.hpp"

#include <algorithm>
#include <iterator>
#include <set>

Cluster::Cluster(void) {}

Cluster::~Cluster(void)
{
    // TODO LATER
    //  delete ptr inside _server_map _connection_map
}

void Cluster::setup(const ConfigParser& config)
{
    this->init_servers_map(config);
    this->open_listener(config);
    this->init_poll_listen_fds();

    std::cout << "set up finished!!!" << std::endl;
    this->print_socket_map();
    this->print_servers_map();
    this->print_pfds();
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
                      << std::endl;  // 某个特定listen_addr Host:Port_Num bind 失败 不影响后续的操作
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
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        std::cerr << "Accept failed: " << strerror(errno) << std::endl;
        return;
    }

    // 2. 设置非阻塞 (非常重要，否则后续 recv 会卡死)
    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    it_servers = this->_servers_map.find(passive_socket->getPort());

    if (it_servers != this->_servers_map.end()) {
        // 3. 创建 Connection 对象并存入 map
        Connection* conn           = new Connection(client_fd, passive_socket, it_servers->second);
        _connection_map[client_fd] = conn;
        // 4. 将新的 FD 注册到 poll 监听列表中
        this->add_to_poll_fds(client_fd);
    } else  // 几乎不可能，但是作为防御编程，还是写了这个检查和报错
    {
        std::cerr << "Warning: No configurtion found for port " << passive_socket->getPort()
                  << std::endl;
        close(client_fd);
    }
}

/* * Cluster::close_connection
 * 彻底清理并关闭一个客户端连接
 * @param poll_idx: 该连接在 _poll_fds 向量中的下标
 */
void Cluster::close_connection(size_t poll_idx)
{
    int fd = _poll_fds[poll_idx].fd;

    // 1. 释放 Connection 对象的内存并从 map 中移除
    std::map<int, Connection*>::iterator it = _connection_map.find(fd);
    if (it != _connection_map.end()) {
        delete it->second;          // 释放 Connection 对象
        _connection_map.erase(it);  // 从 map 中移除
    }

    // 2. 最后关闭文件描述符
    close(fd);

    // 3. 标记容器位置 (不删除，只标记)
    this->_poll_fds[poll_idx].fd      = -1;
    this->_poll_fds[poll_idx].revents = 0;
}

bool Cluster::handle_client_read_event(size_t poll_idx)
{
    int fd = _poll_fds[poll_idx].fd;

    std::map<int, Connection*>::iterator it = _connection_map.find(fd);
    if (it == _connection_map.end() || it->second == NULL) { return false; }
    Connection& conn = *(it->second);

    if (conn.has_cgi() && fd == conn.get_cgi_read_fd()) 
    {
        conn.handle_cgi_read(); // 内部调用 cgi.receiveFromScript()
    }
    else 
    {
        // 2. 否则，它就是普通的 client socket 读取
        conn.handle_read_event();
    }

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

    // 1. 安全获取 Connection 指针
    std::map<int, Connection*>::iterator it = _connection_map.find(fd);
    if (it == _connection_map.end() || it->second == NULL) { return false; }
    Connection& conn = *(it->second);

    conn.handle_write_event();

    short poll_event = conn.get_poll_events();

    if (poll_event == POLLIN)  // WAITING => keep-alive mode
        _poll_fds[poll_idx].events = POLLIN;
    else if (poll_event == POLLOUT)  // WRITING => still writing (send http response msg)
        _poll_fds[poll_idx].events = POLLOUT;
    else {
        this->close_connection(poll_idx);
        this->_poll_fds[poll_idx].fd = -1;
    }
    if (poll_event == POLLIN) std::cout << "Here we have a POLLIN there!" << std::endl;
    return true;
}

void Cluster::run()
{
    while (true) {
        int ret = poll(_poll_fds.data(), _poll_fds.size(), -1);
        if (ret == -1) {
            // if CGI child process received a SIGINT, then it will not interrupt the parent process
            // to continue running!
            if (errno == EINTR)
                continue;
            else {
                std::string error_msg = "Poll failed: ";
                error_msg += strerror(errno);
                throw std::runtime_error(error_msg);
            }
        }
        for (size_t i = 0; i < _poll_fds.size(); ++i) {
            if (_poll_fds[i].fd == -1) continue;
            // 先处理 POLLHUP (挂起) 或 POLLERR (错误), 出错直接跳过本次循环
            if (_poll_fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                this->_poll_fds[i].fd = -1;
                close_connection(i);
                continue;
            }
            if (_poll_fds[i].revents & POLLIN) {
                if (_socket_map.count(_poll_fds[i].fd))
                    handle_new_connection(_poll_fds[i].fd, _socket_map[_poll_fds[i].fd]);
                else {
                    if (this->handle_client_read_event(i) == false) {
                        this->_poll_fds[i].fd = -1;
                        close_connection(i);
                    }
                }
            }
            if (_poll_fds[i].fd != -1 && (_poll_fds[i].revents & POLLOUT)) {
                this->handle_client_write_event(i);
            }
        }
        // 延迟删除被close 的fd, 防止在for循环内误删，数组非法访问
        this->cleanup_inactive_fds();
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