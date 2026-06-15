#include "Connection.hpp"

#include <poll.h>

#include "Router.hpp"
#include "Utils.hpp"

Connection::Connection(int client_fd, PassiveSocket* matched_socket,
                       const std::vector<ServerConfig*>& servers,
                       IClusterMediator*                 cluster_mediator)
    : _client_fd(client_fd),
      _in_buff(""),
      _back_up_in_buff(""),
      _out_buff(""),
      _matched_socket(matched_socket),
      _matched_server(NULL),
      _servers(servers),
      _cluster_mediator(cluster_mediator),
      _request(),
      _route_ctx(),
      _req_handler(),
      _response(),
      _status_code(200),
      _state(WAITING)
{
}

Connection::~Connection(void) {}

const std::string& Connection::get_in_buff(void) const
{ return (this->_in_buff); }

const std::string& Connection::get_out_buff(void) const
{ return (this->_out_buff); }

void Connection::handle_read_event(void)
{
    if (this->_state == Connection::CGI_RUNNING) return;

    char    buffer[4096];
    ssize_t bytes_read = recv(this->_client_fd, buffer, sizeof(buffer), 0);

    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        this->set_state(CLOSED);
        return;
    } else if (bytes_read == 0) {
        this->set_state(CLOSED);
        return;
    } else {
        buffer[bytes_read] = '\0';
        std::string tmp_buff(buffer);

        // 1. 数据必须累积到 Connection 的 _in_buff
        _in_buff.append(buffer, static_cast<std::size_t>(bytes_read));

        this->process_existing_in_buff();
    }
}

void Connection::process_existing_in_buff()
{
    // 2. 尝试解析 (此时只是尝试填充 HttpRequest 内部的数据结构)
    this->_status_code = this->_request.parse(this->_in_buff);

    if (this->_status_code != SUCCESS) {
        this->handle_request_dispatch();
        return;
    }

    // 3. 这里的关键：检查是否真的“请求完成”
    // 不要只依赖 _status_code，必须检查数据完整性
    if (this->check_parse_finished()) 
    {
        std::cout << "[Debug] Request is fully complete, body size: "
                  << _request.get_body().length() << std::endl;
        this->_request.parse_multipart_body();
        this->handle_request_dispatch();  // 只有这时才处理
    } 
    else
    {
        // 如果数据没齐，直接 return，保持 _in_buff 状态，等待下次 POLLIN
        std::cout << "[Debug] Waiting for more body data..." << std::endl;
    }
}

void Connection::handle_request_dispatch()
{
    std::cout << "[Server] Request parsed successfully. Preparing response..." << std::endl;
    std::cout << "[Check] Entering CGI pipeline..." << std::endl;

    try {
        this->set_matched_server();
        this->process_router_match();

        if (_status_code == SUCCESS) {
            this->_req_handler.process_request_handler(this->_request, this->_route_ctx,
                                                       _status_code);
        }

        if (_status_code != SUCCESS)
            buildErrorResponse(_status_code);
        else if (_route_ctx.is_cgi_potential) {
            this->execute_cgi_pipeline();
            return;  // CGI 流程接管了 FD，这里直接退出
        } else {
            // 职责分离：普通静态文件（GET 个图片或 HTML），走普通的响应组装
            this->prepare_static_response();
            this->set_state(WRITING_RESP);
        }
    } catch (const std::exception& e) {
        std::cerr << "CATCHED FATAL ERROR: " << e.what() << std::endl;
        // 如果这里打印了，那就实锤了是 C++ 的异常导致的崩溃
        return;
    }
}

void Connection::execute_cgi_pipeline()
{
    std::cout << "[Server] CGI detected. Initiating Child Process..." << std::endl;

    if (this->_cgi_handler.init(this->_request, this->_route_ctx) == false ||
        this->_cgi_handler.execute(this->_request) == false) {
        // TODO: generate error page(500)
        return;
    }

    this->_state = CGI_RUNNING;

    int read_fd  = _cgi_handler.getReadFd();
    int write_fd = _cgi_handler.getWriteFd();

    if (read_fd != -1) {  // 统一注册接口
        this->_cluster_mediator->register_cgi_fd(read_fd, POLLIN, this);
    }

    if (write_fd != -1) {
        // 如果是 POST 且有 Body，CGIHandler 会保留这个写端，这里才注册
        this->_cluster_mediator->register_cgi_fd(write_fd, POLLOUT, this);
    } else {
        // 如果没有写端（如 GET），CGIHandler 内部已经关了，Connection 根本不需要操心
        std::cout << "[Debug] CGI write pipe not needed or closed." << std::endl;
    }
}

void Connection::handle_write_event(void)
{
    if (this->_state == Connection::CGI_RUNNING) return;

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
        // std::cout << "_out_buff = " << _out_buff << std::endl;
        this->_out_buff.erase(0, bytes_send);
    }

    if (this->_out_buff.empty()) {
        // 🌟 增加判定：如果响应报文里包含了 "Connection: close"，或者请求本身就不支持长连接
        if ((this->_request.get_is_keep_alive() == false ||
             this->_response.get_full_response().find("Connection: close") != std::string::npos) &&
            this->_in_buff.empty()) {
            std::cout << "[Debug] Short connection detected. Switching to CLOSED." << std::endl;
            this->_state = CLOSED;
        } else {
            this->_request.reset();
            this->_response.reset();
            this->_state = WAITING;

            if (!this->_in_buff.empty()) {
                std::cout << "[Pipeline] Remaining data detected in _in_buff ("
                          << this->_in_buff.size() << " bytes). Driving next request inline."
                          << std::endl;
                this->process_existing_in_buff();
            } else
                std::cout << "[Debug] Long connection. Switching to WAITING." << std::endl;
        }
    }
}

bool Connection::check_parse_finished()
{
    // 1. 基础状态校验
    if (this->_request.get_state() != HttpRequest::PARSE_FINISHED) { return false; }

    // 2. 契约校验 (Contract Validation)
    // 检查是否有 Content-Length 头部，如果有，确保 Body 已经读取完整
    // 这里处理 POST, 等带有 payload 的请求
    if (this->_request.get_method() == "POST" && !this->_request.get_is_chunked()) {
        size_t expected_len = this->_request.get_content_length();
        size_t actual_len   = this->_request.get_body().length();

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

    req_header_it = this->_request.get_header_map().find("host");
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

void Connection::buildErrorResponse(int status_code)
{
    _status_code                = status_code;
    std::string error_page_path = "";
    if (_route_ctx.server != NULL)
        error_page_path = Router::get_error_page_path(_route_ctx, _matched_server, status_code);
    this->_out_buff = _response.build_error_response(status_code, error_page_path, _request);
    this->_state    = Connection::WRITING_RESP;
}

void Connection::prepare_static_response()
{
    this->_response.build_static_response(this->_request, this->_req_handler, this->_status_code);
    this->_out_buff = this->_response.get_full_response();
}

void Connection::set_state(State state)
{ this->_state = state; }

Connection::State Connection::get_state(void) const
{ return (this->_state); }

short Connection::get_poll_events() const
{
    // 如果连接处于等待新请求、或者正在读取请求的状态，我们需要监听读（POLLIN）
    if (this->_state == WAITING || this->_state == READING_REQ || this->_state == CGI_RUNNING)
        return POLLIN;

    // 如果连接处于正在向客户端写响应、或者正在往 CGI 喂数据的状态，我们需要监听写（POLLOUT）
    if (this->_state == WRITING_RESP) return POLLOUT;

    // 其他状态（比如 CLOSED 或者正在等待 CGI 读管道），让客户端 Socket 保持不监听任何读写事件
    return 0;
}

void Connection::handle_cgi_write()
{
    // 获取当前正在写的 FD（写完前它是有效的）
    int current_pipe_fd = _cgi_handler.getWriteFd();

    // 只有在 FD 有效时才干活
    if (current_pipe_fd == -1) return;

    int status = _cgi_handler.sendToScript();

    // 🌟 如果返回 0 (写完关了) 或 -1 (出错了)，通知 Cluster 别再 poll 它了
    if (status <= 0) {
        this->_cluster_mediator->unregister_cgi_fd(current_pipe_fd);
        std::cout << "[CGI] Input pipe finished and unregistered." << std::endl;
    }
}

void Connection::handle_cgi_read()
{
    int current_pipe_fd = _cgi_handler.getReadFd();

    if (current_pipe_fd == -1) return;

    int status = _cgi_handler.receiveFromScript();

    if (status == 1) {
        // 数据还在源源不断地来，保持 CGI_RUNNING 状态，什么都不用做
        // 🚨 就算 is_poll_up 是 true，只要 status 是 1，就必须继续读！
        return;
    } else if (status == 0) {
        // 🌟 外包干完活了 (EOF)
        this->_state = Connection::CGI_FINISH;

        // 找外包拿最终的完整数据
        const std::string& full_cgi_output = _cgi_handler.getRawResponse();
        if (this->_response.build_cgi_response(_request, full_cgi_output) == false) {
            buildErrorResponse(502);
            return;
        }
        this->_out_buff = this->_response.get_full_response();
        this->_cluster_mediator->unregister_cgi_fd(current_pipe_fd);

        // 子进程输出EOF PipeOut[0]可以关闭，开始启动waitpid 子进程资源回收。。。
        this->_cgi_handler.checkChildProcess();
        this->_state = Connection::WRITING_RESP;
    } else if (status == -1) {
        // 外包搞砸了 (Pipe 破裂等报错)
        std::cerr << "[Error] CGI read failed!" << std::endl;
        this->buildErrorResponse(500);
        this->_cluster_mediator->update_client_events(this->_client_fd, POLLOUT);
    }
}

void Connection::checkCGI()
{
    _cgi_handler.checkChildProcess();
    if (_cgi_handler.getState() == CGIHandler::CGI_FINISHED) {
        _state                        = CGI_FINISH;
        const std::string& raw_output = _cgi_handler.getRawResponse();
        this->_response.build_cgi_response(_request, raw_output);
    }
}

void Connection::finalize_cgi_success(int cgi_fd)
{
    std::cout << "[Server] CGI Process finished writing all data." << std::endl;

    this->_cgi_handler.checkChildProcess();

    int waitpid_status = _cgi_handler.get_waitpid_status();
    (void)
        waitpid_status;  // TODO extract waitpid_status to formalize cgi response msg status code...

    // 3. 核心解耦：先安全从 poll 中撤销，再关闭管道，防止 FD 复用串流
    this->_cluster_mediator->unregister_cgi_fd(cgi_fd);
    this->_cgi_handler._close_all_pipes();

    // 4. 驱动状态机进入发送响应阶段
    this->_state = WRITING_RESP;
    this->_cluster_mediator->update_client_events(this->_client_fd, POLLOUT);

    std::cout << "[Server] Switched client socket to POLLOUT." << std::endl;
}

bool Connection::isCGITimedOut()
{ return _cgi_handler.isTimeout(); }

void Connection::clean_up_cgi_handler(void)
{ _cgi_handler.reset(); }