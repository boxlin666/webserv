#ifndef HTTP_RESPONSE_HPP
# define HTTP_RESPONSE_HPP

#include <string>
#include <map>

#include "ServerConfig.hpp"

class HttpResponse
{
private:
    int _status_code;
    std::map<std::string, std::string> _headers;
    std::string _body;
    std::string _full_response;
    const ServerConfig* _config;

    HttpResponse(const HttpResponse& other);
    HttpResponse& operator=(const HttpResponse& other);

    void _handle_get();
    void _handle_post();
    void _handle_delete();

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