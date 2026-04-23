#include "HttpRequest.hpp"

#include "HttpRequest.hpp"

const std::string& HttpRequest::get_method() const {
    return this->_method;
}

const std::string& HttpRequest::get_path() const {
    return this->_path;
}

const std::string& HttpRequest::get_http_version() const {
    return this->_http_version;
}

const std::map<std::string, std::string>& HttpRequest::get_header_map() const {
    return this->_header_map;
}

const std::string& HttpRequest::get_body_content() const {
    return this->_body_content;
}

std::size_t HttpRequest::get_body_len() const {
    return this->_body_len;
}