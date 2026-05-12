#include <set>
#include <iterator>
#include <algorithm>
#include "Cluster.hpp"

Cluster::Cluster(void) {}

Cluster::~Cluster(void)
{
    // TO DO LATER
    //  delete ptr inside _server_map _connection_map
}

void Cluster::setup(const ConfigParser& config)
{
    this->init_servers_map(config);
    this->open_listener(config);
    this->init_poll_listen_fds();

    std::cout << "set up finished!!!" << std::endl;
    this->print_socket_map();
    this->print_pfds();
}

void Cluster::open_listener(const ConfigParser& config)
{
    std::vector<ServerConfig*>::const_iterator vector_it;
    std::set<ServerConfig::ListenAddr>::const_iterator set_it;
    std::set<ServerConfig::ListenAddr> set_listen_addrs;

    for (vector_it = config.get_servers().begin(); vector_it != config.get_servers().end(); vector_it++)
    {
        std::vector<ServerConfig::ListenAddr>::const_iterator listen_it;

        if (*vector_it == NULL) 
            continue;
        for (listen_it = (*vector_it)->get_listen_addrs().begin(); listen_it != (*vector_it)->get_listen_addrs().end(); listen_it++)
        {
            set_listen_addrs.insert(*listen_it);
        }
    }

    for (set_it = set_listen_addrs.begin(); set_it != set_listen_addrs.end(); set_it++)
    {
        try
        {
            PassiveSocket* listener = new PassiveSocket(*set_it);
            this->_socket_map.insert(std::make_pair(listener->getFd(), listener));
        }
        catch (const std::exception& e) 
        {
            std::cerr << "Failed to open listener " << set_it->first << ":" << set_it->second << std::endl; //某个特定listen_addr Host:Port_Num bind 失败 不影响后续的操作
        }
    }
    if (this->_socket_map.empty())
        throw std::runtime_error("No listener could be opened, Webserv cannot start!");
}

void Cluster::init_servers_map(const ConfigParser& config)
{
    int port_num;

    for (std::size_t k = 0; k < config.get_servers().size(); k++)
    {
        const std::vector<ServerConfig::ListenAddr> &listen_addrs = config.get_servers()[k]->get_listen_addrs();
        for (std::size_t i = 0; i < listen_addrs.size(); i++)
        {
            port_num = listen_addrs[i].second;
            this->_servers_map[port_num].push_back(config.get_servers()[k]);
        }
    }
       std::cout << "=========INIT SERVER MAP TEST ========" << std::endl;

    for (std::map<int, std::vector<ServerConfig*> >::iterator it = _servers_map.begin(); 
        it != _servers_map.end(); ++it) 
    {
        std::cout << "Key = " << it->first << std::endl;
        const std::vector<ServerConfig*> _servers = it->second;
        for (std::size_t i = 0; i < _servers.size();i++)
        {
            const std::vector<std::string>  _names = it->second[i]->get_servers_name();
            for (std::size_t j = 0; j < _names.size(); j++)
            {
                std::cout << " ServerConfig server name index[" << j << "]" << _names[j] << std::endl;
            }
        }
        std::cout << std::endl;
    }
       std::cout << "=========INIT SERVER MAP TEST ========" << std::endl;
}

void Cluster::init_poll_listen_fds()
{
    std::map<int, PassiveSocket*>::const_iterator map_it;

    for (map_it = this->_socket_map.begin(); map_it != this->_socket_map.end() ; map_it++)
    {
        this->add_to_poll_fds(map_it->first);
    }
}

void Cluster::add_to_poll_fds(int fd)
{
    struct pollfd pfd;

    std::memset(&pfd, 0, sizeof(pfd));
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    this->_poll_fds.push_back(pfd);
}

void Cluster::handle_new_connection(int listen_fd, PassiveSocket* passive_socket)
{
    struct sockaddr_in client_addr;
    socklen_t          client_len = sizeof(client_addr);
    int                client_fd  = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
    std::map<int, std::vector<ServerConfig*> >::const_iterator it_servers;

    if (client_fd < 0) 
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ;
        std::cerr << "Accept failed: " << strerror(errno) << std::endl;
        return;
    }

    // 2. 设置非阻塞 (非常重要，否则后续 recv 会卡死)
    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    it_servers = this->_servers_map.find(passive_socket->getPort());

    if (it_servers != this->_servers_map.end())
    {
        // 3. 创建 Connection 对象并存入 map
        Connection* conn = new Connection(client_fd, passive_socket, it_servers->second);
        this->_connection_map.insert(std::make_pair(client_fd, conn));

        // 4. 将新的 FD 注册到 poll 监听列表中
        this->add_to_poll_fds(client_fd);
    }
    else //几乎不可能，但是作为防御编程，还是写了这个检查和报错
    {
        std::cerr << "Warning: No configurtion found for port " << passive_socket->getPort() << std::endl;
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
    this->_poll_fds[poll_idx].fd = -1;
    this->_poll_fds[poll_idx].revents = 0;
}

bool Cluster::handle_client_read_event(size_t poll_idx)
{
    int fd = _poll_fds[poll_idx].fd;

    // 1. 安全获取 Connection 指针
    std::map<int, Connection*>::iterator it = _connection_map.find(fd);
    if (it == _connection_map.end() || it->second == NULL) { return false; }
    Connection& conn = *(it->second);

    // 2. 读取数据 (非阻塞设计)
    char    buffer[4096];
    std::memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);

    if (bytes_read <= 0) 
    {
        // bytes_read == 0: 客户端关闭; < 0: 读取错误
        return false;
    }
    else
        conn.append_in_buff(buffer, bytes_read);

    // 3. 驱动解析器
    //TODO: replace buffer by _in_buff later
    if (conn.handle_data(buffer, bytes_read) == false) {
        // 解析遇到严重错误 (如 400 Bad Request)
        // 标记该连接已准备好发送错误响应
        //彻底去掉POLLIN,当4xx(400 408 411 413 414)发生，只发送错误响应，然后关闭该客户端连接

        conn.process_request_handler();
        conn.prepare_response();

        _poll_fds[poll_idx].events = POLLOUT;
        return true;
    } 

    // 4. 检查解析是否完成
    if (conn.check_parse_finished()) {
        std::cout << "[Server] Request parsed successfully. Preparing response..." << std::endl;
        //开启路由匹配
        conn.set_matched_server();
        conn.process_router_match();
        conn.process_request_handler();

        //     // 构建响应内容（根据 GET/POST 路径去找文件或跑 CGI）
       
        conn.prepare_response();

        // 核心切换：告诉 poll 我们现在想往这个 socket 写数据了
        _poll_fds[poll_idx].events = POLLOUT; //暗示POLLIN暂时关闭！
    }
    return (true);
}

bool Cluster::handle_client_write_event(size_t poll_idx)
{
   int fd = _poll_fds[poll_idx].fd;

    // 1. 安全获取 Connection 指针
    std::map<int, Connection*>::iterator it = _connection_map.find(fd);
    if (it == _connection_map.end() || it->second == NULL) { return false; }
    Connection& conn = *(it->second);
 
    send(fd, conn.get_out_buff().c_str(), conn.get_out_buff().size(), 0);

    //恢复POLLIN就绪事件，POLLOUT关闭,为下一次http request接收做准备
    _poll_fds[poll_idx].events = POLLIN;
    this->close_connection(poll_idx);
    this->_poll_fds[poll_idx].fd = -1;
    return true;
}

void Cluster::run()
{
    while (true) 
    {
        int ret = poll(_poll_fds.data(), _poll_fds.size(), -1);
        if (ret == -1)
        {
            //if CGI child process received a SIGINT, then it will not interrupt the parent process to continue running!
            if (errno == EINTR)
                continue;
            else
            {
                std::string error_msg = "Poll failed: ";
                error_msg += strerror(errno);
                throw std::runtime_error(error_msg);
            }
        }
        for (size_t i = 0; i < _poll_fds.size(); ++i) 
        {
            if (_poll_fds[i].fd == -1) continue ;
            // 先处理 POLLHUP (挂起) 或 POLLERR (错误), 出错直接跳过本次循环
            if (_poll_fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) 
            {
                this->_poll_fds[i].fd = -1;
                close_connection(i);
                continue ;
            }
            if (_poll_fds[i].revents & POLLIN) 
            {
                if (_socket_map.count(_poll_fds[i].fd))
                    handle_new_connection(_poll_fds[i].fd, _socket_map[_poll_fds[i].fd]); 
                else
                {
                    if (this->handle_client_read_event(i) == false)
                    {
                        this->_poll_fds[i].fd = -1;
                        close_connection(i);
                    }
                }
            }
            if (_poll_fds[i].fd != -1 && (_poll_fds[i].revents & POLLOUT))
            {
                this->handle_client_write_event(i);
            }
        }
        //延迟删除被close 的fd, 防止在for循环内误删，数组非法访问
        this->cleanup_inactive_fds();
    }
}

bool    Cluster::is_invalid_fd(const struct pollfd& pfd)
{
    return (pfd.fd == -1);
}

void    Cluster::cleanup_inactive_fds(void)
{
    std::vector<struct pollfd>::iterator it;

    it = std::remove_if(this->_poll_fds.begin(), this->_poll_fds.end(), is_invalid_fd);
    this->_poll_fds.erase(it, this->_poll_fds.end());
}

//print test check
void    Cluster::print_socket_map(void)const
{
    std::map<int, PassiveSocket *>::const_iterator it; 
   
    for (it = this->_socket_map.begin(); it != this->_socket_map.end(); it++)
    {
        std::cout << "listen fd: " << it->first << " Port No:" << it->second->getPort() << " listen fd: " << it->second->getFd() << " Host: " << it->second->get_host()<< std::endl;
    }
}

void    Cluster::print_pfds(void)const
{
    std::vector<struct pollfd>::const_iterator it;

    for (it = this->_poll_fds.begin(); it != this->_poll_fds.end(); it++)
    {
        if (it->events == POLLIN)
            std::cout << "pfd.events = POLLIN ";
        std::cout << "pfd.fd = " << it->fd << " pfd.revents = " << it->revents << std::endl;
    }
}