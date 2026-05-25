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
#include "HttpConstants.hpp"
#include "Cluster.hpp"

class Cluster;

class Connection
{
    public:
        Connection(int client_fd, PassiveSocket *matched_socket, const std::vector<ServerConfig*> &servers, Cluster *cluster);
        ~Connection(void);

        enum State 
        {
            WAITING,
            READING_REQ,
            CGI_RUNNING,
            CGI_FINISH, 
            WRITING_RESP, 
            CLOSED,
            ERROR
        };
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
        void handle_cgi_write();
        void register_cgi_pipe_to_poll(int fd, short events);

        CGIHandler &get_cgi_handler(void);

        void checkCGI();
        bool isCGITimedOut();
        void updateCGIActivity() ;
    private:
        int _client_fd;
        std::string _in_buff;
        std::string _out_buff;
        PassiveSocket *_matched_socket;
        ServerConfig *_matched_server;
        const std::vector<ServerConfig*> &_servers;

        Cluster *_cluster;
        HttpRequest _request;
        RouterCtx  _route_ctx;
        RequestHandler _req_handler;
        HttpResponse _response;
        CGIHandler _cgi_handler;

        int _status_code;
        bool _cgi_active;

        State _state;

        bool check_parse_finished();
        bool set_matched_server();
        void process_router_match();
        void process_request_handler();
        void prepare_response();
        void handle_request_dispatch();
        void execute_cgi_pipeline();

        void _finalize_cgi_success(int cgi_fd);
        void _handle_cgi_read_error(int cgi_fd);

        void _parseCgiOutputAndMakeResponse(const std::string& raw_output);
        bool _splitCgiHeaderAndBody(const std::string& raw_output, std::string& header_part, std::string& body_part);
        std::map<std::string, std::string> _parseCgiHeaders(const std::string& header_part);
        std::string _assembleHttpResponse(const std::map<std::string, std::string>& cgi_headers, const std::string& body_part);

        Connection(const Connection& other);
        Connection& operator=(const Connection& other);

};

#endif
