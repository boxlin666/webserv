#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <map>
#include <string>
#include <cstdlib>
#include <sstream>

#include <fcntl.h>
#include <sys/stat.h> 
#include <vector>
#include <cstdio>
#include <cerrno>

//tempo
//#include "Location.hpp"

#define GLOBAL_MAX_ALLOWED 1048576
#define URI_SIZE 8192
#define MAX_HEADER_SIZE 8192

class HttpRequest {

   public:
    enum e_request_state {
        PARSE_REQUEST_LINE,
        PARSE_HEADER,
        PARSE_BODY,
        PARSE_CHUNKED,  // 增加对 Chunked 数据的支持
        PARSE_FINISHED,
        PARSE_ERROR
    };

    HttpRequest(void);
    ~HttpRequest(void);

    const std::string& get_method() const;
    const std::string& get_path() const;
    const std::string& get_querystring() const;
    const std::string& get_version() const;
    const std::string& get_body() const;
    const std::string* get_header(const std::string& key) const;  // 方便查找特定头
    std::size_t get_content_length() const;
    std::size_t get_body_len()const;

    //tempo add for compilation with response
    const std::map<std::string, std::string>& get_header_map()const;

    e_request_state get_state() const;

    void reset();

    int parse(std::string& input_data);

    // --- 仅用于隔离测试的 Setter (Unit Test Helpers) ---

    // 设置请求方法（GET, POST, DELETE 等）
    void set_method(const std::string& method) { _method = method; }

    // 设置请求路径（例如 "/index.html"）
    void set_path(const std::string& path) { _path = path; }

    // 设置 HTTP 版本（通常为 "HTTP/1.1"）
    void set_version(const std::string& version) { _http_version = version; }

    // 设置 Header 键值对
    void set_header(const std::string& key, const std::string& value) {
        _header_map[key] = value;
    }

    // 设置 Body 内容，并自动更新长度
    void set_body(const std::string& content) {
        _body = content;
        _content_length = content.length();
    }

    // 极其重要：手动标记解析状态
    // 测试时通常设为 HttpRequest::PARSE_FINISHED
    void set_state(e_request_state state) { _state = state; }

    // 如果需要模拟 Chunked 传输
    void set_is_chunked(bool is_chunked) { _is_chunked = is_chunked; }

   private:
    // request line 部分
    std::string _method;  // GET POST DELETE
    std::string _path;

    std::string _http_version;  // HTTP 1.1
    std::map<std::string, std::string> _headers;
    std::string         _body;
    std::string         _querystring;
    // request header 部分
    //  _header_map["Host"] = "172.19.116.71" ; _header_map["User agent"] = "curl 8.5.0" ;
    //  _header_map["blabla"] = "BLABLA"
    std::map<std::string, std::string> _header_map;

    // 记录当前状态
    e_request_state _state;
    std::size_t              _content_length;
    std::size_t              _chunk_size;    // 用于处理 chunked 传输
    bool                _is_chunked;

    int parse_request_line(std::string& line);
    int parse_request_header(std::string& line);
    int validate_and_prepare_payload();
    bool parse_body(std::string& input_data);
 

    HttpRequest(const HttpRequest& other);
    HttpRequest& operator=(const HttpRequest& other);
};

#endif