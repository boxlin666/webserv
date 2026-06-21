#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <poll.h>
#include <time.h>

#include <string>

#include "CGIHandler.hpp"
#include "HttpConstants.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "IClusterMediator.hpp"
#include "PassiveSocket.hpp"
#include "RequestHandler.hpp"
#include "RouterCtx.hpp"
#include "ServerConfig.hpp"

class IClusterMediator;

class Connection {
   public:
    Connection(int client_fd, PassiveSocket* matched_socket,
               const std::vector<ServerConfig*>& servers, IClusterMediator* cluster_mediator);
    ~Connection(void);

    enum State { WAITING, READING_REQ, CGI_RUNNING, CGI_FINISH, WRITING_RESP, CLOSED, ERROR };

    const std::string& get_in_buff(void) const;
    const std::string& get_out_buff(void) const;

    void handle_read_event(void);
    void process_existing_in_buff(void);
    void handle_write_event(void);

    void  set_state(State state);
    State get_state(void) const;
    short get_poll_events() const;

    int get_client_fd() const
    { return _client_fd; }
    void handle_cgi_read();
    void handle_cgi_write();

    void clean_up_cgi_handler(void);
    void buildErrorResponse(int status_code);
    void checkCGI();
    bool isCGITimedOut();
    void finalize_cgi_success(int cgi_fd);

    bool   is_waiting_request_msg() const;
    time_t get_last_recv_time() const;
    void   set_request_keep_alive(bool is_keep_alive);

   private:
    int                              _client_fd;
    std::string                      _in_buff;
    std::string                      _back_up_in_buff;
    std::string                      _out_buff;
    PassiveSocket*                   _matched_socket;
    ServerConfig*                    _matched_server;
    const std::vector<ServerConfig*> _servers;

    IClusterMediator* _cluster_mediator;
    HttpRequest       _request;
    RouterCtx         _route_ctx;
    RequestHandler    _req_handler;
    HttpResponse      _response;
    CGIHandler        _cgi_handler;

    int _status_code;

    State _state;

    time_t _last_recv_time;

    bool check_parse_finished();
    bool set_matched_server();
    void process_router_match();
    void prepare_static_response();
    void handle_request_dispatch();
    void execute_cgi_pipeline();

    void _update_last_recv_time();
    void _set_status_code(int code);
    void _reset_status_code();

    Connection(const Connection& other);
    Connection& operator=(const Connection& other);
};

#endif
