#include "Cluster.hpp"

Cluster::Cluster(void) {}

Cluster::~Cluster(void)
{
    // TO DO LATER
    //  delete ptr inside _server_map _connection_map
}

// void    Cluster::setup(const ConfigParse& config);
// temporary one, should be rewritten!
// init new_server hard_code instead of providing config input data
void Cluster::setup(void)
{
    Server* new_server = new Server(8080, "./www");
    new_server->setupListener();
    int listen_fd = new_server->getListenFd();

    this->_server_map[listen_fd] = new_server;

    struct pollfd pfd;
    pfd.fd      = listen_fd;
    pfd.events  = POLLIN;
    pfd.revents = 0;
    _poll_fds.push_back(pfd);
}

void Cluster::handle_new_connection(int listen_fd, Server* server)
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
    Connection* conn = new Connection(client_fd, server);
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
    int  fd = _poll_fds[poll_idx].fd;
    char buffer[4096];

    // 1. 读取请求
    ssize_t bytes_read = recv(fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        // 打印出来看看，确保收到了浏览器的 GET 请求
        std::cout << "Received: " << buffer << std::endl;

        // 2. 构造一个最基本的、符合 HTTP 规范的响应
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 18\r\n"
            "Connection: close\r\n"  // 明确告诉浏览器发完就断开
            "\r\n"
            "<h1>Hello 42!</h1>";

        // 3. 发送响应
        send(fd, response.c_str(), response.size(), 0);

        // 4. 关键：立即关闭连接或移除 poll 监听
        // 在你还没实现完整的 HTTP Keep-Alive 逻辑前，发完就 close
        this->close_connection(poll_idx);
    } else if (bytes_read == 0) {
        // 客户端主动关闭
        this->close_connection(poll_idx);
    }
    return true;
}

void Cluster::run()
{
    while (true) {
        int ret = poll(_poll_fds.data(), _poll_fds.size(), -1);
        if (ret < 0) continue;

        for (size_t i = 0; i < _poll_fds.size(); ++i) {
            if (_poll_fds[i].revents & POLLIN) {
                if (_server_map.count(_poll_fds[i].fd)) {
                    handle_new_connection(_poll_fds[i].fd, _server_map[_poll_fds[i].fd]);
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
