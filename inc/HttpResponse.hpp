#ifndef HTTP_RESPONSE_HPP
# define HTTP_RESPONSE_HPP

#include <string>
#include <map>
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"

#define URI_SIZE 8192
#define MAX_HEADER_SIZE 8192

#define SUCCESS 200
#define CREATED 201
#define DELETED 204
#define BAD_REQUEST 400
#define PER_DENIED 403
#define NOT_FOUND 404
#define METHOD_NOT_ALLOWED 405
#define BODY_TOO_LARGE 413
#define URI_TOO_LONG 414
#define SERVER_ERROR 500
#define NO_METHOD 501
#define NO_HTTP_VERSION 505

class HttpResponse
{
private:
    int _status_code;
    std::string _status_line;
    static std::map<int, std::string> _status_msg_map;
    static std::map<std::string, std::string> _ext_map;
    std::map<std::string, std::string> _headers_map;
    std::string _body;
    std::string _full_response;
    std::size_t _body_len;

    std::string _full_path;
    const ServerConfig* _config;

    HttpResponse(const HttpResponse& other);
    HttpResponse& operator=(const HttpResponse& other);

    static void _init_status_msg_map(void);
    static void _init_ext_map(void);
    int _handle_get(void);
    int _handle_post(const HttpRequest& req);
    int _handle_delete(void);

    int _check_request(const HttpRequest& req)const;
    std::string _build_full_path(const HttpRequest& req, const ServerConfig& config)const;
    int _check_resource(const HttpRequest& req);
    std::string &_build_status_line(const HttpRequest& req);

    void _build_headers_map(void);
    void _add_header(const std::string& key, const std::string& value);
    void set_body_len(size_t body_len);
    // 根据文件后缀判断类型
    std::string _get_content_type(void)const;
    std::string _get_date(void)const;
    std::string _get_last_modif_date(void)const;
    void _append_full_response(void);

    void set_status(int code);

public:
    HttpResponse();
    ~HttpResponse();
    
    // response = status line + header + body
    void build(const HttpRequest& req, const ServerConfig& config);
    const std::string &get_full_response()const;

    //reset function for each turn of RUN loop, to clean up the old content inside!!
    void reset(void);
    static void init_response_map();
};

#endif