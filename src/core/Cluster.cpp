#include <set>
#include <iterator>
#include "Cluster.hpp"

Cluster::Cluster(void) {}

Cluster::~Cluster(void)
{
    // TO DO LATER
    //  delete ptr inside _server_map _connection_map
}

void Cluster::setup(const ConfigParser& config)
{
    this->open_listener(config);
    this->init_poll_fds();

    std::cout << "set up finished!!!" << std::endl;
}

void Cluster::open_listener(const ConfigParser& config)
{
    std::set<int> ports;
    std::vector<ServerConfig*>::const_iterator vector_it;
    std::set<int>::const_iterator set_it;

    for (vector_it = config.get_servers().begin(); vector_it != config.get_servers().end(); vector_it++)
    {
        std::vector<int> listen_fds; 
        std::vector<ServerConfig::ListenAddr>::const_iterator listen_it;

        for (listen_it = (*vector_it)->get_listen_addrs().begin(); listen_it != (*vector_it)->get_listen_addrs().end(); listen_it++)
        {
            listen_fds.push_back(listen_it->second);
        }
        ports.insert(listen_fds.begin(), listen_fds.end());
    }
    if (ports.empty())
        throw std::runtime_error("No ports found in configuration file!");
    for (set_it = ports.begin(); set_it != ports.end(); set_it++)
    {
        PassiveSocket* listener = new PassiveSocket(*set_it);
        this->_socket_map.insert(std::make_pair(listener->getFd(), listener));
    }
    //test for setup
    print_socket_map();
}

void Cluster::init_poll_fds()
{
    std::map<int, PassiveSocket*>::const_iterator map_it;

    for (map_it = this->_socket_map.begin(); map_it != this->_socket_map.end() ; map_it++)
    {
        struct pollfd pfd;
        pfd.fd = map_it->first;
        pfd.events = POLLIN;
        pfd.revents = 0;
        this->_poll_fds.push_back(pfd);
    }
}

void Cluster::handle_new_connection(int listen_fd, PassiveSocket* passive_socket)
{
    struct sockaddr_in client_addr;
    socklen_t          client_len = sizeof(client_addr);
    int                client_fd  = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        // 在非阻塞模式下，需要忽略 EAGAIN
        return;
    }

    // 2. 设置非阻塞 (非常重要，否则后续 recv 会卡死)
    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    // 3. 创建 Connection 对象并存入 map
    Connection* conn = new Connection(client_fd, passive_socket);
    this->_connection_map.insert(std::make_pair(client_fd, conn));

    // 4. 将新的 FD 注册到 poll 监听列表中
    struct pollfd pfd;
    pfd.fd      = client_fd;
    pfd.events  = POLLIN;  // 监听读事件
    pfd.revents = 0;
    this->_poll_fds.push_back(pfd);
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

    // 2. 从 pollfd 向量中移除（这是防止轮询到无效 FD 的关键）
    // 使用 erase 移除当前位置的元素
    _poll_fds.erase(_poll_fds.begin() + poll_idx);

    // 3. 最后关闭文件描述符
    close(fd);

    // 注意：由于在 run() 的循环中调用了 erase，
    // 调用完此函数后，外部循环的索引 i 必须进行相应调整（如 i--）
}

bool Cluster::handle_client_data(size_t poll_idx)
{
    int fd = _poll_fds[poll_idx].fd;
    std::string response;

    // 1. 安全获取 Connection 指针
    std::map<int, Connection*>::iterator it = _connection_map.find(fd);
    if (it == _connection_map.end() || it->second == NULL) { return false; }
    Connection& conn = *(it->second);

    // 2. 读取数据 (非阻塞设计)
    char    buffer[4096];
    ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);

    if (bytes_read <= 0) {
        // bytes_read == 0: 客户端关闭; < 0: 读取错误
        return false;
    }

    // 3. 驱动解析器
    if (conn.handle_data(buffer, bytes_read) == false) {
        // 解析遇到严重错误 (如 400 Bad Request)
        // 标记该连接已准备好发送错误响应
        _poll_fds[poll_idx].events |= POLLOUT;
        return true;
    }

    // 4. 检查解析是否完成
    if (conn.check_parse_finished()) {
        std::cout << "[Server] Request parsed successfully. Preparing response..." << std::endl;
        //开启路由匹配
        conn.process_router_match();

        //     // 构建响应内容（根据 GET/POST 路径去找文件或跑 CGI）

        response = conn.prepare_response();
        // 核心切换：告诉 poll 我们现在想往这个 socket 写数据了
        _poll_fds[poll_idx].events |= POLLOUT;

        // 习惯性清理：既然请求解析完了，可以把该 FD 的 POLLIN 暂时关掉（可选）
        // _poll_fds[poll_idx].events &= ~POLLIN;
    }
    // 临时
    /*std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 18\r\n"
        "Connection: close\r\n"  // 明确告诉浏览器发完就断开
        "\r\n"
        "<h1>Hello 42!</h1>";*/

    //std::string response = conn.prepare_response();


    send(fd, response.c_str(), response.size(), 0);
    this->close_connection(poll_idx);
    return true;
}

void Cluster::run()
{
    while (true) {
        int ret = poll(_poll_fds.data(), _poll_fds.size(), -1);
        if (ret < 0) continue;

        for (size_t i = 0; i < _poll_fds.size(); ++i) {
            if (_poll_fds[i].revents & POLLIN) {
                if (_socket_map.count(_poll_fds[i].fd)) {
                    handle_new_connection(_poll_fds[i].fd, _socket_map[_poll_fds[i].fd]);
                } else {
                    // 处理数据，如果返回 false 表示需要关闭连接
                    if (handle_client_data(i) == false) {
                        close_connection(i);
                        i--;  // 核心：抵消 erase 带来的索引偏移
                    }
                }
            }
            // 还可以处理 POLLHUP (挂起) 或 POLLERR (错误)
            else if (_poll_fds[i].revents & (POLLHUP | POLLERR)) {
                close_connection(i);
                i--;
            }
        }
    }
}

//print test check
void    Cluster::print_socket_map(void)const
{
    std::map<int, PassiveSocket *>::const_iterator it; 
   
    for (it = this->_socket_map.begin(); it != this->_socket_map.end(); it++)
    {
        std::cout << "listen fd: " << it->first << " Port No:" << it->second->getPort() << " listen fd: " << it->second->getFd() << std::endl;
    }
}