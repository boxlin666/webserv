#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <string>

#include "PassiveSocket.hpp"
#include "ServerConfig.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "RouterCtx.hpp"
#include "RequestHandler.hpp"

class Connection
{
    private:
        int _client_fd;
        std::string _in_buff;
        std::string _out_buff;
        PassiveSocket *_matched_socket;
        ServerConfig *_matched_server;
        const std::map<int, std::vector<ServerConfig*> >& _server_map;

        HttpRequest _request;
        RouterCtx  _route_ctx;
        RequestHandler _req_handler;
        HttpResponse _response;

        int _status_code;

        //TO DO LATER (状态机)
        enum State 
        {
            READING, WRITING, CLOSED
        };
        State _state;


        Connection(const Connection& other);
        Connection& operator=(const Connection& other);

    public:
        Connection(int client_fd, PassiveSocket *matched_socket, const std::map<int, std::vector<ServerConfig*> >& server_map);
        ~Connection(void);

        int getFd(void)const;
        const std::string &get_in_buff(void)const;
        const std::string &get_out_buff(void)const;

        void append_in_buff(const char *tmp_buff, ssize_t recv_len);
        
        //Just a temporary function, we hard code the http response here...
        void set_out_buff(void);

        bool handle_data(const char* raw_data, ssize_t size);
        bool check_parse_finished();
        bool set_matched_server();
        void process_router_match();
        void process_request_handler();
        void prepare_response();
};

#endif