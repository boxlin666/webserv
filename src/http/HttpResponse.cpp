#include "HttpResponse.hpp"
#include "HttpRequest.hpp"
#include <sstream>
#include <iterator>

#include <unistd.h> //tmp header missing config data 
#include <sys/stat.h> 


HttpResponse::HttpResponse(void)
{

}

HttpResponse::~HttpResponse(void)
{

}

void HttpResponse::build(const HttpRequest& req, const ServerConfig& config)
{
    int ret;
    std::string full_path;

    //1.prerequiste check! 
    ret = this->_check_request(req);
    if (ret != 200)
    {
        this->set_status(ret);
        return ;
    }

    //2. path convert
    full_path = this->_build_full_path(req, config);
 
    //3. check ressource (accessbility)
    ret = this->_check_resource(full_path);

    if (ret != 200)
    {
        this->set_status(ret);
        return ;
    }


    //4. get_raw_data

    //5. append all the elements together!

}

int HttpResponse::_check_request(const HttpRequest& req)const
{
    size_t  req_line_len = 0;
    size_t  req_header_len = 0;
    std::string request_path = "";

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
    request_path = req.get_path();
    if (request_path[0] != '/' || request_path.empty())
        return (BAD_REQUEST);
    /* TO DO
        if find /../../ in path we return BAD_REQUEST as well
    */ 
    return (200); //assume the check is OK! But we can still have 403 (permission denied) 404 （not found）
}

std::string &HttpResponse::_build_full_path(const HttpRequest& req, const ServerConfig& config)const
{
    //we assume _current_path ending without "\" (alreamd trimmed in Config class!)
    std::string root_path; //should be found in config!
    std::string full_path;
    char    buffer[100]; //tmp 

    getcwd(buffer, 100); //tmp because we don't have config!
    root_path = buffer; //tmp
    full_path = root_path + req.get_path(); //tmp funct!

    return (full_path);
}

int HttpResponse::_check_resource(std::string &full_path)
{
    struct stat st;
    struct stat st_index;

    if (stat(full_path.c_str(), &st) == -1)
    {
        if (errno == ENONET)
            return (NOT_FOUND);
        else if (errno == EACCES)
            return (PER_DENIED);
        return (SERVER_ERROR);
    }

    if (S_ISDIR(st.st_mode))
    {
        if (full_path.back() != '/')
            full_path += "/";
        full_path += "index.html";
        if (stat(full_path.c_str(), &st_index) == -1)
        {
            if (errno == ENOENT)
                return (NOT_FOUND);
            return (PER_DENIED);
        }
        if (!S_ISREG(st_index.st_mode))
           return (NOT_FOUND); 
        this->set_body_len(st_index.st_size);
        return (SUCCESS);
    }
    if (S_ISREG(st.st_mode))
    {
        this->set_body_len(st.st_size);
        return (SUCCESS);
    }
    return (NOT_FOUND);
}

void HttpResponse::set_status(int code)
{
    this->_status_code = code;
}

void HttpResponse::set_body_len(size_t input)
{
    this->body_len = input;
}

void HttpResponse::add_header(const std::string& key, const std::string& value)
{

}

std::string HttpResponse::get_raw_data()
{

}





