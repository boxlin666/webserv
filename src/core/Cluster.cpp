#include "Cluster.hpp"

#include <algorithm>
#include <iterator>
#include <set>

Cluster::Cluster(void)
{ _is_running = false; }

Cluster::~Cluster(void)
{
    // TODO: delete ptr inside _server_map _connection_map
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

    // TODO: erase this pollfd
    struct pollfd sig_pfd;
    sig_pfd.fd      = _sig_pipe.getReadFd();
    sig_pfd.events  = POLLIN;
    sig_pfd.revents = 0;
    _poll_fds.push_back(sig_pfd);
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

    std::cout << "client fd = " << client_fd << std::endl;
    // 2. 设置非阻塞 (非常重要，否则后续 recv 会卡死)
    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    it_servers = this->_servers_map.find(passive_socket->getPort());

    if (it_servers != this->_servers_map.end()) {
        // 3. 创建 Connection 对象并存入 map
        Connection* conn = new Connection(client_fd, passive_socket, it_servers->second, this);
        _connection_map[client_fd] = conn;
        // 4. 将新的 FD 注册到 /poll 监听列表中
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

    // 1. 安全获取 Connection 指针
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
    // 获取当前时间的辅助变量，用来控制心跳频率（避免每次循环都去查字典，消耗性能）
    time_t last_check_time = std::time(NULL);

    _is_running = true;
    while (_is_running) {
        // 🚨 修正 1：将 -1 改为 1000ms（1秒）。
        // 这样即使没有任何网络请求，poll 每隔 1 秒也会醒来一次，执行后面的超时检查
        int ret = poll(_poll_fds.data(), _poll_fds.size(), 1000);

        if (ret == -1) {
            if (errno == EINTR) continue;
            throw std::runtime_error("Poll failed: " + std::string(strerror(errno)));
        }

        // ==========================================
        // 阶段 1 & 2：处理 IO 事件 (如果 ret > 0)
        // ==========================================
        if (ret > 0) {
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

                if (_poll_fds[i].revents & POLLIN) { this->_dispatch_read_event(i); }

                if (_poll_fds[i].revents & POLLHUP) { this->_dispatch_pollhup_event(i); }

                if (_poll_fds[i].fd != -1 && (_poll_fds[i].revents & POLLOUT)) {
                    this->_dispatch_write_event(i);
                }
            }
        }

        // ==========================================
        // 阶段 3：状态心跳、收尸与收尾 (不管 poll 有没有事件，都会执行)
        // ==========================================
        this->cleanup_inactive_fds();  // 清理废弃 FD

        // 每隔 1 秒执行一次 CGI 的心跳检查，避免过于频繁
        time_t current_time = std::time(NULL);
        if (current_time - last_check_time >= 1) {
            _manage_cgi_lifecycle(); // 🌟 我们把超时和收尸逻辑封装在这里
            last_check_time = current_time;
        }
    }
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
            // 1. Invalidate FD in poll array
            for (size_t i = 0; i < _poll_fds.size(); ++i) {
                if (_poll_fds[i].fd == cgi_fd) {
                    _poll_fds[i].fd     = -1;
                    _poll_fds[i].events = 0;
                    break;
                }
            }

            // 2. Resource cleanup
            close(cgi_fd);
            conn->set_state(Connection::ERROR);
            _cgi_fd_map.erase(it++);
            conn->buildErrorResponse(504);
            this->update_client_events(conn->get_client_fd(), POLLOUT);
            // conn->setWriteReady();

        } else {
            ++it;
        }
        conn->checkCGI();
    }
}

/**
 * Only checkout POLLERR and POLLNVAL right now !
 */
void Cluster::_process_poll_errors(size_t index)
{
    int fd = _poll_fds[index].fd;

    if (this->_cgi_fd_map.count(fd) > 0) {
        Connection* conn = _cgi_fd_map[fd];

        (void)conn;

        std::cerr << "[Debug] Error on FD: " << fd;

        // 识别 FD 身份
        if (_socket_map.count(fd))
            std::cerr << " (Type: Listen Socket)";
        else if (_cgi_fd_map.count(fd))
            std::cerr << " (Type: CGI Pipe)";
        else
            std::cerr << " (Type: Client Socket)";

        // 识别具体错误位
        if (_poll_fds[index].revents & POLLERR)
            std::cerr << " [POLLERR]";  // 致命错误（如管道破裂）
        if (_poll_fds[index].revents & POLLNVAL)
            std::cerr << " [POLLNVAL]";  // 非法 FD（你可能关早了）

        // TODO: handle cgi error
    }

    // 普通客户端套接字连接出错，直接断开
    this->_poll_fds[index].fd = -1;
    this->close_connection(index);
}

void Cluster::_dispatch_read_event(size_t index)
{
    int fd = _poll_fds[index].fd;

    // =========================================================
    // 场景 A: 属于监听服务器 Socket
    // =========================================================
    if (this->_socket_map.count(fd)) {
        this->handle_new_connection(fd, _socket_map[fd]);
        return;
    }

    // =========================================================
    // 场景 B: 属于 CGI 读管道
    // =========================================================
    if (this->_cgi_fd_map.count(fd)) {
        Connection* conn = this->_cgi_fd_map[fd];

        conn->handle_cgi_read();

        // 🌟 状态核对：只要状态离开了 CGI_RUNNING (进入 FINISH 或直接变 ERROR/WRITING)
        // 就说明管道的使命已经结束，立刻销毁 FD 和映射
        Connection::State current_state = conn->get_state();
        if (current_state == Connection::CGI_FINISH || current_state == Connection::WRITING_RESP) {
            this->_poll_fds[index].fd = -1;  // 标记为废弃，等待 cleanup 统一清理
            this->_cgi_fd_map.erase(fd);     // 斩断关联，防止野指针
            // TODO: 清理CGI中的输入输出fd 以及 waitpid
        }
        return;
    }

    // =========================================================
    // 场景 C: 属于普通的客户端 Socket (浏览器发来了 HTTP 数据)
    // =========================================================
    // 既然不是 Server 也不是 CGI，那必定是普通的 Client 连接
    if (this->handle_client_read_event(index) == false) {
        // 如果返回 false，说明客户端断开了（如 recv 返回 0），或者发生严重错误
        // std::cout << fd << std::endl;
        this->close_connection(index);
    }
}

void Cluster::_dispatch_write_event(size_t index)
{
    int fd = _poll_fds[index].fd;

    // 情况 A: 属于 CGI 写管道
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
    // 情况 B: 属于普通的客户端 Socket
    if (this->handle_client_write_event(index) == false) { this->close_connection(index); }
}

void Cluster::_dispatch_pollhup_event(size_t index)
{
    int fd = _poll_fds[index].fd;

    if (this->_cgi_fd_map.count(fd) > 0) {
        Connection* conn = _cgi_fd_map[fd];

        // 识别 FD 身份
        if (_socket_map.count(fd))
            std::cerr << "[DEBUG] (Type: Listen Socket)";
        else if (_cgi_fd_map.count(fd))
            std::cerr << " [DEBUG] (Type: CGI Pipe) dispatch pollhup event";
        else
            std::cerr << " (Type: Client Socket)";

        if (_poll_fds[index].revents & POLLHUP)
            std::cerr << " [POLLHUP]" << std::endl;  // 对端关闭（常见于 CGI 结束）

        conn->handle_cgi_read();
        conn->finalize_cgi_success(fd);
        if (conn->get_state() == Connection::WRITING_RESP) {
            this->_cgi_fd_map.erase(fd);  // 释放映射
            conn->get_cgi_handler().reset();
            std::cout << "[Server] CGI Pipe POLLHUP handled and removed successfully." << std::endl;
        }
        return;
    }

    // 普通客户端套接字连接出错，直接断开
    this->_poll_fds[index].fd = -1;
    this->close_connection(index);
}

void Cluster::update_client_events(int client_fd, short new_events)
{
    bool found = false;

    // 1. 遍历现有的数组
    for (size_t i = 0; i < this->_poll_fds.size(); ++i) {
        // 🌟 防御性检查：如果发现某个位置的 FD 已经被误改成了 -1，
        // 但我们通过某种业务映射（比如你的 _connection_map）能对上号，或者这本来就是我们要找的坑位
        if (this->_poll_fds[i].fd == client_fd) {
            this->_poll_fds[i].events = new_events;  // 改成 POLLOUT
            found                     = true;
            std::cout << "[Cluster] Successfully set existing fd " << client_fd << " to POLLOUT"
                      << std::endl;
            break;
        }
    }

    // 🌟 2. 核心大招：如果真的被 remove_if 误伤导致彻底找不到了
    if (!found) {
        struct pollfd pfd;
        pfd.fd      = client_fd;   // 把它强行捞回来！
        pfd.events  = new_events;  // 设置为 POLLOUT
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
            _poll_fds[i].fd      = -1;  // 标记为 -1，内核下次就不看它了
            _poll_fds[i].events  = 0;   // 保险起见清空事件
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
