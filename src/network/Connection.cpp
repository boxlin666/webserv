#include "Connection.hpp"

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
{ _update_last_recv_time(); }

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
        this->set_state(CLOSED);
        return;
    } else if (bytes_read == 0) {
        if ((!_in_buff.empty() || _state == READING_REQ)) {
            if (_status_code == BODY_TOO_LARGE)
                buildErrorResponse(BODY_TOO_LARGE);
            else
                buildErrorResponse(BAD_REQUEST);
        } else {
            this->set_state(CLOSED);
            return;
        }
    } else {
        if (_state == WAITING) this->set_state(READING_REQ);

        _update_last_recv_time();
        _in_buff.append(buffer, static_cast<std::size_t>(bytes_read));
        _back_up_in_buff.append(buffer, static_cast<std::size_t>(bytes_read));
        this->process_existing_in_buff();
    }
}

void Connection::process_existing_in_buff()
{
    int ret = this->_request.parse(this->_in_buff);

    _set_status_code(ret);

    if (this->_status_code != SUCCESS) {
        _request.set_is_keep_alive(false);
        this->handle_request_dispatch();
        return;
    }

    if (this->check_parse_finished()) {
        debug_msg_print("REQUEST_MSG", _back_up_in_buff, "\033[31m", 400);
        this->_request.parse_multipart_body();
        this->handle_request_dispatch();
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
        else if (_route_ctx.is_redirect) {
            this->_out_buff = this->_response.build_redirect_response(
                _route_ctx.redirect_code, _route_ctx.redirect_url, this->_request);
            this->set_state(WRITING_RESP);
        } else if (_route_ctx.is_cgi_potential) {
            this->execute_cgi_pipeline();
            return;
        } else {
            this->prepare_static_response();
            this->set_state(WRITING_RESP);
        }
    } catch (const std::exception& e) {
        std::cerr << "CATCHED FATAL ERROR: " << e.what() << std::endl;
        return;
    }
}

void Connection::execute_cgi_pipeline()
{
    std::cout << "[Server] CGI detected. Initiating Child Process..." << std::endl;

    if (this->_cgi_handler.init(this->_request, this->_route_ctx) == false ||
        this->_cgi_handler.execute() == false) {
        buildErrorResponse(500);
        this->set_state(WRITING_RESP);
        return;
    }

    this->_state = CGI_RUNNING;

    int read_fd  = _cgi_handler.getReadFd();
    int write_fd = _cgi_handler.getWriteFd();

    if (read_fd != -1) this->_cluster_mediator->register_cgi_fd(read_fd, POLLIN, this);

    if (write_fd != -1) this->_cluster_mediator->register_cgi_fd(write_fd, POLLOUT, this);
}

void Connection::handle_write_event(void)
{
    if (this->_state == Connection::CGI_RUNNING) return;

    if (this->_out_buff.empty()) return;
    ssize_t bytes_send;

    bytes_send = send(this->_client_fd, this->_out_buff.c_str(), this->_out_buff.size(), 0);

    if (bytes_send < 0) {
        this->_state = CLOSED;
        return;
    } else {
        this->_out_buff.erase(0, bytes_send);
    }

    if (!this->_in_buff.empty() &&
        (this->_status_code == BAD_REQUEST || this->_status_code == REQUEST_TIMEOUT ||
         this->_status_code == BODY_TOO_LARGE))
        this->_in_buff.clear();

    if (this->_out_buff.empty()) {
        if ((this->_request.get_is_keep_alive() == false ||
             this->_response.get_full_response().find("Connection: close") != std::string::npos) &&
            this->_in_buff.empty()) {
            this->_state = CLOSED;
        } else {
            this->_request.reset();
            this->_response.reset();
            this->_reset_status_code();
            if (!this->_in_buff.empty()) {
                std::cout << "[Pipeline] Remaining data detected in _in_buff ("
                          << this->_in_buff.size() << " bytes). Driving next request inline."
                          << std::endl;
                this->_state = READING_REQ;
                this->process_existing_in_buff();
            } else {
                this->_state = WAITING;
            }
        }
    }
}

bool Connection::check_parse_finished()
{
    if (this->_request.get_state() != HttpRequest::PARSE_FINISHED) { return false; }

    if (this->_request.get_method() == "POST" && !this->_request.get_is_chunked()) {
        size_t expected_len = this->_request.get_content_length();
        size_t actual_len   = this->_request.get_body().length();

        std::cout << "expected_len = " << expected_len << std::endl;
        std::cout << "actual_len = " << actual_len << std::endl;

        if (actual_len < expected_len) { return false; }
    }

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
        req_host = _raw_data.substr(0, colon_pos);

    for (std::size_t i = 0; i < this->_servers.size(); i++) {
        for (std::size_t j = 0; j < this->_servers[i]->get_servers_name().size(); j++) {
            if (req_host == this->_servers[i]->get_servers_name()[j]) {
                this->_matched_server = this->_servers[i];
                return (true);
            }
        }
    }
    this->_matched_server = this->_servers[0];
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

    error_page_path = Router::get_error_page_path(_route_ctx, status_code);
    this->_out_buff = _response.build_error_response(status_code, error_page_path, _request);
    this->_state    = Connection::WRITING_RESP;
}

void Connection::prepare_static_response()
{
    if (!this->_response.build_static_response(this->_request, this->_req_handler,
                                               this->_status_code))
        this->buildErrorResponse(this->_status_code);
    this->_out_buff = this->_response.get_full_response();
}

void Connection::set_state(State state)
{ this->_state = state; }

Connection::State Connection::get_state(void) const
{ return (this->_state); }

short Connection::get_poll_events() const
{
    if (this->_state == WAITING || this->_state == READING_REQ || this->_state == CGI_RUNNING)
        return POLLIN;

    if (this->_state == WRITING_RESP) return POLLOUT;

    return 0;
}

int Connection::get_client_fd() const
{ return _client_fd; }

void Connection::handle_cgi_write()
{
    int current_pipe_fd = _cgi_handler.getWriteFd();
    if (current_pipe_fd == -1) return;

    int status = _cgi_handler.sendToScript();
    if (status <= 0) this->_cluster_mediator->unregister_cgi_fd(current_pipe_fd);
}

void Connection::handle_cgi_read()
{
    int current_pipe_fd = _cgi_handler.getReadFd();

    if (current_pipe_fd == -1) return;

    int status = _cgi_handler.receiveFromScript();

    if (status == 1) {
        return;
    } else if (status == 0) {
        this->_state = Connection::CGI_FINISH;

        const std::string& full_cgi_output = _cgi_handler.getRawResponse();
        if (this->_response.build_cgi_response(_request, full_cgi_output) == false) {
            buildErrorResponse(502);
            return;
        }
        this->_out_buff = this->_response.get_full_response();
        this->_cluster_mediator->unregister_cgi_fd(current_pipe_fd);

        this->_cgi_handler.checkChildProcess();
        this->_state = Connection::WRITING_RESP;
    } else if (status == -1) {
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
    _cgi_handler.checkChildProcess();
    _cgi_handler.get_waitpid_status();

    this->_cluster_mediator->unregister_cgi_fd(cgi_fd);
    this->_cgi_handler._close_all_pipes();

    this->_state = WRITING_RESP;
    this->_cluster_mediator->update_client_events(this->_client_fd, POLLOUT);

    std::cout << "[Server] Switched client socket to POLLOUT." << std::endl;
}

bool Connection::isCGITimedOut()
{ return _cgi_handler.isTimeout(); }

void Connection::clean_up_cgi_handler(void)
{ _cgi_handler.reset(); }

void Connection::_update_last_recv_time()
{ _last_recv_time = time(NULL); }

bool Connection::is_waiting_request_msg() const
{
    if (_state == READING_REQ && this->_request.get_state() < HttpRequest::PARSE_FINISHED &&
        _status_code == SUCCESS)
        return (true);
    return (false);
}

time_t Connection::get_last_recv_time() const
{ return _last_recv_time; }

void Connection::_set_status_code(int code)
{
    if (code < 100 || code > 599) {
        this->_status_code = SERVER_ERROR;
        return;
    }
    if (this->_status_code >= 400 && code < 400) return;
    this->_status_code = code;
}

void Connection::_reset_status_code()
{ this->_status_code = SUCCESS; }

void Connection::set_request_keep_alive(bool is_keep_alive)
{ _request.set_is_keep_alive(is_keep_alive); }
