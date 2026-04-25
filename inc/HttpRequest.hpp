#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <map>
#include <string>
#include <cstdlib>

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
    const std::string& get_version() const;
    const std::string& get_body() const;
    const std::string* get_header(const std::string& key) const;  // 方便查找特定头

    e_request_state get_state() const;

    void reset();

    bool parse(std::string& input_data);

   private:
    // request line 部分
    std::string _method;  // GET POST DELETE
    std::string _path;
    std::string _http_version;  // HTTP 1.1
    std::map<std::string, std::string> _headers;
    std::string         _body;

    // request header 部分
    //  _header_map["Host"] = "172.19.116.71" ; _header_map["User agent"] = "curl 8.5.0" ;
    //  _header_map["blabla"] = "BLABLA"
    std::map<std::string, std::string> _header_map;

    // Body 部分 (主要针对POST method, 当客户端上传信息给服务端)
    std::string _body_content;
    std::size_t _body_len;

    // 记录当前状态
    e_request_state _state;
    size_t              _content_length;
    size_t              _chunk_size;    // 用于处理 chunked 传输
    bool                _is_chunked;

    bool parse_request_line(std::string& line);
    bool parse_request_header(std::string& line);
    bool parse_body(std::string& input_data);

    bool prepare_for_body();

    HttpRequest(const HttpRequest& other);
    HttpRequest& operator=(const HttpRequest& other);
};

#endif