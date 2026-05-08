#include "Connection.hpp"

Connection::Connection(int client_fd, PassiveSocket *matched_socket)
:_client_fd(client_fd),
_in_buff(""),
_out_buff(""),
_matched_socket(matched_socket),
_request(),
_response()
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

bool    Connection::handle_data(const char* raw_data, ssize_t size)
{
    std::string _tmp_buff = raw_data;

    this->_in_buff.append(raw_data, size);
    if(_request.parse(_tmp_buff) == false)
    {
        // TODO: prepare error 400 
        // skip Router Match step
        // Go to the Request handler -> request Response
        return false;
    }
   
    if(_request.get_state() == HttpRequest::PARSE_FINISHED)
    {
        // TODO: process logic
    }
    return true;
}

bool Connection::check_parse_finished()
{
    return _request.get_state() == HttpRequest::PARSE_FINISHED;
}

/*void  Connection::set_matched_server()
{
    int port_num = this->_matched_socket->getPort();




    std::map<std::string, std::string>::const_iterator it;
    std::string _raw_data;

    it = this->_request.get_header_map().find("Host");
    if (it != this->_request.get_header_map().end())
        _raw_data = it->second;
    std::cout << "_raw_data " << _host  << std::endl;

    std::stirng host;
    std::string port_str;
    int port;

    std::size_t colon_pos = _raw_data.find(':');

    port_str = _raw_data.substr(colon_pos + 1);
    
    string to Port

    
}*/

void Connection::process_router_match()
{
    if (this->_matched_server)
        this->_route_ctx = Router::build_router_context(this->_request, *(this->_matched_server));
}

void Connection::prepare_response()
{
    this->_response.build(this->_request, *(this->_matched_server));
    this->_out_buff = this->_response.get_full_response(); 
    //return (this->_response.get_full_response());
}