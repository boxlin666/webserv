#include <iostream>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

bool HttpResponse::build_cgi_response(const HttpRequest& request, const std::string& cgi_output)
{
    std::string header_part;

    if (!_split_cgi_header_body(cgi_output, header_part)) {
        std::cerr << "[CGI Parse Error] No header-body delimiter found!" << std::endl;
        return false;
    }
    _parse_cgi_headers(header_part);

    if (!_validate_cgi_content_type()) {
        std::cerr << "[CGI Parse Error] Missing or invalid Content-Type!" << std::endl;
        return false;
    }
    _full_response.reserve(cgi_output.length());
    _prepare_cgi_response(request);
    _append_full_response();
    return true;
}

void HttpResponse::_prepare_cgi_response(const HttpRequest& request)
{
    _build_cgi_status_line(request);
    _build_cgi_headers_map(request);
}

bool HttpResponse::_split_cgi_header_body(const std::string& raw_output, std::string& header_part)
{
    size_t delimiter_pos = raw_output.find("\r\n\r\n");
    size_t delimiter_len = 4;

    if (delimiter_pos == std::string::npos) {
        delimiter_pos = raw_output.find("\n\n");
        delimiter_len = 2;
    }

    if (delimiter_pos == std::string::npos) {
        return false;  // 没找到合法边界，说明数据有问题
    }

    header_part = raw_output.substr(0, delimiter_pos);

    this->_body     = raw_output.substr(delimiter_pos + delimiter_len);
    this->_body_len = this->_body.length();
    return true;
}

void HttpResponse::_parse_cgi_headers(const std::string& header_part)
{
    std::stringstream header_stream(header_part);
    std::string       line;

    while (std::getline(header_stream, line)) {
        if (!line.empty() && line[line.length() - 1] == '\r') { line.erase(line.length() - 1); }
        if (line.empty()) continue;

        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key   = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            size_t first = value.find_first_not_of(" \t");
            size_t last  = value.find_last_not_of(" \t");
            if (first != std::string::npos && last != std::string::npos) {
                value = value.substr(first, (last - first + 1));
            } else {
                value = "";
            }
            _add_cgi_header_vector(key, value);
        }
    }
}

void HttpResponse::_build_cgi_status_line(const HttpRequest& request)
{
    std::string str_status_code = "200 OK";

    if (!_get_cgi_header("Status", "", false).empty()) {
        str_status_code = _get_cgi_header("Status", "", false);
    } else if (!_get_cgi_header("Location", "", false).empty()) {
        str_status_code = "302 Found";
    }
    this->_status_line = request.get_version() + " " + str_status_code + "\r\n";
}

void HttpResponse::_build_cgi_headers_map(const HttpRequest& request)
{
    this->_add_header_vector("Server", "Cat server/1.0.0 (Fedora)");
    this->_add_header_vector("Date", this->_generate_date());

    bool has_content_length = false;
    for (std::size_t i = 0; i < _cgi_headers_vector.size(); i++) {
        if (_cgi_headers_vector[i].first == "Status") continue;
        if (_cgi_headers_vector[i].first == "Content-Length") has_content_length = true;
        _add_header_vector(_cgi_headers_vector[i].first, _cgi_headers_vector[i].second);
    }

    if (!has_content_length) {
        _add_header_vector("Content-Length", Utils::toString(this->_body_len));
    }

    if (request.get_is_keep_alive() == true)
        this->_add_header_vector("Connection", "keep-alive");
    else if (request.get_is_keep_alive() == false)
        this->_add_header_vector("Connection", "close");
}

std::string HttpResponse::_get_cgi_header(const std::string& key, const std::string& value_sub,
                                          bool check_value) const
{
    for (size_t i = 0; i < this->_cgi_headers_vector.size(); ++i) {
        // 1. 先对上 Key
        if (this->_cgi_headers_vector[i].first == key) {
            // 分流 A：不检查内容，直接盲拿这个 Key 对应的第一个 Value
            if (!check_value) { return this->_cgi_headers_vector[i].second; }

            // 分流 B：开启检查，检查当前这个 Value 里是否包含目标特征（如 "id="）
            if (this->_cgi_headers_vector[i].second.find(value_sub) != std::string::npos) {
                return this->_cgi_headers_vector[i].second;  // 找到了，直接返回这行完整的 Value
            }
        }
    }
    return "";
}

void HttpResponse::_add_cgi_header_vector(const std::string& key, const std::string& value)
{
    if (key.empty() || value.empty()) return;
    this->_cgi_headers_vector.push_back(std::make_pair(key, value));
}

bool HttpResponse::_validate_cgi_content_type() const
{
    std::string val = _get_cgi_header("Content-Type", "", false);
    if (val.empty()) return false;
    size_t slash = val.find('/');
    if (slash == std::string::npos || slash == 0) return false;
    return true;
}
