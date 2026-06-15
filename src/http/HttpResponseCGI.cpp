#include <iostream>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

bool HttpResponse::build_cgi_response(const HttpRequest& request, const std::string& cgi_output)
{
    std::string header_part;

    std::cerr << "CGI CHECKING"  << std::endl;
    if (!_split_cgi_header_body(cgi_output, header_part) ||  !_parse_cgi_headers(header_part)
        || !_validate_cgi_content_type() ||  !_validate_cgi_content_length()) 
    {
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
        std::cerr << "[CGI Parse Error] No header-body delimiter found!" << std::endl;
        return false;
    }

    header_part = raw_output.substr(0, delimiter_pos);

    this->_body     = raw_output.substr(delimiter_pos + delimiter_len);
    this->_body_len = this->_body.length();
    return true;
}

bool HttpResponse::_parse_cgi_headers(const std::string& header_part)
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

    std::string str_status_code = _get_cgi_header_value("Status", "", false);
    if (str_status_code == "302 Found" && _get_cgi_header_value("Location", "", false).empty())
    {
        std::cerr << "[CGI Parse Error] Missing Location URL when the status code is 302!" << std::endl;
        return (false);
    }
    return (true);
}

void HttpResponse::_build_cgi_status_line(const HttpRequest& request)
{
    std::string str_status_code = "200 OK";

    if (!_get_cgi_header_value("Status", "", false).empty()) {
        str_status_code = _get_cgi_header_value("Status", "", false);
    } else if (!_get_cgi_header_value("Location", "", false).empty()) {
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

std::string HttpResponse::_get_cgi_header_value(const std::string& key, const std::string& value,
                                          bool check_value) const
{
    for (std::size_t i = 0; i < this->_cgi_headers_vector.size(); i++) 
    { 
        if (this->_cgi_headers_vector[i].first == key) 
        {     
            if (!check_value) { return this->_cgi_headers_vector[i].second; }

            if (this->_cgi_headers_vector[i].second == value)
                return this->_cgi_headers_vector[i].second; 
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
    std::string val = _get_cgi_header_value("Content-Type", "", false);
    if (val.empty()) 
    {
        std::cerr << "[CGI Parse Error] Missing or invalid Content-Type!" << std::endl;
        return false;
    }
    size_t slash = val.find('/');
    if (slash == std::string::npos || slash == 0)
    {
        std::cerr << "[CGI Parse Error] Missing or invalid Content-Type!" << std::endl;
        return false;
    }

    int nb_of_content_header = 0;
    for (std::size_t i = 0; i < _cgi_headers_vector.size(); i++)
    {
        if (_cgi_headers_vector[i].first == "Content-Type" &&
            !_get_cgi_header_value(_cgi_headers_vector[i].first, "", false).empty())
            nb_of_content_header++;
    }
    
    if (nb_of_content_header > 1) 
    {
        std::cerr << "[CGI Parse Error] Multiple Content-Type Header!" << std::endl;
        return false;
    }
    return true;
}

bool HttpResponse::_validate_cgi_content_length() const
{
    std::string val = _get_cgi_header_value("Content-Length", "", false);

    if (val.empty()) return true; 

    if (Utils::toString(this->_body_len) != val)
    {
        std::cout << "body len: " << _body_len << " content length: " << val  << std::endl;
        std::cerr << "[CGI Parse Error] Invalid content length!" << std::endl;
        return false;
    }
    return true;
}