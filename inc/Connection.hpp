#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <string>

#include "CGIHandler.hpp"
#include "IClusterMediator.hpp"
#include "HttpConstants.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
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

    void handle_read_event(int flag);
    void handle_write_event(void);

    void  set_state(State state);
    State get_state(void) const;
    short get_poll_events() const;

    void handle_cgi_read();
    void handle_cgi_write();

    // TODO
    CGIHandler& get_cgi_handler(void);

    void checkCGI();
    bool isCGITimedOut();
    void finalize_cgi_success(int cgi_fd);

   private:
    int                              _client_fd;
    std::string                      _in_buff;
    std::string                      _out_buff;
    PassiveSocket*                   _matched_socket;
    ServerConfig*                    _matched_server;
    const std::vector<ServerConfig*> _servers;

    IClusterMediator* _cluster_mediator;
    HttpRequest    _request;
    RouterCtx      _route_ctx;
    RequestHandler _req_handler;
    HttpResponse   _response;
    CGIHandler     _cgi_handler;

    int  _status_code;

    State _state;

    bool check_parse_finished();
    bool set_matched_server();
    void process_router_match();
    void prepare_response();
    void handle_request_dispatch();
    void execute_cgi_pipeline();

    // TODO: 逻辑分发
    void _parseCgiOutputAndMakeResponse(const std::string& raw_output);
    bool _splitCgiHeaderAndBody(const std::string& raw_output, std::string& header_part,
                                std::string& body_part);
    std::map<std::string, std::string> _parseCgiHeaders(const std::string& header_part);
    std::string _assembleHttpResponse(const std::map<std::string, std::string>& cgi_headers,
                                      const std::string&                        body_part);

    Connection(const Connection& other);
    Connection& operator=(const Connection& other);
};

#endif
