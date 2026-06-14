#include "HttpResponse.hpp"
#include "Utils.hpp"

void HttpResponse::_prepare_response_data(const HttpRequest& request)
{
    this->_build_status_line(request);
    this->_build_headers_map(request);
}

void HttpResponse::_build_status_line(const HttpRequest& request)
{
    std::string str_status_code = Utils::toString(this->_status_code);
    std::map<int, std::string>::const_iterator it = this->_status_msg_map.find(this->_status_code);

    if (it != this->_status_msg_map.end())
        this->_status_line = request.get_version() + " " + str_status_code + " " + this->_status_msg_map[this->_status_code] + "\r\n";
    else
        this->_status_line = request.get_version() + " " + str_status_code + " " + " Internal Server Error" + "\r\n";
}

void HttpResponse::_build_headers_map(const HttpRequest& request)
{
    this->_add_header_vector("Server", "Cat server/1.0.0 (Fedora)");
    this->_add_header_vector("Date", this->_generate_date());

    if (this->_status_code != DELETED)
    {
        if (!this->_body.empty() && !_has_header("Content-Type"))
            this->_add_header_vector("Content-Type", this->_generate_content_type());
        this->_add_header_vector("Content-Length", Utils::toString(this->_body_len));
    }
    this->_add_header_vector("Last-Modified", this->_body_last_modif_date);
    if (request.get_is_keep_alive() == true)
        this->_add_header_vector("Connection", "keep-alive");
    else if (request.get_is_keep_alive() == false)
        this->_add_header_vector("Connection", "close");
}

void HttpResponse::_add_header_vector(const std::string& key, const std::string& value)
{
    if (key.empty() || value.empty())
        return ;
    this->_headers_vector.push_back(std::make_pair(key, value));
}

bool HttpResponse::_has_header(const std::string& key) const
{
    for (size_t i = 0; i < _headers_vector.size(); i++) {
        if (_headers_vector[i].first == key)
            return true;
    }
    return false;
}

std::string HttpResponse::_generate_date(void)const
{
    std::string result;
    time_t now = time(0);

    result = Utils::formatHttpDate(now);
    return (result);
}

std::string HttpResponse::_generate_content_type(void)const
{
    std::size_t  pos;
    std::string ext;
    std::map<std::string, std::string>::const_iterator it;

    pos = this->_full_path.find_last_of(".");
    if (pos == std::string::npos)
        return ("application/octet-stream");
    ext = this->_full_path.substr(pos + 1);
    it = this->_ext_map.find(ext);
    if (it != this->_ext_map.end()) 
        return (it->second);
    return ("application/octet-stream");
}