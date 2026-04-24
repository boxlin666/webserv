#include "HttpRequest.hpp"

void HttpRequest::reset() {
    _method.clear();
    _path.clear();
    _http_version.clear();
    _body.clear();
    _body_content.clear();

    _headers.clear();
    _header_map.clear();

    _state = PARSE_REQUEST_LINE;
    _body_len = 0;
    _content_length = 0;
    _chunk_size = 0;
    _is_chunked = false;

    // _error_code = 0;
}

// 构造函数直接调用 reset 即可
HttpRequest::HttpRequest() {
    this->reset();
}

HttpRequest::~HttpRequest() {
}

bool HttpRequest::parse_request_line(std::string& line)
{
    size_t pos1 = line.find(' ');
    if (pos1 == std::string::npos) return false;
    _method = line.substr(0, pos1);

    size_t pos2 = line.find(' ');
    if (pos2 == std::string::npos) return false;
    _path = line.substr(pos1 + 1, pos2 - (pos1 + 1));

    _http_version = line.substr(pos2 + 1);

    // check http version
    if (_http_version != "HTTP/1.1") return false;

    _state = PARSE_HEADER;
    return true;
}

bool HttpRequest::parse_request_header(std::string& line) 
{
    if (line.empty())
    {
        return prepare_for_body();
    }

    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) return false;

    std::string key = line.substr(0, colon_pos);
    std::string value = line.substr(colon_pos + 1);

    size_t first = value.find_first_not_of(' ');
    size_t last = value.find_last_not_of(' ');
    if(first != std::string::npos)
        value = value.substr(first, (last - first + 1));
    else
        value = "";
    _header_map[key] = value;
    return true;
}

bool HttpRequest::prepare_for_body()
{
    if(_header_map.count("Content-Length"))
    {
        _body_len = strtoul(_header_map["Content-Length"].c_str(), NULL, 10);
        if(_body_len > 0) _state = PARSE_BODY;
        else _state = PARSE_FINISHED;
    }
    else if (_header_map.count("Transfer-Encoding") && _header_map["Transfer-Encoding"] == "chunked") {
        _state = PARSE_CHUNKED; // TODO: 需要实现专门的 Chunked 解析逻辑
    }
    else {
        // GET 请求通常没有 Body
        _state = PARSE_FINISHED;
    }
    return true;
}

bool HttpRequest::parse_body(std::string& input_data) {
    // 还需要读多少字节？
    size_t remaining = _body_len - _body_content.size();
    
    // 能读多少就读多少
    size_t to_read = std::min(remaining, input_data.size());
    
    _body_content.append(input_data.substr(0, to_read));
    input_data.erase(0, to_read);

    if (_body_content.size() == _body_len) {
        _state = PARSE_FINISHED;
    }
    return true;
}

bool HttpRequest::parse(std::string& input_data)
{
    while (_state != PARSE_ERROR && _state != PARSE_FINISHED) {
        if (_state == PARSE_REQUEST_LINE || _state == PARSE_HEADER) {
            size_t pos = input_data.find("\r\n");
            if (pos == std::string::npos) break;

            std::string line = input_data.substr(0, pos);
            input_data.erase(0, pos + 2);  // 清除处理过的行以及CRLF

            if (_state == PARSE_REQUEST_LINE) {
                if (line.empty()) continue;
                if (!parse_request_line(line)) _state = PARSE_ERROR;
            } else {
                if (line.empty()) continue;
                if (!parse_request_header(line)) _state = PARSE_ERROR;
            }
        }

        if (_state == PARSE_BODY) {
            // 根据 Content-Length 读取 Body
            size_t bytes_needed  = _content_length - _body.size();
            size_t bytes_to_copy = std::min(bytes_needed, input_data.size());

            _body.append(input_data.substr(0, bytes_to_copy));
            input_data.erase(0, bytes_to_copy);

            if (_body.size() == _content_length) _state = PARSE_FINISHED;
            break;  // 即使没读完也要跳出循环，等更多数据进入 input_data
        }

        // TODO: PARSE_CHUNKED 逻辑
    }
    return (_state != PARSE_ERROR);
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

std::size_t HttpRequest::get_body_len()const
{
    return this->_body_len;
}

const std::string& HttpRequest::get_body_content()const
{
    return this->_body_content;
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
