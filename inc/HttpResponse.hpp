#ifndef HTTP_RESPONSE_HPP
# define HTTP_RESPONSE_HPP

#include <string>
#include <map>
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"

#define URI_SIZE 8192
#define MAX_HEADER_SIZE 8192
#define URI_TOO_LONG 414
#define MAX_HEADER_SIZE 431
#define SERVER_ERROR 500
#define NO_HTTP_VERSION 505
#define NO_METHOD 501
#define BODY_TOO_LARGE 413
#define PER_DENIED 403
#define NOT_FOUND 404
#define BAD_REQUEST 400
#define SUCCESS 200

class HttpResponse
{
private:
    int _status_code;
    std::map<std::string, std::string> _headers;
    std::string _body;
    std::string _full_response;
    std::size_t body_len;
    const ServerConfig* _config;

    HttpResponse(const HttpResponse& other);
    HttpResponse& operator=(const HttpResponse& other);

    void _handle_get(const std::string& full_path);
    void _handle_post();
    void _handle_delete();

    int _check_request(const HttpRequest& req)const;
    std::string &_build_full_path(const HttpRequest& req, const ServerConfig& config)const;
    int _check_resource(std::string &full_path);

    void set_body_len(size_t input);
    // 根据文件后缀判断类型
    //std::string _get_content_type(std::string path);
public:
    HttpResponse();
    ~HttpResponse();
    
    // response = status line + header + body
    void build(const HttpRequest& req, const ServerConfig& config);
    void set_status(int code);
    void add_header(const std::string& key, const std::string& value);
    std::string get_raw_data();

};

#endif