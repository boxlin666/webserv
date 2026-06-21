#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <fcntl.h>
#include <sys/stat.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "HttpConstants.hpp"
#include "Utils.hpp"

// tempo
// #include "Location.hpp"

#define GLOBAL_MAX_ALLOWED 1000000000
#define URI_SIZE 8192
#define MAX_HEADER_SIZE 8192

class HttpRequest {
   public:
    enum e_request_state {
        PARSE_REQUEST_LINE,
        PARSE_HEADER,
        PARSE_BODY,
        PARSE_CHUNKED,
        PARSE_FINISHED,
        PARSE_ERROR
    };

    enum e_chunk_state { CHUNK_NONE, CHUNK_START, CHUNK_SIZE, CHUNK_DATA, CHUNK_FINISHED };

    HttpRequest(void);
    ~HttpRequest(void);

    const std::string&       get_method() const;
    const std::string&       get_path() const;
    const std::string&       get_querystring() const;
    const std::string&       get_version() const;
    const std::string&       get_body() const;
    const std::string&       get_header(const std::string& key) const;
    std::size_t              get_content_length() const;
    std::size_t              get_body_len() const;
    bool                     get_is_keep_alive() const;
    bool                     get_is_chunked() const;
    static const std::string empty_string;

    const std::map<std::string, std::string>& get_header_map() const;

    e_request_state get_state() const;

    void reset();

    int parse(std::string& input_data);

    void set_method(const std::string& method)
    { _method = method; }

    void set_path(const std::string& path)
    { _path = path; }

    void set_version(const std::string& version)
    { _http_version = version; }

    void set_body(const std::string& content)
    {
        _body           = content;
        _content_length = content.length();
    }

    void set_state(e_request_state state)
    { _state = state; }

    void set_is_chunked(bool is_chunked)
    { _is_chunked = is_chunked; }

    void set_is_keep_alive(bool is_keep_alive)
    { _is_keep_alive = is_keep_alive; }

    bool is_header_parsed() const
    { return _state > PARSE_HEADER; }

    int                parse_multipart_body(void);
    const std::string& get_multipart_filename(void) const;

   private:
    std::string _method;
    std::string _path;
    std::string _http_version;
    std::string _body;
    std::string _querystring;

    std::map<std::string, std::string> _header_map;

    std::vector<std::string> _content_type_vector;

    e_request_state _state;
    e_chunk_state   _chunk_state;
    std::size_t     _content_length;
    std::size_t     _chunk_size;
    std::string     _boundary_value;
    std::string     _multipart_filename;
    bool            _is_chunked;
    bool            _is_keep_alive;

    int parse_request_line(std::string& line);
    int parse_request_header(std::string& line);
    int validate_and_prepare_payload();
    int parse_body(std::string& input_data);
    int parse_chunked_body(std::string& input_data);

    bool parse_chunk_size(std::string& chunk_size_str);

    int _parse_content_type(const std::string& content_type_value);

    HttpRequest(const HttpRequest& other);
    HttpRequest& operator=(const HttpRequest& other);
};

#endif
