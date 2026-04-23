#include "HttpResponse.hpp"
#include "HttpRequest.hpp"
#include <sstream>
#include <iterator>

#include <unistd.h> //tmp header missing config data 
#include <fcntl.h>
#include <sys/stat.h> 
#include <vector>
#include <cstdio>
#include <ctime>

std::map<int, std::string> HttpResponse::_status_msg_map;
std::map<std::string, std::string> HttpResponse::_ext_map;

HttpResponse::HttpResponse(void)
{
    this->reset();
}

HttpResponse::~HttpResponse(void)
{

}

void HttpResponse::init_response_map(void)
{
    HttpResponse::_init_ext_map();
    HttpResponse::_init_status_msg_map();
}

void HttpResponse::build(const HttpRequest& req, const ServerConfig& config)
{
    int ret;

    //1.prerequiste check! 
    ret = this->_check_request(req);
    if (ret != 200)
    {
        this->set_status(ret);
        return ;
    }

    //2. path convert
    this->_full_path = this->_build_full_path(req, config);
 
    //3. check ressource (accessbility)
    ret = this->_check_resource(req);

    if (ret != 200)
    {
        this->set_status(ret);
        return ;
    }
   
    //4. get_raw_data
    if (req.get_method() == "GET")
        ret = this->_handle_get();
    else if (req.get_method() == "POST")
        ret = this->_handle_post(req);
    else if (req.get_method() == "DELETE")
        ret = this->_handle_delete();
 
    if (ret != 200)
    {
        this->set_status(ret);
        return ;
    }
 
    //5. append all the elements together!
    this->_append_full_response();
}

int HttpResponse::_check_request(const HttpRequest& req)const
{
    (void)_config;
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
    if (req.get_method() != "GET" && req.get_method() != "POST" && req.get_method() != "DELETE")
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

std::string HttpResponse::_build_full_path(const HttpRequest& req, const ServerConfig& config)const
{
    (void)config;
    //we assume _current_path ending without "\" (alreamd trimmed in Config class!)
    std::string root_path; //should be found in config!
    std::string full_path;
    char    buffer[100]; //tmp 

    getcwd(buffer, 100); //tmp because we don't have config!
    root_path = buffer; //tmp
    full_path = root_path + req.get_path(); //tmp funct!

    return (full_path);
}

int HttpResponse::_check_resource(const HttpRequest& req)
{
    struct stat st;
    struct stat st_index;
    char last_c;

    if (stat(this->_full_path.c_str(), &st) == -1)
    {
        if (errno == ENOENT)
            return (NOT_FOUND);
        else if (errno == EACCES)
            return (PER_DENIED);
        return (SERVER_ERROR);
    }

    if (S_ISDIR(st.st_mode))
    {
        last_c = this->_full_path[this->_full_path.size() - 1];
        if (req.get_method() != "GET")
            return (METHOD_NOT_ALLOWED);
        if (last_c != '/')
            this->_full_path += "/";
        this->_full_path += "index.html";
        if (stat(this->_full_path.c_str(), &st_index) == -1)
        {
            if (errno == ENOENT)
                return (NOT_FOUND);
            return (PER_DENIED);
        }
        if (!S_ISREG(st_index.st_mode))
            return (NOT_FOUND);
        if (access(this->_full_path.c_str(), R_OK) == -1)
            return (PER_DENIED);
        this->set_body_len(st_index.st_size);
        return (SUCCESS);
    }
    if (S_ISREG(st.st_mode))
    {
        if (access(this->_full_path.c_str(), R_OK) == -1)
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

int HttpResponse::_handle_get(void)
{
    int fd;
    ssize_t ret;
    std::vector<char> tmp(this->_body_len);

    fd = open(this->_full_path.c_str(), O_RDONLY);
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

int HttpResponse::_handle_post(const HttpRequest& req)
{
    int fd;
    ssize_t ret;
    size_t total_size;
    size_t byte_written;
    const char *tmp_buff;

    fd = open(this->_full_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

int HttpResponse::_handle_delete(void)
{
    if (std::remove(this->_full_path.c_str()))
        return (DELETED);
    if (errno == ENOENT)
        return (NOT_FOUND);
    else if (errno == EACCES || errno == EPERM)
        return (PER_DENIED);
    return (SERVER_ERROR);
}

void    HttpResponse::_init_status_msg_map(void) 
{
    /* 2xx */
    HttpResponse::_status_msg_map[SUCCESS] = "OK";
    HttpResponse::_status_msg_map[CREATED] = "Created";
    HttpResponse::_status_msg_map[DELETED] = "No Content";

    /* 4xx */
    HttpResponse::_status_msg_map[BAD_REQUEST] = "Bad Request";
    HttpResponse::_status_msg_map[PER_DENIED] = "Permission Denied"; 
    HttpResponse::_status_msg_map[NOT_FOUND] = "Forbidden";

    /* 5xx */
    HttpResponse::_status_msg_map[METHOD_NOT_ALLOWED] = "Method Not Allowed";
    HttpResponse::_status_msg_map[BODY_TOO_LARGE] = "Content Too Large";
    HttpResponse::_status_msg_map[URI_TOO_LONG] = "URI Too Long";
    HttpResponse::_status_msg_map[SERVER_ERROR] = "Internal Server Error";
    HttpResponse::_status_msg_map[NO_METHOD] = "Not Implemented";
    HttpResponse::_status_msg_map[NO_HTTP_VERSION] = "HTTP Version Not Supported";
}

void    HttpResponse::_init_ext_map(void)
{
    if (!HttpResponse::_ext_map.empty())
        return ;
    HttpResponse::_ext_map["html"] = "text/html";
    HttpResponse::_ext_map["css"]  = "text/css";
    HttpResponse::_ext_map["js"]   = "text/javascript";
    HttpResponse::_ext_map["jpg"]  = "image/jpeg";
    HttpResponse::_ext_map["jpeg"] = "image/jpeg";
    HttpResponse::_ext_map["png"]  = "image/png";
    HttpResponse::_ext_map["gif"]  = "image/gif";
    HttpResponse::_ext_map["txt"]  = "text/plain";
    HttpResponse::_ext_map["ico"]  = "image/x-icon";
}

std::string &HttpResponse::_build_status_line(const HttpRequest& req)
{
    std::stringstream ss;

    ss << this->_status_code;
    this->_status_line = req.get_http_version() + " " + ss.str() + " " + this->_status_msg_map[this->_status_code] + "\r\n";
    return (this->_status_line);
}

std::string HttpResponse::_get_date(void)const
{
    char buf[100];
    std::string result;
    time_t now = time(0);
    struct tm *tm = gmtime(&now);

    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", tm);
    result = buf;
    return (result);
}

std::string HttpResponse::_get_last_modif_date(void)const
{
    struct stat file_info;
    char buf[100];
    std::string result("");

    //should have a centralized function for all stat operation later to optimize!
    if (stat(this->_full_path.c_str(), &file_info) == 0) 
    {
        time_t last_modified_raw = file_info.st_mtime; 
        struct tm *tm_struct = gmtime(&last_modified_raw);
        if (tm_struct)
        {
            std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", tm_struct);
            result = buf;
        }
    }
    return (result);
}

void HttpResponse::_build_headers_map(void)
{
    this->_add_header("Server", "Cat server/1.0.0 (Fedora)");
    this->_add_header("Date", this->_get_date());
    std::stringstream ss;

    ss << this->_body_len;
    if (this->_status_code != DELETED)
    {
        if (!this->_body.empty())
            this->_add_header("Content-Type", this->_get_content_type());
        this->_add_header("Content-Length", ss.str());
    }
    this->_add_header("Last-Modified", this->_get_last_modif_date());
    this->_add_header("Connection", "close"); //temporary hard code depend on the http request
}

std::string HttpResponse::_get_content_type(void)const
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

void HttpResponse::_append_full_response(void)
{
    this->_full_response.reserve(this->_status_line.size() + this->_body.size() + 1024);
    this->_full_response += this->_status_line;
    for (std::map<std::string, std::string>::const_iterator it = this->_headers_map.begin(); it != this->_headers_map.end(); ++it)
    {
        this->_full_response += it->first + ": " + it->second + "\r\n";
    }
    this->_full_response += "\r\n";
    this->_full_response.append(this->_body);
}

void HttpResponse::_add_header(const std::string& key, const std::string& value)
{
    if (key.empty() || value.empty())
        return ;
    this->_headers_map[key] = value;
}

const std::string& HttpResponse::get_full_response(void)const
{
    return (this->_full_response);
}

void HttpResponse::reset(void)
{
    this->set_status(SUCCESS); //default OK at the beginning! 
    this->_status_line.clear();
    this->_status_msg_map.clear();
    this->_ext_map.clear();
    this->_headers_map.clear();
    this->_body.clear();
    this->_full_response.clear();
    this->_body_len = 0;
    this->_full_path.clear();    
}
