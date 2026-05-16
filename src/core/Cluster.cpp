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
        Connection* conn           = new Connection(client_fd, passive_socket, it_servers->second, this);
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
            std::string error_msg = "Poll failed: " + std::string(strerror(errno));
            throw std::runtime_error(error_msg);
        }
        for (size_t i = 0; i < _poll_fds.size(); ++i) {
            if (_poll_fds[i].fd == -1) continue;

            // 1. 处理异常/断开
            if (_poll_fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                this->_process_poll_errors(i);
                continue; 
            }
            // 2. 分发读事件
            if (_poll_fds[i].revents & POLLIN) {
                this->_dispatch_read_event(i);
            }
            // 3. 分发写事件
            if (_poll_fds[i].fd != -1 && (_poll_fds[i].revents & POLLOUT)) {
                this->_dispatch_write_event(i);
            }
        }
        // 延迟删除被close 的fd, 防止在for循环内误删，数组非法访问
        this->cleanup_inactive_fds();
    }
}

void Cluster::_process_poll_errors(size_t index)
{
    int fd = _poll_fds[index].fd;

    // 🌟 绝杀死循环的核心拦截点：
    // 如果触发 POLLHUP 的是 CGI 管道 FD
    if (this->_cgi_fd_map.count(fd) > 0) 
    {
        Connection* conn = _cgi_fd_map[fd];
        
        // 关键动作：虽然子进程挂断了，但管道缓冲区里可能还有 Python 没读完的残留数据！
        // 我们强制驱动一次读取！
        conn->handle_cgi_read();
        
        // 读取完之后，检查 Connection 状态。
        // 你的 handle_cgi_read 会在读到 0 (EOF) 时把状态切走（比如切到 WRITING_RESP）
        if (conn->get_state() != Connection::CGI_READ) {
            this->_poll_fds[index].fd = -1;  // 标记延迟清理，踢出 poll
            this->_cgi_fd_map.erase(fd);      // 释放映射
            std::cout << "[Server] CGI Pipe POLLHUP handled and removed successfully." << std::endl;
        }
        return; 
    }
    
    // 普通客户端套接字连接出错，直接断开
    this->_poll_fds[index].fd = -1;
    this->close_connection(index);
}

void Cluster::_dispatch_read_event(size_t index)
{
    int fd = _poll_fds[index].fd;

    // 情况 A: 属于 CGI 读管道
    if (this->_cgi_fd_map.count(fd)) {
        Connection* conn = _cgi_fd_map[fd];
        
        // 🌟 核心替换：调用你现有的 handle_cgi_read() 
        conn->handle_cgi_read();
        
        // 🌟 完美的闭环判定：看它的状态是不是已经离开 CGI_READ 了
        // 如果它的状态变成了 WRITING_RESP，说明它在 handle_cgi_read 内部已经处理完了 EOF
        if (conn->get_state() != Connection::CGI_READ) {
            this->_poll_fds[index].fd = -1;  // 标记延迟清理
            this->_cgi_fd_map.erase(fd);      // 释放映射
        }
    }
    // 情况 B: 属于监听服务器 Socket
    else if (_socket_map.count(fd)) {
        this->handle_new_connection(fd, _socket_map[fd]);
    }
    // 情况 C: 属于普通的客户端 Socket
    else {
        if (this->handle_client_read_event(index) == false) {
            this->_poll_fds[index].fd = -1;
            this->close_connection(index);
        }
    }
}

void Cluster::_dispatch_write_event(size_t index)
{
    int fd = _poll_fds[index].fd;

    // 情况 A: 属于 CGI 写管道
    if (this->_cgi_fd_map.count(fd)) {
        // Connection* conn = _cgi_fd_map[fd];
        
        // 🌟 坑位留空/或调用你的写处理（比如未来你可以加一个 conn->handle_cgi_write()）
        // 如果目前只跑 GET，它绝对不会进到这里来
        // 暂时可以先写成如果写完了就断开：
        /*
        conn->handle_cgi_write(); 
        if (conn->get_state() != Connection::CGI_WRITE) {
            this->_poll_fds[index].fd = -1;
            this->_cgi_fd_map.erase(fd);
        }
        */
    }
    // 情况 B: 属于普通的客户端 Socket
    else {
        this->handle_client_write_event(index);
    }
}

void Cluster::update_client_events(int client_fd, short new_events)
{
    bool found = false;

    // 1. 遍历现有的数组
    for (size_t i = 0; i < this->_poll_fds.size(); ++i) 
    {
        // 🌟 防御性检查：如果发现某个位置的 FD 已经被误改成了 -1，
        // 但我们通过某种业务映射（比如你的 _connection_map）能对上号，或者这本来就是我们要找的坑位
        if (this->_poll_fds[i].fd == client_fd) 
        {
            this->_poll_fds[i].events = new_events; // 改成 POLLOUT
            found = true;
            std::cout << "[Cluster] Successfully set existing fd " << client_fd << " to POLLOUT" << std::endl;
            break;
        }
    }

    // 🌟 2. 核心大招：如果真的被 remove_if 误伤导致彻底找不到了
    if (!found) 
    {
        struct pollfd pfd;
        pfd.fd = client_fd;       // 把它强行捞回来！
        pfd.events = new_events;   // 设置为 POLLOUT
        pfd.revents = 0;
        
        this->_poll_fds.push_back(pfd);
        std::cout << "[Cluster] Client fd " << client_fd << " was non-existent or wiped! Re-inserted to poll_fds." << std::endl;
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
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
        
    _poll_fds.push_back(pfd);
    _cgi_fd_map[fd] = conn;
}

void Cluster::remove_fd_from_poll(int fd)
{
    if (fd < 0) return;

    for (size_t i = 0; i < this->_poll_fds.size(); ++i) 
    {
        if (this->_poll_fds[i].fd == fd) 
        {
            this->_poll_fds[i].fd = -1;
            this->_poll_fds[i].events = 0;
            this->_poll_fds[i].revents = 0;
            
            std::cout << "[Cluster] Marked fd " << fd << " as -1 for deferred cleanup." << std::endl;
            break; // 找到了就功成身退
        }
    }
}