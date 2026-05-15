#include <poll.h>

#include "Connection.hpp"
#include "Router.hpp"

Connection::Connection(int client_fd, PassiveSocket *matched_socket, const std::vector<ServerConfig*> &servers)
:_client_fd(client_fd),
_in_buff(""),
_out_buff(""),
_matched_socket(matched_socket),
_matched_server(NULL),
_servers(servers),
_request(),
_route_ctx(),
_req_handler(),
_response(),
_status_code(200),
_state(WAITING)
{
}

Connection::~Connection(void)
{

}

int Connection::getFd(void)const
{
    return (this->_client_fd);
}

const std::string &Connection::get_in_buff(void)const
{
    return (this->_in_buff);
}

const std::string &Connection::get_out_buff(void)const
{
    return (this->_out_buff);
}

void    Connection::append_in_buff(const char *tmp_buff, ssize_t recv_len)
{
    if (!tmp_buff || recv_len <= 0)
        return ;
    this->_in_buff.append(tmp_buff, static_cast<std::size_t>(recv_len));
}

//temporary function...
void    Connection::set_out_buff(void)
{
    this->_out_buff = "HTTP/1.1 200 OK\r\n\r\n<h1>Hello from Server Class!</h1>";
}

void Connection::handle_read_event(void)
{
    //1. 读取数据
    char    buffer[4096];
    std::memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_read = recv(this->_client_fd, buffer, sizeof(buffer), 0);

    std::cout << "BUFFER INFO :\n"  << buffer << "\n" << std::endl;
    if (bytes_read <= 0) 
    {
        // bytes_read == 0: 客户端关闭; < 0: 读取错误
        this->set_state(CLOSED);
        return ;
    } 

    std::string tmp_buff = buffer; //tempo TODO update!!
    this->append_in_buff(buffer, bytes_read);
    this->set_state(READING);
    //this->_status_code = this->_request.parse(tmp_buff);
    this->_status_code = this->_request.parse(this->_in_buff);

    if (this->_status_code != SUCCESS)
    {
        this->prepare_response();
        this->set_state(WRITING);
        return ;
    }
  

    // 3. 检查解析是否完成
    if (this->check_parse_finished()) {
        std::cout << "[Server] Request parsed successfully. Preparing response..." << std::endl;
        //开启路由匹配
        this->set_matched_server();
        this->process_router_match();
        this->process_request_handler();

        //     // 构建响应内容（根据 GET/POST 路径去找文件或跑 CGI）
       
        this->prepare_response();
        this->set_state(WRITING);
    }
}

void Connection::handle_write_event(void)
{
    ssize_t bytes_send; 
    
    bytes_send = send(this->_client_fd, this->_out_buff.c_str(), this->_out_buff.size(), 0);

    if (bytes_send < 0)
    {
        std::cerr << "Failed to send the http response on ..."  << std::endl;
        this->_state = CLOSED;
        return ;
    }
    else
        this->_out_buff.erase(0, bytes_send);

    if (this->_out_buff.empty())
    {
        if (this->_request.get_is_keep_alive() == false)
        {
            this->_state = CLOSED;
        }
        else if (this->_request.get_is_keep_alive() == true)
        {
            this->_state = WAITING;
        }
    }
}

bool Connection::check_parse_finished()
{
    return _request.get_state() == HttpRequest::PARSE_FINISHED;
}

bool  Connection::set_matched_server()
{
    //Get info from httpRequest
    std::map<std::string, std::string>::const_iterator req_header_it;
    std::string _raw_data;

    req_header_it = this->_request.get_header_map().find("Host");
    if (req_header_it != this->_request.get_header_map().end())
        _raw_data = req_header_it->second;
    else
        return (false); //failure on server match status_code should be 400 
    std::size_t colon_pos = _raw_data.find(':');
    std::string req_host; //Host: xxxx (in http request header)
    if (colon_pos == std::string::npos)
        req_host = _raw_data;
    else
        req_host = _raw_data.substr(0, std::string::npos);
    
    for (std::size_t i = 0; i < this->_servers.size(); i++)
    {
        for (std::size_t j = 0; j < this->_servers[i]->get_servers_name().size() ;j++)
        {
            if (req_host == this->_servers[i]->get_servers_name()[j])
            {
                this->_matched_server = this->_servers[i];
                std::cout << "selected server name is : "<< this->_servers[i]->get_servers_name()[j] << std::endl;
                return (true);
            }
        }
    }
    this->_matched_server = this->_servers[0];//if no matched server, then select the 1st one associted with this port number by default!
    std::cout << "selected default server name is : " << this->_servers[0]->get_servers_name()[0] << std::endl;
    return (true);
}

void Connection::process_router_match()
{
    if (this->_status_code != SUCCESS)
        return ;
    if (this->_matched_server)
        this->_route_ctx = Router::build_router_context(this->_request, *(this->_matched_server), _status_code);
}

void Connection::process_request_handler()
{
    if (this->_status_code != SUCCESS)
        return ;
    this->_req_handler.process_request_handler(this->_request, this->_route_ctx, _status_code);
}

void Connection::prepare_response()
{
    this->_response.build(this->_request, this->_req_handler, this->_status_code);
    this->_out_buff = this->_response.get_full_response();
    std::cout << "out buff = "<< this->_out_buff << std::endl;
}

void Connection::set_state(State state)
{
    this->_state = state;
}

Connection::State Connection::get_state(void)const
{
	return (this->_state);
}

short Connection::get_poll_events()const
{
    if (this->get_state() == READING || this->get_state() == WAITING) 
        return (POLLIN);
    else if (this->get_state() == WRITING)
        return (POLLOUT);
    return (0);
}

bool Connection::has_cgi()
{
    return (this->_cgi_handler.getPid() != -1 && !this->_cgi_handler.isFinished());
}

int Connection::get_cgi_read_fd() const
{
    return this->_cgi_handler.getReadFd();
}

void Connection::handle_cgi_read()
{
    char buffer[4096];
    int  fd = _cgi_handler.getReadFd();
    
    // 从管道读取数据
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer));

    if (bytes_read > 0)
    {
        _out_buff.append(buffer, bytes_read);
    }
    else if (bytes_read == 0)
    {
        _cgi_active = false;
        
        _state = WRITING;
        
        // TODO: 清理管道映射
        // _cgi.closePipes(); 
    }
    else
    {
        // 如果是 EAGAIN 表示现在没数据了（非阻塞常见情况），直接返回等下次 poll
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            // TODO: senderror
            _status_code = 500;
        }
    }
}