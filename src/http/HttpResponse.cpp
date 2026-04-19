#include "HttpResponse.hpp"
#include "HttpRequest.hpp"
#include <sstream>
#include <iterator>

HttpResponse::HttpResponse(void)
{

}

HttpResponse::~HttpResponse(void)
{

}

void HttpResponse::build(const HttpRequest& req, const ServerConfig& config)
{
    int ret;

    //1.prerequiste check! 
    ret = this->_check_request(req);
    if (ret)
    {
        this->set_status(ret);
        return ;
    }

    //2. path convert
 
    //3. check ressource (accessbility)

    //4. get_raw_data

    //5. append all the elements together!

}

int HttpResponse::_check_request(const HttpRequest& req)
{
    size_t  req_line_len = 0;
    size_t  req_header_len = 0;

    req_line_len = req.get_method().length() + req.get_path().length() + req.get_http_version().length();

    for (std::map<std::string, std::string>::const_iterator it = req.get_header_map().begin(); it != req.get_header_map().end(); ++it)
    {
        req_header_len += it->first.length() + it->second.length() + 4; //CTRL
    }
    if (req_line_len > URI_SIZE)
        return (URI_TOO_LONG);
    if (req_header_len > MAX_HEADER_SIZE)
        return (MAX_HEADER_SIZE);
    if (req.get_http_version() == "HTTP/1.1")
        return (NO_HTTP_VERSION);
    if (req.get_method() != "GET" && req.get_method() != "POST" || req.get_method() != "DELETE")
        return (NO_METHOD);
    if (req.get_body_len() > 100000000000000000 && req.get_method() == "POST") //temporary limit should be set up by config file!
        return (BODY_TOO_LARGE);
    return (200); //assume the check is OK! But we can still have 403 (permission denied) 404 （not found）
}

void HttpResponse::set_status(int code)
{
    this->_status_code = code;
}

void HttpResponse::add_header(const std::string& key, const std::string& value)
{

}

std::string HttpResponse::get_raw_data()
{

}





