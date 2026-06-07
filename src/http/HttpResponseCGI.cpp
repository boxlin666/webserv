#include "HttpResponse.hpp"
#include "HttpRequest.hpp"
#include <iostream>

void HttpResponse::build_cgi_response(const HttpRequest& request, const std::string& cgi_output)
{
    std::string header_part;

    // 1. 切割：如果切割失败（非法数据），直接拉闸报错
    if (!_split_cgi_header_body(cgi_output, header_part)) {
        std::cerr << "[CGI Parse Error] No header-body delimiter found!" << std::endl;
        // this->buildErrorResponse(500);
        return;
    }

    // 2. 解析：将头部字符串转化为字典
    const std::map<std::string, std::string>& cgi_headers = _parse_cgi_headers(header_part); 

    _full_response.reserve(cgi_output.length());
    // 3. 组装：将内容打包成合法的 HTTP 响应并存入输出缓冲区
    _prepare_cgi_response(request, cgi_headers);
    _append_full_response();
}

void HttpResponse::_prepare_cgi_response(const HttpRequest& request, const std::map<std::string, std::string>& cgi_header)
{
    _build_cgi_status_line(request, cgi_header);
    _build_cgi_headers_map(request, cgi_header);
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

    this->_body = raw_output.substr(delimiter_pos + delimiter_len);
    this->_body_len = this->_body.length();
    return true;
}

std::map<std::string, std::string> HttpResponse::_parse_cgi_headers(const std::string& header_part)
{
    std::map<std::string, std::string> headers;
    std::stringstream                  header_stream(header_part);
    std::string                        line;

    while (std::getline(header_stream, line)) {
        if (!line.empty() && line[line.length() - 1] == '\r') { line.erase(line.length() - 1); }
        if (line.empty()) continue;

        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key   = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            // Trim 空格
            size_t first = value.find_first_not_of(" \t");
            size_t last  = value.find_last_not_of(" \t");
            if (first != std::string::npos && last != std::string::npos) {
                value = value.substr(first, (last - first + 1));
            } else {
                value = "";
            }
            headers[key] = value;
        }
    }
    return headers;
}

void HttpResponse::_build_cgi_status_line(const HttpRequest& request, const std::map<std::string, std::string>& cgi_headers)
{
    std::string str_status_code = "200 OK";

    // 确定状态码
    if (cgi_headers.count("Status")) {
        // 注意：C++98 中 map.at() 在某些编译器不可用，这里可以用 find() 或者直接 []
        std::map<std::string, std::string>::const_iterator it = cgi_headers.find("Status");
        if (it != cgi_headers.end()) str_status_code = it->second;
    } else if (cgi_headers.count("Location")) {
        str_status_code = "302 Found";
    }

    this->_status_line = request.get_version() + " " + str_status_code + "\r\n";
}

void HttpResponse::_build_cgi_headers_map(const HttpRequest& request, const std::map<std::string, std::string>& cgi_headers)
{
    this->_add_header_vector("Server", "Cat server/1.0.0 (Fedora)");
    this->_add_header_vector("Date", this->_generate_date());

    bool has_content_length = false;
    for (std::map<std::string, std::string>::const_iterator it = cgi_headers.begin();
         it != cgi_headers.end(); ++it) {
        if (it->first == "Status") continue;  // 跳过 Status，因为上面首行已经处理了
        if (it->first == "Content-Length") has_content_length = true;
        _add_header_vector(it->first, it->second);
    }

    if (!has_content_length) { _add_header_vector("Content-Length", Utils::toString(this->_body_len));}

    if (request.get_is_keep_alive() == true)
        this->_add_header_vector("Connection", "keep-alive");
    else if (request.get_is_keep_alive() == false)
        this->_add_header_vector("Connection", "close");
}
