#include "Connection.hpp"

#include <poll.h>

#include "Router.hpp"
#include "Utils.hpp"

Connection::Connection(int client_fd, PassiveSocket* matched_socket,
                       const std::vector<ServerConfig*>& servers, Cluster* cluster)
    : _client_fd(client_fd),
      _in_buff(""),
      _out_buff(""),
      _matched_socket(matched_socket),
      _matched_server(NULL),
      _servers(servers),
      _cluster(cluster),
      _request(),
      _route_ctx(),
      _req_handler(),
      _response(),
      _status_code(200),
      _state(WAITING)
{
}

Connection::~Connection(void) {}

int Connection::getFd(void) const
{ return (this->_client_fd); }

const std::string& Connection::get_in_buff(void) const
{ return (this->_in_buff); }

const std::string& Connection::get_out_buff(void) const
{ return (this->_out_buff); }

void Connection::append_in_buff(const char* tmp_buff, ssize_t recv_len)
{
    if (!tmp_buff || recv_len <= 0) return;
    this->_in_buff.append(tmp_buff, static_cast<std::size_t>(recv_len));
}

// temporary function...
void Connection::set_out_buff(void)
{ this->_out_buff = "HTTP/1.1 200 OK\r\n\r\n<h1>Hello from Server Class!</h1>"; }

void Connection::handle_read_event(void) {
    char buffer[4096];
    ssize_t bytes_read = recv(this->_client_fd, buffer, sizeof(buffer), 0);

    if (bytes_read <= 0) {
        this->set_state(CLOSED);
        return;
    }

    buffer[bytes_read] = '\0';

    //tempo debug msg don't remove it now pls!
    std::string tmp_buff(buffer);
    debug_request_msg_print("BUFFER INFO", tmp_buff);
    //tempo debug msg don't remove it now pls!


    // 1. 数据必须累积到 Connection 的 _in_buff
    this->append_in_buff(buffer, bytes_read);

    // 2. 尝试解析 (此时只是尝试填充 HttpRequest 内部的数据结构)
    this->_status_code = this->_request.parse(this->_in_buff);

    // 3. 这里的关键：检查是否真的“请求完成”
    // 不要只依赖 _status_code，必须检查数据完整性
    if (this->check_parse_finished()) { 
        std::cout << "[Debug] Request is fully complete, body size: " 
                  << _request.get_body().length() << std::endl;
        this->handle_request_dispatch(); // 只有这时才处理
    } else {
        // 如果数据没齐，直接 return，保持 _in_buff 状态，等待下次 POLLIN
        std::cout << "[Debug] Waiting for more body data..." << std::endl;
    }
}

void Connection::handle_request_dispatch()
{
    std::cout << "[Server] Request parsed successfully. Preparing response..." << std::endl;

    this->set_matched_server();
    this->process_router_match();
    this->process_request_handler();

    if (_status_code != SUCCESS) {
        this->prepare_response();
        this->set_state(WRITING_RESP);
        return;
    }

    if (_route_ctx.is_cgi_potential) {
        // 职责分离：既然是 CGI，立刻移交给专门的 CGI 流水线去处理，这里光荣退场！
        this->execute_cgi_pipeline();
    } else {
        // 职责分离：普通静态文件（GET 个图片或 HTML），走普通的响应组装
        this->prepare_response();
        this->set_state(WRITING_RESP);
    }
}

void Connection::execute_cgi_pipeline()
{
    std::cout << "[Server] CGI detected. Initiating Child Process..." << std::endl;

    if (this->_cgi_handler.init(this->_request, this->_route_ctx) == false ||
        this->_cgi_handler.execute(this->_request) == false) {
        this->_status_code = SERVER_ERROR;  // 500
        this->prepare_response();
        this->_state = WRITING_RESP;
        return;
    }

    this->_cgi_active = true;

    if (this->_request.get_method() == "POST" && this->_request.get_body().length() > 0) {
        this->_state = CGI_WRITE;
        // 🌟 这一步非常重要！通知大管家开始监听向 Python 喂数据的写管道
        this->register_cgi_pipe_to_poll(this->_cgi_handler.getWriteFd(), POLLOUT);
    } else {
        this->_state = CGI_READ;
        // 🌟 通知大管家开始监听准备读取 Python 输出的读管道
        this->register_cgi_pipe_to_poll(this->_cgi_handler.getReadFd(), POLLIN);
    }
}

void Connection::handle_write_event(void)
{
    if (this->_out_buff.empty()) return;
    ssize_t bytes_send;

    // 🌟 打印 1：看看进这个函数时，缓冲区里有多少数据要发
    std::cout << "[Debug] handle_write_event called. Buff size to send: " << this->_out_buff.size()
              << std::endl;

    bytes_send = send(this->_client_fd, this->_out_buff.c_str(), this->_out_buff.size(), 0);

    if (bytes_send < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 只是暂时的无法写入，此时不应关闭连接，而是留在当前状态，等待下一次事件触发
            return; 
        }
        std::cerr << "Failed to send the http response on ..." << std::endl;
        this->_state = CLOSED;
        return;
    } else {
        // 🌟 打印 2：看看实际发了多少字节
        std::cout << "[Debug] send() actually sent: " << bytes_send << " bytes." << std::endl;
        this->_out_buff.erase(0, bytes_send);
    }

    if (this->_out_buff.empty()) {
        // 🌟 增加判定：如果响应报文里包含了 "Connection: close"，或者请求本身就不支持长连接
        if (this->_request.get_is_keep_alive() == false ||
            this->_response.get_full_response().find("Connection: close") != std::string::npos) {
            std::cout << "[Debug] Short connection detected. Switching to CLOSED." << std::endl;
            this->_state = CLOSED;
        } 
        else {
            std::cout << "[Debug] Long connection. Switching to WAITING." << std::endl;
	        this->_request.reset();
	        this->_response.reset();
            this->_state = WAITING;
        }
    }
}

bool Connection::check_parse_finished() 
{
    // 1. 基础状态校验
    if (this->_request.get_state() != HttpRequest::PARSE_FINISHED) {
        return false;
    }

    // 2. 契约校验 (Contract Validation)
    // 检查是否有 Content-Length 头部，如果有，确保 Body 已经读取完整
    // 这里处理 POST, PUT 等带有 payload 的请求
    if (this->_request.get_method() == "POST" || this->_request.get_method() == "PUT") {
        
        size_t expected_len = this->_request.get_content_length();
        size_t actual_len = this->_request.get_body().length();
        
        // 如果期望长度大于已读长度，说明数据还在路上，或者解析器过早进入了 FINISHED
        if (actual_len < expected_len) {
            // 这是一个重要的安全断言：如果还没读够，绝不能进入下一步
            // 此时应该保持状态或继续等待读取
            return false;
        }
    }

    // 3. (可选) Chunked 校验
    // 如果你已经支持了 Chunked，这里应该检查是否有结束符 "0\r\n\r\n"
    
    return true;
}

bool Connection::set_matched_server()
{
    // Get info from httpRequest
    std::map<std::string, std::string>::const_iterator req_header_it;
    std::string                                        _raw_data;

    req_header_it = this->_request.get_header_map().find("Host");
    if (req_header_it != this->_request.get_header_map().end())
        _raw_data = req_header_it->second;
    else
        return (false);  // failure on server match status_code should be 400
    std::size_t colon_pos = _raw_data.find(':');
    std::string req_host;  // Host: xxxx (in http request header)
    if (colon_pos == std::string::npos)
        req_host = _raw_data;
    else
        req_host = _raw_data.substr(0, std::string::npos);

    for (std::size_t i = 0; i < this->_servers.size(); i++) {
        for (std::size_t j = 0; j < this->_servers[i]->get_servers_name().size(); j++) {
            if (req_host == this->_servers[i]->get_servers_name()[j]) {
                this->_matched_server = this->_servers[i];
                std::cout << "selected server name is : "
                          << this->_servers[i]->get_servers_name()[j] << std::endl;
                return (true);
            }
        }
    }
    this->_matched_server = this->_servers[0];  // if no matched server, then select the 1st one
                                                // associted with this port number by default!
    std::cout << "selected default server name is : " << this->_servers[0]->get_servers_name()[0]
              << std::endl;
    return (true);
}

void Connection::process_router_match()
{
    if (this->_status_code != SUCCESS) return;
    if (this->_matched_server)
        this->_route_ctx =
            Router::build_router_context(this->_request, *(this->_matched_server), _status_code);
}

void Connection::process_request_handler()
{
    if (this->_status_code != SUCCESS) return;

    this->_req_handler.process_request_handler(this->_request, this->_route_ctx, _status_code);

    if (_status_code != SUCCESS) {
        this->prepare_response();
        this->_state = WRITING_RESP;
        return;
    }
}

void Connection::prepare_response()
{
    this->_response.build(this->_request, this->_req_handler, this->_status_code);
    this->_out_buff = this->_response.get_full_response();

    //tempo print debug, don't remove it now!
    debug_request_msg_print("OUT BUFF", this->_out_buff);
}

void Connection::set_state(State state)
{ this->_state = state; }

Connection::State Connection::get_state(void) const
{ return (this->_state); }

short Connection::get_poll_events() const
{
    // 如果连接处于等待新请求、或者正在读取请求的状态，我们需要监听读（POLLIN）
    if (this->_state == WAITING || this->_state == READING_REQ) return POLLIN;

    // 如果连接处于正在向客户端写响应、或者正在往 CGI 喂数据的状态，我们需要监听写（POLLOUT）
    if (this->_state == WRITING_RESP || this->_state == CGI_WRITE) return POLLOUT;

    // 其他状态（比如 CLOSED 或者正在等待 CGI 读管道），让客户端 Socket 保持不监听任何读写事件
    return 0;
}

bool Connection::has_cgi()
{ return (this->_cgi_handler.getPid() != -1 && !this->_cgi_handler.isFinished()); }

int Connection::get_cgi_read_fd() const
{ return this->_cgi_handler.getReadFd(); }

void Connection::handle_cgi_read()
{
    char buf[4096];
    int  cgi_fd = this->get_cgi_read_fd();

    ssize_t bytes = read(cgi_fd, buf, sizeof(buf) - 1);

    if (bytes > 0) {
        buf[bytes] = '\0';
        this->_out_buff.append(buf, bytes);
    } else if (bytes == 0) {
        // 🌟 职责分离：一脚踢给成功处理函数
        this->_finalize_cgi_success(cgi_fd);
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // 🌟 职责分离：一脚踢给错误处理函数
            this->_handle_cgi_read_error(cgi_fd);
        }
    }
}

void Connection::handle_cgi_write()
{
    _cgi_handler.sendToScript();
}

void Connection::_finalize_cgi_success(int cgi_fd)
{
    std::cout << "[Server] CGI Process finished writing all data." << std::endl;

    // 1. 专业组装 HTTP 报文
    this->_response.build_from_cgi(this->_out_buff);
    this->_out_buff = this->_response.get_full_response();

    // 2. 子进程清理（收尸防止僵尸进程）
    int status;
    waitpid(this->_cgi_handler.getPid(), &status, WNOHANG);

    // 3. 核心解耦：先安全从 poll 中撤销，再关闭管道，防止 FD 复用串流
    this->_cluster->remove_fd_from_poll(cgi_fd);
    this->_cgi_handler.close_all_pipes();
    this->_cgi_active = false;

    // 4. 驱动状态机进入发送响应阶段
    this->_state = WRITING_RESP;
    this->_cluster->update_client_events(this->_client_fd, POLLOUT);

    std::cout << "[Server] Switched client socket to POLLOUT." << std::endl;
}

void Connection::_handle_cgi_read_error(int cgi_fd)
{
    std::cerr << "[Server Error] CGI read error, errno = " << errno << std::endl;

    // 1. 组装标准的 500 错误响应体
    this->_status_code = SERVER_ERROR;
    this->prepare_response();
    this->_out_buff = this->_response.get_full_response();

    // 2. 清理 poll 队列及管道资源
    this->_cluster->remove_fd_from_poll(cgi_fd);
    this->_cgi_handler.close_all_pipes();
    this->_cgi_active = false;

    // 3. 哪怕出错了，也要把 500 页面传回给客户端
    this->_state = WRITING_RESP;
    this->_cluster->update_client_events(this->_client_fd, POLLOUT);
}

void Connection::register_cgi_pipe_to_poll(int fd, short events)
{ this->_cluster->register_cgi_fd(fd, events, this); }

bool Connection::isCGITimedOut() const {
    return _cgi_handler.isTimeout();
}

void Connection::updateCGIActivity() {
    _cgi_handler.updateTime();
}