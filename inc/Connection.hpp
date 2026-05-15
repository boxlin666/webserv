#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <string>

#include "PassiveSocket.hpp"
#include "ServerConfig.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "RouterCtx.hpp"
#include "RequestHandler.hpp"
#include "CGIHandler.hpp"

class Connection
{
    private:
        int _client_fd;
        std::string _in_buff;
        std::string _out_buff;
        PassiveSocket *_matched_socket;
        ServerConfig *_matched_server;
        const std::vector<ServerConfig*> &_servers;

        HttpRequest _request;
        RouterCtx  _route_ctx;
        RequestHandler _req_handler;
        HttpResponse _response;
        CGIHandler _cgi_handler;

        int _status_code;
        bool _cgi_active;

        //TO DO LATER (状态机)
        enum State 
        {
            WAITING,
            READING, 
            WRITING, 
            CLOSED
        };
        State _state;

        bool check_parse_finished();
        bool set_matched_server();
        void process_router_match();
        void process_request_handler();
        void prepare_response();

        Connection(const Connection& other);
        Connection& operator=(const Connection& other);

    public:
        Connection(int client_fd, PassiveSocket *matched_socket, const std::vector<ServerConfig*> &servers);
        ~Connection(void);

        int getFd(void)const;
        const std::string &get_in_buff(void)const;
        const std::string &get_out_buff(void)const;

        void append_in_buff(const char *tmp_buff, ssize_t recv_len);
        
        //Just a temporary function, we hard code the http response here...
        void set_out_buff(void);

        void handle_read_event(void);

        void handle_write_event(void);
	   
        void set_state(State state);
	    State get_state(void)const;
        short get_poll_events()const;

        bool has_cgi();
        int get_cgi_read_fd() const;
        void handle_cgi_read();
};

#endif
