#include "HttpResponse.hpp"
#include "HttpRequest.hpp"
#include <sstream>
#include <iterator>

#include <unistd.h> //tmp header missing config data 
#include <fcntl.h>
#include <sys/stat.h> 
#include <vector>

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
    if (req.get_method() == "GET")
        ret = this->_handle_get(full_path);
    else if (req.get_method() == "POST")
        ret = this->_handle_post(full_path, req);
    else if (req.get_method() == "DELETE")
        ret = this->_handle_delete(full_path);
 
    if (ret != 200)
    {
        this->set_status(ret);
        return ;
    }
 

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

int HttpResponse::_check_resource(std::string &full_path, const HttpRequest& req)
{
    struct stat st;
    struct stat st_index;

    if (stat(full_path.c_str(), &st) == -1)
    {
        if (errno == ENOENT)
            return (NOT_FOUND);
        else if (errno == EACCES)
            return (PER_DENIED);
        return (SERVER_ERROR);
    }

    if (S_ISDIR(st.st_mode))
    {
        if (req.get_method() != "GET")
            return (METHOD_NOT_ALLOWED);
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
        if (access(full_path.c_str(), R_OK) == -1)
            return (PER_DENIED);
        this->set_body_len(st_index.st_size);
        return (SUCCESS);
    }
    if (S_ISREG(st.st_mode))
    {
        if (access(full_path.c_str(), R_OK) == -1)
            return (PER_DENIED);
        this->set_body_len(st.st_size);
        return (SUCCESS);
    }
    return (NOT_FOUND);
}

void HttpResponse::set_status(int code)
{
    this->_status_code = code;
}

void HttpResponse::set_body_len(size_t body_len)
{
    this->_body_len = body_len;
}

int HttpResponse::_handle_get(const std::string& full_path)
{
    int fd;
    ssize_t ret;
    std::vector<char> tmp(this->_body_len);

    fd = open(full_path.c_str(), O_RDONLY);
    if (fd == -1)
        return (NOT_FOUND); 
    if (this->_body_len == 0)
    {
        this->_body.clear();
        close(fd);
        return (SUCCESS);
    }
    ret = read(fd, &tmp[0], this->_body_len);
    close(fd);
    if (ret < 0) 
    {
        this->_body.clear();
        return (SERVER_ERROR);
    }
    this->_body.assign(tmp.begin(), tmp.begin() + ret);
    return (SUCCESS);
}

int HttpResponse::_handle_post(const std::string& full_path, const HttpRequest& req)
{
    int fd;
    ssize_t ret;
    size_t total_size;
    size_t byte_written;
    const char *tmp_buff;

    fd = open(full_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1)
        return (PER_DENIED);
    if (req.get_body_len() == 0)
    {
        this->_body.clear();
        close(fd);
        return (CREATED);
    }
    total_size = req.get_body_len();
    byte_written = 0;
    tmp_buff = req.get_body_content().data(); 
    ret = 0;
    while (1)
    {
        ret = write(fd, tmp_buff + byte_written, total_size - byte_written);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue ;
            close(fd);
            return (SERVER_ERROR);
        }
        if (ret == 0 && total_size > 0)
        {
            close(fd);
            return (SERVER_ERROR);
        }
        byte_written += ret;
        if (byte_written >= total_size)
            break ;
    }
    close(fd);
    return (CREATED);
}

void HttpResponse::add_header(const std::string& key, const std::string& value)
{

}

std::string HttpResponse::get_raw_data()
{

}





