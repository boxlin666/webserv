#include "HttpResponse.hpp"
#include "Utils.hpp"

void HttpResponse::_prepare_response_data(const HttpRequest& req)
{
    this->_build_status_line(req);
    this->_build_headers_map();
}

void HttpResponse::_build_status_line(const HttpRequest& req)
{
    std::string str_status_code = Utils::toString(this->_status_code);
    this->_status_line = req.get_http_version() + " " + str_status_code + " " + this->_status_msg_map[this->_status_code] + "\r\n"; 
}

void HttpResponse::_build_headers_map(void)
{
    this->_add_header("Server", "Cat server/1.0.0 (Fedora)");
    this->_add_header("Date", this->_generate_date());

    if (this->_status_code != DELETED)
    {
        if (!this->_body.empty())
            this->_add_header("Content-Type", this->_generate_content_type());
        this->_add_header("Content-Length", Utils::toString(this->_body_len));
    }
    this->_add_header("Last-Modified", this->_body_last_modif_date);
    this->_add_header("Connection", "close"); //temporary hard code depend on the http request
}

void HttpResponse::_add_header(const std::string& key, const std::string& value)
{
    if (key.empty() || value.empty())
        return ;
    this->_headers_map[key] = value;
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