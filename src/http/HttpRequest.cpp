#include "HttpRequest.hpp"
#include <iostream>
#include "HttpConstants.hpp"

#define RED     "\033[31m"
#define RESET   "\033[0m"

void HttpRequest::reset() {
    _method.clear();
    _path.clear();
    _http_version.clear();
    _body.clear();
    _headers.clear();
    _header_map.clear();

    _state = PARSE_REQUEST_LINE;
    _content_length = 0;
    _chunk_size = 0;
    _is_keep_alive = true; //in HTTP/1.1, by default, keep alive is true!
    _is_chunked = false;
}

// 构造函数直接调用 reset 即可
HttpRequest::HttpRequest() {
    this->reset();
}

HttpRequest::~HttpRequest() {
}

int    HttpRequest::parse_request_line(std::string& line)
{
    if (line.empty()) return (BAD_REQUEST);

    std::stringstream ss(line);
    std::string extra;
    std::size_t req_line_len = 0;

    if (!(ss >> this->_method >> this->_path >> this->_http_version))
        return (BAD_REQUEST);
    if (ss >> extra)
        return (BAD_REQUEST);
    if (this->_path.empty() || this->_path[0] != '/')
        return (BAD_REQUEST);
    if (this->_method.empty())
        return (BAD_REQUEST);
    if (this->_path.find("/../") != std::string::npos) //不允许访问除了www以外的，他的上一级目录
        return (BAD_REQUEST);
    if (this->_http_version != "HTTP/1.0" && this->_http_version != "HTTP/1.1")
        return (NO_HTTP_VERSION);
    req_line_len = this->_method.length() + this->_path.length() + this->_http_version.length();
    if (req_line_len > URI_SIZE)
        return (URI_TOO_LONG);
    this->_state = PARSE_HEADER;
    return (SUCCESS);
}

int HttpRequest::validate_and_prepare_payload()
{
    std::size_t req_header_len = 0;

    for(std::map<std::string, std::string>::const_iterator it = this->_header_map.begin(); it != this->_header_map.end(); ++it)
    {
        req_header_len += it->first.length() + it->second.length() + 4; //CTRL
    }
    if (req_header_len > MAX_HEADER_SIZE)
        return (REQ_HEADER_TOO_LONG);
    if (this->_header_map.find("Host") == this->_header_map.end())
        return (BAD_REQUEST);
    bool has_cl = this->_header_map.count("Content-Length");
    bool has_te = this->_header_map.count("Transfer-Encoding");

    if (has_cl && has_te) return (BAD_REQUEST);

    if (this->_method == "POST" && !has_cl && !has_te) return (NO_LENGTH);
    
    if (has_cl && this->_content_length > 0)
        _state = PARSE_BODY;
    else if (has_te && this->_header_map["Transfer-Encoding"] == "chunked") 
        _state = PARSE_CHUNKED;
    else
        _state = PARSE_FINISHED;
    return (SUCCESS);
}

int HttpRequest::parse_request_header(std::string& line) 
{
    if (line.empty())
    {
        int ret = validate_and_prepare_payload();
        return (ret);
    }

    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) return (BAD_REQUEST);

    std::string key = line.substr(0, colon_pos);
    std::string value = line.substr(colon_pos + 1);

    size_t first = value.find_first_not_of(' ');
    size_t last = value.find_last_not_of(' ');
    if(first != std::string::npos)
        value = value.substr(first, (last - first + 1));
    else
        value = "";
    _header_map[key] = value;
    if (key == "Content-Length")
    {
        _content_length = strtoul(_header_map["Content-Length"].c_str(), NULL, 10);
        if (this->_content_length > GLOBAL_MAX_ALLOWED)
        {
            //std::cout << "==============REQUEST PARSER ISSUE!!!=================" << std::endl;
            return (BODY_TOO_LARGE);
        }
    }
    else if (key == "Connection")
    {
        if (value == "keep-alive")
            this->_is_keep_alive = true;
        else if (value == "close")
            this->_is_keep_alive = false;
    }
    return (SUCCESS);
}

bool HttpRequest::parse_body(std::string& input_data) {
    // 还需要读多少字节？
    size_t remaining = _content_length - _body.size();
    
    // 能读多少就读多少
    size_t to_read = std::min(remaining, input_data.size());
    
    _body.append(input_data.substr(0, to_read));
    input_data.erase(0, to_read);

    if (_body.size() == _content_length) {
        _state = PARSE_FINISHED;
    }
    return true;
}

int HttpRequest::parse(std::string& input_data)
{
    int ret = SUCCESS;

    while (_state != PARSE_ERROR && _state != PARSE_FINISHED) 
    {
        if (_state == PARSE_REQUEST_LINE || _state == PARSE_HEADER) 
        {
            size_t pos = input_data.find("\r\n");
            if (pos == std::string::npos) break;

            std::string line = input_data.substr(0, pos);
            input_data.erase(0, pos + 2);  // 清除处理过的行以及CRLF

            if (_state == PARSE_REQUEST_LINE) {
                if (line.empty()) continue;
                ret = parse_request_line(line);
                if (ret != SUCCESS) _state = PARSE_ERROR;
            } else {
                ret = parse_request_header(line);
                if (ret != SUCCESS) _state = PARSE_ERROR;
            }
        }

        if (_state == PARSE_BODY) {
            parse_body(input_data);
            break;  // 即使没读完也要跳出循环，等更多数据进入 input_data
        }

        // TODO: PARSE_CHUNKED 逻辑
    }

    return (ret);
}

const std::string& HttpRequest::get_method() const
{
    return this->_method;
}
    
const std::string& HttpRequest::get_path() const
{
    return this->_path;
}

const std::string& HttpRequest::get_version() const
{
    return this->_http_version;
}

const std::string& HttpRequest::get_body() const
{
    return this->_body;
}

std::size_t HttpRequest::get_body_len() const
{
    return this->_body.length();
}

const std::string* HttpRequest::get_header(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _header_map.find(key);
    if (it == _header_map.end()) {
        return NULL; // 或者返回 NULL，由调用者判断
    }
    return &(it->second);
}

//added just to finish the compilation test (to update with get_header later)
const std::map<std::string, std::string>& HttpRequest::get_header_map() const 
{
    return this->_header_map;
}

HttpRequest::e_request_state HttpRequest::get_state() const
{
    return this->_state;
}

std::size_t HttpRequest::get_content_length()const
{
    return this->_content_length;
}

bool HttpRequest::get_is_keep_alive(void)const
{
    return (this->_is_keep_alive);
}