#include "Connection.hpp"
#include "Router.hpp"

Connection::Connection(int client_fd, PassiveSocket *matched_socket, const std::map<int, std::vector<ServerConfig*> >& server_map)
:_client_fd(client_fd),
_in_buff(""),
_out_buff(""),
_matched_socket(matched_socket),
_server_map(server_map),
_request(),
_route_ctx(),
_req_handler(),
_response(),
_status_code(200)
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
    this->_status_code = _request.parse(_tmp_buff);
    if(this->_status_code != SUCCESS)
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

bool  Connection::set_matched_server()
{
    //Passive Socket info
    int port_num = this->_matched_socket->getPort();
    std::string host = this->_matched_socket->get_host();

    std::map<int, std::vector<ServerConfig*> >::const_iterator server_map_it = this->_server_map.find(port_num);
    const std::vector<ServerConfig*>* selected_servers = NULL;

    if (server_map_it != this->_server_map.end())
        selected_servers = &(server_map_it->second);
    else
    {
        std::stringstream ss;
        ss << "No matched server configuration for port [" << port_num << "] on " << host;
        throw std::runtime_error(ss.str());
    }
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
    
    for (std::size_t i = 0; i < (*selected_servers).size(); i++)
    {
        for (std::size_t j = 0; j < (*selected_servers)[i]->get_servers_name().size() ;j++)
        {
            if (req_host == (*selected_servers)[i]->get_servers_name()[j])
            {
                this->_matched_server = (*selected_servers)[i];
                std::cout << (*selected_servers)[i]->get_servers_name()[j] << std::endl;
                return (true);
            }
        }
    }
    this->_matched_server = (*selected_servers)[0];//if no matched server, then select the 1st one associted with this port number by default!
    std::cout << (*selected_servers)[0]->get_servers_name()[0] << std::endl;
    return (true);
}

void Connection::process_router_match()
{
    if (this->_matched_server)
        this->_route_ctx = Router::build_router_context(this->_request, *(this->_matched_server), _status_code);
    std::cout << "full_path = " << this->_route_ctx.full_path << std::endl;
}

void Connection::prepare_response()
{
    this->_response.build(this->_req_handler, this->_status_code);
    this->_out_buff = this->_response.get_full_response(); 
    //return (this->_response.get_full_response());
}